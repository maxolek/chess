#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include "board.h"
#include "move.h"

// ============================================================
// Network Dimensions
// ============================================================

constexpr int INPUT_SIZE  = 768;   // Chess768 features 64*12 -- sq*piece*color + sq
constexpr int HIDDEN_SIZE = 128;   // Your chosen hidden dim

// Quantisation factors used in training
constexpr int QA = 255;
constexpr int QB = 64;
constexpr int SCALE = 400;

// ============================================================
// Accumulator: holds hidden activations BEFORE SCReLU
// ============================================================

struct Accumulator {
    int16_t vals[HIDDEN_SIZE];   // i16 pre-activations

    // Initialize with bias
    void init_bias(const int16_t* bias) {
        memcpy(vals, bias, HIDDEN_SIZE * sizeof(int16_t));
    }

    // Add feature column: vals[i] += W[f][i]
    inline void add_feature(int feature_idx, const int16_t W[][HIDDEN_SIZE]) {
        const int16_t* col = W[feature_idx];
        for (int i = 0; i < HIDDEN_SIZE; i++)
            vals[i] += col[i];
    }

    inline void remove_feature(int feature_idx, const int16_t W[][HIDDEN_SIZE]) {
        const int16_t* col = W[feature_idx];
        for (int i = 0; i < HIDDEN_SIZE; i++)
            vals[i] -= col[i];
    }
};

// ============================================================
// Network
// ============================================================

class NNUE {
private:
public:
    NNUE() {};
    NNUE(const std::string& path) {load(path);};

    // Load quantised.bin
    bool load(const std::string& path);

    // Compute final output from accumulators
    int evaluate() const;
    // Evaluate full position (non-incremental)
    int full_eval(const Board& b);

    // Incremental updates for search
    void on_make_move(const Board& board, const Move& mv);
    void on_unmake_move(const Board& board, const Move& mv); 

    // ========== L0: 768 → 128 ==========
    // Stored column-major: W0[feature][hidden]
    int16_t l0w[INPUT_SIZE][HIDDEN_SIZE];
    int16_t l0b[HIDDEN_SIZE];

    // ========== L1: 256 → 1 ==========
    int16_t l1w[2 * HIDDEN_SIZE]; // (stm_hidden, ntm_hidden)
    int16_t l1b;

    // Cached accumulators
    Accumulator acc_white;
    Accumulator acc_black;

    // ========================================================
    // Helpers
    // ========================================================

    // Square-clipped ReLU: screlu(x) = clamp(x, 0, QA)²
    inline int32_t screlu(int16_t x) const {
        int32_t y = x;
        y = std::clamp(y, 0, QA);
        return y * y;  // produces a number in 0..QA²
    }

    // Build full accumulators from board
    void build_accumulators(const Board& b);
};

