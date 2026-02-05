"""
get characteristics of a position
"""

import chess
from collections import Counter

# pawn value of pieces (approx)
piece_values = {
    'p': 1,
    'n': 3,
    'b': 3,
    'r': 5,
    'q': 9,
    'k': 10000
}

# piece contributions to game phasing
phase_values = {
    'p': 0,
    'n': 1,
    'b': 1,
    'r': 2,
    'q': 4,
    'k': None
}

# HELPERS

def is_passed(board: chess.Board, square: chess.Square, color: bool) -> bool:
    """
    Determines if the pawn on `square` is a passed pawn.
    
    A pawn is passed if there are **no enemy pawns** in the same file or adjacent files
    ahead of it (toward promotion).
    
    Parameters:
        board : chess.Board
        square : chess.Square
        color : True for White, False for Black
    
    Returns:
        bool : True if passed pawn, False otherwise
    """
    rank = chess.square_rank(square)
    file = chess.square_file(square)

    # Ranks ahead depending on color
    if color == chess.WHITE:
        ranks_ahead = range(rank+1, 8)
    else:
        ranks_ahead = range(rank-1, -1, -1)

    # Files to check: same file + adjacent
    files_to_check = [f for f in [file-1, file, file+1] if 0 <= f <= 7]

    # Check each square ahead for enemy pawns
    for r in ranks_ahead:
        for f in files_to_check:
            sq = chess.square(f, r)
            piece = board.piece_at(sq)
            if piece and piece.piece_type == chess.PAWN and piece.color != color:
                return False
    return True


# GAME CHARACTERISTICS

def get_game_phase(fen):
    """
    Returns 'opening', 'midgame', or 'endgame' based on the position.
    uses material based game phasing
    """
    board = chess.Board(fen)
    ply = board.fullmove_number * 2
    if not board.turn:
        ply -= 1  # subtract 1 if black to move, since fullmove_number counts full moves

    # Count pieces
    piece_counts = Counter()
    for square in chess.SQUARES:
        piece = board.piece_at(square)
        if piece:
            piece_counts[piece.symbol().lower()] += 1

    # Compute material-based phase
    # TotalPhase = phase at start of game
    starting_phase = (phase_values['p']*16 + phase_values['n']*4 + 
                      phase_values['b']*4 + phase_values['r']*4 + 
                      phase_values['q']*2)

    phase = starting_phase
    phase -= piece_counts.get('p',0)*phase_values['p']
    phase -= piece_counts.get('n',0)*phase_values['n']
    phase -= piece_counts.get('b',0)*phase_values['b']
    phase -= piece_counts.get('r',0)*phase_values['r']
    phase -= piece_counts.get('q',0)*phase_values['q']

    # Thresholds for 3-phase model
    opening_phase_threshold = 0.25 * starting_phase
    middle_phase_threshold  = 2/3 * starting_phase
    opening_ply_max = 30  # first 15 moves

    if phase < opening_phase_threshold and ply <= opening_ply_max:
        return "opening"
    elif phase < middle_phase_threshold:
        return "midgame"
    else:
        return "endgame"

def get_position_type(fen):
    board = chess.Board(fen)
    legal_moves = list(board.legal_moves)
    
    # Tactical proxy: ratio of captures/checks
    capture_moves = sum(board.is_capture(move) for move in legal_moves)
    check_moves = sum(board.gives_check(move) for move in legal_moves)
    tactical_ratio = (capture_moves + check_moves) / max(len(legal_moves), 1)
    
    if tactical_ratio > 0.2:  # heuristic threshold
        return "tactical"
    else:
        return "strategic"

def get_position_balance(fen):
    board = chess.Board(fen)
    balance = 0
    for square in chess.SQUARES:
        piece = board.piece_at(square)
        if piece:
            value = piece_values[piece.symbol().lower()]
            if piece.color == chess.WHITE:
                balance += value
            else:
                balance -= value
    return balance


def get_pawn_characteristics(fen):
    board = chess.Board(fen)
    pawns = {chess.WHITE: [], chess.BLACK: []}
    for square in chess.SQUARES:
        piece = board.piece_at(square)
        if piece and piece.piece_type == chess.PAWN:
            pawns[piece.color].append(square)

    
    
    def analyze(color):
        backward = 0
        doubled = 0
        passed = 0
        files = [square % 8 for square in pawns[color]]
        counts = Counter(files)
        doubled = sum(v-1 for v in counts.values() if v > 1)
        # passed/backward detection simplified
        # can improve with pawn masks
        passed = sum(1 for sq in pawns[color] if is_passed(board, sq, color))
        backward = len(pawns[color]) - passed - doubled
        return backward, doubled, passed
    
    return analyze(chess.WHITE), analyze(chess.BLACK)


def get_king_safety(fen):
    board = chess.Board(fen)
    
    def king_metrics(color):
        king_square = board.king(color)
        rank, file = divmod(king_square, 8)
        
        # Pawn shield
        shield_squares = []
        if color == chess.WHITE:
            if rank+1 < 8:
                shield_squares = [chess.square(f, rank+1) for f in range(max(file-1,0), min(file+2,8))]
        else:
            if rank-1 >= 0:
                shield_squares = [chess.square(f, rank-1) for f in range(max(file-1,0), min(file+2,8))]
        shield_pawns = sum(1 for sq in shield_squares if board.piece_at(sq) and 
                           board.piece_at(sq).piece_type == chess.PAWN and board.piece_at(sq).color==color)
        
        # Open files near king
        open_files = sum(1 for f in range(max(file-1,0), min(file+2,8)) 
                         if not any(board.piece_at(chess.square(f, r)) for r in range(8)))
        
        # Tropism: sum of weighted inverse-distance of enemy pieces to king
        tropism = 0
        for sq in chess.SQUARES:
            piece = board.piece_at(sq)
            if piece and piece.color != color:
                pr, pf = divmod(sq, 8)
                distance = abs(rank-pr) + abs(file-pf)  # Manhattan distance
                distance = max(distance,1)  # avoid div by zero
                tropism += piece_values[piece.symbol().lower()] / distance
        
        return {'shield_pawns': shield_pawns, 'open_files': open_files, 'tropism': tropism}
    
    return king_metrics(chess.WHITE), king_metrics(chess.BLACK)



def get_mobility_characteristics(fen):
    board = chess.Board(fen)
    mobility = {}
    
    for color in [chess.WHITE, chess.BLACK]:
        legal_moves = list(board.legal_moves)
        # Only consider moves by this color
        legal_moves = [m for m in legal_moves if board.piece_at(m.from_square).color == color]
        capture_moves = sum(board.is_capture(m) for m in legal_moves)
        check_moves = sum(board.gives_check(m) for m in legal_moves)

        # Enemy territory squares
        if color == chess.WHITE:
            enemy_squares = set(range(32,64))  # ranks 4-7
        else:
            enemy_squares = set(range(0,32))   # ranks 0-3
        
        # Legal moves into enemy territory
        legal_enemy = sum(1 for m in legal_moves if m.to_square in enemy_squares)
        
        # Controlled squares in enemy territory
        controlled_squares = set()
        for sq in chess.SQUARES:
            piece = board.piece_at(sq)
            if piece and piece.color == color:
                controlled_squares.update(board.attacks(sq) & enemy_squares)
        
        mobility[color] = {
            'num_moves': len(legal_moves),
            'capture_ratio': capture_moves / max(len(legal_moves),1),
            'check_ratio': check_moves / max(len(legal_moves),1),
            'legal_enemy': legal_enemy,
            'controlled_enemy': len(controlled_squares)
        }
    
    return mobility[chess.WHITE], mobility[chess.BLACK]

def build_position_features(cnxn):
    # Drop and recreate table (DuckDB syntax)
    cnxn.execute("""
        CREATE OR REPLACE TABLE position_features (
            search_id   INTEGER,
            game_id     INTEGER,
            fen         TEXT,
            game_phase  TEXT,
            position_type TEXT,
            balance     INTEGER,
            white_backwards INTEGER,
            white_doubled INTEGER,
            white_passed INTEGER,
            black_backwards INTEGER,
            black_doubled INTEGER,
            black_passed INTEGER,
            white_shield_pawns INTEGER,
            white_open_files INTEGER,
            white_tropism FLOAT,
            black_shield_pawns INTEGER,
            black_open_files INTEGER,
            black_tropism FLOAT,
            white_num_moves INTEGER,
            white_capture_ratio FLOAT,
            white_check_ratio FLOAT,
            white_legal_enemy INTEGER,
            white_controlled_enemy INTEGER,
            black_num_moves INTEGER,
            black_capture_ratio FLOAT,
            black_check_ratio FLOAT,
            black_legal_enemy INTEGER,
            black_controlled_enemy INTEGER
        )
    """)

    # Fetch all positions (from your intermediate table)
    rows = cnxn.execute("SELECT search_id, fen, game_id, sts_id FROM dim_positions").fetchall()

    for row in rows:
        search_id, fen, game_id, sts_id = row

        # Extract features
        game_phase = get_game_phase(fen)
        position_type = get_position_type(fen)
        balance = get_position_balance(fen)

        # Pawns
        wp, bp = get_pawn_characteristics(fen)
        w_backward, w_doubled, w_passed = wp
        b_backward, b_doubled, b_passed = bp

        # King safety + tropism
        ws, bs = get_king_safety(fen)

        # Mobility + space
        wm, bm = get_mobility_characteristics(fen)

        # Flatten into a dict
        features = {
            'search_id': search_id,
            'game_id': game_id,
            'fen': fen,
            'game_phase': game_phase,
            'position_type': position_type,
            'balance': balance,
            'white_backwards': w_backward,
            'white_doubled': w_doubled,
            'white_passed': w_passed,
            'black_backwards': b_backward,
            'black_doubled': b_doubled,
            'black_passed': b_passed,
            'white_shield_pawns': ws['shield_pawns'],
            'white_open_files': ws['open_files'],
            'white_tropism': ws['tropism'],
            'black_shield_pawns': bs['shield_pawns'],
            'black_open_files': bs['open_files'],
            'black_tropism': bs['tropism'],
            'white_num_moves': wm['num_moves'],
            'white_capture_ratio': wm['capture_ratio'],
            'white_check_ratio': wm['check_ratio'],
            'white_legal_enemy': wm['legal_enemy'],
            'white_controlled_enemy': wm['controlled_enemy'],
            'black_num_moves': bm['num_moves'],
            'black_capture_ratio': bm['capture_ratio'],
            'black_check_ratio': bm['check_ratio'],
            'black_legal_enemy': bm['legal_enemy'],
            'black_controlled_enemy': bm['controlled_enemy'],
        }

        cols = ','.join(features.keys())
        placeholders = ','.join('?' for _ in features)

        cnxn.execute(
            f"INSERT INTO position_features ({cols}) VALUES ({placeholders})",
            list(features.values())
        )


import duckdb
import platform
system = platform.system()

if __name__ == "__main__":
    if system == "Windows": DB = "F:/databases/chess_analytics.duckdb"

    cnxn = duckdb.connect(DB)

    build_position_features(cnxn)

    cnxn.close()