#ifndef SESSION_H
#define SESSION_H

#include <atomic>
#include <cstdint>

inline std::atomic<uint64_t> g_session_id{0};

inline uint64_t startNewSession() {
    static std::atomic<uint64_t> next{1};
    g_session_id = next.fetch_add(1, std::memory_order_relaxed);
    return g_session_id.load(std::memory_order_relaxed);
}

inline uint64_t currentSession() {
    return g_session_id.load(std::memory_order_relaxed);
}

#endif
