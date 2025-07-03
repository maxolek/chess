#include "searcher.h"

int Searcher::nodesSearched = 0;
std::unordered_map<U64, TTEntry> Searcher::tt;

SearchResult Searcher::search(Board& board, MoveGenerator& movegen, std::vector<Move>& legal_moves, int depth, std::chrono::steady_clock::time_point start_time, int time_limit_ms) {
    bool maximizing = board.is_white_move;
    int bestEval = board.is_white_move ? -100000 : 100000;
    int alpha = -100000; int beta = 100000;
    Move bestMove = Move::NullMove();
    std::unordered_map<std::string, int> bestComponentEvals;

    for (Move move : legal_moves) {
        board.MakeMove(move);
        int eval = minimax(board, movegen, depth - 1, !maximizing, alpha, beta, start_time, time_limit_ms, false);
        board.UnmakeMove(move);

        if ((maximizing && eval > bestEval) ||
            (!maximizing && eval < bestEval)) {
            bestEval = eval;
            bestMove = move;
            //bestComponentEvals = Evaluator::computeComponents(board);
        }

        // check time
        auto current_time = std::chrono::steady_clock::now();
        int elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();
        if (elapsed_ms >= time_limit_ms) {bestMove = Move::NullMove(); break;}
    }

    return {bestMove, bestEval, bestComponentEvals};
}

int Searcher::minimax(Board& board, MoveGenerator& movegen, int depth, bool maximizing, int alpha, int beta,
                      std::chrono::steady_clock::time_point start_time, int time_limit_ms, bool out_of_time) {
    if (out_of_time) return maximizing ? -100000 : 100000;
    nodesSearched++;

    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count() >= time_limit_ms) {
        out_of_time = true;
        return maximizing ? -100000 : 100000;
    }

    if (depth == 0 || Arbiter::GetGameState(&board) != InProgress) {
        //movegen.mobility(&board); // doubles some key perft position depth5 times, not worth eval increase
        // but there is potential for these pseudo legal moves to be a part of regular movegeneration so 
        // mobility is calculated en-route
        return Evaluator::taperedEval(&board);
    }

    int bestEval = maximizing ? -100000 : 100000; //Move bestMove;
    int alphaOrig = alpha;  // Save original alpha before searching
    U64 hash = board.zobrist_hash;
    // Lookup TT entry
    auto tt_it = tt.find(hash);
    if (tt_it != tt.end()) {
        const TTEntry& entry = tt_it->second;
        if (entry.depth >= depth) {
                if (entry.flag == EXACT) {
                    return entry.eval;
                }
                if (entry.flag == LOWERBOUND && entry.eval > bestEval) {
                    bestEval = entry.eval;
                }
                if (entry.flag == UPPERBOUND && entry.eval < bestEval) {
                    bestEval = entry.eval;
                }
            }
    }

    std::vector<Move> moves = movegen.generateMovesList(&board);

    for (const Move& move : moves) {
        board.MakeMove(move);
        int eval = minimax(board, movegen, depth - 1, !maximizing, alpha, beta,
            start_time, time_limit_ms, out_of_time);
        board.UnmakeMove(move);

        if (out_of_time) return maximizing ? -100000 : 100000;

        if (maximizing) {
            bestEval = std::max(bestEval, eval);
            alpha = std::max(alpha, eval);
        } else {
            bestEval = std::min(bestEval, eval);
            beta = std::min(beta, eval);
        }

        if (beta <= alpha) {break;} // alpha-beta cutoff
    }

    // Store result in TT
    TTEntry newEntry;
    newEntry.eval = bestEval;
    //newEntry.bestMove = bestMove;
    newEntry.depth = depth;
    // Set flag based on alpha-beta bounds logic (EXACT, LOWERBOUND, UPPERBOUND)
    if (bestEval <= alphaOrig) newEntry.flag = UPPERBOUND;
    else if (bestEval >= beta) newEntry.flag = LOWERBOUND;
    else newEntry.flag = EXACT;

    tt[hash] = newEntry;

    return bestEval;
}
