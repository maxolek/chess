"""
Data loading, enrichment, and filtering.
Loads data from DuckDB at startup and provides the apply_filters() function.
"""
import pandas as pd
import numpy as np
import duckdb

from .config import DB_PATH, EVAL_CLIP_CP, NUMERIC_EXCLUDE, numeric_cols


# ──────────────────────────────────────────────────────────────────────────────
# CONNECTION
# ──────────────────────────────────────────────────────────────────────────────

def get_connection(path: str):
    return duckdb.connect(path, read_only=True)


def query(con, sql: str) -> pd.DataFrame:
    return con.execute(sql).df()


def safe_query(con, sql: str, fallback: pd.DataFrame = None) -> pd.DataFrame:
    try:
        return query(con, sql)
    except Exception as e:
        print(f"[WARN] Query failed: {e}\n  SQL: {sql[:120]}")
        return fallback if fallback is not None else pd.DataFrame()


# ──────────────────────────────────────────────────────────────────────────────
# LOAD TABLES
# ──────────────────────────────────────────────────────────────────────────────

con = get_connection(DB_PATH)

engines_df      = safe_query(con, "SELECT * FROM engines")
experiments_df  = safe_query(con, "SELECT * FROM experiments")
games_df        = safe_query(con, "SELECT * FROM game_stats")
searches_df     = safe_query(con, "SELECT * FROM search_stats")

# Prefer `search_features` (denormalised fact table) if present
try:
    sf_tbl = safe_query(con, "SELECT * FROM search_features")
    if not sf_tbl.empty:
        searches_df = sf_tbl
        if "final_eval" in searches_df.columns and "eval" not in searches_df.columns:
            searches_df["eval"] = searches_df["final_eval"]
        if "max_depth" in searches_df.columns and "depth" not in searches_df.columns:
            searches_df["depth"] = searches_df["max_depth"]
except Exception:
    pass

iter_df         = safe_query(con, "SELECT * FROM iterative_deepening_stats")
tree_df         = safe_query(con, "SELECT * FROM search_tree_stats")
timing_df       = safe_query(con, "SELECT * FROM search_timings")
sprt_df         = safe_query(con, "SELECT * FROM sprt_runs")
sts_df          = safe_query(con, "SELECT * FROM sts_runs")
positions_df    = safe_query(con, "SELECT * FROM dim_positions")
perft_df        = pd.DataFrame()

# Data freshness stats (computed once at load time)
DATA_STATS = {
    "n_games": len(games_df),
    "n_searches": len(searches_df),
    "n_engines": len(engines_df),
    "n_iter": len(iter_df),
}

# Try to get last ingestion timestamp
_ts_col = None
for _c in ["ingestion_timestamp_utc", "created_at", "timestamp"]:
    if _c in searches_df.columns:
        _ts_col = _c
        break
if _ts_col:
    try:
        DATA_STATS["last_updated"] = pd.to_datetime(searches_df[_ts_col]).max().strftime("%Y-%m-%d %H:%M")
    except Exception:
        DATA_STATS["last_updated"] = "unknown"
else:
    DATA_STATS["last_updated"] = "unknown"


# ──────────────────────────────────────────────────────────────────────────────
# ENRICHMENT
# ──────────────────────────────────────────────────────────────────────────────

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


def enrich_searches(searches: pd.DataFrame, engines: pd.DataFrame, games: pd.DataFrame) -> pd.DataFrame:
    """Attach engine name + side + result to every search row."""
    df = searches.copy()

    if not engines.empty and "engine_id" in df.columns:
        emap = engines.set_index("id")[["name", "version"]].rename(
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

    if not games.empty and "game_id" in df.columns:
        gcols = ["id", "white_engine_id", "black_engine_id", "result", "opening"]
        gcols = [c for c in gcols if c in games.columns]
        gmap = games[gcols].rename(columns={"id": "game_id"})
        df = df.merge(gmap, on="game_id", how="left", suffixes=("", "_game"))

    if "white_engine_id" in df.columns and "black_engine_id" in df.columns:
        df["engine_side"] = df.apply(
            lambda r: "white" if r.get("engine_id") == r.get("white_engine_id")
            else ("black" if r.get("engine_id") == r.get("black_engine_id") else "unknown"),
            axis=1
        )
    else:
        df["engine_side"] = "unknown"

    def _result(r):
        res = str(r.get("result", "")).strip()
        side = r.get("engine_side", "unknown")
        if res in ("1-0", "1", "white"):
            return "Win" if side == "white" else "Loss"
        if res in ("0-1", "2", "black"):
            return "Win" if side == "black" else "Loss"
        if res in ("1/2-1/2", "3", "½-½", "draw"):
            return "Draw"
        return None

    df["result_label"] = df.apply(_result, axis=1)
    return df


searches_df = enrich_searches(searches_df, engines_df, games_df)


def _attach_engine(detail_df: pd.DataFrame, searches: pd.DataFrame) -> pd.DataFrame:
    if detail_df.empty or searches.empty:
        return detail_df
    key = None
    if "id" in searches.columns:
        key = "id"
    elif "search_id" in searches.columns:
        key = "search_id"
    else:
        return detail_df

    wanted = ["engine_id", "engine_name", "game_id", "fen", "ply"]
    cols = [c for c in ([key] + wanted) if c in searches.columns]
    right = searches[cols].copy()
    if key == "id":
        right = right.rename(columns={"id": "search_id"})
    return detail_df.merge(right, on="search_id", how="left")


iter_df = _attach_engine(iter_df, searches_df)
tree_df = _attach_engine(tree_df, searches_df)


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
# DROPDOWN OPTIONS
# ──────────────────────────────────────────────────────────────────────────────

def _opts(series: pd.Series) -> list[dict]:
    return [{"label": str(v), "value": v} for v in sorted(series.dropna().unique())]


engine_options = (
    [{"label": f"{r['name']} ({r['version']})", "value": r["id"]}
     for _, r in engines_df[["id", "name", "version"]].drop_duplicates().iterrows()]
    if not engines_df.empty else []
)
result_options   = _opts(searches_df["result_label"])  if "result_label"  in searches_df else []
opening_options  = _opts(games_df["opening"]) if "opening" in games_df else []
side_options     = _opts(searches_df["engine_side"])   if "engine_side"   in searches_df else []
pos_type_options = _opts(searches_df["pos_label"])     if "pos_label"     in searches_df else []
game_phase_options = _opts(searches_df["game_phase"]) if "game_phase" in searches_df.columns else []

_search_nums = numeric_cols(searches_df)
_iter_nums   = numeric_cols(iter_df) if not iter_df.empty else []
_tree_nums   = numeric_cols(tree_df) if not tree_df.empty else []


# ──────────────────────────────────────────────────────────────────────────────
# FILTER FUNCTION
# ──────────────────────────────────────────────────────────────────────────────

def apply_filters(
    engine_ids: list | None,
    result_vals: list | None,
    opening_vals: list | None,
    side_vals: list | None,
    pos_type_vals: list | None,
    game_phase_vals: list | None = None,
    include_mates: bool = False,
    mates_only: bool = False,
) -> tuple[pd.DataFrame, pd.DataFrame]:
    gf = games_df.copy()
    sf = searches_df.copy()

    if "depth" in sf.columns:
        sf = sf[sf["depth"].between(0, 60)]

    if "eval" in sf.columns:
        if mates_only:
            sf = sf[sf["eval"].abs().gt(EVAL_CLIP_CP)]
        elif not include_mates:
            sf = sf[sf["eval"].abs().le(EVAL_CLIP_CP)]

    if engine_ids:
        if "white_engine_id" in gf.columns:
            gf = gf[gf["white_engine_id"].isin(engine_ids) | gf["black_engine_id"].isin(engine_ids)]
        if "engine_id" in sf.columns:
            sf = sf[sf["engine_id"].isin(engine_ids)]

    if result_vals and "result_label" in sf.columns:
        sf = sf[sf["result_label"].isin(result_vals)]

    if opening_vals and "opening" in gf.columns:
        gf = gf[gf["opening"].isin(opening_vals)]

    if side_vals and "engine_side" in sf.columns:
        sf = sf[sf["engine_side"].isin(side_vals)]

    if pos_type_vals and "pos_label" in sf.columns:
        sf = sf[sf["pos_label"].isin(pos_type_vals)]

    if game_phase_vals and "game_phase" in sf.columns:
        sf = sf[sf["game_phase"].isin(game_phase_vals)]

    if "game_id" in sf.columns and "id" in gf.columns:
        sf = sf[sf["game_id"].isin(gf["id"].unique())]

    return gf, sf
