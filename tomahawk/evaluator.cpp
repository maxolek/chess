#include "evaluator.h"

bool Evaluator::end_pstLoaded = false; bool Evaluator::open_pstLoaded = false;
int Evaluator::PST_endgame[6][64]; int Evaluator::PST_opening[6][64];

Evaluator::Evaluator() {
     if (!open_pstLoaded) {
        if (!loadPST("C:/Users/maxol/code/chess/bin/pst_opening.txt", PST_opening)) {
            //std::cerr << "failed to load opening pst" << std::endl;
        }
        open_pstLoaded = true;
    }
    if (!end_pstLoaded) {
        if (!loadPST("C:/Users/maxol/code/chess/bin/pst_endgame.txt", PST_endgame)) {
            //std::cerr << "failed to load endgame pst" << std::endl;
        }
        end_pstLoaded = true;
    }
}

void Evaluator::writeEvalDebug(const MoveGenerator* movegen, Board& board, const std::string& filename) {
    if (!open_pstLoaded) {
        if (!loadPST("C:/Users/maxol/code/chess/bin/pst_opening.txt", PST_opening)) {
            std::cerr << "failed to load opening pst" << std::endl;
        }
        open_pstLoaded=true;
    }
    if (!end_pstLoaded) {
        if (!loadPST("C:/Users/maxol/code/chess/bin/pst_endgame.txt", PST_endgame)) {
            std::cerr << "failed to load endgame pst" << std::endl;
        }
        end_pstLoaded=true;
    }

    std::ofstream file(filename, std::ios::app); // append mode = std::ios::app
    if (!file.is_open()) {
        std::cerr << "Failed to open eval debug file: " << filename << std::endl;
        return;
    }

    file << "FEN: " << board.getBoardFEN() << "\n";

    float material = materialDifferences(board);
    float pawnStruct = pawnStructureDifferences(board);
    float mobility = mobilityDifferences(movegen);
    float posDiff_open = positionDifferences(board, PST_opening);
    float posDiff_end = positionDifferences(board, PST_endgame);

    file << std::fixed << std::setprecision(2);
    file << "Material:       " << material << "\n";
    file << "Pawn Structure: " << pawnStruct << "\n";
    file << "Mobility:       " << mobility << "\n";
    file << "Piece Position (open / end): " << posDiff_open << " / " << posDiff_end << "\n";

    int totalEval = taperedEval(&board);
    file << "Total Eval:     " << totalEval << "\n";
    file << "-----------------------------------\n";

    file.close();
}

// Load PST from file: 6 pieces, 64 squares each
bool Evaluator::loadPST(const std::string& filename, int pst[6][64]) {
    std::ifstream file(filename);

    if (!file.is_open()) {
        std::cerr << "Failed to open PST file: " << filename << std::endl;
        return false;
    }

    const std::unordered_map<std::string, int> pieceMap = {
        {"Pawn", 0}, {"Knight", 1}, {"Bishop", 2},
        {"Rook", 3}, {"Queen", 4}, {"King", 5}
    };

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        // Parse the header, like "Knight - endgame"
        std::istringstream headerStream(line);
        std::string pieceName, dash, phaseName;
        headerStream >> pieceName >> dash >> phaseName;

        auto it = pieceMap.find(pieceName);
        if (it == pieceMap.end()) {
           // std::cerr << "Unexpected header line: " << line << std::endl;
            return false;
        }
        int piece = it->second;

        // Read the 8 lines that follow
        for (int rank = 0; rank < 8; ++rank) {
            if (!std::getline(file, line)) {
                //std::cerr << "Unexpected EOF while reading PST data for " << pieceName << std::endl;
                return false;
            }

            std::istringstream iss(line);
            for (int fileIdx = 0; fileIdx < 8; ++fileIdx) {
                int val;
                if (!(iss >> val)) {
                   // std::cerr << "Failed to parse PST value at " << pieceName
                    //          << ", rank " << rank << ", file " << fileIdx << std::endl;
                    return false;
                }
                pst[piece][(7-rank) * 8 + fileIdx] = val;
            }
        }
    }

    return true;
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
        {"PawnStructure", -25}, // pawn structure (bad stuff)
        {"Mobility", 10}, // num legal moves
        {"PiecePosition", 1} // already in 10s
    };



int Evaluator::gamePhase(const Board* position) {
    int pawn_phase = 0; int knight_phase = 1; int bishop_phase = 1;
    int rook_phase = 2; int queen_phase = 4; 
    int total_phase = 16*pawn_phase + 4*knight_phase + 4*bishop_phase + 4*rook_phase + 2*queen_phase;
    int phase = total_phase;

    int wp = countBits(position->colorBitboards[0] & position->pieceBitboards[0]); // white pawns
    int wn = countBits(position->colorBitboards[0] & position->pieceBitboards[1]); // white knights
    int wb = countBits(position->colorBitboards[0] & position->pieceBitboards[2]); // white bishops
    int wr = countBits(position->colorBitboards[0] & position->pieceBitboards[3]); // white rooks
    int wq = countBits(position->colorBitboards[0] & position->pieceBitboards[4]); // white queens
    
    int bp = countBits(position->colorBitboards[1] & position->pieceBitboards[0]); // black pawns
    int bn = countBits(position->colorBitboards[1] & position->pieceBitboards[1]); // black knights
    int bb = countBits(position->colorBitboards[1] & position->pieceBitboards[2]); // black bishops
    int br = countBits(position->colorBitboards[1] & position->pieceBitboards[3]); // black rooks
    int bq = countBits(position->colorBitboards[1] & position->pieceBitboards[4]); // black queens
    
    phase -= (wp+bp)*pawn_phase; phase -= (wn+bn)*knight_phase;
    phase -= (wb+bb)*bishop_phase; phase -= (wr+br)*rook_phase;
    phase -= (wq+bq)*queen_phase; 

    phase = (phase * 256 + (total_phase / 2)) / total_phase;

    return phase;
}

int Evaluator::taperedEval(const Board* board) {
    // assume terminal evals have already been checked for in search
    // if wanting a pure board eval, then that will need to be built into it (perhaps a new func)
    /*
    Result result = Arbiter::GetGameState(board);

    // terminal eval
    if (Arbiter::isWinResult(result)) { // side to move is mated
        if (board->is_white_move) {
            return -INF; 
        } else {
            return +INF;
        } 
    } else if (Arbiter::isDrawResult(result)) {
        return 0;
    }
    */

    int opening = openingEval(board);
    int endgame = endgameEval(board);
    int phase = gamePhase(board);

    return ((opening * (256 - phase)) + (endgame * phase)) / 256;
}

int Evaluator::taperedEval(const Board* board, Result result) {
    // terminal eval
    if (Arbiter::isWinResult(result)) { // side to move is mated
        if (board->is_white_move) {
            return -MATE_SCORE; 
        } else {
            return +MATE_SCORE;
        } 
    } else if (Arbiter::isDrawResult(result)) {
        return 0;
    }

    int opening = openingEval(board);
    int endgame = endgameEval(board);
    int phase = gamePhase(board);

    return ((opening * (256 - phase)) + (endgame * phase)) / 256;
}

int Evaluator::openingEval(const Board* board) {
    return Evaluate(board, PST_opening);
}

int Evaluator::endgameEval(const Board* board) {
    return Evaluate(board, PST_endgame);
}

int Evaluator::Evaluate(const Board* board, int pst[6][64]) {
    //Result result = Arbiter::GetGameState(board);

    // terminal eval
    //if (Arbiter::isWinResult(result)) { // side to move is mated
    //    if (board->is_white_move) {
    //        return -INF; 
    //    } else {
    //        return +INF;
    //
    //    } 
    //}
    //if (Arbiter::isDrawResult(result)) {
    //    return 0;
    //}
    // else, normal eval
    // computes components hash in function loop
    return computeEval(*board, pst);

}

int Evaluator::computeEval(const Board& board, int pst[6][64]) {
    int eval = 0;

    for (const auto& pair : evalWeights) {
        const std::string& name = pair.first;
        int weight = pair.second;
        float value = evaluateComponent(board, name, pst);
        eval += int(weight * value);
    }

    return eval;
}

float Evaluator::evaluateComponent(const Board& board, std::string component, int pst[6][64]) {
    if (component == "Material") {
        componentEvals[component] = materialDifferences(board);
        return componentEvals[component];
    } else if (component == "PawnStructure") {
        componentEvals[component] = pawnStructureDifferences(board);
        return componentEvals[component];
    //} else if (component == "Mobility") {
    //    componentEvals[component] = mobilityDifferences(movegen);
    //    return componentEvals[component];
    } else if (component == "PiecePosition") {
        componentEvals[component] = positionDifferences(board, pst);
        return componentEvals[component];
    } else {
        return 1;
    }
}

std::unordered_map<std::string, int> Evaluator::computeComponents(const Board& board, int pst[6][64]) {
    std::unordered_map<std::string, int> component_hash;
    
    for (const auto& pair : evalWeights) { // used for analysis
        const std::string& name = pair.first;
        float value = evaluateComponent(board, name, pst);
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
            if (piece == bishop && countBits(bb) == 2) {pEval += std::pow(-1,side) * .4;} // 6.6 -> 7 for bishop pair
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
    //int w_mob = movegen->white_mobility; int b_mob = movegen->black_mobility;
    //return w_mob - b_mob;
    return 0;
}
float Evaluator::positionDifferences(const Board& position, int pst[6][64]) {
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
                score += pst[i][sq];
            } else {
                int mirrorSq = mirror(sq);
                score -= pst[i-6][mirrorSq];
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
    int isolatedCount = 0;//std::vector<int> isolatedSquares;

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
            //int sq = sqidx(pawnsOnFile); // find LS1B
            //U64 sqMask = 1ULL << sq;

            // If no adjacent pawns, this pawn is isolated
            if (adjacentPawns == 0) {
                isolatedCount++;//isolatedSquares.push_back(sq);
            }

            pawnsOnFile &= pawnsOnFile - 1; // pop LS1B
        }
    }
    return isolatedCount;
}
