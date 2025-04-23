#include "moveGenerator.h"
#include "board.h"
#include "arbiter.h"
#include "tests.h"

// first set of moves is looking good
// now, test for 1. check 2. pins 3. castling 4. enpassant 
// then run movegencount test

int main() {
    Board board = Board();
    Result gamestate;
    PrecomputedMoveData precomp = PrecomputedMoveData();
    MoveGenerator movegen = MoveGenerator(board);

    std::string yourMove;

    //print_bitboard(precomp.alignMasks[b2][g7]);
    //print_bitboard(precomp.rayMasks[b7][g2]);
    //return 0;

    //board.print_board();

    //print_bitboard(precomp.alignMasks[e1][f2]);
    //print_bitboard(precomp.rayMasks[e1][f2]);

    //print_bitboard(board.colorBitboards[0] & board.pieceBitboards[king]);
    /*
    print_bitboard((1ULL << f2) - (board.colorBitboards[0] & board.pieceBitboards[king]));
    print_bitboard(precomp.alignMasks[e1][f2]);
    print_bitboard(((1ULL << f2) - (board.colorBitboards[0] & board.pieceBitboards[king])) & (precomp.alignMasks[e1][f2]));
    print_bitboard(board.colorBitboards[0] | board.colorBitboards[1]);
    print_bitboard(((1ULL << f2) - (board.colorBitboards[0] & board.pieceBitboards[king])) & (precomp.alignMasks[e1][f2]) & (board.colorBitboards[0] | board.colorBitboards[1]));

    std::cout << countBits((1ULL << f2)) << std::endl;
    std::cout << countBits((1ULL << e1) | (1ULL << f2)) << std::endl;
    std::cout << countBits(((1ULL << f2) - (board.colorBitboards[0] & board.pieceBitboards[king])) & (precomp.alignMasks[e1][f2]) & (board.colorBitboards[0] | board.colorBitboards[1])) << std::endl;
    */
    //std::cout << countBits(((1ULL << h4) - (1ULL << e1)) & precomp.alignMasks[h4][e1] & (board.colorBitboards[0] | board.colorBitboards[1])) << std::endl;

    /*
    for (int i = 0; i < 8; i++) {
        std::cout << i << "\t" << direction_map(i).first << " " << direction_map(i).second << std::endl;
        std::cout << precomp.distToEdge[e1][i] << std::endl;
    }
    
    return 0;
    */

    //print_bitboard(precomp.rayMasks[a5][e1]);
    //return 0;

    /* enpassant pinned testing
    board.setFromFEN("8/8/6K1/r2pP3/8/8/8/8 w - d6 0 1");
    board.print_board();
    movegen.generateMoves(board);
    for (Move move : movegen.moves) {
        move.PrintMove();
        std::cout << std::endl;
    }
    return 0;
    */

   //print_bitboard(precomp.alignMasks[c8][e1]);
   //return 0;

   //print_bitboard(board.colorBitboards[1]);
   //print_bitboard(board.colorBitboards[1] & (1ULL << d7));
   //Move move = Move(f1,b5);
   //std::cout << ((move.TargetSquare() == b5) && (move.StartSquare() == f1) && (board.colorBitboards[1] & (1ULL << d7))) << std::endl;
   //return 0;

   //print_bitboard(precomp.rayMasks[b5][e8]);
   //return 0;

    //return 0;

    //board.setFromFEN("8/2p5/3p4/KP5r/1R3p1k/6P1/4P3/8 b - - ");
    //board.print_board();
    //movegen.generateMoves(board);
    //movegen.board.print_board();
    //return 0;

    Tests test = Tests();

    //board.setFromFEN("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ");
    //std::cout << board.currentGameState.HasKingsideCastleRight(true) << "\t" << board.currentGameState.HasQueensideCastleRight(true) << "\n" << board.currentGameState.HasKingsideCastleRight(false) << "\t" << board.currentGameState.HasQueensideCastleRight(false) << std::endl;
    //board.print_board();
    //std::cout << "tf" << std::endl;

    //return 0;

    //board.setFromFEN("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ");
    //board.MakeMove(Move(b4,f4));
    //board.print_board();


    //return 0;


    // ************************
    // correctly identifying when in check now
    //  not sure its escaping check properly
    //  depth = 2 still incorrect
    // ************************
    test.runMoveGenTest(2, true, "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - "); // quits on depth 5+, depth 4 starting pos is short on moves
    //                                                      4: too many checks, too few captures

    // not detecting opp_attack_map/king legality correctly 
    //      a5->b6 listed
    //          pawn attacks are backwards for opp
    // adding castle king moves
    //      a5->c1/g1 listed

    return 0;

/*
   int res = 0;
   clock_t start = clock();
   clock_t end;
   double duration;
   for (int depth = 1; depth < 5; depth++) {
        res = test.MoveGenerationTest(depth);
        end = clock();
        duration = double(end-start) / CLOCKS_PER_SEC;
        start = clock();
        std::cout << "Depth: " << depth << "\n# positions: " << res << "\nTime: " << duration << "\n----------" << std::endl;
   }

   return 0;
*/

    for (int i = 0; i < 20; i++) {
        // engine move (random)
        //std::cout << "premovegen ep " << &board << "\t" << board.currentGameState.enPassantFile << std::endl;
        movegen.generateMoves(board);
        //std::cout << "postmovegen ep " << &board << "\t" << board.currentGameState.enPassantFile << std::endl;
        //std::cout << movegen.in_check << std::endl;
        /*
        for (Move move : movegen.moves) {
            move.PrintMove();
            std::cout << std::endl;
        }
        */
        /*
        if (movegen.in_check) {
            print_bitboard(movegen.check_ray_mask);
        }
        */

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, movegen.moves.size() - 1);
        int randomIndex = dis(gen);

        Move rand_move = movegen.moves[randomIndex];
        Move defined_move1 = Move(e2,e4,Move::pawnTwoUpFlag);
        Move defined_move2 = Move(e4,e5);
        //rand_move.PrintMove();

        if (i > 1) board.MakeMove(rand_move);
        else if (i == 1) board.MakeMove(defined_move2);
        else board.MakeMove(defined_move1);
        board.print_board();
        // check for gameover
        //std::cout << Arbiter::GetGameState(board) << std::endl;
        gamestate = Arbiter::GetGameState(board);
        if (gamestate == WhiteIsMated || gamestate == BlackIsMated) {
            std::cout << std::endl;
            std::cout << "Checkmate!" << std::endl;
            break;
        }

        // player move (input)
        std::cout << std::endl;
        std::cout << "Your move (use format [a-h][0-7][a-h][0-7])" << std::endl;
        std::cin >> yourMove;
        if (yourMove == "break" || yourMove == "quit" || yourMove == "kill") break;
        Move your_move = Move(0); // needs some sort of default
        if (yourMove.length() == 4) { your_move = Move(algebraic_to_square(yourMove.substr(0,2)), algebraic_to_square(yourMove.substr(2,2))); }
        else if (yourMove.length() == 5) { //  e.g. a2a1Q
            // enpassant
            if (yourMove.substr(4,1) == "e")
                your_move = Move(algebraic_to_square(yourMove.substr(0,2)), algebraic_to_square(yourMove.substr(2,2)), Move::enPassantCaptureFlag);
            // pawn up 2
            else if (yourMove.substr(4,1) == "x") {
                your_move = Move(algebraic_to_square(yourMove.substr(0,2)), algebraic_to_square(yourMove.substr(2,2)), Move::pawnTwoUpFlag);
                std::cout << "enpassant file" << int(algebraic_to_square(yourMove.substr(0,2))) % 8 << std::endl;
            }
            // captures
            else if (yourMove.substr(4,1) == "B")
                your_move = Move(algebraic_to_square(yourMove.substr(0,2)), algebraic_to_square(yourMove.substr(2,2)), Move::promoteToBishopFlag);
            else if (yourMove.substr(4,1) == "N")
                your_move = Move(algebraic_to_square(yourMove.substr(0,2)), algebraic_to_square(yourMove.substr(2,2)), Move::promoteToKnightFlag);
            else if (yourMove.substr(4,1) == "R")
                your_move = Move(algebraic_to_square(yourMove.substr(0,2)), algebraic_to_square(yourMove.substr(2,2)), Move::promoteToRookFlag);
            else if (yourMove.substr(4,1) == "Q")
                your_move = Move(algebraic_to_square(yourMove.substr(0,2)), algebraic_to_square(yourMove.substr(2,2)), Move::promoteToQueenFlag);
        }

        your_move.PrintMove();
        board.MakeMove(your_move);
        board.print_board();
        // check for gameover
        if (board.is_in_check) { // ignores stalemate rn (and 50 move and stuff but fine)
            gamestate = Arbiter::GetGameState(board);
            if (gamestate == WhiteIsMated || gamestate == BlackIsMated) {
                std::cout << std::endl;
                std::cout << "Checkmate!" << std::endl;
                break;
            }
        }
        std::cout << "after your move ep " << board.currentGameState.enPassantFile << std::endl;
    }

    for (Move move : board.allGameMoves) {
        move.PrintMove();
    }
    board.print_board();

    return 0;
}


// didnt pick up on being in check
//      correctly gets check_ray_mask, but the check_ray_mask is the entire diagonal rather than just the piece->king ray
//      can loop to get the ray, or precompute similar to align_masks (instead of entire line it is now segment between 2 square)
//          how would non-aligned squares be represented?
// should institute some legality checker (outside of movegen?)