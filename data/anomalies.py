import duckdb
import json
from pathlib import Path

CONFIG_PATH = Path(__file__).parent / 'alerts_config.json'


def load_config():
    try:
        return json.loads(CONFIG_PATH.read_text())
    except Exception:
        return {"fail_zero_pct_threshold": 90, "min_rows_for_check": 10, "eval_shift_threshold_cp": 50}


def compute_anomalies(db_path=None):
    """Return a list of anomaly strings for each engine_label present in search_features.

    This function computes in-memory anomaly indicators and intentionally does NOT
    write any files — per user preference we keep export/file I/O out of this module.
    """
    if db_path is None:
        db_path = Path.home() / 'Documents' / 'databases' / 'chess_analytics.duckdb'
    con = duckdb.connect(str(db_path))
    cfg = load_config()
    min_rows = cfg.get('min_rows_for_check', 10)

    try:
        df = con.execute(
            'SELECT engine_label, count(*) AS n, SUM(COALESCE(fail_highs,0)) AS fail_highs, SUM(COALESCE(fail_lows,0)) AS fail_lows FROM search_features GROUP BY engine_label'
        ).df()
    except Exception:
        con.close()
        return ["search_features not present or query failed"]

    anomalies = []
    for _, r in df.iterrows():
        engine = r['engine_label']
        n = int(r['n'])
        if n < min_rows:
            anomalies.append(f"{engine}: only {n} rows (below min {min_rows})")
            continue
        fh = int(r['fail_highs'])
        fl = int(r['fail_lows'])
        if fh == 0:
            anomalies.append(f"{engine}: fail_highs = 0 across {n} rows")
        if fl == 0:
            anomalies.append(f"{engine}: fail_lows = 0 across {n} rows")
    con.close()
    return anomalies
