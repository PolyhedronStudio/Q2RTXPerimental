#pragma once

#include "nav_core.h"

#pragma pack(push, 1)

/**
 * A convex polygon extracted from a BSP walkable face.
 * Represents a single navigable floor surface.
 * For half-edge mesh, this is primarily used during generation to build the final nav_face_t.
 */
struct nav_poly_t {
    int32_t poly_id;
    int32_t num_vertices;
    Vector3 vertices[1024]; // Max vertices per convex polygon (safely clamped during extraction)
    Vector3 center;
    Vector3 normal;
    int32_t bsp_leaf_id; // Primary BSP leaf
};

struct nav_halfedge_t {
    int32_t vertex_idx; // Index into g_nav_vertices (origin of this half-edge)
    int32_t twin_idx;   // Opposite half-edge index (-1 if boundary)
    int32_t next_idx;   // Next half-edge in the face loop
    int32_t face_idx;   // The face this half-edge belongs to
    float   z_diff;     // Z-axis height difference to the twin's face.
};

struct nav_face_t {
    int32_t face_id;
    int32_t first_edge_idx; // Index to an arbitrary half-edge of this face
    int32_t num_edges;
    Vector3 center;
    Vector3 normal;
    float   clearance;      // Inscribed radius (max clearance) from center
    int32_t bsp_leaf_id;
};

/**
 * KD-Tree Node used for O(log N) spatial localization of polygons.
 * Split axis: 0=X, 1=Y, 2=Z.
 */
struct nav_kdtree_node_t {
    Vector3 mins;
    Vector3 maxs;
    int32_t first_face_id; // -1 if it's an internal node, valid first face index if it's a leaf node.
    int32_t num_faces;    // Number of faces in this leaf node (0 for internal nodes).
    int32_t bsp_leaf_id;  // Corresponding BSP leaf for this node
    int32_t left_child;   // Index to left child node (-1 if none)
    int32_t right_child;  // Index to right child node (-1 if none)
    int32_t split_axis;
};

/**
 * Lookup mapping linking a specific BSP Leaf ID to a continuous list of Face IDs.
 * Enables O(1) starting-node lookups simply by knowing the entity's current BSP leaf.
 */
struct nav_leaf_link_t {
    int32_t bsp_leaf_id;
    int32_t first_face_index; // Index into a continuous list of face IDs
    int32_t num_faces;        // How many faces reside in this leaf
};

/**
 * Serialized file header for .nav7 format.
 */
struct nav_header_t {
    uint32_t magic;         // NAV7_MAGIC
    uint32_t version;       // NAV7_VERSION
    uint32_t map_checksum;  // BSP Checksum to ensure validity
    
    int32_t num_vertices;
    int32_t num_halfedges;
    int32_t num_faces;
    int32_t num_kdtree_nodes;
    int32_t num_leaf_links;
    int32_t num_leaf_face_ids; // Length of the flat array containing face IDs referenced by leaf links
};

#pragma pack(pop)
