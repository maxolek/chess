#include "searcher.h"

Move Searcher::bestMove(std::vector<Move> potential_moves) {
    // random move
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<> dist(0, potential_moves.size() - 1);
    int randomIndex = dist(rng);
    return potential_moves[randomIndex];
}