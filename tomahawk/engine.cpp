// Engine.cpp
#include "engine.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>

Engine::Engine() {
    //board = &Board();
    movegen = std::make_unique<MoveGenerator>(&search_board);
    movegen->generateMoves(game_board);
    legal_moves = movegen->moves;
    clearState();
}

Engine::Engine(Board* _board) {
    game_board = _board;
    search_board = *_board;
    movegen = std::make_unique<MoveGenerator>(&search_board);
    movegen->generateMoves(game_board);
    legal_moves = movegen->moves;
}

void Engine::clearState() {
    stop = false;
    pondering = false;
    bestMove = Move::NullMove();
    ponderMove = Move::NullMove();
    search_depth = -1;
    time_left[0] = time_left[1] = 0;
    increment[0] = increment[1] = 0;
}

void Engine::setOption(const std::string& name, const std::string& value) {
    if (name == "Move Overhead") {
        moveOverhead = std::stoi(value);
        std::cout << "info string set Move Overhead to " << moveOverhead << std::endl;
    }
    else if (name == "Hash") {
        hashSize = std::stoi(value);
        std::cout << "info string set Hash to " << hashSize << std::endl;
    }
    else if (name == "Threads") {
        threads = std::stoi(value);
        std::cout << "info string set Threads to " << threads << std::endl;
    }
    else if (name == "Ponder") {
        ponder = (value == "true" || value == "1");
        std::cout << "info string set Ponder to " << (ponder ? "true" : "false") << std::endl;
    }
    else if (name == "SyzygyPath") {
        syzygyPath = value;
        std::cout << "info string set SyzygyPath to " << syzygyPath << std::endl;
    }
    else if (name == "UCI_ShowWDL") {
        // You can accept and ignore this option if you don't support WDL tablebases
        uciShowWDL = (value == "true" || value == "1");
        std::cout << "info string set UCI_ShowWDL to " << (uciShowWDL ? "true" : "false") << std::endl;
    }
    else {
        // Silently ignore unknown options to avoid errors
        std::cout << "info string ignoring unknown option: " << name << std::endl;
    }
}



void Engine::setPosition(const std::string& fen, const std::vector<Move>& moves) {
    if (fen == "startpos") {
        game_board->setFromFEN(STARTPOS_FEN);
    } else {
        game_board->setFromFEN(fen);
    }
    playMoves(moves);
}

void Engine::playMoves(const std::vector<Move>& moves) {
    for (const auto& move : moves) {
        game_board->MakeMove(move);
    }
}

void Engine::playMovesStrings(const std::vector<std::string>& moves) {
    for (const auto& move : moves) {
        game_board->MakeMove(Move(move));
    }
}

void Engine::startSearch(SearchSettings settings) {
    search_board = *game_board;
    search_depth = settings.depth;
    time_left[0] = settings.wtime;
    time_left[1] = settings.btime;
    increment[0] = settings.winc;
    increment[1] = settings.binc;
    stop = false;
    pondering = settings.infinite;

    // Set remaining time for this side
    int side = game_board->is_white_move ? 0 : 1;
    int myTime = time_left[side];
    int myInc = increment[side];

    // Time control handling (naive)
    int timeBudget = settings.movetime;
    if (timeBudget == -1 && myTime > 0) {
        int movesToGo = settings.movestogo > 0 ? settings.movestogo : 30;
        timeBudget = myTime / movesToGo + myInc;
    }

    // Begin search
    auto start = std::chrono::steady_clock::now();

    iterativeDeepening();

    // Dummy search — just return the first legal move after sleeping
    //if (timeBudget > 0) {
    //    std::this_thread::sleep_for(std::chrono::milliseconds(std::min(timeBudget, 1000)));
    //}

    //ponderMove = moves.size() > 1 ? moves[1] : Move::NullMove(); // fake pondering target

    auto end = std::chrono::steady_clock::now();
    int elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    //std::cerr << "info string search completed in " << elapsedMs << " ms\n";

    sendBestMove(bestMove, ponderMove);
}


void Engine::startSearch(int depth, int movetime, int wtime, int btime, int winc, int binc) {
    search_board = *game_board;
    search_depth = depth;
    time_left[0] = wtime;
    time_left[1] = btime;
    increment[0] = winc;
    increment[1] = binc;

    stop = false;
    pondering = false;

    // Fake search logic for now
    // Start search on a separate thread to keep UCI responsive
    //search_thread = std::thread(&Engine::iterativeDeepening, this, movetime);
    iterativeDeepening();
    sendBestMove(bestMove, ponderMove);
}

void Engine::stopSearch() {
    stop = true;
    //if (search_thread.joinable()) {
    //    search_thread.join();
    //}
}

void Engine::ponderHit() {
    pondering = false;
}

void Engine::iterativeDeepening() {
    // Dummy search just picks the first legal move
    movegen->generateMoves(&search_board);
    std::vector<Move> moves = movegen->moves;
    if (!moves.empty()) {
        bestMove = Searcher::bestMove(search_board, moves);
    } else {
        bestMove = Move::NullMove();
    }
    // Simulate time-consuming search
    //std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void Engine::sendBestMove(Move best, Move ponder) {
    std::cout << "bestmove " << best.uci();
    if (!ponder.IsNull()) {
        std::cout << " ponder " << ponder.uci();
    }
    std::cout << std::endl;
}

void Engine::uciLoop() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "uci") {
            std::cout << "id name MyEngine 1.0\n";
            std::cout << "id author You\n";
            std::cout << "uciok\n";
        } else if (line == "isready") {
            std::cout << "readyok\n";
        } else if (line == "ucinewgame") {
            clearState();
            game_board->setFromFEN(STARTPOS_FEN);
        } else if (line.rfind("position", 0) == 0) {
            std::istringstream iss(line.substr(9));
            std::string token, fen;
            std::vector<Move> moves;

            if (line.find("startpos") != std::string::npos) {
                fen = "startpos";
                while (iss >> token && token != "moves");
            } else {
                while (iss >> token && token != "moves") {
                    fen += token + " ";
                }
            }

            while (iss >> token) {
                moves.push_back(Move(token));
            }
            setPosition(fen, moves);
        } else if (line.rfind("go", 0) == 0) {
            std::istringstream iss(line.substr(3));
            std::string token;
            int wtime = -1, btime = -1, winc = 0, binc = 0, depth = -1, movetime = -1;

            while (iss >> token) {
                if (token == "wtime") iss >> wtime;
                else if (token == "btime") iss >> btime;
                else if (token == "winc") iss >> winc;
                else if (token == "binc") iss >> binc;
                else if (token == "movetime") iss >> movetime;
                else if (token == "depth") iss >> depth;
            }
            startSearch(depth, movetime, wtime, btime, winc, binc);
        } else if (line == "stop") {
            stopSearch();
        } else if (line == "ponderhit") {
            ponderHit();
        } else if (line == "quit") {
            break;
        }
    }
    exit(0);
}

// OBSOLETE


// move object
Move Engine::getBestMove(Board& board) {
    movegen->generateMoves(&board);
    legal_moves = movegen->moves;
    return Searcher::bestMove(board, legal_moves);
}
// uci
std::string Engine::getBestMoveUCI(Board& board) {
    movegen->generateMoves(&board);
    legal_moves = movegen->moves;
    Move best_move = Searcher::bestMove(board, legal_moves);
    return best_move.uci();
}

// for use with GAME.CPP
// AN INTERFACE TO PLAY WITH USER
void Engine::processPlayerMove(Move move) {
    //board->MakeMove(move);
    //movegen->generateMoves(board);
    //legal_moves = movegen->moves;
}

std::string Engine::processEngineMoveString() {
    Move best_move = Searcher::bestMove(search_board, legal_moves);
    //board->MakeMove(best_move);
    return best_move.uci();
}

Move Engine::processEngineMove() {
    Move best_move = Searcher::bestMove(search_board, legal_moves);
    //board->MakeMove(best_move);
    return best_move;
}


// JAVA INTERFACE SUPPORT

/*
extern "C" JNIEXPORT jlong JNICALL Java_engine_Engine_initNativeEngine(JNIEnv* env, jobject obj) {
    Engine* engine = new Engine();
    return reinterpret_cast<jlong>(engine);
}

extern "C" JNIEXPORT void JNICALL Java_engine_Engine_freeNativeEngine(JNIEnv* env, jobject obj, jlong enginePtr) {
    Engine* engine = reinterpret_cast<Engine*>(enginePtr);
    delete engine;
}

extern "C" JNIEXPORT jstring JNICALL Java_engine_Engine_getBestMove(JNIEnv* env, jobject obj) {
    jclass engineClass = env->GetObjectClass(obj);
    jfieldID nativeEngineField = env->GetFieldID(engineClass, "nativeEngine", "J");
    jlong nativeEnginePtr = env->GetLongField(obj, nativeEngineField);

    Engine* engine = reinterpret_cast<Engine*>(nativeEnginePtr);
    
    std::string move = engine->processEngineMove();
    
    return env->NewStringUTF(move.c_str());
}

extern "C" JNIEXPORT jstring JNICALL Java_engine_Engine_getBoardState(JNIEnv* env, jobject obj) {
    jclass engineClass = env->GetObjectClass(obj);
    jfieldID nativeEngineField = env->GetFieldID(engineClass, "nativeEngine", "J");
    jlong nativeEnginePtr = env->GetLongField(obj, nativeEngineField);

    Engine* engine = reinterpret_cast<Engine*>(nativeEnginePtr);
    
    std::string fen = engine->getBoardState();
    
    return env->NewStringUTF(fen.c_str());
}
    */