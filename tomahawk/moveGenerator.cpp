// Move Generator

// looks for valid:
// sliding moves, king moves, pins, captures, etc.

#include "moveGenerator.h"


// enpassantfile is not getting updated ... so movegen never finds enpassant moves
//  board is showing enpassant is possible correctly

/*
MoveGenerator::MoveGenerator() {
    // reset state
    board = Board();
    side = 0;
    move_idx = 0;
    in_check = false;
    in_double_check = false;
}
*/

MoveGenerator::MoveGenerator(const Board* _board) {
    // load movegen at given state
    board = _board;
    side = _board->is_white_move ? 0 : 1;
    move_idx = _board->plyCount / 2;
    in_check = false; // board->is_in_check;
    in_double_check = false; // gets updated during opponent gen moves
}

// look up kings last 
//      have to generate opponenent attack map
//          might not be that efficient for stuff like checks but it handles discoveries and everything
//      then look up opponent king first

// generate pins and legality
// easier code might be to generate it on iteration and not in this bool stuff

void MoveGenerator::generateMoves() {
    moves.clear();
    updateBitboards(board);
    // gen opponent moves
    // detect checks, pins, etc.
    generatePawnAttacks(false, false);
    generateKnightMoves(false, false);
    generateSlidingMoves(false, false, false);
    generateKingMoves(false, false);

    if (in_double_check) // cannot capture or block out of a double check
        generateKingMoves(true, true);
    else {
        // gen moves
        // uses stored information from gen oppponent moves to determine legality
        generatePawnPushes(true, true);
        generatePawnAttacks(true, true);
        generateKnightMoves(true, true);
        generateSlidingMoves(true, true, false);
        generateKingMoves(true, true);
    }
}

void MoveGenerator::generateMoves(const Board* _board) {
    moves.clear();
    // load movegen at given state
    board = _board;
    updateBitboards(_board);

    // gen opponent moves
    // detect checks, pins, etc.
    generatePawnAttacks(false, false);
    generateKnightMoves(false, false);
    generateSlidingMoves(false, false, false);
    generateKingMoves(false, false);

    //if (check_ray_mask) {in_check = true;}

    if (in_double_check) { // cannot capture or block out of a double check
        generateKingMoves(true, true);
    } else {
        // gen moves
        // uses stored information from gen oppponent moves to determine legality
        generatePawnPushes(true, true);
        generatePawnAttacks(true, true);
        generateKnightMoves(true, true);
        generateSlidingMoves(true, true, false);
        generateKingMoves(true, true);
    }
}


// obstruction difference for sliding moves
// includes blockers
// quiet moves and all pieces as capturable
//      in GenerateSlidingMoves() must remove own pieces
// *pMask is pointer to precomputed blank board attack masks with lower,upper,both format
U64 MoveGenerator::odiff(U64 occ, SMasks pMask) {
    U64 lower, upper, ms1b, odiff_board;
    lower = pMask.lower & occ;
    upper = pMask.upper & occ;

    ms1b = U64(0x8000000000000000) >> lzcnt(lower | 1); // ms1b of lower (at least bit zero)
    odiff_board = upper ^ (upper - ms1b);

    return pMask.lineEx & odiff_board;
}

// count number of pieces along pin_ray to determine if pinned
// if only piece then pinned, otherwise movement is legal (not necessarily good tho)
bool MoveGenerator::isPinned(int square) { 
    U64 bitboard = 0ULL;
    U64 bb_opp = 0ULL; // all squares (incl) above/below king & opp pinning pieces
                        // pin_rays already excludes king
    int target_square;
    bool ortho; // true if orthogonal pin, false if diagonal pin

    //if (square == f4) {std::cout << square << std::endl;}

    if ( (((pin_rays >> square) & 1) != 0)  // in straight_line with sliding opp piece
    ) { // square must be in precomputed pin_rays
        // get singular_pin_ray (alginMasks is guaranteed to have value as pin_rays are only on straight lines)
        bitboard = pin_rays & PrecomputedMoveData::alignMasks[square][own_king_square];
        std::pair<int,int> dir_map = direction_map(square,own_king_square);
        if (dir_map.first == 0 || dir_map.second == 0) {ortho = true;} else {ortho = false;}

        if (square < own_king_square) { 
            // cut down bitboard to own_king -> most significant bit pinning piece
            // i.e. first pinning piece
            if (ortho) {
                // least significant possible
                // excluding batteries since that would still be a pin anyway
                bb_opp = bitboard & (opp&(rooks|queens));
                bb_opp = isolateMSB(bb_opp); 
                if ((square < std::min(own_king_square, sqidx(bb_opp))) || (square > std::max(own_king_square, sqidx(bb_opp)))) {
                    // not between sliding opp piece and own_king == not pinned
                    return false;
                }
                bb_opp = ~(bb_opp-1); // all square above
                bitboard &= bb_opp;
            } else {
                bb_opp = bitboard & (opp&(bishops|queens));
                bb_opp = isolateMSB(bb_opp); 
                if ((square < std::min(own_king_square, sqidx(bb_opp))) || (square > std::max(own_king_square, sqidx(bb_opp)))) {
                    // not between sliding opp piece and own_king == not pinned
                    return false;
                }
                bb_opp = ~(bb_opp-1);
                bitboard &= bb_opp;
            }
        } else { 
            // cut down bitboard to own_king -> least significant bit pinning piece
            // i.e. first pinning piece
            if (ortho) {
                // most significant possible
                // excluding batteries since that would still be a pin anyway
                bb_opp = bitboard & (opp&(rooks|queens));
                bb_opp = isolateLSB(bb_opp);
                if ((square < std::min(own_king_square, sqidx(bb_opp))) || (square > std::max(own_king_square, sqidx(bb_opp)))) {
                    // not between sliding opp piece and own_king == not pinned
                    return false;
                }
                bb_opp |= (bb_opp-1); // all squares below
                bitboard &= bb_opp;
            } else {
                bb_opp = bitboard & (opp&(bishops|queens));
                bb_opp = isolateLSB(bb_opp);
                if ((square < std::min(own_king_square, sqidx(bb_opp))) || (square > std::max(own_king_square, sqidx(bb_opp)))) {
                    // not between sliding opp piece and own_king == not pinned
                    return false;
                }
                bb_opp |= (bb_opp-1);
                bitboard &= bb_opp;
            }
        }

        if (countBits(bitboard & (own|opp)) == 2) { // pinned_piece + pinning_piece = 2, so if >2 then another piece is preventing the pin
    // not legally pinned if there is another of our or opponent pieces in the way
            //pin_rays &= ~bitboard; // remove for future pieces on ray recalc
            return true;
        }
            
    }

    //pin_rays &= ~PrecomputedMoveData::rayMasks[square][own_king_square];
    return false;
}

//need to make sure this doesnt result in a check 
//regular pins (e.g. file and diag) should already be handled via pin_rays
//but along rank, enpassant capture could result in check since both pawns are now gone
bool MoveGenerator::isEnpassantPinned(int start_square, int target_file) {
    // if this is being generated a second time then enpassant must be valid 
    // as this means theres 3 pawns in a line so 1 will always stay to block the check
    if (postEnpassantOpponentAttackMap != 0ULL) return false;

    enPassantMaskBlockers = own|opp;

    pop_bit(enPassantMaskBlockers, start_square);
    if (board->is_white_move) pop_bit(enPassantMaskBlockers, 4*8 + target_file);
    else pop_bit(enPassantMaskBlockers, 3*8 + target_file);
    set_bit(enPassantMaskBlockers, 5*8 + target_file);

    // generate rook/queen moves
    //      this is helping me appreciate the issues with my current function set up
    //      if i run checkForCheck it will update all the pin/check rays which i dont want
    //      add args to indicate whats being updated
    //          may accept defeat on arbiter pre_compute and allow redudancy computation
    // to optimize a bit we can reduce the attack map to the rank along the pawn squares
    //      could also make and unmake move on board? getting fancy lol
    generateSlidingMoves(false, false, true);

    return postEnpassantOpponentAttackMap & (own&kings);
}

// post_move_attacks_only is used to update postEnpassantOpponentAttackMap without updating other vars
//      since this is used to ensure enpassant legality it is only necessary for rook/queen pieces
void MoveGenerator::generateSlidingMoves(bool ours, bool add_to_list, bool post_move_attacks_only) {
    int start_square;
    int target_square;
    U64 o_diff;
    U64 potential_moves_bitboard = Bits::fullSet;
    
    U64 valid_bishops;
    U64 valid_rooks;
    U64 valid_queens; 
    if (ours) {
        valid_rooks = rooks & own;
        valid_bishops = bishops & own;
        valid_queens = queens & own;
    } else {
        valid_rooks = rooks & opp;
        valid_bishops = bishops & opp;
        valid_queens = queens & opp;
    }

    // pinned pieces cannot move if king is in check
    //if (in_check && own) {
    //    valid_bishops &= ~pin_rays;
    //    valid_rooks &= ~pin_rays;
    //    valid_queens &= ~pin_rays;
    //}

    while (valid_rooks) {
        start_square = tzcnt(valid_rooks) - 1; // get LSB
        valid_rooks &= valid_rooks - 1; // clear LSB

        for (int direction = 0; direction < 2; direction++) {
            if (!ours) {
                if (!post_move_attacks_only) {
                    o_diff = odiff(own | opp, PrecomputedMoveData::blankRookAttacks[start_square][direction]);
                    potential_moves_bitboard &= (o_diff & ~opp);
                    // include protected pieces (prevent king capture)
                    opponentAttackMap |= o_diff;
                    if (potential_moves_bitboard & (own & kings)) {
                        in_double_check = in_check;
                        check_ray_mask |= (PrecomputedMoveData::rayMasks[start_square][own_king_square]);
                        check_ray_mask_ext |= (PrecomputedMoveData::alignMasks[start_square][own_king_square]);
                        in_check = true;
                    }
                    if (PrecomputedMoveData::blankRookAttacks[start_square][direction].lineEx & (own & kings)) {
                        // full & reverse minus king
                        pin_rays |= (PrecomputedMoveData::rayMasks[start_square][own_king_square] & ~(1ULL << own_king_square));
                    }
                } else {
                    postEnpassantOpponentAttackMap |= (!side)  
                        ? (odiff(enPassantMaskBlockers, PrecomputedMoveData::blankRookAttacks[start_square][direction]) & ~opp | (1ULL << (board->currentGameState.enPassantFile + 4*8)))
                        : (odiff(enPassantMaskBlockers, PrecomputedMoveData::blankRookAttacks[start_square][direction]) & ~opp | (1ULL << (board->currentGameState.enPassantFile + 3*8)));
                }
            } else {
                if (isPinned(start_square)) { // limit moves to those along the pin array
                    potential_moves_bitboard &= PrecomputedMoveData::alignMasks[start_square][own_king_square]; // cant use pin_rays as multiple pins could open up moves that jump from 1 pin line to another
                }
                if (in_check) { // captures and blocking sliding
                    potential_moves_bitboard &= check_ray_mask;
                    //print_bitboard(potential_moves_bitboard);
                }
                potential_moves_bitboard &= (odiff(own | opp, PrecomputedMoveData::blankRookAttacks[start_square][direction]) & ~own);
                if (add_to_list) {
                    //std::cout << "rook moves" << std::endl;
                    while (potential_moves_bitboard) {
                        target_square = tzcnt(potential_moves_bitboard) - 1;
                        potential_moves_bitboard &= potential_moves_bitboard - 1;

                        moves.push_back(Move(start_square, target_square));
                        //Move(start_square,target_square).PrintMove();
                    }
                }
            }
            
            potential_moves_bitboard = Bits::fullSet; // reset for each direction
        }
    }

    potential_moves_bitboard = Bits::fullSet;

    while (valid_bishops) {
        start_square = tzcnt(valid_bishops) - 1;
        valid_bishops &= valid_bishops -1;

        for (int direction = 0; direction < 2; direction++) {
            if (!ours) {
                    o_diff = odiff(own | opp, PrecomputedMoveData::blankBishopAttacks[start_square][direction]);
                    potential_moves_bitboard &= (o_diff & ~opp);
                    // include protected pieces (prevent king capture)
                    opponentAttackMap |= o_diff;// dont need to generate postEnpassantOpponentAttackMap for bishops (as only rooks/queens can lead to the type of check this is detecting)
                if (potential_moves_bitboard & (own & kings)) {
                    in_double_check = in_check;
                    check_ray_mask |= (PrecomputedMoveData::rayMasks[start_square][own_king_square]);
                    check_ray_mask_ext |= (PrecomputedMoveData::alignMasks[start_square][own_king_square]);
                    in_check = true;
                }
                if (PrecomputedMoveData::blankBishopAttacks[start_square][direction].lineEx & (own & kings))
                    pin_rays |= (PrecomputedMoveData::rayMasks[start_square][own_king_square] & ~(1ULL << own_king_square));

            } else {
                potential_moves_bitboard &= odiff(own | opp, PrecomputedMoveData::blankBishopAttacks[start_square][direction]) & ~own;
                
                if (isPinned(start_square)) { // limit moves to those along the pin array
                    potential_moves_bitboard &= PrecomputedMoveData::alignMasks[start_square][own_king_square];
                }
                if (in_check) { // captures and blocking sliding
                    potential_moves_bitboard &= check_ray_mask;
                }

                if (add_to_list) {
                    //std::cout << "bishop moves" << std::endl;
                    while (potential_moves_bitboard) {
                        target_square = tzcnt(potential_moves_bitboard) - 1;
                        potential_moves_bitboard &= potential_moves_bitboard - 1;

                        moves.push_back(Move(start_square, target_square));
                        //Move(start_square,target_square).PrintMove();
                    }
                }
            }
            
            potential_moves_bitboard = Bits::fullSet; // reset for each direction
        }
    }

    potential_moves_bitboard = Bits::fullSet;
    
    while (valid_queens) {
        start_square = tzcnt(valid_queens) - 1;
        valid_queens &= valid_queens -1;   

        for (int direction = 0; direction < 4; direction++) {
            if (!ours) {
                if (!post_move_attacks_only) {
                    o_diff = odiff(own | opp, PrecomputedMoveData::blankQueenAttacks[start_square][direction]);
                    potential_moves_bitboard &= (o_diff & ~opp);
                    // include protected pieces (prevent king capture)
                    opponentAttackMap |= o_diff;if (potential_moves_bitboard & (own & kings)) {
                        in_double_check = in_check;
                        check_ray_mask |= (PrecomputedMoveData::rayMasks[start_square][own_king_square]);
                        check_ray_mask_ext |= (PrecomputedMoveData::alignMasks[start_square][own_king_square]);
                        in_check = true;
                    }
                    if (PrecomputedMoveData::blankQueenAttacks[start_square][direction].lineEx & (own & kings))
                        pin_rays |= (PrecomputedMoveData::rayMasks[start_square][own_king_square] & ~(1ULL << own_king_square));

                } else {
                    postEnpassantOpponentAttackMap |= (!side)  
                        ? (odiff(enPassantMaskBlockers, PrecomputedMoveData::blankRookAttacks[start_square][direction]) & ~opp | (1ULL << (board->currentGameState.enPassantFile + 4*8)))
                        : (odiff(enPassantMaskBlockers, PrecomputedMoveData::blankRookAttacks[start_square][direction]) & ~opp | (1ULL << (board->currentGameState.enPassantFile + 3*8)));
                }
            } else { 
                if (ours && isPinned(start_square)) { // limit moves to those along the pin array
                    potential_moves_bitboard &= PrecomputedMoveData::alignMasks[start_square][own_king_square];
                }
                if (ours && in_check) { // captures and blocking sliding
                    potential_moves_bitboard &= check_ray_mask;
                } 
                potential_moves_bitboard &= odiff(own | opp, PrecomputedMoveData::blankQueenAttacks[start_square][direction]) & ~own;
                if (add_to_list) {
                    //std::cout << "queen moves" << std::endl;
                    while (potential_moves_bitboard) {
                        target_square = tzcnt(potential_moves_bitboard) - 1;
                        potential_moves_bitboard &= potential_moves_bitboard - 1;

                        moves.push_back(Move(start_square, target_square));
                        //Move(start_square,target_square).PrintMove();
                    }
                }
            }
            
            potential_moves_bitboard = Bits::fullSet; // reset for each direction
        }
    }
}


void MoveGenerator::generateKnightMoves(bool ours, bool add_to_list) {
    int start_square;
    int target_square;
    U64 potential_moves_bitboard = Bits::fullSet;
    
    U64 valid_knights = ours ? knights & own : knights & opp;

    // pinned pieces cannot move if king is in check
    //if (in_check && own) {
    //    valid_knights &= ~pin_rays;
    //}
    
    while (valid_knights) {
        start_square = tzcnt(valid_knights) - 1;
        valid_knights &= valid_knights -1;

        if (ours && in_check) {// captures and blocking sliding
            potential_moves_bitboard &= check_ray_mask;
            //std::cout << "in check" << std::endl;
            //print_bitboard(potential_moves_bitboard);
        }

        if (ours && isPinned(start_square)) {  // limit moves to those along the pin array
            potential_moves_bitboard &= PrecomputedMoveData::alignMasks[start_square][own_king_square];
            //std::cout << "is pinned" << std::endl;
            //print_bitboard(potential_moves_bitboard);
        }

        potential_moves_bitboard &= PrecomputedMoveData::blankKnightAttacks[start_square];
        //std::cout << "blank" << std::endl;
        //print_bitboard(potential_moves_bitboard);

        if (!ours) {
            check_ray_mask |= potential_moves_bitboard & (own & kings) ? 1ULL << start_square : 0;
            if (check_ray_mask) {in_check = true;}
            opponentAttackMap |= potential_moves_bitboard;
        }

        potential_moves_bitboard &= ~own;
        //std::cout << "not own" << std::endl;
        //print_bitboard(potential_moves_bitboard);

        if (add_to_list) {
            //std::cout << "knight moves" << std::endl;
            while (potential_moves_bitboard) {
                target_square = tzcnt(potential_moves_bitboard) - 1;
                potential_moves_bitboard &= potential_moves_bitboard - 1;

                moves.push_back(Move(start_square, target_square));
                //Move(start_square,target_square).PrintMove();
            }
        }

        potential_moves_bitboard = Bits::fullSet;
    }
}


void MoveGenerator::generatePawnPushes(bool ours, bool add_to_list) {
    U64 valid_pawns = ours ? pawns & own : pawn & opp;
    int start_square;
    int target_square;
    U64 potential_moves = Bits::fullSet & ~(own|opp);

    // pinned pieces cannot move if king is in check
    //if (in_check && ours) {
    //    valid_pawns &= ~pin_rays;
    //}

    while (valid_pawns) {
        start_square = tzcnt(valid_pawns) - 1;
        valid_pawns &= valid_pawns - 1;

        if (ours && isPinned(start_square)) {
        // limit moves to those along the pin array
        // still allows captures/pushes along respective pin rays
            potential_moves &= PrecomputedMoveData::alignMasks[start_square][own_king_square];
        }

        potential_moves &= PrecomputedMoveData::blankPawnMoves[start_square][side];
        // check if 1 move forward is possible
        // if not, make sure 2 move forward is removed
        potential_moves &= (get_bit(potential_moves,start_square+std::pow(-1,side)*8)) 
                            ? Bits::fullSet // if it is, change nothing
                            : ~(1ULL << (start_square + static_cast<int>(std::pow(-1,side))*8*2)); 
        // after 2 move check b/c a check that can be blocked
        // by 2x push will prevent 2x push in above method
        if (ours && in_check) {
            potential_moves &= check_ray_mask;
        }

        if (add_to_list) {   
            while (potential_moves) {
                target_square = tzcnt(potential_moves) - 1;
                potential_moves &= potential_moves - 1;

                if (target_square == start_square + std::pow(-1,side) * 8 * 2) {  // double push
                    moves.push_back(Move(start_square, target_square,Move::pawnTwoUpFlag));
                }
                else if ((!side) && ((1ULL << start_square) & Bits::mask_rank_7)) {  // promotions
                    generatePromotions(start_square, target_square);
                }
                else if (side && ((1ULL << start_square) & Bits::mask_rank_2)) {
                    generatePromotions(start_square, target_square);
                }
                else {moves.push_back(Move(start_square, target_square));}// single push
            }
        }

        potential_moves = Bits::fullSet & ~(own|opp);
    }
}

void MoveGenerator::generatePawnAttacks(bool ours, bool add_to_list) {
    U64 valid_pawns = ours ? pawns & own : pawns & opp;
    int start_square;
    int target_square;
    U64 potential_moves = Bits::fullSet;

    // pinned pieces cannot move if king is in check
    //if (in_check && ours) 
        //valid_pawns &= ~pin_rays;

    while (valid_pawns) {
        start_square = tzcnt(valid_pawns) - 1;
        valid_pawns &= valid_pawns - 1;

        if (ours && in_check)
            potential_moves &= check_ray_mask;

        if (ours && isPinned(start_square)) { // limit moves to those along the pin array
            potential_moves &= PrecomputedMoveData::alignMasks[start_square][own_king_square];
        }
        // regular captures
        potential_moves &= (PrecomputedMoveData::fullPawnAttacks[start_square][side] & opp);
        
        // |= since it wouldve been removed in the last step
        if (ours && board->currentGameState.enPassantFile > -1) {
            //std::cout << "enpassant attack square" << std::endl;
            //std::cout << board->currentGameState.enPassantFile << "\t" << 5*8 + board->currentGameState.enPassantFile << std::endl;
            //std::cout << "enpassant pinned\t" << isEnpassantPinned(start_square,board->currentGameState.enPassantFile) << std::endl;
            if (!side) { // white to move
                if (PrecomputedMoveData::fullPawnAttacks[start_square][side] & (1ULL << (5*8 + board->currentGameState.enPassantFile)) && !isEnpassantPinned(start_square, board->currentGameState.enPassantFile))
                    potential_moves |= 1ULL << (5*8 + board->currentGameState.enPassantFile); 
            } else {
                if (PrecomputedMoveData::fullPawnAttacks[start_square][side] & (1ULL << (2*8 + board->currentGameState.enPassantFile)) && !isEnpassantPinned(start_square, board->currentGameState.enPassantFile))
                    potential_moves |= 1ULL << (2*8 + board->currentGameState.enPassantFile); 
            }
        }

        if (!ours) { // dont care about previous intersection subsets
            check_ray_mask |= PrecomputedMoveData::fullPawnAttacks[start_square][1-side] & (own & kings) ? 1ULL << start_square : 0;
            if (check_ray_mask) {in_check = true;}
            opponentAttackMap |= PrecomputedMoveData::fullPawnAttacks[start_square][1-side];
        }

        if (add_to_list) {
            while (potential_moves) {
                target_square = tzcnt(potential_moves) - 1;
                potential_moves &= potential_moves - 1;

                if ((!side) && get_bit(Bits::mask_rank_7, start_square)) {
                    generatePromotions(start_square, target_square);
                } else if (side && get_bit(Bits::mask_rank_2, start_square)) {
                    generatePromotions(start_square, target_square);
                } else if ((target_square % 8) == board->currentGameState.enPassantFile &&
                        ((side == 0 && target_square / 8 == 5) || (side == 1 && target_square / 8 == 2))) {
                    moves.push_back(Move(start_square, target_square, Move::enPassantCaptureFlag));
                } else {
                    moves.push_back(Move(start_square, target_square));
                }
            }
        }

        potential_moves = Bits::fullSet;
    }

}

void MoveGenerator::generatePromotions(int start_square, int target_square) { 
    moves.push_back(Move(start_square, target_square,Move::promoteToQueenFlag));
    moves.push_back(Move(start_square, target_square,Move::promoteToKnightFlag));
    moves.push_back(Move(start_square, target_square,Move::promoteToRookFlag));
    moves.push_back(Move(start_square, target_square,Move::promoteToBishopFlag));
}


void MoveGenerator::generateKingMoves(bool ours, bool add_to_list) {
    int square = ours ? own_king_square : sqidx(kings & opp);
    U64 potential_moves = PrecomputedMoveData::blankKingAttacks[square];
    int target_square;
    U64 castle_blockers, castle_mask, castle_mask_ext;

    if (!ours) 
        opponentAttackMap |= potential_moves;

    potential_moves &= ~own;
    // dont walk into attack
    //      prevents capture of protected piece
    potential_moves &= ~opponentAttackMap;
     // in check
    if (in_check) { // include checking piece
        check_ray_mask_ext |= check_ray_mask; // check_ray_mask_ext only set for sliding pieces
        potential_moves &= (~check_ray_mask_ext | (opp & check_ray_mask)); 
        // only allow onto checking ray if capturing checking piece
        // checking piece is included in check_ray_mask
    }

    if (add_to_list) {
        while (potential_moves) {
            target_square = tzcnt(potential_moves) - 1;
            potential_moves &= potential_moves - 1;

            moves.push_back(Move(square, target_square));
        }
        
        // castling
        if (ours && !in_check) {
            castle_blockers = opponentAttackMap | own | opp;
            if (board->currentGameState.HasKingsideCastleRight(side==0)) {
                castle_mask = (side == 0) ? Bits::whiteKingsideMask : Bits::blackKingsideMask;

                if (!(castle_mask & castle_blockers)) {
                    target_square = (side == 0) ? g1 : g8;
                    moves.push_back(Move(square, target_square, Move::castleFlag));
                }
            }

            if (board->currentGameState.HasQueensideCastleRight(side==0)) {
                castle_mask = (side == 0) ? Bits::whiteQueensideMask : Bits::blackQueensideMask;
                castle_mask_ext = (side == 0) ? Bits::whiteQueensideMaskExt : Bits::blackQueensideMaskExt;

                if (!(castle_mask & castle_blockers) && !((own|opp)&castle_mask_ext)) {
                    target_square = (side == 0) ? c1 : c8;
                    moves.push_back(Move(square, target_square, Move::castleFlag));
                }
            }
        }
    }

}

// if seeing if _own_ is in check, opp moves need to be generated
// and vice versa
bool MoveGenerator::checkForCheck(bool ours) {
    generatePawnAttacks(!ours, false);
    generateKnightMoves(!ours, false);
    generateSlidingMoves(!ours, false, false);

    return (opponentAttackMap & (own&kings)) != 0ULL;
}

// generate moves when in check
//      since the checking piece still has to be discovered, we can then use this information
//      to limit our move gen (block sliding, capture, or king move)
//      rather than narrowing down legality from all blank board moves as we currently do


void MoveGenerator::updateBitboards(const Board* _board) {
    board = _board;
    side = _board->is_white_move ? 0 : 1;
    move_idx = _board->plyCount / 2;
    in_check = false; //board->is_in_check;
    in_double_check = false; // gets updated during gen opponent moves

    own = _board->colorBitboards[side];
    opp = _board->colorBitboards[1-side];
    pawns = _board->pieceBitboards[pawn];
    knights = _board->pieceBitboards[knight];
    bishops = _board->pieceBitboards[bishop];
    rooks = _board->pieceBitboards[rook];
    queens = _board->pieceBitboards[queen];
    kings = _board->pieceBitboards[king];

    check_ray_mask = 0ULL;
    check_ray_mask_ext = 0ULL;
    pin_rays = 0ULL;
    opponentAttackMap = 0ULL;
    postEnpassantOpponentAttackMap = 0ULL;
    own_king_square = sqidx(own&kings);
}
