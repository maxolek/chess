/*package pieces;

import board.*;
import board.Move.CaptureMove;

import java.util.*;

public class Knight extends Piece {
    private final static int[] CANDIDATE_MOVE_COORDINATES = {-17, -15, -10, -6, 6, 10, 15, 17};

    Knight (final int piece_position, final Alliance piece_alliance) {
        super(piece_position, piece_alliance);
    }

    private static boolean isFirstColumnExclusion(final int curr_pos, final int candidate_offset) {
        return BoardUtils.FIRST_COLUMN[curr_pos] && ((candidate_offset == -17) || (candidate_offset == -10) || (candidate_offset == 6) || (candidate_offset == 15));
    }
    private static boolean isSecondColumnExclusion(final int curr_pos, final int candidate_offset) {
        return BoardUtils.SECOND_COLUMN[curr_pos] && ((candidate_offset == -10) || (candidate_offset == 6));
    }
    private static boolean isSeventhColumnExclusion(final int curr_pos, final int candidate_offset) {
        return BoardUtils.SEVENTH_COLUMN[curr_pos] && ((candidate_offset == 10) || (candidate_offset == -6));
    }
    private static boolean isEighthColumnExclusion(final int curr_pos, final int candidate_offset) {
        return BoardUtils.EIGHTH_COLUMN[curr_pos] && ((candidate_offset == 17) || (candidate_offset == 10) || (candidate_offset == -6) || (candidate_offset == -15));
    }

    @Override
    public Collection<Move> calculateLegalMoves(Board board) {
        int candidate_target;
        final List<Move> legalMoves = new ArrayList<>();

        for (final int curr_candidate : CANDIDATE_MOVE_COORDINATES) {
            candidate_target = this.piecePosition + curr_candidate;
            if (BoardUtils.isValidTileCoordinate(candidate_target)) {
                if (isFirstColumnExclusion(this.piecePosition, curr_candidate) || isSecondColumnExclusion(this.piecePosition, curr_candidate) 
                || isSeventhColumnExclusion(this.piecePosition, curr_candidate) || isEighthColumnExclusion(this.piecePosition, curr_candidate)) {
                    continue;
                }

                final Tile candidate_target_tile = board.getTile(candidate_target);
                if (!candidate_target_tile.isTileOccupied()) {
                    legalMoves.add(new PieceMove(board, this, candidate_target));
                } else {
                    final Piece piece_at_target = candidate_target_tile.getPiece();
                    final Alliance piece_alliance = piece_at_target.getPieceAlliance();

                    if (this.pieceAlliance != piece_alliance) {
                        legalMoves.add(new CaptureMove(board, this, candidate_target, piece_at_target));
                    }
                }
            }
        }

        return legalMoves;
    }

}
*/