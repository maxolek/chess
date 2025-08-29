import subprocess
import time
import json
import os
import argparse

# --------------------------
# Engine UCI Runner
# --------------------------

def start_engine(engine_path):
    proc = subprocess.Popen(
        engine_path,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1
    )
    return proc

def send(proc, cmd):
    if proc.poll() is not None:
        raise RuntimeError("Engine process closed")
    proc.stdin.write(cmd + "\n")
    proc.stdin.flush()

def expect_readyok(proc, timeout=5.0):
    t0 = time.time()
    while True:
        if time.time() - t0 > timeout:
            raise TimeoutError("Engine did not respond with readyok")
        line = proc.stdout.readline()
        if not line:
            continue
        if line.strip() == "readyok":
            return

# --------------------------
# EPD parsing
# --------------------------

def parse_epd(epd_file):
    positions = []
    with open(epd_file, "r") as f:
        for line in f:
            if not line.strip() or line.startswith("#"):
                continue
            parts = line.strip().split(";")
            fen = parts[0].strip()
            best_move = None
            for op in parts[1:]:
                if op.strip().startswith("bm"):
                    best_move = op.strip()[3:].strip()
            positions.append((fen, best_move))
    return positions

# --------------------------
# Search runner
# --------------------------

def run_search_on_fen(proc, fen, go_args, timeout_s=60):
    """Run search on a FEN, parse info lines for metrics"""
    snapshot = {
        "fen": fen,
        "bestmove": None,
        "ponder": None,
        "elapsed_ms": None,
        "ok": True,
        "error": None,
        "iter_stats": [],
        "root_researches": 0
    }

    send(proc, f"position fen {fen}")
    send(proc, f"go {go_args}")

    t0 = time.time()
    iter_stats = []

    while True:
        if time.time() - t0 > timeout_s:
            snapshot["ok"] = False
            snapshot["error"] = "timeout"
            try: send(proc, "stop")
            except: pass
            break

        line = proc.stdout.readline()
        if not line:
            time.sleep(0.01)
            continue
        line = line.strip()
        if line.startswith("info"):
            info = parse_info_line(line)
            iter_stats.append(info)
        elif line.startswith("bestmove"):
            parts = line.split()
            snapshot["bestmove"] = parts[1] if len(parts) > 1 else None
            if "ponder" in parts:
                snapshot["ponder"] = parts[parts.index("ponder")+1]
            break

    snapshot["elapsed_ms"] = int((time.time() - t0) * 1000)
    snapshot["iter_stats"] = iter_stats
    snapshot["root_researches"] = len(iter_stats) - 1 if iter_stats else 0

    # Compute derived metrics
    for iter_stat in snapshot["iter_stats"]:
        nodes = iter_stat.get("nodes", 0)
        qnodes = iter_stat.get("qnodes", 0)
        depth = iter_stat.get("depth", 1)
        iter_stat["q_ratio"] = qnodes / nodes if nodes else None
        iter_stat["ebf"] = (nodes ** (1 / depth)) if nodes and depth > 0 else None

    return snapshot

# --------------------------
# Minimal info parser
# --------------------------

def parse_info_line(line):
    tokens = line.strip().split()
    accum = {}
    i = 1  # skip "info"
    while i < len(tokens):
        tok = tokens[i]
        if tok == "depth": accum["depth"] = int(tokens[i+1]); i+=2
        elif tok == "nodes": accum["nodes"] = int(tokens[i+1]); i+=2
        elif tok == "qnodes": accum["qnodes"] = int(tokens[i+1]); i+=2
        elif tok == "score" and i+2 < len(tokens):
            if tokens[i+1] == "cp": accum["score_cp"] = int(tokens[i+2])
            elif tokens[i+1] == "mate": accum["score_mate"] = int(tokens[i+2])
            i+=3
        elif tok == "pv": accum["pv"] = " ".join(tokens[i+1:]); break
        elif tok == "failhigh": accum["fail_highs"] = int(tokens[i+1]); i+=2
        elif tok == "faillow": accum["fail_lows"] = int(tokens[i+1]); i+=2
        elif tok == "tthits": accum["tt_hits"] = int(tokens[i+1]); i+=2
        elif tok == "ttstores": accum["tt_stores"] = int(tokens[i+1]); i+=2
        else: i+=1
    return accum

# --------------------------
# Main
# --------------------------

# python search_tests.py --epd my_positions.epd --go "movetime 5000" --engine ../tomahawk/tomahawk.exe

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Run engine tests on EPD positions.")
    parser.add_argument("--engine", type=str, default="../tomahawk/tomahawk.exe",
                        help="Path to the engine binary")
    parser.add_argument("--epd", type=str, default="./bin/WinAtChess.epd",
                        help="Path to the EPD file with positions")
    parser.add_argument("--go", type=str, default="depth 10",
                        help="UCI go arguments, e.g., 'depth 3' or 'movetime 5000'")
    
    args = parser.parse_args()

    engine = start_engine(args.engine)
    send(engine, "isready")
    expect_readyok(engine)
    send(engine, "setoption name ShowStats value true")

    positions = parse_epd(args.epd)
    for fen, bm in positions:
        snapshot = run_search_on_fen(engine, fen, args.go)
        #print(json.dumps(snapshot, indent=2))

    send(engine, "quit")