"""ETL package for chess engine data ingestion and processing.

Submodules:
  - paths: project paths and constants
  - utils: small helper functions
  - openings: ECO/opening classification from move lists
  - db: database connection helpers and engine probing
  - ingest: bulk logging and ingestion functions (games, searches, timing, STS, SPRT)
"""
from .paths import DATA_DIR, PROJECT_ROOT, LOGS_DIR, GAMES_LOG_DIR, GAME_JSON, SEARCH_JSON, TIMING_JSON
from .utils import safe_val, safe
from .openings import get_opening_from_moves
from .db import get_db, get_engine_id, probe_engine_metadata, extract_engine_id_from_search, clear_log_dir
from .ingest import (
    log_games_directory,
    start_experiment,
    update_experiment,
    register_engine,
    log_perft,
    log_engine_ratings,
    log_sprt,
    bulk_log_sts,
    bulk_log_game,
    bulk_log_search_and_timing,
)
