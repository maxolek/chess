// tests
#ifndef TESTS_H
#define TESTS_H

#include "helpers.h"
#include "moveGenerator.h"
#include "arbiter.h"

struct Tests 
{
private:
public:
    Board board = Board();
    MoveGenerator movegen = MoveGenerator(board);
    Arbiter arbiter = Arbiter();
    std::vector<Move> finalPlyMovesList;
    //std::vector<Move> fullMovesList;
    //std::vector<Move> whiteMoves;
    //std::vector<Move> blackMoves;
    std::vector<Move> check_moves;
    int white = 0, black = 0, pawns_pushes = 0, pawns_captures = 0, knights = 0;
    int bishops = 0, rooks = 0, queens = 0, kings = 0;
    int castles = 0, promotions = 0, enpassants = 0, captures = 0, checks = 0;
    int checkmates = 0, total = 0;

    void resetVars() {
        white = 0, black = 0, pawns_pushes = 0, pawns_captures = 0, knights = 0;
        bishops = 0, rooks = 0, queens = 0, kings = 0;
        castles = 0, promotions = 0, enpassants = 0, captures = 0, checks = 0;
        checkmates = 0, total = 0;
        finalPlyMovesList.clear();
    }

    void resetBoard() {
        //whiteMoves.clear();
        //blackMoves.clear();
        resetVars();
        board = Board();
        movegen.updateBitboards(board);
        // Ensure move_color and plyCount are reset to their initial states
        board.move_color = 0; // Start with white to move
        board.plyCount = 0;   // Starting ply count
    }


    void setBoard(std::string _fen) {
        //whiteMoves.clear();
        //blackMoves.clear();
        board = Board(_fen);
        resetVars();
        movegen.updateBitboards(board);
    }

    void setBoard(Board _board) {
        board = _board;
        resetVars();
        movegen.updateBitboards(board);
    }

    Tests() {}

    Tests(Board _board) {
        setBoard(_board);
    }

    // movegen is not setting the correct side
    // depth = 1 is correct ... depth = 2 is set to black_to_move for first set of moves (should still be white)

    void runPerft(int depth, bool init_moves, std::string _fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") {
        int res = 0;
        //std::vector<Move> first_moves;

        board.setFromFEN(_fen);
        board.print_board();
        
        clock_t start = clock();
        clock_t end;
        double duration;

        for (int i = 2; i <= depth; i++) {
            res = perft(i);

            end = clock();
            duration = double(end-start) / CLOCKS_PER_SEC;
            start = clock();
            std::cout << "--------------------------------------------\n";
            std::cout << "Depth: " << i << "\tTime: " << duration << std::endl;
            std::cout << res << std::endl;
            std::cout << "--------------------------------------------\n";
            PrintMoveBreakdown();
            std::cout << "\nmovegen total moves" << std::endl;
            std::cout << sizeof(movegen.moves) / sizeof(movegen.moves[0]) << std::endl;
            if (init_moves && i==2) {
                for (Move move : movegen.moves) {
                    move.PrintMove();
                }
            }

            for (Move move : finalPlyMovesList) {
                move.PrintMove();
            }
            setBoard(_fen);
        }
    }

    int perft(int depth) {
        if (depth == 0) {
            return 1;
        } 

        movegen.generateMoves(board);
        int pos = 0;

        for (Move move : movegen.moves) {
            moveBreakdown(move, board);
        
            board.MakeMove(move); 
           
            checks += board.is_in_check ? 1 : 0;
            checkmates += (arbiter.GetGameState(board) == WhiteIsMated || arbiter.GetGameState(board) == BlackIsMated) ? 1 : 0;
            captures += board.currentGameState.capturedPieceType > -1 ? 1 : 0;

            pos += perft(depth-1);

            board.UnmakeMove(move);

        }

        return pos;
    }

    int perft(int depth, std::string _fen) {
        setBoard(_fen);
        int p = perft(depth);
        return p;
    }


    void perftDivide(int depth, std::string _fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") {
        setBoard(_fen);
        movegen.generateMoves(board);
        int total = 0;
        int count;
    
        for (Move move :  movegen.moves) {
            moveBreakdown(move, board);

            board.MakeMove(move);
    
            count = perft(depth - 1);

            if (Move::SameMove(move, Move(b4,f4))) {std::cout << count << std::endl;}

            std::cout << move.uci() << ": " << count << std::endl;
            total += count;

            board.UnmakeMove(move);
        }

        std::cout << "Nodes searched: " << total << std::endl;
    }


    void completePerftTest() {
        const char* testCases[][6] = {
            // name       FEN                                                            d1     d2      d3      d4
            {"Initial",   "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",      "20",  "400",  "8902", "197281"},
            {"Kiwipete",  "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", "48",  "2039", "97862", "4085603"},
            {"En Passant Chaos", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -",                         "14",  "191",  "2812", nullptr},
            {"Complex Middle",   "r4rk1/1pp1qppp/p1np1n2/8/2P5/2N1PN2/PP2QPPP/2KR1B1R w - -",     "46",  "2079", "89890", nullptr}
        };

        const int numTests = sizeof(testCases) / sizeof(testCases[0]);
        const int maxDepth = 3;

        for (int t = 0; t < numTests; ++t) {
            const char* name = testCases[t][0];
            const char* fen  = testCases[t][1];

            std::cout << "Running: " << name << "\n";
            std::cout << "FEN: " << fen << "\n";

            for (int depth = 1; depth <= maxDepth && testCases[t][depth + 1]; ++depth) {
                board.setFromFEN(fen);

                auto start = std::chrono::high_resolution_clock::now();
                int nodes = perft(depth);
                auto end = std::chrono::high_resolution_clock::now();

                int expected = std::atoi(testCases[t][depth + 1]);
                bool correct = (nodes == expected);
                double elapsed = std::chrono::duration<double>(end - start).count();

                std::cout << "  Depth " << depth << ": "
                        << (correct ? "[PASS]" : "[FAIL]") << "  "
                        << "Expected: " << expected
                        << ", Got: " << nodes
                        << ", Time: " << std::fixed << std::setprecision(3) << elapsed << "s\n";
            }

            std::cout << std::string(40, '-') << "\n";
        }
    }
    
    

    void moveBreakdown(Move move, Board &board) {
        total++;
        // side
        if (board.move_color == 0) {white++;} else {black++;}
        // piece
        switch (board.getMovedPiece(move.StartSquare())) {
            case pawn:
                if (move.TargetSquare() % 8 == move.StartSquare() % 8) {
                    pawns_pushes++;
                } else {
                    pawns_captures++;
                }
                break;
            case knight:
                knights++;
                break;
            case bishop:
                bishops++;
                break;
            case rook:
                rooks++;
                break;
            case queen:
                queens++;
                break;
            case king:
                kings++;
                break;
        }
        // gamestate
        //board.MakeMove(move);
        //checks += board.is_in_check ? 1 : 0;
        //checkmates += (arbiter.GetGameState(board) == WhiteIsMated || arbiter.GetGameState(board) == BlackIsMated) ? 1 : 0;
        promotions += move.IsPromotion() ? 1 : 0;
        enpassants += move.MoveFlag() == Move::enPassantCaptureFlag ? 1 : 0;
        //captures += board.currentGameState.capturedPieceType > -1 ? 1 : 0;
        castles += move.MoveFlag() == Move::castleFlag ? 1 : 0;
        //board.UnmakeMove(move);
    }

    void PrintMoveBreakdown() {
        std::cout << "--------------------------------------------\n";
        std::cout << "Move Breakdown: " << std::endl;
        std::cout << "--------------------------------------------\n";

        std::cout << "Total Moves: " << total << std::endl;
        std::cout << "White Moves: " << white << std::endl;
        std::cout << "Black Moves: " << black << std::endl;
        std::cout << "\nPiece Breakdown:" << std::endl;
        std::cout << "--------------------------------------------\n";
        std::cout << "Pawns Pushes: " << pawns_pushes << std::endl;
        std::cout << "Pawns Captures: " << pawns_captures << std::endl;
        std::cout << "Knights: " << knights << std::endl;
        std::cout << "Bishops: " << bishops << std::endl;
        std::cout << "Rooks: " << rooks << std::endl;
        std::cout << "Queens: " << queens << std::endl;
        std::cout << "Kings: " << kings << std::endl;
        std::cout << "\nSpecial Moves Breakdown:" << std::endl;
        std::cout << "--------------------------------------------\n";
        std::cout << "Castles: " << castles << std::endl;
        std::cout << "Promotions: " << promotions << std::endl;
        std::cout << "En Passants: " << enpassants << std::endl;
        std::cout << "Captures: " << captures << std::endl;
        std::cout << "Checks: " << checks << std::endl;
        std::cout << "Checkmates: " << checkmates << std::endl;
        std::cout << "--------------------------------------------\n";
    }

    // printMoveBreakdown() is problematic
    //      board is always in starting state (leading to misassigned bitboards, etc)
    //      and doesnt account for white/black moves both being in fullMovesList

/*
    void printMoveBreakdown(int depth, bool is_white_start) {
        // Track statistics for pieces and game state
        int white = 0, black = 0, pawns_pushes = 0, pawns_captures = 0, knights = 0;
        int bishops = 0, rooks = 0, queens = 0, kings = 0;
        int castles = 0, promotions = 0, enpassants = 0, captures = 0, checks = 0;
        int checkmates = 0, total = 0;

        bool is_white_move;

        // For each depth, reset the breakdown counters
        for (int i = 0; i < fullMovesList.size(); i++) {
            Move move = fullMovesList[i];
            total++;

            is_white_move = is_white_start ? (depth % 2 == 0) : (depth % 2 == 1);
            if (is_white_start) {
                white++;
            } else {
                black++;
            }

            // Breakdown for each type of piece
            for (int piece = 0; piece < 6; piece++) {
                if (board.pieceBitboards[piece] & (1ULL << move.StartSquare())) {
                    switch (piece) {
                        case pawn:
                            if (move.TargetSquare() % 8 == move.StartSquare() % 8) {
                                pawns_pushes++;
                            } else {
                                pawns_captures++;
                            }
                            break;
                        case knight:
                            knights++;
                            break;
                        case bishop:
                            bishops++;
                            break;
                        case rook:
                            rooks++;
                            break;
                        case queen:
                            queens++;
                            break;
                        case king:
                            kings++;
                            break;
                    }
                }
            }

            // Print the board state (optional, for debugging)
            //std::cout << "move-breakdown gamestate" << std::endl;
            //board.print_board();

            // Apply the move, and record game state changes
            board.MakeMove(move);
            checks += board.is_in_check ? 1 : 0;
            checkmates += (arbiter.GetGameState(board) == WhiteIsMated || arbiter.GetGameState(board) == BlackIsMated) ? 1 : 0;
            promotions += move.IsPromotion() ? 1 : 0;
            enpassants += move.MoveFlag() == Move::enPassantCaptureFlag ? 1 : 0;
            captures += board.currentGameState.capturedPieceType > -1 ? 1 : 0;
            castles += move.MoveFlag() == Move::castleFlag ? 1 : 0;
            board.UnmakeMove(move);
        }

        // Print the breakdown of the collected statistics
        std::cout << "--------------------------------------------\n";
        std::cout << "Move Breakdown: " << std::endl;
        std::cout << "--------------------------------------------\n";

        std::cout << "Total Moves: " << total << std::endl;
        std::cout << "White Moves: " << white << std::endl;
        std::cout << "Black Moves: " << black << std::endl;
        std::cout << "\nPiece Breakdown:" << std::endl;
        std::cout << "--------------------------------------------\n";
        std::cout << "Pawns Pushes: " << pawns_pushes << std::endl;
        std::cout << "Pawns Captures: " << pawns_captures << std::endl;
        std::cout << "Knights: " << knights << std::endl;
        std::cout << "Bishops: " << bishops << std::endl;
        std::cout << "Rooks: " << rooks << std::endl;
        std::cout << "Queens: " << queens << std::endl;
        std::cout << "Kings: " << kings << std::endl;
        std::cout << "\nSpecial Moves Breakdown:" << std::endl;
        std::cout << "--------------------------------------------\n";
        std::cout << "Castles: " << castles << std::endl;
        std::cout << "Promotions: " << promotions << std::endl;
        std::cout << "En Passants: " << enpassants << std::endl;
        std::cout << "Captures: " << captures << std::endl;
        std::cout << "Checks: " << checks << std::endl;
        std::cout << "Checkmates: " << checkmates << std::endl;
        std::cout << "--------------------------------------------\n";
    }
*/


};

#endif