#include "nnue.h"
#include <fstream>
#include <cstring>
#include <iostream>
#include <algorithm>

// ============================================================
// Helpers
// ============================================================

// Map a piece on a square to a feature index 0..767
static inline int feature_index(int piece, int color, int sq) {
    // piece 0..5 pawn..king
    // color 0=white, 1=black
    // sq 0..63
    return (piece + 6 * color) * 64 + sq;
}

static inline int other_color(int c) { return c ^ 1; }

// ============================================================
// Load quantised.bin
// ============================================================

bool NNUE::load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { std::cerr << "NNUE: failed to open " << path << "\n"; return false; }

    f.read((char*)l0w, sizeof(l0w));
    f.read((char*)l0b, sizeof(l0b));
    f.read((char*)l1w, sizeof(l1w));
    f.read((char*)&l1b, sizeof(l1b));

    if (!f) { std::cerr << "NNUE: file truncated or corrupted\n"; return false; }

    //std::cout << "NNUE loaded: l0w[0]=" << l0w[0] << " l0b[0]=" << l0b[0] 
    //          << " l1w[0]=" << l1w[0] << " l1b=" << l1b << "\n";
    return true;
}

// ============================================================
// Build accumulators fully from board (non-incremental)
// ============================================================

void NNUE::build_accumulators(const Board& b) {
    acc_white.init_bias(l0b);
    acc_black.init_bias(l0b);

    for (int sq = 0; sq < 64; sq++) {
        int pc = b.getMovedPiece(sq);
        if (pc == -1) continue;

        int color = b.getSideAt(sq);  // absolute color
        int feature = feature_index(pc, color, sq);

        if (color == 0)
            acc_white.add_feature(feature, l0w);
        else
            acc_black.add_feature(feature, l0w);

        //std::cout << (color == 0 ? "WHITE" : "BLACK") 
        //          << " feature: piece=" << pc << " sq=" << sq << " feature=" << feature << "\n";
    }
}

// ============================================================
// Evaluate 
// ============================================================

int NNUE::evaluate() const {
    int32_t out64 = 0;

    // Hidden layer contributions
    for (int i = 0; i < HIDDEN_SIZE; ++i)
        out64 += (int64_t)screlu(acc_white.vals[i]) * (int32_t)l1w[i];
    for (int i = 0; i < HIDDEN_SIZE; ++i)
        out64 += (int64_t)screlu(acc_black.vals[i]) * (int32_t)l1w[HIDDEN_SIZE + i];

    // l0 scale
    out64 /= (int32_t)QA;

    // Add output bias
    out64 += (int32_t)l1b;

    // Scale to centipawns
    out64 *= SCALE;

    // Normalize for quantization
    out64 /= (int32_t)(QA * QB);

   //std::cout << "NNUE evaluate: " << result << "\n";
    return out64;
}

// Full position from scratch
int NNUE::full_eval(const Board& b) {
    build_accumulators(b);
    return evaluate();
}

// ============================================================
// Incremental updates on (un)make_move
// ============================================================

// performed BEFORE board.MakeMove()
void NNUE::on_make_move(const Board& before, const Move& mv) {
    int stm = before.move_color;

    int moved_piece  = before.getMovedPiece(mv.StartSquare());
    int captured     = before.getCapturedPiece(mv.TargetSquare());
    bool promo       = mv.IsPromotion();

    // King moved → full rebuild
    if (moved_piece == king) {
        Board after = before;
        after.MakeMove(mv);
        build_accumulators(after);
        return;
    }

    int color = stm;            // moved piece color
    int f_from = feature_index(moved_piece, color, mv.StartSquare());
    int f_to   = feature_index(moved_piece, color, mv.TargetSquare());

    if (color == 0) {
        acc_white.remove_feature(f_from, l0w);
        acc_white.add_feature(f_to, l0w);
    } else {
        acc_black.remove_feature(f_from, l0w);
        acc_black.add_feature(f_to, l0w);
    }

    // Capture
    if (captured != -1) {
        int cap_color = other_color(color);
        int f_cap = feature_index(captured, cap_color, mv.TargetSquare());
        if (cap_color == 0) acc_white.remove_feature(f_cap, l0w);
        else               acc_black.remove_feature(f_cap, l0w);
    }

    // Promotion
    if (promo) {
        int promo_piece = mv.PromotionPieceType();
        // Remove pawn that was added at "to"
        if (color == 0) acc_white.remove_feature(f_to, l0w);
        else            acc_black.remove_feature(f_to, l0w);

        int f_promo = feature_index(promo_piece, color, mv.TargetSquare());
        if (color == 0) acc_white.add_feature(f_promo, l0w);
        else            acc_black.add_feature(f_promo, l0w);
    }
}

// Perform NNUE incremental undo BEFORE board.UnmakeMove()
void NNUE::on_unmake_move(const Board& board, const Move& mv) {
    int stm_restore = board.move_color ^ 1;
    bool promo = mv.IsPromotion();

    int moved_piece  = board.getMovedPiece(mv.TargetSquare());
    int captured_piece = board.currentGameState.capturedPieceType;

    int color = stm_restore;  // piece color on undo

    // King move → full rebuild
    if (moved_piece == king) {
        Board new_board = board;
        new_board.MakeMove(mv);
        build_accumulators(new_board);
        return;
    }

    if (promo) {
        int promo_piece = mv.PromotionPieceType();
        int f_promo = feature_index(promo_piece, color, mv.TargetSquare());
        int f_pawn  = feature_index(pawn, color, mv.TargetSquare());

        if (color == 0) { acc_white.remove_feature(f_promo, l0w); acc_white.add_feature(f_pawn, l0w); }
        else            { acc_black.remove_feature(f_promo, l0w); acc_black.add_feature(f_pawn, l0w); }
    } else {
        int f_to   = feature_index(moved_piece, color, mv.TargetSquare());
        int f_from = feature_index(moved_piece, color, mv.StartSquare());

        if (color == 0) { acc_white.remove_feature(f_to, l0w); acc_white.add_feature(f_from, l0w); }
        else            { acc_black.remove_feature(f_to, l0w); acc_black.add_feature(f_from, l0w); }
    }

    if (captured_piece != -1) {
        int cap_color = other_color(color);
        int f_cap = feature_index(captured_piece, cap_color, mv.TargetSquare());
        if (cap_color == 0) acc_white.add_feature(f_cap, l0w);
        else                acc_black.add_feature(f_cap, l0w);
    }
}
