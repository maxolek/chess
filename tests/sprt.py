import os
import math
import chess
import chess.engine
from datetime import datetime

# ---- Engine Paths ----
ENGINE_A = r"C:\Users\maxol\code\chess\tomahawk\tomahawk.exe"  # candidate
ENGINE_B = r"C:\Users\maxol\code\chess\version_history\v4.2_evenDepth.exe" # baseline

# ---- SPRT Settings ----
elo0 = 0        # null hypothesis: no improvement
elo1 = 50        # alt hypothesis: at least +5 Elo
alpha = 0.1    # type I error rate
beta = 0.1     # type II error rate
max_games = 10000
TIME_LIMIT = .5  # seconds per move

# ---- PGN Output ----
LOG_DIR = "logs"
os.makedirs(LOG_DIR, exist_ok=True)
PGN_FILE = os.path.join(LOG_DIR, f"sprt_{os.path.basename(ENGINE_A)}_{os.path.basename(ENGINE_B)}.pgn")

# ---- LLR Helper ----
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
        f'[TimeControl "{int(TIME_LIMIT * 1000)}+0"]'
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

# ---- Play a Single Game ----
def play_game(engine_a_path, engine_b_path, swap_colors):
    white_path = engine_b_path if swap_colors else engine_a_path
    black_path = engine_a_path if swap_colors else engine_b_path
    white_name = os.path.basename(white_path)
    black_name = os.path.basename(black_path)

    board = chess.Board()
    move_list = []

    try:
        white = chess.engine.SimpleEngine.popen_uci(white_path)
    except Exception as e:
        print(f"Failed to start White engine ({white_name}): {e}")
        return None

    try:
        black = chess.engine.SimpleEngine.popen_uci(black_path)
    except Exception as e:
        print(f"Failed to start Black engine ({black_name}): {e}")
        white.quit()
        return None

    engines = {chess.WHITE: white, chess.BLACK: black}

    try:
        while not board.is_game_over():
            color = board.turn
            try:
                result = engines[color].play(board, chess.engine.Limit(time=TIME_LIMIT))
            except chess.engine.EngineTerminatedError as e:
                print(f"❌ Engine crashed on move {board.fullmove_number}, color {'White' if color else 'Black'}: {e}")
                print("Board state at crash:")
                print(board)
                return None
            except Exception as e:
                print(f"❌ Unexpected engine error on move {board.fullmove_number}: {e}")
                print(board)
                return None

            move = result.move
            if move not in board.legal_moves:
                print(f"❌ Engine returned illegal move on move {board.fullmove_number}: {move}")
                print(board)
                return None

            board.push(move)
            move_list.append(move.uci())

        outcome = board.outcome()
        result_str = outcome.result() if outcome else "*"

    finally:
        try:
            white.quit()
        except Exception as e:
            print(f"⚠️ White engine quit failed: {e}")
        try:
            black.quit()
        except Exception as e:
            print(f"⚠️ Black engine quit failed: {e}")

    write_pgn(white_name, black_name, result_str, move_list)

    # Candidate engine perspective
    if result_str == "1-0":
        return 0 if swap_colors else 1
    elif result_str == "0-1":
        return 1 if swap_colors else 0
    else:
        return 0.5


# ---- Main SPRT Runner ----
def run_sprt():
    n_win = n_draw = n_loss = 0
    lower = math.log(beta / (1 - alpha))
    upper = math.log((1 - beta) / alpha)

    for game_num in range(1, max_games + 1):
        swap = game_num % 2 == 0
        result = play_game(ENGINE_A, ENGINE_B, swap)

        if result is None:
            print("Skipping game due to engine crash.")
            continue
        elif result == 1:
            n_win += 1
        elif result == 0.5:
            n_draw += 1
        else:
            n_loss += 1

        llr_score = llr(n_win, n_draw, n_loss, elo0, elo1)
        elo_est = estimate_elo(n_win, n_draw, n_loss)
        print(f"[Game {game_num:04}] W:{n_win} D:{n_draw} L:{n_loss} | LLR: {llr_score:.4f} | Est. Elo diff: {elo_est:.0f}")

        if llr_score <= lower:
            print("\n❌ H0 accepted: no Elo gain\n")
            break
        elif llr_score >= upper:
            print("\n✅ H1 accepted: candidate is stronger\n")
            break

    total = n_win + n_draw + n_loss
    print(f"\n📊 Final: {n_win} wins, {n_draw} draws, {n_loss} losses (out of {total})")
    print(f"📁 PGN log written to: {PGN_FILE}")

if __name__ == "__main__":
    run_sprt()
