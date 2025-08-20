#ifndef GAME_H
#define GAME_H

#include "helpers.h"
#include "board.h"
#include "move.h"
#include "engine.h"

class Game {
public:
    bool userIsWhite;
    Board board;
    //MoveGenerator* movegen;
    Game();

    void start(Engine* eng); // Main loop for playing a game
    void startUCI(Engine* eng); // still main loop, limit output for UCI compatibility
    void userMove(const std::string& moveStr); // Process a move from user input
    void engineMove(Engine* eng); // Let engine choose a move
    void engineMoveUCI(Engine* eng); // cut output for UCI compatibility
    void undoMove(); // Undo last move
    void printBoard(); // Display board
    bool isGameOver() const; // Check for checkmate, stalemate, etc.

private:
    //PrecomputedMoveData precomp = PrecomputedMoveData();

    void displayResult();
    Move parseMove(const std::string& moveStr) const;
};

#endif // GAME_H
