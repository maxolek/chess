// Move Generator

// looks for valid:
// sliding moves, king moves, pins, captures, etc.

#ifndef MOVEGENERATOR_H
#define MOVEGENERATOR_H

#include "PrecomputedMoveData.h"
#include "move.h"
#include "board.h"

class MoveGenerator {
private:
public:
    const int max_moves = 218;
    std::vector<Move> moves;

    // once building a more robust engine put this in the engine class not in movegen (to save time)
    //      based on the idea that MoveGenerator will be made new every turn
    PrecomputedMoveData attack_masks = PrecomputedMoveData();

    // instance variables
    bool in_check;
    bool in_double_check;

    // board
    Board board;
    U64 own, opp, pawns, knights, bishops, rooks, queens, kings;
    int side;
    int move_idx;

    // precomputes
    int own_king_square;

    // currently with my bool approach, opponentAttackMap is generated at the beginning of each ply no matter what
    //      thorough (a simple pawn move can open up a lot of changes so this captures everything) but not optimized
    // there is probably a way to store this search (e.g. after we make a move we reuse our search/move results as opponentAttackMap)


    // if in check, bitboard contains squares in line from checking piece up to king
    // rays include the attacking piece (as a capture of the pinning/checking piece is still valid during pin/check)
    U64 check_ray_mask = 0ULL;
    U64 pin_rays = 0ULL;
    U64 opponentAttackMap = 0ULL;
    U64 postEnpassantOpponentAttackMap = 0ULL; // used for isEnpassantCheck to see if making a move leads to check

    // if an opponent piece is pinned then it is not added to opponentAttackMap
    //      it needs to be since the king still cannot enter this square


    MoveGenerator();
    MoveGenerator(Board _board);

    // look up kings last 
    //      have to generate opponenent attack map
    //          might not be that efficient for stuff like checks but it handles discoveries and everything
    //      then look up opponent king first

    // generate pins and legality
    // easier code might be to generate it on iteration and not in this bool stuff

    void generateMoves();
    void generateMoves(Board board);


    // obstruction difference for sliding moves
    // includes blockers
    // quiet moves and all pieces as capturable
    //      in GenerateSlidingMoves() must remove own pieces
    // *pMask is pointer to precomputed blank board attack masks with lower,upper,both format
    U64 odiff(U64 occ, SMasks pMask);
    // count number of pieces along pin_ray to determine if pinned
    // if only piece then pinned, otherwise movement is legal (not necessarily good tho)
    bool isPinned(int square);

    //need to make sure this doesnt result in a check 
    //regular pins (e.g. file and diag) should already be handled via pin_rays
    //but along rank, enpassant capture could result in check since both pawns are now gone
    bool isEnpassantPinned(int start_square, int target_file);

    // post_move_attacks_only is used to update postEnpassantOpponentAttackMap without updating other vars
    //      since this is used to ensure enpassant legality it is only necessary for rook/queen pieces
    void generateSlidingMoves(bool ours, bool add_to_list, bool post_move_attacks_only);
    void generateKnightMoves(bool ours, bool add_to_list);
    void generatePawnPushes(bool ours, bool add_to_list);
    void generatePawnAttacks(bool ours, bool add_to_list);
    void generatePromotions(int start_square, int target_square);
    void generateKingMoves(bool ours, bool add_to_list);
    
    // if seeing if _own_ is in check, opp moves need to be generated
    // and vice versa
    bool checkForCheck(bool ours);
    // generate moves when in check
    //      since the checking piece still has to be discovered, we can then use this information
    //      to limit our move gen (block sliding, capture, or king move)
    //      rather than narrowing down legality from all blank board moves as we currently do

    // if going to be used in the same instance the bitboards need to be reupdated with the call commands
    void updateBitboards(Board board);

};

#endif