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
#include "svgame/nav2/nav2_connectors.h"
#include "svgame/nav2/nav2_hierarchy_graph.h"
#include "svgame/nav2/nav2_region_layers.h"
#include "svgame/nav2/nav2_span_grid.h"
#include "svgame/nav2/nav2_topology.h"
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
  //! Number of starvation-prevention boosts issued by fairness selection.
    uint64_t starvation_prevention_boost_count = 0;
    //! Number of times fairness selection observed an unfair queue-delay condition.
    uint64_t unfair_delay_event_count = 0;
    //! Number of times overload policy throttled a granted slice.
    uint64_t overload_throttle_count = 0;
    //! Number of times overload policy requested low-fidelity/provisional fallback behavior.
    uint64_t overload_provisional_fallback_count = 0;
    //! Number of stage transitions executed by scheduler-controlled jobs.
    uint64_t stage_transition_count = 0;
    //! Number of stage-boundary restart requests observed by scheduler-controlled jobs.
    uint64_t stage_restart_count = 0;
    //! Snapshot version bound to cached hierarchy dependencies; 0 means cache is uninitialized.
    uint32_t cached_hierarchy_static_nav_version = 0;
    //! Cached topology artifact reused by later scheduler stages while snapshot version is stable.
    nav2_topology_artifact_t cached_topology_artifact = {};
    //! Cached topology summary reused for bounded diagnostics while snapshot version is stable.
    nav2_topology_summary_t cached_topology_summary = {};
    //! True when cached topology publication is valid for the cached snapshot version.
    bool has_cached_topology_artifact = false;
    //! Cached span-grid snapshot reused by hierarchy dependency build while snapshot version is stable.
    nav2_span_grid_t cached_hierarchy_span_grid = {};
    //! Cached connector extraction reused by hierarchy dependency build while snapshot version is stable.
    nav2_connector_list_t cached_hierarchy_connectors = {};
    //! Cached region-layer graph reused by hierarchy dependency build while snapshot version is stable.
    nav2_region_layer_graph_t cached_hierarchy_region_layers = {};
    //! Cached hierarchy graph reused by coarse stage while snapshot version is stable.
    nav2_hierarchy_graph_t cached_hierarchy_graph = {};
    //! True when cached hierarchy dependencies are valid for the cached snapshot version.
    bool has_cached_hierarchy_dependencies = false;
    //! True when cached hierarchy dependencies were built from the connector-less fallback policy.
    bool cached_hierarchy_is_no_connector_fallback = false;
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
*\t@brief\tExecute the currently granted slice for one scheduler-owned job.
*\t@param\tjob\tScheduler-owned job receiving stage execution for its active slice.
*\t@return\tTrue when the stage executed and updated job state.
*\t@note\tThis is used by both main-thread fallback dispatch and worker completion handoff.
**/
const bool SVG_Nav2_Scheduler_ExecuteGrantedSlice( nav2_query_job_t *job );

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

/**
*	@brief	Erase one terminal or otherwise unneeded job from the scheduler runtime.
*	@param	jobId	Stable job identifier to erase.
*	@return	True when a matching job was found and removed.
*	@note	This also clears any scheduler-local staged runtime artifacts associated with the job id.
**/
const bool SVG_Nav2_Scheduler_RemoveJob( const uint64_t jobId );
