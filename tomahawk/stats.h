// ---------------------
// -- Data Collection --
// ---------------------

#ifndef STATS_H
#define STATS_H

#include "helpers.h"
#include "move.h"
#include <vector>

struct SearchStats {
    // General
    uint64_t nodes = 0;
    uint64_t qnodes = 0;
    uint64_t ttHits = 0;
    uint64_t ttStores = 0;
    uint64_t failHighs = 0;
    uint64_t failLows = 0;

    // Root results
    int rootEval = 0;
    Move bestMove = Move::NullMove();

    // Branching factors
    double ebf = 0.0;
    double qratio = 0.0;

    // Aspiration window re-searches
    std::vector<std::vector<int>> aspirationResearches;

    // Per-depth
    int maxDepth = 0;
    std::vector<int> depthNodes;
    std::vector<int> depthQNodes;
    std::vector<int> depthFailHighs;
    std::vector<int> depthFailLows;
    std::vector<int> depthTTHits;
    std::vector<int> depthTTStores;

    // Move ordering
    uint64_t fail_high_first = 0;
    int bestmoveStable = 0;

    // Timing
    uint64_t timeMs = 0;
    double nps = 0.0;

    // Eval trajectory
    std::vector<int> evalPerDepth;
};

namespace GlobalStats {
    // refer explicitly to the global SearchStats type
    extern bool trackStats;
    extern ::SearchStats stats;
}

#define STATS_INC(field) \
    do { if (GlobalStats::trackStats) GlobalStats::stats.field++; } while(0)

#define STATS_ADD(field, val) \
    do { if (GlobalStats::trackStats) GlobalStats::stats.field += (val); } while(0)

#define STATS_DEPTH_INIT(depth) \
    do { \
        if (GlobalStats::trackStats) { \
            if ((int)GlobalStats::stats.depthNodes.size() <= (depth)) { \
                GlobalStats::stats.depthNodes.resize(depth + 1, 0); \
                GlobalStats::stats.depthQNodes.resize(depth + 1, 0); \
                GlobalStats::stats.depthFailHighs.resize(depth + 1, 0); \
                GlobalStats::stats.depthFailLows.resize(depth + 1, 0); \
                GlobalStats::stats.depthTTHits.resize(depth + 1, 0); \
                GlobalStats::stats.depthTTStores.resize(depth + 1, 0); \
            } \
        } \
    } while(0)

#define STATS_ENTER(depth) \
    do { \
        if (GlobalStats::trackStats) { \
            STATS_DEPTH_INIT(depth); \
            GlobalStats::stats.depthNodes[depth]++; \
            GlobalStats::stats.nodes++; \
        } \
    } while(0)

#define STATS_QNODE(depth) \
    do { \
        if (GlobalStats::trackStats) { \
            STATS_DEPTH_INIT(depth); \
            GlobalStats::stats.depthQNodes[depth]++; \
            GlobalStats::stats.qnodes++; \
        } \
    } while(0)

#define STATS_FAILHIGH(depth) \
    do { \
        if (GlobalStats::trackStats) { \
            STATS_DEPTH_INIT(depth); \
            GlobalStats::stats.depthFailHighs[depth]++; \
            GlobalStats::stats.failHighs++; \
        } \
    } while(0)

#define STATS_FAILLOW(depth) \
    do { \
        if (GlobalStats::trackStats) { \
            STATS_DEPTH_INIT(depth); \
            GlobalStats::stats.depthFailLows[depth]++; \
            GlobalStats::stats.failLows++; \
        } \
    } while(0)

#define STATS_TT_HIT(depth) \
    do { \
        if (GlobalStats::trackStats) { \
            STATS_DEPTH_INIT(depth); \
            GlobalStats::stats.depthTTHits[depth]++; \
            GlobalStats::stats.ttHits++; \
        } \
    } while(0)

#define STATS_TT_STORE(depth) \
    do { \
        if (GlobalStats::trackStats) { \
            STATS_DEPTH_INIT(depth); \
            GlobalStats::stats.depthTTStores[depth]++; \
            GlobalStats::stats.ttStores++; \
        } \
    } while(0)

#endif // STATS_H
