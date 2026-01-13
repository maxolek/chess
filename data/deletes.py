import sqlite3
from pathlib import Path

def drop_all_tables(db_dir="F:/databases") -> None:
    """
    Drops all tables from chess.db.
    WARNING: This deletes schemas and all data permanently.
    """

    db_path = Path(db_dir) / "chess.db"
    cnxn = sqlite3.connect(db_path)
    cur = cnxn.cursor()

    cur.execute("PRAGMA foreign_keys = OFF;")

    tables = [
        "timing",
        "searches_by_depth",
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

def clear_all_tables(db_dir="F:/databases") -> None:
    """
    Deletes all rows from all tables but keeps schemas intact.
    Resets AUTOINCREMENT counters.
    """

    db_path = Path(db_dir) / "chess.db"
    cnxn = sqlite3.connect(db_path)
    cur = cnxn.cursor()

    cur.execute("PRAGMA foreign_keys = OFF;")

    tables = [
        "timing",
        "searches_by_depth",
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
    clear_all_tables()