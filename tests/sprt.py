import os
import math
import chess
import chess.engine
from datetime import datetime
import book  # your book.py

# ---- Engine Paths ----
ENGINE_A = r"C:\Users\maxol\code\chess\tomahawk\tomahawk.exe"  # candidate
ENGINE_B = r"C:\Users\maxol\code\chess\version_history\v5.2_rootOrdering.exe"  # baseline

# ---- SPRT Settings ----
elo0 = 0        # null hypothesis: no improvement
elo1 = 50       # alt hypothesis: at least +50 Elo
alpha = 0.05    # type I error rate
beta = 0.05     # type II error rate
max_games = 1000
TIME_LIMIT = .5      # seconds per move
BOOK_MAX_DEPTH = 8  # number of book moves to play

# ---- PGN Output ----
LOG_DIR = "logs"
os.makedirs(LOG_DIR, exist_ok=True)
PGN_FILE = os.path.join(
    LOG_DIR, f"sprt_{os.path.basename(ENGINE_A)}_{os.path.basename(ENGINE_B)}.pgn"
)

# ---- LLR / Elo Helpers ----
def prob_win_draw_loss(elo):
    p_win = 1 / (1 + 10 ** (-elo / 400))
    p_loss = 1 / (1 + 10 ** (elo / 400))
    p_draw = 1 - p_win - p_loss
    return p_win, p_draw, p_loss

def llr(n_w, n_d, n_l, elo0, elo1):
    p0w, p0d, p0l = prob_win_draw_loss(elo0)
    p1w, p1d, p1l = prob_win_draw_loss(elo1)
    eps = 1e-10
    log = lambda x: math.log(max(x, eps))
    return (
        n_w * (log(p1w) - log(p0w)) +
        n_d * (log(p1d) - log(p0d)) +
        n_l * (log(p1l) - log(p0l))
    )

def estimate_elo(n_win, n_draw, n_loss):
    s = n_win + 0.5 * n_draw
    n = n_win + n_draw + n_loss
    if s == 0 or s == n:  # avoid log(0)
        return float('inf') if s == n else float('-inf')
    return 400 * math.log10(s / (n - s))

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
def play_game(engine_a_path, engine_b_path, opening_moves, swap_colors=False):
    white_path = engine_b_path if swap_colors else engine_a_path
    black_path = engine_a_path if swap_colors else engine_b_path
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
    except Exception as e:
        print(f"Engine start failed: {e}")
        return None

    engines = {chess.WHITE: white, chess.BLACK: black}

    try:
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
            except chess.engine.EngineTerminatedError as e:
                print(f"❌ Engine crashed on move {board.fullmove_number}, color {'White' if color else 'Black'}: {e}")
                return None
            move = result.move
            if move not in board.legal_moves:
                print(f"❌ Illegal move on move {board.fullmove_number}: {move}")
                return None
            board.push(move)
            move_list.append(move.uci())

    finally:
        white.quit()
        black.quit()

    write_pgn(white_name, black_name, outcome, move_list)

    # return candidate perspective
    if outcome == "1-0":
        return 0 if swap_colors else 1
    elif outcome == "0-1":
        return 1 if swap_colors else 0
    else:
        return 0.5

# ---- Main SPRT loop ----
def run_sprt():
    n_win = n_draw = n_loss = 0
    lower = math.log(beta / (1 - alpha))
    upper = math.log((1 - beta) / alpha)
    game_num = 0

    # load book once
    book_games = book.load_book("book.pgn")

    while game_num < max_games:
        opening_moves = book.get_random_opening(book_games, BOOK_MAX_DEPTH)

        # candidate as White
        result = play_game(ENGINE_A, ENGINE_B, opening_moves, swap_colors=False)
        if result is not None:
            game_num += 1
            if result == 1: n_win += 1
            elif result == 0.5: n_draw += 1
            elif result == 0: n_loss += 1

            llr_score = llr(n_win, n_draw, n_loss, elo0, elo1)
            elo_est = estimate_elo(n_win, n_draw, n_loss)
            print(f"[Game {game_num:04}] W:{n_win} D:{n_draw} L:{n_loss} | LLR: {llr_score:.4f} | Est. Elo diff: {elo_est:.0f}")
            if llr_score <= lower or llr_score >= upper or game_num >= max_games:
                break
        else:
            print("⚠ Skipping game due to engine error or illegal move.")

        # candidate as Black
        result = play_game(ENGINE_A, ENGINE_B, opening_moves, swap_colors=True)
        if result is not None:
            game_num += 1
            if result == 1: n_win += 1
            elif result == 0.5: n_draw += 1
            elif result == 0: n_loss += 1

            llr_score = llr(n_win, n_draw, n_loss, elo0, elo1)
            elo_est = estimate_elo(n_win, n_draw, n_loss)
            print(f"[Game {game_num:04}] W:{n_win} D:{n_draw} L:{n_loss} | LLR: {llr_score:.4f} | Est. Elo diff: {elo_est:.0f}")
            if llr_score <= lower or llr_score >= upper or game_num >= max_games:
                break
        else:
            print("⚠ Skipping game due to engine error or illegal move.")

    total = n_win + n_draw + n_loss
    print(f"\n📊 Final: {n_win} wins, {n_draw} draws, {n_loss} losses (out of {total})")
    print(f"📁 PGN log written to: {PGN_FILE}")


if __name__ == "__main__":
    run_sprt()
