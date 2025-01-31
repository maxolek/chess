package board;

import pieces.*;

public abstract class Move {
    final Board board;
    final Piece movedPiece;
    final int from;
    final int to;

    // Constructor should initialize all final fields
    private Move(final Board board, final Piece moved_piece, final int from, final int to) {
        this.board = board;
        this.movedPiece = moved_piece;
        this.from = from;  // Initialize from
        this.to = to;
    }

    // non-pawn pieces
    public static final class PieceMove extends Move {
        PieceMove(final Board board, final Piece moved_piece, final int from, final int to) {
            super(board, moved_piece, from, to);  // Pass from and to
        }
    }

    // capture move
    public static final class CaptureMove extends Move {
        final Piece capturedPiece;

        CaptureMove(final Board board, final Piece moved_piece, final int from, final int to, final Piece captured_piece) {
            super(board, moved_piece, from, to);  // Pass from and to
            this.capturedPiece = captured_piece;
        }
    }
}
