// Precomputed blank board attack masks for all the pieces

#ifndef PRECOMPUTEDMOVEDATA_H
#define PRECOMPUTEDMOVEDATA_H

#include "bits.h"

class PrecomputedMoveData {
private:
public:
    static U64 blankPawnMoves[64][2];
    static U64 fullPawnAttacks[64][2];
    static SMasks blankBishopAttacks[64][2]; // lower, upper, lineEx=lower|upper // direction
    static SMasks blankRookAttacks[64][2]; // lower, upper, lineEx=lower|upper // direction
    static SMasks blankQueenAttacks[64][4]; // bishop | rook // direction
    static U64 blankKnightAttacks[64];
    static U64 blankKingAttacks[64];

    static U64 passedPawnMasks[64][2]; // sq, white/black

    static U64 rayMasks[64][64]; // square_a, square_b
    static U64 alignMasks[64][64]; // square_a, square_b 
    static int distToEdge[64][8]; // square, direction (cardinal)
    static int king_move_distances[64][64]; // chebyshev (max of rank/file)

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

    // used for eval, looks are sqaures ahead of square for side to be used
    // to see if void of enemy pawns
    void generatePassedPawnsMasks();
    // chebyshev distance (king moves)
    void generate_king_distances();

    // straight line mask that contains the entire line containing square a&b
    // if squares are not connected by a straight line, empty bitboard
    void generateAlignMasks();
    // ray between 2 squares (if squares are not connected by straight line, empty bitboard)
    void generateRayMasks();
    // direction = N, NE, E, SE, S, SW, W, NW
    void generateDistToEdge();

};

#endif