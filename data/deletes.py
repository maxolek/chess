import sqlite3
from pathlib import Path
import argparse
from .etl.paths import RAW_DB

def drop_all_tables(db_path=str(RAW_DB)) -> None:
    """
    Drops all tables from chess.db.
    WARNING: This deletes schemas and all data permanently.
    """

    cnxn = sqlite3.connect(db_path)
    cur = cnxn.cursor()

    cur.execute("PRAGMA foreign_keys = OFF;")

    tables = [
        "timing",
        "searches_by_iteration",
        "searches_by_tree_depth",
        "searches",
        "games",
        "sprt",
        "sts",
        "perft",
        "experiments",
        "engines"
    ]

    for table in tables:
        cur.execute(f"DROP TABLE IF EXISTS {table};")

    cur.execute("PRAGMA foreign_keys = ON;")

    cnxn.commit()
    cnxn.close()

def clear_all_tables(db_path=str(RAW_DB)) -> None:
    """
    Deletes all rows from all tables but keeps schemas intact.
    Resets AUTOINCREMENT counters.
    """

    cnxn = sqlite3.connect(db_path)
    cur = cnxn.cursor()

    cur.execute("PRAGMA foreign_keys = OFF;")

    tables = [
        "timing",
        "searches_by_iteration",
        "searches_by_tree_depth",
        "searches",
        "games",
        "sprt",
        "sts",
        "perft",
        "experiments",
        "engines"
    ]

    for table in tables:
        cur.execute(f"DELETE FROM {table};")

    # Reset AUTOINCREMENT counters
    cur.execute("DELETE FROM sqlite_sequence;")

    cur.execute("PRAGMA foreign_keys = ON;")

    cnxn.commit()
    cnxn.close()

if __name__ == "__main__":
    # clear tables or drop tables
    p = argparse.ArgumentParser(description="Clear and/or delete chess.db tables")
    p.add_argument("--db", type=str, required=True)
    p.add_argument("--clear", action="store_true")
    p.add_argument("--delete", action="store_true")
    args = p.parse_args()

    if args.clear:
        clear_all_tables(str(RAW_DB.parent / args.db))
    if args.delete:
        drop_all_tables(str(RAW_DB.parent / args.db))
