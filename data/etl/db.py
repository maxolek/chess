"""Database connection helpers and engine probing."""
import sqlite3
import os
import time
import subprocess
import platform
import shutil
import re
import queue 
import threading
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
    system = platform.system()
    engine_path = os.path.abspath(engine_path)

    if system == "Windows" and not engine_path.lower().endswith(".exe"):
        if os.path.exists(engine_path + ".exe"):
            engine_path += ".exe"

    if not os.path.exists(engine_path):
        raise FileNotFoundError(f"Engine not found at path: {engine_path}")

    def _run_probe(command):
        p = subprocess.Popen(
            [engine_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            bufsize=1
        )

        q = queue.Queue()

        def _reader():
            try:
                for line in p.stdout:
                    q.put(line)
            except Exception:
                pass
            finally:
                q.put(None)  # sentinel: stdout closed

        t = threading.Thread(target=_reader, daemon=True)
        t.start()

        meta = {}
        options = {}
        start = time.time()
        try:
            p.stdin.write(f"{command}\n")
            p.stdin.flush()

            while True:
                remaining = timeout - (time.time() - start)
                if remaining <= 0:
                    raise RuntimeError(f"Timeout waiting for UCI response from {engine_path}")

                try:
                    line = q.get(timeout=min(remaining, 0.5))
                except queue.Empty:
                    continue

                if line is None:
                    raise RuntimeError(f"Engine closed stdout before uciok: {engine_path}")

                line = line.strip()
                if line.startswith("id name"):
                    meta["name"] = line[len("id name"):].strip()
                elif line.startswith("id version"):
                    meta["version"] = line[len("id version"):].strip()
                elif line.startswith("id author"):
                    meta["author"] = line[len("id author"):].strip()
                elif line.startswith("option name "):
                    m = re.match(r'^option name (.+?) type (\w+)(?: default (".*?"|[^ ]+))?(?: min ([^ ]+))?(?: max ([^ ]+))?', line)
                    if m:
                        opt_name = m.group(1).strip()
                        opt_type = m.group(2).strip()
                        default_val = m.group(3)
                        min_val = m.group(4)
                        max_val = m.group(5)

                        if isinstance(default_val, str):
                            default_val = default_val.strip()
                            if default_val.startswith('"') and default_val.endswith('"'):
                                default_val = default_val[1:-1]

                        entry = {"type": opt_type, "default": default_val}
                        if min_val is not None:
                            entry["min"] = min_val
                        if max_val is not None:
                            entry["max"] = max_val

                        options[opt_name] = entry
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

        meta["options"] = options
        return meta

    try:
        return _run_probe("uci_dev")
    except RuntimeError:
        return _run_probe("uci")


def save_engine_config(engine_path, config_name, timeout=10.0):
    """Run the engine via UCI and issue save_config <name>."""
    system = platform.system()
    engine_path = os.path.abspath(engine_path)

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
            if line.strip() == "uciok":
                break

        p.stdin.write(f"save_config {config_name}\n")
        p.stdin.flush()
        p.stdin.write("quit\n")
        p.stdin.flush()
        p.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        p.kill()
        raise RuntimeError(f"Timeout while saving engine config for {engine_path}")
    finally:
        if p.poll() is None:
            p.kill()

    return True


def extract_engine_id_from_search(search_path):
    """Extract engine_id (version string) from the first search object in a search log."""
    import json

    with open(search_path, "r", encoding="utf-8") as f:
        for line_no, line in enumerate(f, start=1):
            text = line.strip()
            if not text:
                continue
            try:
                data = json.loads(text)
            except json.JSONDecodeError:
                print(f"[WARN] skipping malformed JSON line {line_no} in {search_path}")
                continue

            if not isinstance(data, dict):
                print(f"[WARN] skipping non-object JSON line {line_no} in {search_path}: {type(data).__name__}")
                continue

            if "engine_id" in data:
                return data["engine_id"]

    return None
