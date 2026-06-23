"""
Tab renderer functions for all dashboard tabs.
"""
import numpy as np
import pandas as pd
import plotly.express as px
import plotly.graph_objects as go
from plotly.subplots import make_subplots
from dash import dcc, html

try:
    from scipy.optimize import minimize_scalar as _minimize_scalar
    _HAS_SCIPY = True
except ImportError:
    _HAS_SCIPY = False

from .config import (
    ACCENT, ACCENT2, TEXT_PRI, TEXT_SEC, BORDER, EVAL_CLIP_CP,
    _PALETTE, NUMERIC_EXCLUDE, MAX_TABLE_ROWS,
    numeric_cols, metric_label, metric_options, engine_colour_map, hex_to_rgba,
    SIDEBAR_LABEL, DROPDOWN_STYLE,
)
from .components import apply_theme, empty_fig, section, panel, metric_card, graph, table
from .data_loader import (
    engines_df, experiments_df, games_df, searches_df, iter_df, tree_df,
    timing_df, sprt_df, sts_df, positions_df, perft_df,
)

# Aliases used inside tab functions (from old single-file code)
_sidebar_label = SIDEBAR_LABEL
_dropdown_style = DROPDOWN_STYLE

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
# 9c. EVAL CALIBRATION, SEARCH STABILITY, MOVE ORDERING, NMP/LMR EFFECTIVENESS
# ──────────────────────────────────────────────────────────────────────────────

def tab_calibration(sf: pd.DataFrame, gf: pd.DataFrame) -> html.Div:
    """Eval calibration curve: predicted eval (cp) vs actual game outcome (win%).
    Renders controls + placeholder; actual charts built by callback."""
    if "eval" not in sf.columns or "result_label" not in sf.columns:
        return html.Div("Need eval and game result data for calibration.", style={"color": TEXT_SEC})

    split_options = [{"label": " All", "value": "all"}]
    if "game_phase" in sf.columns:
        split_options.append({"label": " Game Phase", "value": "game_phase"})
    if "pos_label" in sf.columns:
        split_options.append({"label": " Position Type", "value": "pos_label"})
    if "engine_label" in sf.columns and len(sf["engine_label"].dropna().unique()) >= 2:
        split_options.append({"label": " Engine Version", "value": "engine_label"})
    # Material count: check for a pieces/material column
    mat_col = next((c for c in ["material_count", "total_pieces", "piece_count", "num_pieces"] if c in sf.columns), None)
    if mat_col:
        split_options.append({"label": " Material Count", "value": mat_col})

    return html.Div([
        panel(
            section("EVAL CALIBRATION"),
            html.Div([
                html.Span("Split by: ", style={"color": TEXT_SEC, "fontSize": "11px", "marginRight": "8px"}),
                dcc.RadioItems(
                    id="calibration-split",
                    options=split_options,
                    value="all",
                    inline=True,
                    labelStyle={"display": "inline-block", "marginRight": "12px",
                                "fontSize": "11px", "fontFamily": "'JetBrains Mono'", "color": TEXT_SEC},
                ),
            ], style={"marginBottom": "12px"}),
        ),
        html.Div(id="calibration-content"),
    ])


def _build_calibration_charts(sf: pd.DataFrame, split_col: str | None) -> html.Div:
    """Internal helper: build calibration curve charts with optional split."""
    result_map = {"Win": 1.0, "Draw": 0.5, "Loss": 0.0}
    BIN_WIDTH = 10
    EVAL_RANGE = 800
    MIN_SAMPLES = 10

    keep_cols = ["eval", "result_label"]
    if split_col and split_col in sf.columns:
        keep_cols.append(split_col)
    df = sf[keep_cols].dropna().copy()
    if df.empty:
        return html.Div("No calibration data.", style={"color": TEXT_SEC})

    df["score"] = df["result_label"].map(result_map)
    df = df.dropna(subset=["score"])
    df["eval_clipped"] = df["eval"].clip(-EVAL_RANGE, EVAL_RANGE)

    bins = np.arange(-EVAL_RANGE, EVAL_RANGE + BIN_WIDTH + 1, BIN_WIDTH)
    df["eval_bin"] = pd.cut(df["eval_clipped"], bins=bins, labels=False)
    df["eval_bin_center"] = df["eval_bin"] * BIN_WIDTH - EVAL_RANGE + BIN_WIDTH / 2

    # Theoretical sigmoid
    x_theory = np.linspace(-EVAL_RANGE, EVAL_RANGE, 300)
    y_theory = 1 / (1 + 10 ** (-x_theory / 400))

    children = []

    if not split_col or split_col == "all" or split_col not in sf.columns:
        # ── Single (all data) ──
        agg = df.groupby("eval_bin_center").agg(
            win_rate=("score", "mean"), count=("score", "count"),
        ).reset_index()
        agg = agg[agg["count"] >= MIN_SAMPLES]
        if agg.empty:
            return html.Div("Not enough data per bucket.", style={"color": TEXT_SEC})

        fig = go.Figure()
        fig.add_trace(go.Scatter(
            x=agg["eval_bin_center"], y=agg["win_rate"],
            mode="markers+lines", name="Actual",
            marker=dict(size=5, color=ACCENT), line=dict(color=ACCENT, width=2),
        ))
        fig.add_trace(go.Scatter(
            x=x_theory, y=y_theory, mode="lines", name="Theoretical (K=400)",
            line=dict(color=TEXT_SEC, width=1.5, dash="dash"),
        ))
        fig.update_layout(title="Eval Calibration Curve",
                          xaxis_title="Eval (cp)", yaxis_title="Win Rate",
                          yaxis=dict(range=[0, 1]), legend=dict(x=0.02, y=0.98))
        apply_theme(fig)
        children.append(panel(section("CALIBRATION CURVE"), graph(fig, 400)))

        # Residuals
        agg["expected"] = 1 / (1 + 10 ** (-agg["eval_bin_center"] / 400))
        agg["residual"] = agg["win_rate"] - agg["expected"]
        fig_r = px.bar(agg, x="eval_bin_center", y="residual",
                       labels={"eval_bin_center": "Eval (cp)", "residual": "Actual − Expected"},
                       color_discrete_sequence=[ACCENT])
        fig_r.update_layout(title="Calibration Residuals")
        apply_theme(fig_r)
        children.append(panel(section("RESIDUALS"), graph(fig_r, 280)))

        # Sample count
        fig_c = px.bar(agg, x="eval_bin_center", y="count",
                       labels={"eval_bin_center": "Eval (cp)", "count": "Samples"},
                       color_discrete_sequence=["#3d4466"])
        fig_c.update_layout(title="Sample Count per Bucket")
        apply_theme(fig_c)
        children.append(panel(section("SAMPLE DISTRIBUTION"), graph(fig_c, 220)))

    else:
        # ── Split by category ──
        # For material count, create bins (groups of ~4 pieces)
        if split_col in sf.columns and sf[split_col].dtype in [np.float64, np.int64, float, int]:
            q_labels = ["Low (≤10)", "Mid (11-20)", "High (21-32)"]
            df["_split"] = pd.cut(df[split_col], bins=[0, 10, 20, 32], labels=q_labels, include_lowest=True)
        else:
            df["_split"] = df[split_col].astype(str)

        groups = sorted(df["_split"].dropna().unique())

        fig = go.Figure()

        colors = px.colors.qualitative.Set2
        for i, grp in enumerate(groups):
            sub = df[df["_split"] == grp]
            agg = sub.groupby("eval_bin_center").agg(
                win_rate=("score", "mean"), count=("score", "count"),
            ).reset_index()
            agg = agg[agg["count"] >= MIN_SAMPLES]
            if agg.empty:
                continue
            color = colors[i % len(colors)]
            # Actual data points
            fig.add_trace(go.Scatter(
                x=agg["eval_bin_center"], y=agg["win_rate"],
                mode="markers+lines", name=str(grp),
                marker=dict(size=4, color=color),
                line=dict(color=color, width=2),
            ))
            # Fit K for this group: find K that minimizes MSE between sigmoid and actual
            def _mse(k, _agg=agg):
                pred = 1 / (1 + 10 ** (-_agg["eval_bin_center"] / k))
                return ((_agg["win_rate"] - pred) ** 2).mean()
            if _HAS_SCIPY:
                res = _minimize_scalar(_mse, bounds=(100, 1200), method="bounded")
                k_fit = res.x if res.success else 400
            else:
                # Fallback: grid search
                k_fit = min(range(100, 1201, 10), key=_mse)
            y_fit = 1 / (1 + 10 ** (-x_theory / k_fit))
            fig.add_trace(go.Scatter(
                x=x_theory, y=y_fit, mode="lines",
                name=f"{grp} fit (K={k_fit:.0f})",
                line=dict(color=color, width=1.5, dash="dash"),
            ))

        fig.update_layout(
            title=f"Calibration Split by {metric_label(split_col)}",
            xaxis_title="Eval (cp)", yaxis_title="Win Rate",
            yaxis=dict(range=[0, 1]), legend=dict(x=0.02, y=0.98),
        )
        apply_theme(fig)
        children.append(panel(section(f"CALIBRATION BY {metric_label(split_col).upper()}"), graph(fig, 420)))

        # Per-group residuals as subplot
        fig_r = make_subplots(rows=1, cols=len(groups), subplot_titles=[str(g) for g in groups],
                              shared_yaxes=True) if len(groups) <= 6 else None
        if fig_r and len(groups) >= 2:
            for i, grp in enumerate(groups):
                sub = df[df["_split"] == grp]
                agg = sub.groupby("eval_bin_center").agg(
                    win_rate=("score", "mean"), count=("score", "count"),
                ).reset_index()
                agg = agg[agg["count"] >= MIN_SAMPLES]
                if agg.empty:
                    continue
                agg["expected"] = 1 / (1 + 10 ** (-agg["eval_bin_center"] / 400))
                agg["residual"] = agg["win_rate"] - agg["expected"]
                fig_r.add_trace(go.Bar(
                    x=agg["eval_bin_center"], y=agg["residual"],
                    marker_color=colors[i % len(colors)], name=str(grp), showlegend=False,
                ), row=1, col=i + 1)
            fig_r.update_layout(title="Residuals by Group", height=280)
            apply_theme(fig_r)
            children.append(panel(section("RESIDUALS BY GROUP"), graph(fig_r, 280)))

    return html.Div(children, style={"display": "flex", "flexDirection": "column", "gap": "12px"})


def tab_stability(sf: pd.DataFrame) -> html.Div:
    """Search stability analysis: PV instability, eval volatility across iterations."""
    children = []

    # 1. Move stability distribution (from search_features)
    if "move_stability" in sf.columns:
        ms = sf["move_stability"].dropna()
        if not ms.empty:
            fig = px.histogram(
                ms, nbins=50, color_discrete_sequence=[ACCENT],
                labels={"value": "Move Stability (% iterations with same best move)", "count": "Searches"},
            )
            fig.update_layout(title="Move Stability Distribution",
                              xaxis_title="Move Stability (%)", yaxis_title="Count")
            apply_theme(fig)
            children.append(panel(section("MOVE STABILITY"), graph(fig, 320)))

            # KPI cards
            cards = html.Div([
                metric_card(f"{ms.mean():.1f}%", "Avg Stability"),
                metric_card(f"{ms.median():.1f}%", "Median Stability"),
                metric_card(f"{(ms < 50).sum()}", "Unstable Searches (<50%)", accent="#e74c3c"),
                metric_card(f"{(ms >= 90).mean()*100:.1f}%", "Very Stable (≥90%)"),
            ], className="metric-grid")
            children.insert(0, cards)

    # 2. Stability by depth
    if "move_stability" in sf.columns and "depth" in sf.columns:
        depth_stab = sf[["depth", "move_stability"]].dropna()
        if not depth_stab.empty:
            agg = depth_stab.groupby("depth")["move_stability"].agg(["mean", "std", "count"]).reset_index()
            agg = agg[agg["count"] >= 5]
            if not agg.empty:
                fig = go.Figure()
                fig.add_trace(go.Scatter(
                    x=agg["depth"], y=agg["mean"], mode="lines+markers",
                    name="Avg Stability", line=dict(color=ACCENT, width=2),
                    marker=dict(size=5),
                ))
                if "std" in agg.columns:
                    fig.add_trace(go.Scatter(
                        x=agg["depth"], y=(agg["mean"] - agg["std"]).clip(lower=0),
                        mode="lines", line=dict(width=0), showlegend=False,
                    ))
                    fig.add_trace(go.Scatter(
                        x=agg["depth"], y=(agg["mean"] + agg["std"]).clip(upper=100),
                        mode="lines", line=dict(width=0), fill="tonexty",
                        fillcolor="rgba(0,210,255,0.15)", name="±1σ",
                    ))
                fig.update_layout(title="Move Stability by Search Depth",
                                  xaxis_title="Depth", yaxis_title="Stability (%)")
                apply_theme(fig)
                children.append(panel(section("STABILITY vs DEPTH"), graph(fig, 320)))

    # 3. Stability by game phase
    if "move_stability" in sf.columns and "game_phase" in sf.columns:
        phase_stab = sf[["game_phase", "move_stability"]].dropna()
        if not phase_stab.empty:
            fig = px.box(
                phase_stab, x="game_phase", y="move_stability",
                color_discrete_sequence=[ACCENT],
                labels={"game_phase": "Game Phase", "move_stability": "Stability (%)"},
            )
            fig.update_layout(title="Stability by Game Phase")
            apply_theme(fig)
            children.append(panel(section("STABILITY vs GAME PHASE"), graph(fig, 320)))

    # 4. Eval volatility (sign flips)
    if "eval_sign_flips" in sf.columns:
        flips = sf["eval_sign_flips"].dropna()
        if not flips.empty:
            fig = px.histogram(
                flips, nbins=max(int(flips.max()) + 1, 10),
                color_discrete_sequence=["#e74c3c"],
                labels={"value": "Eval Sign Flips", "count": "Searches"},
            )
            fig.update_layout(title="Eval Sign Flips per Search (PV Instability)")
            apply_theme(fig)
            children.append(panel(section("EVAL SIGN FLIPS"), graph(fig, 280)))

    # 5. Eval std dev distribution
    if "stddev_eval" in sf.columns:
        ev_std = sf["stddev_eval"].dropna()
        if not ev_std.empty:
            fig = px.histogram(
                ev_std.clip(upper=ev_std.quantile(0.95)), nbins=50,
                color_discrete_sequence=["#f39c12"],
                labels={"value": "Eval Std Dev (cp)", "count": "Searches"},
            )
            fig.update_layout(title="Eval Volatility Across Iterations (Std Dev)")
            apply_theme(fig)
            children.append(panel(section("EVAL VOLATILITY"), graph(fig, 280)))

    # 6. Stability by engine version
    if "move_stability" in sf.columns and "engine_label" in sf.columns:
        eng_stab = sf[["engine_label", "move_stability"]].dropna()
        if not eng_stab.empty and len(eng_stab["engine_label"].unique()) >= 2:
            fig = px.box(
                eng_stab, x="engine_label", y="move_stability",
                color_discrete_sequence=[ACCENT],
                labels={"engine_label": "Engine", "move_stability": "Stability (%)"},
            )
            fig.update_layout(title="Stability by Engine Version")
            apply_theme(fig)
            children.append(panel(section("STABILITY BY VERSION"), graph(fig, 320)))

    if not children:
        return html.Div("No stability data available (need move_stability or eval columns).",
                        style={"color": TEXT_SEC})
    return html.Div(children, style={"display": "flex", "flexDirection": "column", "gap": "12px"})


def tab_move_ordering(sf: pd.DataFrame) -> html.Div:
    """Move ordering quality: first-move cutoff rate and beta cutoff analysis."""
    children = []

    # 1. Fail-High First Rate overview
    fhf_col = "fail_high_first_ratio" if "fail_high_first_ratio" in sf.columns else "avg_fail_high_first_ratio"
    if fhf_col in sf.columns:
        fhf = sf[fhf_col].dropna()
        if not fhf.empty:
            # KPI
            cards = html.Div([
                metric_card(f"{fhf.mean()*100:.1f}%", "Avg 1st-Move Cutoff Rate"),
                metric_card(f"{fhf.median()*100:.1f}%", "Median 1st-Move Cutoff"),
                metric_card(f"{(fhf >= 0.9).mean()*100:.1f}%", "Excellent (≥90%)", accent="#2ecc71"),
                metric_card(f"{(fhf < 0.6).mean()*100:.1f}%", "Poor (<60%)", accent="#e74c3c"),
            ], className="metric-grid")
            children.append(cards)

            # Distribution
            fig = px.histogram(
                fhf * 100, nbins=40, color_discrete_sequence=[ACCENT],
                labels={"value": "Fail-High 1st Move Rate (%)", "count": "Searches"},
            )
            fig.update_layout(title="Move Ordering Quality: First-Move Cutoff Rate Distribution")
            apply_theme(fig)
            children.append(panel(section("1ST-MOVE CUTOFF DISTRIBUTION"), graph(fig, 320)))

    # 2. By depth
    if fhf_col in sf.columns and "depth" in sf.columns:
        depth_df = sf[["depth", fhf_col]].dropna()
        if not depth_df.empty:
            agg = depth_df.groupby("depth")[fhf_col].agg(["mean", "count"]).reset_index()
            agg = agg[agg["count"] >= 5]
            if not agg.empty:
                fig = px.line(
                    agg, x="depth", y="mean", markers=True,
                    labels={"depth": "Search Depth", "mean": "Avg 1st-Move Cutoff Rate"},
                    color_discrete_sequence=[ACCENT],
                )
                fig.update_layout(title="Move Ordering Quality by Depth",
                                  yaxis=dict(tickformat=".0%"))
                apply_theme(fig)
                children.append(panel(section("ORDERING QUALITY vs DEPTH"), graph(fig, 320)))

    # 3. By game phase
    if fhf_col in sf.columns and "game_phase" in sf.columns:
        phase_df = sf[["game_phase", fhf_col]].dropna()
        if not phase_df.empty:
            fig = px.box(
                phase_df, x="game_phase", y=fhf_col,
                color_discrete_sequence=[ACCENT],
                labels={"game_phase": "Game Phase", fhf_col: "1st-Move Cutoff Rate"},
            )
            fig.update_layout(title="Move Ordering by Game Phase", yaxis=dict(tickformat=".0%"))
            apply_theme(fig)
            children.append(panel(section("ORDERING BY GAME PHASE"), graph(fig, 300)))

    # 4. By engine version
    if fhf_col in sf.columns and "engine_label" in sf.columns:
        eng_df = sf[["engine_label", fhf_col]].dropna()
        if not eng_df.empty and len(eng_df["engine_label"].unique()) >= 2:
            agg = eng_df.groupby("engine_label")[fhf_col].mean().reset_index()
            agg = agg.sort_values(fhf_col, ascending=False)
            fig = px.bar(
                agg, x="engine_label", y=fhf_col,
                color_discrete_sequence=[ACCENT],
                labels={"engine_label": "Engine", fhf_col: "Avg 1st-Move Cutoff Rate"},
            )
            fig.update_layout(title="Move Ordering by Engine Version", yaxis=dict(tickformat=".0%"))
            apply_theme(fig)
            children.append(panel(section("ORDERING BY VERSION"), graph(fig, 300)))

    # 5. Fail-High vs Fail-Low ratio
    fh_col = "fail_high_ratio" if "fail_high_ratio" in sf.columns else "avg_fail_high_ratio"
    fl_col = "fail_low_ratio" if "fail_low_ratio" in sf.columns else "avg_fail_low_ratio"
    if fh_col in sf.columns and fl_col in sf.columns:
        ratio_df = sf[[fh_col, fl_col]].dropna()
        if not ratio_df.empty:
            fig = px.scatter(
                ratio_df, x=fh_col, y=fl_col, opacity=0.3,
                color_discrete_sequence=[ACCENT],
                labels={fh_col: "Fail-High Rate", fl_col: "Fail-Low Rate"},
            )
            fig.update_layout(title="Fail-High vs Fail-Low Rate")
            apply_theme(fig)
            children.append(panel(section("FAIL-HIGH vs FAIL-LOW"), graph(fig, 320)))

    if not children:
        return html.Div("No move ordering data available (need fail_high_first_ratio).",
                        style={"color": TEXT_SEC})
    return html.Div(children, style={"display": "flex", "flexDirection": "column", "gap": "12px"})


def tab_pruning(sf: pd.DataFrame) -> html.Div:
    """NMP/LMR/SEE effectiveness breakdown by depth and position type."""
    children = []

    # ── NMP Section ──
    nmp_cols = [c for c in ["nmp", "nmp_fail", "nmp_ratio", "nmp_fail_ratio",
                            "avg_nmp_ratio", "avg_nmp_fail_ratio"] if c in sf.columns]
    nmp_rate_col = next((c for c in ["nmp_ratio", "avg_nmp_ratio"] if c in sf.columns), None)
    nmp_fail_col = next((c for c in ["nmp_fail_ratio", "avg_nmp_fail_ratio"] if c in sf.columns), None)

    if nmp_rate_col:
        nmp_data = sf[nmp_rate_col].dropna()
        nmp_fail_data = sf[nmp_fail_col].dropna() if nmp_fail_col else pd.Series(dtype=float)

        cards = html.Div([
            metric_card(f"{nmp_data.mean()*100:.1f}%", "Avg NMP Rate"),
            metric_card(f"{nmp_fail_data.mean()*100:.1f}%" if not nmp_fail_data.empty else "N/A",
                        "Avg NMP Fail Rate", accent="#e74c3c"),
            metric_card(f"{(1 - nmp_fail_data.mean())*100:.1f}%" if not nmp_fail_data.empty else "N/A",
                        "NMP Success Rate", accent="#2ecc71"),
        ], className="metric-grid")
        children.append(cards)

    # NMP by depth
    if nmp_rate_col and "depth" in sf.columns:
        depth_nmp = sf[["depth", nmp_rate_col]].dropna()
        if nmp_fail_col:
            depth_nmp = sf[["depth", nmp_rate_col, nmp_fail_col]].dropna()

        if not depth_nmp.empty:
            agg = depth_nmp.groupby("depth").mean().reset_index()
            fig = go.Figure()
            fig.add_trace(go.Scatter(
                x=agg["depth"], y=agg[nmp_rate_col], mode="lines+markers",
                name="NMP Attempt Rate", line=dict(color=ACCENT, width=2),
            ))
            if nmp_fail_col and nmp_fail_col in agg.columns:
                fig.add_trace(go.Scatter(
                    x=agg["depth"], y=agg[nmp_fail_col], mode="lines+markers",
                    name="NMP Fail Rate", line=dict(color="#e74c3c", width=2),
                ))
            fig.update_layout(title="NMP Rates by Search Depth",
                              xaxis_title="Depth", yaxis_title="Rate",
                              yaxis=dict(tickformat=".0%"))
            apply_theme(fig)
            children.append(panel(section("NMP BY DEPTH"), graph(fig, 320)))

    # NMP by game phase
    if nmp_rate_col and "game_phase" in sf.columns:
        phase_df = sf[["game_phase", nmp_rate_col]].dropna()
        if nmp_fail_col:
            phase_df = sf[["game_phase", nmp_rate_col, nmp_fail_col]].dropna()
        if not phase_df.empty:
            agg = phase_df.groupby("game_phase").mean().reset_index()
            fig = go.Figure()
            fig.add_trace(go.Bar(
                x=agg["game_phase"], y=agg[nmp_rate_col],
                name="NMP Rate", marker_color=ACCENT,
            ))
            if nmp_fail_col and nmp_fail_col in agg.columns:
                fig.add_trace(go.Bar(
                    x=agg["game_phase"], y=agg[nmp_fail_col],
                    name="NMP Fail Rate", marker_color="#e74c3c",
                ))
            fig.update_layout(title="NMP Effectiveness by Game Phase", barmode="group",
                              xaxis_title="Game Phase", yaxis_title="Rate",
                              yaxis=dict(tickformat=".0%"))
            apply_theme(fig)
            children.append(panel(section("NMP BY GAME PHASE"), graph(fig, 300)))

    # ── PVS Section ──
    pvs_col = next((c for c in ["pvs_research_ratio", "avg_pvs_research_ratio"] if c in sf.columns), None)
    if pvs_col:
        pvs_data = sf[pvs_col].dropna()
        if not pvs_data.empty:
            pvs_cards = html.Div([
                metric_card(f"{pvs_data.mean()*100:.2f}%", "Avg PVS Re-search Rate"),
                metric_card(f"{pvs_data.max()*100:.1f}%", "Max PVS Re-search Rate", accent="#f39c12"),
            ], className="metric-grid")
            children.append(pvs_cards)

            # PVS by depth
            if "depth" in sf.columns:
                pvs_depth = sf[["depth", pvs_col]].dropna()
                if not pvs_depth.empty:
                    agg = pvs_depth.groupby("depth")[pvs_col].mean().reset_index()
                    fig = px.line(
                        agg, x="depth", y=pvs_col, markers=True,
                        color_discrete_sequence=["#f39c12"],
                        labels={"depth": "Search Depth", pvs_col: "PVS Re-search Rate"},
                    )
                    fig.update_layout(title="PVS Re-search Rate by Depth",
                                      yaxis=dict(tickformat=".1%"))
                    apply_theme(fig)
                    children.append(panel(section("PVS BY DEPTH"), graph(fig, 300)))

    # ── SEE Pruning Section ──
    see_col = next((c for c in ["see_prune_ratio", "avg_see_prune_ratio"] if c in sf.columns), None)
    if see_col:
        see_data = sf[see_col].dropna()
        if not see_data.empty:
            # By depth
            if "depth" in sf.columns:
                see_depth = sf[["depth", see_col]].dropna()
                if not see_depth.empty:
                    agg = see_depth.groupby("depth")[see_col].mean().reset_index()
                    fig = px.line(
                        agg, x="depth", y=see_col, markers=True,
                        color_discrete_sequence=["#9b59b6"],
                        labels={"depth": "Depth", see_col: "SEE Prune Rate"},
                    )
                    fig.update_layout(title="SEE Prune Rate by Depth",
                                      yaxis=dict(tickformat=".1%"))
                    apply_theme(fig)
                    children.append(panel(section("SEE PRUNING BY DEPTH"), graph(fig, 300)))

    # ── Delta Pruning Section ──
    delta_col = next((c for c in ["delta_prune_ratio", "avg_delta_prune_ratio"] if c in sf.columns), None)
    if delta_col:
        delta_data = sf[delta_col].dropna()
        if not delta_data.empty and "depth" in sf.columns:
            delta_depth = sf[["depth", delta_col]].dropna()
            if not delta_depth.empty:
                agg = delta_depth.groupby("depth")[delta_col].mean().reset_index()
                fig = px.line(
                    agg, x="depth", y=delta_col, markers=True,
                    color_discrete_sequence=["#1abc9c"],
                    labels={"depth": "Depth", delta_col: "Delta Prune Rate"},
                )
                fig.update_layout(title="Delta Prune Rate by Depth",
                                  yaxis=dict(tickformat=".1%"))
                apply_theme(fig)
                children.append(panel(section("DELTA PRUNING BY DEPTH"), graph(fig, 300)))

    # ── Combined pruning comparison by engine version ──
    prune_cols = [c for c in [nmp_rate_col, pvs_col, see_col, delta_col] if c is not None and c in sf.columns]
    if prune_cols and "engine_label" in sf.columns and len(sf["engine_label"].dropna().unique()) >= 2:
        eng_agg = sf.groupby("engine_label")[prune_cols].mean().reset_index()
        fig = go.Figure()
        colors = [ACCENT, "#f39c12", "#9b59b6", "#1abc9c"]
        names = ["NMP", "PVS", "SEE", "Delta"]
        for i, col in enumerate(prune_cols):
            fig.add_trace(go.Bar(
                x=eng_agg["engine_label"], y=eng_agg[col],
                name=names[i] if i < len(names) else col,
                marker_color=colors[i % len(colors)],
            ))
        fig.update_layout(title="Pruning Rates by Engine Version", barmode="group",
                          xaxis_title="Engine", yaxis_title="Rate",
                          yaxis=dict(tickformat=".1%"))
        apply_theme(fig)
        children.append(panel(section("PRUNING BY VERSION"), graph(fig, 320)))

    if not children:
        return html.Div("No pruning/NMP data available.", style={"color": TEXT_SEC})
    return html.Div(children, style={"display": "flex", "flexDirection": "column", "gap": "12px"})


# ──────────────────────────────────────────────────────────────────────────────
