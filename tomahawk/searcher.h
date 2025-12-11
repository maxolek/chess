#ifndef SEARCHER_H
#define SEARCHER_H

#include "evaluator.h"
#include "helpers.h"
#include "tt.h"
#include "stats.h"
#include "NNUE.h"

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
    EvalReport eval_report;
    PV best_line;
    
    RootMove root_moves[MAX_MOVES];
    int root_count = 0;

    inline void setPV(Move first, const PV& child) {
        best_line.set(first, child);
    }
};

struct SearchLimits {
    std::chrono::steady_clock::time_point start_time;
    int time_limit_ms;
    int max_depth;
    bool stopped;

    SearchLimits(int ms = 0, int depth = -1)
        : start_time(std::chrono::steady_clock::now()),
          time_limit_ms(ms), max_depth(depth), stopped(false) {}

    inline bool out_of_time() {
        if (stopped) return true;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
        if (elapsed >= time_limit_ms && time_limit_ms > 0) {
            stopped = true;
            return true;
        }
        return false;
    }

    inline bool depth_reached(int current_depth) const {
        return (max_depth >= 0 && current_depth > max_depth);
    }

    inline bool should_stop(int current_depth) {
        return out_of_time() || depth_reached(current_depth);
    }
};

class Searcher {
public:
    static constexpr int KILL_SEARCH_RETURN = -5 * MATE_SCORE;
    static constexpr int MAX_DELTA = 1000;

    // Object-owned state
    Board* board;
    MoveGenerator* movegen;
    NNUE* nnue;
    TranspositionTable tt;
    SearchStats stats;

    bool stop = false;
    bool trackStats = true;
    int nodesSearched = 0;
    int quiesence_depth = 0;
    int max_q_depth = 0;

    Move killerMoves[MAX_DEPTH][2] = {};
    int historyHeuristic[12][64] = {};

    std::vector<Move> best_line;
    std::vector<Move> best_quiescence_line;

    Searcher() {};
    Searcher(Board* b, MoveGenerator* mg, NNUE* nnue)
    : board(b), movegen(mg), nnue(nnue) {}

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
        int ply
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

    void enterDepth();
    void exitDepth();
    void resetStats();

    // ------------------------------- Make/unmake w/ NNUE -------------------------------
    void do_move(const Move& move);
    void undo_move(const Move& move, const Board& before);
};

#endif // SEARCHER_H
