#include <magics.h>

namespace Magics {
    U64 rookAttackTable[64][4096]; // size must be 2^maxBits = 2^12
    U64 bishopAttackTable[64][512]; // size must be 2^maxBits = 2^9
    U64 rookMasks[64];
    U64 bishopMasks[64];
    U64 rookMagics[64];
    U64 bishopMagics[64];
    int rookShifts[64];
    int bishopShifts[64];c

    // Known-good magic numbers (from Surge engine, public domain)
    // These work at standard bit counts (popcount of mask)
    static constexpr U64 ROOK_MAGICS_CONST[64] = {
        0x0080001020400080ULL, 0x0040001000200040ULL, 0x0080081000200080ULL, 0x0080040800100080ULL,
        0x0080020400080080ULL, 0x0080010200040080ULL, 0x0080008001000200ULL, 0x0080002040800100ULL,
        0x0000800020400080ULL, 0x0000400020005000ULL, 0x0000801000200080ULL, 0x0000800800100080ULL,
        0x0000800400080080ULL, 0x0000800200040080ULL, 0x0000800100020080ULL, 0x0000800040800100ULL,
        0x0000208000400080ULL, 0x0000404000201000ULL, 0x0000808010002000ULL, 0x0000808008001000ULL,
        0x0000808004000800ULL, 0x0000808002000400ULL, 0x0000010100020004ULL, 0x0000020000408104ULL,
        0x0000208080004000ULL, 0x0000200040005000ULL, 0x0000100080200080ULL, 0x0000080080100080ULL,
        0x0000040080080080ULL, 0x0000020080040080ULL, 0x0000010080800200ULL, 0x0000800080004100ULL,
        0x0000204000800080ULL, 0x0000200040401000ULL, 0x0000100080802000ULL, 0x0000080080801000ULL,
        0x0000040080800800ULL, 0x0000020080800400ULL, 0x0000020001010004ULL, 0x0000800040800100ULL,
        0x0000204000808000ULL, 0x0000200040008080ULL, 0x0000100020008080ULL, 0x0000080010008080ULL,
        0x0000040008008080ULL, 0x0000020004008080ULL, 0x0000010002008080ULL, 0x0000004081020004ULL,
        0x0000204000800080ULL, 0x0000200040008080ULL, 0x0000100020008080ULL, 0x0000080010008080ULL,
        0x0000040008008080ULL, 0x0000020004008080ULL, 0x0000800100020080ULL, 0x0000800041000080ULL,
        0x00FFFCDDFCED714AULL, 0x007FFCDDFCED714AULL, 0x003FFFCDFFD88096ULL, 0x0000040810002101ULL,
        0x0001000204080011ULL, 0x0001000204000801ULL, 0x0001000082000401ULL, 0x0001FFFAABFAD1A2ULL
    };

    static constexpr U64 BISHOP_MAGICS_CONST[64] = {
        0x0002020202020200ULL, 0x0002020202020000ULL, 0x0004010202000000ULL, 0x0004040080000000ULL,
        0x0001104000000000ULL, 0x0000821040000000ULL, 0x0000410410400000ULL, 0x0000104104104000ULL,
        0x0000040404040400ULL, 0x0000020202020200ULL, 0x0000040102020000ULL, 0x0000040400800000ULL,
        0x0000011040000000ULL, 0x0000008210400000ULL, 0x0000004104104000ULL, 0x0000002082082000ULL,
        0x0004000808080800ULL, 0x0002000404040400ULL, 0x0001000202020200ULL, 0x0000800802004000ULL,
        0x0000800400A00000ULL, 0x0000200100884000ULL, 0x0000400082082000ULL, 0x0000200041041000ULL,
        0x0002080010101000ULL, 0x0001040008080800ULL, 0x0000208004010400ULL, 0x0000404004010200ULL,
        0x0000840000802000ULL, 0x0000404002011000ULL, 0x0000808001041000ULL, 0x0000404000820800ULL,
        0x0001041000202000ULL, 0x0000820800101000ULL, 0x0000104400080800ULL, 0x0000020080080080ULL,
        0x0000404040040100ULL, 0x0000808100020100ULL, 0x0001010100020800ULL, 0x0000808080010400ULL,
        0x0000820820004000ULL, 0x0000410410002000ULL, 0x0000082088001000ULL, 0x0000002011000800ULL,
        0x0000080100400400ULL, 0x0001010101000200ULL, 0x0002020202000400ULL, 0x0001010101000200ULL,
        0x0000410410400000ULL, 0x0000208208200000ULL, 0x0000002084100000ULL, 0x0000000020880000ULL,
        0x0000001002020000ULL, 0x0000040408020000ULL, 0x0004040404040000ULL, 0x0002020202020000ULL,
        0x0000104104104000ULL, 0x0000002082082000ULL, 0x0000000020841000ULL, 0x0000000000208800ULL,
        0x0000000010020200ULL, 0x0000000404080200ULL, 0x0000040404040400ULL, 0x0002020202020200ULL
    };
    
    void initMagics() {
        for (int sq = 0; sq < 64; ++sq) {
            // Rook
            rookMasks[sq] = maskRook(sq);
            int rookBits = __builtin_popcountll(rookMasks[sq]);
            rookShifts[sq] = 64 - rookBits;
            rookMagics[sq] = ROOK_MAGICS_CONST[sq];

            std::vector<U64> occupancies = generateAllOccupancies(rookMasks[sq]);
            for (size_t i = 0; i < occupancies.size(); ++i) {
                size_t index = (occupancies[i] * rookMagics[sq]) >> rookShifts[sq];
                rookAttackTable[sq][index] = rookAttacksOnTheFly(sq, occupancies[i]);
            }

            // Bishop
            bishopMasks[sq] = maskBishop(sq);
            int bishopBits = __builtin_popcountll(bishopMasks[sq]);
            bishopShifts[sq] = 64 - bishopBits;
            bishopMagics[sq] = BISHOP_MAGICS_CONST[sq];

            std::vector<U64> bOccupancies = generateAllOccupancies(bishopMasks[sq]);
            for (size_t i = 0; i < bOccupancies.size(); ++i) {
                size_t index = (bOccupancies[i] * bishopMagics[sq]) >> bishopShifts[sq];
                bishopAttackTable[sq][index] = bishopAttacksOnTheFly(sq, bOccupancies[i]);
            }
        }
    }


    // relevant occupancy masks
    U64 maskRook(int sq) {
        U64 mask = 0ULL;
        int rank = sq / 8;
        int file = sq % 8;

        // Horizontal (left to right, excluding edges)
        for (int f = file + 1; f <= 6; f++) mask |= (1ULL << (rank * 8 + f));
        for (int f = file - 1; f >= 1; f--) mask |= (1ULL << (rank * 8 + f));

        // Vertical (up and down, excluding edges)
        for (int r = rank + 1; r <= 6; r++) mask |= (1ULL << (r * 8 + file));
        for (int r = rank - 1; r >= 1; r--) mask |= (1ULL << (r * 8 + file));

        return mask;
    }
    U64 maskBishop(int sq) {
        U64 mask = 0ULL;
        int rank = sq / 8;
        int file = sq % 8;

        // Diagonal ↘↖ (exclude edges)
        for (int r = rank + 1, f = file + 1; r <= 6 && f <= 6; ++r, ++f)
            mask |= (1ULL << (r * 8 + f));
        for (int r = rank - 1, f = file - 1; r >= 1 && f >= 1; --r, --f)
            mask |= (1ULL << (r * 8 + f));

        // Diagonal ↙↗ (exclude edges)
        for (int r = rank + 1, f = file - 1; r <= 6 && f >= 1; ++r, --f)
            mask |= (1ULL << (r * 8 + f));
        for (int r = rank - 1, f = file + 1; r >= 1 && f <= 6; --r, ++f)
            mask |= (1ULL << (r * 8 + f));

        return mask;
    }

    // all possible blocker combinations for a mask
    std::vector<U64> generateAllOccupancies(U64 mask) {
        std::vector<U64> occupancies;
        size_t numBits = static_cast<size_t>(__builtin_popcountll(mask));  // number of bits set in the mask

        std::vector<int> bitPositions;
        for (int i = 0; i < 64; ++i) {
            if ((mask >> i) & 1ULL) {
                bitPositions.push_back(i);
            }
        }

        int permutations = 1 << numBits;  // 2^numBits

        for (int i = 0; i < permutations; ++i) {
            U64 occupancy = 0ULL;
            for (size_t j = 0; j < numBits; ++j) {
                if ((i >> j) & 1) {
                    occupancy |= (1ULL << bitPositions[j]);
                }
            }
            occupancies.push_back(occupancy);
        }

        return occupancies;
    }


    // generate attacks (bitboard) ... brute force
    U64 rookAttacksOnTheFly(int sq, U64 blockers) {
        U64 attacks = 0ULL;
        int r = sq / 8, f = sq % 8;

        // Right
        for (int ff = f + 1; ff <= 7; ff++) {
            int s = r * 8 + ff;
            attacks |= (1ULL << s);
            if (blockers & (1ULL << s)) break;
        }
        // Left
        for (int ff = f - 1; ff >= 0; ff--) {
            int s = r * 8 + ff;
            attacks |= (1ULL << s);
            if (blockers & (1ULL << s)) break;
        }
        // Up
        for (int rr = r + 1; rr <= 7; rr++) {
            int s = rr * 8 + f;
            attacks |= (1ULL << s);
            if (blockers & (1ULL << s)) break;
        }
        // Down
        for (int rr = r - 1; rr >= 0; rr--) {
            int s = rr * 8 + f;
            attacks |= (1ULL << s);
            if (blockers & (1ULL << s)) break;
        }

        return attacks;
    }
    U64 bishopAttacksOnTheFly(int sq, U64 blockers) {
        U64 attacks = 0ULL;
        int rank = sq / 8;
        int file = sq % 8;

        // ↗ Northeast
        for (int r = rank + 1, f = file + 1; r <= 7 && f <= 7; ++r, ++f) {
            int s = r * 8 + f;
            attacks |= (1ULL << s);
            if (blockers & (1ULL << s)) break;
        }

        // ↘ Southeast
        for (int r = rank - 1, f = file + 1; r >= 0 && f <= 7; --r, ++f) {
            int s = r * 8 + f;
            attacks |= (1ULL << s);
            if (blockers & (1ULL << s)) break;
        }

        // ↙ Southwest
        for (int r = rank - 1, f = file - 1; r >= 0 && f >= 0; --r, --f) {
            int s = r * 8 + f;
            attacks |= (1ULL << s);
            if (blockers & (1ULL << s)) break;
        }

        // ↖ Northwest
        for (int r = rank + 1, f = file - 1; r <= 7 && f >= 0; ++r, --f) {
            int s = r * 8 + f;
            attacks |= (1ULL << s);
            if (blockers & (1ULL << s)) break;
        }

        return attacks;
    }

    // generate attacks ... magics
    U64 rookAttacks(int sq, U64 occ) {
        // Multiply blockers by magic number and shift to get index into attack table
        size_t index = ((occ & rookMasks[sq]) * rookMagics[sq]) >> rookShifts[sq];
        return rookAttackTable[sq][index];
    }

    U64 bishopAttacks(int sq, U64 occ) {
        size_t index = ((occ & bishopMasks[sq]) * bishopMagics[sq]) >> bishopShifts[sq];
        return bishopAttackTable[sq][index];
    }
}