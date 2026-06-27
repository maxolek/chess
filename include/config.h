#pragma once
#include <filesystem>
#include <string>
#include <logging.h>

namespace fs = std::filesystem;

// ── constants (never runtime-configurable) ────────────────────────────────────
inline constexpr int  INF                   = 999'999;
inline constexpr int  MATE_SCORE            = 100'000;
inline constexpr int  KILL_SEARCH_RETURN    = -5 * MATE_SCORE;
inline constexpr int  MAX_MOVES             = 256;
inline constexpr int  MAX_DEPTH             = 32;

// ── sub-structs ────────────────────────────────────────────────────────────────

struct SearchParams {
    // delta / SEE
    int   DELTA_PRUNE_THRESHOLD  = 1'000;
    int   SEE_PRUNE_THRESHOLD    = -50;
    // aspiration windows
    int   ASPIRATION_WINDOW      = 50;
    int   ASPIRATION_START_DEPTH = 6;
    int   ASPIRATION_DEPTH_SCALE = 10;
    float ASPIRATION_RESEARCH_SCALE = 2.0f;
    // positional
    int   DRAW_EVAL              = 0;
    int   CONTEMPT               = 0;
    // reductions
    int   R_NMP                  = 3;      // null-move pruning
    float R_LMR_CONST            = 0.99f;  // late move reductions 
    float R_LMR_DENOM            = 3.14f;  //   = const + [log(depth) * log(move_order)] / denom
    int   LMR_MOVE_ORDER_THRESHOLD = 3; // minimum move order # to start using LMR
    int   LMR_DEPTH_THRESHOLD    = 2; // max search depth where LMR doesnt trigger
};

struct SearchOptions {
    bool _QUIESCENCE           = true;
    bool _ASPIRATION           = true;
    bool _DELTA_PRUNING        = true;
    bool _SEE_PRUNING          = true;
    bool _MOVE_ORDERING        = true;
    bool _MVVLVA_ORDERING       = false;
    bool _SEE_ORDERING          = true;
    // eval
    bool UCI_SHOW_WDL       = false;
    bool _NNUE              = true;
};

struct MoveScores {
    int TT_BASE       =  10'000'000;
    int PV_BASE       =   9'000'000;
    int PROMO_BASE    =   8'500'000;
    int GOOD_CAP_BASE =   8'000'000;
    int KILLER_BASE   =   7'000'000;
    int QUIET_BASE    =           0;
    int BAD_CAP_BASE  =  -1'000'000;
};

struct EngineOptions {
    bool MAGICS           = true;
    int  MOVE_OVERHEAD_MS = 30;
    int  MAX_THREADS      = 1;
    int  HASH_SIZE_MB     = 512;
    bool PONDERING        = false;

    fs::path opening_pst_path  = Logging::project_root / "bin/pst/pst_opening.txt";
    fs::path endgame_pst_path  = Logging::project_root / "bin/pst/pst_endgame.txt";
    fs::path nnue_weight_path  = Logging::project_root / "bin/nnue_wgts/768_128x2.bin";
    fs::path opening_book_path = Logging::project_root / "bin/Titans.bin";
    fs::path syzygy_path       = Logging::project_root;
};

// ── root config ───────────────────────────────────────────────────────────────
struct EngineConfig {
    SearchParams  search;
    SearchOptions options;
    MoveScores    scores;
    EngineOptions engine;

    // factory: returns a default config (same as zero-initialising)
    static EngineConfig defaults() { return {}; }
};

// ── loaading functions ───────────────────────────────────────────────────────────────

inline std::unordered_map<std::string, std::string> parse_kv(const fs::path& path) {
    std::unordered_map<std::string, std::string> kv;
    std::ifstream f(path);
    if (!f) return kv;  // missing file = use defaults, not an error
    std::string line;
    while (std::getline(f, line)) {
        // strip comments and whitespace
        if (auto pos = line.find('#'); pos != std::string::npos) line.erase(pos);
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        // trim
        auto trim = [](std::string& s) {
            s.erase(0, s.find_first_not_of(" \t\r\n"));
            s.erase(s.find_last_not_of(" \t\r\n") + 1);
        };
        trim(k); trim(v);
        if (!k.empty()) kv[k] = v;
    }
    return kv;
}

inline void apply_config_file(EngineConfig& cfg, const fs::path& path) {
    auto kv = parse_kv(path);
    auto get = [&](const std::string& k) -> const std::string* {
        auto it = kv.find(k); return it != kv.end() ? &it->second : nullptr;
    };
    auto b = [](const std::string& v) { return v == "true" || v == "1"; };

    // SearchParams
    if (auto* v = get("delta_prune_threshold"))    cfg.search.DELTA_PRUNE_THRESHOLD               = std::stoi(*v);
    if (auto* v = get("see_prune_threshold"))      cfg.search.SEE_PRUNE_THRESHOLD      = std::stoi(*v);
    if (auto* v = get("aspiration_window"))        cfg.search.ASPIRATION_WINDOW        = std::stoi(*v);
    if (auto* v = get("aspiration_start_depth"))   cfg.search.ASPIRATION_START_DEPTH   = std::stoi(*v);
    if (auto* v = get("aspiration_depth_scale"))   cfg.search.ASPIRATION_DEPTH_SCALE  = std::stoi(*v);
    if (auto* v = get("aspiration_research_scale"))cfg.search.ASPIRATION_RESEARCH_SCALE = std::stof(*v);
    if (auto* v = get("draw_eval"))                cfg.search.DRAW_EVAL                = std::stoi(*v);
    if (auto* v = get("contempt"))                 cfg.search.CONTEMPT                 = std::stoi(*v);
    if (auto* v = get("r_nmp"))                    cfg.search.R_NMP                    = std::stoi(*v);
    if (auto* v = get("r_lmr_const"))              cfg.search.R_LMR_CONST              = std::stof(*v);
    if (auto* v = get("r_lmr_denom"))              cfg.search.R_LMR_DENOM              = std::stof(*v);
    if (auto* v = get("lmr_move_order_threshold")) cfg.search.LMR_MOVE_ORDER_THRESHOLD              = std::stof(*v);
    if (auto* v = get("lmr_depth_threshold"))      cfg.search.LMR_DEPTH_THRESHOLD              = std::stof(*v);

    // SearchOptions
    if (auto* v = get("quiescence"))      cfg.options._QUIESCENCE     = b(*v);
    if (auto* v = get("aspiration"))      cfg.options._ASPIRATION      = b(*v);
    if (auto* v = get("delta_pruning"))   cfg.options._DELTA_PRUNING  = b(*v);
    if (auto* v = get("see_pruning"))     cfg.options._SEE_PRUNING    = b(*v);
    if (auto* v = get("move_ordering"))   cfg.options._MOVE_ORDERING   = b(*v);
    if (auto* v = get("mvvlva_ordering")) cfg.options._MVVLVA_ORDERING = b(*v);
    if (auto* v = get("see_ordering"))    cfg.options._SEE_ORDERING    = b(*v);
    if (auto* v = get("uci_show_wdl"))   cfg.options.UCI_SHOW_WDL    = b(*v);
    if (auto* v = get("nnue"))            cfg.options._NNUE             = b(*v);

    // MoveScores
    if (auto* v = get("tt_base"))        cfg.scores.TT_BASE       = std::stoi(*v);
    if (auto* v = get("pv_base"))        cfg.scores.PV_BASE        = std::stoi(*v);
    if (auto* v = get("promo_base"))     cfg.scores.PROMO_BASE     = std::stoi(*v);
    if (auto* v = get("good_cap_base"))  cfg.scores.GOOD_CAP_BASE = std::stoi(*v);
    if (auto* v = get("killer_base"))    cfg.scores.KILLER_BASE   = std::stoi(*v);
    if (auto* v = get("quiet_base"))     cfg.scores.QUIET_BASE     = std::stoi(*v);
    if (auto* v = get("bad_cap_base"))   cfg.scores.BAD_CAP_BASE   = std::stoi(*v);

    // EngineOptions
    if (auto* v = get("move_overhead_ms")) cfg.engine.MOVE_OVERHEAD_MS= std::stoi(*v);
    if (auto* v = get("hash_size_mb"))     cfg.engine.HASH_SIZE_MB     = std::stoi(*v);
    if (auto* v = get("pondering"))        cfg.engine.PONDERING         = b(*v);
    if (auto* v = get("nnue_weight_path"))  cfg.engine.nnue_weight_path  = Logging::project_root / *v;
    if (auto* v = get("opening_book_path")) cfg.engine.opening_book_path = Logging::project_root / *v;
    if (auto* v = get("syzygy_path"))       cfg.engine.syzygy_path       = Logging::project_root / *v;  
}

inline void create_config_file(const EngineConfig& cfg, std::string config_name) {
    // ensure .ini extension
    if (config_name.size() < 4 || config_name.substr(config_name.size() - 4) != ".ini")
        config_name += ".ini";

    fs::path dir = fs::path(PROJECT_ROOT) / "bin/configs";
    fs::create_directories(dir);  // ensure directory exists
    fs::path out_path = dir / config_name;

    std::ofstream f(out_path);
    if (!f) {
        std::cerr << "Error: could not open " << out_path << " for writing.\n";
        return;
    }

    auto b = [](bool v) -> std::string { return v ? "true" : "false"; };

    // helper: write a relative path (strip PROJECT_ROOT prefix if present)
    auto rel = [](const fs::path& p) -> std::string {
        const fs::path root = fs::path(PROJECT_ROOT);
        std::error_code ec;
        fs::path r = fs::relative(p, root, ec);
        return (!ec && !r.empty()) ? r.generic_string() : p.generic_string();
    };

    f << "# =========================================================\n"
      << "# " << fs::path(config_name).stem().string() << " Engine Configuration\n"
      << "# =========================================================\n\n";

    // ── [search] ─────────────────────────────────────────────────────────────
    f << "[search]\n"
      << "delta_prune_threshold      = " << cfg.search.DELTA_PRUNE_THRESHOLD      << "\n"
      << "see_prune_threshold        = " << cfg.search.SEE_PRUNE_THRESHOLD        << "\n\n"
      << "aspiration_window          = " << cfg.search.ASPIRATION_WINDOW          << "\n"
      << "aspiration_start_depth     = " << cfg.search.ASPIRATION_START_DEPTH     << "\n"
      << "aspiration_depth_scale     = " << cfg.search.ASPIRATION_DEPTH_SCALE     << "\n"
      << "aspiration_research_scale  = " << cfg.search.ASPIRATION_RESEARCH_SCALE  << "\n\n"
      << "draw_eval                  = " << cfg.search.DRAW_EVAL                  << "\n"
      << "contempt                   = " << cfg.search.CONTEMPT                   << "\n\n"
      << "r_nmp                      = " << cfg.search.R_NMP                      << "\n"
      << "r_lmr_const                = " << cfg.search.R_LMR_CONST                << "\n"
      << "r_lmr_denom                = " << cfg.search.R_LMR_DENOM                << "\n\n"
      << "lmr_move_order_threshold   = " << cfg.search.LMR_MOVE_ORDER_THRESHOLD   << "\n"
      << "lmr_depth_threshold        = " << cfg.search.LMR_DEPTH_THRESHOLD        << "\n\n\n";

    // ── [options] ─────────────────────────────────────────────────────────────
    f << "[options]\n"
      << "quiescence        = " << b(cfg.options._QUIESCENCE)      << "\n"
      << "aspiration        = " << b(cfg.options._ASPIRATION)      << "\n"
      << "delta_pruning     = " << b(cfg.options._DELTA_PRUNING)   << "\n"
      << "see_pruning       = " << b(cfg.options._SEE_PRUNING)     << "\n"
      << "move_ordering     = " << b(cfg.options._MOVE_ORDERING)   << "\n"
      << "mvvlva_ordering   = " << b(cfg.options._MVVLVA_ORDERING) << "\n"
      << "see_ordering      = " << b(cfg.options._SEE_ORDERING)    << "\n\n"
      << "uci_show_wdl      = " << b(cfg.options.UCI_SHOW_WDL)    << "\n"
      << "nnue              = " << b(cfg.options._NNUE)             << "\n\n\n";

    // ── [scores] ──────────────────────────────────────────────────────────────
    f << "[scores]\n"
      << "tt_base        = " << cfg.scores.TT_BASE       << "\n"
      << "pv_base        = " << cfg.scores.PV_BASE       << "\n"
      << "promo_base     = " << cfg.scores.PROMO_BASE    << "\n"
      << "good_cap_base  = " << cfg.scores.GOOD_CAP_BASE << "\n"
      << "killer_base    = " << cfg.scores.KILLER_BASE   << "\n"
      << "quiet_base     = " << cfg.scores.QUIET_BASE    << "\n"
      << "bad_cap_base   = " << cfg.scores.BAD_CAP_BASE  << "\n\n\n";

    // ── [engine] ──────────────────────────────────────────────────────────────
    f << "[engine]\n"
      << "magics            = " << b(cfg.engine.MAGICS)          << "\n"
      << "move_overhead_ms  = " << cfg.engine.MOVE_OVERHEAD_MS   << "\n"
      << "max_threads       = " << cfg.engine.MAX_THREADS        << "\n"
      << "hash_size_mb      = " << cfg.engine.HASH_SIZE_MB       << "\n"
      << "pondering         = " << b(cfg.engine.PONDERING)       << "\n\n"
      << "nnue_weight_path  = " << fs::relative(cfg.engine.nnue_weight_path,  Logging::project_root).generic_string() << "\n"
      << "opening_book_path = " << fs::relative(cfg.engine.opening_book_path, Logging::project_root).generic_string() << "\n"
      << "syzygy_path       = " << fs::relative(cfg.engine.syzygy_path,       Logging::project_root).generic_string() << "\n";

    std::cout << "Config written to " << out_path << "\n";
}