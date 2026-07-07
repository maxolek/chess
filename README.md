<div align="center">

<img src="bin/logos/logo.png" width="200">

# Tomahawk 

### A NNUE chess engine written in C++

[![UCI](https://img.shields.io/badge/protocol-UCI-green.svg)]()

[![C++](https://img.shields.io/badge/C++-20-blue.svg)]()
[![Python](https://img.shields.io/badge/Python-3-blue.svg)]()

[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey.svg)]()

<br>
Play against Tomahawk!

https://lichess.org/@/tomahawkBOT

</div>

---

## Overview

Tomahawk is a UCI chess engine written in C++ focused on search optimization, neural network evaluation, and data-driven improvement.

##### /src/ + /include/

The engine combines traditional alpha-beta search techniques with NNUE evaluation and an extensive testing framework for measuring strength improvements through automated analysis and SPRT testing.

##### /data/

A large emphasis has been placed on a (python) data collection pipeline with integration into a Node.js dashboard. This data has been used to drive improvements, make programming decisions, and perform automated tuning.

##### /tests/

An even more important aspect of engine development lies in testing. This python based testing schema includes move generation testing, speed testing, and game/position testing. The engine includes further testing possibilities via the UCI commands (e.g. see, dumpzobrist)

##### /bin/

The engines essential information, e.g. neural network weights, test positions, etc.

---

## Data Framework

OLTP-OLAP stored in SQLite & DuckDB. Raw logging is stored in .jsonl files that are connected via UUIDs based on .exe process IDs. Raw logs are stored in SQLite database that feeds into DuckDB database with rich position and search information for deep analysis and engine comparison.

Typical search, game, and function call time information is collected for basic engine performance analysis, but in the ETL during OLTP -> OLAP pipelines additional metrics on search information is calculated, games are better summarized, broad position information is generated, and stockfish eval/move comparisons are computed.


<br> 

##### See /docs/ for more information
