#include "114006232_state.hpp"
#include "114006232_pvs.hpp"

int PVS::eval_ctx(
    State *state,
    int depth,
    int alpha,
    int beta,
    GameHistory& history,
    int ply,
    SearchContext& ctx,
    const PVSParams& p
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

    if(depth <= 0){
        int score = state->evaluate(
            p.use_kp_eval, p.use_eval_mobility, &history
        );
        history.pop(state->hash());
        return score;
    }

    int best_score = M_MAX;
    bool first_child = true;

    for(auto& action : state->legal_actions){
        if(ctx.stop){
            break;
        }

        State* next = state->next_state(action);
        bool same = next->same_player_as_parent();
        int score;

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
        first_child = false;

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

SearchResult PVS::search(
    State *state,
    int depth,
    GameHistory& history,
    SearchContext& ctx
){
    ctx.reset();
    PVSParams p = PVSParams::from_map(ctx.params);
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

    for(auto& action : state->legal_actions){
        if(ctx.stop){
            break;
        }

        State* next = state->next_state(action);
        bool same = next->same_player_as_parent();
        int score;

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
            break;
        }
        move_index++;
    }

    result.score = best_score;
    result.nodes = ctx.nodes;
    result.seldepth = ctx.seldepth;
    result.pv = {result.best_move};
    return result;
}

ParamMap PVS::default_params(){
    return {
        {"UseKPEval", "true"},
        {"UseEvalMobility", "true"},
        {"ReportPartial", "true"},
    };
}

std::vector<ParamDef> PVS::param_defs(){
    return {
        {"UseKPEval", ParamDef::CHECK, "true"},
        {"UseEvalMobility", ParamDef::CHECK, "true"},
        {"ReportPartial", ParamDef::CHECK, "true"},
    };
}
