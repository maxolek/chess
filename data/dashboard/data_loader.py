"""
Data loading, enrichment, and filtering — SCALABLE version.

Design:
- Small reference tables (engines, experiments, sprt, sts, timing, positions) stay in memory.
- Large tables (search_features, iter, tree) are queried on demand via DuckDB.
- apply_filters() pushes WHERE clauses to DuckDB and returns at most QUERY_LIMIT rows.
- query_iter() / query_tree() fetch detail rows on demand for specific search_ids.
"""
import pandas as pd
import numpy as np
import duckdb

from .config import DB_PATH, EVAL_CLIP_CP, NUMERIC_EXCLUDE, numeric_cols

# ──────────────────────────────────────────────────────────────────────────────
# CONFIGURATION
# ──────────────────────────────────────────────────────────────────────────────

# Max rows pulled into pandas for any single query.  Charts degrade gracefully
# beyond this (scatter → sample, aggregations stay accurate via SQL).
QUERY_LIMIT = 1_000_000
SCATTER_SAMPLE = 100_000

# The primary search table — prefer `search_features` if it exists.
_SEARCH_TABLE: str = "search_stats"


# ──────────────────────────────────────────────────────────────────────────────
# CONNECTION
# ──────────────────────────────────────────────────────────────────────────────

def get_connection(path: str):
    import os
    if not os.path.exists(path):
        print(f"[WARN] Database not found at {path}; using in-memory DuckDB")
        return duckdb.connect(":memory:")
    return duckdb.connect(path, read_only=True)


def _query(sql: str) -> pd.DataFrame:
    """Execute a query against the shared connection."""
    return con.execute(sql).df()


def safe_query(sql: str, fallback: pd.DataFrame = None) -> pd.DataFrame:
    try:
        return _query(sql)
    except Exception as e:
        print(f"[WARN] Query failed: {e}\n  SQL: {sql[:120]}")
        return fallback if fallback is not None else pd.DataFrame()


con = get_connection(DB_PATH)


# ──────────────────────────────────────────────────────────────────────────────
# DETECT SEARCH TABLE
# ──────────────────────────────────────────────────────────────────────────────

try:
    _tables = [r[0] for r in con.execute("SHOW TABLES").fetchall()]
except Exception:
    _tables = []

if "search_features" in _tables:
    _SEARCH_TABLE = "search_features"

# Get column list for the search table (used for dropdown population, schema awareness)
try:
    _SEARCH_COLUMNS = [r[0] for r in con.execute(f"DESCRIBE {_SEARCH_TABLE}").fetchall()]
except Exception:
    _SEARCH_COLUMNS = []

# Column aliases: search_features uses different names than search_stats
_EVAL_COL = "eval" if "eval" in _SEARCH_COLUMNS else ("final_eval" if "final_eval" in _SEARCH_COLUMNS else None)
_DEPTH_COL = "depth" if "depth" in _SEARCH_COLUMNS else ("max_depth" if "max_depth" in _SEARCH_COLUMNS else None)


# ──────────────────────────────────────────────────────────────────────────────
# SMALL REFERENCE TABLES (stay in memory — bounded size)
# ──────────────────────────────────────────────────────────────────────────────

engines_df      = safe_query("SELECT * FROM engines")
experiments_df  = safe_query("SELECT * FROM experiments")
games_df        = safe_query("SELECT * FROM game_stats")
sprt_df         = safe_query("SELECT * FROM sprt_runs")
sts_df          = safe_query("SELECT * FROM sts_runs")
positions_df    = safe_query("SELECT * FROM dim_positions")
timing_df       = safe_query("SELECT * FROM search_timings")
perft_df        = pd.DataFrame()

# Map engine IDs to names
if not engines_df.empty:
    _ename = engines_df.set_index('id')['name']
    if 'white_engine_id' in games_df.columns:
        games_df['white_name'] = games_df['white_engine_id'].map(_ename)
    if 'black_engine_id' in games_df.columns:
        games_df['black_name'] = games_df['black_engine_id'].map(_ename)
    if 'white_name' in games_df.columns and 'black_name' in games_df.columns:
        games_df['game_label'] = games_df.apply(
            lambda r: f"{r.get('white_name','?')} vs {r.get('black_name','?')} ({r.get('result','')})",
            axis=1
        )


def _attach_experiment(df: pd.DataFrame, experiments: pd.DataFrame, engines: pd.DataFrame) -> pd.DataFrame:
    if df.empty or experiments.empty:
        return df
    ename = engines.set_index("id")["name"] if not engines.empty else pd.Series(dtype=str)
    exp = experiments.copy()
    exp["engine_name"] = exp["engine_id"].map(ename)
    if "comparison_engine_id" in exp.columns:
        exp["comparison_name"] = exp["comparison_engine_id"].map(ename)
    exp_cols = [c for c in ["id", "engine_id", "engine_name", "comparison_name", "type"] if c in exp.columns]
    exp_map = exp[exp_cols].rename(columns={"id": "experiment_id"})
    if "experiment_id" in df.columns:
        return df.merge(exp_map, on="experiment_id", how="left", suffixes=("", "_exp"))
    return df


sprt_df = _attach_experiment(sprt_df, experiments_df, engines_df)
sts_df  = _attach_experiment(sts_df, experiments_df, engines_df)


# ──────────────────────────────────────────────────────────────────────────────
# DATA STATS (for sidebar)
# ──────────────────────────────────────────────────────────────────────────────

def _get_data_stats() -> dict:
    stats = {
        "n_games": len(games_df),
        "n_engines": len(engines_df),
    }
    # Count searches via SQL (don't load them)
    try:
        stats["n_searches"] = con.execute(f"SELECT COUNT(*) FROM {_SEARCH_TABLE}").fetchone()[0]
    except Exception:
        stats["n_searches"] = 0

    try:
        stats["n_iter"] = con.execute("SELECT COUNT(*) FROM iterative_deepening_stats").fetchone()[0]
    except Exception:
        stats["n_iter"] = 0

    # Last updated timestamp
    ts_candidates = ["ingestion_timestamp_utc", "created_at", "timestamp"]
    for col in ts_candidates:
        if col in _SEARCH_COLUMNS:
            try:
                row = con.execute(f"SELECT MAX({col}) FROM {_SEARCH_TABLE}").fetchone()
                if row and row[0]:
                    stats["last_updated"] = pd.to_datetime(row[0]).strftime("%Y-%m-%d %H:%M")
                    break
            except Exception:
                pass
    else:
        stats["last_updated"] = "unknown"

    return stats


DATA_STATS = _get_data_stats()


# ──────────────────────────────────────────────────────────────────────────────
# DROPDOWN OPTIONS (lightweight DISTINCT queries)
# ──────────────────────────────────────────────────────────────────────────────

def _distinct_opts(table: str, col: str) -> list[dict]:
    """Get distinct values from a column for dropdown options."""
    try:
        vals = con.execute(f"SELECT DISTINCT {col} FROM {table} WHERE {col} IS NOT NULL ORDER BY {col}").fetchall()
        return [{"label": str(r[0]), "value": r[0]} for r in vals]
    except Exception:
        return []


engine_options = (
    [{"label": f"{r['name']} ({r['version']})", "value": r["id"]}
     for _, r in engines_df[["id", "name", "version"]].drop_duplicates().iterrows()]
    if not engines_df.empty else []
)

opening_options  = _distinct_opts("game_stats", "opening")
pos_type_options = _distinct_opts(_SEARCH_TABLE, "pos_label") if "pos_label" in _SEARCH_COLUMNS else []
game_phase_options = _distinct_opts(_SEARCH_TABLE, "game_phase") if "game_phase" in _SEARCH_COLUMNS else []

# Result and side are computed columns — use fixed known values
side_options = [{"label": v, "value": v} for v in ["white", "black", "unknown"]]
result_options = [{"label": v, "value": v} for v in ["Win", "Draw", "Loss"]]


# ──────────────────────────────────────────────────────────────────────────────
# SCHEMA-AWARE COLUMN LISTS (for dropdowns, without loading data)
# ──────────────────────────────────────────────────────────────────────────────

def _get_numeric_columns(table: str) -> list[str]:
    """Get numeric column names from a table's schema."""
    try:
        desc = con.execute(f"DESCRIBE {table}").fetchall()
        numeric_types = {"INTEGER", "BIGINT", "DOUBLE", "FLOAT", "REAL", "DECIMAL",
                         "SMALLINT", "TINYINT", "HUGEINT", "INT", "INT4", "INT8"}
        cols = [r[0] for r in desc
                if any(t in r[1].upper() for t in numeric_types)
                and r[0] not in NUMERIC_EXCLUDE]
        from .config import _METRIC_RANK
        cols.sort(key=lambda c: (_METRIC_RANK.get(c, 9999), c))
        return cols
    except Exception:
        return []


_search_nums = _get_numeric_columns(_SEARCH_TABLE)
_iter_nums   = _get_numeric_columns("search_iteration_features") if "search_iteration_features" in _tables else (
    _get_numeric_columns("iterative_deepening_stats") if "iterative_deepening_stats" in _tables else []
)
_tree_nums   = _get_numeric_columns("search_tree_features") if "search_tree_features" in _tables else (
    _get_numeric_columns("search_tree_stats") if "search_tree_stats" in _tables else []
)


# ──────────────────────────────────────────────────────────────────────────────
# ENRICHMENT (applied to query results, not global state)
# ──────────────────────────────────────────────────────────────────────────────

def enrich_searches(df: pd.DataFrame) -> pd.DataFrame:
    """Attach engine name, side, result label to a searches DataFrame."""
    if df.empty:
        return df

    # Engine label
    if not engines_df.empty and "engine_id" in df.columns:
        emap = engines_df.set_index("id")[["name", "version"]].rename(
            columns={"name": "engine_name", "version": "engine_version"}
        )
        df = df.join(emap, on="engine_id")

    if "engine_name" in df.columns:
        try:
            df["engine_version"] = df.get("engine_version", "").fillna("")
            df["engine_label"] = df["engine_name"].astype(str) + (
                df["engine_version"].astype(str).apply(lambda v: f" ({v})" if v and v != 'None' else "")
            )
        except Exception:
            df["engine_label"] = df["engine_name"].astype(str)

    # Join game info for side/result
    if not games_df.empty and "game_id" in df.columns:
        gcols = ["id", "white_engine_id", "black_engine_id", "result", "opening"]
        gcols = [c for c in gcols if c in games_df.columns]
        gmap = games_df[gcols].rename(columns={"id": "game_id"})
        df = df.merge(gmap, on="game_id", how="left", suffixes=("", "_game"))

    # Side
    if "white_engine_id" in df.columns and "black_engine_id" in df.columns:
        df["engine_side"] = df.apply(
            lambda r: "white" if r.get("engine_id") == r.get("white_engine_id")
            else ("black" if r.get("engine_id") == r.get("black_engine_id") else "unknown"),
            axis=1
        )
    else:
        df["engine_side"] = "unknown"

    # Result label
    def _result(r):
        res = str(r.get("result", "")).strip()
        side = r.get("engine_side", "unknown")
        if res in ("1-0", "1", "white"):
            return "Win" if side == "white" else "Loss"
        if res in ("0-1", "2", "black"):
            return "Win" if side == "black" else "Loss"
        if res in ("1/2-1/2", "3", "\u00bd-\u00bd", "draw"):
            return "Draw"
        return None

    df["result_label"] = df.apply(_result, axis=1)

    # Normalize eval/depth column names
    if "final_eval" in df.columns and "eval" not in df.columns:
        df["eval"] = df["final_eval"]
    if "max_depth" in df.columns and "depth" not in df.columns:
        df["depth"] = df["max_depth"]

    return df


# ──────────────────────────────────────────────────────────────────────────────
# FILTER FUNCTION — pushes filters to DuckDB
# ──────────────────────────────────────────────────────────────────────────────

def _build_where_clauses(
    engine_ids: list | None,
    pos_type_vals: list | None,
    game_phase_vals: list | None,
    include_mates: bool,
    mates_only: bool,
) -> list[str]:
    """Build SQL WHERE clauses for the search table."""
    clauses = []

    # Depth sanity
    if _DEPTH_COL:
        clauses.append(f"{_DEPTH_COL} BETWEEN 0 AND 60")

    # Eval mate filtering
    if _EVAL_COL:
        if mates_only:
            clauses.append(f"ABS({_EVAL_COL}) > {EVAL_CLIP_CP}")
        elif not include_mates:
            clauses.append(f"ABS({_EVAL_COL}) <= {EVAL_CLIP_CP}")

    # Engine filter
    if engine_ids:
        ids_str = ", ".join(str(int(i)) for i in engine_ids)
        clauses.append(f"engine_id IN ({ids_str})")

    # Position type
    if pos_type_vals and "pos_label" in _SEARCH_COLUMNS:
        vals_str = ", ".join(f"'{v}'" for v in pos_type_vals)
        clauses.append(f"pos_label IN ({vals_str})")

    # Game phase
    if game_phase_vals and "game_phase" in _SEARCH_COLUMNS:
        vals_str = ", ".join(f"'{v}'" for v in game_phase_vals)
        clauses.append(f"game_phase IN ({vals_str})")

    return clauses


def apply_filters(
    engine_ids: list | None,
    result_vals: list | None,
    opening_vals: list | None,
    side_vals: list | None,
    pos_type_vals: list | None,
    game_phase_vals: list | None = None,
    include_mates: bool = False,
    mates_only: bool = False,
    limit: int = QUERY_LIMIT,
) -> tuple[pd.DataFrame, pd.DataFrame]:
    """Query DuckDB with filters pushed down, return (games_df, searches_df).

    The searches DataFrame is capped at `limit` rows (sampled via ORDER BY random()
    if the result set is larger). Games are filtered in memory (small table).
    """
    # ── Games filtering (in-memory, always small) ──
    gf = games_df.copy()
    if engine_ids:
        if "white_engine_id" in gf.columns:
            gf = gf[gf["white_engine_id"].isin(engine_ids) | gf["black_engine_id"].isin(engine_ids)]
    if opening_vals and "opening" in gf.columns:
        gf = gf[gf["opening"].isin(opening_vals)]

    # ── Searches filtering (pushed to DuckDB) ──
    where = _build_where_clauses(engine_ids, pos_type_vals, game_phase_vals, include_mates, mates_only)

    # Game-level filtering: restrict to game_ids matching the filtered games
    if opening_vals and "game_id" in _SEARCH_COLUMNS and not gf.empty and "id" in gf.columns:
        game_ids = gf["id"].tolist()
        if len(game_ids) <= 500:
            ids_str = ", ".join(str(int(i)) for i in game_ids)
            where.append(f"game_id IN ({ids_str})")
        # If >500 game_ids, we'll post-filter in pandas (rare case)

    where_sql = (" WHERE " + " AND ".join(where)) if where else ""

    # Check result set size to decide if we need sampling
    count_sql = f"SELECT COUNT(*) FROM {_SEARCH_TABLE}{where_sql}"
    try:
        total_rows = con.execute(count_sql).fetchone()[0]
    except Exception:
        total_rows = 0

    if total_rows == 0:
        sf = pd.DataFrame(columns=_SEARCH_COLUMNS)
    elif total_rows <= limit:
        sql = f"SELECT * FROM {_SEARCH_TABLE}{where_sql}"
        sf = _query(sql)
    else:
        # Use reservoir sampling via ORDER BY random() LIMIT for uniform distribution
        sql = f"SELECT * FROM {_SEARCH_TABLE}{where_sql} ORDER BY random() LIMIT {limit}"
        sf = _query(sql)

    # Enrich with engine names, side, result
    sf = enrich_searches(sf)

    # Post-filter: result and side (these are computed columns)
    if result_vals and "result_label" in sf.columns:
        sf = sf[sf["result_label"].isin(result_vals)]
    if side_vals and "engine_side" in sf.columns:
        sf = sf[sf["engine_side"].isin(side_vals)]

    # If we had too many game_ids, post-filter now
    if opening_vals and "game_id" in sf.columns and "id" in gf.columns:
        sf = sf[sf["game_id"].isin(gf["id"].unique())]

    return gf, sf


# ──────────────────────────────────────────────────────────────────────────────
# ON-DEMAND QUERIES FOR DETAIL TABLES
# ──────────────────────────────────────────────────────────────────────────────

def query_iter(search_ids, limit: int = QUERY_LIMIT) -> pd.DataFrame:
    """Fetch iteration data for given search_ids from DuckDB."""
    table = "search_iteration_features" if "search_iteration_features" in _tables else "iterative_deepening_stats"
    if search_ids is None or len(search_ids) == 0:
        return pd.DataFrame()

    # For very large sets, limit the search_ids queried
    ids = list(search_ids[:2000])
    ids_str = ", ".join(str(int(i)) for i in ids)
    sql = f"SELECT * FROM {table} WHERE search_id IN ({ids_str}) LIMIT {limit}"
    try:
        return _query(sql)
    except Exception:
        return pd.DataFrame()


def query_tree(search_ids, limit: int = QUERY_LIMIT) -> pd.DataFrame:
    """Fetch tree depth data for given search_ids from DuckDB."""
    table = "search_tree_features" if "search_tree_features" in _tables else "search_tree_stats"
    if search_ids is None or len(search_ids) == 0:
        return pd.DataFrame()

    ids = list(search_ids[:2000])
    ids_str = ", ".join(str(int(i)) for i in ids)
    sql = f"SELECT * FROM {table} WHERE search_id IN ({ids_str}) LIMIT {limit}"
    try:
        return _query(sql)
    except Exception:
        return pd.DataFrame()


def query_iter_single(search_id: int) -> pd.DataFrame:
    """Fetch iteration data for a single search (e.g., FEN detail view)."""
    table = "search_iteration_features" if "search_iteration_features" in _tables else "iterative_deepening_stats"
    try:
        return _query(f"SELECT * FROM {table} WHERE search_id = {int(search_id)} ORDER BY depth")
    except Exception:
        return pd.DataFrame()


def query_searches_for_game(game_id: int) -> pd.DataFrame:
    """Fetch all searches for a specific game (for game progress chart)."""
    try:
        df = _query(f"SELECT * FROM {_SEARCH_TABLE} WHERE game_id = {int(game_id)} ORDER BY ply")
        return enrich_searches(df)
    except Exception:
        return pd.DataFrame()


# ──────────────────────────────────────────────────────────────────────────────
# BACKWARD-COMPATIBLE EXPORTS
# ──────────────────────────────────────────────────────────────────────────────
# These provide small/empty DataFrames for code that checks `.empty` or
# iterates columns. They are NOT meant to hold all data.

# A small sample for column-type inference (used by numeric_cols() in dropdowns)
try:
    searches_df = _query(f"SELECT * FROM {_SEARCH_TABLE} LIMIT 100")
    searches_df = enrich_searches(searches_df)
except Exception:
    searches_df = pd.DataFrame()

try:
    _iter_table = "search_iteration_features" if "search_iteration_features" in _tables else "iterative_deepening_stats"
    iter_df = _query(f"SELECT * FROM {_iter_table} LIMIT 0")
except Exception:
    iter_df = pd.DataFrame()

try:
    _tree_table = "search_tree_features" if "search_tree_features" in _tables else "search_tree_stats"
    tree_df = _query(f"SELECT * FROM {_tree_table} LIMIT 0")
except Exception:
    tree_df = pd.DataFrame()
