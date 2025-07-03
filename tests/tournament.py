import subprocess
import chess
import chess.engine
import random
import time
import os
from datetime import datetime

ENGINES = {
    "it_deep": r"C:\Users\maxol\code/chess\version_history\v1.1_iterative_deepening.exe",
    "tap_ev": r"C:\Users\maxol\code/chess\version_history\v1.2_tapered_eval.exe",
    "alphabeta": r"C:\Users\maxol\code/chess\version_history\v1.3_alphabeta.exe",
    "transpoition_table": r"C:\Users\maxol\code/chess\version_history\v1.4_transposition_table.exe"
    # Add more as needed
    #"stockfish": "engines/stockfish.exe"
}

GAMES_PER_PAIR = 50  # 2 as white, 2 as black
TIME_LIMIT = 5  # seconds per move

LOG_DIR = "logs"
PGN_FILE = os.path.join(LOG_DIR, f"tournament_{datetime.now().strftime('%Y%m%d_%H%M%S')}.pgn")

os.makedirs(LOG_DIR, exist_ok=True)


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
    run_tournament()
