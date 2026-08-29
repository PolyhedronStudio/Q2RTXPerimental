/********************************************************************
*
*
*	ServerGame: Crowd & Crew Coordination Types
*	File: svg_crowd_types.h
*	Description:
*		Type definitions, role enums, formation style flags, and
*		coordination parameter structs for the crowd/crew navigation system.
*
*
********************************************************************/
#pragma once

#include "shared/shared.h"

#include <cstdint>

/**
*	@brief	Crowd/crew target chase styles and formation patterns.
**/
enum class crowd_chase_target_type_t : uint32_t {
	//! Abreast line (rank) or column line perpendicular/parallel to move heading.
	CROWD_STYLE_LINE = 0,
	//! V-formation / wedge with a point leader at apex and staggered wings.
	CROWD_STYLE_ARROW,
	//! Concentric radial rings filled inward-outward with angular spacing.
	CROWD_STYLE_CIRCLE_FILLED,
	//! Checkerboard / staggered echelon ranks (dashed pattern) for open firing lines.
	CROWD_STYLE_DASHED_LINE,
	//! Dispersed tactical cover points within max distance of destination/target.
	CROWD_STYLE_TACTICAL_COVER,
	//! Loose flocking / perimeter circle around the target without strict grid slots.
	CROWD_STYLE_SURROUND_PERIMETER,
	//! Single-file column follow (parade / narrow corridor march).
	CROWD_STYLE_COLUMN_MARCH,
	//! Double-column staggered checkerboard march (standard military patrol).
	CROWD_STYLE_STAGGERED_COLUMN,
	//! 4-point diamond / 5-point box perimeter for 360-degree security.
	CROWD_STYLE_BOX_DIAMOND,
	//! Slanted echelon line extending back-left (refusing right flank).
	CROWD_STYLE_ECHELON_LEFT,
	//! Slanted echelon line extending back-right (refusing left flank).
	CROWD_STYLE_ECHELON_RIGHT,
	//! Maximum count of valid formation styles.
	CROWD_STYLE_COUNT
};

/**
*	@brief	Functional tactical role assigned to a crowd member within a formation.
**/
enum class crowd_member_role_t : int32_t {
	//! No specific tactical role assigned.
	ROLE_UNASSIGNED = 0,
	//! Squad commander / anchor entity establishing formation heading.
	ROLE_LEADER = 1,
	//! Point-man at the spearhead/apex of an advancing formation.
	ROLE_POINT = 2,
	//! Left-flank wingman protecting the formation's port side.
	ROLE_FLANK_LEFT = 3,
	//! Right-flank wingman protecting the formation's starboard side.
	ROLE_FLANK_RIGHT = 4,
	//! Core/center formation rank member.
	ROLE_CENTER = 5,
	//! Rear-guard watching the formation's trailing edge.
	ROLE_REAR_GUARD = 6,
	//! Cover operative occupying a fortified tactical position.
	ROLE_COVER_OPERATIVE = 7,
	//! Non-aggressive neutral civilian wandering in an ambient crowd.
	ROLE_CIVILIAN_WANDERER = 8,
	//! Panicked civilian fleeing threat sources.
	ROLE_CIVILIAN_FLEEING = 9
};

/**
*	@brief	Crowd orientation modes for aligning formation geometry.
**/
enum class crowd_orientation_mode_t : int32_t {
	//! Align formation forward axis with the collective movement direction.
	ORIENTATION_MOVE_DIRECTION = 0,
	//! Align formation forward axis with target entity's view yaw angles.
	ORIENTATION_TARGET_ENTITY_YAW = 1,
	//! Align formation forward axis with a fixed user-specified yaw.
	ORIENTATION_FIXED_YAW = 2
};

//! Default arrival radius in world units for satisfying formation slot reachability.
static constexpr double CROWD_DEFAULT_ARRIVAL_RADIUS = 48.0;
//! Default mutual soft-repulsion separation radius between squad members in world units.
static constexpr double CROWD_DEFAULT_SEPARATION_RADIUS = 48.0;
//! Default strength multiplier for mutual separation repulsion force [0.0..1.0].
static constexpr double CROWD_DEFAULT_SEPARATION_STRENGTH = 0.35;
//! Default lateral spacing between formation slots in world units.
static constexpr double CROWD_DEFAULT_LATERAL_SPACING = 64.0;
//! Default longitudinal spacing between formation rows in world units.
static constexpr double CROWD_DEFAULT_LONGITUDINAL_SPACING = 64.0;
//! Minimum lateral spacing threshold when squeezing formations through narrow corridors.
static constexpr double CROWD_DEFAULT_MIN_CORRIDOR_SPACING = 24.0;
//! Maximum allowable vertical Z difference for satisfying formation slot arrival.
static constexpr double CROWD_ARRIVAL_MAX_Z_DIFF = 48.0;
//! Multiplier applied to arrival radius when testing if a stationary/blocked agent has reached its slot.
static constexpr double CROWD_BLOCKED_ARRIVAL_RADIUS_FACTOR = 2.0;
//! Minimum horizontal speed squared under which a blocked crowd member is considered stationary.
static constexpr double CROWD_BLOCKED_STATIONARY_SPEED_SQ = 16.0;
//! Minimum distance the target entity must move before a follow formation is eligible to rebuild.
static constexpr double CROWD_FOLLOW_REBUILD_MIN_DIST = 96.0;
//! Minimum elapsed time interval between follow formation slot recalculations.
static constexpr QMTime CROWD_FOLLOW_REBUILD_MIN_INTERVAL = 500_ms;
//! Vertical downward offset used when attempting KD-leaf face lookups at feet level.
static constexpr double CROWD_SLOT_FEET_SNAP_OFFSET_Z = 24.0;
//! Distance threshold to player beyond which arrived crowd members throttle their think frequency.
static constexpr double CROWD_DORMANT_THROTTLE_DIST = 1500.0;
//! Default agent horizontal radius in world units used for crowd formation clearances.
static constexpr double CROWD_DEFAULT_AGENT_RADIUS = 16.0;
//! Minimum longitudinal separation distance between squad members along a travel corridor.
static constexpr double CROWD_FOLLOW_MIN_SEPARATION = 40.0;
//! Distance range over which trailing squad members smoothly decelerate behind a leading teammate.
static constexpr double CROWD_FOLLOW_SLOWDOWN_RANGE = 32.0;
//! Half-width corridor threshold under which teammates directly ahead trigger longitudinal yielding.
static constexpr double CROWD_CORRIDOR_LATERAL_THRESHOLD = 32.0;
//! Safety standoff margin from solid obstacle boundary walls when applying mutual separation.
static constexpr double CROWD_WALL_STANDOFF_MARGIN = 8.0;
//! Speed multiplier applied when an en-route squad member has fallen behind its assigned slot.
static constexpr double CROWD_CATCHUP_SPEED_SCALE = 1.15;
//! Speed multiplier applied when an en-route squad member has advanced ahead of its assigned slot.
static constexpr double CROWD_SLOWDOWN_SPEED_SCALE = 0.80;
//! Minimum crawl speed scale when decelerating behind a leading teammate in a travel corridor.
static constexpr double CROWD_FOLLOW_CRAWL_SPEED_SCALE = 0.50;
//! Distance threshold from assigned slot within which an en-route squad member is considered in-rank.
static constexpr double CROWD_SLOT_FOLLOW_TOLERANCE = 24.0;
//! Distance threshold beyond which an en-route squad member engages catch-up sprint.
static constexpr double CROWD_SLOT_CATCHUP_DISTANCE = 48.0;
//! Minimum duration an agent must be stalled near its slot before declaring blocked arrival.
static constexpr QMTime CROWD_BLOCKED_ARRIVAL_STALL_TIME = 500_ms;
//! Low-frequency think interval for arrived crowd members located far away from the player.
static constexpr QMTime CROWD_THROTTLE_THINK_INTERVAL = 100_ms;

//! Physical separation margin added to two agent diameters to guarantee non-overlapping formation slots.
static constexpr double CROWD_SLOT_MIN_SEPARATION_MARGIN = 12.0;

//! Distance improvement threshold required to trigger a dynamic 2-Opt slot swap between moving members.
static constexpr double CROWD_SWAP_HYSTERESIS_MOVING = 16.0;

//! Distance improvement threshold required to trigger a dynamic 2-Opt slot swap when a member has already arrived.
static constexpr double CROWD_SWAP_HYSTERESIS_ARRIVED = 48.0;

//! Maximum iterations executed per frame during dynamic 2-Opt slot assignment optimization.
static constexpr int32_t CROWD_MAX_OPTIMIZE_ITERS = 8;

//! Maximum iterations executed during initial ingress-depth 2-Opt slot optimization.
static constexpr int32_t CROWD_MAX_INGRESS_2OPT_ITERS = 16;

//! Rank window searched around candidate depth rank during ingress slot matching.
static constexpr size_t CROWD_INGRESS_RANK_WINDOW = 4;

//! Weight multiplier applied to rank differential relative to lateral offset in ingress slot scoring.
static constexpr double CROWD_INGRESS_RANK_WEIGHT = 20.0;

//! Lateral offset multiplier applied to column ranks when searching for alternative walkable slot positions.
static constexpr double CROWD_COLUMN_LATERAL_OFFSET_RATIO = 0.5;

//! Maximum sequential column ranks searched backwards along the approach corridor when resolving invalid slots.
static constexpr int32_t CROWD_MAX_COLUMN_SEARCH_RANKS = 64;

//! Number of candidate radial angles tested when adaptively packing invalid slots inside interior rooms.
static constexpr int32_t CROWD_INTERIOR_PACKING_ANGLES = 12;

//! Step increment in radians between candidate radial angles during interior room packing.
static constexpr double CROWD_INTERIOR_PACKING_ANGLE_STEP = ( 2.0 * QM_PI ) / static_cast<double>( CROWD_INTERIOR_PACKING_ANGLES );

//! Vertical upward trace clearance offset above feet origin to verify unobstructed slot line-of-sight.
static constexpr double CROWD_SLOT_TRACE_CLEARANCE_OFFSET_Z = 18.0;

//! Trace fraction threshold required to declare a formation slot location physically unobstructed.
static constexpr float CROWD_SLOT_TRACE_FRACTION_THRESHOLD = 0.99f;

//! Tactical cover fallback offset distance behind squad centroid away from threat.
static constexpr double CROWD_TACTICAL_COVER_RESERVE_OFFSET = 48.0;

//! Tolerance distance in world units for preserving ingress depth monotonicity during slot assignment and 2-Opt.
static constexpr double CROWD_INGRESS_ORDER_TOLERANCE = 16.0;

/**
*	@brief	Parameters controlling formation geometry, spacing, and tactical thresholds.
**/
struct svg_crowd_params_t {
	//! Lateral/horizontal spacing between members in world units.
	double lateralSpacing = CROWD_DEFAULT_LATERAL_SPACING;
	//! Longitudinal/forward-back spacing between rows in world units.
	double longitudinalSpacing = CROWD_DEFAULT_LONGITUDINAL_SPACING;
	//! Maximum distance allowed when seeking tactical cover around target (default: 768.0).
	double maxCoverDistance = 768.0;
	//! Minimum stand-off distance from target when seeking tactical cover (default: 192.0).
	double minCoverDistance = 192.0;
	//! Arrival distance tolerance for slots in world units.
	double arrivalRadius = CROWD_DEFAULT_ARRIVAL_RADIUS;
	//! Maximum search/seek duration before auto-refreshing (0 = indefinite).
	QMTime maxTimeToSeek = 0_ms;
	//! Orientation mode for aligning formation forward axis.
	crowd_orientation_mode_t orientationMode = crowd_orientation_mode_t::ORIENTATION_MOVE_DIRECTION;
	//! Fixed yaw in degrees when orientationMode == ORIENTATION_FIXED_YAW.
	double fixedYaw = 0.0;
	//! Staggered path calculation interval across squad members in milliseconds (default: 50ms).
	int32_t pathStaggerMs = 50;
	//! Spacing radius around claimed cover points to prevent crowd clumping (default: 160.0).
	double coverExclusionRadius = 160.0;
	//! Whether dynamic corridor squeezing is enabled in narrow bottlenecks.
	bool enableCorridorSqueeze = true;
	//! Minimum lateral spacing threshold during narrow corridor squeeze.
	double minCorridorSpacing = CROWD_DEFAULT_MIN_CORRIDOR_SPACING;
	//! Radius around agents for mutual soft-repulsion separation.
	double separationRadius = CROWD_DEFAULT_SEPARATION_RADIUS;
	//! Weight strength of mutual separation steering force [0.0..1.0].
	double separationStrength = CROWD_DEFAULT_SEPARATION_STRENGTH;
};

/**
*	@brief	A single computed formation slot position and its assigned metadata in high precision.
**/
struct svg_crowd_slot_t {
	//! Calculated world-space target position in double precision (snapped to walkable navmesh).
	Vector3DP worldPosition = { 0.0, 0.0, 0.0 };
	//! Relative local offset before rotation (X = lateral right, Y = forward, Z = up).
	Vector3DP localOffset = { 0.0, 0.0, 0.0 };
	//! Tactical view orientation yaw in degrees relative to the formation forward axis.
	double relativeYawDeg = 0.0;
	//! Slot index in the formation pattern (0 = primary/leader slot).
	int32_t slotIndex = 0;
	//! Assigned tactical role for this slot.
	crowd_member_role_t role = crowd_member_role_t::ROLE_UNASSIGNED;
	//! Associated tactical cover point index (-1 if geometric slot).
	int32_t coverIndex = -1;
	//! Whether this slot was successfully snapped to a valid navmesh face.
	bool isNavmeshValid = false;
};

/**
*	@brief	Crowd/crew membership and per-agent coordination state stored on entities.
**/
struct crowd_t {
	//! Crowd identifier: -1 = none, 0 = neutral NPC crowd, > 0 = squad/enemy crowd ID.
	int32_t crowdID = -1;
	//! Assigned slot index within the active formation (-1 = unassigned).
	int32_t slotIndex = -1;
	//! Assigned tactical role in the formation.
	crowd_member_role_t role = crowd_member_role_t::ROLE_UNASSIGNED;
	//! Entity number of the designated squad leader (ENTITYNUM_NONE if unassigned or self).
	int32_t leaderEntityNumber = ENTITYNUM_NONE;
	//! World-space target position assigned by the crowd manager.
	Vector3 assignedGoalOrigin = { 0.0f, 0.0f, 0.0f };
	//! Active tactical cover point index claimed by this agent (-1 if not using cover).
	int32_t activeCoverIdx = -1;
	//! Timestamp when movement/seeking order was initiated.
	QMTime startTimeForSeeking = 0_ms;
	//! Maximum allowed duration for seeking the goal before timing out or re-evaluating.
	QMTime maxTimeToSeek = 0_ms;
	//! Last server time when an A* route was computed (for staggered updates).
	QMTime lastPathCalcTime = 0_ms;
	//! Timestamp when an agent first became stationary while stalled near its assigned slot.
	QMTime blockedStartTime = 0_ms;
	//! True when the agent has reached within the arrival threshold of its assigned slot/cover.
	bool reachedGoal = false;
};
