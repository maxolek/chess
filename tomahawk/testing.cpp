#include "moveGenerator.h"
#include "board.h"
#include "arbiter.h"
#include "tests.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    Tests tests = Tests();
    PrecomputedMoveData precomp = PrecomputedMoveData();  // must be initialized for movegen+board to work

    if (argc < 2) {
        std::cerr << "Usage: ./testing [zobrist|search|perft|divide] <args>\n";
        return 1;
    }

    std::string mode = argv[1];

    if (mode == "zobrist") {
        tests.zobristTest();
        return 0;
    } 
    else if (mode == "search") {
        std::string fen = argv[2];
        tests.testSearch(fen);
        return 0;
    } 
    else if (mode == "perft" || mode == "divide") {
        if (argc < 9) {
            std::cerr << "Usage: ./testing " << mode << " <fen> <depth>\n";
            std::cerr << "FEN must have 6 space-separated fields\n";
            return 1;
        }

        // Rebuild FEN string from argv[2]..argv[7] (6 fields)
        std::string fen = argv[2];
        for (int i = 3; i <= 7; ++i) {
            fen += " ";
            fen += argv[i];
        }

        int depth = std::stoi(argv[8]);

        if (mode == "perft") {
            int nodes = tests.perft(depth, fen);
            std::cout << "Perft nodes: " << nodes << std::endl;
        } else {
            tests.perftDivide(depth, fen);
        }

        return 0;
    }
    else {
        std::cerr << "Unknown mode: " << mode << "\n";
        return 1;
    }
}
