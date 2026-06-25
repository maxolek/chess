"""Opening/ECO classification from move lists."""
import json
from pathlib import Path
import chess


def get_opening_from_moves(moves):
    """
    Accepts either a list of move tokens or a space-separated moves string.
    Tokens may be UCI (e2e4) or SAN (Nf3). Returns a tuple (eco, name)
    or ("", "") when no opening is matched.
    """

    # Normalize input to list of tokens. Accept:
    # - a list of tokens
    # - a space-separated string of moves
    # - a JSON array string like '["e2e4", "e7e5"]'
    tokens = []
    if moves is None:
        return "", ""
    if isinstance(moves, str):
        s = moves.strip()
        # JSON array string?
        if s.startswith('[') and s.endswith(']'):
            try:
                parsed = json.loads(s)
                if isinstance(parsed, list):
                    tokens = parsed
                else:
                    tokens = s.split()
            except Exception:
                tokens = s.split()
        else:
            tokens = s.split()
    else:
        try:
            tokens = list(moves)
        except Exception:
            return "", ""

    board = chess.Board()
    uci_moves = []

    for tok in tokens:
        # try UCI first
        try:
            mv = chess.Move.from_uci(tok)
            if mv in board.legal_moves:
                board.push(mv)
                uci_moves.append(mv.uci())
                continue
        except Exception:
            pass

        # try SAN
        try:
            mv = board.parse_san(tok)
            board.push(mv)
            uci_moves.append(mv.uci())
            continue
        except Exception:
            # stop processing on first unparsable token
            break

    # Attempt to use python-chess openings database if available.
    try:
        if hasattr(chess, 'openings') and hasattr(chess.openings, 'ENCODED'):
            board = chess.Board()
            last_eco = None
            last_name = None
            for move_uci in uci_moves:
                try:
                    board.push_uci(move_uci)
                except ValueError:
                    break

                matched = False
                for opening in chess.openings.ENCODED:
                    board_full_fen = board.fen()
                    board_placement = board_full_fen.split(' ')[0]
                    for pos in opening.get('positions', []):
                        if pos == board_full_fen or pos == board_placement:
                            last_eco = opening.get('eco')
                            last_name = opening.get('name')
                            matched = True
                            break
                    if matched:
                        break

                if not matched:
                    break

            return last_eco or "", last_name or ""
    except Exception:
        pass

    # Load canonical ECO/openings database if available.
    try:
        eco_file = Path(__file__).resolve().parent.parent / "openings" / "eco_db.json"
        if eco_file.exists():
            with open(eco_file, 'r') as f:
                eco_list = json.load(f)

            # Build prefix placements for the current game moves
            prefix_placements = []
            try:
                b = chess.Board()
                for mv in uci_moves:
                    b.push_uci(mv)
                    prefix_placements.append(b.fen().split(' ')[0])
            except Exception:
                prefix_placements = []

            # Exact-match: prefer longest exact-matching move sequence
            best_match = (None, None, 0)  # (eco, name, matched_len)
            for entry in eco_list:
                moves_seq = entry.get("moves") or []
                if not moves_seq:
                    continue
                L = len(moves_seq)
                if len(uci_moves) >= L and tuple(uci_moves[:L]) == tuple(moves_seq):
                    if L > best_match[2]:
                        best_match = (entry.get("eco", ""), entry.get("name", ""), L)
                    continue

                # Try matching by intermediate board placements
                try:
                    eb = chess.Board()
                    entry_placements = []
                    for em in moves_seq:
                        eb.push_uci(em)
                        entry_placements.append(eb.fen().split(' ')[0])
                    if len(entry_placements) <= len(prefix_placements) and entry_placements == prefix_placements[:len(entry_placements)]:
                        if len(entry_placements) > best_match[2]:
                            best_match = (entry.get("eco", ""), entry.get("name", ""), len(entry_placements))
                except Exception:
                    pass

            if best_match[0] is not None:
                return best_match[0], best_match[1]

            # Longest-prefix fuzzy match
            for L in range(min(10, len(uci_moves)), 0, -1):
                prefix = tuple(uci_moves[:L])
                for entry in eco_list:
                    moves_seq = entry.get("moves") or []
                    if len(moves_seq) >= L and tuple(moves_seq[:L]) == prefix:
                        return entry.get("eco", ""), entry.get("name", "")
    except Exception:
        pass

    # Fallback: small built-in ECO prefix map
    ECO_PREFIXES = {
        ("e2e4", "e7e5", "g1f3", "b8c6", "f1b5"): ("C60", "Ruy Lopez"),
        ("e2e4", "c7c5"): ("B20", "Sicilian Defence"),
        ("e2e4", "e7e6"): ("C00", "French Defence"),
        ("e2e4", "c7c6"): ("B10", "Caro-Kann Defence"),
        ("d2d4", "d7d5", "c2c4"): ("D06", "Queen's Gambit"),
        ("d2d4", "g8f6", "c2c4", "g7g6"): ("E60", "King's Indian Defence"),
        ("c2c4", "e7e5"): ("A21", "English Opening: Symmetrical"),
        ("e2e4", "e7e5", "g1f3", "g8f6"): ("C40", "King's Knight Opening"),
    }

    if uci_moves:
        for L in range(min(6, len(uci_moves)), 0, -1):
            prefix = tuple(uci_moves[:L])
            for key, val in ECO_PREFIXES.items():
                if prefix[:len(key)] == key:
                    return val

        name = ' '.join(uci_moves[:6])
        return "", name
    return "", ""
