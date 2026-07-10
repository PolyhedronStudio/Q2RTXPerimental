#pragma once

#include "nav_core.h"
#include "nav_containers.h"

#pragma pack(push, 1)

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
    Vector3 vertices[ 1024 ] = {};
    //! Polygon centroid used for partitioning and path queries.
    Vector3 center = {};
    //! Polygon plane normal.
    Vector3 normal = {};
    //! BSP leaf that originally contributed this polygon.
    int32_t bsp_leaf_id = -1;
    //! Entity ID this polygon belongs to (e.g. for doors), or ENTITYNUM_NONE if world.
    int32_t entity_id = ENTITYNUM_NONE;
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
    float z_diff = 0.0f;
    //! If this edge is a boundary, how much was it pushed inward from the original geometry (metadata for runtime inspections).
    float wall_offset = 0.0f;
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
    NAV_EDGE_DISABLED = 1 << 0
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
    Vector3 center = {};
    //! Face plane normal.
    Vector3 normal = {};
    //! Approximate clearance radius measured from the centroid.
    float clearance = 0.0f;
    //! BSP leaf that contributed this face.
    int32_t bsp_leaf_id = -1;
    //! Entity ID this face belongs to (e.g. for doors), or ENTITYNUM_NONE if world.
    int32_t entity_id = ENTITYNUM_NONE;
};

/**
* @brief KD-tree node used for spatial localization of nav faces.
* @note Split axis values map to X=0, Y=1, and Z=2.
**/
struct nav_kdtree_node_t {
    //! Minimum bounds for this node.
    Vector3 mins = {};
    //! Maximum bounds for this node.
    Vector3 maxs = {};
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
};

#pragma pack(pop)
