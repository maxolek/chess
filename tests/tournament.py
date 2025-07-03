import subprocess
import chess
import chess.engine
import random
import time
import os
from datetime import datetime
import concurrent.futures
import threading

ENGINES = {
    "it_deep": r"C:\Users\maxol\code\chess\version_history\v1.1_iterative_deepening.exe",
    "tap_ev": r"C:\Users\maxol\code\chess\version_history\v1.2_tapered_eval.exe",
    "alphabeta": r"C:\Users\maxol\code\chess\version_history\v1.3_alphabeta.exe",
    "transpoition_table": r"C:\Users\maxol\code\chess\version_history\v1.4_transposition_table.exe",
    "move_list_array": r"C:\Users\maxole\code\chess\version_history\v1.5_moveListArray.exe",
    "pstLoadFIX": r"C:\Users\maxole\code\chess\version_history\v1.6_pstLoadFIX.exe"
    # Add more as needed
    #"stockfish": "engines/stockfish.exe"
}

GAMES_PER_PAIR = 5  # 3 as white, 2 as black
TIME_LIMIT = 5  # seconds per move

LOG_DIR = "logs"
PGN_FILE = os.path.join(LOG_DIR, f"tournament_{datetime.now().strftime('%Y%m%d_%H%M%S')}.pgn")

os.makedirs(LOG_DIR, exist_ok=True)

def play_single_game(white_path, black_path, white_name, black_name):
    """Play a single game, return results in dict."""
    board = chess.Board()
    engines = {
        chess.WHITE: chess.engine.SimpleEngine.popen_uci(white_path),
        chess.BLACK: chess.engine.SimpleEngine.popen_uci(black_path)
    }

    game_moves = []
    while not board.is_game_over():
        color = board.turn
        result = engines[color].play(board, chess.engine.Limit(time=TIME_LIMIT))
        move = result.move
        if move not in board.legal_moves:
            break
        board.push(move)
        game_moves.append(move.uci())

    outcome = board.outcome()
    result_str = outcome.result() if outcome else "*"

    for engine in engines.values():
        engine.quit()

    return {
        "white_name": white_name,
        "black_name": black_name,
        "result": result_str,
        "moves": game_moves,
        "final_board": board
    }

def write_pgn_threadsafe(lock, pgn_file, data):
    """Thread-safe PGN write."""
    headers = [
        f'[White "{data["white_name"]}"]',
        f'[Black "{data["black_name"]}"]',
        f'[Result "{data["result"]}"]',
        f'[Date "{datetime.today().strftime("%Y.%m.%d")}"]',
        f'[TimeControl "{int(TIME_LIMIT * 1000)}+0"]'
    ]

    moves_str = ""
    temp_board = chess.Board()
    for i, uci in enumerate(data["moves"]):
        if i % 2 == 0:
            moves_str += f"{temp_board.fullmove_number}. "
        move = chess.Move.from_uci(uci)
        moves_str += temp_board.san(move) + " "
        temp_board.push(move)

    game_text = "\n".join(headers) + "\n\n" + moves_str.strip() + f" {data['result']}\n\n"

    with lock:
        with open(pgn_file, "a") as f:
            f.write(game_text)

def run_tournament_parallel():
    players = list(ENGINES.keys())
    scores = {name: 0 for name in players}
    lock = threading.Lock()

    # Prepare all games to play
    all_games = []
    for i in range(len(players)):
        for j in range(i + 1, len(players)):
            white, black = players[i], players[j]
            for _ in range(GAMES_PER_PAIR // 2):
                all_games.append((ENGINES[white], ENGINES[black], white, black))
                all_games.append((ENGINES[black], ENGINES[white], black, white))

    with concurrent.futures.ProcessPoolExecutor() as executor:
        futures = []
        for game_args in all_games:
            futures.append(executor.submit(play_single_game, *game_args))

        for future in concurrent.futures.as_completed(futures):
            try:
                result = future.result()
                write_pgn_threadsafe(lock, PGN_FILE, result)

                # Update scores safely (since we are in main process, no concurrency here)
                if result["result"] == "1-0":
                    scores[result["white_name"]] += 1
                elif result["result"] == "0-1":
                    scores[result["black_name"]] += 1
                elif result["result"] == "1/2-1/2":
                    scores[result["white_name"]] += 0.5
                    scores[result["black_name"]] += 0.5
            except Exception as e:
                print(f"Game error: {e}")

    print("\n🏁 Final Standings:")
    for name, score in sorted(scores.items(), key=lambda x: -x[1]):
        print(f"{name}: {score} pts") 


def play_game(white_path, black_path, white_name, black_name):
    board = chess.Board()
    engines = {
        chess.WHITE: chess.engine.SimpleEngine.popen_uci(white_path),
        chess.BLACK: chess.engine.SimpleEngine.popen_uci(black_path)
    }

    game_moves = []
    while not board.is_game_over():
        color = board.turn
        result = engines[color].play(board, chess.engine.Limit(time=TIME_LIMIT))
        move = result.move
        if move not in board.legal_moves:
            break
        board.push(move)
        game_moves.append(move.uci())

    outcome = board.outcome()
    result_str = outcome.result() if outcome else "*"

    for engine in engines.values():
        engine.quit()

    return game_moves, result_str, board


def write_pgn(white_name, black_name, result, moves, board):
    headers = [
        f'[White "{white_name}"]',
        f'[Black "{black_name}"]',
        f'[Result "{result}"]',
        f'[Date "{datetime.today().strftime("%Y.%m.%d")}"]',
        f'[TimeControl "{int(TIME_LIMIT * 1000)}+0"]'
    ]

    moves_str = ""
    temp_board = chess.Board()
    for i, uci in enumerate(moves):
        if i % 2 == 0:
            moves_str += f"{temp_board.fullmove_number}. "
        move = chess.Move.from_uci(uci)
        moves_str += temp_board.san(move) + " "
        temp_board.push(move)

    game_text = "\n".join(headers) + "\n\n" + moves_str.strip() + f" {result}\n\n"
    with open(PGN_FILE, "a") as f:
        f.write(game_text)


def run_tournament():
    players = list(ENGINES.keys())
    scores = {name: 0 for name in players}

    for i in range(len(players)):
        for j in range(i + 1, len(players)):
            white, black = players[i], players[j]
            for k in range(GAMES_PER_PAIR // 2):
                for w, b in [(white, black), (black, white)]:
                    print(f"\nPlaying {w} (white) vs {b} (black)")
                    moves, result, board = play_game(ENGINES[w], ENGINES[b], w, b)
                    write_pgn(w, b, result, moves, board)

                    if result == "1-0":
                        scores[w] += 1
                    elif result == "0-1":
                        scores[b] += 1
                    elif result == "1/2-1/2":
                        scores[w] += 0.5
                        scores[b] += 0.5

    print("\n🏁 Final Standings:")
    for name, score in sorted(scores.items(), key=lambda x: -x[1]):
        print(f"{name}: {score} pts")


if __name__ == "__main__":
    #run_tournament()
    run_tournament_parallel()
