import argparse
import json
import queue
import threading
import time
import shlex
import subprocess
import chess

# --------------------------
# Engine wrapper using eval_test
# --------------------------
class Engine:
    def __init__(self, path):
        self.proc = subprocess.Popen(
            shlex.split(path),
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True,
            bufsize=1,
        )
        self._queue = queue.Queue()
        self._thread = threading.Thread(target=self._reader_thread, daemon=True)
        self._thread.start()

        self._send("uci")
        self._wait_for("uciok")
        self._send("isready")
        self._wait_for("readyok")

    def _reader_thread(self):
        for line in self.proc.stdout:
            self._queue.put(line)

    def _send(self, cmd):
        self.proc.stdin.write(cmd + "\n")
        self.proc.stdin.flush()

    def _wait_for(self, token, timeout=10):
        start = time.time()
        while time.time() - start < timeout:
            try:
                line = self._queue.get_nowait()
                if token in line:
                    return line
            except queue.Empty:
                time.sleep(0.01)
        raise RuntimeError(f"Timeout waiting for {token}")

    def set_position(self, fen):
        self._send(f"position fen {fen}")

    def evaluate_position(self, fen, depth=1, timeout=15):
        """Send 'go eval_test depth #' and return (eval, bestmove)."""
        self.set_position(fen)
        self._send(f"go eval_test depth {depth}")

        start = time.time()
        while time.time() - start < timeout:
            try:
                line = self._queue.get(timeout=0.1)
            except queue.Empty:
                continue
            line = line.strip()
            if "eval" in line and "bestmove" in line:
                try:
                    parts = line.split()
                    eval_idx = parts.index("eval")
                    best_idx = parts.index("bestmove")
                    eval_cp = int(parts[eval_idx + 1])
                    bestmove = parts[best_idx + 1]
                    return eval_cp, bestmove
                except (ValueError, IndexError):
                    continue
        raise RuntimeError(f"Engine did not return eval_test output within {timeout}s")

    def quit(self):
        self._send("quit")
        self.proc.wait()


# --------------------------
# EPD parser (strip bm tags)
# --------------------------
def parse_epd(epd_file):
    positions = []
    with open(epd_file, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            # Remove any bm or extra info after FEN
            fen_full = line.split(";")[0].strip()
            fen_fields = fen_full.split()
            valid_fen = " ".join(fen_fields[:4])  # piece, side, castling, ep
            positions.append(valid_fen)
    return positions


# --------------------------
# Consistency tests
# --------------------------
def test_determinism(engine, fen, trials=3):
    """Eval same position multiple times -> should be identical."""
    results = [engine.evaluate_position(fen, depth=1) for _ in range(trials)]
    evals = [r[0] for r in results]
    bestmoves = [r[1] for r in results]
    consistent = all(e == evals[0] for e in evals) and all(m == bestmoves[0] for m in bestmoves)
    return consistent, results[0]

def test_symmetry(engine, fen):
    """Eval position vs mirrored side-to-move -> eval should negate if legal."""
    eval_orig, best_orig = engine.evaluate_position(fen, depth=1)

    try:
        board = chess.Board(fen)
        board_flipped = board.copy()
        board_flipped.turn = not board_flipped.turn

        # Only keep valid FEN fields (piece placement, turn, castling, ep)
        flipped_fen_fields = board_flipped.fen().split()[:4]
        flipped_fen = " ".join(flipped_fen_fields)

        # Try to construct board to check legality
        chess.Board(flipped_fen)

        eval_flipped, best_flipped = engine.evaluate_position(flipped_fen, depth=1)
        symmetric = eval_orig == -eval_flipped
        sym_result = (eval_flipped, best_flipped)

    except ValueError:
        # flipped FEN is illegal, fill symmetry result with None
        symmetric = None
        sym_result = (None, None)

    return symmetric, sym_result



# --------------------------
# Test suite
# --------------------------
def run_suite(engine, fens, log_file):
    with open(log_file, "w", encoding="utf-8") as logf:
        for fen in fens:
            det_consistent, det_result = test_determinism(engine, fen)
            sym_consistent, sym_result = test_symmetry(engine, fen)

            record = {
                "fen": fen,
                "determinism": det_consistent,
                "symmetry": sym_consistent,
                "eval": det_result[0],
                "bestmove": det_result[1]
            }
            logf.write(json.dumps(record) + "\n")
            #print(f"FEN: {fen}")
            #print(f"   Determinism: {det_consistent}, Symmetry: {sym_consistent}, Eval: {det_result[0]}, BestMove: {det_result[1]}")


# --------------------------
# Main
# --------------------------
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--engine", default="../tomahawk/tomahawk.exe")
    parser.add_argument("--epd", default="./bin/STS/STS1.epd")
    parser.add_argument("--log", default="./logs/eval_consistency.log", help="JSONL log file path")
    args = parser.parse_args()

    engine = Engine(args.engine)
    fens = parse_epd(args.epd)

    run_suite(engine, fens, args.log)
    engine.quit()


if __name__ == "__main__":
    main()
