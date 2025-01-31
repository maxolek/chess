package board;

//import javax.management.RuntimeErrorException;

public class BoardUtils {
    public static final boolean[] FIRST_COLUMN = initColumn(0);
    public static final boolean[] SECOND_COLUMN = initColumn(1);
    public static final boolean[] SEVENTH_COLUMN = initColumn(6);
    public static final boolean[] EIGHTH_COLUMN = initColumn(7);

    public static final int NUM_TILES = 64;
    public static final int BOARD_DIM = 8;

    private static boolean[] initColumn(int col) {
        final boolean[] column = new boolean[64];
        do {
            column[col] = true;
            col+=BOARD_DIM;
        } while (col < NUM_TILES);
        return column;
    }

    private BoardUtils() {
        //throw new RuntimeErrorException("You cannot instantiate BoardUtils");
    }

    public static boolean isValidTileCoordinate(int coord) {
        // within idx && within files && within ranks
        // && (coord % 8 >= 0 && coord % 8 < 8) && (coord / 8 >= 0 && coord / 8 < 8)
        return (coord >= 0 && coord < NUM_TILES);
    }
}
