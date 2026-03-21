/********************************************************************
*
*
*	ServerGame: Nav2 Query State - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_query_state.h"


/**
*
*
*	Nav2 Query State Public API:
*
*
**/
/**
*	@brief	Reset a query state object back to a deterministic initial state.
*	@param	state	[out] Query state to reset.
**/
void SVG_Nav2_QueryState_Reset( nav2_query_state_t *state ) {
    /**
    *    Sanity-check the destination state before resetting it.
    **/
    if ( !state ) {
        return;
    }

    // Reset the full query state to deterministic defaults.
    *state = nav2_query_state_t{};
    // Explicitly mark the initial result status as pending.
    state->result_status = nav2_query_result_status_t::Pending;
}

/**
*	@brief	Advance a query state to a new stage while clearing the active slice.
*	@param	state	Query state to mutate.
*	@param	stage	New stage to assign.
**/
void SVG_Nav2_QueryState_SetStage( nav2_query_state_t *state, const nav2_query_stage_t stage ) {
    /**
    *    Sanity-check the query state before mutating any stage-transition fields.
    **/
    if ( !state ) {
        return;
    }

    // Assign the next scheduler stage for the query.
    state->stage = stage;
    // Clear the previous active slice because stage transitions always require a fresh grant.
    state->active_slice = nav2_budget_slice_t{};
}

/**
*	@brief	Return whether the query reached a terminal scheduler state.
*	@param	state	Query state to inspect.
*	@return	True when the query completed, failed, was cancelled, or became stale.
**/
const bool SVG_Nav2_QueryState_IsTerminal( const nav2_query_state_t *state ) {
    /**
    *    Treat missing state as non-terminal so callers do not accidentally consider nullptr jobs complete.
    **/
    if ( !state ) {
        return false;
    }

    /**
    *    Map the stable result status enum onto a simple terminal/non-terminal scheduler predicate.
    **/
    switch ( state->result_status ) {
    case nav2_query_result_status_t::Success:
    case nav2_query_result_status_t::Failed:
    case nav2_query_result_status_t::Stale:
    case nav2_query_result_status_t::Cancelled:
        return true;
    case nav2_query_result_status_t::None:
    case nav2_query_result_status_t::Pending:
    case nav2_query_result_status_t::Partial:
    case nav2_query_result_status_t::Count:
    default:
        break;
    }
    return false;
}
