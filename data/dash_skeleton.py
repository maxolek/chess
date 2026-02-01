# dash_skeleton.py
import dash
from dash import dcc, html, dash_table
from dash.dependencies import Input, Output
import plotly.express as px
import sqlite3
from pathlib import Path
import pandas as pd
import webbrowser

# --- Load Data ---
def pull_data():
    lite_cnxn = sqlite3.connect(Path.home() / "Documents/databases/chess.db")

    games_df = pd.read_sql_query('SELECT * FROM games', lite_cnxn)
    engines_df = pd.read_sql_query('SELECT * FROM engines', lite_cnxn)
    search_df = pd.read_sql_query('SELECT * FROM searches', lite_cnxn)

    return engines_df, games_df, search_df

def combine_data(engines_df, games_df, search_df):
    # --- time control info ---
    def get_tc(row):
        if row['wtime'] > 0:
            return 'tc', f"{row['movestogo']}/{row['wtime']}+{row['winc']}"
        elif row['depth'] > 0:
            return 'depth', row['depth']
        elif row['nodes'] > 0:
            return 'nodes', row['nodes']
        elif row['movetime'] > 0:
            return 'movetime', row['movetime']
        return 'unknown', None

    games_df[['tc_type', 'tc_len']] = games_df.apply(lambda x: get_tc(x), axis=1, result_type='expand')

    # --- merge engine info ---
    search_df = search_df.merge(
        engines_df[['id','name','version']],
        how='left',
        left_on='engine_id',
        right_on='id'
    )

    # --- merge game info ---
    search_df = search_df.merge(
        games_df.drop(columns=['moves','ingestion_timestamp_utc']),
        how='left',
        left_on='game_id',
        right_on='id',
        suffixes=('_engine','_game')
    )

    # --- perspective / result from engine side ---
    def get_side(row):
        if row['engine_id'] == row['white_engine_id']:
            return 'white'
        elif row['engine_id'] == row['black_engine_id']:
            return 'black'
        return 'na'

    def get_result(row):
        if row['result'] == 'draw': return 0.5
        elif row['engine_side'] == row['result']: return 1
        else: return 0

    search_df['engine_side'] = search_df.apply(get_side, axis=1)
    search_df['result_perspective'] = search_df.apply(get_result, axis=1)

    return search_df, games_df, engines_df

engines_df, games_df, search_df = pull_data()
df, games_df, engines_df = combine_data(engines_df, games_df, search_df)

# --- Create Dash App ---
app = dash.Dash(__name__, suppress_callback_exceptions=True)
app.title = "Chess Analytics Dashboard"

# --- Dropdown Options ---
engine_options = [{"label": name, "value": id_} for id_, name in engines_df[['id','name']].drop_duplicates().values]
result_options = [{"label": res, "value": res} for res in games_df['result'].unique()]
opening_options = [{"label": op, "value": op} for op in games_df['opening'].dropna().unique()]

# --- Layout ---
app.layout = html.Div([
    html.H1("Chess Analytics Dashboard", style={'textAlign': 'center'}),

    # FILTER PANE
    html.Div([
        html.Label("Select Engine:"),
        dcc.Dropdown(id='engine-filter', options=engine_options, placeholder="All engines"),
        
        html.Label("Select Game Result:"),
        dcc.Dropdown(id='result-filter', options=result_options, placeholder="All results"),
        
        html.Label("Select Opening:"),
        dcc.Dropdown(id='opening-filter', options=opening_options, placeholder="All openings"),
    ], style={'width': '25%', 'display': 'inline-block', 'verticalAlign': 'top', 'padding': '10px'}),

    # METRIC SELECTION PANE
    html.Div([
        html.Label('x-axis: '),
        dcc.Dropdown(
            id='x-axis',
            options=[{'label': col, 'value': col} for col in df.select_dtypes(include='number').columns],
            value='depth_engine', # default
            clearable=False
        ),

        html.Label('y-axis: '),
        dcc.Dropdown(
            id='y-axis',
            options=[{'label': col, 'value': col} for col in df.select_dtypes(include='number').columns],
            value='nodes_engine', # default
            clearable=False
        ),
    ], style={'width': '50%', 'padding': '10px'}),

    # DATA PANE
    html.Div([
        # -- game table --
        html.H2("Game Overview"),
        dash_table.DataTable(
            id='game-table',
            columns=[{"name": c, "id": c} for c in games_df.columns],
            data=games_df.to_dict('records'),
            page_size=10,
            style_table={'overflowX': 'auto'}
        ),
        
        # -- metrics graph --
        html.H2("Engine Metrics"),
        dcc.Graph(id='engine-metrics-graph'),
        
        # -- search table --
        html.H2("Search/Position Features"),
        dash_table.DataTable(
            id='position-features-table',
            columns=[{"name": c, "id": c} for c in df.columns],
            data=df.to_dict('records'),
            page_size=10,
            style_table={'overflowX': 'auto'}
        ),
    ], style={'width': '70%', 'display': 'inline-block', 'padding': '10px', 'verticalAlign': 'top'})
])

# --- Callbacks ---
@app.callback(
    Output('game-table', 'data'),
    Output('engine-metrics-graph', 'figure'),
    Output('position-features-table', 'data'),
    Input('engine-filter', 'value'),
    Input('result-filter', 'value'),
    Input('opening-filter', 'value'),
    Input('x-axis', 'value'),
    Input('y-axis', 'value')
)
def update_dashboard(engine, result, opening, x_col, y_col):
    filtered_games = games_df.copy()
    
    # --- filter games ---
    if engine:
        filtered_games = filtered_games[
            (filtered_games['white_engine_id'] == engine) | 
            (filtered_games['black_engine_id'] == engine)
        ]
    if result:
        filtered_games = filtered_games[filtered_games['result'] == result]
    if opening:
        filtered_games = filtered_games[filtered_games['opening'] == opening]

    # --- filter searches ---
    filtered_search = df[df['game_id'].isin(filtered_games['id'].unique())]

    # --- build engine metrics figure ---
    if not filtered_search.empty and x_col in filtered_search.columns and y_col in filtered_search.columns:
        # group by engine name, average numeric metrics
        fig_df = filtered_search.groupby('name')[[x_col, y_col]].mean().reset_index()
        figure = {
            'data': [{
                'x': fig_df[x_col],
                'y': fig_df[y_col],
                'type': 'scatter',
                'mode': 'lines+markers',
                'name': 'Engine'
            }],
            'layout': {'title': f'{y_col} vs {x_col}'}
        }
    else:
        figure = {'data': [], 'layout': {'title': f'{y_col} vs {x_col}'}}

    return filtered_games.to_dict('records'), figure, filtered_search.to_dict('records')

# --- Run ---
if __name__ == "__main__":
    webbrowser.open("http://127.0.0.1:8050")
    app.run(debug=True)
