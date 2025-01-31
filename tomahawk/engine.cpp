// Engine.cpp
#include "engine.h"

Engine::Engine() {
    isWhiteTurn = true;
    board = Board();
    MoveGenerator movegen = MoveGenerator(board);
    movegen.generateMoves();
    legal_moves = movegen.moves;
}

Engine::Engine(Board _board) {
    board = _board;
    MoveGenerator movegen = MoveGenerator(board);
    movegen.generateMoves(board);
    legal_moves = movegen.moves;
}

std::string Engine::getBoardState() {
    board.setBoardFEN();
    return board.getBoardFEN();
}

void Engine::processPlayerMove(Move move) {
    board.MakeMove(move);
    isWhiteTurn = !isWhiteTurn;
    movegen.generateMoves(board);
    legal_moves = movegen.moves;
}

std::string Engine::processEngineMove() {
    Move best_move = Searcher::bestMove(legal_moves);
    board.MakeMove(best_move);
    return square_to_algebraic(best_move.StartSquare()) + square_to_algebraic(best_move.TargetSquare());
}

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