// Engine.cpp
#include "engine.h"
#include "searcher.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include <fstream>

#ifndef ENGINE_ID
#define ENGINE_ID "unknown"
#endif

// ------------------
// -- Constructors --
// ------------------

EngineMode mode = EngineMode::ANALYSIS;
EngineSide side = EngineSide::UNKNOWN;
GameTracker tracker;

Engine::Engine()
{
    game_board = Board();
    search_board = game_board;

    movegen = std::make_unique<MoveGenerator>(search_board);

    evaluator.nnue.load(options.nnue_weight_path);
    evaluator.loadOpeningPST(options.opening_pst_path);
    evaluator.loadEndgamePST(options.endgame_pst_path);

    searcher = std::make_unique<Searcher>(*this, search_board, evaluator, evaluator.nnue);

    book.load(options.opening_book_path);

    Logging::track_search_stats = options.showStats;
    g_stats = SearchStats();
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
    //evaluator = Evaluator(&precomp);
    //stats = SearchStats();
    tt.clear();
    game_board.setFromFEN(STARTPOS_FEN);
    search_board = game_board;
}

// -------------------
// -- UCI Handlers  --
// -------------------

void Engine::setOption(const std::string& name, const std::string& value) {
    auto boolFromString = [](const std::string &val) {
        return val == "true" || val == "True" || val == "1";
    };

    if (name == "Move Overhead") {
        limits.stopped = false;
        options.moveOverhead = std::stoi(value);
        std::cout << "info string set Move Overhead = " << options.moveOverhead << std::endl;
    } 
    else if (name == "Hash") {
        options.hashSize = std::stoi(value);
        tt.resize(options.hashSize);
        std::cout << "info string set Hash = " << options.hashSize << std::endl;
    } 
    else if (name == "Threads") {
        options.threads = std::stoi(value);
        // Apply threads to searcher
        std::cout << "info string set Threads = " << options.threads << std::endl;
    } 
    else if (name == "Ponder") {
        options.ponder = boolFromString(value);
        std::cout << "info string set Ponder = " << (options.ponder ? "true" : "false") << std::endl;
    } 
    else if (name == "UCI_ShowWDL") {
        options.uciShowWDL = boolFromString(value);
        std::cout << "info string set UCI_ShowWDL = " << (options.uciShowWDL ? "true" : "false") << std::endl;
    } 
    else if (name == "ShowStats") {
        options.showStats = boolFromString(value);
        Logging::track_search_stats = options.showStats;
        std::cout << "info string set ShowStats = " << (options.showStats ? "true" : "false") << std::endl;
    }

    // -------- customization --------
    else if (name == "MagicBitboards") {
        options.magics = boolFromString(value);
        std::cout << "info string set MagicBitboards = " << (options.magics ? "true" : "false") << std::endl;
    }
    else if (name == "NNUE") {
        options.nnue = boolFromString(value);
        if (options.nnue) {
            evaluator.nnue.load(options.nnue_weight_path); // load NNUE if path is set
        }
        std::cout << "info string set NNUE = " << (options.nnue ? "true" : "false") << std::endl;
    }

    // -------- search --------
    else if (name == "quiescence") {
        options.quiescence = boolFromString(value);
        std::cout << "info string set quiescence = " << (options.quiescence ? "true" : "false") << std::endl;
    }
    else if (name == "aspiration") {
        options.aspiration = boolFromString(value);
        std::cout << "info string set aspiration = " << (options.aspiration ? "true" : "false") << std::endl;
    }
    else if (name == "scorched_earth") {
        options.scorched_earth = std::stoi(value);
        std::cout << "info string set contempt " << options.scorched_earth << std::endl;
    }

    // -------- move ordering --------
    else if (name == "moveordering") {
        options.moveordering = boolFromString(value);
        std::cout << "info string set moveordering = " << (options.moveordering ? "true" : "false") << std::endl;
    }
    else if (name == "mvvlva_ordering") {
        options.mvvlva = boolFromString(value);
        std::cout << "info string set mvvlva_ordering = " << (options.mvvlva ? "true" : "false") << std::endl;
    }
    else if (name == "see_ordering") {
        options.see = boolFromString(value);
        std::cout << "info string set see_ordering = " << (options.see ? "true" : "false") << std::endl;
    }

    // -------- pruning --------
    else if (name == "delta_pruning") {
        options.delta_pruning = boolFromString(value);
        std::cout << "info string set delta_pruning = " << (options.delta_pruning ? "true" : "false") << std::endl;
    }
    else if (name == "see_pruning") {
        options.see_pruning = boolFromString(value);
        std::cout << "info string set see_pruning = " << (options.see_pruning ? "true" : "false") << std::endl;
    }

    // -------- books / paths --------
    else if (name == "opening_pst_file") {
        options.opening_pst_path = "../bin/" + value + ".txt";
        if(evaluator.loadEndgamePST(options.opening_pst_path)) {
            std::cout << "info string OpeningPST loaded successfully: " << options.opening_pst_path << std::endl;
        } else {
            std::cout << "info string Failed to load OpeningPST: " << options.opening_pst_path << std::endl;
        }
    }
    else if (name == "endgame_pst_file") {
        options.endgame_pst_path = "../bin/" + value + ".txt";
        if(evaluator.loadEndgamePST(options.endgame_pst_path)) {
            std::cout << "info string EndgamePST loaded successfully: " << options.endgame_pst_path << std::endl;
        } else {
            std::cout << "info string Failed to load EndgamePST: " << options.endgame_pst_path << std::endl;
        }
    }
    else if (name == "nnue_weight_file") {
        options.nnue_weight_path = "../bin/nnue_wgts/" + value + ".bin";
        if (options.nnue) {
            if(evaluator.nnue.load(options.nnue_weight_path)) {
                std::cout << "info string NNUE loaded successfully: " << options.nnue_weight_path << std::endl;
            } else {
                std::cout << "info string Failed to load NNUE: " << options.nnue_weight_path << std::endl;
            }
        }
    }
    else if (name == "opening_book") {
        options.opening_book_path = "../bin/" + value + ".bin"; // auto-construct full path

        if (!value.empty()) {
            if (book.load(options.opening_book_path))
                std::cout << "info string Book loaded successfully: " << options.opening_book_path << std::endl;
            else
                std::cout << "info string Failed to load book: " << options.opening_book_path << std::endl;
        }
    }
    else if (name == "SyzygyPath") {
        options.syzygyPath = value;
        //tablebases.load(value);
        std::cout << "info string set SyzygyPath = " << options.syzygyPath << std::endl;
    }

    // -------- logging --------
    else if (name == "log_dir") {
        Logging::log_dir = value;
        std::cout << "info string log_dir loaded successfully: " << Logging::log_dir << std::endl;
    }
    else if (name == "timer_logging") {
        if (Logging::track_timers != boolFromString(value)) {Logging::toggleTimers();}
    }
    else if (name == "stats_logging") {
        if (Logging::track_search_stats != boolFromString(value)) {Logging::toggleSearchStats();}
    }
    else if (name == "game_logging") {
        if (Logging::track_game_log != boolFromString(value)) {Logging::toggleGameLog();}
    }
    else if (name == "uci_logging") {
         if (Logging::track_uci != boolFromString(value)) {Logging::toggleUCI();}
    }
    else {
        std::cout << "info string ignoring unknown option: " << name << std::endl;
    }

    std::cout.flush();
}


void Engine::print_info() {
    std::cout << "option name Hash type spin value "
              << options.hashSize << " min 1 max 8192\n";
    std::cout << "option name Threads type spin value "
              << options.threads << " min 1 max 64\n";
    std::cout << "option name Move Overhead type spin value "
              << options.moveOverhead << " min 0 max 5000\n";
    std::cout << "option name Ponder type check value "
              << (options.ponder ? "true" : "false") << "\n";
    std::cout << "option name UCI_ShowWDL type check value "
              << (options.uciShowWDL ? "true" : "false") << "\n";
    std::cout << "option name ShowStats type check value "
              << (options.showStats ? "true" : "false") << "\n";
    // feature toggles
    std::cout << "option name Magics type check value "
              << (options.magics ? "true" : "false") << "\n";

    std::cout << "option name NNUE type check value "
              << (options.nnue ? "true" : "false") << "\n";

    std::cout << "option name Quiescence type check value "
              << (options.quiescence ? "true" : "false") << "\n";

    std::cout << "option name Aspiration type check value "
              << (options.aspiration ? "true" : "false") << "\n";
    // paths
    std::cout << "option name OpeningPST type string value "
              << options.opening_pst_path << "\n";

    std::cout << "option name EndgamePST type string value "
              << options.endgame_pst_path << "\n";

    std::cout << "option name NNUEWeights type string value "
              << options.nnue_weight_path << "\n";

    std::cout << "option name OpeningBook type string value "
              << options.opening_book_path << "\n";

    std::cout << "option name SyzygyPath type string value "
              << options.syzygyPath << "\n";

    std::cout << "option name LogsDir type string value "
              << Logging::log_dir << "\n";
}

void Engine::setPosition(const std::string& fen,
                         const std::vector<std::string>& moveStrs)
{
    // --- Reset board ---
    if (fen == "startpos")
        game_board.setFromFEN(STARTPOS_FEN);
    else
        game_board.setFromFEN(fen);

    // --- Apply moves ---
    // must interpret uci in context of the board state for ep
    for (const auto& moveStr : moveStrs) {

        int start  = algebraic_to_square(moveStr.substr(0, 2));
        int target = algebraic_to_square(moveStr.substr(2, 2));
        int flag   = Move::noFlag;

        int movedPiece = game_board.getMovedPiece(start);
        bool isPawn = (movedPiece == pawn);

        // ---- Promotion ----
        if (moveStr.length() == 5) {
            switch (moveStr[4]) {
                case 'q': flag = Move::promoteToQueenFlag;  break;
                case 'r': flag = Move::promoteToRookFlag;   break;
                case 'b': flag = Move::promoteToBishopFlag;break;
                case 'n': flag = Move::promoteToKnightFlag;break;
            }
        }

        // ---- Castling ----
        if (movedPiece == king &&
            (moveStr == "e1g1" || moveStr == "e1c1" ||
             moveStr == "e8g8" || moveStr == "e8c8")) {
            flag = Move::castleFlag;
        }

        // ---- En-passant ----
        if (isPawn && game_board.currentGameState.enPassantFile != -1) {
            int epRank   = game_board.is_white_move ? 5 : 2;
            int epSquare = game_board.currentGameState.enPassantFile + epRank * 8;
            if (target == epSquare)
                flag = Move::enPassantCaptureFlag;
        }

        // ---- Double pawn push ----
        if (isPawn && flag == Move::noFlag) {
            int sr = start / 8;
            int tr = target / 8;
            if (std::abs(sr - tr) == 2)
                flag = Move::pawnTwoUpFlag;
        }

        Move m(start, target, flag);

        //if (nnue) nnue->on_make_move(game_board, m);
        game_board.MakeMove(m);
    }

    // --- Sync search board ---
    search_board = game_board;

    game_over = checkGameEnd();
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
        limits = SearchLimits(
            settings.movetime,
            settings.depth
        );
        return;
    }

    // game clock
    int side = game_board.is_white_move ? 0 : 1;
    int myTime = (side == 0 ? settings.wtime : settings.btime);
    int myInc  = (side == 0 ? settings.winc  : settings.binc);

    // compute function
    double aggressiveness = 1.0;
    int timeBudget = static_cast<int>(
        (static_cast<double>(myTime) / movesToGo + myInc / 2.0)
        * aggressiveness
    );

    timeBudget = std::max(10, timeBudget - options.moveOverhead);

    limits = timeBudget;
}


void Engine::startSearch() {
    evaluator.nnue.build_accumulators(search_board);

    // book probe
    if (!options.opening_book_path.empty()) {
        uint64_t key = search_board.zobrist_hash;
        auto bookMoves = book.get_moves(key);

        if (!bookMoves.empty()) {
            // pick a move (currently first or weighted)
            std::string bestBookMove = book.pick_weighted_move(bookMoves);

            // Castling translation debug
            if (bestBookMove == "e1h1") { 
                bestBookMove = "e1g1"; 
            }
            else if (bestBookMove == "e1a1") { 
                bestBookMove = "e1c1"; 
            }
            else if (bestBookMove == "e8h8") { 
                bestBookMove = "e8g8"; 
            }
            else if (bestBookMove == "e8a8") { 
                bestBookMove = "e8c8"; 
            }

            sendBestMove(bestBookMove); // ponder move empty
            return; // skip search entirely
        }
    }

    // normal search
    search_depth = settings.depth;
    time_left[0] = settings.wtime;
    time_left[1] = settings.btime;
    increment[0] = settings.winc;
    increment[1] = settings.binc;
    pondering = settings.infinite;

    computeSearchTime(settings);
    iterativeDeepening();

    sendBestMove(bestMove);
}

void Engine::stopSearch() {
    limits.stopped = true;
    tracker.result = GameResult::ABORTED;
    tracker.reason = GameEndReason::NONE;
}

void Engine::ponderHit() {
    pondering = false;
}


// --------------------------------
// -- Iterative Deepening Search --
// --------------------------------

// -- accounts for time / depth requirements
// -- uses previous search information
// ------ transposition table
// ------ principle move ordering

void Engine::iterativeDeepening() {
    ScopedTimer engineTimer(T_SEARCH); // top-level engine timer
    auto start_time = std::chrono::steady_clock::now();
    limits.max_depth = std::min(limits.max_depth, 30);

    // reset global stats
    if (Logging::track_search_stats) {
        g_stats = SearchStats{};           // global-level
    }

    SearchResult last_result;
    SearchResult result;

    // generate first moves once
    Move first_moves[MAX_MOVES];
    int count = movegen->generateMoves(game_board, false);
    std::copy_n(movegen->moves, count, first_moves);

    int aspiration_start_depth = 6;
    int delta = 50;
    int alpha = -MATE_SCORE;
    int beta = MATE_SCORE;

    std::fill(std::begin(last_eval_table), std::end(last_eval_table), INVALID);

    int depth = 1;
    Move prevBest = Move::NullMove();

    while (!limits.should_stop(depth)) {
        ScopedTimer timer(T_ROOT);
        auto depth_start = std::chrono::steady_clock::now();

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
            searcher->orderedMoves(first_moves, count, game_board, 0, {});
        }

        // --- search ---
        if (depth < aspiration_start_depth) {
            result = searcher->search(first_moves, count, depth, limits,
                                     last_result.best_line.line);
        } else {
            while (true) {
                result = searcher->searchAspiration(first_moves, count, depth, limits,
                                                   last_result.best_line.line,
                                                   alpha, beta);

                if (Logging::track_search_stats) {
                    if ((int)g_stats.aspirationResearches.size() <= depth) {
                        g_stats.aspirationResearches.resize(depth + 1);
                    }

                    auto &asp = g_stats.aspirationResearches[depth];
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

        // --- determine best move and eval ---
        bestMove = last_result.bestMove;
        bestEval  = last_result.eval;

        // --- update PV line if current result has one ---
        if (!result.best_line.line.empty()) {
            pv_line = result.best_line.line;
        }

        // --- update per-depth stats ---
        auto depth_end = std::chrono::steady_clock::now();

        if (Logging::track_search_stats) {
            g_stats.maxDepth = depth;
            g_stats.evalPerDepth.push_back(result.eval);
            g_stats.bestMovePerDepth.push_back(result.bestMove);
            g_stats.timerPerDepthMS.push_back( std::chrono::duration<double, std::milli>(depth_end - depth_start).count());
            if (Move::SameMove(bestMove, prevBest)) g_stats.bestmoveStable++;
        }

        prevBest = bestMove;
        if (std::abs(result.eval) >= MATE_SCORE - 10) break;
        depth++;
    }

    // --- finalize cumulative stats ---
    if (Logging::track_search_stats) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start_time).count();
        g_stats.timeMs = elapsed;
        g_stats.nps = elapsed > 0 ? (1000.0 * g_stats.nodes / elapsed) : 0.0;
        g_stats.rootEval = last_result.eval;
        g_stats.bestMove = last_result.bestMove;

        g_stats.ebf = pow(double(g_stats.nodes) / 1.0, 1.0 / g_stats.maxDepth); // rough estimate
        g_stats.qratio = double(g_stats.qnodes) / g_stats.nodes;

        tracker.bestMoves.push_back(bestMove);
        tracker.evals.push_back(bestEval);

        logSearchStats(game_board.getBoardFEN());  // JSON written once at end
    }
    if (Logging::track_timers) {logTimingStats();}
}


void Engine::evaluate_position() {
    search_board = game_board;
    //nnue.refresh(search_board);

    search_depth = settings.depth;
    time_left[0] = settings.wtime;
    time_left[1] = settings.btime;
    increment[0] = settings.winc;
    increment[1] = settings.binc;
    pondering = settings.infinite;

    computeSearchTime(settings);
    iterativeDeepening();

    std::cout << "eval " << bestEval << " bestmove " << bestMove.uci() << std::endl;
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

    game_board.MakeMove(best);
    game_over = checkGameEnd();

    if (mode == EngineMode::GAME) {
        trackGame();
        resetSearchStats();
    }
}


// GAME LOGGING

void Engine::newGame() {
    mode = EngineMode::GAME;
    startNewSession();

    tracker.playedMoves.clear();
    tracker.lastPositionHash = 0;
    tracker.active = true;

    g_gamelog = GameLog{};
    g_gamelog.startFEN = game_board.getBoardFEN();

    side = game_board.is_white_move ? EngineSide::WHITE : EngineSide::BLACK;

    game_board.setFromFEN(STARTPOS_FEN);
    search_board = game_board;
}

bool Engine::checkGameEnd() {
    if (g_gamelog.finalized) return true;

    if (isCheckmate()) {
        g_gamelog.outcome =
            game_board.is_white_move && side == EngineSide::WHITE 
                                     ? GameResult::WIN
                                     : GameResult::LOSS;
        g_gamelog.reason = GameEndReason::CHECKMATE;
    }
    else if (isStalemate()) {
        g_gamelog.outcome = GameResult::DRAW;
        g_gamelog.reason = GameEndReason::STALEMATE;
    }
    else if (game_board.currentGameState.fiftyMoveCounter >= 50) {
        g_gamelog.outcome = GameResult::DRAW;
        g_gamelog.reason = GameEndReason::FIFTY_MOVE;
    }
    else if (isThreefold()) {
        g_gamelog.outcome = GameResult::DRAW;
        g_gamelog.reason = GameEndReason::THREEFOLD;
    }
    else {
        return false; // game not over yet
    }

    // finalize
    g_gamelog.moves = game_board.allGameMoves;
    g_gamelog.finalEval = evaluator.nnue.full_eval(game_board);
    logGameLog();
    g_gamelog.finalized = true;
    tt.clear();

    return true;
}


bool Engine::isCheckmate() {
    return !movegen->hasLegalMoves(game_board) && game_board.is_in_check;
}

bool Engine::isStalemate() {
    return !movegen->hasLegalMoves(game_board) && !game_board.is_in_check;
}

bool Engine::isThreefold() {
    return game_board.hash_history[game_board.zobrist_hash] >= 3;
}


void Engine::trackGame() {
    tracker.active = true;
    size_t old_count = tracker.playedMoves.size();
    size_t new_count = game_board.allGameMoves.size();

    if (new_count > old_count) {
        for (size_t i = old_count; i < new_count; i++) {
            tracker.playedMoves.push_back(game_board.allGameMoves[i]);
        }
    }
}

void Engine::finalizeGameLog() {
    g_gamelog.finalEval = evaluator.nnue.full_eval(game_board);
    logGameLog();
}

// TESTING

uint64_t Engine::perft(int depth) {
    ScopedTimer timer(T_PERFT);
    if (depth == 0) {return 1;}

    uint64_t nodes = 0;

    Move moves[MAX_MOVES];
    int count = movegen->generateMoves(search_board, false);
    std::copy_n(movegen->moves, count, moves);

    // we can skip the last make/unmake move by utilizing
    // that the num_moves at depth=1 is the perft value for that branch
    // its a decent speed up
    //if (depth == 1) return count; 

    Move* end = moves + count;
    for (Move* m = moves; m < end; ++m) {
        search_board.MakeMove(*m);
        nodes += perft(depth - 1);
        search_board.UnmakeMove(*m);
    }

    return nodes;
}

void Engine::perftPrint(int depth) {   
    if (depth == 0) {std::cout << "1" << std::endl;}

    unsigned long long nodes = 0;

    Move moves[MAX_MOVES];
    int count = movegen->generateMoves(search_board, false);
    std::copy_n(movegen->moves, count, moves);

    Move* end = moves + count;
    for (Move* m = moves; m < end; ++m) {
        search_board.MakeMove(*m);
        nodes += perft(depth - 1);
        search_board.UnmakeMove(*m);
    }

    std::cout << "Nodes searched: " << nodes << std::endl;
    logTimingStats();
}

void Engine::perftDivide(int depth) {
    int count = movegen->generateMoves(search_board, false);
    Move* moves = movegen->moves;

    unsigned long long total = 0;

    for (int i = 0; i < count; i++) {
        search_board.MakeMove(moves[i]);
        uint64_t n = perft(depth - 1);
        search_board.UnmakeMove(moves[i]);

        std::cout << moves[i].uci() << ": " << n << "\n";
        total += n;
    }

    std::cout << "Total: " << total << "\n";
}

void Engine::SEETest(int capture_square) {
    int count = movegen->generateMoves(search_board, true);

    for (int i = 0; i < count; i++) {
        Move m = movegen->moves[i];
        // check if capture (otherwise will incl checks and other qsearch stuff)
        if (m.TargetSquare() == capture_square) {
            int see = evaluator.SEE(search_board, m);
            std::cout << "SEE (" << m.uci() << ") = " << see << std::endl;
        }
    }
}

void Engine::staticEvalTest() {
    // Compute opening and endgame reports
    TaperedEvalReport report = evaluator.Evaluate(search_board, evaluator.PST_opening);

    // For clarity, also compute endgame separately
    TaperedEvalReport endgame_report = evaluator.Evaluate(search_board, evaluator.PST_endgame);

    // Merge opening/endgame into one tapered report
    report.opening = report.opening;
    report.endgame = endgame_report.opening; // endgame components
    int phase = evaluator.gamePhase(search_board);
    report.computeTapered(phase, evalWeights);

    std::cout << "=== Static Evaluation ===\n";
    std::cout << "Phase: " << phase << " / 256\n";
    report.printDetailed(evalWeights);
}


void Engine::nnueEvalTest() {
    evaluator.nnue.build_accumulators(search_board);
    int eval = evaluator.nnue.evaluate(search_board.is_white_move);

    if (!search_board.is_white_move) eval = -eval;

    std::cout << "=== NNUE Evaluation ===\n";
    std::cout << "NNUE Eval: " << eval << " centipawns\n";
}

void Engine::moveOrderingTest(int depth) {
    std::cout << "=== Move Ordering Test ===\n";

    // Generate moves at root
    int moveCount = movegen->generateMoves(search_board, false);
    Move* moves = movegen->moves;

    std::vector<std::pair<Move, int>> scoredMoves;

    // Use empty TT and PV for testing at root
    Move ttMove;               // assume default-constructed = no move
    std::vector<Move> previousPV;

    for (int i = 0; i < moveCount; ++i) {
        int score = searcher->moveScore(
            moves[i], 
            search_board, 
            0,        // ply 0 at root
            ttMove, 
            previousPV
        );
        scoredMoves.emplace_back(moves[i], score);
    }

    // Sort by score descending
    std::sort(scoredMoves.begin(), scoredMoves.end(), [](auto& a, auto& b){
        return a.second > b.second;
    });

    // Print
    for (auto& [move, score] : scoredMoves) {
        std::cout << move.uci() << ": " << score << "\n";
    }
}

