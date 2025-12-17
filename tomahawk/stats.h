// ---------------------
// -- Data Collection --
// ---------------------

#ifndef STATS_H
#define STATS_H

#include "helpers.h"
#include "move.h"
#include "session.h"
#include <vector>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <cmath>

// -------------------------
//      search stats
// -------------------------

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
    std::vector<uint64_t> depthNodes;
    std::vector<uint64_t> depthQNodes;
    std::vector<uint64_t> depthFailHighs;
    std::vector<uint64_t> depthFailLows;
    std::vector<uint64_t> depthTTHits;
    std::vector<uint64_t> depthTTStores;
    std::vector<double> timerPerDepthMS;
    std::vector<Move> bestMovePerDepth;

    // Move ordering
    uint64_t fail_high_first = 0;
    int bestmoveStable = 0;

    // Timing
    uint64_t timeMs = 0;
    double nps = 0.0;

    // Eval trajectory
    std::vector<int> evalPerDepth;

};

// --- single header-only global instance ---
inline SearchStats g_stats{};

// --- macros for tracking ---
#define STATS_INC(field) \
    do { if (Logging::track_search_stats) g_stats.field++; } while(0)

#define STATS_ADD(field, val) \
    do { if (Logging::track_search_stats) g_stats.field += (val); } while(0)

#define STATS_DEPTH_INIT(depth) \
    do { \
        if (Logging::track_search_stats) { \
            if ((int)g_stats.depthNodes.size() <= (depth)) { \
                g_stats.depthNodes.resize(depth + 1, 0); \
                g_stats.depthQNodes.resize(depth + 1, 0); \
                g_stats.depthFailHighs.resize(depth + 1, 0); \
                g_stats.depthFailLows.resize(depth + 1, 0); \
                g_stats.depthTTHits.resize(depth + 1, 0); \
                g_stats.depthTTStores.resize(depth + 1, 0); \
            } \
        } \
    } while(0)

#define STATS_ENTER(depth) \
    do { \
        if (Logging::track_search_stats) { \
            STATS_DEPTH_INIT(depth); \
            g_stats.depthNodes[depth]++; \
            g_stats.nodes++; \
        } \
    } while(0)

#define STATS_QNODE(depth) \
    do { \
        if (Logging::track_search_stats) { \
            STATS_DEPTH_INIT(depth); \
            g_stats.depthQNodes[depth]++; \
            g_stats.qnodes++; \
        } \
    } while(0)

#define STATS_FAILHIGH(depth) \
    do { \
        if (Logging::track_search_stats) { \
            STATS_DEPTH_INIT(depth); \
            g_stats.depthFailHighs[depth]++; \
            g_stats.failHighs++; \
        } \
    } while(0)

#define STATS_FAILLOW(depth) \
    do { \
        if (Logging::track_search_stats) { \
            STATS_DEPTH_INIT(depth); \
            g_stats.depthFailLows[depth]++; \
            g_stats.failLows++; \
        } \
    } while(0)

#define STATS_TT_HIT(depth) \
    do { \
        if (Logging::track_search_stats) { \
            STATS_DEPTH_INIT(depth); \
            g_stats.depthTTHits[depth]++; \
            g_stats.ttHits++; \
        } \
    } while(0)

#define STATS_TT_STORE(depth) \
    do { \
        if (Logging::track_search_stats) { \
            STATS_DEPTH_INIT(depth); \
            g_stats.depthTTStores[depth]++; \
            g_stats.ttStores++; \
        } \
    } while(0)

// -------------------------
//      Logging Functions
// -------------------------

inline void logSearchStats(const std::string& fen = "") {
    if (!Logging::track_search_stats) return;

    static std::ofstream out(Logging::log_dir + "/search.jsonl", std::ios::app);
    if (!out.is_open()) return;

    // helpers
    auto vec_to_json = [](const auto& v) {
        std::ostringstream oss;
        oss << "[";
        for (size_t i = 0; i < v.size(); ++i) {
            if (i) oss << ",";
            oss << v[i];
        }
        oss << "]";
        return oss.str();
    };

    auto nested_vec_to_json = [](const auto& vv) {
        std::ostringstream oss;
        oss << "[";
        for (size_t i = 0; i < vv.size(); ++i) {
            if (i) oss << ",";
            oss << "[";
            for (size_t j = 0; j < vv[i].size(); ++j) {
                if (j) oss << ",";
                oss << vv[i][j];
            }
            oss << "]";
        }
        oss << "]";
        return oss.str();
    };

    auto moves_to_json = [](const auto& v) {
        std::ostringstream oss;
        oss << "[";
        for (size_t i = 0; i < v.size(); ++i) {
            if (i) oss << ",";
            oss << "\"" << v[i].uci() << "\"";
        }
        oss << "]";
        return oss.str();
    };

    // single JSONL object
    out << "{"
        << "\"engine_id\":" << ENGINE_ID << ","
        << "\"type\":\"search\","
        << "\"session\":" << currentSession() << ","
        << "\"fen\":\"" << fen << "\","
        << "\"nodes\":" << g_stats.nodes << ","
        << "\"qnodes\":" << g_stats.qnodes << ","
        << "\"tt_hits\":" << g_stats.ttHits << ","
        << "\"tt_stores\":" << g_stats.ttStores << ","
        << "\"fail_highs\":" << g_stats.failHighs << ","
        << "\"fail_lows\":" << g_stats.failLows << ","
        << "\"ebf\":" << g_stats.ebf << ","
        << "\"qratio\":" << g_stats.qratio << ","
        << "\"max_depth\":" << g_stats.maxDepth << ","
        << "\"time_ms\":" << g_stats.timeMs << ","
        << "\"nps\":" << g_stats.nps << ","
        << "\"root_eval\":" << g_stats.rootEval << ","
        << "\"best_move\":\"" << g_stats.bestMove.uci() << "\","
        << "\"depthNodes\":" << vec_to_json(g_stats.depthNodes) << ","
        << "\"depthQNodes\":" << vec_to_json(g_stats.depthQNodes) << ","
        << "\"depthFailHighs\":" << vec_to_json(g_stats.depthFailHighs) << ","
        << "\"depthFailLows\":" << vec_to_json(g_stats.depthFailLows) << ","
        << "\"depthTTHits\":" << vec_to_json(g_stats.depthTTHits) << ","
        << "\"depthTTStores\":" << vec_to_json(g_stats.depthTTStores) << ","
        << "\"aspirationResearches\":" << nested_vec_to_json(g_stats.aspirationResearches) << ","
        << "\"evalPerDepth\":" << vec_to_json(g_stats.evalPerDepth) << ","
        << "\"bestMovePerDepth\":" << moves_to_json(g_stats.bestMovePerDepth)
        << "}\n";

    out.flush();
}


// -------------------------
//      Reset
// -------------------------
inline void resetSearchStats() {
    g_stats = SearchStats{};
}

#endif // STATS_H
