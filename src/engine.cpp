// Engine.cpp
#include <engine.h>
#include <searcher.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include <fstream>


// ------------------
// -- Constructors --
// ------------------

EngineMode mode = EngineMode::ANALYSIS;
EngineSide side = EngineSide::UNKNOWN;
GameTracker tracker;

Engine::Engine(EngineConfig& config)
    : cfg(config)
{
    game_board = Board();
    search_board = game_board; //Board(game_board);

    movegen = std::make_unique<MoveGenerator>(cfg, search_board);

    nnue.load(cfg.engine.nnue_weight_path);
    evaluator.loadOpeningPST(cfg.engine.opening_pst_path);
    evaluator.loadEndgamePST(cfg.engine.endgame_pst_path);

    searcher = std::make_unique<Searcher>(*this, search_board, evaluator, nnue);
    tt.clear();

    book.load(cfg.engine.opening_book_path);

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
        cfg.engine.MOVE_OVERHEAD_MS = std::stoi(value);
        std::cout << "info string set Move Overhead = " << cfg.engine.MOVE_OVERHEAD_MS << std::endl;
    } 
    else if (name == "Hash") {
        cfg.engine.HASH_SIZE_MB = std::stoi(value);
        tt.resize(cfg.engine.HASH_SIZE_MB);
        std::cout << "info string set Hash = " << cfg.engine.HASH_SIZE_MB << std::endl;
    } 
    else if (name == "Threads") {
        cfg.engine.MAX_THREADS = std::stoi(value);
        // Apply threads to searcher
        std::cout << "info string set Threads = " << cfg.engine.MAX_THREADS << std::endl;
    } 
    else if (name == "Ponder") {
        cfg.engine.PONDERING = boolFromString(value);
        std::cout << "info string set Ponder = " << (cfg.engine.PONDERING ? "true" : "false") << std::endl;
    } 
    else if (name == "UCI_ShowWDL") {
        cfg.options.UCI_SHOW_WDL = boolFromString(value);
        std::cout << "info string set UCI_ShowWDL = " << (cfg.options.UCI_SHOW_WDL ? "true" : "false") << std::endl;
    } 
    else if (name == "ShowStats") {
        Logging::track_search_stats = boolFromString(value);
        std::cout << "info string set ShowStats = " << (Logging::track_search_stats ? "true" : "false") << std::endl;
    }

    // -------- customization --------
    else if (name == "MagicBitboards") {
        cfg.engine.MAGICS = boolFromString(value);
        std::cout << "info string set MagicBitboards = " << (cfg.engine.MAGICS ? "true" : "false") << std::endl;
    }
    else if (name == "NNUE") {
        cfg.options._NNUE = boolFromString(value);
        if (cfg.options._NNUE) {
            nnue.load(cfg.engine.nnue_weight_path); // load NNUE if path is set
        }
        std::cout << "info string set NNUE = " << (cfg.options._NNUE ? "true" : "false") << std::endl;
    }

    // -------- search --------
    else if (name == "quiescence") {
        cfg.options._QUIESCENCE = boolFromString(value);
        std::cout << "info string set quiescence = " << (cfg.options._QUIESCENCE ? "true" : "false") << std::endl;
    }
    else if (name == "aspiration") {
        cfg.options._ASPIRATION = boolFromString(value);
        std::cout << "info string set aspiration = " << (cfg.options._ASPIRATION ? "true" : "false") << std::endl;
    }

    // -------- move ordering --------
    else if (name == "moveordering") {
        cfg.options._MOVE_ORDERING = boolFromString(value);
        std::cout << "info string set moveordering = " << (cfg.options._MOVE_ORDERING ? "true" : "false") << std::endl;
    }
    else if (name == "mvvlva_ordering") {
        cfg.options._MVVLVA_ORDERING = boolFromString(value);
        std::cout << "info string set mvvlva_ordering = " << (cfg.options._MVVLVA_ORDERING ? "true" : "false") << std::endl;
    }
    else if (name == "see_ordering") {
        cfg.options._SEE_ORDERING = boolFromString(value);
        std::cout << "info string set see_ordering = " << (cfg.options._SEE_ORDERING ? "true" : "false") << std::endl;
    }

    // -------- pruning --------
    else if (name == "delta_pruning") {
        cfg.options._DELTA_PRUNING = boolFromString(value);
        std::cout << "info string set delta_pruning = " << (cfg.options._DELTA_PRUNING ? "true" : "false") << std::endl;
    }
    else if (name == "see_pruning") {
        cfg.options._SEE_PRUNING = boolFromString(value);
        std::cout << "info string set see_pruning = " << (cfg.options._SEE_PRUNING ? "true" : "false") << std::endl;
    }

    // -------- books / paths --------
    else if (name == "opening_pst_file") {
        cfg.engine.opening_pst_path = PROJECT_ROOT / fs::path("bin") / fs::path(value + ".txt");
        if(evaluator.loadEndgamePST(cfg.engine.opening_pst_path)) {
            std::cout << "info string OpeningPST loaded successfully: " << cfg.engine.opening_pst_path << std::endl;
        } else {
            std::cout << "info string Failed to load OpeningPST: " << cfg.engine.opening_pst_path << std::endl;
        }
    }
    else if (name == "endgame_pst_file") {
        cfg.engine.endgame_pst_path = PROJECT_ROOT / fs::path("bin") / fs::path(value + ".txt");
        if(evaluator.loadEndgamePST(cfg.engine.endgame_pst_path)) {
            std::cout << "info string EndgamePST loaded successfully: " << cfg.engine.endgame_pst_path << std::endl;
        } else {
            std::cout << "info string Failed to load EndgamePST: " << cfg.engine.endgame_pst_path << std::endl;
        }
    }
    else if (name == "nnue_weight_file") {
        cfg.engine.nnue_weight_path = PROJECT_ROOT / fs::path("bin/nnue_wgts") / fs::path(value + ".bin");
        if (cfg.options._NNUE) {
            if(nnue.load(cfg.engine.nnue_weight_path)) {
                std::cout << "info string NNUE loaded successfully: " << cfg.engine.nnue_weight_path << std::endl;
            } else {
                std::cout << "info string Failed to load NNUE: " << cfg.engine.nnue_weight_path << std::endl;
            }
        }
    }
    else if (name == "opening_book") {
        cfg.engine.opening_book_path = PROJECT_ROOT / fs::path("bin") / fs::path(value + ".bin"); // auto-construct full path

        if (!value.empty()) {
            if (book.load(cfg.engine.opening_book_path))
                std::cout << "info string Book loaded successfully: " << cfg.engine.opening_book_path << std::endl;
            else
                std::cout << "info string Failed to load book: " << cfg.engine.opening_book_path << std::endl;
        }
    }
    else if (name == "SyzygyPath") {
        cfg.engine.syzygy_path = value;
        //tablebases.load(value);
        std::cout << "info string set SyzygyPath = " << cfg.engine.syzygy_path << std::endl;
    }

    // -------- logging --------
    else if (name == "log_dir") {
        Logging::setLogDir(value);
        std::cout << "info string log_dir loaded successfully: " << Logging::log_dir << std::endl;
    }
    else if (name == "timer_logging") {
        if (Logging::track_timers != boolFromString(value)) {Logging::setTrackTimers(boolFromString(value));}
    }
    else if (name == "stats_logging") {
        if (Logging::track_search_stats != boolFromString(value)) {Logging::setTrackSearchStats(boolFromString(value));}
    }
    else if (name == "stats_nodes_only") {
        if (Logging::track_search_nodes != boolFromString(value)) {Logging::setTrackSearchNodes(boolFromString(value));}
        if (Logging::track_search_stats == true && Logging::track_search_nodes == true) {Logging::setTrackSearchStats(false);}
    }
    else if (name == "game_logging") {
        if (Logging::track_game_log != boolFromString(value)) {Logging::setTrackGameLog(boolFromString(value));}
    }
    else if (name == "uci_logging") {
         if (Logging::track_uci != boolFromString(value)) {Logging::setTrackUCI(boolFromString(value));}
    }
    else {
        std::cout << "info string ignoring unknown option: " << name << std::endl;
    }

    std::cout.flush();
}


void Engine::print_info() {
    std::cout << "option name Hash type spin value "
              << cfg.engine.HASH_SIZE_MB << " min 1 max 8192\n";
    std::cout << "option name Threads type spin value "
              << cfg.engine.MAX_THREADS << " min 1 max 64\n";
    std::cout << "option name Move Overhead type spin value "
              << cfg.engine.MOVE_OVERHEAD_MS << " min 0 max 5000\n";
    std::cout << "option name Ponder type check value "
              << (cfg.engine.PONDERING ? "true" : "false") << "\n";
    std::cout << "option name UCI_ShowWDL type check value "
              << (cfg.options.UCI_SHOW_WDL ? "true" : "false") << "\n";
    std::cout << "option name ShowStats type check value "
              << (Logging::track_search_stats ? "true" : "false") << "\n";
    // feature toggles
    std::cout << "option name Magics type check value "
              << (cfg.engine.MAGICS ? "true" : "false") << "\n";

    std::cout << "option name NNUE type check value "
              << (cfg.options._NNUE ? "true" : "false") << "\n";

    std::cout << "option name Quiescence type check value "
              << (cfg.options._QUIESCENCE ? "true" : "false") << "\n";

    std::cout << "option name Aspiration type check value "
              << (cfg.options._ASPIRATION? "true" : "false") << "\n";
    // paths
    std::cout << "option name OpeningPST type string value "
              << cfg.engine.opening_pst_path << "\n";

    std::cout << "option name EndgamePST type string value "
              << cfg.engine.endgame_pst_path << "\n";

    std::cout << "option name NNUEWeights type string value "
              << cfg.engine.nnue_weight_path << "\n";

    std::cout << "option name OpeningBook type string value "
              << cfg.engine.opening_book_path << "\n";

    std::cout << "option name SyzygyPath type string value "
              << cfg.engine.syzygy_path << "\n";

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

    // ply tracking
    if (game_board.is_white_move) ply = 1;
    else ply = 2;

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
                case 'b': flag = Move::promoteToBishopFlag; break;
                case 'n': flag = Move::promoteToKnightFlag; break;
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
        ply++;
    }

    // --- Sync search board ---
    search_board = game_board; //Board(game_board);

    // --- game handling ---
    if (mode == EngineMode::GAME) {
        side = game_board.is_white_move ? EngineSide::WHITE : EngineSide::BLACK;
        g_gamelog.side = game_board.is_white_move ? "white" : "black";
    }

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
    if (g_run_context.is_new_game) {
        g_gamelog.wtime = settings.wtime; 
        g_gamelog.btime = settings.btime; 
        g_gamelog.winc = settings.winc; 
        g_gamelog.binc = settings.binc; 
        g_gamelog.movestogo = settings.movestogo;
        g_gamelog.depth = settings.depth;
        g_gamelog.nodes = settings.nodes;
        g_gamelog.movetime = settings.movetime;
        g_run_context.is_new_game = false;
    }

    int movesToGo = settings.movestogo > 0 ? settings.movestogo : 20;

    // hard coded time/depth
    if (settings.movetime > 0 || settings.depth > 0) {
        limits = SearchLimits(
            settings.movetime - cfg.engine.MOVE_OVERHEAD_MS,
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

    timeBudget = std::max(10, timeBudget - cfg.engine.MOVE_OVERHEAD_MS);

    limits = SearchLimits(timeBudget);
}


void Engine::startSearch() {
    nnue.build_accumulators(search_board);

    // book probe
    if (!cfg.engine.opening_book_path.empty()) {
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
    //limits.start_time = start_time;
    //limits.max_depth = (limits.max_depth < 0) ? 30 : std::min(limits.max_depth, 30);
    //limits.stopped = false;
    g_run_context.search_uuid = generate_uuid();

    // reset global stats
    if (Logging::track_search_stats) {
        resetSearchStats();           // global-level
    }

    SearchResult last_result;
    SearchResult result;

    // generate first moves once
    Move first_moves[MAX_MOVES];
    int count = movegen->generateMoves(game_board, false);
    std::copy_n(movegen->moves, count, first_moves);

    // play only-legal-move immediately
    if (count == 1) {
        bestMove = first_moves[0];
        bestEval = -MATE_SCORE-100; // not concerned with eval here
        return;
    }

    int alpha = -MATE_SCORE;
    int beta = MATE_SCORE;
    int delta = cfg.search.ASPIRATION_WINDOW;

    std::fill(std::begin(last_eval_table), std::end(last_eval_table), INVALID);

    int depth = 1;
    Move prevBest = Move::NullMove();

    // --- iterative deepening loop ---
    while (!limits.should_stop(depth)) {
        ScopedTimer timer(T_ROOT);
        auto depth_start = std::chrono::steady_clock::now();
        g_stats.max_depth = depth;

        // --- move ordering ---
        if (depth > 1) {
            std::sort(first_moves, first_moves + count,
                      [&](Move a, Move b) { return get_prev_eval(a) > get_prev_eval(b); });
        } else {
            searcher->orderedMoves(first_moves, count, game_board, 0, {});
        }

        // --- search ---
        if (cfg.options._ASPIRATION && (depth >= cfg.search.ASPIRATION_START_DEPTH)) {
            
            delta = std::max(delta, cfg.search.ASPIRATION_WINDOW + depth * cfg.search.ASPIRATION_DEPTH_SCALE);
            alpha = last_result.eval - delta;
            beta  = last_result.eval + delta;
            
            while (true) {
                result = searcher->searchAspiration(first_moves, count, depth, limits,
                                                   last_result.best_line.line,
                                                   alpha, beta);

                if (Logging::track_search_stats) {
                    if (result.eval <= alpha) STATS_ASPIRATION_FAILLOW(depth);
                    else if (result.eval >= beta) STATS_ASPIRATION_FAILHIGH(depth);
                }

                if (result.eval <= alpha) alpha -= delta;
                else if (result.eval >= beta) beta += delta;
                else break;

                delta *= cfg.search.ASPIRATION_RESEARCH_SCALE;
            }
        } else { 
            result = searcher->search(first_moves, count, depth, limits,
                                     last_result.best_line.line);
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
            //g_stats.max_completed_depth = depth;
            g_stats.it_depth_eval[depth] = result.eval;
            g_stats.it_depth_move[depth] = result.bestMove;
            g_stats.it_depth_time_ms[depth] = std::chrono::duration<double, std::milli>(depth_end - depth_start).count();
            //if (Move::SameMove(bestMove, prevBest)) g_stats.bestmoveStable++;
        }

        prevBest = bestMove;
        if (std::abs(result.eval) >= MATE_SCORE - 10) break;
        depth++;
    }

    // --- finalize cumulative stats ---
    if (Logging::track_search_stats || Logging::track_search_nodes) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start_time).count();
        g_stats.game_ply = ply;
        g_stats.time_ms = elapsed;
        //g_stats.nps = elapsed > 0 ? (1000.0 * g_stats.nodes / elapsed) : 0.0;
        g_stats.eval = last_result.eval;
        g_stats.move = last_result.bestMove;
        g_stats.principal_variation = last_result.best_line.line;

        //g_stats.ebf = pow(double(g_stats.nodes) / 1.0, 1.0 / g_stats.maxDepth); // rough estimate
        //g_stats.qratio = double(g_stats.qnodes) / g_stats.nodes;

        tracker.bestMoves.push_back(bestMove);
        tracker.evals.push_back(bestEval);

        logSearchStats(game_board.getBoardFEN());  // JSON written once at end
    }
    if (Logging::track_timers) {logTimingStats(game_board.getBoardFEN());}

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

    Logging::logUCIout(bestMove.uci());
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

    if (mode == EngineMode::GAME) {trackGame();}

    //resetSearchStats();
    Logging::logUCIout(best.uci());
}


// GAME LOGGING

void Engine::newGame() {
    mode = EngineMode::GAME;
    startNewSession();
    clearState();
    g_run_context.game_uuid = generate_uuid();
    g_run_context.is_new_game = true;
    g_game_start_time = std::chrono::steady_clock::now();

    tracker.playedMoves.clear();
    tracker.lastPositionHash = 0;
    tracker.active = true;

    if (Logging::log_dir == Logging::DEFAULT_LOG_DIR) Logging::setLogDir(Logging::project_root / "logs/game_logs");
    g_gamelog = GameLog{};
    g_gamelog.startFEN = game_board.getBoardFEN();


    game_board.setFromFEN(STARTPOS_FEN);
    search_board = game_board;
}

bool Engine::checkGameEnd() {
    if (g_gamelog.finalized) return true;

    if (isCheckmate()) {
        g_gamelog.outcome =
            !game_board.is_white_move ? GameResult::WHITE_WIN
                                     : GameResult::BLACK_WIN;
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
    else if (game_board.isThreefold()) {
        g_gamelog.outcome = GameResult::DRAW;
        g_gamelog.reason = GameEndReason::THREEFOLD;
    }
    else {
        return false; // game not over yet
    }

    // finalize
    g_game_end_time = std::chrono::steady_clock::now();
    g_gamelog.totalTimeSeconds = std::chrono::duration<double>(g_game_end_time - g_game_start_time).count();
    g_gamelog.moves = game_board.allGameMoves;
    g_gamelog.finalEval = nnue.full_eval(game_board);
    logGameLog();
    g_gamelog.finalized = true;

    //tt.clear();

    return true;
}


bool Engine::isCheckmate() {
    return !movegen->hasLegalMoves(game_board) && game_board.is_in_check;
}

bool Engine::isStalemate() {
    return !movegen->hasLegalMoves(game_board) && !game_board.is_in_check;
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
    g_gamelog.finalEval = nnue.full_eval(game_board);
    logGameLog();
}

// TESTING

uint64_t Engine::perft(int depth) {
    //ScopedTimer timer(T_PERFT);
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
    logTimingStats(game_board.getBoardFEN());
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
    std::cout << "Phase: " << phase << ") / 256\n";
    report.printDetailed(evalWeights);
}


void Engine::nnueEvalTest() {
    nnue.build_accumulators(search_board);
    int eval = nnue.evaluate(search_board.is_white_move);

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

