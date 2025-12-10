#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include "board.h"
#include "move.h"

// ============================================================
// Network Dimensions
// ============================================================

constexpr int INPUT_SIZE  = 768;   // Chess768 features 64*12 -- sq*piece*color (+ sq)
constexpr int HIDDEN_SIZE = 128;   // Hidden dimension

// Quantisation factors used in training
constexpr int QA = 255;
constexpr int QB = 64;
constexpr int SCALE = 400;

// ============================================================
// Accumulator: holds hidden activations BEFORE SCReLU
// ============================================================

struct Accumulator {
    int32_t vals[HIDDEN_SIZE];   // pre-activation

    void init_bias(const int16_t* bias) {
        for (int i = 0; i < HIDDEN_SIZE; i++)
            vals[i] = bias[i];
    }

    inline void add_feature(int feature_idx, int16_t (*W)[HIDDEN_SIZE]) {
        const int16_t* col = W[feature_idx];
        for (int i = 0; i < HIDDEN_SIZE; i++)
            vals[i] += col[i];
    }

    inline void remove_feature(int feature_idx, int16_t (*W)[HIDDEN_SIZE]) {
        const int16_t* col = W[feature_idx];
        for (int i = 0; i < HIDDEN_SIZE; i++)
            vals[i] -= col[i];
    }
};

// ============================================================
// Network
// ============================================================

class NNUE {
public:
    NNUE() {};
    NNUE(const std::string& path) { load(path); };

    // Load quantised network
    bool load(const std::string& path);

    // Compute final output from accumulators
    int evaluate(bool is_white_move) const;
    int full_eval(const Board& b);

    // Incremental updates for search
    void on_make_move(const Board& board, const Move& mv);
    void on_unmake_move(const Board& board, const Move& mv);

    // ========== L0: 768 → 128 ==========
    // Stored column-major: W0[feature][hidden]
    int16_t l0w[INPUT_SIZE][HIDDEN_SIZE];
    int16_t l0b[HIDDEN_SIZE];

    // ========== L1: 2*128 → 1 ==========
    // Dual-perspective: [stm_hidden, ntm_hidden]
    int16_t l1w[2 * HIDDEN_SIZE];
    int16_t l1b;

    // Cached accumulators
    Accumulator acc_stm;
    Accumulator acc_ntm;

    // ========================================================
    // Helpers
    // ========================================================

    inline int32_t screlu(int16_t x) const {
        int32_t y = std::clamp<int32_t>(x, 0, QA);
        return y * y;
    }

    // debugging
    //void debug_acc(const Accumulator& acc, const std::string& name) const;
    void debug_acc_full(const Accumulator& acc, const std::string& name) const;
    //void debug_evaluate(const Accumulator& us, const Accumulator& them) const;
    //void debug_on_move(const std::string& name, const Move& mv, int color, int moved_piece,
    //                     int f_from, int f_to) const;
    //void on_make_move_debug(const Board& before, const Move& mv);
    //void on_unmake_move_debug(const Board& board, const Move& mv);
    //int evaluate_debug(bool is_white_move) const;
    void debug_check_incr_vs_full_after_make(const Board& before, const Move& mv, NNUE& nnue);
    void debug_check_incr_vs_full_after_unmake(const Board& board_with_move, const Move& mv, NNUE& nnue);
    void debug_replay_feature_changes(const Board& before,
                                        const Move& mv,
                                        const Board& after);
    void debug_expected_changes(const Board &before,
                            const Move &m,
                            const Board &after);

    // Build full accumulators from board
    void build_accumulators(const Board& b);
};
