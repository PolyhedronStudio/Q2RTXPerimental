// Minimal stub implementations to satisfy linker when full monster puppet TU
// is not compiled into the target. These are intentionally simple and
// conservative: they provide basic defaults so other translation units
// can link successfully during incremental builds.

#include "svgame/svg_local.h"
#include "svgame/nav/svg_nav.h"

#include "svgame/svg_local.h"
#include "svgame/svg_entity_events.h"
#include "svgame/svg_misc.h"
#include "svgame/svg_trigger.h"
#include "svgame/svg_utils.h"

// TODO: Move elsewhere.. ?
#include "refresh/shared_types.h"

// Entities
#include "sharedgame/sg_entities.h"

// SharedGame UseTargetHints.
#include "sharedgame/sg_usetarget_hints.h"

// Monster Move
#include "svgame/monsters/svg_mmove.h"
#include "svgame/monsters/svg_mmove_slidemove.h"

// Player trail (Q2/Q2RTX pursuit trail)
#include "svgame/player/svg_player_trail.h"

// TestDummy Monster
#include "svgame/entities/monster/svg_monster_testdummy_debug.h"

// Navigation cluster routing (coarse tile routing pre-pass).
#include "svgame/nav/svg_nav_clusters.h"
// Async navigation queue helpers.
#include "svgame/nav/svg_nav_request.h"
// Traversal helpers required for path invalidation.
#include "svgame/nav/svg_nav_traversal.h"

// Local debug toggle for noisy per-frame prints in this test monster.
#ifndef DEBUG_PRINTS
#define DEBUG_PRINTS 1
#endif

/**
*    Debug compile-time toggle for route-filter isolation.
*        When enabled, this debug monster disables the coarse cluster tile-route
*        restriction so A* neighbor diagnostics reflect pure StepTest behavior.
**/
#ifndef MONSTER_TESTDUMMY_DEBUG_BYPASS_ROUTE_FILTER
//#define MONSTER_TESTDUMMY_DEBUG_BYPASS_ROUTE_FILTER 1
#endif

#ifdef DEBUG_PRINTS
static constexpr int32_t DUMMY_NAV_DEBUG = 1;
#else
static constexpr int32_t DUMMY_NAV_DEBUG = 0;
#endif

// Provide nav-agent bbox helper (fallback used when puppet TU not linked).
//void SVG_Monster_GetNavigationAgentBounds( const svg_monster_testdummy_debug_t *ent, Vector3 *out_mins, Vector3 *out_maxs ) {
//    if ( !out_mins || !out_maxs ) {
//        return;
//    }
//    // If we have a live navmesh, prefer its agent bbox.
//    const nav_mesh_t *mesh = g_nav_mesh.get();
//    if ( mesh && ( mesh->agent_maxs.z > mesh->agent_mins.z ) ) {
//        *out_mins = mesh->agent_mins;
//        *out_maxs = mesh->agent_maxs;
//        return;
//    }
//    // Fallback to the entity collision bounds when available.
//    if ( ent ) {
//        *out_mins = ent->mins;
//        *out_maxs = ent->maxs;
//        return;
//    }
//    // Last-resort: reasonable default box.
//    *out_mins = Vector3{ -16.f, -16.f, -36.f };
//    *out_maxs = Vector3{ 16.f, 16.f, 36.f };
//}
void SVG_Monster_ResetNavigationPath( svg_monster_testdummy_debug_t *self );
bool SVG_Monster_TryQueueNavigationRebuild( svg_monster_testdummy_debug_t *self, const Vector3 &start_origin,
	const Vector3 &goal_origin, const svg_nav_path_policy_t &policy, const Vector3 &agent_mins,
		const Vector3 &agent_maxs, const bool force = false ) {
		/**
		*    @brief	Attempt to enqueue an asynchronous navigation rebuild for this entity.
		*    @note	Lightweight diagnostic logging is emitted when DUMMY_NAV_DEBUG is enabled
		*
		*    Guard: only enqueue when the async queue mode is explicitly enabled.
		**/
	if ( !self || !nav_nav_async_queue_mode || nav_nav_async_queue_mode->integer == 0 ) {
		if ( DUMMY_NAV_DEBUG ) {
			gi.dprintf( "[DEBUG] TryQueueNavRebuild: async queue mode disabled, cannot enqueue. ent=%d\n", self ? self->s.number : -1 );
		}
		return false;
	}

	if ( !SVG_Nav_IsAsyncNavEnabled() ) {
		if ( DUMMY_NAV_DEBUG ) {
			gi.dprintf( "[DEBUG] TryQueueNavRebuild: async nav globally disabled, ent=%d\n", self->s.number );
		}
		return false;
	}

	/**
	*    Throttle guard:
	*        If the path process is not allowed to rebuild yet, skip enqueuing and
	*        let callers keep following the current path without forcing sync rebuilds.
	**/
	// Force bypass ensures explicit breadcrumb goals always queue new work.
	if ( !force && !self->navigationState.pathProcess.CanRebuild( policy ) ) {
		// Movement throttled/backoff prevents enqueuing now; callers should keep using current path.
		if ( DUMMY_NAV_DEBUG ) {
			gi.dprintf( "[DEBUG] TryQueueNavRebuild: CanRebuild() == false, throttled/backoff. ent=%d next_rebuild=%lld backoff_until=%lld\n",
				self->s.number, ( long long )self->navigationState.pathProcess.next_rebuild_time.Milliseconds(), ( long long )self->navigationState.pathProcess.backoff_until.Milliseconds() );
		}
		return true;
	}

	/**
	*    Movement heuristic:
	*        Only queue rebuilds when the path process thinks goal/start movement
	*        warrants it; this prevents re-queueing every frame for static goals.
	**/
	const bool movementWarrantsRebuild =
		self->navigationState.pathProcess.ShouldRebuildForGoal2D( goal_origin, policy )
		|| self->navigationState.pathProcess.ShouldRebuildForGoal3D( goal_origin, policy )
		|| self->navigationState.pathProcess.ShouldRebuildForStart2D( start_origin, policy )
		|| self->navigationState.pathProcess.ShouldRebuildForStart3D( start_origin, policy );
	// Force bypass ensures explicit breadcrumb goals bypass movement heuristics.
	if ( !force && !movementWarrantsRebuild ) {
		if ( DUMMY_NAV_DEBUG ) {
			gi.dprintf( "[DEBUG] TryQueueNavRebuild: movement does not warrant rebuild, ent=%d\n", self->s.number );
		}
		return true;
	}

	/**
	*    Allow the request queue to deduplicate / refresh existing requests.
	*    Instead of bailing out early we call the enqueue helper which will
	*    refresh an existing entry or create a new one. This ensures the
	*    path_process pending handle is always up-to-date and avoids the
	*    repeated "request already pending" spam seen in logs.
	**/
	// If a request is already pending, allow the async queue helper to
	// deduplicate / refresh the existing entry rather than bailing out early.
	// This avoids the repeated "request already pending" spam and ensures
	// up-to-date goals get applied to the outstanding queued entry.
	if ( SVG_Nav_IsRequestPending( &self->navigationState.pathProcess ) ) {
		if ( force ) {
			// When forced, cancel the outstanding request so a fresh entry is
			// created immediately with the force flag set.
			SVG_Monster_ResetNavigationPath( self );
		} else {
			// If a pending handle exists for this process and our movement
			// heuristics do not warrant a rebuild, skip refreshing the
			// in-flight request. This comparison is per-handle so callers
			// that intentionally replaced the pending handle are not blocked.
			if ( self->navigationState.pathProcess.pending_request_handle != 0 && !movementWarrantsRebuild ) {
				if ( DUMMY_NAV_DEBUG ) {
					gi.dprintf( "[DEBUG] TryQueueNavRebuild: skipping refresh because pending_handle=%d and movement doesn't warrant it. ent=%d\n",
						self->navigationState.pathProcess.pending_request_handle, self->s.number );
				}
				return true;
			}
		}
	}

	/**
	*  Stronger protection: when a request is already running (rebuild_in_progress)
	*  small changes to the start position should not trigger a refresh. The
	*  nav queue already defers re-init via `needs_refresh` but callers that
	*  drift the start each frame can still cause repeated worker prep. Here
	*  we ignore small start deltas while a request is running so the in-flight
	*  search can make progress.
	**/
	//if ( !force && self->navigationState.pathProcess.rebuild_in_progress ) {
	//    // Use the last prep start as a stable reference if available.
	//    const Vector3 referenceStart = ( self->navigationState.pathProcess.last_prep_time > 0_ms ) ? self->navigationState.pathProcess.last_prep_start : self->navigationState.pathProcess.path_start_position;
	//    const double startDx = QM_Vector3LengthDP( QM_Vector3Subtract( start_origin, referenceStart ) );
	//    // Threshold chosen to ignore small per-frame motion but still react to real relocation.
	//    constexpr double startIgnoreThreshold = 16.0; // units
	//    if ( startDx <= startIgnoreThreshold ) {
	//        if ( DUMMY_NAV_DEBUG ) {
	//            gi.dprintf( "[DEBUG] TryQueueNavRebuild: suppressed refresh while running (startDx=%.2f <= %.2f) ent=%d\n",
	//                startDx, startIgnoreThreshold, self->s.number );
	//        }
	//        return true;
	//    }
	//}

	/**
	*	Enqueue the rebuild request and record the handle for diagnostics.
	*	Note: If a request is already pending and actively being prepared / running
	*	(this entity's `navigationState.pathProcess.rebuild_in_progress == true`), this helper
	*	will avoid refreshing the existing running entry unless `force == true`
	*	or the movement heuristics indicate a rebuild is warranted. This prevents
	*	repeated re-initialization of in-flight searches which can starve the
	*	incremental A* stepper and cause visible steering issues.
	**/
	// Propagate a small per-request start-ignore threshold to the async queue so
	// the queue can avoid re-preparing entries when the start drifts slightly
	// while a search is running. We choose the same 16-unit threshold used
	// locally above to keep behavior consistent.
	constexpr double startIgnoreThresholdForQueue = 16.0;
	const nav_request_handle_t handle = SVG_Nav_RequestPathAsync( &self->navigationState.pathProcess, start_origin, goal_origin, policy, agent_mins, agent_maxs, force, startIgnoreThresholdForQueue );
	if ( handle <= 0 ) {
		if ( DUMMY_NAV_DEBUG ) {
			gi.dprintf( "[DEBUG] TryQueueNavRebuild: enqueue failed (handle=%d) ent=%d\n", handle, self->s.number );
		}
		return false;
	}

	// Record that a rebuild is in progress for diagnostics and possible cancellation.
	// Note: the request queue will have already set the process markers during
	// PrepareAStarForEntry when the entry transitions to Running. Setting them
	// here ensures the entity has the handle immediately for early cancellation
	// if the caller chooses to abort before the queue tick processes it.
	self->navigationState.pathProcess.rebuild_in_progress = true;
	self->navigationState.pathProcess.pending_request_handle = handle;
	if ( DUMMY_NAV_DEBUG ) {
		gi.dprintf( "[DEBUG] TryQueueNavRebuild: queued rebuild handle=%d ent=%d force=%d\n", handle, self->s.number, force ? 1 : 0 );
		// Also print the converted nav-center origins so we can correlate node resolution.
		const nav_mesh_t *mesh = g_nav_mesh.get();
		if ( mesh ) {
			const Vector3 start_center = SVG_Nav_ConvertFeetToCenter( mesh, start_origin, &agent_mins, &agent_maxs );
			const Vector3 goal_center = SVG_Nav_ConvertFeetToCenter( mesh, goal_origin, &agent_mins, &agent_maxs );
			gi.dprintf( "[DEBUG] TryQueueNavRebuild: start=(%.1f %.1f %.1f) start_center=(%.1f %.1f %.1f) goal=(%.1f %.1f %.1f) goal_center=(%.1f %.1f %.1f)\n",
				start_origin.x, start_origin.y, start_origin.z,
				start_center.x, start_center.y, start_center.z,
				goal_origin.x, goal_origin.y, goal_origin.z,
				goal_center.x, goal_center.y, goal_center.z );
		} else {
			gi.dprintf( "[DEBUG] TryQueueNavRebuild: start=(%.1f %.1f %.1f) goal=(%.1f %.1f %.1f) (no mesh)\n",
				start_origin.x, start_origin.y, start_origin.z,
				goal_origin.x, goal_origin.y, goal_origin.z );
		}
	}
	return true;
}

void SVG_Monster_ResetNavigationPath( svg_monster_testdummy_debug_t *self ) {
	/**
	*    Sanity check: require a valid entity before touching path state.
	**/
	if ( !self ) {
		return;
	}

	/**
	*    Cancel any pending async request so we do not reuse stale results.
	**/
	if ( self->navigationState.pathProcess.pending_request_handle > 0 ) {
		SVG_Nav_CancelRequest( self->navigationState.pathProcess.pending_request_handle );
	}

	/**
	*    Clear cached path buffers and reset traversal bookkeeping.
	**/
	SVG_Nav_FreeTraversalPath( &self->navigationState.pathProcess.path );
	self->navigationState.pathProcess.path_index = 0;
	self->navigationState.pathProcess.path_goal_position = {};
	self->navigationState.pathProcess.path_start_position = {};
	self->navigationState.pathProcess.rebuild_in_progress = false;
	self->navigationState.pathProcess.pending_request_handle = 0;
}