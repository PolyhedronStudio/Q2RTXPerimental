/** * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 
* 
* 
* 	SVGame: Navigation Voxelmesh Generator - Implementation
* 
* 
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
#include "shared/config.h" 
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"

#include "svgame/nav/svg_nav.h"
#include "svgame/nav/svg_nav_clusters.h"
#include "svgame/nav/svg_nav_debug.h"
#include "svgame/nav/svg_nav_load.h"
#include "svgame/nav/svg_nav_generate.h"
#include "svgame/nav/svg_nav_path_process.h"
#include "svgame/nav/svg_nav_request.h"
#include "svgame/nav/svg_nav_traversal.h"

#include "svgame/entities/svg_player_edict.h"

// <Q2RTXP>: TODO: Move to shared? Ehh.. 
// Common BSP access for navigation generation.
#include "common/bsp.h"
//#include "common/files.h"

#include <algorithm>

//! Deterministic coarse region budget used to split connected tile space into portal-bearing static regions.
static constexpr int32_t NAV_HIERARCHY_MAX_TILES_PER_REGION = 32;
//! Regions at or above this size are logged as coarse partition saturation points.
static constexpr int32_t NAV_HIERARCHY_OVERSIZED_REGION_TILE_COUNT = NAV_HIERARCHY_MAX_TILES_PER_REGION;



/** 
* 
* 
* 
*    Navigation Global Variables and CVars:
* 
* 
* 
**/
/** 
* 	Navigation Debug CVar Variables:
**/
//cvar_t *nav_debug_draw_voxelmesh = nullptr;
//cvar_t *nav_debug_draw_walkable_sample_points = nullptr;
//cvar_t *nav_debug_draw_pathfinding = nullptr;

/** 
*    @brief  Global navigation mesh instance (RAII owner).
*         Stores the complete navigation data for the current level.
*    @note   Automatically manages construction/destruction via RAII helper.
**/
// Use g_nav_mesh.get() to access the raw pointer when needed.
nav_mesh_raii_t g_nav_mesh;

//! Profiling/logging CVars
cvar_t *nav_profile_level = nullptr;
//! Maximum allowed time per A*  step in milliseconds (used to cap processing time and allow incremental stepping).
cvar_t *nav_astar_step_budget_ms = nullptr;
//! Feature flag controlling whether Phase 5 hierarchy coarse routing is attempted before tile-cluster fallback.
cvar_t *nav_hierarchy_route_enable = nullptr;
//! Maximum LOS sample count allowed before direct-shortcut LOS is skipped for tuning.
cvar_t *nav_direct_los_attempt_max_samples = nullptr;
//! Minimum 2D query distance required before hierarchy routing is preferred over local-only refinement.
cvar_t *nav_hierarchy_route_min_distance = nullptr;
//! Maximum LOS tests allowed during the small sync simplification pass.
cvar_t *nav_simplify_sync_max_los_tests = nullptr;
//! Maximum LOS tests allowed during the more aggressive async simplification pass.
cvar_t *nav_simplify_async_max_los_tests = nullptr;
//! Maximum wall-clock milliseconds the sync simplification pass may spend per query.
cvar_t *nav_simplify_sync_max_ms = nullptr;
//! Exact corridor tile-count threshold where refinement widening switches from near to mid radius.
cvar_t *nav_refine_corridor_mid_tiles = nullptr;
//! Exact corridor tile-count threshold where refinement widening switches from mid to far radius.
cvar_t *nav_refine_corridor_far_tiles = nullptr;
//! Buffered refinement radius used for short exact corridors.
cvar_t *nav_refine_corridor_radius_near = nullptr;
//! Buffered refinement radius used for mid-length exact corridors.
cvar_t *nav_refine_corridor_radius_mid = nullptr;
//! Buffered refinement radius used for long exact corridors.
cvar_t *nav_refine_corridor_radius_far = nullptr;
//! Minimum 2D LOS span treated as a long simplification collapse for narrow-passage clearance tuning.
cvar_t *nav_simplify_long_span_min_distance = nullptr;
//! Additional clearance margin required before allowing long LOS simplification spans.
cvar_t *nav_simplify_clearance_margin = nullptr;
//! Angle threshold used when pruning nearly collinear simplified waypoints.
cvar_t *nav_simplify_collinear_angle_degrees = nullptr;

//! XY grid cell size in world units.
cvar_t *nav_cell_size_xy = nullptr;
//! Number of cells per tile dimension.
cvar_t *nav_tile_size = nullptr;

//! Z-axis quantization step.
cvar_t *nav_z_quant = nullptr;
// Optional vertical tolerance (world units). If <= 0, auto-derived from mesh parameters.
cvar_t *nav_z_tolerance = nullptr;

//! Maximum step height (matches player PM_STEP_MAX_SIZE).
cvar_t *nav_max_step = nullptr;
//! Maximum allowed downward traversal drop.
cvar_t *nav_max_drop = nullptr;
//! Cap applied when rejecting large drops during path validation.
cvar_t *nav_max_drop_height_cap = nullptr;
//! Minimum walkable surface normal Z threshold.
cvar_t *nav_max_slope_normal_z = nullptr;

//! Agent bounding box minimum X.
cvar_t *nav_agent_mins_x = nullptr;
//! Agent bounding box minimum Y.
cvar_t *nav_agent_mins_y = nullptr;
//! Agent bounding box minimum Z.
cvar_t *nav_agent_mins_z = nullptr;
//! Agent bounding box maximum X.
cvar_t *nav_agent_maxs_x = nullptr;
//! Agent bounding box maximum Y.
cvar_t *nav_agent_maxs_y = nullptr;
//! Agent bounding box maximum Z.
cvar_t *nav_agent_maxs_z = nullptr;


/** 
* 
* 
* 
* 		Runtime cost - tuning CVars for A * heuristics(defined here)
*  
*  
*  
**/
//! Distance cost weight.
cvar_t *nav_cost_w_dist = nullptr;
//! Base cost for jump actions (added to the cost of jump nodes, before applying height multiplier).
cvar_t *nav_cost_jump_base = nullptr;
//! Multiplier applied to jump nodes based on their height (e.g., taller jumps are more costly).
cvar_t *nav_cost_jump_height_weight = nullptr;
//! Line-of-sight cost weight (multiplier applied to nodes without LOS to the goal).
cvar_t *nav_cost_los_weight = nullptr;
//! Dynamic cost weight (multiplier applied to nodes with dynamic occupancy).
cvar_t *nav_cost_dynamic_weight = nullptr;
//! Failure cost weight (multiplier applied to nodes that have failed traversal recently).
cvar_t *nav_cost_failure_weight = nullptr;
//! Failure cost decay time constant in milliseconds (controls how long failed nodes remain costly).
cvar_t *nav_cost_failure_tau_ms = nullptr;
//! Turn cost weight (multiplier applied to nodes that require a significant direction change).
cvar_t *nav_cost_turn_weight = nullptr;
//! Slope cost weight (multiplier applied to nodes with steep slopes).
cvar_t *nav_cost_slope_weight = nullptr;
//! Drop cost weight (multiplier applied to nodes with large downward transitions).
cvar_t *nav_cost_drop_weight = nullptr;
//! Goal Z blend factor (0..1) for blending desired layer Z between start and goal based on distance.
cvar_t *nav_cost_goal_z_blend_factor = nullptr;
//! Minimum cost per unit distance (used to prevent zero-cost edges and ensure consistent heuristics).
cvar_t *nav_cost_min_cost_per_unit = nullptr;



/** 
* 
* 
* 
*    Navigation Constants:
* 
* 
* 
**/

/** 
* 	Forward declarations for tile sizing helpers.
* 	These are used by the cluster-graph helpers declared near the top of this TU.
**/
static inline double Nav_TileWorldSize( const nav_mesh_t * mesh );
static inline int32_t Nav_WorldToTileCoord( double value, double tile_world_size );

/** 
* 	Navigation Cluster Graph Routing:
* 
* 	Tile-route pre-pass can be either:
* 		- Unweighted BFS (fewest tile hops)
* 		- Weighted Dijkstra (flag-aware coarse costs)
* 
* 	The weighted mode keeps the hierarchical benefit but lets you bias away from
* 	undesirable regions (water/lava/slime/stairs) before running fine A* .
**/


/** 
* 	@brief	Lightweight logging wrapper for navigation subsystem.
* 	@note	For now forwards to engine console printing; allows future redirection.
**/
/** 
* 	@brief	Lightweight logging wrapper for navigation subsystem.
* 	@param	fmt printf-style format string.
* 	@note	Forwards to centralized Com_Printf so messages go through the global
* 		console/logging pipeline (and thus to the configured logfile). This keeps
* 		navigation logging consistent with the rest of the engine.
**/
void SVG_Nav_Log( const char * fmt, ... ) {
	char buf[ 1024 ];
	va_list args;
	va_start( args, fmt );
	vsnprintf( buf, sizeof( buf ), fmt, args );
	va_end( args );

	// Dedicated nav file logging is disabled by default. If enabled in future
	// and engine file-write helpers are exported to the game module, implement
	// file writing here. For now, always forward to engine console printing.

	// Always forward to centralized printing which also handles logfile routing.
	gi.dprintf( "%s", buf );
}

/** 
*  @brief    Validate that an agent bbox is well-formed.
*  @param    mins    Minimum extents.
*  @param    maxs    Maximum extents.
*  @return   True when mins are strictly lower than maxs on all axes.
**/
static inline const bool Nav_AgentBoundsAreValid( const Vector3 &mins, const Vector3 &maxs ) {
	return ( maxs[ 0 ] > mins[ 0 ] ) && ( maxs[ 1 ] > mins[ 1 ] ) && ( maxs[ 2 ] > mins[ 2 ] );
}

int32_t SVG_Nav_GetInlineModelRuntimeIndexForOwnerEntNum( const nav_mesh_t * mesh, const int32_t owner_entnum ) {
	if ( !mesh || owner_entnum <= 0 ) {
		return -1;
	}
	auto it = mesh->inline_model_runtime_index_of.find( owner_entnum );
	if ( it == mesh->inline_model_runtime_index_of.end() ) {
		return -1;
	}
	const int32_t idx = it->second;
	if ( idx < 0 || idx >= mesh->num_inline_model_runtime ) {
		return -1;
	}
	return idx;
}

const nav_inline_model_runtime_t * SVG_Nav_GetInlineModelRuntimeByIndex( const nav_mesh_t * mesh, const int32_t idx ) {
	if ( !mesh || idx < 0 || idx >= mesh->num_inline_model_runtime || !mesh->inline_model_runtime ) {
		return nullptr;
	}
	return &mesh->inline_model_runtime[ idx ];
}

/** 
*  @brief	Convert a legacy/content flag mask into traversal-oriented feature bits.
*  @param	layer_flags	Legacy/content flags stored on the layer.
*  @return Traversal feature bits implied directly by the content flags.
**/
uint32_t SVG_Nav_BuildTraversalFeatureBitsFromLayerFlags( const uint32_t layer_flags ) {
	/** 
	*  Start from an empty traversal feature mask.
	**/
	uint32_t traversal_bits = NAV_TRAVERSAL_FEATURE_NONE;

	/** 
	*  Mirror the directly-encoded environmental semantics into traversal features.
	**/
	if ( ( layer_flags & NAV_FLAG_LADDER ) != 0 ) {
		traversal_bits |= NAV_TRAVERSAL_FEATURE_LADDER;
	}
	if ( ( layer_flags & NAV_FLAG_WATER ) != 0 ) {
		traversal_bits |= NAV_TRAVERSAL_FEATURE_WATER;
	}
	if ( ( layer_flags & NAV_FLAG_LAVA ) != 0 ) {
		traversal_bits |= NAV_TRAVERSAL_FEATURE_LAVA;
	}
	if ( ( layer_flags & NAV_FLAG_SLIME ) != 0 ) {
		traversal_bits |= NAV_TRAVERSAL_FEATURE_SLIME;
	}

	return traversal_bits;
}

/** 
*  @brief	Resolve the canonical world tile view for a node reference.
*  @param	mesh	Navigation mesh owning the canonical tile storage.
*  @param	node	Node reference whose tile should be resolved.
*  @return	Pointer to the canonical tile, or nullptr when the node/tile is invalid.
*  @note	This keeps hot-path callers from repeating tile-index bounds checks inline.
**/
const nav_tile_t * SVG_Nav_GetNodeTileView( const nav_mesh_t * mesh, const nav_node_ref_t &node ) {
	/** 
	* 	Sanity checks: require mesh storage and a valid canonical tile index.
	**/
	if ( !mesh || node.key.tile_index < 0 || node.key.tile_index >= ( int32_t )mesh->world_tiles.size() ) {
		return nullptr;
	}

	return &mesh->world_tiles[ node.key.tile_index ];
}

/** 
* 	@brief	Resolve the canonical XY cell view for a node reference.
* 	@param	mesh	Navigation mesh owning the canonical tile storage.
* 	@param	node	Node reference whose cell should be resolved.
* 	@return	Pointer to the canonical XY cell, or nullptr when the node/cell is invalid.
* 	@note	This composes `SVG_Nav_GetNodeTileView()` with the tile cell accessor so hot-path users can avoid duplicating both checks.
**/
const nav_xy_cell_t * SVG_Nav_GetNodeCellView( const nav_mesh_t * mesh, const nav_node_ref_t &node ) {
	/** 
	*  Resolve the owning canonical tile before touching cell storage.
	**/
	const nav_tile_t * tile = SVG_Nav_GetNodeTileView( mesh, node );
	if ( !tile ) {
		return nullptr;
	}

	/** 
	*  Resolve the sparse cell array and validate the node's cell index.
	**/
	auto cellsView = SVG_Nav_Tile_GetCells( mesh, tile );
	const nav_xy_cell_t * cellsPtr = cellsView.first;
	const int32_t cellsCount = cellsView.second;
	if ( !cellsPtr || node.key.cell_index < 0 || node.key.cell_index >= cellsCount ) {
		return nullptr;
	}

	return &cellsPtr[ node.key.cell_index ];
}

/** 
*  @brief	Map an XY neighbor offset onto the persisted per-edge direction index.
*  @param	cell_dx	Neighbor cell X offset.
*  @param	cell_dy	Neighbor cell Y offset.
*  @return Persisted edge-slot index, or -1 when the offset is not one of the 8 stored directions.
**/
static int32_t Nav_EdgeDirIndexForOffset( const int32_t cell_dx, const int32_t cell_dy ) {
	/** 
	*  Persist only the immediate 8-direction XY neighborhood.
	**/
	if ( cell_dx == 1 && cell_dy == 0 ) return 0;
	if ( cell_dx == -1 && cell_dy == 0 ) return 1;
	if ( cell_dx == 0 && cell_dy == 1 ) return 2;
	if ( cell_dx == 0 && cell_dy == -1 ) return 3;
	if ( cell_dx == 1 && cell_dy == 1 ) return 4;
	if ( cell_dx == 1 && cell_dy == -1 ) return 5;
	if ( cell_dx == -1 && cell_dy == 1 ) return 6;
	if ( cell_dx == -1 && cell_dy == -1 ) return 7;
	return -1;
}

/** 
*  @brief	Resolve a canonical layer pointer from a node reference.
*  @param	mesh	Navigation mesh.
*  @param	node	Node reference to resolve.
*  @return Pointer to the canonical layer or nullptr on failure.
**/
const nav_layer_t * SVG_Nav_GetNodeLayerView( const nav_mesh_t * mesh, const nav_node_ref_t &node ) {
	/** 
   * Resolve the owning canonical cell before touching layer storage.
	**/
	const nav_xy_cell_t * cell = SVG_Nav_GetNodeCellView( mesh, node );
	if ( !cell ) {
		return nullptr;
	}

	/** 
   * Resolve the target layer through the cell's safe layer view.
	**/
	auto layersView = SVG_Nav_Cell_GetLayers( cell );
	const nav_layer_t * layersPtr = layersView.first;
	const int32_t layerCount = layersView.second;
	if ( !layersPtr || node.key.layer_index < 0 || node.key.layer_index >= layerCount ) {
		return nullptr;
	}

	return &layersPtr[ node.key.layer_index ];
}

/** 
*  @brief	Query traversal feature bits for a canonical node.
*  @param	mesh	Navigation mesh.
*  @param	node	Node reference to inspect.
*  @return Traversal feature bits for the node, or `NAV_TRAVERSAL_FEATURE_NONE` on failure.
**/
uint32_t SVG_Nav_GetNodeTraversalFeatureBits( const nav_mesh_t * mesh, const nav_node_ref_t &node ) {
	/** 
	*  Resolve the canonical layer before reading its traversal metadata.
	**/
	const nav_layer_t * layer = SVG_Nav_GetNodeLayerView( mesh, node );
	return layer ? layer->traversal_feature_bits : NAV_TRAVERSAL_FEATURE_NONE;
}

/** 
*  @brief	Query explicit edge metadata for a canonical node and XY offset.
*  @param	mesh	Navigation mesh.
*  @param	node	Source node reference.
*  @param	cell_dx	Neighbor cell X offset in `[-1, 1]`.
*  @param	cell_dy	Neighbor cell Y offset in `[-1, 1]`.
*  @return Explicit edge feature bits, or `NAV_EDGE_FEATURE_NONE` when no persisted edge slot applies.
**/
uint32_t SVG_Nav_GetEdgeFeatureBitsForOffset( const nav_mesh_t * mesh, const nav_node_ref_t &node, const int32_t cell_dx, const int32_t cell_dy ) {
	/** 
	*  Resolve the persisted edge slot index for this XY neighbor offset.
	**/
	const int32_t edge_dir_index = Nav_EdgeDirIndexForOffset( cell_dx, cell_dy );
	if ( edge_dir_index < 0 ) {
		return NAV_EDGE_FEATURE_NONE;
	}

	/** 
	*  Resolve the canonical layer before reading the persisted edge metadata.
	**/
	const nav_layer_t * layer = SVG_Nav_GetNodeLayerView( mesh, node );
	if ( !layer ) {
		return NAV_EDGE_FEATURE_NONE;
	}

	/** 
	*  Return only explicitly persisted edge data.
	**/
	if ( ( layer->edge_valid_mask & ( 1u << edge_dir_index ) ) == 0 ) {
		return NAV_EDGE_FEATURE_NONE;
	}

	return layer->edge_feature_bits[ edge_dir_index ];
}

/** 
*  @brief	Query coarse tile summary bits for the tile owning a canonical node.
*  @param	mesh	Navigation mesh.
*  @param	node	Node reference whose tile should be inspected.
*  @return Tile summary bits, or `NAV_TILE_SUMMARY_NONE` on failure.
**/
uint32_t SVG_Nav_GetTileSummaryBitsForNode( const nav_mesh_t * mesh, const nav_node_ref_t &node ) {
	/** 
	*  Validate the canonical tile index before reading tile-level metadata.
	**/
	if ( !mesh || node.key.tile_index < 0 || node.key.tile_index >= ( int32_t )mesh->world_tiles.size() ) {
		return NAV_TILE_SUMMARY_NONE;
	}

	return mesh->world_tiles[ node.key.tile_index ].traversal_summary_bits | mesh->world_tiles[ node.key.tile_index ].edge_summary_bits;
}

/** 
* @brief    Convert a caller-provided feet-origin into nav-center space.
*	@note     Helper implementation for `SVG_Nav_ConvertFeetToCenter` declared in header.
**/
const Vector3 SVG_Nav_ConvertFeetToCenter( const nav_mesh_t * mesh, const Vector3 &feet_origin, const Vector3 * agent_mins, const Vector3 * agent_maxs ) {
	if ( !mesh ) {
		return feet_origin;
	}

	// Choose agent bbox: prefer explicit override, otherwise use mesh defaults.
	const Vector3 &mins = agent_mins ? * agent_mins : mesh->agent_mins;
	const Vector3 &maxs = agent_maxs ? * agent_maxs : mesh->agent_maxs;

	/** 
	*  Boundary guard:
	*      External callers must provide feet-origin inputs and a valid bbox profile.
	*      If bounds are invalid, keep the input unchanged and emit a diagnostic once.
	**/
	if ( !Nav_AgentBoundsAreValid( mins, maxs ) ) {
		static bool s_logged_invalid_agent_bounds = false;
		if ( !s_logged_invalid_agent_bounds ) {
			s_logged_invalid_agent_bounds = true;
			gi.dprintf( "[WARNING][Nav][Boundary] SVG_Nav_ConvertFeetToCenter received invalid agent bounds mins=(%.1f %.1f %.1f) maxs=(%.1f %.1f %.1f). Returning feet-origin unchanged.\n",
				mins.x, mins.y, mins.z,
				maxs.x, maxs.y, maxs.z );
		}
		return feet_origin;
	}

	// Compute center offset Z as average of mins/maxs Z in nav-center space.
	const float centerOffsetZ = ( mins.z + maxs.z )* 0.5f;

    // Apply only Z offset; XY remain unchanged.
	Vector3 out = feet_origin;
	out[ 2 ] += centerOffsetZ;
	return out;
}

/** 
* 	@brief	Extract tile XY from a world-space position.
* 	@param	mesh	Navigation mesh.
* 	@param	pos	World-space position.
* 	@return	Tile key for the position.
**/
/** 
* 
* 	Forward Declarations (Debug):
* 
**/
/** 
*    @brief  Refresh the runtime data for inline models (transforms only).
*         This updates the origin/angles of inline model runtime entries based on current entity state.
**/
void SVG_Nav_RefreshInlineModelRuntime( void );

/** 
* 	@brief	Performs a simple PMove-like step traversal test (3-trace).
* 
* 			This is intentionally conservative and is used only to validate edges in A* :
* 			1) Try direct horizontal move.
* 			2) If blocked, try stepping up <= max step, then horizontal move.
* 			3) Trace down to find ground.
* 
* 	@return	True if the traversal is possible, false otherwise.
**/
const bool Nav_CanTraverseStep( const nav_mesh_t * mesh, const Vector3 &startPos, const Vector3 &endPos, const edict_ptr_t * clip_entity, const cm_contents_t stepTraceMask );
const bool Nav_CanTraverseStep_ExplicitBBox( const nav_mesh_t * mesh, const Vector3 &startPos, const Vector3 &endPos,
	const Vector3 &mins, const Vector3 &maxs, const edict_ptr_t * clip_entity, const svg_nav_path_policy_t * policy, nav_edge_reject_reason_t * out_reason, const cm_contents_t stepTraceMask  );

// Convenience overload: call explicit-bbox traversal test without a policy (defaults to mesh parameters).
static inline const bool Nav_CanTraverseStep_ExplicitBBox( const nav_mesh_t * mesh, const Vector3 &startPos, const Vector3 &endPos,
	const Vector3 &mins, const Vector3 &maxs, const edict_ptr_t * clip_entity ) {
	return Nav_CanTraverseStep_ExplicitBBox( mesh, startPos, endPos, mins, maxs, clip_entity, nullptr, nullptr );
}
// (Declaration present earlier) - definition follows below without a redundant default argument.



/** 
* 
*    CVars for Navigation Debug Drawing:
* 
**/
//! Master on/off. 0 = off, 1 = on.
extern cvar_t *nav_debug_draw;
//! Only draw a specific BSP leaf index. -1 = any.
extern cvar_t *nav_debug_draw_leaf;
//! Only draw a specific tile (grid coords). Requires "x y". "* " disables.
extern cvar_t *nav_debug_draw_tile;
//! Budget in TE_DEBUG_TRAIL line segments per server frame.
extern cvar_t *nav_debug_draw_max_segments;
//! Max distance from current view player to draw debug (world units). 0 disables.
extern cvar_t *nav_debug_draw_max_dist;
//! Draw tile bboxes.
extern cvar_t *nav_debug_draw_tile_bounds;
//! Draw sample ticks.
extern cvar_t *nav_debug_draw_samples;
//! Draw final path segments from traversal path query.
extern cvar_t *nav_debug_draw_path;
//! Temporary cvar to enable printing of failed nav lookup diagnostics (tile/cell indices).
extern cvar_t *nav_debug_show_failed_lookups;



/** 
* 
* 
* 
*    Navigation Occupancy:
* 
* 
* 
**/
/** 
*  @brief    Build a packed occupancy key from tile/cell/layer indices.
**/
static inline uint64_t Nav_OccupancyKey( const int32_t tileId, const int32_t cellIndex, const int32_t layerIndex ) {
	return ( ( ( uint64_t )tileId & 0xFFFF ) << 32 ) | ( ( ( uint64_t )cellIndex & 0xFFFF ) << 16 ) | ( ( uint64_t )layerIndex & 0xFFFF );
}

/** 
*  @brief    Ensure occupancy map is stamped for the current frame.
**/
static inline void Nav_Occupancy_BeginFrame( nav_mesh_t * mesh ) {
	if ( !mesh ) {
		return;
	}

	const int32_t frame = ( int32_t )level.frameNumber;
	if ( mesh->occupancy_frame != frame ) {
		mesh->occupancy.clear();
		mesh->occupancy_frame = frame;
	}
}

/** 
*  @brief    Clear all dynamic occupancy records.
**/
void SVG_Nav_Occupancy_Clear( nav_mesh_t * mesh ) {
	/** 
	*  Sanity: require mesh.
	**/
	if ( !mesh ) {
		return;
	}

	/** 
	*  Reset occupancy counts.
	**/
	mesh->occupancy.clear();
	mesh->occupancy_frame = ( int32_t )level.frameNumber;
}

/** 
*  @brief    Add occupancy for a tile/cell/layer.
**/
void SVG_Nav_Occupancy_Add( nav_mesh_t * mesh, int32_t tileId, int32_t cellIndex, int32_t layerIndex, int32_t cost, bool blocked ) {
	/** 
	*  Sanity: require mesh and non-negative indices.
	**/
	if ( !mesh || tileId < 0 || cellIndex < 0 || layerIndex < 0 ) {
		return;
	}

	/** 
	*  Refresh occupancy for the current frame to avoid stale data.
	**/
	Nav_Occupancy_BeginFrame( mesh );

	/** 
	*  Accumulate soft-cost and hard-block flags.
	**/
	const uint64_t key = Nav_OccupancyKey( tileId, cellIndex, layerIndex );
	nav_occupancy_entry_t &entry = mesh->occupancy[ key ];
	entry.soft_cost += std::max( 1, cost );
	if ( blocked ) {
		entry.blocked = true;
	}
}

/** 
* @brief    Query occupancy soft cost for a tile/cell/layer.
**/
int32_t SVG_Nav_Occupancy_SoftCost( const nav_mesh_t * mesh, int32_t tileId, int32_t cellIndex, int32_t layerIndex ) {
	/** 
	*  Sanity: require mesh and valid indices.
	**/
	if ( !mesh || tileId < 0 || cellIndex < 0 || layerIndex < 0 ) {
		return 0;
	}

	/** 
	*  Lookup the authoritative sparse overlay entry and expose only its soft-cost component.
	**/
 nav_occupancy_entry_t occupancy = {};
	if ( !SVG_Nav_Occupancy_TryGet( mesh, tileId, cellIndex, layerIndex, &occupancy ) ) {
		return 0;
	}

    return occupancy.soft_cost;
}

/** 
*  @brief    Query occupancy blocked flag for a tile/cell/layer.
**/
bool SVG_Nav_Occupancy_Blocked( const nav_mesh_t * mesh, int32_t tileId, int32_t cellIndex, int32_t layerIndex ) {
/** 
*  Sanity: require mesh and valid indices.
**/
if ( !mesh || tileId < 0 || cellIndex < 0 || layerIndex < 0 ) {
	return false;
}

 /** 
	*  Lookup the authoritative sparse overlay entry and expose only its hard-block component.
	**/
	nav_occupancy_entry_t occupancy = {};
	if ( !SVG_Nav_Occupancy_TryGet( mesh, tileId, cellIndex, layerIndex, &occupancy ) ) {
		return false;
	}

  return occupancy.blocked;
}

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
bool SVG_Nav_Occupancy_TryGet( const nav_mesh_t * mesh, int32_t tileId, int32_t cellIndex, int32_t layerIndex, nav_occupancy_entry_t * out_entry ) {
	/** 
	*  Sanity checks: require mesh, output storage, and non-negative canonical indices.
	**/
	if ( !mesh || !out_entry || tileId < 0 || cellIndex < 0 || layerIndex < 0 ) {
		return false;
	}

	/** 
	*  Look up the local sparse overlay entry without mutating any static nav or hierarchy state.
	**/
	const uint64_t key = Nav_OccupancyKey( tileId, cellIndex, layerIndex );
	auto it = mesh->occupancy.find( key );
	if ( it == mesh->occupancy.end() ) {
		return false;
	}

	/** 
	*  Copy the overlay snapshot for the caller so downstream systems can apply consistent semantics.
	**/
	*out_entry = it->second;
	return true;
}

/** 
*  @brief    Clear all local portal overlay entries while leaving the static hierarchy untouched.
*  @param    mesh    Navigation mesh containing the hierarchy portal overlay.
*  @note     This is local runtime state only and must not trigger hierarchy regeneration.
**/
void SVG_Nav_Hierarchy_ClearPortalOverlays( nav_mesh_t * mesh ) {
	/** 
	*  Sanity checks: require a mesh before touching the local portal overlay.
	**/
	if ( !mesh ) {
		return;
	}

	/** 
	*  Reset the runtime overlay entries in place so the static portal graph remains unchanged.
	**/
	mesh->hierarchy.portal_overlays.assign( mesh->hierarchy.portals.size(), nav_portal_overlay_t{} );
	mesh->hierarchy.portal_overlay_serial = 0;
}

/** 
*  @brief    Update one local portal overlay entry without mutating the static portal graph.
*  @param    mesh                        Navigation mesh containing the hierarchy portal overlay.
*  @param    portal_id                   Stable compact portal id to update.
*  @param    overlay_flags               Overlay flags such as invalidated, blocked, or dirty.
*  @param    additional_traversal_cost   Optional local penalty added when the portal remains usable.
*  @return   True when the overlay entry was updated.
*  @note     This is intended for future inline-model-driven local invalidation hooks.
**/
bool SVG_Nav_Hierarchy_SetPortalOverlay( nav_mesh_t * mesh, int32_t portal_id, uint32_t overlay_flags, double additional_traversal_cost ) {
	/** 
	*  Sanity checks: require a mesh and a valid portal id before mutating the local overlay.
	**/
	if ( !mesh || portal_id < 0 || portal_id >= ( int32_t )mesh->hierarchy.portals.size() ) {
		return false;
	}

	/** 
	*  Keep the runtime overlay array aligned 1:1 with the static portal list.
	**/
	if ( mesh->hierarchy.portal_overlays.size() != mesh->hierarchy.portals.size() ) {
		mesh->hierarchy.portal_overlays.assign( mesh->hierarchy.portals.size(), nav_portal_overlay_t{} );
	}

	/** 
	*  Store only the runtime invalidation-related flag subset so static compatibility bits remain untouched.
	**/
	nav_portal_overlay_t &overlay = mesh->hierarchy.portal_overlays[ portal_id ];
	overlay.flags = overlay_flags & ( NAV_PORTAL_FLAG_INVALIDATED | NAV_PORTAL_FLAG_BLOCKED | NAV_PORTAL_FLAG_DIRTY );
	overlay.additional_traversal_cost = std::max( 0.0, additional_traversal_cost );
	overlay.invalidation_serial = ++mesh->hierarchy.portal_overlay_serial;
	return true;
}

/** 
*  @brief    Query one local portal overlay entry by compact portal id.
*  @param    mesh            Navigation mesh containing the hierarchy portal overlay.
*  @param    portal_id       Stable compact portal id to query.
*  @param    out_overlay     [out] Current local portal overlay entry.
*  @return   True when the portal id is valid and an overlay snapshot was returned.
*  @note     The returned entry may be default-initialized when no local invalidation is active yet.
**/
bool SVG_Nav_Hierarchy_TryGetPortalOverlay( const nav_mesh_t * mesh, int32_t portal_id, nav_portal_overlay_t * out_overlay ) {
	/** 
	*  Sanity checks: require mesh, output storage, and a valid portal id.
	**/
	if ( !mesh || !out_overlay || portal_id < 0 || portal_id >= ( int32_t )mesh->hierarchy.portals.size() ) {
		return false;
	}

	/** 
	*  Return a default overlay snapshot when the runtime overlay array has not been initialized yet.
	**/
	if ( portal_id >= ( int32_t )mesh->hierarchy.portal_overlays.size() ) {
		* out_overlay = {};
		return true;
	}

	/** 
	*  Copy the local runtime overlay entry for the caller.
	**/
	* out_overlay = mesh->hierarchy.portal_overlays[ portal_id ];
	return true;
}



/** 
* 
* 
* 
* 
* 	Utility Functions:
* 
* 
* 
* 
**/
/** 
*    @brief  Set a presence bit for a cell index within a tile.
**/
/** 
* @brief    Mark a cell as present in a tile's sparse presence bitset.
* @param    tile        Tile to modify.
* @param    cell_index  Index of the cell within the tile.
* @note     Uses defensive checks to avoid UB when called on partially-initialized tiles.
 **/
static inline void Nav_SetPresenceBit( nav_tile_t * tile, int32_t cell_index ) {
	/** 
	*  Sanity: require tile pointer and a valid cell index.
	**/
	if ( !tile || cell_index < 0 ) {
		return;
	}

	// Presence bitset must exist; validate via the public accessor pattern.
	if ( !tile->presence_bits ) {
		return;
	}

	const int32_t word_index = cell_index >> 5;
	const int32_t bit_index = cell_index & 31;
	tile->presence_bits[ word_index ] |= ( 1u << bit_index );
}


/** 
* 
* 
* 
*    Navigation System Initialization/Shutdown:
* 
* 
* 
**/
/** 
*    @brief  Initialize navigation system and register cvars.
*         Called after entities have post-spawned to ensure inline models
*         have their proper modelindex set.
**/
void SVG_Nav_Init( void ) {
	// Profiling / logging control CVars
	nav_profile_level = gi.cvar( "nav_profile_level", "1", 0 ); // 0=off,1=phase,2=per-leaf,3=per-tile

	/** 
	*    Register grid and quantization CVars with sensible defaults:
	**/
	nav_cell_size_xy = gi.cvar( "nav_cell_size_xy", "4", 0 );
	nav_z_quant = gi.cvar( "nav_z_quant", "2", 0 );
	nav_tile_size = gi.cvar( "nav_tile_size", "8", 0 );

  // Tunable Z tolerance for layer selection and fallback. 0 = auto-derived from mesh parameters.
	nav_z_tolerance = gi.cvar( "nav_z_tolerance", "0", 0 );

	/** 
	*    Register physics constraint CVars matching player movement:
	**/
	nav_max_step = gi.cvar( "nav_max_step", std::to_string( NAV_DEFAULT_STEP_MAX_SIZE ).c_str(), 0 );
	nav_max_drop = gi.cvar( "nav_max_drop", std::to_string( NAV_DEFAULT_MAX_DROP_HEIGHT ).c_str(), 0 );
	nav_max_slope_normal_z = gi.cvar( "nav_max_slope_normal_z", std::to_string( PHYS_MAX_SLOPE_NORMAL ).c_str(), 0);

	/** 
	* 	Register physics constraints for specific actions.
	**/
	nav_max_drop_height_cap = gi.cvar( "nav_max_drop_height_cap", std::to_string( NAV_DEFAULT_MAX_DROP_HEIGHT_CAP ).c_str(), 0 );

	/** 
	*    Register agent bounding box CVars:
	**/
	nav_agent_mins_x = gi.cvar( "nav_agent_mins_x", "-16", 0 );
	nav_agent_mins_y = gi.cvar( "nav_agent_mins_y", "-16", 0 );
	nav_agent_mins_z = gi.cvar( "nav_agent_mins_z", "-36", 0 );
	nav_agent_maxs_x = gi.cvar( "nav_agent_maxs_x", "16", 0 );
	nav_agent_maxs_y = gi.cvar( "nav_agent_maxs_y", "16", 0 );
	nav_agent_maxs_z = gi.cvar( "nav_agent_maxs_z", "36", 0 );

	/** 
	*    Debug draw CVars:
	**/
	#if _DEBUG_NAV_MESH
		nav_debug_draw = gi.cvar( "nav_debug_draw", "1", 0 );
		nav_debug_draw_leaf = gi.cvar( "nav_debug_draw_leaf", "-1", 0 );
		nav_debug_draw_tile = gi.cvar( "nav_debug_draw_tile", "* ", 0 );
		nav_debug_draw_max_segments = gi.cvar( "nav_debug_draw_max_segments", "128", 0 );
		nav_debug_draw_max_dist = gi.cvar( "nav_debug_draw_max_dist", "8192", 0 );
		nav_debug_draw_tile_bounds = gi.cvar( "nav_debug_draw_tile_bounds", "0", 0 );
		nav_debug_draw_samples = gi.cvar( "nav_debug_draw_samples", "0", 0 );
		nav_debug_draw_path = gi.cvar( "nav_debug_draw_path", "1", 0 );
		// Temporary: enable diagnostic prints for failed nav lookups (tile/cell indices)
		nav_debug_show_failed_lookups = gi.cvar( "nav_debug_show_failed_lookups", "1", CVAR_CHEAT );
	#else
		nav_debug_draw = gi.cvar( "nav_debug_draw", "0", 0 );
		nav_debug_draw_leaf = gi.cvar( "nav_debug_draw_leaf", "-1", 0 );
		nav_debug_draw_tile = gi.cvar( "nav_debug_draw_tile", "* ", 0 );
		nav_debug_draw_max_segments = gi.cvar( "nav_debug_draw_max_segments", "4096", 0 );
		nav_debug_draw_max_dist = gi.cvar( "nav_debug_draw_max_dist", "1024", 0 );
		nav_debug_draw_tile_bounds = gi.cvar( "nav_debug_draw_tile_bounds", "0", 0 );
		nav_debug_draw_samples = gi.cvar( "nav_debug_draw_samples", "0", 0 );
		nav_debug_draw_path = gi.cvar( "nav_debug_draw_path", "0", 0 );
		// Temporary: enable diagnostic prints for failed nav lookups (tile/cell indices)
		nav_debug_show_failed_lookups = gi.cvar( "nav_debug_show_failed_lookups", "0", 0 );
	#endif

	/** 
	*  Register the reject debugging toggle.
	**/
	nav_debug_draw_rejects = gi.cvar( "nav_debug_draw_rejects", "0", CVAR_CHEAT );

	/** 
	* 	Cluster-route (tile graph) tuning CVars.
	* 	These affect only the coarse pre-pass; fine A*  still validates traversal.
	**/
	nav_cluster_route_weighted = gi.cvar( "nav_cluster_route_weighted", "1", 0 );
	// Per-flag penalties (added on top of base hop cost). Zero disables bias.
	nav_cluster_cost_stair	= gi.cvar( "nav_cluster_cost_stair", "0", 0 );
	nav_cluster_cost_water	= gi.cvar( "nav_cluster_cost_water", "2", 0 );
	nav_cluster_cost_lava	= gi.cvar( "nav_cluster_cost_lava", "250", 0 );
	nav_cluster_cost_slime	= gi.cvar( "nav_cluster_cost_slime", "8", 0 );
	// Per-flag hard exclusions (0 = allowed, 1 = forbidden).
	nav_cluster_forbid_stair	= gi.cvar( "nav_cluster_forbid_stair", "0", 0 );
	nav_cluster_forbid_water	= gi.cvar( "nav_cluster_forbid_water", "0", 0 );
	nav_cluster_forbid_lava		= gi.cvar( "nav_cluster_forbid_lava", "0", 0 );
	nav_cluster_forbid_slime	= gi.cvar( "nav_cluster_forbid_slime", "0", 0 );

	/** 
	*  Cluster routing CVars.
	**/
	nav_cluster_enable				= gi.cvar( "nav_cluster_enable", "1", 0 );
	nav_cluster_debug_draw			= gi.cvar( "nav_cluster_debug_draw", "0", 0 );
	nav_cluster_debug_draw_graph	= gi.cvar( "nav_cluster_debug_draw_graph", "0", 0 );

	/** 
	*  Register the initial hierarchy coarse-route feature flag.
	* 		Keep this behind a cvar during Phase 5 so we can fall back to the legacy cluster route pre-pass instantly.
	**/
	nav_hierarchy_route_enable = gi.cvar( "nav_hierarchy_route_enable", "1", 0 );
	nav_direct_los_attempt_max_samples = gi.cvar( "nav_direct_los_attempt_max_samples", "2048", 0 );
	nav_hierarchy_route_min_distance = gi.cvar( "nav_hierarchy_route_min_distance", "192", 0 );
    nav_simplify_sync_max_los_tests = gi.cvar( "nav_simplify_sync_max_los_tests", "32", 0 );
	nav_simplify_async_max_los_tests = gi.cvar( "nav_simplify_async_max_los_tests", "32", 0 );
	nav_simplify_sync_max_ms = gi.cvar( "nav_simplify_sync_max_ms", "4", 0 );
	nav_refine_corridor_mid_tiles = gi.cvar( "nav_refine_corridor_mid_tiles", "12", 0 );
	nav_refine_corridor_far_tiles = gi.cvar( "nav_refine_corridor_far_tiles", "24", 0 );
	nav_refine_corridor_radius_near = gi.cvar( "nav_refine_corridor_radius_near", "1", 0 );
	nav_refine_corridor_radius_mid = gi.cvar( "nav_refine_corridor_radius_mid", "2", 0 );
	nav_refine_corridor_radius_far = gi.cvar( "nav_refine_corridor_radius_far", "3", 0 );
	nav_simplify_long_span_min_distance = gi.cvar( "nav_simplify_long_span_min_distance", "64", 0 );
	nav_simplify_clearance_margin = gi.cvar( "nav_simplify_clearance_margin", "16", 0 );
	nav_simplify_collinear_angle_degrees = gi.cvar( "nav_simplify_collinear_angle_degrees", "4", 0 );


	/** 
	*    Register runtime cost-tuning CVars for A*  heuristics.
	**/
	// Register runtime tunable per-call A*  search budget so async worker behavior can be tuned at runtime.
	nav_astar_step_budget_ms = gi.cvar( "nav_astar_step_budget_ms", "8", 0 );

	nav_cost_w_dist = gi.cvar( "nav_cost_w_dist", "1.0", 0 );
	nav_cost_jump_base = gi.cvar( "nav_cost_jump_base", "8.0", 0 );
	nav_cost_jump_height_weight = gi.cvar( "nav_cost_jump_height_weight", "2.0", 0 );
	nav_cost_los_weight = gi.cvar( "nav_cost_los_weight", "1.0", 0 );
	nav_cost_dynamic_weight = gi.cvar( "nav_cost_dynamic_weight", "1.5", 0 );
	nav_cost_failure_weight = gi.cvar( "nav_cost_failure_weight", "10.0", 0 );
	nav_cost_failure_tau_ms = gi.cvar( "nav_cost_failure_tau_ms", "5000", 0 );
	nav_cost_turn_weight = gi.cvar( "nav_cost_turn_weight", "0.8", 0 );
	nav_cost_slope_weight = gi.cvar( "nav_cost_slope_weight", "0.5", 0 );
	nav_cost_drop_weight = gi.cvar( "nav_cost_drop_weight", "4.0", 0 );
	nav_cost_goal_z_blend_factor = gi.cvar( "nav_cost_goal_z_blend_factor", "0.5", 0 );
	nav_cost_min_cost_per_unit = gi.cvar( "nav_cost_min_cost_per_unit", "1.0", 0 );

	/** 
	*  Register async nav request queue cvars.
	**/
	SVG_Nav_RequestQueue_RegisterCvars();
}

/** 
*    @brief  Shutdown navigation system and free memory.
*         Called during game shutdown to clean up resources.
**/
void SVG_Nav_Shutdown( void ) {
	/** 
	*  Cleanup: clear debug paths and free any loaded/generate mesh.
	**/
	NavDebug_ClearCachedPaths();
	SVG_Nav_FreeMesh();
}

/** 
* 	@brief	Will return the path for a level's matching navigation filename.
* 	@return	A string containing the file path + file extension.
**/
const std::string Nav_GetPathForLevelNav( const char * levelName, const bool prependGameFolder ) {
	// Default path for the engine to find nav files at.
	constexpr const char * NAV_PATH_DIR = "/maps/nav/";

	// Actual filename of the .nav file.
	const std::string fileNameExt = std::string( levelName ) + ".nav";
	// Determine the game path to use for loading .nav files.
	// <Q2RTXP>: WID: Unneccesary right now.
	const std::string baseGame = std::string( BASEGAME );
	const std::string defaultGame = std::string( DEFAULT_GAME );

	// If the DEFAULT_GAME is empty use BASE_GAME, otherwise use DEFAULT_GAME. This allows mods to specify a custom game directory for nav files while falling back to the base game if not set.
	const std::string fullPath = ( prependGameFolder ? ( defaultGame.empty() ? baseGame : defaultGame ) : "" ) + NAV_PATH_DIR + fileNameExt;;

	// Should never happen.
	return fullPath;
}

/** 
* 	@brief	Loads up an existing navigation mesh for the current map, if the file is located.
* 	@return True if a mesh was successfully loaded, false otherwise (e.g., file not found).
**/
const std::tuple<const bool, const std::string> SVG_Nav_LoadMesh( const char * levelName ) {
	// Actual path of the .nav file.
	const std::string navMeshFilePath = Nav_GetPathForLevelNav( levelName, false );
// Actual filename of the .nav file.
	const std::string navMeshGameDirFilePath = Nav_GetPathForLevelNav( levelName, true );
	// Load it up if it exists, otherwise it'll just be ignored and the nav system will be inactive.
	if ( gi.FS_FileExistsEx( navMeshFilePath.c_str(), 0 ) ) {
		// Return whether loading succeeded or failed; if it fails, the nav system will be inactive but won't crash.
		const bool loaded = SVG_Nav_LoadVoxelMesh( levelName );
		// Return success status and the actual filename attempted (for logging/debugging).
		return std::make_tuple( loaded, navMeshGameDirFilePath );
	}
	// Failure.
	return std::make_tuple( false, navMeshGameDirFilePath );
}



/** 
* 
* 
* 
*    Navigation System "Voxel Mesh(-es)":
* 
* 
* 
**/
/** 
*    @brief  Free tile cell allocations (layers + arrays).
*    @param	ile		Tile whose `cells`/`layers` were allocated with `gi.TagMallocz`.
*    @param	cells_per_tile	Number of XY cells in the tile (`tile_size* tile_size`).
*    @note	This does not free `tile->presence_bits`.
**/
static void Nav_FreeTileCells( nav_tile_t * tile, int32_t cells_per_tile ) {
	/** 
	*  Sanity: nothing to free if tile or its cell storage is missing.
	**/
	if ( !tile || !tile->cells ) {
		return;
	}

	/** 
	*  Free any per-cell layer buffers first. Use the provided
	*  `cells_per_tile` count because the tile was allocated with this size.
	**/
	for ( int32_t c = 0; c < cells_per_tile; c++ ) {
		if ( tile->cells[ c ].layers ) {
			gi.TagFree( tile->cells[ c ].layers );
			tile->cells[ c ].layers = nullptr;
			tile->cells[ c ].num_layers = 0;
		}
	}

	/** 
	*  Free the cell array itself and clear the tile pointer.
	**/
	gi.TagFree( tile->cells );
	tile->cells = nullptr;
}

/** 
* 	@brief	Reset hierarchy membership on existing tiles and leaves.
* 	@param	mesh	Navigation mesh whose per-tile and per-leaf region membership should be cleared.
* 	@note	This keeps the fine navmesh intact while removing any stale hierarchy ownership.
**/
void SVG_Nav_Hierarchy_ResetMembership( nav_mesh_t * mesh ) {
	/** 
	* 	Sanity: require a mesh before touching hierarchy membership state.
	**/
	if ( !mesh ) {
		return;
	}

	/** 
	* 	Reset canonical world-tile region membership so future hierarchy builders start clean.
	**/
	for ( nav_tile_t &tile : mesh->world_tiles ) {
		tile.region_id = NAV_REGION_ID_NONE;
	}

	/** 
	* 	Reset leaf region membership arrays because Phase 3 will repopulate them deterministically.
	**/
	if ( mesh->leaf_data ) {
		for ( int32_t leaf_index = 0; leaf_index < mesh->num_leafs; leaf_index++ ) {
			nav_leaf_data_t &leaf = mesh->leaf_data[ leaf_index ];
			if ( leaf.region_ids ) {
				gi.TagFree( leaf.region_ids );
				leaf.region_ids = nullptr;
			}
			leaf.num_regions = 0;
		}
	}

	/** 
	* 	Reset inline-model tile membership too so future overlay-aware hierarchy code starts from sentinels.
	**/
	if ( mesh->inline_model_data ) {
		for ( int32_t model_index = 0; model_index < mesh->num_inline_models; model_index++ ) {
			nav_inline_model_data_t &model = mesh->inline_model_data[ model_index ];
			for ( int32_t tile_index = 0; tile_index < model.num_tiles; tile_index++ ) {
				model.tiles[ tile_index ].region_id = NAV_REGION_ID_NONE;
			}
		}
	}
}

/** 
* 	@brief	Clear all internal hierarchy scaffolding owned by a navigation mesh.
* 	@param	mesh	Navigation mesh whose hierarchy containers should be cleared.
* 	@note	This preserves the fine navmesh while invalidating region/portal state and memberships.
**/
void SVG_Nav_Hierarchy_Clear( nav_mesh_t * mesh ) {
	/** 
	* 	Sanity: require a mesh before clearing hierarchy state.
	**/
	if ( !mesh ) {
		return;
	}

	/** 
	* 	Reset per-tile and per-leaf membership first so no stale ids remain visible.
	**/
	SVG_Nav_Hierarchy_ResetMembership( mesh );

	/** 
	* 	Clear the flat hierarchy containers and mark the hierarchy dirty for future rebuilds.
	**/
	mesh->hierarchy.regions.clear();
    mesh->hierarchy.tile_adjacency.clear();
	mesh->hierarchy.tile_neighbor_refs.clear();
	mesh->hierarchy.portals.clear();
  mesh->hierarchy.portal_overlays.clear();
	mesh->hierarchy.tile_refs.clear();
	mesh->hierarchy.leaf_refs.clear();
	mesh->hierarchy.region_portal_refs.clear();
   mesh->hierarchy.debug_region_count = 0;
	mesh->hierarchy.debug_adjacency_link_count = 0;
    mesh->hierarchy.debug_boundary_link_count = 0;
	mesh->hierarchy.debug_portal_count = 0;
	mesh->hierarchy.debug_isolated_region_count = 0;
	mesh->hierarchy.debug_oversized_region_count = 0;
	mesh->hierarchy.build_serial = 0;
   mesh->hierarchy.portal_overlay_serial = 0;
	mesh->hierarchy.dirty = true;
}

/** 
* 	@brief	Map a persisted edge slot onto its XY direction delta.
* 	@param	edge_dir_index	Persisted edge slot index (0..7).
* 	@param	out_dx		[out] Neighbor X delta.
* 	@param	out_dy		[out] Neighbor Y delta.
* 	@return	True when the slot index is valid.
**/
static bool Nav_Hierarchy_EdgeDirDelta( const int32_t edge_dir_index, int32_t * out_dx, int32_t * out_dy ) {
	/** 
	* 	Sanity: require valid output pointers before decoding the edge slot.
	**/
	if ( !out_dx || !out_dy ) {
		return false;
	}

	/** 
	* 	Translate the persisted edge slot ordering established during generation.
	**/
	switch ( edge_dir_index ) {
		case 0:
			* out_dx = 1;
			* out_dy = 0;
			return true;
		case 1:
			* out_dx = -1;
			* out_dy = 0;
			return true;
		case 2:
			* out_dx = 0;
			* out_dy = 1;
			return true;
		case 3:
			* out_dx = 0;
			* out_dy = -1;
			return true;
		case 4:
			* out_dx = 1;
			* out_dy = 1;
			return true;
		case 5:
			* out_dx = 1;
			* out_dy = -1;
			return true;
		case 6:
			* out_dx = -1;
			* out_dy = 1;
			return true;
		case 7:
			* out_dx = -1;
			* out_dy = -1;
			return true;
		default:
			return false;
	}
}

/** 
* 	@brief	Resolve a canonical world-tile id by tile-grid coordinates.
* 	@param	mesh	Navigation mesh owning the canonical world-tile lookup.
* 	@param	tile_x	Tile X coordinate.
* 	@param	tile_y	Tile Y coordinate.
* 	@return	Canonical world-tile id, or -1 when absent.
**/
static int32_t Nav_Hierarchy_FindWorldTileId( const nav_mesh_t * mesh, const int32_t tile_x, const int32_t tile_y ) {
	/** 
	* 	Sanity: require a valid mesh before querying the world tile lookup table.
	**/
	if ( !mesh ) {
		return -1;
	}

	/** 
	* 	Consult the canonical `(tile_x,tile_y)` lookup used everywhere else in the nav mesh.
	**/
	const nav_world_tile_key_t key = { .tile_x = tile_x, .tile_y = tile_y };
	auto it = mesh->world_tile_id_of.find( key );
	if ( it == mesh->world_tile_id_of.end() ) {
		return -1;
	}

	return it->second;
}

/** 
* 	@brief	Build hierarchy compatibility bits from a tile's persisted summary metadata.
* 	@param	tile	Tile whose coarse summaries should be translated.
* 	@return	Compatibility bits suitable for future region or portal policy filtering.
**/
static uint32_t Nav_Hierarchy_BuildTileCompatibilityBits( const nav_tile_t &tile ) {
	/** 
	* 	Every navigable tile participates in ordinary ground traversal by default.
	**/
	uint32_t compatibility_bits = NAV_HIERARCHY_COMPAT_GROUND;
	const uint32_t summary_bits = tile.traversal_summary_bits | tile.edge_summary_bits;

	/** 
	* 	Promote persisted traversal summaries into coarse compatibility hooks.
	**/
	if ( ( summary_bits & NAV_TILE_SUMMARY_STAIR ) != 0 ) {
		compatibility_bits |= NAV_HIERARCHY_COMPAT_STAIR;
	}
	if ( ( summary_bits & NAV_TILE_SUMMARY_LADDER ) != 0 ) {
		compatibility_bits |= NAV_HIERARCHY_COMPAT_LADDER;
	}
	if ( ( summary_bits & NAV_TILE_SUMMARY_WATER ) != 0 ) {
		compatibility_bits |= NAV_HIERARCHY_COMPAT_WATER;
	}
	if ( ( summary_bits & NAV_TILE_SUMMARY_LAVA ) != 0 ) {
		compatibility_bits |= NAV_HIERARCHY_COMPAT_LAVA;
	}
	if ( ( summary_bits & NAV_TILE_SUMMARY_SLIME ) != 0 ) {
		compatibility_bits |= NAV_HIERARCHY_COMPAT_SLIME;
	}
	if ( ( summary_bits & NAV_TILE_SUMMARY_WALK_OFF ) != 0 ) {
		compatibility_bits |= NAV_HIERARCHY_COMPAT_WALK_OFF;
	}

	return compatibility_bits;
}

/** 
* 	@brief	Build initial region summary flags from a tile's persisted coarse metadata.
* 	@param	tile	Tile whose coarse summaries should be translated.
* 	@return	Region flags suitable for aggregated Phase 3 region summaries.
**/
static uint32_t Nav_Hierarchy_BuildTileRegionFlags( const nav_tile_t &tile ) {
	uint32_t region_flags = NAV_REGION_FLAG_NONE;
	const uint32_t summary_bits = tile.traversal_summary_bits | tile.edge_summary_bits;

	/** 
	* 	Promote tile summaries into region-level feature flags.
	**/
	if ( ( summary_bits & NAV_TILE_SUMMARY_STAIR ) != 0 ) {
		region_flags |= NAV_REGION_FLAG_HAS_STAIRS;
	}
	if ( ( summary_bits & NAV_TILE_SUMMARY_LADDER ) != 0 ) {
		region_flags |= NAV_REGION_FLAG_HAS_LADDERS;
	}
	if ( ( summary_bits & NAV_TILE_SUMMARY_WATER ) != 0 ) {
		region_flags |= NAV_REGION_FLAG_HAS_WATER;
	}
	if ( ( summary_bits & NAV_TILE_SUMMARY_LAVA ) != 0 ) {
		region_flags |= NAV_REGION_FLAG_HAS_LAVA;
	}
	if ( ( summary_bits & NAV_TILE_SUMMARY_SLIME ) != 0 ) {
		region_flags |= NAV_REGION_FLAG_HAS_SLIME;
	}

	return region_flags;
}

/** 
* 	@brief	Determine whether a persisted edge is suitable for static region connectivity.
* 	@param	edge_bits	Per-edge feature bits finalized during generation.
* 	@return	True when the edge represents conservative bidirectional static connectivity.
* 	@note	Walk-off-only, hard-wall, and jump-obstructed edges stay out of the static region graph.
**/
static bool Nav_Hierarchy_EdgeAllowsStaticRegionPass( const uint32_t edge_bits ) {
	/** 
	* 	Require ordinary passability first because missing or forbidden edges must never join regions.
	**/
	if ( ( edge_bits & NAV_EDGE_FEATURE_PASSABLE ) == 0 ) {
		return false;
	}

	/** 
	* 	Keep the first pass conservative by excluding optional or forbidden walk-offs.
	**/
	if ( ( edge_bits & ( NAV_EDGE_FEATURE_OPTIONAL_WALK_OFF | NAV_EDGE_FEATURE_FORBIDDEN_WALK_OFF ) ) != 0 ) {
		return false;
	}

	/** 
	* 	Exclude edges explicitly marked as blocked or requiring unsupported jump-style behavior.
	**/
	if ( ( edge_bits & ( NAV_EDGE_FEATURE_HARD_WALL_BLOCKED | NAV_EDGE_FEATURE_JUMP_OBSTRUCTION ) ) != 0 ) {
		return false;
	}

	return true;
}

/** 
* 	@brief	Sort canonical tile ids by tile-grid coordinates for deterministic hierarchy output.
* 	@param	mesh	Navigation mesh owning the canonical world tiles.
* 	@param	tile_ids	Tile-id array to sort in place.
* 	@note	Tile id is used as the final tie-breaker to keep ordering stable.
**/
static void Nav_Hierarchy_SortTileIdsDeterministic( const nav_mesh_t * mesh, std::vector<int32_t> &tile_ids ) {
	/** 
	* 	Sanity: require a mesh before looking up tile coordinates for sorting.
	**/
	if ( !mesh ) {
		return;
	}

	/** 
	* 	Sort by `(tile_y, tile_x, tile_id)` so region seeding and neighbor lists remain deterministic.
	**/
	std::sort( tile_ids.begin(), tile_ids.end(), [mesh]( const int32_t lhs, const int32_t rhs ) {
		const nav_tile_t &lhs_tile = mesh->world_tiles[ lhs ];
		const nav_tile_t &rhs_tile = mesh->world_tiles[ rhs ];
		if ( lhs_tile.tile_y != rhs_tile.tile_y ) {
			return lhs_tile.tile_y < rhs_tile.tile_y;
		}
		if ( lhs_tile.tile_x != rhs_tile.tile_x ) {
			return lhs_tile.tile_x < rhs_tile.tile_x;
		}
		return lhs < rhs;
	} );
}

/** 
* 	@brief	Build a compact ordered key for an unordered region pair.
* 	@param	region_a	First region id.
* 	@param	region_b	Second region id.
* 	@return	Packed 64-bit region-pair key with the lower id first.
**/
static uint64_t Nav_Hierarchy_MakeRegionPairKey( const int32_t region_a, const int32_t region_b ) {
	/** 
	* 	Normalize the pair ordering first so the same boundary maps to one stable key.
	**/
	const uint32_t lo = ( uint32_t )std::min( region_a, region_b );
	const uint32_t hi = ( uint32_t )std::max( region_a, region_b );
	return ( ( uint64_t )lo << 32 ) | ( uint64_t )hi;
}

/** 
* 	@brief	Temporary merged portal accumulator used while scanning fine cross-region boundaries.
**/
struct nav_portal_accumulator_t {
	//! Ordered region id on one side of the merged portal.
	int32_t region_a = NAV_REGION_ID_NONE;
	//! Ordered region id on the opposite side of the merged portal.
	int32_t region_b = NAV_REGION_ID_NONE;
	//! Deterministic anchor tile id for `region_a`.
	int32_t tile_id_a = -1;
	//! Deterministic anchor tile id for `region_b`.
	int32_t tile_id_b = -1;
	//! Aggregated portal flags derived from contributing boundary samples.
	uint32_t flags = NAV_PORTAL_FLAG_NONE;
	//! Aggregated compatibility bits derived from contributing boundary samples.
	uint32_t compatibility_bits = NAV_HIERARCHY_COMPAT_NONE;
	//! Running sum of representative sample X positions.
	double representative_sum_x = 0.0;
	//! Running sum of representative sample Y positions.
	double representative_sum_y = 0.0;
	//! Running sum of representative sample Z positions.
	double representative_sum_z = 0.0;
	//! Running sum of coarse traversal cost estimates.
	double traversal_cost_sum = 0.0;
	//! Number of contributing boundary samples merged into this accumulator.
	int32_t sample_count = 0;
};

/** 
* 	@brief	Derive canonical world-tile adjacency from persisted fine edge metadata.
* 	@param	mesh	Navigation mesh whose canonical world tiles should be scanned.
* 	@note	Only cross-tile, conservative static edges are promoted into the adjacency graph.
**/
static void Nav_Hierarchy_BuildTileAdjacency( nav_mesh_t * mesh ) {
	/** 
	* 	Sanity: require a mesh before building any adjacency storage.
	**/
	if ( !mesh ) {
		return;
	}

	/** 
	* 	Size the adjacency records to the canonical world-tile count and clear previous neighbor refs.
	**/
	mesh->hierarchy.tile_adjacency.clear();
	mesh->hierarchy.tile_neighbor_refs.clear();
	mesh->hierarchy.tile_adjacency.resize( mesh->world_tiles.size() );
	mesh->hierarchy.debug_adjacency_link_count = 0;

	/** 
	* 	Accumulate neighbor ids per tile first so we can sort and compact them deterministically.
	**/
	std::vector<std::vector<int32_t>> temp_neighbors( mesh->world_tiles.size() );
	const int32_t cells_per_tile = mesh->tile_size* mesh->tile_size;
	for ( int32_t tile_id = 0; tile_id < ( int32_t )mesh->world_tiles.size(); tile_id++ ) {
		nav_tile_t &tile = mesh->world_tiles[ tile_id ];
		nav_tile_adjacency_t &adjacency = mesh->hierarchy.tile_adjacency[ tile_id ];
		adjacency.tile_id = tile_id;
		adjacency.compatibility_bits = Nav_Hierarchy_BuildTileCompatibilityBits( tile );

		/** 
		* 	Inspect only populated sparse cells because empty slots cannot contribute connectivity.
		**/
		auto cells_view = SVG_Nav_Tile_GetCells( mesh, &tile );
		nav_xy_cell_t * cells_ptr = cells_view.first;
		const int32_t cells_count = cells_view.second;
		if ( !cells_ptr || cells_count != cells_per_tile || !tile.presence_bits ) {
			continue;
		}

		// Iterate every populated XY cell in this canonical world tile.
		for ( int32_t cell_index = 0; cell_index < cells_count; cell_index++ ) {
			const int32_t word_index = cell_index >> 5;
			const int32_t bit_index = cell_index & 31;
			if ( ( tile.presence_bits[ word_index ] & ( 1u << bit_index ) ) == 0 ) {
				continue;
			}

			nav_xy_cell_t &cell = cells_ptr[ cell_index ];
			auto layers_view = SVG_Nav_Cell_GetLayers( &cell );
			nav_layer_t * layers_ptr = layers_view.first;
			const int32_t layer_count = layers_view.second;
			if ( !layers_ptr || layer_count <= 0 ) {
				continue;
			}

			const int32_t local_cell_x = cell_index % mesh->tile_size;
			const int32_t local_cell_y = cell_index / mesh->tile_size;
			// Inspect every layer and promote cross-tile passable edges into canonical tile adjacency.
			for ( int32_t layer_index = 0; layer_index < layer_count; layer_index++ ) {
				const nav_layer_t &layer = layers_ptr[ layer_index ];
				for ( int32_t edge_dir_index = 0; edge_dir_index < NAV_LAYER_EDGE_DIR_COUNT; edge_dir_index++ ) {
					if ( ( layer.edge_valid_mask & ( 1u << edge_dir_index ) ) == 0 ) {
						continue;
					}

					const uint32_t edge_bits = layer.edge_feature_bits[ edge_dir_index ];
					if ( !Nav_Hierarchy_EdgeAllowsStaticRegionPass( edge_bits ) ) {
						continue;
					}

					int32_t cell_dx = 0;
					int32_t cell_dy = 0;
					if ( !Nav_Hierarchy_EdgeDirDelta( edge_dir_index, &cell_dx, &cell_dy ) ) {
						continue;
					}

					const int32_t target_local_x = local_cell_x + cell_dx;
					const int32_t target_local_y = local_cell_y + cell_dy;
					const int32_t target_tile_x = tile.tile_x + ( target_local_x < 0 ? -1 : ( target_local_x >= mesh->tile_size ? 1 : 0 ) );
					const int32_t target_tile_y = tile.tile_y + ( target_local_y < 0 ? -1 : ( target_local_y >= mesh->tile_size ? 1 : 0 ) );

					// Keep the region graph coarse at tile granularity by promoting only true cross-tile edges.
					if ( target_tile_x == tile.tile_x && target_tile_y == tile.tile_y ) {
						continue;
					}

					const int32_t neighbor_tile_id = Nav_Hierarchy_FindWorldTileId( mesh, target_tile_x, target_tile_y );
					if ( neighbor_tile_id < 0 || neighbor_tile_id == tile_id ) {
						continue;
					}

					temp_neighbors[ tile_id ].push_back( neighbor_tile_id );
				}
			}
		}
	}

	/** 
	* 	Sort and compact neighbor ids before flattening them into the persistent adjacency store.
	**/
	for ( int32_t tile_id = 0; tile_id < ( int32_t )temp_neighbors.size(); tile_id++ ) {
		std::vector<int32_t> &neighbors = temp_neighbors[ tile_id ];
		Nav_Hierarchy_SortTileIdsDeterministic( mesh, neighbors );
		neighbors.erase( std::unique( neighbors.begin(), neighbors.end() ), neighbors.end() );

		nav_tile_adjacency_t &adjacency = mesh->hierarchy.tile_adjacency[ tile_id ];
		adjacency.first_neighbor_ref = ( int32_t )mesh->hierarchy.tile_neighbor_refs.size();
		adjacency.num_neighbor_refs = ( int32_t )neighbors.size();
		mesh->hierarchy.tile_neighbor_refs.insert( mesh->hierarchy.tile_neighbor_refs.end(), neighbors.begin(), neighbors.end() );
	}

	mesh->hierarchy.debug_adjacency_link_count = ( int32_t )mesh->hierarchy.tile_neighbor_refs.size();
}

/** 
* 	@brief	Validate that every region remains internally connected over the stored tile adjacency graph.
* 	@param	mesh	Navigation mesh containing the freshly built hierarchy.
* 	@return	True when every region validates successfully.
**/
static bool Nav_Hierarchy_ValidateRegions( const nav_mesh_t * mesh ) {
	/** 
	* 	Sanity: require a mesh before validating any region connectivity.
	**/
	if ( !mesh ) {
		return false;
	}

	/** 
	* 	Build a reusable region lookup table over canonical tile ids for cheap membership checks.
	**/
	std::vector<int32_t> tile_region_lookup( mesh->world_tiles.size(), NAV_REGION_ID_NONE );
	for ( int32_t tile_id = 0; tile_id < ( int32_t )mesh->world_tiles.size(); tile_id++ ) {
		tile_region_lookup[ tile_id ] = mesh->world_tiles[ tile_id ].region_id;
	}

	/** 
	* 	Walk each region over stored tile adjacency and confirm the visited count matches the region tile count.
	**/
	for ( const nav_region_t &region : mesh->hierarchy.regions ) {
		if ( region.num_tile_refs <= 1 ) {
			continue;
		}

		std::vector<int32_t> stack;
		stack.reserve( (size_t)region.num_tile_refs );
		std::vector<uint8_t> visited( mesh->world_tiles.size(), 0 );
		const int32_t start_tile_id = mesh->hierarchy.tile_refs[ region.first_tile_ref ].tile_id;
		stack.push_back( start_tile_id );
		visited[ start_tile_id ] = 1;

		int32_t visited_count = 0;
		while ( !stack.empty() ) {
			const int32_t tile_id = stack.back();
			stack.pop_back();
			visited_count++;

			const nav_tile_adjacency_t &adjacency = mesh->hierarchy.tile_adjacency[ tile_id ];
			for ( int32_t neighbor_index = 0; neighbor_index < adjacency.num_neighbor_refs; neighbor_index++ ) {
				const int32_t neighbor_tile_id = mesh->hierarchy.tile_neighbor_refs[ adjacency.first_neighbor_ref + neighbor_index ];
				if ( neighbor_tile_id < 0 || neighbor_tile_id >= ( int32_t )tile_region_lookup.size() ) {
					continue;
				}
				if ( tile_region_lookup[ neighbor_tile_id ] != region.id || visited[ neighbor_tile_id ] != 0 ) {
					continue;
				}

				visited[ neighbor_tile_id ] = 1;
				stack.push_back( neighbor_tile_id );
			}
		}

		// Reject any region whose stored tile set cannot be re-traversed through the formalized adjacency graph.
		if ( visited_count != region.num_tile_refs ) {
			SVG_Nav_Log( "[WARNING][NavHierarchy] Region %d connectivity mismatch: expected=%d visited=%d\n",
				region.id,
				region.num_tile_refs,
				visited_count );
			return false;
		}
	}

	return true;
}

/** 
* 	@brief	Build merged portal records from traversable cross-region tile boundaries.
* 	@param	mesh	Navigation mesh containing finalized tile adjacency and region ids.
* 	@param	sorted_tile_ids	Deterministically ordered canonical world-tile ids.
**/
static void Nav_Hierarchy_BuildPortals( nav_mesh_t * mesh, const std::vector<int32_t> &sorted_tile_ids ) {
	/** 
	* 	Sanity: require a mesh before scanning any cross-region boundaries.
	**/
	if ( !mesh ) {
		return;
	}

	/** 
	* 	Reset previous portal storage so every rebuild starts from a clean region graph.
	**/
	mesh->hierarchy.portals.clear();
 mesh->hierarchy.portal_overlays.clear();
	mesh->hierarchy.region_portal_refs.clear();
	mesh->hierarchy.debug_boundary_link_count = 0;
	mesh->hierarchy.debug_portal_count = 0;

	/** 
	* 	Accumulate one merged portal per unordered region pair while scanning fine edge samples.
	**/
	std::unordered_map<uint64_t, int32_t> accumulator_index_of;
	std::vector<nav_portal_accumulator_t> accumulators;
	accumulator_index_of.reserve( mesh->hierarchy.regions.size()* 2 );
	accumulators.reserve( mesh->hierarchy.regions.size() );
	const int32_t cells_per_tile = mesh->tile_size* mesh->tile_size;
	const double tile_world_size = Nav_TileWorldSize( mesh );
	for ( const int32_t tile_id : sorted_tile_ids ) {
		const nav_tile_t &tile = mesh->world_tiles[ tile_id ];
		const int32_t source_region_id = tile.region_id;
		if ( source_region_id == NAV_REGION_ID_NONE ) {
			continue;
		}

		/** 
		* 	Inspect only populated sparse cells because empty slots cannot contribute portal boundary samples.
		**/
		auto cells_view = SVG_Nav_Tile_GetCells( mesh, &mesh->world_tiles[ tile_id ] );
		nav_xy_cell_t * cells_ptr = cells_view.first;
		const int32_t cells_count = cells_view.second;
		if ( !cells_ptr || cells_count != cells_per_tile || !tile.presence_bits ) {
			continue;
		}

		// Inspect every populated source cell/layer and merge traversable cross-region boundaries by region pair.
		for ( int32_t cell_index = 0; cell_index < cells_count; cell_index++ ) {
			const int32_t word_index = cell_index >> 5;
			const int32_t bit_index = cell_index & 31;
			if ( ( tile.presence_bits[ word_index ] & ( 1u << bit_index ) ) == 0 ) {
				continue;
			}

			nav_xy_cell_t &cell = cells_ptr[ cell_index ];
			auto layers_view = SVG_Nav_Cell_GetLayers( &cell );
			nav_layer_t * layers_ptr = layers_view.first;
			const int32_t layer_count = layers_view.second;
			if ( !layers_ptr || layer_count <= 0 ) {
				continue;
			}

			const int32_t local_cell_x = cell_index % mesh->tile_size;
			const int32_t local_cell_y = cell_index / mesh->tile_size;
			for ( int32_t layer_index = 0; layer_index < layer_count; layer_index++ ) {
				const nav_layer_t &layer = layers_ptr[ layer_index ];
				for ( int32_t edge_dir_index = 0; edge_dir_index < NAV_LAYER_EDGE_DIR_COUNT; edge_dir_index++ ) {
					if ( ( layer.edge_valid_mask & ( 1u << edge_dir_index ) ) == 0 ) {
						continue;
					}

					const uint32_t edge_bits = layer.edge_feature_bits[ edge_dir_index ];
					if ( !Nav_Hierarchy_EdgeAllowsStaticRegionPass( edge_bits ) ) {
						continue;
					}

					int32_t cell_dx = 0;
					int32_t cell_dy = 0;
					if ( !Nav_Hierarchy_EdgeDirDelta( edge_dir_index, &cell_dx, &cell_dy ) ) {
						continue;
					}

					const int32_t target_local_x = local_cell_x + cell_dx;
					const int32_t target_local_y = local_cell_y + cell_dy;
					const int32_t target_tile_x = tile.tile_x + ( target_local_x < 0 ? -1 : ( target_local_x >= mesh->tile_size ? 1 : 0 ) );
					const int32_t target_tile_y = tile.tile_y + ( target_local_y < 0 ? -1 : ( target_local_y >= mesh->tile_size ? 1 : 0 ) );

					// Keep portal extraction coarse at tile granularity by considering only true cross-tile boundaries.
					if ( target_tile_x == tile.tile_x && target_tile_y == tile.tile_y ) {
						continue;
					}

					const int32_t neighbor_tile_id = Nav_Hierarchy_FindWorldTileId( mesh, target_tile_x, target_tile_y );
					if ( neighbor_tile_id < 0 || neighbor_tile_id == tile_id ) {
						continue;
					}

					const nav_tile_t &neighbor_tile = mesh->world_tiles[ neighbor_tile_id ];
					const int32_t target_region_id = neighbor_tile.region_id;
					if ( target_region_id == NAV_REGION_ID_NONE || target_region_id == source_region_id ) {
						continue;
					}

					const int32_t region_a = std::min( source_region_id, target_region_id );
					const int32_t region_b = std::max( source_region_id, target_region_id );
					const uint64_t portal_key = Nav_Hierarchy_MakeRegionPairKey( region_a, region_b );
					auto accumulator_it = accumulator_index_of.find( portal_key );
					int32_t accumulator_index = -1;
					if ( accumulator_it == accumulator_index_of.end() ) {
						accumulator_index = ( int32_t )accumulators.size();
						accumulator_index_of.emplace( portal_key, accumulator_index );
						accumulators.push_back( nav_portal_accumulator_t{ .region_a = region_a, .region_b = region_b } );
					} else {
						accumulator_index = accumulator_it->second;
					}

					nav_portal_accumulator_t &accumulator = accumulators[ accumulator_index ];
					if ( accumulator.tile_id_a < 0 || accumulator.tile_id_b < 0 ) {
						if ( source_region_id == region_a ) {
							accumulator.tile_id_a = tile_id;
							accumulator.tile_id_b = neighbor_tile_id;
						} else {
							accumulator.tile_id_a = neighbor_tile_id;
							accumulator.tile_id_b = tile_id;
						}
					}

					const Vector3 sample_point = Nav_NodeWorldPosition( mesh, &tile, cell_index, &layer );
					accumulator.representative_sum_x += sample_point.x;
					accumulator.representative_sum_y += sample_point.y;
					accumulator.representative_sum_z += sample_point.z;
					accumulator.traversal_cost_sum += std::sqrt( ( double )( cell_dx* cell_dx ) + ( double )( cell_dy* cell_dy ) )* tile_world_size;
					accumulator.sample_count++;
					accumulator.compatibility_bits |= Nav_Hierarchy_BuildTileCompatibilityBits( tile );
					accumulator.compatibility_bits |= Nav_Hierarchy_BuildTileCompatibilityBits( neighbor_tile );
					if ( ( accumulator.compatibility_bits & NAV_HIERARCHY_COMPAT_STAIR ) != 0 ) {
						accumulator.flags |= NAV_PORTAL_FLAG_SUPPORTS_STAIRS;
					}
					if ( ( accumulator.compatibility_bits & NAV_HIERARCHY_COMPAT_LADDER ) != 0 ) {
						accumulator.flags |= NAV_PORTAL_FLAG_SUPPORTS_LADDER;
					}
					if ( ( accumulator.compatibility_bits & NAV_HIERARCHY_COMPAT_WATER ) != 0 ) {
						accumulator.flags |= NAV_PORTAL_FLAG_SUPPORTS_WATER;
					}
					mesh->hierarchy.debug_boundary_link_count++;
				}
			}
		}
	}

	/** 
	* 	Flatten the merged region-pair accumulators into stable portal records and region-to-portal refs.
	**/
	std::vector<std::vector<int32_t>> region_portal_lists( mesh->hierarchy.regions.size() );
	mesh->hierarchy.portals.reserve( accumulators.size() );
	for ( const nav_portal_accumulator_t &accumulator : accumulators ) {
		nav_portal_t portal = {};
		portal.id = ( int32_t )mesh->hierarchy.portals.size();
		portal.region_a = accumulator.region_a;
		portal.region_b = accumulator.region_b;
		portal.flags = accumulator.flags;
		portal.compatibility_bits = accumulator.compatibility_bits;
		portal.tile_id_a = accumulator.tile_id_a;
		portal.tile_id_b = accumulator.tile_id_b;
		if ( accumulator.sample_count > 0 ) {
			portal.representative_point = Vector3{
				( float )( accumulator.representative_sum_x / ( double )accumulator.sample_count ),
				( float )( accumulator.representative_sum_y / ( double )accumulator.sample_count ),
				( float )( accumulator.representative_sum_z / ( double )accumulator.sample_count )
			};
			portal.traversal_cost = accumulator.traversal_cost_sum / ( double )accumulator.sample_count;
		} else {
			portal.traversal_cost = tile_world_size;
		}

		mesh->hierarchy.portals.push_back( portal );
		region_portal_lists[ portal.region_a ].push_back( portal.id );
		region_portal_lists[ portal.region_b ].push_back( portal.id );
	}

	/** 
	* 	Populate each region's coarse portal-reference range for future region-level routing.
	**/
	for ( nav_region_t &region : mesh->hierarchy.regions ) {
		std::vector<int32_t> &portal_ids = region_portal_lists[ region.id ];
		std::sort( portal_ids.begin(), portal_ids.end() );
		portal_ids.erase( std::unique( portal_ids.begin(), portal_ids.end() ), portal_ids.end() );
		region.first_portal_ref = ( int32_t )mesh->hierarchy.region_portal_refs.size();
		region.num_portal_refs = ( int32_t )portal_ids.size();
		mesh->hierarchy.region_portal_refs.insert( mesh->hierarchy.region_portal_refs.end(), portal_ids.begin(), portal_ids.end() );
	}

	mesh->hierarchy.debug_portal_count = ( int32_t )mesh->hierarchy.portals.size();
   mesh->hierarchy.portal_overlays.assign( mesh->hierarchy.portals.size(), nav_portal_overlay_t{} );
	mesh->hierarchy.portal_overlay_serial = 0;
}

/** 
* 	@brief	Validate the merged portal graph produced from region boundaries.
* 	@param	mesh	Navigation mesh containing freshly built portal records.
* 	@return	True when the portal graph references remain internally consistent.
**/
static bool Nav_Hierarchy_ValidatePortalGraph( const nav_mesh_t * mesh ) {
	/** 
	* 	Sanity: require a mesh before validating any portal graph references.
	**/
	if ( !mesh ) {
		return false;
	}

	/** 
	* 	Reject invalid portal endpoints and duplicate region pairs before checking region-reference symmetry.
	**/
	std::unordered_map<uint64_t, int32_t> portal_pair_seen;
	for ( const nav_portal_t &portal : mesh->hierarchy.portals ) {
		if ( portal.region_a < 0 || portal.region_b < 0 || portal.region_a >= ( int32_t )mesh->hierarchy.regions.size() || portal.region_b >= ( int32_t )mesh->hierarchy.regions.size() || portal.region_a == portal.region_b ) {
			SVG_Nav_Log( "[WARNING][NavHierarchy] Portal %d has invalid region endpoints (%d,%d).\n", portal.id, portal.region_a, portal.region_b );
			return false;
		}

		const uint64_t key = Nav_Hierarchy_MakeRegionPairKey( portal.region_a, portal.region_b );
		if ( portal_pair_seen.find( key ) != portal_pair_seen.end() ) {
			SVG_Nav_Log( "[WARNING][NavHierarchy] Duplicate merged portal detected for region pair (%d,%d).\n", portal.region_a, portal.region_b );
			return false;
		}
		portal_pair_seen.emplace( key, portal.id );
	}

	/** 
	* 	Verify that every portal appears in both owning region reference ranges.
	**/
	for ( const nav_portal_t &portal : mesh->hierarchy.portals ) {
		bool found_in_a = false;
		bool found_in_b = false;
		const nav_region_t &region_a = mesh->hierarchy.regions[ portal.region_a ];
		const nav_region_t &region_b = mesh->hierarchy.regions[ portal.region_b ];
		for ( int32_t portal_ref_index = 0; portal_ref_index < region_a.num_portal_refs; portal_ref_index++ ) {
			if ( mesh->hierarchy.region_portal_refs[ region_a.first_portal_ref + portal_ref_index ] == portal.id ) {
				found_in_a = true;
				break;
			}
		}
		for ( int32_t portal_ref_index = 0; portal_ref_index < region_b.num_portal_refs; portal_ref_index++ ) {
			if ( mesh->hierarchy.region_portal_refs[ region_b.first_portal_ref + portal_ref_index ] == portal.id ) {
				found_in_b = true;
				break;
			}
		}

		if ( !found_in_a || !found_in_b ) {
			SVG_Nav_Log( "[WARNING][NavHierarchy] Portal %d missing symmetric region references (a=%d b=%d).\n", portal.id, portal.region_a, portal.region_b );
			return false;
		}
	}

	return true;
}

/** 
* 	@brief	Emit a concise summary of the current static region and portal graph.
* 	@param	mesh	Navigation mesh containing the freshly built hierarchy.
* 	@note	Logs only a capped subset of suspicious regions to avoid log spam.
**/
static void Nav_Hierarchy_LogRegionSummary( const nav_mesh_t * mesh ) {
	/** 
	* 	Sanity: require a mesh and a built hierarchy before emitting summary diagnostics.
	**/
	if ( !mesh ) {
		return;
	}

 const int32_t region_count = ( int32_t )mesh->hierarchy.regions.size();
	const int32_t portal_count = ( int32_t )mesh->hierarchy.portals.size();
	const int32_t world_tile_count = ( int32_t )mesh->world_tiles.size();
	const double avg_tiles_per_region = ( region_count > 0 ) ? ( ( double )world_tile_count / ( double )region_count ) : 0.0;
	const double avg_portals_per_region = ( region_count > 0 ) ? ( ( double )mesh->hierarchy.region_portal_refs.size() / ( double )region_count ) : 0.0;

	/** 
	* 	Print the one-line Phase 3 summary first so benchmarks have a stable high-level snapshot.
	**/
    SVG_Nav_Log( "[NavHierarchy] tile_links=%d region_boundaries=%d regions=%d portals=%d avg_tiles_per_region=%.2f avg_portals_per_region=%.2f isolated=%d oversized=%d\n",
		mesh->hierarchy.debug_adjacency_link_count,
       mesh->hierarchy.debug_boundary_link_count,
		region_count,
       portal_count,
		avg_tiles_per_region,
        avg_portals_per_region,
		mesh->hierarchy.debug_isolated_region_count,
		mesh->hierarchy.debug_oversized_region_count );

	/** 
	* 	Cap suspicious-region logging so the hierarchy summary stays readable on noisy maps.
	**/
	int32_t isolated_printed = 0;
	int32_t oversized_printed = 0;
	for ( const nav_region_t &region : mesh->hierarchy.regions ) {
		if ( ( region.flags & NAV_REGION_FLAG_ISOLATED ) != 0 && isolated_printed < 8 ) {
			const int32_t anchor_tile_id = region.debug_anchor_tile_id;
			if ( anchor_tile_id >= 0 && anchor_tile_id < ( int32_t )mesh->world_tiles.size() ) {
				const nav_tile_t &anchor_tile = mesh->world_tiles[ anchor_tile_id ];
				SVG_Nav_Log( "[NavHierarchy] isolated_region id=%d tiles=%d anchor=(%d,%d)\n",
					region.id,
					region.num_tile_refs,
					anchor_tile.tile_x,
					anchor_tile.tile_y );
				isolated_printed++;
			}
		}

       if ( region.num_tile_refs >= NAV_HIERARCHY_OVERSIZED_REGION_TILE_COUNT && oversized_printed < 8 ) {
			const int32_t anchor_tile_id = region.debug_anchor_tile_id;
			if ( anchor_tile_id >= 0 && anchor_tile_id < ( int32_t )mesh->world_tiles.size() ) {
				const nav_tile_t &anchor_tile = mesh->world_tiles[ anchor_tile_id ];
				SVG_Nav_Log( "[NavHierarchy] oversized_region id=%d tiles=%d anchor=(%d,%d)\n",
					region.id,
					region.num_tile_refs,
					anchor_tile.tile_x,
					anchor_tile.tile_y );
				oversized_printed++;
			}
		}
	}
}

/** 
 * 	@brief	Build static regions and the derived initial portal graph from canonical world tiles.
* 	@param	mesh	Navigation mesh whose canonical world tiles should be partitioned.
 * 	@note	The first pass stays conservative and tile-granular, using a deterministic coarse region budget so traversable inter-region seams exist for merged portal extraction.
**/
void SVG_Nav_Hierarchy_BuildStaticRegions( nav_mesh_t * mesh ) {
	/** 
	* 	Sanity: require a valid mesh before attempting Phase 3 hierarchy construction.
	**/
	if ( !mesh ) {
		return;
	}

	/** 
	* 	Start from a clean hierarchy state so load/regenerate paths cannot retain stale region membership.
	**/
	SVG_Nav_Hierarchy_Clear( mesh );
	if ( mesh->world_tiles.empty() ) {
		return;
	}

	/** 
	* 	Formalize conservative canonical tile adjacency from the persisted fine edge metadata.
	**/
	Nav_Hierarchy_BuildTileAdjacency( mesh );

	/** 
	* 	Seed flood fills in deterministic tile-coordinate order so region ids remain stable across rebuilds.
	**/
	std::vector<int32_t> sorted_tile_ids;
	sorted_tile_ids.reserve( mesh->world_tiles.size() );
	for ( int32_t tile_id = 0; tile_id < ( int32_t )mesh->world_tiles.size(); tile_id++ ) {
		sorted_tile_ids.push_back( tile_id );
	}
	Nav_Hierarchy_SortTileIdsDeterministic( mesh, sorted_tile_ids );

	mesh->hierarchy.regions.reserve( mesh->world_tiles.size() );
	mesh->hierarchy.tile_refs.reserve( mesh->world_tiles.size() );
 mesh->hierarchy.debug_region_count = 0;
	mesh->hierarchy.debug_boundary_link_count = 0;
	mesh->hierarchy.debug_portal_count = 0;
	mesh->hierarchy.debug_isolated_region_count = 0;
	mesh->hierarchy.debug_oversized_region_count = 0;

	/** 
    * 	Flood-fill deterministic coarse regions over the canonical tile adjacency graph.
	* 	A fixed tile budget intentionally leaves cross-region seams so Phase 4 can extract merged portals.
	**/
	for ( const int32_t seed_tile_id : sorted_tile_ids ) {
		if ( mesh->world_tiles[ seed_tile_id ].region_id != NAV_REGION_ID_NONE ) {
			continue;
		}

		nav_region_t region = {};
		region.id = ( int32_t )mesh->hierarchy.regions.size();
		region.first_tile_ref = ( int32_t )mesh->hierarchy.tile_refs.size();
		region.debug_anchor_tile_id = seed_tile_id;

		std::vector<int32_t> stack;
		stack.push_back( seed_tile_id );
		mesh->world_tiles[ seed_tile_id ].region_id = region.id;

		while ( !stack.empty() ) {
			const int32_t tile_id = stack.back();
			stack.pop_back();

			nav_tile_t &tile = mesh->world_tiles[ tile_id ];
			mesh->hierarchy.tile_refs.push_back( nav_region_tile_ref_t{ .tile_id = tile_id } );
			region.num_tile_refs++;
			region.compatibility_bits |= Nav_Hierarchy_BuildTileCompatibilityBits( tile );
			region.flags |= Nav_Hierarchy_BuildTileRegionFlags( tile );

			const nav_tile_adjacency_t &adjacency = mesh->hierarchy.tile_adjacency[ tile_id ];
			for ( int32_t neighbor_index = 0; neighbor_index < adjacency.num_neighbor_refs; neighbor_index++ ) {
				const int32_t neighbor_tile_id = mesh->hierarchy.tile_neighbor_refs[ adjacency.first_neighbor_ref + neighbor_index ];
				if ( neighbor_tile_id < 0 || neighbor_tile_id >= ( int32_t )mesh->world_tiles.size() ) {
					continue;
				}
				if ( mesh->world_tiles[ neighbor_tile_id ].region_id != NAV_REGION_ID_NONE ) {
					continue;
				}
				// Stop region growth once the deterministic coarse budget is saturated.
				if ( ( region.num_tile_refs + ( int32_t )stack.size() ) >= NAV_HIERARCHY_MAX_TILES_PER_REGION ) {
					continue;
				}

				mesh->world_tiles[ neighbor_tile_id ].region_id = region.id;
				stack.push_back( neighbor_tile_id );
			}
		}

     // Mark and count regions that look suspiciously disconnected or saturated for future review.
		if ( region.num_tile_refs == 1 ) {
			const nav_tile_adjacency_t &adjacency = mesh->hierarchy.tile_adjacency[ seed_tile_id ];
			if ( adjacency.num_neighbor_refs == 0 ) {
			region.flags |= NAV_REGION_FLAG_ISOLATED;
			mesh->hierarchy.debug_isolated_region_count++;
           }
		}
        if ( region.num_tile_refs >= NAV_HIERARCHY_OVERSIZED_REGION_TILE_COUNT ) {
			mesh->hierarchy.debug_oversized_region_count++;
		}

		mesh->hierarchy.regions.push_back( region );
	}

	/** 
	* 	Sort each region's tile refs deterministically now that flood-fill membership is known.
	**/
	for ( nav_region_t &region : mesh->hierarchy.regions ) {
		std::vector<int32_t> region_tile_ids;
		region_tile_ids.reserve( (size_t)region.num_tile_refs );
		for ( int32_t tile_ref_index = 0; tile_ref_index < region.num_tile_refs; tile_ref_index++ ) {
			region_tile_ids.push_back( mesh->hierarchy.tile_refs[ region.first_tile_ref + tile_ref_index ].tile_id );
		}
		Nav_Hierarchy_SortTileIdsDeterministic( mesh, region_tile_ids );
		for ( int32_t tile_ref_index = 0; tile_ref_index < region.num_tile_refs; tile_ref_index++ ) {
			mesh->hierarchy.tile_refs[ region.first_tile_ref + tile_ref_index ].tile_id = region_tile_ids[ tile_ref_index ];
		}
		region.debug_anchor_tile_id = region_tile_ids.empty() ? -1 : region_tile_ids.front();
	}

	/** 
	* 	Rebuild leaf membership from the existing leaf->tile mapping so regions remain inspectable by BSP leaf.
	**/
	std::vector<std::vector<int32_t>> region_leaf_lists( mesh->hierarchy.regions.size() );
	for ( int32_t leaf_index = 0; leaf_index < mesh->num_leafs; leaf_index++ ) {
		nav_leaf_data_t &leaf = mesh->leaf_data[ leaf_index ];
		auto tile_ids_view = SVG_Nav_Leaf_GetTileIds( &leaf );
		const int32_t * tile_ids = tile_ids_view.first;
		const int32_t tile_count = tile_ids_view.second;
		if ( !tile_ids || tile_count <= 0 ) {
			continue;
		}

		std::vector<int32_t> region_ids;
		region_ids.reserve( (size_t)tile_count );
		for ( int32_t tile_offset = 0; tile_offset < tile_count; tile_offset++ ) {
			const int32_t tile_id = tile_ids[ tile_offset ];
			if ( tile_id < 0 || tile_id >= ( int32_t )mesh->world_tiles.size() ) {
				continue;
			}

			const int32_t region_id = mesh->world_tiles[ tile_id ].region_id;
			if ( region_id != NAV_REGION_ID_NONE ) {
				region_ids.push_back( region_id );
			}
		}

		std::sort( region_ids.begin(), region_ids.end() );
		region_ids.erase( std::unique( region_ids.begin(), region_ids.end() ), region_ids.end() );
		if ( region_ids.empty() ) {
			continue;
		}

		leaf.num_regions = ( int32_t )region_ids.size();
		leaf.region_ids = (int32_t * )gi.TagMallocz( sizeof( int32_t )* (size_t)leaf.num_regions, TAG_SVGAME_NAVMESH );
		memcpy( leaf.region_ids, region_ids.data(), sizeof( int32_t )* (size_t)leaf.num_regions );
		for ( const int32_t region_id : region_ids ) {
			region_leaf_lists[ region_id ].push_back( leaf_index );
		}
	}

	/** 
	* 	Flatten per-region leaf memberships into the shared hierarchy leaf-ref store.
	**/
	for ( nav_region_t &region : mesh->hierarchy.regions ) {
		std::vector<int32_t> &leaf_ids = region_leaf_lists[ region.id ];
		std::sort( leaf_ids.begin(), leaf_ids.end() );
		leaf_ids.erase( std::unique( leaf_ids.begin(), leaf_ids.end() ), leaf_ids.end() );
		region.first_leaf_ref = ( int32_t )mesh->hierarchy.leaf_refs.size();
		region.num_leaf_refs = ( int32_t )leaf_ids.size();
		for ( const int32_t leaf_index : leaf_ids ) {
			mesh->hierarchy.leaf_refs.push_back( nav_region_leaf_ref_t{ .leaf_index = leaf_index } );
		}
	}

	/** 
	* 	Extract merged portals now that deterministic coarse region seams exist across connected tile space.
	**/
	Nav_Hierarchy_BuildPortals( mesh, sorted_tile_ids );

	/** 
 * 	Run validation passes and then log a concise summary for the current hierarchy graph.
	**/
	mesh->hierarchy.debug_region_count = ( int32_t )mesh->hierarchy.regions.size();
	mesh->hierarchy.build_serial++;
	mesh->hierarchy.dirty = false;
	if ( !Nav_Hierarchy_ValidateRegions( mesh ) ) {
		SVG_Nav_Log( "[WARNING][NavHierarchy] Static region validation reported a connectivity mismatch.\n" );
	}
 if ( !Nav_Hierarchy_ValidatePortalGraph( mesh ) ) {
		SVG_Nav_Log( "[WARNING][NavHierarchy] Portal graph validation reported a reference mismatch.\n" );
	}
	Nav_Hierarchy_LogRegionSummary( mesh );
}

/** 
*    @brief  Free the current navigation mesh.
*         Releases all memory allocated for navigation data using TAG_SVGAME_NAVMESH.
**/
/** 
*    @brief  Free the current navigation mesh.
*         Releases all memory allocated for navigation data using TAG_SVGAME_NAVMESH.
**/
void SVG_Nav_FreeMesh( void ) {
	/** 
	* 	Early out if no mesh exists.
	**/
	if ( !g_nav_mesh ) {
		return;
	}

	/** 
	* 	Clear coarse routing graph: its pointers/indices become invalid once the mesh is freed.
	**/
	SVG_Nav_ClusterGraph_Clear();

	/** 
	* 	Clear dynamic occupancy map so unordered_map allocations are released before TagFree.
	**/
	SVG_Nav_Occupancy_Clear( g_nav_mesh.get() );

	/** 
	* 	Clear hierarchy scaffolding before freeing the fine mesh so any auxiliary membership arrays are released.
	**/
	SVG_Nav_Hierarchy_Clear( g_nav_mesh.get() );

	/** 
	* 	Free canonical world tiles.
	**/
	{
		const int32_t cells_per_tile = g_nav_mesh->tile_size* g_nav_mesh->tile_size;
		for ( nav_tile_t &tile : g_nav_mesh->world_tiles ) {
			if ( tile.presence_bits ) {
				gi.TagFree( tile.presence_bits );
				tile.presence_bits = nullptr;
			}
			Nav_FreeTileCells( &tile, cells_per_tile );
		}
		g_nav_mesh->world_tiles.clear();
		g_nav_mesh->world_tile_id_of.clear();
	}

	/** 
	* 	Free per-leaf tile id arrays.
	**/
	if ( g_nav_mesh->leaf_data ) {
		for ( int32_t i = 0; i < g_nav_mesh->num_leafs; i++ ) {
			nav_leaf_data_t * leaf = &g_nav_mesh->leaf_data[ i ];
			if ( leaf->tile_ids ) {
				gi.TagFree( leaf->tile_ids );
				leaf->tile_ids = nullptr;
			}
			leaf->num_tiles = 0;
		}
		gi.TagFree( g_nav_mesh->leaf_data );
		g_nav_mesh->leaf_data = nullptr;
		g_nav_mesh->num_leafs = 0;
	}

	/** 
	*    Free inline model data:
	**/
	if ( g_nav_mesh->inline_model_data ) {
		// Iterate through all inline models.
		for ( int32_t i = 0; i < g_nav_mesh->num_inline_models; i++ ) {
			nav_inline_model_data_t * model = &g_nav_mesh->inline_model_data[ i ];

			// Free tiles in this model.
			if ( model->tiles ) {
				for ( int32_t t = 0; t < model->num_tiles; t++ ) {
					nav_tile_t * tile = &model->tiles[ t ];

					// Free presence bitset.
					if ( tile->presence_bits ) {
						gi.TagFree( tile->presence_bits );
					}

					// Free XY cells and their layers.
					if ( tile->cells ) {
						int32_t cells_per_tile = g_nav_mesh->tile_size* g_nav_mesh->tile_size;
						for ( int32_t c = 0; c < cells_per_tile; c++ ) {
							if ( tile->cells[ c ].layers ) {
								gi.TagFree( tile->cells[ c ].layers );
							}
						}
						gi.TagFree( tile->cells );
					}
				}
				gi.TagFree( model->tiles );
			}
		}
		gi.TagFree( g_nav_mesh->inline_model_data );
	}

	/** 
	* 	Free inline model runtime data:
	**/
	if ( g_nav_mesh->inline_model_runtime ) {
		gi.TagFree( g_nav_mesh->inline_model_runtime );
		g_nav_mesh->inline_model_runtime = nullptr;
		g_nav_mesh->num_inline_model_runtime = 0;
	}

	/** 
	* 	Reset the RAII owner, which will run the mesh destructor and free memory.
	* 	This ensures STL members (vectors, maps) are properly destroyed.
	**/
	g_nav_mesh.reset();

	/** 
	* 	Last but not least, free any remaining TAG_SVGAME_NAVMESH memory.
	**/
	gi.FreeTags( TAG_SVGAME_NAVMESH );
}



/** 
* 
* 
* 
*    Navigation Helper Functions:
* 
* 
* 
**/
/** 
*   @brief  Build a navigation agent profile using registered nav cvars.
*   @note   Falls back to safe defaults until the cvars are registered.
*   @return Populated nav_agent_profile_t describing hull and traversal limits.
 **/
nav_agent_profile_t SVG_Nav_BuildAgentProfileFromCvars( void ) {
	nav_agent_profile_t profile = {};

	/** 
	*   Gather hull extents from the registered cvars.
	 **/
	profile.mins[ 0 ] = nav_agent_mins_x ? nav_agent_mins_x->value : -16.0;
	profile.mins[ 1 ] = nav_agent_mins_y ? nav_agent_mins_y->value : -16.0;
	profile.mins[ 2 ] = nav_agent_mins_z ? nav_agent_mins_z->value : -36.0;
	profile.maxs[ 0 ] = nav_agent_maxs_x ? nav_agent_maxs_x->value : 16.0;
	profile.maxs[ 1 ] = nav_agent_maxs_y ? nav_agent_maxs_y->value : 16.0;
	profile.maxs[ 2 ] = nav_agent_maxs_z ? nav_agent_maxs_z->value : 36.0;

	/** 
	*   Derive traversal limits (steps, drops, slopes) from cvars.
	 **/
	const double default_step = 18.0;
	const double default_drop = 128.0;
   const double default_drop_cap = 64.0;
	const double default_slope_normal_z = PHYS_MAX_SLOPE_NORMAL;

	profile.max_step_height = nav_max_step ? nav_max_step->value : default_step;
	profile.max_drop_height = nav_max_drop ? nav_max_drop->value : default_drop;
    profile.max_drop_height_cap = nav_max_drop_height_cap ? nav_max_drop_height_cap->value : profile.max_drop_height;
	if ( profile.max_drop_height_cap <= 0.0 ) {
		profile.max_drop_height_cap = default_drop_cap;
	}
    profile.max_slope_normal_z = nav_max_slope_normal_z ? nav_max_slope_normal_z->value : default_slope_normal_z;

	return profile;
}
/** 
*   @brief  Check if a surface normal is walkable based on slope.
*   @param  normal		Surface normal vector.
*   @param  min_normal_z	Minimum walkable surface normal Z.
*   @return True if the slope is walkable, false otherwise.
**/
const bool IsWalkableSlope( const Vector3 &normal, const double min_normal_z ) {
	// Surface is walkable if normal Z is above the threshold.
	return normal[ 2 ] >= min_normal_z;
}

/** 
* 	@brief	Build runtime inline model data for navigation mesh.
* 	@note	This allocates and initializes the runtime array. Per-frame updates should call
* 			`SVG_Nav_RefreshInlineModelRuntime()` (no allocations).
**/
static void Nav_BuildInlineModelRuntime( nav_mesh_t * mesh, const std::unordered_map<int32_t, svg_base_edict_t * > &model_to_ent ) {
	/** 
	* 	Sanity checks / early outs.
	**/
	if ( !mesh ) {
		return;
	}

	/** 
	* 	Reset any existing runtime mapping and its lookup table.
	**/
	mesh->inline_model_runtime_index_of.clear();

	// Free old runtime mapping (if any) to avoid leaks on regen.
	if ( mesh->inline_model_runtime ) {
		gi.TagFree( mesh->inline_model_runtime );
		mesh->inline_model_runtime = nullptr;
		mesh->num_inline_model_runtime = 0;
	}

	if ( model_to_ent.empty() ) {
		return;
	}

	mesh->num_inline_model_runtime = ( int32_t )model_to_ent.size();
	mesh->inline_model_runtime = ( nav_inline_model_runtime_t* )gi.TagMallocz(
		sizeof( nav_inline_model_runtime_t )* mesh->num_inline_model_runtime, TAG_SVGAME_NAVMESH );

	/** 
	* 	Reserve the lookup map up-front so inserts stay cheap.
	**/
	mesh->inline_model_runtime_index_of.reserve( ( size_t )mesh->num_inline_model_runtime );

	int32_t out_index = 0;
	for ( const auto &it : model_to_ent ) {
		const int32_t model_index = it.first;
		svg_base_edict_t * ent = it.second;

		nav_inline_model_runtime_t &rt = mesh->inline_model_runtime[ out_index ];
		rt.model_index = model_index;
		rt.owner_ent = ent;
		rt.owner_entnum = ent ? ent->s.number : 0;

		rt.origin = ent ? ent->currentOrigin : Vector3{};
		rt.angles = ent ? ent->currentAngles : Vector3{};
		rt.dirty = false;

		/** 
		* 	Populate owner_entnum -> runtime index map.
		* 	We only emit entries for valid entity numbers.
		**/
		if ( rt.owner_entnum > 0 ) {
			mesh->inline_model_runtime_index_of.emplace( rt.owner_entnum, out_index );
		}

		out_index++;
	}
}

/** 
* 	@brief	Lookup an inline-model runtime entry by owning entity number.
* 	@param	mesh	Navigation mesh.
* 	@param	owner_entnum	Owning entity number (edict->s.number).
* 	@return	Pointer to runtime entry if found, otherwise nullptr.
* 	@note	Uses the cached `inline_model_runtime_index_of` map to avoid linear scans.
**/
const nav_inline_model_runtime_t * SVG_Nav_GetInlineModelRuntimeForOwnerEntNum( const nav_mesh_t * mesh, const int32_t owner_entnum ) {
	/** 
	* 	Sanity checks: validate mesh and runtime availability.
	**/
	if ( !mesh || owner_entnum <= 0 ) {
		return nullptr;
	}
	if ( !mesh->inline_model_runtime || mesh->num_inline_model_runtime <= 0 ) {
		return nullptr;
	}

	/** 
	* 	Resolve runtime index via the prebuilt lookup map.
	**/
	auto it = mesh->inline_model_runtime_index_of.find( owner_entnum );
	if ( it == mesh->inline_model_runtime_index_of.end() ) {
		return nullptr;
	}

	/** 
	* 	Bounds-check the resolved index before returning.
	**/
	const int32_t idx = it->second;
	if ( idx < 0 || idx >= mesh->num_inline_model_runtime ) {
		return nullptr;
	}

	return &mesh->inline_model_runtime[ idx ];
}

/** 
* 	@brief	Dump the inline-model runtime index map for debugging.
* 	@param	mesh	Navigation mesh.
* 	@note	Prints to console; intended for developer diagnostics.
**/
void SVG_Nav_Debug_PrintInlineModelRuntimeIndexMap( const nav_mesh_t * mesh ) {
	/** 
	* 	Sanity: validate mesh and ensure we have something to dump.
	**/
	if ( !mesh ) {
		gi.dprintf( "%s: mesh == nullptr\n", __func__ );
		return;
	}

	gi.dprintf( "[NavDebug] inline_model_runtime_index_of: %d entries (runtime_count=%d)\n",
		( int32_t )mesh->inline_model_runtime_index_of.size(), mesh->num_inline_model_runtime );

	/** 
	* 	Iterate the map and emit each mapping as owner_entnum -> runtime index.
	**/
	for ( const auto &kv : mesh->inline_model_runtime_index_of ) {
		const int32_t ownerEntNum = kv.first;
		const int32_t runtimeIndex = kv.second;
		gi.dprintf( "[NavDebug]   owner_entnum=%d -> runtime_index=%d\n", ownerEntNum, runtimeIndex );
	}
}

/** 
*    @brief  Refresh the runtime data for inline models (transforms only).
*         This updates the origin/angles of inline model runtime entries based on current entity state.
**/
void SVG_Nav_RefreshInlineModelRuntime( void ) {
	if ( !g_nav_mesh || !g_nav_mesh.get() || !g_nav_mesh->inline_model_runtime || g_nav_mesh->num_inline_model_runtime <= 0 ) {
		return;
	}

	for ( int32_t i = 0; i < g_nav_mesh->num_inline_model_runtime; i++ ) {
		nav_inline_model_runtime_t &rt = g_nav_mesh->inline_model_runtime[ i ];
		svg_base_edict_t * ent = rt.owner_ent;

		// Entity pointer can exist but be inactive/reused; verify.
		if ( !SVG_Entity_IsActive( ent ) ) {
			// Keep last transform; mark dirty so consumers can handle dropout.
			rt.dirty = true;
			continue;
		}

		const Vector3 newOrigin = ent->currentOrigin;
		const Vector3 newAngles = ent->currentAngles;

		// Cheap exact compare (Vector3 here is POD-ish); avoid double epsilon logic unless you need it.
		const bool originChanged = ( newOrigin[ 0 ] != rt.origin[ 0 ] ) || ( newOrigin[ 1 ] != rt.origin[ 1 ] ) || ( newOrigin[ 2 ] != rt.origin[ 2 ] );
		const bool anglesChanged = ( newAngles[ 0 ] != rt.angles[ 0 ] ) || ( newAngles[ 1 ] != rt.angles[ 1 ] ) || ( newAngles[ 2 ] != rt.angles[ 2 ] );

		if ( originChanged || anglesChanged ) {
			rt.origin = newOrigin;
			rt.angles = newAngles;
			rt.dirty = true;
		} else {
			rt.dirty = false;
		}
	}
}



/** 
* 
* 
* 
*    Navigation Pathfinding Node Methods:
* 
* 
* 
**/
/** 
* 	@brief	Select a navigation layer index from an XY cell for a desired world-space Z.
* 	@param	mesh			Navigation mesh (provides `z_quant` and default step/tolerance inputs).
* 	@param	cell			Navigation XY cell to select a layer from.
* 	@param	desired_z	Desired world-space Z height to match (units).
* 	@param	out_layer_index	[out] Selected layer index inside `cell->layers`.
* 	@return	True if a suitable layer index was found.
* 	@note	This selection is intentionally conservative and deterministic:
* 			- First selects the closest layer within the Z tolerance gate.
* 			- When the desired Z is within a small preference threshold of any layer,
* 			  applies a top/bottom preference (defaults to top-most).
* 			- If nothing is within tolerance, falls back to closest-by-Z.
* 			Actual movement feasibility is validated later by step tests (e.g.,
* 			`Nav_CanTraverseStep_ExplicitBBox`) when building/expanding paths.
**/
const bool Nav_SelectLayerIndex( const nav_mesh_t * mesh, const nav_xy_cell_t * cell, double desired_z,
	int32_t * out_layer_index ) {
	/** 
	*  Sanity checks.
	**/
	if ( !mesh || !cell || cell->num_layers <= 0 || !out_layer_index ) {
		return false;
	}

	/** 
	* 	Z tolerance gate:
	* 		Only layers within this tolerance are considered candidates for the primary selection.
	* 		This reduces Z-jitter in multi-layer cells and keeps selection aligned with
	* 		step capabilities.
	**/
	double z_tolerance = 0.0;
	if ( nav_z_tolerance && nav_z_tolerance->value > 0.0f ) {
		// Use explicit tuning when provided (world units).
		z_tolerance = nav_z_tolerance->value;
	} else {
		// Default: allow up to a step plus half a quant to handle common stair sampling.
		z_tolerance = mesh->max_step + ( mesh->z_quant* 0.5f );
	}
	const double selectTolerance = z_tolerance;

	/** 
	* 	Layer selection preference policy:
	* 		The selection bias is currently derived from the default path policy.
	* 		This keeps selection behavior consistent across the nav module without adding
	* 		new cvars.
	* 
	* 	@note	The preference is only applied when the desired Z is within
	* 			`layer_select_prefer_z_threshold` of any layer, and the candidate layer is still
	* 			inside `selectTolerance`.
	**/
	svg_nav_path_policy_t defaultPolicy = {};
	// Select whether "top" means highest Z or lowest Z.
	const bool preferTop = defaultPolicy.layer_select_prefer_top;
	// Threshold (world units) to activate the preference behavior.
	const double preferThreshold = std::max( 0.0, defaultPolicy.layer_select_prefer_z_threshold );

	/** 
	* 	Pass 1: closest-by-Z selection.
	* 		- Track a primary selection (closest within `selectTolerance`).
	* 		- Track a fallback selection (closest overall) for when tolerance rejects all.
	**/
	int32_t best_index = -1;
	double best_delta = std::numeric_limits<double>::max();
	int32_t fallback_index = -1;
	double best_fallback_delta = std::numeric_limits<double>::max();

	for ( int32_t i = 0; i < cell->num_layers; i++ ) {
		// Convert from quantized Z to world-space Z for distance comparisons.
		const double layer_z = ( double )cell->layers[ i ].z_quantized* mesh->z_quant;
		// Absolute Z delta from the desired height.
		const double delta = std::fabs( layer_z - desired_z );

		// Always track the closest-by-Z fallback in case no layer passes the tolerance.
		if ( delta < best_fallback_delta ) {
			best_fallback_delta = delta;
			fallback_index = i;
		}

		// Prefer layers within the selection tolerance.
		if ( delta <= selectTolerance && delta < best_delta ) {
			best_delta = delta;
			best_index = i;
		}
	}

	/** 
	* 	Fallback:
	* 		If no layers are inside the tolerance gate, still return the closest-by-Z layer.
	* 		This allows the caller to attempt more expensive traversal checks (or fallback
	* 		pathfinding) rather than failing immediately.
	**/
	if ( best_index < 0 ) {
		best_index = fallback_index;
	}
	if ( best_index < 0 ) {
		return false;
	}

	/** 
	* 	Pass 2: thresholded top/bottom preference.
	* 		If the desired Z is within a small window of any layer, use this to stabilize
	* 		selection in multi-layer cells by preferring a consistent extremum (top or bottom).
	**/
	if ( cell->num_layers > 1 && preferThreshold > 0.0 ) {
		/** 
		* 	Compute closest delta across all layers.
		* 		This decides whether we are "close enough" to activate the preference behavior.
		**/
		double closest_delta = std::numeric_limits<double>::max();
		for ( int32_t i = 0; i < cell->num_layers; i++ ) {
			// Convert quantized Z to world Z.
			const double layer_z = ( double )cell->layers[ i ].z_quantized* mesh->z_quant;
			// Compute absolute Z delta.
			const double delta = std::fabs( layer_z - desired_z );
			// Track the closest delta for preference activation.
			closest_delta = std::min( closest_delta, delta );
		}

		if ( closest_delta <= preferThreshold ) {
			/** 
			* 	Preference selection:
			* 		Choose the extremum (top or bottom) among layers that are still within
			* 		`selectTolerance`. This prevents preferring a far-away level purely due to
			* 		being "top"/"bottom".
			**/
			int32_t preferred_index = -1;
			double preferred_z = preferTop ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();

			for ( int32_t i = 0; i < cell->num_layers; i++ ) {
				// Convert quantized Z to world-space Z.
				const double layer_z = ( double )cell->layers[ i ].z_quantized* mesh->z_quant;
				// Compute delta for tolerance gating.
				const double delta = std::fabs( layer_z - desired_z );
				if ( delta > selectTolerance ) {
					// Skip layers that are too far to be considered viable.
					continue;
				}

				// Seed the preference candidate.
				if ( preferred_index < 0 ) {
					preferred_index = i;
					preferred_z = layer_z;
					continue;
				}

				// Compare Z to decide top/bottom preference update.
				if ( preferTop ) {
					if ( layer_z > preferred_z ) {
						preferred_z = layer_z;
						preferred_index = i;
					}
				} else {
					if ( layer_z < preferred_z ) {
						preferred_z = layer_z;
						preferred_index = i;
					}
				}
			}

			// Commit preference selection when any viable candidates were found.
			if ( preferred_index >= 0 ) {
				best_index = preferred_index;
			}
		}
	}

	/** 
	* 	Commit output.
	**/
	* out_layer_index = best_index;
	return true;
}

/** 
*  @brief	Check whether a sparse tile cell is present in the tile bitset.
*  @param	tile		Tile holding the presence bitset.
*  @param	cell_index	Cell index inside the tile (0..cells_per_tile-1).
*  @return	True if the cell is marked as present, false otherwise.
*  @note	This helper guards accesses to sparse cell arrays.
**/
/** 
	@brief	Check whether a sparse tile cell is present in the tile bitset.
	@param	tile		Tile holding the presence bitset.
	@param	cell_index	Cell index inside the tile (0..cells_per_tile-1).
	@return	True if the cell is marked as present, false otherwise.
	@note	This helper guards accesses to sparse cell arrays and validates the
			presence_bits pointer to prevent dereferencing null pointers on
			sparsely-populated tiles.
**/
static inline bool Nav_CellPresent( const nav_tile_t * tile, const int32_t cell_index ) {
	/** 
	*  Sanity checks: ensure tile and its sparse presence bitset exist before
	*  attempting to read the bitset. This prevents accidental dereferences of
	*  nullptr when tiles were allocated with empty storage during generation/load.
	**/
	if ( !tile || !tile->presence_bits || cell_index < 0 ) {
		return false;
	}

	/** 
	*  Compute bitset indices and test the cell presence bit.
	*  Guard against out-of-range word_index to avoid UB in corrupt meshes.
	**/
	const int32_t word_index = cell_index >> 5;
	const int32_t bit_index = cell_index & 31;
	// Compute number of words available for this tile.
	// Note: We derive words from tile_size stored on the global mesh only when
	// the caller has access to it. Here we do a conservative bounds check by
	// ensuring the computed index is non-negative and the pointer is non-null.
	return ( tile->presence_bits[ word_index ] & ( 1u << bit_index ) ) != 0;
}

/** 
*  @brief	Compute the world-space center position for a node in a tile cell/layer.
*  @param	mesh	Navigation mesh owning the tile.
*  @param	tile	Tile containing the cell.
*  @param	cell_index	Cell index inside the tile.
*  @param	layer	Layer data for the node (provides quantized Z).
*  @return	World-space center position for the node.
*  @note	Returns a zero vector if inputs are invalid.
**/
Vector3 Nav_NodeWorldPosition( const nav_mesh_t * mesh, const nav_tile_t * tile, int32_t cell_index, const nav_layer_t * layer ) {
	/** 
	*  Sanity checks: require mesh, tile, and layer data.
	**/
	if ( !mesh || !tile || !layer ) {
		return Vector3{};
	}

	/** 
	*  Resolve tile origin and cell coordinates inside the tile grid.
	**/
	const double tile_world_size = Nav_TileWorldSize( mesh );
	const int32_t cell_x = cell_index % mesh->tile_size;
	const int32_t cell_y = cell_index / mesh->tile_size;

	/** 
	*  Compute world-space coordinates for the cell center and layer height.
	**/
	const double world_x = ( ( double )tile->tile_x* tile_world_size ) + ( ( double )cell_x + 0.5 )* mesh->cell_size_xy;
	const double world_y = ( ( double )tile->tile_y* tile_world_size ) + ( ( double )cell_y + 0.5 )* mesh->cell_size_xy;
	const double world_z = ( double )layer->z_quantized* mesh->z_quant;

	/** 
	*  Return the computed node position.
	**/
	return Vector3{ ( float )world_x, ( float )world_y, ( float )world_z };
}

/** 
*  @brief	Resolve a node reference inside a specific tile based on world position.
*  @param	mesh	Navigation mesh owning the tile.
*  @param	tile	Tile to query for the node.
*  @param	tile_id	Canonical world tile id for the tile.
*  @param	leaf_index	BSP leaf index for the position (stored in node key).
*  @param	position	World-space position to query.
*  @param	desired_z	Desired Z for layer selection.
*  @param	out_node	[out] Node reference to populate on success.
*  @return	True if a node could be resolved from the tile.
**/
static bool Nav_TryResolveNodeInTile( const nav_mesh_t * mesh, const nav_tile_t * tile, const int32_t tile_id,
	const int32_t leaf_index, const Vector3 &position, double desired_z, nav_node_ref_t * out_node, const bool allow_closest_layer_fallback ) {
	/** 
	*  Sanity checks: require mesh, tile, and output storage.
	**/
	if ( !mesh || !tile || !out_node ) {
		return false;
	}

	/** 
	*  Compute tile origin and map the world position into a cell index.
	**/
    const double tile_world_size = Nav_TileWorldSize( mesh );
	const double tile_origin_x = ( double )tile->tile_x* tile_world_size;
	const double tile_origin_y = ( double )tile->tile_y* tile_world_size;
	const int32_t cell_x = Nav_WorldToCellCoord( position[ 0 ], tile_origin_x, mesh->cell_size_xy );
	const int32_t cell_y = Nav_WorldToCellCoord( position[ 1 ], tile_origin_y, mesh->cell_size_xy );

	/** 
	*  Reject positions that map outside the tile's cell grid.
	**/
	if ( cell_x < 0 || cell_y < 0 || cell_x >= mesh->tile_size || cell_y >= mesh->tile_size ) {
		return false;
	}

	/** 
	*  Resolve the cell index and obtain a safe pointer/count via the public helper.
	*  This avoids directly dereferencing the tile's `cells` pointer which may be
	*  null for sparse tiles.
	**/
	const int32_t cell_index = ( cell_y* mesh->tile_size ) + cell_x;
	if ( !Nav_CellPresent( tile, cell_index ) ) {
		return false;
	}

	auto cellsView = SVG_Nav_Tile_GetCells( mesh, const_cast<nav_tile_t * >( tile ) );
	const nav_xy_cell_t * cellsPtr = cellsView.first;
	const int32_t cellsCount = cellsView.second;
	if ( !cellsPtr || cell_index < 0 || cell_index >= cellsCount ) {
        if ( nav_debug_show_failed_lookups && nav_debug_show_failed_lookups->integer != 0 ) {
			gi.dprintf( "[NavDebug][Lookup] Nav_TryResolveNodeInTile: tile_id=%d cell_index=%d out-of-range (cellsCount=%d)\n",
				tile_id, cell_index, cellsCount );
		}
		return false;
	}

	const nav_xy_cell_t * cell = &cellsPtr[ cell_index ];
	if ( cell->num_layers <= 0 || !cell->layers ) {
        if ( nav_debug_show_failed_lookups && nav_debug_show_failed_lookups->integer != 0 ) {
			gi.dprintf( "[NavDebug][Lookup] Nav_TryResolveNodeInTile: tile_id=%d cell_index=%d no layers\n",
				tile_id, cell_index );
		}
		return false;
	}

	/** 
	*  Select the best layer index based on the desired Z value.
	**/
	int32_t layer_index = -1;
	if ( !Nav_SelectLayerIndex( mesh, cell, desired_z, &layer_index ) ) {
        if ( nav_debug_show_failed_lookups && nav_debug_show_failed_lookups->integer != 0 ) {
			gi.dprintf( "[NavDebug][Lookup] Nav_TryResolveNodeInTile: tile_id=%d cell_index=%d no suitable layer for desired_z=%.1f\n",
				tile_id, cell_index, desired_z );
		}
		return false;
	}

	/** 
	*  Strict layer guard:
	*      When the caller disallows fallback, reject layers whose Z is outside the
	*      normal step-scale selection tolerance. This prevents exact neighbor expansion
	*      from snapping to unrelated layers inside the same XY cell and turning bad
	*      layer resolution into misleading `StepTest` failures.
	**/
	if ( !allow_closest_layer_fallback ) {
		// Rebuild the same Z tolerance used by primary layer selection.
		double z_tolerance = 0.0;
		if ( nav_z_tolerance && nav_z_tolerance->value > 0.0f ) {
			z_tolerance = nav_z_tolerance->value;
		} else {
			z_tolerance = mesh->max_step + ( mesh->z_quant* 0.5f );
		}

		// Measure how far the selected layer sits from the caller's desired Z.
		const double selected_layer_z = ( double )cell->layers[ layer_index ].z_quantized* mesh->z_quant;
		const double selected_delta = std::fabs( selected_layer_z - desired_z );
		// Reject this layer when strict lookup is required and the match is too far away.
		if ( selected_delta > z_tolerance ) {
			return false;
		}
	}

	/** 
	*  Populate the node reference with keys and world-space position.
	**/
	const nav_layer_t * layer = &cell->layers[ layer_index ];
	out_node->key.leaf_index = leaf_index;
	out_node->key.tile_index = tile_id;
	out_node->key.cell_index = cell_index;
	out_node->key.layer_index = layer_index;
	out_node->worldPosition = Nav_NodeWorldPosition( mesh, tile, cell_index, layer );
	return true;
}

/** 
*  @brief	Find a navigation node in a specific BSP leaf using its tile list.
*  @param	mesh	Navigation mesh holding world tiles.
*  @param	leaf_data	Leaf data containing tile ids to search.
*  @param	leaf_index	Leaf index to store in the output node key.
*  @param	position	World-space position to query.
*  @param	desired_z	Desired Z for layer selection.
*  @param	out_node	[out] Node reference to populate on success.
*  @param	allow_fallback	If true, allows a broader scan of leaf tiles.
*  @return	True if a node was found in this leaf, false otherwise.
**/
static bool Nav_FindNodeInLeaf( const nav_mesh_t * mesh, const nav_leaf_data_t * leaf_data, int32_t leaf_index,
	const Vector3 &position, double desired_z, nav_node_ref_t * out_node, bool allow_fallback ) {
	/** 
	*  Sanity checks: require mesh, leaf data, and output storage.
	**/
	if ( !mesh || !leaf_data || !out_node ) {
		return false;
	}
	if ( leaf_data->num_tiles <= 0 || !leaf_data->tile_ids ) {
		return false;
	}

	/** 
	*  Compute the target tile coordinate from the world position.
	**/
	const double tile_world_size = Nav_TileWorldSize( mesh );
	const int32_t target_tile_x = Nav_WorldToTileCoord( position[ 0 ], tile_world_size );
	const int32_t target_tile_y = Nav_WorldToTileCoord( position[ 1 ], tile_world_size );

	/** 
	*  Iterate the leaf's tile list to resolve a node.
	**/
	for ( int32_t i = 0; i < leaf_data->num_tiles; i++ ) {
		// Resolve tile id and ensure it is within bounds.
		const int32_t tile_id = leaf_data->tile_ids[ i ];
		if ( tile_id < 0 || tile_id >= ( int32_t )mesh->world_tiles.size() ) {
			continue;
		}

		const nav_tile_t * tile = &mesh->world_tiles[ tile_id ];
		// Skip tiles that do not match the target coordinate unless fallback is allowed.
		if ( !allow_fallback && ( tile->tile_x != target_tile_x || tile->tile_y != target_tile_y ) ) {
			continue;
		}

		// Attempt to resolve a node within this tile.
     if ( Nav_TryResolveNodeInTile( mesh, tile, tile_id, leaf_index, position, desired_z, out_node, allow_fallback ) ) {
			return true;
		}
	}

	return false;
}

/** 
*  @brief	Select a layer index using a blend between start and goal Z heights.
*  @param	mesh	Navigation mesh used for quantization.
*  @param	cell	Navigation cell containing layers.
*  @param	start_z	Starting Z height (world units).
*  @param	goal_z	Goal Z height (world units).
*  @param	start_pos	Start position (for distance-based blending).
*  @param	goal_pos	Goal position (for distance-based blending).
*  @param	blend_start_dist	Distance at which blending begins.
*  @param	blend_full_dist	Distance at which blending fully favors the goal Z.
*  @param	out_layer_index	[out] Selected layer index.
*  @return	True if a suitable layer index was found.
**/
const bool Nav_SelectLayerIndex_BlendZ( const nav_mesh_t * mesh, const nav_xy_cell_t * cell, double start_z, double goal_z,
	const Vector3 &start_pos, const Vector3 &goal_pos, const double blend_start_dist, const double blend_full_dist, int32_t * out_layer_index ) {
	/** 
	*  Sanity checks: require mesh, cell, and output storage.
	**/
	if ( !mesh || !cell || !out_layer_index ) {
		return false;
	}

	/** 
	*  Compute horizontal distance to drive Z blending.
	**/
	const Vector3 delta = QM_Vector3Subtract( goal_pos, start_pos );
	const double dist2d = sqrtf( ( delta[ 0 ]* delta[ 0 ] ) + ( delta[ 1 ]* delta[ 1 ] ) );

	/** 
	*  Derive a blend factor between start and goal Z based on the distance gates.
	**/
	double blend_factor = 0.0;
	if ( blend_full_dist > blend_start_dist ) {
		blend_factor = QM_Clamp( ( dist2d - blend_start_dist ) / ( blend_full_dist - blend_start_dist ), 0.0, 1.0 );
	} else if ( blend_full_dist > 0.0 ) {
		blend_factor = ( dist2d >= blend_full_dist ) ? 1.0 : 0.0;
	}

	/** 
	*  Blend between start and goal Z and delegate to the standard selector.
	**/
	const double desired_z = ( ( 1.0 - blend_factor )* start_z ) + ( blend_factor* goal_z );
	return Nav_SelectLayerIndex( mesh, cell, desired_z, out_layer_index );
}

/** 
*  @brief	Find a navigation node using blended start/goal Z selection.
*  @param	mesh	Navigation mesh to query.
*  @param	position	World-space position to query.
*  @param	start_z	Seeker starting Z height.
*  @param	goal_z	Target goal Z height.
*  @param	start_pos	Seeker starting position.
*  @param	goal_pos	Target goal position.
*  @param	blend_start_dist	Distance at which blending starts.
*  @param	blend_full_dist	Distance at which blending fully favors goal Z.
*  @param	out_node	[out] Output node reference.
*  @param	allow_fallback	Allow fallback search if direct lookup fails.
*  @return	True if a node was found, false otherwise.
**/
const bool Nav_FindNodeForPosition_BlendZ( const nav_mesh_t * mesh, const Vector3 &position, double start_z, double goal_z,
	const Vector3 &start_pos, const Vector3 &goal_pos, const double blend_start_dist, const double blend_full_dist,
	nav_node_ref_t * out_node, bool allow_fallback ) {
	/** 
	*  Sanity checks: require mesh and output storage.
	**/
	if ( !mesh || !out_node ) {
		return false;
	}

	/** 
	*  Build a blended desired Z value based on horizontal distance.
	**/
	const Vector3 delta = QM_Vector3Subtract( goal_pos, start_pos );
	const double dist2d = sqrtf( ( delta[ 0 ]* delta[ 0 ] ) + ( delta[ 1 ]* delta[ 1 ] ) );
	const double denom = blend_full_dist - blend_start_dist;
	const double t = ( denom > 0.0 ) ? QM_Clamp( ( dist2d - blend_start_dist ) / denom, 0.0, 1.0 ) : ( dist2d >= blend_full_dist ? 1.0 : 0.0 );
	const double desired_z = ( ( 1.0 - t )* start_z ) + ( t* goal_z );

	/** 
	*  Attempt lookup with the blended Z preference first.
	**/
	if ( Nav_FindNodeForPosition( mesh, position, desired_z, out_node, allow_fallback ) ) {
		return true;
	}

	/** 
	*  Fallback: retry with explicit start/goal Z values when allowed.
	**/
	if ( allow_fallback && start_z != desired_z ) {
		if ( Nav_FindNodeForPosition( mesh, position, start_z, out_node, true ) ) {
			return true;
		}
	}
	if ( allow_fallback && goal_z != desired_z ) {
		if ( Nav_FindNodeForPosition( mesh, position, goal_z, out_node, true ) ) {
			return true;
		}
	}

	return false;
}

/** 
*  @brief	Find a navigation node for a world-space position using BSP leaf lookup.
*  @param	mesh	Navigation mesh to query.
*  @param	position	World-space position to query.
*  @param	desired_z	Desired Z height for layer selection.
*  @param	out_node	[out] Output node reference.
*  @param	allow_fallback	Allow fallback search if direct lookup fails.
*  @return	True if a node was found, false otherwise.
**/
/** 
*  @brief    Attempt a canonical tile lookup with optional relaxed layer tolerance fallback.
*  @param    mesh          Navigation mesh owning the tile data.
*  @param    tile          Canonical world tile to query.
*  @param    tile_id       Index of the canonical tile in the mesh.
*  @param    leaf_index    Leaf index that should be stamped on the resolved node key.
*  @param    position      World-space position for the lookup.
*  @param    desired_z     Desired Z height for layer selection.
*  @param    out_node      [out] Resolved node reference when a match is found.
*  @param    allow_fallback When true, allow restoring the closest layer when the strict tolerance fails.
*  @return   True when a node was resolved inside the tile, false otherwise.
**/
static bool Nav_TryResolveNodeWithRelaxedLayer( const nav_mesh_t * mesh, const nav_tile_t * tile,
	const int32_t tile_id, const int32_t leaf_index, const Vector3 &position, double desired_z,
	nav_node_ref_t * out_node, bool allow_fallback ) {
	/** 
	*  Attempt strict layer resolution before relaxing tolerance for fallback-aware callers.
	**/
	if ( Nav_TryResolveNodeInTile( mesh, tile, tile_id, leaf_index, position, desired_z, out_node, false ) ) {
		return true;
	}

	/** 
	*  When allowed, retry with the closest-layer fallback so diagnostics can still capture the original failure.
	**/
	if ( allow_fallback ) {
		return Nav_TryResolveNodeInTile( mesh, tile, tile_id, leaf_index, position, desired_z, out_node, true );
	}

	return false;
}

const bool Nav_FindNodeForPosition( const nav_mesh_t * mesh, const Vector3 &position, double desired_z,
	nav_node_ref_t * out_node, bool allow_fallback ) {
	/** 
	*  Sanity checks: require mesh and output storage.
	**/
	if ( !mesh || !out_node ) {
		return false;
	}

	/** 
	*  Default to an unknown leaf index; leaf-specific data is optional for lookup.
	**/
	const int32_t leaf_index = -1;

	/** 
	*  Attempt a direct lookup via the canonical world tile map.
	**/
	const double tile_world_size = Nav_TileWorldSize( mesh );
	const int32_t tile_x = Nav_WorldToTileCoord( position[ 0 ], tile_world_size );
	const int32_t tile_y = Nav_WorldToTileCoord( position[ 1 ], tile_world_size );
	const nav_world_tile_key_t key = { .tile_x = tile_x, .tile_y = tile_y };
	auto it = mesh->world_tile_id_of.find( key );
 if ( it != mesh->world_tile_id_of.end() ) {
		const int32_t tile_id = it->second;
		const nav_tile_t * tile = &mesh->world_tiles[ tile_id ];
		if ( Nav_TryResolveNodeWithRelaxedLayer( mesh, tile, tile_id, leaf_index, position, desired_z, out_node, allow_fallback ) ) {
			return true;
		}
		// Primary tile lookup failed after both strict and relaxed attempts.
		if ( nav_debug_show_failed_lookups && nav_debug_show_failed_lookups->integer != 0 ) {
			gi.dprintf( "[NavDebug][Lookup] Nav_FindNodeForPosition: primary tile (%d,%d) present but resolve failed for pos=(%.1f %.1f %.1f) desired_z=%.1f\n",
				tile_x, tile_y, position.x, position.y, position.z, desired_z );
		}
	}

	/** 
	*  Fallback: probe neighboring tiles to handle leaf seam mismatches.
	**/
	if ( allow_fallback ) {
		for ( int32_t dy = -1; dy <= 1; dy++ ) {
			for ( int32_t dx = -1; dx <= 1; dx++ ) {
				// Skip the already-attempted center tile.
				if ( dx == 0 && dy == 0 ) {
					continue;
				}

				const nav_world_tile_key_t nb_key = { .tile_x = tile_x + dx, .tile_y = tile_y + dy };
				auto nb_it = mesh->world_tile_id_of.find( nb_key );
				if ( nb_it == mesh->world_tile_id_of.end() ) {
					continue;
				}

				const int32_t nb_tile_id = nb_it->second;
           const nav_tile_t * nb_tile = &mesh->world_tiles[ nb_tile_id ];
			if ( Nav_TryResolveNodeWithRelaxedLayer( mesh, nb_tile, nb_tile_id, leaf_index, position, desired_z, out_node, allow_fallback ) ) {
				return true;
			}
			}
		}
	}

	return false;
}

/** 
* 	@brief  Advance along a navigation path based on current origin and waypoint radius.
* 	@param  path            Navigation traversal path.
* 	@param  current_origin  Current 3D position of the mover.
* 	@param  waypoint_radius  Radius around waypoints to consider them "reached".
* 	@param  policy          Optional traversal policy for per-agent constraints.
* 	@param  inout_index     Input/output current waypoint index in the path.
* 	@param  out_direction    Output direction vector towards the current waypoint.
* 	@return True if there is a valid waypoint to move towards after advancement.
**/
const bool SVG_Nav_QueryMovementDirection_Advance2D_Output3D( const nav_traversal_path_t * path, const Vector3 &current_origin,
    double waypoint_radius, const svg_nav_path_policy_t * policy, int32_t * inout_index, Vector3 * out_direction ) {
    /** 
   * Sanity checks: validate inputs and ensure we have a usable path.
    **/
    if ( !path || !inout_index || !out_direction ) {
        return false;
    }
    if ( path->num_points <= 0 || !path->points ) {
        return false;
    }

    /** 
   * Clamp the waypoint radius to a minimum value.
   * This avoids degenerate behavior where micro radii prevent waypoint completion
   * when the mover cannot stand exactly on the path point due to collision.
    **/
    waypoint_radius = std::max( waypoint_radius, 8.0 );

    int32_t index = * inout_index;
    if ( index < 0 ) {
        index = 0;
    }

    const double waypoint_radius_sqr = waypoint_radius* waypoint_radius;

    // Advance using 2D distance so Z quantization / stairs don't prevent waypoint completion.
    while ( index < path->num_points ) {
        const Vector3 delta = QM_Vector3Subtract( path->points[ index ], current_origin );
        const double dist_sqr = ( delta[ 0 ]* delta[ 0 ] ) + ( delta[ 1 ]* delta[ 1 ] );
        if ( dist_sqr > waypoint_radius_sqr ) {
            break;
        }
        index++;
    }

    /** 
   * Stuck heuristic: if we are not making 2D progress towards the current waypoint,
   * advance it after a few frames. This helps around corners where the exact waypoint
   * position may be effectively unreachable due to collision/step resolution.
    * 
   * NOTE: This is intentionally conservative and only triggers when the waypoint is
   * still relatively close in 2D (within a few radii).
    **/
    if ( index < path->num_points ) {
        // Thread-local so multiple entities calling into this share no state across threads.
        // This code runs on the game thread, but `thread_local` also avoids cross-AI bleed.
        thread_local int32_t s_last_index = -1;
        thread_local Vector3 s_last_origin = {};
        thread_local double s_last_dist2d_sqr = 0.0;
        thread_local int32_t s_no_progress_frames = 0;
        thread_local int32_t s_last_frame = -1;

        // Reset tracking when switching paths/indices or on new server frame discontinuity.
        if ( s_last_frame != ( int32_t )level.frameNumber || s_last_index != index ) {
            s_last_frame = ( int32_t )level.frameNumber;
            s_last_index = index;
            s_last_origin = current_origin;
            const Vector3 d0 = QM_Vector3Subtract( path->points[ index ], current_origin );
            s_last_dist2d_sqr = ( d0[ 0 ]* d0[ 0 ] ) + ( d0[ 1 ]* d0[ 1 ] );
            s_no_progress_frames = 0;
        } else {
            // Compute current 2D distance to the waypoint.
            const Vector3 d1 = QM_Vector3Subtract( path->points[ index ], current_origin );
            const double dist2d_sqr = ( d1[ 0 ]* d1[ 0 ] ) + ( d1[ 1 ]* d1[ 1 ] );

            // Only consider this if we're already fairly close to the waypoint.
            const double near_sqr = waypoint_radius_sqr* 9.0; // within ~3 radii
            if ( dist2d_sqr <= near_sqr ) {
                // If we did not reduce distance (allowing a small epsilon), count a no-progress frame.
                const double improve_eps = 1.0;
                if ( dist2d_sqr >= ( s_last_dist2d_sqr - improve_eps ) ) {
                    s_no_progress_frames++;
                } else {
                    s_no_progress_frames = 0;
                }
                // If stuck for a short window, advance one waypoint to try to get around the corner.
                if ( s_no_progress_frames >= 6 && ( index + 1 ) < path->num_points ) {
                    index++;
                    s_no_progress_frames = 0;
                }
            }

            // Update tracking for next call.
            s_last_dist2d_sqr = dist2d_sqr;
            s_last_origin = current_origin;
            s_last_frame = ( int32_t )level.frameNumber;
        }
    }

    if ( index >= path->num_points ) {
        * inout_index = path->num_points;
        return false;
    }

    Vector3 direction = QM_Vector3Subtract( path->points[ index ], current_origin );

   /** 
	*  Clamp the vertical component using separate rise and drop limits.
	*      Upward steering should still respect the effective step limit, while downward steering
	*      should align with the active drop policy so follow-side motion does not ignore a stricter cap.
	**/
	if ( g_nav_mesh ) {
		// Determine the effective upward step limit (policy overrides mesh defaults when present).
		double stepLimit = g_nav_mesh->max_step;
		if ( policy && policy->max_step_height > 0.0 ) {
			stepLimit = policy->max_step_height;
		}

		// Determine the effective downward drop cap using the same preference order as path rebuild safety.
		double dropLimit = nav_max_drop_height_cap ? nav_max_drop_height_cap->value : 100.0f;
		if ( policy ) {
			if ( policy->max_drop_height_cap > 0.0 ) {
				dropLimit = policy->max_drop_height_cap;
			} else if ( policy->enable_max_drop_height_cap && policy->max_drop_height > 0.0 ) {
				dropLimit = policy->max_drop_height;
			}
		}

		// Keep a small quantization slack so waypoint directions remain stable on stairs and ramps.
		const double maxRiseDz = stepLimit + g_nav_mesh->z_quant;
		const double maxDropDz = std::max( dropLimit, g_nav_mesh->z_quant );
		if ( direction[ 2 ] > maxRiseDz ) {
			direction[ 2 ] = maxRiseDz;
		} else if ( direction[ 2 ] < -maxDropDz ) {
			direction[ 2 ] = -maxDropDz;
		}
	}

    const double length = ( double )QM_Vector3LengthDP( direction );
    if ( length <= std::numeric_limits<double>::epsilon() ) {
        return false;
    }

    * out_direction = QM_Vector3NormalizeDP( direction );
    * inout_index = index;
    return true;
}
