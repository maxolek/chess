// Engine.cpp
#include "engine.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>

Engine::Engine() {
    //game_board = &Board();
    //search_board = *game_board;
    movegen = std::make_unique<MoveGenerator>(&search_board);
    int count = movegen->generateMovesList(game_board, legal_moves);
    evaluator = Evaluator();
    clearState();
}

Engine::Engine(Board* _board) {
    game_board = _board;
    search_board = *_board;
    movegen = std::make_unique<MoveGenerator>(&search_board);
    int count = movegen->generateMovesList(game_board, legal_moves);
    evaluator = Evaluator();
}

void Engine::clearState() {
    stop = false;
    pondering = false;
    bestMove = Move::NullMove();
    ponderMove = Move::NullMove();
    search_depth = -1;
    time_left[0] = time_left[1] = 0;
    increment[0] = increment[1] = 0;
    evaluator = Evaluator();
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
    for (const auto& moveStr : moves) {
        int start = algebraic_to_square(moveStr.substr(0, 2));
        int target = algebraic_to_square(moveStr.substr(2, 2));
        int flag = Move::noFlag;
        int epSquare = -1;

        int movedPiece = game_board->getMovedPiece(start);
        bool isPawn = movedPiece == pawn;

        // En Passant
        if (isPawn && game_board->currentGameState.enPassantFile != -1) {
            int epRank = game_board->is_white_move ? 5 : 2;
            epSquare = game_board->currentGameState.enPassantFile + epRank * 8;
            if (target == epSquare) {
                flag = Move::enPassantCaptureFlag;
            }
        }

        // Pawn double push
        if (isPawn) {
            int startRank = start / 8;
            int targetRank = target / 8;
            if (std::abs(startRank - targetRank) == 2) {
                flag = Move::pawnTwoUpFlag;
            }
        }

        // Promotion
        if (moveStr.length() == 5) {
            char promo = moveStr[4];
            switch (promo) {
                case 'q': flag = Move::promoteToQueenFlag; break;
                case 'r': flag = Move::promoteToRookFlag; break;
                case 'b': flag = Move::promoteToBishopFlag; break;
                case 'n': flag = Move::promoteToKnightFlag; break;
            }
        }

        // Castling (optional: could be left to Move constructor if you detect it there)
        if ((moveStr == "e1g1" || moveStr == "e1c1" || moveStr == "e8g8" || moveStr == "e8c8") 
            && (movedPiece == king)) {
            flag = Move::castleFlag;
        }

        game_board->MakeMove(Move(start, target, flag));
    }
}


int Engine::computeSearchTime(SearchSettings settings) {
    int side = game_board->is_white_move ? 0 : 1;
    int ply = game_board->plyCount;

    if (ply < 10) {return 3500;}

    int myTime = settings.wtime;
    int myInc = settings.winc;
    if (side == 1) {
        myTime = settings.btime;
        myInc = settings.binc;
    }

    int movesToGo = settings.movestogo > 0 ? settings.movestogo : 25;
    int overhead = moveOverhead > 0 ? moveOverhead : 15;

    // If fixed movetime is specified, use that directly
    if (settings.movetime > 0) {
        return std::max(500, settings.movetime - overhead);
    }

    // Fallback time control
    double aggressiveness = .75;
    int timeBudget = static_cast<int>((myTime / movesToGo + myInc) * aggressiveness);
    timeBudget -= overhead;

    // Clamp to avoid negative time or overspending
    timeBudget = clamp(timeBudget, 10, myTime - overhead);

    std::ofstream file("C:/Users/maxol/code/chess/uci_interactions.txt", std::ios::app); // append mode = std::ios::app
    file << "(ply " << ply << ")TIME BUDGET DECISION ---- " << timeBudget << std::endl;
    file.close();

    return timeBudget;
}

// move object
Move Engine::getBestMove(Board& board) {
    search_board = *game_board; SearchSettings default_settings = SearchSettings();
    iterativeDeepening(default_settings);
    return bestMove;
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
   // int side = game_board->is_white_move ? 0 : 1;
    //int myTime = time_left[side];
    //int myInc = increment[side];

    // Time control handling (naive)
    //int timeBudget = settings.movetime;
    //if (timeBudget == -1 && myTime > 0) {
    //    int movesToGo = settings.movestogo > 0 ? settings.movestogo : 75;
    //    timeBudget = myTime / movesToGo + myInc;
    //}

    // Begin search
    auto start = std::chrono::steady_clock::now();

    iterativeDeepening(settings);

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

/*
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
    */

void Engine::stopSearch() {
    stop = true;
    //if (search_thread.joinable()) {
    //    search_thread.join();
    //}
}

void Engine::ponderHit() {
    pondering = false;
}


void Engine::iterativeDeepening(SearchSettings settings) {
    // track search time
    auto start_time = std::chrono::steady_clock::now();
    int elapsed_ms;
    int time_limit_ms = computeSearchTime(settings);
    //Move iteration_bestMove; // if time reached mid-depth, return best from last depth
    int iteration_bestEval;
    int depth_limit = settings.depth ? settings.depth : 10;

    // generate first legal moves from current board position
    Move first_moves[MoveGenerator::max_moves];
    int count = movegen->generateMovesList(&search_board, first_moves);
    
    // until time stop
    // or depth limit (currently no limit)
    int depth = 1;
    while (!stop) {
        SearchResult result = Searcher::search(search_board, *movegen, evaluator, first_moves, count, depth, start_time, time_limit_ms);
    
        auto now = std::chrono::steady_clock::now();
        elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
        
        if (elapsed_ms >= time_limit_ms || depth == depth_limit) {stop = true;}
        else if (!result.bestMove.IsNull()) {
            bestMove = result.bestMove;
            iteration_bestEval = result.eval;
            logSearchDepthInfo(depth, bestMove, iteration_bestEval, elapsed_ms);
        }
            
        depth++; // increase depth and re-search if time permits
    }

}

void Engine::sendBestMove(Move best, Move ponder) {
    std::cout << "bestmove " << best.uci();
    if (!ponder.IsNull()) {
        std::cout << " ponder " << ponder.uci();
    }
    std::cout << std::endl;
}


void Engine::logSearchDepthInfo(int depth, Move bestMove, int eval, int elapsed_ms, std::string file_path) {
    std::ofstream file(file_path, std::ios::app); // append mode

    file << "-----------------------------\n";
    file << "-----------------------------\n";
    file << "-----------------------------\n";
    file << "FEN: " << search_board.getBoardFEN() << "\n";
    file << "Depth: " << depth << "\n";
    file << "Best move: " << bestMove.uci() << "\n";
    file << "Eval: " << eval << "\n";
    file << "Time: " << elapsed_ms << " ms\n";
    file << "Nodes searched: " << Searcher::nodesSearched << "\n";
    file << "-----------------------------\n";

    if (depth == 1) {evaluator.writeEvalDebug(movegen.get(), search_board, file_path);}
}




/*
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
    */

// OBSOLETE

/*
// uci
std::string Engine::getBestMoveUCI(Board& board) {
    movegen->generateMoves(&board);
    legal_moves = movegen->moves;
    Move best_move = Searcher::bestMove(search_board, *movegen, moves, 4);
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
    Move best_move = Searcher::bestMove(search_board, *movegen, moves, 4);
    //board->MakeMove(best_move);
    return best_move.uci();
}

Move Engine::processEngineMove() {
    Move best_move = Searcher::bestMove(search_board, *movegen, moves, 4);
    //board->MakeMove(best_move);
    return best_move;
}
*/

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