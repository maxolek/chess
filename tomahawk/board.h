// represents the current state of the board during a game
// state: positions of all pieces, side to move, castling rights, en-passant square, etc.
// additional information to aid in search/eval may be included

// initial state is manual
// subsequently made/unmade moves using MakeMove() and UnmakeMove()

#ifndef BOARD_H
#define BOARD_H

#include "PrecomputedMoveData.h"
#include "move.h"
#include "gamestate.h"

class Board {
private:
public:
    PrecomputedMoveData align_masks = PrecomputedMoveData();

    // default bitboards
    U64 colorBitboards[2];
    U64 pieceBitboards[6];

    // useful info
    GameState currentGameState;
    std::string fen;

    // total plies (half-moves) played in game
    int plyCount;

    // side to move info
    bool is_white_move;
    int move_color;
    bool is_in_check;

    // list of positions since last pawn move or capture (for detecting repetitions)
    //      improve with hash
    //std::vector<U64[8]> repetitionPosHistory;
    // move history
    std::vector<Move> allGameMoves;
    std::vector<GameState> gameStateHistory;


    Board();

    Board(int side, U64 color_bitboards[2], U64 piece_bitboards[6], GameState currentGameState);

    // inSearch controls whether this move is recorded in the game history
    // (for three-fold repetition)
    void MakeMove(Move move, bool in_search = false);

    void UnmakeMove(Move move, bool in_search = false);

    // piece and color
    void MovePiece(int piece, int start_square, int target_square);
    void CapturePiece(int piece, int target_square, bool is_enpassant);

    void PromoteToPiece(int piece, int target_square);

    int getMovedPiece(int start_square);
    int getCapturedPiece(int target_square);
    int getSideAt(int square);
    int getPieceAt(int square, int side);

    void updateFiftyMoveCounter(int moved_piece, bool isCapture);

    // called before side-to-move is updated
    //      attacks are from move_color
    // generate sliding, knight, pawn from king position
    // for opp_sliding_piece: countBits((opp_square - king_square) & align_mask[opp][king] & occ) == 0 -> check
    bool inCheck();

    // <board layout> <castling rights> <en passant> <half-move clock> <full move number>
    void setBoardFEN();
    std::string getBoardFEN();
    void print_board();

};

#endif