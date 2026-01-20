// ---------------------
// -- Data Collection --
// ---------------------

#ifndef STATS_H
#define STATS_H

#include "helpers.h"
#include "move.h"
#include "session.h"
#include "logging.h"
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
    int ply = 1;
    uint64_t nodes = 0;
    uint64_t qnodes = 0;
    uint64_t ttHits = 0;
    uint64_t ttStores = 0;
    double ttFillRatio = 0.0; // snapshot at ply
    uint64_t ttOverwritten = 0; // snapshot
    uint64_t failHighs = 0;
    uint64_t failLows = 0;

    // pruning
    uint64_t delta_prunes = 0;
    uint64_t see_prunes = 0;

    // Root results
    int rootEval = 0;
    Move bestMove = Move::NullMove();
    std::vector<Move> principal_variation;

    // Branching factors
    double ebf = 0.0;
    double qratio = 0.0;

    // Aspiration window re-searches
    int aspirationFailLow;
    int aspirationFailHigh;

    // Per-depth
    int maxDepth = 0;
    std::vector<uint64_t> depthSEEPrunes;
    std::vector<uint64_t> depthDeltaPrunes;
    std::vector<uint64_t> depthAspirationFailLowResearches;
    std::vector<uint64_t> depthAspirationFailHighResearches;
    std::vector<uint64_t> depthNodes;
    std::vector<uint64_t> depthQNodes;
    std::vector<uint64_t> depthFailHighs;
    std::vector<uint64_t> depthFailLows;
    std::vector<uint64_t> depthFailHighFirst;
    std::vector<uint64_t> depthFailHighLate;
    std::vector<uint64_t> depthTTHits;
    std::vector<uint64_t> depthTTStores;
    std::vector<double> timerPerDepthMS;
    std::vector<Move> bestMovePerDepth;

    // Move ordering
    uint64_t fail_high_first = 0;
    uint64_t fail_high_late = 0;
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
                g_stats.depthSEEPrunes.resize(depth + 1, 0); \
                g_stats.depthDeltaPrunes.resize(depth + 1, 0); \
                g_stats.depthFailHighFirst.resize(depth + 1, 0); \
                g_stats.depthFailHighLate.resize(depth + 1, 0); \
                g_stats.depthAspirationFailLowResearches.resize(depth + 1, 0); \
                g_stats.depthAspirationFailHighResearches.resize(depth + 1, 0); \
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

#define STATS_ASPIRATION_FAILLOW(depth) \
    do { \
        if (Logging::track_search_stats) { \
            STATS_DEPTH_INIT(depth); \
            g_stats.aspirationFailLow++; \
            g_stats.depthAspirationFailLowResearches[depth]++; \
        } \
    } while(0)

#define STATS_ASPIRATION_FAILHIGH(depth) \
    do { \
        if (Logging::track_search_stats) { \
            STATS_DEPTH_INIT(depth); \
            g_stats.aspirationFailHigh++; \
            g_stats.depthAspirationFailHighResearches[depth]++; \
        } \
    } while(0)

#define STATS_FAILHIGH(depth, flag) \
    do { \
        if (Logging::track_search_stats) { \
            STATS_DEPTH_INIT(depth); \
            g_stats.depthFailHighs[depth]++; \
            g_stats.failHighs++; \
            if (flag == "first") { \
                g_stats.fail_high_first++; \
                g_stats.depthFailHighFirst[depth]++; \
            } else if (flag == "late") { \
                g_stats.fail_high_late++; \
                g_stats.depthFailHighLate[depth]++; \
            } \
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

#define STATS_TT_HIT(depth, tt_ref) \
    do { \
        if (Logging::track_search_stats) { \
            STATS_DEPTH_INIT(depth); \
            g_stats.depthTTHits[depth]++; \
            g_stats.ttHits++; \
            /*g_stats.ttFillRatio = (tt_ref).fillRatio();*/ /* snapshot */ \
            /*g_stats.ttOverwritten = (tt_ref).stats.overwritten;*/ \
        } \
    } while(0)

#define STATS_TT_STORE(depth, tt_ref) \
    do { \
        if (Logging::track_search_stats) { \
            STATS_DEPTH_INIT(depth); \
            g_stats.depthTTStores[depth]++; \
            g_stats.ttStores++; \
            /*g_stats.ttFillRatio = (tt_ref).fillRatio();*/ \
            /*g_stats.ttOverwritten = (tt_ref).stats.overwritten;*/ \
        } \
    } while(0)

#define STATS_PRUNE(depth, type) \
    do { \
        if (Logging::track_search_stats) { \
            STATS_DEPTH_INIT(depth); \
            if (type == "see") { \
                g_stats.see_prunes++; \
                g_stats.depthSEEPrunes[depth]++; \
            } else if (type == "delta") { \
                g_stats.delta_prunes++; \
                g_stats.depthDeltaPrunes[depth]++; \
            } \
        } \
    } while(0)

// -------------------------
//      Logging Functions
// -------------------------

inline void logSearchStats(const std::string& fen = "") {
    if (!Logging::track_search_stats) return;

    static std::ofstream out(Logging::log_dir / "search.jsonl", std::ios::app);
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
        << "\"engine_id\":\"" << ENGINE_ID << "\","
        << "\"instance_id\":" << instanceID() << ","
        << "\"type\":\"search\","
        << "\"session\":" << currentSession() << ","
        << "\"game_uuid\":\"" << g_run_context.game_uuid << "\","
        << "\"search_uuid\":\"" << g_run_context.search_uuid << "\","
        << "\"fen\":\"" << fen << "\","
        << "\"ply\":" << g_stats.ply << ","
        << "\"nodes\":" << g_stats.nodes << ","
        << "\"qnodes\":" << g_stats.qnodes << ","
        << "\"tt_hits\":" << g_stats.ttHits << ","
        << "\"tt_stores\":" << g_stats.ttStores << ","
        << "\"tt_fill\":" << g_stats.ttFillRatio << ","
        << "\"tt_overwritten\":" << g_stats.ttOverwritten << ","
        << "\"fail_highs\":" << g_stats.failHighs << ","
        << "\"fail_lows\":" << g_stats.failLows << ","
        << "\"fail_high_first\":" << g_stats.fail_high_first << ","
        << "\"fail_high_late\":" << g_stats.fail_high_late << ","
        << "\"ebf\":" << g_stats.ebf << ","
        << "\"qratio\":" << g_stats.qratio << ","
        << "\"max_depth\":" << g_stats.maxDepth << ","
        << "\"time_ms\":" << g_stats.timeMs << ","
        << "\"nps\":" << g_stats.nps << ","
        << "\"root_eval\":" << g_stats.rootEval << ","
        << "\"best_move\":\"" << g_stats.bestMove.uci() << "\","
        << "\"principal_variation\":" << moves_to_json(g_stats.principal_variation) << ","
        << "\"delta_prunes\":" << g_stats.delta_prunes << ","
        << "\"see_prunes\":" << g_stats.see_prunes << ","
        << "\"depthNodes\":" << vec_to_json(g_stats.depthNodes) << ","
        << "\"depthQNodes\":" << vec_to_json(g_stats.depthQNodes) << ","
        << "\"depthFailHighs\":" << vec_to_json(g_stats.depthFailHighs) << ","
        << "\"depthFailLows\":" << vec_to_json(g_stats.depthFailLows) << ","
        << "\"depthTTHits\":" << vec_to_json(g_stats.depthTTHits) << ","
        << "\"depthTTStores\":" << vec_to_json(g_stats.depthTTStores) << ","
        << "\"depthSEEPrunes\":" << vec_to_json(g_stats.depthSEEPrunes) << ","
        << "\"depthDeltaPrunes\":" << vec_to_json(g_stats.depthDeltaPrunes) << ","
        << "\"aspirationFailLowResearches\":" << g_stats.aspirationFailLow << ","
        << "\"aspirationFailHighResearches\":" << g_stats.aspirationFailHigh << ","
        << "\"depthAspirationFailLowResearches\":" << vec_to_json(g_stats.depthAspirationFailLowResearches) << ","
        << "\"depthAspirationFailHighResearches\":" << vec_to_json(g_stats.depthAspirationFailHighResearches) << ","
        << "\"depthFailHighFirst\":" << vec_to_json(g_stats.depthFailHighFirst) << ","
        << "\"depthFailHighLate\":" << vec_to_json(g_stats.depthFailHighLate) << ","
        << "\"evalPerDepth\":" << vec_to_json(g_stats.evalPerDepth) << ","
        << "\"bestMovePerDepth\":" << moves_to_json(g_stats.bestMovePerDepth) << ","
        << "\"timeOnDepthMS\":" << vec_to_json(g_stats.timerPerDepthMS)
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
