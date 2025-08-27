#include "searcher.h"
#include <algorithm>
#include <chrono>

// ============================================================================
// SEARCHER: Negamax + Quiescence search framework
// ----------------------------------------------------------------------------
// This file implements the core search functionality of the engine. It uses a
// negamax framework with alpha-beta pruning, transposition tables, killer move
// heuristics, history heuristics, and a quiescence search for tactical stability.
// ============================================================================

// --------------------
// Static member init
// --------------------
int Searcher::historyHeuristic[12][64] = {};   // history heuristic bonus table
Move Searcher::killerMoves[MAX_DEPTH][2] = {}; // two killer moves per ply
int Searcher::nodesSearched = 0;               // global search node counter
TranspositionTable Searcher::tt; // transposition table
std::vector<Move> Searcher::best_line;         // principal variation from main search
std::vector<Move> Searcher::best_quiescence_line; // best PV from quiescence
int Searcher::quiesence_depth = 0;             // current recursion depth in quiescence
int Searcher::max_q_depth = 0;                 // max depth reached in quiescence

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Track quiescence recursion depth for debugging
inline void Searcher::enterDepth() { quiesence_depth++; max_q_depth = std::max(max_q_depth, quiesence_depth); }
inline void Searcher::exitDepth()  { quiesence_depth--; }

// Replace PV with new move + childPV
void Searcher::updatePV(std::vector<Move>& pv, const Move& move, const std::vector<Move>& childPV) {
    pv.clear();
    pv.push_back(move);
    pv.insert(pv.end(), childPV.begin(), childPV.end());
}

// various pruning
// - Avoids searching obviously losing captures
// - avoids searching moves that cannot be better than known best move
bool Searcher::shouldPrune(Board& board, Move& move, Evaluator& evaluator, int standPat, int alpha) {
    int captured = board.getCapturedPiece(move.TargetSquare());

    // SEE pruning: obviously losing capture
    if (captured != -1 && !move.IsPromotion() && !board.is_in_check) {
        int seeGain = evaluator.SEE(board, move);
        if (seeGain < -50) return true;
    }

    // Delta pruning: ignore quiet captures that can't raise alpha
    if (captured != -1 && !move.IsPromotion() && !board.is_in_check) {
        if (standPat + MAX_DELTA < alpha)
            return true;
    }

    return false;
}

// ============================================================================
// MOVE ORDERING
// ============================================================================

// Assign a heuristic score to each move
// - previous iteration PV move highest priority
// - TT move is strong candidate
// - Captures scored by SEE
// - Promotions weighted by piece type
// - Killer moves preferred if they caused cutoffs earlier
// - Quiet moves fall back to history heuristic
int Searcher::moveScore(const Evaluator& evaluator, const Move& move, const Board& board,
                        int ply, const Move& ttMove, std::vector<Move> previousPV) {
    constexpr int TT_BASE       = 10'000'000;
    constexpr int PV_BASE       = 9'000'000;
    constexpr int PROMO_BASE    = 8'500'000;
    constexpr int GOOD_CAP_BASE = 8'000'000;
    constexpr int KILLER_BASE   = 7'000'000;
    constexpr int QUIET_BASE    = 0;
    constexpr int BAD_CAP_BASE  = -1'000'000;

    // TT + PV
    if (Move::SameMove(ttMove, move)) return TT_BASE;
    if (ply < previousPV.size() && Move::SameMove(move, previousPV[ply])) return PV_BASE;

    // Promotions (scaled by piece value)
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

    // Captures (scored via SEE)
    int captured = board.getCapturedPiece(move.TargetSquare());
    if (captured != -1) {
        int seeScore = evaluator.SEE(board, move);
        return seeScore >= 0 ? GOOD_CAP_BASE + seeScore : BAD_CAP_BASE + seeScore;
    }

    // Killer moves
    if (Move::SameMove(killerMoves[ply][0], move)) return KILLER_BASE;
    if (Move::SameMove(killerMoves[ply][1], move)) return KILLER_BASE - 1;

    // Quiet moves (history heuristic)
    int piece = board.getMovedPiece(move.StartSquare());
    int sidePiece = board.is_white_move ? piece : piece + 6;

    return QUIET_BASE + historyHeuristic[sidePiece][move.TargetSquare()] / 16;
}

// Score + sort moves in descending order
void Searcher::orderedMoves(const Evaluator& evaluator, Move moves[MAX_MOVES], size_t count,
                            const Board& board, int ply, std::vector<Move> previousPV) {
    U64 hash = board.zobrist_hash;

    // transposition table
    Move ttMove = Move::NullMove();
    TTEntry* entry = tt.probe(hash);
    if (entry && entry->key == hash && !Move::SameMove(Move::NullMove(), entry->bestMove)) {
         // Verify TT move is legal in this node
        for (size_t i = 0; i < count; ++i) {
            if (Move::SameMove(moves[i],entry->bestMove)) {
                ttMove = entry->bestMove;
                break;
            }
        }
    }

    std::vector<std::pair<int, Move>> scored;
    scored.reserve(count);

    // score
    for (size_t i = 0; i < count; ++i) {
        scored.emplace_back(moveScore(evaluator, moves[i], board, ply, ttMove, previousPV), moves[i]);
    }

    // sort
    std::sort(scored.begin(), scored.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    for (size_t i = 0; i < count; ++i) {
        moves[i] = scored[i].second;
    }
}

// Generate full move list, then order it
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
// ----------------------------------------------------------------------------
// Extends search in leaf positions where tactics matter (captures, checks, promos)
// Prevents horizon effect by searching only "noisy" moves
// ============================================================================

int Searcher::quiescence(Board& board, Evaluator& evaluator, MoveGenerator& movegen,
                         int alpha, int beta, PV& pv,
                         SearchLimits& limits, int ply) {
    nodesSearched++;
    if (limits.out_of_time()) return alpha;

    // Threefold repetition || 50-move rule
    if (board.hash_history[board.zobrist_hash] >= 3 || board.currentGameState.fiftyMoveCounter >= 50) {
        return 0;
    }

    int standPat = evaluator.taperedEval(board);
    if (standPat >= beta) return beta;
    if (standPat > alpha) alpha = standPat;

    movegen.generateMoves(board, true); // quiescence moves only
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

        if (score >= beta) return beta;
        if (score > bestEval) {
            bestEval = score;
            alpha = std::max(alpha, score);
            pv.set(m, childPV);
        }
    }

    return bestEval;
}


// ============================================================================
// NEGAMAX SEARCH (with alpha-beta pruning)
// ============================================================================

int Searcher::negamax(Board& board, MoveGenerator& movegen, Evaluator& evaluator,
                      int depth, int alpha, int beta, PV& pv, std::vector<Move> previousPV,
                      SearchLimits& limits, int ply, bool use_quiescence) {
    nodesSearched++;
    if (limits.out_of_time()) return alpha;

    // Threefold repetition || 50-move rule
    if (board.hash_history[board.zobrist_hash] >= 3 || board.currentGameState.fiftyMoveCounter >= 50) {
        return 0;
    }

    // Base case: depth = 0
    if (depth == 0) {
        return use_quiescence
            ? quiescence(board, evaluator, movegen, alpha, beta, pv, limits, ply)
            : evaluator.taperedEval(board);
    }

    // === Transposition table probe ===
    int alphaOrig = alpha;
    TTEntry* ttEntry = tt.probe(board.zobrist_hash);

    // require TT_depth > curr_depth  so that we only use TT with more info than current search would give
    if (ttEntry && ttEntry->key == board.zobrist_hash && ttEntry->depth >= depth) {
        int ttScore = ttEntry->eval;

        // Adjust mate scores with ply to get correct distance-to-mate
        if (ttScore > MATE_SCORE - 1000) ttScore -= ply;
        else if (ttScore < -MATE_SCORE + 1000) ttScore += ply;

        if (ttEntry->flag == EXACT) return ttScore;
        else if (ttEntry->flag == LOWERBOUND && ttScore > alpha) alpha = ttScore;
        else if (ttEntry->flag == UPPERBOUND && ttScore < beta)  beta = ttScore;

        if (alpha >= beta) return ttScore;
    }

    // Generate and order moves
    Move moves[MAX_MOVES]; 
    int count = generateAndOrderMoves(board, movegen, evaluator, moves, ply, previousPV);
    if (count == 0) return board.is_in_check ? -(MATE_SCORE - ply) : 0;

    int bestEval = -MATE_SCORE; int score;
    Move bestMove = Move::NullMove();

    for (int i = 0; i < count; i++) {
        if (limits.out_of_time()) break;

        Move m = moves[i];
        board.MakeMove(m);

        // Recursive negamax (sign flip)
        PV childPV;
        score = -negamax(board, movegen, evaluator, depth - 1,
                            -beta, -alpha, childPV, previousPV,
                            limits, ply + 1, use_quiescence);

        board.UnmakeMove(m);

        if (limits.out_of_time()) break;

        // Best move update
        if (score > bestEval) {
            bestEval = score;
            bestMove = m;
            pv.set(m, childPV);
        }

        // Alpha-beta update
        alpha = std::max(alpha, bestEval);
        if (alpha >= beta) {
            // Killer moves
            if (!Move::SameMove(killerMoves[depth][0], m) && !m.IsPromotion() &&
                !(1ULL << m.TargetSquare() & board.colorBitboards[1 - board.move_color])) {
                killerMoves[depth][1] = killerMoves[depth][0];
                killerMoves[depth][0] = m;
            }
            // History heuristic
            int piece = board.getMovedPiece(m.StartSquare());
            historyHeuristic[board.is_white_move ? piece : piece + 6][m.TargetSquare()] += depth * depth;
            break;
        }
    }

    // === Transposition table store ===
    BoundType flag = EXACT;
    if (bestEval <= alphaOrig) flag = UPPERBOUND;
    else if (bestEval >= beta) flag = LOWERBOUND;

    tt.store(board.zobrist_hash, depth, ply, bestEval, flag, bestMove);

    return bestEval;
}


// ============================================================================
// ROOT SEARCH ENTRY POINT
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

        if (limits.out_of_time()) break;

        // update root evals
        result.root_moves[i] = m;
        result.root_evals[i] = eval;
        result.root_count++;

        // update best result
        if (i == 0 || eval > result.eval) {
            result.eval = eval;
            result.bestMove = m;
            result.best_line.set(m, childPV);
        }
    }

    return result;
}


// ============================================================================
// DEBUGGING TOOLS
// ============================================================================

int Searcher::verboseSearch(Board& board, Evaluator& evaluator, MoveGenerator& movegen,
                            int alpha, int beta, std::vector<Move>& pv,
                            SearchLimits& limits, int ply, bool use_quiescence, std::ostream& log) {
    nodesSearched++;
    enterDepth();
    max_q_depth = std::max(max_q_depth, quiesence_depth);

    int standPat = evaluator.taperedEval(board);
    int bestEval = standPat;

    std::string indent(static_cast<size_t>(ply) * 2, ' ');

    // Generate moves
    Move moves[MAX_MOVES];
    std::vector<Move> fakePV;
    int count = generateAndOrderMoves(board, movegen, evaluator, moves, 0, fakePV);
    if (count == 0) {
        if (board.is_in_check) { exitDepth(); return -MATE_SCORE + ply; }
        exitDepth();
        return standPat;
    }

    // Sort moves by SEE descending
    std::sort(moves, moves + count, [&](Move a, Move b) { return evaluator.SEE(board, a) > evaluator.SEE(board, b); });

    std::vector<Move> childPV;

    for (int i = 0; i < count; ++i) {
        if (limits.out_of_time()) break;

        Move m = moves[i];

        // Log move info
        log << indent << "Depth " << ply
            << " | Move: " << m.uci()
            << " | StandPat: " << standPat
            << " | Alpha: " << alpha
            << " | Beta: " << beta;

        bool pruned = false;
        if (shouldPrune(board, m, evaluator, standPat, alpha)) {
            pruned = true;
            log << " | Pruned by SEE/delta | SEE: " << evaluator.SEE(board, m);
        }
        log << "\n";

        if (pruned) continue;

        board.MakeMove(m);
        childPV.clear();

        int score;
        if (use_quiescence && !board.is_in_check) {
            score = -quiescenceVerbose(board, evaluator, movegen, -beta, -alpha, childPV, limits, ply + 1, log);
        } else {
            score = -verboseSearch(board, evaluator, movegen, -beta, -alpha, childPV, limits, ply + 1, use_quiescence, log);
        }

        board.UnmakeMove(m);

        // Log returned score
        log << indent << "Depth " << ply
            << " | Move: " << m.uci()
            << " | Returned score: " << score;

        if (score >= beta) {
            log << " | Alpha-Beta cutoff!\n";
            exitDepth();
            return beta;
        } else {
            log << "\n";
        }

        if (score > bestEval) {
            bestEval = score;
            alpha = std::max(alpha, score);
            updatePV(pv, m, childPV);
            best_quiescence_line = pv;
        }
    }

    exitDepth();
    return bestEval;
}


int Searcher::quiescenceVerbose(Board& board, Evaluator& evaluator, MoveGenerator& movegen,
                                int alpha, int beta, std::vector<Move>& pv,
                                SearchLimits& limits, int ply, std::ostream& log) {
    nodesSearched++;
    enterDepth();
    max_q_depth = std::max(max_q_depth, quiesence_depth);

    int standPat = evaluator.taperedEval(board);
    int bestEval = standPat;

    std::string indent(static_cast<size_t>(ply) * 2, ' ');

    movegen.generateMoves(board, true); // captures, promotions, evasions
    int count = movegen.count;
    if (count == 0) {
        if (board.is_in_check) { exitDepth(); return -MATE_SCORE + ply; }
        exitDepth();
        return standPat;
    }

    Move moves[MAX_MOVES];
    std::copy_n(movegen.moves, count, moves);

    // Sort moves by SEE descending
    std::sort(moves, moves + count, [&](Move a, Move b) { return evaluator.SEE(board, a) > evaluator.SEE(board, b); });

    std::vector<Move> childPV;

    for (int i = 0; i < count; ++i) {
        if (limits.out_of_time()) break;

        Move m = moves[i];

        log << indent << "Depth " << ply
            << " | Move: " << m.uci()
            << " | StandPat: " << standPat
            << " | Alpha: " << alpha
            << " | Beta: " << beta << "\n";

        if (shouldPrune(board, m, evaluator, standPat, alpha)) {
            log << indent << "  Skipped by SEE/delta pruning\n";
            continue;
        }

        board.MakeMove(m);
        childPV.clear();

        int score = -quiescenceVerbose(board, evaluator, movegen, -beta, -alpha, childPV, limits, ply + 1, log);

        board.UnmakeMove(m);

        log << indent << "Depth " << ply
            << " | Move: " << m.uci()
            << " | Returned score: " << score;

        if (score >= beta) {
            log << " | Alpha-Beta cutoff!\n";
            exitDepth();
            return beta;
        } else {
            log << "\n";
        }

        if (score > bestEval) {
            bestEval = score;
            alpha = std::max(alpha, score);
            updatePV(pv, m, childPV);
            best_quiescence_line = pv;
        }
    }

    exitDepth();
    return bestEval;
}

