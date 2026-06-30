#pragma once

#include "svgame/svg_local.h"

/**
* @brief NavMesh file format identifiers and generation limits.
* @note These values are shared by the generator, pathfinder, and persistence layers.
**/
#define NAV7_MAGIC 'NAV7'
#define NAV7_VERSION 2

//! Upper bound for the number of generated nav faces kept in memory.
constexpr int32_t MAX_NAV_FACES = 262144;
//! Upper bound for the number of generated half-edges kept in memory.
constexpr int32_t MAX_NAV_HALFEDGES = 1048576;
//! Upper bound for the number of generated vertices kept in memory.
constexpr int32_t MAX_NAV_VERTICES = 524288;
//! Upper bound for KD-tree nodes allocated during nav generation.
constexpr int32_t MAX_NAV_KDTREE_NODES = 524288;
//! Upper bound for BSP leaf links recorded during generation.
constexpr int32_t MAX_NAV_LEAF_LINKS = 16384;

//! Maximum step height in Quake units that AI may step up without extra logic.
constexpr float NAV_MAX_STEP_HEIGHT = 16.0f;
//! Minimum surface normal Z value required for a face to count as walkable.
constexpr float NAV_MIN_WALKABLE_Z = 0.65f;
