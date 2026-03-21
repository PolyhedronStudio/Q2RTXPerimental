/********************************************************************
*
*
*	ServerGame: Nav2 Scheduler Foundation
*
*
********************************************************************/
#pragma once

#include <vector>

#include "svgame/nav2/nav2_query_job.h"
#include "svgame/memory/svg_raiiobject.hpp"


/**
*
*
*	Nav2 Scheduler Data Structures:
*
*
**/
/**
*	@brief	Runtime scheduler state owning the foundation queue, budget, and snapshot publication models.
*	@note	The queue currently lives on the main thread; later worker integration can build atop this
*			state without changing the public scheduler-facing API.
**/
struct nav2_scheduler_runtime_t {
    //! Next stable request identifier to issue.
    uint64_t next_request_id = 1;
    //! Next stable job identifier to issue.
    uint64_t next_job_id = 1;
    //! Snapshot publication runtime used for worker-safe version binding.
    nav2_snapshot_runtime_t snapshot_runtime = {};
    //! Frame-level budget accounting runtime.
    nav2_budget_runtime_t budget_runtime = {};
    //! Active scheduler-owned query jobs.
    std::vector<nav2_query_job_t> jobs = {};
};

//! RAII owner for the nav2 scheduler runtime foundation.
using nav2_scheduler_runtime_owner_t = SVG_RAIIObject<nav2_scheduler_runtime_t>;


/**
*
*
*	Nav2 Scheduler Public API:
*
*
**/
/**
*	@brief	Initialize the nav2 scheduler foundation runtime.
*	@return	True when the scheduler runtime was created successfully.
*	@note	This allocates the scheduler runtime using svgame tagged ownership conventions.
**/
const bool SVG_Nav2_Scheduler_Init( void );

/**
*	@brief	Shutdown the nav2 scheduler foundation runtime.
*	@note	This releases all scheduler-owned jobs and resets the runtime RAII owner.
**/
void SVG_Nav2_Scheduler_Shutdown( void );

/**
*	@brief	Return whether the scheduler foundation runtime is currently initialized.
*	@return	True when the scheduler runtime exists.
**/
const bool SVG_Nav2_Scheduler_IsInitialized( void );

/**
*	@brief	Return a mutable pointer to the scheduler runtime.
*	@return	Scheduler runtime pointer, or `nullptr` when uninitialized.
**/
nav2_scheduler_runtime_t *SVG_Nav2_Scheduler_GetRuntime( void );

/**
*	@brief	Begin a new scheduler frame and reset budget accounting.
*	@param	frameNumber	Frame number being entered.
**/
void SVG_Nav2_Scheduler_BeginFrame( const int64_t frameNumber );

/**
*	@brief	Service the scheduler foundation from existing svgame frame hooks.
*	@note	This applies worker completions first and then attempts one early or late bounded slice.
**/
void SVG_Nav2_Scheduler_Service( void );

/**
*	@brief	Submit a new nav2 query request into the scheduler queue.
*	@param	request	Request descriptor to enqueue.
*	@return	Assigned job identifier, or 0 on failure.
**/
uint64_t SVG_Nav2_Scheduler_SubmitRequest( const nav2_query_request_t &request );

/**
*	@brief	Issue one deterministic scheduler slice to the best currently eligible job.
*	@param	workerSlice	True when selecting against worker-slice capacity, false for main-thread capacity.
*	@return	True when a job received a slice.
*	@note	The current foundation only grants and records slices; future tasks will execute real solver work.
**/
const bool SVG_Nav2_Scheduler_IssueSlice( const bool workerSlice );

/**
*	@brief	Mark a job for cancellation by identifier.
*	@param	jobId	Stable job identifier to cancel.
*	@return	True when a matching job was found and updated.
**/
const bool SVG_Nav2_Scheduler_CancelJob( const uint64_t jobId );

/**
*	@brief	Retrieve a job by identifier.
*	@param	jobId	Stable job identifier to resolve.
*	@return	Mutable pointer to the job, or `nullptr` when not found.
**/
nav2_query_job_t *SVG_Nav2_Scheduler_FindJob( const uint64_t jobId );
