#include "game.h"
#include <iostream>

int main() {
    std::cout << "=== Welcome to Tomahawk Chess Engine ===\n";

    char choice;
    do {
        std::cout << "Do you want to play as White or Black? (w/b): ";
        std::cin >> choice;
        choice = std::tolower(choice);
    } while (choice != 'w' && choice != 'b');

    // Create game and set player color
    Game game;
    game.userIsWhite = (choice == 'w');

    std::cin.ignore(); // Clear leftover newline from input buffer
    game.start();      // Start the game loop

    return 0;
}
