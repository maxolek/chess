import argparse
import subprocess
import shlex
import time
import queue
import threading
import json

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

    def evaluate_position(self, fen, depth=5, timeout=15):
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

    def evaluate_stockfish(self, fen, depth=5, timeout=15, multi_pv=5):
        """Send 'go depth # multipv #' and parse Stockfish output."""
        self.set_position(fen)
        self._send(f"go depth {depth} multipv {multi_pv}")

        evals = []
        start = time.time()
        bestmove = None

        while True:
            if time.time() - start > timeout:
                break
            try:
                line = self._queue.get(timeout=0.1)
            except queue.Empty:
                continue
            line = line.strip()
            if line.startswith("info") and "multipv" in line and "score" in line:
                parts = line.split()
                try:
                    mpv_idx = parts.index("multipv")
                    mv_num = int(parts[mpv_idx + 1])
                    if "cp" in parts:
                        idx = parts.index("cp")
                        score = int(parts[idx + 1])
                    elif "mate" in parts:
                        idx = parts.index("mate")
                        mate = int(parts[idx + 1])
                        score = 100000 if mate > 0 else -100000
                    else:
                        score = 0
                    mv_idx = parts.index("pv")
                    mv = parts[mv_idx + 1]
                    evals.append((mv_num, mv, score))
                except Exception:
                    continue
            elif line.startswith("bestmove"):
                bestmove = line.split()[1]
                break

        if not evals:
            raise RuntimeError("Stockfish did not return any multipv info")

        # sort by multipv number
        evals.sort(key=lambda x: x[0])
        return bestmove, evals

    def quit(self):
        self._send("quit")
        self.proc.wait()


# --------------------------
# EPD parser
# --------------------------
def parse_epd(epd_file):
    positions = []
    with open(epd_file, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split(";")
            fen = parts[0].strip()
            bm = None
            for op in parts[1:]:
                if op.strip().startswith("bm"):
                    bm = op.strip()[3:].strip()
            positions.append((fen, bm))
    return positions


# --------------------------
# Regression test
# --------------------------
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--engine", default="../tomahawk/tomahawk")
    parser.add_argument("--stockfish", default="../stockfish-windows-x86-64-avx2/stockfish/stockfish-windows-x86-64-avx2")
    parser.add_argument("--epd", default="./bin/WinAtChess.epd")
    parser.add_argument("--depth", type=int, default=5)
    parser.add_argument("--tolerance", type=int, default=50)
    parser.add_argument("--limit", type=int, default=0)
    parser.add_argument("--log", default="./logs/eval_tests.log")
    args = parser.parse_args()

    my_engine = Engine(args.engine)
    sf_engine = Engine(args.stockfish)

    positions = parse_epd(args.epd)
    if args.limit > 0:
        positions = positions[:args.limit]

    with open(args.log, "w", encoding="utf-8") as logf:
        for i, (fen, bm_epd) in enumerate(positions, 1):
            my_eval, my_move = my_engine.evaluate_position(fen, args.depth)
            sf_bestmove, sf_evals = sf_engine.evaluate_stockfish(fen, args.depth)

            # find our move in stockfish's multipv list
            my_rank = None
            for mv_num, mv, score in sf_evals:
                if mv == my_move:
                    my_rank = mv_num
                    break

            record = {
                "fen": fen,
                "my_eval": my_eval,
                "my_best_move": my_move,
                "stockfish_eval": sf_evals[0][2] if sf_evals else 0,
                "stockfish_best_move": sf_bestmove,
                "my_move_rank_in_stockfish": my_rank
            }

            logf.write(json.dumps(record) + "\n")

    my_engine.quit()
    sf_engine.quit()


if __name__ == "__main__":
    main()
