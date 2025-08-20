#include "searcher.h"

// Define static members
int Searcher::historyHeuristic[12][64] = {};        // Assuming 12 piece types and 64 squares
Move Searcher::killerMoves[64][2] = {};             // Assuming max search depth is 64
int Searcher::nodesSearched = 0;
std::unordered_map<U64, TTEntry> Searcher::tt;
std::vector<Move> Searcher::best_line;
std::vector<Move> Searcher::best_quiescence_line;
int Searcher::quiesence_depth = 0;  int Searcher::max_q_depth = 0;


SearchResult Searcher::search(Board& board, MoveGenerator& movegen, Evaluator& evaluator,
                             Move legal_moves[MAX_MOVES], int count, int depth, Move pvMove,
                             std::chrono::steady_clock::time_point start_time, int time_limit_ms) {
    bool out_of_time = false;
   // int prev_evals[MAX_MOVES] = {};
    int bestEval = board.is_white_move ? -MATE_SCORE : MATE_SCORE;
    int alpha = -MATE_SCORE, beta = MATE_SCORE;
    Move bestMove = count>0 ? legal_moves[0] : Move::NullMove();
    std::unordered_map<std::string, int> bestComponentEvals;
    std::vector<Move> principalVariation;
    best_line.clear();
    nodesSearched = 0;

    for (int i = 0; i < count; ++i) {
        Move m = legal_moves[i];
        if (Move::SameMove(m, Move::NullMove())) continue;

        board.MakeMove(m);
        out_of_time = false;

        std::vector<Move> childPV;
        int eval = minimax(board, movegen, evaluator, depth - 1, alpha, beta, // false because now opponent to move
                           childPV, pvMove, start_time, time_limit_ms, out_of_time,
                             depth <= 4 ? false : true); // fine with using static for low depth to build TT and other stuff, since these depths should not be submitting moves anyway

        board.UnmakeMove(m);

        if (out_of_time) {
            if (i > 0) break; // timeout after at least one move searched
            else return {Move::NullMove(), -100 * MATE_SCORE, {}};
        }

        if (i == 0 || 
            (board.is_white_move && eval > bestEval) || 
            (!board.is_white_move && eval < bestEval)) {

            bestEval = eval;
            bestMove = m;

            principalVariation.clear();
            principalVariation.push_back(m);
            principalVariation.insert(principalVariation.end(), childPV.begin(), childPV.end());
            best_line = principalVariation;
        }


        if (board.is_white_move) alpha = std::max(alpha, eval);
        else beta = std::min(beta, eval);


        // check time
        auto current_time = std::chrono::steady_clock::now();
        int elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();
        if (elapsed_ms >= time_limit_ms && i > 0) {
            out_of_time = true;
            break;
        }
    }

    return {bestMove, bestEval, bestComponentEvals, best_line, best_quiescence_line};
}


int Searcher::minimax(Board& board, MoveGenerator& movegen, Evaluator& evaluator,
                      int depth, int alpha, int beta,
                      std::vector<Move>& pv, Move pvMove,
                      //int prev_evals[MAX_MOVES],
                      std::chrono::steady_clock::time_point start_time, int time_limit_ms,
                      bool &out_of_time, bool use_quiesence) {
    nodesSearched++;
    bool isWhiteToMove = board.is_white_move;

    if (depth == 0) {
        int eval; pv.clear();

        if (use_quiesence) eval = quiescence(board, evaluator, movegen, alpha, beta, pv, start_time, time_limit_ms, out_of_time);
        else eval = evaluator.taperedEval(&board);
        //int eval = evaluator.taperedEval(&board);
        return eval;
    }

    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count() >= time_limit_ms) {
        out_of_time = true;
        return isWhiteToMove ? alpha : beta;
    }

    int alphaOrig = alpha;
    U64 hash = board.zobrist_hash;
    auto tt_it = tt.find(hash);
    if (tt_it != tt.end()) {
        const TTEntry& entry = tt_it->second;
        if (entry.depth >= depth) {
            if (entry.flag == EXACT) return entry.eval;
            if (entry.flag == LOWERBOUND && entry.eval >= beta) return entry.eval;
            if (entry.flag == UPPERBOUND && entry.eval <= alphaOrig) return entry.eval;
        }
    }

    Move moves[MAX_MOVES];
    Move ttMove = Move::NullMove();
    int count = generateAndOrderMoves(board, movegen, evaluator, moves, depth, pvMove);

    if (count == 0) {
        if (board.is_in_check) {
            return isWhiteToMove ? -(MATE_SCORE - depth) : (MATE_SCORE - depth);
        } else {
            return 0;
        }
    } else {count = std::min(MAX_MOVES, count);}

    int bestEval = isWhiteToMove ? -MATE_SCORE : MATE_SCORE;
    bool foundMove = false; int score;
    int seeScore; bool prune = false;

    for (int i = 0; i < count; ++i) {
        Move m = moves[i];
        if (Move::SameMove(m, Move::NullMove())) continue;

        // SEE prune: only for quiet situations to avoid missing critical defensive tactics
        bool isCapture = board.getCapturedPiece(m.TargetSquare()) != -1;
        if (isCapture && !m.IsPromotion() && !board.is_in_check) {
            int seeGain = evaluator.SEE(board, m);      // net for the mover
            if (seeGain < 0) {
                // Losing capture -> prune
                continue;
            }
        }

        board.MakeMove(m);

        if (board.hash_history[board.zobrist_hash] >= 3) {board.UnmakeMove(m); return 0;} // threefold

        std::vector<Move> childPV;
        score = bestEval; // pruned moves get skipped while old score is still present
        if (!prune) {
            score = minimax(board, movegen, evaluator, depth - 1, alpha, beta, childPV, m,
                                start_time, time_limit_ms, out_of_time, use_quiesence);
            }

        board.UnmakeMove(m);

        if (out_of_time) break;

        foundMove = true;

        if (isWhiteToMove) {
            if (score > bestEval) {
                bestEval = score;
                ttMove = m;
                pv.clear();
                pv.push_back(m);
                pv.insert(pv.end(), childPV.begin(), childPV.end());
            }
            alpha = std::max(alpha, bestEval);
        } else {
            if (score < bestEval) {
                bestEval = score;
                ttMove = m;
                pv.clear();
                pv.push_back(m);
                pv.insert(pv.end(), childPV.begin(), childPV.end());
            }
            beta = std::min(beta, bestEval);
        }

        if (beta <= alpha) {
            // killer moves NOTS
            if (!((Move::SameMove(killerMoves[depth][0], m)) // stored killer move
                || ((1ULL << m.TargetSquare()) & board.colorBitboards[1-board.move_color]) // capture
                || (m.IsPromotion()))) { // promotion
                killerMoves[depth][1] = killerMoves[depth][0];
                killerMoves[depth][0] = m;
            }

            int piece = board.getMovedPiece(m.StartSquare());
            historyHeuristic[board.is_white_move ? piece : piece+6][m.TargetSquare()] += depth * depth;

            break;
        }
    }

    if (!foundMove) return evaluator.taperedEval(&board);

    TTEntry newEntry;
    newEntry.eval = bestEval;
    newEntry.bestMove = ttMove;
    newEntry.depth = depth;
    if (bestEval <= alphaOrig) newEntry.flag = UPPERBOUND;
    else if (bestEval >= beta) newEntry.flag = LOWERBOUND;
    else newEntry.flag = EXACT;
    // only update if depth is better
    auto it = tt.find(hash);
    if (it == tt.end() || it->second.depth <= depth) {
        tt[hash] = newEntry;
    }


    return bestEval;
}

int Searcher::quiescence(Board& board, Evaluator& evaluator, MoveGenerator& movegen,
                         int alpha, int beta, std::vector<Move>& pv,
                         std::chrono::steady_clock::time_point start_time, int time_limit_ms, bool& out_of_time) {
    nodesSearched++;

    bool isWhiteToMove = board.is_white_move;

    bool isCapture = board.currentGameState.capturedPieceType > -1;
    bool isPromo = !board.allGameMoves.empty() && board.allGameMoves.back().IsPromotion();
    bool inCheck = board.is_in_check;

    int standPat = evaluator.taperedEval(&board);
    int bestEval = standPat;
    int score; bool prune;
    int elapsed_ms;

    if (standPat >= beta) return beta;
    if (standPat > alpha) alpha = standPat;

    if (!inCheck && standPat + MAX_DELTA < alpha) return alpha; // delta pruning

    // If no tactical reason to search deeper, just return standPat
    bool interesting = isCapture || isPromo || inCheck;
    if (!interesting) {pv.clear(); return standPat;}

    Move moves[MAX_MOVES];
    int moveCount = movegen.generateMovesList(&board, moves);
    moveCount = std::min(MAX_MOVES, moveCount);

    if (moveCount == 0) {
        if (inCheck) {
            return isWhiteToMove ? -MATE_SCORE : MATE_SCORE;
        } else {
            return standPat;  // no moves, no check => quiet position
        }
    }

    // Filter: only captures, promotions, and evasions
    Move interesting_moves[MAX_MOVES];
    int filteredCount = 0;
    for (int i = 0; i < moveCount; i++) {
        if (board.getCapturedPiece(moves[i].TargetSquare()) != -1 || moves[i].IsPromotion() || inCheck) {
            interesting_moves[filteredCount++] = moves[i];
        }
    }

    if (filteredCount == 0) return standPat; // no interesting moves

    filteredCount = std::min(filteredCount, MAX_MOVES);
    std::vector<Move> childPV;

    for (int i = 0; i < filteredCount; i++) {
        Move move = interesting_moves[i];

        // SEE prune: only for quiet situations to avoid missing critical defensive tactics
        bool isCapture = board.getCapturedPiece(move.TargetSquare()) != -1;
        if (isCapture && !move.IsPromotion() && !board.is_in_check) {
            int seeGain = evaluator.SEE(board, move);      // net for the mover
            if (seeGain < 0) {
                // Losing capture -> prune
                continue;
            }
        }

        // Make move to check SEE on captures
        board.MakeMove(move);
        quiesence_depth++;

        //score = bestEval;
        score = quiescence(board, evaluator, movegen, alpha, beta, childPV,
                               start_time, time_limit_ms, out_of_time);

        board.UnmakeMove(move);
        max_q_depth = std::max(max_q_depth, quiesence_depth);
        quiesence_depth--;

        //if (prune) continue;  // skip bad capture

        if (score >= beta) return beta;
        if (score > bestEval) {
            bestEval = score;
            alpha = std::max(alpha, score);
            pv.clear();
            pv.push_back(move);
            pv.insert(pv.end(), childPV.begin(), childPV.end());
            best_quiescence_line = childPV;
        }

        auto current_time = std::chrono::steady_clock::now();
        elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();
        if (elapsed_ms >= time_limit_ms && i > 0) {
            out_of_time = true;
            break;
        }
    }

    return bestEval;
}



// call before makeMove()
int Searcher::moveScore(const Evaluator& evaluator, const Move& move, const Board& board, int depth, const Move& ttMove, const Move& pvMove/*, int prev_eval*/) {
    // TT move
    // movegen count solves pvMove null move concern
    if (Move::SameMove(pvMove, move)) return MATE_SCORE + 500;
    if (Move::SameMove(ttMove, move)) return MATE_SCORE + 100;

    // MVV-LVA (hyeristic for all captures)
    if (board.getCapturedPiece(move.TargetSquare()) != -1) {
        int score = 100000 + evaluator.mvvLvaTable[board.getCapturedPiece(move.TargetSquare())][board.getMovedPiece(move.StartSquare())];
        return score;
    }
    // Captures — SEE
    /*
    if (board.getCapturedPiece(move.TargetSquare()) != -1) {
        int seeScore = Evaluator::SEE(board);
        if (seeScore < 0)
            return -999999; // bad capture
        else
            return MATE_SCORE + seeScore;
    }
 */

    // Killer moves
    if (Move::SameMove(killerMoves[depth][0], move)) return MATE_SCORE - 500;
    if (Move::SameMove(killerMoves[depth][1], move)) return MATE_SCORE - 600;

    // History heuristic
    int piece = board.getMovedPiece(move.StartSquare());
    return historyHeuristic[board.is_white_move ? piece : piece+6][move.TargetSquare()]; // + prev_eval;
}


void Searcher::orderedMoves(const Evaluator& evaluator, Move moves[MAX_MOVES], int count, const Board& board, int depth, const Move& pvMove) {
    U64 hash = board.zobrist_hash;
    Move ttMove = Move::NullMove();

    auto it = tt.find(hash);
    if (it != tt.end()) {
        ttMove = it->second.bestMove;
    }

    std::vector<std::pair<int, Move>> scored;

    for (int i = 0; i < count; ++i) {
        int score = moveScore(evaluator, moves[i], board, depth, ttMove, pvMove/*,prev_eval[i]*/);
        scored.emplace_back(score, moves[i]);
    }

    std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
        return a.first > b.first;
    });

    for (int i = 0; i < count; ++i) {
        moves[i] = scored[i].second;
    }
}

int Searcher::generateAndOrderMoves(Board& board, MoveGenerator& movegen, const Evaluator& evaluator, Move moves[MAX_MOVES], int depth, const Move& pvMove) {
    int count = movegen.generateMovesList(&board, moves);
    count = std::min(MAX_MOVES, count);
    orderedMoves(evaluator, moves, count, board, depth, pvMove);
    return count;
}