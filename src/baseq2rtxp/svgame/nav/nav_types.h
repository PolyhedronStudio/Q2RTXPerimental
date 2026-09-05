/********************************************************************
*
*
*	ServerGame: Navigation System Types.
*
*	Contains the following, which, professionally would be described 
*	as a: `Navmesh Spatial Acceleration Structure`
*
*	More specifically, the tree itself is best called:
*	•	``Face-Based KD-tree``
*	•	``KD-tree with AABB Node Bounds``
*	•	``Spatial Partitioning Hierarchy over Nav Faces``
*
*	If you want the most accurate technical phrasing:
*
*	A `Static Face-Based Spatial Index` implemented as an `Axis-Aligned KD-tree with Per-Node Bounding Boxes`.
*	So it is `not` just a `Plain KD-tree` and not a `Pure AABB` tree either; it is a `KD-Tree-Based Spatial Acceleration Structure`.
* 
* 
********************************************************************/
#pragma once

#include "nav_core.h"
#include "nav_containers.h"
#include "nav_cover_types.h"
#include <algorithm>

/**
*	@brief	Axis-Aligned Bounding Box (AABB) helper structure for 3D geometric queries.
**/
struct nav_aabb_t {
	// Our double-precision infinity constant
	static constexpr double inf = std::numeric_limits<double>::infinity();

	//! Minimum bounds extent along X, Y, and Z.
	Vector3DP mins = { inf, inf, inf };
	//! Maximum bounds extent along X, Y, and Z.
	Vector3DP maxs = { -inf, -inf, -inf };

	/**
	*	@brief	Reset bounding box extents to inverted infinite bounds.
	**/
	inline void Clear() {
		mins = Vector3DP{ inf, inf, inf };
		maxs = Vector3DP{ -inf, -inf, -inf };
	}

	/**
	*	@brief	Expand bounding box to include a 3D point location.
	**/
	inline void AddPoint( const Vector3DP &p ) {
		mins.x = std::min<double>( mins.x, p.x );
		mins.y = std::min<double>( mins.y, p.y );
		mins.z = std::min<double>( mins.z, p.z );

		maxs.x = std::max<double>( maxs.x, p.x );
		maxs.y = std::max<double>( maxs.y, p.y );
		maxs.z = std::max<double>( maxs.z, p.z );
	}

	/**
	*	@brief	Expand bounding box by a uniform spatial margin in all directions.
	**/
	inline void Expand( double margin ) {
		mins.x -= margin;
		mins.y -= margin;
		mins.z -= margin;

		maxs.x += margin;
		maxs.y += margin;
		maxs.z += margin;
	}

	/**
	*	@brief	Test whether this bounding box overlaps another bounding box with an optional safety margin.
	**/
	inline bool Overlaps( const nav_aabb_t &other, double margin = 0.0 ) const {
		if ( mins.x - margin > other.maxs.x || maxs.x + margin < other.mins.x ) return false;
		if ( mins.y - margin > other.maxs.y || maxs.y + margin < other.mins.y ) return false;
		if ( mins.z - margin > other.maxs.z || maxs.z + margin < other.mins.z ) return false;
		return true;
	}

	/**
	*	@brief	Test whether a 3D point is inside this bounding box with an optional safety margin.
	**/
	inline bool ContainsPoint( const Vector3DP &p, double margin = 0.0 ) const {
		if ( p.x < mins.x - margin || p.x > maxs.x + margin ) return false;
		if ( p.y < mins.y - margin || p.y > maxs.y + margin ) return false;
		if ( p.z < mins.z - margin || p.z > maxs.z + margin ) return false;
		return true;
	}
};

//#pragma pack(push, 1)

/**
* @brief Convex polygon extracted from a BSP walkable surface.
* @note Used as the input shape for half-edge mesh construction.
**/
struct nav_poly_t {
    //! Stable polygon identifier assigned during generation.
    int32_t poly_id = 0;
    //! Number of valid vertices stored in vertices.
    int32_t num_vertices = 0;
    //! Polygon vertices in winding order.
    Vector3DP vertices[ 1024 ] = {};
    //! Polygon centroid used for partitioning and path queries.
	Vector3DP center = {};
    //! Polygon plane normal.
	Vector3DP normal = {};
    //! BSP leaf that originally contributed this polygon.
    int32_t bsp_leaf_id = -1;
    //! Entity ID this polygon belongs to (e.g. for doors), or ENTITYNUM_NONE if world.
    int32_t entity_id = ENTITYNUM_NONE;
    //! Door entity that produced this polygon as a transition boundary, or ENTITYNUM_NONE if unrelated to a door split.
    int32_t transition_entity_id = ENTITYNUM_NONE;
};

/**
* @brief Half-edge record linking one polygon edge to its adjacent face.
**/
struct nav_halfedge_t {
    //! Index into g_nav_vertices for the edge origin.
    int32_t vertex_idx = -1;
    //! Index of the opposite half-edge, or -1 for a boundary edge.
    int32_t twin_idx = -1;
    //! Index of the next half-edge in the face loop.
    int32_t next_idx = -1;
    //! Face that owns this half-edge.
    int32_t face_idx = -1;
    //! Vertical difference between this edge and its twin edge.
    double z_diff = 0.0;
    //! If this edge is a boundary, how much was it pushed inward from the original geometry (metadata for runtime inspections).
	double wall_offset = 0.0;
    //! Entity ID of the door this edge transitions into, or ENTITYNUM_NONE.
    int32_t edge_entity_id = ENTITYNUM_NONE;
    //! Bitmask for dynamic states (e.g., NAV_EDGE_DISABLED).
    uint32_t flags = 0;
};

/**
* @brief Bitmask flags for half-edges to control runtime traversal.
**/
enum nav_edge_flags_t : uint32_t {
	NAV_EDGE_NONE = 0,
	//! This edge is temporarily blocked (e.g. a closed door) and cannot be traversed.
	NAV_EDGE_DISABLED = 1 << 0,
	//! This boundary edge represents a solid barrier rising upward from the face (wall/obstacle).
	NAV_EDGE_WALL = 1 << 1,
	//! This boundary edge represents an elevated drop-off or cliff dropping down into lower space.
	NAV_EDGE_DROPOFF = 1 << 2
};

/**
*	@brief	Topological classification of navmesh spatial regions and rooms.
**/
enum nav_zone_type_t : uint8_t {
	//! Unbounded open exterior courtyard or field.
	ZONE_TYPE_OPEN_SPACE = 0,
	//! Enclosed interior room bounded by solid walls and doorway portals.
	ZONE_TYPE_ROOM_ENCLOSED = 1,
	//! Horizontal flat passage / hallway connecting zones.
	ZONE_TYPE_CORRIDOR_FLAT = 2,
	//! Stepped staircase corridor (discrete vertical step risers >= 16 units).
	ZONE_TYPE_CORRIDOR_STAIRS = 3,
	//! Smooth sloped / inclined ramp passage (gradient >= 5 degrees).
	ZONE_TYPE_CORRIDOR_RAMP = 4,
	//! Single-portal dead-end niche or tactical defensive nook.
	ZONE_TYPE_ALCOVE = 5,
	//! Elevated catwalk, roof, or ledge with perimeter drop-off edges.
	ZONE_TYPE_ELEVATED_PLATFORM = 6
};

/**
*	@brief	Classification of transition apertures and bottleneck boundaries between adjacent zones.
**/
enum nav_portal_type_t : uint8_t {
	//! Open geometric archway / doorway with no dynamic entity.
	PORTAL_TYPE_OPEN_APERTURE = 0,
	//! Dynamic sliding or rotating door (func_door / func_door_rotating).
	PORTAL_TYPE_DOOR_ENTITY = 1,
	//! Dynamic / togglable or destructible wall brush (func_wall).
	PORTAL_TYPE_FUNC_WALL = 2,
	//! Vertical elevator or lift platform (func_plat).
	PORTAL_TYPE_ELEVATOR_PLAT = 3
};

/**
*	@brief	Transition portal boundary record linking adjacent spatial zones.
**/
struct nav_portal_t {
	//! Unique portal identifier.
	int32_t portal_id = -1;
	//! Classification of the transition boundary.
	nav_portal_type_t portal_type = PORTAL_TYPE_OPEN_APERTURE;
	//! Source zone index from which this portal exits.
	int32_t from_room_id = -1;
	//! Destination zone index which this portal enters.
	int32_t to_room_id = -1;
	//! Associated boundary half-edge index in g_nav_halfedges.
	int32_t halfedge_idx = -1;
	//! Associated dynamic entity number (e.g. for func_door or func_wall), or ENTITYNUM_NONE.
	int32_t entity_number = ENTITYNUM_NONE;
	//! World-space center point of the portal aperture.
	Vector3DP center = {};
	//! Unit normal pointing across the portal from from_room_id toward to_room_id.
	Vector3DP normal = {};
	//! Clearance width across the portal opening in units.
	double width = 0.0;
	//! Runtime passability flag (e.g. false if a door is closed or locked).
	bool is_passable = true;
};

/**
*	@brief	Spatial room / region record containing precalculated topological and geometric properties.
**/
struct nav_room_t {
	//! Unique room identifier and index inside g_nav_rooms.
	int32_t room_id = -1;
	//! Topological classification of this spatial zone.
	nav_zone_type_t zone_type = ZONE_TYPE_ROOM_ENCLOSED;
	//! Geometric centroid of the floor surface.
	Vector3DP centroid = {};
	//! Axis-aligned bounding box enclosing all faces in this zone.
	nav_aabb_t bounds = {};
	//! Indices of all nav faces comprising this room/zone.
	std::vector<int32_t> face_indices;
	//! Indices of all boundary portals linking this room to other zones.
	std::vector<int32_t> portal_indices;
	//! Average floor elevation.
	double avg_elevation = 0.0;
	//! Maximum vertical height difference across the floor surface.
	double max_elevation_delta = 0.0;
	//! Precalculated physical interior wall standoffs from centroid along principal directions.
	double wall_standoff_back = 0.0;
	double wall_standoff_left = 0.0;
	double wall_standoff_right = 0.0;
};

/**
* @brief Final nav face record used for spatial queries and pathfinding.
**/
struct nav_face_t {
    //! Stable face identifier assigned after KD-tree construction.
    int32_t face_id = 0;
    //! Index of the first half-edge belonging to this face.
    int32_t first_edge_idx = 0;
    //! Number of half-edges in the face loop.
    int32_t num_edges = 0;
    //! Face centroid.
	Vector3DP center = {};
    //! Face plane normal.
	Vector3DP normal = {};
    //! Approximate clearance radius measured from the centroid.
	double clearance = 0.0;
    //! BSP leaf that contributed this face.
    int32_t bsp_leaf_id = -1;
    //! Entity ID this face belongs to (e.g. for doors), or ENTITYNUM_NONE if world.
    int32_t entity_id = ENTITYNUM_NONE;
    //! Door entity that produced this face as a transition boundary, or ENTITYNUM_NONE if unrelated.
    int32_t transition_entity_id = ENTITYNUM_NONE;
    //! Direct back-pointer identifier to originating qbism/Q2 BSP convex brush.
    uint32_t brush_id = 0;
    //! Surface contents and material flags (e.g., CONTENTS_SOLID, CM_SURFACE_NO_NAVMESH).
    uint32_t surface_flags = 0;
    //! Topological spatial room / zone index this face belongs to (-1 if unassigned).
    int32_t room_id = -1;
    //! Mailbox query identifier to prevent redundant narrow-phase testing across adjacent leaves.
    mutable uint32_t last_query_id = 0;
};

/**
* @brief KD-tree node used for spatial localization of nav faces.
* @note Split axis values map to X=0, Y=1, and Z=2.
**/
struct nav_kdtree_node_t {
    //! Minimum bounds for this node.
	Vector3DP mins = {};
    //! Maximum bounds for this node.
	Vector3DP maxs = {};
    //! First face index for leaf nodes, or -1 for internal nodes.
    int32_t first_face_id = -1;
    //! Number of faces stored in this leaf node.
    int32_t num_faces = 0;
    //! BSP leaf represented by this node.
    int32_t bsp_leaf_id = -1;
    //! Left child node index, or -1 if none.
    int32_t left_child = -1;
    //! Right child node index, or -1 if none.
    int32_t right_child = -1;
    //! Split axis for internal nodes.
    int32_t split_axis = 0;
    //! Authoritative split coordinate position along split_axis.
    double split_pos = 0.0;
};

/**
* @brief Mapping from a BSP leaf to a contiguous span of face IDs.
* @note This allows fast lookup of candidate faces for leaf-local queries.
**/
struct nav_leaf_link_t {
    //! BSP leaf identifier.
    int32_t bsp_leaf_id = -1;
    //! Index into the flattened face-id array.
    int32_t first_face_index = 0;
    //! Number of face IDs in the flattened span.
    int32_t num_faces = 0;
};

/**
* @brief Serialized header for the nav7 file format.
**/
struct nav_header_t {
    //! File signature, must equal NAV7_MAGIC.
    uint32_t magic = 0;
    //! File format version, must equal NAV7_VERSION.
    uint32_t version = 0;
    //! BSP checksum stored with the navmesh data.
    uint32_t map_checksum = 0;
    //! Number of serialized vertices.
    int32_t num_vertices = 0;
    //! Number of serialized half-edges.
    int32_t num_halfedges = 0;
    //! Number of serialized faces.
    int32_t num_faces = 0;
    //! Number of serialized KD-tree nodes.
    int32_t num_kdtree_nodes = 0;
    //! Number of serialized BSP leaf links.
    int32_t num_leaf_links = 0;
    //! Number of flattened face IDs referenced by leaf links.
    int32_t num_leaf_face_ids = 0;
    //! Number of serialized precalculated tactical cover points.
    int32_t num_cover_points = 0;
};

//#pragma pack(pop)
