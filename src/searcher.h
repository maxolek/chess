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
    static constexpr int KILL_SEARCH_RETURN = -5 * MATE_SCORE;
    static constexpr int MAX_DELTA = 1000;

    // Object-owned state
    Engine& engine;
    Board& board; //= engine.search_board;
    Evaluator& eval; // = engine.evaluator;
    NNUE& nnue; // = engine.evaluator.nnue;

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
        std::vector<Move>& previousPV
    );

    SearchResult searchAspiration(
        Move potential_moves[MAX_MOVES],
        int move_count,
        int depth,
        SearchLimits& limits,
        std::vector<Move>& previousPV,
        int alpha, int beta
    );

    // --------------------------- Negamax & Quiescence --------------------------
    int negamax(
        int depth,
        int alpha,
        int beta,
        PV& pv,
        std::vector<Move>& previousPV,
        SearchLimits& limits,
        int ply,
        bool inQSearch
    );

    int quiescence(
        int alpha,
        int beta,
        PV& pv,
        SearchLimits& limits,
        int ply,
        int depth
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
        int alpha
    );

    // ------------------------------- Make/unmake w/ NNUE -------------------------------
    void do_move(const Move& move);
    void undo_move(const Move& move, const Board& before);
};

#endif // SEARCHER_H
