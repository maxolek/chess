#ifndef SEARCH_RESULT_H
#define SEARCH_RESULT_H

#include "move.h"
#include "helpers.h"
#include <vector>
#include <cstdint>

struct RootMove {
    Move move;
    int eval;
    int64_t time_ms = 0;
    uint64_t nodes = 0;
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
    PV best_line;

    RootMove root_moves[MAX_MOVES];
    int root_count = 0;

    inline void setPV(Move first, const PV& child) {
        best_line.set(first, child);
    }
};

#endif
