// ---------------------
// -- Data Collection --
// ---------------------

#ifndef STATS_H
#define STATS_H

#include "helpers.h"
#include "move.h"
#include "session.h"
#include "logging.h"
#include "search_result.h"
#include <vector>
#include <array>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <cmath>

constexpr int STATS_MAX_PLY = 64;
constexpr int STATS_MAX_ITER_DEPTH = 32;
constexpr int STATS_FH_BUCKETS = 6; // move index buckets: 0, 1, 2, 3, 4-7, 8+

#define STATS_BOUNDS_CHECK(it_d, ply) \
    assert((it_d) >= 0 && (it_d) <= STATS_MAX_ITER_DEPTH && \
            (ply) >= 0 && (ply) <= STATS_MAX_PLY)

#define STATS_BOUNDS_CHECK_1D(d) \
    assert((d) >= 0 && (d) <= STATS_MAX_ITER_DEPTH)

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
    // fail-high move index histogram: buckets [0, 1, 2, 3, 4-7, 8+]
    std::array<uint64_t, STATS_FH_BUCKETS> fail_high_index{};
    uint64_t aspiration_fail_low_researches = 0;
    uint64_t aspiration_fail_high_researches = 0;
    uint64_t pvs_researches = 0;
    uint64_t see_prunes = 0;
    uint64_t delta_prunes = 0;
    uint64_t nmp = 0;
    uint64_t nmp_fail = 0;

    // iterative depth (stats per depth of single search)
    // tracks performance improvements across depths
    std::array<uint64_t, STATS_MAX_ITER_DEPTH> it_depth_time_ms{};
    std::array<int, STATS_MAX_ITER_DEPTH> it_depth_eval{};
    std::array<Move, STATS_MAX_ITER_DEPTH> it_depth_move{};
    //std::vector<std::vector<Move>> it_depth_pv;
    std::array<int, STATS_MAX_ITER_DEPTH> it_depth_qdepth{};
    std::array<uint64_t, STATS_MAX_ITER_DEPTH> it_depth_nodes{};
    std::array<uint64_t, STATS_MAX_ITER_DEPTH> it_depth_qnodes{};
    std::array<uint64_t, STATS_MAX_ITER_DEPTH> it_depth_ttstores{};
    std::array<uint64_t, STATS_MAX_ITER_DEPTH> it_depth_tthits{};
    std::array<uint64_t, STATS_MAX_ITER_DEPTH> it_depth_ttfill{};
    std::array<uint64_t, STATS_MAX_ITER_DEPTH> it_depth_fail_highs{};
    std::array<uint64_t, STATS_MAX_ITER_DEPTH> it_depth_fail_lows{};
    // per-iteration-depth fail-high index histogram
    std::array<std::array<uint64_t, STATS_FH_BUCKETS>, STATS_MAX_ITER_DEPTH> it_depth_fh_index{};
    std::array<uint64_t, STATS_MAX_ITER_DEPTH> it_depth_aspiration_failhigh_researches{};
    std::array<uint64_t, STATS_MAX_ITER_DEPTH> it_depth_aspiration_faillow_researches{};
    std::array<uint64_t, STATS_MAX_ITER_DEPTH> it_depth_pvs_researches{};
    std::array<uint64_t, STATS_MAX_ITER_DEPTH> it_depth_see_prunes{};
    std::array<uint64_t, STATS_MAX_ITER_DEPTH> it_depth_delta_prunes{};
    std::array<uint64_t, STATS_MAX_ITER_DEPTH> it_depth_nmp{};
    std::array<uint64_t, STATS_MAX_ITER_DEPTH> it_depth_nmp_fail{};

    // tree depth (stats per depth of search tree for single search)
    //  this leads to multi-counting as it_depth searches d=1..it_depth tree depths
    //  but this is where the actual work is done
    //      so search improvements based on data may begin here
    std::array<uint64_t, STATS_MAX_PLY> tree_depth_nodes{}; 
    std::array<uint64_t, STATS_MAX_PLY> tree_depth_qnodes{};
    std::array<uint64_t, STATS_MAX_PLY> tree_depth_ttstores{};
    std::array<uint64_t, STATS_MAX_PLY> tree_depth_tthits{};
    std::array<uint64_t, STATS_MAX_PLY> tree_depth_fail_highs{};
    std::array<uint64_t, STATS_MAX_PLY> tree_depth_fail_lows{};
    // per-tree-depth fail-high index histogram
    std::array<std::array<uint64_t, STATS_FH_BUCKETS>, STATS_MAX_PLY> tree_depth_fh_index{};
    std::array<uint64_t, STATS_MAX_PLY> tree_depth_see_prunes{};
    std::array<uint64_t, STATS_MAX_PLY> tree_depth_delta_prunes{};
    std::array<uint64_t, STATS_MAX_PLY> tree_depth_pvs_researches{};
    std::array<uint64_t, STATS_MAX_PLY> tree_depth_nmp{};
    std::array<uint64_t, STATS_MAX_PLY> tree_depth_nmp_fail{};
};

// --- single header-only global instance ---
inline SearchStats g_stats{};

// -------------------------
//      Reset
// -------------------------
inline void resetSearchStats() {
    g_stats = SearchStats{};
}


// --- macros for tracking ---
#define STATS_INC(field) \
    do { if (Logging::track_search_stats) g_stats.field++; } while(0)

#define STATS_ADD(field, val) \
    do { if (Logging::track_search_stats) g_stats.field += (val); } while(0)


#define STATS_NODE(it_d, ply)                                \
    do {                                                    \
        if (Logging::track_search_stats || Logging::track_search_nodes) {                  \
            /*STATS_BOUNDS_CHECK(it_d, ply);    */               \
            g_stats.nodes++;                                \
            g_stats.it_depth_nodes[it_d]++;                \
            g_stats.tree_depth_nodes[ply]++;               \
        }                                                   \
    } while (0)

#define STATS_QNODE(it_d, ply)                               \
    do {                                                    \
        if (Logging::track_search_stats || Logging::track_search_nodes) {                  \
            /*STATS_BOUNDS_CHECK(it_d, ply);*/                    \
            g_stats.qnodes++;                               \
            g_stats.it_depth_qnodes[it_d]++;                \
            g_stats.it_depth_qdepth[it_d] = std::max(g_stats.it_depth_qdepth[it_d], ply); \
            g_stats.tree_depth_qnodes[ply]++;               \
            g_stats.max_qdepth = std::max(g_stats.max_qdepth, ply); \
        }                                                   \
    } while (0)

#define STATS_ASPIRATION_FAILLOW(depth) \
    do { \
        if (Logging::track_search_stats) { \
            /*STATS_BOUNDS_CHECK_1D(depth); */\
            g_stats.aspiration_fail_low_researches++; \
            g_stats.it_depth_aspiration_faillow_researches[depth]++; \
        } \
    } while(0)

#define STATS_ASPIRATION_FAILHIGH(depth) \
    do { \
        if (Logging::track_search_stats) { \
            /*STATS_BOUNDS_CHECK_1D(depth); */\
            g_stats.aspiration_fail_high_researches++; \
            g_stats.it_depth_aspiration_failhigh_researches[depth]++; \
        } \
    } while(0)

#define STATS_PVS_RESEARCH(it_d, ply) \
    do { \
        if (Logging::track_search_stats) { \
            /*STATS_BOUNDS_CHECK(it_d, ply);*/ \
            g_stats.pvs_researches++; \
            g_stats.it_depth_pvs_researches[it_d]++; \
            g_stats.tree_depth_pvs_researches[ply]++; \
        } \
    } while(0)

// Helper to map move index to bucket [0,1,2,3,4-7,8+]
inline int fh_bucket(int move_idx) {
    if (move_idx <= 3) return move_idx;
    if (move_idx <= 7) return 4;
    return 5;
}

#define STATS_FAILHIGH(it_d, ply, move_idx)                  \
    do {                                                    \
        if (Logging::track_search_stats) {                  \
            /*STATS_BOUNDS_CHECK(it_d, ply);  */                  \
            g_stats.fail_highs++;                           \
            g_stats.it_depth_fail_highs[it_d]++;            \
            g_stats.tree_depth_fail_highs[ply]++;           \
            int _fh_b = fh_bucket(move_idx);                \
            g_stats.fail_high_index[_fh_b]++;               \
            g_stats.it_depth_fh_index[it_d][_fh_b]++;       \
            g_stats.tree_depth_fh_index[ply][_fh_b]++;      \
        }                                                   \
    } while (0)

#define STATS_FAILLOW(it_d, ply)                             \
    do {                                                    \
        if (Logging::track_search_stats) {                  \
            /*STATS_BOUNDS_CHECK(it_d, ply);   */                 \
            g_stats.fail_lows++;                            \
            g_stats.it_depth_fail_lows[it_d]++;             \
            g_stats.tree_depth_fail_lows[ply]++;            \
        }                                                   \
    } while (0)

#define STATS_TT_HIT(it_d, ply)                              \
    do {                                                    \
        if (Logging::track_search_stats) {                  \
            /*STATS_BOUNDS_CHECK(it_d, ply);     */               \
            g_stats.tt_hits++;                              \
            g_stats.it_depth_tthits[it_d]++;                \
            g_stats.tree_depth_tthits[ply]++;               \
        }                                                   \
    } while (0)

#define STATS_TT_STORE(it_d, ply)                            \
    do {                                                    \
        if (Logging::track_search_stats) {                  \
            /*STATS_BOUNDS_CHECK(it_d, ply);     */               \
            g_stats.tt_stores++;                            \
            g_stats.it_depth_ttstores[it_d]++;              \
            g_stats.tree_depth_ttstores[ply]++;             \
        }                                                   \
    } while (0)

#define STATS_SEE_PRUNE(it_d, ply)                           \
    do {                                                    \
        if (Logging::track_search_stats) {                  \
            /*STATS_BOUNDS_CHECK(it_d, ply);    */                \
            g_stats.see_prunes++;                           \
            g_stats.it_depth_see_prunes[it_d]++;            \
            g_stats.tree_depth_see_prunes[ply]++;           \
        }                                                   \
    } while (0)

#define STATS_DELTA_PRUNE(it_d, ply)                         \
    do {                                                    \
        if (Logging::track_search_stats) {                  \
            /*STATS_BOUNDS_CHECK(it_d, ply);    */                \
            g_stats.delta_prunes++;                         \
            g_stats.it_depth_delta_prunes[it_d]++;          \
            g_stats.tree_depth_delta_prunes[ply]++;         \
        }                                                   \
    } while (0)

#define STATS_NMP(it_d, ply) \
    do { \
        if (Logging::track_search_stats) { \
            /*STATS_BOUNDS_CHECK(it_d, ply);*/ \
            g_stats.nmp++; \
            g_stats.it_depth_nmp[it_d]++; \
            g_stats.tree_depth_nmp[ply]++; \
        } \
    } while(0)

#define STATS_NMP_FAIL(it_d, ply) \
    do { \
        if (Logging::track_search_stats) { \
            /*STATS_BOUNDS_CHECK(it_d, ply);*/ \
            g_stats.nmp_fail++; \
            g_stats.it_depth_nmp_fail[it_d]++; \
            g_stats.tree_depth_nmp_fail[ply]++; \
        } \
    } while(0)

// -------------------------
//      Logging Functions
// -------------------------

template <typename T, size_t N>
std::string array_to_json(const std::array<T, N>& v, size_t len)
{
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 1; i < len; ++i) // start from 1 to skip depth 0 which is init to 0
    {
        if (i-1) oss << ",";
        oss << v[i];
    }
    oss << "]";
    return oss.str();
};

template <size_t N>
std::string moves_to_json(const std::array<Move, N>& v, size_t len)
{
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 1; i < len; ++i) // start from 1 to skip depth 0 which is init to null_move
    {
        if (i-1) oss << ",";
        oss << "\"" << v[i].uci() << "\"";
    }
    oss << "]";
    return oss.str();
};

// Serialize a fixed-size array as JSON (no skipping index 0)
template <typename T, size_t N>
std::string bucket_array_to_json(const std::array<T, N>& v)
{
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < N; ++i) {
        if (i) oss << ",";
        oss << v[i];
    }
    oss << "]";
    return oss.str();
};

// Serialize per-depth bucket histogram as JSON array of arrays
template <size_t D, size_t B>
std::string depth_buckets_to_json(const std::array<std::array<uint64_t, B>, D>& v, size_t len)
{
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 1; i < len; ++i) {
        if (i-1) oss << ",";
        oss << "[";
        for (size_t j = 0; j < B; ++j) {
            if (j) oss << ",";
            oss << v[i][j];
        }
        oss << "]";
    }
    oss << "]";
    return oss.str();
};


inline void logRootMoves(const SearchResult& result, int depth) {
    static std::ofstream root_log(Logging::log_file_name("root_moves.jsonl"), std::ios::app);
    if (root_log.is_open()) {
        root_log << "{\"search_uuid\":\"" << g_run_context.search_uuid << "\","
                 << "\"depth\":" << depth << ",\"moves\":[";
        for (int rm = 0; rm < result.root_count; ++rm) {
            if (rm) root_log << ",";
            root_log << "{\"move\":\"" << result.root_moves[rm].move.uci() << "\","
                     << "\"eval\":" << result.root_moves[rm].eval << ","
                     << "\"time_ms\":" << result.root_moves[rm].time_ms << ","
                     << "\"nodes\":" << result.root_moves[rm].nodes << "}";
        }
        root_log << "]}\n";
        root_log.flush();
    }
}

inline void logSearchStats(const std::string& fen = "") {
    if (!(Logging::track_search_stats || Logging::track_search_nodes)) return;

    static std::ofstream out(Logging::log_file_name("search.jsonl"), std::ios::app);
    if (!out.is_open()) return;

    size_t n = g_stats.max_depth + 1;
    size_t q_n = g_stats.max_qdepth + 1;

    auto moves_list_to_json = [](const std::vector<Move>& v) { 
        std::ostringstream oss; 
        oss << "["; 
        for (size_t i = 0; i < v.size(); ++i) { 
            if (i) oss << ","; 
            oss << "\"" << v[i].uci() << "\""; 
        } oss << "]"; 
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
        << "\"fail_high_index\":" << bucket_array_to_json(g_stats.fail_high_index) << ","

        << "\"delta_prunes\":" << g_stats.delta_prunes << ","
        << "\"see_prunes\":" << g_stats.see_prunes << ","
        << "\"aspiration_fail_low_researches\":" << g_stats.aspiration_fail_low_researches << ","
        << "\"aspiration_fail_high_researches\":" << g_stats.aspiration_fail_high_researches << ","

        << "\"pvs_researches\":" << g_stats.pvs_researches << ","
        << "\"nmp\":" << g_stats.nmp << ","
        << "\"nmp_fail\":" << g_stats.nmp_fail << ","

        << "\"max_depth\":" << g_stats.max_depth << ","
        << "\"max_qdepth\":" << g_stats.max_qdepth << ","
        << "\"completed_depth\":" << g_stats.max_completed_depth << ","
        << "\"time_ms\":" << g_stats.time_ms << ","
        << "\"root_eval\":" << g_stats.eval << ","
        << "\"best_move\":\"" << g_stats.move.uci() << "\","
        << "\"principal_variation\":" << moves_list_to_json(g_stats.principal_variation) << ","

        // iteration stats
        << "\"itdepth_time_ms\":" << array_to_json(g_stats.it_depth_time_ms, n) << ","
        << "\"itdepth_eval\":" << array_to_json(g_stats.it_depth_eval, n) << ","
        << "\"itdepth_move\":" << moves_to_json(g_stats.it_depth_move, n) << ","
        << "\"itdepth_nodes\":" << array_to_json(g_stats.it_depth_nodes, n) << ","
        << "\"itdepth_qnodes\":" << array_to_json(g_stats.it_depth_qnodes, n) << ","
        << "\"itdepth_qdepth\":" << array_to_json(g_stats.it_depth_qdepth, n) << ","
        << "\"itdepth_tthits\":" << array_to_json(g_stats.it_depth_tthits, n) << ","
        << "\"itdepth_ttstores\":" << array_to_json(g_stats.it_depth_ttstores, n) << ","
        << "\"itdepth_fail_highs\":" << array_to_json(g_stats.it_depth_fail_highs, n) << ","
        << "\"itdepth_fail_lows\":" << array_to_json(g_stats.it_depth_fail_lows, n) << ","
        << "\"itdepth_fh_index\":" << depth_buckets_to_json(g_stats.it_depth_fh_index, n) << ","
        << "\"itdepth_aspiration_failhigh_researches\":" << array_to_json(g_stats.it_depth_aspiration_failhigh_researches, n) << ","
        << "\"itdepth_aspiration_faillow_researches\":" << array_to_json(g_stats.it_depth_aspiration_faillow_researches, n) << ","
        << "\"itdepth_see_prunes\":" << array_to_json(g_stats.it_depth_see_prunes, n) << ","
        << "\"itdepth_delta_prunes\":" << array_to_json(g_stats.it_depth_delta_prunes, n) << ","
        << "\"itdepth_pvs_researches\":" << array_to_json(g_stats.it_depth_pvs_researches, n) << ","
        << "\"itdepth_nmp\":" << array_to_json(g_stats.it_depth_nmp, n) << ","
        << "\"itdepth_nmp_fail\":" << array_to_json(g_stats.it_depth_nmp_fail, n) << ","

        // tree ply stats
        << "\"treedepth_nodes\":" << array_to_json(g_stats.tree_depth_nodes, q_n) << ","
        << "\"treedepth_qnodes\":" << array_to_json(g_stats.tree_depth_qnodes, q_n) << ","
        << "\"treedepth_tt_hits\":" << array_to_json(g_stats.tree_depth_tthits, q_n) << ","
        << "\"treedepth_tt_stores\":" << array_to_json(g_stats.tree_depth_ttstores, q_n) << ","
        << "\"treedepth_fail_highs\":" << array_to_json(g_stats.tree_depth_fail_highs, q_n) << ","
        << "\"treedepth_fail_lows\":" << array_to_json(g_stats.tree_depth_fail_lows, q_n) << ","
        << "\"treedepth_fh_index\":" << depth_buckets_to_json(g_stats.tree_depth_fh_index, q_n) << ","
        << "\"treedepth_see_prunes\":" << array_to_json(g_stats.tree_depth_see_prunes, q_n) << ","
        << "\"treedepth_delta_prunes\":" << array_to_json(g_stats.tree_depth_delta_prunes, q_n) << ","
        << "\"treedepth_pvs_researches\":" << array_to_json(g_stats.tree_depth_pvs_researches, q_n) << ","
        << "\"treedepth_nmp\":" << array_to_json(g_stats.tree_depth_nmp, q_n) << ","
        << "\"treedepth_nmp_fail\":" << array_to_json(g_stats.tree_depth_nmp_fail, q_n)

        << "}\n";

    out.flush();

    //resetSearchStats();
}

inline void dumpSearchStats()
{
    const uint64_t total_nodes = g_stats.nodes + g_stats.qnodes;

    const double nps =
        g_stats.time_ms > 0
            ? (1000.0 * static_cast<double>(total_nodes)) /
              static_cast<double>(g_stats.time_ms)
            : 0.0;

    const double tt_hit_pct =
        (g_stats.tt_hits + g_stats.tt_stores) > 0
            ? 100.0 * static_cast<double>(g_stats.tt_hits) /
              static_cast<double>(g_stats.tt_hits + g_stats.tt_stores)
            : 0.0;

    // Compute fail-high index summary stats (mean, median, p75)
    // Expand histogram to effective index values: 0,1,2,3,5.5(avg 4-7),10(avg 8+)
    const double bucket_midpoints[] = {0.0, 1.0, 2.0, 3.0, 5.5, 10.0};
    double fh_mean = 0.0;
    double fh_median = 0.0;
    double fh_p75 = 0.0;
    double fh_first_pct = 0.0;
    if (g_stats.fail_highs > 0) {
        const double fh_total = static_cast<double>(g_stats.fail_highs);
        // mean
        for (int b = 0; b < STATS_FH_BUCKETS; ++b)
            fh_mean += bucket_midpoints[b] * static_cast<double>(g_stats.fail_high_index[b]);
        fh_mean /= fh_total;
        // percentiles via cumulative distribution
        uint64_t cum = 0;
        bool found_median = false, found_p75 = false;
        const uint64_t median_target = (g_stats.fail_highs + 1) / 2;
        const uint64_t p75_target = (g_stats.fail_highs * 3 + 3) / 4;
        for (int b = 0; b < STATS_FH_BUCKETS; ++b) {
            cum += g_stats.fail_high_index[b];
            if (!found_median && cum >= median_target) { fh_median = bucket_midpoints[b]; found_median = true; }
            if (!found_p75 && cum >= p75_target) { fh_p75 = bucket_midpoints[b]; found_p75 = true; }
        }
        fh_first_pct = 100.0 * static_cast<double>(g_stats.fail_high_index[0]) / fh_total;
    }

    const double nmp_success_pct =
        g_stats.nmp > 0
            ? 100.0 * static_cast<double>(g_stats.nmp - g_stats.nmp_fail) /
              static_cast<double>(g_stats.nmp)
            : 0.0;

    std::cout << std::fixed << std::setprecision(2);

    // ===================== GLOBAL SUMMARY =====================
    std::cout
        << "\n============================================================\n"
        << "SEARCH STATS\n"
        << "============================================================\n\n"

        << "--- Search ---\n"
        << "Best Move        : " << g_stats.move.uci() << "\n"
        << "Eval             : " << g_stats.eval << "\n"
        << "Time (ms)        : " << g_stats.time_ms << "\n"
        << "Depth            : " << g_stats.max_depth << "\n"
        << "Completed Depth  : " << g_stats.max_completed_depth << "\n\n"

        << "--- Nodes ---\n"
        << "Nodes            : " << g_stats.nodes << "\n"
        << "QNodes           : " << g_stats.qnodes << "\n"
        << "Total            : " << total_nodes << "\n"
        << "NPS              : " << (uint64_t)nps << "\n\n"

        << "--- TT ---\n"
        << "Hits             : " << g_stats.tt_hits << "\n"
        << "Stores           : " << g_stats.tt_stores << "\n"
        << "Overwritten      : " << g_stats.tt_overwritten << "\n"
        << "Fill Ratio       : " << (g_stats.tt_fill_ratio * 100.0) << "%\n"
        << "Hit Rate         : " << tt_hit_pct << "%\n\n"

        << "--- Cutoffs ---\n"
        << "Fail Highs       : " << g_stats.fail_highs << "\n"
        << "Fail Lows        : " << g_stats.fail_lows << "\n"
        << "FH Index [0]     : " << g_stats.fail_high_index[0] << "  (" << fh_first_pct << "%)\n"
        << "FH Index [1]     : " << g_stats.fail_high_index[1] << "\n"
        << "FH Index [2]     : " << g_stats.fail_high_index[2] << "\n"
        << "FH Index [3]     : " << g_stats.fail_high_index[3] << "\n"
        << "FH Index [4-7]   : " << g_stats.fail_high_index[4] << "\n"
        << "FH Index [8+]    : " << g_stats.fail_high_index[5] << "\n"
        << "FH Mean Index    : " << fh_mean << "\n"
        << "FH Median Index  : " << fh_median << "\n"
        << "FH p75 Index     : " << fh_p75 << "\n\n"

        << "--- Pruning ---\n"
        << "SEE              : " << g_stats.see_prunes << "\n"
        << "Delta            : " << g_stats.delta_prunes << "\n\n"

        << "--- Null Move ---\n"
        << "Attempts         : " << g_stats.nmp << "\n"
        << "Fails            : " << g_stats.nmp_fail << "\n"
        << "Success %        : " << nmp_success_pct << "%\n\n"

        << "--- Aspiration ---\n"
        << "Fail High Re     : " << g_stats.aspiration_fail_high_researches << "\n"
        << "Fail Low Re      : " << g_stats.aspiration_fail_low_researches << "\n\n"

        << "--- PV ---\n";

    for (const Move& m : g_stats.principal_variation)
        std::cout << m.uci() << " ";
    std::cout << "\n\n";

    // ===================== ITERATIVE DEEPENING =====================
    std::cout
        << "------------------------------------------------------------\n"
        << "D  Eval  Move   Time  Nodes    FH   FL   NMP  SEE  PVS\n"
        << "------------------------------------------------------------\n";

    const size_t max_d = g_stats.max_depth; //g_stats.max_completed_depth;

    for (size_t d = 0; d <= max_d; ++d)
    {
        std::cout
            << std::setw(2) << d << " "
            << std::setw(5) << g_stats.it_depth_eval[d] << " "
            << std::setw(6) << g_stats.it_depth_move[d].uci() << " "
            << std::setw(5) << g_stats.it_depth_time_ms[d] << " "
            << std::setw(7) << g_stats.it_depth_nodes[d] << " "
            << std::setw(4) << g_stats.it_depth_fail_highs[d] << " "
            << std::setw(4) << g_stats.it_depth_fail_lows[d] << " "
            << std::setw(4) << g_stats.it_depth_nmp[d] << " "
            << std::setw(4) << g_stats.it_depth_see_prunes[d] << " "
            << std::setw(4) << g_stats.it_depth_pvs_researches[d]
            << "\n";
    }

    std::cout
        << "------------------------------------------------------------\n\n";
}


#endif // STATS_H
