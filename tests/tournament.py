import chess
import chess.engine
import random
import time
import os
from datetime import datetime
import concurrent.futures
import threading
import utils
import book  # your book.py
import math
import psutil

# ---- Engine Configuration ----
ENGINES = {
    #"large_net": r"..\engines\large_net.exe",
    #"medium_net": r"..\engines\medium_net.exe",
    "small_net": r"..\engines\small_net.exe",
    "static_eval": r"..\engines\classic.exe",
    "experiment": r"..\tomahawk\tomahawk.exe"
}

GAMES_PER_PAIR = 10 
LOG_DIR = "logs"
os.makedirs(LOG_DIR, exist_ok=True)

# ---- Pin entire tournament process to P-cores ----
#P_CORES = [0, 1, 6, 7, 8, 9, 18, 19]
#psutil.Process(os.getpid()).cpu_affinity(P_CORES)
#print(f"⚡ Tournament process pinned to P-cores: {P_CORES}")

# ---- Play a single game ----
def play_single_game(white_path, black_path, white_name, black_name, start_board, time_limit):
    board = start_board.copy()
    move_list = [m.uci() for m in start_board.move_stack]

    try:
        engines = {
            chess.WHITE: chess.engine.SimpleEngine.popen_uci(white_path),
            chess.BLACK: chess.engine.SimpleEngine.popen_uci(black_path)
        }

        while True:
            # --- TERMINAL CHECKS ---
            if board.is_checkmate():
                outcome = "1-0" if board.turn == chess.BLACK else "0-1"
                break
            if board.is_stalemate() or board.is_insufficient_material():
                outcome = "1/2-1/2"
                break
            if board.can_claim_threefold_repetition() or board.can_claim_fifty_moves():
                outcome = "1/2-1/2"
                break

            color = board.turn

            # --- ENGINE MOVE ---
            try:
                result = engines[color].play(board, chess.engine.Limit(time=time_limit))
            except Exception as e:
                print(f"❌ Engine error on move {board.fullmove_number} ({'White' if color else 'Black'}): {e}")
                outcome = "*"
                break

            move = result.move
            if move not in board.legal_moves:
                print(f"❌ Illegal move on move {board.fullmove_number}: {move}")
                outcome = "*"
                break

            board.push(move)
            move_list.append(move.uci())

    finally:
        for eng in engines.values():
            try:
                eng.quit()
            except Exception:
                pass

    return {
        "white_name": white_name,
        "black_name": black_name,
        "result": outcome,
        "moves": move_list,
        "final_board": board
    }

# ---- Thread-safe PGN writer ----
def write_pgn_threadsafe(lock, pgn_file, data, time_limit):
    headers = [
        f'[White "{data["white_name"]}"]',
        f'[Black "{data["black_name"]}"]',
        f'[Result "{data["result"]}"]',
        f'[Date "{datetime.today().strftime("%Y.%m.%d")}"]',
        f'[TimeControl "{int(time_limit * 1000)}+0"]'
    ]

    board = chess.Board()
    moves_str = ""
    for i, uci in enumerate(data["moves"]):
        if i % 2 == 0:
            moves_str += f"{board.fullmove_number}. "
        move = chess.Move.from_uci(uci)
        moves_str += board.san(move) + " "
        board.push(move)

    game_text = "\n".join(headers) + "\n\n" + moves_str.strip() + f" {data['result']}\n\n"

    with lock:
        with open(pgn_file, "a") as f:
            f.write(game_text)

# ---- Tournament Runner ----
def run_tournament_parallel(time_limit, max_threads=None):
    pgn_file = os.path.join(
        LOG_DIR,
        f"tournament_{time_limit}s_{datetime.now().strftime('%Y%m%d_%H%M%S')}.pgn"
    )

    players = list(ENGINES.keys())
    scores = {name: 0.0 for name in players}
    head_to_head = {p: {q: {"W":0,"L":0,"D":0} for q in players} for p in players}
    lock = threading.Lock()
    all_games = []

    # Prepare matchups
    for i in range(len(players)):
        for j in range(i + 1, len(players)):
            white, black = players[i], players[j]
            for _ in range(GAMES_PER_PAIR // 2):
                board = book.random_starting_position()
                all_games.append((ENGINES[white], ENGINES[black], white, black, board, time_limit))
                all_games.append((ENGINES[black], ENGINES[white], black, white, board.copy(), time_limit))

    print(f"\n=== Starting {len(all_games)} games @ {time_limit}s ===\n")
    start_time = time.time()

    # Limit concurrent games
    max_workers = max_threads or min(len(P_CORES), len(all_games))
    print(f"Using up to {max_workers} concurrent game processes.\n")

    with concurrent.futures.ProcessPoolExecutor(max_workers=max_workers) as executor:
        futures = [executor.submit(play_single_game, *args) for args in all_games]

        for idx, future in enumerate(concurrent.futures.as_completed(futures), 1):
            try:
                result = future.result()
                write_pgn_threadsafe(lock, pgn_file, result, time_limit)

                white = result["white_name"]
                black = result["black_name"]
                res = result["result"]

                print(f"[Game {idx:03}] {white} vs {black} -> {res}")

                if res == "1-0":
                    scores[white] += 1
                    head_to_head[white][black]["W"] += 1
                    head_to_head[black][white]["L"] += 1
                elif res == "0-1":
                    scores[black] += 1
                    head_to_head[black][white]["W"] += 1
                    head_to_head[white][black]["L"] += 1
                elif res == "1/2-1/2":
                    scores[white] += 0.5
                    scores[black] += 0.5
                    head_to_head[white][black]["D"] += 1
                    head_to_head[black][white]["D"] += 1

            except Exception as e:
                print(f"[Game {idx:03}] Error: {e}")

    total_time = time.time() - start_time
    print(f"\n🏁 Tournament ({time_limit}s) complete in {total_time:.2f}s")
    print("\n📊 Final Standings:")

    for name, score in sorted(scores.items(), key=lambda x: -x[1]):
        print(f"{name:20} {score:.1f} pts")

    # Head-to-Head table
    print("\n📈 Head-to-Head Breakdown:\n")
    print(f"{'Engine A':<22}{'Engine B':<22}{'W_A':>5}{'W_B':>5}{'D':>5}{'EloDiff':>10}")
    print("-"*69)
    for i, a in enumerate(players):
        for j, b in enumerate(players):
            if a == b:
                continue
            wc = head_to_head[a][b]["W"]
            lc = head_to_head[a][b]["L"]
            dc = head_to_head[a][b]["D"]
            total = wc + lc + dc
            score_frac = (wc + 0.5*dc)/total if total > 0 else 0.5
            if 0 < score_frac < 1:
                elo_diff = int(-400 * math.log10(1/score_frac - 1))
            elif score_frac == 1:
                elo_diff = "Inf"
            else:
                elo_diff = "-Inf"

            print(f"{a:<22}{b:<22}{wc:>5}{lc:>5}{dc:>5}{elo_diff:>10}")

    print(f"\n📁 PGN log written to: {pgn_file}\n")

# ---- Main ----
if __name__ == "__main__":
    run_tournament_parallel(1, max_threads=3)
    #run_tournament_parallel(3, max_threads=3)
    run_tournament_parallel(5, max_threads=3)
    #run_tournament_parallel(10, max_threads=2)
    #run_tournament_parallel(30, max_threads=3)
    #run_tournament_parallel(60, max_threads=3)
