// ChessEngine.h
#ifndef ENGINE_H
#define ENGINE_H

//#include <jni.h>
#include "board.h"
#include "magics.h"
#include "moveGenerator.h"
#include "arbiter.h"
#include "searcher.h"
#include "msc_ver.h"

struct BookEntry { // opening book
    U64 key;
    uint16_t move;
};

class Engine {
private:
    PrecomputedMoveData precomp = PrecomputedMoveData();
    std::unordered_map<U64, uint16_t> book;
    int polyglotPieceIndex(int piece, bool isWhite);
    U64 polyglotKey(const Board& board);
    void loadOpeningBook(const std::string& filename);
    Move bookMoveFromEncoded(uint16_t m);
    Move getBookMove(const Board& board);
    
    Move ponderMove;
    int search_depth;
    int time_left[2], increment[2]; //white,black
    
public:
    std::unique_ptr<MoveGenerator> movegen;
    Board* game_board;
    Board search_board; // want to modify search board
    Move legal_moves[MAX_MOVES];
    Move bestMove;
    bool pondering = false; bool stop = false;

    // to preload PST tables
    Evaluator evaluator;

    // mobility is stored in engine to avoid recomp of moves
    int whiteMobility = 0; int blackMobility = 0;

    // UCI options
    std::string syzygyPath = "";
    int hashSize = 16;       // default example
    int threads = 1;
    bool ponder = false;
    int moveOverhead = 30;
    int multiPV = 1;
    int skillLevel = 20;
    int contempt = 0;
    bool uciShowWDL = false;

    Engine();
    Engine(Board* _board);
    //~Engine();

    void clearState();

    // uci
    void setOption(const std::string& name, const std::string& value);
    void setPosition(const std::string& fen, const std::vector<Move>& moves);
    void playMoves(const std::vector<Move>& moves);
    void playMovesStrings(const std::vector<std::string>& moves);
    void startSearch(SearchSettings settings);
    void startSearch(int depth = -1, int movetime = -1, int wtime = -1, int btime = -1, int winc = 0, int binc = 0);
    void stopSearch();
    void ponderHit();

    // Communication
    void sendBestMove(Move bestMove, Move ponderMove = Move::NullMove());
    void uciLoop();

    // search-eval qualifications
    int computeSearchTime(SearchSettings settings);

    // best moves
    void iterativeDeepening(SearchSettings settings);
    
    Move getBestMove( Board& board); // move obj
    std::string getBestMoveUCI( Board& board); // uci
    // above are not const args as engine will reference game.cpp
    // and searcher will move on game.board to get best moves
    void processPlayerMove(Move move);
    std::string processEngineMoveString();
    Move processEngineMove();
    std::string getBoardState();
    bool isGameOver() const;


    // logging
    void logSearchDepthInfo(
        int depth, int quiesence_depth, Move bestMove, 
        std::vector<Move> best_line, std::vector<Move> best_quiescence_line, int eval, 
        int elapsed_ms, std::string file_path = "C:/Users/maxol/code/chess/search_depth_eval.txt"
    );
};

/*
#ifdef __cplusplus
extern "C" {
#endif
// Export the function for use in other applications
__declspec(dllexport) const jstring getBestMove();
__declspec(dllexport) const jstring getBoardState(); 
#ifdef __cplusplus
}
#endif
*/

#endif // ENGINE_H
