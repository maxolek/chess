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
    static bool isInsufficientMaterial(const Board&board);
    static bool isWinResult(Result result);
    static bool isWhiteWinResult(Result result);
    static bool isBlackWinResult(Result result);
    static Result GetGameState(const Board* board);
};

#endif