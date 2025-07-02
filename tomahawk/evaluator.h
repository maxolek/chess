#ifndef EVALUATOR_H
#define EVALUATOR_H

#include "board.h"
#include "helpers.h"
#include "arbiter.h" // term evals 
#include "moveGenerator.h" // mobility
#include <fstream>  // for file output
#include <iomanip>  // for formatting

class Evaluator {
private:
public:

    static void writeEvalDebug(const MoveGenerator* movegen, Board& board, const std::string& filename);


    // pieces
    static constexpr float pieceValues[5] = {1, 3, 3.3, 5, 9}; 
    // pawn knight bishop rook queen
    // piece positions


   static constexpr int PST[6][64] = {
        // White Pawn
        {
            0,   0,   0,   0,   0,   0,   0,   0,
            5,  10,  10, -20, -20,  10,  10,   5,
            5,  -5, -10,   0,   0, -10,  -5,   5,
            0,   0,   0,  20,  20,   0,   0,   0,
            5,   5,  10,  25,  25,  10,   5,   5,
            10,  10,  20,  30,  30,  20,  10,  10,
            50,  50,  50,  50,  50,  50,  50,  50,
            0,   0,   0,   0,   0,   0,   0,   0
        },
        // White Knight
        {
            -50, -40, -30, -30, -30, -30, -40, -50,
            -40, -20,   0,   0,   0,   0, -20, -40,
            -30,   0,  10,  15,  15,  10,   0, -30,
            -30,   5,  15,  20,  20,  15,   5, -30,
            -30,   0,  15,  20,  20,  15,   0, -30,
            -30,   5,  10,  15,  15,  10,   5, -30,
            -40, -20,   0,   5,   5,   0, -20, -40,
            -50, -40, -30, -30, -30, -30, -40, -50
        },
        // White Bishop
        {
            -20, -10, -10, -10, -10, -10, -10, -20,
            -10,   0,   0,   0,   0,   0,   0, -10,
            -10,   0,   5,  10,  10,   5,   0, -10,
            -10,   5,   5,  10,  10,   5,   5, -10,
            -10,   0,  10,  10,  10,  10,   0, -10,
            -10,  10,  10,  10,  10,  10,  10, -10,
            -10,   5,   0,   0,   0,   0,   5, -10,
            -20, -10, -10, -10, -10, -10, -10, -20
        },
        // White Rook
        {
            0,   0,   0,   0,   0,   0,   0,   0,
            5,   0,   5,   5,   0,   0,   0,     5,
            -5,   0,   0,   0,   0,   0,   0,  -5,
            -5,   0,   0,   0,   0,   0,   0,  -5,
            -5,   0,   0,   0,   0,   0,   0,  -5,
            -5,   0,   0,   0,   0,   0,   0,  -5,
            -5,   10,  10,  10,  10,  10,  10,  -5,
            0,   0,   0,   5,   5,   0,   0,   0
        },
        // White Queen
        {
            -20, -10, -10,  -5,  -5, -10, -10, -20,
            -10,   0,   0,   0,   0,   0,   0, -10,
            -10,   0,   5,   5,   5,   5,   0, -10,
            -5,   0,   5,   5,   5,   5,   0,  -5,
            0,   0,   5,   5,   5,   5,   0,  -5,
            -10,   5,   5,   5,   5,   5,   0, -10,
            -10,   0,   5,   0,   0,   0,   0, -10,
            -20, -10, -10,  -5,  -5, -10, -10, -20
        },
        // White King (middle game)
        {
             20,  20,   0,   0,   0,   0,  20,  20,
            20,  30,  10,   0,   0,  10,  30,  20,
            -20, -30, -30, -40, -40, -30, -30, -20,
            -10, -20, -20, -20, -20, -20, -20, -10,
           -30, -40, -40, -50, -50, -40, -40, -30,
            -30, -40, -40, -50, -50, -40, -40, -30,
            -30, -40, -40, -50, -50, -40, -40, -30,
            -30, -40, -40, -50, -50, -40, -40, -30,
        }
    };

    // pawn structure
    static constexpr int doubled_pawns = 1; 
    static constexpr int blocked_pawns = 1; 
    static constexpr int iso_pawns = 1;

    // logistics
    static constexpr int mobility = 1;

    static const std::unordered_map<std::string, int> evalWeights; 
    static std::unordered_map<std::string, int> componentEvals;

    static int Evaluate(const MoveGenerator* movegen, const Board* position); // run all
    // eval is scaled to centipawn (100=1) and converted to int
    static int computeEval(const MoveGenerator* movegen, const Board& board); // add up below w/ weights
    static std::unordered_map<std::string, int> computeComponents(const MoveGenerator* movegen, const Board& board); // return dict constructed in above but on command
    static float evaluateComponent(const MoveGenerator* movegen, const Board& board, std::string component); // iterate through names and run below funcs

    // counts (weights are in above)
    static float materialDifferences(const Board& position); // add up material
    static float pawnStructureDifferences(const Board& position); // double, block, iso (more is bad)
    static float mobilityDifferences(const MoveGenerator* movegen); // # moves (psuedo possibilities)
    static float positionDifferences(const Board& position); // pst differences

    // helpers
    static int countDoubledPawns(U64 pawns); // via file masks
    static int countIsolatedPawns(U64 pawns); // via file masks
};

#endif