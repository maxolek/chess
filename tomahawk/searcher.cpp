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
std::unordered_map<U64, TTEntry> Searcher::tt; // transposition table
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

// -------------------------------
// Transposition table helpers
// -------------------------------
TTEntry* Searcher::probeTT(U64 key, int depth, int ply, int alpha, int beta, int& score) {
    auto it = tt.find(key);
    if (it == tt.end()) return nullptr;

    TTEntry& entry = it->second;
    int requiredHorizon = ply + depth;

    if (entry.horizon < requiredHorizon)
        return nullptr;

    score = entry.eval;
    switch (entry.flag) {
        case EXACT: return &entry;
        case LOWERBOUND: if (score >= beta) return &entry; break;
        case UPPERBOUND: if (score <= alpha) return &entry; break;
    }
    return nullptr;
}

void Searcher::storeTT(U64 key, int depth, int ply, int score, BoundType flag, const std::vector<Move>& pv) {
    TTEntry entry;
    entry.key = key;
    entry.eval = score;
    entry.flag = flag;
    entry.bestMove = pv.empty() ? Move::NullMove() : pv[0];
    entry.depth = depth;
    entry.horizon = ply + depth; // <-- horizon fix
    entry.age = 0;

    tt[key] = entry;
}


// ============================================================================
// MOVE ORDERING
// ============================================================================

// Assign a heuristic score to each move
// - PV move highest priority
// - TT move is strong candidate
// - Captures scored by SEE
// - Promotions weighted by piece type
// - Killer moves preferred if they caused cutoffs earlier
// - Quiet moves fall back to history heuristic
int Searcher::moveScore(const Evaluator& evaluator, const Move& move, const Board& board,
                        int depth, const Move& ttMove, const Move& pvMove) {
    constexpr int PV_BASE       = 10'000'000;
    constexpr int TT_BASE       = 9'000'000;
    constexpr int GOOD_CAP_BASE = 8'000'000;
    constexpr int PROMO_BASE    = 7'500'000;
    constexpr int KILLER_BASE   = 7'000'000;
    constexpr int QUIET_BASE    = 0;
    constexpr int BAD_CAP_BASE  = -1'000'000;

    if (Move::SameMove(pvMove, move)) return PV_BASE;
    if (Move::SameMove(ttMove, move)) return TT_BASE;

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
    if (Move::SameMove(killerMoves[depth][0], move)) return KILLER_BASE;
    if (Move::SameMove(killerMoves[depth][1], move)) return KILLER_BASE - 1;

    // Quiet moves (history heuristic)
    int piece = board.getMovedPiece(move.StartSquare());
    int sidePiece = board.is_white_move ? piece : piece + 6;
    return QUIET_BASE + historyHeuristic[sidePiece][move.TargetSquare()];
}

// Score + sort moves in descending order
void Searcher::orderedMoves(const Evaluator& evaluator, Move moves[MAX_MOVES], size_t count,
                            const Board& board, int depth, const Move& pvMove) {
    U64 hash = board.zobrist_hash;
    Move ttMove = Move::NullMove();
    auto it = tt.find(hash);
    if (it != tt.end()) ttMove = it->second.bestMove;

    std::vector<std::pair<int, Move>> scored;
    scored.reserve(count);

    for (size_t i = 0; i < count; ++i)
        scored.emplace_back(moveScore(evaluator, moves[i], board, depth, ttMove, pvMove), moves[i]);

    std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    for (size_t i = 0; i < count; ++i) moves[i] = scored[i].second;
}

// Generate full move list, then order it
int Searcher::generateAndOrderMoves(Board& board, MoveGenerator& movegen, const Evaluator& evaluator,
                                    Move moves[MAX_MOVES], int depth, const Move& pvMove) {
    
    movegen.generateMoves(board, false);
    int count = std::min(MAX_MOVES, movegen.count);
    std::copy_n(movegen.moves, count, moves);

    orderedMoves(evaluator, moves, static_cast<size_t>(count), board, depth, pvMove);
    return count;
}

// ============================================================================
// QUIESCENCE SEARCH
// ----------------------------------------------------------------------------
// Extends search in leaf positions where tactics matter (captures, checks, promos)
// Prevents horizon effect by searching only "noisy" moves
// ============================================================================

int Searcher::quiescence(Board& board, Evaluator& evaluator, MoveGenerator& movegen,
                         int alpha, int beta, std::vector<Move>& pv,
                         SearchLimits& limits, int ply) {
    nodesSearched++;
    if (limits.out_of_time()) return alpha;

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
        std::vector<Move> childPV;
        int score = -quiescence(board, evaluator, movegen, -beta, -alpha, childPV, limits, ply+1);
        exitDepth();

        board.UnmakeMove(m);
        
        if (limits.out_of_time()) break;

        if (score >= beta) return beta;
        if (score > bestEval) {
            bestEval = score;
            alpha = std::max(alpha, score);
            updatePV(pv, m, childPV);
            best_quiescence_line = pv;
        }
    }

    return bestEval;
}


// ============================================================================
// NEGAMAX SEARCH (with alpha-beta pruning)
// ============================================================================

int Searcher::negamax(Board& board, MoveGenerator& movegen, Evaluator& evaluator,
                      int depth, int alpha, int beta,
                      std::vector<Move>& pv, Move pvMove,
                      SearchLimits& limits, int ply, bool use_quiescence) {
    nodesSearched++;
    if (limits.out_of_time()) return alpha;

    // Base case: depth = 0
    if (depth == 0)
        return use_quiescence
             ? quiescence(board, evaluator, movegen, alpha, beta, pv, limits, ply)
             : evaluator.taperedEval(board);

    // TT lookup
    int alphaOrig = alpha;
    int hashScore;
    TTEntry* ttEntry = probeTT(board.zobrist_hash, depth, ply, alpha, beta, hashScore);
    if (ttEntry) return hashScore;

    // Generate ordered moves
    Move moves[MAX_MOVES];
    int count = generateAndOrderMoves(board, movegen, evaluator, moves, depth, pvMove);
    if (count == 0) return board.is_in_check ? -(MATE_SCORE - depth) : 0;

    int bestEval = -MATE_SCORE;
    Move bestMove = Move::NullMove();
    std::vector<Move> childPV;

    for (int i = 0; i < count; i++) {
        if (limits.out_of_time()) break;

        Move m = moves[i];
        board.MakeMove(m);

        // Threefold repetition check
        if (board.hash_history[board.zobrist_hash] >= 3) { board.UnmakeMove(m); continue; }

        // Recursive negamax (note sign flip!)
        int score = -negamax(board, movegen, evaluator, depth - 1,
                             -beta, -alpha, childPV, m,
                              limits, ply+1, use_quiescence);
        board.UnmakeMove(m);

        if (limits.out_of_time()) break;

        // Best move update
        if (score > bestEval) {
            bestEval = score;
            bestMove = m;
            updatePV(pv, m, childPV);
        }

        // Alpha-beta update
        alpha = std::max(alpha, bestEval);
        if (alpha >= beta) {
            // Store killer moves
            if (!Move::SameMove(killerMoves[depth][0], m) && !m.IsPromotion() &&
                !(1ULL << m.TargetSquare() & board.colorBitboards[1 - board.move_color])) {
                killerMoves[depth][1] = killerMoves[depth][0];
                killerMoves[depth][0] = m;
            }
            // History heuristic update
            int piece = board.getMovedPiece(m.StartSquare());
            historyHeuristic[board.is_white_move ? piece : piece + 6][m.TargetSquare()] += depth * depth;
            break;
        }
    }

    // Store in TT
    BoundType flag = EXACT;
    if (bestEval <= alphaOrig) flag = UPPERBOUND;
    else if (bestEval >= beta) flag = LOWERBOUND;
    storeTT(board.zobrist_hash, depth, ply, bestEval, flag, pv);

    return bestEval;
}

// ============================================================================
// ROOT SEARCH ENTRY POINT
// ============================================================================

SearchResult Searcher::search(Board& board, MoveGenerator& movegen, Evaluator& evaluator,
                             Move legal_moves[MAX_MOVES], int count, int depth, Move pvMove,
                             SearchLimits& limits) {
    nodesSearched = 0;
    best_line.clear();
    best_quiescence_line.clear();

    int bestEval = -MATE_SCORE;
    Move bestMove = Move::NullMove();
    std::vector<Move> principalVariation;

    for (int i = 0; i < count; ++i) {
        if (limits.out_of_time()) break;

        Move m = legal_moves[i];
        if (Move::SameMove(m, Move::NullMove())) continue;

        board.MakeMove(m);
        std::vector<Move> childPV;
        int eval = -negamax(board, movegen, evaluator, depth - 1,
                            -MATE_SCORE, MATE_SCORE, childPV, m,
                            limits, board.plyCount, true);
        board.UnmakeMove(m);

        if (limits.out_of_time()) break;

        if (i == 0 || eval > bestEval) {
            bestEval = eval;
            bestMove = m;
            updatePV(principalVariation, m, childPV);
            best_line = principalVariation;
        }
    }

    return {bestMove, bestEval, {}, best_line, best_quiescence_line};
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
    int count = generateAndOrderMoves(board, movegen, evaluator, moves, 0, Move::NullMove());
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

