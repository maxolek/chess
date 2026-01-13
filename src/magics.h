#ifndef MAGICS_H
#define MAGICS_H

#include "helpers.h"

namespace Magics {
    struct Magic {
        U64 mask;       // Occupancy mask (relevant blockers)
        U64 magic;      // Magic number
        int shift;      // Shift value (64 - numBits in mask)
        U64* attacks;   // Pointer to attack table
    };

    static std::mt19937_64 rng(std::random_device{}());

    extern U64 rookAttackTable[64][4096]; // precompute and store
    extern U64 bishopAttackTable[64][512];
    extern U64 rookMasks[64];
    extern U64 bishopMasks[64];
    extern U64 rookMagics[64];
    extern U64 bishopMagics[64];
    extern int rookShifts[64];
    extern int bishopShifts[64];

    void initMagics(); // call at engine startup
    bool findMagic(int sq, int bits, bool rook, Magic& outMagic);
    U64 randomU64();
    U64 generateCandidateMagic();
    U64 maskRook(int sq);
    U64 maskBishop(int sq);
    std::vector<U64> generateAllOccupancies(U64 mask);
    U64 rookAttacksOnTheFly(int sq, U64 blockers);
    U64 bishopAttacksOnTheFly(int sq, U64 blockers);
    U64 rookAttacks(int sq, U64 occ);
    U64 bishopAttacks(int sq, U64 occ);
}

#endif