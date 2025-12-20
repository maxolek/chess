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

    if (Logging::track_uci) {
        static std::ofstream logFile("../logs/uci_log.log", std::ios::app);
        if (!logFile.is_open()) return;
        logFile << line << "\n";
        logFile.flush();
    }

    // standard commands
    if (token == "uci") {
            std::cout << "id name tomahawk\n";
            std::cout << "id version " << ENGINE_ID << "\n";
            std::cout << "id author max oleksa\n";

            // engine options
            std::cout << "option name Move Overhead type spin default 30 min 0 max 1000\n";
            std::cout << "option name Threads type spin default 1 min 1 max 1\n";
            std::cout << "option name Hash type spin default 512 min 1 max 1024\n";
            std::cout << "option name Ponder type check default false\n";
            // stats tracking
            std::cout << "option name ShowStats type check default false\n";
            // Required for lichess
            std::cout << "option name UCI_ShowWDL type check default false\n";
            /*
            // -------- customization --------
            std::cout << "option name MagicBitboards type check default true\n";
            std::cout << "option name NNUE type check default true\n";
            // search
            std::cout << "option name quiescence type check default true\n";
            std::cout << "option name aspiration type check default true\n";
            // move ordering
            std::cout << "option name moveordering type check default true\n";
            std::cout << "option name mvvlva_ordering type check default false\n";
            std::cout << "option name see_ordering type check default true\n";
            // pruning
            std::cout << "option name delta_pruning type check default true\n";
            std::cout << "option name see_pruning type check default true\n";
            // books
            std::cout << "option name nnue_weight_file type string default 768_128x2\n";
            */
            std::cout << "option name opening_book type string default Titans\n";
            std::cout << "option name syzygy type string default <empty>\n";
            // logging
            /*
            std::cout << "option name timer_logging type check default false\n";
            std::cout << "option name stats_logging type check default true\n";
            std::cout << "option name game_logging type check default true\n";
            */

            std::cout << "uciok\n";
    }
    else if (token == "isready") {
        std::cout << "readyok\n";
    }
    else if (token == "setoption") {
        handleSetOption(iss);
    }
    else if (token == "ucinewgame") {
        engine->newGame();
    }
    else if (token == "position") { // start all search/tests/everything after set_position
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
    else if (token == "quit") {
        engine->stopSearch();
    }
    else if (token == "ponderhit") {
        //
    }
    // custom commands
    else if (token == "static_eval") {
        // static or tapered evaluation test
        engine->staticEvalTest();
    }
    else if (token == "nnue_eval") {
        engine->nnueEvalTest();
    }
    else if (token == "perft") {
        int depth = 0;
        if (iss >> depth) {
            engine->perftPrint(depth);
        }
    }
    else if (token == "see") {
        std::string target_sq;
        if (iss >> target_sq) {
            engine->SEETest(algebraic_to_square(target_sq));
        }
    }
    else if (token == "dumpzobrist") {
        std::cout << "\nCurrent Hash: 0x" << std::hex 
          << engine->game_board.zobrist_hash << "\n";
        std::cout << "\nLast 10 hashes" << std::endl;
        for (int i = 0; i < std::min(10,(int)engine->game_board.zobrist_history.size()); i++) {
            const auto& elem = engine->game_board.zobrist_history[engine->game_board.zobrist_history.size() - 1 - i];
            std::cout << elem << "\n";
        }
        std::cout << "\n--- Rep Stack ---\n";

        int sum = 0;
        for (const auto& entry : engine->game_board.hash_history) {
            uint64_t hash = entry.first;
            int count = entry.second;
            std::cout << "Hash: 0x" << std::hex << hash 
                    << "  Count: " << std::dec << count << "\n";
            sum += count;
        }
        std::cout << "--- End of Hash History ---\n";
        std::cout << "allgamemoves.size: " << engine->game_board.allGameMoves.size() << "\tzobrist_history.sum: " << sum << "\tzobrist_vec.size: " << engine->search_board.zobrist_history.size() << std::endl;
    }
    else if (token == "dumpstats") {
        //engine->dumpStats(); // you implement
    }
    else if (token == "bench") {
        //engine->bench(depth);  // implement bench mode
    }
    else if (token == "speedtest") {
        //engine->speedTest(); // implement speed test
    }
    else if (token == "flip") {
        engine->search_board.is_white_move = !engine->search_board.is_white_move;
    }
    else if (token == "print") {
        engine->print_info();
    } else if (token == "print_board") {
        engine->game_board.print_board();
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


void UCI::handlePosition(std::istringstream& iss)
{
    std::string token;
    iss >> token;

    std::string fen;
    std::vector<std::string> moves;

    if (token == "startpos") {
        fen = "startpos";
    }
    else if (token == "fen") {
        std::string part;
        for (int i = 0; i < 6; ++i) {
            iss >> part;
            fen += part + " ";
        }
        fen.pop_back();
    }

    if (iss >> token && token == "moves") {
        std::string m;
        while (iss >> m)
            moves.push_back(m);
    }

    engine->setPosition(fen, moves);
}

void UCI::handleGo(std::istringstream& iss) {
    // Reset defaults
    sendEval = false;
    wtime = btime = 0;
    winc = binc = 0;
    movetime = -1;
    depth = -1;

    std::string token;
    int movestogo = 0;
    int nodes = -1;
    bool infinite = false;

    while (iss >> token) {
        if (token == "eval") {
            sendEval = true;            // mark that we want evaluation only
        }
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

    // Apply settings to engine
    engine->settings.wtime = wtime;
    engine->settings.btime = btime;
    engine->settings.winc = winc;
    engine->settings.binc = binc;
    engine->settings.movestogo = movestogo;
    engine->settings.depth = depth;
    engine->settings.nodes = nodes;
    engine->settings.movetime = movetime;
    engine->settings.infinite = infinite;

    if (sendEval) {
        engine->evaluate_position();   // just evaluate the current position
        return;
    }

    // Otherwise start a normal search
    engine->trackGame();
    engine->startSearch();
}

