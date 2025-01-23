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

    board.print_board();


    for (int i = 0; i < 10; i++) {
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
        Move your_move = Move(algebraic_to_square(yourMove.substr(0,2)), algebraic_to_square(yourMove.substr(2,2)));

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
// should institute some legality checker (outside of movegen?)