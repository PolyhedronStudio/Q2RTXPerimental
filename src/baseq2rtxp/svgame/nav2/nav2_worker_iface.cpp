/********************************************************************
*
*
*	ServerGame: Nav2 Worker Interface - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_worker_iface.h"

#include "svgame/nav2/nav2_scheduler.h"

#include <mutex>
#include <vector>


/**
*
*
*	Nav2 Worker Interface Internal Data:
*
*
**/
/**
*	@brief	Transient payload handed to the engine async queue for one nav2 job slice.
*	@note	The payload stores only stable identifiers and copied timing data so worker dispatch does
*			not depend on mutable live svgame state.
**/
struct nav2_worker_payload_t {
    //! Stable scheduler job identifier this payload belongs to.
    uint64_t job_id = 0;
    //! Snapshot of the granted slice being executed.
    nav2_budget_slice_t slice = {};
    //! Timestamp captured when the job entered the dispatch queue.
    uint64_t queue_enter_timestamp_ms = 0;
    //! Timestamp captured when async dispatch was submitted.
    uint64_t dispatch_timestamp_ms = 0;
    //! Measured worker queue wait time for this payload.
    double queue_wait_ms = 0.0;
    //! Measured execution time for this payload.
    double execution_ms = 0.0;
    //! True when the placeholder slice execution finished successfully.
    bool completed = false;
};

//! Runtime state for the nav2 worker interface.
static nav2_worker_runtime_t s_nav2_worker_runtime = {};
//! Cvar controlling whether the nav2 worker interface is enabled.
static cvar_t *s_nav2_worker_enable = nullptr;
//! Cvar controlling whether scheduler slices should use the async queue or inline fallback execution.
static cvar_t *s_nav2_worker_async = nullptr;
//! Mutex protecting the completed-payload handoff list.
static std::mutex s_nav2_worker_completion_mutex;
//! Completed worker payloads waiting for main-thread application.
static std::vector<nav2_worker_payload_t *> s_nav2_worker_completed_payloads = {};


/**
*
*
*	Nav2 Worker Interface Internal Helpers:
*
*
**/
/**
*	@brief	Return the current monotonic engine realtime in milliseconds.
*	@return	Current engine realtime, or zero when unavailable.
**/
static uint64_t Nav2_Worker_GetNowMs( void ) {
    /**
    *    Use the engine realtime import when available so queue and execution measurements line up
    *    with the benchmark harness.
    **/
    if ( gi.GetRealTime ) {
        return gi.GetRealTime();
    }
    return 0;
}

/**
*	@brief	Return the configured worker execution mode from the current cvars.
*	@return	Resolved worker execution mode.
**/
static nav2_worker_mode_t Nav2_Worker_ResolveModeFromCvars( void ) {
    /**
    *    Treat a disabled worker cvar as a hard off switch.
    **/
    if ( !s_nav2_worker_enable || s_nav2_worker_enable->integer == 0 ) {
        return nav2_worker_mode_t::Disabled;
    }

    /**
    *    Choose between async queue dispatch and deterministic inline fallback execution.
    **/
    if ( s_nav2_worker_async && s_nav2_worker_async->integer != 0 && gi.Com_QueueAsyncWork ) {
        return nav2_worker_mode_t::AsyncQueue;
    }
    return nav2_worker_mode_t::InlineMainThread;
}

/**
*	@brief	Enqueue a completed payload for later main-thread application.
*	@param	payload	Completed worker payload to hand off.
**/
static void Nav2_Worker_EnqueueCompletedPayload( nav2_worker_payload_t *payload ) {
    /**
    *    Ignore null payloads so callers can simplify their cleanup logic.
    **/
    if ( !payload ) {
        return;
    }

    /**
    *    Push the completed payload into the protected handoff list so the main thread can apply it
    *    during the next explicit service pass.
    **/
    std::lock_guard<std::mutex> lock( s_nav2_worker_completion_mutex );
    s_nav2_worker_completed_payloads.push_back( payload );
    s_nav2_worker_runtime.pending_completion_count = ( uint32_t )s_nav2_worker_completed_payloads.size();
}

/**
*	@brief	Perform placeholder bounded worker execution for one payload.
*	@param	payload	Payload being executed.
*	@note	This intentionally performs only timing and bookkeeping for Task 2.2; real solver work is added later.
**/
static void Nav2_Worker_ExecutePayload( nav2_worker_payload_t *payload ) {
    /**
    *    Guard against invalid payload dispatches before reading timing fields.
    **/
    if ( !payload ) {
        return;
    }

    /**
    *    Measure worker queue wait using the captured dispatch timestamp and the current worker entry time.
    **/
    const uint64_t beginMs = Nav2_Worker_GetNowMs();
    payload->queue_wait_ms = ( beginMs >= payload->dispatch_timestamp_ms ) ? ( double )( beginMs - payload->dispatch_timestamp_ms ) : 0.0;

    /**
    *    Simulate bounded slice execution by honoring the granted slice budget as the accounting unit.
    *    The real solver stages will later replace this placeholder with actual work.
    **/
    payload->execution_ms = payload->slice.granted_ms;
    payload->completed = true;
}

/**
*	@brief	Worker-thread callback used by the shared async queue.
*	@param	cbArg	Payload pointer cast from the async queue callback argument.
**/
static void Nav2_Worker_AsyncWorkCallback( void *cbArg ) {
    /**
    *    Cast the callback argument back to the transient worker payload and execute the bounded slice.
    **/
    nav2_worker_payload_t *payload = static_cast<nav2_worker_payload_t *>( cbArg );
    Nav2_Worker_ExecutePayload( payload );
}

/**
*	@brief	Done callback used by the shared async queue to hand completed payloads back to the main thread.
*	@param	cbArg	Payload pointer cast from the async queue callback argument.
**/
static void Nav2_Worker_AsyncDoneCallback( void *cbArg ) {
    /**
    *    Queue the completed payload for main-thread application rather than touching scheduler state
    *    directly from the engine callback path.
    **/
    nav2_worker_payload_t *payload = static_cast<nav2_worker_payload_t *>( cbArg );
    Nav2_Worker_EnqueueCompletedPayload( payload );
}

/**
*	@brief	Apply one completed worker payload back onto the owning scheduler job.
*	@param	payload	Completed payload to apply.
**/
static void Nav2_Worker_ApplyCompletedPayload( nav2_worker_payload_t *payload ) {
    /**
    *    Guard against null payloads before attempting scheduler lookup.
    **/
    if ( !payload ) {
        return;
    }

    /**
    *    Resolve the owning scheduler job by stable job identifier.
    **/
    nav2_query_job_t *job = SVG_Nav2_Scheduler_FindJob( payload->job_id );
    if ( !job ) {
        delete payload;
        return;
    }

    /**
    *    Record queue-wait and execution timing into both the job and benchmark query state.
    **/
    job->queue_wait_ms += payload->queue_wait_ms;
    job->execution_time_ms += payload->execution_ms;
    job->worker_in_flight = false;
    job->last_dispatch_timestamp_ms = 0;
    job->queue_enter_timestamp_ms = Nav2_Worker_GetNowMs();
    SVG_Nav2_Bench_AddDuration( &job->bench_query, nav2_bench_timing_bucket_t::WorkerQueueWait, payload->queue_wait_ms );
    SVG_Nav2_Bench_AddDuration( &job->bench_query, nav2_bench_timing_bucket_t::WorkerExecutionSlice, payload->execution_ms );

    /**
    *    Clear the granted slice once the placeholder worker execution has completed.
    **/
    job->state.active_slice = nav2_budget_slice_t{};

    /**
    *    Decrement the in-flight counter conservatively after the job has been updated.
    **/
    if ( s_nav2_worker_runtime.in_flight_count > 0 ) {
        s_nav2_worker_runtime.in_flight_count--;
    }

    /**
    *    Release the transient payload after application.
    **/
    delete payload;
}


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
void SVG_Nav2_Worker_RegisterCvars( void ) {
    /**
    *    Register the worker cvars only once so repeated init paths stay idempotent.
    **/
    if ( s_nav2_worker_runtime.cvars_registered ) {
        return;
    }

    /**
    *    Register the master worker enable switch.
    **/
    s_nav2_worker_enable = gi.cvar( "nav2_worker_enable", "1", 0 );
    /**
    *    Register the async-dispatch switch so developers can force deterministic inline execution.
    **/
    s_nav2_worker_async = gi.cvar( "nav2_worker_async", "1", 0 );
    s_nav2_worker_runtime.cvars_registered = true;
}

/**
*	@brief	Initialize the nav2 worker runtime.
*	@return	True when the worker runtime is ready for dispatch and completion service.
**/
const bool SVG_Nav2_Worker_Init( void ) {
    /**
    *    Ensure the cvars are registered before the mode is resolved.
    **/
    SVG_Nav2_Worker_RegisterCvars();

    /**
    *    Reset transient runtime counters while preserving cvar registration state.
    **/
    s_nav2_worker_runtime.in_flight_count = 0;
    s_nav2_worker_runtime.pending_completion_count = 0;
    s_nav2_worker_runtime.mode = Nav2_Worker_ResolveModeFromCvars();
    return true;
}

/**
*	@brief	Shutdown the nav2 worker runtime and drop transient dispatch bookkeeping.
**/
void SVG_Nav2_Worker_Shutdown( void ) {
    /**
    *    Apply and release any remaining completed payloads before resetting the runtime.
    **/
    SVG_Nav2_Worker_ServiceCompletions();
    s_nav2_worker_runtime.mode = nav2_worker_mode_t::Disabled;
    s_nav2_worker_runtime.in_flight_count = 0;
    s_nav2_worker_runtime.pending_completion_count = 0;
}

/**
*	@brief	Return the configured worker execution mode.
*	@return	Active worker execution mode.
**/
nav2_worker_mode_t SVG_Nav2_Worker_GetMode( void ) {
    return s_nav2_worker_runtime.mode;
}

/**
*	@brief	Begin worker-interface bookkeeping for a new scheduler frame.
*	@param	frameNumber	Frame number being entered.
*	@note	This currently exists to keep the worker layer aligned with scheduler frame boundaries.
**/
void SVG_Nav2_Worker_BeginFrame( const int64_t frameNumber ) {
    /**
    *    The current worker foundation uses frame boundaries only as an opportunity to refresh mode
    *    configuration and keep deterministic fallback behavior aligned with cvar changes.
    **/
    ( void )frameNumber;
    s_nav2_worker_runtime.mode = Nav2_Worker_ResolveModeFromCvars();
}

/**
*	@brief	Dispatch one scheduler-owned job slice through the configured worker execution path.
*	@param	job	Scheduler-owned job to dispatch.
*	@return	True when the job was dispatched or executed successfully.
*	@note	The current Task 2.2 foundation performs bounded worker bookkeeping and mock slice execution;
*			later tasks will replace the placeholder execution body with real solver stages.
**/
const bool SVG_Nav2_Worker_DispatchJobSlice( nav2_query_job_t *job ) {
    /**
    *    Validate the job and require a granted slice before attempting any worker dispatch.
    **/
    if ( !job ) {
        return false;
    }
    if ( job->worker_in_flight ) {
        return false;
    }
    if ( job->state.active_slice.granted_ms <= 0.0 ) {
        return false;
    }

    /**
    *    Initialize benchmark tracking for the job on first dispatch so queue and worker timing can
    *    accumulate across the full lifecycle.
    **/
    if ( !job->bench_query.active && !job->bench_query.enabled ) {
        SVG_Nav2_Bench_BeginQuery( &job->bench_query, job->request.bench_scenario, nullptr );
    }

    /**
    *    Materialize a transient worker payload that carries only stable identifiers and copied slice
    *    data into the execution path.
    **/
    nav2_worker_payload_t *payload = new nav2_worker_payload_t();
    payload->job_id = job->job_id;
    payload->slice = job->state.active_slice;
    payload->queue_enter_timestamp_ms = job->queue_enter_timestamp_ms;
    payload->dispatch_timestamp_ms = Nav2_Worker_GetNowMs();
    job->last_dispatch_timestamp_ms = payload->dispatch_timestamp_ms;
    job->worker_in_flight = true;
    s_nav2_worker_runtime.in_flight_count++;

    /**
    *    Dispatch through the configured execution mode.
    **/
    switch ( s_nav2_worker_runtime.mode ) {
    case nav2_worker_mode_t::InlineMainThread:
        Nav2_Worker_ExecutePayload( payload );
        Nav2_Worker_EnqueueCompletedPayload( payload );
        return true;
    case nav2_worker_mode_t::AsyncQueue: {
        asyncwork_t work = {};
        work.work_cb = Nav2_Worker_AsyncWorkCallback;
        work.done_cb = Nav2_Worker_AsyncDoneCallback;
        work.cb_arg = payload;
        gi.Com_QueueAsyncWork( &work );
        return true;
    }
    case nav2_worker_mode_t::Disabled:
    case nav2_worker_mode_t::Count:
    default:
        break;
    }

    /**
    *    If dispatch is disabled, clean up the transient payload and roll back bookkeeping.
    **/
    job->worker_in_flight = false;
    job->last_dispatch_timestamp_ms = 0;
    if ( s_nav2_worker_runtime.in_flight_count > 0 ) {
        s_nav2_worker_runtime.in_flight_count--;
    }
    delete payload;
    return false;
}

/**
*	@brief	Apply completed worker payloads on the main thread.
*	@note	Call this from existing svgame frame hooks after the engine has completed async work callbacks.
**/
void SVG_Nav2_Worker_ServiceCompletions( void ) {
    /**
    *    Move the completed payload list into a local vector first so application work does not hold
    *    the mutex for longer than necessary.
    **/
    std::vector<nav2_worker_payload_t *> completedPayloads = {};
    {
        std::lock_guard<std::mutex> lock( s_nav2_worker_completion_mutex );
        completedPayloads.swap( s_nav2_worker_completed_payloads );
        s_nav2_worker_runtime.pending_completion_count = 0;
    }

    /**
    *    Apply each completed payload back onto the owning scheduler job.
    **/
    for ( nav2_worker_payload_t *payload : completedPayloads ) {
        Nav2_Worker_ApplyCompletedPayload( payload );
    }
}
