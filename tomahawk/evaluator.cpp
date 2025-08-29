#include "evaluator.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <algorithm>

// =================== Static Member Definitions ===================
int Evaluator::PST_endgame[6][64];
int Evaluator::PST_opening[6][64];
const PrecomputedMoveData* Evaluator::precomp;
int Evaluator::mvvLvaTable[6][6];

// =================== Constructors ===================
Evaluator::Evaluator() {
    loadPST("../bin/pst_opening.txt", PST_opening);
    loadPST("../bin/pst_endgame.txt", PST_endgame);
    initMVVLVA();
}

Evaluator::Evaluator(const PrecomputedMoveData* _precomp) {
    loadPST("../bin/pst_opening.txt", PST_opening);
    loadPST("../bin/pst_endgame.txt", PST_endgame);
    precomp = _precomp;
    initMVVLVA();
}

// =================== Debug Output ===================
void Evaluator::writeEvalDebug(Board& board, const std::string& filename) {
    std::ofstream file(filename, std::ios::app);
    if (!file.is_open()) {
        std::cerr << "Failed to open eval debug file: " << filename << std::endl;
        return;
    }

    file << "FEN: " << board.getBoardFEN() << "\n";

    EvalReport report;
    report.add(MATERIAL, materialDifferences(board));
    report.add(PAWN_STRUCTURE, pawnStructureDifferences(board));
    report.add(PASSED_PAWNS, passedPawnDifferences(board));
    report.add(CENTER_CONTROL, centerControlDifferences(board));
    report.add(MOBILITY, 0); // optional
    report.add(POSITION, positionDifferences(board, PST_opening));
    report.add(KING_SAFETY, kingSafetyDifferences(board));

    int phase = gamePhase(board);

    file << std::fixed << std::setprecision(2);
    report.print(file);
    file << "Phase: " << phase << "\n";

    file << "Total Eval (tapered): " << taperedEval(board) << "\n";
    file << "-----------------------------------\n";
    file.close();
}

// =================== PST Loading ===================
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

        std::istringstream header(line);
        std::string pieceName, dash, phaseName;
        header >> pieceName >> dash >> phaseName;

        auto it = pieceMap.find(pieceName);
        if (it == pieceMap.end()) return false;
        int piece = it->second;

        // Read 8 ranks for the piece
        for (int rank = 0; rank < 8; ++rank) {
            if (!std::getline(file, line)) return false;
            std::istringstream iss(line);
            for (int fileIdx = 0; fileIdx < 8; ++fileIdx) {
                int val;
                if (!(iss >> val)) return false;
                pst[piece][(7 - rank) * 8 + fileIdx] = val; // flip ranks
            }
        }
    }

    return true;
}

// =================== MVV-LVA Table ===================
void Evaluator::initMVVLVA() {
    for (int victim = 0; victim < 6; ++victim) {
        for (int attacker = 0; attacker < 6; ++attacker) {
            mvvLvaTable[victim][attacker] = (victim + 1) * 10 - attacker;
        }
    }
}

// =================== Game Phase ===================
int Evaluator::gamePhase(const Board& board) {
    constexpr int pawn_phase = 1, knight_phase = 1, bishop_phase = 1, rook_phase = 2, queen_phase = 4;
    constexpr int total_phase = 16*pawn_phase + 4*knight_phase + 4*bishop_phase + 4*rook_phase + 2*queen_phase;

    int phase = total_phase;

    // Count remaining pieces
    int wp = countBits(board.colorBitboards[0] & board.pieceBitboards[0]);
    int wn = countBits(board.colorBitboards[0] & board.pieceBitboards[1]);
    int wb = countBits(board.colorBitboards[0] & board.pieceBitboards[2]);
    int wr = countBits(board.colorBitboards[0] & board.pieceBitboards[3]);
    int wq = countBits(board.colorBitboards[0] & board.pieceBitboards[4]);

    int bp = countBits(board.colorBitboards[1] & board.pieceBitboards[0]);
    int bn = countBits(board.colorBitboards[1] & board.pieceBitboards[1]);
    int bb = countBits(board.colorBitboards[1] & board.pieceBitboards[2]);
    int br = countBits(board.colorBitboards[1] & board.pieceBitboards[3]);
    int bq = countBits(board.colorBitboards[1] & board.pieceBitboards[4]);

    phase -= (wp + bp) * pawn_phase;
    phase -= (wn + bn) * knight_phase;
    phase -= (wb + bb) * bishop_phase;
    phase -= (wr + br) * rook_phase;
    phase -= (wq + bq) * queen_phase;

    return (phase * 256 + (total_phase / 2)) / total_phase;
}

// =================== Tapered Eval ===================
int Evaluator::taperedEval(const Board& board) {

    int opening = openingEval(board).weightedTotal;
    int endgame = endgameEval(board).weightedTotal;
    int phase = gamePhase(board);
    int eval = ((opening * (256 - phase)) + (endgame * phase)) / 256;

    return board.is_white_move ? eval : -eval; // negamax
}

// will be expanded for better integration with biases (e.g. has_castling +40 lasts all game)
EvalReport Evaluator::openingEval(const Board& board) {
    return Evaluate(board, PST_opening);
}

EvalReport Evaluator::endgameEval(const Board& board) {
    return Evaluate(board, PST_endgame);
}

// =================== Evaluate Components ===================
EvalReport Evaluator::Evaluate(const Board& board, int pst[6][64]) {
    EvalReport report;

    report.add(MATERIAL, materialDifferences(board));
    report.add(PAWN_STRUCTURE, pawnStructureDifferences(board));
    report.add(PASSED_PAWNS, passedPawnDifferences(board));
    report.add(POSITION, positionDifferences(board, pst));
    report.add(CENTER_CONTROL, centerControlDifferences(board));
    report.add(KING_SAFETY, kingSafetyDifferences(board));
    report.add(MOBILITY, 0); // optional

    report.computeWeighted(evalWeights);
    return report;
}

/*

Evaluation components include:
(All functions operate on Board objects and return an integer score.)

- Material
- Pawn Structure
- Piece-square tables
--- opening + endgame
- Center control
- Passed pawns
- King safety
--- Tropism (proximity of enemy pieces to king)
--- Castling bonuses


Additional functionality:
- Static Exchange Evaluation (SEE)

*/

// -------------------- MATERIAL --------------------

/**
 * Compute material difference: positive for white, negative for black.
 * Adds bishop pair bonus.
 */
int Evaluator::materialDifferences(const Board& board) {
    int pEval = 0;

    for (int side = 0; side < 2; side++) {
        for (int piece = 0; piece < 5; piece++) { // exclude king
            U64 bb = board.colorBitboards[side] & board.pieceBitboards[piece];
            pEval += ((side == 0) ? 1 : -1) * pieceValues[piece] * countBits(bb);

            // Bishop pair bonus
            if (piece == bishop && countBits(bb) == 2) {
                pEval += (side == 0) ? 40 : -40;
            }
        }
    }

    return pEval;
}

// -------------------- PAWN STRUCTURE --------------------

/**
 * Evaluate doubled and isolated pawns.
 */
int Evaluator::pawnStructureDifferences(const Board& board) {
    int psEval = 0;
    U64 pawns = board.pieceBitboards[pawn];
    U64 white = board.colorBitboards[0];
    U64 black = board.colorBitboards[1];

    psEval += countDoubledPawns(pawns & white) - countDoubledPawns(pawns & black);
    psEval += countIsolatedPawns(pawns & white) - countIsolatedPawns(pawns & black);

    return psEval;
}

/** Count doubled pawns on a given color bitboard */
int Evaluator::countDoubledPawns(U64 pawns) {
    int doubledCount = 0;
    for (int file = 0; file < 8; file++) {
        if (countBits(pawns & Bits::file_masks[file]) > 1) {
            doubledCount++;
        }
    }
    return doubledCount / 2;
}

/** Count isolated pawns on a given color bitboard */
int Evaluator::countIsolatedPawns(U64 pawns) {
    int isolatedCount = 0;

    for (int file = 0; file < 8; file++) {
        U64 pawnsOnFile = pawns & Bits::file_masks[file];
        if (!pawnsOnFile) continue;

        U64 adjacentFiles = 0;
        if (file > 0) adjacentFiles |= Bits::file_masks[file - 1];
        if (file < 7) adjacentFiles |= Bits::file_masks[file + 1];

        U64 adjacentPawns = pawns & adjacentFiles;

        while (pawnsOnFile) {
            if (!adjacentPawns) isolatedCount++;
            pawnsOnFile &= pawnsOnFile - 1;
        }
    }

    return isolatedCount;
}

// -------------------- PIECE-SQUARE TABLE / POSITION --------------------

/**
 * Evaluate position using piece-square tables.
 * pst[6][64] = table for each piece type (0..5) and square (0..63)
 */
int Evaluator::positionDifferences(const Board& board, int pst[6][64]) {
    int score = 0;

    for (int i = 0; i < 12; i++) {
        U64 bb = (i < 6) ? board.colorBitboards[0] & board.pieceBitboards[i]
                          : board.colorBitboards[1] & board.pieceBitboards[i - 6];

        while (bb) {
            int sq = getLSB(bb);
            bb &= bb - 1;

            if (i < 6) score += pst[i][sq];
            else score -= pst[i - 6][mirror(sq)];
        }
    }

    return score;
}

// -------------------- CENTER CONTROL --------------------

/**
 * Evaluate center control.
 * Smaller center controlled more by pawns, other pieces contribute less.
 */
int Evaluator::centerControlDifferences(const Board& board) {
    int score = 0;

    for (int color = 0; color <= 1; ++color) {
        int sign = (color == 0) ? 1 : -1;
        for (int pt = 0; pt < 5; ++pt) { // exclude king
            U64 bb = board.pieceBitboards[pt];

            while (bb) {
                int sq = getLSB(bb);
                bb &= bb - 1;

                U64 attacks = 0;

                switch (pt) {
                    case pawn:
                        attacks = precomp->fullPawnAttacks[sq][color];
                        break;
                    case knight:
                        attacks = precomp->blankKnightAttacks[sq];
                        break;
                    case bishop:
                        attacks = Magics::bishopAttacks(sq, 0);
                        break;
                    case rook:
                        attacks = Magics::rookAttacks(sq, 0);
                        break;
                    case queen:
                        attacks = Magics::rookAttacks(sq, 0) | Magics::bishopAttacks(sq, 0);
                        break;
                }

                if (pt == pawn) {
                    score += sign * 5 * countBits(attacks & Bits::centerMasks[0]);
                    score += sign * 5 * countBits(1ULL << sq & Bits::centerMasks[0]);
                } else {
                    score += sign * countBits(attacks & Bits::centerMasks[1]);
                }
            }
        }
    }

    return score / 3;
}

// -------------------- PASSED PAWNS --------------------

/** Check if a pawn is passed */
bool Evaluator::isPassedPawn(bool white, int sq, U64 oppPawns) {
    U64 mask = precomp->passedPawnMasks[sq][white ? 0 : 1];
    return (oppPawns & mask) == 0;
}

/** Evaluate passed pawns difference */
int Evaluator::passedPawnDifferences(const Board& board) {
    U64 white_pawns = board.colorBitboards[0] & board.pieceBitboards[pawn];
    U64 black_pawns = board.colorBitboards[1] & board.pieceBitboards[pawn];

    int pp_white = 0, pp_black = 0;

    int white_king_sq = sqidx(board.colorBitboards[0] & board.pieceBitboards[king]);
    int black_king_sq = sqidx(board.colorBitboards[1] & board.pieceBitboards[king]);

    // White pawns
    while (white_pawns) {
        int sq = getLSB(white_pawns);
        white_pawns &= white_pawns - 1;

        if (isPassedPawn(true, sq, black_pawns)) {
            int rank = sq / 8;
            pp_white += passedBonus[rank];
            if (white_pawns & precomp->fullPawnAttacks[sq][1]) pp_white += 20;

            int own_dist = precomp->king_move_distances[sq][white_king_sq];
            int opp_dist = precomp->king_move_distances[sq][black_king_sq];
            pp_white += std::max(0, 30 - 5 * own_dist);
            pp_white -= (opp_dist <= (7 - rank)) ? 20 : 0;
        }
    }

    // re init
    white_pawns = board.colorBitboards[0] & board.pieceBitboards[pawn];

    // Black pawns
    while (black_pawns) {
        int sq = getLSB(black_pawns);
        black_pawns &= black_pawns - 1;

        if (isPassedPawn(false, sq, white_pawns)) {
            int rank = sq / 8;
            pp_black += passedBonus[7 - rank];
            if (black_pawns & precomp->fullPawnAttacks[sq][0]) pp_black += 20;

            int own_dist = precomp->king_move_distances[sq][black_king_sq];
            int opp_dist = precomp->king_move_distances[sq][white_king_sq];
            pp_black += std::max(0, 30 - 5 * own_dist);
            pp_black -= (opp_dist <= rank) ? 20 : 0;
        }
    }

    return pp_white - pp_black;
}

// -------------------- KING SAFETY --------------------

/** Evaluate king safety for one side */
int Evaluator::kingSafety(const Board& board, bool usWhite) {
    int shield_penalty = kingShield(board, usWhite);        // missing pawns
    int open_penalty = openFilesNearKing(board, usWhite);   // open/semi-open files
    int threat_score = tropism(board, !usWhite);            // attackers nearby

    // Scale down each component
    int scaled_shield = shield_penalty * 3;     // was 10-15 per missing pawn
    int scaled_open = open_penalty * 2;         // was 10-25 per file
    int scaled_threat = threat_score * .5;      // keep tropism additive

    // Limit influence
    scaled_shield = std::min(scaled_shield, 30);
    scaled_open = std::min(scaled_open, 40);
    scaled_threat = std::min(scaled_threat, 60);

    // game phase tapering (king safety matters less in end game)
    int phase = gamePhase(board);
    double phase_factor = std::max(0.2, (24.0 - phase) / 24.0);


    int score = static_cast<int>((scaled_shield + scaled_open + scaled_threat) * phase_factor);

    return score;
}

/** King safety difference (including castling bonus) */
int Evaluator::kingSafetyDifferences(const Board& board) { // black perspective cause kingSafety is penalties
    return castleBias(board) + (kingSafety(board, false) - kingSafety(board, true));
}

/** Castling bonus */
int Evaluator::castleBias(const Board& board) {
    int bonus = 0;
    if (board.white_castled) bonus += 40;
    if (board.black_castled) bonus -= 40;
    return bonus;
}

/** Evaluate missing pawns in front of king */
int Evaluator::kingShield(const Board& board, bool usWhite) {
    int penalty = 0;
    int king_sq = board.kingSquare(usWhite);
    int file = king_sq % 8;
    U64 own_pawns = board.pieceBitboards[pawn] & board.colorBitboards[usWhite ? 0 : 1];

    // Left, center, right
    int left = (file > 0) ? king_sq + (usWhite ? 7 : -9) : 0;
    int center = king_sq + (usWhite ? 8 : -8);
    int right = (file < 7) ? king_sq + (usWhite ? 9 : -7) : 0;
    int left2 = left != 0 ? left + (usWhite ? 8 : -8) : 0;
    int center2 = center + (usWhite ? 8 : -8);
    int right2 = right != 0 ? right + (usWhite ? 8 : -8) : 0;

    auto calcPenalty = [&](int sq1, int sq2) {
        if (!(own_pawns & (1ULL << sq1))) return (!(own_pawns & (1ULL << sq2)) ? 8 : 3);
        return 0;
    };

    penalty += calcPenalty(left, left2);
    penalty += calcPenalty(center, center2);
    penalty += calcPenalty(right, right2);

    return penalty;
}

/** Open and semi-open files near king */
int Evaluator::openFilesNearKing(const Board& board, bool usWhite) {
    int penalty = 0;
    int file = board.kingSquare(usWhite) % 8;

    int open_adj = 10; int open_dir = 15;
    int semi_adj = 5; int semi_dir = 8;

    if (isOpenFile(board, file - 1)) penalty += open_adj;
    if (isOpenFile(board, file)) penalty += open_dir;
    if (isOpenFile(board, file + 1)) penalty += open_adj;

    if (isSemiOpenFile(board, file - 1, usWhite)) penalty += semi_adj;
    if (isSemiOpenFile(board, file, usWhite)) penalty += semi_dir;
    if (isSemiOpenFile(board, file + 1, usWhite)) penalty += semi_adj;

    return penalty;
}

/** Evaluate enemy piece proximity to king */
int Evaluator::tropism(const Board& board, bool usWhite) {
    int score = 0;
    int king_sq = board.kingSquare(usWhite);

    U64 opp_pieces = board.colorBitboards[usWhite ? 1 : 0] & ~board.pieceBitboards[pawn] & ~board.pieceBitboards[king];

    while (opp_pieces) {
        int sq = getLSB(opp_pieces);
        opp_pieces &= opp_pieces - 1;

        int dist = precomp->king_move_distances[sq][king_sq];
        int val = 0;
        switch (board.getMovedPiece(sq)) {
            case 1: val = dist <= 3 ? 8 : 0; break;
            case 2: val = 8; break;
            case 3: val = dist <= 3 ? 15 : 5; break;
            case 4: val = dist <= 3 ? 20 : 10; break;
            default: val = 0; break;
        }

        score += val * (7 - dist);
    }

    return score;
}

/** Checks if file is completely empty */
bool Evaluator::isOpenFile(const Board& board, int file) {
    if (file < 0 || file > 7) return false;
    return !(board.pieceBitboards[0] & Bits::file_masks[file]);
}

/** Checks if file is semi-open for the given color */
bool Evaluator::isSemiOpenFile(const Board& board, int file, bool usWhite) {
    if (file < 0 || file > 7) return false;
    U64 pawns = board.pieceBitboards[0] & (usWhite ? board.colorBitboards[0] : board.colorBitboards[1]);
    return !(Bits::file_masks[file] & pawns);
}

// -------------------- ATTACKER MATERIAL --------------------

int Evaluator::attackerMaterial(const Board& board, int opp_side) {
    int mat = 0;
    U64 bb = board.colorBitboards[opp_side];

    mat += 300 * countBits(bb & board.pieceBitboards[knight]);
    mat += 300 * countBits(bb & board.pieceBitboards[bishop]);
    mat += 500 * countBits(bb & board.pieceBitboards[rook]);
    mat += 900 * countBits(bb & board.pieceBitboards[queen]);

    return mat;
}

// -------------------- STATIC EXCHANGE EVALUATION (SEE) --------------------
// called before capture is made 
// Returns net material gain for the side that STARTS with 'move' (capture).
// If non-capture, return 0 (neutral).
int Evaluator::SEE(const Board& board, const Move& move) {
    int sq = move.TargetSquare();
    int from = move.StartSquare();

    int moverPt12 = board.sqToPiece[from];
    int capturedPt12 = board.sqToPiece[sq];

    if (moverPt12 == -1 || capturedPt12 == -1) return 0; // sanity: only captures

    auto pieceType  = [](int pt12){ return pt12 % 6; };
    auto pieceColor = [](int pt12){ return pt12 / 6; };
    auto valOf      = [&](int pt){ return pieceValues[pt]; };

    int moverColor = pieceColor(moverPt12);

    // Gain stack
    int gain[64];
    int d = 0;
    gain[d++] = valOf(pieceType(capturedPt12));

    // Side to move for recapture
    int stm = 1 - moverColor;

    // Used attackers mask
    U64 used = (1ULL << from);

    // Track current target piece
    int targetPt12 = moverPt12;

    // Compute attackers for a side
    auto computeAttackers = [&](int side) -> U64 {
        U64 attackers = 0ULL;
        U64 occ = board.colorBitboards[0] | board.colorBitboards[1];
        U64 color_bb = board.colorBitboards[side];

        for (int pt = 0; pt < 6; ++pt) {
            U64 bb = board.pieceBitboards[pt] & color_bb & ~used;
            while (bb) {
                int sqFrom = getLSB(bb);
                bb &= bb - 1;

                if (sqFrom < 0 || sqFrom >= 64) continue; 
                int pt12 = board.sqToPiece[sqFrom];
                if (pt12 == -1) continue;

                switch (pt) {
                    case pawn:
                        if (PrecomputedMoveData::fullPawnAttacks[sq][1 - side] & (1ULL << sqFrom))
                            attackers |= 1ULL << sqFrom;
                        break;
                    case knight:
                        if (PrecomputedMoveData::blankKnightAttacks[sqFrom] & (1ULL << sq))
                            attackers |= 1ULL << sqFrom;
                        break;
                    case bishop:
                        if (Magics::bishopAttacks(sqFrom, occ) & (1ULL << sq))
                            attackers |= 1ULL << sqFrom;
                        break;
                    case rook:
                        if (Magics::rookAttacks(sqFrom, occ) & (1ULL << sq))
                            attackers |= 1ULL << sqFrom;
                        break;
                    case queen:
                        if ((Magics::bishopAttacks(sqFrom, occ) | Magics::rookAttacks(sqFrom, occ)) & (1ULL << sq))
                            attackers |= 1ULL << sqFrom;
                        break;
                    case king:
                        if (PrecomputedMoveData::blankKingAttacks[sqFrom] & (1ULL << sq))
                            attackers |= 1ULL << sqFrom;
                        break;
                }
            }
        }

        return attackers;
    };

    U64 attackers[2] = {computeAttackers(0), computeAttackers(1)};

    auto pickLVA = [&](int side, int& fromOut) -> bool {
        U64 cand = attackers[side] & ~used;
        if (!cand) return false;

        int bestSq = -1;
        int bestVal = INT_MAX;
        while (cand) {
            int sqFrom = getLSB(cand);
            cand &= cand - 1;
            if (sqFrom < 0 || sqFrom >= 64) continue;

            int pt12 = board.sqToPiece[sqFrom];
            if (pt12 == -1) continue;

            int v = valOf(pieceType(pt12));
            if (v < bestVal) { bestVal = v; bestSq = sqFrom; }
        }

        if (bestSq == -1) return false;
        fromOut = bestSq;
        return true;
    };

    while (true) {
        int fromAtt = -1;
        if (!pickLVA(stm, fromAtt)) break;

        int attackerPt12 = board.sqToPiece[fromAtt];
        if (attackerPt12 == -1) break;

        //int attackerVal = valOf(pieceType(attackerPt12));
        int victimVal   = valOf(pieceType(targetPt12));

        gain[d++] = victimVal - gain[d-1]; // recapture

        used |= 1ULL << fromAtt;
        targetPt12 = attackerPt12;

        // recompute attackers for next side
        attackers[0] = computeAttackers(0);
        attackers[1] = computeAttackers(1);

        stm ^= 1; // switch side
    }

    // Backtrack for conservative score
    while (--d > 0) gain[d-1] = -std::max(gain[d], -gain[d-1]);

    return gain[0];
}


U64 Evaluator::attackersTo(const Board& board, int sq, bool white, U64 occ) {
    U64 attackers = 0ULL;
    U64 color = white ? board.colorBitboards[0] : board.colorBitboards[1];

    // Pawns
    U64 pawns = board.pieceBitboards[pawn] & color;
    if (white) {
        // Which white pawns can capture on sq? Check squares one rank below
        // Use precomputed pawn attacks from white pawns
        attackers |= pawns & PrecomputedMoveData::fullPawnAttacks[sq][0]; // 0 = white pawns
    } else {
        attackers |= pawns & PrecomputedMoveData::fullPawnAttacks[sq][1]; // 1 = black pawns
    }

    // Knights
    attackers |= (color & board.pieceBitboards[knight]) & PrecomputedMoveData::blankKnightAttacks[sq];

    // Kings
    attackers |= (color & board.pieceBitboards[king]) & PrecomputedMoveData::blankKingAttacks[sq];

    // Sliders
    U64 bishop_sliders = (color & board.pieceBitboards[bishop]) | (color & board.pieceBitboards[queen]);
    U64 rook_sliders   = (color & board.pieceBitboards[rook])   | (color & board.pieceBitboards[queen]);

    attackers |= bishop_sliders & Magics::bishopAttacks(sq, occ);
    attackers |= rook_sliders   & Magics::rookAttacks(sq, occ);

    return attackers;
}
