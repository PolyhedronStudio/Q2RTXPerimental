/********************************************************************
*
*
*	ServerGame: Nav2 Worker Interface
*
*
********************************************************************/
#pragma once

#include "svgame/nav2/nav2_query_job.h"


/**
*
*
*	Nav2 Worker Interface Enumerations:
*
*
**/
/**
*	@brief	Execution mode used by the nav2 worker interface.
*	@note	The scheduler can switch between true async dispatch and deterministic fallback stepping
*			without changing the job or scheduler public contracts.
**/
enum class nav2_worker_mode_t : uint8_t {
    Disabled = 0,
    InlineMainThread,
    AsyncQueue,
    Count
};


/**
*
*
*	Nav2 Worker Interface Data Structures:
*
*
**/
/**
*	@brief	Mutable runtime state for the nav2 worker integration layer.
*	@note	This stores only scheduler-facing dispatch bookkeeping; worker-owned payloads remain
*			transient and are rebuilt per dispatch so no unsafe pointer graphs are persisted.
**/
struct nav2_worker_runtime_t {
    //! Configured worker execution mode.
    nav2_worker_mode_t mode = nav2_worker_mode_t::Disabled;
    //! Whether the runtime has completed one-time cvar registration.
    bool cvars_registered = false;
    //! Number of worker payloads currently in flight.
    uint32_t in_flight_count = 0;
    //! Number of completed worker payloads waiting for main-thread application.
    uint32_t pending_completion_count = 0;
};


/**
*
*
*	Nav2 Worker Interface Public API:
*
*
**/
/**
*	@brief	Register the nav2 worker-interface cvars.
*	@note	This should be called during svgame initialization before scheduling worker slices.
**/
void SVG_Nav2_Worker_RegisterCvars( void );

/**
*	@brief	Initialize the nav2 worker runtime.
*	@return	True when the worker runtime is ready for dispatch and completion service.
**/
const bool SVG_Nav2_Worker_Init( void );

/**
*	@brief	Shutdown the nav2 worker runtime and drop transient dispatch bookkeeping.
**/
void SVG_Nav2_Worker_Shutdown( void );

/**
*	@brief	Return the configured worker execution mode.
*	@return	Active worker execution mode.
**/
nav2_worker_mode_t SVG_Nav2_Worker_GetMode( void );

/**
*	@brief	Begin worker-interface bookkeeping for a new scheduler frame.
*	@param	frameNumber	Frame number being entered.
*	@note	This currently exists to keep the worker layer aligned with scheduler frame boundaries.
**/
void SVG_Nav2_Worker_BeginFrame( const int64_t frameNumber );

/**
*	@brief	Dispatch one scheduler-owned job slice through the configured worker execution path.
*	@param	job	Scheduler-owned job to dispatch.
*	@return	True when the job was dispatched or executed successfully.
*	@note	The current Task 2.2 foundation performs bounded worker bookkeeping and mock slice execution;
*			later tasks will replace the placeholder execution body with real solver stages.
**/
const bool SVG_Nav2_Worker_DispatchJobSlice( nav2_query_job_t *job );

/**
*	@brief	Apply completed worker payloads on the main thread.
*	@note	Call this from existing svgame frame hooks after the engine has completed async work callbacks.
**/
void SVG_Nav2_Worker_ServiceCompletions( void );
