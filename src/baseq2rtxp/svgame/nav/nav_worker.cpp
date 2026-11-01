/********************************************************************
*
*	ServerGame: Owner-Side Navigation Worker Lifecycle and Diagnostics
*	The worker entry/call graph is in nav_worker_service.h and uses no engine API.
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/nav/nav_worker.h"
#include "svgame/nav/nav_actor_fixture.h"
#include "svgame/nav/nav_identity.h"
#include "svgame/nav/nav_thread.h"
#include "svgame/entities/monster/svg_monster_base.h"
#include <memory>
#include <exception>

//! Dedicated service is constructed and destroyed explicitly on the server owner.
static std::unique_ptr<svg_nav_worker_service_t> s_worker;
//! One owner-side lease retains immutable backing through reader join and reclamation.
static const svg_nav_mesh_snapshot_t *s_worker_snapshot = nullptr;
//! Actor activation is opt-in until M8's measured rollout gates pass.
static cvar_t *s_actor_mode = nullptr;
//! Opt-in deterministic minimum owner delivery delay for native regression controls.
static cvar_t *s_actor_delay_ticks = nullptr;
//! Opt-in reverse ready-slot delivery exercises result ordering without changing graph computation.
static cvar_t *s_actor_reverse_delivery = nullptr;

static_assert( NAV_WORKER_ENTITY_CAPACITY == MAX_EDICTS, "Navigation identities must cover every supported entity slot." );
//! Synthetic diagnostics cannot share entity keys with a production actor service.
static bool s_actor_service = false;
//! Unsupported capacity/publication start is retried only after mode or publication changes.
static bool s_actor_start_failed = false;
//! Distinguishes unsupported allocation/capacity from a publication that is still being prepared.
static bool s_worker_start_rejected = false;
//! Failed publication identity prevents allocating/logging the same rejected service every game tick.
static uint64_t s_actor_failed_generation = 0, s_actor_failed_topology = 0, s_actor_failed_epoch = 0;


/**
*	@brief	Read the owner-only runtime switch without exposing cvars to the worker.
*	@return	True when actor scheduling is requested; default remains synchronous.
**/
bool Nav_IsActorQueryWorkerEnabledMainThread() {
	// Register lazily after game imports exist, never during module static initialization.
	if ( s_actor_mode == nullptr ) {
		s_actor_mode = gi.cvar( "nav_actor_worker", "0", 0 );
		s_actor_delay_ticks = gi.cvar( "nav_actor_worker_delay_ticks", "1", 0 );
		s_actor_reverse_delivery = gi.cvar( "nav_actor_worker_reverse_delivery", "0", 0 );
	}
	return s_actor_mode->integer != 0;
}

//! Conservative snapshot ceiling and total storage ceiling are checked independently of slot count.
static constexpr uint64_t kSnapshotBytes = 32u * 1024u * 1024u, kTotalBytes = 36u * 1024u * 1024u;

/**
*	@brief	Start only after immutable snapshot and fixed service limits pass owner checks.
*	@return	True for a live service; false preserves explicit synchronous diagnosis.
**/
bool Nav_StartQueryWorkerMainThread( void ) {
	s_worker_start_rejected = false;
	// Repeated diagnostic start cannot allocate a second reader or context pool.
	if ( s_worker != nullptr ) {
		return true;
	}
	s_worker_snapshot = Nav_AcquireQuerySnapshotMainThread();
	if ( s_worker_snapshot == nullptr ) {
		return false;
	}
	// An acquired publication makes subsequent failures capacity/resource failures, not preparation waits.
	s_worker_start_rejected = true;
	const nav_worker_limits_t limits;
	const uint64_t snapshot_bytes = s_worker_snapshot->RetainedBytes();
	if ( snapshot_bytes > kSnapshotBytes || snapshot_bytes + limits.bytes > kTotalBytes ) {
		Nav_ReleaseQuerySnapshotMainThread( s_worker_snapshot );
		s_worker_snapshot = nullptr;
		gi.dprintf( "[nav worker] unsupported snapshot storage: %" PRIu64 "\n", snapshot_bytes );
		return false;
	}
	/**
	*	Thread creation and preparation exceptions are handled before any job is exposed.
	**/
	try {
		s_worker = std::make_unique<svg_nav_worker_service_t>();
		if ( s_worker->Start( s_worker_snapshot->View(), limits ) ) {
			s_worker_start_rejected = false;
			gi.dprintf( "[nav worker] started snapshot_bytes=%" PRIu64 " transport_bytes=%" PRIu64 " slots=64\n",
				snapshot_bytes, s_worker->Stats().retained_bytes );
			return true;
		}
	} catch ( const std::exception &exception ) {
		gi.dprintf( "[nav worker] owner preparation failed: %s\n", exception.what() );
	}
	// A failed start has no externally visible job and must release its borrowed geometry.
	Nav_StopQueryWorkerMainThread();
	return false;
}

/**
*	@brief	Cancel/join without a queue lock, then reclaim storage and the snapshot on the owner.
*	@note	Idempotent across clear, generation, overflow, engine map reset and module shutdown.
**/
void Nav_StopQueryWorkerMainThread( void ) {
	/**
	*	Close actor pending states before joining; retain installed geometry for normal revalidation.
	**/
	if ( s_actor_service ) {
		s_actor_service = false;
		// Teardown is rare; scan live slots once so no cancelled request remains permanently pending.
		for ( int32_t entity = 0; entity < g_edict_pool.num_edicts; ++entity ) {
			auto *actor = dynamic_cast<svg_monster_base_t *>( g_edict_pool.EdictForNumber( entity ) );
			if ( actor != nullptr && actor->navigationQueryState == svg_monster_base_t::NavigationQueryState::Pending ) {
				actor->SVG_Nav_SetQueryTerminalState( svg_monster_base_t::NavigationQueryState::Cancelled );
			}
		}
	}
	if ( s_worker != nullptr ) {
		s_worker->Stop();
		const auto stats = s_worker->Stats();
		gi.dprintf( "[nav worker] stopped started=%" PRIu64 " completed=%" PRIu64 " cancelled=%" PRIu64 " errors=%" PRIu64 "\n",
			stats.started, stats.completed, stats.cancelled, stats.errors );
		s_worker.reset();
	}
	// Snapshot reclamation always follows the reader join and owned context destruction.
	if ( s_worker_snapshot != nullptr ) {
		Nav_ReleaseQuerySnapshotMainThread( s_worker_snapshot );
		s_worker_snapshot = nullptr;
	}
}

/**
*	@brief	Consume bounded copied replies before entity/crowd thinking, then finish native work on the owner.
*	@note	Two replies and a cooperative 500us graph-drain budget; native time is measured separately.
**/
void Nav_QueryWorkerBeginFrameMainThread( void ) {
	/**
	*	Mode changes drain before restart; diagnostic keys never enter the actor namespace.
	**/
	const bool actor_mode = Nav_IsActorQueryWorkerEnabledMainThread();
	if ( s_actor_service && !actor_mode ) {
		Nav_StopQueryWorkerMainThread();
	}
	// Disabling the mode permits an explicit retry later without retaining failed-start state.
	if ( !actor_mode ) {
		s_actor_start_failed = false;
	}
	const bool rejected_publication = s_actor_start_failed && s_actor_failed_generation == g_nav_mesh_generation &&
		s_actor_failed_topology == g_nav_topology_revision && s_actor_failed_epoch == Nav_IdentityEpochMainThread();
	// Unsupported publications never cause a per-tick thread/context creation attempt.
	if ( actor_mode && !s_actor_service && !rejected_publication ) {
		Nav_StopQueryWorkerMainThread();
		s_actor_service = Nav_StartQueryWorkerMainThread();
		s_actor_start_failed = !s_actor_service && s_worker_start_rejected;
		s_actor_failed_generation = g_nav_mesh_generation;
		s_actor_failed_topology = g_nav_topology_revision;
		s_actor_failed_epoch = Nav_IdentityEpochMainThread();
	}
	if ( s_worker == nullptr ) {
		return;
	}
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::microseconds( 500 );
	// Bound normal consumption; native completion never waits for an unfinished solver.
	for ( uint32_t count = 0; count < 2 && std::chrono::steady_clock::now() < deadline; ++count ) {
		// Frame/time are already advanced; callbacks and transport eligibility use this exact owner tick.
		const uint64_t ownerTick = static_cast<uint64_t>( level.frameNumber );
		const auto reply = s_actor_service ? s_worker->TakeReady( ownerTick, s_actor_reverse_delivery->integer != 0 ) : s_worker->TakeReady();
		if ( reply.result == nullptr ) {
			break;
		}
		svg_monster_base_t *actor = nullptr;
		// Diagnostic service payloads are discarded even if their numeric key names a live actor.
		if ( s_actor_service ) {
			actor = dynamic_cast<svg_monster_base_t *>( g_edict_pool.EdictForNumber( reply.result->identity.entity ) );
			if ( actor != nullptr && actor->SVG_Nav_IsIdentityCurrent( reply.result->identity ) &&
				reply.result->identity.sequence == actor->navigationRequestSequence ) {
				// Copy into retained actor-owned storage while the immutable transport lease is held.
				try {
					actor->navigationGraphWorkspace.result = *reply.result;
					// Retain solver times separately from enqueue age, which queued coalescing intentionally preserves.
					actor->navigationGraphStartNs = reply.start_ns;
					actor->navigationGraphFinishNs = reply.finish_ns;
				} catch ( ... ) {
					// Owner allocation failure must release its lease before error handling can stop the service.
					s_worker->ReleaseReady( reply );
					actor->SVG_Nav_SetQueryTerminalState( svg_monster_base_t::NavigationQueryState::ResourceLimited );
					throw;
				}
			} else {
				// Close only this still-pending incarnation/sequence; never overwrite a newer request or reused slot.
				if ( actor != nullptr && actor->navigationQueryState == svg_monster_base_t::NavigationQueryState::Pending &&
					 actor->navigationIncarnation == reply.result->identity.incarnation &&
					 actor->navigationRequestSequence == reply.result->identity.sequence ) {
					// Record the identity rejection site only for an explicitly active benchmark.
					Nav_ActorBenchmarkRecordRejectionMainThread( 3u );
					actor->SVG_Nav_SetQueryTerminalState( svg_monster_base_t::NavigationQueryState::Stale );
					actor->lastPathCalcTime = level.time;
				}
				actor = nullptr;
			}
		}
		// No callback, native validation or identity rollover may run while a slot is leased.
		s_worker->ReleaseReady( reply );
		if ( actor != nullptr ) {
			actor->SVG_Nav_AcceptWorkerResult();
		}
		// A native callback may have stopped/replaced the service through reset or publication change.
		if ( s_worker == nullptr ) {
			break;
		}
	}
}

/**
*	@brief	Prepare and enqueue one copied actor request without waiting or solving on the owner.
*	@return	False on unsupported publication/capacity; the caller backs off explicitly.
**/
bool Nav_SubmitActorQueryMainThread( svg_nav_request_t &request, const int32_t start, const int32_t goal,
	const nav_path_policy_t &policy, const nav_query_cost_policy_t &cost, const std::span<const uint32_t> marks,
	const uint32_t stamp, const Vector3DP &feet, const Vector3DP &target, const nav_query_identity_t &identity,
	const uint32_t priority, const int32_t group, uint64_t *out_earliest_tick ) {
	/**
	*	Only production services can accept actor keys; preparation copies all borrowed inputs now.
	**/
	if ( !s_actor_service || s_worker == nullptr || s_worker_snapshot == nullptr ||
		 !Nav_PreparePathRequestMainThread( s_worker_snapshot->View(), request, start, goal, policy, cost,
			marks, stamp, &feet, &target, identity ) ) {
		return false;
	}
	// Delay is a bounded scalar transport gate; the worker never reads game time or cvars.
	const uint64_t delay = static_cast<uint64_t>( std::clamp( s_actor_delay_ticks->integer, 1, 20 ) );
	const uint64_t earliestTick = static_cast<uint64_t>( level.frameNumber ) + delay;
	// Publish the exact scalar deadline to optional owner-only evidence before exposing the copied request.
	if ( out_earliest_tick != nullptr ) {
		*out_earliest_tick = earliestTick;
	}
	return s_worker->Submit( request, priority, group, earliestTick );
}

/**
*	@brief	Wake once after entity/crowd updates finish collecting this tick's requests.
**/
void Nav_QueryWorkerEndFrameMainThread( void ) {
	if ( s_worker != nullptr ) {
		s_worker->Publish();
	}
}

/**
*	@brief	Report copied service metrics without reading mutable worker context data.
*	@note	Owner-only diagnostics include the maximum serviced queue age and bounded storage high-water marks.
**/
void Nav_QueryWorkerStatusCommand( void ) {
	/**
	*	An inactive service owns no query storage or queue-age population to report.
	**/
	if ( s_worker == nullptr ) {
		gi.dprintf( "[nav worker status] active=0\n" );
		return;
	}
	/**
	*	Copy synchronized counters, then format outside the service mutex and measured gameplay scopes.
	**/
	const auto stats = s_worker->Stats();
	gi.dprintf( "[nav worker status] active=1 submitted=%" PRIu64 " completed=%" PRIu64 " drained=%" PRIu64
		" saturated=%" PRIu64 " obsolete=%" PRIu64 " limited=%" PRIu64 " errors=%" PRIu64 " bytes=%" PRIu64
		" peak_slots=%" PRIu64 " peak_heap=%" PRIu64 " max_solve_ns=%" PRIu64 " max_cancel_ns=%" PRIu64 " wakes=%" PRIu64 " busy_ns=%" PRIu64 " lock_wait_ns=%" PRIu64 " lock_hold_ns=%" PRIu64 " max_queue_age_ns=%" PRIu64 " current_queued=%" PRIu64 " oldest_queued_age_ns=%" PRIu64 " started=%" PRIu64 " coalesced=%" PRIu64 " cancelled=%" PRIu64 "\n",
		stats.submitted, stats.completed, stats.drained, stats.saturated, stats.obsolete, stats.limited, stats.errors,
		stats.retained_bytes, stats.peak_occupied, stats.peak_heap, stats.maximum_solve_ns, stats.maximum_cancel_ns, stats.wakes, stats.busy_ns, stats.maximum_lock_wait_ns, stats.maximum_lock_hold_ns, stats.maximum_age_ns, stats.current_queued, stats.oldest_queued_age_ns, stats.started, stats.coalesced, stats.cancelled );
}

/**
*	@brief	Create one synthetic scalar request from an immutable copied graph.
*	@note	No actor is changed; IDs use a separate diagnostic range and never commit movement.
**/
static svg_nav_request_t Nav_WorkerDiagnosticRequest( const nav_query_mesh_view_t &view, const uint32_t index, const uint64_t sequence ) {
	svg_nav_request_t request;
	request.start_face = static_cast<int32_t>( ( index * 17u ) % view.faces.size() );
	request.goal_face = static_cast<int32_t>( ( index * 53u + 1u ) % view.faces.size() );
	request.start = view.faces[ request.start_face ].center;
	request.goal = view.faces[ request.goal_face ].center;
	request.identity.entity = static_cast<int32_t>( 1024u + index );
	request.identity.sequence = sequence;
	request.identity.incarnation = 1;
	request.identity.epoch = Nav_IdentityEpochMainThread();
	request.identity.generation = view.generation;
	request.identity.topology = view.topology;
	return request;
}

/**
*	@brief	Exercise native transport against the identical independently owned synchronous graph.
*	@note	Explicit diagnostic wait is bounded; normal frame submission/consumption never waits.
**/
void Nav_QueryWorkerCheckCommand( void ) {
	// Synthetic entity keys require exclusive diagnostic mode, never a production service.
	if ( Nav_IsActorQueryWorkerEnabledMainThread() ) {
		gi.dprintf( "[nav worker diagnostic] rejected: set nav_actor_worker 0 first.\n" );
		return;
	}
	Nav_StopQueryWorkerMainThread();
	if ( !Nav_StartQueryWorkerMainThread() ) {
		gi.dprintf( "[nav worker check] verdict=FAIL reason=start\n" );
		return;
	}
	const auto view = s_worker_snapshot->View();
	bool ok = true;
	uint32_t compared = 0;
	uint64_t maximum_submit_ns = 0, maximum_sync_ns = 0, maximum_work = 0, maximum_heap = 0, maximum_corridor = 0;
	svg_nav_query_context_t reference_context;
	reference_context.states.resize( view.faces.size() );
	reference_context.heap.reserve( 65537 );
	svg_nav_search_result_t reference;
	reference.corridor.reserve( view.faces.size() );
	/**
	*	Measure deterministic geometry/cost searches before selecting the final supported budgets.
	**/
	for ( uint32_t sample = 0; sample < 1024; ++sample ) {
		const auto request = Nav_WorkerDiagnosticRequest( view, sample, 1 );
		nav_query_execution_control_t control;
		const auto start = std::chrono::steady_clock::now();
		Nav_QueryFindPath( view, reference_context, request, reference, &control );
		const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>( std::chrono::steady_clock::now() - start ).count();
		maximum_sync_ns = std::max( maximum_sync_ns, static_cast<uint64_t>( elapsed ) );
		maximum_work = std::max( maximum_work, control.work );
		maximum_heap = std::max( maximum_heap, reference_context.peak_heap );
		maximum_corridor = std::max( maximum_corridor, static_cast<uint64_t>( reference.corridor.size() ) );
	}
	/**
	*	Fill all slots before publication; queued coalescing cannot race a running solver.
	**/
	for ( uint32_t index = 0; index < 64; ++index ) {
		const auto request = Nav_WorkerDiagnosticRequest( view, index, 1 );
		const auto start = std::chrono::steady_clock::now();
		ok = s_worker->Submit( request, index % 4u, static_cast<int32_t>( index % 3u ) ) && ok;
		maximum_submit_ns = std::max( maximum_submit_ns, static_cast<uint64_t>(
			std::chrono::duration_cast<std::chrono::nanoseconds>( std::chrono::steady_clock::now() - start ).count() ) );
	}
	ok = s_worker->Submit( Nav_WorkerDiagnosticRequest( view, 0, 2 ), 0, 0 ) && ok;
	ok = !s_worker->Submit( Nav_WorkerDiagnosticRequest( view, 64, 1 ), 0, 0 ) && ok;
	s_worker->Publish();
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds( 10 );
	while ( compared < 64 && std::chrono::steady_clock::now() < deadline ) {
		const auto reply = s_worker->TakeReady();
		if ( reply.result == nullptr ) {
			std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
			continue;
		}
		const uint32_t index = static_cast<uint32_t>( reply.result->identity.entity - 1024 );
		const auto request = Nav_WorkerDiagnosticRequest( view, index, index == 0 ? 2 : 1 );
		Nav_QueryFindPath( view, reference_context, request, reference );
		ok = reply.result->status == reference.status && reply.result->corridor == reference.corridor &&
			reply.result->cost == reference.cost && reply.result->identity.sequence == request.identity.sequence && ok;
		s_worker->ReleaseReady( reply );
		++compared;
	}
	const auto stats = s_worker->Stats();
	ok = ok && compared == 64 && stats.coalesced == 1 && stats.saturated == 1 && stats.errors == 0 && stats.limited == 0;
	gi.dprintf( "[nav worker budget] faces=%zu edges=%zu snapshot_bytes=%" PRIu64 " transport_bytes=%" PRIu64
		" request_bytes=%zu result_bytes=%zu sampled=1024 max_sync_ns=%" PRIu64 " max_work=%" PRIu64 " max_heap=%" PRIu64 " max_corridor=%" PRIu64 " max_submit_ns=%" PRIu64 "\n",
		view.faces.size(), view.edges.size(), s_worker_snapshot->RetainedBytes(), stats.retained_bytes,
		sizeof( svg_nav_request_t ) + 4096 + 512, sizeof( svg_nav_search_result_t ) + 4096 * sizeof( int32_t ) + 512,
		maximum_sync_ns, maximum_work, maximum_heap, maximum_corridor, maximum_submit_ns );
	Nav_QueryWorkerStatusCommand();
	Nav_StopQueryWorkerMainThread();
	gi.dprintf( "[nav worker check] verdict=%s compared=%" PRIu32 " saturation=1 coalescing=1\n", ok ? "PASS" : "FAIL", compared );
}

/**
*	@brief	Finish copied-query and borrowed-collision readers before engine lifetime replacement.
**/
void Nav_DrainNavigationReadersMainThread( void ) {
	// Never hold transport/progress locks while waiting for either reader to acknowledge stop.
	Nav_StopQueryWorkerMainThread();
	Nav_DrainAsyncGenerationMainThread();
}

/**
*	@brief	Invalidate an actor's queued/running reply through the bounded transport.
**/
void Nav_CancelQueryWorkerEntityMainThread( const int32_t entity ) {
	// Actor lifecycle changes cannot cancel a synthetic diagnostic with the same numeric key.
	if ( s_actor_service && s_worker != nullptr ) {
		s_worker->CancelEntity( entity );
	}
}

/**
*	@brief	Publish a busy copied-graph batch and observe a running reader before lifetime stress.
*	@note	Explicit diagnostic wait is at most 10ms; normal frame scheduling never waits.
**/
void Nav_QueryWorkerQueueCommand( void ) {
	// Synthetic entity keys require exclusive diagnostic mode, never a production service.
	if ( Nav_IsActorQueryWorkerEnabledMainThread() ) {
		gi.dprintf( "[nav worker diagnostic] rejected: set nav_actor_worker 0 first.\n" );
		return;
	}
	Nav_StopQueryWorkerMainThread();
	if ( !Nav_StartQueryWorkerMainThread() ) {
		gi.dprintf( "[nav worker busy] active=0 observed=0\n" );
		return;
	}
	const auto view = s_worker_snapshot->View();
	bool ok = true;
	for ( uint32_t index = 0; index < 64; ++index ) {
		ok = s_worker->Submit( Nav_WorkerDiagnosticRequest( view, index, 1 ), index % 4u, static_cast<int32_t>( index % 3u ) ) && ok;
	}
	s_worker->Publish();
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds( 10 );
	nav_worker_stats_t stats;
	do {
		stats = s_worker->Stats();
		if ( stats.started > stats.completed ) {
			break;
		}
		std::this_thread::yield();
	} while ( std::chrono::steady_clock::now() < deadline );
	gi.dprintf( "[nav worker busy] active=1 observed=%d started=%" PRIu64 " completed=%" PRIu64 "\n",
		ok && stats.started > stats.completed, stats.started, stats.completed );
}
