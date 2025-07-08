#ifndef EVALUATOR_H
#define EVALUATOR_H

#include "board.h"
#include "helpers.h"
#include "arbiter.h" // term evals 
#include "moveGenerator.h" // mobility
#include "PrecomputedMoveData.h" // masks
#include <fstream>  // for file output
#include <iomanip>  // for formatting

class Evaluator {
private:
    static bool open_pstLoaded; static bool end_pstLoaded;
    static const PrecomputedMoveData* precomp;
public:

    Evaluator ();
    Evaluator (const PrecomputedMoveData* _precomp);

    static void writeEvalDebug(const MoveGenerator* movegen, Board& board, const std::string& filename);


    // pieces
    static constexpr float pieceValues[5] = {1, 3.2, 3.5, 5, 9}; 
    static constexpr int passedBonus[8] = { 0, 10, 20, 30, 50, 70, 100, 0 }; // passed pawn rank (white)
    // pawn knight bishop rook queen
    // piece positions

    // Load PST from file: 6 pieces, 64 squares each
    static bool loadPST(const std::string& filename, int pst[6][64]);
    static int PST_opening[6][64]; static int PST_endgame[6][64];

    // pawn structure
    static constexpr int doubled_pawns = 1; 
    static constexpr int blocked_pawns = 1; 
    static constexpr int iso_pawns = 1;

    // logistics
    static constexpr int mobility = 1;


    static const std::unordered_map<std::string, int> evalWeights; 
    static std::unordered_map<std::string, int> componentEvals;

    // run all
    static int Evaluate(
        //const MoveGenerator* movegen, 
        const Board* position,
        int pst[6][64]
    ); 
    // eval is scaled to centipawn (100=1) and converted to int
    static int computeEval(
        //const MoveGenerator* movegen, 
        const Board& board,
        int pst[6][64]
    ); // add up below w/ weights
    static std::unordered_map<std::string, int> computeComponents(
        //const MoveGenerator* movegen, 
        const Board& board,
        int pst[6][64]
    ); // return dict constructed in above but on command
    static float evaluateComponent(
        //const MoveGenerator* movegen, 
        const Board& board, 
        std::string component,
        int pst[6][64]); // some components (pst) depend on phase of game 
        // iterate through names and run below funcs

    // tapered eval
    static int gamePhase(const Board* position); // 0->256 desc based on captured pieces
    static int taperedEval(const Board* board); // get eval of board
    static int taperedEval(const Board* board, Result result); // get eval during search 
    static int openingEval(const Board* board);
    static int endgameEval(const Board* board);
    static int negamax_eval(const Board* board); // negamax still performs comparable to magic_bitboard version (so eval must be difference)
    
    // counts (weights are in above)
    static float materialDifferences(const Board& position); // add up material
    static float pawnStructureDifferences(const Board& position); // double, block, iso (more is bad)
    // currently mobilityDiff lags the position to eval by 1 ply as it relies on movegen which ran on the prior position
    static float mobilityDifferences(const MoveGenerator* movegen); // # moves (psuedo possibilities)
    static float positionDifferences(const Board& position, int pst[6][64]); // pst with phase
    static int centerControlDifferences(const Board& position); // precomp masks
    // end game
    static int mopUp(const Board& position); // late endgames without pawns -- drive opp king to edge
    // opening
    static int earlyQueenPenalty(const Board& board); // penalty based on other piece development
    static int castleBias(const Board& board); // increase eval for castle moves

    // static exchange evaluation - run through exchanges on a square and see results
    static int SEE(const Board& board, int sq, bool white);
    static int hangingPiecePenalty(const Board& board); // loop through pieces to see whats hanging via SEE
    static U64 attackersTo(const Board& board, int sq, bool white, U64 occ); // get all attackers to a square
    // pawns
    static int countDoubledPawns(U64 pawns); // via file masks
    static int countIsolatedPawns(U64 pawns); // via file masks
    static bool isPassedPawn(bool white, int sq, U64 opp_pawns); // via precomp masks
    static int passedPawnDifferences(const Board& board); // add bonuses (+based on file) for passed pawns to eval
};

#endif