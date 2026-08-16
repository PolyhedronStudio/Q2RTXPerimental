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

/**
*	@brief	Parameters controlling formation geometry, spacing, and tactical thresholds.
**/
struct svg_crowd_params_t {
	//! Lateral/horizontal spacing between members in world units (default: 64.0).
	double lateralSpacing = 64.0;
	//! Longitudinal/forward-back spacing between rows in world units (default: 64.0).
	double longitudinalSpacing = 64.0;
	//! Maximum distance allowed when seeking tactical cover around target (default: 768.0).
	double maxCoverDistance = 768.0;
	//! Minimum stand-off distance from target when seeking tactical cover (default: 192.0).
	double minCoverDistance = 192.0;
	//! Arrival distance tolerance for slots in world units (default: 32.0).
	double arrivalRadius = 32.0;
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
};

/**
*	@brief	A single computed formation slot position and its assigned metadata in high precision.
**/
struct svg_crowd_slot_t {
	//! Calculated world-space target position in double precision (snapped to walkable navmesh).
	Vector3DP worldPosition = { 0.0, 0.0, 0.0 };
	//! Relative local offset before rotation (X = lateral right, Y = forward, Z = up).
	Vector3DP localOffset = { 0.0, 0.0, 0.0 };
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
	//! True when the agent has reached within the arrival threshold of its assigned slot/cover.
	bool reachedGoal = false;
};
