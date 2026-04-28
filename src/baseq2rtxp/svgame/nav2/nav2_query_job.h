/********************************************************************
*
*
*	ServerGame: Nav2 Query Job
*
*
********************************************************************/
#pragma once

#include <vector>

#include "svgame/nav2/nav2_query_state.h"


/**
*
*
*	Nav2 Query Request and Job Data:
*
*
**/
/**
*	@brief	Stable scheduler-facing request descriptor for one nav2 path query.
*	@note	This request payload intentionally avoids pointer-heavy ownership so it can later be
*			copied into worker jobs, persisted by stable IDs, or mirrored into compare-mode diagnostics.
**/
struct nav2_query_request_t {
    //! Stable scheduler-assigned request identifier.
    uint64_t request_id = 0;
    //! Stable entity identifier of the requesting agent.
    int32_t agent_entnum = 0;
    //! Stable entity identifier of the goal owner, if any.
    int32_t goal_entnum = 0;
    //! Query start position in feet-origin world space.
    Vector3 start_origin = {};
    //! Query goal position in feet-origin world space.
    Vector3 goal_origin = {};
    //! Query agent mins in feet-origin space captured from the requesting mover at submission time.
    Vector3 agent_mins = {};
    //! Query agent maxs in feet-origin space captured from the requesting mover at submission time.
    Vector3 agent_maxs = {};
    //! Request-local movement policy snapshot captured at submission time.
    nav2_query_policy_t policy = {};
    //! Scheduler priority class for the request.
    nav2_query_priority_t priority = nav2_query_priority_t::Normal;
    //! Optional urgency hint used by future fairness policy.
    uint32_t urgency = 0;
    //! Optional deadline or age hint in milliseconds.
    uint64_t deadline_ms = 0;
    //! True when the request wants an immediate main-thread debug path.
    bool debug_force_sync = false;
    //! True when the request should publish compare-mode diagnostics later.
    bool enable_compare_mode = false;
    //! True when the request should use deterministic benchmark/replay paths.
    bool deterministic_replay = false;
    //! Benchmark scenario used for diagnostics, replay, and future regression wiring.
    nav2_bench_scenario_t bench_scenario = nav2_bench_scenario_t::SameFloorOpen;
};

/**
*	@brief	Resumable scheduler-owned nav2 query job.
*	@note	This is the main unit queued by the scheduler foundation before worker integration hardens.
**/
struct nav2_query_job_t {
    //! Stable job identifier separate from the original request id.
    uint64_t job_id = 0;
    //! Request descriptor that created the job.
    nav2_query_request_t request = {};
    //! Mutable resumable state owned by the scheduler.
    nav2_query_state_t state = {};
    //! Committed world-space waypoints produced by the scheduler pipeline on successful completion.
    std::vector<Vector3> committed_waypoints = {};
    //! True when `committed_waypoints` contains a valid committed path payload.
    bool has_committed_waypoints = false;
    //! Total time this job has spent waiting in the scheduler queue.
    double queue_wait_ms = 0.0;
    //! Total execution time accumulated across granted slices.
    double execution_time_ms = 0.0;
    //! Total main-thread revalidation time accumulated for the job.
    double revalidation_time_ms = 0.0;
    //! Number of times the job has been restarted from a stage boundary.
    uint32_t restart_count = 0;
  //! Benchmark query state used to accumulate queue and worker timing for this job lifecycle.
    nav2_bench_query_t bench_query = {};
    //! Timestamp when the job most recently entered a dispatchable queue state.
    uint64_t queue_enter_timestamp_ms = 0;
    //! Timestamp when the job most recently left the queue for dispatch.
    uint64_t last_dispatch_timestamp_ms = 0;
    //! True while one worker-owned slice is in flight for this job.
    bool worker_in_flight = false;
  //! Monotonic frame index when the job was submitted to the scheduler queue.
    int64_t submitted_frame_number = 0;
    //! Monotonic frame index when the job most recently received a slice grant.
    int64_t last_granted_frame_number = 0;
    //! Number of fairness-based starvation boosts granted to this job.
    uint32_t starvation_boost_count = 0;
    //! Number of overload-throttled slices granted to this job.
    uint32_t throttled_slice_count = 0;
    //! Number of times overload policy requested provisional-fallback behavior for this job.
    uint32_t overload_provisional_fallback_count = 0;
    //! Number of stage transitions observed during this job lifecycle.
    uint32_t stage_transition_count = 0;
    //! Number of stage-boundary restarts observed during this job lifecycle.
    uint32_t stage_restart_count = 0;
};


/**
*
*
*	Nav2 Query Job Public API:
*
*
**/
/**
*	@brief	Initialize a fresh nav2 job from a request descriptor.
*	@param	job	[out] Job object to initialize.
*	@param	request	Request descriptor to copy into the job.
*	@param	jobId	Stable job identifier assigned by the scheduler.
**/
void SVG_Nav2_QueryJob_Init( nav2_query_job_t *job, const nav2_query_request_t &request, const uint64_t jobId );

/**
*	@brief	Request cancellation for a nav2 job.
*	@param	job	Job to mark for cancellation.
**/
void SVG_Nav2_QueryJob_RequestCancel( nav2_query_job_t *job );

/**
*	@brief	Request a stage-boundary restart for a nav2 job.
*	@param	job	Job to mark for restart.
**/
void SVG_Nav2_QueryJob_RequestRestart( nav2_query_job_t *job );

/**
*	@brief	Return whether the job is in a terminal state.
*	@param	job	Job to inspect.
*	@return	True when the underlying query state is terminal.
**/
const bool SVG_Nav2_QueryJob_IsTerminal( const nav2_query_job_t *job );
