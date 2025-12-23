import os
import math
import threading
from datetime import datetime
import chess
import chess.engine
import tkinter as tk
from tkinter import Tk, Canvas, PhotoImage
from PIL import Image, ImageTk
import book  # your book.py
import utils

# ---- Engine Paths ----
ENGINE_A = r"../tomahawk/tomahawk.exe"  # candidate
ENGINE_B = r"../engines/small_net_stats.exe"  # baseline

# ---- SPRT Settings ----
elo0 = 0
elo1 = 50
alpha = 0.05
beta = 0.05
max_games = 1000
TIME_LIMIT = 1
BOOK_MAX_DEPTH = 8

# ---- PGN Output ----
LOG_DIR = "logs"
os.makedirs(LOG_DIR, exist_ok=True)
PGN_FILE = os.path.join(LOG_DIR, f"sprt_{os.path.basename(ENGINE_A)}_{os.path.basename(ENGINE_B)}.pgn")

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
    return n_w*(log(p1w)-log(p0w)) + n_d*(log(p1d)-log(p0d)) + n_l*(log(p1l)-log(p0l))

def estimate_elo(n_win, n_draw, n_loss):
    s = n_win + 0.5 * n_draw
    n = n_win + n_draw + n_loss
    if s == 0 or s == n:
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

# ---- Live Board GUI ----
class LiveBoard:
    def __init__(self, master, asset_dir="../visual_assets", square_size=40, anim_steps=10, anim_delay=10):
        """
        master: Tk root or frame
        asset_dir: folder containing piece PNGs
        square_size: pixel size of each square
        anim_steps: number of frames per move animation
        anim_delay: delay in ms between frames
        """
        self.master = master
        self.square_size = square_size
        self.anim_steps = anim_steps
        self.anim_delay = anim_delay
        self.asset_dir = asset_dir
        self.board = chess.Board()
        self.images = {}  # cached PhotoImages

        self.canvas = Canvas(master, width=8*square_size, height=8*square_size)
        self.canvas.pack()
        self.draw_board()
        self.load_all_images()

    # ---------------- Board Drawing ----------------
    def draw_board(self):
        color1 = "#F0D9B5"
        color2 = "#B58863"
        self.canvas.delete("square")
        for r in range(8):
            for f in range(8):
                x1 = f * self.square_size
                y1 = r * self.square_size
                x2 = x1 + self.square_size
                y2 = y1 + self.square_size
                color = color1 if (r+f)%2==0 else color2
                self.canvas.create_rectangle(x1, y1, x2, y2, fill=color, outline="", tags="square")

    # ---------------- Image Loading ----------------
    def load_piece_image(self, piece_char, is_white):
        color = 'l' if is_white else 'd'
        key = f"{piece_char}{color}"
        if key in self.images:
            return self.images[key]

        filename = f"Chess_{piece_char}{color}t60.png"
        path = os.path.join(self.asset_dir, filename)
        if not os.path.exists(path):
            print(f"⚠ Missing piece image: {filename}")
            return None

        img = Image.open(path)
        img = img.resize((self.square_size, self.square_size), Image.LANCZOS)
        photo = ImageTk.PhotoImage(img)
        self.images[key] = photo
        return photo

    def load_all_images(self):
        for piece in "prnbqk":
            for color in [True, False]:  # True=white, False=black
                self.load_piece_image(piece, color)

    # ---------------- GUI Update ----------------
    def update_gui(self):
        self.draw_board()
        self.canvas.delete("piece")
        for square in chess.SQUARES:
            piece = self.board.piece_at(square)
            if piece:
                is_white = piece.color == chess.WHITE
                key = f"{piece.symbol().lower()}{'l' if is_white else 'd'}"
                img = self.images.get(key)
                if img:
                    x = (square % 8) * self.square_size
                    y = (7 - square // 8) * self.square_size
                    self.canvas.create_image(x, y, anchor='nw', image=img, tags="piece")
        self.master.update_idletasks()

    # ---------------- Move Animation ----------------
    def animate_move(self, move):
        piece = self.board.piece_at(move.from_square)
        if piece is None:
            return

        is_white = piece.color == chess.WHITE
        key = f"{piece.symbol().lower()}{'l' if is_white else 'd'}"
        img = self.images.get(key)
        if img is None:
            return

        fx = (move.from_square % 8) * self.square_size
        fy = (7 - move.from_square // 8) * self.square_size
        tx = (move.to_square % 8) * self.square_size
        ty = (7 - move.to_square // 8) * self.square_size

        delta_x = (tx - fx) / self.anim_steps
        delta_y = (ty - fy) / self.anim_steps

        # Delete any piece at the source square so it doesn't remain
        items = self.canvas.find_overlapping(fx, fy, fx + self.square_size, fy + self.square_size)
        for item in items:
            if "piece" in self.canvas.gettags(item):
                self.canvas.delete(item)

        # Create the moving piece on top
        moving_piece = self.canvas.create_image(fx, fy, anchor='nw', image=img, tags="moving_piece")

        for _ in range(self.anim_steps):
            self.canvas.move(moving_piece, delta_x, delta_y)
            self.master.update_idletasks()
            self.master.after(self.anim_delay)

        # Remove captured piece at destination if any
        items = self.canvas.find_overlapping(tx, ty, tx + self.square_size, ty + self.square_size)
        for item in items:
            if "piece" in self.canvas.gettags(item):
                self.canvas.delete(item)

        # Update board state
        self.board.push(move)

        # Draw piece at destination
        self.canvas.delete(moving_piece)
        self.canvas.create_image(tx, ty, anchor='nw', image=img, tags="piece")


    # ---------------- Make Move ----------------
    def make_move(self, uci_move):
        """
        Push a move to the board and animate it.
        uci_move: string like 'e2e4' or 'e7e8q'
        """
        move = chess.Move.from_uci(uci_move)
        if move in self.board.legal_moves:
            self.animate_move(move)
        else:
            print(f"❌ Illegal move attempted: {uci_move} in {self.board.fen()}")

    # ---------------- Reset Board ----------------
    def reset_board(self):
        self.board.reset()
        self.update_gui()


# ---- Play a single game ----
def play_game(engine_a, engine_b, opening_moves, live_board=None, swap_colors=False):
    white_path = engine_b if swap_colors else engine_a
    black_path = engine_a if swap_colors else engine_b
    white_name = os.path.basename(white_path)
    black_name = os.path.basename(black_path)
    board = chess.Board()
    move_list = []

    if live_board:
        live_board.reset_board()

    # Apply opening moves
    for move in opening_moves:
        if move in board.legal_moves:
            board.push(move)
            move_list.append(move.uci())
            if live_board: live_board.make_move(move.uci())

    try:
        white_engine = chess.engine.SimpleEngine.popen_uci(white_path)
        black_engine = chess.engine.SimpleEngine.popen_uci(black_path)
    except Exception as e:
        print(f"Engine start failed: {e}")
        return None

    engines = {chess.WHITE: white_engine, chess.BLACK: black_engine}

    try:
        while True:
            if board.is_game_over(claim_draw=True):
                outcome = board.result(claim_draw=True)
                break

            turn = board.turn
            result = engines[turn].play(board, chess.engine.Limit(time=TIME_LIMIT))
            move = result.move
            if move not in board.legal_moves:
                print(f"Illegal move: {move}")
                return None
            board.push(move)
            move_list.append(move.uci())
            if live_board: live_board.make_move(move.uci())
    finally:
        white_engine.quit()
        black_engine.quit()

    write_pgn(white_name, black_name, outcome, move_list)

    if outcome == "1-0":
        return 0 if swap_colors else 1
    elif outcome == "0-1":
        return 1 if swap_colors else 0
    else:
        return 0.5

# ---- Main SPRT Loop ----
def run_sprt():
    n_win = n_draw = n_loss = 0
    lower = math.log(beta / (1 - alpha))
    upper = math.log((1 - beta) / alpha)
    game_num = 0

    book_games = book.load_book("../bin/Kasparov.pgn")

    root = Tk()
    root.title("Live SPRT Board")
    live_board = LiveBoard(root, square_size=70)

    def sprt_thread():
        nonlocal n_win, n_draw, n_loss, game_num
        while game_num < max_games:
            opening_moves = book.get_random_opening(book_games, BOOK_MAX_DEPTH)

            # Candidate as White
            result = play_game(ENGINE_A, ENGINE_B, opening_moves, live_board, swap_colors=False)
            if result is not None:
                game_num += 1
                if result == 1: n_win += 1
                elif result == 0.5: n_draw += 1
                elif result == 0: n_loss += 1

                llr_score = llr(n_win, n_draw, n_loss, elo0, elo1)
                elo_est = estimate_elo(n_win, n_draw, n_loss)
                print(f"[Game {game_num:04}] W:{n_win} D:{n_draw} L:{n_loss} | LLR:{llr_score:.4f} | Est. Elo:{elo_est:.0f}")
                if llr_score <= lower or llr_score >= upper or game_num >= max_games:
                    break

            # Candidate as Black
            result = play_game(ENGINE_A, ENGINE_B, opening_moves, live_board, swap_colors=True)
            if result is not None:
                game_num += 1
                if result == 1: n_win += 1
                elif result == 0.5: n_draw += 1
                elif result == 0: n_loss += 1

                llr_score = llr(n_win, n_draw, n_loss, elo0, elo1)
                elo_est = estimate_elo(n_win, n_draw, n_loss)
                print(f"[Game {game_num:04}] W:{n_win} D:{n_draw} L:{n_loss} | LLR:{llr_score:.4f} | Est. Elo:{elo_est:.0f}")
                if llr_score <= lower or llr_score >= upper or game_num >= max_games:
                    break

        total = n_win + n_draw + n_loss
        print(f"\n📊 Final: {n_win} wins, {n_draw} draws, {n_loss} losses (out of {total})")
        print(f"📁 PGN log: {PGN_FILE}")

    threading.Thread(target=sprt_thread, daemon=True).start()
    root.mainloop()

if __name__ == "__main__":
    run_sprt()
