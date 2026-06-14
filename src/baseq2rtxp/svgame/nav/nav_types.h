#pragma once

#include "nav_core.h"

#pragma pack(push, 1)

/**
 * A convex polygon extracted from a BSP walkable face.
 * Represents a single navigable floor surface (a node in A*).
 */
struct nav_poly_t {
    int32_t poly_id;
    int32_t num_vertices;
    Vector3 vertices[8]; // Max vertices per convex polygon (safely clamped during extraction)
    Vector3 center;
    Vector3 normal;
    int32_t bsp_leaf_id; // Primary BSP leaf
};

/**
 * KD-Tree Node used for O(log N) spatial localization of polygons.
 * Split axis: 0=X, 1=Y, 2=Z.
 */
struct nav_kdtree_node_t {
    Vector3 mins;
    Vector3 maxs;
    int32_t poly_id;      // -1 if it's an internal node, valid poly ID if it's a leaf node.
    int32_t bsp_leaf_id;  // Corresponding BSP leaf for this node
    int32_t left_child;   // Index to left child node (-1 if none)
    int32_t right_child;  // Index to right child node (-1 if none)
    int32_t split_axis;
};

/**
 * Lookup mapping linking a specific BSP Leaf ID to a continuous list of Poly IDs.
 * Enables O(1) starting-node lookups simply by knowing the entity's current BSP leaf.
 */
struct nav_leaf_link_t {
    int32_t bsp_leaf_id;
    int32_t first_poly_index; // Index into a continuous list of polygon IDs
    int32_t num_polys;        // How many polys reside in this leaf
};

/**
 * Serialized file header for .nav6 format.
 */
struct nav_header_t {
    uint32_t magic;         // NAV6_MAGIC
    uint32_t version;       // NAV6_VERSION
    uint32_t map_checksum;  // BSP Checksum to ensure validity
    
    int32_t num_polys;
    int32_t num_kdtree_nodes;
    int32_t num_leaf_links;
    int32_t num_leaf_poly_ids; // Length of the flat array containing poly IDs referenced by leaf links
};

#pragma pack(pop)
