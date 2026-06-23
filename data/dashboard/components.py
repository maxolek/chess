"""
Reusable UI components: panel, section, graph, table, metric_card, apply_theme.
"""
import numpy as np
import pandas as pd
import dash
from dash import dash_table, dcc, html
import plotly.graph_objects as go

from .config import (
    ACCENT, TEXT_PRI, TEXT_SEC, BORDER, PLOTLY_THEME, _PALETTE, MAX_TABLE_ROWS,
)


def apply_theme(fig) -> go.Figure:
    fig.update_layout(**PLOTLY_THEME)
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
    style = {"flex": flex} if flex else {}
    style.update(style_kwargs)
    return html.Div(list(children), className="panel", style=style)


def metric_card(value, label: str, accent=ACCENT, sparkline=None) -> html.Div:
    """KPI card with optional inline SVG sparkline."""
    children = [
        html.Div(str(value), className="metric-val", style={"color": accent}),
        html.Div(label, className="metric-lbl"),
    ]
    if sparkline and len(sparkline) > 1:
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

    def _coerce_cell(v):
        try:
            if v is None:
                return None
            if pd.isna(v):
                return None
        except Exception:
            pass
        if isinstance(v, (str, bytes, int, float, bool)):
            if isinstance(v, bytes):
                try:
                    return v.decode('utf-8')
                except Exception:
                    return str(v)
            return v
        try:
            s = str(v)
            return s if len(s) <= 200 else s[:197] + '...'
        except Exception:
            return None

    for c in disp.columns:
        try:
            disp = disp.applymap(_coerce_cell)
        except Exception:
            try:
                disp = disp.applymap(lambda v: None if v is None else str(v))
            except Exception:
                disp = disp.astype(str)

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
