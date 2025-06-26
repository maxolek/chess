// stores important "non-visual" information of the board
// i.e. 50 move rule, en-passant, castling, captures
//      these require previous information to be known

#include "helpers.h"
#include "gamestate.h"

/*
int GameState::capturedPieceType = -1;
int GameState::enPassantFile = -1;
int GameState::castlingRights = 0b1111;
int GameState::fiftyMoveCounter = 0;
*/

GameState::GameState() {
    capturedPieceType = -1;
    enPassantFile = -1;
    castlingRights = 0b1111;
    fiftyMoveCounter = 0;
}

GameState::GameState(int capture_piece, int en_passant_file, int castling_rights, int fifty_move_count) {
    capturedPieceType = capture_piece;
    enPassantFile = en_passant_file;
    castlingRights = castling_rights;
    fiftyMoveCounter = fifty_move_count;
}

bool GameState::HasKingsideCastleRight(bool white) const {
    int mask = white ? 1 : 4;
    return (castlingRights & mask) != 0;
}

bool GameState::HasQueensideCastleRight(bool white) const {
    int mask = white ? 2 : 8;
    return (castlingRights & mask) != 0;
}

//int GameState::FiftyMoveCounter() const { return fiftyMoveCounter; }

void GameState::PrintGamestate() {
    std::cout << "Captured Piece: " << capturedPieceType << std::endl;
    std::cout << "En-passant File: " << enPassantFile << std::endl;
    std::cout << "Castling Rights: " << castlingRights << std::endl;
    std::cout << "50 move counter: " << fiftyMoveCounter << std::endl;
}

