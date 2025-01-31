// determine result of game
#ifndef ARBITER_H
#define ARBITER_H

#include "helpers.h"
#include "moveGenerator.h"


class Arbiter {
private:
public:

    //static bool isDrawResult(Result result) {
    //    return result == Stalemate || result == Repetition || result == FiftyMoveRule || result == InsufficientMaterial || result == DrawByArbiter;
    //}
    static bool isWinResult(Result result);
    static bool isWhiteWinResult(Result result);
    static bool isBlackWinResult(Result result);
    static Result GetGameState(Board board);

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
};

#endif