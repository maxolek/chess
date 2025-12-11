import chess.pgn
import random

def load_book(file_path):
    """Load PGN book, return list of games"""
    games = []
    with open(file_path, "r") as f:
        while True:
            game = chess.pgn.read_game(f)
            if game is None:
                break
            games.append(game)
    return games

def get_random_opening(games, max_depth=8):
    """Pick a random opening from the book, truncated to max_depth half-moves"""
    opening_game = random.choice(games)
    board = opening_game.board()
    moves = []
    for i, move in enumerate(opening_game.mainline_moves()):
        if i >= max_depth:
            break
        board.push(move)
        moves.append(move)
    return moves

def random_starting_position(games=None, max_depth=8):
    """Return a board set to a random book opening"""
    if games is None:
        games = load_book("../bin/kasparov.pgn")
    opening_moves = get_random_opening(games, max_depth)
    board = chess.Board()
    for move in opening_moves:
        board.push(move)
    return board
