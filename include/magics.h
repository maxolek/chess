#ifndef MAGICS_H
#define MAGICS_H

#include "helpers.h"

namespace Magics {
    extern U64 rookAttackTable[64][4096]; // precompute and store
    extern U64 bishopAttackTable[64][512];
    extern U64 rookMasks[64];
    extern U64 bishopMasks[64];
    extern U64 rookMagics[64];
    extern U64 bishopMagics[64];
    extern int rookShifts[64];
    extern int bishopShifts[64];

    void initMagics(); // call at engine startup
    U64 maskRook(int sq);
    U64 maskBishop(int sq);
    std::vector<U64> generateAllOccupancies(U64 mask);
    U64 rookAttacksOnTheFly(int sq, U64 blockers);
    U64 bishopAttacksOnTheFly(int sq, U64 blockers);
    U64 rookAttacks(int sq, U64 occ);
    U64 bishopAttacks(int sq, U64 occ);
}

#endif