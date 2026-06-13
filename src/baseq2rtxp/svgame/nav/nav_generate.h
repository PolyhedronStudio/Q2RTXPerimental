#pragma once

#include "nav_core.h"
#include "nav_types.h"
#include "nav_containers.h"

// Expose the global navmesh data for saving/loading and pathfinding
extern nav_vector_t<nav_poly_t> g_nav_polys;
extern nav_vector_t<nav_kdtree_node_t> g_nav_nodes;
extern nav_vector_t<nav_leaf_link_t> g_nav_leaf_links;
extern nav_vector_t<int32_t> g_nav_leaf_poly_ids;

// Triggers the generation command from the console
void Nav_GenerateCommand();

// Clears the current active navmesh from memory
void Nav_Clear();

// Starts the actual extraction (called by the async thread)
void Nav_DoExtractionWork();
