#ifndef SEARCHER_H
#define SEARCHER_H

#include "evaluator.h"
#include "helpers.h"
#include "tt.h"


/**
 * Struct for tracking principal variation
 */
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


/**
 * Result of a search iteration
 */
struct SearchResult {
    Move bestMove = Move::NullMove();   // Best move found
    int eval = -MATE_SCORE;               // Evaluation score
    EvalReport eval_report;             // Breakdown of evaluation components
    PV best_line;                        // Principal variation (with normal_len)
    // full root moves
    Move root_moves[MAX_MOVES];
    int root_evals[MAX_MOVES];
    int root_count = 0;

    // Convenience to copy PV from a child
    inline void setPV(Move first, const PV& child) {
        best_line.set(first, child);
    }
};


/**
 * Time & Depth Management
 */
struct SearchLimits {
    std::chrono::steady_clock::time_point start_time;
    int time_limit_ms;   // total allowed time for the search
    int max_depth;       // maximum depth allowed (-1 = no limit)
    bool stopped;        // set true when we must stop

    SearchLimits(int ms = 0, int depth = -1)
        : start_time(std::chrono::steady_clock::now()),
          time_limit_ms(ms),
          max_depth(depth),
          stopped(false) {}

    inline bool out_of_time() {
        if (stopped) return true;
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
        
        if (elapsed >= time_limit_ms && time_limit_ms > 0 ) {
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


/**
 * Searcher class: handles all search-related functionality including
 * minimax/negamax, quiescence search, move ordering, PV tracking, TT access.
 */
class Searcher {
public:
    // Constants
    static constexpr int KILL_SEARCH_RETURN = -5 * MATE_SCORE;
    static constexpr int MAX_DELTA = 1000; // Delta pruning threshold in quiescence

    // Control flags
    inline static bool stop = false; // Set externally to abort search

    // Search stats
    static int nodesSearched;
    static TranspositionTable tt;

    // Quiescence tracking
    static int quiesence_depth; // Current depth inside quiescence
    static int max_q_depth;     // Maximum depth reached by quiescence


    // Move scoring heuristics
    static Move killerMoves[MAX_DEPTH][2]; // Up to 2 killer moves per depth
    static int historyHeuristic[12][64];  // History heuristic table: [piece][toSquare]

    // Principal variation storage
    static std::vector<Move> best_line;             // PV line tracker
    static std::vector<Move> best_quiescence_line;  // PV built in quiescence

    // -------------------------------
    // Main search entry
    // -------------------------------
    static SearchResult search(
        Board& board,
        MoveGenerator& movegen,
        Evaluator& evaluator,
        Move potential_moves[MAX_MOVES], // sorted from prev_it_evals in engine.cpp
        int move_count, // Only first `move_count` moves are valid
        int depth,
        SearchLimits& limits,
        std::vector<Move> previousPV // from last it_deep depth, used for move ordering
    );

    // aspiration window search with given alpha-beta values
    static SearchResult searchAspiration(
        Board& board,
        MoveGenerator& movegen,
        Evaluator& evaluator,
        Move potential_moves[MAX_MOVES], // sorted from prev_it_evals in engine.cpp
        int move_count, // Only first `move_count` moves are valid
        int depth,
        SearchLimits& limits,
        std::vector<Move> previousPV, // from last it_deep depth, used for move ordering
        int alpha, int beta
    );

    // -------------------------------
    // Minimax / Negamax search
    // -------------------------------
    static int negamax(
        Board& board,
        MoveGenerator& movegen,
        Evaluator& evaluator,
        int depth, // distance from end of search
        int alpha,
        int beta,
        PV& pv,  // PV line
        std::vector<Move> previousPV, // not updated so not using PV struct
        SearchLimits& limits,
        int ply,// distance from root
        bool quiesence
    );

    // -------------------------------
    // Quiescence search
    // -------------------------------
    static int quiescence(
        Board& board,
        Evaluator& evaluator,
        MoveGenerator& movegen,
        int alpha,
        int beta,
        PV& pv,  // Quiescence PV line
        SearchLimits& limits,
        int ply
    );


    // -------------------------------
    // Move ordering and scoring
    // -------------------------------
    static int moveScore(
        const Evaluator& evaluator,
        const Move& move,
        const Board& board,
        int ply,
        const Move& ttMove,
        std::vector<Move> previousPV
    );

    static void orderedMoves(
        const Evaluator& evaluator,
        Move moves[MAX_MOVES],
        size_t count,
        const Board& board,
        int ply,
        std::vector<Move> previousPV
    );

    static int generateAndOrderMoves( // called in negamax/quiescence
        Board& board,
        MoveGenerator& movegen,
        const Evaluator& evaluator,
        Move moves[MAX_MOVES],
        int ply,
        std::vector<Move> previousPV
    );

    // -------------------------------
    // Principal variation helpers
    // -------------------------------
    static void updatePV(
        std::vector<Move>& pv,
        const Move& move,
        const std::vector<Move>& childPV
    );

    // -------------------------------
    // Pruning helpers
    // -------------------------------
    static bool shouldPrune(
        Board& board,
        Move& move,
        Evaluator& evaluator,
        int standPat,
        int alpha
    );

    // -------------------------------
    // Depth helpers
    // -------------------------------
    static void enterDepth();
    static void exitDepth();


    // -------------------------------
    // Debugging
    // -------------------------------
    static int quiescenceVerbose(Board& board, Evaluator& evaluator, MoveGenerator& movegen,
                                int alpha, int beta, std::vector<Move>& pv,
                                SearchLimits& limits, int ply, std::ostream& log);
    static int verboseSearch(Board& board, Evaluator& evaluator, MoveGenerator& movegen,
                            int alpha, int beta, std::vector<Move>& pv,
                            SearchLimits& limits, int ply, bool use_quiescence, std::ostream& log);
};

#endif // SEARCHER_H
