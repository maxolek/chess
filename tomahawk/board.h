#ifndef BOARD_H
#define BOARD_H

#include "PrecomputedMoveData.h"
#include "move.h"
#include "gamestate.h"

/**
 * @class Board
 * @brief Represents the current state of a chess game, including piece positions, side to move,
 *        castling rights, en-passant square, Zobrist hash, and move history.
 *
 * The board can be initialized from a FEN string or a default starting position.
 * Moves can be made/unmade with MakeMove/UnmakeMove, updating the internal state.
 */
class Board {
public:
    // ==================== Bitboards ====================
    U64 colorBitboards[2];   ///< [0] = white, [1] = black
    U64 pieceBitboards[6];   ///< 0=pawn, 1=knight, 2=bishop, 3=rook, 4=queen, 5=king
    int sqToPiece[64];       ///< Maps square to piece index (0..11), -1 if empty

    // ==================== Game state ====================
    GameState currentGameState;          ///< Tracks castling, en-passant, fifty-move counter
    std::string fen;                     ///< Current board FEN
    int plyCount;                        ///< Number of half-moves played
    bool is_white_move;                  ///< True if white to move
    int move_color;                      ///< 0=white, 1=black
    bool is_in_check;                    ///< True if the side to move is in check

    // ==================== Move & position history ====================
    std::vector<Move> allGameMoves;            ///< All moves played
    std::vector<GameState> gameStateHistory;  ///< Game state history for unmaking moves
    std::unordered_map<U64,int> hash_history; ///< Zobrist hash occurrences for repetition detection

    // ==================== Castling trackers ====================
    bool white_castled = false; ///< True if white has castled
    bool black_castled = false; ///< True if black has castled

    // ==================== Zobrist hashing ====================
    U64 zobrist_hash;                ///< Current Zobrist hash
    U64 zobrist_table[12][64];       ///< Zobrist keys for 12 pieces x 64 squares
    U64 zobrist_side_to_move;        ///< Side to move key
    U64 zobrist_castling[4];         ///< Castling rights keys: KQkq
    U64 zobrist_enpassant[8];        ///< En passant file keys: a-h

    // ==================== Constructors ====================
    Board();                 ///< Default starting position
    Board(std::string _fen); ///< Initialize from FEN

    // ==================== Move execution ====================
    void MakeMove(Move move = false);   ///< Apply a move and update board state
    void UnmakeMove(Move move = false); ///< Undo a move

    // ==================== Piece manipulation ====================
    void putPiece(int pt12, int sq);         ///< Place a piece on a square
    void removePiece(int sq);                ///< Remove a piece from a square
    void MovePiece(int piece, int start, int target); ///< Move a piece between squares
    void CapturePiece(int piece, int target, bool is_enpassant, bool captured_is_moved_piece);
    void PromoteToPiece(int piece, int target); ///< Promote pawn to another piece

    // ==================== Piece queries ====================
    int getMovedPiece(int start_square) const;
    int getCapturedPiece(int target_square) const;
    int getSideAt(int square) const;
    int getPieceAt(int square, int side) const;
    int kingSquare(bool white) const;

    // ==================== Special move checks ====================
    bool canEnpassantCapture(int epFile) const; ///< Checks if en-passant is possible
    void updateFiftyMoveCounter(int moved_piece, bool isCapture);

    // ==================== Board state checks ====================
    bool inCheck(bool init); ///< True if the given side is in check

    // ==================== FEN handling ====================
    void setFromFEN(std::string _fen); ///< Initialize board from FEN
    void setBoardFEN();                 ///< Generate FEN string from current state
    std::string getBoardFEN();          ///< Return current board FEN

    // ==================== Zobrist helper functions ====================
    U64 randomU64();                           ///< Generate random 64-bit number
    void initZobristKeys();                    ///< Initialize Zobrist keys
    U64 computeZobristHash();                  ///< Compute current Zobrist hash
    U64 zobristCastlingHash(int castling_rights); ///< Hash from castling rights
    void debugZobristDifference(uint64_t old_hash, uint64_t new_hash);

    // ==================== Debug ====================
    void print_board() const; ///< Pretty-print board with FEN and additional info
};

#endif // BOARD_H
