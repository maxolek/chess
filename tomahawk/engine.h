// ChessEngine.h
#ifndef ENGINE_H
#define ENGINE_H

#include <jni.h>
#include "board.h"
#include "moveGenerator.h"
#include "arbiter.h"
#include "Searcher.h"

class Engine {
private:
    Board board;
    bool isWhiteTurn;
    MoveGenerator movegen;
    std::vector<Move> legal_moves;
public:
    Engine();
    Engine(Board _board);
    //~Engine();

    void initializeGame();

    void processPlayerMove(Move move);
    std::string processEngineMove();
    std::string getBoardState();
    bool isGameOver() const;
};


#ifdef __cplusplus
extern "C" {
#endif
// Export the function for use in other applications
__declspec(dllexport) const jstring getBestMove();
__declspec(dllexport) const jstring getBoardState(); 
#ifdef __cplusplus
}
#endif

#endif // ENGINE_H
