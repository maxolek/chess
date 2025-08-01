import chess.polyglot
import chess
import struct

book_in = "Titans.bin"
book_out = "book.bin"

seen = set()
board = chess.Board()

with chess.polyglot.open_reader(book_in) as reader, open(book_out, "wb") as out:
    for entry in reader:
        key = entry.key  # This is the Zobrist hash

        move = entry.move
        from_sq = chess.SQUARE_NAMES.index(move.uci()[:2])
        to_sq = chess.SQUARE_NAMES.index(move.uci()[2:4])
        promo = move.uci()[4] if len(move.uci()) == 5 else None

        # Don't duplicate positions
        if (key, move.uci()) in seen:
            continue
        seen.add((key, move.uci()))

        # Encode move: 6 bits from, 6 bits to, 4 bits promo
        move_bin = (from_sq & 0x3F) | ((to_sq & 0x3F) << 6)
        if promo:
            promo_map = {"n": 1, "b": 2, "r": 3, "q": 4}
            move_bin |= (promo_map[promo] << 12)

        # Write as: [8 bytes key][2 bytes move]
        out.write(struct.pack(">QH", key, move_bin))
