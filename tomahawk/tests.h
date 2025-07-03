// tests
#ifndef TESTS_H
#define TESTS_H

#include "helpers.h"
#include "moveGenerator.h"
#include "arbiter.h"
#include "engine.h"

struct Tests 
{
private:
public:
    Board board;
    std::unique_ptr<MoveGenerator> movegen = std::make_unique<MoveGenerator>(&board);
    //Engine engine;
    //Arbiter arbiter = Arbiter();
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
        board = Board();
        movegen = std::make_unique<MoveGenerator>(&board);
        // Ensure move_color and plyCount are reset to their initial states
        board.move_color = 0; // Start with white to move
        board.plyCount = 0;   // Starting ply count
        resetVars();
    }


    void setBoard(std::string _fen) {
        //whiteMoves.clear();
        //blackMoves.clear();
        board = Board(_fen);
        movegen = std::make_unique<MoveGenerator>(&board);
        resetVars();
    }

    void setBoard(const Board& _board) {
        board = _board;
        movegen = std::make_unique<MoveGenerator>(&board);
        resetVars();
    }

    Tests() {
        board = Board();
        movegen = std::make_unique<MoveGenerator>(&board);
    }

    Tests(Board _board) {
        setBoard(_board);
    }

    // INTERNAL TESTS
    void printHashHistory() const {
        std::cout << "\nZobrist Hash History:\n";
        for (const auto& pair : board.hash_history) {
            std::cout << "Hash " << pair.first << ": " << pair.second << " occurrence(s)\n";
        }
        std::cout << "----------------------------------------\n";
    }

    void printZobristComponents(const Board& board) {
        std::cout << "\nZobrist Debug Info:\n";
        std::cout << "Current Hash: " << board.zobrist_hash << "\n";

        // Side to move
        if (!board.is_white_move) {
            std::cout << "- Side to move: Black (xor: " << board.zobrist_side_to_move << ")\n";
        } else {
            std::cout << "- Side to move: White (xor: 0)\n";
        }

        // En passant
        if (board.currentGameState.enPassantFile != -1) {
            int file = board.currentGameState.enPassantFile;
            std::cout << "- En Passant File: " << char('a' + file)
                    << " (xor: " << board.zobrist_enpassant[file] << ")\n";
        } else {
            std::cout << "- En Passant File: none (xor: 0)\n";
        }

        // Castling rights
        std::cout << "- Castling rights: ";
        if (board.currentGameState.castlingRights == 0)
            std::cout << "none";
        else {
            if (board.currentGameState.castlingRights & 1) std::cout << "K";
            if (board.currentGameState.castlingRights & 2) std::cout << "Q";
            if (board.currentGameState.castlingRights & 4) std::cout << "k";
            if (board.currentGameState.castlingRights & 8) std::cout << "q";
        }

        std::cout << " (xor: " << board.zobrist_castling[board.currentGameState.castlingRights] << ")\n";
    }

    // should see new hashs appear on new positions, increments/decrements on repeat hashs, and hashs disappear before first appearances
    void zobristTest() {
        U64 computed_zobrist;
        std::string input;
        std::string start_str, target_str; int start, target; int ep; std::string pp;
        Move move = Move(0); // init to null move 

        std::cout << "Initial Board:" << std::endl;
        board.print_board();

        while (true) {
            std::cout << "\nEnter move (or 'unmake <move>') (format: [a-z][1-8][a-z][1-8]): ";
            std::getline(std::cin, input);

            if (input == "quit") {break;}

            if (input.rfind("unmake ", 0) == 0) {
                start_str = input.substr(7,2); target_str = input.substr(9,2);
                start = algebraic_to_square(start_str); target = algebraic_to_square(target_str);
                ep = !board.is_white_move ? board.gameStateHistory[board.gameStateHistory.size()-2].enPassantFile + 8*5 : board.gameStateHistory[board.gameStateHistory.size()-2].enPassantFile + 8*2;

                // castling
                if ((start_str == "e1" || start_str == "e8") && (target_str == "g1" || target_str == "g8" || target_str == "c1" || target_str == "c8")) {
                    move = Move(start, target, Move::castleFlag);
                } // ep
                else if (target == ep) {
                    move = Move(start, target, Move::enPassantCaptureFlag);
                } // pawn up 2
                else if (start % 8 == target % 8 && std::abs(start/8 - target/8)==2) {
                    move = Move(start, target, Move::pawnTwoUpFlag);
                } else if ((a2 <= start && start <= h2 && a1 <= target && target <= h1) || (a7 <= start && start <= h7 && a8 <= target && target <= h8)) { // promotion
                    if (pp == "q") move = Move(start,target,Move::promoteToQueenFlag);
                    else if (pp == "n") move = Move(start,target,Move::promoteToKnightFlag);
                    else if (pp == "r") move = Move(start,target,Move::promoteToRookFlag);
                    else if (pp == "b") move = Move(start,target,Move::promoteToBishopFlag);
                } else {move = Move(start,target);}
                    
                if (!board.allGameMoves.empty() && Move::SameMove(board.allGameMoves.back(), move)) {
                    board.UnmakeMove(move);
                    //board.allGameMoves.pop_back();
                    std::cout << "Move '" << input << "' unmade.\n";
                } else {
                    std::cout << "Warning: Move to unmake does not match last move.\n";
                }
            } else {
                start_str = input.substr(0,2); target_str = input.substr(2,2);
                start = algebraic_to_square(start_str); target = algebraic_to_square(target_str);
                ep = board.is_white_move ? board.currentGameState.enPassantFile + 8*5 : board.currentGameState.enPassantFile + 8*2;

                // castling
                if ((start_str == "e1" || start_str == "e8") && (target_str == "g1" || target_str == "g8" || target_str == "c1" || target_str == "c8")) {
                    move = Move(start, target, Move::castleFlag);
                } // ep
                else if (target == ep) {
                    move = Move(start, target, Move::enPassantCaptureFlag);
                } // pawn up 2
                else if (start % 8 == target % 8 && std::abs(start/8 - target/8)==2) {
                    move = Move(start, target, Move::pawnTwoUpFlag);
                } else if ((a2 <= start && start <= h2 && a1 <= target && target <= h1) || (a7 <= start && start <= h7 && a8 <= target && target <= h8)) { // promotion
                    if (pp == "q") move = Move(start,target,Move::promoteToQueenFlag);
                    else if (pp == "n") move = Move(start,target,Move::promoteToKnightFlag);
                    else if (pp == "r") move = Move(start,target,Move::promoteToRookFlag);
                    else if (pp == "b") move = Move(start,target,Move::promoteToBishopFlag);
                } else {move = Move(start,target);}

                board.MakeMove(move);
                //board.allGameMoves.push_back(move);
                std::cout << "Move '" << input << "' made.\n";
            }

            std::cout << "\nUpdated Board:\n";
            board.print_board();
            printHashHistory();
            printZobristComponents(board);

            if (board.zobrist_hash != board.computeZobristHash()) {
                computed_zobrist = board.computeZobristHash();
                std::cout << "board | computed" << std::endl;
                std::cout << board.zobrist_hash << " || " << computed_zobrist << std::endl;
                board.debugZobristDifference(board.zobrist_hash, computed_zobrist);
            }
        }
    }

    // MOVEGEN TESTS

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
            std::cout << sizeof(movegen->moves) / sizeof(movegen->moves[0]) << std::endl;
            if (init_moves && i==2) {
                for (Move move : movegen->moves) {
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

        //std::cout << "movegen board (post first move)" << std::endl;
        //movegen->board.print_board();
        Move first_moves[MoveGenerator::max_moves];
        int move_cnt = movegen->generateMovesList(&board, first_moves);
        Move move;
        
        //std::cout << "movegen board (post first move) - post movegeneration" << std::endl;
        //movegen->board.print_board();
        int pos = 0;

        //std::cout << "perft, depth: " << depth << std::endl;
        //board.print_board();
        for (int i = 0; i < move_cnt; i++) {
            move = first_moves[i];
            //moveBreakdown(move, board);
        
            board.MakeMove(move); 
            //std::cout << "board after move" << std::endl;
            //board.print_board();

           
            //checks += board.is_in_check ? 1 : 0;
            //checkmates += (Arbiter::GetGameState(&board) == WhiteIsMated || Arbiter::GetGameState(&board) == BlackIsMated) ? 1 : 0;
            //captures += board.currentGameState.capturedPieceType > -1 ? 1 : 0;

            pos += perft(depth-1);

            board.UnmakeMove(move);
            //std::cout << "board after move" << std::endl;
            //board.print_board();

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
        Move first_moves[MoveGenerator::max_moves];
        int move_cnt = movegen->generateMovesList(&board, first_moves);
        Move move;
        int total = 0;
        int count;

        //std::cout << "movegen board" << std::endl;
        //movegen->board.print_board();
    
        for (int i = 0; i < move_cnt; i++) {
            move = first_moves[i];
            //moveBreakdown(move, board);

            //if (Move::SameMove(Move(a2,a3),move)) {continue;}

            if (Move::SameMove(Move(d5,c6,Move::enPassantCaptureFlag),move)) {
                board.print_board();
            }

            board.MakeMove(move);

            
            //std::cout << "perftDivide post move" << std::endl;
            //board.print_board();
    
            count = perft(depth - 1);

            std::cout << move.uci() << ": " << count << std::endl;
            total += count;

            //if (total >= 3) {std::cout << "total >= 3" << std::endl; break;}

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
            {"Complex Middle",   "r4rk1/1pp1qppp/p1np1n2/8/2P5/2N1PN2/PP2QPPP/2KR1B1R w - -",     "46",  "2079", "89890", nullptr},
            {"Promotions & Castling", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8  ",nullptr,"1486",nullptr,nullptr}
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


    // search tests

    void testSearch(std::string fen = STARTPOS_FEN, int maxDepth = 5, int timeLimitMs = 60000) {
        std::cout << "=== Iterative Deepening Search Test ===\n";
        std::cout << "Starting FEN:\n" << fen << "\n";

        for (int depth = 1; depth <= maxDepth; ++depth) {
            setBoard(fen);
            Move legalMoves[MoveGenerator::max_moves];
            movegen->generateMovesList(&board, legalMoves);

            Searcher::nodesSearched = 0;  // reset nodes counter before each depth

            auto start = std::chrono::steady_clock::now();
            // search eval
            SearchResult search_result = Searcher::search(board, *movegen, legalMoves, movegen->count, depth, start, timeLimitMs);
            int eval_search = search_result.eval;
            std::unordered_map<std::string, int> component_search = search_result.component_evals;
            auto split = std::chrono::steady_clock::now();
            // static eval
            //int eval_static = Evaluator::Evaluate(&board);
            std::unordered_map<std::string, int> component_static = Evaluator::componentEvals;
            auto end = std::chrono::steady_clock::now();

            auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            Move best = search_result.bestMove;

            std::cout << "Depth: " << depth << "\n";
            std::cout << "Time: " << durationMs << " ms\n";
            std::cout << "Nodes: " << Searcher::nodesSearched << "\n";
            std::cout << "Best move: " << best.uci() << "\n";
            //std::cout << "Evaluation (static/search): " << eval_static << "\t" << eval_search << "\n";
            std::cout << "---------- Eval Components (static/search) -----------\n";
            for (const auto& [key, value] : component_static) {
                std::cout << key << ": " << value << "\n";
            }
            std::cout << "=======================================\n";
            for (const auto& [key, value] : component_search) {
                std::cout << key << ": " << value << "\n";
            }


            if (best.IsNull()) {
                std::cout << "Search aborted due to time limit.\n";
                break;
            }

            std::cout << "=======================================\n";
            std::cout << "=======================================\n";
            std::cout << "=======================================\n";
        }
    }



    // eval tests
    
    

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



};

#endif