#include "nnue.h"
#include <fstream>
#include <iostream>
#include <algorithm>

// ============================================================
// Helpers
// ============================================================

inline int feature_index_stm(int sq, int piece, int color) {
    int pc = piece;
    return (color == 0 ? 0 : 384) + pc*64 + sq;
}

inline int feature_index_ntm(int sq, int piece, int color) {
    int pc = piece;
    int sq_flip = sq ^ 56; // flip rank
    return (color == 0 ? 384 : 0) + pc*64 + sq_flip;
}

inline int other_color(int c) { return c ^ 1; }

// ============================================================
// Load quantised.bin
// ============================================================

bool NNUE::load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        std::cerr << "NNUE: failed to open " << path << "\n";
        return false;
    }

    f.read((char*)l0w, sizeof(l0w));
    f.read((char*)l0b, sizeof(l0b));
    f.read((char*)l1w, sizeof(l1w));
    f.read((char*)&l1b, sizeof(l1b));

    if (!f) {
        std::cerr << "NNUE: file truncated or corrupted\n";
        return false;
    }

    std::cout << "[DEBUG] NNUE loaded\n";
    return true;
}

// ============================================================
// Build accumulators fully from board (STM/NTM)
// ============================================================

void NNUE::build_accumulators(const Board& b) {
    acc_stm.init_bias(l0b);
    acc_ntm.init_bias(l0b);

    int stm = b.move_color;

    for (int sq = 0; sq < 64; ++sq) {
        int pc = b.getMovedPiece(sq);
        if (pc == -1) continue;
        int color = b.getSideAt(sq);

        int stm_idx = feature_index_stm(sq, pc, color);
        int ntm_idx = feature_index_ntm(sq, pc, color);

        acc_stm.add_feature(stm_idx, l0w);
        acc_ntm.add_feature(ntm_idx, l0w);
    }

    // Ensure STM corresponds to side to move
    if (!b.is_white_move) std::swap(acc_stm, acc_ntm);
}

// ============================================================
// Evaluation
// ============================================================

int NNUE::evaluate(bool is_white_move) const {
    int64_t out64 = 0;

    for (int i = 0; i < HIDDEN_SIZE; ++i)
        out64 += (int64_t)screlu(acc_stm.vals[i]) * (int32_t)l1w[i];
    for (int i = 0; i < HIDDEN_SIZE; ++i)
        out64 += (int64_t)screlu(acc_ntm.vals[i]) * (int32_t)l1w[HIDDEN_SIZE + i];

    out64 /= (int64_t)QA;
    out64 += (int64_t)l1b;
    out64 *= SCALE;
    out64 /= (int64_t)(QA * QB);

    //std::cout << "eval: " << out64 << "\n\n";

    return is_white_move ? out64 : -out64;
}

int NNUE::full_eval(const Board& b) {
    build_accumulators(b);
    return evaluate(b.is_white_move);
}

// ============================================================
// Incremental updates (STM/NTM)
// ============================================================

void NNUE::on_make_move(const Board& before, const Move& mv) {
    int stm = before.move_color;
    int ntm = other_color(stm);
    int moved_piece = before.getMovedPiece(mv.StartSquare());
    bool promo = mv.IsPromotion();

    // ---- STM: moved piece ----
    int f_from = feature_index_stm(mv.StartSquare(), moved_piece, stm);
    int f_to   = feature_index_stm(mv.TargetSquare(), moved_piece, stm);
    acc_stm.remove_feature(f_from, l0w);
    acc_stm.add_feature(f_to, l0w);

    // Promotion
    if (promo) {
        int promo_piece = mv.PromotionPieceType();
        int f_promo = feature_index_stm(mv.TargetSquare(), promo_piece, stm);
        acc_stm.remove_feature(f_to, l0w);
        acc_stm.add_feature(f_promo, l0w);
    }

    // ---- NTM: captured pieces ----
    int captured_piece = before.getCapturedPiece(mv.TargetSquare());
    if (captured_piece != -1) {
        int f_cap = feature_index_ntm(mv.TargetSquare(), captured_piece, ntm);

        //std::cerr << "[DEBUG] Capture: piece=" << captured_piece
        //        << " sq=" << mv.TargetSquare()
        //        << " ntm_color=" << ntm
        //        << " f_index=" << f_cap << "\n";

        acc_ntm.remove_feature(f_cap, l0w);
    }

    // En passant
    if (mv.MoveFlag() == Move::enPassantCaptureFlag) {
        int cap_sq = mv.TargetSquare() + (stm == 0 ? -8 : 8);
        int f_cap = feature_index_ntm(cap_sq, pawn, ntm);
        acc_ntm.remove_feature(f_cap, l0w);
    }

    // Castling rook
    if (mv.MoveFlag() == Move::castleFlag) {
        int rank = (stm == 0 ? 0 : 7);
        int rook_from = (mv.TargetSquare() % 8 == 6 ? rank*8 + 7 : rank*8);
        int rook_to   = (mv.TargetSquare() % 8 == 6 ? rank*8 + 5 : rank*8 + 3);
        int f_r_from = feature_index_stm(rook_from, rook, stm);
        int f_r_to   = feature_index_stm(rook_to,   rook, stm);
        acc_stm.remove_feature(f_r_from, l0w);
        acc_stm.add_feature(f_r_to,   l0w);
    }

    // Swap accumulators so STM always points to side-to-move
    std::swap(acc_stm, acc_ntm);
}

void NNUE::on_unmake_move(const Board& board, const Move& mv) {
    // Swap back before undoing
    std::swap(acc_stm, acc_ntm);

    int stm = other_color(board.move_color); // the side that made the move
    int ntm = other_color(stm);
    bool promo = mv.IsPromotion();
    int moved_piece = board.getMovedPiece(mv.TargetSquare());
    int captured_piece = board.currentGameState.capturedPieceType;

    // Undo promotion
    if (promo) {
        int promo_piece = mv.PromotionPieceType();
        int f_promo = feature_index_stm(mv.TargetSquare(), promo_piece, stm);
        int f_pawn  = feature_index_stm(mv.TargetSquare(), pawn, stm);
        acc_stm.remove_feature(f_promo, l0w);
        acc_stm.add_feature(f_pawn, l0w);
    } else {
        int f_to   = feature_index_stm(mv.TargetSquare(), moved_piece, stm);
        int f_from = feature_index_stm(mv.StartSquare(), moved_piece, stm);
        acc_stm.remove_feature(f_to, l0w);
        acc_stm.add_feature(f_from, l0w);
    }

    // Undo captures in NTM
    if (captured_piece != -1) {
        int f_cap = feature_index_ntm(mv.TargetSquare(), captured_piece, ntm);
        acc_ntm.add_feature(f_cap, l0w);
    }

    // Undo en passant
    if (mv.MoveFlag() == Move::enPassantCaptureFlag) {
        int cap_sq = mv.TargetSquare() + (stm == 0 ? -8 : 8);
        int f_cap = feature_index_ntm(cap_sq, pawn, ntm);
        acc_ntm.add_feature(f_cap, l0w);
    }

    // Undo castling rook
    if (mv.MoveFlag() == Move::castleFlag) {
        int rank = (stm == 0 ? 0 : 7);
        int rook_from = (mv.TargetSquare() % 8 == 6 ? rank*8 + 7 : rank*8);
        int rook_to   = (mv.TargetSquare() % 8 == 6 ? rank*8 + 5 : rank*8 + 3);
        int f_r_from = feature_index_stm(rook_from, rook, stm);
        int f_r_to   = feature_index_stm(rook_to,   rook, stm);
        acc_stm.remove_feature(f_r_to, l0w);
        acc_stm.add_feature(f_r_from, l0w);
    }
}

// ============================================================
// Debug helpers
// ============================================================

void NNUE::debug_acc_full(const Accumulator& acc, const std::string& name) const {
    int32_t sum = 0, minv = acc.vals[0], maxv = acc.vals[0];
    for (int i = 0; i < HIDDEN_SIZE; ++i) {
        sum += acc.vals[i];
        if (acc.vals[i] < minv) minv = acc.vals[i];
        if (acc.vals[i] > maxv) maxv = acc.vals[i];
    }
    std::cout << "[DEBUG] Acc " << name << " sum=" << sum
              << " min=" << minv << " max=" << maxv << " first8=[";
    for (int i = 0; i < 8; ++i) std::cout << acc.vals[i] << (i < 7 ? "," : "");
    std::cout << "]\n";
}
/*
int NNUE::evaluate_debug(bool is_white_move) const {
    debug_acc_full(acc_stm, "STM before screlu");
    debug_acc_full(acc_ntm, "NTM before screlu");

    debug_evaluate(acc_stm, acc_ntm);

    return evaluate(is_white_move);
}
*/

// Utility: Compare two accumulators and print differing feature indices and values
static void debug_diff_features_full(const Accumulator& incr,
                                     const Accumulator& full,
                                     const char* label,
                                     int max_diffs = 40) {
    std::cerr << "   --- Feature Differences (" << label << ") ---\n";
    int count = 0;
    for (int i = 0; i < 256; ++i) {
        if (incr.vals[i] != full.vals[i]) {
            std::cerr << "      idx=" << i
                      << " incr=" << incr.vals[i]
                      << " full=" << full.vals[i] << "\n";

            // Attempt to decode piece/color/square if possible
            int color = (i >= 384) ? 1 : 0;
            int idx_rel = (i >= 384) ? i - 384 : i;
            int piece = idx_rel / 64;
            int sq = idx_rel % 64;
            std::cerr << "         decoded: piece=" << piece
                      << " sq=" << sq
                      << " color=" << color << "\n";

            if (++count >= max_diffs) {
                std::cerr << "      ... (more omitted)\n";
                break;
            }
        }
    }
}

// ============================================================
// DEBUG AFTER MAKE
// ============================================================
void NNUE::debug_check_incr_vs_full_after_make(const Board& before,
                                               const Move& mv,
                                               NNUE& nnue) {
    Board b_after = before;
    nnue.on_make_move(before, mv);
    b_after.MakeMove(mv);

    NNUE nnue_full;
    nnue_full.load("../bin/12x64_0.0.bin");
    nnue_full.build_accumulators(b_after);

    int full_eval = nnue_full.evaluate(b_after.is_white_move);
    int incr_eval = nnue.evaluate(b_after.is_white_move);

    if (abs(full_eval - incr_eval) > 25) {
        int stm = before.move_color;
        int ntm = stm ^ 1;
        int from = mv.StartSquare();
        int to   = mv.TargetSquare();
        int moved_piece = before.getMovedPiece(from);
        int captured_piece = before.getCapturedPiece(to);

        std::cerr << "\n[NNUE DEBUG] MISMATCH AFTER MAKE: "
                  << mv.uci() << " full=" << full_eval
                  << " incr=" << incr_eval << "\n";

        std::cerr << "   STM=" << stm << " NTM=" << ntm
                  << " moved=" << moved_piece
                  << " captured=" << captured_piece << "\n";

        int f_from_stm = feature_index_stm(from, moved_piece, stm);
        int f_to_stm   = feature_index_stm(to, moved_piece, stm);
        int f_cap_ntm  = (captured_piece != -1 ?
            feature_index_ntm(to, captured_piece, ntm) : -1);

        std::cerr << "   f_from_stm=" << f_from_stm
                  << " f_to_stm=" << f_to_stm
                  << " f_cap_ntm=" << f_cap_ntm << "\n";

        debug_diff_features_full(nnue.acc_stm, nnue_full.acc_stm, "STM");
        debug_diff_features_full(nnue.acc_ntm, nnue_full.acc_ntm, "NTM");

        abort();
    }
}

// ============================================================
// DEBUG AFTER UNMAKE
// ============================================================
void NNUE::debug_check_incr_vs_full_after_unmake(const Board& board_with_move,
                                                 const Move& mv,
                                                 NNUE& nnue) {
    Board b_before = board_with_move;
    nnue.on_unmake_move(board_with_move, mv);
    b_before.UnmakeMove(mv);

    NNUE nnue_full;
    nnue_full.load("../bin/12x64_0.0.bin");
    nnue_full.build_accumulators(b_before);

    int full_eval = nnue_full.evaluate(b_before.is_white_move);
    int incr_eval = nnue.evaluate(b_before.is_white_move);

    if (abs(full_eval - incr_eval) > 25) {
        int stm = b_before.move_color;
        int ntm = stm ^ 1;

        std::cerr << "\n[NNUE DEBUG] MISMATCH AFTER UNMAKE: "
                  << mv.uci() << " full=" << full_eval
                  << " incr=" << incr_eval << "\n";

        debug_diff_features_full(nnue.acc_stm, nnue_full.acc_stm, "STM");
        debug_diff_features_full(nnue.acc_ntm, nnue_full.acc_ntm, "NTM");

        abort();
    }
}
