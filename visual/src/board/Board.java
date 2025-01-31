package board;

//import board.Tile.EmptyTile;
//import engine.*;
import pieces.*;
import java.util.*;

public class Board {
    public Tile[] boardTiles = new Tile[BoardUtils.NUM_TILES];;
    public String fen;

    public Board() {
        setFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        makeBoardTilesFromFEN();

    }

    public Board(String fen) {
        setFEN(fen);
        makeBoardTilesFromFEN();
    }

    public void setFEN(String _fen) {
        fen = _fen;
    }

    public void makeBoardTilesFromFEN() {
        int idx = BoardUtils.NUM_TILES - 1;
        char fen_char;
        Piece piece;

        for (int i = 0; i < fen.length(); i++) {
            fen_char = fen.charAt(i);
            if (idx < 0) {break;}
            else if (Character.isDigit(fen_char)) {// if num
                for (int blanks = 0; blanks < Character.getNumericValue(fen_char); blanks++) {
                    idx-- ;
                    piece = null;
                    boardTiles[idx] = Tile.createTile(idx, piece);
                }
            } else if (fen_char == '/') { // if '/'
                continue;
            } else { // if piece
                if (Character.isLowerCase(fen_char)) { piece = new Piece(idx, Character.toLowerCase(fen_char), Alliance.BLACK); }
                else {piece = new Piece(idx, Character.toLowerCase(fen_char), Alliance.WHITE);}
                boardTiles[idx] = Tile.createTile(idx, piece);
                idx--;
            }
        }
    }
    
    public Tile getTile(final int tile_cord) {
        System.out.println(tile_cord);
        System.out.println(boardTiles[tile_cord].getPiece());
        System.out.print(boardTiles[tile_cord].isTileOccupied());
        return boardTiles[tile_cord];
    }


}
