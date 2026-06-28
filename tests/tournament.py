#!/usr/bin/env python3
"""Round-robin tournament runner for Elo calculation.

Runs the candidate engine against all engines in the database at up to 4 time
controls (bullet, blitz, rapid, classical) and computes Elo ratings using the
standard pairwise comparison method.

Time control categories:
  - bullet:    < 1 min/game     (default: 0:30+0.3)
  - blitz:     < 10 min/game    (default: 3+0.03)
  - rapid:     < 30 min/game    (default: 10+0.1)
  - classical: >= 30 min/game   (default: 60+0.6)

Usage (standalone):
  python -m tests.tournament --engine engines/1.0.exe --tc blitz --games 50

Called from release.py as:
  tournament.run_tournament(args, cnxn, engine_id)
"""
import argparse
import math
import os
import re
import subprocess
import sqlite3
import platform
from pathlib import Path
from ..data import etl

system = platform.system()

TESTS_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = TESTS_DIR.parent
ENGINES_DIR = PROJECT_ROOT / "engines"
LOGS_DIR = PROJECT_ROOT / "logs"
TOURNAMENT_LOG_DIR = LOGS_DIR / "tournament_logs"

# Default time controls per category
TC_DEFAULTS = {
    "bullet":    "0:30+0.3",
    "blitz":     "3+0.03",
    "rapid":     "10+0.1",
    "classical": "60+0.6",
}


def get_all_engines(cnxn):
    """Get all engine versions and their exe paths from the DB."""
    rows = cnxn.execute(
        "SELECT id, version FROM engines ORDER BY id"
    ).fetchall()
    engines = []
    for row in rows:
        engine_id, version = row[0], row[1]
        # Try common exe naming patterns
        exe_path = ENGINES_DIR / f"{version}.exe"
        if not exe_path.exists():
            exe_path = ENGINES_DIR / version
            if not exe_path.exists():
                continue
        engines.append({"id": engine_id, "version": version, "path": str(exe_path)})
    return engines


def parse_tournament_output(output, engine_names):
    """Parse cutechess-cli round-robin output into pairwise results.
    
    Returns dict: {engine_name: {"wins": W, "losses": L, "draws": D, "games": N}}
    """
    results = {name: {"wins": 0, "losses": 0, "draws": 0, "games": 0} for name in engine_names}

    # Match lines like: "Score of EngA vs EngB: 5 - 3 - 2  [0.600] 10"
    score_re = re.compile(
        r"Score of (.+?) vs (.+?):\s*(\d+)\s*-\s*(\d+)\s*-\s*(\d+)"
    )

    for line in output.splitlines():
        m = score_re.search(line)
        if m:
            name_a, name_b = m.group(1).strip(), m.group(2).strip()
            w, l, d = int(m.group(3)), int(m.group(4)), int(m.group(5))
            if name_a in results:
                results[name_a]["wins"] += w
                results[name_a]["losses"] += l
                results[name_a]["draws"] += d
                results[name_a]["games"] += w + l + d
            if name_b in results:
                results[name_b]["wins"] += l
                results[name_b]["losses"] += w
                results[name_b]["draws"] += d
                results[name_b]["games"] += w + l + d

    return results


def compute_elo(wins, losses, draws):
    """Compute Elo difference from score. Returns None if no games played."""
    games = wins + losses + draws
    if games == 0:
        return None
    score = (wins + draws * 0.5) / games
    # Avoid log(0) at extremes
    if score <= 0.0:
        return -800.0
    if score >= 1.0:
        return 800.0
    elo = -400.0 * math.log10(1.0 / score - 1.0)
    return round(elo, 1)


def run_cutechess_tournament(candidate_path, opponents, tc, games_per_pair, cutechess_cli, book=None):
    """Run candidate vs all opponents using cutechess-cli.
    
    Returns raw stdout text for parsing.
    """
    TOURNAMENT_LOG_DIR.mkdir(parents=True, exist_ok=True)

    # Build engine blocks
    engine_blocks = [
        "-engine", f"name=Candidate", f"cmd={os.path.abspath(candidate_path)}",
        f"dir={os.path.dirname(os.path.abspath(candidate_path))}",
    ]
    
    opponent_names = []
    for opp in opponents:
        name = f"v{opp['version']}"
        opponent_names.append(name)
        engine_blocks += [
            "-engine", f"name={name}", f"cmd={os.path.abspath(opp['path'])}",
            f"dir={os.path.dirname(os.path.abspath(opp['path']))}",
        ]

    # Book
    book_block = []
    if book:
        ext = os.path.splitext(book)[1][1:]
        book_block = [
            "-openings", f"file={os.path.abspath(book)}",
            f"format={ext}", "order=random", "plies=8",
        ]

    cmd = [
        cutechess_cli,
    ] + engine_blocks + [
        "-each", "proto=uci", f"tc={tc}",
        "-tournament", "round-robin",
        "-games", str(games_per_pair),
        "-rounds", str(games_per_pair),
        "-repeat",
        "-maxmoves", "200",
        "-concurrency", "2",
        "-pgnout", str(TOURNAMENT_LOG_DIR / "tournament.pgn"),
    ] + book_block

    print(f"[TOURNAMENT] Running: tc={tc}, {len(opponents)} opponents, {games_per_pair} games each")
    print(f"[TOURNAMENT] cmd: {' '.join(cmd[:6])}...")

    output_lines = []
    with subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    ) as proc:
        for line in proc.stdout:
            print(line, end="")
            output_lines.append(line)
        proc.wait()

    if proc.returncode != 0:
        print(f"[TOURNAMENT] WARNING: cutechess-cli exited with code {proc.returncode}")

    return "".join(output_lines), ["Candidate"] + opponent_names


def classify_tc(tc_str):
    """Classify a time control string into bullet/blitz/rapid/classical.
    
    Parses formats: "seconds+increment", "min:sec+increment", "seconds"
    Total game time estimate = base_time * 2 + 40 * increment (typical game ~80 plies)
    """
    tc_str = tc_str.strip()
    
    # Parse base time
    if ":" in tc_str.split("+")[0]:
        # "min:sec" format
        parts = tc_str.split("+")
        time_parts = parts[0].split(":")
        base_sec = int(time_parts[0]) * 60 + float(time_parts[1]) if len(time_parts) == 2 else float(time_parts[0]) * 60
        inc = float(parts[1]) if len(parts) > 1 else 0
    else:
        parts = tc_str.split("+")
        base_sec = float(parts[0])
        inc = float(parts[1]) if len(parts) > 1 else 0

    # Estimated game time in minutes (base + ~40 moves of increment per side)
    game_time_min = (base_sec + 40 * inc) / 60.0

    if game_time_min < 1:
        return "bullet"
    elif game_time_min < 10:
        return "blitz"
    elif game_time_min < 30:
        return "rapid"
    else:
        return "classical"


def run_tournament(args, cnxn, engine_id):
    """Main entry point from release.py.
    
    Runs tournaments at requested time controls and logs ratings.
    """
    candidate_path = args.engine
    cutechess_cli = args.cutechess_cli
    book = getattr(args, "opening_book", None)
    games_per_pair = getattr(args, "tournament_games", 20)
    tc_categories = getattr(args, "tournament_tc", ["blitz"])

    # Get all other engines from DB
    all_engines = get_all_engines(cnxn)
    # Exclude the candidate itself
    opponents = [e for e in all_engines if e["id"] != engine_id]

    if not opponents:
        print("[TOURNAMENT] No opponents in database, skipping tournament.")
        return

    print(f"[TOURNAMENT] Found {len(opponents)} opponents: {[e['version'] for e in opponents]}")

    # Start experiment
    experiment_id = etl.start_experiment(cnxn, "tournament", engine_id)

    ratings = {}
    for tc_cat in tc_categories:
        tc = TC_DEFAULTS.get(tc_cat, tc_cat)
        actual_cat = classify_tc(tc)

        print(f"\n[TOURNAMENT] === {actual_cat.upper()} (tc={tc}) ===")

        output, names = run_cutechess_tournament(
            candidate_path, opponents, tc, games_per_pair, cutechess_cli, book
        )

        results = parse_tournament_output(output, names)
        candidate_results = results.get("Candidate", {"wins": 0, "losses": 0, "draws": 0, "games": 0})

        elo = compute_elo(
            candidate_results["wins"],
            candidate_results["losses"],
            candidate_results["draws"],
        )

        ratings[actual_cat] = {
            "elo": elo,
            "games": candidate_results["games"],
        }

        print(f"[TOURNAMENT] {actual_cat}: Elo={elo}, "
              f"W={candidate_results['wins']} L={candidate_results['losses']} D={candidate_results['draws']}")

    # Log to DB
    etl.log_engine_ratings(cnxn, engine_id, ratings, experiment_id)
    etl.update_experiment(
        cnxn, experiment_id,
        {"end_time_utc": __import__("datetime").datetime.now(__import__("datetime").timezone.utc).isoformat()}
    )

    print(f"\n[TOURNAMENT] Ratings logged for engine {engine_id}")
    for cat, r in ratings.items():
        print(f"  {cat:12s}: Elo={r['elo']}, games={r['games']}")


def main():
    parser = argparse.ArgumentParser(description="Round-robin tournament runner")
    parser.add_argument("--engine", required=True, help="Candidate engine path")
    parser.add_argument("--tc", nargs="+", default=["blitz"],
                        choices=["bullet", "blitz", "rapid", "classical"],
                        help="Time control categories to run")
    parser.add_argument("--games", type=int, default=20, help="Games per opponent per TC")
    parser.add_argument("--book", default=str(PROJECT_ROOT / "bin" / "opening_books" / "8moves_v3.pgn"))
    parser.add_argument("--cutechess-cli",
                        default=r"C:\Program Files (x86)\Cute Chess\cutechess-cli.exe")

    args = parser.parse_args()

    if system == "Windows":
        db_path = "F:/databases/chess.db"
    else:
        db_path = str(Path.home() / "Documents/databases/chess.db")

    cnxn = sqlite3.connect(db_path)

    # Get candidate engine ID
    meta = etl.probe_engine_metadata(args.engine)
    engine_id = etl.get_engine_id(cnxn, version=meta["version"])
    if engine_id is None:
        print(f"[ERROR] Engine version {meta['version']} not found in database. Register it first.")
        return

    # Build namespace for run_tournament
    ns = argparse.Namespace(
        engine=args.engine,
        cutechess_cli=getattr(args, "cutechess_cli"),
        opening_book=args.book,
        tournament_games=args.games,
        tournament_tc=args.tc,
    )

    run_tournament(ns, cnxn, engine_id)
    cnxn.close()


if __name__ == "__main__":
    main()
