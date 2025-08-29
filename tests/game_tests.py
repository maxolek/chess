import argparse
import subprocess
import time
import chess
import chess.pgn
import os
#from utils import init_db, log_game_move, log_game_summary

# --------------------------
# Engine wrapper for UCI
# --------------------------
class Engine:
    def __init__(self, path):
        self.proc = subprocess.Popen(
            path,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            universal_newlines=True,
            bufsize=1,
        )

    def send(self, cmd: str):
        self.proc.stdin.write(cmd + "\n")
        self.proc.stdin.flush()

    def get_bestmove(self):
        bestmove = None
        while True:
            line = self.proc.stdout.readline()
            if not line:
                break
            line = line.strip()
            if line.startswith("bestmove"):
                bestmove = line.split()[1]
                break
        return bestmove

    def quit(self):
        self.send("quit")
        self.proc.wait()


# --------------------------
# Game loop
# --------------------------
def play_game(engine_path_1, engine_path_2, max_moves=100, movetime=1000, start_time_ms=None, inc_ms=None):
    """Play a game between two engines and append PGN to game_tests.pgn"""
    #init_db()
    board = chess.Board()
    proc1, proc2 = Engine(engine_path_1), Engine(engine_path_2)
    engines = [proc1, proc2]
    turn = 0
    move_number = 1

    # PGN setup
    game = chess.pgn.Game()
    game.headers["White"] = engine_path_1
    game.headers["Black"] = engine_path_2
    game.headers["Date"] = time.strftime("%Y.%m.%d")
    node = game

    # clocks if needed
    clocks = [start_time_ms, start_time_ms] if start_time_ms else [None, None]

    while move_number <= max_moves and not board.is_game_over():
        engine = engines[turn]
        fen = board.fen()
        engine.send(f"position fen {fen}")

        # send go command
        if movetime:
            engine.send(f"go movetime {movetime}")
        elif clocks[0] is not None:
            engine.send(f"go wtime {clocks[0]} btime {clocks[1]} winc {inc_ms} binc {inc_ms}")

        move_start = time.time()
        bestmove = engine.get_bestmove()
        elapsed_ms = int((time.time() - move_start) * 1000)

        if bestmove is None or bestmove not in [m.uci() for m in board.legal_moves]:
            print(f"Illegal move detected: {bestmove} at move {move_number}")
            break

        move = chess.Move.from_uci(bestmove)
        board.push(move)

        # Add move to PGN
        node = node.add_variation(move)

        move_number += 1
        turn = 1 - turn

    # final PGN
    game.headers["Result"] = board.result()
    pgn_file = "./logs/game_tests.pgn"
    os.makedirs(os.path.dirname(pgn_file), exist_ok=True)

    # append PGN to file
    with open(pgn_file, "a", encoding="utf-8") as f:
        exporter = chess.pgn.FileExporter(f)
        game.accept(exporter)

    print(f"Game finished: {board.result()}, appended to {pgn_file}")

    proc1.quit()
    proc2.quit()


# --------------------------
# Main
# --------------------------
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--engine1", required=True)
    parser.add_argument("--engine2", required=True)
    parser.add_argument("--movetime", type=int, default=1000, help="per-move time in ms")
    parser.add_argument("--max_moves", type=int, default=100)
    parser.add_argument("--start_time", type=int, help="initial clock in ms")
    parser.add_argument("--inc", type=int, help="increment per move in ms")
    args = parser.parse_args()

    play_game(
        engine_path_1=args.engine1,
        engine_path_2=args.engine2,
        max_moves=args.max_moves,
        movetime=args.movetime,
        start_time_ms=args.start_time,
        inc_ms=args.inc
    )


if __name__ == "__main__":
    main()
