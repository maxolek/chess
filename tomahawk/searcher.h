#ifndef SEARCHER_H
#define SEARCHER_H

#include "moveGenerator.h"

class Searcher {
private:
    std::vector<Move> potential_moves;
public:
    static Move bestMove(std::vector<Move> potential_moves);
};

#endif