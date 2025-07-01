#include "searcher.h"

int Searcher::nodesSearched = 0;

SearchResult Searcher::search(Board& board, MoveGenerator& movegen, std::vector<Move>& legal_moves, int depth, std::chrono::steady_clock::time_point start_time, int time_limit_ms) {
    bool maximizing = board.is_white_move;
    int bestEval = board.is_white_move ? -100000 : 100000;
    Move bestMove = Move::NullMove();
    std::unordered_map<std::string, int> bestComponentEvals;

    for (Move move : legal_moves) {
        board.MakeMove(move);
        int eval = minimax(board, movegen, depth - 1, !maximizing);
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

int Searcher::minimax(Board& board, MoveGenerator& movegen, int depth, bool maximizingPlayer) {
    nodesSearched++;
    
    if (depth == 0 || Arbiter::GetGameState(&board) != InProgress) {
        return Evaluator::Evaluate(&board);
    } //else if (depth == 5) {Evaluator::writeEvalDebug(board, "C:/Users/maxol/code/chess/eval.txt");}

    std::vector<Move> moves = movegen.generateMovesList(&board);
    int bestEval = maximizingPlayer ? -100000 : 100000;

    for (const Move& move : moves) {
        board.MakeMove(move);
        int eval = minimax(board, movegen, depth - 1, !maximizingPlayer);
        board.UnmakeMove(move);

        if (maximizingPlayer)
            bestEval = std::max(bestEval, eval);
        else
            bestEval = std::min(bestEval, eval);
    }

    return bestEval;
}
