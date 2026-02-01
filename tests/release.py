#!/usr/bin/env python3
import argparse
import subprocess
import os
import sys
import shutil
import sqlite3
from datetime import datetime
from pathlib import Path
from data import etl
from tests import perft, sprt, sts

# ============================================================
# Project layout (run from /chess)
# ============================================================

# paths
TESTS_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = TESTS_DIR.parent

ENGINES_DIR = os.path.join(PROJECT_ROOT, "engines")
LOGS_DIR    = os.path.join(PROJECT_ROOT, "logs")
TESTS_DIR   = os.path.join(PROJECT_ROOT, "tests")
BIN_DIR     = os.path.join(PROJECT_ROOT, "bin")
DB_PATH     = "F:/databases/chess.db"

SPRT_LOG_DIR  = os.path.join(LOGS_DIR, "sprt_logs")
STS_LOG_DIR   = os.path.join(LOGS_DIR, "sts_logs")
PERFT_LOG_DIR = os.path.join(LOGS_DIR, "perft")

STOCKFISH = os.path.join(
    ENGINES_DIR,
    "stockfish",
    "stockfish-windows-x86-64-avx2.exe"
)

# ============================================================
# Build
# ============================================================

def run_make(args):
    print(f"[BUILD] Compiling engine VERSION={args.version}")

    build_dir = "build"

    configure_cmd = [
        "cmake",
        "-S", ".",                # source dir
        "-B", build_dir,           # build dir
        f"-DVERSION={args.version}",
        "-DCMAKE_BUILD_TYPE=Release",
    ]

    build_cmd = [
        "cmake",
        "--build", build_dir,
        "--config", "Release",     # important for MSVC
        "--parallel",
    ]

    try:
        subprocess.check_call(configure_cmd)
        subprocess.check_call(build_cmd)
    except subprocess.CalledProcessError:
        sys.exit("[BUILD] ❌ build failed")

    print("[BUILD] ✅ done")

# ============================================================
# SPRT
# ============================================================

def run_sprt(args):
    print("[SPRT] Running SPRT")

    sprt_args = argparse.Namespace(
        engine_a=args.engine,
        engine_b=args.base_engine, 
        depth=args.sprt_depth,
        time=args.sprt_time,
        tc=args.sprt_tc,
        elo0=args.elo0,
        elo1=args.elo1,
        alpha=args.alpha,
        beta=args.beta,
        max_games=args.sprt_games,
        book=args.opening_book,
        book_depth=args.sprt_book_depth,
        logroot=SPRT_LOG_DIR,
        cutechess_cli=args.cutechess_cli
    )

    sprt.main(sprt_args)

# ============================================================
# STS
# ============================================================

def run_sts(args):
    print("[STS] Running STS")

    sts_args = argparse.Namespace(
        engine=args.engine,
        time=args.sts_time,
        depth=args.sts_depth,
        sts=args.sts_files,
        log_dir=STS_LOG_DIR
    )

    sts.main(sts_args)

# ============================================================
# PERFT
# ============================================================

def run_perft(args):
    print("[PERFT] Running PERFT")

    perft_args = argparse.Namespace(
        engine=args.engine, 
        stockfish=STOCKFISH, 
        positions=args.perft_positions,
        depth=args.perft_depth
    )

    perft.main(perft_args)

# ============================================================
# Main
# ============================================================

def main():
    parser = argparse.ArgumentParser("Release pipeline")

    parser.add_argument("--version", required=True)
    parser.add_argument("--name")
    parser.add_argument("--description")
    parser.add_argument("--makefile", default="makefile.mak")

    parser.add_argument("--cutechess-cli", default=r"C:\Program Files (x86)\Cute Chess\cutechess-cli.exe", help="Full path to cutechess-cli.exe\nUsed for game testing")

    # SPRT
    parser.add_argument("--base_engine", default=os.path.join(ENGINES_DIR, "classic.exe"))
    parser.add_argument("--elo0", type=int, default=0)
    parser.add_argument("--elo1", type=float, default=10)
    parser.add_argument("--alpha", type=float, default=0.05)
    parser.add_argument("--beta", type=float, default=0.05)
    parser.add_argument("--sprt_tc", type=str)
    parser.add_argument("--sprt_time", type=float)
    parser.add_argument("--sprt_depth", type=int)
    parser.add_argument("--sprt_games", type=int, default=1000)
    parser.add_argument("--opening_book", default=os.path.join(BIN_DIR, "opening_books", "8moves_v3.pgn"))
    parser.add_argument("--sprt_book_depth", type=int, default=8)

    # STS
    parser.add_argument("--sts_files", nargs="+")
    parser.add_argument("--sts_time", type=float, default=5000)
    parser.add_argument("--sts_depth", type=int)

    # PERFT
    parser.add_argument("--perft", default=False)
    parser.add_argument("--perft_positions", default=os.path.join(BIN_DIR, "test_positions", "perft.epd"))
    parser.add_argument("--perft_depth", type=int, default=5)

    args = parser.parse_args()

    args.engine = os.path.join(ENGINES_DIR, f"{args.version}.exe")

    cnxn = sqlite3.connect(DB_PATH)

    run_make(args)

    engine_id = etl.register_engine(
        cnxn,
        {
            "name": args.name,
            "version": args.version,
            "description": args.description,
        }
    )

    print('\n============================\n')

    if (args.perft): run_perft(args)
    run_sprt(args)
    run_sts(args)

    print("[PIPELINE] ✅ release complete")

if __name__ == "__main__":
    main()
