#include "game.h"

// Constructor initializes board and user color (white by default)
Game::Game() {
    userIsWhite = true;
    board = Board();
}

// Main loop could be implemented outside or here; here is a simple placeholder
void Game::start(Engine* eng) {
    printBoard();

    while (!isGameOver()) {
        if ((board.move_color == white && userIsWhite) ||
            (board.move_color == black && !userIsWhite)) {
            std::string userInput;
            std::cout << "Your move: ";
            std::getline(std::cin, userInput);
            userMove(userInput);
        } else {
            engineMove(eng);
        }
        printBoard();
    }

    displayResult();
}

void Game::startUCI(Engine* eng) {
    /*while (!isGameOver()) {
        if ((board.move_color == white && userIsWhite) ||
            (board.move_color == black && !userIsWhite)) {
            std::string userInput;
            //std::cout << "Your move: ";
            std::getline(std::cin, userInput);
            userMove(userInput);
        } else {
            engineMoveUCI(eng);
        }
    }*/
}

// Process a user move given as a string
void Game::userMove(const std::string& moveStr) {
    if (moveStr == "quit" || moveStr == "kill") {exit(0); return;}

    Move move = parseMove(moveStr);
    /*if (!board.isLegalMove(move)) {
        std::cout << "Illegal move. Try again.\n";
        return;
    }*/
    board.MakeMove(move);
}

// Let the engine choose and make a move
void Game::engineMove(Engine* eng) {
    Move bestMove = eng->getBestMove(board);
    std::cout << "Engine plays: " << bestMove.uci() << "\n";
    board.MakeMove(bestMove);
}

// UCI handles output so dont need any here, just making move
void Game::engineMoveUCI(Engine* eng) {
    Move bestMove = eng->getBestMove(board);
    board.MakeMove(bestMove);
}

// Undo the last move
void Game::undoMove() {
    board.UnmakeMove(board.allGameMoves.back());
}

// Display the current board state
void Game::printBoard() {
    board.print_board();  // Assuming Board has a print() method
}

// Check if game over by checkmate, stalemate, or draw
bool Game::isGameOver() const {
    Result result = Arbiter::GetGameState(&board);
    return result != InProgress;
}

// Display the game result to user
void Game::displayResult() {
    Result result = Arbiter::GetGameState(&board);
    if (Arbiter::isWinResult(result)) {
        if ((board.move_color == white && userIsWhite) ||
            (board.move_color == white && !userIsWhite)) {
            std::cout << "Checkmate! You lose.\n";
        } else {
            std::cout << "Checkmate! You win!\n";
        }
    } else {
        std::cout << "Draw.\n";
        std::cout << results_string[static_cast<int>(result)] << std::endl;
    }
}

// Convert user input string to a Move object
Move Game::parseMove(const std::string& moveStr) const {
    // Basic placeholder: convert UCI string like "e2e4" to Move
    // You need to implement this properly based on your Move and Board class
    return Move(moveStr);
}
