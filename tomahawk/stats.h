// ---------------------
// -- Data Collection --
// ---------------------

#ifndef STATS_H
#define STATS_H

#include "helpers.h"
#include "move.h"

#define STATS_INC(field) do { if (Searcher::trackStats) Searcher::stats.field++; } while(0)
#define STATS_ADD(field, val) do { if (Searcher::trackStats) Searcher::stats.field += (val); } while(0)

// Per-depth tracking helpers
#define STATS_DEPTH_INIT(depth) do {                                  \
    if (Searcher::trackStats) {                                      \
        if ((int)Searcher::stats.depthNodes.size() <= (depth)) {     \
            Searcher::stats.depthNodes.resize(depth + 1, 0);         \
            Searcher::stats.depthQNodes.resize(depth + 1, 0);        \
            Searcher::stats.depthFailHighs.resize(depth + 1, 0);     \
            Searcher::stats.depthFailLows.resize(depth + 1, 0);      \
        }                                                            \
    }                                                                \
} while(0)

#define STATS_ENTER(depth) do {                 \
    if (Searcher::trackStats) {                \
        STATS_DEPTH_INIT(depth);                \
        Searcher::stats.depthNodes[depth]++;   \
        Searcher::stats.nodes++;               \
    }                                          \
} while(0)

#define STATS_QNODE(depth) do {                 \
    if (Searcher::trackStats) {                \
        STATS_DEPTH_INIT(depth);                \
        Searcher::stats.depthQNodes[depth]++;  \
        Searcher::stats.qnodes++;              \
    }                                          \
} while(0)

#define STATS_FAILHIGH(depth) do {              \
    if (Searcher::trackStats) {                \
        STATS_DEPTH_INIT(depth);                \
        Searcher::stats.depthFailHighs[depth]++; \
        Searcher::stats.failHighs++;           \
    }                                          \
} while(0)

#define STATS_FAILLOW(depth) do {               \
    if (Searcher::trackStats) {                \
        STATS_DEPTH_INIT(depth);                \
        Searcher::stats.depthFailLows[depth]++; \
        Searcher::stats.failLows++;            \
    }                                          \
} while(0)

#define STATS_TT_HIT(depth) do { \
    if (Searcher::trackStats) { \
        STATS_DEPTH_INIT(depth); \
        Searcher::stats.depthTTHits.resize(depth + 1); \
        Searcher::stats.depthTTHits[depth]++; \
        Searcher::stats.ttHits++; \
    } \
} while(0)

#define STATS_TT_STORE(depth) do { \
    if (Searcher::trackStats) { \
        STATS_DEPTH_INIT(depth); \
        Searcher::stats.depthTTStores.resize(depth + 1); \
        Searcher::stats.depthTTStores[depth]++; \
        Searcher::stats.ttStores++; \
    } \
} while(0)


struct SearchStats {
    // General
    uint64_t nodes = 0;
    uint64_t qnodes = 0;        // quiescence nodes
    uint64_t ttHits = 0;
    uint64_t ttStores = 0;
    uint64_t failHighs = 0;
    uint64_t failLows = 0;

    // root (final) results
    int rootEval = 0;               // final reported eval
    Move bestMove = Move::NullMove(); // final best move

    // Branching factors
    double ebf = 0.0;           // effective branching factor
    double qratio = 0.0;        // ratio quiescence / total nodes

    // Aspiration windows
    // one inner vector per depth
    // +1/-1 per fail high/low re-search
    std::vector<std::vector<int>> aspirationResearches;   

    // Depth / search iterations
    int maxDepth = 0;
    std::vector<int> depthNodes;    // per-depth node counts
    std::vector<int> depthQNodes;
    std::vector<int> depthFailHighs;
    std::vector<int> depthFailLows;
    std::vector<int> depthTTHits;
    std::vector<int> depthTTStores;

    // Move ordering
    uint64_t fail_high_first = 0;   // cutoffs on the first move searched
    int bestmoveStable = 0;         // how often bestmove stayed same between depths

    // Timing
    uint64_t timeMs = 0;
    double nps = 0.0;

    // Eval consistency
    std::vector<int> evalPerDepth;  // eval trajectory over ID
};

#endif