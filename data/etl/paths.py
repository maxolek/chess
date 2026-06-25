"""Project paths and constants."""
from pathlib import Path

DATA_DIR = Path(__file__).resolve().parent.parent  # data/
PROJECT_ROOT = DATA_DIR.parent
LOGS_DIR = PROJECT_ROOT / "logs"
GAMES_LOG_DIR = LOGS_DIR / "game_logs"
GAME_JSON = GAMES_LOG_DIR / "game.jsonl"
SEARCH_JSON = GAMES_LOG_DIR / "search.jsonl"
TIMING_JSON = GAMES_LOG_DIR / "timing.jsonl"
