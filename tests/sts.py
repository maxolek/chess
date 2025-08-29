import argparse
import subprocess
import time
import os
import sys

# --------------------------
# Engine evaluation
# --------------------------
def run_eval(engine, fen, depth=8):
    """Send position and go command, return (score, bestmove)."""
    engine.stdin.write(f"position fen {fen}\n".encode())
    engine.stdin.write(f"go depth {depth}\n".encode())
    engine.stdin.flush()
    score, bestmove = None, None
    while True:
        line = engine.stdout.readline().decode().strip()
        if line.startswith("info") and "score cp" in line:
            try:
                parts = line.split()
                idx = parts.index("cp")
                score = int(parts[idx + 1])
            except:
                pass
        if line.startswith("bestmove"):
            parts = line.split()
            if len(parts) > 1:
                bestmove = parts[1]
            break
    return score, bestmove


# --------------------------
# Parse EPD line
# --------------------------
def parse_epd_line(line):
    """Parses one EPD line into (fen, ops dict)."""
    if ";" not in line:
        return None, {}
    parts = line.split(";")
    fen = parts[0].strip()
    ops = {}
    for token in parts[1:]:
        token = token.strip()
        if not token:
            continue
        if token.startswith("bm"):
            ops["bm"] = token.split()[1:]
        elif token.startswith("ce"):
            try:
                ops["ce"] = int(token.split()[1])
            except:
                pass
        elif token.startswith("id"):
            ops["id"] = token.split(" ", 1)[1].strip('" ')
    return fen, ops


# --------------------------
# Run STS for a single file
# --------------------------
def run_sts_file(engine_path, epd_file, depth=8):
    engine = subprocess.Popen([engine_path], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    total, correct = 0, 0
    diffs = []
    category_stats = {}

    with open(epd_file, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue

            fen, ops = parse_epd_line(line)
            if not fen:
                continue

            score, bestmove = run_eval(engine, fen, depth)
            total += 1
            cat = ops.get("id", "Unknown")

            if "bm" in ops:
                expected_moves = ops["bm"]
                ok = bestmove in expected_moves
                if ok:
                    correct += 1
                category_stats.setdefault(cat, {"total": 0, "correct": 0})
                category_stats[cat]["total"] += 1
                if ok:
                    category_stats[cat]["correct"] += 1
                print(f"[{total}] {cat}  Engine: {bestmove}, Expected: {expected_moves} {'✅' if ok else '❌'}")

            elif "ce" in ops:
                expected_score = ops["ce"]
                if score is not None:
                    diff = abs(score - expected_score)
                    diffs.append(diff)
                    print(f"[{total}] {cat}  Engine: {score}, Expected: {expected_score}, Diff: {diff}")
                else:
                    print(f"[{total}] {cat}  Engine: None, Expected: {expected_score}")

    engine.kill()

    file_summary = {
        "file": epd_file,
        "total": total,
        "correct": correct,
        "accuracy": 100.0 * correct / total if total > 0 else 0,
        "avg_diff": sum(diffs)/len(diffs) if diffs else None,
        "per_category": category_stats
    }

    return file_summary


# --------------------------
# Main
# --------------------------
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--engine", required=True, help="Path to UCI engine")
    parser.add_argument("--depth", type=int, default=8)
    parser.add_argument("--sts_dir", default="./bin/STS", help="Folder with STS##.epd files")
    parser.add_argument("--log", default="./logs/sts_test.log", help="Output log file")
    args = parser.parse_args()

    os.makedirs(os.path.dirname(args.log), exist_ok=True)

    epd_files = sorted([os.path.join(args.sts_dir, f) for f in os.listdir(args.sts_dir)
                        if f.lower().endswith(".epd")])

    global_total, global_correct = 0, 0
    all_summaries = []

    for epd_file in epd_files:
        print(f"\n=== Running STS file: {epd_file} ===")
        summary = run_sts_file(args.engine, epd_file, args.depth)
        all_summaries.append(summary)
        print(f"File summary: {summary['correct']}/{summary['total']} correct "
              f"({summary['accuracy']:.1f}%)")
        global_total += summary['total']
        global_correct += summary['correct']

    # Global summary
    global_acc = 100.0 * global_correct / global_total if global_total > 0 else 0
    print("\n=== Global STS Summary ===")
    print(f"Total tests: {global_total}")
    print(f"Total correct: {global_correct} ({global_acc:.1f}%)")

    # Write log
    with open(args.log, "w", encoding="utf-8") as f:
        import json
        json.dump({
            "global_total": global_total,
            "global_correct": global_correct,
            "global_accuracy": global_acc,
            "files": all_summaries
        }, f, indent=2)

    print(f"Results logged to {args.log}")


if __name__ == "__main__":
    main()
