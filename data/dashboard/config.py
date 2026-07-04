"""
Configuration constants: colors, theme, DB path, metric labels, ordering.
"""
from pathlib import Path
import importlib.util

import numpy as np

#from ..etl.paths import ANALYTICS_DB
# import analytics_db directly from etl.paths.py without trigger  etl/__init__.py
# (pulls in heavy deps like python-chess that the dashboard doesnt need)
_paths_file = Path(__file__).parent.parent / "etl" / "paths.py"
_spec = importlib.util.spec_from_file_location("etl.paths", _paths_file)
_paths_module = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_paths_module)
ANALYTICS_DB = _paths_module.ANALYTICS_DB

# ──────────────────────────────────────────────────────────────────────────────
# COLORS
# ──────────────────────────────────────────────────────────────────────────────

DARK_BG   = "#0d0f14"
PANEL_BG  = "#141720"
BORDER    = "#252a38"
TEXT_PRI  = "#e8eaf0"
TEXT_SEC  = "#8892a4"
ACCENT    = "#00d2ff"
ACCENT2   = "#ff6b35"

_PALETTE = [
    "#00d2ff", "#ff6b35", "#7fff6b", "#ff3cac", "#f7b731",
    "#a29bfe", "#fd79a8", "#55efc4", "#fdcb6e", "#e17055",
]

# ──────────────────────────────────────────────────────────────────────────────
# LAYOUT STYLES
# ──────────────────────────────────────────────────────────────────────────────

SIDEBAR_LABEL = {
    "fontSize": "9px",
    "fontWeight": "700",
    "letterSpacing": "0.14em",
    "textTransform": "uppercase",
    "color": TEXT_SEC,
    "marginBottom": "3px",
    "marginTop": "10px",
}

DROPDOWN_STYLE = {
    "backgroundColor": "#1a1f2e",
    "borderColor": BORDER,
    "color": TEXT_PRI,
    "fontSize": "12px",
    "borderRadius": "6px",
}

TAB_STYLE = {
    "fontFamily": "'JetBrains Mono', monospace",
    "fontSize": "10px",
    "fontWeight": "700",
    "letterSpacing": "0.08em",
    "padding": "8px 12px",
    "backgroundColor": "transparent",
    "color": TEXT_SEC,
    "border": "none",
    "borderBottom": "2px solid transparent",
}

TAB_SELECTED_STYLE = {
    **TAB_STYLE,
    "color": ACCENT,
    "borderBottom": f"2px solid {ACCENT}",
    "backgroundColor": "transparent",
}

# ──────────────────────────────────────────────────────────────────────────────
# PLOTLY THEME
# ──────────────────────────────────────────────────────────────────────────────

PLOTLY_THEME = dict(
    template="plotly_dark",
    paper_bgcolor="rgba(0,0,0,0)",
    plot_bgcolor="rgba(20,23,32,0.6)",
    font=dict(family="JetBrains Mono, monospace", color=TEXT_PRI, size=11),
    legend=dict(bgcolor="rgba(0,0,0,0)", bordercolor="rgba(0,0,0,0)", borderwidth=0,
                font=dict(size=10), orientation="h", yanchor="bottom", y=-0.2, xanchor="center", x=0.5),
    margin=dict(l=44, r=16, t=36, b=32),
    colorway=_PALETTE,
)

# ──────────────────────────────────────────────────────────────────────────────
# DATABASE
# ──────────────────────────────────────────────────────────────────────────────

DB_PATH = str(ANALYTICS_DB)

# ──────────────────────────────────────────────────────────────────────────────
# THRESHOLDS
# ──────────────────────────────────────────────────────────────────────────────

EVAL_CLIP_CP = 1500
MAX_TABLE_ROWS = 5000

# ──────────────────────────────────────────────────────────────────────────────
# NUMERIC EXCLUSIONS
# ──────────────────────────────────────────────────────────────────────────────

NUMERIC_EXCLUDE = {
    "id", "engine_id", "game_id", "sts_id", "search_id",
    "white_engine_id", "black_engine_id", "experiment_id",
    "baseline_engine_id", "candidate_engine_id",
}

# ──────────────────────────────────────────────────────────────────────────────
# METRIC LABELS + ORDERING
# ──────────────────────────────────────────────────────────────────────────────

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


# ──────────────────────────────────────────────────────────────────────────────
# HELPER FUNCTIONS
# ──────────────────────────────────────────────────────────────────────────────

def numeric_cols(df) -> list[str]:
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
    return hex_color
