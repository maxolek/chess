import sqlite3
from pathlib import Path 
import json
import subprocess
import glob
import os
import shutil
import argparse
import chess
import platform

# paths
DATA_DIR = Path(__file__).resolve().parent 
PROJECT_ROOT = DATA_DIR.parent 
# log paths
LOGS_DIR = PROJECT_ROOT / "logs"
GAMES_LOG_DIR = LOGS_DIR / "game_logs"
GAME_JSON = GAMES_LOG_DIR / "game.jsonl"
SEARCH_JSON = GAMES_LOG_DIR / "search.jsonl"
TIMING_JSON = GAMES_LOG_DIR / "timing.jsonl"

# utils

def safe_val(val):
    if isinstance(val, list) or isinstance(val, dict):
        return json.dumps(val)
    return val

def safe(arr, i):
    return arr[i] if i < len(arr) else 0

def get_opening_from_moves(moves):
    """
    Accepts either a list of move tokens or a space-separated moves string.
    Tokens may be UCI (e2e4) or SAN (Nf3). Returns a tuple (eco, name)
    or ("", "") when no opening is matched.
    """

    # Normalize input to list of tokens. Accept:
    # - a list of tokens
    # - a space-separated string of moves
    # - a JSON array string like '["e2e4", "e7e5"]'
    tokens = []
    if moves is None:
        return "", ""
    if isinstance(moves, str):
        s = moves.strip()
        # JSON array string?
        if s.startswith('[') and s.endswith(']'):
            try:
                import json
                parsed = json.loads(s)
                if isinstance(parsed, list):
                    tokens = parsed
                else:
                    tokens = s.split()
            except Exception:
                tokens = s.split()
        else:
            tokens = s.split()
    else:
        try:
            tokens = list(moves)
        except Exception:
            return "", ""

    board = chess.Board()
    uci_moves = []

    for tok in tokens:
        # try UCI first
        parsed = None
        try:
            mv = chess.Move.from_uci(tok)
            if mv in board.legal_moves:
                board.push(mv)
                uci_moves.append(mv.uci())
                continue
        except Exception:
            pass

        # try SAN
        try:
            mv = board.parse_san(tok)
            board.push(mv)
            uci_moves.append(mv.uci())
            continue
        except Exception:
            # stop processing on first unparsable token
            break

    # Attempt to use python-chess openings database if available. Some
    # installations of python-chess do not include `chess.openings`; in
    # that case, fall back to a simple heuristic: label the opening by the
    # first few UCI moves (this ensures `game_stats.opening` is always
    # populated upstream).
    try:
        if hasattr(chess, 'openings') and hasattr(chess.openings, 'ENCODED'):
            board = chess.Board()
            last_eco = None
            last_name = None
            for move_uci in uci_moves:
                try:
                    board.push_uci(move_uci)
                except ValueError:
                    break

                matched = False
                for opening in chess.openings.ENCODED:
                    board_full_fen = board.fen()
                    board_placement = board_full_fen.split(' ')[0]
                    for pos in opening.get('positions', []):
                        if pos == board_full_fen or pos == board_placement:
                            last_eco = opening.get('eco')
                            last_name = opening.get('name')
                            matched = True
                            break
                    if matched:
                        break

                if not matched:
                    break

            return last_eco or "", last_name or ""
    except Exception:
        # If anything goes wrong with the openings lookup, fall through
        # to the heuristic below.
        pass

    # First attempt: load a canonical ECO/openings database if available.
    # Format expected: a JSON array of objects {"eco": "C60", "name": "Ruy Lopez", "moves": ["e2e4","e7e5", ...]}
    try:
        eco_file = Path(__file__).resolve().parent / "openings" / "eco_db.json"
        if eco_file.exists():
            with open(eco_file, 'r') as f:
                eco_list = json.load(f)

            # Build prefix placements for the current game moves so we can match by position.
            prefix_placements = []
            try:
                b = chess.Board()
                for mv in uci_moves:
                    b.push_uci(mv)
                    prefix_placements.append(b.fen().split(' ')[0])
            except Exception:
                prefix_placements = []

            # Exact-match: prefer longest exact-matching move sequence
            best_match = (None, None, 0)  # (eco, name, matched_len)
            for entry in eco_list:
                moves_seq = entry.get("moves") or []
                if not moves_seq:
                    continue
                L = len(moves_seq)
                # If moves are provided, try exact UCI-sequence match first
                if len(uci_moves) >= L and tuple(uci_moves[:L]) == tuple(moves_seq):
                    if L > best_match[2]:
                        best_match = (entry.get("eco",""), entry.get("name",""), L)
                    continue

                # Otherwise, try matching by intermediate board placements if moves available in DB
                try:
                    eb = chess.Board()
                    entry_placements = []
                    for em in moves_seq:
                        eb.push_uci(em)
                        entry_placements.append(eb.fen().split(' ')[0])
                    # if entry_placements is a prefix of our prefix_placements, we have a match
                    if len(entry_placements) <= len(prefix_placements) and entry_placements == prefix_placements[:len(entry_placements)]:
                        if len(entry_placements) > best_match[2]:
                            best_match = (entry.get("eco",""), entry.get("name",""), len(entry_placements))
                except Exception:
                    pass

            if best_match[0] is not None:
                return best_match[0], best_match[1]

            # Longest-prefix fuzzy match: try decreasing lengths based on UCI moves
            for L in range(min(10, len(uci_moves)), 0, -1):
                prefix = tuple(uci_moves[:L])
                for entry in eco_list:
                    moves_seq = entry.get("moves") or []
                    if len(moves_seq) >= L and tuple(moves_seq[:L]) == prefix:
                        return entry.get("eco",""), entry.get("name","")
    except Exception:
        # If anything goes wrong loading the file, fall back to simple prefixes below
        pass

    # Fallback: attempt a small built-in ECO prefix map for common openings.
    ECO_PREFIXES = {
        # key: tuple of moves (uci), value: (eco, name)
        ("e2e4","e7e5","g1f3","b8c6","f1b5"): ("C60", "Ruy Lopez"),
        ("e2e4","c7c5"): ("B20", "Sicilian Defence"),
        ("e2e4","e7e6"): ("C00", "French Defence"),
        ("e2e4","c7c6"): ("B10", "Caro-Kann Defence"),
        ("d2d4","d7d5","c2c4"): ("D06", "Queen's Gambit"),
        ("d2d4","g8f6","c2c4","g7g6"): ("E60", "King's Indian Defence"),
        ("c2c4","e7e5"): ("A21", "English Opening: Symmetrical"),
        ("e2e4","e7e5","g1f3","g8f6"): ("C40", "King's Knight Opening"),
    }

    if uci_moves:
        # Try longest-prefix match
        for L in range(min(6, len(uci_moves)), 0, -1):
            prefix = tuple(uci_moves[:L])
            for key, val in ECO_PREFIXES.items():
                if prefix[:len(key)] == key:
                    return val

        # Otherwise return a descriptive label made from the first few moves
        name = ' '.join(uci_moves[:6])
        return "", name
    return "", ""


# --------------------------
#        CLEAR LOGS
# --------------------------

def clear_log_dir(log_dir):
    if not os.path.isdir(log_dir):
        return  # nothing to do

    for entry in os.listdir(log_dir):
        path = os.path.join(log_dir, entry)
        try:
            if os.path.isfile(path) or os.path.islink(path):
                os.unlink(path)
            else:
                shutil.rmtree(path)
        except Exception as e:
            print(f"[WARN] Failed to delete {path}: {e}")

# --------------------------
#         GETTERS
# --------------------------

def get_db(path="F:/databases/chess.db"):
    cnxn = sqlite3.connect(path)
    cnxn.row_factory = sqlite3.Row 
    return cnxn

def get_engine_id(cnxn, version=None):
    row = cnxn.execute(
        """
        SELECT id FROM engines
        WHERE version=?
        """,
        (
            version,
        )
    ).fetchone()

    if row is None: return None 
    else: return row[0]


import time

def probe_engine_metadata(engine_path, timeout=10.0):
    """
    Query a chess engine via UCI and extract version info.
    Handles Windows .exe extension automatically.
    Forces kill if engine hangs.
    """
    system = platform.system()
    engine_path = os.path.abspath(engine_path)

    # Auto-append .exe on Windows if missing
    if system == "Windows" and not engine_path.lower().endswith(".exe"):
        if os.path.exists(engine_path + ".exe"):
            engine_path += ".exe"

    if not os.path.exists(engine_path):
        raise FileNotFoundError(f"Engine not found at path: {engine_path}")

    p = subprocess.Popen(
        [engine_path],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        bufsize=1
    )

    meta = {}
    start = time.time()
    try:
        # Send UCI
        p.stdin.write("uci\n")
        p.stdin.flush()

        while True:
            if time.time() - start > timeout:
                raise RuntimeError(f"Timeout waiting for UCI response from {engine_path}")

            line = p.stdout.readline()
            if not line:
                time.sleep(0.01)
                continue

            line = line.strip()
            if line.startswith("id version"):
                meta["version"] = line[len("id version"):].strip()
            elif line == "uciok":
                break

        # Try to quit cleanly
        try:
            p.stdin.write("quit\n")
            p.stdin.flush()
            p.wait(timeout=1)
        except subprocess.TimeoutExpired:
            p.kill()

    finally:
        if p.poll() is None:
            p.kill()

    if "version" not in meta:
        raise RuntimeError(f"Failed to probe engine metadata: {engine_path}")

    return meta


def extract_engine_id_from_search(search_path):
    with open(search_path, "r") as f:
        for line in f:
            data = json.loads(line.strip())
            if "engine_id" in data:
                return data["engine_id"]
    return None

# -------------------------------
#       LOG GAMES DIRECTORY
# -------------------------------

def log_games_directory(cnxn):
    game_map = {}

    # ---- games ----
    if Path(GAME_JSON).is_file():
        game_map = bulk_log_game(cnxn, GAME_JSON)
    else:
        print(f"[INFO] No game log found: {GAME_JSON}")

    # ---- searches / timing ----
    if Path(SEARCH_JSON).is_file():
        bulk_log_search_and_timing(
            cnxn,
            SEARCH_JSON,
            game_map,
            timing_path=TIMING_JSON if Path(TIMING_JSON).is_file() else None
        )
    else:
        print(f"[INFO] No search log found: {SEARCH_JSON}")

    clear_log_dir(GAMES_LOG_DIR)

# -------------------------------
#   LOG SINGLE ROW TO DATABASE
# -------------------------------

# start experiment and get ID for FKs
def start_experiment(cnxn, experiment, engine_id, comparison_engine_id=None, info=None):
    cursor = cnxn.cursor()
    cursor.execute(
        """
        INSERT INTO experiments (engine_id, type, comparison_engine_id, metadata, start_time_utc)
        VALUES (?, ?, ?, ?, CURRENT_TIMESTAMP)
        """,
        (engine_id, experiment, comparison_engine_id, info)
    )
    cnxn.commit()
    cursor.execute("SELECT last_insert_rowid()")
    return cursor.fetchone()[0]

# update experiment rows (e.g. adding runtime)
def update_experiment(cnxn, experiment_id, info=None):
    if not info: return

    cols = ", ".join(f"{col} = ?" for col in info.keys())
    values = list(info.values()) + [experiment_id]

    sql = f"""
        UPDATE experiments
        SET {cols}
        WHERE id = ?
    """

    cur = cnxn.cursor()
    cur.execute(sql, values)
    cnxn.commit()


# returns row_id from respsective table of inserted row

def register_engine(cnxn, engine):

    cur = cnxn.execute(
        """
        INSERT INTO engines (
            name, version, description, compile_flags, uci_options
        )
        VALUES (?, ?, ?, ?, ?)
        """,
        (
            engine["name"],
            engine["version"],
            engine.get("description"),
            engine.get("compile_flags"),
            json.dumps(engine.get("uci_options")),
        )
    )

    cnxn.commit()
    return cur.lastrowid


def log_perft(cnxn, perft_info):

    cur = cnxn.execute(
        """
        INSERT INTO perft (
            experiment_id, fen, depth, nodes, expected_nodes, correct, time_ms
        )
        VALUES (?, ?, ?, ?, ?, ?, ?)
        """,
        (
            perft_info["experiment_id"],
            perft_info["fen"],
            perft_info.get("depth"),
            perft_info.get("nodes"),
            perft_info.get("expected_nodes"),
            perft_info.get("correct"),
            perft_info.get("time_ms"),
        )
    )

    cnxn.commit()
    return cur.lastrowid


def log_sprt(cnxn, experiment_id, candidate_engine_id, baseline_engine_id, **kwargs):
    """
    Logs an SPRT experiment to the database.
    kwargs can include optional parameters like elo0, elo1, alpha, beta, etc.
    """
    cur = cnxn.execute(
        """
        INSERT INTO sprt (
            experiment_id, baseline_engine_id, candidate_engine_id, 
            opening_book, book_depth, time_control, time_per_move,
            depth_per_move, elo0, elo1, alpha, beta, result, elo_diff,
            llr, los, candidate_wins, candidate_losses, candidate_draws,
            candidate_white_wins, candidate_white_losses, candidate_white_draws,
            candidate_black_wins, candidate_black_losses, candidate_black_draws,
            games_played, run_time_s
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        (
            experiment_id,
            baseline_engine_id,
            candidate_engine_id,
            kwargs.get("book"),
            kwargs.get("book_depth"),
            kwargs.get("tc"),
            kwargs.get("time"),
            kwargs.get("depth"),
            kwargs.get("elo0"),
            kwargs.get("elo1"),
            kwargs.get("alpha"),
            kwargs.get("beta"),
            kwargs.get("result"),
            kwargs.get("elo_diff"),
            kwargs.get("llr"),
            kwargs.get("los"),
            kwargs.get("candidate_wins"),
            kwargs.get("candidate_losses"),
            kwargs.get("candidate_draws"),
            kwargs.get("candidate_white_wins"),
            kwargs.get("candidate_white_losses"),
            kwargs.get("candidate_white_draws"),
            kwargs.get("candidate_black_wins"),
            kwargs.get("candidate_black_losses"),
            kwargs.get("candidate_black_draws"),
            kwargs.get("games_played"),
            kwargs.get("runtime"),
        )
    )
    cnxn.commit()
    return cur.lastrowid


# -------------------------------
#   LOG FULL FILE TO DATABASE
# -------------------------------

# instead of inserting and returning a single row (which sometimes is necessary)
# we will have .jsonl files that we would like to bulk load into the table
# instead of iterating row by row through the .jsonl 
#   (we essentially do that anyway but use executemany() to avoid slow table addition)

# will read .jsonl dumps from engine and load into tables
# dont return row id (since may not be single valued e.g. search)
def bulk_log_sts(cnxn, path, sts_id, **kwargs):
    rows = []

    with open(path, "r") as f:
        for line in f:
            data = json.loads(line.strip())

            move = data.get('engine_move')
            # most STS has only 1 best move, some have 2
            expected_moves = data.get('expected_moves') or []
            expected_move_1 = expected_moves[0] if len(expected_moves) > 0 else None
            expected_move_2 = expected_moves[1] if len(expected_moves) > 1 else None
            avoid_moves = data.get('avoid_moves') or []

            if expected_moves != []:
                correct = (move == expected_move_1 or move == expected_move_2)
            elif avoid_moves != []:
                correct = (move != avoid_moves[0])

            rows.append((
                sts_id,
                data.get("epd_file"),
                data.get("category"),
                data.get("fen"),
                kwargs.get("time"),
                kwargs.get("depth"),
                move,
                data.get("engine_score"),
                expected_move_1,
                data.get("expected_score"),
                expected_move_2,
                avoid_moves[0] if avoid_moves else None,
                correct
            ))
            
    cnxn.executemany(
        """
        INSERT INTO sts (
            experiment_id, suite, position_name,
            fen, search_time_ms, search_depth, 
            engine_move, engine_score, expected_move, expected_score,
            alt_expected_move, avoid_move, move_is_correct
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        rows
    )
    cnxn.commit()


def bulk_log_game(cnxn, path, experiment_id=None, second_engine_id=None):
    cursor = cnxn.cursor()

    rows = []
    game_uuids = []

    white_engine_id = None 
    black_engine_id = None 


    with open(path, "r") as f:
        for line in f:
            data = json.loads(line.strip())

            game_uuids.append(data["game_uuid"])
            if data.get('side') == "white":
                white_engine_id = get_engine_id(cnxn, data.get('engine_id')) # data.id stores version
                black_engine_id = second_engine_id
            else:
                white_engine_id = second_engine_id
                black_engine_id = get_engine_id(cnxn, data.get('engine_id'))

            rows.append((
                experiment_id,
                white_engine_id,
                black_engine_id,
                data.get("wtime"),
                data.get("btime"),
                data.get("winc"),
                data.get("binc"),
                data.get("movestogo"),
                data.get("depth"),
                data.get("nodes"),
                data.get("movetime"),
                data.get("result"),
                data.get("reason"),
                data.get("opening"),
                data.get("start_fen"),
                json.dumps(data.get("moves")),
                data.get("time_s")
            ))

    cursor.executemany(
        """
        INSERT INTO games (
            experiment_id,
            white_engine_id, black_engine_id,
            wtime, btime, winc, binc, movestogo, depth, nodes, movetime,
            result, termination,
            opening, start_fen, moves, run_time_s
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        rows
    )

    # Recover inserted IDs deterministically
    cursor.execute(
        "SELECT id FROM games ORDER BY id DESC LIMIT ?",
        (len(rows),)
    )
    game_ids = [r[0] for r in cursor.fetchall()][::-1]

    cnxn.commit()
    return dict(zip(game_uuids, game_ids))


def bulk_log_search_and_timing(
    cnxn,
    search_path,
    game_map,
    engine_id=None,
    timing_path=None,
    sts_id=None
):
    cursor = cnxn.cursor()

    searches_rows = []
    uuid_map = {}  # search_uuid -> search_id
    game_id = None

    if engine_id is None:
        engine_version = extract_engine_id_from_search(search_path)
        if engine_version is None:
            raise RuntimeError("engine_version not provided and not found in search log")

        engine_id = get_engine_id(cnxn, engine_version)
        if engine_id is None:
            raise RuntimeError("engine_id not provided and not found in database")
        
    # ----------------
    # --- SEARCHES ---
    # ----------------

    # full search summary
    with open(search_path, "r") as f:
        for line in f:
            data = json.loads(line.strip())

            if not sts_id or game_map == {}:
                game_uuid = data["game_uuid"]
                game_id = game_map.get(game_uuid)

            searches_rows.append((
                engine_id,
                game_id, # one or the other, or neither for an offline search
                sts_id,
                data.get("fen"),
                data.get("ply"),
                data.get("time_ms"),
                data.get("root_eval"),
                data.get("max_depth"),
                data.get("max_qdepth"),
                data.get("best_move"),
                safe_val(data.get("principal_variation")),
                data.get("nodes"),
                data.get("qnodes"),
                data.get("tt_stores"),
                data.get("tt_hits"),
                data.get("tt_fill"),
                data.get("fail_highs"),
                data.get("fail_lows"),
                data.get("fail_high_first"),
                data.get("fail_high_late"),
                data.get("aspiration_fail_high_researches"),
                data.get("aspiration_fail_low_researches"),
                data.get("see_prunes"),
                data.get("delta_prunes"),
                data["search_uuid"],  # temp
            ))

    cursor.executemany(
        """
        INSERT INTO searches (
            engine_id, game_id, sts_id, fen, ply, time_ms, eval, depth, qdepth, move, 
            principal_variation, nodes, qnodes, tt_stores, tt_hits, tt_fill,
            fail_highs, fail_lows, fail_high_first, fail_high_late, fail_high_researches, fail_low_researches,
            see_prunes, delta_prunes
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        [row[:-1] for row in searches_rows]
    )

    # Recover search IDs safely
    cursor.execute(
        "SELECT id FROM searches ORDER BY id DESC LIMIT ?",
        (len(searches_rows),)
    )
    search_ids = [r[0] for r in cursor.fetchall()][::-1]

    for row, sid in zip(searches_rows, search_ids):
        uuid_map[row[-1]] = sid

    # Per-iteration-depth data
    #  captures d=1,2,3 of an iterative deepening search
    depth_rows = []
    with open(search_path, "r") as f:
        for line in f:
            data = json.loads(line.strip())
            search_id = uuid_map[data["search_uuid"]]

            n = len(data.get("itdepth_nodes", []))
            for d in range(n):
                depth_rows.append((
                    search_id,
                    d + 1,
                    safe(data.get("itdepth_time_ms", []), d),
                    safe(data.get("itdepth_eval", []), d),
                    safe(data.get("itdepth_move", []), d),
                    safe(data.get("itdepth_qdepth", []), d),
                    safe(data.get("itdepth_nodes", []), d),
                    safe(data.get("itdepth_qnodes", []), d),
                    safe(data.get("itdepth_ttstores", []), d),
                    safe(data.get("itdepth_tthits", []), d),
                    safe(data.get("itdepth_ttfill", []), d),
                    safe(data.get("itdepth_fail_highs", []), d),
                    safe(data.get("itdepth_fail_lows", []), d),
                    safe(data.get("itdepth_fail_high_firsts", []), d),
                    safe(data.get("itdepth_fail_high_lates", []), d),
                    safe(data.get("itdepth_aspiration_failhigh_researches", []), d),
                    safe(data.get("itdepth_aspiration_faillow_researches", []), d),
                    safe(data.get("itdepth_see_prunes", []), d),
                    safe(data.get("itdepth_delta_prunes", []), d)
                ))

    cursor.executemany(
        """
        INSERT INTO searches_by_iteration (
            search_id, depth, time_ms, eval, move, qdepth,
            nodes, qnodes, tt_stores, tt_hits, tt_fill,
            fail_highs, fail_lows, fail_high_first, fail_high_late,
            fail_high_researches, fail_low_researches, see_prunes, delta_prunes
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        depth_rows
    )

    # per-search_tree-depth data
    #   captures d=1,2,3 of the search tree
    #     d >= i all touch d=i, so multiple counting occurs
    ply_rows = []
    with open(search_path, "r") as f:
        for line in f:
            data = json.loads(line.strip())
            search_id = uuid_map[data["search_uuid"]]

            n = len(data.get("treedepth_nodes", []))
            for d in range(n):
                ply_rows.append((
                    search_id,
                    d + 1, 
                    #data.get("ply_time_ms", [0]*n)[d],
                    safe(data.get("treedepth_nodes", []), d),
                    safe(data.get("treedepth_qnodes", []), d),
                    safe(data.get("treedepth_tt_stores", []), d),
                    safe(data.get("treedepth_tt_hits", []), d),
                    #data.get("ply_tt_fill", [0]*n)[d],
                    safe(data.get("treedepth_fail_highs", []), d),
                    safe(data.get("treedepth_fail_lows", []), d),
                    safe(data.get("treedepth_fail_high_firsts", []), d),
                    safe(data.get("treedepth_fail_high_lates", []), d),
                    safe(data.get("treedepth_see_prunes", []), d),
                    safe(data.get("treedepth_delta_prunes", []), d),
                ))

    cursor.executemany(
        """
        INSERT INTO searches_by_tree_depth (
            search_id, depth, 
            nodes, qnodes, tt_stores, tt_hits, fail_highs, fail_lows,
            fail_high_first, fail_high_late, see_prunes, delta_prunes
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        ply_rows
    )

    # ----------------
    # ---- TIMING ----
    # ----------------

    timing_rows = []
    if timing_path:
        with open(timing_path, "r") as f:
            for line in f:
                data = json.loads(line.strip())
                if data.get("type") != "timing":
                    continue

                search_id = uuid_map.get(data.get("search_uuid"))
                if search_id is None:
                    continue

                for func, stats in data.items():
                    if func in {
                        "engine_id", "instance_id", "type", "session",
                        "game_uuid", "search_uuid", "fen", "total_search_time_ms"
                    }:
                        continue

                    timing_rows.append((
                        search_id,
                        func,
                        stats.get("total_ms", 0),
                        stats.get("calls", 0),
                    ))

        cursor.executemany(
            """
            INSERT INTO timing (search_id, function, total_time_ms, num_calls)
            VALUES (?, ?, ?, ?)
            """,
            timing_rows
        )

    cnxn.commit()
    print(
        f"[INFO] Inserted {len(searches_rows)} searches, "
        f"{len(depth_rows), len(ply_rows)} depth rows, "
        f"{len(timing_rows)} timing rows."
    )



if __name__ == "__main__":
    system = platform.system()
    
    p = argparse.ArgumentParser(description="ETL functions for chess.db\nRegister engines, upload logs, clear directories")
    

    # register engine to database for use in experiments
    #   release pipeline automatically registers engine
    p.add_argument("--register_engine", action="store_true", help="Flag to register engine to chess.db")
    p.add_argument("--name", default=None, type=str, help="Engine name")
    p.add_argument("--version", default=None, type=str, help="Engine version")
    p.add_argument("--description", default=None, type=str, help="Engine description (changes from last iteration, etc)")
    p.add_argument("--uci_options", default=None, type=str, help="UCI settings of the engine (e.g. threads, hash size, etc.)")

    # log game files
    #   currently no auto sch, but this will game that easier
    p.add_argument("--log_games", action="store_true", help="Flag to log game directory (games.jsonl, search.jsonl [, timing.jsonl]) to chess.db")
    #p.add_argument("--games_dir", type=str, default="logs/game_logs", help="Directory where game files are stored")

    args = p.parse_args()
    if system == "Windows": cnxn = sqlite3.connect('F:/databases/chess.db')
    elif system == "Darwin": cnxn = sqlite3.connect(Path.home() / "Documents/databases/chess.db")

    if args.register_engine:
        register_engine(cnxn, {"name": args.name, "version": args.version, "description": args.description, "uci_options": args.uci_options})
    elif args.log_games:
        log_games_directory(cnxn) 