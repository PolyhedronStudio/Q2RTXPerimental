
/********************************************************************
*
*
*	SVGame: Navigation Voxelmesh Generator
*
*
********************************************************************/
#pragma once

#include "svgame/nav/svg_nav.h"

/**
*	@brief	Generate navigation voxelmesh for the current level.
*	@note	This is the main entry point called by the `sv nav_gen_voxelmesh` server command.
*/
void SVG_Nav_GenerateVoxelMesh( void );

/**
*
*	Forward Declarations:
*
**/
struct svg_base_edict_t;


/**
*   @brief  Generate navigation mesh for world (world-only collision).
*           Creates tiles for each BSP leaf containing walkable surface samples.
*   @param  mesh    Navigation mesh structure to populate.
**/
void GenerateWorldMesh( nav_mesh_t *mesh );

/**
*   @brief  Generate navigation mesh for inline BSP models (brush entities).
*           Creates tiles for each inline model in local space for later transform application.
*   @param  mesh    Navigation mesh structure to populate.
**/
void GenerateInlineModelMesh( nav_mesh_t *mesh );
/**
*	@brief	Build runtime inline model data for navigation mesh.
*	@note	This allocates and initializes the runtime array. Per-frame updates should call
*			`SVG_Nav_RefreshInlineModelRuntime()` (no allocations).
**/
void Nav_BuildInlineModelRuntime( nav_mesh_t *mesh, const std::unordered_map<int32_t, svg_base_edict_t *> &model_to_ent );