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

    static int moveScore(const Move& move, const Board& board);
    static void orderedMoves(Move moves[MoveGenerator::max_moves], const Board& board); // order in place
};

#endif