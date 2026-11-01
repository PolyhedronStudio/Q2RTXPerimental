/********************************************************************
*
*	ServerGame: Dedicated Bounded Graph Query Transport
*	One event-driven reader. Buffer ownership and retirement stay on the owner.
*
********************************************************************/
#pragma once
#include "nav_query_algorithm.h"
#include <array>
#include <condition_variable>
#include <mutex>
#include <thread>

//! Pure transport mirrors engine MAX_EDICTS (8192), covering every valid entity slot independently of queue occupancy.
inline constexpr uint32_t NAV_WORKER_ENTITY_CAPACITY = 8192;

/**
*	@brief	Explicit budgets supplied by the measured owner-side configuration.
**/
struct nav_worker_limits_t {
	//! Supported geometry and prepared storage ceilings; oversize inputs are rejected.
	uint32_t slots = 64, faces = 4096, edges = 32768, heap = 65537;
	//! Total transport bytes include object storage and conservative allocator overhead.
	uint64_t bytes = 4u * 1024u * 1024u, work = 131072;
	//! Search quantum and aging interval, expressed in monotonic microseconds.
	uint64_t quantum_us = 1000, aging_us = 100000;
};

/**
*	@brief	Copied service telemetry; shared counters are read under the short queue lock.
**/
struct nav_worker_stats_t {
	//! Request lifecycle counters include saturation and obsolete replies explicitly.
	uint64_t submitted = 0, coalesced = 0, saturated = 0, cancelled = 0, obsolete = 0;
	uint64_t started = 0, completed = 0, drained = 0, limited = 0, errors = 0, wakes = 0, busy_ns = 0;
	//! Storage and concurrency high-water marks stay bounded across restarts.
	uint64_t retained_bytes = 0, peak_occupied = 0, peak_heap = 0;
	//! Longest measured solve, cancellation acknowledgement and queue age.
	uint64_t maximum_solve_ns = 0, maximum_cancel_ns = 0, maximum_age_ns = 0;
	//! Current queued population and oldest enqueue age are sampled independently of serviced age maxima.
	uint64_t current_queued = 0, oldest_queued_age_ns = 0;
	//! Short-lock wait and ownership durations are measured separately from solve time.
	uint64_t maximum_lock_wait_ns = 0, maximum_lock_hold_ns = 0;
};

/**
*	@brief	Terminal copied graph reply and the exclusive owner-consumption lease.
*	@note	The result pointer is immutable until ReleaseReady; no other owner operation may alter it.
**/
template<template<typename> class Buffer>
struct nav_worker_ready_t {
	//! Pool index, immutable owned graph reply, copied search counters and timestamps.
	int32_t slot = -1;
	const nav_query_result_t<Buffer> *result = nullptr;
	uint64_t enqueue_ns = 0, start_ns = 0, finish_ns = 0, work = 0;
};

/**
*	@brief	One persistent bounded worker with exclusive graph context and fixed request slots.
*	@tparam	Buffer	Owner-allocated storage; element operations must not allocate inside the solver.
*	@note	Public lifecycle, submission and consumption APIs belong to exactly one owner thread.
**/
template<template<typename> class Buffer, size_t SlotCapacity = 64>
class nav_worker_service_t {
private:
	/**
	*	@brief	Measure owner queue lock wait/hold time without importing profiler or allocator state.
	**/
	class measured_lock_t {
	public:
		/**
		*	@brief	Acquire synchronization before reading/updating shared service state.
		**/
		explicit measured_lock_t( nav_worker_service_t &instance ) : service( instance ), lock( instance.mutex, std::defer_lock ) {
			const uint64_t before = Now();
			lock.lock();
			entered = Now();
			service.stats.maximum_lock_wait_ns = std::max( service.stats.maximum_lock_wait_ns, entered - before );
		}
		/**
		*	@brief	Record hold time before the RAII lock releases publication ownership.
		**/
		~measured_lock_t() {
			service.stats.maximum_lock_hold_ns = std::max( service.stats.maximum_lock_hold_ns, Now() - entered );
		}
	private:
		//! Borrowed service and lock are retained only through this owner operation.
		nav_worker_service_t &service;
		std::unique_lock<std::mutex> lock;
		uint64_t entered = 0;
	};
public:
	/**
	*	Construction / destruction.
	**/
	nav_worker_service_t() = default;
	nav_worker_service_t( const nav_worker_service_t & ) = delete;
	nav_worker_service_t &operator=( const nav_worker_service_t & ) = delete;
	/**
	*	@brief	Join before owned buffers and synchronization objects can be destroyed.
	**/
	~nav_worker_service_t() { Stop(); }

	/**
	*	@brief	Prepare every buffer and launch only after geometry/storage bounds pass.
	*	@return	False for unsupported inputs; allocation/thread creation exceptions stay on the owner.
	*	@note	The external snapshot lease must survive Stop and all ready-consumption leases.
	**/
	bool Start( const nav_query_mesh_view_t &mesh, const nav_worker_limits_t &limits ) {
		// Restart cannot silently overlap a live reader or adopt a different owner.
		if ( worker.joinable() || ( owner != std::thread::id{} && owner != std::this_thread::get_id() ) ||
			 limits.slots == 0 || limits.slots > SlotCapacity || mesh.faces.empty() ||
			 mesh.faces.size() > limits.faces || mesh.edges.size() > limits.edges || limits.heap == 0 ||
			 limits.faces > 4096 || limits.edges > 32768 || limits.heap > 65537 ||
			 limits.quantum_us == 0 || limits.quantum_us > 1000000 || limits.aging_us == 0 || limits.aging_us > 1000000000 ||
			 limits.work == 0 || limits.work > 131072 ) {
			return false;
		}
		// Reject the requested byte footprint before any capacity preparation can allocate.
		const uint64_t requested_bytes = sizeof( *this ) + static_cast<uint64_t>( limits.faces ) * sizeof( nav_query_face_state_t ) +
			static_cast<uint64_t>( limits.heap ) * sizeof( nav_query_heap_node_t ) + 1024 +
			SlotCapacity * ( static_cast<uint64_t>( limits.faces ) * ( sizeof( uint8_t ) + sizeof( int32_t ) ) + 1024 );
		if ( requested_bytes > limits.bytes ) {
			return false;
		}
		owner = std::this_thread::get_id();
		/**
		*	Prepare capacities before starting the reader; actual capacities are checked again.
		**/
		config = limits;
		view = mesh;
		context.states.resize( config.faces );
		context.heap.reserve( config.heap );
		for ( auto &slot : slots ) {
			slot.request.prior_faces.reserve( config.faces );
			slot.result.corridor.reserve( config.faces );
			slot.state = state_t::free;
			slot.leased = false;
		}
		const uint64_t retained = RetainedBytes();
		if ( retained > config.bytes ) {
			return false;
		}
		// Reset transient identities and counters before publishing the new running lifetime.
		latest.fill( {} );
		stats = {};
		stats.retained_bytes = retained;
		last_group = -1;
		stopping = false;
		open = true;
		worker = std::thread( &nav_worker_service_t::Run, this );
		return true;
	}

	/**
	*	@brief	Close submission, cancel all jobs and join without holding the queue lock.
	*	@note	No worker callback or owner work is required to acknowledge stop.
	**/
	void Stop() {
		// Empty or previously joined services need no synchronization or destruction work.
		if ( !worker.joinable() ) {
			return;
		}
		Q_assert( owner == std::this_thread::get_id() );
		{
			measured_lock_t lock( *this );
			open = false;
			stopping = true;
			for ( auto &slot : slots ) {
				// Running storage cannot be recycled before the thread has acknowledged cancellation.
				if ( slot.state == state_t::running ) {
					slot.cancel_time = Now();
					slot.cancel.store( true, std::memory_order_release );
				}
			}
		}
		wake.notify_one();
		worker.join();
		// All payloads are owner-exclusive again; outstanding consumption leases are a caller error.
		for ( auto &slot : slots ) {
			Q_assert( !slot.leased );
			slot.state = state_t::free;
		}
	}

	/**
	*	@brief	Coalesce a latest actor request or report bounded saturation without allocating.
	*	@param	priority	Zero is urgent; three is optional refresh. Aging promotes waiting jobs.
	*	@param	group	Copied fairness key; no live crowd pointer enters the service.
	*	@param	owner_not_before_tick	Optional owner delivery boundary; zero preserves ordinary immediate delivery.
	*	@return	True only when the copied request occupies a queued slot.
	*	@note	The newest coalesced request replaces its delivery boundary while preserving queue age.
	**/
	bool Submit( const nav_query_request_t<Buffer> &request, const uint32_t priority, const int32_t group,
		const uint64_t owner_not_before_tick = 0 ) {
		Q_assert( owner == std::this_thread::get_id() );
		// Reject unsupported identity, policy storage and publication before any queue mutation.
		const int32_t entity = request.identity.entity;
		if ( entity < 0 || static_cast<uint32_t>( entity ) >= latest.size() || priority > 3 ||
			 request.identity.sequence == 0 || request.identity.generation != view.generation ||
			 request.identity.topology != view.topology || request.prior_faces.size() > view.faces.size() ) {
			return false;
		}
		measured_lock_t lock( *this );
		if ( !open ) {
			return false;
		}
		// Sequence/epoch identify the newest submission independently of pool reuse.
		const auto &previous = latest[ entity ];
		if ( previous.epoch > request.identity.epoch ||
			 ( previous.epoch == request.identity.epoch && previous.sequence >= request.identity.sequence ) ) {
			return false;
		}
		int32_t queued = -1, available = -1;
		const uint64_t now = Now();
		for ( uint32_t index = 0; index < config.slots; ++index ) {
			auto &slot = slots[ index ];
			// Coalesce only owner-exclusive queued storage; cancel a running reader atomically.
			if ( slot.state != state_t::free && slot.request.identity.entity == entity ) {
				if ( slot.state == state_t::queued ) {
					queued = static_cast<int32_t>( index );
				} else if ( slot.state == state_t::running ) {
					if ( !slot.cancel.load( std::memory_order_relaxed ) ) {
						slot.cancel_time = now;
						slot.cancel.store( true, std::memory_order_release );
					}
				} else if ( !slot.leased ) {
					// A ready old reply is obsolete before the replacement request is exposed.
					slot.state = state_t::free;
					++stats.obsolete;
				}
			}
			if ( slot.state == state_t::free && available < 0 ) {
				available = static_cast<int32_t>( index );
			}
		}
		// Even a saturated replacement invalidates an old running result; retry intent stays with its caller.
		latest[ entity ] = request.identity;
		const int32_t target = queued >= 0 ? queued : available;
		if ( target < 0 ) {
			++stats.saturated;
			return false;
		}
		if ( queued >= 0 ) {
			++stats.coalesced;
		}
		/**
		*	Copy into reserved storage. Submission does not resize beyond owner-prepared capacity.
		**/
		auto &slot = slots[ target ];
		Q_assert( slot.request.prior_faces.capacity() >= request.prior_faces.size() );
		slot.request = request;
		slot.request.max_expansions = std::min( request.max_expansions, config.work );
		slot.request.max_edge_tests = std::min( request.max_edge_tests, config.work );
		slot.priority = priority;
		slot.group = group;
		// Delivery gating belongs to this copied request; coalescing cannot reuse the previous deadline.
		slot.owner_not_before_tick = owner_not_before_tick;
		// Replacing a queued request preserves its age to prevent hot actors from starving.
		slot.enqueue_time = queued >= 0 ? slot.enqueue_time : now;
		slot.cancel_time = 0;
		slot.cancel.store( false, std::memory_order_release );
		slot.published = false;
		slot.state = state_t::queued;
		++stats.submitted;
		uint64_t occupied = 0;
		for ( const auto &item : slots ) {
			occupied += item.state != state_t::free;
		}
		stats.peak_occupied = std::max( stats.peak_occupied, occupied );
		// Tick-end Publish wakes the reader once for the collected batch.
		return true;
	}

	/**
	*	@brief	Invalidate one actor's pending replies and cancel its running reader without waiting.
	*	@note	Queued/ready storage is owner-exclusive; a running slot is recycled only after acknowledgement.
	**/
	void CancelEntity( const int32_t entity ) {
		Q_assert( owner == std::this_thread::get_id() );
		if ( entity < 0 || static_cast<size_t>( entity ) >= latest.size() ) {
			return;
		}
		measured_lock_t lock( *this );
		latest[ entity ] = {};
		for ( auto &slot : slots ) {
			if ( slot.state == state_t::free || slot.request.identity.entity != entity ) {
				continue;
			}
			if ( slot.state == state_t::running ) {
				slot.cancel_time = Now();
				slot.cancel.store( true, std::memory_order_release );
			} else if ( !slot.leased ) {
				slot.state = state_t::free;
				++stats.obsolete;
			}
		}
	}

	/**
	*	@brief	Wake once after a tick's batch, or immediately after an urgent submission.
	**/
	void Publish() {
		Q_assert( owner == std::this_thread::get_id() );
		bool useful = false;
		{
			// Jobs become selectable together at the declared batch boundary.
			measured_lock_t lock( *this );
			for ( auto &slot : slots ) {
				// Empty frame batches must leave the idle reader asleep.
				if ( slot.state == state_t::queued && !slot.published ) {
					slot.published = true;
					useful = true;
				}
			}
		}
		// Notify only when this batch exposed new work; shutdown has its separate unconditional wake.
		if ( useful ) {
			wake.notify_one();
		}
	}

	/**
	*	@brief	Lease one current due result, rejecting obsolete identities before owner consumption.
	*	@param	owner_tick	Owner delivery clock; the default disables optional tick deferral.
	*	@param	reverse	Select descending slot indices for bounded out-of-order delivery fixtures.
	*	@return	An empty lease when no current due result is ready; owner consumption runs outside the lock.
	*	@note	Deferral retains the existing ready slot; it never allocates another result or delays the solver.
	**/
	nav_worker_ready_t<Buffer> TakeReady( const uint64_t owner_tick = UINT64_MAX, const bool reverse = false ) {
		Q_assert( owner == std::this_thread::get_id() );
		measured_lock_t lock( *this );
		// Traverse every bounded slot in the requested owner order without moving any result payload.
		for ( uint32_t offset = 0; offset < config.slots; ++offset ) {
			const uint32_t index = reverse ? config.slots - offset - 1 : offset;
			auto &slot = slots[ index ];
			// Worker never touches ready payloads; leased results cannot be recycled by submission.
			if ( slot.state != state_t::ready || slot.leased ) {
				continue;
			}
			const auto &id = slot.result.identity;
			const auto &current = latest[ id.entity ];
			if ( id.sequence != current.sequence || id.epoch != current.epoch || id.incarnation != current.incarnation ) {
				slot.state = state_t::free;
				++stats.obsolete;
				continue;
			}
			// A current deferred reply remains unleased and cancellable until its owner delivery boundary.
			if ( owner_tick < slot.owner_not_before_tick ) {
				continue;
			}
			slot.leased = true;
			return { static_cast<int32_t>( index ), &slot.result, slot.enqueue_time, slot.start_time, slot.finish_time, slot.work };
		}
		return {};
	}

	/**
	*	@brief	Recycle only a matched ready-consumption lease on the owner.
	**/
	void ReleaseReady( const nav_worker_ready_t<Buffer> &reply ) {
		Q_assert( owner == std::this_thread::get_id() );
		measured_lock_t lock( *this );
		Q_assert( reply.slot >= 0 && static_cast<size_t>( reply.slot ) < slots.size() );
		auto &slot = slots[ reply.slot ];
		Q_assert( slot.state == state_t::ready && slot.leased && reply.result == &slot.result );
		slot.leased = false;
		slot.state = state_t::free;
		++stats.drained;
	}

	/**
	*	@brief	Read copied counters and current oldest queued age without exposing mutable storage.
	*	@return	One synchronized statistics snapshot, including unpublished queued work.
	*	@note	The current backlog age is distinct from the maximum age of previously serviced requests.
	**/
	nav_worker_stats_t Stats() {
		/**
		*	Copy lifecycle counters and inspect the fixed slot population under the same short lock.
		**/
		measured_lock_t lock( *this );
		auto snapshot = stats;
		const uint64_t now = Now();
		// Running/ready replies are not queued backlog; owner latency reports cover their later lifetime.
		for ( const auto &slot : slots ) {
			if ( slot.state != state_t::queued ) {
				continue;
			}
			++snapshot.current_queued;
			const uint64_t age = now >= slot.enqueue_time ? now - slot.enqueue_time : 0;
			snapshot.oldest_queued_age_ns = std::max( snapshot.oldest_queued_age_ns, age );
		}
		return snapshot;
	}

private:
	/**
	*	Pool ownership states.
	**/
	enum class state_t : uint8_t { free, queued, running, ready };
	struct slot_t {
		//! Request and reply buffers are constructed/reserved/destroyed only on the owner.
		nav_query_request_t<Buffer> request;
		nav_query_result_t<Buffer> result;
		//! State and consumption lease are protected by the queue mutex.
		state_t state = state_t::free;
		bool leased = false, published = false;
		//! Reader cancellation is separate from the immutable running payload.
		std::atomic<bool> cancel{ false };
		//! Copied priority/fairness values and lifecycle clocks.
		uint32_t priority = 3;
		int32_t group = -1;
		uint64_t enqueue_time = 0, start_time = 0, finish_time = 0, cancel_time = 0, work = 0;
		//! Optional copied owner delivery boundary leaves ready storage in its original bounded pool slot.
		uint64_t owner_not_before_tick = 0;
	};

	/**
	*	@brief	Sample the standard monotonic clock without engine imports.
	**/
	static uint64_t Now() {
		return static_cast<uint64_t>( std::chrono::duration_cast<std::chrono::nanoseconds>( std::chrono::steady_clock::now().time_since_epoch() ).count() );
	}

	/**
	*	@brief	Compute a conservative transport byte ceiling from actual prepared capacities.
	*	@note	Includes 1024 bytes per request/result pair and context for engine/module/debug bookkeeping; OS thread stack is separate.
	**/
	uint64_t RetainedBytes() const {
		uint64_t bytes = sizeof( *this ) + context.states.capacity() * sizeof( nav_query_face_state_t ) +
			context.heap.capacity() * sizeof( nav_query_heap_node_t ) + 1024;
		for ( const auto &slot : slots ) {
			bytes += slot.request.prior_faces.capacity() + slot.result.corridor.capacity() * sizeof( int32_t ) + 1024;
		}
		return bytes;
	}

	/**
	*	@brief	Select aged priority, then alternate available groups within that priority.
	*	@note	Stable enqueue order is the final tie-break; a continuously hot group cannot monopolize equals.
	**/
	int32_t Select( const uint64_t now ) const {
		int32_t best = -1;
		uint32_t best_priority = UINT32_MAX;
		bool best_same = true;
		for ( uint32_t index = 0; index < config.slots; ++index ) {
			const auto &slot = slots[ index ];
			if ( slot.state != state_t::queued || !slot.published ) {
				continue;
			}
			const uint64_t promotion = ( now - slot.enqueue_time ) / ( config.aging_us * 1000 );
			const uint32_t priority = slot.priority > promotion ? static_cast<uint32_t>( slot.priority - promotion ) : 0;
			const bool same = slot.group == last_group;
			if ( best < 0 || priority < best_priority || ( priority == best_priority &&
				 ( same < best_same || ( same == best_same && slot.enqueue_time < slots[ best ].enqueue_time ) ) ) ) {
				best = static_cast<int32_t>( index );
				best_priority = priority;
				best_same = same;
			}
		}
		return best;
	}

	/**
	*	@brief	Wait for useful work, solve outside locks, then publish terminal owned output.
	*	@note	No owner callback, engine API, buffer allocation/free or live geometry is used here.
	**/
	void Run() {
		std::unique_lock<std::mutex> lock( mutex );
		while ( !stopping ) {
			wake.wait( lock, [&]() { return stopping || Select( Now() ) >= 0; } );
			if ( stopping ) {
				break;
			}
			const uint64_t selection_lock_start = Now();
			++stats.wakes;
			const int32_t selected = Select( Now() );
			auto &slot = slots[ selected ];
			slot.state = state_t::running;
			++stats.started;
			slot.start_time = Now();
			last_group = slot.group;
			stats.maximum_age_ns = std::max( stats.maximum_age_ns, slot.start_time - slot.enqueue_time );
			stats.maximum_lock_hold_ns = std::max( stats.maximum_lock_hold_ns, Now() - selection_lock_start );
			lock.unlock();
			/**
			*	The running payload and single context are exclusive until terminal publication.
			**/
			nav_query_execution_control_t control;
			control.cancel = &slot.cancel;
			control.maximum_work = config.work;
			// A time quantum suspends this reader, while the total work ceiling still bounds the whole request.
			control.slice_us = config.quantum_us;
			control.deadline = std::chrono::steady_clock::now() + std::chrono::microseconds( config.quantum_us );
			bool failed = false;
			try {
				Nav_QueryFindPath( view, context, slot.request, slot.result, &control );
			} catch ( ... ) {
				// Unexpected algorithm failures publish a terminal error; never escape the thread entry point.
				slot.result.corridor.clear();
				slot.result.status = nav_query_status_t::invalid_input;
				slot.result.identity = slot.request.identity;
				failed = true;
			}
			const uint64_t finish = Now();
			lock.lock();
			const uint64_t publication_lock_start = Now();
			stats.maximum_lock_wait_ns = std::max( stats.maximum_lock_wait_ns, publication_lock_start - finish );
			// Publish while synchronized; owner reads a ready reply only after taking this same mutex.
			slot.finish_time = finish;
			slot.work = control.work;
			slot.state = state_t::ready;
			++stats.completed;
			stats.errors += failed;
			stats.cancelled += control.cancelled;
			stats.limited += slot.result.status == nav_query_status_t::resource_limited;
			const uint64_t duration = finish - slot.start_time;
			stats.busy_ns += duration;
			stats.maximum_solve_ns = std::max( stats.maximum_solve_ns, duration );
			stats.peak_heap = std::max( stats.peak_heap, context.peak_heap );
			if ( slot.cancel_time != 0 ) {
				stats.maximum_cancel_ns = std::max( stats.maximum_cancel_ns, finish >= slot.cancel_time ? finish - slot.cancel_time : 0 );
			}
			stats.maximum_lock_hold_ns = std::max( stats.maximum_lock_hold_ns, Now() - publication_lock_start );
		}
	}

	/**
	*	Exclusive owners and synchronized transport state.
	**/
	//! Fixed pool and persistent identity namespace never grow with request arrival.
	std::array<slot_t, SlotCapacity> slots;
	std::array<nav_query_identity_t, NAV_WORKER_ENTITY_CAPACITY> latest;
	//! One immutable pinned view and one exclusive executing graph context.
	nav_query_mesh_view_t view;
	nav_query_context_t<Buffer> context;
	//! Owner-issued budgets, synchronization and thread lifetime.
	nav_worker_limits_t config;
	nav_worker_stats_t stats;
	std::thread worker;
	std::thread::id owner;
	std::mutex mutex;
	std::condition_variable wake;
	bool open = false, stopping = false;
	//! Last served group is used only while holding the queue lock.
	int32_t last_group = -1;
};
