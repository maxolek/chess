#ifndef SEARCHER_H
#define SEARCHER_H

#include "evaluator.h"
#include "helpers.h"

// transposition table
enum BoundType { EXACT, LOWERBOUND, UPPERBOUND };

struct TTEntry {
    int eval;
    int depth;
    Move bestMove;
    BoundType flag;
};

struct ScoredMove {
    Move move;
    int score;

    bool operator<(const ScoredMove& other) const {
        return score > other.score;  // Higher score = better
    }
};

struct SearchResult {
    Move bestMove;
    int eval;
    std::unordered_map<std::string, int> component_evals;
};

class Searcher {
private:
    //Board* board;
    //std::vector<Move>* potential_moves;
public:
    static constexpr int KILL_SEARCH_RETURN = -5*MATE_SCORE;

    inline static bool stop = false; // this will be externally reset by the engine to kill search
    static int nodesSearched;
    static std::unordered_map<U64, TTEntry> tt;
 

    // eval enhancements (eval is based on board, not move)
    static const int castle_increase = 50; // centipawn

    // move scoring
    // Killer moves: [depth][2] (we store up to 2 killer moves per depth)
    static Move killerMoves[MAX_DEPTH][2];
    // History heuristic: [piece][toSquare]
    static int historyHeuristic[12][64];

    static std::vector<Move> best_line; // pv line tracker

    static SearchResult search(
        Board& board,
        MoveGenerator& movegen,
        Evaluator& evaluator,
        Move potential_moves[MAX_MOVES],
        int move_count, // potential_moves will possibly contain old moves, shielded by count
        int depth,
        Move pvMove,
        std::chrono::steady_clock::time_point start_time, 
        int time_limit_ms
    );
    /*static Move bestMove(
        Board& board, 
        MoveGenerator& movegen, 
        std::vector<Move>& potential_moves, 
        int depth, 
        std::chrono::steady_clock::time_point start_time, 
        int time_limit_ms
    );*/
    static int negamax( // not negated on return as usual as eval is side agnostic
        Board& board, // so flip is performed in the return, not the call
        MoveGenerator& movegen, 
        Evaluator& evaluator, 
        int depth, 
        int alpha, int beta, // alpha beta pruning (will be swapped in func calls)
        std::vector<Move>& pv, // best_line
        const std::vector<Move>& inputPV, // for passing through negamax recursion
        int prev_evals[MAX_MOVES],
        std::chrono::steady_clock::time_point start_time, 
        int time_limit_ms, bool& out_of_time
    );
    static int quiescence(
        Board& board,
        Evaluator& evaluator,
        MoveGenerator& movegen,
        int alpha, int beta,
        std::chrono::steady_clock::time_point start_time, 
        int time_limit_ms
    );

    static int moveScore(
        const Move& move, 
        const Board& board, 
        int depth, 
        const Move& ttMove,
        const Move& pvMove,
        int prev_eval
    );
    static void orderedMoves(
        Move moves[MAX_MOVES], 
        int count, 
        const Board& board, 
        int depth, 
        const Move& pvMove,
        int prev_eval[MAX_MOVES]
    ); // order in place

    static int generateAndOrderMoves(
        Board& board, 
        MoveGenerator& movegen, 
        Move moves[MAX_MOVES], 
        int depth,
        const Move& pvMove,
        int prev_eval[MAX_MOVES]
    );
};

#endif