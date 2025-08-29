# stockfish_elo_estimate.py
import os
import math
import chess
import chess.engine
import random
from datetime import datetime
import book  # your book.py
import utils

# ---- Engine Paths ----
CANDIDATE = r"..\tomahawk\tomahawk.exe"
STOCKFISH = r"..\stockfish-windows-x86-64-avx2\stockfish\stockfish-windows-x86-64-avx2"

# ---- Time & Skill Settings ----
TIME_LIMIT = .1             # seconds per move
BOOK_MAX_DEPTH = 8         # opening book moves
GAMES_TO_PLAY = 20         # total games
STOCKFISH_SKILL_LEVEL = 5  # approx 1800 Elo

# ---- PGN Output ----
LOG_DIR = "logs"
os.makedirs(LOG_DIR, exist_ok=True)
PGN_FILE = os.path.join(LOG_DIR, f"stockfish_elo_estimate_{datetime.now().strftime('%Y%m%d_%H%M%S')}.pgn")

# ---- Win/draw/loss counters ----
n_win = n_draw = n_loss = 0

# ---- PGN Writer ----
def write_pgn(white_name, black_name, result_str, moves):
    headers = [
        f'[White "{white_name}"]',
        f'[Black "{black_name}"]',
        f'[Result "{result_str}"]',
        f'[Date "{datetime.today().strftime("%Y.%m.%d")}"]',
        f'[TimeControl "{int(TIME_LIMIT*1000)}+0"]'
    ]
    board = chess.Board()
    moves_str = ""
    for i, uci in enumerate(moves):
        if i % 2 == 0:
            moves_str += f"{board.fullmove_number}. "
        move = chess.Move.from_uci(uci)
        moves_str += board.san(move) + " "
        board.push(move)
    game_text = "\n".join(headers) + "\n\n" + moves_str.strip() + f" {result_str}\n\n"
    with open(PGN_FILE, "a") as f:
        f.write(game_text)

# ---- Play a single game ----
def play_game(candidate_path, stockfish_path, opening_moves, swap_colors=False):
    white_path = stockfish_path if swap_colors else candidate_path
    black_path = candidate_path if swap_colors else stockfish_path
    white_name = os.path.basename(white_path)
    black_name = os.path.basename(black_path)

    board = chess.Board()
    move_list = []

    # push opening moves
    for move in opening_moves:
        if move not in board.legal_moves:
            break
        board.push(move)
        move_list.append(move.uci())

    try:
        white = chess.engine.SimpleEngine.popen_uci(white_path)
        black = chess.engine.SimpleEngine.popen_uci(black_path)

        # set Stockfish skill level
        if not swap_colors:  # candidate is white, Stockfish black
            black.configure({"Skill Level": STOCKFISH_SKILL_LEVEL})
        else:
            white.configure({"Skill Level": STOCKFISH_SKILL_LEVEL})

    except Exception as e:
        print(f"Engine start failed: {e}")
        return None

    engines = {chess.WHITE: white, chess.BLACK: black}

    try:
        while True:
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
            try:
                result = engines[color].play(board, chess.engine.Limit(time=TIME_LIMIT))
            except Exception as e:
                print(f"❌ Engine error on move {board.fullmove_number}: {e}")
                outcome = "*"
                break

            move = result.move
            if move not in board.legal_moves:
                print(f"❌ Illegal move: {move}")
                outcome = "*"
                break

            board.push(move)
            move_list.append(move.uci())

    finally:
        white.quit()
        black.quit()

    write_pgn(white_name, black_name, outcome, move_list)

    if outcome == "1-0":
        return 0 if swap_colors else 1
    elif outcome == "0-1":
        return 1 if swap_colors else 0
    elif outcome == "1/2-1/2":
        return 0.5
    else:
        return None

# ---- Elo estimate ----
def estimate_elo(n_win, n_draw, n_loss):
    s = n_win + 0.5 * n_draw
    n = n_win + n_draw + n_loss
    if s == 0 or s == n:
        return float('inf') if s == n else float('-inf')
    return 400 * math.log10(s / (n - s))

# ---- Main loop ----
def main():
    global n_win, n_draw, n_loss
    book_games = book.load_book("book.pgn")
    for game_num in range(1, GAMES_TO_PLAY + 1):
        opening_moves = book.get_random_opening(book_games, BOOK_MAX_DEPTH)

        # Candidate as White
        result = play_game(CANDIDATE, STOCKFISH, opening_moves, swap_colors=False)
        if result == 1: n_win += 1
        elif result == 0.5: n_draw += 1
        elif result == 0: n_loss += 1

        # Candidate as Black
        result = play_game(CANDIDATE, STOCKFISH, opening_moves, swap_colors=True)
        if result == 1: n_win += 1
        elif result == 0.5: n_draw += 1
        elif result == 0: n_loss += 1

        elo_est = estimate_elo(n_win, n_draw, n_loss)
        print(f"[Games {game_num*2:03}] W:{n_win} D:{n_draw} L:{n_loss} | Est. Elo: {elo_est:.0f}")

    print(f"\n📊 Final Score: W:{n_win} D:{n_draw} L:{n_loss}")
    print(f"📁 PGN log written to: {PGN_FILE}")

if __name__ == "__main__":
    main()
