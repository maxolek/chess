#include "evaluator.h"

constexpr float Evaluator::pieceValues[5];
const std::unordered_map<std::string, int> Evaluator::evalWeights = { // eval is in terms of centipawn
        {"Material", 100}, // material - raw        of which material is base units
        {"PawnStructure", -50}, // pawn structure (bad stuff)
        {"Mobility", 10} // num legal moves
    };

int Evaluator::Evaluate(const Board* board) {
    Result result = Arbiter::GetGameState(board);

    // terminal eval
    if (Arbiter::isWinResult(result)) {
        return board->is_white_move ? -INF : +INF; // side to move is mated
    }
    if (Arbiter::isDrawResult(result)) {
        return 0;
    }
    // else, normal eval
    return computeEval(*board);
}

float Evaluator::evaluateComponent(const Board& board, std::string component) {
    if (component == "Material") {
        return materialDifferences(board);
    } else if (component == "PawnStructure") {
        return pawnStructureDifferences(board);
    } else if (component == "Mobility") {
        return mobilityDifferences(board);
    } else {
        return 1;
    }
}

int Evaluator::computeEval(const Board& board) {
    int eval = 0;

    for (const auto& pair : evalWeights) {
        const std::string& name = pair.first;
        int weight = pair.second;
        float value = evaluateComponent(board, name);
        eval += int(weight * value);
    }

    return eval;
}

float Evaluator::materialDifferences(const Board& board) { // add up material
    U64 bb;
    float pEval = 0.0f;

    for (int side = 0; side < 2; side++) {
        // +white -black
        for (int piece = 0; piece < 5; piece++) {
            // exlude king
            bb = board.colorBitboards[side] & board.pieceBitboards[piece];
            pEval += std::pow(-1,side) * pieceValues[piece] * countBits(bb);
        }
    }

    return pEval;
}
float Evaluator::pawnStructureDifferences(const Board& position) { // double, block, iso
    return 1;
}
float Evaluator::mobilityDifferences(const Board& position) { // # moves (psuedo possibilities)
    return 1;
}
