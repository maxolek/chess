#include "searcher.h"
#include <limits>
#include <cstring>
#include <cassert>
#include <algorithm>

using namespace std;

static constexpr int INF = 30000;


// ============================================================================
// Construction / small helpers
// ============================================================================

void Searcher::enterDepth() { quiesence_depth++; max_q_depth = std::max(max_q_depth, quiesence_depth); }
void Searcher::exitDepth()  { if (quiesence_depth > 0) quiesence_depth--; }

void Searcher::updatePV(std::vector<Move>& pv, const Move& move, const std::vector<Move>& childPV) {
    pv.clear();
    pv.push_back(move);
    pv.insert(pv.end(), childPV.begin(), childPV.end());
}

bool Searcher::shouldPrune(Move& move, int standPat, int alpha) {
    // NOTE: uses board member
    int captured = board->getCapturedPiece(move.TargetSquare());

    // If capture exists and is bad according to SEE, prune (cheap heuristics)
    if (captured != -1 && !move.IsPromotion() && !board->is_in_check) {
        int seeGain = Evaluator::SEE(*board, move);
        if (seeGain < -50) return true;
    }

    if (captured != -1 && !move.IsPromotion() && !board->is_in_check) {
        if (standPat + MAX_DELTA < alpha)
            return true;
    }

    return false;
}

// ============================================================================
// MOVE ORDERING
// ============================================================================

int Searcher::moveScore(const Move& move, const Board& boardRef,
                        int ply, const Move& ttMove, const std::vector<Move>& previousPV) {
    constexpr int TT_BASE       = 10'000'000;
    constexpr int PV_BASE       = 9'000'000;
    constexpr int PROMO_BASE    = 8'500'000;
    constexpr int GOOD_CAP_BASE = 8'000'000;
    constexpr int KILLER_BASE   = 7'000'000;
    constexpr int QUIET_BASE    = 0;
    constexpr int BAD_CAP_BASE  = -1'000'000;

    if (Move::SameMove(ttMove, move)) return TT_BASE;
    if (ply < (int)previousPV.size() && Move::SameMove(move, previousPV[ply])) return PV_BASE;

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
            int seeScore = Evaluator::SEE(boardRef, move);
            return seeScore >= 0 ? GOOD_CAP_BASE + seeScore + promoBonus
                                 : BAD_CAP_BASE  + seeScore + promoBonus;
        }
        return PROMO_BASE + promoBonus;
    }

    int captured = boardRef.getCapturedPiece(move.TargetSquare());
    if (captured != -1) {
        int seeScore = Evaluator::SEE(boardRef, move);
        return seeScore >= 0 ? GOOD_CAP_BASE + seeScore : BAD_CAP_BASE + seeScore;
    }

    if (Move::SameMove(killerMoves[ply][0], move)) return KILLER_BASE;
    if (Move::SameMove(killerMoves[ply][1], move)) return KILLER_BASE - 1;

    int piece = boardRef.getMovedPiece(move.StartSquare());
    int sidePiece = boardRef.is_white_move ? piece : piece + 6;

    return QUIET_BASE + historyHeuristic[sidePiece][move.TargetSquare()] / 16;
}

void Searcher::orderedMoves(Move moves[MAX_MOVES], size_t count,
                            const Board& boardRef, int ply, const std::vector<Move>& previousPV) {
    U64 hash = boardRef.zobrist_hash;

    Move ttMove = Move::NullMove();
    TTEntry* entry = tt.probe(hash);
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
    // NOTE: adapt to your MoveGenerator API. This assumes movegen is a pointer member
    if (!movegen) return 0;
    movegen->generateMoves(*board, false); // the bool flag for captures-only or not — adapt if signature differs
    int count = std::min(MAX_MOVES, movegen->count);
    std::copy_n(movegen->moves, count, moves);
    orderedMoves(moves, static_cast<size_t>(count), *board, ply, previousPV);
    return count;
}

// ============================================================================
// QUIESCENCE SEARCH
// ============================================================================

int Searcher::quiescence(int alpha, int beta, PV& pv, SearchLimits& limits, int ply) {
    nodesSearched++;
    if (limits.out_of_time()) return alpha;

    if (board->hash_history[board->zobrist_hash] >= 3 || board->currentGameState.fiftyMoveCounter >= 50)
        return 0;

    // Use incremental NNUE output (accumulators must be kept in sync)
    //board->allGameMoves.back().PrintMove();
    int standPat = nnue->evaluate(board->is_white_move);
    if (standPat >= beta) return beta;
    if (standPat > alpha) alpha = standPat;

    //return standPat;

    STATS_QNODE(ply); // macro unchanged

    // generate only captures/promotions if your movegen supports that flag
    movegen->generateMoves(*board, true);
    int count = movegen->count;
    Move moves[MAX_MOVES];
    if (count == 0) {
        if (board->is_in_check) { exitDepth(); return -MATE_SCORE + ply; }
        exitDepth();
        return standPat;
    }
    std::copy_n(movegen->moves, count, moves);

    int bestEval = standPat;

    for (int i = 0; i < count; i++) {
        if (limits.out_of_time()) break;

        Move m = moves[i];
        if (shouldPrune(m, standPat, alpha)) continue;

        // Apply NNUE incremental update then board move
        nnue->on_make_move(*board, m);
        board->MakeMove(m);

        enterDepth();
        PV childPV;
        int score = -quiescence(-beta, -alpha, childPV, limits, ply+1);
        exitDepth();

        // Undo board & NNUE (capture before must be reconstructed from states)
        nnue->on_unmake_move(*board, m);
        board->UnmakeMove(m);

        if (limits.out_of_time()) break;

        if (score >= beta) { STATS_FAILHIGH(ply); return beta; }
        if (score > bestEval) {
            bestEval = score;
            alpha = std::max(alpha, score);
            pv.set(m, childPV);
        }
    }

    return bestEval;
}

// ============================================================================
// NEGAMAX SEARCH
// ============================================================================

int Searcher::negamax(int depth, int alpha, int beta, PV& pv,
                      std::vector<Move>& previousPV, SearchLimits& limits, int ply, bool use_quiescence) {
    STATS_ENTER(ply); // track node per depth
    nodesSearched++;
    if (limits.out_of_time()) return alpha;

    if (board->hash_history[board->zobrist_hash] >= 3 || board->currentGameState.fiftyMoveCounter >= 50)
        return 0;

    if (depth == 0) {
        return use_quiescence ? quiescence(alpha, beta, pv, limits, ply) : nnue->evaluate(board->is_white_move);
    }

    int alphaOrig = alpha;
    TTEntry* ttEntry = tt.probe(board->zobrist_hash);

    if (ttEntry && ttEntry->key == board->zobrist_hash && ttEntry->depth >= depth) {
        STATS_TT_HIT(ply);
        int ttScore = ttEntry->eval;
        if (ttScore > MATE_SCORE - 1000) ttScore -= ply;
        else if (ttScore < -MATE_SCORE + 1000) ttScore += ply;

        if (ttEntry->flag == EXACT) return ttScore;
        else if (ttEntry->flag == LOWERBOUND && ttScore > alpha) alpha = ttScore;
        else if (ttEntry->flag == UPPERBOUND && ttScore < beta)  beta = ttScore;

        if (alpha >= beta) return ttScore;
    }

    Move moves[MAX_MOVES];
    int count = generateAndOrderMoves(moves, ply, previousPV);
    if (count == 0) return board->is_in_check ? -(MATE_SCORE - ply) : 0;

    int bestEval = -MATE_SCORE;
    Move bestMove = Move::NullMove();

    for (int i = 0; i < count; i++) {
        if (limits.out_of_time()) break;

        Move m = moves[i];

        // Apply NNUE/update & board
        nnue->on_make_move(*board, m);
        board->MakeMove(m);

        PV childPV;
        int score = -negamax(depth - 1, -beta, -alpha, childPV, previousPV, limits, ply + 1, use_quiescence);

        // Undo
        nnue->on_unmake_move(*board, m);
        board->UnmakeMove(m);

        if (limits.out_of_time()) break;

        if (score > bestEval) {
            bestEval = score;
            bestMove = m;
            pv.set(m, childPV);
        }

        alpha = std::max(alpha, bestEval);
        if (alpha >= beta) {
            STATS_FAILHIGH(ply);
            if (i == 0) STATS_INC(fail_high_first);

            if (!Move::SameMove(killerMoves[depth][0], m) && !m.IsPromotion() &&
                !(1ULL << m.TargetSquare() & board->colorBitboards[1 - board->move_color])) {
                killerMoves[depth][1] = killerMoves[depth][0];
                killerMoves[depth][0] = m;
            }
            int piece = board->getMovedPiece(m.StartSquare());
            historyHeuristic[board->is_white_move ? piece : piece + 6][m.TargetSquare()] += depth * depth;
            break;
        }
    }

    BoundType flag = EXACT;
    if (bestEval <= alphaOrig) { flag = UPPERBOUND; STATS_FAILLOW(ply); }
    else if (bestEval >= beta) { flag = LOWERBOUND; }

    tt.store(board->zobrist_hash, depth, ply, bestEval, flag, bestMove);
    STATS_TT_STORE(depth);

    return bestEval;
}

// ============================================================================
// ROOT SEARCH
// ============================================================================

SearchResult Searcher::search(Move legal_moves[MAX_MOVES], int count, int depth, SearchLimits& limits, std::vector<Move>& previousPV) {
    nodesSearched = 0;
    SearchResult result;

    // Build NNUE accumulators for root position
    nnue->build_accumulators(*board);

    for (int i = 0; i < count; ++i) {
        if (limits.out_of_time()) break;
        Move m = legal_moves[i];
        if (Move::SameMove(m, Move::NullMove())) continue;

        //nnue->debug_check_incr_vs_full_after_make(*board, m, *nnue);
        nnue->on_make_move(*board, m);
        board->MakeMove(m);

        //std::cout << "------" << std::endl; m.PrintMove(); std::cout << "------" << std::endl;

        PV childPV;
        int eval = -negamax(depth - 1, -MATE_SCORE, MATE_SCORE, childPV, previousPV, limits, 0, true);

        // Undo
        //nnue->debug_check_incr_vs_full_after_unmake(*board, m, *nnue);
        nnue->on_unmake_move(*board, m);
        board->UnmakeMove(m);

        //std::cout << "after unmake - should be init position" << std::endl;
        //board->print_board();
        //nnue->evaluate(board->is_white_move);
        //exit(0);

        if (limits.out_of_time() && i > 0) break;

        result.root_moves[i].move = m;
        result.root_moves[i].eval = eval;
        result.root_count++;

        if (i == 0 || eval > result.eval) {
            result.eval = eval;
            result.bestMove = m;
            result.best_line.set(m, childPV);
            //std::cout << "depth: " << depth << "\n"; m.PrintMove(); std::cout<<eval<<std::endl;
        }
    }

    //std::cout << "depth: " << depth << "\tbestmove: " << result.bestMove.uci() << "\teval: " << result.eval << std::endl;

    return result;
}

SearchResult Searcher::searchAspiration(Move legal_moves[MAX_MOVES], int count, int depth, SearchLimits& limits, std::vector<Move>& previousPV, int alpha, int beta) {
    // Simple wrapper using existing negamax with given alpha/beta window
    nodesSearched = 0;
    SearchResult result;

    // Build NNUE accumulators for root position
    nnue->build_accumulators(*board);

    for (int i = 0; i < count; ++i) {
        if (limits.out_of_time()) break;
        Move m = legal_moves[i];
        if (Move::SameMove(m, Move::NullMove())) continue;

        nnue->on_make_move(*board, m);
        board->MakeMove(m);

        PV childPV;
        STATS_ENTER(depth);
        int eval = -negamax(depth - 1, -beta, -alpha, childPV, previousPV, limits, 0, true);

        nnue->on_unmake_move(*board, m);
        board->UnmakeMove(m);

        if (limits.out_of_time() && i > 0) break;

        result.root_moves[i].move = m;
        result.root_moves[i].eval = eval;
        result.root_count++;

        if (i == 0 || eval > result.eval) {
            result.eval = eval;
            result.bestMove = m;
            result.best_line.set(m, childPV);
        }
    }

    return result;
}

// ============================================================================
// DATA TRACKING
// ============================================================================

void Searcher::resetStats() {
    if (trackStats) stats = SearchStats();
}

// -----------------------
// make/undo move helpers
// -----------------------

void Searcher::do_move(const Move& move) {
    nnue->on_make_move(*board, move);
    board->MakeMove(move);
}

void Searcher::undo_move(const Move& move, const Board& before) {
    // board currently contains the position AFTER the move, so Unmake it then call on_unmake_move
    nnue->on_unmake_move(*board, move);
    board->UnmakeMove(move);
}
