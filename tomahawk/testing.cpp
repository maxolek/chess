#include "moveGenerator.h"
#include "board.h"
#include "arbiter.h"
#include "tests.h"
#include <iostream>


int main(int argc, char* argv[]) {
    Tests tests = Tests();
    // precomp must always be initialized for movegenerator+board to work
    PrecomputedMoveData precomp = PrecomputedMoveData();

    if (argc == 2){
        tests.zobristTest();
        return 0;
    }

    if (argc < 9) {
        std::cerr << "Usage: ./testing [perft|divide] <fen> <depth>\n";
        return 1;
    }

    std::string mode = argv[1];
    // Rebuild FEN from argv[2] to argv[argc - 2]
    std::string fen = argv[2];
    for (int i = 3; i < argc - 1; ++i) {
        fen += " " + std::string(argv[i]);
    }
    int depth = std::stoi(argv[argc - 1]);

    if (mode == "perft") {
        std::cout << tests.perft(depth, fen) << "\n";  // perft should return a node count
    } else if (mode == "divide") {
        tests.perftDivide(depth, fen);  // divide should print moves + counts
    } else {
        std::cerr << "Unknown mode: " << mode << "\n";
        return 1;
    }

    return 0;
}


