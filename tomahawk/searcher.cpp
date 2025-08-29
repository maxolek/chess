#include "searcher.h"

// ============================================================================
// SEARCHER: Negamax + Quiescence search framework
// ============================================================================

// --------------------
// Static member init
// --------------------
int Searcher::historyHeuristic[12][64] = {};
Move Searcher::killerMoves[MAX_DEPTH][2] = {};
int Searcher::nodesSearched = 0;
TranspositionTable Searcher::tt;
std::vector<Move> Searcher::best_line;
std::vector<Move> Searcher::best_quiescence_line;
int Searcher::quiesence_depth = 0;
int Searcher::max_q_depth = 0;

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

inline void Searcher::enterDepth() { quiesence_depth++; max_q_depth = std::max(max_q_depth, quiesence_depth); }
inline void Searcher::exitDepth()  { quiesence_depth--; }

void Searcher::updatePV(std::vector<Move>& pv, const Move& move, const std::vector<Move>& childPV) {
    pv.clear();
    pv.push_back(move);
    pv.insert(pv.end(), childPV.begin(), childPV.end());
}

bool Searcher::shouldPrune(Board& board, Move& move, Evaluator& evaluator, int standPat, int alpha) {
    int captured = board.getCapturedPiece(move.TargetSquare());

    if (captured != -1 && !move.IsPromotion() && !board.is_in_check) {
        int seeGain = evaluator.SEE(board, move);
        if (seeGain < -50) return true;
    }

    if (captured != -1 && !move.IsPromotion() && !board.is_in_check) {
        if (standPat + MAX_DELTA < alpha)
            return true;
    }

    return false;
}

// ============================================================================
// MOVE ORDERING
// ============================================================================

int Searcher::moveScore(const Evaluator& evaluator, const Move& move, const Board& board,
                        int ply, const Move& ttMove, std::vector<Move> previousPV) {
    constexpr int TT_BASE       = 10'000'000;
    constexpr int PV_BASE       = 9'000'000;
    constexpr int PROMO_BASE    = 8'500'000;
    constexpr int GOOD_CAP_BASE = 8'000'000;
    constexpr int KILLER_BASE   = 7'000'000;
    constexpr int QUIET_BASE    = 0;
    constexpr int BAD_CAP_BASE  = -1'000'000;

    if (Move::SameMove(ttMove, move)) return TT_BASE;
    if (ply < previousPV.size() && Move::SameMove(move, previousPV[ply])) return PV_BASE;

    if (move.IsPromotion()) {
        int promoBonus = 0;
        switch (move.PromotionPieceType()) {
            case queen:  promoBonus = 900; break;
            case knight: promoBonus = 300; break;
            case rook:   promoBonus = 100; break;
            case bishop: promoBonus = 100; break;
        }
        int captured = board.getCapturedPiece(move.TargetSquare());
        if (captured != -1) {
            int seeScore = evaluator.SEE(board, move);
            return seeScore >= 0 ? GOOD_CAP_BASE + seeScore + promoBonus
                                 : BAD_CAP_BASE  + seeScore + promoBonus;
        }
        return PROMO_BASE + promoBonus;
    }

    int captured = board.getCapturedPiece(move.TargetSquare());
    if (captured != -1) {
        int seeScore = evaluator.SEE(board, move);
        return seeScore >= 0 ? GOOD_CAP_BASE + seeScore : BAD_CAP_BASE + seeScore;
    }

    if (Move::SameMove(killerMoves[ply][0], move)) return KILLER_BASE;
    if (Move::SameMove(killerMoves[ply][1], move)) return KILLER_BASE - 1;

    int piece = board.getMovedPiece(move.StartSquare());
    int sidePiece = board.is_white_move ? piece : piece + 6;

    return QUIET_BASE + historyHeuristic[sidePiece][move.TargetSquare()] / 16;
}

void Searcher::orderedMoves(const Evaluator& evaluator, Move moves[MAX_MOVES], size_t count,
                            const Board& board, int ply, std::vector<Move> previousPV) {
    U64 hash = board.zobrist_hash;

    Move ttMove = Move::NullMove();
    TTEntry* entry = tt.probe(hash);
    if (entry && entry->key == hash && !Move::SameMove(Move::NullMove(), entry->bestMove)) {
        for (size_t i = 0; i < count; ++i)
            if (Move::SameMove(moves[i],entry->bestMove)) ttMove = entry->bestMove;
    }

    std::vector<std::pair<int, Move>> scored; scored.reserve(count);
    for (size_t i = 0; i < count; ++i)
        scored.emplace_back(moveScore(evaluator, moves[i], board, ply, ttMove, previousPV), moves[i]);

    std::sort(scored.begin(), scored.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    for (size_t i = 0; i < count; ++i) moves[i] = scored[i].second;
}

int Searcher::generateAndOrderMoves(Board& board, MoveGenerator& movegen, const Evaluator& evaluator,
                                    Move moves[MAX_MOVES], int ply, std::vector<Move> previousPV) {
    movegen.generateMoves(board, false);
    int count = std::min(MAX_MOVES, movegen.count);
    std::copy_n(movegen.moves, count, moves);
    orderedMoves(evaluator, moves, static_cast<size_t>(count), board, ply, previousPV);
    return count;
}

// ============================================================================
// QUIESCENCE SEARCH
// ============================================================================

int Searcher::quiescence(Board& board, Evaluator& evaluator, MoveGenerator& movegen,
                         int alpha, int beta, PV& pv,
                         SearchLimits& limits, int ply) {
    nodesSearched++;
    if (limits.out_of_time()) return alpha;

    if (board.hash_history[board.zobrist_hash] >= 3 || board.currentGameState.fiftyMoveCounter >= 50)
        return 0;

    int standPat = evaluator.taperedEval(board);
    if (standPat >= beta) return beta;
    if (standPat > alpha) alpha = standPat;

    STATS_QNODE(ply); // track quiescence node per depth (after regular eval returns)
    movegen.generateMoves(board, true);
    int count = movegen.count; Move moves[MAX_MOVES];
    if (count == 0) {
        if (board.is_in_check) { exitDepth(); return -MATE_SCORE + ply; }
        exitDepth();
        return standPat;
    }
    std::copy_n(movegen.moves, count, moves);

    int bestEval = standPat;

    for (int i = 0; i < count; i++) {
        if (limits.out_of_time()) break;

        Move m = moves[i];
        if (shouldPrune(board, m, evaluator, standPat, alpha)) continue;
        board.MakeMove(m);

        enterDepth();
        PV childPV;
        int score = -quiescence(board, evaluator, movegen, -beta, -alpha, childPV, limits, ply+1);
        exitDepth();

        board.UnmakeMove(m);

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

int Searcher::negamax(Board& board, MoveGenerator& movegen, Evaluator& evaluator,
                      int depth, int alpha, int beta, PV& pv, std::vector<Move> previousPV,
                      SearchLimits& limits, int ply, bool use_quiescence) {
    STATS_ENTER(ply); // track node per depth
    nodesSearched++;
    if (limits.out_of_time()) return alpha;

    if (board.hash_history[board.zobrist_hash] >= 3 || board.currentGameState.fiftyMoveCounter >= 50)
        return 0;

    if (depth == 0)
        return use_quiescence
            ? quiescence(board, evaluator, movegen, alpha, beta, pv, limits, ply)
            : evaluator.taperedEval(board);

    int alphaOrig = alpha;
    TTEntry* ttEntry = tt.probe(board.zobrist_hash);

    if (ttEntry && ttEntry->key == board.zobrist_hash && ttEntry->depth >= depth) {
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
    int count = generateAndOrderMoves(board, movegen, evaluator, moves, ply, previousPV);
    if (count == 0) return board.is_in_check ? -(MATE_SCORE - ply) : 0;

    int bestEval = -MATE_SCORE; 
    Move bestMove = Move::NullMove();

    for (int i = 0; i < count; i++) {
        if (limits.out_of_time()) break;

        Move m = moves[i];
        board.MakeMove(m);

        PV childPV;
        int score = -negamax(board, movegen, evaluator, depth - 1,
                              -beta, -alpha, childPV, previousPV,
                              limits, ply + 1, use_quiescence);

        board.UnmakeMove(m);

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
    if (bestEval <= alphaOrig) { flag = UPPERBOUND; STATS_FAILLOW(ply); }
    else if (bestEval >= beta) { flag = LOWERBOUND; }

    tt.store(board.zobrist_hash, depth, ply, bestEval, flag, bestMove);
    STATS_TT_STORE(depth);

    return bestEval;
}

// ============================================================================
// ROOT SEARCH
// ============================================================================

SearchResult Searcher::search(Board& board, MoveGenerator& movegen, Evaluator& evaluator,
                             Move legal_moves[MAX_MOVES], int count, int depth, 
                             SearchLimits& limits, std::vector<Move> previousPV) {
    nodesSearched = 0;
    SearchResult result = SearchResult();

    for (int i = 0; i < count; ++i) {
        if (limits.out_of_time()) break;
        Move m = legal_moves[i];
        if (Move::SameMove(m, Move::NullMove())) continue;

        board.MakeMove(m);
        PV childPV;
        int eval = -negamax(board, movegen, evaluator, depth - 1,
                            -MATE_SCORE, MATE_SCORE, childPV, previousPV,
                            limits, 0, true);
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
    }

    return result;
}

SearchResult Searcher::searchAspiration(Board& board, MoveGenerator& movegen, Evaluator& evaluator,
                                        Move legal_moves[MAX_MOVES], int count, int depth, 
                                        SearchLimits& limits, std::vector<Move> previousPV,
                                        int alpha, int beta) {
    nodesSearched = 0;
    SearchResult result = SearchResult();

    for (int i = 0; i < count; ++i) {
        if (limits.out_of_time()) break;
        Move m = legal_moves[i];
        if (Move::SameMove(m, Move::NullMove())) continue;

        board.MakeMove(m);
        PV childPV;

        STATS_ENTER(depth); // track nodes at this root depth
        int eval = -negamax(board, movegen, evaluator, depth - 1,
                            -beta, -alpha, childPV, previousPV,
                            limits, 0, true);

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
    }

    return result;
}

// ============================================================================
// DATA TRACKING
// ============================================================================

void Searcher::resetStats() {
    if (trackStats) stats = SearchStats();
}
