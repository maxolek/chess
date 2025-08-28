import os
import random
import chess
import chess.engine

# ---- Engine path (compiled with -fprofile-generate) ----
ENGINE_PATH = r"C:\Users\maxol\code\chess\tomahawk\tomahawk.exe"

# ---- Profile settings ----
NUM_FENS = 200       # Total unique positions
PLIES_PER_FEN = 40   # Random moves to play from start to create FEN
MOVES_PER_FEN = 5    # Engine plays this many moves from each FEN
TIME_PER_MOVE = 0.5 # seconds

def generate_random_fens(num_fens, plies_per_fen):
    fens = []
    for _ in range(num_fens):
        board = chess.Board()
        for _ in range(random.randint(1, plies_per_fen)):
            if board.is_game_over():
                break
            move = random.choice(list(board.legal_moves))
            board.push(move)
        fens.append(board.fen())
    return fens

def run_profile():
    try:
        engine = chess.engine.SimpleEngine.popen_uci(ENGINE_PATH)
    except Exception as e:
        print(f"Failed to start engine: {e}")
        return

    fens = generate_random_fens(NUM_FENS, PLIES_PER_FEN)

    for i, fen in enumerate(fens, 1):
        board = chess.Board(fen)
        print(f"[{i}/{NUM_FENS}] FEN: {fen}")

        for _ in range(MOVES_PER_FEN):
            if board.is_game_over():
                break
            try:
                result = engine.play(board, chess.engine.Limit(time=TIME_PER_MOVE))
            except chess.engine.EngineTerminatedError:
                print("Engine crashed!")
                break

            move = result.move
            if move not in board.legal_moves:
                print(f"Illegal move {move} at {board.fen()}")
                break
            board.push(move)

    engine.quit()
    print("PGO profile run complete! Now compile with -fprofile-use.")

if __name__ == "__main__":
    run_profile()
