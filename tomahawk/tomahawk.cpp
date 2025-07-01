#include "uci.h"
#include "engine.h"
#include "game.h"
#include <iostream>
#include <atomic>
#include <thread>

std::atomic<bool> keepListening(true);
void listenLoop(UCI& uci) {
    std::string line;
    while (keepListening) {
        if (!std::getline(std::cin, line)) {
            // End of input or error, stop listening
            keepListening = false;
            break;
        }
        // Process the input line here
        uci.handleCommand(line);

        if (line == "quit") {
            keepListening = false;
            break;
        }
    }
}

int main() {
    Game game = Game();
    Engine engine = Engine(&game.board);
    UCI uci(engine, game);

    std::thread listener([&uci](){
        uci.loop(); // loop internally
    });
    listener.join();

    return 0;
}
