#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include "114006232_state.hpp"
#include "114006232_submission.hpp"

namespace {

int piece_value(int piece){
    static const int value[7] = {0, 100, 500, 320, 330, 900, 20000};
    return (piece >= 0 && piece <= 6) ? value[piece] : 0;
}

int score_for_reporting(int score){
    if(score >= P_MAX - 100){
        return P_MAX - 1000;
    }
    if(score <= M_MAX + 100){
        return M_MAX + 1000;
    }
    return score;
}

int move_order_score(State* state, const Move& action){
    const int player = state->player;
    const int opp = 1 - player;
    const int moved = state->piece_at(player, action.first.first, action.first.second);
    const int captured = state->piece_at(opp, action.second.first, action.second.second);

    int score = 0;
    if(captured){
        score += 10 * piece_value(captured) - piece_value(moved);
    }
    if(moved == 1 && (action.second.first == 0 || action.second.first + 1 == (size_t)state->board_h())){
        score += piece_value(5) - piece_value(1);
    }
    return score;
}

bool is_immediate_win(State* state, const Move& action){
    const int opp = 1 - state->player;
    return state->piece_at(opp, action.second.first, action.second.second) == 6;
}

bool is_capture(State* state, const Move& action){
    const int opp = 1 - state->player;
    return state->piece_at(opp, action.second.first, action.second.second) != 0;
}

bool is_promotion(State* state, const Move& action){
    const int moved = state->piece_at(state->player, action.first.first, action.first.second);
    return moved == 1
        && (action.second.first == 0 || action.second.first + 1 == (size_t)state->board_h());
}

bool is_quiescence_move(State* state, const Move& action){
    return is_capture(state, action) || is_promotion(state, action) || is_immediate_win(state, action);
}

bool opponent_can_win_next(State* state, const Move& action){
    State* next = state->next_state(action);
    next->get_legal_actions();
    bool can_win = next->game_state == WIN;
    delete next;
    return can_win;
}

enum TTFlag {
    TT_EXACT,
    TT_LOWER,
    TT_UPPER
};

struct TTEntry {
    int depth = -1;
    int score = 0;
    TTFlag flag = TT_EXACT;
    Move best_move;
    bool has_best_move = false;
};

std::unordered_map<uint64_t, TTEntry> tt;
uint64_t tt_root_hash = 0;
bool tt_has_root = false;

constexpr int MAX_KILLER_PLY = 128;
Move killer_moves[MAX_KILLER_PLY][2];
bool killer_valid[MAX_KILLER_PLY][2] = {};
std::unordered_map<uint64_t, int> history_scores;

bool same_move(const Move& a, const Move& b){
    return a == b;
}

uint64_t move_key(const Move& m){
    return (static_cast<uint64_t>(m.first.first) << 48)
        ^ (static_cast<uint64_t>(m.first.second) << 32)
        ^ (static_cast<uint64_t>(m.second.first) << 16)
        ^ static_cast<uint64_t>(m.second.second);
}

bool is_killer(const Move& m, int ply){
    if(ply < 0 || ply >= MAX_KILLER_PLY){
        return false;
    }
    return (killer_valid[ply][0] && same_move(m, killer_moves[ply][0]))
        || (killer_valid[ply][1] && same_move(m, killer_moves[ply][1]));
}

void remember_killer(const Move& m, int ply){
    if(ply < 0 || ply >= MAX_KILLER_PLY || is_killer(m, ply)){
        return;
    }
    killer_moves[ply][1] = killer_moves[ply][0];
    killer_valid[ply][1] = killer_valid[ply][0];
    killer_moves[ply][0] = m;
    killer_valid[ply][0] = true;
}

void remember_history(const Move& m, int depth){
    int& score = history_scores[move_key(m)];
    score += depth * depth;
    if(score > 100000000){
        for(auto& item : history_scores){
            item.second /= 2;
        }
    }
}

int history_score(const Move& m){
    auto it = history_scores.find(move_key(m));
    return (it == history_scores.end()) ? 0 : it->second;
}

void order_actions(
    State* state,
    std::vector<Move>& actions,
    const Move* tt_move = nullptr,
    int ply = 0,
    bool use_search_heuristics = true
){
    std::stable_sort(actions.begin(), actions.end(), [state, tt_move, ply, use_search_heuristics](const Move& a, const Move& b){
        if(tt_move){
            bool a_tt = same_move(a, *tt_move);
            bool b_tt = same_move(b, *tt_move);
            if(a_tt != b_tt){
                return a_tt;
            }
        }
        if(is_immediate_win(state, a) != is_immediate_win(state, b)){
            return is_immediate_win(state, a);
        }
        int a_score = move_order_score(state, a);
        int b_score = move_order_score(state, b);

        if(use_search_heuristics){
            bool a_killer = is_killer(a, ply);
            bool b_killer = is_killer(b, ply);
            if(a_killer != b_killer){
                return a_killer;
            }
        }

        if(a_score != b_score){
            return a_score > b_score;
        }
        return use_search_heuristics && history_score(a) > history_score(b);
    });
}

} // namespace


/*============================================================
 * PVSAdv - quiescence
 *
 * Extends leaf nodes through forcing moves so evaluation does not stop
 * in the middle of captures, promotions, or immediate wins.
 *============================================================*/
int PVSAdv::quiescence(
    State *state,
    int alpha,
    int beta,
    GameHistory& history,
    int ply,
    int q_depth_left,
    SearchContext& ctx,
    const PVSAdvParams& p
){
    ctx.nodes++;
    if(ply > ctx.seldepth){
        ctx.seldepth = ply;
    }
    if(ctx.stop){
        return 0;
    }

    if(state->legal_actions.empty() && state->game_state == UNKNOWN){
        state->get_legal_actions();
    }

    if(state->game_state == WIN){
        return P_MAX - ply;
    }
    if(state->game_state == DRAW){
        return 0;
    }

    int rep_score;
    if(state->check_repetition(history, rep_score)){
        return rep_score;
    }
    history.push(state->hash());

    int best_score = state->evaluate(
        p.use_kp_eval, p.use_eval_mobility, &history
    );
    if(best_score > alpha){
        alpha = best_score;
    }
    if(alpha >= beta || q_depth_left <= 0){
        history.pop(state->hash());
        return best_score;
    }

    auto actions = state->legal_actions;
    order_actions(state, actions, nullptr, ply, p.use_pvs_move_ordering);

    for(auto& action : actions){
        if(ctx.stop){
            break;
        }
        if(!is_quiescence_move(state, action)){
            continue;
        }

        int score;
        if(is_immediate_win(state, action)){
            score = P_MAX - ply;
        }else{
            State* next = state->next_state(action);
            bool same = next->same_player_as_parent();
            int next_alpha = same ? alpha : -beta;
            int next_beta = same ? beta : -alpha;
            int raw_score = quiescence(next, next_alpha, next_beta, history, ply + 1, q_depth_left - 1, ctx, p);
            score = same ? raw_score : -raw_score;
            delete next;
        }

        if(score > best_score){
            best_score = score;
        }
        if(best_score > alpha){
            alpha = best_score;
        }
        if(alpha >= beta){
            break;
        }
    }

    history.pop(state->hash());
    return best_score;
}


/*============================================================
 * PVSAdv - eval_ctx
 *
 * Principal Variation Search in negamax form. Scores are always from
 * state->player's perspective; child scores are negated when the turn changes.
 *============================================================*/
int PVSAdv::eval_ctx(
    State *state,
    int depth,
    int alpha,
    int beta,
    GameHistory& history,
    int ply,
    SearchContext& ctx,
    const PVSAdvParams& p
){
    ctx.nodes++;
    if(ply > ctx.seldepth){
        ctx.seldepth = ply;
    }
    if(ctx.stop){
        return 0;
    }

    if(state->legal_actions.empty() && state->game_state == UNKNOWN){
        state->get_legal_actions();
    }

    if(state->game_state == WIN){
        return P_MAX - ply;
    }
    if(state->game_state == DRAW){
        return 0;
    }

    int rep_score;
    if(state->check_repetition(history, rep_score)){
        return rep_score;
    }

    int original_alpha = alpha;
    uint64_t key = state->hash();
    Move tt_move;
    Move* tt_move_ptr = nullptr;
    auto tt_it = tt.find(key);
    if(tt_it != tt.end()){
        const TTEntry& entry = tt_it->second;
        if(entry.has_best_move){
            tt_move = entry.best_move;
            tt_move_ptr = &tt_move;
        }
        if(entry.depth >= depth){
            if(entry.flag == TT_EXACT){
                return entry.score;
            }
            if(entry.flag == TT_LOWER && entry.score > alpha){
                alpha = entry.score;
            }else if(entry.flag == TT_UPPER && entry.score < beta){
                beta = entry.score;
            }
            if(alpha >= beta){
                return entry.score;
            }
        }
    }

    if(depth <= 0){
        return quiescence(state, alpha, beta, history, ply, p.max_q_depth, ctx, p);
    }

    history.push(state->hash());

    int best_score = M_MAX;
    bool first_child = true;
    Move best_move;
    bool has_best_move = false;

    auto actions = state->legal_actions;
    order_actions(state, actions, tt_move_ptr, ply, p.use_pvs_move_ordering);

    for(auto& action : actions){
        if(ctx.stop){
            break;
        }

        int score;
        if(is_immediate_win(state, action)){
            score = P_MAX - ply;
        }else{
            State* next = state->next_state(action);
            bool same = next->same_player_as_parent();

            if(first_child){
                int next_alpha = same ? alpha : -beta;
                int next_beta = same ? beta : -alpha;
                int raw_score = eval_ctx(next, depth - 1, next_alpha, next_beta, history, ply + 1, ctx, p);
                score = same ? raw_score : -raw_score;
            }else{
                int null_alpha = alpha;
                int null_beta = alpha + 1;
                int next_alpha = same ? null_alpha : -null_beta;
                int next_beta = same ? null_beta : -null_alpha;
                int raw_score = eval_ctx(next, depth - 1, next_alpha, next_beta, history, ply + 1, ctx, p);
                score = same ? raw_score : -raw_score;

                if(score > alpha && score < beta){
                    next_alpha = same ? alpha : -beta;
                    next_beta = same ? beta : -alpha;
                    raw_score = eval_ctx(next, depth - 1, next_alpha, next_beta, history, ply + 1, ctx, p);
                    score = same ? raw_score : -raw_score;
                }
            }

            delete next;
        }

        first_child = false;
        if(score > best_score){
            best_score = score;
            best_move = action;
            has_best_move = true;
        }
        if(best_score > alpha){
            alpha = best_score;
        }
        if(alpha >= beta){
            if(p.use_pvs_move_ordering && move_order_score(state, action) <= 0){
                remember_killer(action, ply);
                remember_history(action, depth);
            }
            break;
        }
    }

    history.pop(state->hash());

    if(ctx.stop || !has_best_move){
        return best_score;
    }

    TTEntry entry;
    entry.depth = depth;
    entry.score = best_score;
    entry.best_move = best_move;
    entry.has_best_move = true;
    if(best_score <= original_alpha){
        entry.flag = TT_UPPER;
    }else if(best_score >= beta){
        entry.flag = TT_LOWER;
    }else{
        entry.flag = TT_EXACT;
    }
    tt[key] = entry;

    return best_score;
}


/*============================================================
 * PVSAdv - search
 *============================================================*/
SearchResult PVSAdv::search(
    State *state,
    int depth,
    GameHistory& history,
    SearchContext& ctx
){
    ctx.reset();
    uint64_t root_hash = state->hash();
    if(!tt_has_root || tt_root_hash != root_hash || depth <= 1){
        tt.clear();
        history_scores.clear();
        std::fill(&killer_valid[0][0], &killer_valid[0][0] + MAX_KILLER_PLY * 2, false);
        tt_root_hash = root_hash;
        tt_has_root = true;
    }
    tt.reserve(1 << 16);
    PVSAdvParams p = PVSAdvParams::from_map(ctx.params);
    SearchResult result;
    result.depth = depth;

    if(!state->legal_actions.size()){
        state->get_legal_actions();
    }

    int best_score = M_MAX - 10;
    int move_index = 0;
    int total_moves = (int)state->legal_actions.size();

    int alpha = M_MAX;
    int beta = P_MAX;
    bool first_child = true;

    auto actions = state->legal_actions;
    Move tt_move;
    Move* tt_move_ptr = nullptr;
    auto tt_it = tt.find(root_hash);
    if(tt_it != tt.end() && tt_it->second.has_best_move){
        tt_move = tt_it->second.best_move;
        tt_move_ptr = &tt_move;
    }
    order_actions(state, actions, tt_move_ptr, 0, p.use_pvs_move_ordering);

    for(auto& action : actions){
        if(ctx.stop){
            break;
        }

        int score;
        if(is_immediate_win(state, action)){
            score = P_MAX - 1;
        }else if(depth <= 1 && opponent_can_win_next(state, action)){
            score = M_MAX + 1;
        }else{
            State* next = state->next_state(action);
            bool same = next->same_player_as_parent();

            if(first_child){
                int next_alpha = same ? alpha : -beta;
                int next_beta = same ? beta : -alpha;
                int raw_score = eval_ctx(next, depth - 1, next_alpha, next_beta, history, 1, ctx, p);
                score = same ? raw_score : -raw_score;
            }else{
                int null_alpha = alpha;
                int null_beta = alpha + 1;
                int next_alpha = same ? null_alpha : -null_beta;
                int next_beta = same ? null_beta : -null_alpha;
                int raw_score = eval_ctx(next, depth - 1, next_alpha, next_beta, history, 1, ctx, p);
                score = same ? raw_score : -raw_score;

                if(score > alpha && score < beta){
                    next_alpha = same ? alpha : -beta;
                    next_beta = same ? beta : -alpha;
                    raw_score = eval_ctx(next, depth - 1, next_alpha, next_beta, history, 1, ctx, p);
                    score = same ? raw_score : -raw_score;
                }
            }

            delete next;
        }

        first_child = false;
        if(score > best_score){
            best_score = score;
            result.best_move = action;
            if(p.report_partial && ctx.on_root_update){
                ctx.on_root_update({result.best_move, best_score, depth, move_index + 1, total_moves});
            }
        }
        if(best_score > alpha){
            alpha = best_score;
        }
        if(alpha >= beta){
            if(p.use_pvs_move_ordering && move_order_score(state, action) <= 0){
                remember_history(action, depth);
            }
            break;
        }
        move_index++;
    }

    if(result.best_move == Move() && !actions.empty()){
        result.best_move = actions[0];
    }

    TTEntry root_entry;
    root_entry.depth = depth;
    root_entry.score = best_score;
    root_entry.flag = TT_EXACT;
    root_entry.best_move = result.best_move;
    root_entry.has_best_move = true;
    tt[root_hash] = root_entry;

    result.score = score_for_reporting(best_score);
    result.nodes = ctx.nodes;
    result.seldepth = ctx.seldepth;
    result.pv = {result.best_move};
    return result;
}


ParamMap PVSAdv::default_params(){
    return {
        {"UseKPEval", "true"},
        {"UseEvalMobility", "true"},
        {"ReportPartial", "true"},
        {"UsePVSMoveOrdering", "true"},
        {"MaxQDepth", "4"},
    };
}

std::vector<ParamDef> PVSAdv::param_defs(){
    return {
        {"UseKPEval", ParamDef::CHECK, "true"},
        {"UseEvalMobility", ParamDef::CHECK, "true"},
        {"ReportPartial", ParamDef::CHECK, "true"},
        {"UsePVSMoveOrdering", ParamDef::CHECK, "true"},
        {"MaxQDepth", ParamDef::SPIN, "4", 0, 12},
    };
}
