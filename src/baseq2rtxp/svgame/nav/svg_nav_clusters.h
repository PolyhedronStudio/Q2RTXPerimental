/********************************************************************
*
*
*    SVGame: Navigation Cluster Graph Helpers
*
*
********************************************************************/
#pragma once

#include "svgame/nav/svg_nav.h"

#include <vector>

/**
*	@brief	Cluster routing configuration CVars.
**/
extern cvar_t *nav_cluster_route_weighted;
extern cvar_t *nav_cluster_cost_stair;
extern cvar_t *nav_cluster_cost_water;
extern cvar_t *nav_cluster_cost_lava;
extern cvar_t *nav_cluster_cost_slime;
extern cvar_t *nav_cluster_forbid_stair;
extern cvar_t *nav_cluster_forbid_water;
extern cvar_t *nav_cluster_forbid_lava;
extern cvar_t *nav_cluster_forbid_slime;
extern cvar_t *nav_cluster_enable;
extern cvar_t *nav_cluster_debug_draw;
extern cvar_t *nav_cluster_debug_draw_graph;

/**
*	@brief	Builds the tile-level cluster graph for a navigation mesh.
*	@param	mesh	Navigation mesh containing world tiles.
*	@note	Used by generation code to populate coarse tile routes.
**/
void SVG_Nav_ClusterGraph_BuildFromMesh_World( const nav_mesh_t *mesh );

/**
*    @brief    Clear cached cluster graph state and adjacency data.
**/
void SVG_Nav_ClusterGraph_Clear( void );

/**
*    @brief    Find a tile-route between start and goal positions via the cluster graph.
*    @param    mesh        Navigation mesh containing world tiles.
*    @param    startPos    World-space start position.
*    @param    goalPos     World-space goal position.
*    @param    outRoute    [out] Tile keys forming the coarse route.
*    @return   True if a route was found.
**/
const bool SVG_Nav_ClusterGraph_FindRoute( const nav_mesh_t *mesh, const Vector3 &startPos, const Vector3 &goalPos,
    std::vector<nav_tile_cluster_key_t> &outRoute );
/**
*	@brief    Get the tile key for a given world position.
**/
nav_tile_cluster_key_t SVG_Nav_GetTileKeyForPosition( const nav_mesh_t *mesh, const Vector3 &pos );