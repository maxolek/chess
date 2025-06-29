#ifndef SEARCHER_H
#define SEARCHER_H

#include "evaluator.h"

class Searcher {
private:
    //Board* board;
    //std::vector<Move>* potential_moves;
public:
    static Move bestMove(Board& board, std::vector<Move>& potential_moves);
};

#endif