#include "magics.h"

namespace Magics {
    U64 rookAttackTable[64][4096]; // size must be 2^maxBits = 2^12
    U64 bishopAttackTable[64][512]; // size must be 2^maxBits = 2^9
    U64 rookMasks[64];
    U64 bishopMasks[64];
    U64 rookMagics[64];
    U64 bishopMagics[64];
    int rookShifts[64];
    int bishopShifts[64];
    
    void initMagics() {
        for (int sq = 0; sq < 64; ++sq) {
            // Rook
            rookMasks[sq] = maskRook(sq);
            int rookBits = __builtin_popcountll(rookMasks[sq]);
            rookShifts[sq] = 64 - rookBits;

            std::vector<U64> occupancies = generateAllOccupancies(rookMasks[sq]);
            std::vector<U64> attacks(occupancies.size());
            for (size_t i = 0; i < occupancies.size(); ++i) {
                attacks[i] = rookAttacksOnTheFly(sq, occupancies[i]);
            }

            while (true) {
                U64 magic = generateCandidateMagic();
                std::vector<U64> used(1ULL << rookBits, 0ULL);
                bool fail = false;

                for (size_t i = 0; i < occupancies.size(); ++i) {
                    size_t index = (occupancies[i] * magic) >> (64 - rookBits);
                    if (used[index] == 0ULL) {
                        used[index] = attacks[i];
                    } else if (used[index] != attacks[i]) {
                        fail = true;
                        break;
                    }
                }

                if (!fail) {
                    rookMagics[sq] = magic;
                    for (size_t i = 0; i < occupancies.size(); ++i) {
                        size_t index = (occupancies[i] * magic) >> (64 - rookBits);
                        rookAttackTable[sq][index] = attacks[i];
                    }
                    break;
                }
            }

            // Bishop
            bishopMasks[sq] = maskBishop(sq);
            int bishopBits = __builtin_popcountll(bishopMasks[sq]);
            bishopShifts[sq] = 64 - bishopBits;

            std::vector<U64> bOccupancies = generateAllOccupancies(bishopMasks[sq]);
            std::vector<U64> bAttacks(bOccupancies.size());
            for (size_t i = 0; i < bOccupancies.size(); ++i) {
                bAttacks[i] = bishopAttacksOnTheFly(sq, bOccupancies[i]);
            }

            while (true) {
                U64 magic = generateCandidateMagic();
                std::vector<U64> used(1ULL << bishopBits, 0ULL);
                bool fail = false;

                for (size_t i = 0; i < bOccupancies.size(); ++i) {
                    size_t index = (bOccupancies[i] * magic) >> (64 - bishopBits);
                    if (used[index] == 0ULL) {
                        used[index] = bAttacks[i];
                    } else if (used[index] != bAttacks[i]) {
                        fail = true;
                        break;
                    }
                }

                if (!fail) {
                    bishopMagics[sq] = magic;
                    for (size_t i = 0; i < bOccupancies.size(); ++i) {
                        size_t index = (bOccupancies[i] * magic) >> (64 - bishopBits);
                        bishopAttackTable[sq][index] = bAttacks[i];
                    }
                    break;
                }
            }
        }

        /*for (int sq = 0; sq < 64; ++sq) {
            for (U64 occ : generateAllOccupancies(rookMasks[sq])) {
                int index = (occ * rookMagics[sq]) >> rookShifts[sq];
                if (rookAttackTable[sq][index] != rookAttacksOnTheFly(sq, occ)) {
                    std::cerr << "Mismatch at rook square " << sq << std::endl;
                }
                if (bishopAttackTable[sq][index] != bishopAttacksOnTheFly(sq, occ)) {
                    std::cerr << "Mismatch at bishop square " << sq << std::endl;
                }
            }
        }*/
    }


    // brute force
    bool findMagic(int sq, int bits, bool rook, Magic& outMagic) {
        U64 mask = rook ? maskRook(sq) : maskBishop(sq);
        std::vector<U64> occupancies = generateAllOccupancies(mask);
        std::vector<U64> attacks(occupancies.size());

        for (size_t i = 0; i < occupancies.size(); ++i) {
            attacks[i] = rook
                ? rookAttacksOnTheFly(sq, occupancies[i])
                : bishopAttacksOnTheFly(sq, occupancies[i]);
        }

        for (int trial = 0; trial < 1000000; ++trial) {
            U64 magic = generateCandidateMagic(); // You need a good generator
            std::vector<U64> table(1ULL << bits, 0ULL);
            bool fail = false;

            for (size_t i = 0; i < occupancies.size(); ++i) {
                size_t index = (occupancies[i] * magic) >> (64 - bits);
                if (table[index] == 0ULL) {
                    table[index] = attacks[i];
                } else if (table[index] != attacks[i]) {
                    fail = true;
                    break;
                }
            }

            if (!fail) {
                outMagic = { mask, magic, 64 - bits, new U64[1ULL << bits] };
                for (size_t i = 0; i < occupancies.size(); ++i) {
                    size_t index = (occupancies[i] * magic) >> (64 - bits);
                    outMagic.attacks[index] = attacks[i];
                }
                return true;
            }
        }

        return false;
    }

    
    U64 randomU64() {
        //std::mt19937_64 rng(time(0));
        return rng();
    }

    U64 generateCandidateMagic() {
        // AND multiple randoms to get sparse number, which is typical for magic numbers
        return randomU64() & randomU64() & randomU64();
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