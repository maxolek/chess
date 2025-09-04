#include "NNUE.h"
#include "Board.h"
#include <algorithm>
#include <cassert>

namespace nnue {

// ============================================================
// Accumulator
// ============================================================

Accumulator::Accumulator() : hidden(HIDDEN_DIM, 0) {}

void Accumulator::build(const Board& board, int king_sq, const std::vector<int16_t>& W1, const std::vector<int32_t>& B1) {
    std::fill(hidden.begin(), hidden.end(), 0);

    // Loop over both colors
    for (int color = 0; color < NUM_COLORS; ++color) {
        for (int piece = 0; piece < NUM_PIECE_TYPES; ++piece) {
            uint64_t bb = board.pieceBitboards[piece] & board.colorBitboards[color];
            while (bb) {
                int sq = getLSB(bb); // use your own bit-scan/popcount
                bb &= bb-1;
                int idx = feature_index(king_sq, piece, sq, color);
                const int16_t* w = &W1[idx * HIDDEN_DIM];
                for (int j = 0; j < HIDDEN_DIM; ++j)
                    hidden[j] += w[j];
            }
        }
    }

    // Add bias for hidden layer once per neuron
    // (important: otherwise accumulator is missing bias1)
    for (int j = 0; j < HIDDEN_DIM; ++j)
        hidden[j] += B1[j];
}

void Accumulator::update(int king_sq,
                         int piece, int from_sq, int to_sq, int color,
                         const std::vector<int16_t>& W1) {
    // Remove "from" square contribution
    if (from_sq >= 0) {
        int idx_from = feature_index(king_sq, piece, from_sq, color);
        const int16_t* w = &W1[idx_from * HIDDEN_DIM];
        for (int j = 0; j < HIDDEN_DIM; ++j)
            hidden[j] -= w[j];
    }

    // Add "to" square contribution
    if (to_sq >= 0) {
        int idx_to = feature_index(king_sq, piece, to_sq, color);
        const int16_t* w = &W1[idx_to * HIDDEN_DIM];
        for (int j = 0; j < HIDDEN_DIM; ++j)
            hidden[j] += w[j];
    }
}

// ============================================================
// NNUE
// ============================================================

NNUE::NNUE() : B3(0), acc() {}

bool NNUE::load(const std::string& path) {
    std::ifstream fin(path, std::ios::binary);
    if (!fin) {
        std::cerr << "NNUE load failed: cannot open file " << path << "\n";
        return false;
    }

    // Resize containers
    W1.resize(INPUT_DIM * HIDDEN_DIM);
    B1.resize(HIDDEN_DIM);
    W2.resize(HIDDEN_DIM * MID_DIM);
    B2.resize(MID_DIM);
    W3.resize(MID_DIM);

    // Read everything
    fin.read(reinterpret_cast<char*>(W1.data()), W1.size() * sizeof(int16_t));
    fin.read(reinterpret_cast<char*>(B1.data()), B1.size() * sizeof(int32_t));
    fin.read(reinterpret_cast<char*>(W2.data()), W2.size() * sizeof(int16_t));
    fin.read(reinterpret_cast<char*>(B2.data()), B2.size() * sizeof(int32_t));
    fin.read(reinterpret_cast<char*>(W3.data()), W3.size() * sizeof(int16_t));
    fin.read(reinterpret_cast<char*>(&B3), sizeof(int32_t));

    if (!fin) {
        std::cerr << "NNUE load failed: file too short or corrupted\n";
        return false;
    }

    return true;
}

// ======================
// Incremental Updates
// ======================

// call before board.makemove(move) in searcher
void NNUE::on_make_move(const Board& board, const Move& move) {
    int stm = board.move_color;                // side to move before making move
    int king_sq = board.kingSquare(stm);

    int from = move.StartSquare();
    int to = move.TargetSquare();
    int moved_piece = board.getMovedPiece(from);
    int captured_piece = board.getCapturedPiece(to);


    // If king moves → rebuild from scratch
    if (moved_piece % NUM_PIECE_TYPES == king) {
        acc.build(board, king_sq, W1, B1);
        king_sq = king_sq;
        return;
    }

    // If king square changed (castling, etc.)
    if (king_sq != king_sq) {
        acc.build(board, king_sq, W1, B1);
        king_sq = king_sq;
        return;
    }

    // Normal incremental updates
    acc.update(king_sq, moved_piece, from, to, W1);

    // Capture
    if (captured_piece != -1) {
        acc.update(king_sq, captured_piece, to, -1, W1);
    }

    // Promotion
    if (move.IsPromotion()) {
        // Remove pawn we just moved into `to`
        acc.update(king_sq, moved_piece, to, -1, W1);
        // Add promoted piece at `to`
        acc.update(king_sq, move.PromotionPieceType(), -1, to, W1);
    }
}

// call before board.unmakemove(move) in searcher
void NNUE::on_unmake_move(const Board& board, const Move& move) {
    int stm = !board.move_color;              // side who originally moved
    int king_sq = board.kingSquare(stm);

    int from = move.StartSquare();
    int to = move.TargetSquare();
    int moved_piece = board.getMovedPiece(to);
    int captured_piece = board.currentGameState.capturedPieceType;


    // If king moved → rebuild
    if (moved_piece % NUM_PIECE_TYPES == king || king_sq != king_sq) {
        acc.build(board, king_sq, W1, B1);
        king_sq = king_sq;
        return;
    }

    // Undo promotion
    if (move.IsPromotion()) {
        // Remove promoted piece from `to`
        acc.update(king_sq, move.PromotionPieceType(), to, -1, W1);
        // Restore pawn at `to`
        acc.update(king_sq, moved_piece, -1, to, W1);
    } else {
        // Undo normal move → remove from `to`, add back to `from`
        acc.update(king_sq, moved_piece, to, from, W1);
    }

    // Restore capture
    if (captured_piece != -1) {
        acc.update(king_sq, captured_piece -1, to, W1);
    }
}



int NNUE::evaluate(const Board& board) {
    // 1. Get king square for side to move
    int stm = board.move_color;
    int king_sq = board.kingSquare(stm);

    // 2. (Re)build accumulator if needed
    acc.build(board, king_sq, W1, B1);

    // 3. Forward pass: hidden → mid → output
    // hidden pre-activations are in acc.hidden
    std::vector<int32_t> mid(MID_DIM, 0);

    // Layer 1: hidden → mid
    for (int i = 0; i < MID_DIM; ++i) {
        int32_t sum = B2[i];
        for (int j = 0; j < HIDDEN_DIM; ++j) {
            sum += acc.hidden[j] * W2[j * MID_DIM + i];
        }
        mid[i] = std::max<int32_t>(0, sum); // ReLU
    }

    // Layer 2: mid → output
    int32_t out = B3;
    for (int i = 0; i < MID_DIM; ++i)
        out += mid[i] * W3[i];

    // Scale to centipawns (tune later)
    return static_cast<int>(out / 256);
}

int32_t NNUE::forward_hidden(const Accumulator& acc) const {
    // helper if you want raw hidden outputs
    int32_t sum = 0;
    for (int v : acc.hidden)
        sum += v;
    return sum;
}

} // namespace nnue
