#include "searcher.h"

Move Searcher::bestMove(std::vector<Move> potential_moves) {
    // random move
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, potential_moves.size() - 1);
    int randomIndex = dis(gen);
    return potential_moves[randomIndex];
}