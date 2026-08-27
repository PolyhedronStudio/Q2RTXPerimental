#pragma once

#include "svgame/nav/nav_types.h"

//! Maximum legal step height for the path policy.
static constexpr float NAV_MAX_STEP_SIZE = 18.25f;
//! Maximum drop height that is still considered traversable.
static constexpr float NAV_DROPOFF_ALLOWED_SIZE = 128.0f;
//! Absolute cap for drop height checks.
static constexpr float NAV_DROPOFF_MAX_SIZE = 196.0f;
//! Squared epsilon used when comparing waypoints.
static constexpr float WAYPOINT_EPS_SQR = 4.0f * 4.0f;
//! Squared epsilon used for portal crossing checks.
static constexpr float PORTAL_EPS_SQR = 16.0f * 16.0f;
/**
*	@brief	Path policy describing movement limits and traversal preferences.
*	@note	These values are consumed by the nav pathfinder and the movement steering code.
**/
struct nav_path_policy_t {
	//! Radius of the navigating agent (used for clearance and portal checks).
	float agent_radius = 16.0f;
	//! Bounding box minimums of the agent (used for full physical collision volume traces).
	Vector3 agent_mins = { -16.0f, -16.0f, -36.0f };
	//! Bounding box maximums of the agent (used for full physical collision volume traces).
	Vector3 agent_maxs = { 16.0f, 16.0f, 36.0f };
	//! Analytical collision shape for mover traces (0 = Auto, 1 = Capsule, 2 = Cylinder).
	int32_t trace_shape = 1;
	//! Minimum upward surface normal required to treat a landing as a step.
	float min_step_normal = 0.7f;
	//! Minimum step height in world units.
	float min_step_height = 0.0f;
	//! Maximum step height in world units.
	float max_step_height = NAV_MAX_STEP_SIZE;
	//! Maximum drop height in world units.
	float max_drop_height = NAV_DROPOFF_ALLOWED_SIZE;
	//! Enable the additional drop-height cap.
	bool enable_max_drop_height_cap = true;
	//! Additional cap applied when max drop height limiting is enabled.
	float max_drop_height_cap = NAV_DROPOFF_MAX_SIZE;
	//! Minimum portal width for traversable edges.
	float min_portal_width = 0.5f;
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
	//! Ignore disabled edges to allow generating paths through closed doors.
	bool ignore_disabled_edges = false;
};
/**
*	@brief	Walk the KD-tree to locate the leaf node that contains a point.
*	@param	point World-space position to query.
*	@return	Index of the leaf node, or -1 when the point lies outside world bounds.
**/
int32_t Nav_FindLeafNode( const Vector3DP &point );
inline int32_t Nav_FindLeafNode( const Vector3 &point ) {
	return Nav_FindLeafNode( Vector3DP( point ) );
}

/**
*	@brief	Check if a point lies within the 2D projection of a face.
*	@param	point World-space position.
*	@param	face Face to test against.
*	@return	True if the point is inside.
**/
bool Nav_PointInsideFace2D( const Vector3DP &point, const nav_face_t &face );
inline bool Nav_PointInsideFace2D( const Vector3 &point, const nav_face_t &face ) {
	return Nav_PointInsideFace2D( Vector3DP( point ), face );
}

/**
*	@brief	Return the nav face that actually contains a point.
*	@param	point World-space position.
*	@return	Index of the face, or -1 if none was found.
**/
int32_t Nav_FindPolyInLeaf( const Vector3DP &point );
inline int32_t Nav_FindPolyInLeaf( const Vector3 &point ) {
	return Nav_FindPolyInLeaf( Vector3DP( point ) );
}

/**
*	@brief	Compute a path using A* from the start face to the goal face.
*	@param	startFace Index of the starting face.
*	@param	goalFace Index of the goal face.
*	@param	outPath Output vector that receives the face sequence.
*	@param	policy Traversal policy used to bound vertical movement.
*	@return	True when a valid path was found.
**/
bool Nav_FindPath( int32_t startFace, int32_t goalFace, std::vector<int32_t> &outPath, const nav_path_policy_t &policy );

/**
 *	@brief	One-shot rejection summary produced by the most recent A* query.
 *	@note	The counters describe only edges examined by that query and do not alter
 *			path traversal or emit per-frame diagnostics.
**/
struct nav_path_diagnostics_t {
	//! Start face supplied to the most recent query.
	int32_t start_face = -1;
	//! Goal face supplied to the most recent query.
	int32_t goal_face = -1;
	//! Whether the most recent query returned a path.
	bool route_found = false;
	//! Whether the query indices were valid.
	bool query_valid = false;
	//! Number of unique faces expanded by A*.
	int32_t expanded_faces = 0;
	//! Number of transitions that passed all traversal checks.
	int32_t accepted_transitions = 0;
	//! Number of disabled transitions rejected.
	int32_t rejected_disabled = 0;
	//! Number of transitions without valid portal overlap.
	int32_t rejected_no_portal = 0;
	//! Number of transitions rejected because their portal is too narrow.
	int32_t rejected_narrow_portal = 0;
	//! Number of transitions without a usable agent-center corridor.
	int32_t rejected_no_agent_corridor = 0;
	//! Number of transitions rejected because the upward step is too high.
	int32_t rejected_step_height = 0;
	//! Number of transitions rejected because the downward drop is too high.
	int32_t rejected_drop_height = 0;
};

/**
 *	@brief	Print the most recent A* rejection summary once.
 *	@note	Intended for the existing `nav_dbg_test` command after a failed route.
**/
void Nav_LogLastPathDiagnostics( void );

/**
*	@brief	Fallback that finds the globally closest face by 3D distance.
*	@param	point World-space position to search from.
*	@return	Index of the closest face, or -1 if none exist.
**/
int32_t Nav_FindClosestPolyGlobal( const Vector3DP &point );
inline int32_t Nav_FindClosestPolyGlobal( const Vector3 &point ) {
	return Nav_FindClosestPolyGlobal( Vector3DP( point ) );
}

/**
*	@brief	Calculate portal endpoints between two adjacent nav faces.
*	@param	faceA Index of the first face.
*	@param	faceB Index of the second face.
*	@param	outV0 Output receiving the first endpoint.
*	@param	outV1 Output receiving the second endpoint.
*	@return	True when a shared edge was identified.
**/
bool Nav_GetPortalEndpoints( int32_t faceA, int32_t faceB, Vector3DP *outV0, Vector3DP *outV1 );
inline bool Nav_GetPortalEndpoints( int32_t faceA, int32_t faceB, Vector3 *outV0, Vector3 *outV1 ) {
	Vector3DP v0{}, v1{};
	bool ok = Nav_GetPortalEndpoints( faceA, faceB, &v0, &v1 );
	if ( ok ) {
		if ( outV0 ) *outV0 = static_cast<Vector3>( v0 );
		if ( outV1 ) *outV1 = static_cast<Vector3>( v1 );
	}
	return ok;
}


/**
*	@brief	Build a smoothed string-pulled path using the Funnel algorithm in full double precision.
*	@param	path The sequence of face IDs to traverse.
*	@param	startPos The exact starting position in double precision.
*	@param	goalPos The exact ending position in double precision.
*	@param	agentRadius The collision radius to steer clear of walls.
*	@param	outWaypoints Output sequence of 3D double-precision points.
*	@param	outForcedWaypoints Optional output flags parallel to `outWaypoints`; true
*						for stair approach and crossing constraints that must not be smoothed.
*	@param	agentMins Bounding box minimums for full physical swept volume verification.
*	@param	agentMaxs Bounding box maximums for full physical swept volume verification.
*	@param	traceShape Analytical collision shape (0 = Auto, 1 = Capsule, 2 = Cylinder).
*	@return	True if a valid corridor and string-pull could be generated.
**/
bool Nav_StringPull( const std::vector<int32_t> &path, const Vector3DP &startPos, const Vector3DP &goalPos, double agentRadius, std::vector<Vector3DP> &outWaypoints, std::vector<bool> *outForcedWaypoints = nullptr, const Vector3 &agentMins = { -16.0f, -16.0f, -36.0f }, const Vector3 &agentMaxs = { 16.0f, 16.0f, 36.0f }, int32_t traceShape = 1 );

/**
*	@brief	Build a smoothed string-pulled path using the Funnel algorithm (single-precision convenience wrapper).
*	@param	path The sequence of face IDs to traverse.
*	@param	startPos The exact starting position.
*	@param	goalPos The exact ending position.
*	@param	agentRadius The collision radius to steer clear of walls.
*	@param	outWaypoints Output sequence of 3D points.
*	@param	outForcedWaypoints Optional output flags parallel to `outWaypoints`; true
*						for stair approach and crossing constraints that must not be smoothed.
*	@param	agentMins Bounding box minimums for full physical swept volume verification.
*	@param	agentMaxs Bounding box maximums for full physical swept volume verification.
*	@param	traceShape Analytical collision shape (0 = Auto, 1 = Capsule, 2 = Cylinder).
*	@return	True if a valid corridor and string-pull could be generated.
**/
bool Nav_StringPull( const std::vector<int32_t> &path, const Vector3 &startPos, const Vector3 &goalPos, float agentRadius, std::vector<Vector3> &outWaypoints, std::vector<bool> *outForcedWaypoints = nullptr, const Vector3 &agentMins = { -16.0f, -16.0f, -36.0f }, const Vector3 &agentMaxs = { 16.0f, 16.0f, 36.0f }, int32_t traceShape = 1 );

/**
* 	@brief	Reapply dynamic nav edge state from the current runtime entity states.
* 	@note	This is needed after runtime navmesh regeneration clears and rebuilds the edge registry.
**/
void Nav_ResyncDynamicEntityEdges();

/**
*	@brief	Globally enables or disables all nav mesh edges associated with a specific entity ID.
*	@param	entity_id The runtime ID of the entity (e.g. a door).
*	@param	flags The edge flags to modify (e.g. NAV_EDGE_DISABLED).
*	@param	enable If true, the flags are added to the edges. If false, the flags are cleared.
**/
void Nav_SetEntityEdgesState( int32_t entity_id, uint32_t flags, bool enable );


/**
*	Global navmesh data containers.
**/
//! Global navmesh polygon data (temporary during build).
extern nav_vector_t<nav_poly_t> g_nav_polys;

//! Half-edge mesh global vertices data
extern std::vector<Vector3DP> g_nav_vertices;
//! Half-edge mesh global half-edges data
extern std::vector<nav_halfedge_t> g_nav_halfedges;
//! Entity to half-edge mapping table for O(1) dynamic edge updates.
extern std::vector<std::vector<int32_t>> g_nav_entity_edges;
//! Half-edge mesh global faces data
extern std::vector<nav_face_t> g_nav_faces;

//! Global navmesh kdtree node data.
extern nav_vector_t<nav_kdtree_node_t> g_nav_nodes;
//! Global navmesh leaf link data.
extern nav_vector_t<nav_leaf_link_t> g_nav_leaf_links;
//! Global navmesh leaf polygon ID data.
extern nav_vector_t<int32_t> g_nav_leaf_poly_ids;