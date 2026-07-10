#include <searcher.h>
#include <engine.h>
#include <limits>
#include <cstring>
#include <cassert>
#include <algorithm>
#include <iostream>

// ============================================================================
// Construction / small helpers
// ============================================================================

void Searcher::updatePV(std::vector<Move>& pv, const Move& move, const std::vector<Move>& childPV) {
    pv.clear();
    pv.push_back(move);
    pv.insert(pv.end(), childPV.begin(), childPV.end());
}

// leaf node pruning
bool Searcher::shouldPrune(Move& move, int standPat, int alpha, int search_depth, int ply) {
    const int captured = board.getCapturedPiece(move.TargetSquare());
    const bool is_promo = move.IsPromotion();
    const bool in_check = board.is_in_check;

    
    if (captured != -1 
        && !is_promo
        && !in_check
    ) {
        // see pruning (bad captures ... excl promo and checks)
        int seeGain = eval.SEE(board, move);
        if (seeGain < params.SEE_PRUNE_THRESHOLD) {
            #ifdef DEV
                STATS_SEE_PRUNE(search_depth, ply);
            #endif
            return true;
        }

        // delta purning (quiet moves that wont raise alpha anyway)
        if (standPat + params.DELTA_PRUNE_THRESHOLD < alpha) {
            #ifdef DEV
                STATS_DELTA_PRUNE(search_depth, ply);
            #endif
            return true;
        }
    }

    return false;
}

// ============================================================================
// MOVE ORDERING
// ============================================================================

int Searcher::moveScore(const Move& move, const Board& boardRef,
                        int ply, const Move& ttMove, const std::vector<Move>& previousPV) {
    int seeScore;

    // TT + PV
    if (Move::SameMove(ttMove, move)) return move_scores.TT_BASE;
    if (ply < (int)previousPV.size() && Move::SameMove(move, previousPV[ply])) return move_scores.PV_BASE;

    // promotions
    if (move.IsPromotion()) {
        int promoBonus = 0;
        switch (move.PromotionPieceType()) {
            case queen:  promoBonus = 900; break;
            case knight: promoBonus = 300; break;
            case rook:   promoBonus = 100; break;
            case bishop: promoBonus = 100; break;
        }
        int captured = boardRef.getCapturedPiece(move.TargetSquare());
        seeScore = 0;
        if (captured != -1) {
            seeScore = eval.SEE(boardRef, move);
            return seeScore >= 0 ? move_scores.GOOD_CAP_BASE + seeScore + promoBonus
                                 : move_scores.BAD_CAP_BASE  + seeScore + promoBonus;
        }
        return move_scores.PROMO_BASE + promoBonus + seeScore;
    }

    // captures (scored via SEE or MVVLVA)
    int captured = boardRef.getCapturedPiece(move.TargetSquare());
    if (captured != -1) {
        // SEE
        seeScore = eval.SEE(boardRef, move);
        return seeScore >= 0 ? move_scores.GOOD_CAP_BASE + seeScore : move_scores.BAD_CAP_BASE + seeScore;
        // MVVLVA
        //int attacker = board.getMovedPiece(move.StartSquare());
        //int victim = board.getMovedPiece(move.TargetSquare());
        //return 100000 + 10 * victim - attacker;
    }

    // killer moves
    if (Move::SameMove(killerMoves[ply][0], move)) return move_scores.KILLER_BASE;
    if (Move::SameMove(killerMoves[ply][1], move)) return move_scores.KILLER_BASE - 1;

    // quiet moves (history heuristic)
    int piece = boardRef.getMovedPiece(move.StartSquare());
    int sidePiece = boardRef.is_white_move ? piece : piece + 6;

    return move_scores.QUIET_BASE + historyHeuristic[sidePiece][move.TargetSquare()] / 16;
}

void Searcher::orderedMoves(Move moves[MAX_MOVES], size_t count,
                            const Board& boardRef, int ply, const std::vector<Move>& previousPV) {
    
    #ifdef DEV
        ScopedTimer timer(T_SCORE_ORDER);
    #endif
    U64 hash = boardRef.zobrist_hash;

    // TT move choice
    Move ttMove = Move::NullMove();
    TTEntry* entry = engine.tt.probe(hash);
    if (entry && entry->key == hash && !Move::SameMove(Move::NullMove(), entry->bestMove)) {
        for (size_t i = 0; i < count; ++i)
            if (Move::SameMove(moves[i], entry->bestMove)) ttMove = entry->bestMove;
    }

    // sorting
    std::pair<int, Move> scored[MAX_MOVES];
    for (size_t i = 0; i < count; ++i)
        scored[i] = {moveScore(moves[i], boardRef, ply, ttMove, previousPV), moves[i]};

    std::sort(scored, scored + count,
              [](const auto& a, const auto& b) { return a.first > b.first; });

    for (size_t i = 0; i < count; ++i) moves[i] = scored[i].second;
}

int Searcher::generateAndOrderMoves(Move moves[MAX_MOVES], int ply, const std::vector<Move>& previousPV) {
   if (!engine.movegen) return 0;

    int count = engine.movegen->generateMoves(board, false); // the bool flag for captures-only or not — adapt if signature differs
    std::copy_n(engine.movegen->moves, count, moves);

    orderedMoves(moves, static_cast<size_t>(count), board, ply, previousPV);
    
    return count;
}

// ============================================================================
// QUIESCENCE SEARCH
// ============================================================================

int Searcher::quiescence(int alpha, int beta, PV& pv, SearchLimits& limits, int ply, int depth, int search_depth) {
    if (limits.out_of_time()) return alpha;

    // draw detection
    if (board.isThreefold() || board.currentGameState.fiftyMoveCounter >= 50) {
        //engine.tt.store(board.zobrist_hash, depth, ply, 0, EXACT, Move::NullMove());
        return params.DRAW_EVAL;
    }


    // --- TT probe ---
    /*
    TTEntry* ttEntry = engine.tt.probe(board.zobrist_hash);
    if (ttEntry && ttEntry->key == board.zobrist_hash) {
        STATS_TT_HIT(search_depth, ply);
        int ttScore = ttEntry->eval;

        if (ttEntry->flag == EXACT) return ttEntry->eval;
        else if (ttEntry->flag == UPPERBOUND && ttEntry->eval <= alpha) return ttScore;
        else if (ttEntry->flag == LOWERBOUND && ttEntry->eval >= beta)  return ttScore;

        //if (alpha >= beta) return ttEntry->eval;
    }
    */

    // Use incremental NNUE output (accumulators must be kept in sync)
    //boardallGameMoves.back().PrintMove();
    int standPat = nnue.evaluate(board.is_white_move); //eval.taperedEval(board);
    if (standPat >= beta) return standPat;
    if (standPat > alpha) alpha = standPat;

    //return standPat;

    #ifdef DEV
        STATS_QNODE(search_depth, ply); 
    #else 
        g_stats.nodes++;
    #endif

    // generate only captures/promotions 
    int count = engine.movegen->generateMoves(board, true);
    Move moves[MAX_MOVES];
    if (count == 0) {
        if (board.is_in_check) { return -MATE_SCORE + ply; }
        return standPat;
    }
    std::copy_n(engine.movegen->moves, count, moves);

    int bestEval = standPat;
    Move bestMove = Move::NullMove();

    for (int i = 0; i < count; i++) {
        if (limits.out_of_time()) break;

        Move m = moves[i];
        if (shouldPrune(m, standPat, alpha, search_depth, ply)) continue;

        // Apply NNUE incremental update then board move
        nnue.on_make_move(board, m);
        board.MakeMove(m);

        int score; PV childPV;
        score = -quiescence(-beta, -alpha, childPV, limits, ply+1, depth, search_depth);

        // Undo board & NNUE (capture before must be reconstructed from states)
        nnue.on_unmake_move(board, m);
        board.UnmakeMove(m);

        if (score >= beta) { 
            #ifdef DEV
                STATS_FAILHIGH(search_depth, ply, i);
            #endif
            return score; 
        }
        if (score > bestEval) {
            bestEval = score;
            bestMove = m;
            alpha = std::max(alpha, score);
            pv.set(m, childPV);
        }
    }

    // store best capture or draw
    //engine.tt.store(board.zobrist_hash, depth, ply, bestEval, EXACT, bestMove);
    //STATS_TT_STORE(search_depth, search_depth);

    return bestEval;
}

// ============================================================================
// NEGAMAX SEARCH
// ============================================================================

int Searcher::negamax(int depth, int alpha, int beta, PV& pv,
                      std::vector<Move>& previousPV, SearchLimits& limits, int ply) {

    #ifdef DEV
        STATS_NODE(depth+ply, ply); // track node per depth
    #else 
        g_stats.nodes++;
    #endif 
    if (limits.out_of_time()) return alpha;

    // --- end of search conditions ---

    if (board.isThreefold() || board.currentGameState.fiftyMoveCounter >= 50) {
        //engine.tt.store(board.zobrist_hash, depth, ply, 0, EXACT, Move::NullMove());
        return params.DRAW_EVAL;
    }

    if (depth == 0) {
        return quiescence(alpha, beta, pv, limits, ply, depth, ply);
    }

    // --- TT probe ---

    int alphaOrig = alpha;
    TTEntry* ttEntry = engine.tt.probe(board.zobrist_hash);

    if (ttEntry && ttEntry->key == board.zobrist_hash && ttEntry->depth >= depth) {
        #ifdef DEV
            STATS_TT_HIT(depth+ply, ply);
        #endif
        int ttScore = ttEntry->eval;

        if (ttEntry->flag == EXACT) return ttScore;
        else if (ttEntry->flag == UPPERBOUND && ttScore <= alpha) return ttScore;
        else if (ttEntry->flag == LOWERBOUND && ttScore >= beta)  return ttScore;
    }

    // -----------------------------
    // Null Move Pruning
    // -----------------------------
    // making a move is almost always better than passing
    // so we pass the move (null move) and perform a null-window search with reduced depth around beta
    // if the null move fails high then we can assume the best move will also fail high
    // certain position restrictions must be used for the assumptions to hold
    /*/
    if ( // not in check, and not king-pawn endgame (zugzwang prevention)
        (depth - params.R_NMP > 0)
        &&
        !(
            board.is_in_check 
            || board.pawn_endgame 
        )
    ) {
        ScopedTimer timer(T_NMP_SEARCH);
        STATS_NMP(depth+ply, ply);
        PV emptyPV;

        // null moves just change the side to move (and last-move cache)
        board.MakeNullMove();
        int null_score = -negamax(depth - 1 - params.R_NMP, -beta, -(beta - 1), emptyPV, previousPV, limits, ply + 1, use_quiescence);
        board.UnmakeNullMove();

        // null window around beta so if if null move fails high 
        // then so should the real move with a full window search
        if (null_score >= beta) {
            STATS_NMP_FAIL(depth+ply, ply);
            return beta;
        }
    }
    */

    // --- search ---

    Move moves[MAX_MOVES];
    int count = generateAndOrderMoves(moves, ply, previousPV);
    if (count == 0) return board.is_in_check ? -(MATE_SCORE - ply) : 0;

    int bestEval = -MATE_SCORE;
    Move bestMove = Move::NullMove();

    int _lmr_R = 0;

    bool in_check, is_pawn_endgame, was_capture, is_capture;
    Move m; int score; PV childPV;

    // --- move loop ---

    for (int i = 0; i < count; i++) {
        if (limits.out_of_time()) break;

        m = moves[i];

        // current board state info
        in_check = board.is_in_check;
        is_pawn_endgame = board.pawn_endgame;
        was_capture = board.currentGameState.capturedPieceType != -1;
        is_capture = board.getCapturedPiece(m.TargetSquare()) != -1;

        // Apply NNUE/update & board
        nnue.on_make_move(board, m);
        board.MakeMove(m);
        
        score = 0; childPV = {};

        // -----------------------------
        // Late Move Reduction
        // -----------------------------
        // if a move is late in the move ordering and non-tactical then we can reduce its search depth
        // if the reduced search returns > alpha then we can re-search it with full depth
        // integration with PVS: use PVS+LMR concurrently
        //    if the move returns > alpha then do full-search (depth+window)
        if (
            // positional conditions apply
            !is_capture
            && !m.IsPromotion()
            && !in_check
        ) {
            // obsidian log formula
            _lmr_R = R_lmr(depth, i);
        } else {
            _lmr_R = 0;
        }

        // -----------------------------
        // principal variation search 
        // -----------------------------
        // run a null-window search around alpha
        // after generating a PV move (and thus alpha=eval) from a full-window search
        // if a move fails high then we can re-search it with a full window 
        // else it fails low and is not going to be a better move than what has been found
        // null window searches are cheap and so the re-searches are worth the speedup
        //if (!i) { // first move (full window)
        score = -negamax(depth - 1 - _lmr_R, -beta, -alpha, childPV, previousPV, limits, ply + 1);
        if (_lmr_R > 0 && score > alpha) { // research at full depth if move raises alpha
            childPV = {};   // don't let the reduced-search line leak into the full-depth result
            score = -negamax(depth - 1, -beta, -alpha, childPV, previousPV, limits, ply+1);
        }
            //if (limits.out_of_time()) break;   // score may be garbage, discard and stop this node
        //} else { // other moves (null window around alpha + full researches if suspected >alpha)
        //    {
            //ScopedTimer timer(T_PVS_SEARCH);
            //score = -negamax(depth - 1 - _lmr_R, -(alpha+1), -alpha, childPV, previousPV, limits, ply + 1);
            //if (limits.out_of_time()) break;   // score may be garbage, discard and stop this node
        //    }
            // fail-high --> re-search with full window (can raise alpha)
            // beta-alpha>1 is a redudancy check
            //if (score > alpha && beta-alpha > 1) {
                //ScopedTimer timer(T_PVS_RESEARCH);
            //    STATS_PVS_RESEARCH(depth+ply, ply);
            //    score = -negamax(depth - 1, -beta, -alpha, childPV, previousPV, limits, ply + 1);
            //    if (limits.out_of_time()) break;   // score may be garbage, discard and stop this node
            //}
        //}

        nnue.on_unmake_move(board, m);
        board.UnmakeMove(m);

        if (score > bestEval) {
            bestEval = score;
            bestMove = m;
            pv.set(m, childPV);
        }

        alpha = std::max(alpha, bestEval);
        if (alpha >= beta) {
            #ifdef DEV
                STATS_FAILHIGH(depth+ply, ply, i);
            #endif

            if (!Move::SameMove(killerMoves[ply][0], m) && !m.IsPromotion() &&
                !(1ULL << m.TargetSquare() & board.colorBitboards[1 - board.move_color])) {
                killerMoves[ply][1] = killerMoves[ply][0];
                killerMoves[ply][0] = m;
            }
            int piece = board.getMovedPiece(m.StartSquare());
            historyHeuristic[board.is_white_move ? piece : piece + 6][m.TargetSquare()] += depth * depth;
            break;
        }
    }

    // --- tt-store ---

    BoundType flag = EXACT;
    if (bestEval <= alphaOrig) { 
        flag = UPPERBOUND; 
        #ifdef DEV
            STATS_FAILLOW(depth+ply, ply); 
        #endif
    }
    else if (bestEval >= beta) { 
        flag = LOWERBOUND; 
    }
    engine.tt.store(board.zobrist_hash, depth, ply, bestEval, flag, bestMove);
    //STATS_TT_STORE(depth+ply, ply);

    return bestEval;
}

// ============================================================================
// ROOT SEARCH
// ============================================================================

SearchResult Searcher::search(Move legal_moves[MAX_MOVES], int count, int depth, SearchLimits& limits, std::vector<Move>& previousPV, int previousEval) {
    
    int eval;
    int ply = 0; // root moves are depth=0, ply+1 in arg call makes made moves depth=1
    SearchResult result;
    
    int delta = params.ASPIRATION_WINDOW;
    int alpha = -MATE_SCORE;
    int beta = MATE_SCORE;
    
    // aspiration search .. window generation
    if (depth >= params.ASPIRATION_START_DEPTH) {
        delta = std::max(delta, params.ASPIRATION_WINDOW + depth * params.ASPIRATION_DEPTH_SCALE);
        alpha = previousEval - delta;
        beta  = previousEval + delta;
    }

    // root move loop for both aspiration + regular search
    while (true) {
        SearchResult iter_result;
        // Build NNUE accumulators for root position
        nnue.build_accumulators(board);

        // explore (our) root moves
        int i = 0; // for stats check later
        for (i; i < count; ++i) {
            if (limits.out_of_time()) break;
            Move m = legal_moves[i];
            if (Move::SameMove(m, Move::NullMove())) continue;

            // root move stat tracking
            #ifdef DEV
                uint64_t nodes_before = g_stats.nodes;
                auto move_start = std::chrono::steady_clock::now();
            #endif

            nnue.on_make_move(board, m);
            board.MakeMove(m);

            #ifdef DEV
                STATS_NODE(depth, ply);     
            #endif  
            PV childPV; // must be declared per root_move

            // --- PVS ---
            //if (!i) { // first move (full window)
                eval = -negamax(depth - 1, -beta, -alpha, childPV, previousPV, limits, ply+1);
            //} else {
            //    {
            //    ScopedTimer timer(T_PVS_ROOT_SEARCH);
                //eval = -negamax(depth - 1, -(alpha+1), -alpha, childPV, previousPV, limits, ply+1);
            //    }
                // fail-high --> re-search with full window (can raise alpha)
                // beta-alpha>1 is a redudancy check
            //    if (eval > alpha && beta-alpha > 1) {
            //        ScopedTimer timer(T_PVS_ROOT_RESEARCH);
            //        STATS_PVS_RESEARCH(depth+ply, ply);
            //        eval = -negamax(depth - 1, -beta, -alpha, childPV, previousPV, limits, ply+1);
            //    }
            //}


            nnue.on_unmake_move(board, m);
            board.UnmakeMove(m);

            #ifdef DEV
            if (Logging::track_timers) {
                auto move_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - move_start).count();
                int64_t move_nodes = g_stats.nodes - nodes_before;
                iter_result.root_moves[i].time_ms = move_ms;
                iter_result.root_moves[i].nodes = move_nodes;
            }
            #endif

            // time out is propagated up the tree, so eval and move cannot be trusted
            if (limits.out_of_time() && i > 0) break;

            iter_result.root_moves[i].move = m;
            iter_result.root_moves[i].eval = eval;
            iter_result.root_count++;

            if (i == 0 || eval > iter_result.eval) {
                iter_result.eval = eval;
                iter_result.bestMove = m;
                iter_result.best_line.set(m, childPV); // propagate raised alpha across root moves (in non-aspiration search)
                //if (depth < params.ASPIRATION_START_DEPTH) alpha = std::max(alpha, eval); 
            }
        }

        // aspiration check
        if (!limits.should_stop(depth) && (depth >= params.ASPIRATION_START_DEPTH)) {
            if (iter_result.eval <= alpha) {
                #ifdef DEV
                    if (Logging::track_search_stats) STATS_ASPIRATION_FAILLOW(depth);
                #endif
                alpha -= delta;
                delta *= params.ASPIRATION_RESEARCH_SCALE;
                continue;
            } else if (iter_result.eval >= beta) {
                #ifdef DEV
                    if (Logging::track_search_stats) STATS_ASPIRATION_FAILHIGH(depth);
                #endif
                beta += delta;
                delta *= params.ASPIRATION_RESEARCH_SCALE;
                continue;
            }
        } 

        // ran through all root moves 
        // (= max_search_depth (-1 if last depth is terminated early))
        if (i >= count - 1) g_stats.max_completed_depth = depth;  
        result = iter_result;
        break;
    }

    return result;
}

// -----------------------
// LMR reduction formula
// -----------------------
// log formula: constant + [log(depth) * log(move_order)] / denominator
int Searcher::R_lmr(int depth, int move_order) {
    if (move_order <= params.LMR_MOVE_ORDER_THRESHOLD) return 0; // no reduction for first 3 moves
    if (depth <= params.LMR_DEPTH_THRESHOLD) return 0;      // no reduction for shallow depths

    // logarithmic reduction formula
    float log_depth = std::log(static_cast<float>(depth));
    float log_order = std::log(static_cast<float>(move_order));
    // unlikely to exceed 4 without heavy parameter tuning (or very deep search)
    int reduction = static_cast<int>(params.R_LMR_CONST + (log_depth * log_order) / params.R_LMR_DENOM);
    
    return std::min(reduction, depth-2);
}