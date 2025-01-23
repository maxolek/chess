// determine result of game

#include "arbiter.h"

//static bool isDrawResult(Result result) {
//    return result == Stalemate || result == Repetition || result == FiftyMoveRule || result == InsufficientMaterial || result == DrawByArbiter;
//}
bool Arbiter::isWinResult(Result result) {
    return result == isWhiteWinResult(result) || result == isBlackWinResult(result);
}
bool Arbiter::isWhiteWinResult(Result result) {
    return result == BlackIsMated || result == BlackTimeout || result == BlackIllegalMove;
}
bool Arbiter::isBlackWinResult(Result result) {
    return result == WhiteIsMated || result == WhiteTimeout || result == WhiteIllegalMove;
}

Result Arbiter::GetGameState(Board board) {
    MoveGenerator movegen = MoveGenerator(board);
    movegen.generateMoves();

    // checkmate and stalemate
    if (movegen.moves.size() == 0) {
        if (board.is_in_check) return (board.is_white_move) ? WhiteIsMated : BlackIsMated;
        else return Stalemate;
    }
    else if (board.currentGameState.FiftyMoveCounter() > 100) return FiftyMoveRule;
    // repetition
    //else if (countPosDuplicates(board.repetitionPosHistory))
    //else if (InsufficientMaterial(board)) return InsufficientMaterial;

    return InProgress;
}

/*
static bool InsufficientMaterial(Board &board) {
    // not insufficient if pawns, rooks, or queens
    if (board.pieceBitboards[0] | board.pieceBitboards[rook] | board.pieceBitboards[queen]) return false;

    int num_white_bishops = countBits(board.pieceBitboards[bishop] & board.colorBitboards[0]);
    int num_black_bishops = countBits(board.pieceBitboards[bishop] & board.colorBitboards[1]);
    int num_white_knights = countBits(board.pieceBitboards[knight] & board.colorBitboards[0]);
    int num_black_knights = countBits(board.pieceBitboards[knight] & board.colorBitboards[1]);

    // king vs king & king vs king + single minor = insufficient
    if (num_white_bishops + num_black_bishops + num_white_knights + num_black_knights <= 1) return true;

    // bishop


    return false;
}
*/
