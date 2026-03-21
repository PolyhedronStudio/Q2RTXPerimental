/********************************************************************
*
*
*	ServerGame: Nav2 Scheduler Foundation - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_scheduler.h"

#include "svgame/nav2/nav2_worker_iface.h"

#include <algorithm>
#include <limits>


/**
*
*
*	Nav2 Scheduler Global State:
*
*
**/
//! RAII owner for the scheduler foundation runtime.
static nav2_scheduler_runtime_owner_t s_nav2_scheduler_runtime = {};


/**
*
*
*	Nav2 Scheduler Internal Helpers:
*
*
**/
/**
*	@brief	Resolve a comparable integer weight for a priority class.
*	@param	priority	Priority class to convert.
*	@return	Higher values represent more urgent scheduling priority.
**/
static int32_t Nav2_Scheduler_PriorityWeight( const nav2_query_priority_t priority ) {
    switch ( priority ) {
    case nav2_query_priority_t::Low: return 0;
    case nav2_query_priority_t::Normal: return 1;
    case nav2_query_priority_t::High: return 2;
    case nav2_query_priority_t::Critical: return 3;
    case nav2_query_priority_t::Count: break;
    default: break;
    }
    return 0;
}

/**
*	@brief	Return whether a job is eligible to receive another scheduler slice.
*	@param	job	Job to inspect.
*	@return	True when the job is active, non-terminal, and not already cancelled.
**/
static const bool Nav2_Scheduler_IsSliceEligible( const nav2_query_job_t &job ) {
    /**
    *    Exclude jobs that already reached a terminal result state.
    **/
    if ( SVG_Nav2_QueryJob_IsTerminal( &job ) ) {
        return false;
    }

    /**
    *    Exclude jobs that have an outstanding explicit cancel request.
    **/
    if ( job.state.cancel_requested ) {
        return false;
    }

    /**
    *    Exclude jobs that already reached the completed pseudo-stage even if their terminal result
    *    has not yet been published by a future integration layer.
    **/
    if ( job.state.stage == nav2_query_stage_t::Completed ) {
        return false;
    }

    return true;
}

/**
*	@brief	Choose the next job index that should receive a scheduler slice.
*	@param	runtime	Scheduler runtime containing queued jobs.
*	@return	Index of the best eligible job, or -1 when no job can receive work.
*	@note	The current tie-break is deterministic: higher priority first, then older lower job id.
**/
static int32_t Nav2_Scheduler_SelectNextJobIndex( const nav2_scheduler_runtime_t &runtime ) {
    int32_t bestIndex = -1;
    int32_t bestPriorityWeight = std::numeric_limits<int32_t>::min();
    uint64_t bestJobId = std::numeric_limits<uint64_t>::max();

    /**
    *    Scan the active job list and select the highest-priority eligible job using a stable tie-break.
    **/
    for ( size_t i = 0; i < runtime.jobs.size(); i++ ) {
        const nav2_query_job_t &job = runtime.jobs[ i ];
        if ( !Nav2_Scheduler_IsSliceEligible( job ) ) {
            continue;
        }

        const int32_t priorityWeight = Nav2_Scheduler_PriorityWeight( job.request.priority );
        if ( priorityWeight > bestPriorityWeight ) {
            bestPriorityWeight = priorityWeight;
            bestJobId = job.job_id;
            bestIndex = ( int32_t )i;
            continue;
        }

        if ( priorityWeight == bestPriorityWeight && job.job_id < bestJobId ) {
            bestJobId = job.job_id;
            bestIndex = ( int32_t )i;
        }
    }

    return bestIndex;
}


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
const bool SVG_Nav2_Scheduler_Init( void ) {
    /**
    *    Reuse the existing runtime if the scheduler foundation was already initialized.
    **/
    if ( s_nav2_scheduler_runtime ) {
        return true;
    }

    /**
    *    Allocate the runtime using svgame-level tagged ownership so the scheduler foundation fits
    *    the existing memory model from the beginning.
    **/
    if ( !s_nav2_scheduler_runtime.create( TAG_SVGAME ) ) {
        return false;
    }

    /**
    *    Seed deterministic budget and snapshot defaults inside the newly allocated runtime.
    **/
    SVG_Nav2_Snapshot_ResetRuntime( &s_nav2_scheduler_runtime->snapshot_runtime );
    SVG_Nav2_Budget_ResetFrame( &s_nav2_scheduler_runtime->budget_runtime, 0 );
    return true;
}

/**
*	@brief	Shutdown the nav2 scheduler foundation runtime.
*	@note	This releases all scheduler-owned jobs and resets the runtime RAII owner.
**/
void SVG_Nav2_Scheduler_Shutdown( void ) {
    /**
    *    Reset the runtime owner so all scheduler-owned state is destroyed through the established
    *    svgame RAII path.
    **/
    s_nav2_scheduler_runtime.reset();
}

/**
*	@brief	Return whether the scheduler foundation runtime is currently initialized.
*	@return	True when the scheduler runtime exists.
**/
const bool SVG_Nav2_Scheduler_IsInitialized( void ) {
    return ( bool )s_nav2_scheduler_runtime;
}

/**
*	@brief	Return a mutable pointer to the scheduler runtime.
*	@return	Scheduler runtime pointer, or `nullptr` when uninitialized.
**/
nav2_scheduler_runtime_t *SVG_Nav2_Scheduler_GetRuntime( void ) {
    return s_nav2_scheduler_runtime.get();
}

/**
*	@brief	Begin a new scheduler frame and reset budget accounting.
*	@param	frameNumber	Frame number being entered.
**/
void SVG_Nav2_Scheduler_BeginFrame( const int64_t frameNumber ) {
    /**
    *    Skip frame reset work when the scheduler foundation has not been initialized yet.
    **/
    if ( !s_nav2_scheduler_runtime ) {
        return;
    }

    /**
    *    Reset the frame-level budget accounting while preserving the current policy defaults.
    **/
    SVG_Nav2_Budget_ResetFrame( &s_nav2_scheduler_runtime->budget_runtime, frameNumber, &s_nav2_scheduler_runtime->budget_runtime.policy );
   /**
    *    Keep the worker interface aligned with scheduler frame boundaries so cvar-driven mode changes
    *    are observed deterministically.
    **/
    SVG_Nav2_Worker_BeginFrame( frameNumber );
}

/**
*	@brief	Service the scheduler foundation from existing svgame frame hooks.
*	@note	This applies worker completions first and then attempts one early or late bounded slice.
**/
void SVG_Nav2_Scheduler_Service( void ) {
    /**
    *    Skip service work when the scheduler runtime has not been initialized yet.
    **/
    if ( !s_nav2_scheduler_runtime ) {
        return;
    }

    /**
    *    Apply completed worker payloads first so jobs become dispatchable again before selecting the
    *    next bounded slice.
    **/
    SVG_Nav2_Worker_ServiceCompletions();

    /**
    *    Ask the scheduler foundation for one worker-oriented slice. The worker interface will fall
    *    back to inline execution automatically if async dispatch is disabled.
    **/
    if ( !SVG_Nav2_Scheduler_IssueSlice( true ) ) {
        return;
    }

    /**
    *    Find the job that just received a slice by scanning for the highest-priority eligible job with
    *    an active granted slice and no worker already in flight.
    **/
    for ( nav2_query_job_t &job : s_nav2_scheduler_runtime->jobs ) {
        if ( job.worker_in_flight ) {
            continue;
        }
        if ( job.state.active_slice.granted_ms <= 0.0 ) {
            continue;
        }

        /**
        *    Record the queue-enter timestamp lazily so queue-wait timing starts once the job first
        *    becomes dispatchable.
        **/
        if ( job.queue_enter_timestamp_ms == 0 ) {
            job.queue_enter_timestamp_ms = gi.GetRealTime ? gi.GetRealTime() : 0;
        }

        // Dispatch exactly one granted slice through the worker interface.
        SVG_Nav2_Worker_DispatchJobSlice( &job );
        break;
    }
}

/**
*	@brief	Submit a new nav2 query request into the scheduler queue.
*	@param	request	Request descriptor to enqueue.
*	@return	Assigned job identifier, or 0 on failure.
**/
uint64_t SVG_Nav2_Scheduler_SubmitRequest( const nav2_query_request_t &request ) {
    /**
    *    Ensure the scheduler runtime exists before accepting new work.
    **/
    if ( !SVG_Nav2_Scheduler_Init() ) {
        return 0;
    }

    /**
    *    Copy the request, assign a stable request identifier when the caller did not provide one,
    *    and then create the scheduler-owned job wrapper.
    **/
    nav2_query_request_t materializedRequest = request;
    if ( materializedRequest.request_id == 0 ) {
        materializedRequest.request_id = s_nav2_scheduler_runtime->next_request_id++;
    }
    const uint64_t jobId = s_nav2_scheduler_runtime->next_job_id++;

    /**
    *    Append a new default-initialized job slot and then initialize it deterministically.
    **/
    s_nav2_scheduler_runtime->jobs.emplace_back();
    SVG_Nav2_QueryJob_Init( &s_nav2_scheduler_runtime->jobs.back(), materializedRequest, jobId );
    return jobId;
}

/**
*	@brief	Issue one deterministic scheduler slice to the best currently eligible job.
*	@param	workerSlice	True when selecting against worker-slice capacity, false for main-thread capacity.
*	@return	True when a job received a slice.
*	@note	The current foundation only grants and records slices; future tasks will execute real solver work.
**/
const bool SVG_Nav2_Scheduler_IssueSlice( const bool workerSlice ) {
    /**
    *    Ensure the scheduler runtime exists before attempting to grant slices.
    **/
    if ( !s_nav2_scheduler_runtime ) {
        return false;
    }

    /**
    *    Choose the best currently eligible job using the deterministic priority and age ordering.
    **/
    const int32_t jobIndex = Nav2_Scheduler_SelectNextJobIndex( *s_nav2_scheduler_runtime );
    if ( jobIndex < 0 ) {
        return false;
    }

    /**
    *    Attempt to grant a bounded slice from the current frame budget.
    **/
    nav2_query_job_t &job = s_nav2_scheduler_runtime->jobs[ ( size_t )jobIndex ];
    nav2_budget_slice_t grantedSlice = {};
    if ( !SVG_Nav2_Budget_GrantSlice( &s_nav2_scheduler_runtime->budget_runtime, job.state.stage, job.request.priority, workerSlice, &grantedSlice ) ) {
        return false;
    }

    /**
    *    Record the granted slice on the job and bump generic progress counters so the scheduler
    *    foundation already exposes deterministic slice accounting.
    **/
    job.state.active_slice = grantedSlice;
    job.state.progress.slices_consumed++;
    job.execution_time_ms += grantedSlice.granted_ms;

    /**
    *    Commit the granted slice directly into frame consumption for the foundation layer. Future
    *    worker integration can replace this with measured actual consumption.
    **/
    SVG_Nav2_Budget_CommitSliceConsumption( &s_nav2_scheduler_runtime->budget_runtime, grantedSlice, grantedSlice.granted_ms );
    return true;
}

/**
*	@brief	Mark a job for cancellation by identifier.
*	@param	jobId	Stable job identifier to cancel.
*	@return	True when a matching job was found and updated.
**/
const bool SVG_Nav2_Scheduler_CancelJob( const uint64_t jobId ) {
    /**
    *    Resolve the job first so cancellation remains a simple point update.
    **/
    nav2_query_job_t *job = SVG_Nav2_Scheduler_FindJob( jobId );
    if ( !job ) {
        return false;
    }

    // Forward the cancellation request into the shared job helper.
    SVG_Nav2_QueryJob_RequestCancel( job );
    return true;
}

/**
*	@brief	Retrieve a job by identifier.
*	@param	jobId	Stable job identifier to resolve.
*	@return	Mutable pointer to the job, or `nullptr` when not found.
**/
nav2_query_job_t *SVG_Nav2_Scheduler_FindJob( const uint64_t jobId ) {
    /**
    *    Skip lookup work when the scheduler runtime does not exist yet.
    **/
    if ( !s_nav2_scheduler_runtime ) {
        return nullptr;
    }

    /**
    *    Scan the active job vector for the requested stable identifier.
    **/
    for ( nav2_query_job_t &job : s_nav2_scheduler_runtime->jobs ) {
        if ( job.job_id == jobId ) {
            return &job;
        }
    }
    return nullptr;
}
