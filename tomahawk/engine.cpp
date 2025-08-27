// Engine.cpp
#include "engine.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include <fstream>

// ------------------
// -- Constructors --
// ------------------

Engine::Engine(Board* _board) {
    game_board = _board;
    search_board = *game_board;
    movegen = std::make_unique<MoveGenerator>(search_board);
    evaluator = Evaluator(&precomp);
    Magics::initMagics();
    clearState();
    movegen->generateMoves(search_board, false);
    legal_move_count = movegen->count;
}

// ------------------
// -- Engine State --
// ------------------

void Engine::clearState() {
    pondering = false;
    bestMove = Move::NullMove();
    ponderMove = Move::NullMove();
    search_depth = 0;
    time_left[0] = time_left[1] = 0;
    increment[0] = increment[1] = 0;
    limits = SearchLimits(); // reset time control
    evaluator = Evaluator(&precomp);
}

// -------------------
// -- UCI Handlers  --
// -------------------

void Engine::setOption(const std::string& name, const std::string& value) {
    if (name == "Move Overhead") {
        limits.stopped = false;
        options.moveOverhead = std::stoi(value);
        std::cout << "info string set Move Overhead = " << options.moveOverhead << std::endl;
    } 
    else if (name == "Hash") {
        options.hashSize = std::stoi(value);
        std::cout << "info string set Hash = " << options.hashSize << std::endl;
    } 
    else if (name == "Threads") {
        options.threads = std::stoi(value);
        std::cout << "info string set Threads = " << options.threads << std::endl;
    } 
    else if (name == "Ponder") {
        options.ponder = (value == "true" || value == "1");
        std::cout << "info string set Ponder = " << (options.ponder ? "true" : "false") << std::endl;
    } 
    else if (name == "SyzygyPath") {
        options.syzygyPath = value;
        std::cout << "info string set SyzygyPath = " << options.syzygyPath << std::endl;
    } 
    else if (name == "UCI_ShowWDL") {
        options.uciShowWDL = (value == "true" || value == "1");
        std::cout << "info string set UCI_ShowWDL = " << (options.uciShowWDL ? "true" : "false") << std::endl;
    } 
    else {
        std::cout << "info string ignoring unknown option: " << name << std::endl;
    }
}

void Engine::setPosition(const std::string& fen, const std::vector<Move>& moves) {
    if (fen == "startpos") {
        game_board->setFromFEN(STARTPOS_FEN);
    } else {
        game_board->setFromFEN(fen);
    }
    playMoves(moves);
}

void Engine::playMoves(const std::vector<Move>& moves) {
    for (const auto& move : moves) {
        game_board->MakeMove(move);
    }
}

void Engine::playMovesStrings(const std::vector<std::string>& moves) {
    for (const auto& moveStr : moves) {
        // translate UCI string -> Move
        int start = algebraic_to_square(moveStr.substr(0, 2));
        int target = algebraic_to_square(moveStr.substr(2, 2));
        int flag = Move::noFlag;

        int movedPiece = game_board->getMovedPiece(start);
        bool isPawn = (movedPiece == pawn);

        // En passant
        if (isPawn && game_board->currentGameState.enPassantFile != -1) {
            int epRank = game_board->is_white_move ? 5 : 2;
            int epSquare = game_board->currentGameState.enPassantFile + epRank * 8;
            if (target == epSquare) flag = Move::enPassantCaptureFlag;
        }

        // Double pawn push
        if (isPawn) {
            int startRank = start / 8;
            int targetRank = target / 8;
            if (std::abs(startRank - targetRank) == 2) flag = Move::pawnTwoUpFlag;
        }

        // Promotions
        if (moveStr.length() == 5) {
            switch (moveStr[4]) {
                case 'q': flag = Move::promoteToQueenFlag; break;
                case 'r': flag = Move::promoteToRookFlag;  break;
                case 'b': flag = Move::promoteToBishopFlag; break;
                case 'n': flag = Move::promoteToKnightFlag; break;
            }
        }

        // Castling
        if ((moveStr == "e1g1" || moveStr == "e1c1" || moveStr == "e8g8" || moveStr == "e8c8") 
            && movedPiece == king) {
            flag = Move::castleFlag;
        }

        game_board->MakeMove(Move(start, target, flag));
    }
}

// --------------------
// -- Search Control --
// --------------------

void Engine::computeSearchTime(const SearchSettings& settings) {
    int movesToGo = settings.movestogo > 0 ? settings.movestogo : 20;

    // hard coded time/depth
    if (settings.movetime > 0 || settings.depth > 0) {
        limits = SearchLimits(settings.movetime, settings.depth);
        return;
    }
       
    // game clock
    int side = game_board->is_white_move ? 0 : 1;
    int myTime = (side == 0 ? settings.wtime : settings.btime);
    int myInc  = (side == 0 ? settings.winc  : settings.binc);

    // compute function
    double aggressiveness = 1;
    int timeBudget = static_cast<int>((static_cast<double>(myTime) / movesToGo + myInc) * aggressiveness);
    timeBudget = std::max(10, timeBudget - options.moveOverhead);

    limits = SearchLimits(timeBudget);
}

void Engine::startSearch(const SearchSettings& settings) {
    search_board = *game_board;

    search_depth = settings.depth;
    time_left[0] = settings.wtime;
    time_left[1] = settings.btime;
    increment[0] = settings.winc;
    increment[1] = settings.binc;
    pondering = settings.infinite;

    computeSearchTime(settings);

    iterativeDeepening();
    sendBestMove(bestMove, ponderMove);
}

void Engine::stopSearch() {
    limits.stopped = true;
}

void Engine::ponderHit() {
    pondering = false;
}

Move Engine::getBestMove(Board* board) {
    game_board = board;
    search_board = *game_board;
    SearchSettings defaults;
    iterativeDeepening();
    return bestMove;
}


// --------------------------------
// -- Iterative Deepening Search --
// --------------------------------

// -- accounts for time / depth requirements
// -- uses previous search information
// ------ transposition table
// ------ principle move ordering

void Engine::iterativeDeepening() {
    // constraints
    auto start_time = std::chrono::steady_clock::now();
    limits.max_depth = std::min(limits.max_depth, 30); // limit depth to 30 (e.g. in rook v nothing endgame it can hit depth 837)

    // updating best move
    Move iteration_bestMove = Move::NullMove();
    int iteration_bestEval = 0;
    // previous result for last it_deep iteration
    SearchResult last_result;

    // generate first moves once
    Move first_moves[MAX_MOVES]; int count = 0;
    movegen->generateMoves(*game_board, false);
    // copy
    std::copy_n(movegen->moves, movegen->count, first_moves);
    count = movegen->count;


    // limits include time / depth
    int depth = 1;
    while (!limits.should_stop(depth)) {

        // --- sort by previous iteration evals (descending) ---
        if (depth > 1) {
            // sort first_moves by previous iteration’s evals
            std::sort(first_moves, first_moves + count,
                    [&](Move a, Move b) {
                        int eval_a = -MATE_SCORE, eval_b = -MATE_SCORE;

                        // look up evals from last result
                        for (int i = 0; i < last_result.root_count; ++i) {
                            if (Move::SameMove(a, last_result.root_moves[i]))
                                eval_a = last_result.root_evals[i];
                            if (Move::SameMove(b, last_result.root_moves[i]))
                                eval_b = last_result.root_evals[i];
                        }
                        return eval_a > eval_b; // higher eval first
                    });
        }

        // search
        SearchResult result = Searcher::search(search_board, *movegen, evaluator, first_moves, count, depth, limits, last_result.best_line.line);
        last_result = result;

        //
        //  Best Move Handling
        //
        if (!result.bestMove.IsNull()) {
            iteration_bestMove = result.bestMove;
            iteration_bestEval = result.eval;
        }

        bestMove = result.bestMove.IsNull() ? iteration_bestMove : result.bestMove;
        pv_line = result.best_line.line.empty() ? pv_line : result.best_line.line;

        // check time & log
        auto now = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
        logSearchDepthInfo(depth, Searcher::max_q_depth, bestMove, 
                           result.best_line.line, iteration_bestEval, elapsed_ms);

        // re-enter?
        if (std::abs(iteration_bestEval) >= MATE_SCORE - 10) break;
        depth++;
    }
}

// -------------------
// -- Communication --
// -------------------

void Engine::sendBestMove(Move best, Move ponder) {
    std::cout << "bestmove " << best.uci();
    if (!ponder.IsNull()) {
        std::cout << " ponder " << ponder.uci();
    }
    std::cout << std::endl;
}

// -------------------
// -- Logging Utils --
// -------------------

void Engine::logSearchDepthInfo(
    int depth, int q_depth, Move _bestMove,
    const std::vector<Move>& best_line,
    int eval, long long elapsed_ms,
    const std::string& file_path) 
{
    std::ofstream file(file_path, std::ios::app);

    file << "-----------------------------\n";
    file << "FEN: " << search_board.getBoardFEN() << "\n";
    file << "Depth: " << depth << "\n";
    file << "Max Q Depth: " << q_depth << "\n";
    file << "Best move: " << _bestMove.uci() << "\n";
    file << "Eval: " << eval << "\nBest line: ";
    for (auto& mv : best_line) file << mv.uci() << " ";
    file << "\nTime: " << elapsed_ms << " ms\n";
    file << "Nodes searched: " << Searcher::nodesSearched << "\n";
    file << "-----------------------------\n";

    if (depth == 1) evaluator.writeEvalDebug(search_board, file_path);
}
