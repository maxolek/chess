#!/usr/bin/env python3
"""Run the full ETL pipeline from the data package.

Usage:
  python3 -m data.run_pipeline
  or
  python3 data/run_pipeline.py
"""
import subprocess
import sys
from pathlib import Path
import argparse


MODULES = [
    "data.load_analytics",
    "data.transform_positions",
    "data.transform_search",
    "data.normalize_metadata",
]



def run_module(module: str) -> None:
    print(f"\n=== Running module: {module} ===")
    cmd = [sys.executable, "-m", module]
    try:
        # run subprocess from repository root so package imports like 'data.*' resolve
        repo_root = Path(__file__).resolve().parent.parent
        subprocess.run(cmd, check=True, cwd=str(repo_root))
    except subprocess.CalledProcessError as e:
        print(f"Module {module} failed with exit code {e.returncode}")
        raise


def main():
    cwd = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description="Run ETL pipeline modules")
    parser.add_argument('--skip-load', action='store_true', help='Skip data.load_analytics step')
    args = parser.parse_args()

    print(f"Running pipeline from: {cwd}")
    modules_to_run = MODULES[1:] if args.skip_load else MODULES
    if args.skip_load:
        print("Skipping data.load_analytics (use --skip-load to enable)")
    for m in modules_to_run:
        run_module(m)
    print("\nPipeline completed successfully.")


if __name__ == '__main__':
    main()
