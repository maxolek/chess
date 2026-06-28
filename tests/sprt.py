#!/usr/bin/env python3
import os
import argparse
import subprocess
import sys
from datetime import datetime
import sqlite3
from data import etl
import re 
from pathlib import Path
import time
from datetime import datetime, timezone
import platform

system = platform.system()

# paths
TESTS_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = TESTS_DIR.parent
# log paths
LOGS_DIR = PROJECT_ROOT / "logs"
SPRT_LOG_DIR = LOGS_DIR / "sprt_logs"
GAME_JSON = SPRT_LOG_DIR / "game.jsonl"
SEARCH_JSON = SPRT_LOG_DIR / "search.jsonl"
TIMING_JSON = SPRT_LOG_DIR / "timing.jsonl"

def parse_cutechess_output(output, candidate_name="Candidate"):

    def safe_int(x):
        try:
            return int(x)
        except Exception:
            return 0

    def safe_float(x):
        try:
            v = float(x)
            if not (v == v and abs(v) != float("inf")):
                return None
            return v
        except Exception:
            return None

    stats = {
        "candidate_wins": 0,
        "candidate_losses": 0,
        "candidate_draws": 0,

        "candidate_white_wins": 0,
        "candidate_white_losses": 0,
        "candidate_white_draws": 0,

        "candidate_black_wins": 0,
        "candidate_black_losses": 0,
        "candidate_black_draws": 0,

        "games_played": 0,
        "result": None,
    }

    # --- Regexes ---
    score_re = re.compile(
        rf"Score of {re.escape(candidate_name)} vs .+?:\s*(\d+)\s*-\s*(\d+)\s*-\s*(\d+)"
    )

    white_re = re.compile(
        rf"\.\.\.\s*{re.escape(candidate_name)} playing White:\s*(\d+)\s*-\s*(\d+)\s*-\s*(\d+)"
    )

    black_re = re.compile(
        rf"\.\.\.\s*{re.escape(candidate_name)} playing Black:\s*(\d+)\s*-\s*(\d+)\s*-\s*(\d+)"
    )

    number = r"[+-]?(?:\d+(?:\.\d+)?|inf|nan)"
    elo_re = re.compile(
        rf"""
        Elo\ difference:\s*(?P<elo>{number})\s*\+/-\s*{number}
        ,\s*LOS:\s*(?P<los>{number})\s*%
        ,\s*DrawRatio:\s*(?P<draw>{number})\s*%
        """,
        re.IGNORECASE | re.VERBOSE
    )

    sprt_re = re.compile(
        r"SPRT:\s*llr\s*([-\d\.]+)\s*\([^)]+\),\s*lbound\s*([-\d\.]+),\s*ubound\s*([-\d\.]+)",
        re.IGNORECASE,
    )

    # We overwrite on every match → last one wins
    for line in output.splitlines():

        line = line.strip()

        if m := score_re.search(line):
            w, l, d = map(safe_int, m.groups())
            stats["candidate_wins"] = w
            stats["candidate_losses"] = l
            stats["candidate_draws"] = d
            stats["games_played"] = w + l + d
            continue

        if m := white_re.search(line):
            stats["candidate_white_wins"] = safe_int(m.group(1))
            stats["candidate_white_losses"] = safe_int(m.group(2))
            stats["candidate_white_draws"] = safe_int(m.group(3))
            continue

        if m := black_re.search(line):
            stats["candidate_black_wins"] = safe_int(m.group(1))
            stats["candidate_black_losses"] = safe_int(m.group(2))
            stats["candidate_black_draws"] = safe_int(m.group(3))
            continue

        if m := elo_re.search(line):
            stats["elo_diff"] = safe_float(m.group("elo"))
            stats["los"] = safe_float(m.group("los"))
            stats["draw_ratio"] = safe_float(m.group("draw"))
            continue

        if m := sprt_re.search(line):
            stats["llr"] = safe_float(m.group(1))
            stats["lbound"] = safe_float(m.group(2))
            stats["ubound"] = safe_float(m.group(3))
            continue

    # derive final result string
    #   improvement test: elo1 > elo0 
    #   non-regression test: elo1 < elo0
    #       [BOTH] llr > ubound = pass, llr < lbound = fail, else inconclusive
    if stats["llr"] > stats["ubound"]:
        stats["result"] = "pass"
    elif stats["llr"] < stats["lbound"]:
        stats["result"] = "fail"
    else:
        stats["result"] = "inconclusive"

    return stats



def upload_logs(args, cute_chess_stats, runtime=None):
    if system == "Windows": cnxn = sqlite3.connect('F:/databases/chess.db')
    elif system == "Darwin": cnxn = sqlite3.connect(Path.home() / "Documents/databases/chess.db")

    candidate_engine_version = etl.probe_engine_metadata(args.engine_a)['version']
    baseline_engine_version = etl.probe_engine_metadata(args.engine_b)['version']

    # get engine_id by probing db.engines via version
    candidate_engine_id = etl.get_engine_id(cnxn, version=candidate_engine_version)
    baseline_engine_id = etl.get_engine_id(cnxn, version=baseline_engine_version)

    # log sprt experiment
    sprt_id = etl.start_experiment(
        cnxn, 
        "SPRT",
        candidate_engine_id,
        comparison_engine_id = baseline_engine_id
    )


    # map search --> game
    game_map = etl.bulk_log_game(
        cnxn, 
        GAME_JSON, 
        sprt_id,
        baseline_engine_id
    )

    # log search+timing with game mapping
    etl.bulk_log_search_and_timing(
        cnxn, 
        SEARCH_JSON,
        game_map, 
        timing_path=TIMING_JSON,
        engine_id=candidate_engine_id
    )

    # log sprt experiment details in db.sprt
    etl.log_sprt(
        cnxn,
        sprt_id,  # experiment_id
        candidate_engine_id,
        baseline_engine_id,
        **{**vars(args), **cute_chess_stats},
        runtime=runtime
    )
    etl.update_experiment(
        cnxn, 
        sprt_id, 
        {"end_time_utc": datetime.now(timezone.utc).isoformat()}
    )

    etl.clear_log_dir(args.logroot)
    print(f"[DATA] Logging completed for SPRT {sprt_id}.")


def parse_args():
    p = argparse.ArgumentParser(description="SPRT runner using cutechess-cli")

    # Engines
    p.add_argument("--engine-a", required=True, help="Candidate engine path")
    p.add_argument("--engine-b", required=True, help="Baseline engine path")

    # cutechess
    p.add_argument(
        "--cutechess-cli",
        default=r"C:\Program Files (x86)\Cute Chess\cutechess-cli.exe",
        help="Path to cutechess-cli.exe"
    )

    # Time control (choose ONE)
    p.add_argument("--depth", type=int, default=None, help="Depth per move")
    p.add_argument("--time", type=float, default=None, help="Seconds per move")
    p.add_argument("--tc", type=str, default=None, help="Time control (e.g. 0+1)")

    # SPRT parameters
    p.add_argument("--elo0", type=int, default=0)
    p.add_argument("--elo1", type=int, default=10)
    p.add_argument("--alpha", type=float, default=0.05)
    p.add_argument("--beta", type=float, default=0.05)
    p.add_argument("--max-games", type=int, default=1000)

    # Opening book
    p.add_argument("--book", default= PROJECT_ROOT / "bin" / "opening_books" / "8moves_v3.pgn" , help="Opening book file")
    p.add_argument("--book-depth", type=int, default=16) # 8 full moves

    # Logging
    p.add_argument('--log', action="store_true", help="Flag to turn on logging for candidate engine")
    p.add_argument(
        "--logroot",
        default=SPRT_LOG_DIR,
        help="Root directory for SPRT logs"
    )

    return p.parse_args()



def main(args=None):
    if args is None:
        args = parse_args()

    # Validate TC
    if sum(x is not None for x in (args.depth, args.time, args.tc)) != 1:
        raise ValueError("Specify exactly one of --depth, --time, or --tc")

    print(f"[SPRT] Log directory: {args.logroot}")

    engine_a = os.path.abspath(args.engine_a)
    engine_b = os.path.abspath(args.engine_b)

    each_block = [
        "-each",
        "proto=uci"
    ]
    log_a_block = [
        f"option.log_dir={args.logroot}",
        f"option.timer_logging={"true" if args.log else "false"}",
        f"option.stats_logging={"true" if args.log else "false"}",
        f"option.game_logging={"true" if args.log else "false"}",
        f"option.uci_logging=true",
    ]
    log_b_block = [
        "option.timer_logging=false",
        "option.stats_logging=false",
        "option.game_logging=false",
        "option.uci_logging=false",
    ]

    # Time control
    if args.depth is not None:
        each_block.append(f"depth={args.depth}")
    elif args.time is not None:
        each_block += [f"st={args.time}", "timemargin=30"]
    else:
        each_block.append(f"tc={args.tc}")

    # opening book
    book_block = []
    if args.book is not None:
        book_block.append("-openings")
        book_block.append(f"file={os.path.abspath(args.book)}")
        book_block.append(f"format={os.path.splitext(args.book)[1][1:]}")
        book_block.append("order=random")
        book_block.append(f"plies={args.book_depth}")
        book_block.append("-srand")
        book_block.append("42")

    cmd = [
        args.cutechess_cli,

        # Candidate engine
        "-engine",
        "name=Candidate",
        f"cmd={engine_a}",
        #"option.stats_nodes_only=true",
        f"dir={os.path.dirname(engine_a)}",
    ] + log_a_block + [

        # Baseline engine
        "-engine",
        "name=Baseline",
        f"cmd={engine_b}",
        f"dir={os.path.dirname(engine_b)}",
    ] + log_b_block + each_block + [

        # SPRT
        "-maxmoves", "100",
        "-games", str(args.max_games),
        "-sprt",
        f"elo0={args.elo0}",
        f"elo1={args.elo1}",
        f"alpha={args.alpha}",
        f"beta={args.beta}",
    ] + book_block + [

        # Runtime
        "-repeat",
        "-concurrency", "2",
        "-pgnout", os.path.join(args.logroot, "cc_sprt.pgn"),
    ]

    print("[SPRT] Launching cutechess:")
    print(" ".join(cmd))

    output_lines = []
    start_time = time.time()

    stdout_log_path = Path(args.logroot) / "cutechess_stdout.log"
    stderr_log_path = Path(args.logroot) / "cutechess_stderr.log"

    #stdout_f = open(stdout_log_path, "w", encoding="utf-8")
    #stderr_f = open(stderr_log_path, "w", encoding="utf-8")

    with subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        #stderr=subprocess.PIPE,   # IMPORTANT: do NOT merge streams
        text=True,
        bufsize=1  # line-buffered
    ) as proc:
        
        for line in proc.stdout:
            print(line, end="")      # live console output
            output_lines.append(line)

        """
        # read both streams until process ends
        while True:
            out_line = proc.stdout.readline() if proc.stdout else ""
            err_line = proc.stderr.readline() if proc.stderr else ""

            if out_line:
                print(out_line, end="")
                stdout_f.write(out_line)
                output_lines.append(out_line)

            if err_line:
                print("[ERR]", err_line, end="")
                stderr_f.write(err_line)

            if not out_line and not err_line and proc.poll() is not None:
                break
        """

        ret = proc.wait()

    #stdout_f.close()
    #stderr_f.close()

    if ret != 0:
        raise subprocess.CalledProcessError(ret, cmd)

    run_time = time.time() - start_time
    output = "".join(output_lines)
    stats = parse_cutechess_output(output)

    upload_logs(args, cute_chess_stats=stats, runtime=run_time)

    print("[SPRT] Completed successfully")
    print(f"[SPRT] Logs written to {args.logroot}")


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print(f"[SPRT] ERROR: {e}", file=sys.stderr)
        sys.exit(1)
