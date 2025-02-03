#include "moveGenerator.h"
#include "board.h"
#include "arbiter.h"

// first set of moves is looking good
// now, test for 1. check 2. pins 3. castling 4. enpassant 
// then run movegencount test

int main() {
    Board board = Board();
    Result gamestate;
    //PrecomputedMoveData precomp = PrecomputedMoveData();
    MoveGenerator movegen = MoveGenerator(board);

    std::string yourMove;

    //print_bitboard(precomp.alignMasks[b2][g7]);
    //print_bitboard(precomp.rayMasks[b7][g2]);
    //return 0;

    //board.print_board();


    for (int i = 0; i < 20; i++) {
        // engine move (random)
        movegen.generateMoves(board);

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, movegen.moves.size() - 1);
        int randomIndex = dis(gen);

        Move rand_move = movegen.moves[randomIndex];
        rand_move.PrintMove();

        board.MakeMove(rand_move);
        board.print_board();
        // check for gameover
        std::cout << Arbiter::GetGameState(board) << std::endl;
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
        if (yourMove == "break") break;
        Move your_move = Move(0); // needs some sort of default
        if (yourMove.length() == 4) { your_move = Move(algebraic_to_square(yourMove.substr(0,2)), algebraic_to_square(yourMove.substr(2,2))); }
        else if (yourMove.length() == 5) { //  e.g. a2a1Q
            // enpassant
            if (yourMove.substr(4,1) == "e")
                your_move = Move(algebraic_to_square(yourMove.substr(0,2)), algebraic_to_square(yourMove.substr(2,2)), Move::enPassantCaptureFlag);
            // captures
            if (yourMove.substr(4,1) == "B")
                your_move = Move(algebraic_to_square(yourMove.substr(0,2)), algebraic_to_square(yourMove.substr(2,2)), Move::promoteToBishopFlag);
            if (yourMove.substr(4,1) == "N")
                your_move = Move(algebraic_to_square(yourMove.substr(0,2)), algebraic_to_square(yourMove.substr(2,2)), Move::promoteToKnightFlag);
            if (yourMove.substr(4,1) == "R")
                your_move = Move(algebraic_to_square(yourMove.substr(0,2)), algebraic_to_square(yourMove.substr(2,2)), Move::promoteToRookFlag);
            if (yourMove.substr(4,1) == "Q")
                your_move = Move(algebraic_to_square(yourMove.substr(0,2)), algebraic_to_square(yourMove.substr(2,2)), Move::promoteToQueenFlag);
        }

        your_move.PrintMove();
        board.MakeMove(your_move);
        board.print_board();
        // check for gameover
        gamestate = Arbiter::GetGameState(board);
        if (gamestate == WhiteIsMated || gamestate == BlackIsMated) {
            std::cout << std::endl;
            std::cout << "Checkmate!" << std::endl;
            break;
        }
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