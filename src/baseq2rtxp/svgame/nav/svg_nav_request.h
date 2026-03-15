/** * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 
* 
* 
*  SVGame: Navigation Request Queue
* 
* 
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
#pragma once

#include <cstdint>
#include "svgame/nav/svg_nav_path_process.h"
#include "svgame/nav/svg_nav_traversal_async.h"
#include "svgame/svg_local.h"

//! Toggle controlling whether path rebuilds should be enqueued via the async request queue.
extern cvar_t *nav_nav_async_queue_mode;

/** 
*  @brief    Handle for an async navigation request.
*  @note     Returned values start at 1 and increment per queue entry.
**/
using nav_request_handle_t = int32_t;

/** 
*  @brief    Status of an outstanding navigation request.
**/
enum class nav_request_status_t {
	Queued,
    Preparing,
	Running,
	Completed,
	Failed,
	Cancelled,
};

/** 
*  @brief    Entry describing a queued navigation task.
**/
struct nav_request_entry_t {
	//! A unique handle representing this request in the async queue, used for cancellation and diagnostics.
	nav_request_handle_t handle = 0;
	//! Monotonic generation counter from the owning path process used to discard stale async results when requests are replaced before completion.
	uint32_t generation = 0;
	//! When true, this request will bypass throttling heuristics and be processed immediately. 
	//! Use with caution as this can lead to performance hitches if used excessively.
	bool force = false;
	//! Owner path process requesting this navigation work. 
	//! Used for commit/cancellation and to prevent multiple concurrent requests from the same process.
	svg_nav_path_process_t * path_process = nullptr;

	//! Policy for path processing and follow behavior, used to enforce throttles and tuning after async completion.
	svg_nav_path_policy_t policy = {};
	//! The resolved policy after applying any dynamic tuning or overrides at the time of request processing. 
	//! This is what gets applied to the path process when the async result commits so we can ensure the 
	//! async result is applied with the same policy that was used to generate it.
	svg_nav_path_policy_t resolved_policy = {};

	//! When true, LOS simplification/direct shortcut collapse must be skipped so detailed waypoints stay intact.
	bool preserve_detailed_waypoints = false;

	//! Start and goal positions for this request (feet-origin).
	Vector3 start = {};
	//! Goal position (feet-origin).
	Vector3 goal = {};

	//! Agent bbox for this request (feet-origin) used for validating async results and ensuring the nav query is generated with the correct agent size.
	Vector3 agent_mins = {};
	Vector3 agent_maxs = {};

	//! Current status of this request used for diagnostics and to prevent cancellation of already completed/failed requests.
	nav_request_status_t status = nav_request_status_t::Queued;
	//! The A*  state for this request, used to track progress when the request is being processed and for diagnostics.
	nav_a_star_state_t a_star = {};
  //! When true the running entry should be refreshed once the current search finishes.
	//! This avoids repeatedly re-initializing an in-flight search every frame.
	bool needs_refresh = false;
};

/** 
*  @brief    Async navigation request queue configuration reference.
*  @note     Controlled by three cvars:
*              nav_nav_async_enable          - Master toggle for async queue execution.
*              nav_requests_per_frame        - Maximum queued entries processed per frame.
*              nav_expansions_per_request    - Expansion budget used while stepping each request.
**/

/** 
*  @brief    Enqueue an asynchronous path rebuild request.
*  @param    pathProcess    Owner path-process state requesting work.
*  @param    start_origin   Current feet-origin start position.
*  @param    goal_origin    Goal feet-origin position.
*  @param    policy         Path-follow policy for traversal tuning.
*  @param    agent_mins     Feet-origin agent mins for bbox validation.
*  @param    agent_maxs     Feet-origin agent maxs for bbox validation.
*  @param    force          If true, bypass throttling heuristics (requires caution).
*  @return   Handle representing this request (0 = invalid).
**/
nav_request_handle_t SVG_Nav_RequestPathAsync( svg_nav_path_process_t * pathProcess, const Vector3 &start_origin,
	const Vector3 &goal_origin, const svg_nav_path_policy_t &policy, const Vector3 &agent_mins,
	const Vector3 &agent_maxs, bool force = false, double startIgnoreThreshold = 0.0 );

/** 
*  @brief    Cancel a pending navigation request.
*  @param    handle    Handle returned by SVG_Nav_RequestPathAsync.
**/
void SVG_Nav_CancelRequest( nav_request_handle_t handle );

/** 
*  @brief    Query whether the given path process currently owns a pending request.
**/
bool SVG_Nav_IsRequestPending( const svg_nav_path_process_t * pathProcess );

/** 
*  @brief    Drive the async request queue once per frame.
*  @note     This should be invoked from the main nav tick so work can be throttled.
**/
void SVG_Nav_ProcessRequestQueue( void );

/** 
*  @brief    Return whether the async queue should receive a second late-frame service tick.
*  @return   True when the late-frame follow-up tick is enabled.
*  @note     The early-frame tick exposes completed worker results before entity think, while this
*            optional late-frame tick keeps already-running searches advancing within the same frame.
**/
bool SVG_Nav_ShouldRunLateFrameQueueTick( void );

/** 
*  @brief    Register the cvars that control async request queue execution and diagnostics.
*  @note     Call from SVG_Nav_Init to ensure the cvars exist before use.
**/
void SVG_Nav_RequestQueue_RegisterCvars( void );

/** 
*  @brief    Query whether async navigation requests are enabled via cvar.
*  @return   True when nav_nav_async_enable is non-zero and registration succeeded.
**/
bool SVG_Nav_IsAsyncNavEnabled( void );

/** 
*  @brief    Retrieve the configured per-frame request budget driven by nav_requests_per_frame.
*  @return   At least one request budget value (clamped to 1).
**/
int32_t SVG_Nav_GetAsyncRequestBudget( void );

/** 
*  @brief    Retrieve the configured per-request expansion budget (nav_expansions_per_request).
*  @return   At least one expansion unit (clamped to 1).
**/
int32_t SVG_Nav_GetAsyncRequestExpansions( void );
