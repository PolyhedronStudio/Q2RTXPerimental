#pragma once

#include "svgame/nav/nav_types.h"
#include <vector>

//! Maximum legal step height for the path policy.
static constexpr float NAV_MAX_STEP_SIZE = 18.25f;
//! Maximum drop height that is still considered traversable.
static constexpr float PM_DROPOFF_ALLOWED_SIZE = 128.0f;
//! Absolute cap for drop height checks.
static constexpr float PM_DROPOFF_MAX_SIZE = 196.0f;
//! Squared epsilon used when comparing waypoints.
static constexpr float WAYPOINT_EPS_SQR = 4.0f * 4.0f;
//! Squared epsilon used for portal crossing checks.
static constexpr float PORTAL_EPS_SQR = 16.0f * 16.0f;

/**
* @brief Path policy describing movement limits and traversal preferences.
* @note These values are consumed by the nav pathfinder and the movement steering code.
**/
struct nav_path_policy_t {
	//! Minimum step height in world units.
	float min_step_height = 0.0f;
	//! Maximum step height in world units.
	float max_step_height = NAV_MAX_STEP_SIZE;
	//! Maximum drop height in world units.
	float max_drop_height = PM_DROPOFF_ALLOWED_SIZE;
	//! Enable the additional drop-height cap.
	bool enable_max_drop_height_cap = true;
	//! Additional cap applied when max drop height limiting is enabled.
	float max_drop_height_cap = PM_DROPOFF_MAX_SIZE;
	//! Radius used when deciding if the agent has reached a waypoint.
	float waypoint_radius = 32.0f;
	//! 2D distance threshold for rebuilding a path.
	float rebuild_goal_dist_2d = 32.0f;
	//! 3D distance threshold for rebuilding a path.
	float rebuild_goal_dist_3d = 32.0f;
	//! Milliseconds between rebuild attempts.
	int32_t rebuild_interval = 25;
	//! Allow small jumps over low obstructions.
	bool allow_small_obstruction_jump = true;
	//! Maximum obstruction height that may be jumped.
	float max_obstruction_jump_height = 48.0f;
	//! Additional clearance before attempting a step.
	float step_clearance = 4.0f;
	//! Ignore visibility checks when set.
	bool ignore_visibility = false;
	//! Ignore in-front tests when set.
	bool ignore_infront = false;
	//! Allow gap-jumping behavior.
	bool allow_gap_jumping = true;
	//! Maximum distance for gap jumps.
	float max_jump_distance = 256.0f;
	//! Minimum width for a gap to be considered jumpable.
	float min_gap_width = 24.0f;
};

/**
* @brief Walk the KD-tree to locate the leaf node that contains a point.
* @param point World-space position to query.
* @return Index of the leaf node, or -1 when the point lies outside world bounds.
**/
int32_t Nav_FindLeafNode( const Vector3 &point );

/**
* @brief Check if a point lies within the 2D projection of a face.
* @param point World-space position.
* @param face Face to test against.
* @return True if the point is inside.
**/
bool Nav_PointInsideFace2D( const Vector3 &point, const nav_face_t &face );

/**
* @brief Return the nav face that actually contains a point.
* @param point World-space position.
* @return Index of the face, or -1 if none was found.
**/
int32_t Nav_FindPolyInLeaf( const Vector3 &point );

/**
* @brief Compute a path using A* from the start face to the goal face.
* @param startFace Index of the starting face.
* @param goalFace Index of the goal face.
* @param outPath Output vector that receives the face sequence.
* @param policy Traversal policy used to bound vertical movement.
* @return True when a valid path was found.
**/
bool Nav_FindPath( int32_t startFace, int32_t goalFace, std::vector<int32_t> &outPath, const nav_path_policy_t &policy );

/**
* @brief Fallback that finds the globally closest face by 3D distance.
* @param point World-space position to search from.
* @return Index of the closest face, or -1 if none exist.
**/
int32_t Nav_FindClosestPolyGlobal( const Vector3 &point );

/**
* @brief Calculate portal endpoints between two adjacent nav faces.
* @param faceA Index of the first face.
* @param faceB Index of the second face.
* @param outV0 Output receiving the first endpoint.
* @param outV1 Output receiving the second endpoint.
* @return True when a shared edge was identified.
**/
bool Nav_GetPortalEndpoints( int32_t faceA, int32_t faceB, Vector3 *outV0, Vector3 *outV1 );
