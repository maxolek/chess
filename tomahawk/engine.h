// engine.h
#ifndef ENGINE_H
#define ENGINE_H

#include "board.h"
#include "magics.h"
#include "moveGenerator.h"
#include "searcher.h"
#include "evaluator.h"
#include "stats.h"


// ---------------------
// -- Engine Settings --
// ---------------------

struct UCIOptions {
    std::string syzygyPath = "";
    int hashSize = 16;
    int threads = 1;
    bool ponder = false;
    int moveOverhead = 30;
    int multiPV = 1;
    int skillLevel = 20;
    int contempt = 0;
    bool uciShowWDL = false;
    bool showStats = false;
};

struct SearchSettings {
    int depth = 0;         // max depth (0 = no limit)
    int nodes = 0;         // max nodes
    int movetime = 0;      // fixed search time (ms)
    int mate = 0;          // search for mate in N
    int wtime = 0;         // white time left (ms)
    int btime = 0;         // black time left (ms)
    int winc = 0;          // white increment (ms)
    int binc = 0;          // black increment (ms)
    int movestogo = 0;     // moves to next time control
    bool infinite = false; // search until "stop"
    bool ponder = false;   // pondering search
};


// ------------------
// -- Engine Class --
// ------------------

class Engine {
private:
    PrecomputedMoveData precomp = PrecomputedMoveData();
    Move ponderMove = Move::NullMove();
    int search_depth = 0;
    int time_left[2] = {0, 0};          // [white, black] time left
    int increment[2] = {0, 0};          // [white, black] increment
    SearchLimits limits;
    UCIOptions options;

    // iteration-local eval table
    static constexpr int INVALID = MATE_SCORE + 10000;
    int last_eval_table[1 << 16];   // keyed by Move.Value()
    // book keeping
    void store_last_result(const SearchResult& res);
    int get_prev_eval(Move m) const;

public:
    explicit Engine(Board* _board);

    std::unique_ptr<MoveGenerator> movegen;
    Board* game_board;        // main game board
    Board search_board;       // modifable copy of game board
    // movegen for current move
    int legal_move_count = 0;
    Move legal_moves[MAX_MOVES];
    // output
    Move bestMove = Move::NullMove();
    int bestEval = -MATE_SCORE;
    std::vector<Move> pv_line;

    bool pondering = false;

    Evaluator evaluator;                // preload PST tables, eval

    SearchStats stats;

    // --- State ---
    void clearState();

    // --- UCI Handlers ---
    void setOption(const std::string& name, const std::string& value);
    void setPosition(const std::string& fen, const std::vector<Move>& moves);
    void playMoves(const std::vector<Move>& moves);
    void playMovesStrings(const std::vector<std::string>& moves);
    void ponderHit();

    // --- Search Helpers ---
    void startSearch(const SearchSettings& settings);
    void stopSearch();

    // --- Search ---
    void computeSearchTime(const SearchSettings& settings);
    void iterativeDeepening();
    Move getBestMove(Board* board); // returns best move
    void evaluate_position(SearchSettings settings); // for testing

    // --- Communication ---
    void sendBestMove(Move bestMove, Move ponder = Move::NullMove());
    void logStats(const std::string& fen) const;
};

#endif // ENGINE_H
