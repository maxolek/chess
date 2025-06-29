#include "searcher.h"

Move Searcher::bestMove(Board& board, std::vector<Move>& potential_moves) {
    // get best move from evaluator
    bool maximizing = board.is_white_move;
    int best_eval = board.is_white_move ? -100000 : 100000; // start off will loss 
    int eval = 0; 
    Move best_move;

    for (Move move : potential_moves) {
        board.MakeMove(move);
        eval = Evaluator::Evaluate(&board);

        if ((maximizing && eval > best_eval) ||
            (!maximizing && eval < best_eval)) {
                best_eval = eval;
                best_move = move;
            }
        
        board.UnmakeMove(move);
    }

    return best_move;
}