"""Project paths and constants."""
import os
import platform
from pathlib import Path

DATA_DIR = Path(__file__).resolve().parent.parent  # data/
PROJECT_ROOT = DATA_DIR.parent
LOGS_DIR = PROJECT_ROOT / "logs"
GAMES_LOG_DIR = LOGS_DIR / "game_logs"
GAME_JSON = GAMES_LOG_DIR / "game.jsonl"
SEARCH_JSON = GAMES_LOG_DIR / "search.jsonl"
TIMING_JSON = GAMES_LOG_DIR / "timing.jsonl"

# ─────────────────────────────────────────────────────────────────────────────
# Database paths — configurable via CHESS_RAW_DB / CHESS_ANALYTICS_DB env vars
# ─────────────────────────────────────────────────────────────────────────────
_system = platform.system()

if _system == "Windows":
    _DEFAULT_RAW = Path("F:/databases/chess.db")
    _DEFAULT_ANALYTICS = Path("F:/databases/chess_analytics.duckdb")
elif _system == "Darwin":
    _DEFAULT_RAW = Path.home() / "Documents/databases/chess.db"
    _DEFAULT_ANALYTICS = Path.home() / "Documents/databases/chess_analytics.duckdb"
else:
    _DEFAULT_RAW = Path.home() / "Documents/databases/chess.db"
    _DEFAULT_ANALYTICS = Path.home() / "Documents/databases/chess_analytics.duckdb"

RAW_DB = Path(os.environ.get("CHESS_RAW_DB") or _DEFAULT_RAW)
ANALYTICS_DB = Path(os.environ.get("CHESS_ANALYTICS_DB") or _DEFAULT_ANALYTICS)
