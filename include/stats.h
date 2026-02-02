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

// 3 layer structure
//  1. full search summary 
//      total nodes, total ttHits, final eval/move
//  2. iterative deepening breakdown
//      nodes, etc. on it_d = 4,5,6
//  3. tree ply breakdown
//      nodes, ttHits, etc. on tree ply (depth in search tree)
//      multiple counts lower plies as it_deep will lead to searching ply < it_deep multiple times
//      Qnodes are assigned to the search_depth ply 
//          (e.g. if Qsearch is triggered at ply=6, all Q-stats are assigned to ply=6 even though Qsearch technically increases ply)
struct SearchStats {
    //  results / env
    int game_ply = 1;
    int eval = 0;
    Move move = Move::NullMove();
    std::vector<Move> principal_variation;
    uint64_t time_ms = 0;

    // full summary
    int max_completed_depth = 0;
    int max_depth = 0;
    int max_qdepth = 0;
    uint64_t nodes = 0;
    uint64_t qnodes = 0;
    uint64_t tt_hits = 0;
    uint64_t tt_stores = 0;
    double tt_fill_ratio = 0.0; // snapshot
    uint64_t tt_overwritten = 0;
    uint64_t fail_highs = 0;
    uint64_t fail_lows = 0;
    uint64_t fail_high_firsts = 0;
    uint64_t fail_high_lates = 0;
    uint64_t aspiration_fail_low_researches = 0;
    uint64_t aspiration_fail_high_researches = 0;
    uint64_t see_prunes = 0;
    uint64_t delta_prunes = 0;

    // iterative depth (stats per depth of single search)
    // tracks performance improvements across depths
    std::vector<uint64_t> it_depth_time_ms;
    std::vector<int> it_depth_eval;
    std::vector<Move> it_depth_move;
    //std::vector<std::vector<Move>> it_depth_pv;
    std::vector<uint64_t> it_depth_nodes;
    std::vector<uint64_t> it_depth_qnodes;
    std::vector<uint64_t> it_depth_ttstores;
    std::vector<uint64_t> it_depth_tthits;
    std::vector<uint64_t> it_depth_ttfill;
    std::vector<uint64_t> it_depth_fail_highs;
    std::vector<uint64_t> it_depth_fail_lows;
    std::vector<uint64_t> it_depth_fail_high_firsts;
    std::vector<uint64_t> it_depth_fail_high_lates;
    std::vector<uint64_t> it_depth_aspiration_failhigh_researches;
    std::vector<uint64_t> it_depth_aspiration_faillow_researches;
    std::vector<uint64_t> it_depth_see_prunes;
    std::vector<uint64_t> it_depth_delta_prunes;

    // tree depth (stats per depth of search tree for single search)
    //  this leads to multi-counting as it_depth searches d=1..it_depth tree depths
    //  but this is where the actual work is done
    //      so search improvements based on data may begin here
    std::vector<uint64_t> tree_depth_nodes; 
    std::vector<uint64_t> tree_depth_qnodes;
    std::vector<uint64_t> tree_depth_ttstores;
    std::vector<uint64_t> tree_depth_tthits;
    std::vector<uint64_t> tree_depth_fail_highs;
    std::vector<uint64_t> tree_depth_fail_lows;
    std::vector<uint64_t> tree_depth_fail_high_firsts;
    std::vector<uint64_t> tree_depth_fail_high_lates;
    std::vector<uint64_t> tree_depth_see_prunes;
    std::vector<uint64_t> tree_depth_delta_prunes;
};

// --- single header-only global instance ---
inline SearchStats g_stats{};

// --- macros for tracking ---
#define STATS_INC(field) \
    do { if (Logging::track_search_stats) g_stats.field++; } while(0)

#define STATS_ADD(field, val) \
    do { if (Logging::track_search_stats) g_stats.field += (val); } while(0)



template <typename T>
inline void statsDepthInit(std::vector<T> &vec, size_t index) {
    if (index >= vec.size())
        vec.insert(vec.end(), index + 1 - vec.size(), T(0));
}

#define STATS_DEPTH_INIT(it_d, ply)                                    \
    do {                                                                \
        if (Logging::track_search_stats) {                              \
            statsDepthInit(g_stats.it_depth_nodes, it_d);               \
            statsDepthInit(g_stats.it_depth_qnodes, it_d);              \
            statsDepthInit(g_stats.it_depth_ttstores, it_d);            \
            statsDepthInit(g_stats.it_depth_tthits, it_d);              \
            statsDepthInit(g_stats.it_depth_fail_highs, it_d);          \
            statsDepthInit(g_stats.it_depth_fail_lows, it_d);           \
            statsDepthInit(g_stats.it_depth_fail_high_firsts, it_d);    \
            statsDepthInit(g_stats.it_depth_fail_high_lates, it_d);     \
            statsDepthInit(g_stats.it_depth_aspiration_failhigh_researches, it_d); \
            statsDepthInit(g_stats.it_depth_aspiration_faillow_researches, it_d); \
            statsDepthInit(g_stats.it_depth_see_prunes, it_d);          \
            statsDepthInit(g_stats.it_depth_delta_prunes, it_d);        \
                                                                            \
            statsDepthInit(g_stats.tree_depth_nodes, ply);              \
            statsDepthInit(g_stats.tree_depth_qnodes, ply);             \
            statsDepthInit(g_stats.tree_depth_ttstores, ply);           \
            statsDepthInit(g_stats.tree_depth_tthits, ply);             \
            statsDepthInit(g_stats.tree_depth_fail_highs, ply);         \
            statsDepthInit(g_stats.tree_depth_fail_lows, ply);          \
            statsDepthInit(g_stats.tree_depth_fail_high_firsts, ply);   \
            statsDepthInit(g_stats.tree_depth_fail_high_lates, ply);    \
            statsDepthInit(g_stats.tree_depth_see_prunes, ply);         \
            statsDepthInit(g_stats.tree_depth_delta_prunes, ply);       \
        }                                                               \
    } while (0)


#define STATS_NODE(it_d, ply)                                \
    do {                                                    \
        if (Logging::track_search_stats) {                  \
            STATS_DEPTH_INIT(it_d, ply);                   \
            g_stats.nodes++;                                \
            g_stats.it_depth_nodes[it_d]++;                \
            g_stats.tree_depth_nodes[ply]++;               \
        }                                                   \
    } while (0)

#define STATS_QNODE(it_d, ply)                               \
    do {                                                    \
        if (Logging::track_search_stats) {                  \
            STATS_DEPTH_INIT(it_d, it_d);                    \
            g_stats.qnodes++;                               \
            g_stats.it_depth_qnodes[it_d]++;                \
            g_stats.tree_depth_qnodes[it_d]++;               \
            g_stats.max_qdepth = std::max(g_stats.max_qdepth, ply); \
        }                                                   \
    } while (0)

#define STATS_ASPIRATION_FAILLOW(depth) \
    do { \
        if (Logging::track_search_stats) { \
            STATS_DEPTH_INIT(depth, 0); \
            g_stats.aspiration_fail_low_researches++; \
            g_stats.it_depth_aspiration_faillow_researches[depth]++; \
        } \
    } while(0)

#define STATS_ASPIRATION_FAILHIGH(depth) \
    do { \
        if (Logging::track_search_stats) { \
            STATS_DEPTH_INIT(depth, 0); \
            g_stats.aspiration_fail_high_researches++; \
            g_stats.it_depth_aspiration_failhigh_researches[depth]++; \
        } \
    } while(0)

#define STATS_FAILHIGH(it_d, ply, first)                     \
    do {                                                    \
        if (Logging::track_search_stats) {                  \
            STATS_DEPTH_INIT(it_d, ply);                    \
            g_stats.fail_highs++;                           \
            g_stats.it_depth_fail_highs[it_d]++;            \
            g_stats.tree_depth_fail_highs[ply]++;           \
            if (first) {                                    \
                g_stats.fail_high_firsts++;                 \
                g_stats.it_depth_fail_high_firsts[it_d]++;  \
                g_stats.tree_depth_fail_high_firsts[ply]++; \
            } else {                                        \
                g_stats.fail_high_lates++;                  \
                g_stats.it_depth_fail_high_lates[it_d]++;   \
                g_stats.tree_depth_fail_high_lates[ply]++;  \
            }                                               \
        }                                                   \
    } while (0)

#define STATS_FAILLOW(it_d, ply)                             \
    do {                                                    \
        if (Logging::track_search_stats) {                  \
            STATS_DEPTH_INIT(it_d, ply);                    \
            g_stats.fail_lows++;                            \
            g_stats.it_depth_fail_lows[it_d]++;             \
            g_stats.tree_depth_fail_lows[ply]++;            \
        }                                                   \
    } while (0)

#define STATS_TT_HIT(it_d, ply)                              \
    do {                                                    \
        if (Logging::track_search_stats) {                  \
            STATS_DEPTH_INIT(it_d, ply);                    \
            g_stats.tt_hits++;                              \
            g_stats.it_depth_tthits[it_d]++;                \
            g_stats.tree_depth_tthits[ply]++;               \
        }                                                   \
    } while (0)

#define STATS_TT_STORE(it_d, ply)                            \
    do {                                                    \
        if (Logging::track_search_stats) {                  \
            STATS_DEPTH_INIT(it_d, ply);                    \
            g_stats.tt_stores++;                            \
            g_stats.it_depth_ttstores[it_d]++;              \
            g_stats.tree_depth_ttstores[ply]++;             \
        }                                                   \
    } while (0)

#define STATS_SEE_PRUNE(it_d, ply)                           \
    do {                                                    \
        if (Logging::track_search_stats) {                  \
            STATS_DEPTH_INIT(it_d, ply);                    \
            g_stats.see_prunes++;                           \
            g_stats.it_depth_see_prunes[it_d]++;            \
            g_stats.tree_depth_see_prunes[ply]++;           \
        }                                                   \
    } while (0)

#define STATS_DELTA_PRUNE(it_d, ply)                         \
    do {                                                    \
        if (Logging::track_search_stats) {                  \
            STATS_DEPTH_INIT(it_d, ply);                    \
            g_stats.delta_prunes++;                         \
            g_stats.it_depth_delta_prunes[it_d]++;          \
            g_stats.tree_depth_delta_prunes[ply]++;         \
        }                                                   \
    } while (0)

// -------------------------
//      Logging Functions
// -------------------------

inline void logSearchStats(const std::string& fen = "") {
    if (!Logging::track_search_stats) return;

    static std::ofstream out(Logging::log_dir / "search.jsonl", std::ios::app);
    if (!out.is_open()) return;

    auto vec_to_json = [](const auto& v) {
        std::ostringstream oss;
        oss << "[";
        for (size_t i = 1; i < v.size(); ++i) {
            if (i > 1) oss << ",";
            oss << v[i];
        }
        oss << "]";
        return oss.str();
    };

    auto vec_to_json_itdp_summary = [](const auto& v) {
        std::ostringstream oss;
        oss << "[";
        for (size_t i = 0; i < v.size(); ++i) {
            if (i) oss << ",";
            oss << v[i];
        }
        oss << "]";
        return oss.str();
    };

    auto moves_to_json = [](const std::vector<Move>& v) {
        std::ostringstream oss;
        oss << "[";
        for (size_t i = 0; i < v.size(); ++i) {
            if (i) oss << ",";
            oss << "\"" << v[i].uci() << "\"";
        }
        oss << "]";
        return oss.str();
    };

    out << "{"
        << "\"engine_id\":\"" << ENGINE_ID << "\","
        << "\"instance_id\":" << instanceID() << ","
        << "\"type\":\"search\","
        << "\"session\":" << currentSession() << ","
        << "\"game_uuid\":\"" << g_run_context.game_uuid << "\","
        << "\"search_uuid\":\"" << g_run_context.search_uuid << "\","
        << "\"fen\":\"" << fen << "\","
        << "\"ply\":" << g_stats.game_ply << ","

        << "\"nodes\":" << g_stats.nodes << ","
        << "\"qnodes\":" << g_stats.qnodes << ","
        << "\"tt_hits\":" << g_stats.tt_hits << ","
        << "\"tt_stores\":" << g_stats.tt_stores << ","
        << "\"tt_fill\":" << g_stats.tt_fill_ratio << ","
        << "\"tt_overwritten\":" << g_stats.tt_overwritten << ","

        << "\"fail_highs\":" << g_stats.fail_highs << ","
        << "\"fail_lows\":" << g_stats.fail_lows << ","
        << "\"fail_high_first\":" << g_stats.fail_high_firsts << ","
        << "\"fail_high_late\":" << g_stats.fail_high_lates << ","

        << "\"delta_prunes\":" << g_stats.delta_prunes << ","
        << "\"see_prunes\":" << g_stats.see_prunes << ","
        << "\"aspiration_fail_low_researches\":" << g_stats.aspiration_fail_low_researches << ","
        << "\"aspiration_fail_high_researches\":" << g_stats.aspiration_fail_high_researches << ","

        << "\"max_depth\":" << g_stats.max_depth << ","
        << "\"max_qdepth\":" << g_stats.max_qdepth << ","
        << "\"completed_depth\":" << g_stats.max_completed_depth << ","
        << "\"time_ms\":" << g_stats.time_ms << ","
        << "\"root_eval\":" << g_stats.eval << ","
        << "\"best_move\":\"" << g_stats.move.uci() << "\","
        << "\"principal_variation\":" << moves_to_json(g_stats.principal_variation) << ","

        // iteration stats
        << "\"itdepth_time_ms\":" << vec_to_json_itdp_summary(g_stats.it_depth_time_ms) << ","
        << "\"itdepth_eval\":" << vec_to_json_itdp_summary(g_stats.it_depth_eval) << ","
        << "\"itdepth_move\":" << moves_to_json(g_stats.it_depth_move) << ","
        << "\"itdepth_nodes\":" << vec_to_json(g_stats.it_depth_nodes) << ","
        << "\"itdepth_qnodes\":" << vec_to_json(g_stats.it_depth_qnodes) << ","
        << "\"itdepth_tthits\":" << vec_to_json(g_stats.it_depth_tthits) << ","
        << "\"itdepth_ttstores\":" << vec_to_json(g_stats.it_depth_ttstores) << ","
        << "\"itdepth_fail_highs\":" << vec_to_json(g_stats.it_depth_fail_highs) << ","
        << "\"itdepth_fail_lows\":" << vec_to_json(g_stats.it_depth_fail_lows) << ","
        << "\"itdepth_fail_high_firsts\":" << vec_to_json(g_stats.it_depth_fail_high_firsts) << ","
        << "\"itdepth_fail_high_lates\":" << vec_to_json(g_stats.it_depth_fail_high_lates) << ","
        << "\"itdepth_aspiration_failhigh_researches\":" << vec_to_json(g_stats.it_depth_aspiration_failhigh_researches) << ","
        << "\"itdepth_aspiration_faillow_researches\":" << vec_to_json(g_stats.it_depth_aspiration_faillow_researches) << ","
        << "\"itdepth_see_prunes\":" << vec_to_json(g_stats.it_depth_see_prunes) << ","
        << "\"itdepth_delta_prunes\":" << vec_to_json(g_stats.it_depth_delta_prunes) << ","

        // tree ply stats
        << "\"treedepth_nodes\":" << vec_to_json(g_stats.tree_depth_nodes) << ","
        << "\"treedepth_qnodes\":" << vec_to_json(g_stats.tree_depth_qnodes) << ","
        << "\"treedepth_tt_hits\":" << vec_to_json(g_stats.tree_depth_tthits) << ","
        << "\"treedepth_tt_stores\":" << vec_to_json(g_stats.tree_depth_ttstores) << ","
        << "\"treedepth_fail_highs\":" << vec_to_json(g_stats.tree_depth_fail_highs) << ","
        << "\"treedepth_fail_lows\":" << vec_to_json(g_stats.tree_depth_fail_lows) << ","
        << "\"treedepth_fail_high_firsts\":" << vec_to_json(g_stats.tree_depth_fail_high_firsts) << ","
        << "\"treedepth_fail_high_lates\":" << vec_to_json(g_stats.tree_depth_fail_high_lates) << ","
        << "\"treedepth_see_prunes\":" << vec_to_json(g_stats.tree_depth_see_prunes) << ","
        << "\"treedepth_delta_prunes\":" << vec_to_json(g_stats.tree_depth_delta_prunes)

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
