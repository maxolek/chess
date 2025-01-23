#include "helpers.h"

// print bitboard
void print_bitboard(U64 bitboard) {
    // loop over board ranks
    for (int rank = 7; rank >= 0; rank--) {
        // loop over board files
        for (int file = 0; file < 8; file++) {
            // init square
            int square = rank * 8 + file;

            // print ranks
            if (!file)
                std::cout << rank + 1 << "\t";

            // print bit state 
            std::cout << " " << (get_bit(bitboard, square)) ? 1 : 0;
        }
        // print new line every rank
        std::cout << "\n";
    }

    // print board files
    std::cout << "\n\t a b c d e f g h";

    // print bitboard as unsigned decimal number
    std::cout << "\n\nBitboard:\t" << bitboard << std::endl;
}

//                 N,   NE,    E,    SE,    S,     SW,     W,     NW
// (rank,file) = (1,0) (1,1) (0,1) (-1,1) (-1,0) (-1,-1) (0,-1) (1,-1)
int direction_idx(int start_square, int target_square) {
    int rank_dir = std::copysign(1.0,target_square/8 - start_square/8);
    int file_dir = std::copysign(1.0,target_square%8 - start_square%8);
    // 0 direction means no movement

    switch (rank_dir) {
        case -1:
            switch (file_dir) {
                case -1: 
                    return 5;
                    break;
                case 0: 
                    return 4;
                    break;
                case 1: 
                    return 3;
                    break;
            }
        case 0:
            switch (file_dir) {
                case -1: 
                    return 6;
                    break;
                case 0: 
                    return -1; // start=target
                    break;
                case 1: 
                    return 2;
                    break;
            }
        case 1:
            switch (file_dir) {
                case -1: 
                    return 7;
                    break;
                case 0: 
                    return 0;
                    break;
                case 1: 
                    return 1;
                    break;
                break;
            }
        default:
            return -1;
            break;
    }
}

// rank, file direction map
std::pair<int,int> direction_map(int direction_idx) {
    switch (direction_idx) {
        case -1: 
            return std::make_pair(0,0);
            break;
        case 0: 
            return std::make_pair(1,0);
            break;
        case 1: 
            return std::make_pair(1,1);
            break;
        case 2: 
            return std::make_pair(0,1);
            break;
        case 3: 
            return std::make_pair(-1,1);
            break;
        case 4: 
            return std::make_pair(-1,0);
            break;
        case 5: 
            return std::make_pair(-1,-1);
            break;
        case 6: 
            return std::make_pair(0,-1);
            break;
        case 7: 
            return std::make_pair(1,-1);
            break;
        default:
            return std::make_pair(0,0);
            break;
    }
}

int countBits(U64 x) {
    return __builtin_popcountll(x);
}


int lzcnt(U64 x) {
    return __builtin_clzll(x);
}

int tzcnt(U64 x) {
    return __builtin_ffsll(x);
}

// Function to get the square index from a bitboard with only one bit set
int sqidx(U64 bitboard) {
    // Assuming there is only one bit set
    return __builtin_ctzll(bitboard);  // __builtin_ctzll is for GCC/Clang
}

/* for repetition detection
int countPosDuplicates(std::vector<U64[6]> &vec) {
    std::unordered_map<U64, int> cnt_map;
    int max_pos_dup = 0;

    for (U64[6] &pos : vec) {
        cnt_map[pos]++;
        if (cnt_map[pos] > max_pos_dup)
            max_pos_dup++;
    }

    return max_pos_dup;
}
*/

// Converts a square index (0-63) to algebraic notation (a1-h8)
std::string square_to_algebraic(int square) {
    char file = 'a' + (square % 8); // Get file (column)
    char rank = '1' + (square / 8); // Get rank (row)
    return std::string(1, file) + rank; // Combine file and rank
}

// convert an algebraic move (a-h,0-7) to square index (0-63)
int algebraic_to_square(std::string square) {
    char file = square[0];
    char rank = square[1];
    // Convert file to 0-based index (0 for 'a', 1 for 'b', ..., 7 for 'h')
    int file_index = file - 'a';
    // Convert rank to 0-based index (0 for '1', 1 for '2', ..., 7 for '8')
    int rank_index = rank - '1';
    // Calculate the index for the 1D array representing the board
    int index = (rank_index * 8) + file_index;
    return index;
}

// prints a given move in algebraic notation
/*
void print_algebraic_move(Move move){
    char piece_names[5] = {'N','B','R','Q','K'};
    std::string move_string = "";

    // if pawn, dont print piece name
    if (move.piece) 
        move_string += piece_names[move.piece -1];
    if (move.captured_piece != -1) {
        if (!move.piece) 
            move_string += square_to_algebraic(move.from)[0];
        move_string += "x";
    }
    move_string += square_to_algebraic(move.to);
    if (move.promoted_piece != -1) {
        move_string += "=";
        move_string += piece_names[move.promoted_piece];
    }
    if (move.is_check)
        move_string += "+";

    std::cout << move_string << std::endl;
}
*/