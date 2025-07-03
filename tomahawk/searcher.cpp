#include "searcher.h"

// Define static members
int Searcher::historyHeuristic[12][64] = {};        // Assuming 12 piece types and 64 squares
Move Searcher::killerMoves[64][2] = {};             // Assuming max search depth is 64
int Searcher::nodesSearched = 0;
std::unordered_map<U64, TTEntry> Searcher::tt;
Move Searcher::best_line[Searcher::MAX_DEPTH];

SearchResult Searcher::search(Board& board, MoveGenerator& movegen, Evaluator& evaluator, Move legal_moves[MoveGenerator::max_moves], int count, int depth, std::chrono::steady_clock::time_point start_time, int time_limit_ms) {
    bool maximizing = board.is_white_move;
    int bestEval = board.is_white_move ? -100000 : 100000;
    int alpha = -100000; int beta = 100000;
    Move bestMove = Move::NullMove();
    std::unordered_map<std::string, int> bestComponentEvals;
    int prev_evals[MoveGenerator::max_moves];
    std::fill_n(prev_evals, MoveGenerator::max_moves, bestEval);

    for (int i = 0; i < count; i++) {
        Move m =  legal_moves[i];
        board.MakeMove(m);
        int eval = minimax(
            board, movegen, evaluator,
            depth - 1, !maximizing, 
            alpha, beta, Move(0), prev_evals,
            start_time, time_limit_ms, false
        );
        prev_evals[i] = eval;
        board.UnmakeMove(m);

        if ((maximizing && eval > bestEval) ||
            (!maximizing && eval < bestEval)) {
            bestEval = eval;
            bestMove = m;
            //bestComponentEvals = Evaluator::computeComponents(board);
            // Set PV to current root move + line from deeper search
        }

        // check time
        auto current_time = std::chrono::steady_clock::now();
        int elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();
        // if  evaluating prev search best move first then we can still return the best move
        if (elapsed_ms >= time_limit_ms) {break;}//{bestMove = Move::NullMove(); break;}
    }

    return {bestMove, bestEval, bestComponentEvals};
}

int Searcher::minimax(Board& board, MoveGenerator& movegen, Evaluator& evaluator, int depth, bool maximizing, int alpha, int beta, const Move& pvMove, int prev_eval[MoveGenerator::max_moves],
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
        return evaluator.taperedEval(&board);
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

    Move next_moves[MoveGenerator::max_moves]; Move tt_move = Move(0); Move it_pvMove = pvMove;
    int count = generateAndOrderMoves(board, movegen, next_moves, depth, pvMove, prev_eval);

    for (int i = 0; i < count; i++) {
        Move m = next_moves[i];
        board.MakeMove(m);
        int eval = minimax(board, movegen, evaluator, depth - 1, !maximizing, alpha, beta, it_pvMove, prev_eval,
            start_time, time_limit_ms, out_of_time);
        board.UnmakeMove(m);

        if (out_of_time) return maximizing ? -100000 : 100000;

        bool isBetter;
        if (maximizing) {
            isBetter = (eval > bestEval);
        } else {
            isBetter = (eval < bestEval);
        }
        if (isBetter) {
            bestEval = eval;
            it_pvMove = m;
            tt_move = m;
            if (maximizing) alpha = std::max(alpha, eval);
            else beta = std::min(beta, eval);
        }

        if (beta <= alpha) {
            if (board.getCapturedPiece(m.TargetSquare()) == -1) {  // quiet move
                if (!Move::SameMove(killerMoves[depth][0], m)) {
                    killerMoves[depth][1] = killerMoves[depth][0];
                    killerMoves[depth][0] = m;
                }

                int piece = board.getMovedPiece(m.StartSquare());
                historyHeuristic[board.is_white_move ? piece : piece+6][m.TargetSquare()] += depth * depth;
            }
            break;
        }

    }

    // Store result in TT
    TTEntry newEntry;
    newEntry.eval = bestEval;
    newEntry.bestMove = tt_move;
    newEntry.depth = depth;
    // Set flag based on alpha-beta bounds logic (EXACT, LOWERBOUND, UPPERBOUND)
    if (bestEval <= alphaOrig) newEntry.flag = UPPERBOUND;
    else if (bestEval >= beta) newEntry.flag = LOWERBOUND;
    else newEntry.flag = EXACT;

    tt[hash] = newEntry;

    return bestEval;
}

// call before makeMove()
int Searcher::moveScore(const Move& move, const Board& board, int depth, const Move& ttMove, const Move& pvMove, int prev_eval) {
    // TT move
    if (Move::SameMove(pvMove, move)) return 1100000;
    if ((Move::SameMove(ttMove, move))) return 1000000;

    // Captures — MVV-LVA
    if (board.getCapturedPiece(move.TargetSquare()) != -1) {
        int attacker = board.getMovedPiece(move.StartSquare());
        int victim = board.getMovedPiece(move.TargetSquare());
        return 100000 + 10 * victim - attacker;
    }

    // Killer moves
    if (Move::SameMove(killerMoves[depth][0], move)) return 90000;
    if (Move::SameMove(killerMoves[depth][1], move)) return 80000;

    // History heuristic
    int piece = board.getMovedPiece(move.StartSquare());
    return historyHeuristic[board.is_white_move ? piece : piece+6][move.TargetSquare()] + prev_eval;
}


void Searcher::orderedMoves(Move moves[MoveGenerator::max_moves], int count, const Board& board, int depth, const Move& pvMove, int prev_eval[MoveGenerator::max_moves]) {
    U64 hash = board.zobrist_hash;
    Move ttMove = Move::NullMove();

    auto it = tt.find(hash);
    if (it != tt.end()) {
        ttMove = it->second.bestMove;
    }

    std::vector<std::pair<int, Move>> scored;

    for (int i = 0; i < count; ++i) {
        int score = moveScore(moves[i], board, depth, ttMove, pvMove, prev_eval[i]);
        scored.emplace_back(score, moves[i]);
    }

    std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
        return a.first > b.first;
    });

    for (int i = 0; i < count; ++i) {
        moves[i] = scored[i].second;
    }
}

int Searcher::generateAndOrderMoves(Board& board, MoveGenerator& movegen, Move moves[], int depth, const Move& pvMove, int prev_eval[MoveGenerator::max_moves]) {
    int count = movegen.generateMovesList(&board, moves);
    orderedMoves(moves, count, board, depth, pvMove, prev_eval);
    return count;
}