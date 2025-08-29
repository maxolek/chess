import sqlite3
import numpy as np
import scipy.stats as st
from collections import defaultdict
import utils

DB_PATH = "engine_game_results.db"

# ----------------------
# Database Utilities
# ----------------------
def fetch_games(engine_a=None, engine_b=None):
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    query = "SELECT id, engine_version, opponent, result FROM games"
    conditions = []
    params = []
    if engine_a:
        conditions.append("engine_version = ?")
        params.append(engine_a)
    if engine_b:
        conditions.append("opponent = ?")
        params.append(engine_b)
    if conditions:
        query += " WHERE " + " AND ".join(conditions)
    c.execute(query, params)
    rows = c.fetchall()
    conn.close()
    return rows

def fetch_game_moves(game_id):
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute("SELECT eval_score, move_number FROM game_moves WHERE game_id = ?", (game_id,))
    rows = c.fetchall()
    conn.close()
    return rows

# ----------------------
# LOS / Confidence
# ----------------------
def los_confidence(wins, total_games, confidence=0.95):
    """Returns LOS and confidence interval for winning probability"""
    if total_games == 0:
        return 0.5, 0.5, 0.5
    los = wins / total_games
    ci_low, ci_up = st.binom.interval(confidence, total_games, los, loc=0)
    return los, ci_low / total_games, ci_up / total_games

# ----------------------
# Elo Calculation
# ----------------------
def elo_diff_from_score(score):
    """Elo difference from win rate score (0..1)"""
    if score <= 0:
        return -9999
    if score >= 1:
        return 9999
    return -400 * np.log10((1 - score) / score)

# ----------------------
# Compute Match Stats
# ----------------------
def compute_match_stats(engine_a, engine_b):
    games = fetch_games(engine_a, engine_b)
    total_games = len(games)
    wins_a = sum(1 for g in games if g[3] == "1-0")
    wins_b = sum(1 for g in games if g[3] == "0-1")
    draws  = sum(1 for g in games if g[3] == "1/2-1/2")

    score_a = (wins_a + 0.5 * draws) / total_games if total_games > 0 else 0.5
    elo_diff = elo_diff_from_score(score_a)
    los, los_low, los_high = los_confidence(wins_a + 0.5 * draws, total_games)

    return {
        "engine_a": engine_a,
        "engine_b": engine_b,
        "games_played": total_games,
        "wins_a": wins_a,
        "wins_b": wins_b,
        "draws": draws,
        "elo_diff": elo_diff,
        "los": los,
        "los_ci_low": los_low,
        "los_ci_high": los_high
    }

# ----------------------
# Eval -> Win Conversion
# ----------------------
def compute_eval_to_win(engine_version, binsize=50):
    """Compute win probability for each evaluation bin"""
    games = fetch_games(engine_a=engine_version)
    eval_bins = defaultdict(list)

    for game in games:
        game_id, _, _, result = game
        moves = fetch_game_moves(game_id)
        for eval_score, move_number in moves:
            # convert game result relative to side to move
            # assume odd moves = white, even moves = black (simplified)
            # +1 for win, 0 draw, -1 loss from white POV
            if result == "1-0":
                outcome = 1 if move_number % 2 == 1 else -1
            elif result == "0-1":
                outcome = -1 if move_number % 2 == 1 else 1
            else:
                outcome = 0
            bin_key = int(eval_score / binsize) * binsize
            eval_bins[bin_key].append(outcome)

    # compute probability of winning for each bin
    eval_win_curve = {}
    for bin_key, outcomes in eval_bins.items():
        outcomes_arr = np.array(outcomes)
        win_prob = np.mean((outcomes_arr == 1).astype(float) + 0.5 * (outcomes_arr == 0).astype(float))
        eval_win_curve[bin_key] = win_prob

    return eval_win_curve

# ----------------------
# Store match stats into DB
# ----------------------
def log_match_stats(match_stats):
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute("""
        CREATE TABLE IF NOT EXISTS match_stats (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            engine_a TEXT,
            engine_b TEXT,
            games_played INTEGER,
            wins_a INTEGER,
            wins_b INTEGER,
            draws INTEGER,
            elo_diff REAL,
            los REAL,
            los_ci_low REAL,
            los_ci_high REAL,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    """)
    columns = ", ".join(match_stats.keys())
    placeholders = ", ".join("?" * len(match_stats))
    values = list(match_stats.values())
    c.execute(f"INSERT INTO match_stats ({columns}) VALUES ({placeholders})", values)
    conn.commit()
    conn.close()

# ----------------------
# Example usage
# ----------------------
if __name__ == "__main__":
    # Compute match stats
    stats = compute_match_stats("v5.0", "Stockfish")
    print(stats)
    log_match_stats(stats)

    # Compute eval -> win conversion
    eval_curve = compute_eval_to_win("v5.0", binsize=50)
    for bin_key in sorted(eval_curve):
        print(f"Eval {bin_key:+5} -> Win Prob: {eval_curve[bin_key]:.3f}")
