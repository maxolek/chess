#pragma once
#include <string>
#include "helpers.h"
#include <filesystem>

struct RunContext {
    std::string game_uuid; // updates when a new game starts
    std::string search_uuid; // updates every search
};
inline RunContext g_run_context{};

struct Logging {
    // ---- toggles ----
    static inline bool track_timers        = false;
    static inline bool track_search_stats  = true;
    static inline bool track_game_log      = true;
    static inline bool track_uci           = true;

    // ---- directory ----
    // uci is .log, rest are .jsonl
    static inline std::string DEFAULT_LOG_DIR = "../logs/test_logs";
    static inline std::string log_dir = DEFAULT_LOG_DIR;
    static inline std::ofstream uci_file;
    static inline std::ofstream search_file;
    static inline std::ofstream game_file; 
    static inline std::ofstream timing_file;

    // life cycle
    static void initFiles() {
        std::filesystem::create_directories(log_dir);

        if (track_uci)
            uci_file.open(log_dir + "/uci.log", std::ios::app);
        if (track_search_stats)
            search_file.open(log_dir + "/search.jsonl", std::ios::app);
        if (track_game_log)
            game_file.open(log_dir + "/game.jsonl", std::ios::app);
        if (track_timers)
            timing_file.open(log_dir + "/timing.jsonl", std::ios::app);
    }

    static void closeFiles() {
        if (uci_file.is_open())     uci_file.close();
        if (search_file.is_open())  search_file.close();
        if (game_file.is_open())    game_file.close();
        if (timing_file.is_open())  timing_file.close();
    }

    static void reopenFiles() {
        closeFiles();
        initFiles();
    }

    // uci handlers
    static inline void logUCIin(const std::string& line) {
        if (!Logging::track_uci) return;
        Logging::uci_file << "< " << line << "\n";
    }
    static inline void logUCIout(const std::string& line) {
        if (!Logging::track_uci) return;
        Logging::uci_file << " > " << line << "\n";
    }

    // ---- setters (called from setoption) ----
    static void setLogDir(const std::string& path) {
        if (log_dir == path) return;
        log_dir = path;
        reopenFiles();
    }

    static void setTrackTimers(bool v) {
        if (track_timers == v) return;
        track_timers = v;
        reopenFiles();
    }

    static void setTrackSearchStats(bool v) {
        if (track_search_stats == v) return;
        track_search_stats = v;
        reopenFiles();
    }

    static void setTrackGameLog(bool v) {
        if (track_game_log == v) return;
        track_game_log = v;
        reopenFiles();
    }

    static void setTrackUCI(bool v) {
        if (track_uci == v) return;
        track_uci = v;
        reopenFiles();
    }

    static inline void disableAll() {
        track_timers        = false;
        track_search_stats  = false;
        track_game_log      = false;
        track_uci           = true;
    }

    static inline void enableAll() {
        track_timers        = true;
        track_search_stats  = true;
        track_game_log      = true;
        track_uci           = true;
    }
};