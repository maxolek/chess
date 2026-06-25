"""CLI entry point for ETL operations.

Usage:
  python -m data.etl --register_engine --name Tomahawk --version 1.0
  python -m data.etl --log_games
"""
import sqlite3
import argparse
import platform
import os
from pathlib import Path

from .db import get_db
from .ingest import register_engine, log_games_directory


def main():
    system = platform.system()

    p = argparse.ArgumentParser(
        description="ETL functions for chess.db\nRegister engines, upload logs, clear directories"
    )

    p.add_argument("--register_engine", action="store_true",
                   help="Flag to register engine to chess.db")
    p.add_argument("--name", default=None, type=str, help="Engine name")
    p.add_argument("--version", default=None, type=str, help="Engine version")
    p.add_argument("--description", default=None, type=str,
                   help="Engine description (changes from last iteration, etc)")
    p.add_argument("--uci_options", default=None, type=str,
                   help="UCI settings of the engine (e.g. threads, hash size, etc.)")

    p.add_argument("--log_games", action="store_true",
                   help="Flag to log game directory to chess.db")

    args = p.parse_args()

    # Resolve DB path
    if system == "Windows":
        _default_raw = 'F:/databases/chess.db'
    elif system == "Darwin":
        _default_raw = str(Path.home() / "Documents/databases/chess.db")
    else:
        _default_raw = str(Path.home() / "Documents/databases/chess.db")
    raw_path = os.environ.get('CHESS_RAW_DB') or _default_raw

    cnxn = sqlite3.connect(raw_path)

    if args.register_engine:
        register_engine(cnxn, {
            "name": args.name,
            "version": args.version,
            "description": args.description,
            "uci_options": args.uci_options,
        })
    elif args.log_games:
        log_games_directory(cnxn)

    cnxn.close()


if __name__ == '__main__':
    main()
