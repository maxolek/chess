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
    //setBoardFEN(); 
    initZobristKeys();
    zobrist_hash = computeZobristHash();
    hash_history[zobrist_hash] = 1;
}

Board::Board(std::string _fen) {
    setFromFEN(_fen);
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

    is_in_check = inCheck(true);
    //setBoardFEN();
    initZobristKeys();
    zobrist_hash = computeZobristHash();
    hash_history[zobrist_hash] = 1;
}


U64 Board::randomU64() {
    std::mt19937_64 rng(42); 
    return rng();
}

void Board::initZobristKeys() {
    std::mt19937_64 rng(42); 

    for (int piece = 0; piece < 12; ++piece) {
        for (int square = 0; square < 64; ++square) {
            zobrist_table[piece][square] = rng();
        }
    }

    for (int i = 0; i < 4; ++i) {
        zobrist_castling[i] = rng();
    }

    for (int i = 0; i < 8; ++i) {
        zobrist_enpassant[i] = rng();
    }

    zobrist_side_to_move = rng();
}

// right now using this at the end of make/unmake move
// performance hit of 5-10% so worth cleaning up but to just get it working
// we use this
U64 Board::computeZobristHash() {
    U64 hash = 0;

    for (int sq = 0; sq < 64; ++sq) {
        int piece = getMovedPiece(sq);
        int color = getSideAt(sq);
        if (piece != -1) {
            int pieceIndex = (color == 0 ? 0 : 6) + piece;
            hash ^= zobrist_table[pieceIndex][sq];
        }
    }

    if (!is_white_move)
        hash ^= zobrist_side_to_move;

    // Castling rights
    if (currentGameState.HasKingsideCastleRight(true))
        hash ^= zobrist_castling[0]; // K
    if (currentGameState.HasQueensideCastleRight(true))
        hash ^= zobrist_castling[1]; // Q
    if (currentGameState.HasKingsideCastleRight(false))
        hash ^= zobrist_castling[2]; // k
    if (currentGameState.HasQueensideCastleRight(false))
        hash ^= zobrist_castling[3]; // q

    // En passant file
    int epFile = currentGameState.enPassantFile;
    if (epFile >= 0 && epFile < 8)
        hash ^= zobrist_enpassant[epFile];

    return hash;
}
/*
U64 Board::updateHash(
    U64 currentHash,
    int start_square, int target_square, int move_flag, bool is_promotion, int promotion_piece,
    int moved_piece, int captured_piece, bool is_enpassant, int old_castling_rights
) {
    int color = is_white_move ? 0 : 1;

    // XOR out moving piece from source
    currentHash ^= zobrist_table[color * 6 + moved_piece][start_square];

    // XOR in moving piece at destination
    currentHash ^= zobrist_table[color * 6 + moved_piece][target_square];

    // XOR out captured piece (if not en passant)
    if (captured_piece != -1 && move_flag != Move::enPassantCaptureFlag) {
        currentHash ^= zobrist_table[(1 - color) * 6 + captured_piece][target_square];
    }

    // XOR out en passant pawn (if en passant)
    if (move_flag == Move::enPassantCaptureFlag) {
        int ep_pawn_square = target_square + (color == 0 ? -8 : 8);
        currentHash ^= zobrist_table[(1 - color) * 6 + 0][ep_pawn_square];
    }

    // XOR castling rook move
    if (move_flag == Move::castleFlag) {
        if (target_square == g1) { // white kingside
            currentHash ^= zobrist_table[rook][h1];
            currentHash ^= zobrist_table[rook][f1];
        } else if (target_square == c1) { // white queenside
            currentHash ^= zobrist_table[rook][a1];
            currentHash ^= zobrist_table[rook][d1];
        } else if (target_square == g8) { // black kingside
            currentHash ^= zobrist_table[rook + 6][h8];
            currentHash ^= zobrist_table[rook + 6][f8];
        } else if (target_square == c8) { // black queenside
            currentHash ^= zobrist_table[rook + 6][a8];
            currentHash ^= zobrist_table[rook + 6][d8];
        }
    }

    // XOR promotion
    if (is_promotion) {
        // remove pawn
        currentHash ^= zobrist_table[color * 6 + pawn][target_square];
        // add promoted piece
        currentHash ^= zobrist_table[color * 6 + promotion_piece][target_square];
    }

    // XOR old en passant file if any
    if (currentGameState.enPassantFile != -1) {
        currentHash ^= zobrist_enpassant[currentGameState.enPassantFile];
    }

    // XOR new en passant file if applicable
    if (move_flag == Move::pawnTwoUpFlag) {
        int ep_file = start_square % 8;
        currentHash ^= zobrist_enpassant[ep_file];
    }

    // XOR castling rights if they changed
    currentHash ^= zobrist_castling[old_castling_rights];
    currentHash ^= zobrist_castling[currentGameState.castlingRights]; // must calculate this manually

    // XOR side to move
    currentHash ^= zobrist_side_to_move;

    return currentHash;
}
*/


// inSearch controls whether this move is recorded in the game history
// (for three-fold repetition)
void Board::MakeMove(Move move, bool in_search) {
    // get info about move
    int start_square = move.StartSquare();
    int target_square = move.TargetSquare();
    int move_flag = move.MoveFlag();
    bool is_promotion = move.IsPromotion();
    bool is_enpassant = (move_flag == Move::enPassantCaptureFlag);

    int moved_piece = getMovedPiece(start_square);
    int captured_piece = is_enpassant ? 0 : getCapturedPiece(target_square);
    int promotion_piece = move.PromotionPieceType();


    // update bitboards
    MovePiece(moved_piece, start_square, target_square);
    //std::cout << "moved: " << moved_piece << "\t" << start_square << "\t" << target_square << std::endl;
    if (captured_piece > -1) { CapturePiece(captured_piece, target_square, is_enpassant, moved_piece == captured_piece); }

    // castling
    if (move_flag == Move::castleFlag) {
        if (!move_color) {
            if (target_square == g1) MovePiece(rook, h1, f1);
            else MovePiece(rook, a1, d1);
        } else {
            if (target_square == g8) MovePiece(rook, h8, f8);
            else MovePiece(rook, a8, d8);
        }
    }
        // promotion
    if (is_promotion) 
        PromoteToPiece(promotion_piece, target_square);

    // update game state
    currentGameState.capturedPieceType = captured_piece;
    // zobrist enpassant (remove old)
    if (currentGameState.enPassantFile != -1) 
        zobrist_hash ^= zobrist_enpassant[currentGameState.enPassantFile];
    currentGameState.enPassantFile = (move_flag == Move::pawnTwoUpFlag) ? start_square % 8 : -1;
    updateFiftyMoveCounter(moved_piece, captured_piece > -1, false);
    // zobrist ep (add new)
    if (currentGameState.enPassantFile != -1) 
        zobrist_hash ^= zobrist_enpassant[currentGameState.enPassantFile];

    // zobrist
    zobrist_hash ^= zobrist_castling[currentGameState.castlingRights];
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
                    break;
                case h1:
                    currentGameState.castlingRights &= GameState::clearWhiteKingSideMask; 
                    break;
                case a8:
                    currentGameState.castlingRights &= GameState::clearBlackQueenSideMask; 
                    break;
                case h8:
                    currentGameState.castlingRights &= GameState::clearBlackKingSideMask;    
                    break;
            }
        }
        if (captured_piece == rook) {
            switch (target_square) {
                case a1:
                    currentGameState.castlingRights &= GameState::clearWhiteQueenSideMask;    
                    break;
                case h1:
                    currentGameState.castlingRights &= GameState::clearWhiteKingSideMask; 
                    break;
                case a8:
                    currentGameState.castlingRights &= GameState::clearBlackQueenSideMask; 
                    break;
                case h8:
                    currentGameState.castlingRights &= GameState::clearBlackKingSideMask;    
                    break;
            }
        }
    }
    // zobrist castling
    zobrist_hash ^= zobrist_castling[currentGameState.castlingRights];

    is_in_check = inCheck(false);
    plyCount++;
    allGameMoves.push_back(move);
    gameStateHistory.push_back(currentGameState);
    is_white_move = !is_white_move;
    move_color = 1-move_color;
    zobrist_hash ^= zobrist_side_to_move;
    setBoardFEN();
    //zobrist_hash = computeZobristHash();
    hash_history[zobrist_hash]++;
}

void Board::UnmakeMove(Move move, bool in_search) {
    hash_history[zobrist_hash]--;
    if (hash_history[zobrist_hash] == 0)
        hash_history.erase(zobrist_hash); // save memory
    is_white_move = !is_white_move;
    move_color = 1-move_color;
    zobrist_hash ^= zobrist_side_to_move;
        
    // get move info
    int moved_from = move.StartSquare();
    int moved_to = move.TargetSquare();
    int move_flag = move.MoveFlag();

    int moved_piece = (move.IsPromotion() || move_flag==Move::enPassantCaptureFlag || move_flag==Move::pawnTwoUpFlag) ? pawn : getMovedPiece(moved_to);
    int captured_piece = currentGameState.capturedPieceType;
    //if (move_flag == Move::pawnTwoUpFlag) currentGameState.enPassantFile = -1;
    int promotion_piece = move.PromotionPieceType();

    // zobrist
    zobrist_hash ^= zobrist_castling[currentGameState.castlingRights];

    // update bitboards
    MovePiece(moved_piece, moved_to, moved_from);
    //std::cout << "unmoved: " << moved_piece << "\t" << moved_to << "\t" << moved_from << std::endl;

    // undo promotion 
    if (move.IsPromotion()) {
        pop_bit(pieceBitboards[promotion_piece], moved_to);
        zobrist_hash ^= zobrist_table[move_color * 6 + promotion_piece][moved_to]; // already removed in bitboard
    }

    // undo captures
    if (move_flag == Move::enPassantCaptureFlag) {
        int ep_square = moved_to + ((move_color == 0) ? -8 : 8);
        set_bit(pieceBitboards[pawn], ep_square);
        set_bit(colorBitboards[1 - move_color], ep_square);
        zobrist_hash ^= zobrist_table[(1 - move_color) * 6 + pawn][ep_square];
    } else if (captured_piece > -1) {
        set_bit(pieceBitboards[captured_piece], moved_to);
        set_bit(colorBitboards[1 - move_color], moved_to);
        zobrist_hash ^= zobrist_table[(1 - move_color) * 6 + captured_piece][moved_to];
    }
    // undo castling
    if (move_flag == Move::castleFlag) {
        if (moved_to == g1) MovePiece(rook, f1, h1);
        else if (moved_to == c1) MovePiece(rook, d1, a1);
        else if (moved_to == g8) MovePiece(rook, f8, h8);
        else if (moved_to == c8) MovePiece(rook, d8, a8);
    }

    // detect rules after undoing move
    is_in_check = inCheck(true);
    gameStateHistory.pop_back();
    currentGameState = gameStateHistory.back();
    // zobrist
    zobrist_hash ^= zobrist_castling[currentGameState.castlingRights];
    //zobrist_hash = computeZobristHash();
    plyCount--;
    //updateFiftyMoveCounter(-1,false,true); // -1/false cause it doesnt matter
    allGameMoves.pop_back();
    //setBoardFEN();
}

// piece and color
void Board::MovePiece(int piece, int start_square, int target_square) {
    set_bit(pieceBitboards[piece],target_square);
    pop_bit(pieceBitboards[piece],start_square);

    set_bit(colorBitboards[move_color],target_square);
    pop_bit(colorBitboards[move_color],start_square);

    zobrist_hash ^= zobrist_table[move_color * 6 + piece][start_square];
    zobrist_hash ^= zobrist_table[move_color*6 + piece][target_square];
}

void Board::CapturePiece(int piece, int target_square, bool is_enpassant, bool captured_is_moved_piece) {
    if (is_enpassant) {
        pop_bit(pieceBitboards[piece], target_square + std::pow(-1,1-move_color) * 8); // shift capture square by 1 rank
        pop_bit(colorBitboards[1-move_color], target_square + std::pow(-1,1-move_color) * 8);
        zobrist_hash ^= zobrist_table[(1-move_color)*6 + piece][int(target_square + std::pow(-1,1-move_color)*8)];
    } else if (captured_is_moved_piece) {
        pop_bit(colorBitboards[1-move_color], target_square);
        zobrist_hash ^= zobrist_table[(1-move_color)*6 + piece][target_square];
    } else {
        pop_bit(pieceBitboards[piece], target_square);
        pop_bit(colorBitboards[1-move_color], target_square);
        zobrist_hash ^= zobrist_table[(1-move_color)*6 + piece][target_square];
    }
}

void Board::PromoteToPiece(int piece, int target_square) {
    pop_bit(pieceBitboards[pawn], target_square);
    set_bit(pieceBitboards[piece], target_square); // side bitboards are set during movePiece, side doesnt change when changing piece type

    zobrist_hash ^= zobrist_table[move_color*6 + piece][target_square];
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

int Board::getSideAt(int square) {
    for (int side = 0; side < 2; side++) {
        if (colorBitboards[side] & (1ULL << square))
            return side;
    }
    return -1;
}

int Board::getPieceAt(int square, int side) {
    for (int piece = 0; piece < 6; piece++) {
        if (get_bit(pieceBitboards[piece],square))
            return piece + side*10; 
            // white {0} : {0,1,2,3,4,5}
            // black {1} : {10,11,12,13,14,15}
    }
    return -1;
}

void Board::updateFiftyMoveCounter(int moved_piece, bool isCapture, bool unmake) {
    if (isCapture || moved_piece == pawn) {
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
bool Board::inCheck(bool init) {
    int piece_square, bits; 
    int side = init ? 1-move_color : move_color; // the checking side 

    U64 diag = colorBitboards[side] & (pieceBitboards[bishop] | pieceBitboards[queen]);
    U64 ortho = colorBitboards[side] & (pieceBitboards[rook] | pieceBitboards[queen]);
    U64 knights = colorBitboards[side] & pieceBitboards[knight];
    U64 pawns = colorBitboards[side] & pieceBitboards[pawn];
    U64 king_bitboard = colorBitboards[1-side] & pieceBitboards[king];
    int king_square = sqidx(king_bitboard);

    // sliding pieces
    while (diag) {
        piece_square = tzcnt(diag) - 1;
        diag &= diag - 1;
        // if not aligned then move to next piece
        if (!PrecomputedMoveData::alignMasks[piece_square][king_square]) continue;
        // diag dir (odd idx)
        if (direction_index(piece_square, king_square) % 2 == 0) continue;

        if (king_square > piece_square) { // switch to ray_masks ? alignMasks should be working, so this should be tested more
            bits = countBits((king_bitboard - (1ULL << piece_square)) & PrecomputedMoveData::rayMasks[piece_square][king_square] & (colorBitboards[0] | colorBitboards[1]));
            if ( bits == 1 ) {
                //std::cout << square_to_algebraic(piece_square) << "\t" << square_to_algebraic(opp_king_square) << std::endl;
                //print_bitboard(PrecomputedMoveData::alignMasks[piece_square][opp_king_square]);
                // opp_king_bitboard - (1ULL << piece_square) gives bits from piece_square to king_square-1 so 1 of the endpoints is included hence >1 
                return true;
            }
        } else {
            bits = countBits(((1ULL << piece_square) - king_bitboard) & PrecomputedMoveData::rayMasks[piece_square][king_square] & (colorBitboards[0] | colorBitboards[1]));
            if ( bits == 1 ) {
                //std::cout << square_to_algebraic(piece_square) << "\t" << square_to_algebraic(opp_king_square) << std::endl;
                //print_bitboard(PrecomputedMoveData::alignMasks[piece_square][opp_king_square]);
                return true;
            }
        }
    }

    while (ortho) {
        piece_square = tzcnt(ortho) - 1;
        ortho &= ortho - 1;
        // even idx are ortho directions
        if (direction_index(piece_square, king_square) % 2 == 1) continue;
        // not aligned
        if (PrecomputedMoveData::alignMasks[piece_square][king_square] == 0) continue;

        if (king_square > piece_square) {
            bits = countBits((king_bitboard - (1ULL << piece_square)) & PrecomputedMoveData::rayMasks[piece_square][king_square] & (colorBitboards[0] | colorBitboards[1])) == 1;
            if ( bits == 1 ) {
                // opp_king_bitboard - (1ULL << piece_square) gives bits from piece_square to king_square-1 so 1 of the endpoints is included hence =1 
                return true;
            }
        } else {
            bits = countBits(((1ULL << piece_square) - king_bitboard) & PrecomputedMoveData::rayMasks[piece_square][king_square] & (colorBitboards[0] | colorBitboards[1])) == 1;
            if ( bits == 1 ) {
                return true;
            }
        }
    }

    // knights
    if (knights & PrecomputedMoveData::blankKnightAttacks[king_square]) {
        return true;
    }

    // pawns (treat enemy king as enemy pawn, if this intersects of pawns then king is in pawn attack)
    if (pawns & PrecomputedMoveData::fullPawnAttacks[king_square][1-side]) {
        return true;
    }

    return false;
}

std::string Board::getBoardFEN() {
    setBoardFEN();
    return fen;
}

void Board::setFromFEN(std::string _fen) {
    // default fen: rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
    std::istringstream fenStream(_fen);
    std::string boardState;
    std::string turn;
    std::string castling_rights;
    std::string ep;
    int fifty_move = 0;
    int full_moves = 1;

    fen = _fen;
    fenStream >> boardState >> turn >> castling_rights >> ep >> fifty_move >> full_moves;

    // reset state
    currentGameState = GameState();
    for (int _ = 0; _ < 6; _++) {pieceBitboards[_] = 0ULL;}
    for (int _ = 0; _ < 2; _++) {colorBitboards[_] = 0ULL;}

    // physical state
    // rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR
    int row = 7;
    int col = 0;
    for (char square : boardState) {
        if (square == '/') {
            row--;
            col = 0;
        } else if (isdigit(square)) {
            col += square - '0';
        } else {
            pieceBitboards[piece_int(tolower(square))] |= (1ULL << (row*8+col));
            if (std::islower(square)) {
                colorBitboards[1] |= (1ULL << (row*8+col));
            } else {
                colorBitboards[0] |= (1ULL << (row*8+col));
            }
            col++;
        }
    }

    // non-visual state
    //  w KQkq - 0 1
    is_white_move = (turn == "w");
    currentGameState.castlingRights = 0;
    if (castling_rights != "-") {
        if (castling_rights.find("K") != std::string::npos) currentGameState.castlingRights |= 0x1;
        if (castling_rights.find("Q") != std::string::npos) currentGameState.castlingRights |= 0x2;
        if (castling_rights.find("k") != std::string::npos) currentGameState.castlingRights |= 0x4;
        if (castling_rights.find("q") != std::string::npos) currentGameState.castlingRights |= 0x8;
    }
    currentGameState.enPassantFile = (ep == "-") ? -1 : file_char.find(ep.substr(0,1));
    currentGameState.fiftyMoveCounter = int(fifty_move);
    plyCount = is_white_move ? full_moves*2 : full_moves*2+1;

    // self generated
    move_color = is_white_move ? 0 : 1;
    is_in_check = inCheck(true);
    gameStateHistory.push_back(currentGameState);

    initZobristKeys();
    zobrist_hash = computeZobristHash();
}

void Board::setBoardFEN() {
    fen = "";
    int emptyCount = 0;
    int square_index;
    char piece;

    // board state
    for (int rank = 7; rank >= 0; rank--) {
        emptyCount = 0;
        for (int file = 0; file < 8; file++) {
            square_index = rank*8 + file;
            
            piece = piece_label(getPieceAt(square_index, getSideAt(square_index)));
            if (piece != '.') {
                if (emptyCount > 0) {
                    fen += std::to_string(emptyCount);
                    emptyCount = 0;
                }
                fen += piece;
            } else {
                emptyCount++;
            }
        }

        if (emptyCount > 0) 
            fen += std::to_string(emptyCount);
        if (rank > 0)
            fen += '/';
    }

    // gamestate
    fen += ' ';
    fen += is_white_move ? 'w' : 'b';
    // castling rights
    std::string castling_rights = "";
    if (currentGameState.HasKingsideCastleRight(true)) castling_rights += "K";
    if (currentGameState.HasQueensideCastleRight(true)) castling_rights += "Q";
    if (currentGameState.HasKingsideCastleRight(false)) castling_rights += "k";
    if (currentGameState.HasQueensideCastleRight(false)) castling_rights += "q";
    if (castling_rights == "") castling_rights = "-";
    fen += ' ';
    fen += castling_rights;
    //en passant
    if (currentGameState.enPassantFile == -1) fen += " -";
    else {
        fen += " "; 
        fen += (file_char[currentGameState.enPassantFile]);
        fen += is_white_move ? "6" : "3";
    }
    // moves
    fen += " ";
    fen += std::to_string(currentGameState.fiftyMoveCounter);
    fen += " ";
    fen += std::to_string(plyCount/2);
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
    if (allGameMoves.size() != 0) {allGameMoves.back().PrintMove();}
    std::cout << "\n" << std::endl;
    for (int row = 7; row >= 0; row--) {
        std::cout << row + 1 << "  ";  // Row label with extra space
        for (int col = 0; col < 8; col++) {
            // Print each piece with more spacing between them
            std::cout << "[" << pieces[row * 8 + col] << "] "; 
        }
        std::cout << std::endl; 
    }
    
    // Print the column labels
    std::cout << "\n    a   b   c   d   e   f   g   h\n" << std::endl;

    // Print gamestate info
    setBoardFEN();
    std::cout << fen << std::endl;
    std::cout << "\nMove: " << plyCount/2 << std::endl;
    std::cout << (is_white_move ? "White to move" : "Black to move") << std::endl;
    std::cout << (is_in_check ? "Check" : "") << std::endl;
    std::cout << "Castling Rights: " << currentGameState.castlingRights << std::endl;
    std::cout << "50 Move Counter: " << currentGameState.fiftyMoveCounter << std::endl;
    std::cout << "En Passant: " << currentGameState.enPassantFile << "\n\n" << std::endl;
}


