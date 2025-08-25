import os
import chess
import chess.engine
from random import choice, random

ENGINE_PATH = r"C:\Users\maxol\code\chess\tomahawk\tomahawk.exe"
PROFILE_DIR = "./pgo_profile"
os.makedirs(PROFILE_DIR, exist_ok=True)

GAMES_PER_FEN = 100

# Example FENs to cover common tactical motifs and edge cases
FENS = [
    chess.STARTING_FEN,
    "r2q1rk1/pp1nbppp/2n1p3/3p4/3P4/2NBPN2/PP3PPP/R1BQ1RK1 w - - 0 1",
    "4r1k1/5ppp/8/3b4/3P4/2N5/PPP2PPP/R3R1K1 w - - 0 1",
    "8/P7/8/8/8/8/7p/7K w - - 0 1",
    "8/8/8/8/2k5/8/3K4/8 w - - 0 1",
]

def choose_time_and_length():
    """Mix shallow, medium, and deeper workloads."""
    r = random()
    if r < 0.6:   # 60%: shallow fast games
        return 0.01, 40
    elif r < 0.9: # 30%: medium games
        return 0.1, 80
    else:         # 10%: deeper games
        return 1, 120

def play_profile_game(fen, game_idx):
    board = chess.Board(fen)
    try:
        engine = chess.engine.SimpleEngine.popen_uci(ENGINE_PATH)
    except Exception as e:
        print(f"Failed to start engine: {e}")
        return

    TIME_LIMIT, MOVES_PER_GAME = choose_time_and_length()
    moves_played = []

    for _ in range(MOVES_PER_GAME):
        if board.is_game_over():
            break
        result = engine.play(board, chess.engine.Limit(time=TIME_LIMIT))
        if result.move is None:
            break
        board.push(result.move)
        moves_played.append(result.move.uci())

    engine.quit()

    # optional logging
    fname = os.path.join(PROFILE_DIR, f"fen{FENS.index(fen)}_game{game_idx}.pgn")
    with open(fname, "w") as f:
        f.write(" ".join(moves_played) + "\n")

def main():
    for fen in FENS:
        for game_idx in range(GAMES_PER_FEN):
            play_profile_game(fen, game_idx)
            print(f"FEN {FENS.index(fen)} Game {game_idx} done")

if __name__ == "__main__":
    main()
