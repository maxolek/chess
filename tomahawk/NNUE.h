#pragma once

#include <vector>
#include <cstdint>
#include <string>

// Forward declarations from your engine
class Board;
struct Move;


// ============================================================
// Constants
// ============================================================

// HalfKP: king square × (piece × square)
constexpr int NUM_PIECE_TYPES   = 6;    // pawn..king
constexpr int NUM_COLORS        = 2;    // white, black
constexpr int NUM_SQUARES       = 64;
constexpr int NUM_PIECE_SQ      = NUM_PIECE_TYPES * NUM_COLORS * NUM_SQUARES; // 768
constexpr int INPUT_DIM         = NUM_SQUARES * NUM_PIECE_SQ;                 // 49152

// Hidden layer size (tune this to your net, e.g. 256 or 512)
constexpr int HIDDEN_DIM        = 256;
constexpr int MID_DIM           = 32;

// ============================================================
// Helper: Feature Indexing
// ============================================================

inline int feature_index(int king_sq, int piece, int sq, int color) {
    // piece ∈ {0..5}, color ∈ {0,1}, sq ∈ {0..63}, king_sq ∈ {0..63}
    int piece_index    = color * NUM_PIECE_TYPES + piece;        // 0..11
    int piece_sq_index = piece_index * NUM_SQUARES + sq;         // 0..767
    return king_sq * NUM_PIECE_SQ + piece_sq_index;              // 0..49151
}

// ============================================================
// Accumulator: stores hidden layer pre-activations
// ============================================================

struct Accumulator {
    std::vector<int32_t> hidden; // pre-activation values (HIDDEN_DIM)

    Accumulator();

    // Recompute fully from board state
    void build(const Board& board, int king_sq, const std::vector<int16_t>& weights, const std::vector<int32_t>& biases);

    // Incremental update for a single piece move
    void update(int king_sq,
                int piece, int from_sq, int to_sq, int color,
                const std::vector<int16_t>& weights);
};

// ============================================================
// NNUE class
// ============================================================

class NNUE {
public:
    NNUE();

    // Load weights from a binary file
    bool load(const std::string& path);

    // Evaluate a position
    int evaluate(const Board& board);

    // Incremental update when making a move
    void on_make_move(const Board& board, const Move& move);

    // Incremental update when unmaking a move (for search)
    void on_unmake_move(const Board& board, const Move& move);

private:
    // Network weights
    // l1: input → hidden
    std::vector<int16_t> W1; // size: INPUT_DIM * HIDDEN_DIM
    std::vector<int32_t> B1; // size: HIDDEN_DIM

    // l2: hidden → mid
    std::vector<int16_t> W2; // size: HIDDEN_DIM * MID_DIM
    std::vector<int32_t> B2; // size: MID_DIM

    // l3: mid → output
    std::vector<int16_t> W3; // size: MID_DIM * 1
    int32_t B3;

    // Cached accumulator for current board
    Accumulator acc;

    // Internal helpers
    int32_t forward_hidden(const Accumulator& acc) const;
};

