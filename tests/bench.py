import subprocess
import time
import re
import sys
from dataclasses import dataclass, field
from typing import Optional

# --- Config ---
ENGINES = [
    "engines/0.0.0.exe",
    "engines/0.1.0.exe",
]

POSITIONS = [
    ("startpos",                    "Starting position"),
    ("r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3", "Ruy Lopez"),
    ("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", "Kiwipete"),
    ("3r1rk1/pppqb1pp/1nn2p2/4p3/1P4b1/P1NPBNP1/2Q1PPBP/2R2RK1 b - - 3 13", "Middlegame"),
    ("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", "Endgame"),
]

MOVETIME_MS  = 2000
RUNS_PER_POS = 1  # average over multiple runs


@dataclass
class SearchStats:
    nps:          list[float] = field(default_factory=list)
    nodes:        list[float] = field(default_factory=list)
    depth:        list[float] = field(default_factory=list)
    eval:         list[float] = field(default_factory=list)
    tt_hit_rate:  list[float] = field(default_factory=list)
    fh_first_pct: list[float] = field(default_factory=list)
    see_pruned:   list[float] = field(default_factory=list)
    nmp_success:  list[float] = field(default_factory=list)

    def avg(self, key):
        vals = getattr(self, key)
        return sum(vals) / len(vals) if vals else 0


def print_results(results: dict, engines: list[str], positions: list[tuple]):
    baseline = engines[0]

    for _, pos_name in positions:
        print(f"\n{'='*70}")
        print(f"  {pos_name}")
        print(f"{'='*70}")
        print(f"  {'Engine':<20} {'NPS':>10} {'Nodes':>10} {'Depth':>6} {'TT%':>6} {'FH1%':>6} {'NMP%':>6}  {'vs baseline':>11}")
        print(f"  {'-'*20} {'-'*10} {'-'*10} {'-'*6} {'-'*6} {'-'*6} {'-'*6}  {'-'*11}")

        base_nps = results[baseline][pos_name].avg("nps")

        for engine in engines:
            s = results[engine][pos_name]
            pct = ((s.avg("nps") / base_nps) - 1) * 100 if base_nps and engine != baseline else None
            pct_str = f"{pct:+.1f}%" if pct is not None else "baseline"
            name = engine.split("/")[-1].replace(".exe", "")
            print(f"  {name:<20} {s.avg('nps'):>10,.0f} {s.avg('nodes'):>10,.0f} "
                  f"{s.avg('depth'):>6.1f} {s.avg('tt_hit_rate'):>6.1f} "
                  f"{s.avg('fh_first_pct'):>6.1f} {s.avg('nmp_success'):>6.1f}  {pct_str:>11}")


def parse_dumpstats(output: str) -> dict:
    stats = {}
    patterns = {
        "eval":           r"Eval\s*:\s*(-?\d+)",
        "time":           r"Time \(ms\)\s*:\s*(\d+)",
        "depth":          r"Completed Depth\s*:\s*(\d+)",
        "nodes":          r"Total\s*:\s*(\d+)",
        "qnodes":         r"Quiescence\s*:\s*(\d+)",
        "nps":            r"NPS\s*:\s*(\d+)",
        "tt_hit_rate":    r"Hit Rate\s*:\s*([\d.]+)%",
        "fh_first_pct":   r"FH First %\s*:\s*([\d.]+)%",
        "see_pruned":     r"SEE\s*:\s*(\d+)",
        "delta_pruned":   r"Delta\s*:\s*(\d+)",
        "nmp_attempts":   r"Attempts\s*:\s*(\d+)",
        "nmp_success":    r"Success %\s*:\s*([\d.]+)%",
    }
    for key, pattern in patterns.items():
        m = re.search(pattern, output)
        if m:
            stats[key] = float(m.group(1))
    return stats


def run_search(engine_path: str, position: str, movetime: int) -> Optional[dict]:
    try:
        proc = subprocess.Popen(
            [engine_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
        )

        if position == "startpos":
            pos_cmd = "position startpos\n"
        else:
            pos_cmd = f"position fen {position}\n"

        commands = f"uci\nisready\n{pos_cmd}go movetime {movetime}\n"
        proc.stdin.write(commands)
        proc.stdin.flush()

        # wait for bestmove
        start = time.time()
        while time.time() - start < (movetime / 1000) + 5:
            line = proc.stdout.readline().strip()
            if line.startswith("bestmove"):
                break

        # request dumpstats and collect output
        proc.stdin.write("dumpstats\n")
        proc.stdin.flush()

        dump_output = []
        start = time.time()
        while time.time() - start < 3:
            line = proc.stdout.readline()
            if not line:
                break
            dump_output.append(line)
            if "----" in line and len(dump_output) > 10:
                # end of per-depth table
                break

        proc.stdin.write("quit\n")
        proc.stdin.flush()
        proc.wait(timeout=3)

        return parse_dumpstats("".join(dump_output))

    except Exception as e:
        print(f"  Error running {engine_path}: {e}")
        return None


def benchmark(engines: list[str], positions: list[tuple], movetime: int, runs: int) -> dict:
    # results[engine][pos_name] = SearchStats
    results = {e: {name: SearchStats() for _, name in positions} for e in engines}

    total = len(engines) * len(positions) * runs
    done = 0

    for pos_fen, pos_name in positions:
        for engine in engines:
            for run in range(runs):
                done += 1
                print(f"[{done}/{total}] {engine} | {pos_name} | run {run+1}/{runs}", end="\r")
                stats = run_search(engine, pos_fen, movetime)
                if stats:
                    s = results[engine][pos_name]
                    if "nps"          in stats: s.nps.append(stats["nps"])
                    if "nodes"        in stats: s.nodes.append(stats["nodes"])
                    if "depth"        in stats: s.depth.append(stats["depth"])
                    if "eval"         in stats: s.eval.append(stats["eval"])
                    if "tt_hit_rate"  in stats: s.tt_hit_rate.append(stats["tt_hit_rate"])
                    if "fh_first_pct" in stats: s.fh_first_pct.append(stats["fh_first_pct"])
                    if "see_pruned"   in stats: s.see_pruned.append(stats["see_pruned"])
                    if "nmp_success"  in stats: s.nmp_success.append(stats["nmp_success"])

    print()
    return results


def print_results(results: dict, engines: list[str], positions: list[tuple]):
    baseline = engines[0]

    for _, pos_name in positions:
        print(f"\n{'='*60}")
        print(f"  {pos_name}")
        print(f"{'='*60}")
        print(f"  {'Engine':<25} {'NPS':>10} {'Nodes':>10} {'Depth':>6} {'Eval':>6}  {'NPS vs baseline':>15}")
        print(f"  {'-'*25} {'-'*10} {'-'*10} {'-'*6} {'-'*6}  {'-'*15}")

        base_nps = results[baseline][pos_name].avg("nps")

        for engine in engines:
            s = results[engine][pos_name]
            avg_nps   = s.avg("nps")
            avg_nodes = s.avg("nodes")
            avg_qnodes = s.avg('qnodes')
            avg_depth = s.avg("depth")
            avg_eval  = s.avg("eval")
            pct = ((avg_nps / base_nps) - 1) * 100 if base_nps else 0
            pct_str = f"{pct:+.1f}%" if engine != baseline else "baseline"
            name = engine.split("/")[-1].replace(".exe", "")
            print(f"  {name:<25} {avg_nps:>10,.0f} {avg_nodes:>10,.0f} {avg_qnodes:>10,.0f} {avg_depth:>6.1f} {avg_eval:>6.0f}  {pct_str:>15}")

    # summary: avg nps gain across all positions
    print(f"\n{'='*60}")
    print(f"  NPS summary (avg across all positions)")
    print(f"{'='*60}")
    base_nps_vals = [results[baseline][name].avg("nps") for _, name in positions]
    for engine in engines:
        eng_nps_vals = [results[engine][name].avg("nps") for _, name in positions]
        overall_pct = sum(
            ((e / b) - 1) * 100 for e, b in zip(eng_nps_vals, base_nps_vals) if b
        ) / len(positions)
        name = engine.split("/")[-1].replace(".exe", "")
        label = "baseline" if engine == baseline else f"{overall_pct:+.1f}%"
        print(f"  {name:<25} {label}")


if __name__ == "__main__":
    engines = ENGINES
    move_time_ms = MOVETIME_MS
    runs = RUNS_PER_POS
    positions = POSITIONS

    if "--engines" in sys.argv:
        idx = sys.argv.index("--engines")
        # collect all args after --engines until next flag
        engines = []
        for arg in sys.argv[idx+1:]:
            if arg.startswith("--"): break
            engines.append(arg)

    if "--time" in sys.argv:
        idx = sys.argv.index("--time")
        move_time_ms = int(float(sys.argv[idx+1]) * 1000)  # accept seconds like sprt

    if "--runs" in sys.argv:
        idx = sys.argv.index("--runs")
        runs = int(sys.argv[idx+1])

    if "--positions" in sys.argv:
        idx = sys.argv.index("--positions")
        path = sys.argv[idx+1]
        positions = []
        with open(path) as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"): continue
                # format: "fen | name" or just "fen"
                if "|" in line:
                    fen, name = line.split("|", 1)
                    positions.append((fen.strip(), name.strip()))
                else:
                    positions.append((line, line[:40]))

    print(f"Benchmarking {len(engines)} engines over {len(positions)} positions x {runs} runs @ {move_time_ms}ms each\n")
    results = benchmark(engines, positions, move_time_ms, runs)
    print_results(results, engines, positions)