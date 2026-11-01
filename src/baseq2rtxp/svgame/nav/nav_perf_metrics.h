/********************************************************************
*
*	ServerGame: Pure Navigation Performance Metric Contracts
*	These owner-side arithmetic helpers have no engine, allocation or clock calls.
*
********************************************************************/
#pragma once
#include <cstdint>
#include <cmath>

/**
*	@brief	Exact paired request latency components in monotonic nanoseconds.
*	@note	Handoff includes publication, tick/commit budget delay and native owner completion.
**/
struct nav_perf_worker_latency_sample_t {
	//! Time from the latest actual owner submission attempt to successful route installation.
	uint64_t total_ns = 0;
	//! Time from that submission attempt until the selected worker solve starts.
	uint64_t queue_ns = 0;
	//! Time spent solving the copied graph, excluding result publication.
	uint64_t solve_ns = 0;
	//! Time from graph solve completion until successful native owner installation.
	uint64_t handoff_ns = 0;
};

/**
*	@brief	Validate one monotonic request timeline and derive its exact additive components.
*	@param	submitted_ns	Latest actual owner Submit attempt; fairness-preserving queue age is excluded.
*	@param	start_ns	Worker solve start, sampled from the same steady clock.
*	@param	finish_ns	Worker graph solve finish, before result publication.
*	@param	completed_ns	Successful native owner installation, sampled from the same steady clock.
*	@param	out	[out] Exact durations, unchanged on invalid input.
*	@return	True for nonzero, ordered timestamps; zero-duration intervals remain valid.
**/
inline bool Nav_Perf_CalculateWorkerLatency( const uint64_t submitted_ns, const uint64_t start_ns,
	const uint64_t finish_ns, const uint64_t completed_ns, nav_perf_worker_latency_sample_t &out ) {
	/**
	*	Reject missing or misordered timestamps before unsigned subtraction can underflow.
	**/
	if ( submitted_ns == 0 || submitted_ns > start_ns || start_ns > finish_ns || finish_ns > completed_ns ) {
		return false;
	}
	out.total_ns = completed_ns - submitted_ns;
	out.queue_ns = start_ns - submitted_ns;
	out.solve_ns = finish_ns - start_ns;
	out.handoff_ns = completed_ns - finish_ns;
	return true;
}

/**
*	@brief	Accumulate paired owner navigation phase costs into one zero-inclusive tick sample.
*	@note	Selected nested callbacks are timed once by their outermost scope; other phases are excluded.
**/
struct nav_perf_tick_aggregate_t {
	//! True only between the accepted gameplay tick begin and its recorded end.
	bool open = false;
	//! Selected scope depth keeps callback orchestration inside native completion from double counting.
	uint32_t scope_depth = 0;
	//! Sum of selected per-call wall costs for the current tick in microseconds.
	double wall_us = 0.0;
	//! Sum of selected per-call owner CPU costs for the current tick in microseconds.
	double cpu_us = 0.0;

	/**
	*	@brief	Begin a fresh aggregate without discarding an unfinished tick.
	*	@return	False if a previous tick or selected scope remains open.
	**/
	bool Begin() {
		// Repeated begin must not silently erase an unrecorded owner sample.
		if ( open || scope_depth != 0 ) {
			return false;
		}
		open = true;
		wall_us = 0.0;
		cpu_us = 0.0;
		return true;
	}

	/**
	*	@brief	Add a measured owner phase only inside the matching tick.
	*	@param	elapsed_us	Measured selected wall cost in microseconds.
	*	@param	thread_cpu_us	Measured selected owner CPU cost in microseconds.
	*	@return	False for calls outside a tick or invalid elapsed durations.
	**/
	bool Add( const double elapsed_us, const double thread_cpu_us ) {
		// Out-of-tick diagnostic queries and invalid clocks cannot contaminate gameplay aggregates.
		if ( !open || !std::isfinite( elapsed_us ) || !std::isfinite( thread_cpu_us ) || elapsed_us < 0.0 || thread_cpu_us < 0.0 ) {
			return false;
		}
		wall_us += elapsed_us;
		cpu_us += thread_cpu_us;
		return true;
	}

	/**
	*	@brief	Enter a selected interval, including intervals for out-of-tick diagnostic calls.
	*	@return	False only if selected nesting exceeds the fixed integer namespace.
	**/
	bool BeginScope() {
		// A malformed recursive chain cannot wrap the ownership depth to zero.
		if ( scope_depth == UINT32_MAX ) {
			return false;
		}
		++scope_depth;
		return true;
	}

	/**
	*	@brief	Close a matched selected interval and add only its outermost cost to an open tick.
	*	@param	elapsed_us	Wall duration of this completed selected interval.
	*	@param	thread_cpu_us	Owner CPU duration of this completed selected interval.
	*	@return	False on an unmatched close or invalid outermost measured duration.
	**/
	bool EndScope( const double elapsed_us, const double thread_cpu_us ) {
		// An unmatched close cannot underflow the nesting count and corrupt later tick costs.
		if ( scope_depth == 0 ) {
			return false;
		}
		--scope_depth;
		// Diagnostic intervals outside ticks retain phase samples but do not manufacture gameplay samples.
		if ( scope_depth != 0 || !open ) {
			return true;
		}
		return Add( elapsed_us, thread_cpu_us );
	}

	/**
	*	@brief	Seal the aggregate while retaining zero-work ticks as valid samples.
	*	@param	out_wall_us	[out] Sum of selected owner wall costs.
	*	@param	out_cpu_us	[out] Sum of selected owner CPU costs.
	*	@return	False if no matching gameplay tick was begun or a selected interval remains open.
	**/
	bool Finish( double &out_wall_us, double &out_cpu_us ) {
		// A missing begin must not manufacture an extra idle tick in the percentile reservoir.
		if ( !open || scope_depth != 0 ) {
			return false;
		}
		open = false;
		out_wall_us = wall_us;
		out_cpu_us = cpu_us;
		return true;
	}
};
