"""
Normalize engine and game metadata in the analytics DuckDB and create helper columns.
- ensure `engines.normalized_version` (string)
- ensure `game_stats.date_iso` (TIMESTAMP) when `date` exists
- create simple indexes where available (DuckDB may ignore indexes but commands are safe)
"""
import duckdb

if __name__ == '__main__':
    from etl.paths import ANALYTICS_DB
    con = duckdb.connect(str(ANALYTICS_DB))

    # engines.normalized_version
    try:
        cur = con.execute("PRAGMA table_info('engines')").fetchall()
        cols = {r[1] for r in cur}
        if 'normalized_version' not in cols:
            con.execute("ALTER TABLE engines ADD COLUMN normalized_version VARCHAR")
        con.execute("UPDATE engines SET normalized_version = COALESCE(version, '')")
        print('Normalized engines.version -> engines.normalized_version')
    except Exception as e:
        print('engines normalization skipped:', e)

    # game_stats.date_iso
    try:
        cur = con.execute("PRAGMA table_info('game_stats')").fetchall()
        cols = {r[1] for r in cur}
        if 'date' in cols and 'date_iso' not in cols:
            con.execute("ALTER TABLE game_stats ADD COLUMN date_iso TIMESTAMP")
            # TRY_CAST is available in DuckDB to avoid errors when parsing
            con.execute("UPDATE game_stats SET date_iso = TRY_CAST(date AS TIMESTAMP)")
            print('Populated game_stats.date_iso from game_stats.date')
    except Exception as e:
        print('game_stats date normalization skipped:', e)

    # create simple indexes (no-op if unsupported)
    try:
        con.execute("CREATE INDEX IF NOT EXISTS idx_search_features_engine_label ON search_features(engine_label)")
        con.execute("CREATE INDEX IF NOT EXISTS idx_search_features_game_id ON search_features(game_id)")
        print('Created indexes on search_features')
    except Exception as e:
        print('Index creation skipped or unsupported:', e)

    con.close()
    print('Done')
