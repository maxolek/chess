import chess
import chess.engine
import random
import time
import os
from datetime import datetime
import concurrent.futures
import threading

import book  # your book.py

# ---- Engine Configuration ----
ENGINES = {
    #"v4.2_evenDepth": r"C:\Users\maxol\code\chess\version_history\v4.2_evenDepth.exe",
    #"v5_phoenix": r"C:\Users\maxol\code\chess\version_history\v5_phoenix.exe",
    #"v5.1_tt": r"C:\Users\maxol\code\chess\version_history\v5.1_tt.exe",
    #"v5.2_rootOrdering": r"C:\Users\maxol\code\chess\version_history\v5.2_rootOrdering.exe",
    #"v5.3_pvOrdering": r"C:\Users\maxol\code\chess\version_history\v5.3_pvOrdering.exe",
    "v5.4_kingSafety": r"C:\Users\maxol\code\chess\version_history\v5.4_kingSafety.exe",
    "v5.5_aspiration": r"C:\Users\maxol\code\chess\version_history\v5.5_aspiration.exe",
    "vTEST": r"C:\Users\maxol\code\chess\tomahawk\tomahawk.exe"
}

GAMES_PER_PAIR = 16
TIME_LIMIT = 1
LOG_DIR = "logs"
PGN_FILE = os.path.join(LOG_DIR, f"tournament_{datetime.now().strftime('%Y%m%d_%H%M%S')}.pgn")
os.makedirs(LOG_DIR, exist_ok=True)


# ---- Play a single game ----
def play_single_game(white_path, black_path, white_name, black_name, start_board):
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

            # --- ENGINE MOVE ---
            color = board.turn
            try:
                result = engines[color].play(board, chess.engine.Limit(time=TIME_LIMIT))
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
def write_pgn_threadsafe(lock, pgn_file, data):
    headers = [
        f'[White "{data["white_name"]}"]',
        f'[Black "{data["black_name"]}"]',
        f'[Result "{data["result"]}"]',
        f'[Date "{datetime.today().strftime("%Y.%m.%d")}"]',
        f'[TimeControl "{int(TIME_LIMIT*1000)}+0"]'
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


# ---- Tournament runner with head-to-head ----
def run_tournament_parallel():
    players = list(ENGINES.keys())
    scores = {name: 0.0 for name in players}

    # head-to-head dictionary
    head_to_head = {p: {q: 0.0 for q in players} for p in players}

    lock = threading.Lock()
    all_games = []

    for i in range(len(players)):
        for j in range(i + 1, len(players)):
            white, black = players[i], players[j]
            for _ in range(GAMES_PER_PAIR // 2):
                board = book.random_starting_position()
                # mirrored games
                all_games.append((ENGINES[white], ENGINES[black], white, black, board))
                all_games.append((ENGINES[black], ENGINES[white], black, white, board.copy()))

    print(f"\nStarting {len(all_games)} games...\n")
    start_time = time.time()

    with concurrent.futures.ProcessPoolExecutor() as executor:
        futures = [executor.submit(play_single_game, *args) for args in all_games]

        for idx, future in enumerate(concurrent.futures.as_completed(futures), 1):
            try:
                result = future.result()
                write_pgn_threadsafe(lock, PGN_FILE, result)

                white, black, res = result["white_name"], result["black_name"], result["result"]
                game_time = time.time() - start_time
                print(f"[Game {idx:03}] {white} vs {black} -> {res}  (⏱ {game_time:.1f}s)")

                # only count completed games
                if res == "1-0":
                    scores[white] += 1
                    head_to_head[white][black] += 1
                elif res == "0-1":
                    scores[black] += 1
                    head_to_head[black][white] += 1
                elif res == "1/2-1/2":
                    scores[white] += 0.5
                    scores[black] += 0.5
                    head_to_head[white][black] += 0.5
                    head_to_head[black][white] += 0.5

            except Exception as e:
                print(f"[Game {idx:03}] Error: {e}")

    total_time = time.time() - start_time
    print(f"\n🏁 Tournament complete in {total_time:.2f} seconds")
    print("\n📊 Final Standings:")
    for name, score in sorted(scores.items(), key=lambda x: -x[1]):
        print(f"{name:20} {score:.1f} pts")

    # ---- Print head-to-head table ----
    print("\n📈 Head-to-Head Results:")
    header = ["Engine"] + players
    print("{:<15}".format(header[0]), end="")
    for h in header[1:]:
        print("{:>8}".format(h), end="")
    print()
    for p in players:
        print("{:<15}".format(p), end="")
        for q in players:
            val = "-" if p == q else head_to_head[p][q]
            print("{:>8}".format(val), end="")
        print()

    print(f"\n📁 PGN log written to: {PGN_FILE}")


if __name__ == "__main__":
    run_tournament_parallel()
