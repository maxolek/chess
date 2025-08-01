#include "uci.h"
#include <vector>
#include <string>
#include <iostream>

UCI::UCI(Engine& eng, Game& gm) {
    engine=&eng; game=&gm;
}  

void UCI::loop() {
    std::string line;
    bool isRunning = true;

    while (isRunning && std::getline(std::cin, line)) {
        if (line.empty()) continue;

        handleCommand(line);

        if (line == "quit") {
            isRunning = false;
            exit(0);
        }
    }
}

void UCI::handleCommand(const std::string& line) {
    std::istringstream iss(line);
    std::string token;
    iss >> token;

    std::ofstream file("C:/Users/maxol/code/chess/uci_interactions.txt", std::ios::app); // append mode = std::ios::app
    file << iss.str() << std::endl;
    file.close();

    if (token == "uci") {
            std::cout << "id name tomahawk\n";
            std::cout << "id author maxolek\n";
            std::cout << "option name Move Overhead type spin default 30 min 0 max 1000\n";
            std::cout << "option name Hash type spin default 16 min 1 max 1024\n";
            std::cout << "option name Threads type spin default 4 min 1 max 128\n";
            std::cout << "option name Ponder type check default false\n";
            // Required for lichess
            std::cout << "option name UCI_ShowWDL type check default false" << std::endl;
            std::cout << "uciok\n";
    }
    else if (token == "print_board") {
        game->printBoard();
    }
    else if (token == "isready") {
        std::cout << "readyok\n";
    }
    else if (token == "setoption") {
        handleSetOption(iss);
    }
    else if (token == "ucinewgame") {
        engine->clearState();
        //game->startUCI(engine);
    }
    else if (token == "position") {
        handlePosition(iss);
    }
    else if (token == "go") {
        handleGo(iss);
    }
    else if (token == "stop") {
        engine->stopSearch();
        std::string best = engine->bestMove.uci();
        std::cout << "bestmove " << best << "\n";
    }
    //else if (token == "ponderhit") {
    //    engine->ponderHit();
    //}
    else if (token == "quit") {
        engine->stopSearch();
        exit(0);
    }
    // else ignore unknown commands or add more handlers
}

void UCI::handleSetOption(std::istringstream& iss) {
    std::string token;
    std::string name;
    std::string value;

    // Read tokens until "value" or end
    // First token should be "name"
    iss >> token;
    if (token != "name") {
        // Invalid command format, ignore or handle error
        return;
    }

    // Read option name until "value" or end of stream
    while (iss >> token) {
        if (token == "value") {
            // Now read the rest as value
            std::getline(iss, value);
            // Trim leading spaces from value
            size_t first = value.find_first_not_of(" \t");
            if (first != std::string::npos) {
                value = value.substr(first);
            } else {
                value.clear(); // no value after "value"
            }
            break;
        } else {
            if (!name.empty()) name += " ";
            name += token;
        }
    }

    // If no "value" token, value remains empty string

    // Now pass to your engine's option setter
    engine->setOption(name, value);
}


void UCI::handlePosition(std::istringstream& iss) {
    std::string posType;
    iss >> posType;

    if (posType == "startpos") {
        engine->setPosition("startpos",{});
        std::string nextWord;
        iss >> nextWord;
        if (nextWord == "moves") {
            std::vector<std::string> moves;
            std::string moveStr;
            while (iss >> moveStr) moves.push_back(moveStr);
            engine->playMovesStrings(moves);
        }
    }
    else if (posType == "fen") {
        std::string fen;
        std::string part;
        for (int i = 0; i < 6; ++i) {
            iss >> part;
            fen += part + " ";
        }
        fen.pop_back();
        engine->setPosition(fen,{});

        std::string nextWord;
        iss >> nextWord;
        if (nextWord == "moves") {
            std::vector<std::string> moves;
            std::string moveStr;
            while (iss >> moveStr) moves.push_back(moveStr);
            engine->playMovesStrings(moves);
        }
    }
}

void UCI::handleGo(std::istringstream& iss) {
    // Reset to defaults
    wtime = btime = 0;
    winc = binc = 0;
    movetime = -1;
    depth = -1;

    std::string token;
    int movestogo = 0;
    int nodes = -1;
    bool infinite = false;

    while (iss >> token) {
        if (token == "wtime") iss >> wtime;
        else if (token == "btime") iss >> btime;
        else if (token == "winc") iss >> winc;
        else if (token == "binc") iss >> binc;
        else if (token == "movestogo") iss >> movestogo;
        else if (token == "depth") iss >> depth;
        else if (token == "nodes") iss >> nodes;
        else if (token == "movetime") iss >> movetime;
        else if (token == "infinite") infinite = true;
    }

    SearchSettings settings;
    settings.wtime = wtime;
    settings.btime = btime;
    settings.winc = winc;
    settings.binc = binc;
    settings.movestogo = movestogo;
    settings.depth = depth;
    settings.nodes = nodes;
    settings.movetime = movetime;
    settings.infinite = infinite;

    engine->startSearch(settings);

}
