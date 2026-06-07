import duckdb 
from pathlib import Path 
import platform
import os
from . import etl

system = platform.system()

if system == "Windows":
    RAW_DB = Path('F:/databases/chess.db')
    DEFAULT_ANALYTICS_DB = Path('F:/databases/chess_analytics.duckdb')
elif system == "Darwin":
    RAW_DB = Path.home() / "Documents/databases/chess.db"
    DEFAULT_ANALYTICS_DB = Path.home() / "Documents/databases/chess_analytics.duckdb"
else:
    RAW_DB = Path.home() / "Documents/databases/chess.db"
    DEFAULT_ANALYTICS_DB = Path.home() / "Documents/databases/chess_analytics.duckdb"

# Allow overriding the analytics DB path via environment variable to avoid locked file.
ANALYTICS_DB = Path(os.environ.get('CHESS_ANALYTICS_DB') or DEFAULT_ANALYTICS_DB)

# connect raw_db to analytics_db
cnxn = duckdb.connect(str(ANALYTICS_DB))

cnxn.execute(f"""
    ATTACH '{RAW_DB}' AS raw (TYPE SQLITE)
""")


# udf helper functions
def opening_name_udf(moves_list: str):
    try:
        eco, name = etl.get_opening_from_moves(moves_list)
        return name or ""
    except Exception:
        return ""


def opening_code_udf(moves_list: str):
    try:
        eco, name = etl.get_opening_from_moves(moves_list)
        return eco or ""
    except Exception:
        return ""

# Register scalar UDFs for opening name and ECO code
cnxn.create_function(
    "get_opening_name",
    opening_name_udf,
    parameters=["VARCHAR"],
    return_type="VARCHAR"
)
cnxn.create_function(
    "get_opening_code",
    opening_code_udf,
    parameters=["VARCHAR"],
    return_type="VARCHAR"
)

# copy tables

cnxn.execute("DROP TABLE IF EXISTS engines")
cnxn.execute("""
    CREATE TABLE engines AS 
    SELECT * 
    FROM raw.engines    
""")

cnxn.execute("DROP TABLE IF EXISTS experiments")
cnxn.execute("""
    CREATE TABLE experiments AS 
    SELECT *
    FROM raw.experiments
""")

cnxn.execute("DROP TABLE IF EXISTS sprt_runs")
cnxn.execute("""
    CREATE TABLE sprt_runs AS
    SELECT * REPLACE (
                CAST(alpha AS DOUBLE) AS alpha,
                CAST(beta AS DOUBLE) AS beta,
                list_extract(string_split(opening_book, '\\'), -1) AS opening_book,
                CAST((COALESCE(elo1,0) - COALESCE(elo0,0)) AS DOUBLE) AS elo_diff
            )
    FROM raw.sprt    
""")

cnxn.execute("DROP TABLE IF EXISTS sts_runs")
cnxn.execute("""
    CREATE TABLE sts_runs AS
    SELECT * REPLACE (
                list_extract(string_split(suite, '\\'), -1) AS suite 
            ) ,
            trim(split_part(position_name, '-', 2)) as position_type
    FROM raw.sts          
""")

# not creating a PERFT table as analytics will focus on searches not testing

cnxn.execute("DROP TABLE IF EXISTS game_stats")
cnxn.execute("""
    CREATE TABLE game_stats AS 
             
    WITH openings AS (
        SELECT *,
             -- (eco_code , name)
             get_opening_name(moves) as opening,
             get_opening_code(moves) as opening_eco
        FROM raw.games         
    )

    SELECT * REPLACE (
                CASE
                    WHEN result = 1 THEN 'white'
                    WHEN result = 2 THEN 'black'
                    WHEN result = 3 THEN 'draw'
                END as result,
                CASE
                    WHEN termination = 1 THEN 'checkmate'
                    WHEN termination = 2 THEN 'stalemate'
                    WHEN termination = 3 THEN 'threefold'
                    WHEN termination = 4 THEN 'fiftymove'
                    WHEN termination = 5 THEN 'time'
                    WHEN termination = 6 THEN 'resign'
                END as termination
            )
        -- opening and opening_eco are provided by the UDFs in the CTE
        FROM openings
""")

# Ensure opening columns are populated (compute directly from `moves` if necessary).
cnxn.execute("""
UPDATE game_stats
SET opening = get_opening_name(moves),
    opening_eco = get_opening_code(moves)
WHERE opening IS NULL OR opening = ''
""")

# NOTE: opening/opening_eco are created directly by the UDFs in the CTE above.

cnxn.execute("DROP TABLE IF EXISTS search_stats")
cnxn.execute("""
    CREATE TABLE search_stats AS 
    SELECT *
    FROM raw.searches          
""")

cnxn.execute("DROP TABLE IF EXISTS iterative_deepening_stats")
cnxn.execute("""
    CREATE TABLE iterative_deepening_stats AS
    SELECT *
    FROM raw.searches_by_iteration
""")

cnxn.execute("DROP TABLE IF EXISTS search_tree_stats")
cnxn.execute("""
    CREATE TABLE search_tree_stats AS
    SELECT *
    FROM raw.searches_by_tree_depth
""")

cnxn.execute("DROP TABLE IF EXISTS search_timings")
cnxn.execute("""
    CREATE TABLE search_timings AS
    SELECT *
    FROM raw.timing          
""")

cnxn.execute("DROP TABLE IF EXISTS dim_positions")
cnxn.execute("""
    CREATE TABLE dim_positions AS
    SELECT 
        id as search_id, fen, game_id, sts_id
    FROM raw.searches       
""")

cnxn.close()