"""Bulk ingestion functions: games, searches, timing, STS, SPRT."""
import json
import shutil
from pathlib import Path

from .paths import GAME_JSON, SEARCH_JSON, TIMING_JSON, ROOT_MOVES_JSON, GAMES_LOG_DIR, LOG_DIRS, get_jsonl_paths
from .utils import safe_val, safe, consolidate_instance_logs
from .db import get_engine_id, clear_log_dir, extract_engine_id_from_search, probe_engine_metadata, save_engine_config

def _iter_json_objects_from_path(path):
    with open(path, "r", encoding="utf-8") as f:
        for line_no, line in enumerate(f, start=1):
            text = line.strip()
            if not text:
                continue
            try:
                data = json.loads(text)
            except json.JSONDecodeError:
                print(f"[WARN] skipping malformed JSON line {line_no} in {path}")
                continue

            if not isinstance(data, dict):
                print(f"[WARN] skipping non-object JSON line {line_no} in {path}: {type(data).__name__}")
                continue

            yield data


# ─────────────────────────────────────────────────────────────────────────────
# Top-level directory ingestion
# ─────────────────────────────────────────────────────────────────────────────

def log_games_directory(cnxn):
    """Ingest all game logs from the standard log directory."""
    ingest_log_dir(cnxn, GAMES_LOG_DIR)


def ingest_log_dir(cnxn, log_dir, clear=True):
    """Ingest all JSONL files from a given log directory."""
    paths = get_jsonl_paths(log_dir)
    game_map = {}

    consolidate_instance_logs(log_dir)

    if paths["game"].is_file():
        game_map = bulk_log_game(cnxn, paths["game"])
    else:
        print(f"[INFO] No game log found: {paths['game']}")

    if paths["search"].is_file():
        bulk_log_search_and_timing(
            cnxn,
            paths["search"],
            game_map,
            timing_path=paths["timing"] if paths["timing"].is_file() else None,
            root_moves_path=paths["root_moves"] if paths["root_moves"].is_file() else None
        )
    else:
        print(f"[INFO] No search log found: {paths['search']}")

    # clear only if searches+games
    if clear and paths["search"].is_file() and paths["game"].is_file():
        clear_log_dir(log_dir)


def ingest_all_log_dirs(cnxn):
    """Sweep all known log directories and ingest any JSONL data found."""
    for log_dir in LOG_DIRS:
        if not log_dir.exists():
            continue
        paths = get_jsonl_paths(log_dir)
        has_data = any(p.is_file() for p in paths.values())
        if has_data:
            print(f"[INGEST] Processing: {log_dir}")
            ingest_log_dir(cnxn, log_dir)


# ─────────────────────────────────────────────────────────────────────────────
# Experiment management
# ─────────────────────────────────────────────────────────────────────────────

def start_experiment(cnxn, experiment, engine_id, comparison_engine_id=None, info=None):
    """Start an experiment and return its ID."""
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


def update_experiment(cnxn, experiment_id, info=None):
    """Update experiment metadata columns."""
    if not info:
        return

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


# ─────────────────────────────────────────────────────────────────────────────
# Single-row logging
# ─────────────────────────────────────────────────────────────────────────────

def _int_option(options, key):
    def _get_option(options, key):
        # try variants to handle different naming conventions
        candidates = [
            key,
            key.lower(),
            key.upper(),
            key.replace("_", " "),
            key.replace("_", " ").lower(),
            key.replace("_", " ").title(),
        ]
        for c in candidates:
            if c in options:
                return options[c]
        return None

    value = _get_option(options, key)
    if value is None:
        return None
    raw = value.get("default")
    if raw is None:
        return None
    try:
        return int(raw)
    except (TypeError, ValueError):
        try:
            return int(float(raw))
        except Exception:
            return None


def _float_option(options, key):
    def _get_option(options, key):
        candidates = [
            key,
            key.lower(),
            key.upper(),
            key.replace("_", " "),
            key.replace("_", " ").lower(),
            key.replace("_", " ").title(),
        ]
        for c in candidates:
            if c in options:
                return options[c]
        return None

    value = _get_option(options, key)
    if value is None:
        return None
    raw = value.get("default")
    if raw is None:
        return None
    try:
        return float(raw)
    except (TypeError, ValueError):
        return None


def _float_option_scaled(options, key, scale=100):
    v = _float_option(options, key)
    if v is None:
        return None
    try:
        return float(v) / scale
    except Exception:
        return None


def _bool_option(options, key):
    def _get_option(options, key):
        candidates = [
            key,
            key.lower(),
            key.upper(),
            key.replace("_", " "),
            key.replace("_", " ").lower(),
            key.replace("_", " ").title(),
        ]
        for c in candidates:
            if c in options:
                return options[c]
        return None

    value = _get_option(options, key)
    if value is None:
        return None
    raw = value.get("default")
    if raw is None:
        return None
    if isinstance(raw, str):
        return raw.lower() in {"1", "true", "yes", "on"}
    return bool(raw)


def register_engine(cnxn, engine):
    """Register an engine and return its row ID.
    
    If an engine with the same version is already registered, returns its ID instead.
    Prompts the user for a description if one is not provided
    """
    # First, extract version to check for duplicates
    if engine.get("engine_path"):
        engine_path = engine["engine_path"]
        print(f"[ETL] Probing engine UCI at: {engine_path}")
        engine_meta = probe_engine_metadata(engine_path)
        options = engine_meta.get("options", {})
        print(f"[ETL] Probed option keys: {list(options.keys())}")
        version = engine.get("version") or engine_meta.get("version")
        name = engine.get("name") or Path(engine_path).name.removesuffix('.exe')
        #description = engine.get("description") or f"Auto-registered from {engine_path}"

        # prompt user for description instead of auto-filling
        if engine.get("description"): 
            description = engine['description']
        else:
            print(f"\n[ETL] Registering new engine: {name} ({version})")
            name        = input("  Enter a name for this engine version (or press Enter to skip): ").strip()
            description = input("  Enter a description for this engine version (or press Enter to skip): ").strip()
            if not description: 
                description = f"Auto-registered from {engine_path}"

        move_overhead_ms = _int_option(options, "Move Overhead")
        max_threads = _int_option(options, "Threads")
        hash_size_mb = _int_option(options, "Hash")
        pondering = _bool_option(options, "Ponder")

        delta_prune_threshold = _int_option(options, "DELTA_PRUNE_THRESHOLD")
        see_prune_threshold = _int_option(options, "SEE_PRUNE_THRESHOLD")
        aspiration_window = _int_option(options, "ASPIRATION_WINDOW")
        aspiration_start_depth = _int_option(options, "ASPIRATION_START_DEPTH")
        aspiration_depth_scale = _int_option(options, "ASPIRATION_DEPTH_SCALE")
        aspiration_research_scale = _float_option(options, "ASPIRATION_RESEARCH_SCALE")
        draw_eval = _int_option(options, "DRAW_EVAL")
        contempt = _int_option(options, "CONTEMPT")
        r_nmp = _int_option(options, "R_NMP")
        r_lmr_const = _float_option_scaled(options, "R_LMR_CONST", 100)
        r_lmr_denom = _float_option_scaled(options, "R_LMR_DENOM", 100)
        lmr_move_order_threshold = _int_option(options, "LMR_MOVE_ORDER_THRESHOLD")
        lmr_depth_threshold = _int_option(options, "LMR_DEPTH_THRESHOLD")

        # diagnostics: report any missing mapped values
        mapped = {
            'delta_prune_threshold': delta_prune_threshold,
            'see_prune_threshold': see_prune_threshold,
            'aspiration_window': aspiration_window,
            'aspiration_start_depth': aspiration_start_depth,
            'aspiration_depth_scale': aspiration_depth_scale,
            'aspiration_research_scale': aspiration_research_scale,
            'draw_eval': draw_eval,
            'contempt': contempt,
            'r_nmp': r_nmp,
            'r_lmr_const': r_lmr_const,
            'r_lmr_denom': r_lmr_denom,
            'lmr_move_order_threshold': lmr_move_order_threshold,
            'lmr_depth_threshold': lmr_depth_threshold,
        }
        missing = [k for k, v in mapped.items() if v is None]
        if missing:
            print(f"[ETL][WARN] The following mapped engine params were not found or parsed: {missing}")

        if version:
            print(f"[ETL] Saving engine config as: {version}")
            save_engine_config(engine_path, version)
        else:
            print(f"[WARN] Engine version not found; skipping save_config for {engine_path}")
    else:
        version = engine.get("version")
        name = engine.get("name")
        description = engine.get("description")
        move_overhead_ms = engine.get("move_overhead_ms")
        max_threads = engine.get("max_threads")
        hash_size_mb = engine.get("hash_size_mb")
        pondering = engine.get("pondering")
        delta_prune_threshold = engine.get("delta_prune_threshold")
        see_prune_threshold = engine.get("see_prune_threshold")
        aspiration_window = engine.get("aspiration_window")
        aspiration_start_depth = engine.get("aspiration_start_depth")
        aspiration_depth_scale = engine.get("aspiration_depth_scale")
        aspiration_research_scale = engine.get("aspiration_research_scale")
        draw_eval = engine.get("draw_eval")
        contempt = engine.get("contempt")
        r_nmp = engine.get("r_nmp")
        r_lmr_const = engine.get("r_lmr_const")
        r_lmr_denom = engine.get("r_lmr_denom")
        lmr_move_order_threshold = engine.get("lmr_move_order_threshold")
        lmr_depth_threshold = engine.get("lmr_depth_threshold")

    # Check if engine with this version already exists
    if version:
        existing_id = get_engine_id(cnxn, version=version)
        if existing_id is not None:
            print(f"[ETL] Engine {name} ({version}) already registered with ID {existing_id}")
            return existing_id

    cur = cnxn.execute(
        """
        INSERT INTO engines (
            name, version, description, compile_flags,
            move_overhead_ms, max_threads, hash_size_mb, pondering,
            delta_prune_threshold, see_prune_threshold,
            aspiration_window, aspiration_start_depth, aspiration_depth_scale,
            aspiration_research_scale, draw_eval, contempt,
            r_nmp, r_lmr_const, r_lmr_denom,
            lmr_move_order_threshold, lmr_depth_threshold
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        (
            name,
            version,
            description,
            engine.get("compile_flags"),
            # uci engine options
            move_overhead_ms,
            max_threads,
            hash_size_mb,
            pondering,
            # search params
            delta_prune_threshold,
            see_prune_threshold,
            aspiration_window,
            aspiration_start_depth,
            aspiration_depth_scale,
            aspiration_research_scale,
            draw_eval,
            contempt,
            r_nmp,
            r_lmr_const,
            r_lmr_denom,
            lmr_move_order_threshold,
            lmr_depth_threshold
        )
    )
    cnxn.commit()
    print(f"[ETL] Registered engine {name} ({version})")
    return cur.lastrowid


def log_perft(cnxn, perft_info):
    """Log a perft result and return its row ID."""
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

def log_engine_ratings(cnxn, engine_id, ratings):
    """Log engine elo ratings by time control category
    
    ratings: dict with keys like 'bullet', 'blitz', 'rapid', 'classical'
                each value is a dict with 'elo' and 'games'
    """
    # prepare values for each TC category (use None for missing elos, 0 for games)
    tc_keys = ["ultra_fast", "bullet", "blitz", "rapid", "classical"]
    elos = [None] * len(tc_keys)
    games = [0] * len(tc_keys)
    for i, k in enumerate(tc_keys):
        v = ratings.get(k)
        if isinstance(v, dict):
            elos[i] = v.get("elo")
            games[i] = int(v.get("games", 0) or 0)

    params = (
        elos[0], elos[1], elos[2], elos[3], elos[4],
        games[0], games[1], games[2], games[3], games[4],
        engine_id,
    )

    # If a row for this engine already exists, update it; otherwise insert.
    existing = cnxn.execute(
        "SELECT id FROM engine_ratings WHERE engine_id = ? LIMIT 1",
        (engine_id,),
    ).fetchone()

    if existing:
        cur = cnxn.execute(
            """
            UPDATE engine_ratings SET
                elo_ultra_fast = ?, elo_bullet = ?, elo_blitz = ?, elo_rapid = ?, elo_classical = ?,
                games_ultra_fast = ?, games_bullet = ?, games_blitz = ?, games_rapid = ?, games_classical = ?
            WHERE engine_id = ?
            """,
            params,
        )
    else:
        cur = cnxn.execute(
            """
            INSERT INTO engine_ratings (
                engine_id,
                elo_ultra_fast, elo_bullet, elo_blitz, elo_rapid, elo_classical,
                games_ultra_fast, games_bullet, games_blitz, games_rapid, games_classical
            )
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (engine_id, elos[0], elos[1], elos[2], elos[3], elos[4],
             games[0], games[1], games[2], games[3], games[4]),
        )

    cnxn.commit()
    return cur.lastrowid

def log_sprt(cnxn, experiment_id, candidate_engine_id, baseline_engine_id, **kwargs):
    """Log an SPRT experiment result and return its row ID."""
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


# ─────────────────────────────────────────────────────────────────────────────
# Bulk logging
# ─────────────────────────────────────────────────────────────────────────────

def bulk_log_sts(cnxn, path, sts_id, **kwargs):
    """Bulk-load STS results from a JSONL file."""
    rows = []

    for data in _iter_json_objects_from_path(path):
        move = data.get('engine_move')
        expected_moves = data.get('expected_moves') or []
        expected_move_1 = expected_moves[0] if len(expected_moves) > 0 else None
        expected_move_2 = expected_moves[1] if len(expected_moves) > 1 else None
        avoid_moves = data.get('avoid_moves') or []

        if expected_moves != []:
            correct = (move == expected_move_1 or move == expected_move_2)
        elif avoid_moves != []:
            correct = (move != avoid_moves[0])
        else:
            correct = False

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
    """
    Bulk-load games from a JSONL file. Returns {game_uuid: game_id} mapping.
    
    Handles multi-engine JSONL files: if multiple records share the same game_uuid
    (from different engines), both engine IDs are resolved from the records themselves.
    Falls back to second_engine_id for the opponent when only one engine logged.
    """
    cursor = cnxn.cursor()

    # first pass: collect all records grouped by game_uuid
    game_records = {} # game_uuid -> list of record dicts
    game_order = [] # preserve insertion order of first-seen game_uuids

    for data in _iter_json_objects_from_path(path):
        uuid = data['game_uuid']
        if uuid not in game_records:
            game_records[uuid] = []
            game_order.append(uuid)
        game_records[uuid].append(data)

    rows = []
    #game_uuids = []

    for uuid in game_order:
        records = game_records[uuid]
        white_engine_id = None
        black_engine_id = None

        # use the first record as the primary source for the game metadata
        primary = records[0]

        # resolve engine  IDs from all records 
        for rec in records:
            rec_engine_id = get_engine_id(cnxn, rec.get('engine_id'))
            if rec.get('side') == 'white':
                white_engine_id = rec_engine_id
            else:
                black_engine_id = rec_engine_id

        # fall back to second_engine_id for the missing side
        if white_engine_id is None:
            white_engine_id = second_engine_id
        if black_engine_id is None:
            black_engine_id = second_engine_id

        rows.append((
            experiment_id,
            white_engine_id,
            black_engine_id,
            primary.get("wtime"),
            primary.get("btime"),
            primary.get("winc"),
            primary.get("binc"),
            primary.get("movestogo"),
            primary.get("depth"),
            primary.get("nodes"),
            primary.get("movetime"),
            primary.get("result"),
            primary.get("reason"),
            primary.get("opening"),
            primary.get("start_fen"),
            json.dumps(primary.get("moves")),
            primary.get("time_s")
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
    return dict(zip(game_order, game_ids))


def bulk_log_search_and_timing(
    cnxn,
    search_path,
    game_map,
    engine_id=None,
    timing_path=None,
    root_moves_path=None,
    sts_id=None
):
    """Bulk-load searches, per-depth iterations, tree stats, and timing from JSONL.

    Supports search logs that mix multiple engines: each record's engine is
    resolved individually via its own `engine_version` field (cached so we
    don't re-query the DB per row). Pass `engine_id` explicitly to force a
    single engine for every record (old single-engine behavior).
    """
    cursor = cnxn.cursor()

    searches_rows = []
    uuid_map = {}  # search_uuid -> search_id
    game_id = None
    skipped_uuids = set() # search_uuids dropped due to unresolved engine

    # version_string -> db_engine_id, avoids re-querying the DB for every row
    engine_id_cache = {}

    def resolve_engine_id(data):
        """Return the DB engine_id (int) for a single record (or None if unresolved)."""
        if engine_id is not None:
            # Caller forced a single engine for the whole file.
            return engine_id

        version = data.get("engine_id")
        if version is None:
            # Fall back to file-level detection, for logs that don't
            # stamp engine_id on every record.
            version = extract_engine_id_from_search(search_path)
            if version is None:
                return None

        if version in engine_id_cache:
            return engine_id_cache[version]

        resolved = get_engine_id(cnxn, version)
        engine_id_cache[version] = resolved 
        return resolved

    # ── SEARCHES ──
    for data in _iter_json_objects_from_path(search_path):

        row_engine_id = resolve_engine_id(data) 
        if row_engine_id is None: 
            skipped_uuids.add(data.get('search_uuid'))
            continue # unresolved / dev / test

        if not sts_id or game_map == {}:
            game_uuid = data["game_uuid"]
            game_id = game_map.get(game_uuid)

        searches_rows.append((
            row_engine_id,
            game_id,
            sts_id,
            data.get("fen"),
            data.get("ply"),
            data.get("time_ms"),
            data.get("root_eval"),
            data.get("completed_depth"),
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
            data.get("aspiration_fail_high_researches"),
            data.get("aspiration_fail_low_researches"),
            # fail-high move index histogram buckets
            safe(data.get("fail_high_index", []), 0),
            safe(data.get("fail_high_index", []), 1),
            safe(data.get("fail_high_index", []), 2),
            safe(data.get("fail_high_index", []), 3),
            safe(data.get("fail_high_index", []), 4),
            safe(data.get("fail_high_index", []), 5),
            data.get("see_prunes"),
            data.get("delta_prunes"),
            data.get("nmp"),
            data.get("nmp_fail"),
            data.get("tt_overwritten"),
            data["search_uuid"],   # temp key: uuid mapping
            #engine_version,        # temp key: dev/test filtering
        ))

    # engines that never get logged — filtered by version string, not id
    #searches_rows = [
    #    row for row in searches_rows
    #    if row[-1] not in ('dev', 'test')
    #]

    cursor.executemany(
        """
        INSERT INTO searches (
            engine_id, game_id, sts_id, fen, ply, time_ms, eval, completed_depth, depth, qdepth, move,
            principal_variation, nodes, qnodes, tt_stores, tt_hits, tt_fill,
            fail_highs, fail_lows, fail_high_researches, fail_low_researches,
            fh_index_0, fh_index_1, fh_index_2, fh_index_3, fh_index_4to7, fh_index_8plus,
            see_prunes, delta_prunes, nmp, nmp_fail, tt_overwritten
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        [row[:-1] for row in searches_rows]  # drop search_uuid temp keys
    )

    # Recover search IDs
    cursor.execute(
        "SELECT id FROM searches ORDER BY id DESC LIMIT ?",
        (len(searches_rows),)
    )
    search_ids = [r[0] for r in cursor.fetchall()][::-1]

    for row, sid in zip(searches_rows, search_ids):
        uuid_map[row[-1]] = sid  # search_uuid is now last element

    # ── PER-ITERATION DEPTH ──
    depth_rows = []
    for data in _iter_json_objects_from_path(search_path):
        search_id = uuid_map.get(data['search_uuid'])
        if search_id is None: continue

        n = len(data.get("itdepth_nodes", []))
        fh_index_data = data.get("itdepth_fh_index", [])
        for d in range(n):
            # Extract per-depth fh_index bucket (array of 6 values)
            fh_buckets = safe(fh_index_data, d) if d < len(fh_index_data) else None
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
                safe(data.get("itdepth_aspiration_failhigh_researches", []), d),
                safe(data.get("itdepth_aspiration_faillow_researches", []), d),
                # fail-high index histogram per iteration depth
                fh_buckets[0] if isinstance(fh_buckets, list) and len(fh_buckets) > 0 else None,
                fh_buckets[1] if isinstance(fh_buckets, list) and len(fh_buckets) > 1 else None,
                fh_buckets[2] if isinstance(fh_buckets, list) and len(fh_buckets) > 2 else None,
                fh_buckets[3] if isinstance(fh_buckets, list) and len(fh_buckets) > 3 else None,
                fh_buckets[4] if isinstance(fh_buckets, list) and len(fh_buckets) > 4 else None,
                fh_buckets[5] if isinstance(fh_buckets, list) and len(fh_buckets) > 5 else None,
                safe(data.get("itdepth_see_prunes", []), d),
                safe(data.get("itdepth_delta_prunes", []), d),
                safe(data.get("itdepth_pvs_researches", []), d),
                safe(data.get("itdepth_nmp", []), d),
                safe(data.get("itdepth_nmp_fail", []), d),
            ))

    cursor.executemany(
        """
        INSERT INTO searches_by_iteration (
            search_id, depth, time_ms, eval, move, qdepth,
            nodes, qnodes, tt_stores, tt_hits, tt_fill,
            fail_highs, fail_lows,
            fail_high_researches, fail_low_researches,
            fh_index_0, fh_index_1, fh_index_2, fh_index_3, fh_index_4to7, fh_index_8plus,
            see_prunes, delta_prunes,
            pvs_researches, nmp, nmp_fail
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        depth_rows
    )

    # ── PER-TREE DEPTH ──
    ply_rows = []
    for data in _iter_json_objects_from_path(search_path):
        search_id = uuid_map.get(data['search_uuid'])
        if search_id is None: continue

        n = len(data.get("treedepth_nodes", []))
        fh_index_tree = data.get("treedepth_fh_index", [])
        for d in range(n):
            fh_buckets = safe(fh_index_tree, d) if d < len(fh_index_tree) else None
            ply_rows.append((
                search_id,
                d + 1,
                safe(data.get("treedepth_nodes", []), d),
                safe(data.get("treedepth_qnodes", []), d),
                safe(data.get("treedepth_tt_stores", []), d),
                safe(data.get("treedepth_tt_hits", []), d),
                safe(data.get("treedepth_fail_highs", []), d),
                safe(data.get("treedepth_fail_lows", []), d),
                # fail-high index histogram per tree depth
                fh_buckets[0] if isinstance(fh_buckets, list) and len(fh_buckets) > 0 else None,
                fh_buckets[1] if isinstance(fh_buckets, list) and len(fh_buckets) > 1 else None,
                fh_buckets[2] if isinstance(fh_buckets, list) and len(fh_buckets) > 2 else None,
                fh_buckets[3] if isinstance(fh_buckets, list) and len(fh_buckets) > 3 else None,
                fh_buckets[4] if isinstance(fh_buckets, list) and len(fh_buckets) > 4 else None,
                fh_buckets[5] if isinstance(fh_buckets, list) and len(fh_buckets) > 5 else None,
                safe(data.get("treedepth_see_prunes", []), d),
                safe(data.get("treedepth_delta_prunes", []), d),
                safe(data.get("treedepth_pvs_researches", []), d),
                safe(data.get("treedepth_nmp", []), d),
                safe(data.get("treedepth_nmp_fail", []), d),
            ))

    cursor.executemany(
        """
        INSERT INTO searches_by_tree_depth (
            search_id, depth,
            nodes, qnodes, tt_stores, tt_hits, fail_highs, fail_lows,
            fh_index_0, fh_index_1, fh_index_2, fh_index_3, fh_index_4to7, fh_index_8plus,
            see_prunes, delta_prunes,
            pvs_researches, nmp, nmp_fail
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        ply_rows
    )

    # ── TIMING ──
    timing_rows = []
    if timing_path:
        for data in _iter_json_objects_from_path(timing_path):
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

    # -- ROOT MOVES --
    root_moves_rows = []
    if root_moves_path and Path(root_moves_path).is_file():
        for data in _iter_json_objects_from_path(root_moves_path):
            search_id = uuid_map.get(data.get("search_uuid"))
            if search_id is None:
                continue
            depth = data.get("depth", 0)
            for idx, mv in enumerate(data.get("moves", [])):
                root_moves_rows.append((
                    search_id,
                    depth,
                    idx,
                    mv.get("move", ""),
                    mv.get("eval"),
                    mv.get("time_ms"),
                    mv.get("nodes"),
                ))

        cursor.executemany(
            """
            INSERT INTO root_moves (search_id, depth, move_index, move, eval, time_ms, nodes)
            VALUES (?, ?, ?, ?, ?, ?, ?)
            """,
            root_moves_rows
        )

    cnxn.commit()
    print(
        f"[INFO] Inserted {len(searches_rows)} searches, "
        f"{len(depth_rows), len(ply_rows)} depth rows, "
        f"{len(timing_rows)} timing rows, "
        f"{len(root_moves_rows)} root move rows. "
        f"Skipped {len(skipped_uuids)} records with unresolved/dev/test engines. "
    )
