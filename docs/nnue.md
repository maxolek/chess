---

## NNUE Architecture · `768-128×2-1`

```mermaid
graph TD
    subgraph Input["🎯 Input Features · Chess768"]
        BOARD["♟️ Board State<br/><i>64 squares × 12 piece types</i>"]
        FEAT["📐 768 Binary Features<br/><i>One-hot per piece/square</i>"]
    end

    subgraph Accumulator["⚡ Dual-Perspective Accumulators"]
        STM["🔵 STM Accumulator<br/><i>int32[128] · Side-To-Move view</i>"]
        NTM["🔴 NTM Accumulator<br/><i>int32[128] · Non-To-Move view</i>"]
        INC["🔄 Incremental Updates<br/><i>O(128) per make/unmake</i>"]
    end

    subgraph Hidden["🧠 Hidden Layer · 128×2 = 256 Neurons"]
        SCRELU_S["⚡ SCReLU (STM)<br/><i>clamp(0, 255) → square</i>"]
        SCRELU_N["⚡ SCReLU (NTM)<br/><i>clamp(0, 255) → square</i>"]
        CONCAT["🔗 Concatenate<br/><i>[128 STM ∥ 128 NTM]</i>"]
    end

    subgraph Output["📊 Output Layer"]
        DOT["✖️ Dot Product<br/><i>256 weights × int16</i>"]
        BIAS["➕ Bias<br/><i>+ l1b</i>"]
        SCALE["📏 Scale & Dequantize<br/><i>× 400 / (255 × 64)</i>"]
        EVAL["🎯 Centipawn Eval<br/><i>int output</i>"]
    end

    subgraph Integration["🔍 Search Integration"]
        QSEARCH["⏱️ Quiescence Search<br/><i>Stand-pat eval</i>"]
        NEGAMAX["🌲 Negamax<br/><i>Leaf evaluation</i>"]
        MAKE["♟️ Make/Unmake Move<br/><i>Keeps accumulators in sync</i>"]
    end

    subgraph Weights["💾 Weight File · 197 KB"]
        FILE["📦 768_128x2.bin<br/><i>All int16 quantized</i>"]
        L0W["L0 Weights<br/><i>768×128 = 196,608 B</i>"]
        L0B["L0 Bias<br/><i>128 × 2 B</i>"]
        L1W["L1 Weights<br/><i>256 × 2 B</i>"]
        L1B["L1 Bias<br/><i>2 B</i>"]
    end

    BOARD ==>|"encode"| FEAT
    FEAT ==>|"column add/sub"| STM
    FEAT ==>|"column add/sub<br/>(flipped sq^56)"| NTM
    INC -.->|"± weight column"| STM
    INC -.->|"± weight column"| NTM
    STM ==>|"int32[128]"| SCRELU_S
    NTM ==>|"int32[128]"| SCRELU_N
    SCRELU_S ==> CONCAT
    SCRELU_N ==> CONCAT
    CONCAT ==>|"int32[256]"| DOT
    DOT ==> BIAS
    BIAS ==> SCALE
    SCALE ==> EVAL
    EVAL ==> QSEARCH
    EVAL ==> NEGAMAX
    MAKE -.->|"on_make_move()"| STM
    MAKE -.->|"on_unmake_move()"| NTM
    FILE -.-> L0W
    FILE -.-> L0B
    FILE -.-> L1W
    FILE -.-> L1B
    L0W -.->|"loaded at init"| STM
    L0W -.->|"loaded at init"| NTM
    L1W -.->|"loaded at init"| DOT

    style BOARD fill:#1a1f2e,stroke:#00d2ff,stroke-width:2px,color:#e8eaf0
    style FEAT fill:#1a1f2e,stroke:#00d2ff,stroke-width:2px,color:#e8eaf0
    style STM fill:#0a1e3d,stroke:#3b82f6,stroke-width:3px,color:#fff
    style NTM fill:#2d0a0a,stroke:#ef4444,stroke-width:3px,color:#fff
    style CONCAT fill:#1a1f2e,stroke:#a78bfa,stroke-width:2px,color:#e8eaf0
    style EVAL fill:#0a2e1a,stroke:#7fff6b,stroke-width:3px,color:#fff
    style FILE fill:#2d1b00,stroke:#f7b731,stroke-width:2px,color:#fff
    style SCRELU_S fill:#0a1e3d,stroke:#3b82f6,stroke-width:2px,color:#e8eaf0
    style SCRELU_N fill:#2d0a0a,stroke:#ef4444,stroke-width:2px,color:#e8eaf0

    linkStyle 0,1,2,5,6,7,8,9,10,11,12,13,14 stroke:#00d2ff,stroke-width:2.5px
    linkStyle 3,4,15,16,17,18,19,20,21,22 stroke:#8892a4,stroke-width:1.5px,stroke-dasharray:5
```

### Key Constants

| Parameter | Value | Purpose |
|-----------|-------|---------|
| `QA` | 255 | SCReLU clamp range |
| `QB` | 64 | Output dequantization divisor |
| `SCALE` | 400 | Centipawn scale factor |
| `HIDDEN_SIZE` | 128 | Neurons per perspective |
| **Total params** | 98,689 | (768×128 + 128 + 256 + 1) |
| **File size** | 197 KB | All int16 quantized |
