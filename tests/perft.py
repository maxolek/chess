import subprocess
import time
import re
import chess
#from stockfish import Stockfish

STOCKFISH_PATH = r"C:\Users\maxol\stockfish-windows-x86-64-avx2\stockfish\stockfish-windows-x86-64-avx2"
ENGINE_PATH = r"C:\Users\maxol\code\chess\tomahawk\testing.exe"
LOG_FILE_PATH = r"C:\Users\maxol\code\chess\perft_log.txt"

def log(line: str):
    with open(LOG_FILE_PATH, "a", encoding="utf-8") as f:
        f.write(line + "\n")


def run_stockfish_perft(fen, depth, divide=False):
    commands = f"uci\nisready\nposition fen {fen}\ngo perft {depth}\nquit\n"
    try:
        sf = subprocess.Popen(
            STOCKFISH_PATH,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            universal_newlines=True
        )
        stdout, _ = sf.communicate(commands, timeout=30)
        output = stdout.strip().splitlines()

        if divide:
            divide_dict = {}
            for line in output:
                if ':' in line:
                    try:
                        move, nodes = line.split(':')
                        divide_dict[move.strip()] = int(nodes.strip())
                    except ValueError:
                        continue
            return divide_dict, output

        # Non-divide mode: parse total nodes
        total_nodes = 0
        for line in output:
            if ':' in line:
                try:
                    nodes = int(line.split(':')[-1].strip())
                    total_nodes += nodes
                except ValueError:
                    continue
            elif line.strip().isdigit():
                total_nodes += int(line.strip())
        return total_nodes, output

    except subprocess.TimeoutExpired:
        sf.kill()
        print("[Stockfish error] Timed out.")
        return ({} if divide else 0), []


def run_cpp_perft(fen, depth, divide=False):
    mode = "divide" if divide else "perft"
    cmd = [ENGINE_PATH, mode] + fen.split(" ") + [str(depth)]
    #print("cmd\t",cmd)
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
        lines = result.stdout.strip().splitlines()

        if not divide:
            return int(lines[-1]), lines
        divide_dict = {}
        for line in lines:
            if ':' in line:
                move, nodes = line.split(':')
                divide_dict[move.strip()] = int(nodes.strip())
        return divide_dict, lines
    except Exception as e:
        print(f"[Engine error] {e}")
        return 0 if not divide else {}, []

def compare_perft(fen, depth) -> None:
    print(f"FEN: {fen}\n")
    print(chess.Board(fen))
    print(f"\nDepth: {depth}")
    start = time.time()
    my_nodes, my_output = run_cpp_perft(fen, depth)
    mid = time.time()
    sf_nodes, _ = run_stockfish_perft(fen, depth)
    end = time.time()

    print(f"\nYour engine nodes: {my_nodes} (in {mid - start:.2f}s)")
    print(f"Stockfish nodes  : {sf_nodes} (in {end - mid:.2f}s)")

    if my_nodes != sf_nodes:
        print("❌ Mismatch detected!")
    else:
        print("✅ Correct result.")

def compare_divide(fen, depth) -> None:
    print(f"FEN: {fen}\n")
    print(chess.Board(fen))
    print(f"\nDepth: {depth}")
    print(f"Running divide...\n")

    log(f"\nFEN: {fen}")
    log(f"Depth: {depth}")

    start = time.time()
    my_divide, my_lines = run_cpp_perft(fen, depth, divide=True)
    mid = time.time()
    sf_divide, sf_lines = run_stockfish_perft(fen, depth, divide=True)
    end = time.time()

    all_moves = sorted(set(my_divide.keys()) | set(sf_divide.keys()))
    correct = True

    for move in all_moves:
        my_val = my_divide.get(move, None)
        sf_val = sf_divide.get(move, None)
        if my_val != sf_val:
            correct = False
            line = f"❌ {move:5}  Your engine: {my_val}  Stockfish: {sf_val}"
        else:
            line = f"✅ {move:5}  {my_val}"
        print(line)
        log(line)

    if correct:
        print("\n✅ All perft divide results match.")
        log("✅ All perft divide results match.\n")
    else:
        print("\n❌ Discrepancy found in perft divide.")
        log("❌ Discrepancy found in perft divide.\n")

    log(f"Times ||  Your engine: {mid-start:.2f}s  Stockfish: {end-mid:.2f}s")


    
    
def compare_subdivide(child_fen):
    print(f"FEN: {child_fen}")
    print(chess.Board(child_fen))

    my_divide_sub, _ = run_cpp_perft(child_fen, 1, divide=True)
    sf_divide_sub, _ = run_stockfish_perft(child_fen, 1, divide=True)

    all_submoves = sorted(set(my_divide_sub.keys()) | set(sf_divide_sub.keys()))
    discrepancies = False

    for move in all_submoves:
        my_count = my_divide_sub.get(move)
        sf_count = sf_divide_sub.get(move)
        if my_count != sf_count:
            discrepancies = True
            print(f"❌ {move:5}  Your engine: {my_count}  Stockfish: {sf_count}")
        else:
            print(f"✅ {move:5}  {my_count}")

    if not discrepancies:
        print("✅ Subdivide matches for this branch.\n")
    else:
        print("❌ Discrepancies found in this sub-branch.\n")



def show_second_level_discrepancy(fen: str, move: str):
    print(f"\n🔍 Investigating discrepancy after move: {move}")
    board = chess.Board(fen)
    try:
        board.push_uci(move)
    except Exception as e:
        print(f"⚠️ Could not push move {move}: {e}")
        return

    child_fen = board.fen()

    my_divide, _ = run_cpp_perft(child_fen, 1, divide=True)
    sf_divide, _ = run_stockfish_perft(child_fen, 1, divide=True)

    my_moves = set(my_divide.keys())
    sf_moves = set(sf_divide.keys())

    missing = sf_moves - my_moves
    extra = my_moves - sf_moves

    if missing:
        print("\n❌ Missing moves (in Stockfish, not in your engine):")
        for m in sorted(missing):
            print(f"   {m}")

    if extra:
        print("\n❌ Extra moves (in your engine, not in Stockfish):")
        for m in sorted(extra):
            print(f"   {m}")

    if not missing and not extra:
        print(f"✅ All child moves match at depth 1. mm: {(len(my_moves))}\tsf: {(len(sf_moves))}")


# not currently used
def get_missing_moves(fen, depth=1):
    board = chess.Board(fen)

    my_divide, _ = run_cpp_perft(fen, depth, divide=True)
    sf_divide, _ = run_stockfish_perft(fen, depth, divide=True)

    my_moves = set(my_divide.keys())
    sf_moves = set(sf_divide.keys())

    missing_from_your_engine = sf_moves - my_moves
    extra_in_your_engine = my_moves - sf_moves

    if missing_from_your_engine:
        print(f"\n❌ Missing moves at depth {depth}, path: {' '.join(path) or '(root)'}")
        for move in sorted(missing_from_your_engine):
            print(f"   Missing: {move}")

    if extra_in_your_engine:
        print(f"\n❌ Extra moves at depth {depth}, path: {' '.join(path) or '(root)'}")
        for move in sorted(extra_in_your_engine):
            print(f"   Extra:   {move}")


def run_test_suite(depth = 2) -> None:
    tests = [
        ("Initial",    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", depth),
        ("Kiwipete",   "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", depth),
        ("En-passant Hell",  "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", depth),
        ("Complex Middle",   "r4rk1/1pp1qppp/p1np1n2/8/2P5/2N1PN2/PP2QPPP/2KR1B1R w - - 0 1", depth),
        ("Promotions & Castling", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", depth)
    ]

    for name, fen, depth in tests:
        print(f"\n--- {name} ---")
        compare_divide(fen, depth)




def main():
    print("Choose test type: ")
    print("1. perft")
    print("2. perft divide")
    print("3. full test suite")
    choice = input("Enter choice (1-3): ").strip()

    if choice == "1" or choice == "perft":
        test_type = "perft"
        fen = input("Enter FEN string: ").strip()
        depth = input("Enter depth: ").strip()
    elif choice == "2" or choice == 'perft divide':
        test_type = "divide"
        fen = input("Enter FEN string: ").strip()
        depth = input("Enter depth: ").strip()
    elif choice == "3" or choice == 'full test suite':
        test_type = "full"
        depth = input("Enter depth: ").strip()
    else:
        print("Invalid choice.")
        return

    try: # doesnt check for valid fen
        depth = int(depth)
    except ValueError:
        print("Invalid depth. Must be an integer.")
        return

    if test_type == "perft":
        compare_perft(fen, depth)
    elif test_type == 'divide':
        compare_divide(fen, depth)
    else:
        run_test_suite(depth)


if __name__ == "__main__":
    main()

