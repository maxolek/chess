// determine result of game

#include "arbiter.h"
/*
enum Result
{
    NotStarted,
    InProgress,
    WhiteIsMated,
    BlackIsMated,
    Stalemate,
    Repetition,
    FiftyMoveRule,
    InsufficientMaterial,
    DrawByArbiter,
    WhiteTimeout,
    BlackTimeout,
    WhiteIllegalMove,
    BlackIllegalMove
};
*/

bool Arbiter::isDrawResult(Result result) {
    return result == Stalemate || result == Repetition || result == FiftyMoveRule || result == InsufficientMaterial || result == DrawByArbiter;
}
bool Arbiter::isWinResult(Result result) {
    return isWhiteWinResult(result) || isBlackWinResult(result);
}
bool Arbiter::isWhiteWinResult(Result result) {
    return result == BlackIsMated || result == BlackTimeout || result == BlackIllegalMove;
}
bool Arbiter::isBlackWinResult(Result result) {
    return result == WhiteIsMated || result == WhiteTimeout || result == WhiteIllegalMove;
}

Result Arbiter::GetGameState(const Board& board) {
    //if (board.currentGameState.FiftyMoveCounter() > 100) {return FiftyMoveRule;}
    if (board.currentGameState.fiftyMoveCounter > 100) {return FiftyMoveRule;}
    auto it = board.hash_history.find(board.zobrist_hash);
    if (it != board.hash_history.end() && it->second >= 3) {return Repetition;}
    if (isInsufficientMaterial(board)) {return InsufficientMaterial;}    

    MoveGenerator movegen = MoveGenerator(board);
    bool hasMoves = movegen.hasLegalMoves(board);

    // checkmate and stalemate
    if (!hasMoves) {
        if (board.is_in_check) return (board.is_white_move) ? WhiteIsMated : BlackIsMated;
        else {return Stalemate;}
    }

    return InProgress;
}

bool Arbiter::isInsufficientMaterial(const Board& board) {
    // If any side has pawns, rooks, or queens → not insufficient
    if (board.pieceBitboards[pawn] | board.pieceBitboards[rook] | board.pieceBitboards[queen])
        return false;

    int num_white_bishops  = countBits(board.pieceBitboards[bishop] & board.colorBitboards[0]);
    int num_black_bishops  = countBits(board.pieceBitboards[bishop] & board.colorBitboards[1]);
    int num_white_knights  = countBits(board.pieceBitboards[knight] & board.colorBitboards[0]);
    int num_black_knights  = countBits(board.pieceBitboards[knight] & board.colorBitboards[1]);

    int total_minors = num_white_bishops + num_black_bishops + num_white_knights + num_black_knights;

    // King vs King
    // King and one minor piece vs King
    if (total_minors <= 1)
        return true;

    // Only bishops?
    if (total_minors == num_white_bishops + num_black_bishops) {
        uint64_t bishops = board.pieceBitboards[bishop];
        bool all_light = (bishops & Bits::dark_squares_mask) == 0;
        bool all_dark  = (bishops & Bits::light_squares_mask) == 0;

        if (all_light || all_dark)
            return true; // All bishops on same color squares
    }

    return false;
}


