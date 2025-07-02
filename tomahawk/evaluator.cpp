#include "evaluator.h"

void Evaluator::writeEvalDebug(const MoveGenerator* movegen, Board& board, const std::string& filename) {
    std::ofstream file(filename, std::ios::app); // append mode = std::ios::app
    if (!file.is_open()) {
        std::cerr << "Failed to open eval debug file: " << filename << std::endl;
        return;
    }

    file << "FEN: " << board.getBoardFEN() << "\n";

    float material = materialDifferences(board);
    float pawnStruct = pawnStructureDifferences(board);
    float mobility = mobilityDifferences(movegen);
    float posDiff = positionDifferences(board);

    file << std::fixed << std::setprecision(2);
    file << "Material:       " << material << "\n";
    file << "Pawn Structure: " << pawnStruct << "\n";
    file << "Mobility:       " << mobility << "\n";
    file << "Piece Position: " << posDiff << "\n";

    int totalEval = computeEval(movegen, board);
    file << "Total Eval:     " << totalEval << "\n";
    file << "-----------------------------------\n";

    file.close();
}


constexpr float Evaluator::pieceValues[5];

std::unordered_map<std::string, int> Evaluator::componentEvals = { // eval is in terms of centipawn
        {"Material", 0}, // to view the function of evaluation and improve
        {"PawnStructure", 0}, 
        {"Mobility", 0}, 
        {"PiecePosition", 0} 
    };

const std::unordered_map<std::string, int> Evaluator::evalWeights = { // eval is in terms of centipawn
        {"Material", 100}, // material - raw        of which material is base units
        {"PawnStructure", -50}, // pawn structure (bad stuff)
        {"Mobility", 10}, // num legal moves
        {"PiecePosition", 1} // already in 10s
    };

int Evaluator::Evaluate(const MoveGenerator* movegen, const Board* board) {
    Result result = Arbiter::GetGameState(board);

    // terminal eval
    if (Arbiter::isWinResult(result)) { // side to move is mated
        if (board->is_white_move) {
            return -INF; 
        } else {
            return +INF;

        } 
    }
    if (Arbiter::isDrawResult(result)) {
        return 0;
    }
    // else, normal eval
    // computes components hash in function loop
    return computeEval(movegen, *board);

}

float Evaluator::evaluateComponent(const MoveGenerator* movegen, const Board& board, std::string component) {
    if (component == "Material") {
        componentEvals[component] = materialDifferences(board);
        return componentEvals[component];
    } else if (component == "PawnStructure") {
        componentEvals[component] = pawnStructureDifferences(board);
        return componentEvals[component];
    } else if (component == "Mobility") {
        componentEvals[component] = mobilityDifferences(movegen);
        return componentEvals[component];
    } else if (component == "PiecePosition") {
        componentEvals[component] = positionDifferences(board);
        return componentEvals[component];
    } else {
        return 1;
    }
}

int Evaluator::computeEval(const MoveGenerator* movegen, const Board& board) {
    int eval = 0;

    for (const auto& pair : evalWeights) {
        const std::string& name = pair.first;
        int weight = pair.second;
        float value = evaluateComponent(movegen, board, name);
        eval += int(weight * value);
    }

    return eval;
}

std::unordered_map<std::string, int> Evaluator::computeComponents(const MoveGenerator* movegen, const Board& board) {
    std::unordered_map<std::string, int> component_hash;
    
    for (const auto& pair : evalWeights) { // used for analysis
        const std::string& name = pair.first;
        float value = evaluateComponent(movegen, board, name);
        component_hash[name] = value;
    }

    return component_hash;
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
    int psEval = 0;
    U64 pawns = position.pieceBitboards[0];
    U64 white = position.colorBitboards[0]; U64 black = position.colorBitboards[1];

    psEval += (countDoubledPawns(pawns&white) - countDoubledPawns(pawns&black));
    psEval += (countIsolatedPawns(pawns&white) - countIsolatedPawns(pawns&black));
    
    return psEval;
}
float Evaluator::mobilityDifferences(const MoveGenerator* movegen) { // # moves (psuedo possibilities)
    // return values computed during movegen 
    int w_mob = movegen->white_mobility; int b_mob = movegen->black_mobility;
    return w_mob - b_mob;
}
float Evaluator::positionDifferences(const Board& position) {
    int score = 0;
    U64 bb;

    for (int i = 0; i < 12; ++i) {
        if (i < 6) { // white
            bb = position.colorBitboards[0] & position.pieceBitboards[i];
        } else { // black (shift pieces)
            bb = position.colorBitboards[1] & position.pieceBitboards[i-6];
        }
        while (bb) {
            int sq = __builtin_ctzll(bb);
            if (i < 6) {
                score += PST[i][sq];
            } else {
                int mirrorSq = mirror(sq);
                score -= PST[i-6][mirrorSq];
            }
            bb &= bb - 1;
        }
    }

    return static_cast<float>(score);
}

// Detect doubled pawns on a color bitboard
int Evaluator::countDoubledPawns(U64 pawns) {
    std::vector<int> doubledFiles;
    for (int file = 0; file < 8; file++) {
        U64 pawnsOnFile = pawns & Bits::file_masks[file];
        if (countBits(pawnsOnFile) > 1) {
            doubledFiles.push_back(file); // file with doubled pawns
        }
    }
    return doubledFiles.size();
}

// Detect isolated pawns
int Evaluator::countIsolatedPawns(U64 pawns) {
    std::vector<int> isolatedSquares;

    for (int file = 0; file < 8; file++) {
        U64 pawnsOnFile = pawns & Bits::file_masks[file];
        if (pawnsOnFile == 0) continue;

        // Adjacent files mask
        U64 adjacentFiles = 0;
        if (file > 0) adjacentFiles |= Bits::file_masks[file - 1];
        if (file < 7) adjacentFiles |= Bits::file_masks[file + 1];

        U64 adjacentPawns = pawns & adjacentFiles;

        // For each pawn on this file, check if adjacent pawns exist
        while (pawnsOnFile) {
            int sq = sqidx(pawnsOnFile); // find LS1B
            U64 sqMask = 1ULL << sq;

            // If no adjacent pawns, this pawn is isolated
            if ((adjacentPawns & sqMask) == 0) {
                isolatedSquares.push_back(sq);
            }

            pawnsOnFile &= pawnsOnFile - 1; // pop LS1B
        }
    }
    return isolatedSquares.size();
}
