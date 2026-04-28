/********************************************************************
*
*
*	ServerGame: Nav2 Query Interface Internal Legacy Seam
*
*
********************************************************************/
#pragma once

#include "svgame/nav2/nav2_query_iface.h"
#include "svgame/nav2/nav2_span_adjacency.h"



/**
*
*
*	Nav2 Query Internal Legacy Conversions:
*
*
**/
/**
*	@brief	Build a staged legacy path-follow policy snapshot from a nav2-owned policy wrapper.
*	@param	policy	Nav2-owned path-follow policy wrapper.
*	@return	Legacy policy snapshot consumed by the staged implementation.
**/
nav2_path_policy_t SVG_Nav2_QueryBuildLegacyPolicy( const nav2_query_policy_t &policy );

/**
 *	@brief	Build an agent-adjacency policy snapshot from a nav2-owned request policy wrapper.
 *	@param	policy	Nav2-owned path-follow policy wrapper.
 *	@return	Adjacency policy snapshot used by span adjacency and fine refinement.
 **/
nav2_span_adjacency_policy_t SVG_Nav2_QueryBuildAdjacencyPolicy( const nav2_query_policy_t &policy );

/**
*	@brief	Build a staged legacy path-process snapshot from a nav2-owned process wrapper.
*	@param	process	Nav2-owned path-process wrapper.
*	@return	Legacy process snapshot consumed by the staged implementation.
**/
nav2_path_process_t SVG_Nav2_QueryBuildLegacyProcess( const nav2_query_process_t &process );

/**
*	@brief	Copy staged legacy path-process state back into a nav2-owned wrapper.
*	@param	legacyProcess	Staged legacy path-process state produced by the implementation.
*	@param	outProcess	[out] Nav2-owned process wrapper receiving mirrored state.
**/
void SVG_Nav2_QueryCopyProcessFromLegacy( const nav2_path_process_t &legacyProcess, nav2_query_process_t *outProcess );

/**
*	@brief	Return the staged `main mesh` pointer wrapped by a nav2 mesh wrapper.
*	@param	mesh	Nav2-owned mesh wrapper.
*	@return	Legacy mesh pointer consumed by the staged implementation.
**/
const nav2_mesh_t *SVG_Nav2_QueryGetLegacyMesh( const nav2_query_mesh_t &mesh );

/**
*	@brief	Build a nav2-owned read-only layer view from a staged runtime layer.
*	@param	legacyLayer	Legacy runtime layer to mirror.
*	@return	Nav2-owned layer view containing copied scalar metadata.
**/
//nav2_query_layer_view_t SVG_Nav2_QueryMakeLayerView( const nav2_layer_t &legacyLayer );

/**
*	@brief	Build a nav2-owned read-only cell view from staged runtime cell storage.
*	@param	legacyCell	Legacy runtime cell to mirror.
*	@return	Nav2-owned cell view containing copied layer metadata.
**/
//nav2_query_cell_view_t SVG_Nav2_QueryMakeCellView( const nav2_xy_cell_t &legacyCell );

/**
*	@brief	Build a nav2-owned read-only tile view from staged runtime tile storage.
*	@param	legacyTile	Legacy runtime tile to mirror.
*	@return	Nav2-owned tile view containing copied summary metadata.
**/
nav2_query_tile_view_t SVG_Nav2_QueryMakeTileView( const nav2_tile_t &legacyTile );

/**
*	@brief	Build a nav2-owned read-only inline-model runtime view from a staged runtime entry.
*	@param	legacyRuntime	Legacy runtime entry to mirror.
*	@return	Nav2-owned inline-model runtime view containing copied scalar metadata.
**/
nav2_query_inline_model_runtime_view_t SVG_Nav2_QueryMakeInlineModelRuntimeView( const nav2_inline_model_runtime_t &legacyRuntime );
