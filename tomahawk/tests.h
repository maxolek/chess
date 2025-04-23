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

    void runMoveGenTest(int depth, bool init_moves, std::string _fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") {
        int res = 0;
        //std::vector<Move> first_moves;

        board.setFromFEN(_fen);
        board.print_board();
        
        clock_t start = clock();
        clock_t end;
        double duration;

        for (int i = 2; i <= depth; i++) {
            res = MoveGenerationTest(i);

            end = clock();
            duration = double(end-start) / CLOCKS_PER_SEC;
            start = clock();
            std::cout << "--------------------------------------------\n";
            std::cout << "Depth: " << i << "\tTime: " << duration << std::endl;
            std::cout << res << std::endl;
            std::cout << "--------------------------------------------\n";
            PrintMoveBreakdown();
            if (init_moves && i==1) {
                for (Move move : movegen.moves) {
                    move.PrintMove();
                }
            }

            setBoard(_fen);
        }
    }

    int MoveGenerationTest(int depth) {
        if (depth == 0) {
            return 1;
        }

        movegen.generateMoves(board);
        std::vector<Move> moves = movegen.moves;
        int pos = 0;

        for (Move move : moves) {
            moveBreakdown(move, board);

            if (Move::SameMove(move,Move(f4,f3))) {
                std::cout << "check for pinned pawn" << std::endl;
            }

            board.MakeMove(move); 
            board.print_board();

            checks += board.is_in_check ? 1 : 0;
            checkmates += (arbiter.GetGameState(board) == WhiteIsMated || arbiter.GetGameState(board) == BlackIsMated) ? 1 : 0;
            captures += board.currentGameState.capturedPieceType > -1 ? 1 : 0;

            pos += MoveGenerationTest(depth-1);

            board.UnmakeMove(move);
        }

        return pos;
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