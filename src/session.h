#ifndef SESSION_H
#define SESSION_H

#include <atomic>
#include <cstdint>

#ifdef _WIN32
    #include <windows.h>
#else   
    #include <unistd.h>
#endif

// -------- process wide identifiers --------
// immutable per-process identifier (pid)
inline uint32_t g_instance_id;
// increaments per game/session
inline std::atomic<uint64_t> g_session_id{0};

// -------- Inits --------
// call ONCE at engine startup
inline void initInstance() {
    #ifdef _WIN32
        g_instance_id = static_cast<uint32_t>(GetCurrentProcessId());
    #else 
        g_instance_id = static_cast<uint32_t>(getpid());
    #endif
}
// call at start of new games
inline uint64_t startNewSession() {
    static std::atomic<uint64_t> next{1};
    g_session_id = next.fetch_add(1, std::memory_order_relaxed);
    return g_session_id.load(std::memory_order_relaxed);
}

// ------ Getters ------
inline uint64_t currentSession() {
    return g_session_id.load(std::memory_order_relaxed);
}

inline uint32_t instanceID() {
    return g_instance_id;
}

#endif
