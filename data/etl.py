import sqlite3
from pathlib import Path 
import json
import subprocess
import glob
import os
import shutil
import argparse

# utils

def safe_val(val):
    if isinstance(val, list) or isinstance(val, dict):
        return json.dumps(val)
    return val

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
            data = json.loads(line)

            move = data.get('engine_move')
            # most STS has only 1 best move, some have 2
            expected_moves = data.get('expected_moves') or []
            expected_move_1 = expected_moves[0] if len(expected_moves) > 0 else None
            expected_move_2 = expected_moves[1] if len(expected_moves) > 1 else None
            avoid_moves = data.get('avoid_moves')

            if expected_moves != []:
                correct = (move == expected_move_1 or move == expected_move_2)
            elif avoid_moves is not None:
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
            data = json.loads(line)

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
                data.get("time_control"),
                data.get("time_per_move"),
                data.get("depth_per_move"),
                data.get("result"),
                data.get("termination"),
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
            time_control, time_per_move, depth_per_move,
            result, termination,
            opening, start_fen, moves, run_time_s
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
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

    with open(search_path, "r") as f:
        for line in f:
            data = json.loads(line)

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
                data.get("aspirationFailHighResearches"),
                data.get("aspirationFailLowResearches"),
                data.get("see_prunes"),
                data.get("delta_prunes"),
                data["search_uuid"],  # temp
            ))

    cursor.executemany(
        """
        INSERT INTO searches (
            engine_id, game_id, sts_id, fen, ply, time_ms, eval, depth, move, 
            principal_variation, nodes, q_nodes, tt_stores, tt_hits, tt_fill,
            fail_highs, fail_lows, fail_high_first, fail_high_late, fail_high_researches, fail_low_researches,
            see_prunes, delta_prunes
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
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
                    data.get("depthTTFill", [0]*n)[d],
                    data.get("depthFailHighs", [0]*n)[d],
                    data.get("depthFailLows", [0]*n)[d],
                    data.get("depthFailHighFirst", [0]*n)[d],
                    data.get("depthFailHighLate", [0]*n)[d],
                    data.get("depthAspirationFailHighResearches", [0]*n)[d],
                    data.get("depthAspirationFailLowResearches", [0]*n)[d],
                    data.get("depthSEEPrunes", [0]*n)[d], 
                    data.get("depthDeltaPrunes", [0]*n)[d]
                ))

    cursor.executemany(
        """
        INSERT INTO searches_by_depth (
            search_id, depth, time_ms, eval, move,
            nodes, q_nodes, tt_stores, tt_hits, tt_fill,
            fail_highs, fail_lows, fail_high_first, fail_high_late,
            fail_high_researches, fail_low_researches, see_prunes, delta_prunes
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
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
            INSERT INTO timing (search_id, function, total_time_ms, num_calls)
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



if __name__ == "__main__":
    p = argparse.ArgumentParser(description="ETL functions for chess.db\nRegister engines, upload logs, clear directories")
    
    p.add_argument("--register_engine", action="store_true", help="Flag to register engine to chess.db")
    p.add_argument("--name", default=None, type=str, help="Engine name")
    p.add_argument("--version", default=None, type=str, help="Engine version")
    p.add_argument("--description", default=None, type=str, help="Engine description (changes from last iteration, etc)")
    p.add_argument("--uci_options", default=None, type=str, help="UCI settings of the engine (e.g. threads, hash size, etc.)")

    args = p.parse_args()
    cnxn = sqlite3.connect('F:/databases/chess.db')

    if args.register_engine:
        register_engine(cnxn, {"name": args.name, "version": args.version, "description": args.description, "uci_options": args.uci_options})
