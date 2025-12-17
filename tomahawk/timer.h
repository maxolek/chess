#ifndef TIMER_H
#define TIMER_H

#include "helpers.h"
#include "move.h"
#include "session.h"
#include <vector>
#include <windows.h>
#include <fstream>

// Timer IDs
enum TimerID {
    T_MOVEGEN, T_MAKEMOVE, T_UNMAKE_MOVE,
    T_ROOT, T_SEARCH, T_QSEARCH,  
    T_SCORE_ORDER, T_TT_PROBE, T_TT_STORE, 
    T_EVAL, T_NNUE, 
    T_SEE, T_PERFT,
    T_BOOK_PROBE, 
    T_COUNT
};

// C++17 inline variable avoids multiple definitions
inline const char* TimerNames[T_COUNT] = {
    "MOVEGEN", "MAKEMOVE", "UNMAKE_MOVE",
    "ROOT", "SEARCH", "QSEARCH",
    "SCORE_ORDER", "TT_PROBE", "TT_STORE",
    "EVAL", "NNUE",
    "SEE", "PERFT",
    "BOOK_PROBE"
};

// Timer structs
struct Timer {
    LARGE_INTEGER start;

    __forceinline void begin() { QueryPerformanceCounter(&start); }

    __forceinline uint64_t end() const {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return now.QuadPart - start.QuadPart;
    }

    static uint64_t freq() {
        static uint64_t f = []{
            LARGE_INTEGER li;
            QueryPerformanceFrequency(&li);
            return li.QuadPart;
        }();
        return f;
    }
};

struct TimerStats { uint64_t cycles = 0; uint64_t calls = 0; };
struct Timing { TimerStats stats[T_COUNT]{}; };
inline Timing g_timing{};

// Scoped timer
struct ScopedTimer {
    TimerID id;
    Timer t;
    bool active;

    __forceinline ScopedTimer(TimerID tid) 
        : id(tid), active(Logging::track_timers) { if (active) t.begin(); }

    __forceinline ~ScopedTimer() {
        if (!active) return;
        g_timing.stats[id].cycles += t.end();
        g_timing.stats[id].calls++;
    }
};

// Header-only logging
inline void logTimingStats() {
    static std::ofstream out(Logging::log_dir + "/timing.jsonl", std::ios::app);
    if (!out.is_open()) return;

    const auto& root = g_timing.stats[T_ROOT];
    double total_ms = double(root.cycles) / Timer::freq() * 1000.0;

    out << "{";
    out << "\"engine_id\":" << ENGINE_ID << ",";
    out << "\"type\":\"timing\",";
    out << "\"session\":" << currentSession() << ",";
    out << "\"total_search_time_ms\":" << total_ms;

    for (int i = 0; i < T_COUNT; ++i) {
        const auto& ts = g_timing.stats[i];
        double ms = double(ts.cycles) / Timer::freq() * 1000.0;
        double pct = total_ms > 0 ? 100.0 * ms / total_ms : 0.0;
        double avg_ms = ts.calls ? ms / ts.calls : 0.0;

        out << ",\"" << TimerNames[i] << "\":{"
            << "\"total_ms\":" << ms << ","
            << "\"calls\":" << ts.calls << ","
            << "\"avg_ms\":" << avg_ms << ","
            << "\"pct_of_root_ms\":" << pct
            << "}";
    }

    out << "}\n";
    out.flush();
}


#endif
