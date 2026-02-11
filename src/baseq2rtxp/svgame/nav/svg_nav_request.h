/********************************************************************
*
*
*    SVGame: Navigation Request Queue
*
*
********************************************************************/
#pragma once

#include "svgame/svg_local.h"
#include "svgame/nav/svg_nav_path_process.h"
#include "svgame/nav/svg_nav_traversal_async.h"

//! Toggle controlling whether path rebuilds should be enqueued via the async request queue.
extern cvar_t *nav_nav_async_queue_mode;

/**
*    @brief    Handle for an async navigation request.
*    @note     Returned values start at 1 and increment per queue entry.
**/
using nav_request_handle_t = int32_t;

/**
*    @brief    Status of an outstanding navigation request.
**/
enum class nav_request_status_t {
	Queued,
	Running,
	Completed,
	Failed,
	Cancelled,
};

/**
*    @brief    Entry describing a queued navigation task.
**/
struct nav_request_entry_t {
	svg_nav_path_process_t *path_process = nullptr;
	Vector3 start = {};
	Vector3 goal = {};
  svg_nav_path_policy_t policy = {};
	svg_nav_path_policy_t resolved_policy = {};
	Vector3 agent_mins = {};
	Vector3 agent_maxs = {};
	nav_request_status_t status = nav_request_status_t::Queued;
   nav_a_star_state_t a_star = {};
	bool force = false;
	nav_request_handle_t handle = 0;
};

/**
*    @brief    Async navigation request queue configuration reference.
*    @note     Controlled by three cvars:
*                nav_nav_async_enable          - Master toggle for async queue execution.
*                nav_requests_per_frame        - Maximum queued entries processed per frame.
*                nav_expansions_per_request    - Expansion budget used while stepping each request.
**/

/**
*    @brief    Enqueue an asynchronous path rebuild request.
*    @param    pathProcess    Owner path-process state requesting work.
*    @param    start_origin   Current feet-origin start position.
*    @param    goal_origin    Goal feet-origin position.
*    @param    policy         Path-follow policy for traversal tuning.
*    @param    agent_mins     Feet-origin agent mins for bbox validation.
*    @param    agent_maxs     Feet-origin agent maxs for bbox validation.
*    @param    force          If true, bypass throttling heuristics (requires caution).
*    @return   Handle representing this request (0 = invalid).
**/
nav_request_handle_t SVG_Nav_RequestPathAsync( svg_nav_path_process_t *pathProcess, const Vector3 &start_origin,
	const Vector3 &goal_origin, const svg_nav_path_policy_t &policy, const Vector3 &agent_mins,
	const Vector3 &agent_maxs, bool force = false );

/**
*    @brief    Cancel a pending navigation request.
*    @param    handle    Handle returned by SVG_Nav_RequestPathAsync.
**/
void SVG_Nav_CancelRequest( nav_request_handle_t handle );

/**
*    @brief    Query whether the given path process currently owns a pending request.
**/
bool SVG_Nav_IsRequestPending( const svg_nav_path_process_t *pathProcess );

/**
*    @brief    Drive the async request queue once per frame.
*    @note     This should be invoked from the main nav tick so work can be throttled.
**/
void SVG_Nav_ProcessRequestQueue( void );

/**
*    @brief    Register the cvars that control async request queue execution and diagnostics.
*    @note     Call from SVG_Nav_Init to ensure the cvars exist before use.
**/
void SVG_Nav_RequestQueue_RegisterCvars( void );

/**
*    @brief    Query whether async navigation requests are enabled via cvar.
*    @return   True when nav_nav_async_enable is non-zero and registration succeeded.
**/
bool SVG_Nav_IsAsyncNavEnabled( void );

/**
*    @brief    Retrieve the configured per-frame request budget driven by nav_requests_per_frame.
*    @return   At least one request budget value (clamped to 1).
**/
int32_t SVG_Nav_GetAsyncRequestBudget( void );

/**
*    @brief    Retrieve the configured per-request expansion budget (nav_expansions_per_request).
*    @return   At least one expansion unit (clamped to 1).
**/
int32_t SVG_Nav_GetAsyncRequestExpansions( void );
