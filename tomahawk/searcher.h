#ifndef SEARCHER_H
#define SEARCHER_H

#include "evaluator.h"

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
    static int nodesSearched;
    static std::unordered_map<U64, TTEntry> tt;

    // eval enhancements (eval is based on board, not move)
    static const int castle_increase = 50; // centipawn

    // move scoring
    static constexpr int MAX_DEPTH = 64;
    static constexpr int MAX_MOVES = MoveGenerator::max_moves; 
    // Killer moves: [depth][2] (we store up to 2 killer moves per depth)
    static Move killerMoves[MAX_DEPTH][2];
    // History heuristic: [piece][toSquare]
    static int historyHeuristic[6][64];

    static SearchResult search(
        Board& board,
        MoveGenerator& movegen,
        Evaluator& evaluator,
        Move potential_moves[MoveGenerator::max_moves],
        int move_count, // potential_moves will possibly contain old moves, shielded by count
        int depth,
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
    static int minimax(
        Board& board, 
        MoveGenerator& movegen, 
        Evaluator& evaluator, 
        int depth, 
        bool maximizing,
        int alpha, int beta, // alpha beta pruning
        std::chrono::steady_clock::time_point start_time, 
        int time_limit_ms, bool out_of_time
    );

    static int moveScore(
        const Move& move, 
        const Board& board, 
        int depth, 
        const Move& ttMove
    );
    static void orderedMoves(
        Move moves[MoveGenerator::max_moves], 
        int count, 
        const Board& board, 
        int depth
    ); // order in place

    static int Searcher::generateAndOrderMoves(
        Board& board, 
        MoveGenerator& movegen, 
        Move moves[MoveGenerator::max_moves], 
        int depth
    );
};

#endif