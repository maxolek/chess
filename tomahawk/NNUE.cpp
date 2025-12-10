#include "nnue.h"
#include <fstream>
#include <iostream>
#include <algorithm>

// ============================================================
// Helpers
// ============================================================

inline int feature_index_stm(int sq, int piece, int color) {
    // color: 0 = white, 1 = black
    return (color == 0 ? 0 : 384) + piece*64 + sq;
}

inline int feature_index_ntm(int sq, int piece, int color) {
    int sq_flip = sq ^ 56;
    return (color == 0 ? 384 : 0) + piece*64 + sq_flip;
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
    int stm = before.move_color;            // side making the move
    int ntm = other_color(stm);
    int moved_piece = before.getMovedPiece(mv.StartSquare());
    bool promo = mv.IsPromotion();

    // Feature indices for moved piece (both accumulators)
    int f_from_stm = feature_index_stm(mv.StartSquare(), moved_piece, stm);
    int f_to_stm   = feature_index_stm(mv.TargetSquare(), moved_piece, stm);
    int f_from_ntm = feature_index_ntm(mv.StartSquare(), moved_piece, stm);
    int f_to_ntm   = feature_index_ntm(mv.TargetSquare(), moved_piece, stm);

    // ---- Update both POV accumulators for the moved piece ----
    acc_stm.remove_feature(f_from_stm, l0w);
    acc_stm.add_feature(f_to_stm, l0w);

    acc_ntm.remove_feature(f_from_ntm, l0w);
    acc_ntm.add_feature(f_to_ntm, l0w);

    // Promotion: replace pawn feature with promoted piece (both POVs)
    if (promo) {
        int promo_piece = mv.PromotionPieceType();
        int f_promo_stm = feature_index_stm(mv.TargetSquare(), promo_piece, stm);
        int f_promo_ntm = feature_index_ntm(mv.TargetSquare(), promo_piece, stm);

        // remove pawn entry we just added, then add promoted piece
        acc_stm.remove_feature(f_to_stm, l0w);
        acc_stm.add_feature(f_promo_stm, l0w);

        acc_ntm.remove_feature(f_to_ntm, l0w);
        acc_ntm.add_feature(f_promo_ntm, l0w);
    }

    // ---- Captured piece: remove from BOTH accumulators ----
    int captured_piece = before.getCapturedPiece(mv.TargetSquare());
    if (captured_piece != -1) {
        // actual color of captured piece (should equal ntm but use board for safety)
        int cap_color = before.getSideAt(mv.TargetSquare());
        int f_cap_ntm = feature_index_ntm(mv.TargetSquare(), captured_piece, cap_color);
        int f_cap_stm = feature_index_stm(mv.TargetSquare(), captured_piece, cap_color);
        acc_ntm.remove_feature(f_cap_ntm, l0w);
        acc_stm.remove_feature(f_cap_stm, l0w);
    }

    // ---- En passant: captured pawn is on cap_sq (remove from BOTH) ----
    if (mv.MoveFlag() == Move::enPassantCaptureFlag) {
        int cap_sq = mv.TargetSquare() + (stm == 0 ? -8 : 8);
        int cap_color = before.getSideAt(cap_sq);
        int f_cap_ntm = feature_index_ntm(cap_sq, pawn, cap_color);
        int f_cap_stm = feature_index_stm(cap_sq, pawn, cap_color);
        acc_ntm.remove_feature(f_cap_ntm, l0w);
        acc_stm.remove_feature(f_cap_stm, l0w);
    }

    // ---- Castling rook: update both POVs for rook from/to ----
    if (mv.MoveFlag() == Move::castleFlag) {
        int rank = (stm == 0 ? 0 : 7);
        int rook_from = (mv.TargetSquare() % 8 == 6 ? rank*8 + 7 : rank*8);
        int rook_to   = (mv.TargetSquare() % 8 == 6 ? rank*8 + 5 : rank*8 + 3);

        int f_r_from_stm = feature_index_stm(rook_from, rook, stm);
        int f_r_to_stm   = feature_index_stm(rook_to,   rook, stm);
        int f_r_from_ntm = feature_index_ntm(rook_from, rook, stm);
        int f_r_to_ntm   = feature_index_ntm(rook_to,   rook, stm);

        acc_stm.remove_feature(f_r_from_stm, l0w);
        acc_stm.add_feature(f_r_to_stm, l0w);

        acc_ntm.remove_feature(f_r_from_ntm, l0w);
        acc_ntm.add_feature(f_r_to_ntm, l0w);
    }

    // Finally swap so acc_stm always points to the side-to-move for the new board state
    std::swap(acc_stm, acc_ntm);
}


void NNUE::on_unmake_move(const Board& board, const Move& mv) {
    // Swap back so accumulators correspond to the side that made the move
    std::swap(acc_stm, acc_ntm);

    int stm = other_color(board.move_color); // side that made the move
    int ntm = other_color(stm);
    bool promo = mv.IsPromotion();

    // moved_piece AFTER move is located at TargetSquare in board
    int moved_piece = board.getMovedPiece(mv.TargetSquare());

    // Indices for moved piece (reverse of make)
    int f_to_stm   = feature_index_stm(mv.TargetSquare(), moved_piece, stm);
    int f_from_stm = feature_index_stm(mv.StartSquare(), moved_piece, stm);
    int f_to_ntm   = feature_index_ntm(mv.TargetSquare(), moved_piece, stm);
    int f_from_ntm = feature_index_ntm(mv.StartSquare(), moved_piece, stm);

    // Undo promotion: remove promoted piece, add pawn back (both POVs)
    if (promo) {
        int promo_piece = mv.PromotionPieceType();
        int f_promo_stm = feature_index_stm(mv.TargetSquare(), promo_piece, stm);
        int f_pawn_stm  = feature_index_stm(mv.TargetSquare(), pawn, stm);
        acc_stm.remove_feature(f_promo_stm, l0w);
        acc_stm.add_feature(f_pawn_stm, l0w);

        int f_promo_ntm = feature_index_ntm(mv.TargetSquare(), promo_piece, stm);
        int f_pawn_ntm  = feature_index_ntm(mv.TargetSquare(), pawn, stm);
        acc_ntm.remove_feature(f_promo_ntm, l0w);
        acc_ntm.add_feature(f_pawn_ntm, l0w);
    } else {
        // Normal: remove moved (to) and add moved (from) in both POVs
        acc_stm.remove_feature(f_to_stm, l0w);
        acc_stm.add_feature(f_from_stm, l0w);

        acc_ntm.remove_feature(f_to_ntm, l0w);
        acc_ntm.add_feature(f_from_ntm, l0w);
    }

    // Undo captures: re-add captured piece to BOTH accumulators (if any)
    int captured_piece = board.currentGameState.capturedPieceType;
    if (captured_piece != -1) {
        int cap_sq = mv.TargetSquare();
        int cap_color = board.move_color; // still on after-move state, so previous captured piece is of current stm color
        int f_cap_ntm = feature_index_ntm(cap_sq, captured_piece, cap_color);
        int f_cap_stm = feature_index_stm(cap_sq, captured_piece, cap_color);
        acc_ntm.add_feature(f_cap_ntm, l0w);
        acc_stm.add_feature(f_cap_stm, l0w);
    }

    // Undo en passant restore
    if (mv.MoveFlag() == Move::enPassantCaptureFlag) {
        int cap_sq = mv.TargetSquare() + (stm == 0 ? -8 : 8);
        int cap_color = other_color(stm); // captured pawn color
        int f_cap_ntm = feature_index_ntm(cap_sq, pawn, cap_color);
        int f_cap_stm = feature_index_stm(cap_sq, pawn, cap_color);
        acc_ntm.add_feature(f_cap_ntm, l0w);
        acc_stm.add_feature(f_cap_stm, l0w);
    }

    // Undo castling rook: reverse the rook update (both POVs)
    if (mv.MoveFlag() == Move::castleFlag) {
        int rank = (stm == 0 ? 0 : 7);
        int rook_from = (mv.TargetSquare() % 8 == 6 ? rank*8 + 7 : rank*8);
        int rook_to   = (mv.TargetSquare() % 8 == 6 ? rank*8 + 5 : rank*8 + 3);

        int f_r_from_stm = feature_index_stm(rook_from, rook, stm);
        int f_r_to_stm   = feature_index_stm(rook_to,   rook, stm);
        int f_r_from_ntm = feature_index_ntm(rook_from, rook, stm);
        int f_r_to_ntm   = feature_index_ntm(rook_to,   rook, stm);

        acc_stm.remove_feature(f_r_to_stm, l0w);
        acc_stm.add_feature(f_r_from_stm, l0w);

        acc_ntm.remove_feature(f_r_to_ntm, l0w);
        acc_ntm.add_feature(f_r_from_ntm, l0w);
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

        //debug_replay_feature_changes(before, mv, b_after);
        debug_expected_changes(before, mv, b_after);
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

void NNUE::debug_expected_changes(const Board &before,
                            const Move &m,
                            const Board &after) {

    auto stm_before = before.move_color;
    auto stm_after  = after.move_color;

    int movingPiece = before.getMovedPiece(m.StartSquare());
    int capturedPiece = before.getCapturedPiece(m.TargetSquare());

    std::cerr << "\n[EXPECTED NNUE CHANGES]\n";

    // Remove from old STM (source)
    std::cerr << "Remove moved (STM old): "
              << feature_index_stm(m.StartSquare(), movingPiece, stm_before) << "\n";

    // Add into old STM (target)
    std::cerr << "Add moved (STM old): "
              << feature_index_stm(m.TargetSquare(), movingPiece, stm_before) << "\n";

    if (capturedPiece != -1) {
        std::cerr << "Remove captured (NTM old): "
                  << feature_index_ntm(m.TargetSquare(), capturedPiece, stm_before ^ 1) << "\n";
    }

    // NEW STM perspective (flipped)
    std::cerr << "Re-add moved under new POV (NTM old side): "
              << feature_index_ntm(m.TargetSquare(), movingPiece, stm_after) << "\n\n";
}


void NNUE::debug_replay_feature_changes(const Board& before,
                                        const Move& mv,
                                        const Board& after) {

    std::cerr << "[NNUE FEATURE CHANGE LOG]\n";

    int stm_before = before.move_color;
    int stm_after  = after.move_color; // flipped after move

    int moved_piece = before.getMovedPiece(mv.StartSquare());
    int captured_piece = before.getCapturedPiece(mv.TargetSquare());

    int from = mv.StartSquare();
    int to   = mv.TargetSquare();

    // 1️⃣ moved piece
    std::cerr << "Deactivate: f_from_stm = "
              << feature_index_stm(from, moved_piece, stm_before) << "\n";
    std::cerr << "Activate:   f_to_stm   = "
              << feature_index_stm(to, moved_piece, stm_before) << "\n";

    // 2️⃣ captured piece (if any)
    if (captured_piece != -1) {
        std::cerr << "Deactivate captured: f_cap_ntm = "
                  << feature_index_ntm(to, captured_piece, stm_before ^ 1)
                  << "\n";
    }

    // 3️⃣ After move, STM perspective flips → so these must flip too
    std::cerr << "Post-move should activate NTM: "
              << feature_index_ntm(to, moved_piece, stm_after) << "\n";
}
