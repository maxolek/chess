#ifndef GAME_H
#define GAME_H

#include "helpers.h"
#include "board.h"
#include "move.h"
#include "engine.h"

class Game {
public:
    bool userIsWhite;
    Game();

    void start(); // Main loop for playing a game
    void userMove(const std::string& moveStr); // Process a move from user input
    void engineMove(); // Let engine choose a move
    void undoMove(); // Undo last move
    void printBoard(); // Display board
    bool isGameOver() const; // Check for checkmate, stalemate, etc.

private:
    Board board;
    PrecomputedMoveData precomp = PrecomputedMoveData();

    void displayResult();
    Move parseMove(const std::string& moveStr) const;
};

#endif // GAME_H
