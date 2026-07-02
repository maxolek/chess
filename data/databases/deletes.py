import sqlite3
from pathlib import Path
import argparse
from ..etl.paths import RAW_DB

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
    print(f"[DB] Dropped all tables from {db_path}")

def clear_all_tables(db_path=str(RAW_DB), exclude_engines=False) -> None:
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
    ] + (["engines"] if not exclude_engines else [])

    for table in tables:
        cur.execute(f"DELETE FROM {table};")

    # Reset AUTOINCREMENT counters
    cur.execute("DELETE FROM sqlite_sequence;")

    cur.execute("PRAGMA foreign_keys = ON;")

    cnxn.commit()
    cnxn.close()
    print(f"[DB] Cleared all tables from {db_path} {'(excluding engines)' if exclude_engines else ''}")

if __name__ == "__main__":
    # clear tables or drop tables
    p = argparse.ArgumentParser(description="Clear and/or delete chess.db tables")
    p.add_argument("--db", type=str, required=True)
    p.add_argument("--clear", action="store_true")
    p.add_argument("--clear_no_engines", action="store_true")
    p.add_argument("--delete", action="store_true")
    args = p.parse_args()

    if args.clear:
        db_path = str(RAW_DB.parent / args.db)
        print(f"[DB] Clearing all tables in {db_path}")
        clear_all_tables(db_path)
    if args.clear_no_engines:
        db_path = str(RAW_DB.parent / args.db)
        print(f"[DB] Clearing all tables in {db_path} (excluding engines)")
        clear_all_tables(db_path, exclude_engines=True)
    if args.delete:
        db_path = str(RAW_DB.parent / args.db)
        print(f"[DB] Dropping all tables in {db_path}")
        drop_all_tables(db_path)
