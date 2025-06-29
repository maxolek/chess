#ifndef EVALUATOR_H
#define EVALUATOR_H

#include "board.h"
#include "helpers.h"
#include "arbiter.h" // term evals 

class Evaluator {
private:
    //const Board* position;
    //int eval;

public:
    static const std::unordered_map<std::string, int> evalWeights; 

    // count differences, and ability for finer tuning weights than macor of evalWeights
    // will need to do floats
    static constexpr float pieceValues[5] = {1, 3, 3.3, 5, 9}; // pawn knight bishop rook queen
    static constexpr int doubled_pawns = 1; 
    static constexpr int blocked_pawns = 1; 
    static constexpr int iso_pawns = 1;
    static constexpr int mobility = 1;

    static int Evaluate(const Board* position); // run all
    // eval is scaled to centipawn (100=1) and converted to int
    static int computeEval(const Board& board); // add up below w/ weights
    static float evaluateComponent(const Board& board, std::string component); // iterate through names and run below funcs
    
    // counts (weights are in above)
    static float materialDifferences(const Board& position); // add up material
    static float pawnStructureDifferences(const Board& position); // double, block, iso (more is bad)
    static float mobilityDifferences(const Board& position); // # moves (psuedo possibilities)
};

#endif