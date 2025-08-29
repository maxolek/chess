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
    stats = SearchStats();
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
    stats = SearchStats();
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
    } else if (name == "ShowStats") {
        options.showStats = (value == "true" || value == "True" || value == "1");
        Searcher::enableStats(options.showStats);
        std::cout << "info string set ShowStats - " << (options.showStats ? "true" : "false") << std::endl;
    }
    else {
        std::cout << "info string ignoring unknown option: " << name << std::endl;
    }

    std::cout.flush();
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

// ------------------------
// -- Local Eval Storage --
// ------------------------

void Engine::store_last_result(const SearchResult& res) {
    // clear table
    std::fill(std::begin(last_eval_table), std::end(last_eval_table), INVALID);

    // write moves/evals from last depth
    for (int i = 0; i < res.root_count; ++i) {
        last_eval_table[res.root_moves[i].move.Value()] = res.root_moves[i].eval;
    }
}

inline int Engine::get_prev_eval(Move m) const {
    int v = last_eval_table[m.Value()];
    return (v == INVALID) ? -MATE_SCORE : v;
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
    int timeBudget = static_cast<int>((static_cast<double>(myTime) / movesToGo + myInc/2) * aggressiveness);
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
    auto start_time = std::chrono::steady_clock::now();
    limits.max_depth = std::min(limits.max_depth, 30);

    if (Searcher::trackStats) {
        stats = SearchStats();           // reset engine-level stats
        Searcher::stats = SearchStats(); // reset global stats
    }

    SearchResult last_result;
    SearchResult result;

    // generate first moves once
    Move first_moves[MAX_MOVES];
    movegen->generateMoves(*game_board, false);
    int count = std::min(movegen->count, MAX_MOVES);
    std::copy_n(movegen->moves, count, first_moves);

    int aspiration_start_depth = 6;
    int delta = 50;
    int alpha = -MATE_SCORE;
    int beta = MATE_SCORE;

    std::fill(std::begin(last_eval_table), std::end(last_eval_table), INVALID);

    int depth = 1;
    Move prevBest = Move::NullMove();

    while (!limits.should_stop(depth)) {
        // --- initialize per-depth stats ---
        if (Searcher::trackStats) {
            STATS_DEPTH_INIT(depth);
            if ((int)Searcher::stats.depthTTHits.size() <= depth) Searcher::stats.depthTTHits.push_back(0);
            if ((int)Searcher::stats.depthTTStores.size() <= depth) Searcher::stats.depthTTStores.push_back(0);
            if ((int)Searcher::stats.aspirationResearches.size() <= depth) Searcher::stats.aspirationResearches.push_back({});
        }

        // --- move ordering ---
        if (depth > 1) {
            std::sort(first_moves, first_moves + count,
                [&](Move a, Move b) { return get_prev_eval(a) > get_prev_eval(b); });

            if (depth >= aspiration_start_depth) {
                delta = std::max(delta, 50 + depth * 10);
                alpha = last_result.eval - delta;
                beta  = last_result.eval + delta;
            }
        } else {
            Searcher::orderedMoves(evaluator, first_moves, count, *game_board, 0, {});
        }

        // --- search ---
        if (depth < aspiration_start_depth) {
            result = Searcher::search(search_board, *movegen, evaluator,
                                      first_moves, count, depth, limits,
                                      last_result.best_line.line);
        } else {
            while (true) {
                result = Searcher::searchAspiration(search_board, *movegen, evaluator,
                                                    first_moves, count, depth, limits,
                                                    last_result.best_line.line,
                                                    alpha, beta);

                if (Searcher::trackStats) {
                    auto &asp = Searcher::stats.aspirationResearches[depth];
                    if (result.eval <= alpha) asp.push_back(-1);
                    else if (result.eval >= beta) asp.push_back(+1);
                }

                if (result.eval <= alpha) alpha -= delta;
                else if (result.eval >= beta) beta += delta;
                else break;

                delta *= 2;
            }
        }

        // --- store results ---
        if (!Move::SameMove(result.bestMove, Move::NullMove())) {
            last_result = result;
            store_last_result(result);
        }

        bestMove = result.bestMove.IsNull() ? last_result.bestMove : result.bestMove;
        bestEval = result.bestMove.IsNull() ? last_result.eval : result.eval;
        if (!result.best_line.line.empty()) pv_line = result.best_line.line;

        // --- update per-depth stats ---
        if (Searcher::trackStats) {
            Searcher::stats.maxDepth = depth;
            Searcher::stats.evalPerDepth.push_back(result.eval);
            if (Move::SameMove(bestMove, prevBest)) Searcher::stats.bestmoveStable++;
        }

        prevBest = bestMove;
        if (std::abs(result.eval) >= MATE_SCORE - 10) break;
        depth++;
    }

    // --- finalize cumulative stats ---
    if (Searcher::trackStats) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - start_time).count();
        Searcher::stats.timeMs = elapsed;
        Searcher::stats.nps = elapsed > 0 ? (1000.0 * Searcher::stats.nodes / elapsed) : 0.0;
        Searcher::stats.rootEval = last_result.eval;
        Searcher::stats.bestMove = last_result.bestMove;

        stats = Searcher::stats;
        stats.ebf = pow(double(stats.nodes) / 1.0, 1.0 / stats.maxDepth); // roughly
        stats.qratio = double(stats.qnodes) / stats.nodes;

        logStats(game_board->getBoardFEN());  // JSON written once at end
    }
}

void Engine::evaluate_position(SearchSettings settings) {
    search_board = *game_board;

    search_depth = settings.depth;
    time_left[0] = settings.wtime;
    time_left[1] = settings.btime;
    increment[0] = settings.winc;
    increment[1] = settings.binc;
    pondering = settings.infinite;

    computeSearchTime(settings);
    iterativeDeepening();

    std::cout << "eval " << bestEval << " bestmove " << bestMove.uci() << std::endl;
    std::cout.flush();
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

void Engine::logStats(const std::string& fen) const {
    if (!options.showStats) return;

    static std::ofstream statsFile("../tests/logs/search_stats.log", std::ios::app);
    if (!statsFile.is_open()) return;

    std::ostringstream out;
    out << "{"
        << "\"fen\":\"" << fen << "\","
        << "\"nodes\":" << stats.nodes << ","
        << "\"qnodes\":" << stats.qnodes << ","
        << "\"tt_hits\":" << stats.ttHits << ","
        << "\"tt_stores\":" << stats.ttStores << ","
        << "\"fail_highs\":" << stats.failHighs << ","
        << "\"fail_high_first\":" << stats.fail_high_first << ","
        << "\"fail_lows\":" << stats.failLows << ","
        << "\"ebf\":" << stats.ebf << ","
        << "\"qratio\":" << stats.qratio << ","
        << "\"max_depth\":" << stats.maxDepth << ","
        << "\"time_ms\":" << stats.timeMs << ","
        << "\"nps\":" << stats.nps << ","
        << "\"root_eval\":" << stats.rootEval << ","
        << "\"best_move\":" << stats.bestMove.uci() << ","
        << "\"bestmove_stable\":" << stats.bestmoveStable << ",";

    // --- helper for flat vectors ---
    auto vec_to_json = [](const auto& v) {
        std::ostringstream oss;
        oss << "[";
        for (size_t i = 0; i < v.size(); ++i) {
            if (i > 0) oss << ",";
            oss << v[i];
        }
        oss << "]";
        return oss.str();
    };

    // --- helper for vector<vector<int>> ---
    auto nested_vec_to_json = [](const auto& vv) {
        std::ostringstream oss;
        oss << "[";
        for (size_t i = 0; i < vv.size(); ++i) {
            if (i > 0) oss << ",";
            oss << "[";
            for (size_t j = 0; j < vv[i].size(); ++j) {
                if (j > 0) oss << ",";
                oss << vv[i][j];
            }
            oss << "]";
        }
        oss << "]";
        return oss.str();
    };

    // --- per-depth stats ---
    out << "\"depthNodes\":" << vec_to_json(stats.depthNodes) << ","
        << "\"depthQNodes\":" << vec_to_json(stats.depthQNodes) << ","
        << "\"depthFailHighs\":" << vec_to_json(stats.depthFailHighs) << ","
        << "\"depthFailLows\":" << vec_to_json(stats.depthFailLows) << ","
        << "\"depthTTHits\":" << vec_to_json(stats.depthTTHits) << ","
        << "\"depthTTStores\":" << vec_to_json(stats.depthTTStores) << ","
        << "\"depthAspirationResearches\":" << nested_vec_to_json(stats.aspirationResearches) << ","
        << "\"evalPerDepth\":" << vec_to_json(stats.evalPerDepth)
        << "}\n";

    statsFile << out.str();
    statsFile.flush();
}

