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

def numeric_cols(df: pd.DataFrame) -> list[str]:
    return [c for c in df.select_dtypes(include=np.number).columns if c not in NUMERIC_EXCLUDE]


def apply_filters(
    engine_ids: list | None,
    result_vals: list | None,
    opening_vals: list | None,
    side_vals: list | None,
    pos_type_vals: list | None,
) -> tuple[pd.DataFrame, pd.DataFrame]:
    gf = games_df.copy()
    sf = searches_df.copy()

    # Sanity limits
    sf = sf[sf["depth"].between(0, 60) & sf["eval"].abs().le(30000)] if "depth" in sf.columns and "eval" in sf.columns else sf

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

MAX_TABLE_ROWS = 5000


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

            html.Hr(style={"borderColor": BORDER, "margin": "16px 0", "opacity": "0.5"}),

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
    ("tab-games",      "GAMES"),
    ("tab-search",     "SEARCH"),
    ("tab-iter",       "ITER DEPTH"),
    ("tab-tree",       "TREE DEPTH"),
    ("tab-compare",    "COMPARE"),
    ("tab-timing",     "TIMING"),
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
                style={"borderBottom": f"1px solid {BORDER}"},
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


def metric_card(value, label: str, accent=ACCENT) -> html.Div:
    return html.Div([
        html.Div(str(value), className="metric-val", style={"color": accent}),
        html.Div(label, className="metric-lbl"),
    ], className="metric-card")


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

    kpis = html.Div([
        metric_card(f"{total_games:,}",    "TOTAL GAMES"),
        metric_card(f"{total_searches:,}", "TOTAL SEARCHES", accent=ACCENT2),
        metric_card(engine_count,          "ENGINE VERSIONS", accent="#7fff6b"),
        metric_card(avg_depth,             "AVG SEARCH DEPTH", accent="#f7b731"),
    ], style={"display": "grid", "gridTemplateColumns": "repeat(4,1fr)", "gap": "10px", "marginBottom": "10px"})

    # Second KPI row: pruning / NMP efficiency
    avg_nmp_ratio = round(sf["nmp_ratio"].mean() * 100, 2) if "nmp_ratio" in sf.columns and not sf.empty else "—"
    avg_nmp_fail = round(sf["nmp_fail_ratio"].mean() * 100, 1) if "nmp_fail_ratio" in sf.columns and not sf.empty else "—"
    avg_fh_first = round(sf["fail_high_first_ratio"].mean() * 100, 1) if "fail_high_first_ratio" in sf.columns and not sf.empty else "—"
    avg_tt_hit = round(sf["tt_hit_ratio"].mean() * 100, 1) if "tt_hit_ratio" in sf.columns and not sf.empty else "—"

    kpis2 = html.Div([
        metric_card(f"{avg_nmp_ratio}%", "NMP ATTEMPT RATE", accent="#a29bfe"),
        metric_card(f"{avg_nmp_fail}%", "NMP FAIL RATE", accent="#fd79a8"),
        metric_card(f"{avg_fh_first}%", "FAIL-HIGH FIRST", accent="#55efc4"),
        metric_card(f"{avg_tt_hit}%", "TT HIT RATE", accent="#fdcb6e"),
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

    return html.Div([
        kpis,
        kpis2,
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
            dcc.Dropdown(id="srch-x", options=[{"label": c, "value": c} for c in nums],
                         value=x_default, clearable=False, style=_dropdown_style),
        ], style={"flex": "1"}),
        html.Div([
            html.Div("Y AXIS", style=_sidebar_label),
            dcc.Dropdown(id="srch-y", options=[{"label": c, "value": c} for c in nums],
                         value=y_default, clearable=False, style=_dropdown_style),
        ], style={"flex": "1"}),
        html.Div([
            html.Div("COLOUR BY", style=_sidebar_label),
            # Include categorical defaults plus numeric axis columns so users can
            # colour by any axis (e.g., eval_diff by depth with see_prune colouring)
            dcc.Dropdown(id="srch-color",
                         options=[{"label": c, "value": c} for c in (
                             [c for c in ["engine_name", "result_label", "engine_side"] if c in sf.columns]
                             + [c for c in nums if c in sf.columns]
                         )],
                         value=("engine_name" if "engine_name" in sf.columns else (nums[0] if nums else None)),
                         clearable=True, style=_dropdown_style),
        ], style={"flex": "1"}),
    ], style={"display": "flex", "gap": "16px", "marginBottom": "16px"})

    return html.Div([
        panel(
            section("SEARCH SCATTER"),
            axis_row,
            html.Div(id="srch-graph-container"),
        ),
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
    y_opts = [{"label": c, "value": c} for c in nums]

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
        panel(section("RAW ITERATION DATA"), table(idf)),
    ])


def tab_tree(sf: pd.DataFrame) -> html.Div:
    """Per-tree-ply view: how do stats distribute across plies in the search tree?"""
    if tree_df.empty:
        return html.Div("No searches_by_tree_depth data found.", style={"color": TEXT_SEC})

    valid_ids = sf["id"].unique() if "id" in sf.columns else []
    tdf = tree_df[tree_df["search_id"].isin(valid_ids)].copy() if len(valid_ids) else tree_df.copy()

    nums = numeric_cols(tdf)
    y_opts = [{"label": c, "value": c} for c in nums]

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
    metric_opts = [{"label": c, "value": c} for c in nums]

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
                html.Button("Export Anomalies CSV", id="cmp-export-anomalies", n_clicks=0,
                            style={"padding": "6px 12px", "background": "transparent", "border": f"1px solid {BORDER}",
                                   "borderRadius": "5px", "color": TEXT_SEC, "fontSize": "10px", "fontFamily": "'JetBrains Mono'",
                                   "cursor": "pointer", "marginRight": "8px"}),
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
# 10. MAIN TAB CALLBACK
# ──────────────────────────────────────────────────────────────────────────────

@app.callback(
    Output("tab-content", "children"),
    Input("main-tabs",      "value"),
    Input("filter-engine",  "value"),
    Input("filter-result",  "value"),
    Input("filter-opening", "value"),
    Input("filter-side",    "value"),
    Input("filter-pos-type","value"),
)
def render_tab(tab, engine_ids, result_vals, opening_vals, side_vals, pos_type_vals):
    gf, sf = apply_filters(engine_ids, result_vals, opening_vals, side_vals, pos_type_vals)

    if tab == "tab-overview": return tab_overview(gf, sf)
    if tab == "tab-games":    return tab_games(gf)
    if tab == "tab-search":   return tab_search(sf)
    if tab == "tab-iter":     return tab_iter(sf)
    if tab == "tab-tree":     return tab_tree(sf)
    if tab == "tab-compare":  return tab_compare(sf)
    if tab == "tab-timing":   return tab_timing()
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
    Output("filter-engine",   "value"),
    Output("filter-result",   "value"),
    Output("filter-opening",  "value"),
    Output("filter-side",     "value"),
    Output("filter-pos-type", "value"),
    Input("btn-reset", "n_clicks"),
    prevent_initial_call=True,
)
def reset_filters(_):
    return None, None, None, None, None


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
)
def update_search_scatter(x_col, y_col, color_col, engine_ids, result_vals, opening_vals, side_vals, pos_type_vals):
    _, sf = apply_filters(engine_ids, result_vals, opening_vals, side_vals, pos_type_vals)
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

    fig.update_layout(title=f"{y_col} vs {x_col}")
    apply_theme(fig)
    return graph(fig, 480)


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
)
def update_iter_graph(y_col, agg, engine_ids, result_vals, opening_vals, side_vals, pos_type_vals):
    _, sf = apply_filters(engine_ids, result_vals, opening_vals, side_vals, pos_type_vals)
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
                  labels={"depth": "Iteration Depth", y_col: f"{agg}({y_col})"})
    fig.update_layout(title=f"{agg}({y_col}) by Iteration Depth")
    apply_theme(fig)
    return graph(fig, 440)


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
)
def update_tree_graph(y_col, scale, engine_ids, result_vals, opening_vals, side_vals, pos_type_vals):
    _, sf = apply_filters(engine_ids, result_vals, opening_vals, side_vals, pos_type_vals)
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
                  labels={"depth": "Tree Ply", y_col: f"mean({y_col})"})
    fig.update_layout(title=f"mean({y_col}) by Tree Ply")
    apply_theme(fig)
    return graph(fig, 440)


# Per-game progress plot: eval (primary) and nodes/time (secondary)
@app.callback(
    Output("game-progress-container", "children"),
    Input("game-select", "value"),
    Input("filter-engine",  "value"),
    Input("filter-result",  "value"),
    Input("filter-opening", "value"),
    Input("filter-side",    "value"),
    Input("filter-pos-type","value"),
)
def update_game_progress(game_id, engine_ids, result_vals, opening_vals, side_vals, pos_type_vals):
    if not game_id:
        return html.Div("Select a game to view progress.", style={"color": TEXT_SEC})

    gf, sf = apply_filters(engine_ids, result_vals, opening_vals, side_vals, pos_type_vals)
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
        fig.add_trace(go.Scatter(x=x, y=gsearch["eval"], mode="lines+markers", name="Eval",
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
    return graph(fig, 420)


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
)
def update_compare(eng_a, eng_b, metric, chart_type, engine_ids, result_vals, opening_vals, side_vals, pos_type_vals):
    _, sf = apply_filters(engine_ids, result_vals, opening_vals, side_vals, pos_type_vals)
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

    fig.update_layout(title=f"{metric} — {eng_a} vs {eng_b}")
    apply_theme(fig)

    # Build main graph area: primary chart + ECDF + KS-test summary
    main_graph_children = [graph(fig, 440)]

    # ECDF (CDF) to visualize distributional differences
    try:
        ecdf_df = pair[[metric, "engine_label"]].dropna()
        if not ecdf_df.empty:
            cdf_fig = px.ecdf(ecdf_df, x=metric, color="engine_label")
            cdf_fig.update_layout(title=f"CDF: {metric} distribution")
            apply_theme(cdf_fig)
            main_graph_children.append(graph(cdf_fig, 360))
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

    # --- Anomalies / basic checks ---
    anomalies = []
    for eng_label in (eng_a, eng_b):
        grp = pair[pair["engine_label"] == eng_label]
        if grp.empty:
            anomalies.append(f"{eng_label}: no matching rows")
            continue
        # check for fail_highs/fail_lows presence and counts
        for col in ("fail_highs", "fail_lows"):
            if col in grp.columns:
                total = int(grp[col].sum()) if not grp[col].isna().all() else 0
                zeros = int((grp[col] == 0).sum())
                pct_zero = round(zeros / len(grp) * 100, 1) if len(grp) else 0
                if total == 0:
                    anomalies.append(f"{eng_label}: {col} = 0 across all rows")
                elif pct_zero > 90:
                    anomalies.append(f"{eng_label}: {pct_zero}% rows have {col} == 0")

    anomalies_block = panel(
        section("ANOMALIES & CHECKS"),
        html.Div([html.Div(a, style={"color": ("#ff6b35" if "= 0" in a else TEXT_SEC), "fontSize": "11px",
                                      "fontFamily": "'JetBrains Mono'", "marginBottom": "4px"}) for a in anomalies])
    )

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
    Input("cmp-export-anomalies", "n_clicks"),
    Input("cmp-export-report", "n_clicks"),
    State("cmp-eng-a", "value"),
    State("cmp-eng-b", "value"),
    State("cmp-metric", "value"),
    State("filter-engine", "value"),
    State("filter-result", "value"),
    State("filter-opening", "value"),
    State("filter-side", "value"),
    State("filter-pos-type","value"),
)
def export_compare(anom_clicks, rpt_clicks, eng_a, eng_b, metric, engine_ids, result_vals, opening_vals, side_vals, pos_type_vals):
    """Generate anomalies CSV or a markdown report for the current comparison selection.
    Writes outputs to `data/exports/` and returns an HTML link to the file.
    """
    import time, pathlib
    gf, sf = apply_filters(engine_ids, result_vals, opening_vals, side_vals, pos_type_vals)
    if not eng_a or not eng_b:
        return ""
    ts = int(time.time())
    out_dir = pathlib.Path(__file__).resolve().parent / "exports"
    out_dir.mkdir(exist_ok=True)

    # If anomalies button clicked
    ctx_id = ctx.triggered_id if hasattr(ctx, 'triggered_id') else None
    try:
        ctx_id = ctx.triggered_id
    except Exception:
        ctx_id = None

    if ctx_id == "cmp-export-anomalies":
        pair = sf[sf["engine_label"].isin([eng_a, eng_b])]
        rows = []
        for eng_label in (eng_a, eng_b):
            grp = pair[pair["engine_label"] == eng_label]
            if grp.empty:
                continue
            total_rows = len(grp)
            fail_highs = int(grp["fail_highs"].sum()) if "fail_highs" in grp.columns else 0
            fail_lows = int(grp["fail_lows"].sum()) if "fail_lows" in grp.columns else 0
            rows.append({"engine": eng_label, "rows": total_rows, "fail_highs": fail_highs, "fail_lows": fail_lows})
        import csv
        out = out_dir / f"anomalies_{eng_a.replace(' ','_')}_vs_{eng_b.replace(' ','_')}_{ts}.csv"
        with out.open('w', newline='') as fh:
            writer = csv.DictWriter(fh, fieldnames=["engine","rows","fail_highs","fail_lows"])
            writer.writeheader()
            for r in rows:
                writer.writerow(r)
        return html.A(f"Download anomalies CSV: {out.name}", href=str(out), target="_blank")

    if ctx_id == "cmp-export-report":
        pair = sf[sf["engine_label"].isin([eng_a, eng_b])]
        # basic summary in Markdown
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

    return ""


# ──────────────────────────────────────────────────────────────────────────────
# 16. ENTRY POINT
# ──────────────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    webbrowser.open("http://127.0.0.1:8050")
    app.run(debug=True, port=8050)