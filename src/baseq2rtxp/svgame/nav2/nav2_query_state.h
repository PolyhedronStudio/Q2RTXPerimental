/********************************************************************
*
*
*	ServerGame: Nav2 Query State
*
*
********************************************************************/
#pragma once

#include "svgame/nav2/nav2_bench.h"
#include "svgame/nav2/nav2_budget.h"
#include "svgame/nav2/nav2_snapshot.h"


/**
*
*
*	Nav2 Query Working State:
*
*
**/
/**
*	@brief	Stable partial-result status published by a nav2 job.
*	@note	This stays deliberately coarse for the scheduler foundation; later solver stages can add
*			more detail without changing the scheduler-facing contract.
**/
enum class nav2_query_result_status_t : uint8_t {
    None = 0,
    Pending,
    Partial,
    Success,
    Failed,
    Stale,
    Cancelled,
    Count
};

/**
*	@brief	Solver-progress counters carried by a resumable nav2 query job.
*	@note	These counters are intentionally generic so the scheduler foundation can track progress
*			before the concrete coarse/fine solver implementations exist.
**/
struct nav2_query_progress_t {
    //! Number of candidate endpoints evaluated so far.
    uint32_t candidate_count = 0;
    //! Number of coarse-search expansions consumed so far.
    uint32_t coarse_expansions = 0;
    //! Number of fine-refinement expansions consumed so far.
    uint32_t fine_expansions = 0;
    //! Number of mover-local refinement expansions consumed so far.
    uint32_t mover_expansions = 0;
    //! Number of stage slices consumed so far.
    uint32_t slices_consumed = 0;
};

/**
*	@brief	Scheduler-owned resumable state for one nav2 query.
*	@note	The fields in this struct are designed to remain pointer-light and serialization-friendly.
**/
struct nav2_query_state_t {
    //! Current stage the job is waiting to execute or resume.
    nav2_query_stage_t stage = nav2_query_stage_t::Intake;
    //! Current result status known for the query.
    nav2_query_result_status_t result_status = nav2_query_result_status_t::Pending;
    //! Snapshot handle captured when the query bound worker-safe state.
    nav2_snapshot_handle_t snapshot = {};
    //! Last budget slice granted to the query.
    nav2_budget_slice_t active_slice = {};
    //! Aggregate solver progress counters tracked by the scheduler.
    nav2_query_progress_t progress = {};
    //! Whether the query has a provisional corridor-only result available.
    bool has_provisional_result = false;
    //! Whether the query requires main-thread revalidation before acceptance.
    bool requires_revalidation = false;
   //! True when the current stage should execute through the worker dispatch path.
    bool prefer_worker_dispatch = true;
    //! Whether the query requested cancellation.
    bool cancel_requested = false;
    //! Whether the query should restart from its current stage boundary.
    bool restart_requested = false;
    //! Benchmark scenario classification used for future diagnostics and replay wiring.
    nav2_bench_scenario_t bench_scenario = nav2_bench_scenario_t::SameFloorOpen;
};


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
void SVG_Nav2_QueryState_Reset( nav2_query_state_t *state );

/**
*	@brief	Advance a query state to a new stage while clearing the active slice.
*	@param	state	Query state to mutate.
*	@param	stage	New stage to assign.
**/
void SVG_Nav2_QueryState_SetStage( nav2_query_state_t *state, const nav2_query_stage_t stage );

/**
*	@brief	Return whether the query reached a terminal scheduler state.
*	@param	state	Query state to inspect.
*	@return	True when the query completed, failed, was cancelled, or became stale.
**/
const bool SVG_Nav2_QueryState_IsTerminal( const nav2_query_state_t *state );
