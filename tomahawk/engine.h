// ChessEngine.h
#ifndef ENGINE_H
#define ENGINE_H

//#include <jni.h>
#include "board.h"
#include "moveGenerator.h"
#include "arbiter.h"
#include "Searcher.h"

class Engine {
private:
    PrecomputedMoveData precomp = PrecomputedMoveData();
    std::unique_ptr<MoveGenerator> movegen;
    Move ponderMove;
    int search_depth;
    int time_left[2], increment[2]; //white,black

    // internal helpers
    void iterativeDeepening();

public:
    Board* game_board;
    Board search_board; // want to modify search board
    Move bestMove;
    std::vector<Move> legal_moves;
    bool pondering = false; bool stop = false;

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

    // best moves
    Move getBestMove( Board& board); // move obj
    std::string getBestMoveUCI( Board& board); // uci
    // above are not const args as engine will reference game.cpp
    // and searcher will move on game.board to get best moves
    void processPlayerMove(Move move);
    std::string processEngineMoveString();
    Move processEngineMove();
    std::string getBoardState();
    bool isGameOver() const;
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
