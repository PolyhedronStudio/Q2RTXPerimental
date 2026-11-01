#pragma once

#include "svgame/nav/nav_path.h"
#include "svgame/nav/nav_query_algorithm.h"
#include <span>
#include "nav_clearance_tree.h"

class svg_monster_base_t;

/**
*	@brief	Owned source geometry retained until semantic route records are assembled.
**/
struct nav_waypoint_anchor_t {
	//! Exact generated feet position and copied source obligation.
	Vector3DP feet = {};
	svg_nav_waypoint_constraint_t source = {};
	//! Supporting face within the pinned geometry revision.
	int32_t face = -1;
};

/**
*	@brief	Copied obstacle corner geometry with no live entity pointer.
**/
struct nav_cached_corner_t {
	//! Vertex, incident wall directions/normals and safe-side bisector.
	Vector3DP vertex = {}, u1 = {}, u2 = {}, n1 = {}, n2 = {}, bisectorNorm = {};
	//! Bisector scale, elevation interval and native wall/drop classification.
	double bisectorCosHalf = 1.0, minZ = 0.0, maxZ = 0.0;
	uint32_t cornerType = NAV_EDGE_WALL;
};

/**
*	@brief	One route's proposed corner standoff and source provenance.
**/
struct nav_corridor_corner_t {
	//! Exact corner, proposed standoff and its distance.
	Vector3DP vertex = {}, standoffMid = {};
	double standoffDist = 0.0;
	//! Copied incident-wall geometry.
	Vector3DP bisectorNorm = {}, u1 = {}, u2 = {}, n1 = {}, n2 = {};
	//! Copied elevation, classification and source index.
	double minZ = 0.0, maxZ = 0.0;
	uint32_t cornerType = NAV_EDGE_WALL;
	int32_t sourceCorner = -1;
};

/**
*	@brief	Temporary projection into this context's retained corridor-corner buffer.
**/
struct nav_segment_corner_t {
	//! Ordered projection and index into this context's owned corner buffer.
	double t = 0.0;
	uint32_t corner = 0;
};

/**
*	@brief	Prepared directed funnel aperture and mandatory-point indication.
**/
struct funnel_portal_t {
	//! Exact directed endpoints in feet space.
	Vector3DP left = {}, right = {};
	//! Collapsed aperture must retain its obligation.
	bool force_waypoint = false;
};

//! Immutable spatial buckets are allocated, deep-copied and reclaimed only on the owner.
using nav_route_grid_t = svg_unordered_map_t<int64_t, svg_vector_t<int32_t, TAG_SVGAME_NAVMESH>, std::hash<int64_t>, std::equal_to<int64_t>, TAG_SVGAME_NAVMESH>;

/**
*	@brief	Pointer-free room classification needed by copied geometric ray casts.
**/
struct nav_route_room_t {
	//! Room role without live or nested room buffers.
	nav_zone_type_t zone_type = ZONE_TYPE_OPEN_SPACE;
};

/**
*	@brief	Const copied geometry borrowed for the duration of a snapshot lease.
*	@note	No entity/collision pointers escape into these records; native proof is separate.
**/
struct nav_route_mesh_view_t {
	//! Stable identities for every index and provenance entry.
	uint64_t generation = 0, topology = 0;
	//! Indexed geometry copied before publication.
	std::span<const Vector3DP> vertices;
	std::span<const nav_halfedge_t> halfedges;
	std::span<const nav_face_t> faces;
	std::span<const nav_kdtree_node_t> nodes;
	std::span<const nav_route_room_t> rooms;
	//! Owner-classified immutable obstacle geometry and reachability policy.
	std::span<const nav_cached_corner_t> corners;
	std::span<const std::pair<Vector3DP, Vector3DP>> solid_edges;
	std::span<const uint8_t> edge_blocks_xy;
	std::span<const int32_t> components;
	std::span<const double> widths;
	//! Const indices remain owned by the pinned snapshot, never the live cache.
	const nav_route_grid_t *grid_edges = nullptr, *grid_corners = nullptr;
	//! Exact nearest-boundary index prepared before this publication becomes visible.
	std::span<const nav_clearance_tree_node_t> clearance_nodes;
	bool clearance_tree_ready = false;
};

/**
*	@brief	Complete owned route reply retained through owner validation and atomic installation.
*	@note	Compatibility diagnostics receive a copy; no global last-query state is an input.
**/
struct svg_nav_result_t {
	//! Exact request identity shared by the graph and native route phases.
	nav_query_identity_t identity;
	//! Corridor, feet-space polyline and mandatory semantic records owned by this reply.
	std::vector<int32_t> corridor;
	std::vector<Vector3DP> waypoints;
	std::vector<bool> forced;
	svg_vector_t<svg_nav_waypoint_t, TAG_SVGAME_NAVMESH> records;
	//! Complete graph/refinement diagnostics and native installation proof.
	nav_path_diagnostics_t diagnostics = {};
	nav_route_validation_t validation = {};
};

/**
*	@brief	Exclusive mutable route state passed explicitly through native route preparation.
*	@note	Owner wrappers prepare/reclaim tagged storage; concurrent solves never share contexts.
**/
struct svg_nav_route_context_t {
	//! Source anchors and request-local refinement diagnostics.
	svg_vector_t<nav_waypoint_anchor_t, TAG_SVGAME_NAVMESH> anchors;
	nav_path_diagnostics_t diagnostics = {};
	//! Corridor refinement proposals and retained obligation flags.
	svg_vector_t<nav_corridor_corner_t, TAG_SVGAME_NAVMESH> corridor_corners;
	svg_vector_t<uint32_t, TAG_SVGAME_NAVMESH> candidate_corners;
	svg_vector_t<Vector3DP, TAG_SVGAME_NAVMESH> refined_waypoints;
	svg_vector_t<bool, TAG_SVGAME_NAVMESH> refined_forced;
	svg_vector_t<nav_segment_corner_t, TAG_SVGAME_NAVMESH> segment_corners;
	//! Flat support intervals and portal-local corner candidates.
	svg_vector_t<std::pair<double, double>, TAG_SVGAME_NAVMESH> flat_intervals;
	std::vector<uint32_t> clip_local_corners;
	//! Funnel, subdivision, cleanup and decimation scratch.
	svg_vector_t<funnel_portal_t, TAG_SVGAME_NAVMESH> portals;
	svg_vector_t<int32_t, TAG_SVGAME_NAVMESH> portal_indices;
	svg_vector_t<Vector3DP, TAG_SVGAME_NAVMESH> sub_waypoints, clean_waypoints, simplified_waypoints;
	svg_vector_t<bool, TAG_SVGAME_NAVMESH> sub_forced, clean_forced, simplified_forced;
	//! Owner-side convenience conversion buffer.
	std::vector<Vector3DP> waypoints_dp;
	//! Independent spatial visitation; rollover clears only this context.
	svg_vector_t<uint32_t, TAG_SVGAME_NAVMESH> edge_tokens, corner_tokens;
	uint32_t edge_stamp = 0, corner_stamp = 0;
};

/**
*	@brief	Find a containing face in a caller-owned mesh view without acquiring a publication.
*	@note	The caller keeps every referenced geometry span stable for the call.
**/
int32_t Nav_FindFaceInLeafStrict( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const Vector3DP &point );

/**
*	@brief	Find the closest face in a caller-owned mesh view with the legacy global fallback.
*	@note	The caller keeps every referenced geometry span stable for the call.
**/
int32_t Nav_FindClosestFaceInLeaf( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const Vector3DP &point );

/**
*	@brief	Expose the stable live geometry during exclusive navmesh generation.
*	@note	This transitional view is valid only while generation owns the live mesh;
*			it must never escape to an ordinary route query or survive publication.
**/
nav_route_mesh_view_t Nav_MakeGenerationRouteView();

/**
*	@brief	Cast across a caller-owned route view for generation-time room profiling.
*	@note	The mesh and context are retained by the caller through the entire cast.
**/
bool Nav_RaycastHalfEdge2D( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context,
	const Vector3DP &start, const Vector3DP &dirNorm, const double maxDistance, nav_raycast_result_t *outResult );

/**
*	@brief	Print a copied actor/static scene only on explicit route diagnostics.
**/
void Nav_DiagnoseActorDetourMainThread( const svg_monster_base_t &monster );
