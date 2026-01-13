#!/usr/bin/env python3
import os
import argparse
import subprocess
import sys
from datetime import datetime
import sqlite3
from data import etl

def upload_logs(args):
    cnxn = sqlite3.connect("F:/databases/chess.db")
    engine_version = etl.probe_engine_metadata(args.engine_a)['version']

    # get engine_id by probing db.engines via version
    engine_id = etl.get_engine_id(cnxn, version=engine_version)

    # log sprt experiment
    sprt_id = etl.start_experiment(
        cnxn, 
        "SPRT",
        engine_id
    )

    # map search --> game
    game_map = etl.bulk_log_game(
        cnxn, 
        "./logs/sprt_logs/game.jsonl", 
        sprt_id
    )
    # log search+timing with game mapping
    etl.bulk_log_search_and_timing(
        cnxn, 
        "./logs/sprt_logs/search.jsonl",
        game_map, 
        timing_path="./logs/sprt_logs/timing.jsonl"
    )

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
    p.add_argument("--book", required=True, help="Opening book file")
    p.add_argument("--book-depth", type=int, default=8)

    # Logging
    p.add_argument(
        "--logroot",
        default="../logs/sprt_logs",
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
        "proto=uci",
        f"option.log_dir={args.logroot}",
        "option.timer_logging=true",
        "option.stats_logging=true",
        "option.game_logging=true",
        "option.uci_logging=true",
    ]

    # Time control
    if args.depth is not None:
        each_block.append(f"depth={args.depth}")
    elif args.time is not None:
        each_block += [f"st={args.time}", "timemargin=100"]
    else:
        each_block.append(f"tc={args.tc}")

    cmd = [
        args.cutechess_cli,

        # Candidate engine
        "-engine",
        "name=Candidate",
        f"cmd={engine_a}",
        f"dir={os.path.dirname(engine_a)}",

        # Baseline engine
        "-engine",
        "name=Baseline",
        f"cmd={engine_b}",
        f"dir={os.path.dirname(engine_b)}",
    ] + each_block + [
        # SPRT
        "-maxmoves", "100",
        "-games", str(args.max_games),
        "-sprt",
        f"elo0={args.elo0}",
        f"elo1={args.elo1}",
        f"alpha={args.alpha}",
        f"beta={args.beta}",

        # Openings
        "-openings",
        f"file={os.path.abspath(args.book)}",
        f"format={os.path.splitext(args.book)[1][1:]}",
        "order=random",
        f"plies={args.book_depth}",
        "-srand", "42",

        # Runtime
        "-repeat",
        "-concurrency", "2",
        "-pgnout", os.path.join(args.logroot, "cc_sprt.pgn"),
    ]

    print("[SPRT] Launching cutechess:")
    print(" ".join(cmd))

    subprocess.check_call(cmd)
    upload_logs(args)
    etl.clear_log_folder(args.logroot)

    print("[SPRT] Completed successfully")
    print(f"[SPRT] Logs written to {args.logroot}")


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print(f"[SPRT] ERROR: {e}", file=sys.stderr)
        sys.exit(1)
