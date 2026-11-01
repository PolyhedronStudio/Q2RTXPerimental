/********************************************************************
*
*	ServerGame: Bounded Pure Query Execution Control
*	Only atomics and the standard monotonic clock are read by the solver.
*
********************************************************************/
#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>
#include <thread>

/**
*	@brief	Terminal limit reason is independent of unreachable geometry.
**/
enum class nav_query_limit_reason_t : uint8_t { none, work, deadline, storage, expansions, edge_tests, heap };

/**
*	@brief	Exclusive execution control, separate from immutable request policy.
*	@note	Cancellation storage stays alive until the running reader acknowledges completion.
**/
struct nav_query_execution_control_t {
	//! Owner publishes cancellation; the running solver only reads the token.
	const std::atomic<bool> *cancel = nullptr;
	//! Wall deadline is prepared once by the worker before entering the algorithm.
	std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::time_point::max();
	//! Nonzero worker quantum yields with the entire search stack/context retained instead of failing it.
	uint64_t slice_us = 0, slices = 0;
	//! Hard work count includes heap pops, edges, reconstruction and stamp clearing.
	uint64_t maximum_work = std::numeric_limits<uint64_t>::max(), work = 0;
	//! Outcomes distinguish cancellation from time/work exhaustion.
	bool cancelled = false, limited = false;
	//! Exact reason is copied into the terminal reply before publication.
	nav_query_limit_reason_t reason = nav_query_limit_reason_t::none;
};

/**
*	@brief	Check bounded work on every call and cancellation/time at most 32 operations apart.
*	@return	True when the caller must terminate and discard partial output.
*	@note	A null control preserves the synchronous unlimited reference contract.
**/
inline bool Nav_QueryCheckpoint( nav_query_execution_control_t *control, const bool force = false ) {
	// Ordinary synchronous calls do not sample a clock or access transport state.
	if ( control == nullptr ) {
		return false;
	}
	// Work exhaustion is checked before advancing, including stale heap entries.
	if ( control->work == control->maximum_work ) {
		control->limited = true;
		control->reason = nav_query_limit_reason_t::work;
		return true;
	}
	++control->work;
	// Poll frequently enough to bound cancellation while avoiding a clock call per edge.
	if ( force || ( control->work & 31u ) == 0 ) {
		if ( control->cancel != nullptr && control->cancel->load( std::memory_order_acquire ) ) {
			control->cancelled = true;
			return true;
		}
		if ( std::chrono::steady_clock::now() >= control->deadline ) {
			// Worker-only cooperative slices retain heap, edge position and reconstruction state on this stack.
			// A short reader pause gives the game/render thread CPU time; the owner never waits for it.
			if ( control->slice_us != 0 ) {
				++control->slices;
				std::this_thread::sleep_for( std::chrono::microseconds( 250 ) );
				// Cancellation during the pause discards partial output through the ordinary terminal path.
				if ( control->cancel != nullptr && control->cancel->load( std::memory_order_acquire ) ) {
					control->cancelled = true;
					return true;
				}
				control->deadline = std::chrono::steady_clock::now() + std::chrono::microseconds( control->slice_us );
			} else {
				// Explicit diagnostic deadlines retain their original terminal-limit contract.
				control->limited = true;
				control->reason = nav_query_limit_reason_t::deadline;
				return true;
			}
		}
	}
	return false;
}
