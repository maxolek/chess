#include "UCI.h"
#include <vector>
#include <string>
#include <iostream>

UCI::UCI(Engine& eng) {
    engine=&eng;
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

    if (token == "uci") {
            std::cout << "id name tomahawk\n";
            std::cout << "id author maxolek\n";
            // engine options
            std::cout << "option name Move Overhead type spin default 30 min 0 max 1000\n";
            std::cout << "option name Hash type spin default 16 min 1 max 1024\n";
            std::cout << "option name Threads type spin default 4 min 1 max 128\n";
            std::cout << "option name Ponder type check default false\n";
            // stats tracking
            std::cout << "option name ShowStats type check default false\n";
            // Required for lichess
            std::cout << "option name UCI_ShowWDL type check default false" << std::endl;
            std::cout << "uciok\n";
    }
    else if (token == "print_board") {
        engine->game_board->print_board();
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

// --- handle "setoption" ---
void UCI::handleSetOption(std::istringstream& iss) {
    std::string token;
    std::string name;
    std::string value;

    // First token should be "name"
    iss >> token;
    if (token != "name") {
        return; // invalid format
    }

    // Read option name until "value" or end
    while (iss >> token) {
        if (token == "value") {
            // Everything after "value" is the value string
            std::getline(iss, value);
            // Trim leading whitespace
            size_t first = value.find_first_not_of(" \t");
            if (first != std::string::npos) {
                value = value.substr(first);
            } else {
                value.clear();
            }
            break;
        } else {
            if (!name.empty()) name += " ";
            name += token;
        }
    }

    // Pass parsed option to engine
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

    SearchSettings settings;
    bool eval_test = false;

    while (iss >> token) {
        if (token == "eval_test") eval_test = true;
        else if (token == "wtime") iss >> wtime;
        else if (token == "btime") iss >> btime;
        else if (token == "winc") iss >> winc;
        else if (token == "binc") iss >> binc;
        else if (token == "movestogo") iss >> movestogo;
        else if (token == "depth") iss >> depth;
        else if (token == "nodes") iss >> nodes;
        else if (token == "movetime") iss >> movetime;
        else if (token == "infinite") infinite = true;
    }

    settings.wtime = wtime;
    settings.btime = btime;
    settings.winc = winc;
    settings.binc = binc;
    settings.movestogo = movestogo;
    settings.depth = depth;
    settings.nodes = nodes;
    settings.movetime = movetime;
    settings.infinite = infinite;

    if (eval_test) {engine->evaluate_position(settings);}
    else {engine->startSearch(settings);}
}
