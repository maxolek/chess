#include "evaluator.h"

// static vars
constexpr float Evaluator::pieceValues[5];
constexpr int Evaluator::passedBonus[8];
bool Evaluator::end_pstLoaded = false; bool Evaluator::open_pstLoaded = false;
int Evaluator::PST_endgame[6][64]; int Evaluator::PST_opening[6][64];
const PrecomputedMoveData* Evaluator::precomp;

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

Evaluator::Evaluator(const PrecomputedMoveData* _precomp) {
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

    precomp = _precomp;
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
    int pass_pawn = passedPawnDifferences(board);
    int cent_cntrl = centerControlDifferences(board);
    float mobility = mobilityDifferences(movegen);
    float posDiff_open = positionDifferences(board, PST_opening);
    float posDiff_end = positionDifferences(board, PST_endgame);

    file << std::fixed << std::setprecision(2);
    file << "Material:       " << material << "\n";
    file << "Pawn Structure: " << pawnStruct << "\n";
    file << "Passed Pawns: " << pass_pawn << "\n";
    file << "Center Control: " << cent_cntrl << "\n";
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


std::unordered_map<std::string, int> Evaluator::componentEvals = { // eval is in terms of centipawn
        {"Material", 0}, // to view the function of evaluation and improve
        {"PawnStructure", 0}, 
        {"PassedPawns", 0},
        {"Mobility", 0}, 
        {"PiecePosition", 0},
        {"CenterControl", 0}
        //{"HangingPieces", 0}
    };

const std::unordered_map<std::string, int> Evaluator::evalWeights = { // eval is in terms of centipawn
        {"Material", 100}, // material - raw        of which material is base units 
        {"PawnStructure", -25}, // pawn structure (bad stuff)
        {"PassedPawns", 1}, // 30-40s-ish with scaling
        {"Mobility", 10}, // num legal moves
        {"PiecePosition", 1}, // already in 10s
        {"CenterControl", 1} // attack counts so lots of 1,2,3s
        //{"HangingPieces", -75} // value of SEE
    };



int Evaluator::gamePhase(const Board* position) {
    int pawn_phase = 1; int knight_phase = 1; int bishop_phase = 1;
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

// movegen count used for result instead of Arbiter
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
/*
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

    if (board->pieceBitboards[0] == 0) { // no pawns
        return ((opening * (256 - phase)) + (endgame * phase)) / 256 + mopUp(*board);
    } else if (phase <= 128) { // early queen development penalty
        return ((opening * (256 - phase)) + (endgame * phase)) / 256 - earlyQueenPenalty(*board);
    } else {
        return ((opening * (256 - phase)) + (endgame * phase)) / 256;
    }
}
    */

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

    // eval adjustments (must return in centipawn from whites perspective)
    int mop_up = board->pieceBitboards[0] == 0 ? mopUp(*board) : 0;
    int early_queen = gamePhase(board) <= 128 ? earlyQueenPenalty(*board) : 0;
    int hanging_pieces = hangingPiecePenalty(*board);
    int castle_bias = castleBias(*board);

    int eval_adj = /*mop_up +*/ early_queen + hanging_pieces + castle_bias;

    return computeEval(*board, pst) + eval_adj;

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
    } else if (component == "PassedPawns") {
        componentEvals[component] = passedPawnDifferences(board);
        return componentEvals[component];
    //} else if (component == "Mobility") {
    //    componentEvals[component] = mobilityDifferences(movegen);
    //    return componentEvals[component];
    } else if (component == "PiecePosition") {
        componentEvals[component] = positionDifferences(board, pst);
        return componentEvals[component];
    } else if (component == "CenterControl") {
        componentEvals[component] = centerControlDifferences(board);
        return componentEvals[component];
    //} else if (component == "HangingPieces") {
    //    componentEvals[component] = hangingPiecePenalty(board);
    //    return componentEvals[component];
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

//int Evaluator::negamax_eval(const Board& board)

float Evaluator::materialDifferences(const Board& board) { // add up material
    U64 bb;
    float pEval = 0.0f;

    for (int side = 0; side < 2; side++) {
        // +white -black
        for (int piece = 0; piece < 5; piece++) {
            // exlude king
            bb = board.colorBitboards[side] & board.pieceBitboards[piece];
            pEval += std::pow(-1,side) * pieceValues[piece] * countBits(bb);
            if (piece == bishop && countBits(bb) == 2) {pEval += (side==0) ? .3 : -.3;} // 7 -> 7.3 for bishop pair
        }
    }

    return pEval;
}
float Evaluator::pawnStructureDifferences(const Board& position) { // double, block, iso
    int psEval = 0;
    U64 pawns = position.pieceBitboards[0];
    U64 white = position.colorBitboards[0]; U64 black = position.colorBitboards[1];

    psEval += (countDoubledPawns(pawns&white) - countDoubledPawns(pawns&black)); // func counts doubles files
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
int Evaluator::centerControlDifferences(const Board& position) {
    int score = 0;
    const U64 centerMask_pawns = precomp->centerMasks[0];
    const U64 centerMask_pieces = precomp->centerMasks[1];
    int sq; int sign; int controlled; U64 bb; U64 attacks;

    for (int color = 0; color <= 1; ++color) {
        sign = (color == 0) ? 1 : -1; // white = +, black = -

        for (int pt = 0; pt < 5; ++pt) { // exclude king
            bb = position.pieceBitboards[pt];

            while (bb) {
                sq = tzcnt(bb) - 1;
                bb &= bb -1; 

                U64 attacks = 0; // +pawns on smaller center space

                switch (pt) {
                    case pawn:
                        attacks = precomp->fullPawnAttacks[sq][color];
                        break;
                    case knight:
                        attacks = precomp->blankKnightAttacks[sq];
                        break;
                    case bishop:
                        for (int direction = 0; direction < 2; direction++) {
                            attacks |= precomp->blankBishopAttacks[sq][direction].lineEx; // adjust if sliding masks vary
                        }
                        break;
                    case rook:
                        for (int direction = 0; direction < 2; direction++) {
                            attacks |= precomp->blankRookAttacks[sq][direction].lineEx;
                        }
                        break;
                    case queen:
                        for (int direction = 0; direction < 4; direction++) {
                            attacks |= precomp->blankQueenAttacks[sq][direction].lineEx;
                        }
                        break;
                }

                if (pt == pawn) {
                    score += sign * 5 * countBits(attacks & precomp->centerMasks[0]); // smaller centers
                    score += sign * 5 * countBits((1ULL << sq) & precomp->centerMasks[0]); // occ of center square
                } else {
                    score += sign * 1 * countBits(attacks & precomp->centerMasks[1]);
                }
            }
        }
    }

    return score/3;
}

// not tested
int Evaluator::mopUp(const Board& position) {
    U64 white_pawns = position.colorBitboards[0] & position.pieceBitboards[pawn];
    U64 black_pawns = position.colorBitboards[1] & position.pieceBitboards[pawn];

    if (white_pawns != 0 && black_pawns != 0) {
        return 0; // Not an endgame mop up scenario
    }

    int score = 0;

    // Example: bonus for active king in endgame
    int white_king_sq = sqidx(position.colorBitboards[0] & position.pieceBitboards[king]);
    int black_king_sq = sqidx(position.colorBitboards[1] & position.pieceBitboards[king]);

    // King proximity to center or opponent pawns
    score += 10 * (14 - precomp->king_move_distances[white_king_sq][black_king_sq]); // encourage close kings

    // Possibly add bonuses for passed pawns or piece activity here

    return score;
}

// not tested
int Evaluator::earlyQueenPenalty(const Board& board) {
    int phase = gamePhase(&board);
    if (phase > 192) return 0; // Late enough in the game

    int penalty = 0;

    // Evaluate white queen
    U64 wq = board.colorBitboards[0] & board.pieceBitboards[queen];
    if (wq) {
        int sq = tzcnt(wq) - 1;
        if (sq != d1) {
            int minor_count = countBits(board.colorBitboards[0] & 
                                        (board.pieceBitboards[knight] | board.pieceBitboards[bishop]));
            if (minor_count > 2) penalty += 50;
            else if (board.currentGameState.HasKingsideCastleRight(true) || board.currentGameState.HasQueensideCastleRight(true))
                penalty -= 50;
            else if (sq / 8 >= 4) penalty += 30;
            else penalty += 10;
        }
    }

    // Evaluate black queen
    U64 bq = board.colorBitboards[1] & board.pieceBitboards[queen];
    if (bq) {
        int sq = tzcnt(bq) - 1;
        if (sq != d8) {
            int minor_count = countBits(board.colorBitboards[1] & 
                                        (board.pieceBitboards[knight] | board.pieceBitboards[bishop]));
            if (minor_count > 2) penalty -= 50;
            else if (board.currentGameState.HasKingsideCastleRight(false) || board.currentGameState.HasQueensideCastleRight(false))
                penalty -= 50;
            else if (sq / 8 <= 3) penalty -= 30;
            else penalty -= 10;
        }
    }

    return -penalty;
}
int Evaluator::castleBias(const Board& board) {
    int bonus = 0;

    if (board.plyCount < 4) {return bonus;}

    // Reward castling rights (only reward castle moves)
    /*
    if (board.currentGameState.HasKingsideCastleRight(true))
        bonus += castleRightBonus;
    if (board.currentGameState.HasQueensideCastleRight(true))
        bonus += castleRightBonus;

    if (board.currentGameState.HasKingsideCastleRight(false))
        bonus -= castleRightBonus;
    if (board.currentGameState.HasQueensideCastleRight(false))
        bonus -= castleRightBonus;
    */

    // after move, color is flipped
    if (!board.is_white_move && board.allGameMoves.back().MoveFlag() == Move::castleFlag) bonus += 40;
    if (board.is_white_move && board.allGameMoves.back().MoveFlag() == Move::castleFlag) bonus -= 40;

    return bonus;
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

// static exchange evaluation
int Evaluator::SEE(const Board& board, int sq, bool usWhite) {
    constexpr int pieceOrder[6] = { pawn, knight, bishop, rook, queen, king };

    auto pieceAt = [&](int sq) -> int {
        for (int pt = 0; pt < 12; ++pt) {
            if (board.pieceBitboards[pt] & (1ULL << sq)) return pt;
        }
        return -1;
    };

    U64 occupied = board.colorBitboards[0] | board.colorBitboards[1];

    // Initialize attackers
    U64 whiteAttackers = attackersTo(board, sq, true, occupied);
    U64 blackAttackers = attackersTo(board, sq, false, occupied);

    U64 attackers[2] = { whiteAttackers, blackAttackers };
    int color = usWhite ? 0 : 1;

    float gain[32] = {};
    int depth = 0;

    int target = pieceAt(sq);
    if (target == -1) return 0;

    gain[depth++] = pieceValues[target];

    U64 used = 0ULL;

    while (true) {
        // Find the least valuable attacker
        int from = -1;
        float minValue = 10000.0f;

        for (int i = 0; i < 64; ++i) {
            if ((attackers[color] & (1ULL << i)) && !(used & (1ULL << i))) {
                int pt = pieceAt(i);
                if (pt != -1 && pieceValues[pt] < minValue) {
                    minValue = pieceValues[pt];
                    from = i;
                }
            }
        }

        if (from == -1) break;

        // Remove attacker
        used |= (1ULL << from);
        occupied &= ~(1ULL << from);

        int pt = pieceAt(from);
        gain[depth] = pieceValues[pt] - gain[depth - 1];
        gain[depth] = std::max(-gain[depth - 1], gain[depth]);
        depth++;

        // Recalculate attackers dynamically (x-ray attacks)
        attackers[0] = attackersTo(board, sq, true, occupied);
        attackers[1] = attackersTo(board, sq, false, occupied);

        color ^= 1;
    }

    return static_cast<int>(gain[depth - 1]);
}


int Evaluator::hangingPiecePenalty(const Board& board) {
    int penalty = 0;

    for (int color = 0; color <= 1; ++color) {
        bool usWhite = (color == 0);
        int sign = usWhite ? -1 : 1; // decrease eval for white hanging, increase for black

        for (int piece = knight; piece <= queen; ++piece) {
            U64 bb = board.colorBitboards[color] & board.pieceBitboards[piece];
            while (bb) {
                int sq = __builtin_ctzll(bb);
                bb &= bb - 1;

                int see = SEE(board, sq, usWhite);
                if (see < 0) {
                    int value = pieceValues[piece];
                    penalty += sign * (2*value - see); // extra penalty for deeper loss
                }
            }
        }
    }

    return penalty;
}


U64 Evaluator::attackersTo(const Board& board, int sq, bool white, U64 occ) {
    U64 attackers = 0ULL;
    U64 color = white ? board.colorBitboards[0] : board.colorBitboards[1];

    if (white) {
        // Pawn attacks: attacked by white pawn means we use black pawn attack mask
        attackers |= (color & board.pieceBitboards[pawn]) & PrecomputedMoveData::fullPawnAttacks[sq][1];
        attackers |= (color & board.pieceBitboards[knight]) & PrecomputedMoveData::blankKnightAttacks[sq];
        attackers |= (color & board.pieceBitboards[king]) & PrecomputedMoveData::blankKingAttacks[sq];

        U64 bishop_sliders = (color & board.pieceBitboards[bishop]) | (color & board.pieceBitboards[queen]);
        U64 rook_sliders   = (color & board.pieceBitboards[rook])   | (color & board.pieceBitboards[queen]);

        attackers |= bishop_sliders & Magics::bishopAttacks(sq, occ);
        attackers |= rook_sliders   & Magics::rookAttacks(sq, occ);
    } else {
        // Pawn attacks: attacked by black pawn means we use white pawn attack mask
        attackers |= (color & board.pieceBitboards[pawn]) & PrecomputedMoveData::fullPawnAttacks[sq][0];
        attackers |= (color & board.pieceBitboards[knight]) & PrecomputedMoveData::blankKnightAttacks[sq];
        attackers |= (color & board.pieceBitboards[king]) & PrecomputedMoveData::blankKingAttacks[sq];

        U64 bishop_sliders = (color & board.pieceBitboards[bishop]) | (color & board.pieceBitboards[queen]);
        U64 rook_sliders   = (color & board.pieceBitboards[rook])   | (color & board.pieceBitboards[queen]);

        attackers |= bishop_sliders & Magics::bishopAttacks(sq, occ);
        attackers |= rook_sliders   & Magics::rookAttacks(sq, occ);
    }

    return attackers;
}


// count backwards pawns
// passed pawns
bool Evaluator::isPassedPawn(bool white, int sq, U64 opp_pawns) {
    U64 mask = precomp->passedPawnMasks[sq][white ? 0 : 1];
    return (opp_pawns & mask) == 0;
}
int Evaluator::passedPawnDifferences(const Board& board) {
    U64 white_pawns = board.colorBitboards[0] & board.pieceBitboards[0];
    U64 black_pawns = board.colorBitboards[1] & board.pieceBitboards[0];
    int pp_white = 0; int pp_black = 0;
    int sq; int rank;
    int white_king_square = sqidx(board.colorBitboards[0] & board.pieceBitboards[king]);
    int black_king_square = sqidx(board.colorBitboards[1] & board.pieceBitboards[king]);
    int own_king_dist; int opp_king_dist;
    
    while (white_pawns) {
        sq = tzcnt(white_pawns) - 1;
        white_pawns &= white_pawns -1;  
        
        if (isPassedPawn(true, sq, black_pawns)) {
            rank = sq / 8;
            pp_white += passedBonus[rank];
            
            // connected bonus
            if (white_pawns & precomp->fullPawnAttacks[sq][1]) { // opp side cause cnxn is behind
                pp_white += 20;
            }
            // king proximity decrease
            own_king_dist = precomp->king_move_distances[sq][white_king_square];
            opp_king_dist = precomp->king_move_distances[sq][black_king_square];
            pp_white += std::max(0, 30 - 5 * own_king_dist); // own king to escort
            pp_white -= (opp_king_dist <= (7 - rank)) ? 20 : 0; // opp king "in square" to stop pawn
        }
    }

    white_pawns = board.colorBitboards[0] & board.pieceBitboards[0];
    while (black_pawns) {
        sq = tzcnt(black_pawns) - 1;
        black_pawns &= black_pawns -1;  
        
        if (isPassedPawn(false, sq, white_pawns)) {
            rank = sq / 8;
            pp_black += passedBonus[7 - rank];

            // connected bonus
            if (black_pawns & precomp->fullPawnAttacks[sq][0]) { // opp side cause cnxn is behind
                pp_black += 20;
            }
            // king proximity decrease
            own_king_dist = precomp->king_move_distances[sq][black_king_square];
            opp_king_dist = precomp->king_move_distances[sq][white_king_square];
            pp_black += std::max(0, 30 - 5 * own_king_dist); // own king to escort
            pp_black -= (opp_king_dist <= (rank)) ? 20 : 0; // opp king "in square" to stop pawn
        }
    }

    return (pp_white - pp_black); 
}

// 