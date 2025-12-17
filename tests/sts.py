import argparse
import subprocess
import os
import json
import chess

# --------------------------
# Run engine and get eval/bestmove
# --------------------------
def run_eval(engine, fen, depth=8, log_dir="../tests/logs"):
    """Send position and go eval command, return (score, bestmove)."""
    engine.stdin.write(f"setoption name log_dir value {log_dir}\n".encode())
    engine.stdin.write(f"position fen {fen}\n".encode())
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

# --------------------------
# Convert expected moves to UCI
# --------------------------
def moves_to_uci(board, expected_moves):
    """
    Convert a list of expected moves (SAN or just squares like 'f5') into UCI moves.
    """
    uci_moves = []
    for m in expected_moves:
        m = m.rstrip("+#")  # remove check/mate suffixes
        try:
            # Try SAN first
            move = board.parse_san(m)
            uci_moves.append(move.uci())
        except ValueError:
            # fallback: try as destination square (for pawn pushes like 'f5')
            for legal in board.legal_moves:
                if chess.square_name(legal.to_square) == m:
                    uci_moves.append(legal.uci())
                    break
            else:
                print(f"⚠ Could not parse expected move '{m}' on board {board.fen()}")
    return uci_moves

# --------------------------
# Run STS for a single file
# --------------------------
def run_sts_file(engine_path, epd_file, depth=8):
    engine = subprocess.Popen([engine_path], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    total, correct = 0, 0
    diffs = []
    category_stats = {}

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

        score, bestmove = run_eval(engine, fen, depth)
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
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--engine", default="../tomahawk/tomahawk.exe", help="Path to UCI engine")
    parser.add_argument("--depth", type=int, default=8)
    parser.add_argument("--sts_dir", default="./bin/STS", help="Folder with STS##.epd files")
    parser.add_argument("--log", default="./logs/sts_test.json", help="Output log file")
    args = parser.parse_args()

    os.makedirs(os.path.dirname(args.log), exist_ok=True)
    epd_files = sorted([os.path.normpath(os.path.join(args.sts_dir, f)) for f in os.listdir(args.sts_dir) if f.lower().endswith(".epd")])

    global_total, global_correct = 0, 0
    all_summaries = []

    for epd_file in epd_files:
        print(f"\n=== Running STS file: {epd_file} ===")
        summary = run_sts_file(args.engine, epd_file, args.depth)
        all_summaries.append(summary)
        print(f"File summary: {summary['correct']}/{summary['total']} correct ({summary['accuracy']:.1f}%)")
        global_total += summary['total']
        global_correct += summary['correct']

    global_acc = 100.0 * global_correct / global_total if global_total else 0
    print("\n=== Global STS Summary ===")
    print(f"Total tests: {global_total}")
    print(f"Total correct: {global_correct} ({global_acc:.1f}%)")

    with open(args.log, "w", encoding="utf-8") as f:
        json.dump({
            "global_total": global_total,
            "global_correct": global_correct,
            "global_accuracy": global_acc,
            "files": all_summaries
        }, f, indent=2)

    print(f"Results logged to {args.log}")

if __name__ == "__main__":
    main()
