// ---------------------
// -- Data Collection --
// ---------------------

#ifndef GAME_LOG_H
#define GAME_LOG_H

#include "helpers.h"
#include "move.h"
#include "session.h"
#include <vector>


enum class GameResult { ONGOING, WIN, LOSS, DRAW , ABORTED };
enum class GameEndReason {
    NONE,
    CHECKMATE,
    STALEMATE,
    THREEFOLD,
    FIFTY_MOVE,
    TIME,
    RESIGN,
    ADJUDICATION
};

struct GameLog {
    bool finalized = false;

    int nodes, movetime, depth = 0;
    int wtime, btime, winc, binc, movestogo = 0;

    std::string startFEN;
    std::vector<Move> moves;

    GameResult outcome = GameResult::ONGOING;
    GameEndReason reason = GameEndReason::NONE;

    int plies = 0;
    int finalEval = 0;

    uint64_t totalNodes = 0;
    uint64_t totalQNodes = 0;

    double totalTimeMs = 0;
    double avgNPS = 0;

};
// global instance
inline GameLog g_gamelog;

inline void logGameLog() {
    if (!Logging::track_game_log) return;

    static std::ofstream out(Logging::log_dir + "/game.jsonl", std::ios::app);
    if (!out.is_open()) return;

    out << "{"
        << "\"engine_id\":" << ENGINE_ID << ","
        << "\"type\":\"game\","
        << "\"session\":" << currentSession() << ","
        << "\"result\":" << int(g_gamelog.outcome) << ","
        << "\"reason\":" << int(g_gamelog.reason) << ","
        << "\"plies\":" << g_gamelog.plies << ",";

    // moves
    out << "\"moves\":[";
    for (size_t i = 0; i < g_gamelog.moves.size(); ++i) {
        if (i) out << ",";
        out << "\"" << g_gamelog.moves[i].uci() << "\"";
    }
    out << "],";

    out << "\"nodes\":" << g_gamelog.totalNodes << ","
        << "\"qnodes\":" << g_gamelog.totalQNodes << ","
        << "\"time_ms\":" << g_gamelog.totalTimeMs << ","
        << "\"avg_nps\":" << g_gamelog.avgNPS << ","
        << "\"start_fen\":\"" << g_gamelog.startFEN << "\""
        << "}\n";

    out.flush();
}


#endif