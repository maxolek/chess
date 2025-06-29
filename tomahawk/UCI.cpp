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
        }
    }
}

void UCI::handleCommand(const std::string& line) {
    std::istringstream iss(line);
    std::string token;
    iss >> token;

    if (token == "uci") {
        std::cout << "Tomahawk v0\n";
        std::cout << "Max Oleksa\n";
        // Add option descriptions here if needed
        std::cout << "uciok\n";
    }
    else if (token == "isready") {
        std::cout << "readyok\n";
    }
    else if (token == "setoption") {
        handleSetOption(iss);
    }
    else if (token == "ucinewgame") {
        //engine->newGame();
        game->start();
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
    else if (token == "ponderhit") {
        engine->ponderHit();
    }
    else if (token == "quit") {
        engine->stopSearch();
    }
    // else ignore unknown commands or add more handlers
}

void UCI::handleSetOption(std::istringstream& iss) {
    std::string word;
    std::string name, value;

    iss >> word; // expect "name"
    if (word == "name") {
        std::string part;
        while (iss >> part && part != "value") {
            if (!name.empty()) name += " ";
            name += part;
        }
        // read rest as value
        std::getline(iss, value);
        if (!value.empty()) {
            size_t first = value.find_first_not_of(" \t");
            if (first != std::string::npos)
                value = value.substr(first);
        }
        engine->setOption(name, value);
    }
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
