import json
import sqlite3
from datetime import datetime
import os

DB_PATH = "engine_game_results.db"

import subprocess
import time

def start_engine(engine_path):
    """Start a UCI engine and return the subprocess."""
    proc = subprocess.Popen(
        engine_path,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1
    )
    return proc

def send(proc, cmd):
    """Send a UCI command to the engine."""
    if proc.poll() is not None:
        raise RuntimeError("Engine process closed")
    proc.stdin.write(cmd + "\n")
    proc.stdin.flush()

def expect_readyok(proc, timeout=5.0):
    """Wait for engine to respond 'readyok'."""
    t0 = time.time()
    while True:
        if time.time() - t0 > timeout:
            raise TimeoutError("Engine did not respond with readyok")
        line = proc.stdout.readline()
        if not line:
            continue
        if line.strip() == "readyok":
            return


# --- JSON Loader ---
def load_json(path):
    """Load a JSON file and return the data."""
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)

# --- EPD Loader ---
def load_epd(path):
    """
    Load an EPD file. Returns a list of dicts:
    [{'fen': ..., 'id': ..., 'other_fields': ...}, ...]
    """
    positions = []
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            # Split by semicolon or space
            parts = line.split(";")
            fen = parts[0].strip()
            fields = {}
            for p in parts[1:]:
                if not p.strip():
                    continue
                key_value = p.strip().split(" ", 1)
                if len(key_value) == 2:
                    fields[key_value[0]] = key_value[1]
            positions.append({"fen": fen, **fields})
    return positions

def parse_epd(epd_file):
    """
    Parse an EPD file into a list of (FEN, best_move) tuples.
    Each line should have the format:
        FEN ; bm <bestmove> ;
    Lines starting with # or empty lines are ignored.
    """
    positions = []
    with open(epd_file, "r") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split(";")
            fen = parts[0].strip()
            best_move = None
            for op in parts[1:]:
                op = op.strip()
                if op.startswith("bm"):
                    best_move = op[2:].strip()
            positions.append((fen, best_move))
    return positions


# --- Database Init ---
def init_db(db_path=DB_PATH):
    conn = sqlite3.connect(db_path)
    c = conn.cursor()

    # Table for individual moves
    c.execute("""
    CREATE TABLE IF NOT EXISTS game_moves (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        game_id INTEGER,
        move_number INTEGER,
        fen TEXT,
        move TEXT,
        bestmove TEXT,
        pv TEXT,
        nodes INTEGER,
        qnodes INTEGER,
        tt_hits INTEGER,
        tt_probes INTEGER,
        fail_highs INTEGER,
        fail_lows INTEGER,
        cut_nodes INTEGER,
        ordering_score REAL,
        eval_score REAL,
        elapsed_ms REAL
    )
    """)

    # Table for overall games
    c.execute("""
    CREATE TABLE IF NOT EXISTS games (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        engine_version TEXT,
        opponent TEXT,
        result TEXT,
        total_nodes INTEGER,
        total_qnodes INTEGER,
        avg_nps REAL,
        start_time DATETIME,
        end_time DATETIME
    )
    """)

    # --- New tables for engine positions & iterative stats ---
    c.execute("""
    CREATE TABLE IF NOT EXISTS positions (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        fen TEXT NOT NULL,
        bestmove TEXT,
        ponder TEXT,
        elapsed_ms INTEGER,
        ok BOOLEAN,
        error TEXT,
        root_researches INTEGER
    )
    """)
    c.execute("""
    CREATE TABLE IF NOT EXISTS iter_stats (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        position_id INTEGER,
        depth INTEGER,
        seldepth INTEGER,
        nodes INTEGER,
        qnodes INTEGER,
        fail_highs INTEGER,
        fail_lows INTEGER,
        tt_hits INTEGER,
        tt_probes INTEGER,
        ordering_first_good INTEGER,
        cut_nodes INTEGER,
        bestmove TEXT,
        eval_cp INTEGER,
        pv_changed BOOLEAN,
        eval_delta INTEGER,
        q_ratio REAL,
        ebf REAL,
        FOREIGN KEY(position_id) REFERENCES positions(id)
    )
    """)

    conn.commit()
    conn.close()

# --- Logging functions ---
def log_game_move(game_id, move_dict, db_path=DB_PATH):
    conn = sqlite3.connect(db_path)
    c = conn.cursor()
    columns = ", ".join(move_dict.keys())
    placeholders = ", ".join("?" * len(move_dict))
    values = list(move_dict.values())
    c.execute(f"INSERT INTO game_moves (game_id, {columns}) VALUES ({placeholders})", values)
    conn.commit()
    conn.close()

def log_game_summary(game_summary, db_path=DB_PATH):
    conn = sqlite3.connect(db_path)
    c = conn.cursor()
    columns = ", ".join(game_summary.keys())
    placeholders = ", ".join("?" * len(game_summary))
    values = list(game_summary.values())
    c.execute(f"INSERT INTO games ({columns}) VALUES ({placeholders})", values)
    conn.commit()
    conn.close()

# --- New: Logging positions & iterative stats ---
def log_position(snapshot, db_path=DB_PATH):
    """
    Logs a position snapshot into the 'positions' table.
    Returns the inserted position_id.
    """
    conn = sqlite3.connect(db_path)
    c = conn.cursor()
    c.execute("""
    INSERT INTO positions (fen, bestmove, ponder, elapsed_ms, ok, error, root_researches)
    VALUES (?, ?, ?, ?, ?, ?, ?)
    """, (
        snapshot["fen"],
        snapshot.get("bestmove"),
        snapshot.get("ponder"),
        snapshot.get("elapsed_ms"),
        snapshot.get("ok"),
        snapshot.get("error"),
        snapshot.get("root_researches")
    ))
    position_id = c.lastrowid
    conn.commit()
    conn.close()
    return position_id

def log_iter_stats(position_id, iter_list, db_path=DB_PATH):
    """
    Logs a list of iteration stats dictionaries for a given position_id.
    Each dict should contain:
    depth, seldepth, nodes, qnodes, fail_highs, fail_lows,
    tt_hits, tt_probes, ordering_first_good, cut_nodes,
    bestmove, eval_cp, pv_changed, eval_delta, q_ratio, ebf
    """
    conn = sqlite3.connect(db_path)
    c = conn.cursor()
    for iter_stats in iter_list:
        c.execute("""
        INSERT INTO iter_stats (
            position_id, depth, seldepth, nodes, qnodes, fail_highs, fail_lows,
            tt_hits, tt_probes, ordering_first_good, cut_nodes,
            bestmove, eval_cp, pv_changed, eval_delta, q_ratio, ebf
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, (
            position_id,
            iter_stats.get("depth"),
            iter_stats.get("seldepth"),
            iter_stats.get("nodes"),
            iter_stats.get("qnodes"),
            iter_stats.get("fail_highs"),
            iter_stats.get("fail_lows"),
            iter_stats.get("tt_hits"),
            iter_stats.get("tt_probes"),
            iter_stats.get("ordering_first_good"),
            iter_stats.get("cut_nodes"),
            iter_stats.get("bestmove"),
            iter_stats.get("eval_cp"),
            iter_stats.get("pv_changed"),
            iter_stats.get("eval_delta"),
            iter_stats.get("q_ratio"),
            iter_stats.get("ebf")
        ))
    conn.commit()
    conn.close()

# --- Fetch results ---
def fetch_results(query="SELECT * FROM games", db_path=DB_PATH):
    conn = sqlite3.connect(db_path)
    c = conn.cursor()
    c.execute(query)
    rows = c.fetchall()
    conn.close()
    return rows

# --- UCI Info Parsing ---
def parse_uci_info_line(line):
    """
    Parses a UCI 'info' line and extracts metrics.
    Returns a dict with fields:
    nodes, nps, qnodes, tt_hits, tt_probes,
    fail_highs, fail_lows, cut_nodes, eval_score, ordering_score, pv
    """
    tokens = line.strip().split()
    data = {
        "nodes": 0,
        "nps": 0,
        "qnodes": 0,
        "tt_hits": 0,
        "tt_probes": 0,
        "fail_highs": 0,
        "fail_lows": 0,
        "cut_nodes": 0,
        "eval_score": 0,
        "ordering_score": 0.0,
        "pv": ""
    }

    i = 0
    while i < len(tokens):
        if tokens[i] == "nodes" and i+1 < len(tokens):
            data["nodes"] = int(tokens[i+1]); i += 2
        elif tokens[i] == "nps" and i+1 < len(tokens):
            data["nps"] = int(tokens[i+1]); i += 2
        elif tokens[i] == "qnodes" and i+1 < len(tokens):
            data["qnodes"] = int(tokens[i+1]); i += 2
        elif tokens[i] == "tt_hits" and i+1 < len(tokens):
            data["tt_hits"] = int(tokens[i+1]); i += 2
        elif tokens[i] == "tt_probes" and i+1 < len(tokens):
            data["tt_probes"] = int(tokens[i+1]); i += 2
        elif tokens[i] == "fail_highs" and i+1 < len(tokens):
            data["fail_highs"] = int(tokens[i+1]); i += 2
        elif tokens[i] == "fail_lows" and i+1 < len(tokens):
            data["fail_lows"] = int(tokens[i+1]); i += 2
        elif tokens[i] == "cut_nodes" and i+1 < len(tokens):
            data["cut_nodes"] = int(tokens[i+1]); i += 2
        elif tokens[i] == "ordering_score" and i+1 < len(tokens):
            data["ordering_score"] = float(tokens[i+1]); i += 2
        elif tokens[i] == "score" and i+2 < len(tokens):
            if tokens[i+1] == "cp":
                data["eval_score"] = int(tokens[i+2])
            elif tokens[i+1] == "mate":
                data["eval_score"] = 100000 if int(tokens[i+2]) > 0 else -100000
            i += 3
        elif tokens[i] == "pv":
            data["pv"] = " ".join(tokens[i+1:])
            break
        else:
            i += 1
    return data

# --- Read engine stats log ---
def read_stats_log(path="stats.log"):
    """
    Reads the engine's stats log file and returns a list of parsed info dicts.
    """
    if not os.path.exists(path):
        return []
    parsed = []
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or not line.startswith("info depth"):
                continue
            parsed.append(parse_uci_info_line(line))
    return parsed

def safe_send(proc, cmd):
    if proc.poll() is not None:  # process exited
        raise RuntimeError("Engine process has exited")
    proc.stdin.write(cmd + "\n")
    proc.stdin.flush()
