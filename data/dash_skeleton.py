# chess_dashboard_robust.py
import dash
from dash import dcc, html, dash_table
from dash.dependencies import Input, Output, State
import pandas as pd
import plotly.express as px
import plotly.graph_objects as go
import numpy as np
import sqlite3
import duckdb
from pathlib import Path
import platform
import webbrowser

# --- Load Data ---
system = platform.system()
DB_PATH = "F:/databases/chess_analytics.duckdb" if system == "Windows" else Path.home() / "Documents/databases/chess.db"
DB_PATH = "F:/databases/chess_analytics.duckdb" if system == "Windows" else Path.home() / "Documents/databases/chess_analytics.duckdb"

# Connect to your DuckDB analytics file
con = duckdb.connect(DB_PATH)

# Load your analytical tables into DataFrames for Dash
# We pull from the pre-processed "features" tables we built earlier
games_df = con.execute("SELECT * FROM game_stats").df()
engines_df = con.execute("SELECT * FROM engines").df()
search_df = con.execute("SELECT * FROM search_features").df()
itdeep_df = con.execute("SELECT * FROM search_iteration_features").df()

# --- Data Clean-up and Enriching ---
# Merge engine info
search_df = search_df.merge(engines_df[['id', 'name', 'version']], how='left', left_on='engine_id', right_on='id', suffixes=('', '_engine'))
search_df = search_df.merge(games_df.add_prefix('game_'), how='left', left_on='game_id', right_on='game_id')

# Add engine_side, result, etc, if possible by game data (if columns present)
def get_side(row):
    # Check if the required IDs are missing first
    if pd.isna(row.get('engine_id')) or pd.isna(row.get('game_white_engine_id')):
        return 'unknown'
        
    if row['engine_id'] == row['game_white_engine_id']:
        return 'white'
    return 'black'

def get_result(row):
    # Expect result: 1,2,3 as string or int (1=white win, 2=black win, 3=draw)
    result = row.get('game_result', None)
    if pd.isnull(result):
        return np.nan
    rp = row.get('engine_side', 'unknown')
    if str(result) == '3':
        return 0.5
    elif str(result) == '1' and rp == 'white':
        return 1.0
    elif str(result) == '2' and rp == 'black':
        return 1.0
    elif str(result) in ['1', '2']:
        return 0.0
    return np.nan

if 'game_white_engine_id' in search_df and 'game_black_engine_id' in search_df:
    search_df['engine_side'] = search_df.apply(get_side, axis=1)
    search_df['result_perspective'] = search_df.apply(get_result, axis=1)
    search_df['result_label'] = search_df['result_perspective'].map({1.0: 'Win', 0.5: 'Draw', 0.0: 'Loss'})
else:
    search_df['engine_side'] = 'unknown'
    search_df['result_label'] = None

# --- Utility: Filtering ---
def filter_data(engine_vals, result_vals, opening_vals):
    games_filt = games_df.copy()
    search_filt = search_df.copy()

    search_filt = search_filt[
        (search_filt['depth'] <= 30)
        & (search_filt['eval'].abs() <= 10000)
    ]

    if engine_vals:
        # Try to match both white & black engine if present
        if "white_engine_id" in games_filt.columns and "black_engine_id" in games_filt.columns:
            games_filt = games_filt[
                (games_filt['white_engine_id'].isin(engine_vals)) |
                (games_filt['black_engine_id'].isin(engine_vals))
            ]
        else:
            pass  # cannot filter by engine, fallback to unfiltered
    if result_vals and 'result_label' in search_filt:
        search_filt = search_filt[search_filt['result_label'].isin(result_vals)]
    if opening_vals and 'opening' in games_filt:
        games_filt = games_filt[games_filt['opening'].isin(opening_vals)]
    if 'game_id' in search_filt.columns and 'id' in games_filt.columns:
        search_filt = search_filt[search_filt['game_id'].isin(games_filt['id'].unique())]
    return games_filt, search_filt

# --- Options ---
engine_options = [{"label": n, "value": i} for i, n in engines_df[['id', 'name']].drop_duplicates().values]
result_options = [{"label": r, "value": r} for r in search_df['result_label'].dropna().unique()]
opening_options = [{"label": o, "value": o} for o in games_df['opening'].dropna().unique()] if 'opening' in games_df else []

# --- LAYOUT ---
app = dash.Dash(__name__, suppress_callback_exceptions=True)
app.title = "Chess Robust Analytics Dashboard"

app.layout = html.Div([
    html.H1("Chess Analytics Dashboard", style={'textAlign': 'center', 'marginBottom': '20px'}),
    html.Div([
        # Sidebar
        html.Div([
            html.H3("Filters"),
            html.Label("Engine"),
            dcc.Dropdown(id='engine-filter', options=engine_options, multi=True, placeholder="All engines"),
            html.Br(),
            html.Label("Result"),
            dcc.Dropdown(id='result-filter', options=result_options, multi=True, placeholder="All results"),
            html.Br(),
            html.Label("Opening"),
            dcc.Dropdown(id='opening-filter', options=opening_options, multi=True, placeholder="All openings"),
            html.Br(),
            html.Hr(),
            html.Button("Reset Filters", id='reset-filters', n_clicks=0, style={'width': '100%'})
        ], style={'width': '20%', 'padding': '15px', 'borderRight': '1px solid #ddd',
                  'backgroundColor': '#f9f9f9', 'height': '100vh', 'overflowY': 'auto'}),

        # Main Panel
        html.Div([
            dcc.Tabs(id='tabs', value='tab-overview', children=[
                dcc.Tab(label='Overview', value='tab-overview'),
                dcc.Tab(label='Search Metrics', value='tab-search'),
                dcc.Tab(label='Per-FEN Analysis', value='tab-fen'),
                dcc.Tab(label='Game Table', value='tab-games'),
                dcc.Tab(label='Search Table', value='tab-searches'),
                dcc.Tab(label='Engine Comparison', value='tab-compare'),
                dcc.Tab(label='Stats/Distribution', value='tab-dist'),
                dcc.Tab(label='Correlation Matrix', value='tab-corr')
            ]),
            html.Div(id='tabs-content', style={'marginTop': '20px'})
        ], style={'width': '80%', 'padding': '20px'})
    ], style={'display': 'flex', 'flexDirection': 'row'})
])

# --- CALLBACKS ---

# FILTER RESET
@app.callback(
    [Output('engine-filter', 'value'), Output('result-filter', 'value'), Output('opening-filter', 'value')],
    [Input('reset-filters', 'n_clicks')]
)
def reset_filters(n):
    # Reset to None/no filters
    if n:
        return None, None, None
    return dash.no_update, dash.no_update, dash.no_update

# TAB RENDERING
@app.callback(
    Output('tabs-content', 'children'),
    Input('tabs', 'value'),
    Input('engine-filter', 'value'),
    Input('result-filter', 'value'),
    Input('opening-filter', 'value')
)
def render_tab(tab, engine_filter, result_filter, opening_filter):
    games_filt, search_filt = filter_data(engine_filter, result_filter, opening_filter)
    # Limit large search table rendering
    max_table_rows = 5000

    # --- Tab: OVERVIEW ---
    if tab == 'tab-overview':
        # Engine performance summary (games, winrate, etc)
        # Try to compute per engine if possible
        if 'result_label' in search_filt and 'name' in search_filt:
            per_game = search_filt.groupby(['name', 'game_id'], as_index=False).agg(
                result_perspective=('result_label', lambda x: x.iloc[0])
            )
            summary = per_game.groupby('name').agg(
                games=('game_id', 'nunique'),
                wins=('result_perspective', lambda x: (x == 'Win').sum()),
                draws=('result_perspective', lambda x: (x == 'Draw').sum()),
                losses=('result_perspective', lambda x: (x == 'Loss').sum())
            ).reset_index()
            summary['score'] = summary['wins'] + 0.5 * summary['draws']
            summary['WinRate'] = 100 * summary['wins'] / summary['games']
            summary['DrawRate'] = 100 * summary['draws'] / summary['games']
            summary['LossRate'] = 100 * summary['losses'] / summary['games']
            summary = summary.fillna(0)
            bar_fig = px.bar(summary.melt(id_vars='name', value_vars=['WinRate', 'DrawRate', 'LossRate']),
                             x='name', y='value', color='variable',
                             barmode='group', text_auto='.2f',
                             labels={'value': 'Rate (%)', 'name': 'Engine', 'variable': 'Result'})
        else:
            summary = pd.DataFrame()
            bar_fig = go.Figure()
        return html.Div([
            html.H3("Engine Performance Overview"),
            dcc.Graph(figure=bar_fig),
            html.H4("Summary Table"),
            dash_table.DataTable(
                columns=[{"name": c, "id": c} for c in summary.columns],
                data=summary.to_dict('records'),
                page_size=10,
                style_table={'overflowX': 'auto'}, style_cell={'fontSize': '13px'}
            )
        ])

    # --- Tab: Search Metrics ---
    elif tab == 'tab-search':
        numeric_cols = search_filt.select_dtypes(include=np.number).columns.tolist()
        exclude_cols = [
            'id', 'engine_id', 'game_id', 'sts_id', 'id_engine', 'id_game',
            'game_id', 'game_game_id'
        ]
        var_cols = [c for c in numeric_cols if c not in exclude_cols]
        if not var_cols:
            return html.Div("No numeric columns found for search plotting!")
        # Sensible default: time_ms, eval, depth, nodes, ... fallback
        x_col = 'depth' if 'depth' in var_cols else var_cols[0]
        y_col = 'eval' if 'eval' in var_cols else var_cols[1] if len(var_cols) > 1 else x_col
        return html.Div([
            html.Div([
                html.Div([
                    html.Label("X-axis"),
                    dcc.Dropdown(
                        id='search-x-axis',
                        options=[{'label': c, 'value': c} for c in var_cols],
                        value=x_col,
                        clearable=False
                    )
                ], style={'width': '48%', 'display': 'inline-block'}),
                html.Div([
                    html.Label("Y-axis"),
                    dcc.Dropdown(
                        id='search-y-axis',
                        options=[{'label': c, 'value': c} for c in var_cols],
                        value=y_col,
                        clearable=False
                    )
                ], style={'width': '48%', 'display': 'inline-block', 'float': 'right'})
            ], style={'marginBottom': '15px'}),
            html.Div(id='search-metrics-graph-container')
        ])

    # --- Tab: PER FEN ANALYSIS ---
    elif tab == 'tab-fen':
        # Fen drop, stats, histogram of eval/time/depth by FEN
        fen_options = [{'label': f, 'value': f} for f in search_filt['fen'].unique()[:100]]
        return html.Div([
            html.H3("Per-FEN Evaluation"),
            html.Label("Select FEN"),
            dcc.Dropdown(
                id='fen-dropdown',
                options=fen_options,
                multi=False,
                placeholder="Select FEN...",
            ),
            html.Div(id='fen-analysis-container')
        ])

    # --- Tab: Game Table ---
    elif tab == 'tab-games':
        return html.Div([
            html.H3("Games Table"),
            dash_table.DataTable(
                columns=[{"name": c, "id": c} for c in games_filt.columns],
                data=games_filt.head(max_table_rows).to_dict('records'),
                page_size=16,
                sort_action="native", filter_action="native",
                style_table={'overflowX': 'auto'},
                style_cell={'fontSize': '12px', 'textAlign': 'left'},
            ),
            html.Div(f"Showing {min(len(games_filt), max_table_rows)} of {len(games_filt)} rows.")
        ])

    # --- Tab: Search Table ---
    elif tab == 'tab-searches':
        return html.Div([
            html.H3("Searches Table"),
            dash_table.DataTable(
                columns=[{"name": c, "id": c} for c in search_filt.columns],
                data=search_filt.head(max_table_rows).to_dict('records'),
                page_size=16,
                sort_action="native", filter_action="native",
                style_table={'overflowX': 'auto', 'height': '600px'},
                style_cell={'fontSize': '12px', 'textAlign': 'left'},
            ),
            html.Div(f"Showing {min(len(search_filt), max_table_rows)} of {len(search_filt)} rows.")
        ])

    # --- Tab: Engine Comparison ---
    elif tab == 'tab-compare':
        # Engine-vs-Engine spread/distribution on eval, time, depth, etc
        engines = search_filt['name'].dropna().unique()
        if len(engines) < 2:
            return html.Div("Please select at least 2 engines for comparison.")
        eg1 = engines[0]
        eg2 = engines[1]
        compare_var = 'eval' if 'eval' in search_filt else search_filt.select_dtypes(include=np.number).columns[0]
        fig = px.box(
            search_filt[search_filt['name'].isin([eg1, eg2])],
            x='name', y=compare_var, color='result_label' if 'result_label' in search_filt else None,
            points='all',
            labels={compare_var: f"{compare_var.title()}", "name": "Engine", "result_label": "Result"}
        )
        return html.Div([
            html.H3(f"Engine Comparison: {eg1} vs {eg2}"),
            dcc.Graph(figure=fig)
        ])

    # --- Tab: Distribution ---
    elif tab == 'tab-dist':
        numeric_cols = search_filt.select_dtypes(include=np.number).columns.tolist()
        dist_figs = []
        for col in numeric_cols[:8]:  # limit for speed
            dist = go.Figure()
            for name, g in search_filt.groupby('name'):
                dist.add_trace(go.Histogram(x=g[col], name=str(name), opacity=0.6, nbinsx=30))
            dist.update_layout(barmode='overlay', title=f"Distribution of {col}", xaxis_title=col, yaxis_title="Count")
            dist.update_traces(opacity=0.6)
            dist_figs.append(dcc.Graph(figure=dist, style={'height': '320px'}))
        return html.Div([
            html.H3("Feature Distributions"),
            *dist_figs
        ])
    
    # --- Tab: Correlation Matrix ---
    elif tab == 'tab-corr':
        num_df = search_filt.select_dtypes(include=np.number)
        if num_df.shape[1] == 0:
            return html.Div("No numeric data for correlation matrix.")
        corr = num_df.corr().fillna(0)
        fig = px.imshow(
            corr, text_auto=True, color_continuous_scale='RdBu_r',
            aspect="auto", title="Correlation Matrix"
        )
        return html.Div([
            html.H3("Feature Correlation Matrix"),
            dcc.Graph(figure=fig)
        ])
    
    return html.Div("Select a tab.")

# --- SEARCH METRICS SCATTER CALLBACK ---
@app.callback(
    Output('search-metrics-graph-container', 'children'),
    Input('search-x-axis', 'value'),
    Input('search-y-axis', 'value'),
    Input('engine-filter', 'value'),
    Input('result-filter', 'value'),
    Input('opening-filter', 'value')
)
def update_search_graph(x_col, y_col, engine_filter, result_filter, opening_filter):
    _, search_filt = filter_data(engine_filter, result_filter, opening_filter)
    if x_col not in search_filt or y_col not in search_filt:
        return html.Div("Invalid X or Y column selected.")
    # Clean data
    scatter_data = search_filt[[x_col, y_col, 'name', 'result_label', 'game_id', 'engine_side']].dropna()
    if scatter_data.empty:
        return html.Div("No data for selected axes and filters.")
    fig = px.scatter(
        scatter_data, x=x_col, y=y_col, 
        color='name' if 'name' in scatter_data else None,
        symbol='result_label' if 'result_label' in scatter_data else None,
        hover_data=['game_id', 'engine_side', 'depth']
    )
    for name, g in scatter_data.groupby('name'):
        if len(g) < 3:
            continue
        try:
            coeffs = np.polyfit(g[x_col], g[y_col], 3)
            poly = np.poly1d(coeffs)
            x_sorted = np.sort(g[x_col])
            y_fit = poly(x_sorted)
            fig.add_scatter(x=x_sorted, y=y_fit, mode='lines', name=f"{name} trend ({x_col}-{y_col})")
        except Exception:
            continue
    fig.update_layout(
        legend_title_text='Engine',
        template='plotly_white',
        title=f"{y_col} vs {x_col} by Engine",
        xaxis_title=x_col,
        yaxis_title=y_col
    )
    return dcc.Graph(figure=fig)

# --- FEN ANALYSIS CALLBACK ---
@app.callback(
    Output('fen-analysis-container', 'children'),
    Input('fen-dropdown', 'value'),
    Input('engine-filter', 'value'),
    Input('result-filter', 'value'),
    Input('opening-filter', 'value')
)
def analyze_fen(fen_value, engine_filter, result_filter, opening_filter):
    _, search_filt = filter_data(engine_filter, result_filter, opening_filter)
    if not fen_value or fen_value not in search_filt['fen'].values:
        return html.Div("Please select a FEN.")
    rows = search_filt[search_filt['fen'] == fen_value]
    summary_stat = rows.agg(['mean', 'std', 'min', 'max'])
    # Show metrics for: eval, time_ms, depth, qdepth, nodes
    fields = [f for f in ['eval', 'time_ms', 'depth', 'qdepth', 'nodes', 'tt_hits', 'fail_highs', 'fail_lows'] if f in rows]
    stat_table = summary_stat[fields].T.reset_index()
    stat_table.columns = ['Metric', 'Mean', 'Std', 'Min', 'Max']
    figs = []
    for col in fields:
        fig = px.histogram(rows, x=col, color='name', nbins=24,
                           title=f"{col} distribution for selected FEN", marginal="box")
        figs.append(dcc.Graph(figure=fig, style={'height': '230px'}))
    pv_text = rows['principal_variation'].dropna().unique()
    return html.Div([
        html.H4(f"Selected FEN Analysis"),
        html.H5("Principal Variations Found:"),
        html.Ul([html.Li(p) for p in pv_text]),
        html.H5("Stat Overview:"),
        dash_table.DataTable(
            columns=[{"name": c, "id": c} for c in stat_table.columns],
            data=stat_table.to_dict('records'), page_size=8
        ),
        html.Div(figs)
    ])

# --- Run ---
if __name__ == "__main__":
    webbrowser.open("http://127.0.0.1:8050")
    app.run(debug=True, port=8050)