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
    //PrecomputedMoveData align_masks = PrecomputedMoveData();
    
    
    // zobrist hashing for board history store (repetition tables etc)
    U64 zobrist_hash;
    std::unordered_map<U64,int> hash_history;
    U64 zobrist_table[12][64]; // 0-11: 6 pieces * 2 colors
    U64 zobrist_side_to_move;
    U64 zobrist_castling[4]; // KQkq
    U64 zobrist_enpassant[8]; // a-h
    U64 randomU64();
    void initZobristKeys();
    U64 computeZobristHash();
    U64 zobristCastlingHash(int castling_rights); // returns the corresponding hash for the castling rights
    void debugZobristDifference(U64 old_hash, U64 new_hash);
    U64 updateHash( // called inside makeMove so these are all passed as args
        U64 currentHash,  // since it isnt stored outside of that
        int start_square, int target_square, int move_flag, bool is_promotion, int promotion_piece,
        int moved_piece, int captured_piece, bool is_enpassant, int old_castling_rights
    );

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
    std::unordered_map<U64,int> positionHistory;
    std::vector<Move> allGameMoves;
    std::vector<GameState> gameStateHistory;



    Board();
    Board(std::string _fen);
    Board(int side, U64 color_bitboards[2], U64 piece_bitboards[6], GameState currentGameState);

    // inSearch controls whether this move is recorded in the game history
    // (for three-fold repetition)
    void MakeMove(Move move, bool in_search = false);

    void UnmakeMove(Move move, bool in_search = false);

    // piece and color
    void MovePiece(int piece, int start_square, int target_square);
    void CapturePiece(int piece, int target_square, bool is_enpassant, bool captured_is_moved_piece);

    void PromoteToPiece(int piece, int target_square);

    int getMovedPiece(int start_square);
    int getCapturedPiece(int target_square);
    int getSideAt(int square);
    int getPieceAt(int square, int side);

    void updateFiftyMoveCounter(int moved_piece, bool isCapture, bool unmake);

    // called before side-to-move is updated
    //      attacks are from move_color
    // generate sliding, knight, pawn from king position
    // for opp_sliding_piece: countBits((opp_square - king_square) & align_mask[opp][king] & occ) == 0 -> check
    bool inCheck(bool init);

    // <board layout> <castling rights> <en passant> <half-move clock> <full move number>
    void setFromFEN(std::string _fen);
    void setBoardFEN();
    std::string getBoardFEN();
    void print_board();

};

#endif