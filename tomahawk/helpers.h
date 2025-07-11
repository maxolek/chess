#ifndef HELPERS_H
#define HELPERS_H

#include <fstream>
#include <iostream>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <vector>
#include <string>
#include <stack>
#include <unordered_map>
#include <memory>
#include <cstdlib>  
#include <ctime>  
#include <random> 
#include <cctype>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <limits>

typedef uint64_t U64;
typedef unsigned short ushort;

constexpr const char* STARTPOS_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
const float INF = std::numeric_limits<float>::infinity();
constexpr int MATE_SCORE = 100000;
constexpr int MAX_DEPTH = 64;
constexpr int MAX_MOVES = 256; 

// board squares
enum {
    a1, b1, c1, d1, e1, f1, g1, h1,
    a2, b2, c2, d2, e2, f2, g2, h2,
    a3, b3, c3, d3, e3, f3, g3, h3,
    a4, b4, c4, d4, e4, f4, g4, h4,
    a5, b5, c5, d5, e5, f5, g5, h5,
    a6, b6, c6, d6, e6, f6, g6, h6,
    a7, b7, c7, d7, e7, f7, g7, h7,
    a8, b8, c8, d8, e8, f8, g8, h8
};

// sides to move
enum {white, black};

// pieces (sorted by value)
enum {pawn, knight, bishop, rook, queen, king};

enum Result
{
    NotStarted,
    InProgress,
    WhiteIsMated,
    BlackIsMated,
    Stalemate,
    Repetition,
    FiftyMoveRule,
    InsufficientMaterial,
    DrawByArbiter,
    WhiteTimeout,
    BlackTimeout,
    WhiteIllegalMove,
    BlackIllegalMove
};

extern std::string file_char;
extern std::string results_string[];

// set/get/pop macros
inline void set_bit(U64 &bitboard, int square) {
    bitboard |= (1ULL << square);
}
inline int get_bit(U64 bitboard, int square) {
    return (bitboard >> square) & 1;
}
// Inline function to pop the bit for a given square
inline void pop_bit(U64 &bitboard, int square) {
    bitboard &= ~(1ULL << square);
}

// c++ 14 or later has this in std::
template<typename T>
constexpr const T& clamp(const T& v, const T& lo, const T& hi) {
    return (v < lo) ? lo : (hi < v) ? hi : v;
}

// function to print a biboard to the console
void print_bitboard(U64 bitboard);

// struct for sliding piece rays
struct SMasks {
    U64 lower; // from lower idx square -> piece
    U64 upper; // from piece -> upper idx square
    U64 lineEx; // lower | upper
};

// for UCI game settings
struct SearchSettings {
    int wtime = 0;
    int btime = 0;
    int winc = 0;
    int binc = 0;
    int movestogo = 0;
    int depth = -1;
    int nodes = -1;
    int movetime = -1;
    bool infinite = false;
};

// max of rank,file
int chebyshev_distance(int sq1, int sq2);

// function to get the cardinal direction from start_square -> target_square
//      clockwise from N indexing
std::pair<int,int> direction_map(int start_square, int target_square);
int direction_index(int start_square, int target_square);
// get relevant bits
U64 isolateLSB(U64 x);
U64 isolateMSB(U64 x);
// function to count the number of set bits
int countBits(U64 x);
// function to get leading zero count
int lzcnt(U64 x);
// function to get trailing zero count
int tzcnt(U64 x);
// get 0-63 square index of bitboard with only 1 bit set
int sqidx(U64 bitboard);
// Flip square for black piece evaluation
int mirror(int square);

//int countPosDuplicates(std::vector<U64> &vec);

char piece_label(int piece);
int piece_int(char piece);
std::string square_to_algebraic(int square);
int algebraic_to_square(std::string square);

// print move in algebraic notation
//void print_algebraic_move(Move move);

#endif 