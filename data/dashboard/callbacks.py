"""
Dash callbacks for the dashboard.
"""

import numpy as np
import pandas as pd
import plotly.express as px
import plotly.graph_objects as go
from dash import Input, Output, State, ctx, html, dcc

from .app import app
from .config import (
    ACCENT, ACCENT2, TEXT_PRI, TEXT_SEC, BORDER, _PALETTE,
    numeric_cols, metric_label, metric_options, engine_colour_map, hex_to_rgba,
    SIDEBAR_LABEL, DROPDOWN_STYLE, EVAL_CLIP_CP,
)
from .components import apply_theme, empty_fig, section, panel, metric_card, graph, table
from .data_loader import (
    engines_df, experiments_df, searches_df,
    sprt_df, sts_df, perft_df,
    apply_filters, _search_nums, _iter_nums, _tree_nums,
    query_iter, query_tree, query_iter_single, query_searches_for_game,
    query_iter_agg, query_tree_agg,
)
from .tabs import (
    tab_overview, tab_trends, tab_games, tab_search, tab_iter, tab_tree,
    tab_compare, tab_openings, tab_quality, tab_timing, tab_root_moves, tab_time_mgmt, tab_tt,
    tab_calibration, tab_stability, tab_move_ordering, tab_pruning,
    tab_ratings, tab_sprt, tab_sts, tab_perft, tab_positions, tab_corr,
    _build_calibration_charts,
)

# Aliases
_sidebar_label = SIDEBAR_LABEL
_dropdown_style = DROPDOWN_STYLE

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
    if tab == "tab-root-moves": return tab_root_moves()
    if tab == "tab-time-mgmt": return tab_time_mgmt(sf)
    if tab == "tab-tt":       return tab_tt(sf)
    if tab == "tab-calibration": return tab_calibration(sf, gf)
    if tab == "tab-stability": return tab_stability(sf)
    if tab == "tab-ordering": return tab_move_ordering(sf)
    if tab == "tab-pruning":  return tab_pruning(sf)
    if tab == "tab-ratings":  return tab_ratings()
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
    if search_id:
        irows = query_iter_single(search_id)
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
    if not y_col:
        return html.Div("No data.", style={"color": TEXT_SEC})

    # Push aggregation to DuckDB — returns ~(depths × engines) rows
    agg_df = query_iter_agg(
        y_col, agg,
        engine_ids=engine_ids,
        pos_type_vals=pos_type_vals,
        game_phase_vals=game_phase_vals,
        include_mates=(include_mates_val == "include"),
        mates_only=(include_mates_val == "only"),
    )
    if agg_df.empty:
        return html.Div("No iteration data.", style={"color": TEXT_SEC})

    if "depth" not in agg_df.columns or y_col not in agg_df.columns:
        return html.Div("Column not found.", style={"color": TEXT_SEC})

    color_col = "engine_name" if "engine_name" in agg_df.columns and agg_df["engine_name"].nunique() > 1 else None
    fig = px.line(agg_df, x="depth", y=y_col, color=color_col,
                  color_discrete_sequence=_PALETTE,
                  markers=True,
                  labels={"depth": "Iteration Depth", y_col: f"{agg}({metric_label(y_col)})"})

    # p10/p90 bands come pre-computed from SQL
    if agg in ("mean", "median") and "p10" in agg_df.columns and "p90" in agg_df.columns:
        if color_col is None:
            fig.add_trace(go.Scatter(x=agg_df["depth"], y=agg_df["p90"], mode="lines",
                                     line=dict(width=0), showlegend=False, hoverinfo="skip"))
            fig.add_trace(go.Scatter(x=agg_df["depth"], y=agg_df["p10"], mode="lines",
                                     line=dict(width=0), fill="tonexty",
                                     fillcolor=hex_to_rgba(ACCENT, 0.15),
                                     showlegend=True, name="p10–p90"))
        else:
            for i, eng in enumerate(agg_df[color_col].unique()):
                eb = agg_df[agg_df[color_col] == eng]
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
    if not y_col:
        return html.Div("No data.", style={"color": TEXT_SEC})

    # Push aggregation to DuckDB — returns ~(depths × engines) rows
    agg_df = query_tree_agg(
        y_col,
        engine_ids=engine_ids,
        pos_type_vals=pos_type_vals,
        game_phase_vals=game_phase_vals,
        include_mates=(include_mates_val == "include"),
        mates_only=(include_mates_val == "only"),
    )
    if agg_df.empty:
        return html.Div("No tree data.", style={"color": TEXT_SEC})

    if "depth" not in agg_df.columns or y_col not in agg_df.columns:
        return html.Div("Column not found.", style={"color": TEXT_SEC})

    color_col = "engine_name" if "engine_name" in agg_df.columns and agg_df["engine_name"].nunique() > 1 else None
    fig = px.line(agg_df, x="depth", y=y_col, color=color_col,
                  color_discrete_sequence=_PALETTE, markers=True,
                  log_y=(scale == "log"),
                  labels={"depth": "Tree Ply", y_col: f"mean({metric_label(y_col)})"})

    # p10/p90 bands come pre-computed from SQL
    if "p10" in agg_df.columns and "p90" in agg_df.columns:
        if color_col is None:
            fig.add_trace(go.Scatter(x=agg_df["depth"], y=agg_df["p90"], mode="lines",
                                     line=dict(width=0), showlegend=False, hoverinfo="skip"))
            fig.add_trace(go.Scatter(x=agg_df["depth"], y=agg_df["p10"], mode="lines",
                                     line=dict(width=0), fill="tonexty",
                                     fillcolor=hex_to_rgba(ACCENT, 0.15),
                                     showlegend=True, name="p10–p90"))
        else:
            for i, eng in enumerate(agg_df[color_col].unique()):
                eb = agg_df[agg_df[color_col] == eng]
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

    # Query all searches for this specific game directly from DB
    gsearch = query_searches_for_game(game_id)
    if gsearch.empty:
        return html.Div("No per-ply search data available.", style={"color": TEXT_SEC})

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
# 14b. CALIBRATION CALLBACK
# ──────────────────────────────────────────────────────────────────────────────

@app.callback(
    Output("calibration-content", "children"),
    Input("calibration-split", "value"),
    Input("filter-engine",     "value"),
    Input("filter-result",     "value"),
    Input("filter-opening",    "value"),
    Input("filter-side",       "value"),
    Input("filter-pos-type",   "value"),
    Input("filter-game-phase", "value"),
    Input("filter-include-mates", "value"),
)
def update_calibration(split_val, engine_ids, result_vals, opening_vals, side_vals, pos_type_vals, game_phase_vals, include_mates_val):
    _, sf = apply_filters(engine_ids, result_vals, opening_vals, side_vals, pos_type_vals, game_phase_vals,
                          include_mates=(include_mates_val == "include"),
                          mates_only=(include_mates_val == "only"))
    split_col = split_val if split_val and split_val != "all" else None
    return _build_calibration_charts(sf, split_col)


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
