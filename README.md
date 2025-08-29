# Tomahawk Chess Engine

Tomahawk is a high-performance chess engine written in C++ with a Python-based testing and visualization framework. It combines modern search algorithms, efficient move generation, and a rich evaluation function to deliver competitive gameplay and easy experimentation.

Play at https://lichess.org/@/tomahawkBOT

## Features

### Engine Core
- **Iterative Deepening**: Depth-limited search with progressive deepening for accurate time management.
- **Negamax Search**: Efficient minimax implementation for zero-sum game evaluation.
- **Alpha-Beta Pruning**: Cuts off unnecessary branches to improve search speed.
- **Quiescence Search**: Extends search at tactical positions to avoid horizon effect.
- **Aspiration Windows**: Dyanmic alpha-beta values based on previous iteration, with dynamic scaling.
- **Move Ordering Heuristics**: Includes TT move, SEE, MVA-LVA, killer moves, history heuristics, and PV moves.
- **Tapered Evaluation**: Weights evaluation based on game phase.
- **Evaluation Function**:
  - King safety (pawn shields, tropism)
  - Pawn structure and passed pawns
  - Piece-square tables (optimized PST)
  - Static Exchange Evaluation (SEE) for tactical accuracy
  - Center control metrics
- **Magic Bitboards**: Fast sliding piece move generation.
- **Transposition Table (TT)**: Caches evaluated positions for faster search.
- **UCI Interface**: Full Universal Chess Interface support.

### Testing & Analysis
- Python-based **testing framework** for SPRT, Elo estimation, and statistical analysis.
- Data collection for move performance, NPS, evaluation breakdowns, and tournament-style testing.
- **Game Visualization** using Tkinter and PNG piece sets.

## Project Structure

/tomahawk       --> engine + make file
/tests          --> testing + visual games
  ./pgo_profile --> profile build components
/visual_assets  --> .svg/.png for visuals
/bin            --> opening book, PST tables

## Requirements

- **C++ Compiler**: Supports C++17 or later.
- **Python 3.10+**
- **Python Libraries**:
  - `python-chess`
  - `tkinter` (standard with Python)
- Optional: piece PNG assets for visualization.

## Usage

Compile is optimized for the following CPU with the compile flag -march=native

Name                                      NumberOfCores  NumberOfLogicalProcessors
Intel(R) Core(TM) i7-8665U CPU @ 1.90GHz  4              8

Full compile flags: -pthread -O3 -ffast-math -march=native -flto

### Compile Engine

bash

cd tomahawk

make -f makefile.mak

### Play Game

bash

cd tomahawk

tomahawk

position startpos

go movetime [time in ms]

### Run Game Tests
bash

cd tests

py sprt.py
