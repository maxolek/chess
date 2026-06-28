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
from tests import perft, sprt, sts, tournament
import platform

system = platform.system()

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
if system == "Windows": DB_PATH = 'F:/databases/chess.db'
elif system == "Darwin": DB_PATH = str(Path.home() / "Documents/databases/chess.db")


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
        cutechess_cli=args.cutechess_cli,
        log=True
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
    parser.add_argument("--perft", action="store_true")
    parser.add_argument("--perft_positions", default=os.path.join(BIN_DIR, "test_positions", "perft.epd"))
    parser.add_argument("--perft_depth", type=int, default=5)

    # Tournament (Elo rating)
    parser.add_argument("--tournament_tc", nargs="+", default=["blitz"],
                        choices=["ultra_fast", "bullet", "blitz", "rapid", "classical"],
                        help="Time control categories for rating tournament")
    parser.add_argument("--tournament_games", type=int, default=20,
                        help="Games per opponent per time control")
    parser.add_argument("--tournament_engines", type=int, default=3,
                        help="# Engines (1st version + n-1 latest version) to play against")

    args = parser.parse_args()

    args.engine = os.path.join(ENGINES_DIR, f"{args.version}.exe")

    cnxn = sqlite3.connect(DB_PATH)

    run_make(args)

    # Probe engine options and search params via UCI
    engine_meta = etl.probe_engine_metadata(args.engine)
    opts = engine_meta.get("options", {})

    def opt_int(name):
        o = opts.get(name)
        if o is None:
            return None
        try:
            return int(o["default"])
        except (ValueError, KeyError):
            return None

    def opt_float(name):
        o = opts.get(name)
        if o is None:
            return None
        try:
            return float(o["default"])
        except (ValueError, KeyError):
            return None
        
    def opt_float_scaled(name, scale=100):
        """for params stored as int(val*scale) in UCI, recover the float (e.g. pi=314)"""
        o = opts.get(name)
        if o is None:
            return None
        try:
            return float(o["default"]) / scale
        except (ValueError, KeyError):
            return None

    engine_id = etl.register_engine(
        cnxn,
        {
            "name": args.name,
            "version": args.version,
            "description": args.description,
            # UCI engine options
            "move_overhead_ms": opt_int("Move Overhead"),
            "max_threads": opt_int("Threads"),
            "hash_size_mb": opt_int("Hash"),
            "pondering": opt_int("Ponder"),
            # search params
            "delta_prune_threshold": opt_int("DELTA_PRUNE_THRESHOLD"),
            "see_prune_threshold": opt_int("SEE_PRUNE_THRESHOLD"),
            "aspiration_window": opt_int("ASPIRATION_WINDOW"),
            "aspiration_start_depth": opt_int("ASPIRATION_START_DEPTH"),
            "aspiration_depth_scale": opt_int("ASPIRATION_DEPTH_SCALE"),
            "aspiration_research_scale": opt_float("ASPIRATION_RESEARCH_SCALE"),
            "draw_eval": opt_int("DRAW_EVAL"),
            "contempt": opt_int("CONTEMPT"),
            "r_nmp": opt_int("R_NMP"),
            "r_lmr_const": opt_float_scaled("R_LMR_CONST", 100),
            "r_lmr_denom": opt_float_scaled("R_LMR_DENOM", 100),
            "lmr_move_order_threshold": opt_int("LMR_MOVE_ORDER_THRESHOLD"),
            "lmr_depth_threshold": opt_int("LMR_DEPTH_THRESHOLD"),
        }
    )

    print('\n============================\n')

    if (args.perft): run_perft(args)
    run_sprt(args)
    if (args.sts_files): run_sts(args)

    # Rating tournament against all engines in DB
    tournament.run_tournament(args, cnxn, engine_id)

    print("[PIPELINE] ✅ release complete")

if __name__ == "__main__":
    main()
