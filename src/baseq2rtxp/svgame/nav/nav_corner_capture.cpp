#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"
#include "svgame/entities/monster/svg_monster_base.h"
#include "svgame/entities/monster/svg_monster_testdummy_debug.h"
#include <nlohmann/json.hpp>
#include "svgame/nav/nav_corner_capture.h"
#include "svgame/nav/nav_debug_draw.h"
#include "svgame/crowd/svg_crowd_manager.h"

#include <array>
#include <charconv>
#include <cinttypes>
#include <cstring>
#include <cstdarg>
#include <cstdio>

/**
*	@brief	Immutable copied evidence for one selected monster movement update.
**/
struct nav_corner_frame_t {
	//! Server simulation timestamp and fixed tick duration in seconds.
	double seconds = 0.0;
	//! Bbox-origin command start, predicted endpoint and realized endpoint.
	Vector3DP start, predicted, realized;
	//! Unmodified command velocity before authoritative slide/step resolution.
	Vector3DP command;
	//! Native shape bounds for feet/center conversions.
	Vector3 mins, maxs;
	//! Explicit mover primitive, including held frames without a native candidate.
	int32_t shape = MM_SHAPE_AUTO;
	//! Current active and adjacent route points in exact feet coordinates.
	Vector3DP previousWaypoint, waypoint, nextWaypoint;
	//! Mesh/topology identity; face/edge witnesses are valid only for these revisions.
	uint64_t mesh = 0, topology = 0;
	//! Active waypoint index after this tick's release decision.
	size_t index = 0;
	//! Direct physical candidate outcome and its internal native trace count.
	svg_nav_movement_result_t validation;
	//! Geometric rejection/witness must be distinguished from an unexecuted physical phase.
	nav_segment_clearance_t geometric;
	//! Total internal native validation sweeps, including rejected candidates and releases.
	uint32_t totalValidationTraces = 0;
	//! Accepted-trial counts exclude the discarded ground/elevated alternative.
	mm_motion_diagnostics_t accepted, ground, elevated;
	//! Final flags do not replace direct clip counts.
	int32_t blocked = 0;
	//! Accepted route/goal/crowd-order identities and semantic constraint copied at observation time.
	uint64_t route = 0, goal = 0, acceptedGoal = 0, crowdOrder = 0;
	//! Copied admission state and finite stop-plane geometry for reserved aperture oracles.
	bool ingress = false, ingressReleased = false, egress = false, egressReleased = false;
	Vector3DP portalOrigin = {}, portalInward = {};
	double portalHalfWidth = 0.0, releaseDepth = 0.0;
	//! Current origin-aperture geometry and pool-validated cleared-prefix ownership.
	bool egressCleared = false;
	//! Copied origin portal and outward flow for strict realized re-entry diagnostics.
	Vector3DP egressPortalOrigin = {}, egressPortalOutward = {};
	//! Full native exit aperture and required clearance depth.
	double egressHalfWidth = 0.0, egressClearanceDepth = 0.0;

	svg_nav_waypoint_t constraint;
	//! Owned structured release, named steering stages and immutable traversal policy.
	svg_nav_release_t release;
	svg_nav_steering_t steering;
	//! Tick-local fired callback route/index/final audit; no route pointers survive.
	std::array<uint64_t, 3> eventRoutes;
	std::array<uint32_t, 3> eventIndices;
	std::array<bool, 3> eventFinal;
	uint32_t eventCount = 0;
	nav_path_policy_t policy;
	//! Entity incarnation, AI state, animation frame, post-command yaw and causal fixture mode.
	int32_t entity = -1, spawn = -1, ai = -1, animation = 0;
	double yawAfter = 0.0;
	uint32_t mode = 0;
	//! Copied request/accepted ownership distinguishes pending work from realized arrival in M7 evidence.
	nav_query_identity_t requestIdentity, acceptedIdentity;
	uint32_t queryState = 0;
	//! Actual owner clock and latest copied request timeline used by deterministic delayed-result assertions.
	uint64_t tick = 0, submittedTick = 0, earliestDeliveryTick = 0;
	//! Separate rejection signal for held command/replan diagnosis.
	bool rejected = false;
};

/**
*	@brief	Bounded immutable accepted-route evidence copied only when an opt-in route revision changes.
*	@note	Storage is prepared by the capture command; overflow is reported as incomplete evidence.
**/
struct nav_corner_route_snapshot_t {
	//! Accepted revision and its mesh/topology/goal identity.
	uint64_t revision = 0, mesh = 0, topology = 0, goal = 0;
	//! Exact final route and original A* face centers, with explicit valid counts.
	std::array<svg_nav_waypoint_t, 512> waypoints;
	std::array<Vector3DP, 512> corridor;
	uint32_t waypoint_count = 0, corridor_count = 0;
	//! Complete installation result supplies route length, clearance and native work counts.
	nav_route_validation_t validation;
};
//! Optional route snapshots use command-time allocation and bounded movement-time copies.
static svg_vector_t<nav_corner_route_snapshot_t, TAG_SVGAME_NAVMESH> s_corner_routes;
//! Valid route snapshots and explicit overflow state prevent incomplete evidence from passing acceptance.
static size_t s_corner_route_count = 0;
//! Whether either the route count or a route's fixed geometry capacity was exceeded.
static bool s_corner_route_overflow = false;

//! Fixed single-mover capture storage; no per-entity or per-tick heap allocations.
static std::array<nav_corner_frame_t, 120> s_corner_frames;
//! Optional full-run evidence allocated only by an explicit console command, never during sampling.
static svg_vector_t<nav_corner_frame_t, TAG_SVGAME_NAVMESH> s_corner_full_frames;
//! Valid copied full-run samples; bounded by the opt-in command budget of 8192 (including artificial delayed-crowd controls).
static size_t s_corner_full_count = 0;
//! Selected stable slot, or -1 while capture is disabled.
static int32_t s_corner_entity = -1;
//! Selected engine spawn count prevents evidence from a recycled entity slot.
static int32_t s_corner_spawn = -1;
//! Write index and valid retained frame count for the fixed ring.
static size_t s_corner_write = 0, s_corner_count = 0;
//! Observation budget before contact, then a bounded 40-frame contact/post-contact window.
static int32_t s_corner_remaining = 0, s_corner_post_remaining = -1;
//! Whether a contact/rejection trigger was observed in this capture.
static bool s_corner_triggered = false;
//! Explicit lifecycle invalidation keeps partial old evidence distinguishable from a complete capture.
static bool s_corner_invalidated = false;
//! Copied map identity for report-time comparison across level changes.
static char s_corner_map[ MAX_QPATH ] = {};

/**
*	@brief	Invalidate opt-in roster audit ownership before navigation lifetimes change.
**/
static void Nav_Corner_InvalidateAudit();

/**
*	@brief	Flush capture lifetime before navigation clear or same-map reload can reuse an entity incarnation.
*	@note	Retained copied evidence remains reportable but is explicitly incomplete; no stale sampling continues.
**/
void Nav_Corner_InvalidateCapture() {
	Nav_Corner_InvalidateAudit();
	// Only a selected capture needs invalidation; ordinary map clear has no diagnostic work.
	if ( s_corner_entity < 0 ) {
		return;
	}
	s_corner_invalidated = true;
	s_corner_remaining = 0;
	s_corner_full_frames.resize( s_corner_full_count );
}

/**
*	@brief	Parse a nonnegative command integer without accepting trailing garbage or overflow.
*	@param	text	Null-terminated command argument.
*	@param	value	Output integer.
*	@return	True for a complete representable nonnegative argument.
**/
static bool Nav_Corner_ParseInteger( const char *text, int32_t &value ) {
	/**
	*	Reject absent/empty arguments before the bounded integer conversion.
	**/
	if ( text == nullptr || text[ 0 ] == '\0' ) {
		return false;
	}
	const char *end = text + std::strlen( text );
	const auto parsed = std::from_chars( text, end, value );
	return parsed.ec == std::errc{} && parsed.ptr == end && value >= 0;
}

/**
*	@brief	Select one live compiled monster for a bounded no-allocation movement capture.
*	@note	The original problematic corner is never inferred from arbitrary coordinates.
**/
void Nav_Corner_CaptureCommand() {
	/**
	*	Validate exact arguments and active monster lifetime before resetting prior evidence.
	**/
	int32_t entity = -1, frames = 0, full = 0;
	if ( ( gi.argc() != 4 && gi.argc() != 5 ) || !Nav_Corner_ParseInteger( gi.argv( 2 ), entity ) ||
		 !Nav_Corner_ParseInteger( gi.argv( 3 ), frames ) || frames < 1 || frames > 8192 ||
		 ( gi.argc() == 5 && ( !Nav_Corner_ParseInteger( gi.argv( 4 ), full ) || full > 1 ) ) ) {
		gi.cprintf( nullptr, PRINT_HIGH, "Usage: sv nav_corner_capture <entityNum> <frameCount:1..8192> [fullRun:0|1]\n" );
		return;
	}
	svg_base_edict_t *base = g_edict_pool.EdictForNumber( entity );
	const auto *monster = dynamic_cast<svg_monster_base_t *>( base );
	// Capture only active monster instances; casts and slot lookup do not establish aliveness alone.
	if ( monster == nullptr || !SVG_Entity_IsActive( monster ) ) {
		gi.cprintf( nullptr, PRINT_HIGH, "nav_corner_capture: entity is not an active monster.\n" );
		return;
	}

	/**
	*	Copy identity and reset ring indices; prior samples are ignored without clearing 120 records.
	**/
	s_corner_invalidated = false;
	s_corner_entity = entity;
	s_corner_spawn = monster->spawn_count;
	s_corner_remaining = frames;
	s_corner_post_remaining = -1;
	s_corner_triggered = false;
	s_corner_write = s_corner_count = 0;
	// Allocate the entire optional budget now; movement copies cannot grow this storage.
	s_corner_full_frames.clear();
	s_corner_full_count = 0;
	s_corner_routes.clear();
	s_corner_route_count = 0;
	s_corner_route_overflow = false;
	if ( full != 0 ) {
		// Forty observed crowd revisions exceeded the old 32-slot history; retain measured headroom with a fixed ceiling.
		constexpr size_t routeHistoryCapacity = 128;
		s_corner_routes.resize( std::min( routeHistoryCapacity, static_cast<size_t>( frames ) ) );
		s_corner_full_frames.resize( static_cast<size_t>( frames ) );
	}
	Q_strlcpy( s_corner_map, level.mapname, sizeof( s_corner_map ) );
	gi.cprintf( nullptr, PRINT_HIGH, "nav_corner_capture: entity=%" PRId32 " spawn=%" PRId32 " budget=%" PRId32 " ring=120 routes=%zu route_bytes=%zu; report with sv nav_corner_report %" PRId32 ".\n",
		entity, s_corner_spawn, frames, s_corner_routes.size(), s_corner_routes.size() * sizeof( nav_corner_route_snapshot_t ), entity );
}

/**
*	@brief	Copy selected realized movement into a fixed ring and freeze after the contact window.
*	@param	monster	Mover after authoritative movement.
*	@param	startOrigin	BBox origin before authoritative movement.
*	@param	commandVelocity	Original command, independent of clipped result velocity.
*	@param	blockedMask	Final movement flags.
*	@note	Trigger includes rejected commands; the report separates rejection from actual accepted-wall clips.
**/
void Nav_Corner_RecordMovement( const svg_monster_base_t &monster, const Vector3DP &startOrigin,
	const Vector3DP &commandVelocity, const int32_t blockedMask ) {
	/**
	*	Disabled captures and different/recycled entity slots do no record work.
	**/
	const bool fullActive = s_corner_full_count < s_corner_full_frames.size();
	if ( ( s_corner_remaining <= 0 && !fullActive ) || monster.s.number != s_corner_entity ) {
		return;
	}
	// Never mix evidence from another spawn or level into the selected mover's ring.
	if ( monster.spawn_count != s_corner_spawn || std::strcmp( level.mapname, s_corner_map ) != 0 ) {
		s_corner_remaining = 0;
		s_corner_full_frames.resize( s_corner_full_count );
		return;
	}

	/**
	*	Copy owned scalar/vector records; no pointers into routes or temporary physics buffers survive.
	**/
	nav_corner_frame_t &frame = fullActive ? s_corner_full_frames[ s_corner_full_count++ ] : s_corner_frames[ s_corner_write ];
	frame = {};
	frame.seconds = level.time.Seconds();
	frame.start = startOrigin;
	frame.command = commandVelocity;
	frame.predicted = startOrigin + commandVelocity * FRAME_TIME_S.Seconds();
	frame.realized = Vector3DP( monster.currentOrigin );
	frame.mins = monster.mins;
	frame.maxs = monster.maxs;
	frame.shape = static_cast<int32_t>( SVG_MMove_GetNativeShape( &monster ) );
	frame.mesh = g_nav_mesh_generation;
	frame.topology = g_nav_topology_revision;
	frame.requestIdentity = monster.navigationGraphWorkspace.request.identity;
	frame.acceptedIdentity = monster.navigationAcceptedIdentity;
	frame.queryState = static_cast<uint32_t>( monster.navigationQueryState );
	frame.tick = static_cast<uint64_t>( level.frameNumber );
	frame.submittedTick = monster.navigationSubmittedTick;
	frame.earliestDeliveryTick = monster.navigationEarliestDeliveryTick;

	frame.index = monster.stringPathPos;
	frame.entity = monster.s.number;
	frame.spawn = monster.spawn_count;
	frame.route = monster.navigationRouteRevision;
	/**
	*	Copy each accepted route once; no pointers or generation-dependent lookup survive the movement sample.
	**/
	if ( fullActive && !monster.navigationWaypoints.empty() &&
		 ( s_corner_route_count == 0 || s_corner_routes[ s_corner_route_count - 1 ].revision != frame.route ) ) {
		// Refuse to truncate geometry or exceed the declared route-history bound behind a passing capture.
		if ( s_corner_route_count == s_corner_routes.size() || monster.navigationWaypoints.size() > 512 || monster.navPath.size() > 512 ) {
			s_corner_route_overflow = true;
		} else {
			auto &snapshot = s_corner_routes[ s_corner_route_count++ ];
			snapshot.revision = frame.route;
			snapshot.mesh = frame.mesh;
			snapshot.topology = frame.topology;
			snapshot.goal = monster.navigationAcceptedGoalRevision;
			snapshot.validation = monster.lastRouteValidation;
			snapshot.waypoint_count = static_cast<uint32_t>( monster.navigationWaypoints.size() );
			snapshot.corridor_count = static_cast<uint32_t>( monster.navPath.size() );
			std::copy( monster.navigationWaypoints.begin(), monster.navigationWaypoints.end(), snapshot.waypoints.begin() );
			// Capture the original corridor as coordinates while its face identifiers are valid.
			for ( uint32_t index = 0; index < snapshot.corridor_count; ++index ) {
				const int32_t face = monster.navPath[ index ];
				if ( face < 0 || static_cast<size_t>( face ) >= g_nav_faces.size() ) {
					s_corner_route_overflow = true;
					break;
				}
				snapshot.corridor[ index ] = g_nav_faces[ face ].center;
			}
		}
	}

	frame.goal = monster.navigationGoalRevision;
	frame.acceptedGoal = monster.navigationAcceptedGoalRevision;
	const auto *group = monster.crowd.crowdID >= 0 ? SVG_Crowd_GetGroup( monster.crowd.crowdID ) : nullptr;
	frame.crowdOrder = group != nullptr ? group->orderRevision : 0;
	// Reservation ownership is copied with the realized frame, so the oracle can test exact crossings.
	frame.ingressReleased = monster.crowd.ingressReleased;
	frame.egressReleased = monster.crowd.egressReleased;
	if ( group != nullptr ) {
		frame.ingress = group->hasSerializedIngress;
		frame.egress = group->hasSerializedEgress;
		frame.portalOrigin = group->ingressPortalOrigin;
		frame.portalInward = group->ingressPortalInward;
		frame.portalHalfWidth = group->ingressPortalHalfWidth;
		frame.releaseDepth = group->ingressReleaseDepth;
		// Read the frozen owner key rather than treating exit permission as completed clearance.
		frame.egressCleared = SVG_Crowd_HasClearedEgress( &monster );
		frame.egressPortalOrigin = group->egressPortalOrigin;
		frame.egressPortalOutward = group->egressPortalOutward;
		frame.egressHalfWidth = group->egressPortalHalfWidth;
		frame.egressClearanceDepth = SVG_Crowd_GetEgressClearanceDepth( &monster );
	}

	frame.release = monster.lastWaypointRelease;
	frame.steering = monster.navigationSteering;
	if ( monster.navigationEventFrame == static_cast<uint64_t>( level.frameNumber ) ) {
		frame.eventCount = monster.navigationEventCount;
		frame.eventRoutes = monster.navigationEventRoutes;
		frame.eventIndices = monster.navigationEventIndices;
		frame.eventFinal = monster.navigationEventFinal;
	}
	frame.policy = monster.pathNavigationState.policy;
	// No callback or entity pointers may survive in the capture's immutable policy snapshot.
	frame.policy.edge_cost_callback = nullptr;
	frame.policy.edge_cost_monster = nullptr;
	frame.yawAfter = monster.currentAngles[ YAW ];
	frame.animation = static_cast<int32_t>( monster.s.frame );
	frame.mode = monster.navigationFixtureMode;
	const auto *dummy = dynamic_cast<const svg_monster_testdummy_debug_t *>( &monster );
	frame.ai = dummy != nullptr ? static_cast<int32_t>( dummy->thinkAIState ) : -1;
	if ( frame.index < monster.navigationWaypoints.size() ) {
		frame.constraint = monster.navigationWaypoints[ frame.index ];
	}
	frame.validation = monster.lastMovementValidation;
	frame.geometric = monster.lastGeometricValidation;
	frame.totalValidationTraces = monster.navigationValidationTraceCount;
	frame.accepted = monster.monsterMove.acceptedDiagnostics;
	frame.ground = monster.monsterMove.groundDiagnostics;
	frame.elevated = monster.monsterMove.elevatedDiagnostics;
	frame.blocked = blockedMask;
	frame.rejected = monster.navigationCommandRejected;
	// Record only available adjacent points; report validity from route index/count separately.
	if ( frame.index < monster.stringPulledPath.size() ) {
		frame.waypoint = monster.stringPulledPath[ frame.index ];
		frame.previousWaypoint = monster.stringPulledPath[ frame.index > 0 ? frame.index - 1 : 0 ];
		frame.nextWaypoint = monster.stringPulledPath[ std::min( frame.index + 1, monster.stringPulledPath.size() - 1 ) ];
	}
	/**
	*	The contact ring freezes independently; full-run evidence continues through arrival or timeout.
	**/
	if ( s_corner_remaining <= 0 ) {
		return;
	}
	if ( fullActive ) {
		s_corner_frames[ s_corner_write ] = frame;
	}
	s_corner_write = ( s_corner_write + 1 ) % s_corner_frames.size();
	s_corner_count = std::min( s_corner_count + 1, s_corner_frames.size() );

	/**
	*	Retain up to 80 pre-contact plus 40 contact/post frames; no per-tick output is emitted.
	**/
	const bool trigger = frame.rejected || frame.accepted.static_wall_clips > 0 || frame.accepted.startsolid > 0 ||
		frame.accepted.allsolid > 0 || frame.accepted.recovery_nudges > 0;
	// The first trigger starts a bounded post window even near the observation-budget deadline.
	if ( trigger && !s_corner_triggered ) {
		s_corner_triggered = true;
		s_corner_post_remaining = 40;
	}
	// Contact frame is the first of the forty post-window frames.
	if ( s_corner_triggered ) {
		--s_corner_post_remaining;
		s_corner_remaining = s_corner_post_remaining;
	} else {
		--s_corner_remaining;
	}
}

/**
*	@brief	Write a report record to an explicit file or the interactive console.
*	@param	file	Report file, or nullptr for console output.
*	@param	format	Printf-style record format followed by matching scalar values.
*	@note	Explicit files avoid fragmented engine console-log writes; runs only while dumping a frozen ring.
**/
static void Nav_Corner_Output( FILE *file, const char *format, ... ) {
	/**
	*	Keep report I/O outside sampling; formatting never retains pointers into caller arguments.
	**/
	va_list arguments;
	va_start( arguments, format );
	// File reports use stdio directly so console prefix/buffering cannot corrupt JSON records.
	if ( file != nullptr ) {
		std::vfprintf( file, format, arguments );
	} else {
		char text[ 2048 ];
		std::vsnprintf( text, sizeof( text ), format, arguments );
		gi.cprintf( nullptr, PRINT_HIGH, "%s", text );
	}
	va_end( arguments );
}

/**
*	@brief	Freeze and emit copied frame evidence as JSON lines outside measured movement.
*	@note	Partial pre-contact windows and interrupted captures are reported, never presented as full acceptance.
**/
void Nav_Corner_ReportCommand() {
	/**
	*	Only the explicitly selected entity may access the retained ring.
	**/
	int32_t entity = -1;
	if ( ( gi.argc() != 3 && gi.argc() != 4 ) || !Nav_Corner_ParseInteger( gi.argv( 2 ), entity ) || entity != s_corner_entity ) {
		gi.cprintf( nullptr, PRINT_HIGH, "Usage: sv nav_corner_report <capturedEntityNum> [output.jsonl]\n" );
		return;
	}
	/**
	*	Open the optional explicit report artifact before freezing; preserve evidence on I/O failure.
	**/
	FILE *output = gi.argc() == 4 ? std::fopen( gi.argv( 3 ), "w" ) : nullptr;
	// A requested artifact must never silently fall back to potentially fragmented console logging.
	if ( gi.argc() == 4 && output == nullptr ) {
		gi.cprintf( nullptr, PRINT_HIGH, "nav_corner_report: could not open output file.\n" );
		return;
	}
	s_corner_remaining = 0;
	// Freeze both stores before serialization; only copied valid full-run entries are emitted.
	s_corner_full_frames.resize( s_corner_full_count );
	const size_t reportCount = s_corner_full_count != 0 ? s_corner_full_count : s_corner_count;
	gi.cprintf( nullptr, PRINT_HIGH, "[nav_corner] schema=2 map=%s entity=%" PRId32 " spawn=%" PRId32 " frames=%zu triggered=%d post_remaining=%" PRId32 " build=%s %s; coordinates=bbox-origin, waypoints=feet; visual acceptance pending\n",
		s_corner_map, entity, s_corner_spawn, reportCount, s_corner_triggered ? 1 : 0, s_corner_post_remaining, __DATE__, __TIME__ );

	/**
	*	Emit ring records oldest first; frame flags cannot hide direct accepted-trial clip counters.
	**/
	const size_t oldest = ( s_corner_write + s_corner_frames.size() - s_corner_count ) % s_corner_frames.size();
	// The loop is bounded by 120 retained samples and executes only on the explicit report command.
	for ( size_t offset = 0; offset < reportCount; ++offset ) {
		const auto &f = s_corner_full_count != 0 ? s_corner_full_frames[ offset ] : s_corner_frames[ ( oldest + offset ) % s_corner_frames.size() ];
		const auto vector = []( const Vector3DP &point ) { return nlohmann::json::array( { point.x, point.y, point.z } ); };
		const auto geometry = [&]( const nav_segment_clearance_t &hit ) {
			return nlohmann::json{ { "reason", static_cast<uint32_t>( hit.reason ) }, { "minimum_clearance", hit.minimum_clearance },
				{ "edge", hit.witness_edge }, { "edge_start", vector( hit.witness_start ) }, { "edge_end", vector( hit.witness_end ) },
				{ "mesh", hit.mesh_generation }, { "topology", hit.topology_revision }, { "space", "feet_xy" } };
		};
		const auto native = [&]( const svg_nav_movement_result_t &hit ) {
			nlohmann::json probes = nlohmann::json::array();
			// Emit only performed phases from the fixed five-trace core, including discarded alternatives.
			for ( uint32_t phase = 0; phase < std::min( hit.trace_count, static_cast<uint32_t>( hit.probes.size() ) ); ++phase ) {
				const auto &probe = hit.probes[ phase ];
				const double dx = probe.end.x - probe.realized.x, dy = probe.end.y - probe.realized.y;
				probes.push_back( { { "phase", phase }, { "space", "bbox_origin" }, { "mask", probe.mask }, { "shape", probe.shape },
					{ "start", vector( probe.start ) }, { "end", vector( probe.end ) }, { "realized", vector( probe.realized ) },
					{ "fraction", probe.fraction }, { "normal", vector( Vector3DP( probe.normal ) ) }, { "startsolid", probe.startsolid },
					{ "allsolid", probe.allsolid }, { "entity", probe.entity }, { "brush", probe.brush }, { "xy_error", std::sqrt( dx * dx + dy * dy ) },
					{ "role", probe.start.x != probe.end.x || probe.start.y != probe.end.y ? "horizontal" : probe.end.z < probe.start.z ? "support" : "rise_or_stationary" } } );
			}
			return nlohmann::json{ { "reason", static_cast<uint32_t>( hit.reason ) }, { "supported_bbox", vector( hit.bbox_endpoint ) },
				{ "traces", hit.trace_count }, { "shape", static_cast<int32_t>( hit.shape ) }, { "blocking_actor", hit.blocking_actor }, { "probes", probes } };
		};
		const auto diagnostics = [&]( const mm_motion_diagnostics_t &hit ) {
			nlohmann::json clips = nlohmann::json::array();
			// Report each direct velocity-change witness from its accepted or discarded trial.
			for ( uint32_t index = 0; index < hit.clip_count; ++index ) {
				const auto &clip = hit.clips[ index ];
				clips.push_back( { { "space", "bbox_origin" }, { "contact", vector( clip.contact ) }, { "before", vector( clip.before ) },
					{ "after", vector( clip.after ) }, { "normal", vector( clip.normal ) }, { "entity", clip.entity }, { "brush", clip.brush }, { "actor", clip.actor } } );
			}
			return nlohmann::json{ { "wall_clips", hit.static_wall_clips }, { "actor_clips", hit.actor_clips },
				{ "clips", clips }, { "clip_overflow", hit.clip_overflow }, { "floor_riser_clips", hit.floor_or_riser_clips }, { "startsolid", hit.startsolid }, { "allsolid", hit.allsolid }, { "nudges", hit.recovery_nudges } };
		};
		nlohmann::json sources = nlohmann::json::array(), candidates = nlohmann::json::array();
		// Preserve every merged semantic obligation and every evaluated projected candidate.
		for ( uint32_t source = 0; source < f.constraint.constraint_count; ++source ) {
			const auto &constraint = f.constraint.constraints[ source ];
			sources.push_back( { { "flags", constraint.flags }, { "portal", constraint.portal }, { "corner", constraint.corner } } );
		}
		for ( uint32_t candidate = 0; candidate < f.steering.candidate_count; ++candidate ) {
			const auto &decision = f.steering.candidates[ candidate ];
			candidates.push_back( { { "index", candidate }, { "proposed", vector( decision.proposed ) }, { "projected", vector( decision.projected ) },
				{ "endpoint_feet", vector( decision.endpoint ) }, { "geometry", geometry( decision.geometric ) }, { "native", native( decision.native ) }, { "accepted", decision.accepted } } );
		}
		// Ownership metadata is copied at sampling time; report serialization never consults the live actor.
		const auto identity = []( const nav_query_identity_t &id ) {
			return nlohmann::json{ { "sequence", id.sequence }, { "incarnation", id.incarnation }, { "epoch", id.epoch },
				{ "goal", id.goal }, { "state", id.state }, { "crowd", id.crowd }, { "mesh", id.generation },
				{ "topology", id.topology }, { "entity", id.entity } };
		};
		nlohmann::json record = {
			{ "schema", 2 }, { "map", s_corner_map }, { "build", __DATE__ " " __TIME__ }, { "entity", f.entity }, { "spawn", f.spawn },
			{ "t", f.seconds }, { "dt", FRAME_TIME_S.Seconds() }, { "mesh", f.mesh }, { "topology", f.topology }, { "route", f.route },
			{ "tick", f.tick },
			{ "query", { { "state", f.queryState }, { "submitted_tick", f.submittedTick }, { "earliest_delivery_tick", f.earliestDeliveryTick },
				{ "requested", identity( f.requestIdentity ) }, { "accepted", identity( f.acceptedIdentity ) } } },
			{ "goal", f.goal }, { "accepted_goal", f.acceptedGoal }, { "crowd_order", f.crowdOrder },
			{ "reservation", { { "ingress", f.ingress }, { "ingress_released", f.ingressReleased }, { "egress", f.egress }, { "egress_released", f.egressReleased },
				{ "portal_origin", vector( f.portalOrigin ) }, { "portal_inward", vector( f.portalInward ) }, { "half_width", f.portalHalfWidth }, { "release_depth", f.releaseDepth }, { "egress_cleared", f.egressCleared },
				{ "egress_portal_origin", vector( f.egressPortalOrigin ) }, { "egress_portal_outward", vector( f.egressPortalOutward ) },
				{ "egress_half_width", f.egressHalfWidth }, { "egress_clearance_depth", f.egressClearanceDepth } } }, { "ai", f.ai }, { "animation", f.animation }, { "mode", f.mode },
			{ "k", f.index }, { "old_k", f.steering.old_index }, { "bbox_start", vector( f.start ) }, { "predicted", vector( f.predicted ) },
			{ "realized", vector( f.realized ) }, { "command", vector( f.command ) }, { "mins", vector( Vector3DP( f.mins ) ) }, { "maxs", vector( Vector3DP( f.maxs ) ) },
			{ "waypoint_feet", vector( f.waypoint ) }, { "previous_feet", vector( f.previousWaypoint ) }, { "next_feet", vector( f.nextWaypoint ) },
			{ "constraint", { { "flags", f.constraint.flags }, { "id", f.constraint.constraint_id }, { "face", f.constraint.face }, { "radius", f.constraint.required_clearance },
				{ "incoming", vector( f.constraint.incoming ) }, { "outgoing", vector( f.constraint.outgoing ) }, { "callback_fired", f.constraint.callback_fired }, { "sources", sources } } },
			{ "release", { { "reason", static_cast<uint32_t>( f.release.reason ) }, { "index", f.release.index }, { "route", f.release.route }, { "goal", f.release.goal },
				{ "turn_dot", f.release.turn_dot }, { "plane_distance", f.release.plane_distance }, { "geometry", geometry( f.release.geometric ) }, { "native", native( f.release.native ) } } },
			{ "steering", { { "raw", vector( f.steering.raw ) }, { "lookahead", vector( f.steering.lookahead ) }, { "transit", vector( f.steering.transit ) },
				{ "monotonic", vector( f.steering.monotonic ) }, { "deadband", vector( f.steering.deadband ) }, { "separation", vector( f.steering.separation ) },
				{ "turn_scale", f.steering.turn_scale }, { "squad_scale", f.steering.squad_scale }, { "follow_scale", f.steering.follow_scale },
				{ "catchup_scale", f.steering.catchup_scale }, { "fixture_scale", f.steering.fixture_scale }, { "constraint_distance", f.steering.constraint_distance },
				{ "final_velocity", vector( f.steering.final_velocity ) }, { "candidates", candidates } } },
			{ "yaw_before", f.steering.yaw_before }, { "yaw_after", f.yawAfter },
			{ "policy", { { "radius", f.policy.agent_radius }, { "step", f.policy.max_step_height }, { "drop", f.policy.max_drop_height }, { "normal", f.policy.min_step_normal },
				{ "drop_cap_enabled", f.policy.enable_max_drop_height_cap }, { "drop_cap", f.policy.max_drop_height_cap } } },
			{ "geometry", geometry( f.geometric ) }, { "native", native( f.validation ) }, { "accepted_trial", diagnostics( f.accepted ) },
			{ "ground_trial", diagnostics( f.ground ) }, { "elevated_trial", diagnostics( f.elevated ) },
			// Retain stable scalar keys used by existing independent regression oracles.
			{ "geometric", static_cast<uint32_t>( f.geometric.reason ) }, { "witness_edge", f.geometric.witness_edge }, { "total_validation_traces", f.totalValidationTraces },
			{ "validation", static_cast<uint32_t>( f.validation.reason ) }, { "shape", f.shape }, { "validation_traces", f.validation.trace_count },
			{ "rejected", f.rejected ? 1 : 0 }, { "blocked", f.blocked }, { "accepted_wall_clips", f.accepted.static_wall_clips }, { "actor_clips", f.accepted.actor_clips },
			{ "floor_riser_clips", f.accepted.floor_or_riser_clips }, { "startsolid", f.accepted.startsolid }, { "allsolid", f.accepted.allsolid }, { "nudges", f.accepted.recovery_nudges },
			{ "ground_wall_clips", f.ground.static_wall_clips }, { "elevated_wall_clips", f.elevated.static_wall_clips }
		};

		record[ "events" ] = nlohmann::json::array();
		for ( uint32_t event = 0; event < f.eventCount; ++event ) {
			record[ "events" ].push_back( { { "route", f.eventRoutes[ event ] }, { "index", f.eventIndices[ event ] }, { "final", f.eventFinal[ event ] } } );
		}
		record[ "capture_complete" ] = !s_corner_route_overflow && !s_corner_invalidated;
		// Emit a route once, in its first frame, rather than duplicating hundreds of points every tick.
		if ( s_corner_full_count != 0 && ( offset == 0 || s_corner_full_frames[ offset - 1 ].route != f.route ) ) {
			for ( size_t index = 0; index < s_corner_route_count; ++index ) {
				const auto &snapshot = s_corner_routes[ index ];
				if ( snapshot.revision != f.route ) {
					continue;
				}
				nlohmann::json points = nlohmann::json::array(), corridor = nlohmann::json::array();
				for ( uint32_t point = 0; point < snapshot.waypoint_count; ++point ) {
					const auto &waypoint = snapshot.waypoints[ point ];
					// Copy every source obligation so independent tests can verify coalescing, not only the active point.
					nlohmann::json sources = nlohmann::json::array();
					for ( uint32_t source = 0; source < waypoint.constraint_count; ++source ) {
						const auto &constraint = waypoint.constraints[ source ];
						sources.push_back( { { "flags", constraint.flags }, { "portal", constraint.portal }, { "corner", constraint.corner } } );
					}
					points.push_back( { { "feet", vector( waypoint.feet ) }, { "flags", waypoint.flags }, { "id", waypoint.constraint_id }, { "face", waypoint.face },
						{ "radius", waypoint.required_clearance }, { "incoming", vector( waypoint.incoming ) }, { "outgoing", vector( waypoint.outgoing ) }, { "sources", sources } } );
				}
				for ( uint32_t face = 0; face < snapshot.corridor_count; ++face ) {
					corridor.push_back( vector( snapshot.corridor[ face ] ) );
				}
				record[ "route_geometry" ] = { { "revision", snapshot.revision }, { "mesh", snapshot.mesh }, { "topology", snapshot.topology }, { "goal", snapshot.goal },
					{ "points", points }, { "corridor", corridor }, { "length", snapshot.validation.length }, { "minimum_clearance", snapshot.validation.minimum_clearance },
					{ "validation_reason", static_cast<uint32_t>( snapshot.validation.reason ) }, { "samples", snapshot.validation.samples }, { "traces", snapshot.validation.traces } };
				break;
			}
		}
		// Serialization is explicit report-time work; infinity becomes JSON null rather than malformed numeric text.
		const std::string serialized = record.dump();
		if ( output != nullptr ) {
			Nav_Corner_Output( output, "%s\n", serialized.c_str() );
		} else {
			// Full records exceed the engine console's bounded print buffer; request a canonical file artifact explicitly.
			gi.dprintf( "[nav frame] t=%.3f k=%zu route=%" PRIu64 " rejected=%d wall_clips=%" PRIu32 "; use an output.jsonl for full schema=2 evidence\n",
				f.seconds, f.index, f.route, f.rejected, f.accepted.static_wall_clips );
		}
	}
	/**
	*	Check report write/close status instead of treating an incomplete artifact as success.
	**/
	if ( output != nullptr ) {
		const bool writeFailed = std::ferror( output ) != 0;
		const bool closeFailed = std::fclose( output ) != 0;
		// Report the concrete I/O result without entering the measured movement path.
		gi.cprintf( nullptr, PRINT_HIGH, "nav_corner_report: artifact %s (%zu frames).\n",
			writeFailed || closeFailed ? "FAILED" : "written", reportCount );
	}

}

/**
*	@brief	Report exact physical/XY clearance decisions without moving a monster or committing a route.
*	@note	Reject incomplete/nonfinite command coordinates before any collision query.
**/
void Nav_Corner_ProbeCommand() {
	/**
	*	Validate entity, optional height policy and all six feet-coordinate arguments.
	**/
	int32_t entity = -1, height = 0;
	if ( ( gi.argc() != 9 && gi.argc() != 10 ) || !Nav_Corner_ParseInteger( gi.argv( 2 ), entity ) ||
		 ( gi.argc() == 10 && ( !Nav_Corner_ParseInteger( gi.argv( 9 ), height ) || height > 1 ) ) ) {
		gi.cprintf( nullptr, PRINT_HIGH, "Usage: sv nav_corner_probe <entity> <sx sy sz> <ex ey ez> [requireHeight:0|1]\n" );
		return;
	}
	auto *monster = dynamic_cast<svg_monster_base_t *>( g_edict_pool.EdictForNumber( entity ) );
	// Slot lookup alone is insufficient to authorize a native profile query.
	if ( monster == nullptr || !SVG_Entity_IsActive( monster ) ) {
		gi.cprintf( nullptr, PRINT_HIGH, "nav_corner_probe: entity is not an active monster.\n" );
		return;
	}
	double coordinates[ 6 ] = {};
	// Parse complete finite values so a trailing suffix cannot select different collision geometry.
	for ( int32_t index = 0; index < 6; ++index ) {
		const char *argument = gi.argv( index + 3 );
		const char *end = argument + std::strlen( argument );
		const auto parsed = std::from_chars( argument, end, coordinates[ index ] );
		if ( parsed.ec != std::errc{} || parsed.ptr != end || !std::isfinite( coordinates[ index ] ) ) {
			gi.cprintf( nullptr, PRINT_HIGH, "nav_corner_probe: invalid finite coordinate.\n" );
			return;
		}
	}

	/**
	*	Query the production validator and report its exact endpoint alongside static clearance.
	**/
	const Vector3DP start{ coordinates[ 0 ], coordinates[ 1 ], coordinates[ 2 ] };
	const Vector3DP end{ coordinates[ 3 ], coordinates[ 4 ], coordinates[ 5 ] };
	const auto movement = SVG_Nav_TestMovementCandidate( monster, start, end, monster->pathNavigationState.policy, height != 0, true );
	// Explicit diagnostics prepare the owner-thread cache outside measured movement.
	Nav_EnsureObstacleCornersTable();
	const auto geometric = Nav_TestSegmentClearance( start, end, static_cast<double>( monster->maxs.x ) );
	gi.cprintf( nullptr, PRINT_HIGH, "[nav probe] ent=%" PRId32 " height=%d physical=%u geometric=%u clearance=%.6f edge=%" PRId32 " traces=%" PRIu32 " endpoint_bbox=(%.6f %.6f %.6f)\n",
		entity, height, static_cast<uint32_t>( movement.reason ), static_cast<uint32_t>( geometric.reason ),
		geometric.minimum_clearance, geometric.witness_edge, movement.trace_count,
		movement.bbox_endpoint.x, movement.bbox_endpoint.y, movement.bbox_endpoint.z );
	gi.cprintf( nullptr, PRINT_HIGH, "[nav probe edge] cache_index=%" PRId32 " start=(%.6f %.6f %.6f) end=(%.6f %.6f %.6f)\n",
		geometric.witness_edge, geometric.witness_start.x, geometric.witness_start.y, geometric.witness_start.z,
		geometric.witness_end.x, geometric.witness_end.y, geometric.witness_end.z );
}

/**
*	@brief	Place an active monster at a collision-checked supported feet position for explicit engine fixtures.
*	@note	Console only; resets route and velocity after full native hull/support validation succeeds.
**/
void Nav_Corner_PlaceCommand() {
	/**
	*	Validate the selected incarnation and complete finite coordinates before querying collision.
	**/
	int32_t entity = -1;
	// Require exactly one entity and a feet-space position.
	if ( gi.argc() != 6 || !Nav_Corner_ParseInteger( gi.argv( 2 ), entity ) ) {
		gi.cprintf( nullptr, PRINT_HIGH, "Usage: sv nav_corner_place <entity> <feet x y z>\n" );
		return;
	}
	auto *monster = dynamic_cast<svg_monster_base_t *>( g_edict_pool.EdictForNumber( entity ) );
	// Recycled/inactive slots cannot be used as fixture actors.
	if ( monster == nullptr || !SVG_Entity_IsActive( monster ) ) {
		gi.cprintf( nullptr, PRINT_HIGH, "nav_corner_place: entity is not an active monster.\n" );
		return;
	}
	double coordinates[ 3 ] = {};
	// Parse every complete coordinate; NaN or a numeric suffix must not reach engine traces.
	for ( int32_t index = 0; index < 3; ++index ) {
		const char *argument = gi.argv( index + 3 );
		const char *end = argument + std::strlen( argument );
		const auto parsed = std::from_chars( argument, end, coordinates[ index ] );
		// Reject malformed coordinates before any world mutation.
		if ( parsed.ec != std::errc{} || parsed.ptr != end || !std::isfinite( coordinates[ index ] ) ) {
			gi.cprintf( nullptr, PRINT_HIGH, "nav_corner_place: invalid finite coordinate.\n" );
			return;
		}
	}

	/**
	*	A stationary native query proves the requested hull is clear and supported at this height.
	**/
	const Vector3DP feet{ coordinates[ 0 ], coordinates[ 1 ], coordinates[ 2 ] };
	const auto placement = SVG_Nav_TestMovementCandidate( monster, feet, feet, monster->pathNavigationState.policy, true );
	// Never relocate into a wall, actor, missing floor, or another elevation.
	if ( placement.reason != svg_nav_movement_reason_t::clear ) {
		gi.cprintf( nullptr, PRINT_HIGH, "[nav place] FAIL ent=%" PRId32 " reason=%u\n", entity, static_cast<uint32_t>( placement.reason ) );
		return;
	}

	/**
	*	Commit the supported fixture start, discard old motion/path, and refresh native grounding.
	**/
	monster->ResetNavigationPath();
	monster->velocity = {};
	monster->monsterMove.state.velocity = Vector3DP{};
	monster->monsterMove.state.origin = placement.bbox_endpoint;
	monster->currentStandingFace = -1;
	SVG_Util_SetEntityOrigin( monster, placement.bbox_endpoint, true );
	gi.linkentity( monster );
	monster->RecategorizeGroundAndLiquidState();
	gi.cprintf( nullptr, PRINT_HIGH, "[nav place] PASS ent=%" PRId32 " feet=(%.6f %.6f %.6f)\n",
		entity, placement.bbox_endpoint.x, placement.bbox_endpoint.y, placement.bbox_endpoint.z + monster->mins.z );
}

/**
*	@brief	Set an exact supported destination for an isolated single-monster engine fixture.
*	@note	Requires an existing one-member crowd order; bypasses formation fitting only, retaining normal AI/navigation/physics.
**/
void Nav_Corner_GoalCommand() {
	/**
	*	Resolve a single-member test crowd and parse complete finite feet coordinates.
	**/
	int32_t entity = -1;
	// Explicit fixture arguments are required; ordinary crowd commands keep formation fitting.
	if ( gi.argc() != 6 || !Nav_Corner_ParseInteger( gi.argv( 2 ), entity ) ) {
		gi.cprintf( nullptr, PRINT_HIGH, "Usage: sv nav_corner_goal <entity> <feet x y z>\n" );
		return;
	}
	auto *monster = dynamic_cast<svg_monster_base_t *>( g_edict_pool.EdictForNumber( entity ) );
	svg_crowd_group_t *group = monster != nullptr ? SVG_Crowd_GetGroup( monster->crowd.crowdID ) : nullptr;
	// Test setup must not alter another actor's goal, slots or reservation ownership.
	if ( monster == nullptr || !SVG_Entity_IsActive( monster ) || group == nullptr ||
		 group->memberEntityNumbers.size() != 1 || group->memberEntityNumbers.front() != entity ||
		 group->slots.size() != 1 || group->hasSerializedIngress || group->hasSerializedEgress ) {
		gi.cprintf( nullptr, PRINT_HIGH, "nav_corner_goal: requires an isolated one-member crowd order.\n" );
		return;
	}
	double coordinates[ 3 ] = {};
	// Every coordinate must be fully consumed and finite before collision or state mutation.
	for ( int32_t index = 0; index < 3; ++index ) {
		const char *argument = gi.argv( index + 3 );
		const char *end = argument + std::strlen( argument );
		const auto parsed = std::from_chars( argument, end, coordinates[ index ] );
		// Invalid input cannot produce a test destination.
		if ( parsed.ec != std::errc{} || parsed.ptr != end || !std::isfinite( coordinates[ index ] ) ) {
			gi.cprintf( nullptr, PRINT_HIGH, "nav_corner_goal: invalid finite coordinate.\n" );
			return;
		}
	}

	/**
	*	Prove the exact destination's native hull/support before committing the singleton fixture target.
	**/
	const Vector3DP feet{ coordinates[ 0 ], coordinates[ 1 ], coordinates[ 2 ] };
	// Use the production contact rule for literal surface goals; retain the original target coordinates.
	const auto support = Nav_ResolveHullGoalSupport( feet, monster->maxs.x, static_cast<int32_t>( SVG_MMove_GetNativeShape( monster ) ) );
	const auto supported = SVG_Nav_TestMovementCandidate( monster, support.probe, support.probe, monster->pathNavigationState.policy, true );
	// A failed destination remains visible and does not redirect the test to another floor or room.
	if ( supported.reason != svg_nav_movement_reason_t::clear ) {
		gi.cprintf( nullptr, PRINT_HIGH, "[nav goal] FAIL ent=%" PRId32 " reason=%u\n", entity, static_cast<uint32_t>( supported.reason ) );
		return;
	}
	// Exact retargets are new orders even when formation fitting was bypassed.
	++group->orderRevision;
	group->destinationOrigin = feet;
	group->slots.front().worldPosition = feet;
	group->slots.front().isNavmeshValid = true;
	group->isMoving = true;
	// Invalidate submitted routes before publishing this assignment field.
	monster->crowd.SetNavigationAssignment( monster->crowd.assignedGoalOrigin, QM_Vector3FromDP( feet ) );
	monster->crowd.reachedGoal = false;
	monster->crowd.startTimeForSeeking = level.time;
	// Keep owned old geometry until the new exact goal is accepted; stale-goal steering must hold.
	monster->lastPathCalcTime = 0_ms;
	gi.cprintf( nullptr, PRINT_HIGH, "[nav goal] PASS ent=%" PRId32 " feet=(%.6f %.6f %.6f)\n", entity, feet.x, feet.y, feet.z );
}

/**
*	@brief	Report generated route geometry, semantic provenance and owned validation failures.
*	@note	Explicit diagnostic allocations occur outside movement; the active actor and route remain unchanged.
**/
void Nav_Corner_RouteCommand() {
	/**
	*	Resolve a live native profile and finite exact destination before generating diagnostic candidates.
	**/
	int32_t entity = -1;
	if ( ( gi.argc() != 3 && gi.argc() != 6 && gi.argc() != 9 ) || !Nav_Corner_ParseInteger( gi.argv( 2 ), entity ) ) {
		gi.cprintf( nullptr, PRINT_HIGH, "Usage: sv nav_corner_route <entity> [goal feet x y z [start feet x y z]]\n" );
		return;
	}
	auto *monster = dynamic_cast<svg_monster_base_t *>( g_edict_pool.EdictForNumber( entity ) );
	// Inactive/recycled slots cannot supply the current hull or movement policy.
	if ( monster == nullptr || !SVG_Entity_IsActive( monster ) ) {
		return;
	}
	double coordinates[ 6 ] = {};
	// Consume all coordinate tokens so malformed diagnostics cannot query a substituted endpoint.
	for ( int32_t index = 0; index < gi.argc() - 3; ++index ) {
		const char *argument = gi.argv( index + 3 );
		const char *end = argument + std::strlen( argument );
		const auto parsed = std::from_chars( argument, end, coordinates[ index ] );
		if ( parsed.ec != std::errc{} || parsed.ptr != end || !std::isfinite( coordinates[ index ] ) ) {
			return;
		}
	}
	const Vector3DP start = gi.argc() == 9 ? Vector3DP{ coordinates[ 3 ], coordinates[ 4 ], coordinates[ 5 ] } : SVG_GetEntityFeetOriginDP( monster );
	const Vector3DP goal = gi.argc() >= 6 ? Vector3DP{ coordinates[ 0 ], coordinates[ 1 ], coordinates[ 2 ] } : Vector3DP( monster->crowd.assignedGoalOrigin );
	// The optional start is read-only; invalid-endpoint tests never teleport the live actor.
	const int32_t startFace = Nav_FindClosestFaceInLeaf( start );
	const int32_t goalFace = Nav_FindClosestFaceInLeaf( goal );
	std::vector<int32_t> corridor;
	std::vector<Vector3DP> points;
	std::vector<bool> forced;
	svg_vector_t<svg_nav_waypoint_t, TAG_SVGAME_NAVMESH> records;
	auto policy = monster->pathNavigationState.policy;
	policy.agent_radius = monster->maxs.x;
	nav_route_validation_t validation;
	/**
	*	Run the production search/funnel/native contract and copy failures rather than changing the live command.
	**/
	const bool found = Nav_FindPath( startFace, goalFace, corridor, policy );
	// Capture compatibility graph evidence explicitly before entering request-local route preparation.
	auto diagnostics = Nav_CopyLastPathDiagnosticsMainThread();
	const bool valid = found && Nav_StringPull( corridor, start, goal, policy.agent_radius, points, &forced,
		monster->mins, monster->maxs, static_cast<int32_t>( SVG_MMove_GetNativeShape( monster ) ), &records, &policy, &validation, monster, &diagnostics );
	gi.dprintf( "[nav route] ent=%" PRId32 " found=%d valid=%d faces=%zu points=%zu reason=%u segment=%" PRId32 " physical=%u geometric=%u clearance=%.6f edge=%" PRId32 " samples=%" PRIu32 " traces=%" PRIu32 " supported=(%.6f %.6f %.6f)\n",
		entity, found, valid, corridor.size(), points.size(), static_cast<uint32_t>( validation.reason ), validation.segment,
		static_cast<uint32_t>( validation.physical ), static_cast<uint32_t>( validation.geometric.reason ), validation.geometric.minimum_clearance,
		validation.geometric.witness_edge, validation.samples, validation.traces,
		validation.supported_endpoint.x, validation.supported_endpoint.y, validation.supported_endpoint.z );
	Nav_LogLastPathDiagnostics();
	/**
	*	Reproduce the rejected static command from its last supported feet and print owned native brush/plane witnesses.
	**/
	if ( validation.reason == nav_route_reason_t::native_failure && validation.segment >= 0 &&
		 static_cast<size_t>( validation.segment + 1 ) < points.size() ) {
		Vector3DP end = points[ validation.segment + 1 ];
		Vector3DP delta = end - validation.supported_endpoint;
		delta.z = 0.0;
		const double length = QM_Vector3LengthDP( delta );
		// Elevation continuation uses the same bounded local sampling as production; flat failures retain their complete chord.
		if ( std::fabs( end.z - validation.supported_endpoint.z ) > NAV_MOVEMENT_ENDPOINT_TOLERANCE && length > 5.5 ) {
			end = validation.supported_endpoint + delta * ( 5.5 / length );
		}
		end.z = validation.supported_endpoint.z;
		SVG_Nav_TestMovementHull( validation.supported_endpoint, end, monster->mins, monster->maxs,
			SVG_MMove_GetNativeShape( monster ), policy, monster, CM_CONTENTMASK_SOLID | CONTENTS_MONSTERCLIP, false, true );
	}
	// Report all retained points, including a rejected link's exact endpoints, only on explicit request.
	for ( size_t index = 0; index < points.size(); ++index ) {
		gi.dprintf( "[nav route point] k=%zu feet=(%.6f %.6f %.6f) flags=%" PRIu32 " sources=%" PRIu32 "\n",
			index, points[ index ].x, points[ index ].y, points[ index ].z,
			index < records.size() ? records[ index ].flags : ( index < forced.size() && forced[ index ] ? NAV_WAYPOINT_MANDATORY : 0u ), index < records.size() ? records[ index ].constraint_count : 0u );
	}
	// Explicit diagnostics copy the local scene; ordinary movement never prints geometry.
	Nav_DiagnoseActorDetourMainThread( *monster );
}

/**
*	@brief	Initialize reproducible map-spawn randomness only when explicitly requested.
*	@note	Both C and C++ generators are seeded before entity setup; ordinary games retain time seeds.
**/
void Nav_Corner_InitRandomSeed() {
	/**
	*	Resolve the opt-in non-archived seed before choosing deterministic or ordinary initialization.
	**/
	const cvar_t *fixtureSeed = gi.cvar( "sv_nav_fixture_seed", "0", 0 );
	if ( fixtureSeed->integer > 0 ) {
		const uint32_t seed = static_cast<uint32_t>( fixtureSeed->integer );
		Q_srand( seed );
		mt_rand.seed( seed );
		gi.dprintf( "[nav fixture seed] seed=%" PRIu32 "\n", seed );
		return;
	}
	Q_srand( time( nullptr ) );
	mt_rand.seed( static_cast<uint32_t>( std::chrono::system_clock::now().time_since_epoch().count() ) );
}

/**
*	@brief	Apply a validated native profile and independent speed/yaw controls to an explicit fixture actor.
*	@note	Uses the production hull query before mutation, then discards the old route and re-categorizes support.
**/
void Nav_Corner_ProfileCommand() {
	/**
	*	Validate complete finite fixture tokens and a live entity before native collision work.
	**/
	int32_t entity = -1;
	if ( gi.argc() != 12 || !Nav_Corner_ParseInteger( gi.argv( 2 ), entity ) ) {
		gi.dprintf( "Usage: sv nav_corner_profile <entity> <radius> <shape:1|2> <minZ> <maxZ> <step> <drop> <normalZ> <yaw> <speedScale>\n" );
		return;
	}
	auto *monster = dynamic_cast<svg_monster_base_t *>( g_edict_pool.EdictForNumber( entity ) );
	if ( monster == nullptr || !SVG_Entity_IsActive( monster ) ) {
		return;
	}
	double values[ 9 ] = {};
	// Entire finite tokens are required; malformed input cannot partially alter a profile.
	for ( int32_t index = 0; index < 9; ++index ) {
		const char *argument = gi.argv( index + 3 );
		const char *end = argument + std::strlen( argument );
		const auto parsed = std::from_chars( argument, end, values[ index ] );
		if ( parsed.ec != std::errc{} || parsed.ptr != end || !std::isfinite( values[ index ] ) ) {
			return;
		}
	}
	const double radius = values[ 0 ];
	// Fixture profiles remain representable and use the two supported analytical primitives.
	if ( radius <= 0.0 || radius > 128.0 || ( values[ 1 ] != 1.0 && values[ 1 ] != 2.0 ) ||
		 values[ 3 ] - values[ 2 ] < 2.0 * radius || values[ 4 ] < 0.0 || values[ 4 ] > 512.0 ||
		 values[ 5 ] < 0.0 || values[ 5 ] > 512.0 || values[ 6 ] < 0.0 || values[ 6 ] > 1.0 ||
		 values[ 8 ] < 0.1 || values[ 8 ] > 2.0 ) {
		return;
	}
	const Vector3 mins{ static_cast<float>( -radius ), static_cast<float>( -radius ), static_cast<float>( values[ 2 ] ) };
	const Vector3 maxs{ static_cast<float>( radius ), static_cast<float>( radius ), static_cast<float>( values[ 3 ] ) };
	auto policy = monster->pathNavigationState.policy;
	policy.agent_radius = maxs.x;
	policy.agent_mins = mins;
	policy.agent_maxs = maxs;
	policy.trace_shape = static_cast<int32_t>( values[ 1 ] );
	policy.max_step_height = static_cast<float>( values[ 4 ] );
	policy.max_drop_height = static_cast<float>( values[ 5 ] );
	policy.min_step_normal = static_cast<float>( values[ 6 ] );
	const auto shape = static_cast<mm_trace_shape_t>( static_cast<int32_t>( values[ 1 ] ) );
	const Vector3DP feet = SVG_GetEntityFeetOriginDP( monster );
	const auto supported = SVG_Nav_TestMovementHull( feet, feet, mins, maxs, shape, policy,
		monster, SVG_GetClipMask( monster ), true );
	// A blocked/unsupported resized hull leaves all live entity state unchanged.
	if ( supported.reason != svg_nav_movement_reason_t::clear ) {
		gi.dprintf( "[nav profile] FAIL ent=%" PRId32 " reason=%u\n", entity, static_cast<uint32_t>( supported.reason ) );
		return;
	}
	/**
	*	Commit the complete profile atomically and restore native grounding at the unchanged feet position.
	**/
	// Invalidate outstanding profile requests before publishing changed hull and movement policy.
	monster->ResetNavigationPath();
	monster->navigationFixtureProfile = true;
	monster->navigationFixtureMins = mins;
	monster->navigationFixtureMaxs = maxs;
	monster->mins = mins;
	monster->maxs = maxs;
	monster->solid = shape == MM_SHAPE_CAPSULE ? SOLID_CAPSULE : SOLID_CYLINDER;
	monster->pathNavigationState.policy = policy;
	monster->navigationFixtureSpeedScale = values[ 8 ];
	monster->currentAngles[ YAW ] = static_cast<float>( QM_AngleMod( values[ 7 ] ) );
	monster->ideal_yaw = monster->currentAngles[ YAW ];
	monster->monsterMove.mins = mins;
	monster->monsterMove.maxs = maxs;
	monster->monsterMove.state.origin = supported.bbox_endpoint;
	monster->velocity = {};
	monster->monsterMove.state.velocity = Vector3DP{};
	SVG_Util_SetEntityOrigin( monster, supported.bbox_endpoint, true );
	gi.linkentity( monster );
	monster->RecategorizeGroundAndLiquidState();
	gi.dprintf( "[nav profile] PASS ent=%" PRId32 " radius=%.3f shape=%" PRId32 " z=(%.3f %.3f) policy=(%.3f %.3f %.3f) yaw=%.3f scale=%.3f\n",
		entity, radius, static_cast<int32_t>( shape ), values[ 2 ], values[ 3 ], values[ 4 ], values[ 5 ], values[ 6 ], values[ 7 ], values[ 8 ] );
}

/**
*	@brief	Select one bounded causal mode without altering default monster behavior.
*	@note	Frozen modes reuse accepted route geometry; final hull/reservation validation remains authoritative.
**/
void Nav_Corner_ModeCommand() {
	/**
	*	Require an exact known mode and active actor before mutating diagnostic state.
	**/
	int32_t entity = -1, mode = -1;
	if ( gi.argc() != 4 || !Nav_Corner_ParseInteger( gi.argv( 2 ), entity ) ||
		 !Nav_Corner_ParseInteger( gi.argv( 3 ), mode ) || mode > 5 ) {
		return;
	}
	auto *monster = dynamic_cast<svg_monster_base_t *>( g_edict_pool.EdictForNumber( entity ) );
	if ( monster == nullptr || !SVG_Entity_IsActive( monster ) ) {
		return;
	}
	monster->navigationFixtureMode = static_cast<uint32_t>( mode );
	gi.dprintf( "[nav mode] ent=%" PRId32 " mode=%" PRId32 "\n", entity, mode );
}

/**
*	@brief	Replay an explicit bounded authored mandatory route after full static hull/support validation.
*	@note	Only an isolated opt-in singleton with matching exact start/goal may install this fixture.
*			Source indices remain -1 and FIXTURE flags distinguish authored obligations from generated provenance.
**/
void Nav_Corner_ReplayCommand() {
	/**
	*	Reject malformed arguments or live orders owned by another actor before constructing evidence.
	**/
	int32_t entity = -1;
	if ( gi.argc() < 12 || gi.argc() > 51 || ( gi.argc() - 3 ) % 3 != 0 || !Nav_Corner_ParseInteger( gi.argv( 2 ), entity ) ) {
		gi.cprintf( nullptr, PRINT_HIGH, "Usage: sv nav_corner_replay <entity> <3..16 feet points: x y z ...>\n" );
		return;
	}
	auto *monster = dynamic_cast<svg_monster_base_t *>( g_edict_pool.EdictForNumber( entity ) );
	const auto *group = monster != nullptr ? SVG_Crowd_GetGroup( monster->crowd.crowdID ) : nullptr;
	// Fixture installation cannot override multi-actor assignments or serialized reservations.
	if ( monster == nullptr || !SVG_Entity_IsActive( monster ) || !monster->navigationFixtureProfile ||
		 group == nullptr || group->memberEntityNumbers.size() != 1 || group->slots.size() != 1 ||
		 group->hasSerializedIngress || group->hasSerializedEgress ) {
		gi.cprintf( nullptr, PRINT_HIGH, "[nav replay] FAIL: requires an opt-in isolated profile/order.\n" );
		return;
	}
	const int32_t pointCount = ( gi.argc() - 3 ) / 3;
	std::vector<Vector3DP> points( static_cast<size_t>( pointCount ) );
	// Consume every literal coordinate, preserving exact finite endpoints without snapping.
	for ( int32_t coordinate = 0; coordinate < pointCount * 3; ++coordinate ) {
		const char *argument = gi.argv( coordinate + 3 );
		const char *end = argument + std::strlen( argument );
		double value = 0.0;
		const auto parsed = std::from_chars( argument, end, value );
		// Invalid tokens fail without mutating actor state or the accepted route.
		if ( parsed.ec != std::errc{} || parsed.ptr != end || !std::isfinite( value ) ) {
			gi.cprintf( nullptr, PRINT_HIGH, "[nav replay] FAIL: invalid finite coordinate.\n" );
			return;
		}
		points[ static_cast<size_t>( coordinate / 3 ) ][ coordinate % 3 ] = value;
	}
	// Check the live setup and target so a replay cannot silently teleport or substitute an endpoint.
	if ( QM_Vector3DistanceSqrDP( points.front(), SVG_GetEntityFeetOriginDP( monster ) ) > 0.125 * 0.125 ||
		 QM_Vector3DistanceSqrDP( points.back(), Vector3DP( monster->crowd.assignedGoalOrigin ) ) > 1e-12 ) {
		gi.cprintf( nullptr, PRINT_HIGH, "[nav replay] FAIL: exact start/goal mismatch.\n" );
		return;
	}

	/**
	*	Prove all closed links with unchanged native radius and policy before accepting authored constraints.
	**/
	const auto proof = Nav_ValidateHullRoute( points, monster->mins, monster->maxs,
		static_cast<int32_t>( SVG_MMove_GetNativeShape( monster ) ), monster->pathNavigationState.policy, monster );
	if ( proof.reason != nav_route_reason_t::clear ) {
		gi.cprintf( nullptr, PRINT_HIGH, "[nav replay] FAIL reason=%u segment=%" PRId32 "\n", static_cast<uint32_t>( proof.reason ), proof.segment );
		return;
	}
	svg_vector_t<svg_nav_waypoint_t, TAG_SVGAME_NAVMESH> records( points.size() );
	std::vector<int32_t> corridor;
	// Resolve supported faces and directions for each explicitly authored source obligation.
	for ( size_t index = 0; index < points.size(); ++index ) {
		auto &record = records[ index ];
		record.feet = points[ index ];
		record.face = Nav_FindClosestFaceInLeaf( record.feet );
		// Missing support metadata cannot authorize a route whose face IDs are stale or invalid.
		if ( record.face < 0 ) {
			gi.cprintf( nullptr, PRINT_HIGH, "[nav replay] FAIL: missing supporting face.\n" );
			return;
		}
		corridor.push_back( record.face );
		record.flags = NAV_WAYPOINT_FIXTURE | ( index == 0 ? NAV_WAYPOINT_START : index + 1 == points.size() ? NAV_WAYPOINT_FINAL_GOAL : NAV_WAYPOINT_CORNER_APEX | NAV_WAYPOINT_MANDATORY );
		record.constraint_id = static_cast<uint32_t>( index );
		record.required_clearance = monster->maxs.x;
		record.constraint_count = 1;
		record.constraints[ 0 ].flags = record.flags;
		// Normalize only nonzero horizontal links; endpoints keep their absent direction at zero.
		if ( index > 0 ) {
			record.incoming = points[ index ] - points[ index - 1 ];
			record.incoming.z = 0.0;
			record.incoming = QM_Vector3NormalizeDP( record.incoming );
		}
		if ( index + 1 < points.size() ) {
			record.outgoing = points[ index + 1 ] - points[ index ];
			record.outgoing.z = 0.0;
			record.outgoing = QM_Vector3NormalizeDP( record.outgoing );
		}
	}

	/**
	*	Commit owned geometry/revisions atomically, then leave W1 release to the shared live contract.
	**/
	monster->ResetNavigationPath();
	monster->navigationRequestedGoal = points.back();
	monster->navigationRequestedGoalValid = true;
	monster->navigationGoalRevision = Nav_IssueIdentityRevisionMainThread();
	monster->navigationAcceptedGoalRevision = monster->navigationGoalRevision;
	monster->navigationWaypoints = std::move( records );
	monster->stringPulledPath = std::move( points );
	monster->navPath = std::move( corridor );
	monster->navigationRouteRevision = Nav_IssueIdentityRevisionMainThread();
	monster->waypointFacesMeshGeneration = g_nav_mesh_generation;
	monster->waypointTopologyRevision = g_nav_topology_revision;
	monster->lastRouteValidation = proof;
	monster->pathPos = 0;
	monster->stringPathPos = 1;
	monster->finalWaypointEventFired = false;
	monster->pathNavigationState.lastGoal.origin = monster->crowd.assignedGoalOrigin;
	monster->pathNavigationState.lastGoal.isValid = true;
	monster->lastPathCalcTime = level.time;
	monster->navigationFixtureMode = 2;
	// Make the fixture's already-issued crowd transition explicit before idle can clear its authored route.
	if ( auto *dummy = dynamic_cast<svg_monster_testdummy_debug_t *>( monster ) ) {
		dummy->thinkAIState = svg_monster_testdummy_debug_t::AIThinkState::CrowdFormation;
	}
	// Fixture-authored geometry uses the same complete commit identity as a production reply.
	monster->navigationAcceptedIdentity = monster->SVG_Nav_CaptureIdentity();
	// Authored replay owns the same complete physical contract as a validated production route.
	monster->navigationCommittedPolicy = monster->pathNavigationState.policy;
	monster->navigationCommittedPolicy.agent_radius = monster->maxs.x;
	monster->navigationCommittedPolicy.agent_mins = monster->mins;
	monster->navigationCommittedPolicy.agent_maxs = monster->maxs;
	monster->navigationCommittedPolicy.trace_shape = static_cast<int32_t>( SVG_MMove_GetNativeShape( monster ) );
	monster->navigationQueryState = svg_monster_base_t::NavigationQueryState::Ready;

	gi.cprintf( nullptr, PRINT_HIGH, "[nav replay] PASS ent=%" PRId32 " radius=%.6f length=%.6f points=%" PRId32 "\n", entity, monster->maxs.x, proof.length, pointCount );
}

/**
*	@brief	Fixed owned evidence for every member of one selected opt-in crowd order.
**/
struct nav_corner_audit_t {
	//! Stable actor identity and aggregate actual movement counters.
	int32_t entity = -1, spawn = -1;
	uint64_t frames = 0, wall = 0, actor = 0, solid = 0, nudges = 0, incomplete = 0;
	//! Independent admission, steering and evidence-completeness counters.
	uint64_t reservation = 0, separation = 0, held = 0, rejected = 0, overflow = 0;
	//! Measured physical travel and observation interval, independent of generated routes.
	double travel = 0.0, first = 0.0, last = 0.0, minimum = std::numeric_limits<double>::infinity();
};
//! Opt-in group identifier; ordinary movement takes only the disabled guard.
static int32_t s_corner_audit_group = -1;
//! Direct entity-slot lookup maps the command-frozen roster to 64 fixed audit records.
static std::array<int32_t, MAX_EDICTS> s_corner_audit_lookup;
//! Bounded selected roster storage, initialized outside movement by an explicit command.
static std::array<nav_corner_audit_t, 64> s_corner_audits;
//! Number of command-frozen roster entries valid in the selected audit.
static uint32_t s_corner_audit_count = 0;

/**
*	@brief	Freeze a whole-crowd audit roster or report its copied results outside movement.
*	@note	Missing/recycled members and storage overflow are explicit failures, never silently omitted.
**/
void Nav_Corner_AuditCommand() {
	/**
	*	Validate the explicit group and action before touching existing audit ownership.
	**/
	int32_t groupID = -1;
	if ( gi.argc() != 4 || !Nav_Corner_ParseInteger( gi.argv( 2 ), groupID ) ) {
		gi.cprintf( nullptr, PRINT_HIGH, "Usage: sv nav_corner_audit <crowd> <reset|report>\n" );
		return;
	}
	const auto *group = SVG_Crowd_GetGroup( groupID );
	// Only a nonempty bounded current roster can be observed as a complete crowd fixture.
	if ( group == nullptr || group->memberEntityNumbers.empty() || group->memberEntityNumbers.size() > s_corner_audits.size() ) {
		gi.cprintf( nullptr, PRINT_HIGH, "[nav audit] FAIL: invalid bounded roster.\n" );
		return;
	}
	if ( Q_stricmp( gi.argv( 3 ), "reset" ) == 0 ) {
		/**
		*	Prepare direct lookup and owned lifetime identities once, outside physical movement.
		**/
		s_corner_audit_group = -1;
		s_corner_audit_count = 0;
		s_corner_audit_lookup.fill( -1 );
		// Reject every invalid member rather than publishing a partial or mismatched roster.
		for ( const int32_t entity : group->memberEntityNumbers ) {
			auto *monster = dynamic_cast<svg_monster_base_t *>( g_edict_pool.EdictForNumber( entity ) );
			if ( entity < 0 || static_cast<size_t>( entity ) >= s_corner_audit_lookup.size() || monster == nullptr || !SVG_Entity_IsActive( monster ) ) {
				gi.cprintf( nullptr, PRINT_HIGH, "[nav audit] FAIL: invalid live roster member.\n" );
				return;
			}
			auto &audit = s_corner_audits[ s_corner_audit_count ];
			audit = {};
			audit.entity = entity;
			audit.spawn = monster->spawn_count;
			s_corner_audit_lookup[ entity ] = static_cast<int32_t>( s_corner_audit_count++ );
		}
		s_corner_audit_group = groupID;
		gi.cprintf( nullptr, PRINT_HIGH, "[nav audit] READY group=%" PRId32 " members=%" PRIu32 "\n", groupID, s_corner_audit_count );
		return;
	}
	// Report only the selected roster; unrecognized actions cannot silently reset evidence.
	if ( Q_stricmp( gi.argv( 3 ), "report" ) != 0 || s_corner_audit_group != groupID ) {
		gi.cprintf( nullptr, PRINT_HIGH, "[nav audit] FAIL: no matching selected audit.\n" );
		return;
	}
	/**
	*	Print every member's copied counters; unchanged live roster identity is part of completeness.
	**/
	bool pass = group->memberEntityNumbers.size() == s_corner_audit_count;
	for ( uint32_t index = 0; index < s_corner_audit_count; ++index ) {
		const auto &a = s_corner_audits[ index ];
		const auto *live = g_edict_pool.EdictForNumber( a.entity );
		const bool valid = live != nullptr && SVG_Entity_IsActive( live ) && live->spawn_count == a.spawn && live->crowd.crowdID == groupID;
		pass = pass && valid && a.frames > 0 && a.wall == 0 && a.solid == 0 && a.nudges == 0 && a.incomplete == 0 && a.reservation == 0 && a.overflow == 0;
		gi.cprintf( nullptr, PRINT_HIGH, "[nav audit member] ent=%" PRId32 " frames=%" PRIu64 " wall=%" PRIu64 " actor=%" PRIu64 " solid=%" PRIu64 " nudges=%" PRIu64 " incomplete=%" PRIu64 " reservation=%" PRIu64 " separation=%" PRIu64 " held=%" PRIu64 " rejected=%" PRIu64 " overflow=%" PRIu64 " travel=%.6f elapsed=%.6f minimum=%.6f\n",
			a.entity, a.frames, a.wall, a.actor, a.solid, a.nudges, a.incomplete, a.reservation, a.separation, a.held, a.rejected, a.overflow, a.travel, a.last - a.first + FRAME_TIME_S.Seconds(), a.minimum );
	}
	gi.cprintf( nullptr, PRINT_HIGH, "[nav audit] VERDICT: %s group=%" PRId32 " members=%" PRIu32 "\n", pass ? "PASS" : "FAIL", groupID, s_corner_audit_count );
}

/**
*	@brief	Accumulate actual motion and finite reserved-aperture crossing for the command-frozen roster.
*	@param	monster	Actor after authoritative native resolution and entity origin synchronization.
*	@param	startOrigin	Command-start bbox position.
*	@param	commandVelocity	Unmodified original velocity.
*	@note	Direct O(1) slot lookup and fixed scalar writes; no allocation, formatting or geometry-wide search.
**/
void Nav_Corner_AuditMovement( const svg_monster_base_t &monster, const Vector3DP &startOrigin, const Vector3DP &commandVelocity ) {
	/**
	*	Ordinary gameplay and actors outside the selected roster do no audit sampling.
	**/
	if ( s_corner_audit_group < 0 || monster.crowd.crowdID != s_corner_audit_group || monster.s.number < 0 ||
		 static_cast<size_t>( monster.s.number ) >= s_corner_audit_lookup.size() ) {
		return;
	}
	const int32_t index = s_corner_audit_lookup[ monster.s.number ];
	// Missing or recycled roster slots are never merged with an earlier actor's evidence.
	if ( index < 0 || static_cast<uint32_t>( index ) >= s_corner_audit_count ) {
		return;
	}
	auto &audit = s_corner_audits[ index ];
	if ( audit.spawn != monster.spawn_count ) {
		++audit.overflow;
		return;
	}
	/**
	*	Count accepted-trial outcomes and independent intended-versus-realized horizontal completion.
	**/
	const auto &d = monster.monsterMove.acceptedDiagnostics;
	audit.wall += d.static_wall_clips;
	audit.actor += d.actor_clips;
	audit.solid += d.startsolid + d.allsolid;
	audit.nudges += d.recovery_nudges;
	audit.overflow += d.clip_overflow ? 1 : 0;
	audit.rejected += monster.navigationCommandRejected ? 1 : 0;
	const Vector3DP realized( monster.currentOrigin );
	const Vector3DP predicted = startOrigin + commandVelocity * FRAME_TIME_S.Seconds();
	audit.incomplete += QM_Vector3Distance2DSqrDP( predicted, realized ) > 0.125 * 0.125 ? 1 : 0;
	audit.travel += QM_Vector3Distance2DDP( startOrigin, realized );
	audit.held += commandVelocity.x == 0.0 && commandVelocity.y == 0.0 ? 1 : 0;
	audit.separation += QM_Vector3Distance2DSqrDP( monster.navigationSteering.deadband, monster.navigationSteering.separation ) > 1e-8 ? 1 : 0;
	// Minimum clearance is meaningful only for an actually evaluated strict geometric candidate.
	if ( monster.lastGeometricValidation.reason == nav_clearance_reason_t::clear ) {
		audit.minimum = std::min( audit.minimum, monster.lastGeometricValidation.minimum_clearance );
	}
	if ( audit.frames++ == 0 ) {
		audit.first = level.time.Seconds();
	}
	audit.last = level.time.Seconds();

	/**
	*	Unadmitted agents must not cross the finite reserved stop plane, even after physical resolution.
	**/
	const auto *group = SVG_Crowd_GetGroup( s_corner_audit_group );
	if ( group != nullptr && group->hasSerializedIngress && !monster.crowd.ingressReleased ) {
		const double before = QM_Vector3DotProductDP( startOrigin - group->ingressPortalOrigin, group->ingressPortalInward );
		const double after = QM_Vector3DotProductDP( realized - group->ingressPortalOrigin, group->ingressPortalInward );
		const double stop = -group->ingressReleaseDepth;
		// Test the exact line intersection; infinite-plane crossings beside the opening are unrelated.
		if ( before <= stop && after > stop ) {
			const double t = ( stop - before ) / ( after - before );
			const Vector3DP tangent{ group->ingressPortalInward.y, -group->ingressPortalInward.x, 0.0 };
			const double lateral = QM_Vector3DotProductDP( startOrigin + ( realized - startOrigin ) * t - group->ingressPortalOrigin, tangent );
			audit.reservation += std::fabs( lateral ) <= group->ingressPortalHalfWidth + group->ingressReleaseDepth ? 1 : 0;
		}
	}
	/**
	*	Cleared origin owners cannot return through the aperture while its remaining exit queue is active.
	**/
	if ( group != nullptr && SVG_Crowd_HasClearedEgress( &monster ) ) {
		const double before = QM_Vector3DotProductDP( startOrigin - group->egressPortalOrigin, group->egressPortalOutward );
		const double after = QM_Vector3DotProductDP( realized - group->egressPortalOrigin, group->egressPortalOutward );
		const double stop = SVG_Crowd_GetEgressClearanceDepth( &monster );
		// Check the realized finite-plane intersection using the actor's exact full footprint.
		if ( before >= stop && after < stop ) {
			const double fraction = ( before - stop ) / ( before - after );
			const Vector3DP tangent{ -group->egressPortalOutward.y, group->egressPortalOutward.x, 0.0 };
			const double lateral = QM_Vector3DotProductDP( startOrigin + ( realized - startOrigin ) * fraction - group->egressPortalOrigin, tangent );
			const double radius = std::max( { std::fabs( monster.mins.x ), std::fabs( monster.maxs.x ),
				std::fabs( monster.mins.y ), std::fabs( monster.maxs.y ) } );
			audit.reservation += std::fabs( lateral ) <= group->egressPortalHalfWidth + radius ? 1 : 0;
		}
	}
	// Egress waiting is a complete physical hold; any horizontal movement violates admission ownership.
	if ( group != nullptr && group->hasSerializedEgress && !monster.crowd.egressReleased && QM_Vector3Distance2DSqrDP( startOrigin, realized ) > 0.125 * 0.125 ) {
		++audit.reservation;
	}
}

/**
*	@brief	Disable selected crowd sampling when navigation/map lifetimes are invalidated.
*	@note	An explicit matching reset is required before old copied evidence can be accepted again.
**/
static void Nav_Corner_InvalidateAudit() {
	// A new lifetime cannot inherit a previously selected roster, even for the same map name.
	s_corner_audit_group = -1;
}

//! Explicit visual-fixture overlay switch; ordinary drawing retains the existing nav debug controls.
static bool s_corner_overlay = false;

/**
*	@brief	Select the already copied fixture overlay without changing movement or generation.
*	@note	Only explicit 0/1 arguments change state; renderer debug drawing must also be enabled.
**/
void Nav_Corner_OverlayCommand() {
	// Reject malformed toggles rather than accidentally enabling diagnostic work.
	int32_t enabled = 0;
	if ( gi.argc() != 3 || !Nav_Corner_ParseInteger( gi.argv( 2 ), enabled ) || enabled > 1 ) {
		gi.cprintf( nullptr, PRINT_HIGH, "Usage: sv nav_corner_overlay <0|1>\n" );
		return;
	}
	s_corner_overlay = enabled != 0;
	gi.cprintf( nullptr, PRINT_HIGH, "[nav overlay] enabled=%d\n", s_corner_overlay );
}

/**
*	@brief	Position a seeded local inspection camera independently of monster motion and yaw.
*	@note	The explicit command requires cheats and a connected player; ordinary movement is unaffected.
**/
void Nav_Corner_CameraCommand() {
	/**
	*	Require an explicitly enabled local fixture and finite complete camera coordinates.
	**/
	auto *player = g_edict_pool.EdictForNumber( 1 );
	if ( gi.argc() != 7 || player == nullptr || !SVG_Entity_IsActive( player ) || player->client == nullptr ||
		 gi.cvar( "cheats", "0", 0 )->integer == 0 || gi.cvar( "sv_nav_fixture_seed", "0", 0 )->integer <= 0 ) {
		gi.cprintf( nullptr, PRINT_HIGH, "[nav camera] FAIL: requires seeded local client, cheats, and x y z pitch yaw.\n" );
		return;
	}
	double values[ 5 ] = {};
	// Every value is consumed before mutating entity/player-state coordinate ownership.
	for ( int32_t index = 0; index < 5; ++index ) {
		const char *argument = gi.argv( index + 2 );
		const char *end = argument + std::strlen( argument );
		const auto parsed = std::from_chars( argument, end, values[ index ] );
		if ( parsed.ec != std::errc{} || parsed.ptr != end || !std::isfinite( values[ index ] ) || std::fabs( values[ index ] ) > 32768.0 ) {
			gi.cprintf( nullptr, PRINT_HIGH, "[nav camera] FAIL: invalid bounded coordinate.\n" );
			return;
		}
	}
	/**
	*	Synchronize bbox position, visual orientation and command-angle delta once for the inspection view.
	**/
	const Vector3 origin{ static_cast<float>( values[ 0 ] ), static_cast<float>( values[ 1 ] ), static_cast<float>( values[ 2 ] ) };
	const Vector3 angles{ static_cast<float>( values[ 3 ] ), static_cast<float>( values[ 4 ] ), 0.0f };
	player->movetype = MOVETYPE_NOCLIP;
	player->flags |= FL_NOTARGET;
	player->velocity = {};
	SVG_Util_SetEntityOrigin( player, origin, true );
	SVG_Util_SetEntityAngles( player, angles, true );
	player->client->ps.pmove.origin = origin;
	player->client->ps.pmove.velocity = {};
	player->client->ps.viewangles = angles;
	player->client->viewMove.viewAngles = angles;
	player->client->ps.pmove.delta_angles = QM_Vector3AngleMod( angles - player->client->resp.cmd_angles );
	QM_AngleVectors( angles, &player->client->viewMove.viewForward, nullptr, nullptr );
	gi.linkentity( player );
	gi.cprintf( nullptr, PRINT_HIGH, "[nav camera] PASS origin=(%.3f %.3f %.3f) angles=(%.3f %.3f)\n", origin.x, origin.y, origin.z, angles.x, angles.y );
}

/**
*	@brief	Overlay copied original/final geometry, mandatory ownership, physical trail and native command evidence.
*	@note	Colours: grey A* centers, cyan retained route, orange mandatory, green realized trail,
*			yellow active point, purple hull, grey candidates, green accepted or red held final velocity.
**/
void Nav_Corner_DebugDraw() {
	/**
	*	Require explicit opt-in and a valid copied full-run frame in this capture lifetime.
	**/
	if ( !s_corner_overlay || s_corner_invalidated || s_corner_full_count == 0 || !SVG_Nav_DebugDraw_IsEnabled() ) {
		return;
	}
	const auto &frame = s_corner_full_frames[ s_corner_full_count - 1 ];
	/**
	*	Label the current owned rejection evidence for the local inspection view.
	*	This opt-in caption does not allocate or format during ordinary navigation updates.
	**/
	auto *observer = g_edict_pool.EdictForNumber( 1 );
	// A dedicated server has no local player; only an enabled client inspection receives the caption.
	if ( observer != nullptr && SVG_Entity_IsActive( observer ) && observer->client != nullptr ) {
		constexpr const char *nativeReasons[] = { "clear", "invalid input", "unsupported hull", "blocked", "incomplete", "unsupported landing", "elevation policy" };
		const uint32_t reason = static_cast<uint32_t>( frame.validation.reason );
		const char *label = reason < std::size( nativeReasons ) ? nativeReasons[ reason ] : "unknown";
		gi.centerprintf( observer, "M4 ent %" PRId32 " waypoint %" PRIu32 "\nNative: %s  release: %" PRIu32 "  held: %d",
			frame.entity, static_cast<uint32_t>( frame.index ), label, static_cast<uint32_t>( frame.release.reason ), frame.rejected ? 1 : 0 );
	}
	const auto raised = []( const Vector3DP &p ) { return QM_Vector3FromDP( p + Vector3DP{ 0.0, 0.0, 2.0 } ); };
	/**
	*	Draw the accepted revision's original corridor and exact retained semantic polyline.
	**/
	for ( size_t route = 0; route < s_corner_route_count; ++route ) {
		const auto &snapshot = s_corner_routes[ route ];
		// Ignore superseded accepted routes so stale geometry cannot look like the active constraint.
		if ( snapshot.revision != frame.route ) {
			continue;
		}
		for ( uint32_t index = 1; index < snapshot.corridor_count; ++index ) {
			SVG_Nav_DebugDraw_AddLine( raised( snapshot.corridor[ index - 1 ] ), raised( snapshot.corridor[ index ] ), MakeColor( 160, 160, 160, 255 ) );
		}
		for ( uint32_t index = 0; index < snapshot.waypoint_count; ++index ) {
			const auto &point = snapshot.waypoints[ index ];
			const uint32_t colour = Nav_WaypointIsMandatory( point ) ? MakeColor( 255, 140, 0, 255 ) : MakeColor( 0, 220, 255, 255 );
			SVG_Nav_DebugDraw_AddSphere( raised( point.feet ), Nav_WaypointIsMandatory( point ) ? 4.0f : 2.0f, colour );
			if ( index > 0 ) {
				SVG_Nav_DebugDraw_AddLine( raised( snapshot.waypoints[ index - 1 ].feet ), raised( point.feet ), MakeColor( 0, 220, 255, 255 ), SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE, 2.5f );
			}
		}
	}
	/**
	*	Render at most the last 256 realized feet links without modifying local gameplay progress.
	**/
	const size_t first = s_corner_full_count > 256 ? s_corner_full_count - 256 : 1;
	for ( size_t index = first; index < s_corner_full_count; ++index ) {
		const auto &a = s_corner_full_frames[ index - 1 ];
		const auto &b = s_corner_full_frames[ index ];
		SVG_Nav_DebugDraw_AddLine( raised( a.realized + Vector3DP{ 0.0, 0.0, static_cast<double>( a.mins.z ) } ), raised( b.realized + Vector3DP{ 0.0, 0.0, static_cast<double>( b.mins.z ) } ), MakeColor( 20, 255, 70, 255 ), SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE, 3.0f );
	}
	/**
	*	Display analytical hull independently of facing, and exact accepted/projected candidate velocity.
	**/
	Vector3DP feet = frame.realized;
	feet.z += frame.mins.z;
	const Vector3 start = raised( feet );
	const float radius = frame.maxs.x;
	const Vector3 bottom = QM_Vector3FromDP( feet + Vector3DP{ 0.0, 0.0, static_cast<double>( radius ) } );
	const Vector3 top = QM_Vector3FromDP( feet + Vector3DP{ 0.0, 0.0, static_cast<double>( frame.maxs.z - frame.mins.z - radius ) } );
	// Holding motion does not change the physical profile represented by the overlay.
	if ( frame.shape == MM_SHAPE_CYLINDER ) {
		SVG_Nav_DebugDraw_AddCylinder( QM_Vector3FromDP( feet ), QM_Vector3FromDP( feet + Vector3DP{ 0.0, 0.0, static_cast<double>( frame.maxs.z - frame.mins.z ) } ), radius, MakeColor( 210, 80, 255, 255 ) );
	} else {
		SVG_Nav_DebugDraw_AddCapsule( bottom, top, radius, MakeColor( 210, 80, 255, 255 ) );
	}
	SVG_Nav_DebugDraw_AddSphere( raised( frame.waypoint ), 6.0f, MakeColor( 255, 240, 0, 255 ) );
	for ( uint32_t index = 0; index < frame.steering.candidate_count; ++index ) {
		const auto &candidate = frame.steering.candidates[ index ];
		SVG_Nav_DebugDraw_AddArrow( start, raised( feet + candidate.projected * 0.25 ), 4.0f, candidate.accepted ? MakeColor( 20, 255, 70, 255 ) : MakeColor( 255, 60, 60, 255 ) );
	}
	SVG_Nav_DebugDraw_AddArrow( start, raised( feet + frame.command * 0.25 ), 6.0f, frame.rejected ? MakeColor( 255, 60, 60, 255 ) : MakeColor( 20, 255, 70, 255 ), SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE, 3.0f );
}
