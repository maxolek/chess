package engine;

import board.*;

public class Engine {
    private long nativeEngine;

    //load dll
    static {
        System.loadLibrary("engine");
    }

    // engine methods
    public native long initNativeEngine();
    public native void freeNativeEngine(long enginePtr);
    public native String getBestMove();
    public native String getBoardState();

    public Engine() {
        nativeEngine = initNativeEngine();
    }

    // java Engine
    public static Board board = new Board();
    public Board readBoardState() {
        String fen = getBoardState();
        board.setFEN(fen);
        board.makeBoardTilesFromFEN();
    
        return board;
    }

    public static void main(String[] args) {
        Engine engine = new Engine();
        board = engine.readBoardState();
        String bestMove = engine.getBestMove();
        System.out.println(bestMove);
    }
}