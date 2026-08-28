#pragma once

#include "svgame/svg_local.h"

/**
* @brief NavMesh file format identifiers and generation limits.
* @note These values are shared by the generator, pathfinder, and persistence layers.
**/
#define NAV7_MAGIC 'NAV7'
#define NAV7_VERSION 5

//! Upper bound for the number of generated nav faces kept in memory.
constexpr int32_t NAV_MAX_FACES = 262144;
//! Upper bound for the number of generated half-edges kept in memory.
constexpr int32_t NAV_MAX_HALFEDGES = 1048576;
//! Upper bound for the number of generated vertices kept in memory.
constexpr int32_t NAV_MAX_VERTICES = 524288;
//! Upper bound for KD-tree nodes allocated during nav generation.
constexpr int32_t NAV_MAX_KDTREE_NODES = 16777216;
//! Upper bound for BSP leaf links recorded during generation.
constexpr int32_t NAV_MAX_LEAF_LINKS = 16384;



//! Maximum step height in Quake units that AI may step up without extra logic.
constexpr float NAV_MAX_STEP_HEIGHT = PHYS_STEP_MAX_SIZE + PHYS_STEP_GROUND_DIST;
//! Minimum surface normal Z value required for a face to count as walkable.
constexpr float NAV_MIN_WALKABLE_Z = 0.65f;

//! Minimum horizontal width of a portal that is considered traversable.
constexpr float NAV_LADDER_AGENT_CLEARANCE = 16.0f;
//! Minimum vertical clearance above a ladder tread for safe traversal.
constexpr float NAV_LADDER_SAFETY_MARGIN = 2.0f;