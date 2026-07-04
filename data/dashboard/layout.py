"""
Dashboard layout: sidebar with data freshness stats, tab structure, dcc.Loading.
"""
from dash import dcc, html

from .app import app
from .config import (
    DARK_BG, PANEL_BG, BORDER, TEXT_PRI, TEXT_SEC, ACCENT,
    SIDEBAR_LABEL, DROPDOWN_STYLE, TAB_STYLE, TAB_SELECTED_STYLE,
)
from .data_loader import (
    DATA_STATS, engine_options, result_options, opening_options,
    side_options, pos_type_options, game_phase_options,
)


# ──────────────────────────────────────────────────────────────────────────────
# TABS
# ──────────────────────────────────────────────────────────────────────────────

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
    ("tab-root-moves", "ROOT MOVES"),
    ("tab-time-mgmt",  "TIME MGMT"),
    ("tab-tt",         "TT ANALYSIS"),
    ("tab-calibration","CALIBRATION"),
    ("tab-stability",  "STABILITY"),
    ("tab-ordering",   "MOVE ORDER"),
    ("tab-pruning",    "PRUNING"),
    ("tab-ratings",    "RATINGS"),
    ("tab-sprt",       "SPRT"),
    ("tab-sts",        "STS"),
    ("tab-perft",      "PERFT"),
    ("tab-positions",  "POSITIONS"),
    ("tab-corr",       "CORRELATION"),
]


# ──────────────────────────────────────────────────────────────────────────────
# SIDEBAR
# ──────────────────────────────────────────────────────────────────────────────

def _data_stats_section():
    """Small data freshness / summary section at the top of the sidebar."""
    stats = DATA_STATS
    return html.Div([
        html.Div("DATA", style={**SIDEBAR_LABEL, "marginTop": "10px", "color": ACCENT,
                                "fontSize": "8px", "letterSpacing": "0.2em"}),
        html.Div([
            html.Span(f"{stats['n_games']:,}", style={"color": ACCENT, "fontWeight": "700"}),
            html.Span(" games", style={"color": TEXT_SEC}),
        ], style={"fontSize": "11px", "fontFamily": "'JetBrains Mono'"}),
        html.Div([
            html.Span(f"{stats['n_searches']:,}", style={"color": ACCENT, "fontWeight": "700"}),
            html.Span(" searches", style={"color": TEXT_SEC}),
        ], style={"fontSize": "11px", "fontFamily": "'JetBrains Mono'"}),
        html.Div([
            html.Span(f"{stats['n_engines']}", style={"color": ACCENT, "fontWeight": "700"}),
            html.Span(" engines", style={"color": TEXT_SEC}),
        ], style={"fontSize": "11px", "fontFamily": "'JetBrains Mono'"}),
        html.Div([
            html.Span("Updated: ", style={"color": TEXT_SEC}),
            html.Span(stats.get("last_updated", "unknown"),
                      style={"color": TEXT_PRI, "fontWeight": "600"}),
        ], style={"fontSize": "9px", "fontFamily": "'JetBrains Mono'", "marginTop": "4px"}),
        html.Hr(style={"borderColor": BORDER, "margin": "10px 0", "opacity": "0.3"}),
    ], style={"padding": "0 14px"})


def make_sidebar():
    return html.Div([
        # Logo
        html.Div([
            html.Div("⬡", style={"fontSize": "20px", "color": ACCENT, "lineHeight": "1"}),
            html.Div("CHESS", style={"fontFamily": "'JetBrains Mono'", "fontSize": "10px",
                                     "fontWeight": "700", "letterSpacing": "0.22em",
                                     "color": TEXT_PRI, "marginTop": "2px"}),
            html.Div("ANALYTICS", style={"fontFamily": "'JetBrains Mono'", "fontSize": "8px",
                                         "fontWeight": "400", "letterSpacing": "0.28em",
                                         "color": TEXT_SEC}),
        ], style={"padding": "16px 14px 12px", "borderBottom": f"1px solid {BORDER}"}),

        # Data freshness
        _data_stats_section(),

        # Filters
        html.Div([
            html.Div("FILTERS", style={**SIDEBAR_LABEL, "marginTop": "4px", "color": ACCENT,
                                       "fontSize": "8px", "letterSpacing": "0.2em"}),

            html.Div("Engine", style=SIDEBAR_LABEL),
            dcc.Dropdown(id="filter-engine", options=engine_options, multi=True,
                         placeholder="All engines", style=DROPDOWN_STYLE),

            html.Div("Side", style=SIDEBAR_LABEL),
            dcc.Dropdown(id="filter-side", options=side_options, multi=True,
                         placeholder="Both sides", style=DROPDOWN_STYLE),

            html.Div("Result", style=SIDEBAR_LABEL),
            dcc.Dropdown(id="filter-result", options=result_options, multi=True,
                         placeholder="All results", style=DROPDOWN_STYLE),

            html.Div("Opening", style=SIDEBAR_LABEL),
            dcc.Dropdown(id="filter-opening", options=opening_options, multi=True,
                         placeholder="All openings", style=DROPDOWN_STYLE),

            html.Div("Position Type", style=SIDEBAR_LABEL),
            dcc.Dropdown(id="filter-pos-type", options=pos_type_options, multi=True,
                         placeholder="All types", style=DROPDOWN_STYLE),

            html.Div("Game Phase", style=SIDEBAR_LABEL),
            dcc.Dropdown(id="filter-game-phase", options=game_phase_options, multi=True,
                         placeholder="All phases", style=DROPDOWN_STYLE),

            html.Hr(style={"borderColor": BORDER, "margin": "16px 0", "opacity": "0.5"}),

            html.Div("Checkmates", style=SIDEBAR_LABEL),
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


# ──────────────────────────────────────────────────────────────────────────────
# APP LAYOUT (with dcc.Loading wrapper)
# ──────────────────────────────────────────────────────────────────────────────

app.layout = html.Div([
    dcc.Location(id='url', refresh=False),
    html.Div([
        make_sidebar(),
        html.Div([
            html.Div(
                [html.Button(
                    lbl, id={"type": "tab-btn", "index": val},
                             n_clicks=0,
                             style=TAB_STYLE,
                ) for val, lbl in TABS],
                style={"display": "flex", "flexWrap": "wrap", "gap": "0", 
                       "borderBottom": f"1px solid {BORDER}", "backgroundColor": "#0b0d12",
                       "paddingLeft": "8px", "flexShrink": "0"},
            ),
            dcc.Store(id="main-tabs", data="tab-overview"),
            html.Div(id="tab-content", className="tab-content",
                     style={"padding": "20px 24px", "overflowY": "auto", "flex": "1"})
        ], style={"flex": "1", "display": "flex", "flexDirection": "column",
                   "overflow": "auto", "height": "100vh"}),
    ], style={"display": "flex", "flexDirection": "row", "height": "100vh"}),
], style={"backgroundColor": DARK_BG, "minHeight": "100vh"})
