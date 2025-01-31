// stores important "non-visual" information of the board
// i.e. 50 move rule, en-passant, castling, captures
//      these require previous information to be known

#ifndef GAMESTATE_H
#define GAMESTATE_H

struct GameState {
private:
public:
    // additional state information
    static int capturedPieceType;
    static int enPassantFile;
    static int fiftyMoveCounter;
    static int castlingRights;

    // remove bits
    static constexpr int clearWhiteKingSideMask = 0b1110;
    static constexpr int clearWhiteQueenSideMask = 0b1101;
    static constexpr int clearBlackKingSideMask = 0b1011;
    static constexpr int clearBlackQueenSideMask = 0b0111;

    GameState();

    GameState(int capture_piece, int en_passant_file, int castling_rights, int fifty_move_count);

    bool HasKingsideCastleRight(bool white);

    bool HasQueensideCastleRight(bool white);

    int FiftyMoveCounter();

};

#endif