#ifndef SEARCHER_H
#define SEARCHER_H

#include "engine.h"
#include "search_limits.h"
#include "helpers.h"
#include "stats.h"
#include "timer.h"
#include "NNUE.h"


class Engine;
class Evaluator;
class NNUE;

struct RootMove {
    Move move;
    int eval;
};

// ---- search constants ----
struct SearchParams {
    // delta / SEE
    int   DELTA_PRUNE_THRESHOLD  = 1'000;
    int   SEE_PRUNE_THRESHOLD    = -50;
    // aspiration windows
    int   ASPIRATION_WINDOW      = 50;
    int   ASPIRATION_START_DEPTH = 6;
    int   ASPIRATION_DEPTH_SCALE = 10;
    float ASPIRATION_RESEARCH_SCALE = 2.0f;
    // positional
    int   DRAW_EVAL              = 0;
    int   CONTEMPT               = 0;
    // reductions
    int   R_NMP                  = 3;      // null-move pruning
    float R_LMR_CONST            = 0.99f;  // late move reductions 
    float R_LMR_DENOM            = 3.14f;  //   = const + [log(depth) * log(move_order)] / denom
    int   LMR_MOVE_ORDER_THRESHOLD = 3; // minimum move order # to start using LMR
    int   LMR_DEPTH_THRESHOLD    = 2; // max search depth where LMR doesnt trigger
};

// ---- move ordering priorities ----
struct MoveScores {
    int TT_BASE       =  10'000'000;
    int PV_BASE       =   9'000'000;
    int PROMO_BASE    =   8'500'000;
    int GOOD_CAP_BASE =   8'000'000;
    int KILLER_BASE   =   7'000'000;
    int QUIET_BASE    =           0;
    int BAD_CAP_BASE  =  -1'000'000;
};

// ---- principal variation tracking ----
struct PV {
    std::vector<Move> line;
    void clear() { line.clear(); }
    inline void set(Move first, const PV& child) {
        line.clear();
        line.reserve(1 + child.line.size());
        line.push_back(first);
        line.insert(line.end(), child.line.begin(), child.line.end());
    }
};

// ---- search returns move / eval / PV ----
struct SearchResult {
    Move bestMove = Move::NullMove();
    int eval = -MATE_SCORE;
    //TaperedEvalReport eval_report;
    PV best_line;
    
    RootMove root_moves[MAX_MOVES];
    int root_count = 0;

    inline void setPV(Move first, const PV& child) {
        best_line.set(first, child);
    }
};

class Searcher {
public:

    // Object-owned state
    Engine& engine;
    Board& board; //= engine.search_board;
    Evaluator& eval; // = engine.evaluator;
    NNUE& nnue; // = engine.nnue;

    SearchParams params;
    MoveScores move_scores;

    bool stop = false;

    Move killerMoves[MAX_DEPTH][2] = {};
    int historyHeuristic[12][64] = {};

    std::vector<Move> best_line;
    std::vector<Move> best_quiescence_line;

    Searcher(Engine& e, Board& b, Evaluator& ev, NNUE& nn) 
        : engine(e), 
          board(b), 
          eval(ev), 
          nnue(nn) {}

    // ------------------------------- Main Search -------------------------------
    SearchResult search(
        Move potential_moves[MAX_MOVES],
        int move_count,
        int depth,
        SearchLimits& limits,
        std::vector<Move>& previousPV,
        int previousEval
    );

    // --------------------------- Negamax & Quiescence --------------------------
    int negamax(
        int depth,
        int alpha,
        int beta,
        PV& pv,
        std::vector<Move>& previousPV,
        SearchLimits& limits,
        int ply
    );

    int quiescence(
        int alpha,
        int beta,
        PV& pv,
        SearchLimits& limits,
        int ply,
        int depth,
        int search_depth
    );

    // ------------------------------- Ordering & Scoring -------------------------------
    int moveScore(
        const Move& move,
        const Board& board,
        int ply,
        const Move& ttMove,
        const std::vector<Move>& previousPV
    );

    void orderedMoves(
        Move moves[MAX_MOVES],
        size_t count,
        const Board& board,
        int ply,
        const std::vector<Move>& previousPV
    );

    int generateAndOrderMoves(
        Move moves[MAX_MOVES],
        int ply,
        const std::vector<Move>& previousPV
    );

    // ------------------------------- PV / pruning / helpers -------------------------------
    void updatePV(std::vector<Move>& pv, const Move& move, const std::vector<Move>& childPV);

    bool shouldPrune(
        Move& move,
        int standPat,
        int alpha,
        int search_depth,
        int ply
    );

    // -------------------------------- Search Reduction Parameters ----------------------------
    int R_lmr(
        int depth, 
        int move_order
    );

};

#endif // SEARCHER_H
