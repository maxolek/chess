// engine.h
#ifndef ENGINE_H
#define ENGINE_H

#include "board.h"
#include "magics.h"
#include "moveGenerator.h"
#include "evaluator.h"
#include "searcher.h"
#include "tt.h"
#include "search_limits.h"
#include "stats.h"
#include "timer.h"
#include "game_log.h"
#include "NNUE.h"
#include "book.h"

class Searcher;
struct SearchLimits;
struct SearchResult;


// ---------------------
// -- Engine Settings --
// ---------------------

// CORE

enum class EngineMode {
    ANALYSIS,
    GAME
};

struct UCIOptions {
    // regular options
    int moveOverhead = 30;
    int threads = 1;
    int hashSize = 512;
    bool ponder = false;
    // stats
    bool uciShowWDL = false;
    bool showStats = true;
    // customization
    bool magics = true;
    bool nnue = true;
    // search
    bool quiescence = true;
    bool aspiration = true;
    bool scorched_earth = true;
    // move ordering
    bool moveordering = true;
    bool mvvlva = true;
    bool see = true;
    // pruning
    bool delta_pruning = true;
    bool see_pruning = true;
    // books
    std::string opening_pst_path = "../bin/pst_opening.txt";
    std::string endgame_pst_path = "../bin/pst_endgame.txt";
    std::string nnue_weight_path = "../bin/nnue_wgts/768_128x2.bin";
    std::string opening_book_path = "../bin/Titans.bin";
    std::string syzygyPath = "";
};

// GAMES

enum class EngineSide {
    WHITE,
    BLACK,
    UNKNOWN
};

struct GameTracker {
    GameResult result;
    GameEndReason reason;
    std::vector<Move> playedMoves;
    std::vector<int> evals;
    std::vector<Move> bestMoves;
    uint64_t lastPositionHash = 0;
    bool active = false;
};

// SEARCH

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
    bool send_eval = false; // send evaluation instead of move
};


// ------------------
// -- Engine Class --
// ------------------

class Engine {
private:
public:
    // precomp data
    PolyglotBook book;
    
    // search info
    int search_depth = 0;
    int time_left[2] = {0, 0};          // [white, black] time left
    int increment[2] = {0, 0};          // [white, black] increment
    SearchSettings settings;
    SearchLimits limits;
    UCIOptions options;

    // iteration-local eval table
    static constexpr int INVALID = MATE_SCORE + 10000;
    int last_eval_table[1 << 16];   // keyed by Move.Value()
    // book keeping
    void store_last_result(const SearchResult& res);
    int get_prev_eval(Move m) const;

    // constructors
    explicit Engine();

    // search + eval
    std::unique_ptr<Searcher> searcher;
    Evaluator evaluator;                // preload PST tables, eval
    TranspositionTable tt; // outside searcher for future multi-thread

    // boards
    Board game_board;        // main game board
    Board search_board;       // modifable copy of game board for searcher
    bool game_over = false;

    // movegen for current move
    std::unique_ptr<MoveGenerator> movegen;
    int legal_move_count = 0;
    Move legal_moves[MAX_MOVES];

    // output
    Move bestMove = Move::NullMove();
    int bestEval = -MATE_SCORE;
    std::vector<Move> pv_line;

    // think on opponent time
    bool pondering = false;
    Move ponderMove = Move::NullMove();

    // --- State ---
    void clearState();

    // --- UCI Handlers ---
    void setOption(const std::string& name, const std::string& value);
    void setPosition(const std::string& fen, const std::vector<std::string>& uci_moves);
    void ponderHit();
    void print_info();

    // --- Search Helpers ---
    void startSearch();
    void stopSearch();

    // --- Search ---
    void computeSearchTime(const SearchSettings& settings); // defines the settings for it_dp
    void iterativeDeepening(); // main search
    void evaluate_position(); // for testing
    // --- Search Result ---
    void sendBestMove(Move bestMove, Move ponder = Move::NullMove()); // output of it_dp

    // --- Games ---
    void newGame();
    bool checkGameEnd();
    void trackGame();
    void finalizeGameLog();
    // --- Results ---
    bool isCheckmate();
    bool isStalemate();
    bool isThreefold();

    // --- Tests ---
    uint64_t perft(int depth);
    void perftPrint(int depth); // same as perft but print instead of return
    void perftDivide(int depth);
    void SEETest(int capture_square);
    void staticEvalTest();
    void nnueEvalTest();
    void moveOrderingTest(int depth);
};

#endif // ENGINE_H
