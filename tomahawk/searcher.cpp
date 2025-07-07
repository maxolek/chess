#include "searcher.h"

// Define static members
int Searcher::historyHeuristic[12][64] = {};        // Assuming 12 piece types and 64 squares
Move Searcher::killerMoves[64][2] = {};             // Assuming max search depth is 64
int Searcher::nodesSearched = 0;
std::unordered_map<U64, TTEntry> Searcher::tt;
std::vector<Move> Searcher::best_line;


SearchResult Searcher::search(Board& board, MoveGenerator& movegen, Evaluator& evaluator, Move legal_moves[MAX_MOVES], int count, int depth, Move pvMove, std::chrono::steady_clock::time_point start_time, int time_limit_ms) {
    bool out_of_time = false;
    int bestEval = -MATE_SCORE;
    int alpha = -MATE_SCORE; int beta = MATE_SCORE;
    Move bestMove = legal_moves[0]; //Move::NullMove();
    std::unordered_map<std::string, int> bestComponentEvals;
    int prev_evals[MAX_MOVES];
    std::vector<Move> principalVariation;
    best_line.clear();
    nodesSearched = 0;
    std::fill_n(prev_evals, MAX_MOVES, -MATE_SCORE);

    for (int i = 0; i < count; i++) {
        Move m =  legal_moves[i];
        if (Move::SameMove(m, Move::NullMove())) {continue;} 
        else {
            board.MakeMove(m);

            out_of_time = false;
            std::vector<Move> childPV;
            std::vector<Move> emptyPV;
            int eval = -negamax(
                board, movegen, evaluator,
                depth - 1, 
                -beta, -alpha, childPV, emptyPV, prev_evals,
                start_time, time_limit_ms, out_of_time
            );

            board.UnmakeMove(m);;

            prev_evals[i] = eval;
            if (out_of_time) {
                if (i > 0) break; // timeout after at least one good move: keep the best one
                else return {Move::NullMove(), -100*MATE_SCORE, {}}; // first move timed out, return last iterations result
            }

            if ((i == 0) || eval > bestEval) {
                bestEval = eval;
                bestMove = m;
                //best_line[0] = m; // start tracking new best move
                //for (int i = 1; i < MAX_DEPTH; i++) {if(Move::SameMove(tmp_best_line[i],Move(0))){break;} else {best_line[i] = tmp_best_line[i];}}
                //bestComponentEvals = Evaluator::computeComponents(board);
                // Set PV to current root move + line from deeper search
                principalVariation.clear();
                principalVariation.push_back(m);
                principalVariation.insert(principalVariation.end(), childPV.begin(), childPV.end());
                best_line = principalVariation;
            }

            // update alpha/beta as usual if you want iterative deepening with pruning here
            alpha = std::max(alpha, eval);

            // check time
            auto current_time = std::chrono::steady_clock::now();
            int elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();
            // if  evaluating prev search best move first then we can still return the best move
            if (elapsed_ms >= time_limit_ms && i>0) {out_of_time = true; break;}//{bestMove = Move::NullMove(); break;}
        }
    }

    return {bestMove, board.is_white_move ? bestEval : -bestEval, bestComponentEvals};
}

int Searcher::negamax(Board& board, MoveGenerator& movegen, Evaluator& evaluator, int depth, int alpha, int beta, std::vector<Move>& pv, const std::vector<Move>& nextPV,
                         int prev_eval[MAX_MOVES], std::chrono::steady_clock::time_point start_time, int time_limit_ms, bool &out_of_time) {
    
                        //if (out_of_time) return bestEval;
    nodesSearched++;
    int eval;
    int local_prev_eval[MAX_MOVES] = {};
    int bestEval = -MATE_SCORE;
    bool foundMove = false;
    //std::fill_n(tmp_best_line, MAX_DEPTH, Move(0));
    //Result result = Arbiter::GetGameState(&board);

    if (depth == 0 /*|| result != InProgress*/) {
        //movegen.mobility(&board); // doubles some key perft position depth5 times, not worth eval increase
        // but there is potential for these pseudo legal moves to be a part of regular movegeneration so 
        // mobility is calculated en-route
        pv = {}; // leaf node = no children
        eval = evaluator.taperedEval(&board);
        return board.is_white_move ? eval : -eval; // eval is side to move agnostic but negamax
    }                                              // returns the eval from the perspective of side to move

    //if ((nodesSearched & 1023) == 0) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count() >= time_limit_ms) {
            out_of_time = true;
            if (foundMove) return bestEval;
            else return alpha; // best lower bound ("no better eval found")
            //return bestEval;
        }
    //}
    
    int alphaOrig = alpha;  // Save original alpha before searching
    U64 hash = board.zobrist_hash;
    // Lookup TT entry
    auto tt_it = tt.find(hash);
    if (tt_it != tt.end()) {
        const TTEntry& entry = tt_it->second;
        if (entry.depth >= depth) {
            if (entry.flag == EXACT) return entry.eval;
            if (entry.flag == LOWERBOUND && entry.eval >= beta) return  entry.eval; // cutoff- fail hard beta
            if (entry.flag == UPPERBOUND && entry.eval <= alpha) return entry.eval; // cutoff- fail soft alpha
        }
    }

    Move next_moves[MAX_MOVES]; 
    Move tt_move = Move(0); Move pvMove = (!pv.empty( )) ? pv[0] : Move::NullMove();
    int flipped_prev_eval[MAX_MOVES];
    for (int i = 0; i < MAX_MOVES; i++) {flipped_prev_eval[i] = -prev_eval[i];}
    int count = generateAndOrderMoves(board, movegen, next_moves, depth, pvMove, flipped_prev_eval);
    // terminal eval check (rather than using arbiter and generating "twice")
    if (count == 0) { 
        if (board.is_in_check) {return -(MATE_SCORE - depth);}
        else {return 0;}
    }
    
    for (int i = 0; i < count; i++) {
        Move m = next_moves[i];
        if (Move::SameMove(m, Move::NullMove())) {continue;} 
        else {
            board.MakeMove(m);

            std::vector<Move> childPV;
            std::vector<Move> childNextPV; 
            if (pv.size() >= 1 && Move::SameMove(m, pv[0])) {childNextPV.insert(childNextPV.end(), pv.begin() + 1, pv.end());}
            eval = -negamax(board, movegen, evaluator, depth - 1, -beta, -alpha, childPV, childNextPV, local_prev_eval,
                start_time, time_limit_ms, out_of_time);
            if (m.MoveFlag() == Move::castleFlag) {eval += 50;} // bias castling
            if (depth-1 % 2 == 0 && board.currentGameState.capturedPieceType > 0 && board.getMovedPiece(m.TargetSquare()) == 0) {
            // discourage getting captured by pawn (bandaid)
                eval -= 150;
            }
            foundMove = true;
            
            board.UnmakeMove(m);

            local_prev_eval[i] = eval;

            if (eval > bestEval) { // check if first move, forcing pv/best_eval to update
                bestEval = eval;
                tt_move = m;
                // build PV
                pv.clear();
                pv.push_back(m);
                pv.insert(pv.end(), childPV.begin(), childPV.end());
            }

            if (out_of_time) break;

            alpha = std::max(alpha, eval);
            if (alpha >= beta) {
                if (board.getCapturedPiece(m.TargetSquare()) == -1) {  // quiet move
                    if (!Move::SameMove(killerMoves[depth][0], m)) {
                        killerMoves[depth][1] = killerMoves[depth][0];
                        killerMoves[depth][0] = m;
                    }

                    int piece = board.getMovedPiece(m.StartSquare());
                    historyHeuristic[board.is_white_move ? piece : piece+6][m.TargetSquare()] += depth * depth;
                }
                break;
            }
        }
    }

    // Store result in TT
    TTEntry newEntry;
    newEntry.eval = board.is_white_move ? bestEval : -bestEval; // store as whites perspective
    newEntry.bestMove = tt_move;
    newEntry.depth = depth;
    // Set flag based on alpha-beta bounds logic (EXACT, LOWERBOUND, UPPERBOUND)
    if (bestEval <= alphaOrig) newEntry.flag = UPPERBOUND;
    else if (bestEval >= beta) newEntry.flag = LOWERBOUND;
    else newEntry.flag = EXACT;

    tt[hash] = newEntry;

    return bestEval;
}

// call before makeMove()
int Searcher::moveScore(const Move& move, const Board& board, int depth, const Move& ttMove, const Move& pvMove, int prev_eval) {
    // TT move
    // movegen count solves pvMove null move concern
    if (Move::SameMove(pvMove, move)) return MATE_SCORE + 500;
    if (Move::SameMove(ttMove, move)) return MATE_SCORE + 100;

    // Captures — MVV-LVA
    if (board.getCapturedPiece(move.TargetSquare()) != -1) {
        int attacker = board.getMovedPiece(move.StartSquare());
        int victim = board.getMovedPiece(move.TargetSquare());
        return MATE_SCORE + 10 * Evaluator::pieceValues[victim] - Evaluator::pieceValues[attacker];
    }
    // Catpures - SEE
    if (board.getCapturedPiece(move.TargetSquare()) != -1) {
        int seeScore = Evaluator::SEE(board, move.TargetSquare(), board.is_white_move);
        if (seeScore < 0) {
            return -999999; // Bad trade, heavily deprioritize
    }
    }

    // Killer moves
    if (Move::SameMove(killerMoves[depth][0], move)) return MATE_SCORE - 500;
    if (Move::SameMove(killerMoves[depth][1], move)) return MATE_SCORE - 600;

    // History heuristic
    int piece = board.getMovedPiece(move.StartSquare());
    return historyHeuristic[board.is_white_move ? piece : piece+6][move.TargetSquare()]; // + prev_eval;
}


void Searcher::orderedMoves(Move moves[MAX_MOVES], int count, const Board& board, int depth, const Move& pvMove, int prev_eval[MAX_MOVES]) {
    U64 hash = board.zobrist_hash;
    Move ttMove = Move::NullMove();

    auto it = tt.find(hash);
    if (it != tt.end()) {
        ttMove = it->second.bestMove;
    }

    std::vector<std::pair<int, Move>> scored;

    for (int i = 0; i < count; ++i) {
        int score = moveScore(moves[i], board, depth, ttMove, pvMove, prev_eval[i]);
        scored.emplace_back(score, moves[i]);
    }

    std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
        return a.first > b.first;
    });

    for (int i = 0; i < count; ++i) {
        moves[i] = scored[i].second;
    }
}

int Searcher::generateAndOrderMoves(Board& board, MoveGenerator& movegen, Move moves[], int depth, const Move& pvMove, int prev_eval[MAX_MOVES]) {
    int count = movegen.generateMovesList(&board, moves);
    orderedMoves(moves, count, board, depth, pvMove, prev_eval);
    return count;
}