/********************************************************************
*
*
*	ServerGame: Nav2 Query Job - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_query_job.h"


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
void SVG_Nav2_QueryJob_Init( nav2_query_job_t *job, const nav2_query_request_t &request, const uint64_t jobId ) {
    /**
    *    Sanity-check the destination job before assigning any request or scheduler state.
    **/
    if ( !job ) {
        return;
    }

    /**
    *    Reset the full job object so reused storage starts from deterministic defaults.
    **/
    *job = nav2_query_job_t{};

    /**
    *    Copy the stable request descriptor and initialize the embedded resumable query state.
    **/
    job->job_id = jobId;
    job->request = request;
    SVG_Nav2_QueryState_Reset( &job->state );
    job->state.bench_scenario = request.bench_scenario;
}

/**
*	@brief	Request cancellation for a nav2 job.
*	@param	job	Job to mark for cancellation.
**/
void SVG_Nav2_QueryJob_RequestCancel( nav2_query_job_t *job ) {
    /**
    *    Ignore cancellation requests when no job object is available.
    **/
    if ( !job ) {
        return;
    }

    // Mark the embedded query state for cancellation.
    job->state.cancel_requested = true;
    // Publish the terminal cancelled result status immediately for the foundation layer.
    job->state.result_status = nav2_query_result_status_t::Cancelled;
}

/**
*	@brief	Request a stage-boundary restart for a nav2 job.
*	@param	job	Job to mark for restart.
**/
void SVG_Nav2_QueryJob_RequestRestart( nav2_query_job_t *job ) {
    /**
    *    Ignore restart requests when no job object is available.
    **/
    if ( !job ) {
        return;
    }

    // Mark the embedded query state for a stage-boundary restart.
    job->state.restart_requested = true;
    // Increment the restart counter for scheduler diagnostics.
    job->restart_count++;
}

/**
*	@brief	Return whether the job is in a terminal state.
*	@param	job	Job to inspect.
*	@return	True when the underlying query state is terminal.
**/
const bool SVG_Nav2_QueryJob_IsTerminal( const nav2_query_job_t *job ) {
    /**
    *    Treat missing jobs as non-terminal so callers do not accidentally finalize nullptr jobs.
    **/
    if ( !job ) {
        return false;
    }

    // Delegate the terminal-state check to the shared query-state helper.
    return SVG_Nav2_QueryState_IsTerminal( &job->state );
}
