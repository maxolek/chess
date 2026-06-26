```mermaid
graph TD
    subgraph Sources["Sources"]
        ENG["Chess Engine<br/><i>C++ / UCI Protocol</i>"]
        LOGS["Engine Logs<br/><i>game.jsonl / search.jsonl / timing.jsonl</i>"]
    end

    subgraph ETL["ETL Package · data/etl/"]
        ING["ingest.py<br/><i>Parse & bulk insert</i>"]
        RAW["init_raw_db.py<br/><i>Initialize SQLite schema</i>"]
        PATHS["paths.py<br/><i>Single source of truth</i>"]
        DB["db.py<br/><i>SQLite helpers</i>"]
        OPEN["openings.py<br/><i>ECO classification</i>"]
    end

    subgraph Raw["Raw Layer"]
        SQLITE[("chess.db<br/><b>SQLite</b>")]
    end

    subgraph Builder["Analytics Builder · data/"]
        RUN["run_pipeline.py<br/><i>Pipeline orchestrator</i>"]

        LOAD["load_analytics.py<br/><i>Import raw tables</i>"]
        TP["transform_positions.py<br/><i>Position features</i>"]
        TS["transform_search.py<br/><i>Search features</i>"]
        NM["normalize_metadata.py<br/><i>Metadata normalization</i>"]

        direction LR
        LOAD
        TP
        TS
        NM
        
    end

    subgraph Analytics["Analytics Layer"]
        DUCK[("chess_analytics.duckdb<br/><b>DuckDB</b>")]
    end

    subgraph Dashboard["Dashboard · data/dashboard/"]
        DL["data_loader.py<br/><i>Cached SQL queries</i>"]
        CB["callbacks.py"]
        TABS["tabs.py"]
        APP["Dash App<br/><i>localhost:8050</i>"]
    end

    ENG -->|"UCI stdout"| LOGS
    LOGS -->|"bulk parse"| ING
    ING --> RAW
    RAW --> SQLITE

    SQLITE -->|"source data"| RUN

    RUN -.-> LOAD
    RUN -.-> TP
    RUN -.-> TS
    RUN -.-> NM

    LOAD --> DUCK
    TP --> DUCK
    TS --> DUCK
    NM --> DUCK

    DUCK -->|"SQL queries"| DL
    DL --> CB
    DL --> TABS
    CB --> APP
    TABS --> APP

    PATHS -.-> LOAD
    PATHS -.-> TP
    PATHS -.-> TS
    PATHS -.-> DL

    style ENG fill:#1a1f2e,stroke:#00d2ff,stroke-width:2px,color:#e8eaf0
    style LOGS fill:#1a1f2e,stroke:#00d2ff,stroke-width:2px,color:#e8eaf0
    style SQLITE fill:#2d1b00,stroke:#f7b731,stroke-width:3px,color:#fff
    style DUCK fill:#2d1b00,stroke:#f7b731,stroke-width:3px,color:#fff
    style APP fill:#1a0a2e,stroke:#ff6b35,stroke-width:3px,color:#fff
    style RUN fill:#0a2e1a,stroke:#7fff6b,stroke-width:3px,color:#fff

    linkStyle 0,1,2,3,4,9,10,11,12,13,14 stroke:#00d2ff,stroke-width:2.5px
    linkStyle 5,6,7,8,15,16,17,18 stroke:#8892a4,stroke-width:1.5px,stroke-dasharray:5
```