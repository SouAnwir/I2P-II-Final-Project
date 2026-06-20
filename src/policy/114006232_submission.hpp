#pragma once
#include "search_types.hpp"
#include "game_history.hpp"

struct PVSAdvParams {
    bool use_kp_eval = true;
    bool use_eval_mobility = true;
    bool report_partial = true;
    bool use_pvs_move_ordering = true;
    int max_q_depth = 4;

    static PVSAdvParams from_map(const ParamMap& m){
        PVSAdvParams p;
        p.use_kp_eval           = param_bool(m, "UseKPEval", true);
        p.use_eval_mobility     = param_bool(m, "UseEvalMobility", true);
        p.report_partial        = param_bool(m, "ReportPartial", true);
        p.use_pvs_move_ordering = param_bool(m, "UsePVSMoveOrdering", true);
        p.max_q_depth           = param_int(m, "MaxQDepth", 4);
        return p;
    }
};

class PVSAdv{
public:
    static int quiescence(
        State *state,
        int alpha,
        int beta,
        GameHistory& history,
        int ply,
        int q_depth_left,
        SearchContext& ctx,
        const PVSAdvParams& p
    );

    static int eval_ctx(
        State *state,
        int depth,
        int alpha,
        int beta,
        GameHistory& history,
        int ply,
        SearchContext& ctx,
        const PVSAdvParams& p
    );
    static SearchResult search(
        State *state,
        int depth,
        GameHistory& history,
        SearchContext& ctx
    );

    static ParamMap default_params();
    static std::vector<ParamDef> param_defs();
};
