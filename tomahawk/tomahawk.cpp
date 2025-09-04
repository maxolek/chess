#include "UCI.h"
#include "engine.h"
#include "board.h"
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
    Board board = Board();
    Engine engine = Engine(&board);
    UCI uci(engine);

    //board.setNNUE(&engine.nnue);

    std::thread listener([&uci](){
        uci.loop(); // loop internally
    });
    listener.join();

    return 0;
}
