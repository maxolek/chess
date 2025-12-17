#ifndef EVALUATOR_H
#define EVALUATOR_H

#include "NNUE.h"
#include "board.h"
#include "helpers.h"
#include "PrecomputedMoveData.h" // masks
#include "magics.h"
#include "stats.h"
#include <array>
#include <ostream>

// =================== Eval Components ===================
enum EvalComponent {
    MATERIAL,
    PAWN_STRUCTURE,
    PASSED_PAWNS,
    POSITION,
    CENTER_CONTROL,
    KING_SAFETY,
    MOBILITY,
    COMPONENT_COUNT
};

inline const char* EvalComponentNames[] = {
    "Material",
    "PawnStructure",
    "PassedPawns",
    "Position",
    "CenterControl",
    "KingSafety",
    "Mobility"
};


// =================== Constants ===================
constexpr int pieceValues[6] = {100, 300, 330, 500, 900, 10000}; 
constexpr int passedBonus[8] = {0, 10, 20, 30, 50, 70, 100, 0}; // rank-based bonus
constexpr std::array<int, COMPONENT_COUNT> evalWeights = { 
    1,   // MATERIAL
    -20,  // PAWN_STRUCTURE
    1,  // PASSED_PAWNS
    1,  // POSITION
    3,   // CENTER_CONTROL
    1,  // KING_SAFETY
    0   // MOBILITY
};


// =================== Full Eval Report ===================
struct TaperedEvalReport {
    std::array<int, COMPONENT_COUNT> opening{};   // per-component opening eval
    std::array<int, COMPONENT_COUNT> endgame{};   // per-component endgame eval
    std::array<int, COMPONENT_COUNT> tapered{};   // per-component tapered eval
    int openingTotal = 0;    // weighted sum for opening
    int endgameTotal = 0;    // weighted sum for endgame
    int taperedTotal = 0;    // weighted sum for tapered eval

    // Compute tapered per-component values and weighted totals
    void computeTapered(int phase, const std::array<int, COMPONENT_COUNT>& weights) {
        taperedTotal = 0;
        openingTotal = 0;
        endgameTotal = 0;

        for (size_t i = 0; i < COMPONENT_COUNT; ++i) {
            // Compute tapered value for each component
            tapered[i] = ((opening[i] * (256 - phase)) + (endgame[i] * phase)) / 256;

            // Compute weighted totals
            openingTotal += opening[i] * weights[i];
            endgameTotal += endgame[i] * weights[i];
            taperedTotal += tapered[i] * weights[i];
        }
    }

    // Print detailed per-component values with weights
    void printDetailed(const std::array<int, COMPONENT_COUNT>& weights) const {
        std::cout << "Component breakdown:\n";
        for (size_t i = 0; i < COMPONENT_COUNT; ++i) {
            int w = weights[i];
            std::cout << EvalComponentNames[i]
                      << ": opening=" << opening[i]
                      << ", endgame=" << endgame[i]
                      << ", tapered=" << tapered[i]
                      << ", weight=" << w << "\n";
        }
        std::cout << "Opening weighted total: " << openingTotal 
                  << ", Endgame weighted total: " << endgameTotal
                  << ", Tapered weighted total: " << taperedTotal << "\n";
    }

    // Optionally, simple print with only tapered totals
    void printSummary() const {
        std::cout << "Tapered weighted total: " << taperedTotal << "\n";
    }
};


// =================== Evaluator ===================
class Evaluator {
private:
public:
    Evaluator();

    // nnue
    NNUE nnue;

    // MVV-LVA
    int mvvLvaTable[6][6];
    void initMVVLVA();

    // Piece-Square Tables
    bool loadOpeningPST(const std::string& filename);
    bool loadEndgamePST(const std::string& filename);
    int PST_opening[6][64];
    int PST_endgame[6][64];

    // Main evaluation
    int gamePhase(const Board& board);
    int taperedEval(const Board& board); 
    TaperedEvalReport Evaluate(const Board& board, int pst[6][64]);
    TaperedEvalReport openingEval(const Board& board);
    TaperedEvalReport endgameEval(const Board& board);

    // Components
    int materialDifferences(const Board& board);
    int pawnStructureDifferences(const Board& board);
    int passedPawnDifferences(const Board& board);
    int positionDifferences(const Board& board, int pst[6][64]);
    int centerControlDifferences(const Board& board);
    int kingSafetyDifferences(const Board& board);

    // Specialized heuristics

    // Exchange / hanging piece evaluation
    int SEE(const Board& board, const Move& move);
    U64 attackersTo(const Board& board, int sq, bool white, U64 occ);

    // Pawn helpers
    int countDoubledPawns(U64 pawns);
    int countIsolatedPawns(U64 pawns);
    bool isPassedPawn(bool white, int sq, U64 opp_pawns);

    // King safety helpers
    int kingSafety(const Board& board, bool usWhite);
    int castleBias(const Board& board);
    int kingShield(const Board& board, bool usWhite);
    int openFilesNearKing(const Board& board, bool usWhite);
    int tropism(const Board& board, bool usWhite);

    // File helpers
    bool isOpenFile(const Board& board, int file);
    bool isSemiOpenFile(const Board& board, int file, bool usWhite);

    // Misc
    int attackerMaterial(const Board& board, int opp_side);
};

#endif
