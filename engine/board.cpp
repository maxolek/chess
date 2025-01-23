// represents the current state of the board during a game
// state: positions of all pieces, side to move, castling rights, en-passant square, etc.
// additional information to aid in search/eval may be included

// initial state is manual
// subsequently made/unmade moves using MakeMove() and UnmakeMove()

#include "PrecomputedMoveData.h"
#include "move.h"
#include "gamestate.h"
#include "board.h"

Board::Board() {
    currentGameState = GameState();

    colorBitboards[0] = Bits::initWhite;
    colorBitboards[1] = Bits::initBlack;

    pieceBitboards[0] = Bits::initPawns;
    pieceBitboards[1] = Bits::initKnights;
    pieceBitboards[2] = Bits::initBishops;
    pieceBitboards[3] = Bits::initRooks;
    pieceBitboards[4] = Bits::initQueens;
    pieceBitboards[5] = Bits::initKings;

    is_white_move = true;
    move_color = is_white_move ? 0 : 1;
    is_in_check = false;
    plyCount = 0; 
}

Board::Board(int side, U64 _color_bitboards[2], U64 _piece_bitboards[6], GameState currentGameState) {
    is_white_move = (side==0) ? true : false;
    move_color = is_white_move ? 0 : 1;

    // board with args (position load)
    colorBitboards[0] = _color_bitboards[0];
    colorBitboards[1] = _color_bitboards[1];

    pieceBitboards[0] = _piece_bitboards[0];
    pieceBitboards[1] = _piece_bitboards[1];
    pieceBitboards[2] = _piece_bitboards[2];
    pieceBitboards[3] = _piece_bitboards[3];
    pieceBitboards[4] = _piece_bitboards[4];
    pieceBitboards[5] = _piece_bitboards[5];

    is_in_check = inCheck();
}

// inSearch controls whether this move is recorded in the game history
// (for three-fold repetition)
void Board::MakeMove(Move move, bool in_search) {
    // get info about move
    int start_square = move.StartSquare();
    int target_square = move.TargetSquare();
    int move_flag = move.MoveFlag();
    bool is_promotion = move.IsPromotion();
    bool is_enpassant = (move_flag == move.enPassantCaptureFlag);

    int moved_piece = getMovedPiece(start_square);
    int captured_piece = getCapturedPiece(target_square);
    int promotion_piece = move.PromotionPieceType();

    // update bitboards
    MovePiece(moved_piece, start_square, target_square);
    if (captured_piece > -1) { CapturePiece(captured_piece, target_square, is_enpassant); }

        // castling
    if (move_flag == Move::castleFlag) {
        if (!move_color) {
            if (target_square = g1) MovePiece(rook, h1, f1);
            else MovePiece(rook, a1, d1);
        } else {
            if (target_square = g8) MovePiece(rook, h8, f8);
            else MovePiece(rook, a8, d8);
        }
    }
        // promotion
    if (is_promotion) 
        PromoteToPiece(promotion_piece, target_square);

    // update game state
    currentGameState.capturedPieceType = captured_piece;
    currentGameState.enPassantFile = (move_flag == move.pawnTwoUpFlag) ? start_square % 8 : -1;
    updateFiftyMoveCounter(moved_piece, captured_piece > -1);
    if (currentGameState.castlingRights != 0) {
        if (moved_piece == king) {
            if (is_white_move) {
                currentGameState.castlingRights &= GameState::clearWhiteKingSideMask;
                currentGameState.castlingRights &= GameState::clearWhiteQueenSideMask;
            } else {
                currentGameState.castlingRights &= GameState::clearBlackKingSideMask;
                currentGameState.castlingRights &= GameState::clearBlackQueenSideMask;
            }
        } else if (moved_piece == rook) {
            switch (start_square) {
                case a1:
                    currentGameState.castlingRights &= GameState::clearWhiteQueenSideMask;    
                case h1:
                    currentGameState.castlingRights &= GameState::clearWhiteKingSideMask; 
                case a8:
                    currentGameState.castlingRights &= GameState::clearBlackQueenSideMask; 
                case h8:
                    currentGameState.castlingRights &= GameState::clearBlackKingSideMask;    
            }
        }
    }

    is_in_check = inCheck();

    plyCount++;
    allGameMoves.push_back(move);
    gameStateHistory.push_back(currentGameState);
    is_white_move = !is_white_move;
    move_color = 1-move_color;
}

void Board::UnmakeMove(Move move, bool in_search) {
    is_white_move = !is_white_move;
    move_color = 1-move_color;

    // get move info
    int moved_from = move.StartSquare();
    int moved_to = move.TargetSquare();
    int move_flag = move.MoveFlag();

    int moved_piece = (move.IsPromotion() || move_flag==Move::enPassantCaptureFlag) ? pawn : getMovedPiece(moved_to);
    int captured_piece = currentGameState.capturedPieceType;
    int promotion_piece = move.PromotionPieceType();

    // update bitboards
    MovePiece(moved_piece, moved_to, moved_from);

    // undo promotion 
    if (move.IsPromotion()) 
        pop_bit(pieceBitboards[promotion_piece], moved_to);

    // undo captures
    if (move_flag == Move::enPassantCaptureFlag) {
        set_bit(pieceBitboards[pawn], moved_to + std::pow(-1,1-move_color) * 8);
        set_bit(colorBitboards[1-move_color], moved_to + std::pow(-1,1-move_color) * 8);
    } else if (captured_piece > -1) {
        set_bit(pieceBitboards[captured_piece], moved_to);
        set_bit(colorBitboards[1-move_color], moved_to);
    }
    // undo castling
    if (move_flag == Move::castleFlag) {
        if (moved_to == g1) MovePiece(rook, f1, h1);
        else if (moved_to == c1) MovePiece(rook, d1, a1);
        else if (moved_to == g8) MovePiece(rook, f8, h8);
        else if (moved_to == c8) MovePiece(rook, d8, a8);
    }

    // detect rules after undoing move
    is_in_check = inCheck();


    plyCount--;
    currentGameState.fiftyMoveCounter--;
    allGameMoves.pop_back();
    gameStateHistory.pop_back();
}

// piece and color
void Board::MovePiece(int piece, int start_square, int target_square) {
    set_bit(pieceBitboards[piece],target_square);
    pop_bit(pieceBitboards[piece],start_square);

    set_bit(colorBitboards[move_color],target_square);
    pop_bit(colorBitboards[move_color],start_square);
}

void Board::CapturePiece(int piece, int target_square, bool is_enpassant) {
    if (is_enpassant) {
        pop_bit(pieceBitboards[piece], target_square + std::pow(-1,1-move_color) * 8); // shift capture square by 1 rank
        pop_bit(colorBitboards[1-move_color], target_square + std::pow(-1,1-move_color) * 8);
    } else {
        pop_bit(pieceBitboards[piece], target_square);
        pop_bit(colorBitboards[1-move_color], target_square);
    }
}

void Board::PromoteToPiece(int piece, int target_square) {
    pop_bit(pieceBitboards[pawn], target_square);
    set_bit(pieceBitboards[piece], target_square); // side bitboards are set during movePiece, side doesnt change when changing piece type
}

int Board::getMovedPiece(int start_square) {
    for (int piece = 0; piece < 6; piece++) {
        if (get_bit(pieceBitboards[piece],start_square))
            return piece;
    }
    return -1;
}

int Board::getCapturedPiece(int target_square) {
    for (int piece = 0; piece < 6; piece++) {
        if (get_bit(pieceBitboards[piece],target_square))
            return piece;
    }
    return -1;
}

void Board::updateFiftyMoveCounter(int moved_piece, bool isCapture) {
    if (isCapture && moved_piece == -1) {
        currentGameState.fiftyMoveCounter = 0;
        //repetitionPosHistory.clear();
    } else {
        currentGameState.fiftyMoveCounter++;
        //repetitionPosHistory.push_back({colorBitboards[0],colorBitboards[1],pieceBitboards[0],pieceBitboards[1],pieceBitboards[2],pieceBitboards[3],pieceBitboards[4],pieceBitboards[5]});
    }
}

// called before side-to-move is updated
//      attacks are from move_color
// generate sliding, knight, pawn from king position
// for opp_sliding_piece: countBits((opp_square - king_square) & align_mask[opp][king] & occ) == 0 -> check
bool Board::inCheck() {
    int piece_square;
    U64 diag = colorBitboards[move_color] & (pieceBitboards[bishop] | pieceBitboards[queen]);
    U64 ortho = colorBitboards[move_color] & (pieceBitboards[rook] | pieceBitboards[queen]);
    U64 knights = colorBitboards[move_color] & pieceBitboards[knight];
    U64 pawns = colorBitboards[move_color] & pieceBitboards[pawn];
    U64 opp_king_bitboard = colorBitboards[1-move_color] & pieceBitboards[king];
    int opp_king_square = sqidx(opp_king_bitboard);

    // sliding pieces
    while (diag) {
        piece_square = tzcnt(diag) - 1;
        diag &= diag - 1;
        // even idx are ortho directions
        if (direction_idx(piece_square,opp_king_square) % 2 == 0) continue;

        if (opp_king_square > piece_square) {
            if ( countBits((opp_king_bitboard - (1ULL << piece_square)) & align_masks.alignMasks[piece_square][opp_king_square] & (colorBitboards[0]&colorBitboards[1])) > 1 ) {
                // opp_king_bitboard - (1ULL << piece_square) gives bits from piece_square to king_square-1 so 1 of the endpoints is included hence >1 
                return false;
            }
        } else {
            if ( countBits(((1ULL << piece_square) - opp_king_bitboard) & align_masks.alignMasks[piece_square][opp_king_square] & (colorBitboards[0]&colorBitboards[1])) > 1 ) {
                return false;
            }
        }
    }

    while (ortho) {
        piece_square = tzcnt(ortho) - 1;
        ortho &= ortho - 1;
        // even idx are ortho directions
        if (direction_idx(piece_square,opp_king_square) % 2 != 0) continue;

        if (opp_king_square > piece_square) {
            if ( countBits((opp_king_bitboard - (1ULL << piece_square)) & align_masks.alignMasks[piece_square][opp_king_square] & (colorBitboards[0]&colorBitboards[1])) == 1 ) {
                // opp_king_bitboard - (1ULL << piece_square) gives bits from piece_square to king_square-1 so 1 of the endpoints is included hence =1 
                return true;
            }
        } else {
            if ( countBits(((1ULL << piece_square) - opp_king_bitboard) & align_masks.alignMasks[piece_square][opp_king_square] & (colorBitboards[0]&colorBitboards[1])) == 1 ) {
                return true;
            }
        }
    }

    // knights
    if (knights & align_masks.blankKnightAttacks[opp_king_square]) 
        return true;

    // pawns (treat enemy king as enemy pawn, if this intersects of pawns then king is in pawn attack)
    if (pawns & align_masks.fullPawnAttacks[opp_king_square][1-move_color])
        return true;

    return false;
}

void Board::print_board() {
    // Create a character array to store the board representation
    char pieces[64]; 

    // Iterate through all 64 squares on the board
    for (int i = 64; i >= 0; i--) {
        pieces[i] = '.'; // Default to empty square ('.')
        
        // Check for each piece type (pawn, knight, bishop, rook, queen, king)
        for (int piece = 0; piece < 6; piece++) {
            if (get_bit(pieceBitboards[piece], i)) {
                // Check for white or black pieces and assign the appropriate character
                if (get_bit(colorBitboards[0], i)) {
                    switch (piece) {
                        case 0: pieces[i] = 'P'; break; 
                        case 1: pieces[i] = 'N'; break; 
                        case 2: pieces[i] = 'B'; break;
                        case 3: pieces[i] = 'R'; break; 
                        case 4: pieces[i] = 'Q'; break; 
                        case 5: pieces[i] = 'K'; break; 
                    }
                } else {
                    switch (piece) {
                        case 0: pieces[i] = 'p'; break; 
                        case 1: pieces[i] = 'n'; break; 
                        case 2: pieces[i] = 'b'; break; 
                        case 3: pieces[i] = 'r'; break; 
                        case 4: pieces[i] = 'q'; break; 
                        case 5: pieces[i] = 'k'; break; 
                    }
                }
            }
        }
    }

    // Print the board 
    for (int row = 7; row >= 0; row--) {
        std::cout << row + 1 << "\t";
        for (int col = 0; col <8; col++) {
            std::cout << pieces[row * 8 + col] << "  "; 
        }
        std::cout << std::endl; 
    }
    std::cout << "\n \ta  b  c  d  e  f  g  h\n" << std::endl;

    // Print gamestate info
    std::cout << "\nMove: " << plyCount/2 << std::endl;
    std::cout << (is_white_move ? "White to move" : "Black to move") << std::endl;
    std::cout << (is_in_check ? "Check" : "") << std::endl;
    std::cout << "Castling Rights: " << currentGameState.castlingRights << std::endl;
    std::cout << "50 Move Counter: " << currentGameState.FiftyMoveCounter() << std::endl;
    std::cout << "En Passant: " << (currentGameState.enPassantFile > -1) << std::endl;
}


