#include "searcher.h"

int Searcher::nodesSearched = 0;

SearchResult Searcher::search(Board& board, MoveGenerator& movegen, std::vector<Move>& legal_moves, int depth, std::chrono::steady_clock::time_point start_time, int time_limit_ms) {
    bool maximizing = board.is_white_move;
    int bestEval = board.is_white_move ? -100000 : 100000;
    Move bestMove = Move::NullMove();
    std::unordered_map<std::string, int> bestComponentEvals;

    for (Move move : legal_moves) {
        board.MakeMove(move);
        int eval = minimax(board, movegen, depth - 1, !maximizing, start_time, time_limit_ms, false);
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

int Searcher::minimax(Board& board, MoveGenerator& movegen, int depth, bool maximizing,
                      std::chrono::steady_clock::time_point start_time, int time_limit_ms, bool out_of_time) {
    if (out_of_time) maximizing ? -100000 : 100000;

    nodesSearched++;

    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count() >= time_limit_ms) {
        out_of_time = true;
        maximizing ? -100000 : 100000;
    }

    if (depth == 0 || Arbiter::GetGameState(&board) != InProgress) {
        return Evaluator::Evaluate(&board);
    }

    std::vector<Move> moves = movegen.generateMovesList(&board);
    int bestEval = maximizing ? -100000 : 100000;

    for (const Move& move : moves) {
        board.MakeMove(move);
        int eval = minimax(board, movegen, depth - 1, !maximizing, start_time, time_limit_ms, out_of_time);
        board.UnmakeMove(move);

        if (out_of_time) maximizing ? -100000 : 100000;

        if (maximizing)
            bestEval = std::max(bestEval, eval);
        else
            bestEval = std::min(bestEval, eval);
    }

    return bestEval;
}
