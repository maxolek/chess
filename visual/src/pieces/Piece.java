package pieces;

import java.util.*;

public class Piece {
    protected final int piecePosition;
    protected final char type;
    protected final Alliance pieceAlliance;

    public Piece (final int piece_position, final char _type, final Alliance piece_alliance) {
        this.piecePosition = piece_position;
        this.type = _type;
        this.pieceAlliance = piece_alliance;
    }

    public char upperCharType() {
        return Character.toUpperCase(type);
    }

    public char lowerCharAlliance() {
        return pieceAlliance == Alliance.WHITE ? 'w' : 'b';
    }

    public Alliance getPieceAlliance() {
        return this.pieceAlliance;
    }

    // will be overriden for each piece_type
    //public Collection<Move> calculateLegalMoves(final Board board);
}
