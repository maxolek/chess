#include "evaluator.h"

// static vars
constexpr int Evaluator::pieceValues[6];
constexpr int Evaluator::passedBonus[8];
bool Evaluator::end_pstLoaded = false; bool Evaluator::open_pstLoaded = false;
int Evaluator::PST_endgame[6][64]; int Evaluator::PST_opening[6][64];
const PrecomputedMoveData* Evaluator::precomp;
int Evaluator::mvvLvaTable[6][6]; // viction/attacker higher is better

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

    initMVVLVA();
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
    initMVVLVA();
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

    // main eval
    int material = materialDifferences(board);
    int pawnStruct = pawnStructureDifferences(board);
    int pass_pawn = passedPawnDifferences(board);
    int cent_cntrl = centerControlDifferences(board);
    //int mobility = mobilityDifferences(movegen);
    int king_safety = kingSafetyDifferences(board);
    int posDiff_open = positionDifferences(board, PST_opening);
    int posDiff_end = positionDifferences(board, PST_endgame);
    int phase = gamePhase(&board);

    // adjustments
    int mop_up = board.pieceBitboards[0] == 0 ? mopUp(board) : 0;
    int early_queen = gamePhase(&board) <= 128 ? earlyQueenPenalty(board) : 0;
    //int hanging_pieces = hangingPiecePenalty(board);
    //int castle_bias = castleBias(board);

    file << std::fixed << std::setprecision(2);
    file << "Material:       " << material << "\n";
    file << "Pawn Structure: " << pawnStruct << "\n";
    file << "Passed Pawns: " << pass_pawn << "\n";
    file << "Center Control: " << cent_cntrl << "\n";
    file << "Mobility:       " << mobility << "\n";
    file << "Piece Position (open / end): " << posDiff_open << " / " << posDiff_end << "\n\n";
    file << "Phase: " << phase << "\n";
    file << "Early Queen Penalty: " << early_queen << "\n";
    file << "King Safety: " << king_safety << "\n";
    //file << "Hanging Pieces Penalty: " << hanging_pieces << "\n";


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

void Evaluator::initMVVLVA() {
    for (int victim = 0; victim < 6; ++victim) {
        for (int attacker = 0; attacker < 6; ++attacker) {
            // More valuable victim, less valuable attacker -> higher score
            mvvLvaTable[victim][attacker] = (victim + 1) * 10 - attacker;
        }
    }
}



std::unordered_map<std::string, int> Evaluator::componentEvals = { // eval is in terms of centipawn
        {"Material", 0}, // to view the function of evaluation and improve
        {"PawnStructure", 0}, 
        {"PassedPawns", 0},
        {"Mobility", 0}, 
        {"PiecePosition", 0},
        {"CenterControl", 0},
        {"kingSafety", 0}
        //{"HangingPieces", 0}
    };

const std::unordered_map<std::string, int> Evaluator::evalWeights = { // eval is in terms of centipawn
        {"Material", 1}, // material - raw        of which material is base units 
        {"PawnStructure", -25}, // pawn structure (bad stuff)
        {"PassedPawns", 1}, // 30-40s-ish with scaling
        {"Mobility", 10}, // num legal moves
        {"PiecePosition", 1}, // already in 10s
        {"CenterControl", 1}, // attack counts so lots of 1,2,3s
        {"KingSafety", 1} // already in 10s
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
// replace SEE in evaluate
int Evaluator::taperedEval(const Board* board, int SEE) {
    int phase = gamePhase(board);
    // SEE >0 is good, SEE < 0 is bad
    // in these func calls, a bad capture was just made (so piece is hanging, so hanging_penalty is known)
    //int mop_up = board->pieceBitboards[0] == 0 ? mopUp(*board) : 0;
    int early_queen = gamePhase(board) <= 64 ? earlyQueenPenalty(*board) : 0;

    int eval_adj = /*mop_up +*/ early_queen /*+ SEE*/;

    //int eval = computeEval(*board,pst) + eval_adj;

    return computeEval(*board, PST_opening) + computeEval(*board, PST_endgame) + eval_adj;
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
    // should king safety
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
    //int mop_up = board->pieceBitboards[0] == 0 ? mopUp(*board) : 0;
    int early_queen = gamePhase(board) <= 128 ? earlyQueenPenalty(*board) : 0;
    //int hanging_pieces = hangingPiecePenalty(*board);
    //int castle_bias = castleBias(*board);

    int eval_adj = early_queen; // quiesence for hanging pieces

    //int eval = computeEval(*board,pst) + eval_adj;

    return computeEval(*board, pst) + eval_adj;

}

int Evaluator::computeEval(const Board& board, int pst[6][64]) {
    int eval = 0;

    for (const auto& pair : evalWeights) {
        const std::string& name = pair.first;
        int weight = pair.second;
        int value = evaluateComponent(board, name, pst);
        eval += int(weight * value);
    }

    return eval;
}

int Evaluator::evaluateComponent(const Board& board, std::string component, int pst[6][64]) {
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
    } else if (component == "KingSafety") {
        componentEvals[component] = kingSafetyDifferences(board);
        return componentEvals[component];
    //} else if (component == "CenterControl") {
    //    componentEvals[component] = centerControlDifferences(board);
    //    return componentEvals[component];
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
        int value = evaluateComponent(board, name, pst);
        component_hash[name] = value;
    }

    return component_hash;
}

//int Evaluator::negamax_eval(const Board& board)

int Evaluator::materialDifferences(const Board& board) { // add up material
    U64 bb;
    int pEval = 0.0f;

    for (int side = 0; side < 2; side++) {
        // +white -black
        for (int piece = 0; piece < 5; piece++) {
            // exlude king
            bb = board.colorBitboards[side] & board.pieceBitboards[piece];
            pEval += ((side==0) ? 1 : -1) * pieceValues[piece] * countBits(bb);
            if (piece == bishop && countBits(bb) == 2) {pEval += (side==0) ? 40 : -40;} // 6.6 -> 7 for bishop pair
        }
    }

    return pEval;
}
int Evaluator::pawnStructureDifferences(const Board& position) { // double, block, iso
    int psEval = 0;
    U64 pawns = position.pieceBitboards[0];
    U64 white = position.colorBitboards[0]; U64 black = position.colorBitboards[1];

    psEval += (countDoubledPawns(pawns&white) - countDoubledPawns(pawns&black)); // func counts doubles files
    psEval += (countIsolatedPawns(pawns&white) - countIsolatedPawns(pawns&black));
    
    return psEval;
}
int Evaluator::mobilityDifferences(const MoveGenerator* movegen) { // # moves (psuedo possibilities)
    // return values computed during movegen 
    //int w_mob = movegen->white_mobility; int b_mob = movegen->black_mobility;
    //return w_mob - b_mob;
    return 0;
}
int Evaluator::positionDifferences(const Board& position, int pst[6][64]) {
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

    return score;
}
int Evaluator::centerControlDifferences(const Board& position) {
    int score = 0;
    const U64 centerMask_pawns = Bits::centerMasks[0];
    const U64 centerMask_pieces = Bits::centerMasks[1];
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
                    score += sign * 5 * countBits(attacks & Bits::centerMasks[0]); // smaller centers
                    score += sign * 5 * countBits((1ULL << sq) & Bits::centerMasks[0]); // occ of center square
                } else {
                    score += sign * 1 * countBits(attacks & Bits::centerMasks[1]);
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
// called after capture is made and init gain[] to captured piece
// Static Exchange Evaluation (side-independent)
// Returns net material gain for the side that STARTS with 'move' (capture).
// If non-capture, return 0 (neutral).
int Evaluator::SEE(const Board& board, const Move& move) {
    // Fast exit: only do SEE for captures
    int capturedPT = board.getCapturedPiece(move.TargetSquare()); // piece type 0..5 or -1
    if (capturedPT == -1) return 0;

    // --- helpers ---
    auto pieceAt12 = [&](int sq) -> int {
        // Return 0..11 (type+color) or -1 if empty
        for (int pt = 0; pt < 12; ++pt) {
            if (board.pieceBitboards[pt] & (1ULL << sq)) return pt;
        }
        return -1;
    };

    auto pieceType = [&](int pt12) -> int { return pt12 % 6; };      // 0..5
    auto pieceColor = [&](int pt12) -> int { return pt12 / 6; };     // 0 white, 1 black

    // --- occupancy after the initial capture is hypothetically made ---
    U64 occ = board.colorBitboards[0] | board.colorBitboards[1];
    // remove captured piece from the target
    occ &= ~(1ULL << move.TargetSquare());
    // move the attacker from start->target
    occ &= ~(1ULL << move.StartSquare());
    occ |=  (1ULL << move.TargetSquare());

    // find which side started the capture
    int moverPT12 = pieceAt12(move.StartSquare());
    // If you store moved-piece type elsewhere you can use that too
    int moverSide = pieceColor(moverPT12); // 0=white,1=black

    // current square being fought over
    int sq = move.TargetSquare();

    // attackers to sq with *updated* occ (after initial capture)
    U64 attackers[2];
    attackers[0] = attackersTo(board, sq, /*white*/true,  occ);
    attackers[1] = attackersTo(board, sq, /*white*/false, occ);

    // Mark the piece that just moved as used (it now sits on sq)
    // We need a used-mask to avoid picking the same attacker twice.
    U64 used = 0ULL;
    used |= (1ULL << move.TargetSquare()); // the attacker now occupies sq

    // gain[d] = material net after d exchanges (classical SEE)
    int gain[32];
    int d = 0;

    // First gain is the value of the captured piece
    gain[d++] = pieceValues[capturedPT];

    // Side to move to RECAPTURE is the opponent of the mover
    int stm = 1 - moverSide;

    // Helper to pick least valuable attacker for a side
    auto pick_least_valuable_attacker = [&](int side, int& fromSq, int& attackerType) -> bool {
        U64 cand = attackers[side] & ~used;
        if (!cand) return false;

        int bestSq = -1;
        int bestVal = 1e9;
        int bestType = -1;

        // Iterate through candidates (LSBs)
        while (cand) {
            int sqFrom = __builtin_ffsll(cand) - 1; // LSB index
            cand &= cand - 1;

            int pt12 = pieceAt12(sqFrom);
            if (pt12 == -1 || pieceColor(pt12) != side) continue;
            int t = pieceType(pt12);
            int val = pieceValues[t];
            if (val < bestVal) {
                bestVal = val;
                bestType = t;
                bestSq = sqFrom;
            }
        }
        if (bestSq == -1) return false;
        fromSq = bestSq; attackerType = bestType; 
        return true;
    };

    // Now play the sequence of recaptures greedily by least valuable attacker
    while (true) {
        int fromSq = -1, attType = -1;
        if (!pick_least_valuable_attacker(stm, fromSq, attType)) break;

        // mark this attacker as used and update occupancy
        used |= (1ULL << fromSq);
        occ &= ~(1ULL << fromSq);
        // attacker from 'fromSq' moves to 'sq' (already occupied by last capturer)
        // we don't toggle a bit for sq in 'used' because the square is already counted
        // but occ is unchanged for sq because it remains occupied after recapture.

        // If king is the attacker, we stop the sequence (no further exchanges allowed safely)
        if (attType == king) {
            // King takes; sequence stops here by convention
            gain[d] = pieceValues[king] - gain[d-1];
            ++d;
            break;
        }

        // Normal: add next layer of gain
        gain[d] = pieceValues[attType] - gain[d-1];
        ++d;

        // recompute dynamic attackers with new occ
        attackers[0] = attackersTo(board, sq, true,  occ);
        attackers[1] = attackersTo(board, sq, false, occ);

        stm ^= 1; // switch side
    }

    // Backtrack optimal stopping
    while (--d > 0) {
        gain[d-1] = std::max(-gain[d], gain[d-1]);
    }

    // gain[0] is net result for the side that started the capture
    return gain[0];
}



// would like to see if pieces are hanging before capture is made on board
int Evaluator::hangingPiecePenalty(const Board& board) {
    int penalty = 0; int sign = board.is_white_move ? 1 : -1; 
    // eval at leaf node is called after move is made
    // white to move = black just move = penalty to black = add to eval
    // side to move is after move was made, so penalty is applied to previous player
    // so if player to move has positive SEE then piece is hanging

    for (int piece = knight; piece <= queen; piece++) { // include pawns is heavy
        U64 bb = board.colorBitboards[1-board.move_color] & board.pieceBitboards[piece];
        while (bb) {
            int sq = __builtin_ctzll(bb);
            bb &= bb -1;

            //int see = SEE(board);
            int see = 0;
            if (see > 0) {
                int value = pieceValues[piece];
                penalty += sign*see;
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

int Evaluator::kingSafety(const Board& board, bool usWhite) {
    int opp_mat = attackerMaterial(board, !usWhite);
    int material_factor = (opp_mat * 1024) / (opp_mat + 800); // Scale factor by 1024 to preserve fractional part
    int phase_scale = 1024 - (gamePhase(&board) * 1024 / 24);
    //phase_scale *= phase_scale; // quadratic taper

    int shield = -kingShield(board, usWhite); // penalty
    int open = -openFilesNearKing(board, usWhite); // penalty
    int trop = tropism(board, !usWhite); // (score) enemy pieces near king = decrease eval

    int total = (shield + open); 
    long long temp = (long long)total * material_factor - (long long)isqrt(trop)*10;
    return static_cast<int>(temp * phase_scale / 1024 / 1024);
}
int Evaluator::kingSafetyDifferences(const Board& board) {
    return castleBias(board) + (kingSafety(board, false) - kingSafety(board, true));
}
int Evaluator::castleBias(const Board& board) {
    int bonus = 0;

    //if (board.plyCount < 4) {return bonus;}

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

    if (board.white_castled) bonus += 40;
    if (board.black_castled) bonus -= 40;

    return bonus;
}
int Evaluator::kingShield(const Board& board, bool usWhite) {
    int penalty = 0;
    // missing from first/second rank infront of king
    // singly pushed
    int missing_pawn_penalty = 30; int pushed_pawn_penalty = 10;
    
    int king_sq = board.kingSquare(usWhite); 
    int file = king_sq % 8;
    U64 own_pawns = board.pieceBitboards[pawn] & board.colorBitboards[usWhite ? 0 : 1];

    // left, center, right are from whites perspectice
    int left = (file > 0) ? king_sq + (usWhite ? 7 : -9) : 0; // pawns will never be on a1 (0)
    int center = king_sq + (usWhite ? 8 : -8);
    int right = (file < 7) ? king_sq + (usWhite ? 9 : -7) : 0;
    // 2nd instance allow single pawn pushes 
    // we only want to penalize double advanced or further pawns
    int left2 = (left != 0) ? left + (usWhite ? 8 : -8) : 0;
    int center2 = center + (usWhite ? 8 : -8);
    int right2 = (right != 0) ? right + (usWhite ? 8 : -8) : 0;

    if (!(own_pawns & (1ULL << left))) {
        if (!(own_pawns & (1ULL << left2))) {
            penalty += missing_pawn_penalty;
        } else {penalty += pushed_pawn_penalty;}
    }
    if (!(own_pawns & (1ULL << center))) {
        if (!(own_pawns & (1ULL << center2))) {
            penalty += missing_pawn_penalty;
        } else {penalty += pushed_pawn_penalty;}
    }
    if (!(own_pawns & (1ULL << right))) {
        if (!(own_pawns & (1ULL << right2))) {
            penalty += missing_pawn_penalty;
        } else {penalty += pushed_pawn_penalty;}
    }

    return penalty;
}
int Evaluator::openFilesNearKing(const Board& board, bool usWhite) {
    int penalty = 0; int king_sq = board.kingSquare(usWhite); int file = king_sq % 8;

    // open files
    if (isOpenFile(board, file-1)) penalty += 20;
    if (isOpenFile(board, file)) penalty += 25;
    if (isOpenFile(board, file+1)) penalty += 20;
    // semi open files
    if (isSemiOpenFile(board, file-1, usWhite)) penalty += 10;
    if (isSemiOpenFile(board, file, usWhite)) penalty += 15;
    if (isSemiOpenFile(board, file+1, usWhite)) penalty += 10;

    return penalty;
}
int Evaluator::tropism(const Board& board, bool usWhite) {
    int score = 0; int king_sq = board.kingSquare(usWhite); 
    U64 opp_pieces = board.colorBitboards[usWhite ? 1 : 0] & ~board.pieceBitboards[0] & ~board.pieceBitboards[king]; // exclude pawns and king
    int dist; int val; int sq;

    while (opp_pieces) {
        sq = tzcnt(opp_pieces) - 1; // get LSB
        opp_pieces &= opp_pieces - 1; // clear LSB

        dist = precomp->king_move_distances[sq][king_sq];
        switch (board.getMovedPiece(sq)) {
            case 1: // knight
                val = (dist <= 3) ? 8 : 0;
                break;
            case 2: // bishop
                val = 8; 
                break;
            case 3: // rook
                val = (dist <= 3) ? 15 : 5;
                break;
            case 4: // queen
                val = (dist <= 3) ? 20 : 10;
                break;
            default:
                val = 0;
                break;
        }

        score += val * (7-dist);
    }

    return score;
}

bool Evaluator::isOpenFile(const Board& board, int file) {
    if (file < 0 || file > 7) return false;
    return !(board.pieceBitboards[0] & Bits::file_masks[file]);
}
bool Evaluator::isSemiOpenFile(const Board& board, int file, bool usWhite) {
    if (file < 0 || file > 7) return false;
    U64 relevant_pawns = board.pieceBitboards[0] & (usWhite ? board.colorBitboards[0] : board.colorBitboards[1]);
    return !(Bits::file_masks[file] & relevant_pawns);
}

int Evaluator::attackerMaterial(const Board& board, int opp_side) {
    int mat = 0; U64 opp_bb = board.colorBitboards[opp_side];

    mat += 300 * countBits(opp_bb & board.pieceBitboards[1]);
    mat += 300 * countBits(opp_bb & board.pieceBitboards[2]);
    mat += 500 * countBits(opp_bb & board.pieceBitboards[3]);
    mat += 900 * countBits(opp_bb & board.pieceBitboards[4]);

    return mat;
}