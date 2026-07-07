#ifndef TIMER_H
#define TIMER_H

#include "helpers.h"
#include "move.h"
#include "session.h"
#include "logging.h"
#include <vector>
#include <chrono>
#include <fstream>

#ifdef _MSC_VER
    #define FORCEINLINE FORCEINLINE
#else
    #define FORCEINLINE inline __attribute__((always_inline))
#endif

// Timer IDs
enum TimerID {
    T_MOVEGEN, T_MAKEMOVE, T_UNMAKE_MOVE,
    T_SEARCH, T_PVS_SEARCH, T_PVS_RESEARCH, T_NMP_SEARCH,
    T_SCORE_ORDER,
    T_TT_PROBE, T_TT_STORE, 
    T_EVAL, T_NNUE, 
    T_SEE,
    T_BOOK_PROBE, 
    T_COUNT
};

// inline variable avoids multiple definitions
inline const char* TimerNames[T_COUNT] = {
    "MOVEGEN", "MAKEMOVE", "UNMAKE_MOVE",
    "SEARCH", "PVS_SEARCH", "PVS_RESEARCH", "NMP_SEARCH",
    "SCORE_ORDER",
    "TT_PROBE", "TT_STORE",
    "EVAL", "NNUE",
    "SEE",
    "BOOK_PROBE"
};

// Timer struct using std::chrono
struct Timer {
    using clock = std::chrono::high_resolution_clock;
    clock::time_point start;

    FORCEINLINE void begin() { start = clock::now(); }

    FORCEINLINE uint64_t end() const {
        auto now = clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(now - start).count();
    }

    static uint64_t freq() { return 1'000'000'000ULL; } // nanoseconds
};

struct TimerStats { uint64_t cycles = 0; uint64_t calls = 0; };
struct Timing { TimerStats stats[T_COUNT]{}; };

inline Timing g_timing{};
inline uint64_t g_game_start = 0;
inline uint64_t g_game_end = 0;

// Scoped timer
struct ScopedTimer {
    TimerID id;
    Timer t;
    bool active;

    FORCEINLINE ScopedTimer(TimerID tid) 
        : id(tid), active(Logging::track_timers) { if (active) t.begin(); }

    FORCEINLINE ~ScopedTimer() {
        if (!active) return;
        g_timing.stats[id].cycles += t.end();
        g_timing.stats[id].calls++;
    }
};

// Header-only logging
inline void logTimingStats(const std::string& fen = "") {
    static std::ofstream out(Logging::log_file_name("timing.jsonl"), std::ios::app);
    if (!out.is_open()) return;

    //const auto& root = g_timing.stats[T_ROOT];
    //double total_ms = double(root.cycles) / Timer::freq() * 1000.0;

    out << "{";
    out << "\"engine_id\":\"" << ENGINE_ID << "\","; 
    out << "\"instance_id\":" << instanceID() << ",";
    out << "\"type\":\"timing\",";
    out << "\"session\":" << currentSession() << ",";
    out << "\"game_uuid\":\"" << g_run_context.game_uuid << "\",";
    out << "\"search_uuid\":\"" << g_run_context.search_uuid << "\",";
    out << "\"fen\":\"" << fen;
    //out << "\"total_search_time_ms\":" << total_ms;

    for (int i = 0; i < T_COUNT; ++i) {
        const auto& ts = g_timing.stats[i];
        double ms = double(ts.cycles) / Timer::freq() * 1000.0;
        //double pct = total_ms > 0 ? 100.0 * ms / total_ms : 0.0;
        double avg_ms = ts.calls ? ms / ts.calls : 0.0;

        out << ",\"" << TimerNames[i] << "\":{"
            << "\"total_ms\":" << ms << ","
            << "\"calls\":" << ts.calls << ","
            << "\"avg_ms\":" << avg_ms 
            //<< "\"pct_of_root_ms\":" << pct
            << "}";
    }

    out << "}\n";
    out.flush();
}

#endif
