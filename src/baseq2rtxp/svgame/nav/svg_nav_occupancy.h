/****************************************************************************************************************************************
*
*
*	SVGame: Navigation Occupancy Helpers
*
*
**********************************************************************************************************************************/
#pragma once

#include <cstdint>

struct nav_occupancy_entry_t;
struct nav_mesh_t;

/**
*  @brief    Clear all dynamic occupancy records on the given mesh.
**/
void SVG_Nav_Occupancy_Clear( nav_mesh_t * mesh );

/**
*  @brief    Add a dynamic occupancy entry for a tile/cell/layer.
*  @param    mesh        Target mesh.
*  @param    tileId      Canonical tile id into `world_tiles`.
*  @param    cellIndex   Cell index inside the tile.
*  @param    layerIndex  Layer index inside the cell.
*  @param    cost        Soft cost to accumulate (default 1).
*  @param    blocked     If true, mark this location as hard blocked.
**/
void SVG_Nav_Occupancy_Add( nav_mesh_t * mesh, int32_t tileId, int32_t cellIndex, int32_t layerIndex, int32_t cost, bool blocked );

/**
*  @brief    Query the dynamic occupancy soft cost for a tile/cell/layer.
*  @return   Accumulated soft cost (0 if none or mesh missing).
**/
int32_t SVG_Nav_Occupancy_SoftCost( const nav_mesh_t * mesh, int32_t tileId, int32_t cellIndex, int32_t layerIndex );

/**
*  @brief    Query if a tile/cell/layer is marked as hard blocked.
*  @return   True if blocked.
**/
bool SVG_Nav_Occupancy_Blocked( const nav_mesh_t * mesh, int32_t tileId, int32_t cellIndex, int32_t layerIndex );

/**
*  @brief    Query the full sparse occupancy overlay entry for a tile/cell/layer.
*  @param    mesh        Navigation mesh containing the sparse occupancy overlay.
*  @param    tileId      Canonical tile id.
*  @param    cellIndex   Canonical cell index within the tile.
*  @param    layerIndex  Canonical layer index within the cell.
*  @param    out_entry   [out] Occupancy entry snapshot when present.
*  @return   True when the sparse overlay contains an entry for the queried location.
*  @note     This is the authoritative local dynamic-overlay lookup and must not imply any nav or hierarchy rebuild.
**/
bool SVG_Nav_Occupancy_TryGet( const nav_mesh_t * mesh, int32_t tileId, int32_t cellIndex, int32_t layerIndex, nav_occupancy_entry_t * out_entry );
