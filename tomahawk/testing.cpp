#include "moveGenerator.h"
#include "board.h"
#include "arbiter.h"
#include "tests.h"
#include <iostream>

// first set of moves is looking good
// now, test for 1. check 2. pins 3. castling 4. enpassant 
// then run movegencount test


int main(int argc, char* argv[]) {
    Tests tests = Tests();
    // precomp must always be initialized for movegenerator to work
    PrecomputedMoveData precomp = PrecomputedMoveData();

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
    //int depth = 2;

    //std::string mode = argv[1];
    //std::string fen = argv[2];
    //int depth = std::stoi(argv[3]);
    //std::cout << "argc: " << argc << std::endl;
    //std::cout << "fen: " << fen << std::endl;
    //std::cout << i << std::endl;
    //std::cout << "hard coded last arg: " << argv[8] << std::endl;
    //std::cout << "last arg: " << argv[argc - 1] << std::endl;
    //std::cout << "mode: " << mode << std::endl;
    //std::cout << "fen: " << fen << std::endl;
    //std::cout << "depth: " << depth << std::endl;
    //std::cout << mode << "\t" << fen << "\t" << depth << std::endl;

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


