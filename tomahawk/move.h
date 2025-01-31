/*
16 bit move representation (to preserve memory during search)

ffffttttttssssss
bits 0-5: start square index
bits 6-11: target square index
bits 12-15: flag (promotion type, etc)
*/

#ifndef MOVE_H
#define MOVE_H

#include "helpers.h"

struct Move 
{
private:
    ushort moveValue;
public:
    // flags
    static constexpr int noFlag = 0b0000;
    static constexpr int enPassantCaptureFlag = 0b0001;
    static constexpr int castleFlag = 0b0010;
    static constexpr int pawnTwoUpFlag = 0b0011;

    static constexpr int promoteToQueenFlag = 0b0100;
    static constexpr int promoteToKnightFlag = 0b0101;
    static constexpr int promoteToRookFlag = 0b0110;
    static constexpr int promoteToBishopFlag = 0b0111;

    // masks
    static constexpr ushort startSquareMask = 0b0000000000111111;
    static constexpr ushort targetSquareMask = 0b0000111111000000;
    static constexpr ushort flagMask = 0b1111000000000000;

    // constructors
    Move (ushort _moveValue) {
        moveValue = _moveValue;
    }

    Move (int _startSquare, int _targetSquare) {
        moveValue = (ushort)(_startSquare | _targetSquare << 6);
    }

    Move (int _startSquare, int _targetSquare, int _flag) {
        moveValue = (ushort)(_startSquare | _targetSquare << 6 | _flag << 12);
    }

    // getters
    ushort Value() { return moveValue; }
    bool IsNull() { return moveValue == 0; }
    int StartSquare() { return moveValue & startSquareMask; }
    int TargetSquare() { return ( moveValue & targetSquareMask) >> 6; }
    bool IsPromotion() { return MoveFlag() >= promoteToQueenFlag; }
    int MoveFlag() { return moveValue >> 12; }
    void PrintMove() { 
        int start = StartSquare();
        int target = TargetSquare();
        std::cout << square_to_algebraic(start) << "->" << square_to_algebraic(target) << "\n" << std::endl; 
    }

    int PromotionPieceType() {
        switch (MoveFlag()) {
            case promoteToQueenFlag:
                return queen;
            case promoteToRookFlag:
                return rook;
            case promoteToBishopFlag:
                return bishop;
            case promoteToKnightFlag:
                return knight;
            default:
                return -1;
        }
    }

    // statics
    static Move NullMove() { return Move(0); }
    static bool SameMove(Move a, Move b) { return a.moveValue == b.moveValue; }

};

#endif