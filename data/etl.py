import sqlite3
from pathlib import Path 
import json
import subprocess
import glob
import os

# --------------------------
#        CLEAR LOGS
# --------------------------

def clear_log_folder(log_path):
    """Remove all JSONL files in the same folder as log_path"""

    folder = os.path.dirname(log_path)
    pattern = os.path.join(folder, "*.jsonl")

    for f in glob.glob(pattern):
        try:
            os.remove(f)
            print(f"[STS] Deleted old log file: {f}")
        except Exception as e:
            print(f"[STS] Failed to delete {f}: {e}")

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

def probe_engine_metadata(engine_path):
    """
    Query engine via UCI and extract id name + id version.
    """
    p = subprocess.Popen(
        [engine_path],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        bufsize=1
    )

    meta = {}

    p.stdin.write("uci\n")
    p.stdin.flush()

    for line in p.stdout:
        line = line.strip()

        #if line.startswith("id name"):
        #    meta["name"] = line[len("id name"):].strip()

        if line.startswith("id version"):
            meta["version"] = line[len("id version"):].strip()

        elif line == "uciok":
            break

    p.stdin.write("quit\n")
    p.stdin.flush()
    p.wait(timeout=1)

    if "version" not in meta: # "name" not in meta or
        raise RuntimeError(f"Failed to probe engine metadata: {engine_path}")

    return meta


# -------------------------------
#   LOG SINGLE ROW TO DATABASE
# -------------------------------

# start experiment and get ID for FKs
def start_experiment(cnxn, experiment, engine_id, info=None):
    cursor = cnxn.cursor()
    cursor.execute(
        """
        INSERT INTO experiments (engine_id, type, metadata, start_time)
        VALUES (?, ?, ?, CURRENT_TIMESTAMP)
        """,
        (engine_id, experiment, info)
    )
    cnxn.commit()
    cursor.execute("SELECT last_insert_rowid()")
    return cursor.fetchone()[0]

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
            experiment_id, fen, depth, nodes, expected_nodes, correct, time
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
            perft_info.get("time"),
        )
    )

    return cur.lastrowid


def log_sprt(cnxn, sprt_info):

    cur = cnxn.execute(
        """
        INSERT INTO sprt (
            experiment_id, baseline_engine_id, candidate_engine_id, 
            opening_book, book_depth, time_control, time_per_move,
            depth_per_move, elo0, elo1, alpha, beta, result, elo_diff,
            llr, games_played, run_time
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        (
            sprt_info["experiment_id"],
            sprt_info["baseline_engine_id"],
            sprt_info["candidate_engine_id"],
            sprt_info.get("opening_book"),
            sprt_info.get("book_depth"),
            sprt_info.get("time_control"),
            sprt_info.get("time_per_move"),
            sprt_info.get("depth_per_move"),
            sprt_info.get("elo0"),
            sprt_info.get("elo1"),
            sprt_info.get("alpha"),
            sprt_info.get("beta"),
            sprt_info.get("result"),
            sprt_info.get("elo_diff"),
            sprt_info.get("llr"),
            sprt_info.get("games_played"),
            sprt_info.get("run_time"),
        )
    )

    return cur.lastrowid


def log_sts(cnxn, sts_info):

    cur = cnxn.execute(
        """
        INSERT INTO sts (
            experiment_id, suite, position_name, 
            fen, search_time, search_depth, move_is_correct,
            engine_move, engine_score, expected_move, expected_score
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        (
            sts_info["experiment_id"],
            sts_info["suite"],
            sts_info["position_name"],
            sts_info.get("fen"),
            sts_info.get("search_time"),
            sts_info.get("search_depth"),
            sts_info.get("move_is_correct"),
            sts_info.get("engine_move"),
            sts_info.get("engine_score"),
            sts_info.get("expected_move"),
            sts_info.get("expected_score")
        )
    )

    return cur.lastrowid


def log_game(cnxn, game):

    cur = cnxn.execute(
        """
        INSERT INTO games (
            experiment_id, suite, position_name, 
            fen, search_time, search_depth, move_is_correct,
            engine_move, engine_score, expected_move, expected_score
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        (
            game["experiment_id"],
            game["white_engine_id"],
            game["black_engine_id"],
            game.get("time_control"),
            game.get("time_per_move"),
            game.get("depth_per_move"),
            game.get("white_player"),
            game.get("black_player"),
            game.get("white_elo"),
            game.get("black_elo"),
            game.get("result"),
            game.get("termination"),
            game.get("opening"),
            game.get("start_fen"),
            json.dumps(game.get("moves"))
        )
    )

    return cur.lastrowid


def log_search(cnxn, search):

    cur = cnxn.execute(
        """
        INSERT INTO searches (
            game_id, sts_id, fen, time, 
            eval, move, principal_variation, depth,
            nodes, q_nodes, tt_stores, tt_hits,
            fail_highs, fail_lows
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        (
            search.get("game_id"),
            search.get("sts_id"),
            search["fen"],
            search.get("time"),
            search.get("eval"),
            search.get("move"),
            search.get("principal_variation"),
            search.get("depth"),
            search.get("nodes"),
            search.get("q_nodes"),
            search.get("tt_stores"),
            search.get("tt_hits"),
            search.get("fail_highs"),
            search.get("fail_lows")
        )
    )

    return cur.lastrowid


def log_depth(cnxn, depth):

    cur = cnxn.execute(
        """
        INSERT INTO searches_by_depth (
            search_id, depth, time, 
            eval, move,
            nodes, q_nodes, tt_stores, tt_hits,
            fail_highs, fail_lows
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        (
            depth["search_id"],
            depth["depth"],
            depth.get("time"),
            depth.get("eval"),
            depth.get("move"),
            depth.get("nodes"),
            depth.get("q_nodes"),
            depth.get("tt_stores"),
            depth.get("tt_hits"),
            depth.get("fail_highs"),
            depth.get("fail_lows")
        )
    )

    return cur.lastrowid


def log_timing(cnxn, timing):

    cur = cnxn.execute(
        """
        INSERT INTO timing (
            search_id, function, time
        )
        VALUES (?, ?, ?)
        """,
        (
            timing["search_id"],
            timing["function"],
            timing.get("time")
        )
    )

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
def bulk_log_sts(cnxn, path):
    rows = []

    with open(path, "r") as f:
        for line in f:
            data = json.loads(line)
            rows.append((
                data["experiment_id"],
                data.get("suite"),
                data.get("position_name"),
                data.get("fen"),
                data.get("search_time"),
                data.get("search_depth"),
                data.get("move_is_correct"),
                data.get("engine_move"),
                data.get("engine_score"),
                data.get("expected_move"),
                data.get("expected_score")
            ))
            
    cnxn.executemany(
        """
        INSERT INTO sts (
            experiment_id, suite, position_name,
            fen, search_time, search_depth, move_is_correct,
            engine_move, engine_score, expected_move, expected_score
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        rows
    )


def bulk_log_game(cnxn, path, experiment_id=None):
    cursor = cnxn.cursor()

    rows = []
    game_uuids = []

    with open(path, "r") as f:
        for line in f:
            data = json.loads(line)
            game_uuids.append(data["game_uuid"])
            rows.append((
                experiment_id,
                data.get("white_engine_id"),
                data.get("black_engine_id"),
                data.get("time_control"),
                data.get("time_per_move"),
                data.get("depth_per_move"),
                data.get("white_player"),
                data.get("black_player"),
                data.get("white_elo"),
                data.get("black_elo"),
                data.get("result"),
                data.get("termination"),
                data.get("opening"),
                data.get("start_fen"),
                json.dumps(data.get("moves")),
            ))

    cursor.executemany(
        """
        INSERT INTO games (
            experiment_id,
            white_engine_id, black_engine_id,
            time_control, time_per_move, depth_per_move,
            white_player, black_player,
            white_elo, black_elo,
            result, termination,
            opening, start_fen, moves
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
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
    timing_path=None,
    sts_id=None
):
    cursor = cnxn.cursor()

    searches_rows = []
    uuid_map = {}  # search_uuid -> search_id

    with open(search_path, "r") as f:
        for line in f:
            data = json.loads(line)

            game_uuid = data["game_uuid"]
            game_id = game_map.get(game_uuid)

            if game_id is None:
                raise RuntimeError(f"Unknown game_uuid {game_uuid}")

            searches_rows.append((
                game_id,
                sts_id,
                data.get("fen"),
                data.get("ply"),
                data.get("time_ms"),
                data.get("root_eval"),
                data.get("best_move"),
                data.get("principal_variation"),
                data.get("max_depth"),
                data.get("nodes"),
                data.get("qnodes"),
                data.get("tt_stores"),
                data.get("tt_hits"),
                data.get("fail_highs"),
                data.get("fail_lows"),
                data["search_uuid"],  # temp
            ))

    cursor.executemany(
        """
        INSERT INTO searches (
            game_id, sts_id, fen, ply, time, eval, move,
            principal_variation, depth, nodes, q_nodes, tt_stores, tt_hits,
            fail_highs, fail_lows
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
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

    # Per-depth data
    depth_rows = []
    with open(search_path, "r") as f:
        for line in f:
            data = json.loads(line)
            search_id = uuid_map[data["search_uuid"]]

            n = len(data.get("evalPerDepth", []))
            for d in range(n):
                depth_rows.append((
                    search_id,
                    d + 1,
                    data.get("timeOnDepthMS", [0]*n)[d],
                    data.get("evalPerDepth", [0]*n)[d],
                    data.get("bestMovePerDepth", [None]*n)[d],
                    data.get("depthNodes", [0]*n)[d],
                    data.get("depthQNodes", [0]*n)[d],
                    data.get("depthTTStores", [0]*n)[d],
                    data.get("depthTTHits", [0]*n)[d],
                    data.get("depthFailHighs", [0]*n)[d],
                    data.get("depthFailLows", [0]*n)[d],
                ))

    cursor.executemany(
        """
        INSERT INTO searches_by_depth (
            search_id, depth, time, eval, move,
            nodes, q_nodes, tt_stores, tt_hits,
            fail_highs, fail_lows
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        depth_rows
    )

    # Timing
    timing_rows = []
    if timing_path:
        with open(timing_path, "r") as f:
            for line in f:
                data = json.loads(line)
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
            INSERT INTO timing (search_id, function, total_time, num_calls)
            VALUES (?, ?, ?, ?)
            """,
            timing_rows
        )

    cnxn.commit()
    print(
        f"[INFO] Inserted {len(searches_rows)} searches, "
        f"{len(depth_rows)} depth rows, "
        f"{len(timing_rows)} timing rows."
    )
