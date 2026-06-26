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
    int captured = board.getCapturedPiece(move.TargetSquare());

    // see pruning (bad captures ... excl promo and checks)
    if (captured != -1 && !move.IsPromotion() && !board.is_in_check) {
        int seeGain = eval.SEE(board, move);
        if (seeGain < -50) {
            STATS_SEE_PRUNE(search_depth, ply);
            return true;
        }
    }

    // delta purning (quiet moves that wont raise alpha anyway)
    if (captured != -1 && !move.IsPromotion() && !board.is_in_check) {
        if (standPat + engine.cfg.search.DELTA_PRUNE_THRESHOLD < alpha) {
            STATS_DELTA_PRUNE(search_depth, ply);
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

    if (Move::SameMove(ttMove, move)) return engine.cfg.scores.TT_BASE;
    if (ply < (int)previousPV.size() && Move::SameMove(move, previousPV[ply])) return engine.cfg.scores.PV_BASE;

    if (move.IsPromotion()) {
        int promoBonus = 0;
        switch (move.PromotionPieceType()) {
            case queen:  promoBonus = 900; break;
            case knight: promoBonus = 300; break;
            case rook:   promoBonus = 100; break;
            case bishop: promoBonus = 100; break;
        }
        int captured = boardRef.getCapturedPiece(move.TargetSquare());
        if (captured != -1) {
            int seeScore = eval.SEE(boardRef, move);
            return seeScore >= 0 ? engine.cfg.scores.GOOD_CAP_BASE + seeScore + promoBonus
                                 : engine.cfg.scores.BAD_CAP_BASE  + seeScore + promoBonus;
        }
        return engine.cfg.scores.PROMO_BASE + promoBonus;
    }

    int captured = boardRef.getCapturedPiece(move.TargetSquare());
    if (captured != -1) {
        int seeScore = eval.SEE(boardRef, move);
        return seeScore >= 0 ? engine.cfg.scores.GOOD_CAP_BASE + seeScore : engine.cfg.scores.BAD_CAP_BASE + seeScore;
    }

    if (Move::SameMove(killerMoves[ply][0], move)) return engine.cfg.scores.KILLER_BASE;
    if (Move::SameMove(killerMoves[ply][1], move)) return engine.cfg.scores.KILLER_BASE - 1;

    int piece = boardRef.getMovedPiece(move.StartSquare());
    int sidePiece = boardRef.is_white_move ? piece : piece + 6;

    return engine.cfg.scores.QUIET_BASE + historyHeuristic[sidePiece][move.TargetSquare()] / 16;
}

void Searcher::orderedMoves(Move moves[MAX_MOVES], size_t count,
                            const Board& boardRef, int ply, const std::vector<Move>& previousPV) {
    
    ScopedTimer timer(T_SCORE_ORDER);
    U64 hash = boardRef.zobrist_hash;

    Move ttMove = Move::NullMove();
    TTEntry* entry = engine.tt.probe(hash);
    if (entry && entry->key == hash && !Move::SameMove(Move::NullMove(), entry->bestMove)) {
        for (size_t i = 0; i < count; ++i)
            if (Move::SameMove(moves[i], entry->bestMove)) ttMove = entry->bestMove;
    }

    std::vector<std::pair<int, Move>> scored; scored.reserve(count);
    for (size_t i = 0; i < count; ++i)
        scored.emplace_back(moveScore(moves[i], boardRef, ply, ttMove, previousPV), moves[i]);

    std::sort(scored.begin(), scored.end(),
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
    ScopedTimer timer(T_QSEARCH);
    
    if (limits.out_of_time()) return alpha;

    // draw detection
    if (board.isThreefold() || board.currentGameState.fiftyMoveCounter >= 50) {
        //engine.tt.store(board.zobrist_hash, depth, ply, 0, EXACT, Move::NullMove());
        return 0;
    }


    // --- TT probe ---
    /*
    TTEntry* ttEntry = engine.tt.probe(board.zobrist_hash);
    if (ttEntry && ttEntry->key == board.zobrist_hash) {
        STATS_TT_HIT(search_depth, ply);
        if (ttEntry->flag == EXACT) return ttEntry->eval;
        else if (ttEntry->flag == UPPERBOUND && ttEntry->eval <= alpha) return alpha;
        else if (ttEntry->flag == LOWERBOUND && ttEntry->eval >= beta)  return beta;

        //if (alpha >= beta) return ttEntry->eval;
    }
    */

    // Use incremental NNUE output (accumulators must be kept in sync)
    //boardallGameMoves.back().PrintMove();
    int standPat = nnue.evaluate(board.is_white_move);
    if (standPat >= beta) return beta;
    if (standPat > alpha) alpha = standPat;

    //return standPat;

    STATS_QNODE(search_depth, ply); // macro unchanged

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
            if (i == 0) STATS_FAILHIGH(search_depth, ply, "first");
            else if (i >= count / 2) STATS_FAILHIGH(search_depth, ply, 0);
            else STATS_FAILHIGH(search_depth, ply, 0); 
            return beta; 
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
                      std::vector<Move>& previousPV, SearchLimits& limits, int ply, bool use_quiescence) {

    STATS_NODE(depth+ply, ply); // track node per depth
    ScopedTimer timer(T_SEARCH);
    if (limits.out_of_time()) return alpha;

    // --- end of search conditions ---

    if (board.isThreefold() || board.currentGameState.fiftyMoveCounter >= 50) {
        //engine.tt.store(board.zobrist_hash, depth, ply, 0, EXACT, Move::NullMove());
        return 0;
    }

    if (depth == 0) {
        return use_quiescence ? quiescence(alpha, beta, pv, limits, ply, depth, ply) : nnue.evaluate(board.is_white_move);
    }

    // --- TT probe ---

    int alphaOrig = alpha;
    TTEntry* ttEntry = engine.tt.probe(board.zobrist_hash);

    if (ttEntry && ttEntry->key == board.zobrist_hash && ttEntry->depth >= depth) {
        STATS_TT_HIT(depth+ply, ply);
        int ttScore = ttEntry->eval;

        if (ttEntry->flag == EXACT) return ttScore;
        else if (ttEntry->flag == UPPERBOUND && ttScore <= alpha) return alpha;
        else if (ttEntry->flag == LOWERBOUND && ttScore >= beta)  return beta;

        //if (alpha >= beta) return ttScore;
    }

    // -----------------------------
    // Null Move Pruning
    // -----------------------------
    // making a move is almost always better than passing
    // so we pass the move (null move) and perform a null-window search with reduced depth around beta
    // if the null move fails high then we can assume the best move will also fail high
    // certain position restrictions must be used for the assumptions to hold
    if ( // not in check, and not king-pawn endgame (zugzwang prevention)
        (depth - engine.cfg.search.R_NMP > 0)
        &&
        !(
            board.is_in_check 
            || board.pawn_endgame 
        )
    ) {
        STATS_NMP(depth+ply, ply);
        PV emptyPV;

        // null moves just change the side to move (and last-move cache)
        board.MakeNullMove();
        int null_score = -negamax(depth - 1 - engine.cfg.search.R_NMP, -beta, -(beta - 1), emptyPV, previousPV, limits, ply + 1, use_quiescence);
        board.UnmakeNullMove();

        // null window around beta so if if null move fails high 
        // then so should the real move with a full window search
        if (null_score >= beta) {
            STATS_NMP_FAIL(depth+ply, ply);
            return beta;
        }
    }

    // --- search ---

    Move moves[MAX_MOVES];
    int count = generateAndOrderMoves(moves, ply, previousPV);
    if (count == 0) return board.is_in_check ? -(MATE_SCORE - ply) : 0;

    int bestEval = -MATE_SCORE;
    Move bestMove = Move::NullMove();

    int _lmr_R = 0;

    for (int i = 0; i < count; i++) {
        if (limits.out_of_time()) break;

        Move m = moves[i];

        // current board state info
        bool in_check = board.is_in_check;
        bool is_pawn_endgame = board.pawn_endgame;
        bool was_capture = board.currentGameState.capturedPieceType != -1;

        // Apply NNUE/update & board
        U64 pre_hash = board.zobrist_hash;
        nnue.on_make_move(board, m);
        board.MakeMove(m);
        U64 mid_hash = board.zobrist_hash;

        int score; PV childPV;

        // -----------------------------
        // Late Move Reduction
        // -----------------------------
        // if a move is late in the move ordering and non-tactical then we can reduce its search depth
        // if the reduced search returns > alpha then we can re-search it with full depth
        // integration with PVS: use PVS+LMR concurrently
        //    if the move returns > alpha then do full-search (depth+window)
        if (
            // positional conditions apply
            board.currentGameState.capturedPieceType == -1
            && !m.IsPromotion()
            && !board.is_in_check
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
        if (!i) score = -negamax(depth - 1, -beta, -alpha, childPV, previousPV, limits, ply + 1, use_quiescence);
        else {
            score = -negamax(depth - 1 - _lmr_R, -(alpha+1), -alpha, childPV, previousPV, limits, ply + 1, use_quiescence);
            // fail-high --> re-search with full window
            // beta-alpha>1 is a redudancy check
            if (score > alpha && beta-alpha > 1) {
                STATS_PVS_RESEARCH(depth+ply, ply);
                score = -negamax(depth - 1, -beta, -alpha, childPV, previousPV, limits, ply + 1, use_quiescence);
            }
        }

        nnue.on_unmake_move(board, m);
        board.UnmakeMove(m);

        if (score > bestEval) {
            bestEval = score;
            bestMove = m;
            pv.set(m, childPV);
        }

        alpha = std::max(alpha, bestEval);
        if (alpha >= beta) {
            
            if (i == 0) STATS_FAILHIGH(depth+ply, ply, "first");
            else if (i >= count / 2) STATS_FAILHIGH(depth+ply, ply, 0);
            else STATS_FAILHIGH(depth+ply, ply, 0);

            if (!Move::SameMove(killerMoves[depth][0], m) && !m.IsPromotion() &&
                !(1ULL << m.TargetSquare() & board.colorBitboards[1 - board.move_color])) {
                killerMoves[depth][1] = killerMoves[depth][0];
                killerMoves[depth][0] = m;
            }
            int piece = board.getMovedPiece(m.StartSquare());
            historyHeuristic[board.is_white_move ? piece : piece + 6][m.TargetSquare()] += depth * depth;
            break;
        }
    }

    BoundType flag = EXACT;
    if (bestEval <= alphaOrig) { flag = UPPERBOUND; STATS_FAILLOW(depth+ply, ply); }
    else if (bestEval >= beta) { flag = LOWERBOUND; }

    if (flag == LOWERBOUND)
        engine.tt.store(board.zobrist_hash, depth, ply, beta, LOWERBOUND, Move::NullMove());
    else
        engine.tt.store(board.zobrist_hash, depth, ply, bestEval, flag, bestMove);
    //STATS_TT_STORE(depth+ply, ply);

    return bestEval;
}

// ============================================================================
// ROOT SEARCH
// ============================================================================

SearchResult Searcher::search(Move legal_moves[MAX_MOVES], int count, int depth, SearchLimits& limits, std::vector<Move>& previousPV) {
    SearchResult result;

    // Build NNUE accumulators for root position
    nnue.build_accumulators(board);

    for (int i = 0; i < count; ++i) {
        if (limits.out_of_time()) break;
        Move m = legal_moves[i];
        if (Move::SameMove(m, Move::NullMove())) continue;

        //nnue->debug_check_incr_vs_full_after_make(board, m, *nnue);
        nnue.on_make_move(board, m);
        board.MakeMove(m);

        //std::cout << "------" << std::endl; m.PrintMove(); std::cout << "------" << std::endl;

        int eval; PV childPV;
        int ply = 1;
        STATS_NODE(depth, ply);       // change to result.eval for proper alpha propogation
        eval = -negamax(depth - 1, -MATE_SCORE, MATE_SCORE, childPV, previousPV, limits, ply+1, true);

        // Undo
        //nnue->debug_check_incr_vs_full_after_unmake(board, m, *nnue);
        nnue.on_unmake_move(board, m);
        board.UnmakeMove(m);

        //std::cout << "after unmake - should be init position" << std::endl;
        //boardprint_board();
        //nnue->evaluate(boardis_white_move);
        //exit(0);

        if (limits.out_of_time() && i > 0) break;

        result.root_moves[i].move = m;
        result.root_moves[i].eval = eval;
        result.root_count++;

        if (i == 0 || eval > result.eval) {
            result.eval = eval;
            result.bestMove = m;
            result.best_line.set(m, childPV);
        }

        // ran through all root moves (= max_search_depth (-1 if last depth is terminated early))
        if (i >= count - 1) g_stats.max_completed_depth = depth;
    }

    //std::cout << "depth: " << depth << "\tbestmove: " << result.bestMove.uci() << "\teval: " << result.eval << std::endl;

    return result;
}

// same as ^^search as the only diff with aspirtation is the alpha_beta args
SearchResult Searcher::searchAspiration(Move legal_moves[MAX_MOVES], int count, int depth, SearchLimits& limits, std::vector<Move>& previousPV, int alpha, int beta) {
    // Simple wrapper using existing negamax with given alpha/beta window
    SearchResult result;

    // Build NNUE accumulators for root position
    nnue.build_accumulators(board);

    for (int i = 0; i < count; ++i) {
        if (limits.out_of_time()) break;
        Move m = legal_moves[i];
        if (Move::SameMove(m, Move::NullMove())) continue;

        nnue.on_make_move(board, m);
        board.MakeMove(m);

        int eval; PV childPV;
        int ply = 1;
        STATS_NODE(depth, ply);
        eval = -negamax(depth - 1, -beta, -alpha, childPV, previousPV, limits, ply+1, true);

        nnue.on_unmake_move(board, m);
        board.UnmakeMove(m);

        if (limits.out_of_time() && i > 0) break;

        result.root_moves[i].move = m;
        result.root_moves[i].eval = eval;
        result.root_count++;


        if (i == 0 || eval > result.eval) { 
            result.eval = eval;
            result.bestMove = m;
            result.best_line.set(m, childPV);
        }

        // ran through all root moves (= max_search_depth (-1 if last depth is terminated early))
        if (i >= count - 1) g_stats.max_completed_depth = depth;
    }

    return result;
}

// -----------------------
// make/undo move helpers
// -----------------------

void Searcher::do_move(const Move& move) {
    nnue.on_make_move(board, move);
    board.MakeMove(move);
}

void Searcher::undo_move(const Move& move, const Board& before) {
    // board currently contains the position AFTER the move, so Unmake it then call on_unmake_move
    nnue.on_unmake_move(board, move);
    board.UnmakeMove(move);
}

// -----------------------
// LMR reduction formula
// -----------------------
// log formula: constant + [log(depth) * log(move_order)] / denominator
int Searcher::R_lmr(int depth, int move_order) {
    if (move_order <= engine.cfg.search.LMR_MOVE_ORDER_THRESHOLD) return 0; // no reduction for first 3 moves
    if (depth <= engine.cfg.search.LMR_DEPTH_THRESHOLD) return 0;      // no reduction for shallow depths

    // logarithmic reduction formula
    float log_depth = std::log(static_cast<float>(depth));
    float log_order = std::log(static_cast<float>(move_order));
    // unlikely to exceed 4 without heavy parameter tuning (or very deep search)
    return static_cast<int>(engine.cfg.search.R_LMR_CONST + (log_depth * log_order) / engine.cfg.search.R_LMR_DENOM);;
}