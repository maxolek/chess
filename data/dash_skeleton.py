# chess_dashboard_pretty.py
import dash
from dash import dcc, html, dash_table
from dash.dependencies import Input, Output, State
import pandas as pd
import plotly.express as px
import numpy as np
import sqlite3
from pathlib import Path
import platform
import webbrowser

# --- Load Data ---
system = platform.system()
DB_PATH = "F:/databases/chess.db" if system=="Windows" else Path.home()/ "Documents/databases/chess.db"
cnxn = sqlite3.connect(DB_PATH)

games_df = pd.read_sql_query("SELECT * FROM games", cnxn)
engines_df = pd.read_sql_query("SELECT * FROM engines", cnxn)
search_df = pd.read_sql_query("SELECT * FROM searches", cnxn)

# Merge engine info
search_df = search_df.merge(engines_df[['id','name','version']], how='left', left_on='engine_id', right_on='id')
search_df = search_df.merge(games_df.drop(columns=['moves','ingestion_timestamp_utc']), how='left',
                            left_on='game_id', right_on='id', suffixes=('_engine','_game'))

# Engine perspective
def get_side(row):
    if row['engine_id']==row['white_engine_id']: return 'white'
    elif row['engine_id']==row['black_engine_id']: return 'black'
    return 'na'

def get_result(row):
    if row['result']=='3': return 0.5
    elif row['engine_side']=='white' and row['result']=='1': return 1.0
    elif row['engine_side']=='black' and row['result']=='2': return 1.0
    return 0.0

search_df['engine_side'] = search_df.apply(get_side, axis=1)
search_df['result_perspective'] = search_df.apply(get_result, axis=1)
search_df['result_label'] = search_df['result_perspective'].map({1.0:'Win',0.5:'Draw',0.0:'Loss'})

# --- Dash App ---
app = dash.Dash(__name__, suppress_callback_exceptions=True)
app.title = "Chess Analytics Dashboard"

# Dropdown options
engine_options = [{"label": n, "value": i} for i,n in engines_df[['id','name']].drop_duplicates().values]
result_options = [{"label": r, "value": r} for r in search_df['result_label'].unique()]
opening_options = [{"label": o, "value": o} for o in games_df['opening'].dropna().unique()]

# --- Layout ---
app.layout = html.Div([
    html.H1("Chess Analytics Dashboard", style={'textAlign':'center','marginBottom':'20px'}),

    html.Div([
        # Sidebar Filters
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
        ], style={'width':'20%','padding':'15px','borderRight':'1px solid #ddd',
                  'backgroundColor':'#f9f9f9','height':'100vh','overflowY':'auto'}),

        # Main Panel
        html.Div([
            dcc.Tabs(id='tabs', value='tab-overview', children=[
                dcc.Tab(label='Overview', value='tab-overview'),
                dcc.Tab(label='Search Metrics', value='tab-search'),
                dcc.Tab(label='Games', value='tab-games'),
                dcc.Tab(label='Engine Comparison', value='tab-compare'),
            ]),
            html.Div(id='tabs-content', style={'marginTop':'20px'})
        ], style={'width':'80%','padding':'20px'})
    ], style={'display':'flex','flexDirection':'row'})
])

# --- Callbacks ---
@app.callback(
    Output('search-metrics-graph', 'figure'),
    Input('search-x-axis', 'value'),
    Input('search-y-axis', 'value'),
    Input('engine-filter', 'value'),
    Input('result-filter', 'value'),
    Input('opening-filter', 'value')
)
def update_search_graph(x_col, y_col, engine_filter, result_filter, opening_filter):
    filtered_games = games_df.copy()
    filtered_search = search_df.copy()

    if engine_filter:
        filtered_games = filtered_games[(filtered_games['white_engine_id'].isin(engine_filter)) |
                                        (filtered_games['black_engine_id'].isin(engine_filter))]
    if result_filter:
        filtered_search = filtered_search[filtered_search['result_label'].isin(result_filter)]
    if opening_filter:
        filtered_games = filtered_games[filtered_games['opening'].isin(opening_filter)]
    
    filtered_search = filtered_search[filtered_search['game_id'].isin(filtered_games['id'].unique())]

    filtered_search = filtered_search[filtered_search['depth_engine'] <= 30]

    # Build scatter
    fig = px.scatter(filtered_search, x=x_col, y=y_col, color='name', symbol='result_label',
                     hover_data=['game_id','engine_side','depth_engine','nodes_engine'])
    
    # Trendlines per engine
    for name, g in filtered_search.groupby('name'):
        g = g.dropna(subset=[x_col,y_col])
        if len(g) < 3: continue
        coeffs = np.polyfit(g[x_col], g[y_col], 2)
        poly = np.poly1d(coeffs)
        x_sorted = np.sort(g[x_col])
        y_fit = poly(x_sorted)
        fig.add_scatter(x=x_sorted, y=y_fit, mode='lines', name=f"{name} trend", line={'width':3})
    
    fig.update_layout(legend_title_text='Engine', template='plotly_white')
    fig.update_traces(opacity=0.6, selector=dict(mode='markers'))

    return fig


@app.callback(
    Output('tabs-content','children'),
    Input('tabs','value'),
    Input('engine-filter','value'),
    Input('result-filter','value'),
    Input('opening-filter','value')
)
def render_tab(tab, engine_filter, result_filter, opening_filter):
    # Apply filters
    filtered_games = games_df.copy()
    filtered_search = search_df.copy()
    if engine_filter:
        filtered_games = filtered_games[(filtered_games['white_engine_id'].isin(engine_filter))|
                                        (filtered_games['black_engine_id'].isin(engine_filter))]
    if result_filter:
        filtered_search = filtered_search[filtered_search['result_label'].isin(result_filter)]
    if opening_filter:
        filtered_games = filtered_games[filtered_games['opening'].isin(opening_filter)]
    filtered_search = filtered_search[filtered_search['game_id'].isin(filtered_games['id'].unique())]

    filtered_search = filtered_search[filtered_search['depth_engine'] <= 30]

    # --- Tab: Overview ---
    if tab=='tab-overview':
        # Summary stats per engine
        summary = filtered_search.groupby('name').agg(
            Games=('game_id','nunique'),
            Wins=('result_perspective', lambda x: (x==1.0).sum()),
            Draws=('result_perspective', lambda x: (x==0.5).sum()),
            Losses=('result_perspective', lambda x: (x==0.0).sum()),
        ).reset_index()
        summary['WinRate'] = summary['Wins']/summary['Games']
        summary['DrawRate'] = summary['Draws']/summary['Games']
        summary['LossRate'] = summary['Losses']/summary['Games']

        fig = px.bar(summary.melt(id_vars='name', value_vars=['WinRate','DrawRate','LossRate']),
                     x='name', y='value', color='variable',
                     barmode='group', text_auto='.2f', labels={'value':'Rate','name':'Engine','variable':'Result'})
        return html.Div([
            html.H3("Engine Performance Overview"),
            dcc.Graph(figure=fig)
        ])

    # --- Tab: Search Metrics ---
    elif tab=='tab-search':
        numeric_cols = filtered_search.select_dtypes(include='number').columns.tolist()
        
        # Default X/Y
        x_col, y_col = numeric_cols[0], numeric_cols[1] if len(numeric_cols) > 1 else numeric_cols[0]
        
        return html.Div([
            html.Div([
                html.Div([
                    html.Label("X-axis"),
                    dcc.Dropdown(
                        id='search-x-axis',
                        options=[{'label': c, 'value': c} for c in numeric_cols],
                        value=x_col,
                        clearable=False
                    )
                ], style={'width':'48%', 'display':'inline-block'}),

                html.Div([
                    html.Label("Y-axis"),
                    dcc.Dropdown(
                        id='search-y-axis',
                        options=[{'label': c, 'value': c} for c in numeric_cols],
                        value=y_col,
                        clearable=False
                    )
                ], style={'width':'48%', 'display':'inline-block', 'float':'right'}),
            ], style={'marginBottom':'15px'}),

            dcc.Graph(id='search-metrics-graph')
        ])


    # --- Tab: Games ---
    elif tab=='tab-games':
        return html.Div([
            html.H3("Games Table"),
            dash_table.DataTable(
                columns=[{"name":c,"id":c} for c in filtered_games.columns],
                data=filtered_games.to_dict('records'),
                page_size=10,
                sort_action="native",
                filter_action="native",
                style_table={'overflowX':'auto'},
                style_cell={'fontSize':'12px'}
            )
        ])

    # --- Tab: Engine Comparison ---
    elif tab=='tab-compare':
        # Pick top 2 engines (for simplicity)
        engines = filtered_search['name'].unique()[:2]
        if len(engines)<2:
            return html.Div("Select at least 2 engines for comparison.")
        comp_df = filtered_search[filtered_search['name'].isin(engines)]
        fig = px.box(comp_df, x='name', y='eval', color='result_label',
                     labels={'eval':'Evaluation','name':'Engine','result_label':'Result'})
        return html.Div([
            html.H3(f"Engine Comparison: {engines[0]} vs {engines[1]}"),
            dcc.Graph(figure=fig)
        ])

# --- Run ---
if __name__=="__main__":
    webbrowser.open("http://127.0.0.1:8050")
    app.run(debug=True)
