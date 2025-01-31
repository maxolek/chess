// Precomputed blank board attack masks for all the pieces

#ifndef PRECOMPUTEDMOVEDATA_H
#define PRECOMPUTEDMOVEDATA_H

#include "bits.h"

class PrecomputedMoveData {
private:
public:
    U64 blankPawnMoves[64][2];
    U64 fullPawnAttacks[64][2];
    SMasks blankBishopAttacks[64][2]; // lower, upper, lineEx=lower|upper // direction
    SMasks blankRookAttacks[64][2]; // lower, upper, lineEx=lower|upper // direction
    SMasks blankQueenAttacks[64][4]; // bishop | rook // direction
    U64 blankKnightAttacks[64];
    U64 blankKingAttacks[64];

    U64 rayMasks[64][64]; // square_a, square_b
    U64 alignMasks[64][64]; // square_a, square_b 
    int distToEdge[64][8]; // square, direction (cardinal)

    PrecomputedMoveData();

    void generateFullPawnMoves();
    void generateFullPawnAttacks();

    void generateBlankKnightAttacks();
    void generateBlankKingAttacks();
    void generateBlankBishopAttacks();
    void generateBlankRookAttacks();
    void generateBlankQueenAttacks();
    // may be useful to have double array of bitboards with line connecting square a&b
    //      useful for MoveGenerator::isPinned()

    // straight line mask that contains the entire line containing square a&b
    void generateAlignMasks();
    // ray between 2 squares (if squares are not connected by straight line, empty bitboard)
    void generateRayMasks();
    // direction = N, NE, E, SE, S, SW, W, NW
    void generateDistToEdge(int direction_idx, int square);

};

#endif