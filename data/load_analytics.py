"""Load raw SQLite data into analytics DuckDB.

Supports two modes:
  --full    : drop and reload all tables (original behaviour)
  (default) : incremental — only insert rows with id > max(existing id)

Usage:
  python -m data.load_analytics          # incremental
  python -m data.load_analytics --full   # full refresh
"""
import duckdb
import argparse
from . import etl
from .etl.paths import RAW_DB, ANALYTICS_DB

# ─────────────────────────────────────────────────────────────────────────────
# UDFs
# ─────────────────────────────────────────────────────────────────────────────
def opening_name_udf(moves_list: str):
    try:
        eco, name = etl.get_opening_from_moves(moves_list)
        return name or ""
    except Exception:
        return ""


def opening_code_udf(moves_list: str):
    try:
        eco, name = etl.get_opening_from_moves(moves_list)
        return eco or ""
    except Exception:
        return ""


def _register_udfs(cnxn):
    cnxn.create_function(
        "get_opening_name", opening_name_udf,
        parameters=["VARCHAR"], return_type="VARCHAR"
    )
    cnxn.create_function(
        "get_opening_code", opening_code_udf,
        parameters=["VARCHAR"], return_type="VARCHAR"
    )


# ─────────────────────────────────────────────────────────────────────────────
# Helpers
# ─────────────────────────────────────────────────────────────────────────────
def _table_exists(cnxn, table: str) -> bool:
    result = cnxn.execute(
        "SELECT COUNT(*) FROM information_schema.tables WHERE table_name = ?", [table]
    ).fetchone()
    return result[0] > 0


def _max_id(cnxn, table: str, id_col: str = 'id') -> int:
    """Return max id in an analytics table, or 0 if table doesn't exist."""
    if not _table_exists(cnxn, table):
        return 0
    result = cnxn.execute(f"SELECT COALESCE(MAX({id_col}), 0) FROM {table}").fetchone()
    return result[0]


# ─────────────────────────────────────────────────────────────────────────────
# Full refresh (original behaviour)
# ─────────────────────────────────────────────────────────────────────────────
def load_full(cnxn):
    """Drop and recreate all tables from raw SQLite."""
    print("  [full] Loading engines...")
    cnxn.execute("DROP TABLE IF EXISTS engines")
    cnxn.execute("CREATE TABLE engines AS SELECT * FROM raw.engines")

    print("  [full] Loading experiments...")
    cnxn.execute("DROP TABLE IF EXISTS experiments")
    cnxn.execute("CREATE TABLE experiments AS SELECT * FROM raw.experiments")

    print("  [full] Loading sprt_runs...")
    cnxn.execute("DROP TABLE IF EXISTS sprt_runs")
    cnxn.execute("""
        CREATE TABLE sprt_runs AS
        SELECT * REPLACE (
                    CAST(alpha AS DOUBLE) AS alpha,
                    CAST(beta AS DOUBLE) AS beta,
                    list_extract(string_split(opening_book, '\\'), -1) AS opening_book,
                    CAST((COALESCE(elo1,0) - COALESCE(elo0,0)) AS DOUBLE) AS elo_diff
                )
        FROM raw.sprt
    """)

    print("  [full] Loading sts_runs...")
    cnxn.execute("DROP TABLE IF EXISTS sts_runs")
    cnxn.execute("""
        CREATE TABLE sts_runs AS
        SELECT * REPLACE (
                    list_extract(string_split(suite, '\\'), -1) AS suite
                ),
                trim(split_part(position_name, '-', 2)) as position_type
        FROM raw.sts
    """)

    print("  [full] Loading game_stats...")
    cnxn.execute("DROP TABLE IF EXISTS game_stats")
    cnxn.execute("""
        CREATE TABLE game_stats AS
        WITH openings AS (
            SELECT *,
                 get_opening_name(moves) as opening,
                 get_opening_code(moves) as opening_eco
            FROM raw.games
        )
        SELECT * REPLACE (
                    CASE
                        WHEN result = 1 THEN 'white'
                        WHEN result = 2 THEN 'black'
                        WHEN result = 3 THEN 'draw'
                    END as result,
                    CASE
                        WHEN termination = 1 THEN 'checkmate'
                        WHEN termination = 2 THEN 'stalemate'
                        WHEN termination = 3 THEN 'threefold'
                        WHEN termination = 4 THEN 'fiftymove'
                        WHEN termination = 5 THEN 'time'
                        WHEN termination = 6 THEN 'resign'
                    END as termination
                )
            FROM openings
    """)
    # backfill any null openings
    cnxn.execute("""
        UPDATE game_stats
        SET opening = get_opening_name(moves),
            opening_eco = get_opening_code(moves)
        WHERE opening IS NULL OR opening = ''
    """)

    print("  [full] Loading search_stats...")
    cnxn.execute("DROP TABLE IF EXISTS search_stats")
    cnxn.execute("CREATE TABLE search_stats AS SELECT * FROM raw.searches")

    print("  [full] Loading iterative_deepening_stats...")
    cnxn.execute("DROP TABLE IF EXISTS iterative_deepening_stats")
    cnxn.execute("CREATE TABLE iterative_deepening_stats AS SELECT * FROM raw.searches_by_iteration")

    print("  [full] Loading search_tree_stats...")
    cnxn.execute("DROP TABLE IF EXISTS search_tree_stats")
    cnxn.execute("CREATE TABLE search_tree_stats AS SELECT * FROM raw.searches_by_tree_depth")

    print("  [full] Loading search_timings...")
    cnxn.execute("DROP TABLE IF EXISTS search_timings")
    cnxn.execute("CREATE TABLE search_timings AS SELECT * FROM raw.timing")

    print("  [full] Loading dim_positions...")
    cnxn.execute("DROP TABLE IF EXISTS dim_positions")
    cnxn.execute("""
        CREATE TABLE dim_positions AS
        SELECT id as search_id, fen, game_id, sts_id
        FROM raw.searches
    """)


# ─────────────────────────────────────────────────────────────────────────────
# Incremental load
# ─────────────────────────────────────────────────────────────────────────────
def load_incremental(cnxn):
    """Only insert new rows (by id) into existing analytics tables.
    
    If a table doesn't exist yet, falls back to full creation for that table.
    Uses BEGIN TRANSACTION for atomicity — either all inserts for a table
    succeed, or none do.
    """
    totals = {}  # table -> rows inserted

    # ── engines (small, full-refresh is fine) ──
    print("  [incr] Syncing engines...")
    cnxn.execute("DROP TABLE IF EXISTS engines")
    cnxn.execute("CREATE TABLE engines AS SELECT * FROM raw.engines")

    # ── experiments (small, full-refresh) ──
    print("  [incr] Syncing experiments...")
    cnxn.execute("DROP TABLE IF EXISTS experiments")
    cnxn.execute("CREATE TABLE experiments AS SELECT * FROM raw.experiments")

    # ── sprt_runs ──
    print("  [incr] Syncing sprt_runs...")
    if not _table_exists(cnxn, 'sprt_runs'):
        cnxn.execute("""
            CREATE TABLE sprt_runs AS
            SELECT * REPLACE (
                        CAST(alpha AS DOUBLE) AS alpha,
                        CAST(beta AS DOUBLE) AS beta,
                        list_extract(string_split(opening_book, '\\'), -1) AS opening_book,
                        CAST((COALESCE(elo1,0) - COALESCE(elo0,0)) AS DOUBLE) AS elo_diff
                    )
            FROM raw.sprt
        """)
        totals['sprt_runs'] = cnxn.execute("SELECT COUNT(*) FROM sprt_runs").fetchone()[0]
    else:
        max_id = _max_id(cnxn, 'sprt_runs')
        before = cnxn.execute("SELECT COUNT(*) FROM sprt_runs").fetchone()[0]
        cnxn.execute("BEGIN TRANSACTION")
        cnxn.execute(f"""
            INSERT INTO sprt_runs
            SELECT * REPLACE (
                        CAST(alpha AS DOUBLE) AS alpha,
                        CAST(beta AS DOUBLE) AS beta,
                        list_extract(string_split(opening_book, '\\'), -1) AS opening_book,
                        CAST((COALESCE(elo1,0) - COALESCE(elo0,0)) AS DOUBLE) AS elo_diff
                    )
            FROM raw.sprt WHERE id > {max_id}
        """)
        cnxn.execute("COMMIT")
        after = cnxn.execute("SELECT COUNT(*) FROM sprt_runs").fetchone()[0]
        totals['sprt_runs'] = after - before

    # ── sts_runs ──
    print("  [incr] Syncing sts_runs...")
    if not _table_exists(cnxn, 'sts_runs'):
        cnxn.execute("""
            CREATE TABLE sts_runs AS
            SELECT * REPLACE (
                        list_extract(string_split(suite, '\\'), -1) AS suite
                    ),
                    trim(split_part(position_name, '-', 2)) as position_type
            FROM raw.sts
        """)
        totals['sts_runs'] = cnxn.execute("SELECT COUNT(*) FROM sts_runs").fetchone()[0]
    else:
        max_id = _max_id(cnxn, 'sts_runs')
        before = cnxn.execute("SELECT COUNT(*) FROM sts_runs").fetchone()[0]
        cnxn.execute("BEGIN TRANSACTION")
        cnxn.execute(f"""
            INSERT INTO sts_runs
            SELECT * REPLACE (
                        list_extract(string_split(suite, '\\'), -1) AS suite
                    ),
                    trim(split_part(position_name, '-', 2)) as position_type
            FROM raw.sts WHERE id > {max_id}
        """)
        cnxn.execute("COMMIT")
        after = cnxn.execute("SELECT COUNT(*) FROM sts_runs").fetchone()[0]
        totals['sts_runs'] = after - before

    # ── game_stats ──
    print("  [incr] Syncing game_stats...")
    if not _table_exists(cnxn, 'game_stats'):
        cnxn.execute("""
            CREATE TABLE game_stats AS
            WITH openings AS (
                SELECT *,
                     get_opening_name(moves) as opening,
                     get_opening_code(moves) as opening_eco
                FROM raw.games
            )
            SELECT * REPLACE (
                        CASE
                            WHEN result = 1 THEN 'white'
                            WHEN result = 2 THEN 'black'
                            WHEN result = 3 THEN 'draw'
                        END as result,
                        CASE
                            WHEN termination = 1 THEN 'checkmate'
                            WHEN termination = 2 THEN 'stalemate'
                            WHEN termination = 3 THEN 'threefold'
                            WHEN termination = 4 THEN 'fiftymove'
                            WHEN termination = 5 THEN 'time'
                            WHEN termination = 6 THEN 'resign'
                        END as termination
                    )
                FROM openings
        """)
        totals['game_stats'] = cnxn.execute("SELECT COUNT(*) FROM game_stats").fetchone()[0]
    else:
        max_id = _max_id(cnxn, 'game_stats')
        before = cnxn.execute("SELECT COUNT(*) FROM game_stats").fetchone()[0]
        cnxn.execute("BEGIN TRANSACTION")
        cnxn.execute(f"""
            INSERT INTO game_stats
            WITH openings AS (
                SELECT *,
                     get_opening_name(moves) as opening,
                     get_opening_code(moves) as opening_eco
                FROM raw.games WHERE id > {max_id}
            )
            SELECT * REPLACE (
                        CASE
                            WHEN result = 1 THEN 'white'
                            WHEN result = 2 THEN 'black'
                            WHEN result = 3 THEN 'draw'
                        END as result,
                        CASE
                            WHEN termination = 1 THEN 'checkmate'
                            WHEN termination = 2 THEN 'stalemate'
                            WHEN termination = 3 THEN 'threefold'
                            WHEN termination = 4 THEN 'fiftymove'
                            WHEN termination = 5 THEN 'time'
                            WHEN termination = 6 THEN 'resign'
                        END as termination
                    )
                FROM openings
        """)
        cnxn.execute("COMMIT")
        after = cnxn.execute("SELECT COUNT(*) FROM game_stats").fetchone()[0]
        totals['game_stats'] = after - before

    # ── search_stats ──
    print("  [incr] Syncing search_stats...")
    if not _table_exists(cnxn, 'search_stats'):
        cnxn.execute("CREATE TABLE search_stats AS SELECT * FROM raw.searches")
        totals['search_stats'] = cnxn.execute("SELECT COUNT(*) FROM search_stats").fetchone()[0]
    else:
        max_id = _max_id(cnxn, 'search_stats')
        before = cnxn.execute("SELECT COUNT(*) FROM search_stats").fetchone()[0]
        cnxn.execute("BEGIN TRANSACTION")
        cnxn.execute(f"INSERT INTO search_stats SELECT * FROM raw.searches WHERE id > {max_id}")
        cnxn.execute("COMMIT")
        after = cnxn.execute("SELECT COUNT(*) FROM search_stats").fetchone()[0]
        totals['search_stats'] = after - before

    # ── iterative_deepening_stats ──
    print("  [incr] Syncing iterative_deepening_stats...")
    if not _table_exists(cnxn, 'iterative_deepening_stats'):
        cnxn.execute("CREATE TABLE iterative_deepening_stats AS SELECT * FROM raw.searches_by_iteration")
        totals['iterative_deepening_stats'] = cnxn.execute("SELECT COUNT(*) FROM iterative_deepening_stats").fetchone()[0]
    else:
        max_id = _max_id(cnxn, 'iterative_deepening_stats')
        before = cnxn.execute("SELECT COUNT(*) FROM iterative_deepening_stats").fetchone()[0]
        cnxn.execute("BEGIN TRANSACTION")
        cnxn.execute(f"INSERT INTO iterative_deepening_stats SELECT * FROM raw.searches_by_iteration WHERE id > {max_id}")
        cnxn.execute("COMMIT")
        after = cnxn.execute("SELECT COUNT(*) FROM iterative_deepening_stats").fetchone()[0]
        totals['iterative_deepening_stats'] = after - before

    # ── search_tree_stats ──
    print("  [incr] Syncing search_tree_stats...")
    if not _table_exists(cnxn, 'search_tree_stats'):
        cnxn.execute("CREATE TABLE search_tree_stats AS SELECT * FROM raw.searches_by_tree_depth")
        totals['search_tree_stats'] = cnxn.execute("SELECT COUNT(*) FROM search_tree_stats").fetchone()[0]
    else:
        max_id = _max_id(cnxn, 'search_tree_stats')
        before = cnxn.execute("SELECT COUNT(*) FROM search_tree_stats").fetchone()[0]
        cnxn.execute("BEGIN TRANSACTION")
        cnxn.execute(f"INSERT INTO search_tree_stats SELECT * FROM raw.searches_by_tree_depth WHERE id > {max_id}")
        cnxn.execute("COMMIT")
        after = cnxn.execute("SELECT COUNT(*) FROM search_tree_stats").fetchone()[0]
        totals['search_tree_stats'] = after - before

    # ── search_timings ──
    print("  [incr] Syncing search_timings...")
    if not _table_exists(cnxn, 'search_timings'):
        cnxn.execute("CREATE TABLE search_timings AS SELECT * FROM raw.timing")
        totals['search_timings'] = cnxn.execute("SELECT COUNT(*) FROM search_timings").fetchone()[0]
    else:
        max_id = _max_id(cnxn, 'search_timings')
        before = cnxn.execute("SELECT COUNT(*) FROM search_timings").fetchone()[0]
        cnxn.execute("BEGIN TRANSACTION")
        cnxn.execute(f"INSERT INTO search_timings SELECT * FROM raw.timing WHERE id > {max_id}")
        cnxn.execute("COMMIT")
        after = cnxn.execute("SELECT COUNT(*) FROM search_timings").fetchone()[0]
        totals['search_timings'] = after - before

    # ── dim_positions (derived from search_stats) ──
    print("  [incr] Syncing dim_positions...")
    if not _table_exists(cnxn, 'dim_positions'):
        cnxn.execute("""
            CREATE TABLE dim_positions AS
            SELECT id as search_id, fen, game_id, sts_id
            FROM raw.searches
        """)
        totals['dim_positions'] = cnxn.execute("SELECT COUNT(*) FROM dim_positions").fetchone()[0]
    else:
        max_id = _max_id(cnxn, 'dim_positions', id_col='search_id')
        before = cnxn.execute("SELECT COUNT(*) FROM dim_positions").fetchone()[0]
        cnxn.execute("BEGIN TRANSACTION")
        cnxn.execute(f"""
            INSERT INTO dim_positions
            SELECT id as search_id, fen, game_id, sts_id
            FROM raw.searches WHERE id > {max_id}
        """)
        cnxn.execute("COMMIT")
        after = cnxn.execute("SELECT COUNT(*) FROM dim_positions").fetchone()[0]
        totals['dim_positions'] = after - before

    # ── Summary ──
    print("\n  Incremental load summary:")
    for table, count in totals.items():
        status = f"+{count:,} rows" if count > 0 else "up to date"
        print(f"    {table:.<35s} {status}")


# ─────────────────────────────────────────────────────────────────────────────
# Main entry point
# ─────────────────────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description="Load raw data into analytics DuckDB")
    parser.add_argument('--full', action='store_true', help='Full refresh (drop + reload all tables)')
    args = parser.parse_args()

    print(f"RAW_DB:       {RAW_DB}")
    print(f"ANALYTICS_DB: {ANALYTICS_DB}")

    cnxn = duckdb.connect(str(ANALYTICS_DB))
    cnxn.execute(f"ATTACH '{RAW_DB}' AS raw (TYPE SQLITE)")
    _register_udfs(cnxn)

    try:
        if args.full:
            print("Running FULL refresh...")
            load_full(cnxn)
        else:
            print("Running INCREMENTAL load...")
            load_incremental(cnxn)
        print("Load completed successfully.")
    except Exception:
        # On failure, attempt rollback if in a transaction
        try:
            cnxn.execute("ROLLBACK")
        except Exception:
            pass
        raise
    finally:
        cnxn.close()


if __name__ == '__main__':
    main()