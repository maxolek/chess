#ifndef EVALUATOR_H
#define EVALUATOR_H

#include "board.h"
#include "helpers.h"
#include "moveGenerator.h"    // mobility
#include "PrecomputedMoveData.h" // masks
#include <array>
#include <ostream>

//////////////////////////////////////
//////////////////////////////////////
//
//      out dated (static)
//
//////////////////////////////////////
//////////////////////////////////////

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
static constexpr int pieceValues[6] = {100, 300, 330, 500, 900, 10000}; 
static constexpr int passedBonus[8] = {0, 10, 20, 30, 50, 70, 100, 0}; // rank-based bonus
static std::array<int, COMPONENT_COUNT> evalWeights = { 
    1,   // MATERIAL
    -20,  // PAWN_STRUCTURE
    1,  // PASSED_PAWNS
    1,  // POSITION
    3,   // CENTER_CONTROL
    1,  // KING_SAFETY
    0   // MOBILITY
};


// =================== Eval Report ===================
struct EvalReport {
    std::array<int, COMPONENT_COUNT> components{}; // raw component values
    int weightedTotal = 0; // weighted sum

    void add(EvalComponent c, int val) { components[c] = val; }

    int total() const { // raw sum
        int sum = 0;
        for (int val : components) sum += val;
        return sum;
    }

    void computeWeighted(const std::array<int, COMPONENT_COUNT>& weights) {
        weightedTotal = 0;
        for (size_t i = 0; i < COMPONENT_COUNT; ++i)
            weightedTotal += components[i] * weights[i];
    }

    void print(std::ostream& os, const std::array<int, COMPONENT_COUNT>* weights = nullptr) const {
        for (size_t i = 0; i < COMPONENT_COUNT; ++i)
            os << EvalComponentNames[i] << ": " << components[i] << "\n";

        if (weights)
            os << "Weighted Total: " << weightedTotal << "\n";
        os << "Raw Total: " << total() << "\n";
    }

    void print(const std::array<int, COMPONENT_COUNT>* weights = nullptr) const {
        for (size_t i = 0; i < COMPONENT_COUNT; ++i)
            std::cout << EvalComponentNames[i] << ": " << components[i] << "\n";

        if (weights)
            std::cout << "Weighted Total: " << weightedTotal << "\n";
        std::cout << "Raw Total: " << total() << "\n";
    }
};


// =================== Evaluator ===================
class Evaluator {
private:
    static const PrecomputedMoveData* precomp;

public:
    Evaluator();
    Evaluator(const PrecomputedMoveData* _precomp);

    // Debug output
    static void writeEvalDebug(Board& board, const std::string& filename);

    // MVV-LVA
    static int mvvLvaTable[6][6];
    static void initMVVLVA();

    // Piece-Square Tables
    static bool loadPST(const std::string& filename, int pst[6][64]);
    static int PST_opening[6][64];
    static int PST_endgame[6][64];

    // Main evaluation
    static int gamePhase(const Board& board);
    static int taperedEval(const Board& board); 
    static EvalReport Evaluate(const Board& board, int pst[6][64]);
    static EvalReport openingEval(const Board& board);
    static EvalReport endgameEval(const Board& board);

    // Components
    static int materialDifferences(const Board& board);
    static int pawnStructureDifferences(const Board& board);
    static int passedPawnDifferences(const Board& board);
    static int positionDifferences(const Board& board, int pst[6][64]);
    static int centerControlDifferences(const Board& board);
    static int kingSafetyDifferences(const Board& board);

    // Specialized heuristics

    // Exchange / hanging piece evaluation
    static int SEE(const Board& board, const Move& move);
    static U64 attackersTo(const Board& board, int sq, bool white, U64 occ);

    // Pawn helpers
    static int countDoubledPawns(U64 pawns);
    static int countIsolatedPawns(U64 pawns);
    static bool isPassedPawn(bool white, int sq, U64 opp_pawns);

    // King safety helpers
    static int kingSafety(const Board& board, bool usWhite);
    static int castleBias(const Board& board);
    static int kingShield(const Board& board, bool usWhite);
    static int openFilesNearKing(const Board& board, bool usWhite);
    static int tropism(const Board& board, bool usWhite);

    // File helpers
    static bool isOpenFile(const Board& board, int file);
    static bool isSemiOpenFile(const Board& board, int file, bool usWhite);

    // Misc
    static int attackerMaterial(const Board& board, int opp_side);
};

#endif
