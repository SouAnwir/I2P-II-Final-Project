#pragma once
#include "search_types.hpp"
#include "game_history.hpp"

struct QSearchParams {
    bool use_kp_eval = true;
    bool use_eval_mobility = true;
    bool report_partial = true;
    int max_q_depth = 4;

    static QSearchParams from_map(const ParamMap& m){
        QSearchParams p;
        p.use_kp_eval       = param_bool(m, "UseKPEval", true);
        p.use_eval_mobility = param_bool(m, "UseEvalMobility", true);
        p.report_partial    = param_bool(m, "ReportPartial", true);
        p.max_q_depth       = param_int(m, "MaxQDepth", 4);
        return p;
    }
};

class QSearch{
public:
    static int quiescence(
        State *state,
        int alpha,
        int beta,
        GameHistory& history,
        int ply,
        int q_depth_left,
        SearchContext& ctx,
        const QSearchParams& p
    );

    static int eval_ctx(
        State *state,
        int depth,
        int alpha,
        int beta,
        GameHistory& history,
        int ply,
        SearchContext& ctx,
        const QSearchParams& p
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
