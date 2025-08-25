#pragma once
#include "helpers.h"
#include "move.h"

// -------------------------------
// Bound types for TT entries
// -------------------------------
enum BoundType : uint8_t {
    EXACT,
    LOWERBOUND,
    UPPERBOUND
};

// -------------------------------
// Transposition Table Entry
// -------------------------------
struct TTEntry {
    U64 key = 0;           // Zobrist key
    int16_t eval = 0;      // Stored evaluation (centipawns)
    int16_t depth = 0;     // Search depth
    int16_t horizon = 0;   // ply + depth
    uint16_t age = 0;      // Age counter
    BoundType flag = EXACT; 
    uint16_t bestMove = 0; // Encoded move (use 16-bit int instead of Move object)
};

// -------------------------------
// Transposition Table
// -------------------------------
class TranspositionTable {
public:
    TranspositionTable(size_t mbSize = 512) { resize(mbSize); }

    // Resize table to given MB size
    void resize(size_t mbSize) {
        size_t bytes = mbSize * 1024 * 1024;
        entriesCount = 1ULL << static_cast<size_t>(std::log2(bytes / sizeof(TTEntry)));
        table.assign(entriesCount, TTEntry{});
    }

    // Probe TT for a given key
    inline TTEntry* probe(U64 key) {
        return &table[key & (entriesCount - 1)]; // power-of-2 indexing
    }

    // Store an entry
    inline void store(U64 key, int depth, int ply, int score,
                      BoundType flag, Move bestMove) {
        TTEntry* entry = probe(key);
        int horizon = ply + depth;

        // Replace if new key or deeper horizon
        if (entry->key != key || horizon >= entry->horizon) {
            entry->key = key;
            entry->eval = static_cast<int16_t>(score);
            entry->depth = static_cast<int16_t>(depth);
            entry->horizon = static_cast<int16_t>(horizon);
            entry->flag = flag;
            entry->bestMove = bestMove.Value();
            entry->age++;
        }
    }

    // Clear all entries
    void clear() {
        std::fill(table.begin(), table.end(), TTEntry{});
    }

private:
    std::vector<TTEntry> table;
    size_t entriesCount = 0;
};
