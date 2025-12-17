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
#include <fstream>
#include <iostream>
#include <cstring>
#include <algorithm>
#include <cassert>
#include <windows.h>
#include <mutex>

typedef uint64_t U64;
typedef unsigned short ushort;

constexpr const char* STARTPOS_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
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

// LOGS CONTROL

struct Logging {
    static inline bool track_timers = false; 
    static inline bool track_search_stats = true;
    static inline bool track_game_log = true; 
    static inline bool track_uci = true;
    static inline std::string log_dir = "../logs";

    static void toggleTimers() {track_timers = !track_timers;}
    static void toggleSearchStats() {track_search_stats = !track_search_stats;}
    static void toggleGameLog() {track_game_log = !track_game_log;}
    static void toggleUCI() {track_uci = !track_uci;}
    static void setLogDir(std::string path) {log_dir = path;}

    static void disableAll() {
        track_timers = false;
        track_search_stats = false;
        track_game_log = false;
        track_uci = true;
    }

    static void enableAll() {
        track_timers = true;
        track_search_stats = true;
        track_game_log = true;
        track_uci = true;
    }
};

inline void logEventRaw(const std::string& line) {
    static std::mutex mtx;
    static std::ofstream out("../logs/all_logs.jsonl", std::ios::app);

    if (!out.is_open())
        return;

    std::lock_guard<std::mutex> lock(mtx);
    out << line << '\n';
    out.flush();
}

extern std::string file_char;
extern std::string results_string[];

// set/get/pop macros
inline void set_bit(U64& bitboard, int square) {
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

// integer sqrt
inline int isqrt(int x) {
    int r = static_cast<int>(std::sqrt(x));
    return r;
}

// function to print a biboard to the console
void print_bitboard(U64 bitboard);

// big-endianness reader
uint64_t read_u64_be(std::ifstream &file);
uint32_t read_u32_be(std::ifstream &file);
uint16_t read_u16_be(std::ifstream &file);


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
int getMSB(U64 x);
// function to get trailing zero count
int getLSB(U64 x);
// get 0-63 square index of bitboard with only 1 bit set
int sqidx(U64 bitboard);
// Flip square for black piece evaluation
int mirror(int square);
// masks
U64 bitsBelow(int sq);
U64 bitsAbove(int sq);
// iterate through bitboard and perform func
template<typename Func>
inline void forEachBit(U64 bb, Func func) {
    int sq;
    while (bb) {
        sq = getLSB(bb);
        bb &= bb -1;
        func(sq);
    }
}

//int countPosDuplicates(std::vector<U64> &vec);

char piece_label(int piece);
int piece_int(char piece);
std::string square_to_algebraic(int square);
int algebraic_to_square(std::string square);

// print move in algebraic notation
//void print_algebraic_move(Move move);

#endif 