#ifndef SEARCHER_H
#define SEARCHER_H

#include "evaluator.h"

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

    static SearchResult search(
        Board& board,
        MoveGenerator& movegen,
        std::vector<Move>& potential_moves,
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
        int depth, 
        bool maximizing,
        std::chrono::steady_clock::time_point start_time, 
        int time_limit_ms, 
        bool out_of_time
    );
};

#endif