// precomputed bitboards

#ifndef BITS_H
#define BITS_H

#include "helpers.h"

class Bits {
private:
public:
    static constexpr U64 fullSet = 0xFFFFFFFFFFFFFFFF;

    // init board
    static constexpr U64 initWhite = 0x000000000000FF00ULL | 0x0000000000000042ULL | 0x0000000000000024ULL | 0x0000000000000081ULL | 0x0000000000000010ULL | 0x0000000000000008ULL;  
    static constexpr U64 initBlack = 0x00FF000000000000ULL | 0x1000000000000000ULL | 0x4200000000000000ULL | 0x2400000000000000ULL | 0x0800000000000000ULL | 0x8100000000000000ULL; 
    static constexpr U64 initPawns = 0x000000000000FF00ULL | 0x00FF000000000000ULL;  
    static constexpr U64 initKnights = 0x4200000000000000ULL | 0x0000000000000042ULL; 
    static constexpr U64 initBishops = 0x2400000000000000ULL | 0x0000000000000024ULL;  
    static constexpr U64 initRooks = 0x8100000000000000ULL | 0x0000000000000081ULL;   
    static constexpr U64 initQueens = 0x0800000000000000ULL | 0x0000000000000008ULL; 
    static constexpr U64 initKings = 0x1000000000000000ULL | 0x0000000000000010ULL; 

    // castle masks
    static constexpr U64 whiteKingsideMask = 1ULL << f1 | 1ULL << g1;
    static constexpr U64 whiteQueensideMask = 1ULL << d1 | 1ULL << c1;
    static constexpr U64 whiteQueensideMaskExt = whiteQueensideMask  | 1ULL << b1;
    static constexpr U64 blackKingsideMask = 1ULL << f8 | 1ULL << g8;
    static constexpr U64 blackQueensideMask = 1ULL << d8 | 1ULL << c8;
    static constexpr U64 blackQueensideMaskExt = blackQueensideMask | 1ULL << b8;


    // file-rank masks
    static constexpr U64 mask_a_file = 0x0101010101010101ULL; 
    static constexpr U64 mask_b_file = 0x0202020202020202ULL;
    static constexpr U64 mask_g_file = 0x4040404040404040ULL;
    static constexpr U64 mask_h_file = 0x8080808080808080ULL; 
    static constexpr U64 mask_rank_2 = 0x000000000000FF00ULL;
    static constexpr U64 mask_rank_7 = 0x00FF000000000000ULL;
};

#endif