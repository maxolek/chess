"""Database connection helpers and engine probing."""
import sqlite3
import os
import time
import subprocess
import platform
import shutil
from .paths import RAW_DB


def clear_log_dir(log_dir):
    """Remove all files and subdirectories in a log directory."""
    if not os.path.isdir(log_dir):
        return

    for entry in os.listdir(log_dir):
        path = os.path.join(log_dir, entry)
        try:
            if os.path.isfile(path) or os.path.islink(path):
                os.unlink(path)
            else:
                shutil.rmtree(path)
        except Exception as e:
            print(f"[WARN] Failed to delete {path}: {e}")


def get_db(path=None):
    """Get a SQLite connection with Row factory."""
    if path is None:
        path = str(RAW_DB)
    cnxn = sqlite3.connect(path)
    cnxn.row_factory = sqlite3.Row
    return cnxn


def get_engine_id(cnxn, version=None):
    """Look up engine ID by version string."""
    row = cnxn.execute(
        "SELECT id FROM engines WHERE version=?",
        (version,)
    ).fetchone()

    if row is None:
        return None
    else:
        return row[0]


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
    """Extract engine_id (version string) from the first line of a search log."""
    import json
    with open(search_path, "r") as f:
        for line in f:
            data = json.loads(line.strip())
            if "engine_id" in data:
                return data["engine_id"]
    return None
