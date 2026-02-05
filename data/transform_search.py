"""
function(s) that perform ETL on raw database 
to turn raw data into analytics ready data
"""

ASPIRATION_START_DEPTH = 6

def build_search_iterations_features(cnxn):
    rolling_window = 5
    window_spec = "(PARTITION BY search_id ORDER BY depth ASC)"
    rolling_window_spec = f"(PARTITION BY search_id ORDER BY depth ASC ROWS BETWEEN {rolling_window} PRECEDING AND CURRENT ROW)"

    cnxn.execute(f"""
        CREATE OR REPLACE TABLE search_iteration_features AS
                 
        WITH base_metrics AS (
            SELECT 
                *,
                nodes + qnodes AS total_nodes,
                -- We calculate the group identifier here
                ROW_NUMBER() OVER (PARTITION BY search_id ORDER BY depth) - 
                ROW_NUMBER() OVER (PARTITION BY search_id, move ORDER BY depth) as move_grp
            FROM iterative_deepening_stats
        )
        SELECT
            -- base metrics / keys
            search_id, depth, qdepth,
            time_ms, SUM(time_ms) OVER {window_spec} as running_time_ms,
            eval, move, 
            nodes, qnodes, 
            tt_stores, tt_hits,
            fail_highs, fail_lows, fail_high_first, fail_high_late,
            fail_high_researches, fail_low_researches,
            see_prunes, delta_prunes,

            -- within iteration derived metrics
            total_nodes,
            total_nodes / (NULLIF(time_ms, 0) / 1000) AS nps,
            qnodes / NULLIF(nodes, 0) AS qratio,
            tt_hits / NULLIF(total_nodes, 0) AS tt_hit_ratio,
            tt_stores / NULLIF(total_nodes, 0) AS tt_store_ratio,
            fail_highs / NULLIF(total_nodes, 0) AS fail_high_ratio,
            fail_lows / NULLIF(total_nodes, 0) AS fail_low_ratio,
            fail_high_first / NULLIF(fail_highs, 0) AS fail_high_first_ratio,
            see_prunes / NULLIF(qnodes, 0) AS see_prune_ratio,
            delta_prunes / NULLIF(qnodes, 0) AS delta_prune_ratio,
            (see_prunes + delta_prunes) / NULLIF(qnodes, 0) AS prune_ratio,         

            -- across iterations derived metrics
            time_ms / LAG(time_ms) OVER {window_spec} as time_increase_ratio,
            total_nodes / LAG(total_nodes) OVER {window_spec} as ebf,
            qnodes / LAG(qnodes) OVER {window_spec} as qebf,

            -- stability metrics
            eval - LAG(eval) OVER {window_spec} as prior_eval_delta,
            eval - FIRST_VALUE(eval) OVER {window_spec} as first_eval_delta,
            CASE
                WHEN (eval > 0 AND LAG(eval) OVER {window_spec} < 0) OR 
                     (eval < 0 AND LAG(eval) OVER {window_spec} > 0) THEN 1
                ELSE 0
            END as eval_sign_flips,
            STDDEV(eval) OVER {window_spec} AS stddev_eval,
            STDDEV(eval) OVER {rolling_window_spec} AS stddev_last5_eval,
            
            -- Final move stability calculation using the pre-calculated group
            ROW_NUMBER() OVER (PARTITION BY search_id, move_grp ORDER BY depth) - 1 AS move_stability

        FROM base_metrics
    """)

def build_search_tree_features(cnxn):
    window_spec = "(PARTITION BY search_id ORDER BY depth asc)"
    
    cnxn.execute(f"""
        CREATE OR REPLACE TABLE search_tree_features AS
        SELECT
                 
        -- base metrics / keys
        search_id, depth, 
        -- qnodes are called from depth (i.e. the 'true ply' since qsearch makes moves, is not what is represented here. these are the qnodes from when depth==0 was achieved at <-- depth)
        -- nodes are multi-counted across iteration deepening depths (e.g. i_d=3 and 4 both will search at tree depth 2)
        tt_stores, tt_hits,
        nodes, qnodes, 
        fail_highs, fail_lows, 
        fail_high_first, fail_high_late,
        see_prunes, delta_prunes,
                 
        -- within iteration derived metrics
        nodes + qnodes AS total_nodes,
        qnodes / nodes as qratio, -- also a slightly different definition than iterations 
        tt_hits / total_nodes AS tt_hit_ratio,
        tt_stores / total_nodes AS tt_store_ratio,
        fail_highs / total_nodes AS fail_high_ratio,
        fail_lows / total_nodes AS fail_low_ratio,
        fail_high_first / fail_highs AS fail_high_first_ratio,
        see_prunes / qnodes AS see_prune_ratio,     -- denom is qnodes as pruning only happens in qnodes
        delta_prunes / qnodes AS delta_prune_ratio, -- qnodes are tracked before pruning is applied
        (see_prunes + delta_prunes) / qnodes AS prune_ratio,  
                 
        -- across depth derived metrics
        total_nodes / LAG(total_nodes) OVER {window_spec} as ebf,
        qnodes / LAG(qnodes) OVER {window_spec} as qebf,
                 
                 
        FROM search_tree_stats
    """)
    
def build_search_features(cnxn):
    ASPIRATION_START_DEPTH = 6
    cnxn.execute(f"""
        CREATE OR REPLACE TABLE search_features AS
                 
        WITH times AS (
            SELECT
                search_id, 
                -- We define the total search time once here
                MAX(CASE WHEN function = 'ROOT' THEN total_time_ms END) AS total_search_time,
                
                -- MakeMove
                MAX(CASE WHEN function = 'MakeMove' THEN total_time_ms END) AS make_move_total_ms,
                MAX(CASE WHEN function = 'MakeMove' THEN total_time_ms / NULLIF(num_calls, 0) END) AS make_move_avg_ms,
                MAX(CASE WHEN function = 'MakeMove' THEN total_time_ms END) / 
                    NULLIF(MAX(CASE WHEN function = 'ROOT' THEN total_time_ms END), 0) AS make_move_perc_total_ms,

                -- UnMakeMove
                MAX(CASE WHEN function = 'UnMakeMove' THEN total_time_ms END) AS unmake_move_total_ms,
                MAX(CASE WHEN function = 'UnMakeMove' THEN total_time_ms / NULLIF(num_calls, 0) END) AS unmake_move_avg_ms,
                MAX(CASE WHEN function = 'UnMakeMove' THEN total_time_ms END) / 
                    NULLIF(MAX(CASE WHEN function = 'ROOT' THEN total_time_ms END), 0) AS unmake_move_perc_total_ms,

                -- Movegen
                MAX(CASE WHEN function = 'Movegen' THEN total_time_ms END) AS movegen_total_ms,
                MAX(CASE WHEN function = 'Movegen' THEN total_time_ms / NULLIF(num_calls, 0) END) AS movegen_avg_ms,
                MAX(CASE WHEN function = 'Movegen' THEN total_time_ms END) / 
                    NULLIF(MAX(CASE WHEN function = 'ROOT' THEN total_time_ms END), 0) AS movegen_perc_total_ms,

                -- Score_Order (Move Order)
                MAX(CASE WHEN function = 'Score_Order' THEN total_time_ms END) AS move_order_total_ms,
                MAX(CASE WHEN function = 'Score_Order' THEN total_time_ms / NULLIF(num_calls, 0) END) AS move_order_avg_ms,
                MAX(CASE WHEN function = 'Score_Order' THEN total_time_ms END) / 
                    NULLIF(MAX(CASE WHEN function = 'ROOT' THEN total_time_ms END), 0) AS move_order_perc_total_ms,

                -- NNUE
                MAX(CASE WHEN function = 'NNUE' THEN total_time_ms END) AS nnue_total_ms,
                MAX(CASE WHEN function = 'NNUE' THEN total_time_ms / NULLIF(num_calls, 0) END) AS nnue_avg_ms,
                MAX(CASE WHEN function = 'NNUE' THEN total_time_ms END) / 
                    NULLIF(MAX(CASE WHEN function = 'ROOT' THEN total_time_ms END), 0) AS nnue_perc_total_ms,

                -- Eval (Static)
                MAX(CASE WHEN function = 'Eval' THEN total_time_ms END) AS static_eval_total_ms,
                MAX(CASE WHEN function = 'Eval' THEN total_time_ms / NULLIF(num_calls, 0) END) AS static_eval_avg_ms,
                MAX(CASE WHEN function = 'Eval' THEN total_time_ms END) / 
                    NULLIF(MAX(CASE WHEN function = 'ROOT' THEN total_time_ms END), 0) AS static_eval_perc_total_ms,

                -- SEE
                MAX(CASE WHEN function = 'SEE' THEN total_time_ms END) AS see_total_ms,
                MAX(CASE WHEN function = 'SEE' THEN total_time_ms / NULLIF(num_calls, 0) END) AS see_avg_ms,
                MAX(CASE WHEN function = 'SEE' THEN total_time_ms END) / 
                    NULLIF(MAX(CASE WHEN function = 'ROOT' THEN total_time_ms END), 0) AS see_perc_total_ms,

                -- TT_PROBE
                MAX(CASE WHEN function = 'TT_PROBE' THEN total_time_ms END) AS tt_probe_total_ms,
                MAX(CASE WHEN function = 'TT_PROBE' THEN total_time_ms / NULLIF(num_calls, 0) END) AS tt_probe_avg_ms,
                MAX(CASE WHEN function = 'TT_PROBE' THEN total_time_ms END) / 
                    NULLIF(MAX(CASE WHEN function = 'ROOT' THEN total_time_ms END), 0) AS tt_probe_perc_total_ms,

                -- TT_STORE
                MAX(CASE WHEN function = 'TT_STORE' THEN total_time_ms END) AS tt_store_total_ms,
                MAX(CASE WHEN function = 'TT_STORE' THEN total_time_ms / NULLIF(num_calls, 0) END) AS tt_store_avg_ms,
                MAX(CASE WHEN function = 'TT_STORE' THEN total_time_ms END) / 
                    NULLIF(MAX(CASE WHEN function = 'ROOT' THEN total_time_ms END), 0) AS tt_store_perc_total_ms
                    
            FROM search_timings
            GROUP BY search_id
        ),
                 

        iterative_depth AS (
            SELECT
                
            search_id,
                 
            AVG(itdeep.nps) AS avg_nps,
            STDDEV(itdeep.nps) AS stddev_nps,
            MAX(itdeep.nps) AS peak_nps,
            MIN(itdeep.nps) AS worst_nps,
            MAX_BY(itdeep.nps, itdeep.depth) AS final_nps,

            AVG(itdeep.qratio) AS avg_qratio,
            MAX(itdeep.qratio) AS max_qratio,
            STDDEV(itdeep.qratio) AS stddev_qratio,

            AVG(itdeep.ebf) AS avg_ebf,
            MAX(itdeep.ebf) AS max_ebf,
            exp(avg(log(CASE WHEN itdeep.ebf > 0 THEN itdeep.ebf ELSE 1 END))) AS geo_mean_ebf,

            AVG(itdeep.qebf) AS avg_qebf,
            MAX(itdeep.qebf) AS max_qebf,
            exp(avg(log(CASE WHEN itdeep.qebf > 0 THEN itdeep.ebf ELSE 1 END))) AS geo_mean_qebf,

            AVG(itdeep.tt_hit_ratio) AS avg_tt_hit_ratio,
            MAX(itdeep.tt_hit_ratio) AS max_tt_hit_ratio,
            STDDEV(itdeep.tt_hit_ratio) AS stddev_tt_hit_ratio,

            AVG(itdeep.tt_store_ratio) AS avg_tt_store_ratio,
            MAX(itdeep.tt_store_ratio) AS max_tt_store_ratio,
            STDDEV(itdeep.tt_store_ratio) AS stddev_tt_store_ratio,

            AVG(itdeep.fail_high_ratio) AS avg_fail_high_ratio,
            MAX(itdeep.fail_high_ratio) AS max_fail_high_ratio,
            AVG(itdeep.fail_low_ratio) AS avg_fail_low_ratio,
            MAX(itdeep.fail_low_ratio) AS max_fail_low_ratio,
            AVG(itdeep.fail_high_first / NULLIF(itdeep.fail_highs,1)) AS avg_fail_high_first_ratio,
            AVG(itdeep.fail_high_late / NULLIF(itdeep.fail_highs,1)) AS avg_fail_high_late_ratio,

            AVG(itdeep.see_prune_ratio) AS avg_see_prune_ratio,
            AVG(itdeep.delta_prune_ratio) AS avg_delta_prune_ratio,
            AVG(itdeep.prune_ratio) AS avg_prune_ratio,

            MAX(itdeep.fail_high_researches) AS max_fail_high_researches,
            MAX(itdeep.fail_low_researches) AS max_fail_low_researches,

            MAX(itdeep.move_stability) AS max_move_stability,
            MAX_BY(itdeep.move_stability, itdeep.depth) AS final_move_stability,

            MAX_BY(itdeep.eval_sign_flips, itdeep.depth) AS eval_sign_flips,
            
            MAX(itdeep.eval) AS max_eval,
            AVG(itdeep.eval) AS avg_eval,
            STDDEV(itdeep.eval) AS stddev_eval,
            
            FROM search_iteration_features itdeep
            GROUP BY search_id
        )


        SELECT
            -- raw search stats
            s.id as search_id,
            s.engine_id,
            s.game_id,
            s.sts_id,
            s.fen,
            s.ply,
            s.time_ms AS total_time_ms,
            s.eval AS final_eval,
            s.move AS best_move,
            s.principal_variation,
            s.depth AS max_depth,
            s.qdepth AS max_qdepth,
            s.nodes AS total_internal_nodes,
            s.qnodes AS total_qnodes,
            s.nodes + s.qnodes AS total_nodes,
            s.tt_stores AS total_tt_stores,
            s.tt_hits AS total_tt_hits,
            s.fail_highs AS total_fail_highs,
            s.fail_lows AS total_fail_lows,
            s.fail_high_first AS total_fail_high_first,
            s.fail_high_late AS total_fail_high_late,
            s.fail_high_researches AS total_fail_high_researches,
            s.fail_low_researches AS total_fail_low_researches,
            s.see_prunes AS total_see_prunes,
            s.delta_prunes AS total_delta_prunes,

            -- total ratios
            (s.nodes + s.qnodes) / NULLIF(s.time_ms / 1000,0) AS nps,
            s.qnodes / NULLIF(s.nodes,0) AS qratio,
            s.tt_hits / NULLIF(s.nodes + s.qnodes,0) AS tt_hit_ratio,
            s.tt_stores / NULLIF(s.nodes + s.qnodes,0) AS tt_store_ratio,
            s.fail_highs / NULLIF(s.nodes + s.qnodes,0) AS fail_high_ratio,
            s.fail_lows / NULLIF(s.nodes + s.qnodes,0) AS fail_low_ratio,
            s.fail_high_first / NULLIF(s.fail_highs,1) AS fail_high_first_ratio,
            s.fail_high_late / NULLIF(s.fail_highs,1) AS fail_high_late_ratio,
            s.fail_high_researches / GREATEST(1, 1 + s.depth - {ASPIRATION_START_DEPTH}) AS fail_high_researches_per_depth,
            s.fail_low_researches / GREATEST(1, 1 + s.depth - {ASPIRATION_START_DEPTH}) AS fail_low_researches_per_depth,

            -- iterative deepening aggregated stats
            itdeep.* EXCLUDE (search_id),
            itdeep.eval_sign_flips / NULLIF(s.depth,1) AS eval_sign_flips_per_depth,

            -- timing stats
            t.make_move_avg_ms AS make_move_avg_ms, 
            t.make_move_perc_total_ms AS make_move_perc_total_time,
            t.unmake_move_avg_ms AS unmake_move_avg_ms, 
            t.unmake_move_perc_total_ms AS unmake_move_perc_total_time,
            t.movegen_avg_ms AS movegen_avg_ms, 
            t.movegen_perc_total_ms AS movegen_perc_total_time,
            t.move_order_avg_ms AS move_order_avg_ms, 
            t.move_order_perc_total_ms AS move_order_perc_total_time,
            t.tt_probe_avg_ms AS tt_probe_avg_ms, 
            t.tt_probe_perc_total_ms AS tt_probe_perc_total_time,
            t.tt_store_avg_ms AS tt_store_avg_ms, 
            t.tt_store_perc_total_ms AS tt_store_perc_total_time,
            t.see_avg_ms AS see_avg_ms, 
            t.see_perc_total_ms AS see_perc_total_time,
            t.nnue_avg_ms AS nnue_avg_ms, 
            t.nnue_perc_total_ms AS nnue_perc_total_time,
            t.static_eval_avg_ms AS static_eval_avg_ms, 
            t.static_eval_perc_total_ms AS static_eval_perc_total_time,
            
            -- position features
            pf.balance AS position_balance,
            pf.white_backwards AS position_white_backwards,
            pf.white_doubled AS position_white_doubled,
            pf.white_passed AS position_white_passed,
            pf.black_backwards AS position_black_backwards,
            pf.black_doubled AS position_black_doubled,
            pf.black_passed AS position_black_passed,
            pf.white_shield_pawns AS position_white_shield_pawns,
            pf.white_open_files AS position_white_open_files,
            pf.white_tropism AS position_white_tropism,
            pf.black_shield_pawns AS position_black_shield_pawns,
            pf.black_open_files AS position_black_open_files,
            pf.black_tropism AS position_black_tropism,
            pf.white_num_moves AS position_white_num_moves,
            pf.white_capture_ratio AS position_white_capture_ratio,
            pf.white_check_ratio AS position_white_check_ratio,
            pf.white_legal_enemy AS position_white_legal_enemy,
            pf.white_controlled_enemy AS position_white_controlled_enemy,
            pf.black_num_moves AS position_black_num_moves,
            pf.black_capture_ratio AS position_black_capture_ratio,
            pf.black_check_ratio AS position_black_check_ratio,
            pf.black_legal_enemy AS position_black_legal_enemy,
            pf.black_controlled_enemy AS position_black_controlled_enemy


        FROM search_stats s
        LEFT JOIN iterative_depth itdeep
            ON s.id = itdeep.search_id
        LEFT JOIN times t 
            ON s.id = t.search_id
        LEFT JOIN position_features pf
            ON s.id = pf.search_id
    """)

import duckdb
import platform
system = platform.system()

if __name__ == "__main__":
    if system == "Windows": DB = "F:/databases/chess_analytics.duckdb"

    cnxn = duckdb.connect(DB)

    build_search_tree_features(cnxn)
    build_search_iterations_features(cnxn)
    build_search_features(cnxn)

    cnxn.close()