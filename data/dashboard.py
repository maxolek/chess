"""
Chess Engine Analytics Dashboard
==================================
A full-featured Dash application for analyzing and comparing chess engine versions.
Connects to a DuckDB (analytics) or SQLite (raw) database.

Tables used (DuckDB analytics names):
    engines, experiments, game_stats, search_stats,
    iterative_deepening_stats, search_tree_stats,
    search_timings, sprt_runs, sts_runs, dim_positions
    (perft not yet present in analytics DB)

Run:
    python chess_dashboard.py
"""

import platform
import webbrowser
from pathlib import Path

import dash
from dash import Input, Output, State, ctx, dash_table, dcc, html
import numpy as np
import pandas as pd
import plotly.express as px
import plotly.graph_objects as go
from plotly.subplots import make_subplots
import duckdb
#import sqlite3


def get_connection(path: str):
    """Return a DuckDB or SQLite connection depending on availability."""
    return duckdb.connect(path, read_only=True)


def query(con, sql: str) -> pd.DataFrame:
    return con.execute(sql).df()


# Resolve DB path per OS
_system = platform.system()
if _system == "Windows":
    DB_PATH = "F:/databases/chess_analytics.duckdb"
else:
    # Try DuckDB first, then SQLite fallback
    _base = Path.home() / "Documents/databases"
    DB_PATH = str(_base / "chess_analytics.duckdb") if (_base / "chess_analytics.duckdb").exists() else str(_base / "chess.db")

con = get_connection(DB_PATH)


# ──────────────────────────────────────────────────────────────────────────────
# 2. DATA LOADING
# ──────────────────────────────────────────────────────────────────────────────

def safe_query(con, sql: str, fallback: pd.DataFrame = None) -> pd.DataFrame:
    try:
        return query(con, sql)
    except Exception as e:
        print(f"[WARN] Query failed: {e}\n  SQL: {sql[:120]}")
        return fallback if fallback is not None else pd.DataFrame()


engines_df      = safe_query(con, "SELECT * FROM engines")
experiments_df  = safe_query(con, "SELECT * FROM experiments")
games_df        = safe_query(con, "SELECT * FROM game_stats")
searches_df     = safe_query(con, "SELECT * FROM search_stats")
# Prefer `search_features` (denormalised fact table) if present — dashboard should be
# built around that table which already joins search_stats, iterative_depth, times
# and position_features. Fall back to `search_stats` when `search_features` is missing.
try:
    sf_tbl = safe_query(con, "SELECT * FROM search_features")
    if not sf_tbl.empty:
        searches_df = sf_tbl
        # Normalise common column names so existing dashboard code (which expects
        # `eval` and `depth`) continues to work when using `search_features`.
        if "final_eval" in searches_df.columns and "eval" not in searches_df.columns:
            searches_df["eval"] = searches_df["final_eval"]
        if "max_depth" in searches_df.columns and "depth" not in searches_df.columns:
            searches_df["depth"] = searches_df["max_depth"]
        #print("[INFO] Dashboard using `search_features` (preferred) instead of `search_stats`")
except Exception:
    pass
iter_df         = safe_query(con, "SELECT * FROM iterative_deepening_stats")
tree_df         = safe_query(con, "SELECT * FROM search_tree_stats")
timing_df       = safe_query(con, "SELECT * FROM search_timings")
sprt_df         = safe_query(con, "SELECT * FROM sprt_runs")
sts_df          = safe_query(con, "SELECT * FROM sts_runs")
positions_df    = safe_query(con, "SELECT * FROM dim_positions")
perft_df        = pd.DataFrame()   # not yet in analytics DB

# Map engine IDs to names for nicer labels
if not engines_df.empty:
    _ename = engines_df.set_index('id')['name']
    if 'white_engine_id' in games_df.columns:
        games_df['white_name'] = games_df['white_engine_id'].map(_ename)
    if 'black_engine_id' in games_df.columns:
        games_df['black_name'] = games_df['black_engine_id'].map(_ename)
    # human-friendly game label
    if 'white_name' in games_df.columns and 'black_name' in games_df.columns:
        games_df['game_label'] = games_df.apply(
            lambda r: f"{r.get('white_name','?')} vs {r.get('black_name','?')} ({r.get('result','')})",
            axis=1
        )



# ──────────────────────────────────────────────────────────────────────────────
# 3. DATA ENRICHMENT
# ──────────────────────────────────────────────────────────────────────────────

def enrich_searches(searches: pd.DataFrame, engines: pd.DataFrame, games: pd.DataFrame) -> pd.DataFrame:
    """Attach engine name + side + result to every search row."""
    df = searches.copy()

    # Engine name
    if not engines.empty and "engine_id" in df.columns:
        emap = engines.set_index("id")[["name", "version"]].rename(
            columns={"name": "engine_name", "version": "engine_version"}
        )
        df = df.join(emap, on="engine_id")
    # Combined label used in comparisons: `Name (version)` when available
    if "engine_name" in df.columns:
        try:
            df["engine_version"] = df.get("engine_version", "").fillna("")
            df["engine_label"] = df["engine_name"].astype(str) + (
                df["engine_version"].astype(str).apply(lambda v: f" ({v})" if v and v != 'None' else "")
            )
        except Exception:
            df["engine_label"] = df["engine_name"].astype(str)

    # Game-level info
    if not games.empty and "game_id" in df.columns:
        gcols = ["id", "white_engine_id", "black_engine_id", "result", "opening"]
        gcols = [c for c in gcols if c in games.columns]
        gmap = games[gcols].rename(columns={"id": "game_id"})
        df = df.merge(gmap, on="game_id", how="left", suffixes=("", "_game"))

    # Engine side (white / black)
    if "white_engine_id" in df.columns and "black_engine_id" in df.columns:
        df["engine_side"] = df.apply(
            lambda r: "white" if r.get("engine_id") == r.get("white_engine_id")
            else ("black" if r.get("engine_id") == r.get("black_engine_id") else "unknown"),
            axis=1
        )
    else:
        df["engine_side"] = "unknown"

    # Result from side perspective
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

# Attach engine name to iteration/tree tables
def _attach_engine(detail_df: pd.DataFrame, searches: pd.DataFrame) -> pd.DataFrame:
    if detail_df.empty or searches.empty:
        return detail_df
    # searches may be either `search_stats` (has `id`) or `search_features` (has `search_id`).
    # Normalize to a right-hand dataframe that contains `search_id` for the merge.
    possible = ["id", "search_id"]
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

# Attach engine names to sprt / sts / perft via experiments
def _attach_experiment(df: pd.DataFrame, experiments: pd.DataFrame, engines: pd.DataFrame) -> pd.DataFrame:
    if df.empty or experiments.empty:
        return df
    ename = engines.set_index("id")["name"] if not engines.empty else pd.Series(dtype=str)
    exp = experiments.copy()
    exp["engine_name"] = exp["engine_id"].map(ename)
    merged = df.merge(exp[["id", "engine_id", "engine_name", "type"]].rename(columns={"id": "experiment_id"}),
                      on="experiment_id", how="left")
    return merged

sprt_df  = _attach_experiment(sprt_df, experiments_df, engines_df)
sts_df   = _attach_experiment(sts_df,  experiments_df, engines_df)
perft_df = _attach_experiment(perft_df, experiments_df, engines_df)

# Build human-friendly labels for SPRT runs: baseline vs candidate engine names
if not sprt_df.empty and not engines_df.empty:
    ename = engines_df.set_index('id')['name']
    if 'baseline_engine_id' in sprt_df.columns:
        sprt_df['baseline_engine_name'] = sprt_df['baseline_engine_id'].map(ename)
    if 'candidate_engine_id' in sprt_df.columns:
        sprt_df['candidate_engine_name'] = sprt_df['candidate_engine_id'].map(ename)
    if 'baseline_engine_name' in sprt_df.columns and 'candidate_engine_name' in sprt_df.columns:
        sprt_df['sprt_label'] = sprt_df.apply(
            lambda r: f"{r.get('candidate_engine_name','?')} vs {r.get('baseline_engine_name','?')}",
            axis=1
        )

# ──────────────────────────────────────────────────────────────────────────────
# 4. FILTER HELPERS
# ──────────────────────────────────────────────────────────────────────────────

NUMERIC_EXCLUDE = {
    "id", "engine_id", "game_id", "sts_id", "search_id",
    "white_engine_id", "black_engine_id", "experiment_id",
    "baseline_engine_id", "candidate_engine_id",
}

# ── Metric display labels and logical ordering ────────────────────────────────
# Groups: Core → Nodes → Pruning → TT → Move Ordering → Stability → Timing → Ratios (agg)
METRIC_LABELS: dict[str, str] = {
    # ── Core ──
    "depth": "Depth",
    "max_depth": "Max Depth",
    "qdepth": "QSearch Depth",
    "max_qdepth": "Max QSearch Depth",
    "eval": "Eval (cp)",
    "final_eval": "Final Eval (cp)",
    "sf_eval": "Stockfish Eval",
    "eval_diff": "Eval Diff (vs SF)",
    "engine_move_rank": "Move Rank (vs SF)",
    "best_move_match": "Best Move Match",
    "ply": "Game Ply",
    "time_ms": "Time (ms)",
    "total_time_ms": "Total Time (ms)",
    "running_time_ms": "Running Time (ms)",
    "total_search_time": "Search Duration (ms)",
    # ── Nodes ──
    "nodes": "Nodes",
    "qnodes": "QSearch Nodes",
    "total_nodes": "Total Nodes",
    "total_internal_nodes": "Internal Nodes (total)",
    "total_qnodes": "QSearch Nodes (total)",
    "nps": "Nodes/sec (NPS)",
    "avg_nps": "Avg NPS",
    "stddev_nps": "NPS Std Dev",
    "peak_nps": "Peak NPS",
    "worst_nps": "Worst NPS",
    "final_nps": "Final NPS",
    "qratio": "QNode Ratio",
    "avg_qratio": "Avg QNode Ratio",
    "max_qratio": "Max QNode Ratio",
    "stddev_qratio": "QNode Ratio Std Dev",
    # ── Branching Factor ──
    "ebf": "Eff. Branching Factor",
    "avg_ebf": "Avg EBF",
    "max_ebf": "Max EBF",
    "geo_mean_ebf": "Geometric Mean EBF",
    "qebf": "QSearch EBF",
    "avg_qebf": "Avg QSearch EBF",
    "max_qebf": "Max QSearch EBF",
    "geo_mean_qebf": "Geometric Mean QEBF",
    "time_increase_ratio": "Time Growth Ratio",
    # ── Pruning ──
    "see_prunes": "SEE Prunes",
    "total_see_prunes": "SEE Prunes (total)",
    "see_prune_ratio": "SEE Prune Rate",
    "avg_see_prune_ratio": "Avg SEE Prune Rate",
    "delta_prunes": "Delta Prunes",
    "total_delta_prunes": "Delta Prunes (total)",
    "delta_prune_ratio": "Delta Prune Rate",
    "avg_delta_prune_ratio": "Avg Delta Prune Rate",
    "prune_ratio": "Combined Prune Rate",
    "avg_prune_ratio": "Avg Combined Prune Rate",
    # ── Null Move Pruning ──
    "nmp": "NMP Attempts",
    "total_nmp": "NMP Attempts (total)",
    "nmp_fail": "NMP Failures",
    "total_nmp_fail": "NMP Failures (total)",
    "nmp_ratio": "NMP Rate",
    "avg_nmp_ratio": "Avg NMP Rate",
    "max_nmp_ratio": "Max NMP Rate",
    "nmp_fail_ratio": "NMP Fail Rate",
    "avg_nmp_fail_ratio": "Avg NMP Fail Rate",
    # ── PVS ──
    "pvs_researches": "PVS Re-searches",
    "pvs_research_ratio": "PVS Re-search Rate",
    "avg_pvs_research_ratio": "Avg PVS Re-search Rate",
    "max_pvs_research_ratio": "Max PVS Re-search Rate",
    # ── Transposition Table ──
    "tt_stores": "TT Stores",
    "total_tt_stores": "TT Stores (total)",
    "tt_hits": "TT Hits",
    "total_tt_hits": "TT Hits (total)",
    "tt_overwritten": "TT Overwrites",
    "total_tt_overwritten": "TT Overwrites (total)",
    "tt_hit_ratio": "TT Hit Rate",
    "avg_tt_hit_ratio": "Avg TT Hit Rate",
    "max_tt_hit_ratio": "Max TT Hit Rate",
    "stddev_tt_hit_ratio": "TT Hit Rate Std Dev",
    "tt_store_ratio": "TT Store Rate",
    "avg_tt_store_ratio": "Avg TT Store Rate",
    "max_tt_store_ratio": "Max TT Store Rate",
    "stddev_tt_store_ratio": "TT Store Rate Std Dev",
    # ── Move Ordering (fail high/low) ──
    "fail_highs": "Fail Highs",
    "total_fail_highs": "Fail Highs (total)",
    "fail_lows": "Fail Lows",
    "total_fail_low": "Fail Lows (total)",
    "fail_high_first": "Fail High 1st Move",
    "total_fail_high_first": "Fail High 1st (total)",
    "fail_high_late": "Fail High Late",
    "total_fail_high_late": "Fail High Late (total)",
    "fail_high_ratio": "Fail High Rate",
    "avg_fail_high_ratio": "Avg Fail High Rate",
    "max_fail_high_ratio": "Max Fail High Rate",
    "fail_low_ratio": "Fail Low Rate",
    "avg_fail_low_ratio": "Avg Fail Low Rate",
    "max_fail_low_ratio": "Max Fail Low Rate",
    "fail_high_first_ratio": "Fail High 1st Rate",
    "avg_fail_high_first_ratio": "Avg Fail High 1st Rate",
    "fail_high_late_ratio": "Fail High Late Rate",
    "avg_fail_high_late_ratio": "Avg Fail High Late Rate",
    "fail_high_researches": "Aspiration Fail-High Re-searches",
    "total_fail_high_researches": "Aspiration Fail-High (total)",
    "fail_low_researches": "Aspiration Fail-Low Re-searches",
    "total_fail_low_researches": "Aspiration Fail-Low (total)",
    "max_fail_high_researches": "Max Fail-High Re-searches",
    "max_fail_low_researches": "Max Fail-Low Re-searches",
    "fail_high_researches_per_depth": "Fail-High Re-searches/Depth",
    "fail_low_researches_per_depth": "Fail-Low Re-searches/Depth",
    # ── Eval Stability ──
    "prior_eval_delta": "Eval Delta (prev iter)",
    "first_eval_delta": "Eval Delta (from 1st iter)",
    "eval_sign_flips": "Eval Sign Flips",
    "eval_sign_flips_per_depth": "Eval Flips / Depth",
    "stddev_eval": "Eval Std Dev",
    "stddev_last5_eval": "Eval Std Dev (last 5)",
    "max_eval": "Max Eval",
    "avg_eval": "Avg Eval",
    "move_stability": "Move Stability",
    "max_move_stability": "Max Move Stability",
    "final_move_stability": "Final Move Stability",
    # ── Timing Breakdown ──
    "make_move_avg_ms": "MakeMove Avg (ms)",
    "make_move_perc_total_time": "MakeMove % Time",
    "make_move_total_ms": "MakeMove Total (ms)",
    "unmake_move_avg_ms": "UnmakeMove Avg (ms)",
    "unmake_move_perc_total_time": "UnmakeMove % Time",
    "unmake_move_total_ms": "UnmakeMove Total (ms)",
    "movegen_avg_ms": "MoveGen Avg (ms)",
    "movegen_perc_total_time": "MoveGen % Time",
    "movegen_total_ms": "MoveGen Total (ms)",
    "move_order_avg_ms": "Move Order Avg (ms)",
    "move_order_perc_total_time": "Move Order % Time",
    "move_order_total_ms": "Move Order Total (ms)",
    "nnue_avg_ms": "NNUE Avg (ms)",
    "nnue_perc_total_time": "NNUE % Time",
    "nnue_total_ms": "NNUE Total (ms)",
    "static_eval_avg_ms": "Static Eval Avg (ms)",
    "static_eval_perc_total_time": "Static Eval % Time",
    "static_eval_total_ms": "Static Eval Total (ms)",
    "see_avg_ms": "SEE Avg (ms)",
    "see_perc_total_time": "SEE % Time",
    "see_total_ms": "SEE Total (ms)",
    "tt_probe_avg_ms": "TT Probe Avg (ms)",
    "tt_probe_perc_total_time": "TT Probe % Time",
    "tt_probe_total_ms": "TT Probe Total (ms)",
    "tt_store_avg_ms": "TT Store Avg (ms)",
    "tt_store_perc_total_time": "TT Store % Time",
    "tt_store_total_ms": "TT Store Total (ms)",
}

# Canonical ordering by group; columns not listed here go to the end alphabetically.
_METRIC_ORDER = [
    # Core
    "depth", "max_depth", "qdepth", "max_qdepth", "eval", "final_eval",
    "sf_eval", "eval_diff", "engine_move_rank", "best_move_match", "ply",
    "time_ms", "total_time_ms", "running_time_ms", "total_search_time",
    # Nodes
    "nodes", "qnodes", "total_nodes", "total_internal_nodes", "total_qnodes",
    "nps", "avg_nps", "stddev_nps", "peak_nps", "worst_nps", "final_nps",
    "qratio", "avg_qratio", "max_qratio", "stddev_qratio",
    # Branching
    "ebf", "avg_ebf", "max_ebf", "geo_mean_ebf",
    "qebf", "avg_qebf", "max_qebf", "geo_mean_qebf", "time_increase_ratio",
    # Pruning
    "see_prunes", "total_see_prunes", "see_prune_ratio", "avg_see_prune_ratio",
    "delta_prunes", "total_delta_prunes", "delta_prune_ratio", "avg_delta_prune_ratio",
    "prune_ratio", "avg_prune_ratio",
    # NMP
    "nmp", "total_nmp", "nmp_fail", "total_nmp_fail",
    "nmp_ratio", "avg_nmp_ratio", "max_nmp_ratio",
    "nmp_fail_ratio", "avg_nmp_fail_ratio",
    # PVS
    "pvs_researches", "pvs_research_ratio", "avg_pvs_research_ratio", "max_pvs_research_ratio",
    # TT
    "tt_stores", "total_tt_stores", "tt_hits", "total_tt_hits",
    "tt_overwritten", "total_tt_overwritten",
    "tt_hit_ratio", "avg_tt_hit_ratio", "max_tt_hit_ratio", "stddev_tt_hit_ratio",
    "tt_store_ratio", "avg_tt_store_ratio", "max_tt_store_ratio", "stddev_tt_store_ratio",
    # Move Ordering
    "fail_highs", "total_fail_highs", "fail_lows", "total_fail_low",
    "fail_high_first", "total_fail_high_first", "fail_high_late", "total_fail_high_late",
    "fail_high_ratio", "avg_fail_high_ratio", "max_fail_high_ratio",
    "fail_low_ratio", "avg_fail_low_ratio", "max_fail_low_ratio",
    "fail_high_first_ratio", "avg_fail_high_first_ratio",
    "fail_high_late_ratio", "avg_fail_high_late_ratio",
    "fail_high_researches", "total_fail_high_researches",
    "fail_low_researches", "total_fail_low_researches",
    "max_fail_high_researches", "max_fail_low_researches",
    "fail_high_researches_per_depth", "fail_low_researches_per_depth",
    # Stability
    "prior_eval_delta", "first_eval_delta", "eval_sign_flips", "eval_sign_flips_per_depth",
    "stddev_eval", "stddev_last5_eval", "max_eval", "avg_eval",
    "move_stability", "max_move_stability", "final_move_stability",
    # Timing
    "make_move_avg_ms", "make_move_perc_total_time", "make_move_total_ms",
    "unmake_move_avg_ms", "unmake_move_perc_total_time", "unmake_move_total_ms",
    "movegen_avg_ms", "movegen_perc_total_time", "movegen_total_ms",
    "move_order_avg_ms", "move_order_perc_total_time", "move_order_total_ms",
    "nnue_avg_ms", "nnue_perc_total_time", "nnue_total_ms",
    "static_eval_avg_ms", "static_eval_perc_total_time", "static_eval_total_ms",
    "see_avg_ms", "see_perc_total_time", "see_total_ms",
    "tt_probe_avg_ms", "tt_probe_perc_total_time", "tt_probe_total_ms",
    "tt_store_avg_ms", "tt_store_perc_total_time", "tt_store_total_ms",
]
_METRIC_RANK = {col: i for i, col in enumerate(_METRIC_ORDER)}


def numeric_cols(df: pd.DataFrame) -> list[str]:
    """Return numeric column names sorted by canonical group order."""
    cols = [c for c in df.select_dtypes(include=np.number).columns if c not in NUMERIC_EXCLUDE]
    cols.sort(key=lambda c: (_METRIC_RANK.get(c, 9999), c))
    return cols


def metric_label(col: str) -> str:
    """Human-readable label for a metric column."""
    return METRIC_LABELS.get(col, col.replace("_", " ").title())


def metric_options(cols: list[str]) -> list[dict]:
    """Build dropdown options with human-readable labels, preserving canonical order."""
    return [{"label": metric_label(c), "value": c} for c in cols]


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

    # Sanity limits — always exclude clearly broken data (depth > 60)
    if "depth" in sf.columns:
        sf = sf[sf["depth"].between(0, 60)]

    # Checkmate filtering: exclude (default), include all, or mates only
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

    # Keep searches consistent with filtered games
    if "game_id" in sf.columns and "id" in gf.columns:
        sf = sf[sf["game_id"].isin(gf["id"].unique())]

    return gf, sf


# ──────────────────────────────────────────────────────────────────────────────
# 5. DROPDOWN OPTIONS
# ──────────────────────────────────────────────────────────────────────────────

def _opts(series: pd.Series) -> list[dict]:
    return [{"label": str(v), "value": v} for v in sorted(series.dropna().unique())]

engine_options  = (
    [{"label": f"{r['name']} ({r['version']})", "value": r["id"]}
     for _, r in engines_df[["id", "name", "version"]].drop_duplicates().iterrows()]
    if not engines_df.empty else []
)
result_options   = _opts(searches_df["result_label"])  if "result_label"  in searches_df else []
# Opening options: must be provided by ETL via `game_stats.opening`.
opening_options = _opts(games_df["opening"]) if "opening" in games_df else []
side_options     = _opts(searches_df["engine_side"])   if "engine_side"   in searches_df else []
pos_type_options = _opts(searches_df["pos_label"])     if "pos_label"     in searches_df else []
# Search numeric columns for axis dropdowns
_search_nums = numeric_cols(searches_df)
_iter_nums   = numeric_cols(iter_df) if not iter_df.empty else []
_tree_nums   = numeric_cols(tree_df) if not tree_df.empty else []
game_phase_options = _opts(searches_df["game_phase"]) if "game_phase" in searches_df.columns else []

MAX_TABLE_ROWS = 5000

# Eval clipping: checkmate scores (~±10000) skew charts meant for normal evals (±1000).
# We clip eval to ±EVAL_CLIP_CP for all visualizations. Rows are NOT excluded — just clamped.
EVAL_CLIP_CP = 1500


# ──────────────────────────────────────────────────────────────────────────────
# 6. COLOUR PALETTE (deterministic per engine)
# ──────────────────────────────────────────────────────────────────────────────

_PALETTE = [
    "#00d2ff", "#ff6b35", "#7fff6b", "#ff3cac", "#f7b731",
    "#a29bfe", "#fd79a8", "#55efc4", "#fdcb6e", "#e17055",
]

def engine_colour_map(names) -> dict:
    return {n: _PALETTE[i % len(_PALETTE)] for i, n in enumerate(sorted(set(names)))}


def hex_to_rgba(hex_color: str, alpha: float = 0.4) -> str:
    """Convert a #RRGGBB hex color to an rgba(...) string with given alpha."""
    if not hex_color or not hex_color.startswith('#'):
        return hex_color
    h = hex_color.lstrip('#')
    if len(h) == 6:
        r = int(h[0:2], 16)
        g = int(h[2:4], 16)
        b = int(h[4:6], 16)
        return f'rgba({r},{g},{b},{alpha})'
    # fallback: return original
    return hex_color


# ──────────────────────────────────────────────────────────────────────────────
# 7. LAYOUT
# ──────────────────────────────────────────────────────────────────────────────

DARK_BG   = "#0d0f14"
PANEL_BG  = "#141720"
BORDER    = "#252a38"
TEXT_PRI  = "#e8eaf0"
TEXT_SEC  = "#8892a4"
ACCENT    = "#00d2ff"
ACCENT2   = "#ff6b35"

_sidebar_label = {
    "fontSize": "9px",
    "fontWeight": "700",
    "letterSpacing": "0.14em",
    "textTransform": "uppercase",
    "color": TEXT_SEC,
    "marginBottom": "3px",
    "marginTop": "10px",
}

_dropdown_style = {
    "backgroundColor": "#1a1f2e",
    "borderColor": BORDER,
    "color": TEXT_PRI,
    "fontSize": "12px",
    "borderRadius": "6px",
}

app = dash.Dash(__name__, suppress_callback_exceptions=True)
app.title = "Chess Engine Analytics"

app.index_string = """
<!DOCTYPE html>
<html>
<head>
    {%metas%}
    <title>{%title%}</title>
    {%favicon%}
    {%css%}
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link href="https://fonts.googleapis.com/css2?family=JetBrains+Mono:wght@400;600;700&family=Syne:wght@400;600;800&display=swap" rel="stylesheet">
    <style>
        *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            background: #0d0f14;
            color: #e8eaf0;
            font-family: 'Syne', sans-serif;
            min-height: 100vh;
            -webkit-font-smoothing: antialiased;
        }
        ::-webkit-scrollbar { width: 5px; height: 5px; }
        ::-webkit-scrollbar-track { background: transparent; }
        ::-webkit-scrollbar-thumb { background: #252a3866; border-radius: 4px; }
        ::-webkit-scrollbar-thumb:hover { background: #00d2ff55; }

        /* Dropdown overrides */
        .Select-control { background-color: #1a1f2e !important; border-color: #252a38 !important; color: #e8eaf0 !important; border-radius: 6px !important; min-height: 32px !important; }
        .Select-menu-outer { background-color: #1a1f2e !important; border-color: #252a38 !important; border-radius: 0 0 6px 6px !important; }
        .Select-option { background-color: #1a1f2e !important; color: #e8eaf0 !important; padding: 6px 10px !important; }
        .Select-option:hover, .Select-option.is-focused { background-color: #00d2ff18 !important; }
        .Select-value-label { color: #e8eaf0 !important; }
        .Select-placeholder { color: #8892a4 !important; font-size: 12px !important; }
        .Select-arrow { border-top-color: #8892a4 !important; }
        .VirtualizedSelectOption { background-color: #1a1f2e !important; color: #e8eaf0 !important; }
        .Select-multi-value-wrapper .Select-value { background-color: #00d2ff18 !important; border-color: #00d2ff44 !important; border-radius: 4px !important; }
        .Select-multi-value-wrapper .Select-value-label { color: #00d2ff !important; font-size: 11px !important; }

        /* Tab animation */
        .tab-content { animation: fadeIn 0.18s ease-out; }
        @keyframes fadeIn { from { opacity: 0; transform: translateY(6px); } to { opacity: 1; transform: translateY(0); } }

        /* Metric cards */
        .metric-card {
            background: linear-gradient(135deg, #141720 0%, #181d2a 100%);
            border: 1px solid #252a38;
            border-radius: 10px;
            padding: 18px 20px 14px;
            position: relative;
            overflow: hidden;
            transition: border-color 0.2s, box-shadow 0.2s, transform 0.15s;
        }
        .metric-card:hover {
            border-color: #00d2ff44;
            box-shadow: 0 4px 20px rgba(0, 210, 255, 0.06);
            transform: translateY(-1px);
        }
        .metric-card::before {
            content: '';
            position: absolute;
            top: 0; left: 0; right: 0;
            height: 2px;
            background: linear-gradient(90deg, #00d2ff, #ff6b35);
            opacity: 0.7;
        }
        .metric-val {
            font-family: 'JetBrains Mono', monospace;
            font-size: 24px;
            font-weight: 700;
            color: #00d2ff;
            line-height: 1.2;
        }
        .metric-lbl {
            font-size: 9px;
            font-weight: 700;
            letter-spacing: 0.12em;
            text-transform: uppercase;
            color: #8892a4;
            margin-top: 6px;
        }

        /* Section titles */
        .section-title {
            font-family: 'JetBrains Mono', monospace;
            font-size: 10px;
            font-weight: 700;
            letter-spacing: 0.18em;
            text-transform: uppercase;
            color: #00d2ff;
            border-bottom: 1px solid #252a38;
            padding-bottom: 6px;
            margin-bottom: 12px;
        }

        /* Panel cards */
        .panel {
            background: #141720;
            border: 1px solid #252a38;
            border-radius: 10px;
            padding: 16px 18px;
            transition: border-color 0.2s, box-shadow 0.2s;
        }
        .panel:hover {
            border-color: #252a3888;
            box-shadow: 0 2px 16px rgba(0, 0, 0, 0.25);
        }

        /* Data tables */
        .dash-table-container .dash-spreadsheet-container .dash-spreadsheet-inner td,
        .dash-table-container .dash-spreadsheet-container .dash-spreadsheet-inner th {
            background-color: #141720 !important;
            color: #e8eaf0 !important;
            border-color: #1e2333 !important;
            font-family: 'JetBrains Mono', monospace !important;
            font-size: 11px !important;
            padding: 5px 8px !important;
        }
        .dash-table-container .dash-spreadsheet-container .dash-spreadsheet-inner th {
            background-color: #0f1118 !important;
            color: #6b7a8d !important;
            font-weight: 700 !important;
            font-size: 10px !important;
            letter-spacing: 0.08em !important;
            text-transform: uppercase !important;
        }
        .dash-table-container .dash-spreadsheet-container .dash-spreadsheet-inner tr:hover td {
            background-color: #00d2ff08 !important;
        }
        .dash-table-container .previous-next-container button,
        .dash-table-container .previous-page, .dash-table-container .next-page,
        .dash-table-container .first-page, .dash-table-container .last-page {
            background-color: #1a1f2e !important;
            color: #e8eaf0 !important;
            border-color: #252a38 !important;
            border-radius: 4px !important;
            font-size: 11px !important;
        }
        input[type="text"] {
            background-color: #1a1f2e !important;
            color: #e8eaf0 !important;
            border-color: #252a38 !important;
            border-radius: 4px !important;
        }

        .rc-slider-track { background-color: #00d2ff !important; }
        .rc-slider-handle { border-color: #00d2ff !important; background-color: #00d2ff !important; }

        /* Plotly modebar */
        .modebar { opacity: 0; transition: opacity 0.2s; }
        .js-plotly-plot:hover .modebar { opacity: 1; }
        .modebar-btn path { fill: #8892a4 !important; }
        .modebar-btn:hover path { fill: #00d2ff !important; }

        /* Tabs */
        .tab--selected { position: relative; }
    </style>
</head>
<body>
    {%app_entry%}
    <footer>
        {%config%}
        {%scripts%}
        {%renderer%}
    </footer>
</body>
</html>
"""

def make_sidebar():
    return html.Div([
        html.Div([
            html.Div("⬡", style={"fontSize": "20px", "color": ACCENT, "lineHeight": "1"}),
            html.Div("CHESS", style={"fontFamily": "'JetBrains Mono'", "fontSize": "10px", "fontWeight": "700", "letterSpacing": "0.22em", "color": TEXT_PRI, "marginTop": "2px"}),
            html.Div("ANALYTICS", style={"fontFamily": "'JetBrains Mono'", "fontSize": "8px", "fontWeight": "400", "letterSpacing": "0.28em", "color": TEXT_SEC}),
        ], style={"padding": "16px 14px 12px", "borderBottom": f"1px solid {BORDER}"}),

        html.Div([
            html.Div("FILTERS", style={**_sidebar_label, "marginTop": "14px", "color": ACCENT, "fontSize": "8px", "letterSpacing": "0.2em"}),

            html.Div("Engine", style=_sidebar_label),
            dcc.Dropdown(
                id="filter-engine", options=engine_options, multi=True,
                placeholder="All engines",
                style=_dropdown_style,
            ),

            html.Div("Side", style=_sidebar_label),
            dcc.Dropdown(
                id="filter-side", options=side_options, multi=True,
                placeholder="Both sides",
                style=_dropdown_style,
            ),

            html.Div("Result", style=_sidebar_label),
            dcc.Dropdown(
                id="filter-result", options=result_options, multi=True,
                placeholder="All results",
                style=_dropdown_style,
            ),

            html.Div("Opening", style=_sidebar_label),
            dcc.Dropdown(
                id="filter-opening", options=opening_options, multi=True,
                placeholder="All openings",
                style=_dropdown_style,
            ),

            html.Div("Position Type", style=_sidebar_label),
            dcc.Dropdown(
                id="filter-pos-type", options=pos_type_options, multi=True,
                placeholder="All types",
                style=_dropdown_style,
            ),

            html.Div("Game Phase", style=_sidebar_label),
            dcc.Dropdown(
                id="filter-game-phase", options=game_phase_options, multi=True,
                placeholder="All phases",
                style=_dropdown_style,
            ),

            html.Hr(style={"borderColor": BORDER, "margin": "16px 0", "opacity": "0.5"}),

            html.Div("Checkmates", style=_sidebar_label),
            dcc.RadioItems(
                id="filter-include-mates",
                options=[
                    {"label": " Exclude", "value": "exclude"},
                    {"label": " Include", "value": "include"},
                    {"label": " Only", "value": "only"},
                ],
                value="exclude",
                style={"fontSize": "11px", "fontFamily": "'JetBrains Mono'", "color": TEXT_SEC,
                       "marginBottom": "12px"},
                labelStyle={"display": "inline-block", "marginRight": "10px"},
            ),

            html.Button(
                "↺  RESET", id="btn-reset", n_clicks=0,
                style={
                    "width": "100%", "padding": "8px",
                    "background": "transparent",
                    "border": f"1px solid {BORDER}",
                    "borderRadius": "6px",
                    "color": TEXT_SEC,
                    "fontFamily": "'JetBrains Mono'",
                    "fontSize": "10px",
                    "fontWeight": "700",
                    "letterSpacing": "0.14em",
                    "cursor": "pointer",
                    "transition": "all 0.2s",
                }
            ),
        ], style={"padding": "0 14px 16px", "overflowY": "auto", "flex": "1"}),
    ], style={
        "width": "200px",
        "minWidth": "200px",
        "backgroundColor": PANEL_BG,
        "borderRight": f"1px solid {BORDER}",
        "display": "flex",
        "flexDirection": "column",
        "height": "100vh",
        "position": "sticky",
        "top": "0",
    })


_tab_style = {
    "fontFamily": "'JetBrains Mono', monospace",
    "fontSize": "10px",
    "fontWeight": "700",
    "letterSpacing": "0.08em",
    "padding": "8px 12px",
    "backgroundColor": "transparent",
    "color": TEXT_SEC,
    "border": "none",
    "borderBottom": f"2px solid transparent",
}
_tab_selected_style = {
    **_tab_style,
    "color": ACCENT,
    "borderBottom": f"2px solid {ACCENT}",
    "backgroundColor": "transparent",
}

TABS = [
    ("tab-overview",   "OVERVIEW"),
    ("tab-trends",     "TRENDS"),
    ("tab-games",      "GAMES"),
    ("tab-search",     "SEARCH"),
    ("tab-iter",       "ITER DEPTH"),
    ("tab-tree",       "TREE DEPTH"),
    ("tab-compare",    "COMPARE"),
    ("tab-openings",   "OPENINGS"),
    ("tab-quality",    "MOVE QUALITY"),
    ("tab-timing",     "TIMING"),
    ("tab-time-mgmt",  "TIME MGMT"),
    ("tab-tt",         "TT ANALYSIS"),
    ("tab-sprt",       "SPRT"),
    ("tab-sts",        "STS"),
    ("tab-perft",      "PERFT"),
    ("tab-positions",  "POSITIONS"),
    ("tab-corr",       "CORRELATION"),
]

app.layout = html.Div([
    dcc.Location(id='url', refresh=False),
    html.Div([
        make_sidebar(),
        html.Div([
            dcc.Tabs(
                id="main-tabs", value="tab-overview",
                children=[
                    dcc.Tab(label=lbl, value=val, style=_tab_style, selected_style=_tab_selected_style)
                    for val, lbl in TABS
                ],
                style={"borderBottom": f"1px solid {BORDER}", "backgroundColor": "#0b0d12",
                       "paddingLeft": "8px"},
            ),
            html.Div(id="tab-content", className="tab-content",
                     style={"padding": "20px 24px", "overflowY": "auto", "flex": "1"}),
        ], style={"flex": "1", "display": "flex", "flexDirection": "column", "overflow": "hidden"}),
    ], style={"display": "flex", "flexDirection": "row", "height": "100vh", "overflow": "hidden"}),
], style={"backgroundColor": DARK_BG, "minHeight": "100vh"})


# ──────────────────────────────────────────────────────────────────────────────
# 8. PLOT THEME HELPERS
# ──────────────────────────────────────────────────────────────────────────────

_PLOTLY_THEME = dict(
    template="plotly_dark",
    paper_bgcolor="rgba(0,0,0,0)",
    plot_bgcolor="rgba(20,23,32,0.6)",
    font=dict(family="JetBrains Mono, monospace", color=TEXT_PRI, size=11),
    legend=dict(bgcolor="rgba(0,0,0,0)", bordercolor="rgba(0,0,0,0)", borderwidth=0,
                font=dict(size=10), orientation="h", yanchor="bottom", y=-0.2, xanchor="center", x=0.5),
    margin=dict(l=44, r=16, t=36, b=32),
    colorway=_PALETTE,
)

def apply_theme(fig) -> go.Figure:
    fig.update_layout(**_PLOTLY_THEME)
    fig.update_xaxes(gridcolor="#1e2333", zerolinecolor="#252a38", gridwidth=1,
                     tickfont=dict(size=10), title_font=dict(size=11))
    fig.update_yaxes(gridcolor="#1e2333", zerolinecolor="#252a38", gridwidth=1,
                     tickfont=dict(size=10), title_font=dict(size=11))
    return fig


def empty_fig(msg="No data") -> go.Figure:
    fig = go.Figure()
    fig.add_annotation(text=msg, xref="paper", yref="paper",
                       x=0.5, y=0.5, showarrow=False,
                       font=dict(size=14, color=TEXT_SEC))
    return apply_theme(fig)


def section(title: str) -> html.Div:
    return html.Div(title, className="section-title")


def panel(*children, flex=None, **style_kwargs) -> html.Div:
    """Wrap children in a styled panel card."""
    style = {"flex": flex} if flex else {}
    style.update(style_kwargs)
    return html.Div(list(children), className="panel", style=style)


def metric_card(value, label: str, accent=ACCENT, sparkline=None) -> html.Div:
    """Create a KPI card. If sparkline (list of numbers) is provided, render a tiny inline SVG."""
    children = [
        html.Div(str(value), className="metric-val", style={"color": accent}),
        html.Div(label, className="metric-lbl"),
    ]
    if sparkline and len(sparkline) > 1:
        # Build a tiny SVG sparkline
        vals = [v for v in sparkline if v is not None and not (isinstance(v, float) and np.isnan(v))]
        if len(vals) > 1:
            mn, mx = min(vals), max(vals)
            rng = mx - mn if mx != mn else 1
            w, h = 80, 24
            points = []
            for i, v in enumerate(vals):
                x = i / (len(vals) - 1) * w
                y = h - ((v - mn) / rng) * h
                points.append(f"{x:.1f},{y:.1f}")
            polyline = " ".join(points)
            svg = f'''<svg width="{w}" height="{h}" viewBox="0 0 {w} {h}" xmlns="http://www.w3.org/2000/svg">
                <polyline points="{polyline}" fill="none" stroke="{accent}" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" opacity="0.7"/>
            </svg>'''
            children.append(html.Div(
                dash.dcc.Markdown(f'<div>{svg}</div>', dangerously_allow_html=True),
                style={"marginTop": "6px", "lineHeight": "0"}
            ))
    return html.Div(children, className="metric-card")


def graph(fig, height=380, **kwargs) -> dcc.Graph:
    return dcc.Graph(
        figure=fig,
        style={"height": f"{height}px", "width": "100%"},
        config={"displayModeBar": "hover", "displaylogo": False,
                "modeBarButtonsToRemove": ["lasso2d", "select2d"]},
        **kwargs
    )


def table(df: pd.DataFrame, page_size=15, max_rows=MAX_TABLE_ROWS) -> dash_table.DataTable:
    disp = df.head(max_rows).copy()
    # Sanitize non-primitive column values (lists/tuples/dicts) so Dash DataTable
    # doesn't receive nested objects which cause JS errors.
    # Coerce any non-primitive cell values to string. Dash DataTable expects
    # only primitives (string/number/boolean/None) for cell values.
    def _coerce_cell(v):
        # Preserve None/NaN
        try:
            if v is None:
                return None
            if pd.isna(v):
                return None
        except Exception:
            pass
        # Allow primitive types through
        if isinstance(v, (str, bytes, int, float, bool)):
            # bytes -> decode for nicer display
            if isinstance(v, bytes):
                try:
                    return v.decode('utf-8')
                except Exception:
                    return str(v)
            return v
        # For all other types (lists, tuples, dicts, numpy arrays, duckdb lists, etc.)
        # fall back to a compact string representation.
        try:
            s = str(v)
            # Truncate very long strings
            return s if len(s) <= 200 else s[:197] + '...'
        except Exception:
            return None

    for c in disp.columns:
        # Replace per-column assignments with a single safe applymap to avoid
        # index-alignment issues when columns contain exotic dtypes (duckdb lists, etc.).
        try:
            disp = disp.applymap(_coerce_cell)
        except Exception:
            # Last-resort: convert entire frame to strings (preserve None)
            try:
                disp = disp.applymap(lambda v: None if v is None else str(v))
            except Exception:
                # If even that fails, coerce via pandas astype
                disp = disp.astype(str)

    # Provide search_id tooltip when available
    tooltip_data = None
    if "search_id" in disp.columns:
        records = disp.to_dict("records")
        tooltip_data = [{"search_id": {"value": str(r.get("search_id", "")), "type": "text"}} for r in records]

    return dash_table.DataTable(
        columns=[{"name": c, "id": c} for c in disp.columns],
        data=disp.to_dict("records"),
        page_size=page_size,
        sort_action="native",
        filter_action="native",
        style_table={"overflowX": "auto", "maxHeight": "480px", "overflowY": "auto"},
        style_cell={"textAlign": "left", "padding": "4px 8px", "maxWidth": "200px",
                    "overflow": "hidden", "textOverflow": "ellipsis", "whiteSpace": "nowrap"},
        style_header={"fontWeight": "700", "position": "sticky", "top": 0},
        style_data_conditional=[
            {"if": {"row_index": "odd"}, "backgroundColor": "#0f1118"},
        ],
        tooltip_data=tooltip_data,
        tooltip_duration=None,
    )


# ──────────────────────────────────────────────────────────────────────────────
# 9. TAB RENDERERS
# ──────────────────────────────────────────────────────────────────────────────

def tab_overview(gf: pd.DataFrame, sf: pd.DataFrame) -> html.Div:
    # KPI row
    total_games   = len(gf)
    total_searches = len(sf)
    engine_count  = sf["engine_id"].nunique() if "engine_id" in sf else 0
    avg_depth     = round(sf["depth"].mean(), 1) if "depth" in sf and not sf.empty else "—"

    # Sparklines: metric trends across engine versions (sorted by engine_id as proxy for time)
    def _version_sparkline(col):
        if "engine_label" not in sf.columns or col not in sf.columns or sf.empty:
            return None
        grp = sf.groupby("engine_label")[col].mean()
        return grp.tolist() if len(grp) > 1 else None

    spark_depth = _version_sparkline("depth")
    spark_nps = _version_sparkline("nps")

    kpis = html.Div([
        metric_card(f"{total_games:,}",    "TOTAL GAMES"),
        metric_card(f"{total_searches:,}", "TOTAL SEARCHES", accent=ACCENT2),
        metric_card(engine_count,          "ENGINE VERSIONS", accent="#7fff6b"),
        metric_card(avg_depth,             "AVG SEARCH DEPTH", accent="#f7b731", sparkline=spark_depth),
    ], style={"display": "grid", "gridTemplateColumns": "repeat(4,1fr)", "gap": "10px", "marginBottom": "10px"})

    # Second KPI row: pruning / NMP efficiency
    avg_nmp_ratio = round(sf["nmp_ratio"].mean() * 100, 2) if "nmp_ratio" in sf.columns and not sf.empty else "—"
    avg_nmp_fail = round(sf["nmp_fail_ratio"].mean() * 100, 1) if "nmp_fail_ratio" in sf.columns and not sf.empty else "—"
    avg_fh_first = round(sf["fail_high_first_ratio"].mean() * 100, 1) if "fail_high_first_ratio" in sf.columns and not sf.empty else "—"
    avg_tt_hit = round(sf["tt_hit_ratio"].mean() * 100, 1) if "tt_hit_ratio" in sf.columns and not sf.empty else "—"

    spark_nmp = _version_sparkline("nmp_ratio")
    spark_fh = _version_sparkline("fail_high_first_ratio")
    spark_tt = _version_sparkline("tt_hit_ratio")

    kpis2 = html.Div([
        metric_card(f"{avg_nmp_ratio}%", "NMP ATTEMPT RATE", accent="#a29bfe", sparkline=spark_nmp),
        metric_card(f"{avg_nmp_fail}%", "NMP FAIL RATE", accent="#fd79a8"),
        metric_card(f"{avg_fh_first}%", "FAIL-HIGH FIRST", accent="#55efc4", sparkline=spark_fh),
        metric_card(f"{avg_tt_hit}%", "TT HIT RATE", accent="#fdcb6e", sparkline=spark_tt),
    ], style={"display": "grid", "gridTemplateColumns": "repeat(4,1fr)", "gap": "10px", "marginBottom": "14px"})

    # Win/draw/loss by engine
    if "engine_name" in sf.columns and "result_label" in sf.columns and "game_id" in sf.columns:
        per_game = (
            sf.dropna(subset=["engine_name", "result_label", "game_id"])
              .groupby(["engine_name", "game_id"], as_index=False)["result_label"]
              .first()
        )
        wdl = (
            per_game.groupby("engine_name")["result_label"]
                    .value_counts(normalize=True)
                    .mul(100).rename("pct").reset_index()
        )
        bar_fig = px.bar(
            wdl, x="engine_name", y="pct", color="result_label",
            barmode="stack", text_auto=".1f",
            color_discrete_map={"Win": "#7fff6b", "Draw": "#f7b731", "Loss": "#ff6b35"},
            labels={"pct": "% of games", "engine_name": "Engine", "result_label": "Result"},
        )
        bar_fig.update_layout(title="Win / Draw / Loss by Engine")
        apply_theme(bar_fig)
    else:
        bar_fig = empty_fig("No result data available")

    # Eval distribution by engine
    if "eval" in sf.columns and "engine_name" in sf.columns:
        ev_fig = go.Figure()
        cmap = engine_colour_map(sf["engine_name"].dropna().unique())
        for eng, grp in sf.dropna(subset=["engine_name", "eval"]).groupby("engine_name"):
            base = cmap.get(eng, ACCENT)
            ev_fig.add_trace(go.Violin(
                x=grp["engine_name"], y=grp["eval"],
                name=eng, box_visible=True, meanline_visible=True,
                fillcolor=hex_to_rgba(base, 0.4),
                line_color=base,
                points=False,
            ))
        ev_fig.update_layout(title="Eval Distribution by Engine", showlegend=False)
        apply_theme(ev_fig)
    else:
        ev_fig = empty_fig("No eval data")

    # Pruning efficiency by engine (NMP, SEE, Delta)
    prune_metrics = [c for c in ["nmp_ratio", "nmp_fail_ratio", "avg_see_prune_ratio", "avg_delta_prune_ratio",
                                  "see_prune_ratio", "delta_prune_ratio", "avg_pvs_research_ratio", "pvs_research_ratio"]
                     if c in sf.columns]
    if prune_metrics and "engine_name" in sf.columns and not sf.empty:
        prune_agg = sf.groupby("engine_name")[prune_metrics].mean().reset_index()
        prune_melt = prune_agg.melt(id_vars="engine_name", var_name="metric", value_name="value")
        prune_fig = px.bar(prune_melt, x="engine_name", y="value", color="metric",
                           barmode="group", color_discrete_sequence=_PALETTE,
                           labels={"engine_name": "Engine", "value": "Ratio", "metric": "Pruning Metric"})
        prune_fig.update_layout(title="Pruning & NMP Efficiency by Engine")
        apply_theme(prune_fig)
    else:
        prune_fig = empty_fig("No pruning data available")

    # Regression alerts: flag metrics where latest version is worse than prior
    regression_items = []
    if "engine_label" in sf.columns and len(sf["engine_label"].dropna().unique()) >= 2:
        versions = sf.groupby("engine_label")["engine_id"].first().sort_values()
        if len(versions) >= 2:
            latest = versions.index[-1]
            prior = versions.index[-2]
            check_cols = [c for c in ["nps", "eval", "depth", "tt_hit_ratio", "fail_high_first_ratio",
                                       "nmp_ratio", "best_move_match"] if c in sf.columns]
            for col in check_cols:
                v_latest = sf[sf["engine_label"] == latest][col].mean()
                v_prior = sf[sf["engine_label"] == prior][col].mean()
                if pd.notna(v_latest) and pd.notna(v_prior) and v_prior != 0:
                    pct_change = (v_latest - v_prior) / abs(v_prior) * 100
                    if pct_change < -5:  # >5% regression
                        regression_items.append(
                            html.Div(f"⚠ {metric_label(col)}: {pct_change:+.1f}% ({prior} → {latest})",
                                     style={"color": "#ff6b35", "fontSize": "11px", "fontFamily": "'JetBrains Mono'",
                                            "marginBottom": "4px"})
                        )

    regression_panel = panel(
        section("REGRESSION ALERTS"),
        html.Div(regression_items if regression_items else
                 [html.Div("No regressions detected (>5% decline)", style={"color": TEXT_SEC, "fontSize": "11px"})])
    ) if "engine_label" in sf.columns else html.Div()

    # Experiment summary panel
    exp_panel = html.Div()
    if not experiments_df.empty:
        exp_summary = experiments_df.groupby("type").size().reset_index(name="count")
        exp_items = [
            html.Div(f"{row['type'].upper()}: {row['count']} experiments",
                     style={"color": TEXT_PRI, "fontSize": "11px", "fontFamily": "'JetBrains Mono'", "marginBottom": "4px"})
            for _, row in exp_summary.iterrows()
        ]
        exp_panel = panel(section("EXPERIMENTS"), html.Div(exp_items))

    return html.Div([
        kpis,
        kpis2,
        html.Div([
            regression_panel,
            exp_panel,
        ], style={"display": "grid", "gridTemplateColumns": "1fr 1fr", "gap": "12px", "marginBottom": "12px"}) if not experiments_df.empty else regression_panel,
        html.Div(style={"height": "12px"}) if experiments_df.empty else html.Div(),
        html.Div([
            panel(section("PERFORMANCE"), graph(bar_fig, 320), flex="1"),
            panel(section("EVAL SPREAD"), graph(ev_fig, 320), flex="1"),
        ], style={"display": "grid", "gridTemplateColumns": "1fr 1fr", "gap": "12px", "marginBottom": "12px"}),
        html.Div([
            panel(section("PRUNING & NMP EFFICIENCY"), graph(prune_fig, 320), flex="1"),
        ], style={"display": "grid", "gridTemplateColumns": "1fr", "gap": "12px"}),
    ])


def tab_games(gf: pd.DataFrame) -> html.Div:
    # Games per engine
    figs = []
    if not gf.empty:
        if "result" in gf.columns and "white_engine_id" in gf.columns:
            emap = engines_df.set_index("id")["name"] if not engines_df.empty else pd.Series(dtype=str)
            gf2 = gf.copy()
            gf2["white_name"] = gf2["white_engine_id"].map(emap)
            gf2["black_name"] = gf2["black_engine_id"].map(emap)
            res_counts = (
                pd.concat([
                    gf2.assign(engine=gf2["white_name"]),
                    gf2.assign(engine=gf2["black_name"]),
                ]).groupby(["engine", "result"]).size().reset_index(name="count")
                .dropna(subset=["engine"])
            )
            rf = px.bar(res_counts, x="engine", y="count", color="result", barmode="group",
                        color_discrete_sequence=_PALETTE, labels={"engine": "Engine", "count": "Games"})
            rf.update_layout(title="Games by Engine & Result")
            apply_theme(rf)
            figs.append(panel(section("GAME RESULTS"), graph(rf, 300), flex="1"))

        if "run_time_s" in gf.columns:
            rt_fig = px.histogram(gf, x="run_time_s", nbins=40, color_discrete_sequence=[ACCENT],
                                  labels={"run_time_s": "Run time (s)"})
            rt_fig.update_layout(title="Game Duration Distribution")
            apply_theme(rt_fig)
            figs.append(panel(section("GAME DURATION"), graph(rt_fig, 300), flex="1"))

    display_cols = [c for c in gf.columns if c not in {"moves", "start_fen"}]

    # Game selector for per-ply progress plot
    game_options = []
    if not gf.empty and "id" in gf.columns:
        # label: "<id>: <opening> (date)" — keep concise
        def _lbl(r):
            opening = r.get("opening") or ""
            date = str(r.get("date") or "")
            return f"{int(r['id'])}: {opening} {date}".strip()

        # Sort by `date` when available, otherwise fall back to `id` (or unsorted)
        if "id" in gf.columns:
            sort_df = gf.sort_values(by="id", na_position="last")
        else:
            sort_df = gf
        game_options = [{"label": _lbl(r), "value": int(r["id"])} for _, r in sort_df.iterrows()]

    selector = html.Div([
        html.Div("Select Game", style=_sidebar_label),
        dcc.Dropdown(id="game-select", options=game_options, placeholder="Pick a game", style=_dropdown_style),
    ], style={"width": "360px", "marginBottom": "12px"})

    return html.Div([
        html.Div(figs, style={"display": "grid", "gridTemplateColumns": "1fr 1fr", "gap": "12px", "marginBottom": "16px"}),
        selector,
        html.Div(id="game-progress-container", style={"marginBottom": "12px"}),
        panel(section("GAME TABLE"), table(gf[display_cols])),
        html.Div(f"Showing {min(len(gf), MAX_TABLE_ROWS):,} of {len(gf):,} rows",
                 style={"color": TEXT_SEC, "fontSize": "10px", "marginTop": "6px", "fontFamily": "'JetBrains Mono'"}),
    ])


def tab_search(sf: pd.DataFrame) -> html.Div:
    nums = numeric_cols(sf)
    if not nums:
        return html.Div("No numeric columns in search data.", style={"color": TEXT_SEC})

    x_default = "depth" if "depth" in nums else nums[0]
    y_default = "eval" if "eval" in nums else (nums[1] if len(nums) > 1 else nums[0])

    axis_row = html.Div([
        html.Div([
            html.Div("X AXIS", style=_sidebar_label),
            dcc.Dropdown(id="srch-x", options=metric_options(nums),
                         value=x_default, clearable=False, style=_dropdown_style),
        ], style={"flex": "1"}),
        html.Div([
            html.Div("Y AXIS", style=_sidebar_label),
            dcc.Dropdown(id="srch-y", options=metric_options(nums),
                         value=y_default, clearable=False, style=_dropdown_style),
        ], style={"flex": "1"}),
        html.Div([
            html.Div("COLOUR BY", style=_sidebar_label),
            # Include categorical defaults plus numeric axis columns so users can
            # colour by any axis (e.g., eval_diff by depth with see_prune colouring)
            dcc.Dropdown(id="srch-color",
                         options=(
                             [{"label": c.replace("_", " ").title(), "value": c}
                              for c in ["engine_name", "result_label", "engine_side"] if c in sf.columns]
                             + metric_options([c for c in nums if c in sf.columns])
                         ),
                         value=("engine_name" if "engine_name" in sf.columns else (nums[0] if nums else None)),
                         clearable=True, style=_dropdown_style),
        ], style={"flex": "1"}),
    ], style={"display": "flex", "gap": "16px", "marginBottom": "16px"})

    return html.Div([
        panel(
            section("SEARCH SCATTER"),
            axis_row,
            html.Div(id="srch-graph-container"),
            html.Div("Click a point to inspect the position details below.", style={"color": TEXT_SEC, "fontSize": "10px", "marginTop": "8px"}),
        ),
        html.Div(style={"height": "12px"}),
        html.Div(id="fen-detail-container"),
        html.Div(style={"height": "12px"}),
        panel(
            section("SEARCH TABLE"),
            table(sf),
            html.Div(f"Showing {min(len(sf), MAX_TABLE_ROWS):,} of {len(sf):,} rows",
                     style={"color": TEXT_SEC, "fontSize": "10px", "marginTop": "6px", "fontFamily": "'JetBrains Mono'"}),
        ),
    ])


def tab_iter(sf: pd.DataFrame) -> html.Div:
    """Per-iteration depth view: how do metrics evolve as iterative deepening progresses?"""
    if iter_df.empty:
        return html.Div("No searches_by_iteration data found.", style={"color": TEXT_SEC})

    # Join to filtered searches
    valid_ids = sf["id"].unique() if "id" in sf.columns else []
    idf = iter_df[iter_df["search_id"].isin(valid_ids)].copy() if len(valid_ids) else iter_df.copy()

    nums = numeric_cols(idf)
    y_opts = metric_options(nums)

    # EBF heatmap: branching factor by depth × game phase
    ebf_heatmap = html.Div()
    if "ebf" in idf.columns and "depth" in idf.columns:
        phase_col = "game_phase" if "game_phase" in idf.columns else None
        if phase_col:
            hm_data = idf.dropna(subset=["ebf", "depth", phase_col])
            if not hm_data.empty:
                hm_pivot = hm_data.pivot_table(values="ebf", index=phase_col, columns="depth", aggfunc="mean")
                hm_fig = px.imshow(hm_pivot, color_continuous_scale="Viridis", aspect="auto",
                                   labels={"x": "Iteration Depth", "y": "Game Phase", "color": "Avg EBF"})
                hm_fig.update_layout(title="Branching Factor Heatmap (Depth × Game Phase)")
                apply_theme(hm_fig)
                ebf_heatmap = panel(section("EBF HEATMAP"), graph(hm_fig, 300))

    return html.Div([
        panel(
            section("METRIC vs ITERATION DEPTH"),
            html.Div([
                html.Div([
                    html.Div("Y METRIC", style=_sidebar_label),
                    dcc.Dropdown(id="iter-y", options=y_opts,
                                 value="eval" if "eval" in nums else (nums[0] if nums else None),
                                 clearable=False, style=_dropdown_style),
                ], style={"flex": "1"}),
                html.Div([
                    html.Div("AGGREGATION", style=_sidebar_label),
                    dcc.Dropdown(id="iter-agg",
                                 options=[{"label": a, "value": a} for a in ["mean", "median", "std"]],
                                 value="mean", clearable=False, style=_dropdown_style),
                ], style={"flex": "1"}),
            ], style={"display": "flex", "gap": "12px", "marginBottom": "12px"}),
            html.Div(id="iter-graph-container"),
        ),
        html.Div(style={"height": "12px"}),
        ebf_heatmap,
        html.Div(style={"height": "12px"}),
        panel(section("RAW ITERATION DATA"), table(idf)),
    ])


def tab_tree(sf: pd.DataFrame) -> html.Div:
    """Per-tree-ply view: how do stats distribute across plies in the search tree?"""
    if tree_df.empty:
        return html.Div("No searches_by_tree_depth data found.", style={"color": TEXT_SEC})

    valid_ids = sf["id"].unique() if "id" in sf.columns else []
    tdf = tree_df[tree_df["search_id"].isin(valid_ids)].copy() if len(valid_ids) else tree_df.copy()

    nums = numeric_cols(tdf)
    y_opts = metric_options(nums)

    return html.Div([
        panel(
            section("STAT vs TREE PLY"),
            html.Div([
                html.Div([
                    html.Div("Y METRIC", style=_sidebar_label),
                    dcc.Dropdown(id="tree-y", options=y_opts,
                                 value="nodes" if "nodes" in nums else (nums[0] if nums else None),
                                 clearable=False, style=_dropdown_style),
                ], style={"flex": "1"}),
                html.Div([
                    html.Div("SCALE", style=_sidebar_label),
                    dcc.Dropdown(id="tree-scale",
                                 options=[{"label": "Linear", "value": "linear"},
                                          {"label": "Log", "value": "log"}],
                                 value="linear", clearable=False, style=_dropdown_style),
                ], style={"flex": "1"}),
            ], style={"display": "flex", "gap": "12px", "marginBottom": "12px"}),
            html.Div(id="tree-graph-container"),
        ),
        html.Div(style={"height": "12px"}),
        panel(section("RAW TREE DEPTH DATA"), table(tdf)),
    ])


def tab_compare(sf: pd.DataFrame) -> html.Div:
    """Head-to-head engine comparison with delta charts (by engine label/version)."""
    if "engine_label" not in sf.columns:
        return html.Div("Engine name data not available.", style={"color": TEXT_SEC})

    eng_names = sorted(sf["engine_label"].dropna().unique())
    eng_opts = [{"label": e, "value": e} for e in eng_names]
    nums = numeric_cols(sf)
    metric_opts = metric_options(nums)

    return html.Div([
        panel(
            section("ENGINE COMPARISON"),
            html.Div([
                html.Div([
                    html.Div("ENGINE A (baseline)", style=_sidebar_label),
                    dcc.Dropdown(id="cmp-eng-a", options=eng_opts,
                                 value=eng_names[0] if eng_names else None,
                                 clearable=False, style=_dropdown_style),
                ], style={"flex": "1"}),
                html.Div([
                    html.Div("ENGINE B (candidate)", style=_sidebar_label),
                    dcc.Dropdown(id="cmp-eng-b", options=eng_opts,
                                 value=eng_names[1] if len(eng_names) > 1 else (eng_names[0] if eng_names else None),
                                 clearable=False, style=_dropdown_style),
                ], style={"flex": "1"}),
                html.Div([
                    html.Div("METRIC", style=_sidebar_label),
                    dcc.Dropdown(id="cmp-metric", options=metric_opts,
                                 value="eval" if "eval" in nums else (nums[0] if nums else None),
                                 clearable=False, style=_dropdown_style),
                ], style={"flex": "1"}),
                html.Div([
                    html.Div("CHART TYPE", style=_sidebar_label),
                    dcc.Dropdown(id="cmp-chart",
                                 options=[{"label": t, "value": t} for t in
                                          ["Box", "Violin", "Histogram", "Delta (A-B)"]],
                                 value="Box", clearable=False, style=_dropdown_style),
                ], style={"flex": "1"}),
            ], style={"display": "flex", "gap": "12px", "marginBottom": "12px"}),
            html.Div([
                html.Button("Export Comparison Report", id="cmp-export-report", n_clicks=0,
                            style={"padding": "6px 12px", "background": "transparent", "border": f"1px solid {BORDER}",
                                   "borderRadius": "5px", "color": TEXT_SEC, "fontSize": "10px", "fontFamily": "'JetBrains Mono'",
                                   "cursor": "pointer"}),
                html.Div(id="cmp-export-link", style={"marginLeft": "12px", "display": "inline-block"}),
            ], style={"marginBottom": "12px"}),
            html.Div(id="cmp-graph-container"),
        ),
        html.Div(style={"height": "12px"}),
        panel(
            section("MULTI-METRIC SUMMARY (ALL ENGINES)"),
            html.Div(id="cmp-summary-container"),
        ),
    ])


def tab_timing() -> html.Div:
    if timing_df.empty:
        return html.Div("No timing data found.", style={"color": TEXT_SEC})

    # Top functions by total time
    tsum = timing_df.groupby("function", as_index=False).agg(
        total_ms=("total_time_ms", "sum"),
        calls=("num_calls", "sum"),
    ).sort_values("total_ms", ascending=False)
    tsum["ms_per_call"] = tsum["total_ms"] / tsum["calls"].replace(0, np.nan)

    bar_fig = px.bar(tsum.head(20), x="function", y="total_ms",
                     color_discrete_sequence=[ACCENT],
                     labels={"function": "Function", "total_ms": "Total ms"})
    bar_fig.update_layout(title="Top 20 Functions by Total Time", xaxis_tickangle=-40)
    apply_theme(bar_fig)

    call_fig = px.bar(tsum.head(20), x="function", y="ms_per_call",
                      color_discrete_sequence=[ACCENT2],
                      labels={"function": "Function", "ms_per_call": "ms / call"})
    call_fig.update_layout(title="Average Time per Call", xaxis_tickangle=-40)
    apply_theme(call_fig)

    # Per-engine if engine_name is available
    extra = []
    if "engine_name" in timing_df.columns:
        eng_tsum = timing_df.groupby(["engine_name", "function"], as_index=False).agg(
            total_ms=("total_time_ms", "sum")
        )
        hm_df = eng_tsum.pivot(index="function", columns="engine_name", values="total_ms").fillna(0)
        hm_fig = px.imshow(hm_df, color_continuous_scale="Blues", aspect="auto",
                           labels={"color": "Total ms"})
        hm_fig.update_layout(title="Time Heatmap: Function × Engine")
        apply_theme(hm_fig)
        extra.append(panel(section("HEATMAP: FUNCTION × ENGINE"), graph(hm_fig, 400)))

    return html.Div([
        html.Div([
            panel(section("TOTAL TIME"), graph(bar_fig, 320), flex="1"),
            panel(section("AVG TIME / CALL"), graph(call_fig, 320), flex="1"),
        ], style={"display": "grid", "gridTemplateColumns": "1fr 1fr", "gap": "12px"}),
        *extra,
        html.Div(style={"height": "12px"}),
        panel(section("TIMING TABLE"), table(tsum)),
    ])


def tab_sprt() -> html.Div:
    if sprt_df.empty:
        return html.Div("No SPRT data found.", style={"color": TEXT_SEC})

    figs = []

    # LLR over time / experiments
    if "llr" in sprt_df.columns:
        xcol = "sprt_label" if "sprt_label" in sprt_df.columns else ("id" if "id" in sprt_df.columns else None)
        llr_fig = px.scatter(
            sprt_df, x=xcol, y="llr",
            color="result" if "result" in sprt_df.columns else None,
            hover_data=[c for c in ["elo_diff", "los", "games_played", "engine_name"] if c in sprt_df.columns],
            color_discrete_map={"pass": "#7fff6b", "fail": "#ff6b35", "inconclusive": "#f7b731"},
            labels={xcol: "SPRT", "llr": "LLR"},
        )
        llr_fig.add_hline(y=0, line_dash="dot", line_color=TEXT_SEC)
        llr_fig.update_layout(title="LLR by SPRT")
        apply_theme(llr_fig)
        figs.append(panel(section("LLR"), graph(llr_fig, 320), flex="1"))

    # Elo diff per SPRT (scalar per run: candidate - baseline)
    if "elo_diff" in sprt_df.columns:
        xcol = "sprt_label" if "sprt_label" in sprt_df.columns else ("id" if "id" in sprt_df.columns else None)
        elo_fig = px.scatter(
            sprt_df, x=xcol, y="elo_diff",
            color="result" if "result" in sprt_df.columns else None,
            color_discrete_map={"pass": "#7fff6b", "fail": "#ff6b35", "inconclusive": "#f7b731"},
            hover_data=[c for c in ["elo_diff", "los", "games_played", "engine_name"] if c in sprt_df.columns],
            labels={xcol: "SPRT", "elo_diff": "Elo (candidate - baseline)"},
        )
        elo_fig.update_traces(marker=dict(size=10))
        elo_fig.add_hline(y=0, line_dash="dot", line_color=TEXT_SEC)
        elo_fig.update_layout(title="Elo Diff per SPRT (candidate - baseline)")
        apply_theme(elo_fig)
        figs.append(panel(section("ELO DIFF"), graph(elo_fig, 320), flex="1"))

    # WDL breakdown
    wdl_cols = [c for c in ["candidate_wins", "candidate_draws", "candidate_losses"] if c in sprt_df.columns]
    if wdl_cols:
        id_col = "sprt_label" if "sprt_label" in sprt_df.columns else ("id" if "id" in sprt_df.columns else None)
        if id_col is not None:
            wdl_melt = sprt_df[[id_col] + wdl_cols].melt(id_vars=id_col, var_name="outcome", value_name="count")
            wdl_fig = px.bar(wdl_melt, x=id_col, y="count", color="outcome", barmode="stack",
                         color_discrete_map={
                             "candidate_wins": "#7fff6b",
                             "candidate_draws": "#f7b731",
                             "candidate_losses": "#ff6b35",
                         },
                         labels={id_col: "SPRT", "count": "Games", "outcome": "Outcome"})
        wdl_fig.update_layout(title="Candidate W/D/L per SPRT")
        apply_theme(wdl_fig)
        figs_bottom = [panel(section("W/D/L PER SPRT"), graph(wdl_fig, 300))]
    else:
        figs_bottom = []

    return html.Div([
        html.Div(figs, style={"display": "grid", "gridTemplateColumns": "1fr 1fr", "gap": "12px"}),
        html.Div(style={"height": "12px"}),
        *figs_bottom,
        html.Div(style={"height": "12px"}),
        panel(section("SPRT TABLE"), table(sprt_df)),
    ])


def tab_sts() -> html.Div:
    if sts_df.empty:
        return html.Div("No STS data found.", style={"color": TEXT_SEC})

    figs = []

    # Accuracy by engine
    if "move_is_correct" in sts_df.columns and "engine_name" in sts_df.columns:
        acc = (sts_df.groupby("engine_name")["move_is_correct"]
               .mean().mul(100).reset_index(name="accuracy"))
        af = px.bar(acc, x="engine_name", y="accuracy", color_discrete_sequence=[ACCENT],
                    text_auto=".1f", labels={"engine_name": "Engine", "accuracy": "Accuracy (%)"})
        af.update_layout(title="STS Accuracy by Engine")
        apply_theme(af)
        figs.append(panel(section("ACCURACY"), graph(af, 300), flex="1"))

    # Score by suite
    if "suite" in sts_df.columns and "engine_score" in sts_df.columns and "engine_name" in sts_df.columns:
        suite_sc = sts_df.groupby(["suite", "engine_name"])["engine_score"].mean().reset_index()
        sf2 = px.bar(suite_sc, x="suite", y="engine_score", color="engine_name",
                     barmode="group", color_discrete_sequence=_PALETTE,
                     labels={"suite": "Suite", "engine_score": "Avg Score", "engine_name": "Engine"})
        sf2.update_layout(title="Avg Score by Suite & Engine")
        apply_theme(sf2)
        figs.append(panel(section("SUITE SCORES"), graph(sf2, 300), flex="1"))

    return html.Div([
        html.Div(figs, style={"display": "grid", "gridTemplateColumns": "1fr 1fr", "gap": "12px"}),
        html.Div(style={"height": "12px"}),
        panel(section("STS TABLE"), table(sts_df)),
    ])


def tab_perft() -> html.Div:
    if perft_df.empty:
        return html.Div("No perft data found.", style={"color": TEXT_SEC})

    figs = []

    # NPS
    if "nodes" in perft_df.columns and "time_ms" in perft_df.columns:
        pf = perft_df.copy()
        pf["nps"] = pf["nodes"] / (pf["time_ms"] / 1000).replace(0, np.nan)
        if "engine_name" in pf.columns:
            nps_fig = px.box(pf, x="engine_name", y="nps", color="engine_name",
                             color_discrete_sequence=_PALETTE,
                             labels={"engine_name": "Engine", "nps": "Nodes/sec"})
            nps_fig.update_layout(title="NPS by Engine", showlegend=False)
            apply_theme(nps_fig)
            figs.append(panel(section("NPS"), graph(nps_fig, 320), flex="1"))

    # Correctness rate
    if "correct" in perft_df.columns and "engine_name" in perft_df.columns:
        corr_rate = perft_df.groupby("engine_name")["correct"].mean().mul(100).reset_index(name="pct_correct")
        cf = px.bar(corr_rate, x="engine_name", y="pct_correct",
                    color_discrete_sequence=["#7fff6b"], text_auto=".1f",
                    labels={"engine_name": "Engine", "pct_correct": "% Correct"})
        cf.update_layout(title="Perft Correctness by Engine")
        apply_theme(cf)
        figs.append(panel(section("CORRECTNESS"), graph(cf, 320), flex="1"))

    return html.Div([
        html.Div(figs, style={"display": "grid", "gridTemplateColumns": "1fr 1fr", "gap": "12px"}),
        html.Div(style={"height": "12px"}),
        panel(section("PERFT TABLE"), table(perft_df)),
    ])


def tab_positions(sf: pd.DataFrame) -> html.Div:
    """FEN-based position type analysis tab."""
    # Detect tactical/positional/endgame score column names produced by ETL.
    # accept multiple possible ETL naming conventions
    tactical_col = next((c for c in ("position_tactical_score", "pos_tactical") if c in sf.columns), None)
    positional_col = next((c for c in ("position_positional_score", "pos_positional") if c in sf.columns), None)
    endgame_col = next((c for c in ("position_endgame_score", "pos_endgame") if c in sf.columns), None)

    required = ["pos_label"]
    missing = [c for c in required if c not in sf.columns]
    if sf.empty or missing or not (tactical_col and positional_col and endgame_col):
        msg = "No position classification data. Ensure searches have FEN values and position score columns."
        details = []
        if missing:
            details.append(f"Missing: {', '.join(missing)}")
        if not tactical_col or not positional_col or not endgame_col:
            found = {"tactical": tactical_col, "positional": positional_col, "endgame": endgame_col}
            details.append("Detected columns: " + ", ".join(f"{k}={v or 'None'}" for k, v in found.items()))
        if details:
            msg = msg + " " + "; ".join(details)
        return html.Div(msg, style={"color": TEXT_SEC})

    # ── Distribution of position types ──────────────────────────────────────
    label_counts = sf["pos_label"].value_counts().reset_index()
    label_counts.columns = ["type", "count"]
    pie_fig = px.pie(
        label_counts, values="count", names="type",
        color="type",
        color_discrete_map={"Tactical": ACCENT2, "Positional": ACCENT, "Endgame": "#7fff6b"},
        hole=0.45,
    )
    pie_fig.update_layout(title="Position Type Distribution")
    apply_theme(pie_fig)

    # ── Score distributions as violin ────────────────────────────────────────
    # Build a tidy dataframe using detected column names and map them to readable labels
    score_df = sf[["pos_label", tactical_col, positional_col, endgame_col]].rename(
        columns={tactical_col: "tactical", positional_col: "positional", endgame_col: "endgame"}
    )
    score_melt = score_df.melt(id_vars="pos_label", var_name="score_type", value_name="score")
    score_melt["score_type"] = score_melt["score_type"].str.capitalize()
    violin_fig = px.violin(
        score_melt, x="score_type", y="score", color="pos_label",
        color_discrete_map={"Tactical": ACCENT2, "Positional": ACCENT, "Endgame": "#7fff6b"},
        box=True, points=False,
        labels={"score_type": "Score Component", "score": "Score (0–1)", "pos_label": "Classified As"},
    )
    violin_fig.update_layout(title="Score Component Distributions by Position Type")
    apply_theme(violin_fig)

    # ── How position type interacts with engine metrics ──────────────────────
    metric_figs = []
    for metric, label in [("eval", "Eval (cp)"), ("depth", "Search Depth"), ("nodes", "Nodes"), ("time_ms", "Time (ms)")]:
        if metric not in sf.columns:
            continue
        mf = px.box(
            sf.dropna(subset=[metric, "pos_label"]),
            x="pos_label", y=metric, color="pos_label",
            color_discrete_map={"Tactical": ACCENT2, "Positional": ACCENT, "Endgame": "#7fff6b"},
            labels={"pos_label": "Position Type", metric: label},
            points=False,
        )
        mf.update_layout(title=f"{label} by Position Type", showlegend=False)
        apply_theme(mf)
        metric_figs.append(panel(section(label.upper()), graph(mf, 260), flex="1"))

    # ── Position type breakdown per engine ───────────────────────────────────
    eng_pos_figs = []
    if "engine_name" in sf.columns:
        ep = sf.groupby(["engine_name", "pos_label"]).size().reset_index(name="count")
        ep_pct = ep.copy()
        totals = ep.groupby("engine_name")["count"].transform("sum")
        ep_pct["pct"] = ep_pct["count"] / totals * 100
        ep_fig = px.bar(
            ep_pct, x="engine_name", y="pct", color="pos_label", barmode="stack",
            color_discrete_map={"Tactical": ACCENT2, "Positional": ACCENT, "Endgame": "#7fff6b"},
            text_auto=".1f",
            labels={"engine_name": "Engine", "pct": "% of Positions", "pos_label": "Type"},
        )
        ep_fig.update_layout(title="Position Type Mix per Engine")
        apply_theme(ep_fig)
        eng_pos_figs.append(panel(section("POSITION MIX PER ENGINE"), graph(ep_fig, 320)))

        # Avg score metrics by engine × position type — use detected column names
        cols_to_avg = [c for c in (tactical_col, positional_col, endgame_col) if c in sf.columns]
        if cols_to_avg:
            score_eng = sf.groupby(["engine_name", "pos_label"])[cols_to_avg].mean().reset_index()
            for sc, sc_label in [(tactical_col, "Avg Tactical Score"), (positional_col, "Avg Positional Score")]:
                if sc and sc in score_eng.columns:
                    sf2 = px.bar(
                        score_eng, x="engine_name", y=sc, color="pos_label", barmode="group",
                        color_discrete_map={"Tactical": ACCENT2, "Positional": ACCENT, "Endgame": "#7fff6b"},
                        labels={"engine_name": "Engine", sc: sc_label, "pos_label": "Type"},
                    )
                    sf2.update_layout(title=f"{sc_label} by Engine & Position Type")
                    apply_theme(sf2)
                    eng_pos_figs.append(panel(section(sc_label.upper()), graph(sf2, 280)))

    # ── Ternary scatter: each search as a point in (T, P, E) space ──────────
    tern_fig = None
    if tactical_col and positional_col and endgame_col and all(c in sf.columns for c in [tactical_col, positional_col, endgame_col]):
        sample = sf.dropna(subset=[tactical_col, positional_col, endgame_col]).head(5000)
        tern_fig = go.Figure(go.Scatterternary(
            a=sample[tactical_col],
            b=sample[positional_col],
            c=sample[endgame_col],
            mode="markers",
            marker=dict(
                size=4,
                color=sample["pos_label"].map({"Tactical": ACCENT2, "Positional": ACCENT, "Endgame": "#7fff6b"}),
                opacity=0.5,
            ),
            text=sample.get("engine_name", pd.Series([""] * len(sample))),
        ))
        tern_fig.update_layout(
            title="Position Space: Tactical / Positional / Endgame (ternary)",
            ternary=dict(
                aaxis=dict(title="Tactical", color=ACCENT2, gridcolor=BORDER),
                baxis=dict(title="Positional", color=ACCENT, gridcolor=BORDER),
                caxis=dict(title="Endgame", color="#7fff6b", gridcolor=BORDER),
                bgcolor=PANEL_BG,
            ),
        )
        apply_theme(tern_fig)

    return html.Div([
        html.Div([
            panel(section("POSITION TYPE BREAKDOWN"), graph(pie_fig, 320), flex="1"),
            panel(section("SCORE COMPONENTS"), graph(violin_fig, 320), flex="1"),
        ], style={"display": "grid", "gridTemplateColumns": "1fr 1fr", "gap": "12px", "marginBottom": "12px"}),

        html.Div(metric_figs[:2], style={"display": "grid", "gridTemplateColumns": "1fr 1fr", "gap": "12px", "marginBottom": "12px"}),
        html.Div(metric_figs[2:], style={"display": "grid", "gridTemplateColumns": "1fr 1fr", "gap": "12px", "marginBottom": "12px"}),

        *[html.Div([f], style={"marginBottom": "12px"}) for f in eng_pos_figs],

        *([] if tern_fig is None else [
            panel(section("TERNARY POSITION SPACE"), graph(tern_fig, 440))
        ]),
    ])


def tab_corr(sf: pd.DataFrame) -> html.Div:
    num_df = sf.select_dtypes(include=np.number)[[c for c in sf.select_dtypes(include=np.number).columns if c not in NUMERIC_EXCLUDE]]
    if num_df.shape[1] < 2:
        return html.Div("Not enough numeric columns for correlation.", style={"color": TEXT_SEC})
    corr = num_df.corr().round(3).fillna(0)
    fig = px.imshow(
        corr, text_auto=".2f", color_continuous_scale="RdBu_r",
        aspect="auto", zmin=-1, zmax=1,
        labels={"color": "r"},
    )
    fig.update_layout(title="Search Feature Correlation Matrix")
    apply_theme(fig)
    return panel(
        section("CORRELATION MATRIX"),
        graph(fig, height=max(380, len(corr) * 24)),
    )


# ──────────────────────────────────────────────────────────────────────────────
# 9b. NEW TAB RENDERERS (Trends, Openings, Move Quality, Time Mgmt, TT Analysis)
# ──────────────────────────────────────────────────────────────────────────────

def tab_trends(sf: pd.DataFrame) -> html.Div:
    """Version trend timeline: key metrics across engine versions."""
    if "engine_label" not in sf.columns or sf.empty:
        return html.Div("No engine version data available.", style={"color": TEXT_SEC})

    versions = sorted(sf["engine_label"].dropna().unique())
    if len(versions) < 2:
        return html.Div("Need at least 2 engine versions for trends.", style={"color": TEXT_SEC})

    trend_metrics = [c for c in ["nps", "depth", "eval", "tt_hit_ratio", "fail_high_first_ratio",
                                  "nmp_ratio", "ebf", "avg_ebf", "best_move_match", "qratio"]
                     if c in sf.columns]
    if not trend_metrics:
        return html.Div("No trend-compatible metrics found.", style={"color": TEXT_SEC})

    agg = sf.groupby("engine_label")[trend_metrics].mean().reindex(versions).reset_index()

    figs = []
    for col in trend_metrics:
        fig = px.line(agg, x="engine_label", y=col, markers=True,
                      color_discrete_sequence=[ACCENT],
                      labels={"engine_label": "Version", col: metric_label(col)})
        fig.update_layout(title=metric_label(col))
        apply_theme(fig)
        figs.append(panel(graph(fig, 280)))

    # Build a grid of mini charts
    rows = []
    for i in range(0, len(figs), 3):
        rows.append(html.Div(figs[i:i+3],
                    style={"display": "grid", "gridTemplateColumns": "1fr 1fr 1fr", "gap": "12px", "marginBottom": "12px"}))

    return html.Div([section("METRIC TRENDS ACROSS VERSIONS")] + rows)


def tab_openings(sf: pd.DataFrame) -> html.Div:
    """Opening performance breakdown: win rate, eval, depth by opening/ECO."""
    if "opening" not in sf.columns or sf.empty:
        return html.Div("No opening data available. Ensure games have opening classification.", style={"color": TEXT_SEC})

    has_result = "result_label" in sf.columns
    has_eval = "eval" in sf.columns

    # Aggregate by opening
    agg_cols = {}
    if has_eval:
        agg_cols["eval"] = "mean"
    if "depth" in sf.columns:
        agg_cols["depth"] = "mean"
    if "nps" in sf.columns:
        agg_cols["nps"] = "mean"

    opening_stats = sf.groupby("opening").agg(searches=("opening", "size"), **{
        f"avg_{k}": (k, v) for k, v in agg_cols.items()
    }).reset_index()

    # Win rate by opening
    wr_fig = empty_fig("No result data")
    if has_result:
        wr = sf.dropna(subset=["opening", "result_label"]).groupby(["opening", "result_label"]).size().reset_index(name="count")
        totals = wr.groupby("opening")["count"].sum().rename("total")
        wr = wr.merge(totals, on="opening")
        wr["pct"] = wr["count"] / wr["total"] * 100
        # Only show openings with enough data
        big_openings = wr.groupby("opening")["count"].sum()
        big_openings = big_openings[big_openings >= 5].index
        wr = wr[wr["opening"].isin(big_openings)]
        if not wr.empty:
            wr_fig = px.bar(wr, x="opening", y="pct", color="result_label", barmode="stack",
                            color_discrete_map={"Win": "#7fff6b", "Draw": "#f7b731", "Loss": "#ff6b35"},
                            labels={"pct": "% of games", "opening": "Opening", "result_label": "Result"})
            wr_fig.update_layout(title="Win Rate by Opening", xaxis_tickangle=-45)
            apply_theme(wr_fig)

    # Avg eval by opening
    eval_fig = empty_fig("No eval data")
    if has_eval:
        opening_eval = sf.groupby("opening")["eval"].mean().reset_index()
        opening_eval = opening_eval.sort_values("eval", ascending=False).head(20)
        if not opening_eval.empty:
            eval_fig = px.bar(opening_eval, x="opening", y="eval", color_discrete_sequence=[ACCENT],
                              labels={"opening": "Opening", "eval": "Avg Eval (cp)"})
            eval_fig.update_layout(title="Average Eval by Opening (Top 20)", xaxis_tickangle=-45)
            apply_theme(eval_fig)

    # Per-engine opening comparison
    eng_open_fig = empty_fig("No engine/opening data")
    if "engine_name" in sf.columns and has_eval:
        eng_open = sf.groupby(["engine_name", "opening"])["eval"].mean().reset_index()
        top_opens = sf["opening"].value_counts().head(10).index
        eng_open = eng_open[eng_open["opening"].isin(top_opens)]
        if not eng_open.empty:
            eng_open_fig = px.bar(eng_open, x="opening", y="eval", color="engine_name",
                                  barmode="group", color_discrete_sequence=_PALETTE,
                                  labels={"opening": "Opening", "eval": "Avg Eval", "engine_name": "Engine"})
            eng_open_fig.update_layout(title="Eval by Opening × Engine (Top 10 openings)", xaxis_tickangle=-45)
            apply_theme(eng_open_fig)

    return html.Div([
        html.Div([
            panel(section("WIN RATE BY OPENING"), graph(wr_fig, 340)),
            panel(section("EVAL BY OPENING"), graph(eval_fig, 340)),
        ], style={"display": "grid", "gridTemplateColumns": "1fr 1fr", "gap": "12px", "marginBottom": "12px"}),
        panel(section("ENGINE × OPENING COMPARISON"), graph(eng_open_fig, 380)),
        html.Div(style={"height": "12px"}),
        panel(section("OPENING STATS TABLE"), table(opening_stats)),
    ])


def tab_quality(sf: pd.DataFrame) -> html.Div:
    """Move quality analysis: eval_diff distribution, move rank, nodes-to-solution."""
    if sf.empty:
        return html.Div("No search data available.", style={"color": TEXT_SEC})

    children = []

    # Eval diff histogram (vs Stockfish)
    if "eval_diff" in sf.columns:
        ed = sf["eval_diff"].dropna()
        if not ed.empty:
            # Clip extreme outliers for better visualization
            ed_clipped = ed.clip(-500, 500)
            if "engine_name" in sf.columns:
                plot_df = sf[["eval_diff", "engine_name"]].dropna()
                plot_df["eval_diff"] = plot_df["eval_diff"].clip(-500, 500)
                diff_fig = px.histogram(plot_df, x="eval_diff", color="engine_name",
                                        barmode="overlay", nbins=80, opacity=0.7,
                                        color_discrete_sequence=_PALETTE,
                                        labels={"eval_diff": "Eval Diff vs SF (cp)", "engine_name": "Engine"})
            else:
                diff_fig = px.histogram(pd.DataFrame({"eval_diff": ed_clipped}), x="eval_diff",
                                        nbins=80, color_discrete_sequence=[ACCENT],
                                        labels={"eval_diff": "Eval Diff vs SF (cp)"})
            diff_fig.add_vline(x=0, line_dash="dot", line_color=TEXT_SEC)
            diff_fig.update_layout(title="Eval Difference vs Stockfish")
            apply_theme(diff_fig)
            children.append(panel(section("EVAL DIFF DISTRIBUTION"), graph(diff_fig, 340)))

    # Move quality by game phase
    if "eval_diff" in sf.columns and "game_phase" in sf.columns:
        phase_df = sf[["eval_diff", "game_phase"]].dropna()
        if not phase_df.empty:
            phase_fig = px.box(phase_df, x="game_phase", y="eval_diff",
                               color_discrete_sequence=[ACCENT],
                               labels={"game_phase": "Game Phase", "eval_diff": "Eval Diff (cp)"})
            phase_fig.update_layout(title="Move Quality by Game Phase")
            apply_theme(phase_fig)
            children.append(panel(section("QUALITY BY GAME PHASE"), graph(phase_fig, 320)))

    # Move rank distribution
    if "engine_move_rank" in sf.columns:
        rank_data = sf["engine_move_rank"].dropna()
        if not rank_data.empty:
            rank_counts = rank_data.value_counts().sort_index().reset_index()
            rank_counts.columns = ["rank", "count"]
            rank_counts["rank"] = rank_counts["rank"].astype(int)
            rank_counts = rank_counts[rank_counts["rank"].between(0, 5)]
            rank_labels = {0: "Not Top-5", 1: "#1 (Best)", 2: "#2", 3: "#3", 4: "#4", 5: "#5"}
            rank_counts["label"] = rank_counts["rank"].map(rank_labels)
            rank_fig = px.bar(rank_counts, x="label", y="count", color_discrete_sequence=[ACCENT],
                              labels={"label": "Move Rank vs SF", "count": "Count"})
            rank_fig.update_layout(title="Engine Move Rank (vs Stockfish Top-5)")
            apply_theme(rank_fig)
            children.append(panel(section("MOVE RANK"), graph(rank_fig, 320)))

    # Nodes to solution: iterations needed to find the best move
    if "best_move_match" in sf.columns and "depth" in sf.columns:
        matched = sf[sf["best_move_match"] == 1]
        if not matched.empty and "engine_name" in matched.columns:
            nts_fig = px.box(matched, x="engine_name", y="depth",
                             color_discrete_sequence=_PALETTE,
                             labels={"engine_name": "Engine", "depth": "Depth to Find Best Move"})
            nts_fig.update_layout(title="Depth-to-Solution (positions where engine found SF best move)")
            apply_theme(nts_fig)
            children.append(panel(section("DEPTH TO SOLUTION"), graph(nts_fig, 320)))

    if not children:
        return html.Div("No move quality data (eval_diff, engine_move_rank) available.", style={"color": TEXT_SEC})

    return html.Div(children, style={"display": "flex", "flexDirection": "column", "gap": "12px"})


def tab_time_mgmt(sf: pd.DataFrame) -> html.Div:
    """Time management analysis: time vs complexity, time vs game phase."""
    if "time_ms" not in sf.columns or sf.empty:
        return html.Div("No time data available.", style={"color": TEXT_SEC})

    children = []

    # Time vs tactical score
    if "tactical_score" in sf.columns:
        sample = sf[["time_ms", "tactical_score"]].dropna().head(5000)
        if not sample.empty:
            fig = px.scatter(sample, x="tactical_score", y="time_ms", opacity=0.5,
                             color_discrete_sequence=[ACCENT],
                             labels={"tactical_score": "Tactical Complexity", "time_ms": "Time (ms)"})
            fig.update_layout(title="Time Allocated vs Tactical Complexity")
            apply_theme(fig)
            children.append(panel(section("TIME vs COMPLEXITY"), graph(fig, 340)))

    # Time by game phase
    if "game_phase" in sf.columns:
        phase_time = sf[["game_phase", "time_ms"]].dropna()
        if not phase_time.empty:
            fig = px.box(phase_time, x="game_phase", y="time_ms",
                         color_discrete_sequence=[ACCENT],
                         labels={"game_phase": "Game Phase", "time_ms": "Time (ms)"})
            fig.update_layout(title="Time Allocation by Game Phase")
            apply_theme(fig)
            children.append(panel(section("TIME BY PHASE"), graph(fig, 320)))

    # Time vs eval_diff (are we spending more time and still getting it wrong?)
    if "eval_diff" in sf.columns:
        td = sf[["time_ms", "eval_diff"]].dropna().head(5000)
        if not td.empty:
            color_col = "engine_name" if "engine_name" in sf.columns else None
            plot_df = sf[["time_ms", "eval_diff"] + (["engine_name"] if color_col else [])].dropna().head(5000)
            fig = px.scatter(plot_df, x="time_ms", y="eval_diff",
                             color=color_col, opacity=0.5,
                             color_discrete_sequence=_PALETTE,
                             labels={"time_ms": "Time (ms)", "eval_diff": "Eval Error (cp)"})
            fig.update_layout(title="Time Spent vs Eval Error")
            fig.add_hline(y=0, line_dash="dot", line_color=TEXT_SEC)
            apply_theme(fig)
            children.append(panel(section("TIME vs ERROR"), graph(fig, 340)))

    # Time efficiency: NPS over ply (does the engine slow down?)
    if "nps" in sf.columns and "ply" in sf.columns:
        nps_by_ply = sf[["ply", "nps", "engine_name"]].dropna() if "engine_name" in sf.columns else sf[["ply", "nps"]].dropna()
        if not nps_by_ply.empty:
            agg = nps_by_ply.groupby(["ply"] + (["engine_name"] if "engine_name" in nps_by_ply.columns else []))["nps"].mean().reset_index()
            fig = px.line(agg, x="ply", y="nps",
                          color="engine_name" if "engine_name" in agg.columns else None,
                          color_discrete_sequence=_PALETTE,
                          labels={"ply": "Game Ply", "nps": "Avg NPS"})
            fig.update_layout(title="NPS Over Game Progress")
            apply_theme(fig)
            children.append(panel(section("NPS OVER GAME"), graph(fig, 320)))

    if not children:
        return html.Div("Insufficient data for time management analysis.", style={"color": TEXT_SEC})

    return html.Div(children, style={"display": "flex", "flexDirection": "column", "gap": "12px"})


def tab_tt(sf: pd.DataFrame) -> html.Div:
    """TT (Transposition Table) deep-dive: hit rate, store rate, overwrites by depth."""
    if sf.empty:
        return html.Div("No search data available.", style={"color": TEXT_SEC})

    children = []

    # TT hit rate by depth (using iteration data if available)
    if not iter_df.empty and "tt_hit_ratio" in iter_df.columns and "depth" in iter_df.columns:
        tt_by_depth = iter_df.groupby("depth")["tt_hit_ratio"].mean().reset_index()
        fig = px.line(tt_by_depth, x="depth", y="tt_hit_ratio", markers=True,
                      color_discrete_sequence=[ACCENT],
                      labels={"depth": "Iteration Depth", "tt_hit_ratio": "TT Hit Rate"})
        fig.update_layout(title="TT Hit Rate by Iteration Depth")
        apply_theme(fig)
        children.append(panel(section("TT HIT RATE BY DEPTH"), graph(fig, 320)))

    # TT store rate by depth
    if not iter_df.empty and "tt_store_ratio" in iter_df.columns and "depth" in iter_df.columns:
        ts_by_depth = iter_df.groupby("depth")["tt_store_ratio"].mean().reset_index()
        fig = px.line(ts_by_depth, x="depth", y="tt_store_ratio", markers=True,
                      color_discrete_sequence=[ACCENT2],
                      labels={"depth": "Iteration Depth", "tt_store_ratio": "TT Store Rate"})
        fig.update_layout(title="TT Store Rate by Iteration Depth")
        apply_theme(fig)
        children.append(panel(section("TT STORE RATE BY DEPTH"), graph(fig, 320)))

    # TT effectiveness: hit rate by engine (search-level)
    if "tt_hit_ratio" in sf.columns and "engine_name" in sf.columns:
        fig = px.box(sf.dropna(subset=["tt_hit_ratio", "engine_name"]),
                     x="engine_name", y="tt_hit_ratio",
                     color_discrete_sequence=_PALETTE,
                     labels={"engine_name": "Engine", "tt_hit_ratio": "TT Hit Rate"})
        fig.update_layout(title="TT Hit Rate Distribution by Engine")
        apply_theme(fig)
        children.append(panel(section("TT HIT RATE BY ENGINE"), graph(fig, 320)))

    # Overwrite rate (if available)
    if "total_tt_overwritten" in sf.columns and "total_tt_stores" in sf.columns:
        sf_tt = sf[["total_tt_overwritten", "total_tt_stores", "engine_name", "depth"]].dropna()
        if not sf_tt.empty:
            sf_tt["overwrite_ratio"] = sf_tt["total_tt_overwritten"] / sf_tt["total_tt_stores"].replace(0, np.nan)
            if "engine_name" in sf_tt.columns:
                ow_agg = sf_tt.groupby("engine_name")["overwrite_ratio"].mean().reset_index()
                fig = px.bar(ow_agg, x="engine_name", y="overwrite_ratio",
                             color_discrete_sequence=[ACCENT],
                             labels={"engine_name": "Engine", "overwrite_ratio": "Overwrite Ratio"})
                fig.update_layout(title="TT Overwrite Ratio by Engine (stores that replaced existing entries)")
                apply_theme(fig)
                children.append(panel(section("TT OVERWRITE RATIO"), graph(fig, 300)))

    # TT hit rate vs search time scatter
    if "tt_hit_ratio" in sf.columns and "time_ms" in sf.columns:
        sample = sf[["tt_hit_ratio", "time_ms"]].dropna().head(5000)
        if not sample.empty:
            fig = px.scatter(sample, x="tt_hit_ratio", y="time_ms", opacity=0.4,
                             color_discrete_sequence=[ACCENT],
                             labels={"tt_hit_ratio": "TT Hit Rate", "time_ms": "Search Time (ms)"})
            fig.update_layout(title="TT Hit Rate vs Search Time")
            apply_theme(fig)
            children.append(panel(section("TT vs SEARCH TIME"), graph(fig, 300)))

    if not children:
        return html.Div("No TT-related columns found.", style={"color": TEXT_SEC})

    return html.Div(children, style={"display": "flex", "flexDirection": "column", "gap": "12px"})


# ──────────────────────────────────────────────────────────────────────────────
# 10. MAIN TAB CALLBACK
# ──────────────────────────────────────────────────────────────────────────────

@app.callback(
    Output("tab-content", "children"),
    Input("main-tabs",       "value"),
    Input("filter-engine",   "value"),
    Input("filter-result",   "value"),
    Input("filter-opening",  "value"),
    Input("filter-side",     "value"),
    Input("filter-pos-type", "value"),
    Input("filter-game-phase","value"),
    Input("filter-include-mates","value"),
)
def render_tab(tab, engine_ids, result_vals, opening_vals, side_vals, pos_type_vals, game_phase_vals, include_mates_val):
    gf, sf = apply_filters(engine_ids, result_vals, opening_vals, side_vals, pos_type_vals, game_phase_vals,
                           include_mates=(include_mates_val == "include"),
                           mates_only=(include_mates_val == "only"))

    if tab == "tab-overview": return tab_overview(gf, sf)
    if tab == "tab-trends":   return tab_trends(sf)
    if tab == "tab-games":    return tab_games(gf)
    if tab == "tab-search":   return tab_search(sf)
    if tab == "tab-iter":     return tab_iter(sf)
    if tab == "tab-tree":     return tab_tree(sf)
    if tab == "tab-compare":  return tab_compare(sf)
    if tab == "tab-openings": return tab_openings(sf)
    if tab == "tab-quality":  return tab_quality(sf)
    if tab == "tab-timing":   return tab_timing()
    if tab == "tab-time-mgmt": return tab_time_mgmt(sf)
    if tab == "tab-tt":       return tab_tt(sf)
    if tab == "tab-sprt":     return tab_sprt()
    if tab == "tab-sts":      return tab_sts()
    if tab == "tab-perft":    return tab_perft()
    if tab == "tab-positions": return tab_positions(sf)
    if tab == "tab-corr":     return tab_corr(sf)
    return html.Div("Unknown tab")


# ──────────────────────────────────────────────────────────────────────────────
# 11. FILTER RESET
# ──────────────────────────────────────────────────────────────────────────────

@app.callback(
    Output("filter-engine",     "value"),
    Output("filter-result",     "value"),
    Output("filter-opening",    "value"),
    Output("filter-side",       "value"),
    Output("filter-pos-type",   "value"),
    Output("filter-game-phase", "value"),
    Output("filter-include-mates", "value"),
    Input("btn-reset", "n_clicks"),
    prevent_initial_call=True,
)
def reset_filters(_):
    return None, None, None, None, None, None, "exclude"


# ──────────────────────────────────────────────────────────────────────────────
# 12. SEARCH SCATTER CALLBACK
# ──────────────────────────────────────────────────────────────────────────────

@app.callback(
    Output("srch-graph-container", "children"),
    Input("srch-x",         "value"),
    Input("srch-y",         "value"),
    Input("srch-color",     "value"),
    Input("filter-engine",  "value"),
    Input("filter-result",  "value"),
    Input("filter-opening", "value"),
    Input("filter-side",    "value"),
    Input("filter-pos-type","value"),
    Input("filter-game-phase","value"),
    Input("filter-include-mates","value"),
)
def update_search_scatter(x_col, y_col, color_col, engine_ids, result_vals, opening_vals, side_vals, pos_type_vals, game_phase_vals, include_mates_val):
    _, sf = apply_filters(engine_ids, result_vals, opening_vals, side_vals, pos_type_vals, game_phase_vals,
                          include_mates=(include_mates_val == "include"),
                          mates_only=(include_mates_val == "only"))
    if not x_col or not y_col:
        return html.Div("Select axes.", style={"color": TEXT_SEC})
    if x_col not in sf.columns or y_col not in sf.columns:
        return html.Div("Column not found.", style={"color": TEXT_SEC})

    plot_cols = list({x_col, y_col, color_col, "game_id", "engine_side", "depth"} & set(sf.columns))
    # include search_id for hover if present
    if "search_id" in sf.columns:
        plot_cols.append("search_id")
    data = sf[plot_cols].dropna(subset=[x_col, y_col]).head(10_000)

    if data.empty:
        return html.Div("No data for selected axes.", style={"color": TEXT_SEC})

    hover_cols = [c for c in ["search_id", "game_id", "engine_side", "depth"] if c in data.columns]
    fig = px.scatter(
        data, x=x_col, y=y_col,
        color=color_col if color_col and color_col in data.columns else None,
        hover_data=hover_cols,
        color_discrete_sequence=_PALETTE,
        opacity=0.6,
    )

    # Trend line per engine
    if color_col and color_col in data.columns:
        for grp_val, grp in data.groupby(color_col):
            if len(grp) < 5:
                continue
            try:
                coeffs = np.polyfit(grp[x_col].astype(float), grp[y_col].astype(float), 2)
                poly = np.poly1d(coeffs)
                xs = np.linspace(grp[x_col].min(), grp[x_col].max(), 100)
                fig.add_scatter(x=xs, y=poly(xs), mode="lines",
                                name=f"{grp_val} trend", line=dict(width=2, dash="dot"))
            except Exception:
                pass

    fig.update_layout(title=f"{metric_label(y_col)} vs {metric_label(x_col)}")
    apply_theme(fig)
    return graph(fig, 400, id="srch-scatter-plot")


# ──────────────────────────────────────────────────────────────────────────────
# 12b. FEN DETAIL CALLBACK (click on scatter point)
# ──────────────────────────────────────────────────────────────────────────────

@app.callback(
    Output("fen-detail-container", "children"),
    Input("srch-scatter-plot", "clickData"),
    State("filter-engine",  "value"),
    State("filter-result",  "value"),
    State("filter-opening", "value"),
    State("filter-side",    "value"),
    State("filter-pos-type","value"),
    State("filter-game-phase","value"),
    State("filter-include-mates","value"),
    prevent_initial_call=True,
)
def show_fen_detail(click_data, engine_ids, result_vals, opening_vals, side_vals, pos_type_vals, game_phase_vals, include_mates_val):
    """Show position details when a scatter point is clicked."""
    if not click_data:
        return html.Div()

    _, sf = apply_filters(engine_ids, result_vals, opening_vals, side_vals, pos_type_vals, game_phase_vals,
                          include_mates=(include_mates_val == "include"),
                          mates_only=(include_mates_val == "only"))

    point = click_data["points"][0]
    custom = point.get("customdata", [])
    # Try to find the search row by search_id or by x/y match
    row = None
    if custom and "search_id" in sf.columns:
        # hover_data includes search_id as first custom field
        sid = custom[0] if isinstance(custom, list) else custom
        matches = sf[sf["search_id"] == sid] if "search_id" in sf.columns else pd.DataFrame()
        if not matches.empty:
            row = matches.iloc[0]

    if row is None:
        return html.Div("Could not identify position from click.", style={"color": TEXT_SEC})

    # Build a detail panel
    fen = row.get("fen", "N/A")
    best_move = row.get("best_move", row.get("move", "N/A"))
    pv = row.get("principal_variation", "N/A")
    eval_val = row.get("eval", "N/A")
    depth_val = row.get("depth", "N/A")
    time_val = row.get("time_ms", row.get("total_time_ms", "N/A"))
    engine = row.get("engine_name", "N/A")
    sf_eval = row.get("sf_eval", None)
    sf_move = row.get("sf_best_move", None)

    detail_items = [
        ("Engine", engine),
        ("FEN", fen),
        ("Best Move", best_move),
        ("Eval (cp)", eval_val),
        ("Depth", depth_val),
        ("Time (ms)", time_val),
        ("PV", str(pv)[:100] if pv else "N/A"),
    ]
    if sf_eval is not None:
        detail_items.append(("SF Eval", sf_eval))
    if sf_move is not None:
        detail_items.append(("SF Best Move", sf_move))

    # Add key metrics
    for col in ["nps", "tt_hit_ratio", "fail_high_first_ratio", "nmp_ratio", "ebf"]:
        if col in row.index and pd.notna(row[col]):
            detail_items.append((metric_label(col), f"{row[col]:.4f}"))

    detail_grid = html.Div([
        html.Div([
            html.Span(label + ": ", style={"color": TEXT_SEC, "fontWeight": "700", "fontSize": "10px",
                                            "textTransform": "uppercase", "letterSpacing": "0.08em"}),
            html.Span(str(val), style={"color": TEXT_PRI, "fontFamily": "'JetBrains Mono'", "fontSize": "11px"}),
        ], style={"marginBottom": "6px"})
        for label, val in detail_items
    ])

    # Iteration history for this search (if available)
    iter_section = html.Div()
    search_id = row.get("search_id", row.get("id"))
    if search_id and not iter_df.empty and "search_id" in iter_df.columns:
        irows = iter_df[iter_df["search_id"] == search_id].sort_values("depth")
        if not irows.empty and "eval" in irows.columns:
            iter_fig = go.Figure()
            iter_fig.add_trace(go.Scatter(x=irows["depth"], y=irows["eval"],
                                          mode="lines+markers", name="Eval",
                                          line=dict(color=ACCENT)))
            if "nps" in irows.columns:
                iter_fig.add_trace(go.Bar(x=irows["depth"], y=irows["nps"],
                                          name="NPS", marker_color=hex_to_rgba(ACCENT2, 0.5),
                                          yaxis="y2"))
                iter_fig.update_layout(yaxis2=dict(overlaying="y", side="right", showgrid=False,
                                                    title="NPS", title_font=dict(color=ACCENT2)))
            iter_fig.update_layout(title="Iteration History for Selected Position",
                                    xaxis_title="Depth", yaxis_title="Eval")
            apply_theme(iter_fig)
            iter_section = html.Div([
                html.Div(style={"height": "8px"}),
                graph(iter_fig, 260),
            ])

    return panel(
        section("POSITION DETAIL"),
        detail_grid,
        iter_section,
    )


# ──────────────────────────────────────────────────────────────────────────────
# 13. ITER DEPTH CALLBACK
# ──────────────────────────────────────────────────────────────────────────────

@app.callback(
    Output("iter-graph-container", "children"),
    Input("iter-y",         "value"),
    Input("iter-agg",       "value"),
    Input("filter-engine",  "value"),
    Input("filter-result",  "value"),
    Input("filter-opening", "value"),
    Input("filter-side",    "value"),
    Input("filter-pos-type","value"),
    Input("filter-game-phase","value"),
    Input("filter-include-mates","value"),
)
def update_iter_graph(y_col, agg, engine_ids, result_vals, opening_vals, side_vals, pos_type_vals, game_phase_vals, include_mates_val):
    _, sf = apply_filters(engine_ids, result_vals, opening_vals, side_vals, pos_type_vals, game_phase_vals,
                          include_mates=(include_mates_val == "include"),
                          mates_only=(include_mates_val == "only"))
    if iter_df.empty or not y_col:
        return html.Div("No data.", style={"color": TEXT_SEC})

    valid_ids = sf["id"].unique() if "id" in sf.columns else []
    idf = iter_df[iter_df["search_id"].isin(valid_ids)].copy() if len(valid_ids) else iter_df.copy()

    if y_col not in idf.columns or "depth" not in idf.columns:
        return html.Div("Column not found.", style={"color": TEXT_SEC})

    agg_fn = {"mean": "mean", "median": "median", "std": "std"}.get(agg, "mean")
    group_by = ["depth", "engine_name"] if "engine_name" in idf.columns else ["depth"]
    agg_df = idf.groupby(group_by)[y_col].agg(agg_fn).reset_index()

    color_col = "engine_name" if "engine_name" in agg_df.columns else None
    fig = px.line(agg_df, x="depth", y=y_col, color=color_col,
                  color_discrete_sequence=_PALETTE,
                  markers=True,
                  labels={"depth": "Iteration Depth", y_col: f"{agg}({metric_label(y_col)})"})

    # Add p10/p90 percentile bands
    if agg in ("mean", "median"):
        band_group = ["depth"] if color_col is None else ["depth", "engine_name"]
        p10 = idf.groupby(band_group)[y_col].quantile(0.1).reset_index().rename(columns={y_col: "p10"})
        p90 = idf.groupby(band_group)[y_col].quantile(0.9).reset_index().rename(columns={y_col: "p90"})
        bands = p10.merge(p90, on=band_group)
        if color_col is None:
            fig.add_trace(go.Scatter(x=bands["depth"], y=bands["p90"], mode="lines",
                                     line=dict(width=0), showlegend=False, hoverinfo="skip"))
            fig.add_trace(go.Scatter(x=bands["depth"], y=bands["p10"], mode="lines",
                                     line=dict(width=0), fill="tonexty",
                                     fillcolor=hex_to_rgba(ACCENT, 0.15),
                                     showlegend=True, name="p10–p90"))
        else:
            for i, eng in enumerate(bands[color_col].unique()):
                eb = bands[bands[color_col] == eng]
                col = _PALETTE[i % len(_PALETTE)]
                fig.add_trace(go.Scatter(x=eb["depth"], y=eb["p90"], mode="lines",
                                         line=dict(width=0), showlegend=False, hoverinfo="skip"))
                fig.add_trace(go.Scatter(x=eb["depth"], y=eb["p10"], mode="lines",
                                         line=dict(width=0), fill="tonexty",
                                         fillcolor=hex_to_rgba(col, 0.1),
                                         showlegend=False, hoverinfo="skip"))

    fig.update_layout(title=f"{agg}({metric_label(y_col)}) by Iteration Depth")
    apply_theme(fig)
    return graph(fig, 380)


# ──────────────────────────────────────────────────────────────────────────────
# 14. TREE DEPTH CALLBACK
# ──────────────────────────────────────────────────────────────────────────────

@app.callback(
    Output("tree-graph-container", "children"),
    Input("tree-y",         "value"),
    Input("tree-scale",     "value"),
    Input("filter-engine",  "value"),
    Input("filter-result",  "value"),
    Input("filter-opening", "value"),
    Input("filter-side",    "value"),
    Input("filter-pos-type","value"),
    Input("filter-game-phase","value"),
    Input("filter-include-mates","value"),
)
def update_tree_graph(y_col, scale, engine_ids, result_vals, opening_vals, side_vals, pos_type_vals, game_phase_vals, include_mates_val):
    _, sf = apply_filters(engine_ids, result_vals, opening_vals, side_vals, pos_type_vals, game_phase_vals,
                          include_mates=(include_mates_val == "include"),
                          mates_only=(include_mates_val == "only"))
    if tree_df.empty or not y_col:
        return html.Div("No data.", style={"color": TEXT_SEC})

    valid_ids = sf["id"].unique() if "id" in sf.columns else []
    tdf = tree_df[tree_df["search_id"].isin(valid_ids)].copy() if len(valid_ids) else tree_df.copy()

    if y_col not in tdf.columns or "depth" not in tdf.columns:
        return html.Div("Column not found.", style={"color": TEXT_SEC})

    group_by = ["depth", "engine_name"] if "engine_name" in tdf.columns else ["depth"]
    agg_df = tdf.groupby(group_by)[y_col].mean().reset_index()

    color_col = "engine_name" if "engine_name" in agg_df.columns else None
    fig = px.line(agg_df, x="depth", y=y_col, color=color_col,
                  color_discrete_sequence=_PALETTE, markers=True,
                  log_y=(scale == "log"),
                  labels={"depth": "Tree Ply", y_col: f"mean({metric_label(y_col)})"})

    # Add p10/p90 percentile bands
    band_group = ["depth"] if color_col is None else ["depth", "engine_name"]
    p10 = tdf.groupby(band_group)[y_col].quantile(0.1).reset_index().rename(columns={y_col: "p10"})
    p90 = tdf.groupby(band_group)[y_col].quantile(0.9).reset_index().rename(columns={y_col: "p90"})
    bands = p10.merge(p90, on=band_group)
    if color_col is None:
        fig.add_trace(go.Scatter(x=bands["depth"], y=bands["p90"], mode="lines",
                                 line=dict(width=0), showlegend=False, hoverinfo="skip"))
        fig.add_trace(go.Scatter(x=bands["depth"], y=bands["p10"], mode="lines",
                                 line=dict(width=0), fill="tonexty",
                                 fillcolor=hex_to_rgba(ACCENT, 0.15),
                                 showlegend=True, name="p10–p90"))
    else:
        for i, eng in enumerate(bands[color_col].unique()):
            eb = bands[bands[color_col] == eng]
            col = _PALETTE[i % len(_PALETTE)]
            fig.add_trace(go.Scatter(x=eb["depth"], y=eb["p90"], mode="lines",
                                     line=dict(width=0), showlegend=False, hoverinfo="skip"))
            fig.add_trace(go.Scatter(x=eb["depth"], y=eb["p10"], mode="lines",
                                     line=dict(width=0), fill="tonexty",
                                     fillcolor=hex_to_rgba(col, 0.1),
                                     showlegend=False, hoverinfo="skip"))

    fig.update_layout(title=f"mean({metric_label(y_col)}) by Tree Ply")
    apply_theme(fig)
    return graph(fig, 380)


# Per-game progress plot: eval (primary) and nodes/time (secondary)
@app.callback(
    Output("game-progress-container", "children"),
    Input("game-select", "value"),
    Input("filter-engine",  "value"),
    Input("filter-result",  "value"),
    Input("filter-opening", "value"),
    Input("filter-side",    "value"),
    Input("filter-pos-type","value"),
    Input("filter-game-phase","value"),
    Input("filter-include-mates","value"),
)
def update_game_progress(game_id, engine_ids, result_vals, opening_vals, side_vals, pos_type_vals, game_phase_vals, include_mates_val):
    if not game_id:
        return html.Div("Select a game to view progress.", style={"color": TEXT_SEC})

    gf, sf = apply_filters(engine_ids, result_vals, opening_vals, side_vals, pos_type_vals, game_phase_vals,
                           include_mates=(include_mates_val == "include"),
                           mates_only=(include_mates_val == "only"))
    if sf.empty or "game_id" not in sf.columns:
        return html.Div("No per-ply search data available.", style={"color": TEXT_SEC})

    gsearch = sf[sf["game_id"] == game_id].copy()
    if gsearch.empty:
        return html.Div("No search rows for selected game.", style={"color": TEXT_SEC})

    # Determine x-axis (ply). Prefer `ply` column otherwise use incremental index.
    if "ply" in gsearch.columns:
        gsearch = gsearch.sort_values("ply")
        x = gsearch["ply"].tolist()
    else:
        gsearch = gsearch.sort_index()
        x = list(range(1, len(gsearch) + 1))

    fig = make_subplots(specs=[[{"secondary_y": True}]])

    if "eval" in gsearch.columns:
        fig.add_trace(go.Scatter(x=x, y=gsearch["eval"],
                                 mode="lines+markers", name="Eval",
                                 line=dict(color=ACCENT)), secondary_y=False)

    # Add nodes/time on secondary axis if present
    if "nodes" in gsearch.columns:
        fig.add_trace(go.Bar(x=x, y=gsearch["nodes"], name="Nodes", marker_color=hex_to_rgba(ACCENT2, 0.6)), secondary_y=True)
    elif "time_ms" in gsearch.columns:
        fig.add_trace(go.Bar(x=x, y=gsearch["time_ms"], name="Time (ms)", marker_color=hex_to_rgba(ACCENT2, 0.6)), secondary_y=True)

    fig.update_xaxes(title_text="Ply")
    fig.update_yaxes(title_text="Eval", secondary_y=False)
    fig.update_yaxes(title_text="Nodes / Time", secondary_y=True)
    fig.update_layout(title=f"Game {game_id} Progress: Eval vs Ply", legend=dict(orientation="h", y=-0.15))
    apply_theme(fig)
    return graph(fig, 360)


# ──────────────────────────────────────────────────────────────────────────────
# 15. COMPARE CALLBACKS
# ──────────────────────────────────────────────────────────────────────────────

@app.callback(
    Output("cmp-graph-container",   "children"),
    Output("cmp-summary-container", "children"),
    Input("cmp-eng-a",      "value"),
    Input("cmp-eng-b",      "value"),
    Input("cmp-metric",     "value"),
    Input("cmp-chart",      "value"),
    Input("filter-engine",  "value"),
    Input("filter-result",  "value"),
    Input("filter-opening", "value"),
    Input("filter-side",    "value"),
    Input("filter-pos-type","value"),
    Input("filter-game-phase","value"),
    Input("filter-include-mates","value"),
)
def update_compare(eng_a, eng_b, metric, chart_type, engine_ids, result_vals, opening_vals, side_vals, pos_type_vals, game_phase_vals, include_mates_val):
    _, sf = apply_filters(engine_ids, result_vals, opening_vals, side_vals, pos_type_vals, game_phase_vals,
                          include_mates=(include_mates_val == "include"),
                          mates_only=(include_mates_val == "only"))
    if "engine_label" not in sf.columns or not metric:
        return html.Div("No data.", style={"color": TEXT_SEC}), html.Div()

    pair = sf[sf["engine_label"].isin([eng_a, eng_b])].dropna(subset=[metric, "engine_label"])

    a_data = pair[pair["engine_label"] == eng_a][metric]
    b_data = pair[pair["engine_label"] == eng_b][metric]

    if pair.empty or a_data.empty:
        return html.Div("No overlapping data.", style={"color": TEXT_SEC}), html.Div()

    cmap = {eng_a: ACCENT, eng_b: ACCENT2}

    if chart_type == "Box":
        fig = px.box(pair, x="engine_name", y=metric, color="engine_name",
                     color_discrete_map=cmap, points="outliers")
    elif chart_type == "Violin":
        fig = go.Figure()
        for eng, col in cmap.items():
            grp = pair[pair["engine_name"] == eng]
            fig.add_trace(go.Violin(y=grp[metric], name=eng, box_visible=True,
                                    meanline_visible=True, fillcolor=col + "55", line_color=col))
    elif chart_type == "Histogram":
        fig = px.histogram(pair, x=metric, color="engine_name", barmode="overlay",
                           color_discrete_map=cmap, nbins=50, opacity=0.7)
    elif chart_type == "Delta (A-B)":
        # Align by FEN if possible
        if "fen" in sf.columns:
            merged = pd.merge(
                pair[pair["engine_label"] == eng_a][["fen", metric]].rename(columns={metric: "val_a"}),
                pair[pair["engine_label"] == eng_b][["fen", metric]].rename(columns={metric: "val_b"}),
                on="fen"
            )
            merged["delta"] = merged["val_a"] - merged["val_b"]
            fig = px.histogram(merged, x="delta", nbins=60, color_discrete_sequence=[ACCENT],
                               labels={"delta": f"{metric}: {eng_a} − {eng_b}"})
            fig.add_vline(x=0, line_dash="dot", line_color=TEXT_SEC)
        else:
            return html.Div("Delta chart requires FEN column.", style={"color": TEXT_SEC}), html.Div()
    else:
        fig = go.Figure()

    fig.update_layout(title=f"{metric_label(metric)} — {eng_a} vs {eng_b}")
    apply_theme(fig)

    # Build main graph area: primary chart + ECDF + KS-test summary
    main_graph_children = [graph(fig, 380)]

    # ECDF (CDF) to visualize distributional differences
    try:
        ecdf_df = pair[[metric, "engine_label"]].dropna()
        if not ecdf_df.empty:
            cdf_fig = px.ecdf(ecdf_df, x=metric, color="engine_label")
            cdf_fig.update_layout(title=f"CDF: {metric_label(metric)} distribution")
            apply_theme(cdf_fig)
            main_graph_children.append(graph(cdf_fig, 300))
    except Exception:
        pass

    # KS two-sample test (if scipy available)
    ks_text = None
    try:
        from scipy.stats import ks_2samp
        a_vals = a_data.dropna()
        b_vals = b_data.dropna()
        if len(a_vals) and len(b_vals):
            ks_stat, ks_p = ks_2samp(a_vals, b_vals)
            ks_text = f"KS stat={ks_stat:.4f}, p={ks_p:.3g}"
    except Exception:
        ks_text = None

    if ks_text:
        main_graph_children.append(html.Div(ks_text, style={"color": TEXT_SEC, "marginTop": "8px"}))

    main_graph = html.Div(main_graph_children, style={"display": "flex", "flexDirection": "column", "gap": "12px"})

    # Summary stats table
    rows = []
    for eng, grp in [(eng_a, a_data), (eng_b, b_data)]:
        rows.append({
            "Engine": eng,
            "N": len(grp),
            "Mean": round(grp.mean(), 4),
            "Median": round(grp.median(), 4),
            "Std": round(grp.std(), 4),
            "Min": round(grp.min(), 4),
            "Max": round(grp.max(), 4),
        })
    summary_tbl = table(pd.DataFrame(rows), page_size=5)

    # --- Per-game aligned deltas (by metric) ---
    per_game_delta_block = html.Div()
    if "game_id" in pair.columns:
        a_game = pair[pair["engine_label"] == eng_a].groupby("game_id")[metric].mean().rename("val_a")
        b_game = pair[pair["engine_label"] == eng_b].groupby("game_id")[metric].mean().rename("val_b")
        merged_games = pd.merge(a_game.reset_index(), b_game.reset_index(), on="game_id", how="inner")
        if not merged_games.empty:
            merged_games["delta"] = merged_games["val_a"] - merged_games["val_b"]
            top_games = merged_games.reindex(merged_games["delta"].abs().sort_values(ascending=False).index).head(10)
            per_game_delta_block = html.Div([
                section(f"TOP 10 GAMES BY |Δ {metric}|"),
                table(top_games.rename(columns={"val_a": f"{eng_a}", "val_b": f"{eng_b}"}))
            ], style={"marginTop": "16px"})


    # Multi-metric radar / bar summary across all engines
    all_engs = sf["engine_label"].dropna().unique()
    key_metrics = [m for m in ["eval", "nodes", "depth", "time_ms", "tt_hits", "fail_highs", "fail_lows"]
                   if m in sf.columns]
    if key_metrics and len(all_engs) >= 1:
        agg_all = sf.groupby("engine_label")[key_metrics].mean().reset_index()
        agg_norm = agg_all.copy()
        for m in key_metrics:
            rng = agg_norm[m].max() - agg_norm[m].min()
            agg_norm[m] = (agg_norm[m] - agg_norm[m].min()) / rng if rng else 0
        agg_melt = agg_norm.melt(id_vars="engine_label", var_name="metric", value_name="norm_value")
        radar_fig = px.bar(agg_melt, x="metric", y="norm_value", color="engine_label",
                           barmode="group", color_discrete_sequence=_PALETTE,
                           labels={"norm_value": "Normalised value", "metric": "Metric", "engine_label": "Engine"})
        radar_fig.update_layout(title="Normalised Metrics Across All Engines")
        apply_theme(radar_fig)
        multi_section = panel(graph(radar_fig, 320))
    else:
        multi_section = html.Div()

    summary_block = html.Div([
        panel(section("STAT SUMMARY"), summary_tbl),
        html.Div(style={"height": "12px"}),
        multi_section,
    ])

    return html.Div([
        panel(main_graph),
    ]), summary_block


@app.callback(
    Output("cmp-export-link", "children"),
    Input("cmp-export-report", "n_clicks"),
    State("cmp-eng-a", "value"),
    State("cmp-eng-b", "value"),
    State("cmp-metric", "value"),
    State("filter-engine", "value"),
    State("filter-result", "value"),
    State("filter-opening", "value"),
    State("filter-side", "value"),
    State("filter-pos-type","value"),
    State("filter-game-phase","value"),
    State("filter-include-mates","value"),
)
def export_compare(rpt_clicks, eng_a, eng_b, metric, engine_ids, result_vals, opening_vals, side_vals, pos_type_vals, game_phase_vals, include_mates_val):
    """Generate a markdown report for the current comparison selection.
    Writes output to `data/exports/` and returns an HTML link to the file.
    """
    import time, pathlib
    gf, sf = apply_filters(engine_ids, result_vals, opening_vals, side_vals, pos_type_vals, game_phase_vals,
                           include_mates=(include_mates_val == "include"),
                           mates_only=(include_mates_val == "only"))
    if not eng_a or not eng_b or not rpt_clicks:
        return ""
    ts = int(time.time())
    out_dir = pathlib.Path(__file__).resolve().parent / "exports"
    out_dir.mkdir(exist_ok=True)

    pair = sf[sf["engine_label"].isin([eng_a, eng_b])]
    md = []
    md.append(f"# Comparison report: {eng_a} vs {eng_b}")
    md.append(f"Metric: {metric}")
    for eng_label in (eng_a, eng_b):
        grp = pair[pair["engine_label"] == eng_label]
        md.append(f"## {eng_label}")
        md.append(f"Rows: {len(grp)}")
        if metric in grp.columns and len(grp):
            md.append(f"Mean {metric}: {grp[metric].mean():.4f}")
            md.append(f"Median {metric}: {grp[metric].median():.4f}")
    out = out_dir / f"report_{eng_a.replace(' ','_')}_vs_{eng_b.replace(' ','_')}_{ts}.md"
    out.write_text('\n\n'.join(md))
    return html.A(f"Download report: {out.name}", href=str(out), target="_blank")


# ──────────────────────────────────────────────────────────────────────────────
# 16. ENTRY POINT
# ──────────────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    webbrowser.open("http://127.0.0.1:8050")
    app.run(debug=True, port=8050)