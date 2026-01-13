import argparse
import subprocess
import os
import json
import chess
import sqlite3
from data import etl

# --------------------------
#       logging
# --------------------------

def upload_logs(args):
    cnxn = sqlite3.connect("F:/databases/chess.db")
    cnxn.row_factory = sqlite3.Row 

    # probe metadata
    meta = etl.probe_engine_metadata(args.engine)
    engine_id = etl.get_engine_id(cnxn, version=meta["version"])

    # log
    sts_id = etl.start_experiment(
        cnxn, 
        "STS",
        engine_id
    )

    # no games to map search to, so no game_map like in SPRT
    etl.bulk_log_search_and_timing(
        cnxn,
        args.log,
        {},
        timing_path = args.log + "/timing.jsonl"
    )

    print(f"[DATA] Logging completed for STS {sts_id}")

# --------------------------
# Run engine and get eval/bestmove
# --------------------------
def run_eval(engine, fen, depth=8, time=None):
    """Send position and go eval command, return (score, bestmove)."""
    # position + eval
    engine.stdin.write(f"position fen {fen}\n".encode())
    if time is not None: 
        engine.stdin.write(f"go eval movetime {time*1000}\n".encode())
    else: 
        engine.stdin.write(f"go eval depth {depth}\n".encode())
    engine.stdin.flush()

    score, bestmove = None, None
    while True:
        line = engine.stdout.readline().decode().strip()
        if line.startswith("eval"):
            parts = line.split()
            try:
                score_idx = parts.index("eval") + 1
                bestmove_idx = parts.index("bestmove") + 1
                score = int(parts[score_idx])
                bestmove = parts[bestmove_idx]
            except (ValueError, IndexError):
                pass
            break
    return score, bestmove

# --------------------------
# Parse EPD line
# --------------------------
def parse_epd_line(line):
    """
    Parse EPD line into (fen, ops dict, expected_moves)
    """
    line = line.strip()
    if not line or line.startswith("#"):
        return None, {}, []

    # Split on semicolon for EPD operations
    parts = line.split(";")
    main_part = parts[0].strip()  # FEN + optional bm
    ops_parts = parts[1:]

    tokens = main_part.split()
    if len(tokens) < 4:
        return None, {}, []

    # Standard FEN fields
    pieces = tokens[0]
    turn = tokens[1]
    castling = tokens[2]
    en_passant = tokens[3]
    halfmove = "0"
    fullmove = "1"
    fen = f"{pieces} {turn} {castling} {en_passant} {halfmove} {fullmove}"

    # Extract expected moves after 'bm' in main_part
    expected_moves_raw = []
    if "bm" in tokens:
        bm_index = tokens.index("bm")
        expected_moves_raw = tokens[bm_index + 1:]

    # Parse ops (id, ce, c0, etc.)
    ops = {}
    for token in ops_parts:
        token = token.strip()
        if not token:
            continue
        if token.startswith("id"):
            ops["id"] = token.split(" ", 1)[1].strip('" ')
        elif token.startswith("ce"):
            try:
                ops["ce"] = int(token.split()[1])
            except:
                ops["ce"] = None
        elif token.startswith("c0"):
            ops["c0"] = token.split(" ", 1)[1].strip('" ')

    return fen, ops, expected_moves_raw

def collect_epd_files(paths):
    epd_files = []

    for path in paths:
        path = os.path.normpath(path)
        if os.path.isfile(path) and path.lower().endswith(".epd"):
            epd_files.append(path)
        elif os.path.isdir(path):
            for root, _, files in os.walk(path):
                for f in files:
                    if f.lower().endswith(".epd"):
                        epd_files.append(os.path.join(root, f))
        else:
            print(f"⚠ Skipping unknown path: {path}")

    return sorted(set(os.path.normpath(p) for p in epd_files))

# --------------------------
# Convert expected moves to UCI
# --------------------------
import re
SAN_CLEAN_RE = re.compile(r"[!?+#]")

def moves_to_uci(board, expected_moves):
    """
    Convert expected moves (SAN, capture SAN, or square-only hints) into UCI moves.
    """
    uci_moves = []

    for raw in expected_moves:
        m =raw.strip().rstrip(",;").strip()
        m = SAN_CLEAN_RE.sub("", m)  # remove + # ! ?

        # 1️⃣ Try SAN (handles captures, promotions, castles, etc.)
        try:
            move = board.parse_san(m)
            uci_moves.append(move.uci())
            continue
        except ValueError:
            pass

        # 2️⃣ Square-only fallback (e.g. "f5", "e4")
        if re.fullmatch(r"[a-h][1-8]", m):
            for legal in board.legal_moves:
                if chess.square_name(legal.to_square) == m:
                    uci_moves.append(legal.uci())
                    break
            else:
                print(f"⚠ Could not match square move '{raw}' on {board.fen()}")
            continue

        # 3️⃣ UCI fallback (some EPDs give e2e4 directly)
        if re.fullmatch(r"[a-h][1-8][a-h][1-8][qrbn]?", m):
            move = chess.Move.from_uci(m)
            if move in board.legal_moves:
                uci_moves.append(m)
                continue

        # ❌ Failed
        print(f"⚠ Could not parse expected move '{raw}' on board {board.fen()}")

    return uci_moves

# --------------------------
# Run STS for a single file
# --------------------------
def run_sts_file(engine_path, epd_file, depth=8, time = None, log_file = None):
    engine = subprocess.Popen([engine_path], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    total, correct = 0, 0
    diffs = []
    category_stats = {}

    # logging options
    engine.stdin.write(f"setoption name log_dir value ../logs/sts_logs\n".encode())
    engine.stdin.write(f"setoption name timer_logging value true\n".encode())
    engine.stdin.write(f"setoption name stats_logging value true\n".encode())
    engine.stdin.write(f"setoption name uci_logging value true\n".encode())
    engine.stdin.write(f"setoption name game_logging value false\n".encode())

    with open(epd_file, "r", encoding="utf-8") as f:
        lines = [l.strip() for l in f if l.strip() and not l.startswith("#")]

    for i, line in enumerate(lines, 1):
        fen, ops, expected_moves_raw = parse_epd_line(line)
        if not fen:
            print(f"[{i}/{len(lines)}] Skipping malformed line: {line}")
            continue

        cat = ops.get("id", "Unknown")

        try:
            board = chess.Board(fen)
        except ValueError as e:
            print(f"[{i}/{len(lines)}] Invalid FEN: {fen} | Error: {e}")
            continue

        # Convert expected moves to UCI
        expected_moves = moves_to_uci(board, expected_moves_raw)

        print(f"[STS] {epd_file}: position {i}")
        score, bestmove = run_eval(engine, fen, depth, time)
        total += 1

        move_ok = expected_moves and bestmove in expected_moves
        if move_ok:
            correct += 1
        category_stats.setdefault(cat, {"total": 0, "correct": 0})
        category_stats[cat]["total"] += 1
        if move_ok:
            category_stats[cat]["correct"] += 1

        expected_score = ops.get("ce", None)
        score_diff = None
        if expected_score is not None and score is not None:
            score_diff = score - expected_score
            diffs.append(abs(score_diff))

        print(f"[{i}/{len(lines)}] {cat}")
        print(f"  FEN:      {fen}")
        print(f"  Expected: moves={expected_moves}, score={expected_score}")
        print(f"  Engine:   move={bestmove}, score={score}")
        print("  ✅ Move correct" if move_ok else "  ❌ Move incorrect")
        if score_diff is not None:
            print(f"  Score diff: {score_diff}")
        print("-" * 50)

        record = {
            "epd_file": epd_file, 
            "index": i, 
            "category": cat, 
            "fen": fen, 
            "expected_moves": expected_moves, 
            "expected_score": expected_score,
            "engine_move": bestmove,
            "engine_score": score, 
            "move_ok": move_ok, 
            "score_diff": score_diff
        }
        log_file.write(json.dumps(record) + "\n")
        log_file.flush()

    engine.kill()

    file_summary = {
        "file": epd_file,
        "total": total,
        "correct": correct,
        "accuracy": 100.0 * correct / total if total else 0,
        "avg_diff": sum(diffs)/len(diffs) if diffs else None,
        "per_category": category_stats
    }

    return file_summary

# --------------------------
# Main
# --------------------------

def parse_args():
    parser = argparse.ArgumentParser(description="SPRT runner using cutechess-cli")

    parser.add_argument("--engine", default="../src/tomahawk.exe", help="Path to UCI engine")
    parser.add_argument("--time", type=int, default=None, help = "Move time in MS")
    parser.add_argument("--depth", type=int, default=8, help = "Search depth")
    parser.add_argument("--sts", nargs="+", default=["./bin/STS"], help="One or more EPD files and/or folders")
    parser.add_argument("--log", default="../logs/sts_logs/full_sts_suite.jsonl", help="Output log file")

    return parser.parse_args()


def main(args=None):
    if args is None: 
        args = parse_args()

    os.makedirs(os.path.dirname(args.log), exist_ok=True)
    log_f = open(args.log, "a", encoding="utf-8")
    print(f"Results logged to {args.log}")

    epd_files = collect_epd_files(args.sts)
    if not epd_files:
        print("❌ No EPD files found.")
        return
    
    global_total, global_correct = 0, 0
    all_summaries = []

    for epd_file in epd_files:
        print(f"\n=== Running STS file: {epd_file} ===")
        summary = run_sts_file(args.engine, epd_file, args.depth, args.time, log_f)
        all_summaries.append(summary)
        print(f"File summary: {summary['correct']}/{summary['total']} correct ({summary['accuracy']:.1f}%)")
        global_total += summary['total']
        global_correct += summary['correct']

    global_acc = 100.0 * global_correct / global_total if global_total else 0
    print("\n=== Global STS Summary ===")
    print(f"Total tests: {global_total}")
    print(f"Total correct: {global_correct} ({global_acc:.1f}%)")

    # upload to db
    upload_logs(args)
    

if __name__ == "__main__":
    main()
