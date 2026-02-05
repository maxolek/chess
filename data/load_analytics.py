import duckdb 
from pathlib import Path 
import platform
from . import etl

system = platform.system()

if system == "Windows":
    RAW_DB = 'F:/databases/chess.db'
    ANALYTICS_DB = 'F:/databases/chess_analytics.duckdb'
elif system == "Darwin":
    RAW_DB = Path.home() / "Documents/databases/chess.db"
    ANALYTICS_DB = Path.home() / "Documents/databases/chess_analytics.duckdb"

# connect raw_db to analytics_db
cnxn = duckdb.connect(ANALYTICS_DB)

cnxn.execute(f"""
    ATTACH '{RAW_DB}' AS raw (TYPE SQLITE)
""")

# udf helper functions
def opening_eco_udf(moves_list: str):
    # moves_list comes as a string from DuckDB
    moves = moves_list.split()
    eco, name = etl.get_opening_from_moves(moves)
    return [eco, name]  # return as a list of two strings

# Use DuckDB ListType for return
cnxn.create_function(
    "get_opening_from_moves",
    opening_eco_udf,
    parameters=["VARCHAR"],
    return_type="VARCHAR[]"
)

# copy tables

cnxn.execute("""
    CREATE OR REPLACE TABLE engines AS 
    SELECT * 
    FROM raw.engines    
""")

cnxn.execute("""
    CREATE OR REPLACE TABLE experiments AS 
    SELECT *
    FROM raw.experiments
""")

cnxn.execute("""
    CREATE OR REPLACE TABLE sprt_runs AS
    SELECT * REPLACE (
                list_extract(string_split(opening_book, '\\'), -1) AS opening_book
            )
    FROM raw.sprt    
""")

cnxn.execute("""
    CREATE OR REPLACE TABLE sts_runs AS
    SELECT * REPLACE (
                list_extract(string_split(suite, '\\'), -1) AS suite 
            ),
            trim(split_part(position_name, '-', 2)) as position_type
    FROM raw.sts          
""")

# not creating a PERFT table as analytics will focus on searches not testing

cnxn.execute("""
    CREATE OR REPLACE TABLE game_stats AS 
             
    WITH openings AS (
        SELECT *,
             -- (eco_code , name)
             get_opening_from_moves(moves) as opening_tuple
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
                END as termination,
                opening_tuple[1] as opening
            ),
        opening_tuple[0] as opening_eco 
    FROM openings
""")

cnxn.execute("""
    CREATE OR REPLACE TABLE search_stats AS 
    SELECT *
    FROM raw.searches          
""")

cnxn.execute("""
    CREATE OR REPLACE TABLE iterative_deepening_stats AS
    SELECT *
    FROM raw.searches_by_iteration
""")

cnxn.execute("""
    CREATE OR REPLACE TABLE search_tree_stats AS
    SELECT *
    FROM raw.searches_by_tree_depth
""")

cnxn.execute("""
    CREATE OR REPLACE TABLE search_timings AS
    SELECT *
    FROM raw.timing          
""")

cnxn.execute("""
    CREATE OR REPLACE TABLE dim_positions AS
    SELECT 
        id as search_id, fen, game_id, sts_id
    FROM raw.searches       
""")

cnxn.close()