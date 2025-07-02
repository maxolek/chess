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


    // file masks
    static constexpr U64 file_masks[8] = {
    0x0101010101010101ULL,
    0x0202020202020202ULL,
    0x0404040404040404ULL,
    0x0808080808080808ULL,
    0x1010101010101010ULL,
    0x2020202020202020ULL,
    0x4040404040404040ULL,
    0x8080808080808080ULL
};
    // rank
    static constexpr U64 mask_rank_2 = 0x000000000000FF00ULL;
    static constexpr U64 mask_rank_7 = 0x00FF000000000000ULL;

    // colors masks
    static constexpr U64 light_squares_mask = 0x55AA55AA55AA55AAULL;
    static constexpr U64 dark_squares_mask  = 0xAA55AA55AA55AA55ULL;
};

#endif