/********************************************************************
*
*
*	SVGame: Navigation Voxelmesh Generator - Implementation
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"

#include "svgame/nav/svg_nav.h"
#include "svgame/nav/svg_nav_clusters.h"
#include "svgame/nav/svg_nav_debug.h"
#include "svgame/nav/svg_nav_generate.h"
#include "svgame/nav/svg_nav_path_process.h"
#include "svgame/nav/svg_nav_traversal.h"

#include "svgame/entities/svg_player_edict.h"

// <Q2RTXP>: TODO: Move to shared? Ehh.. 
// Common BSP access for navigation generation.
#include "common/bsp.h"
#include "common/files.h"




/**
*
*
*
*   Navigation Constants:
*
*
*
**/


/**
*	Forward declarations for tile sizing helpers.
*	These are used by the cluster-graph helpers declared near the top of this TU.
**/
static inline double Nav_TileWorldSize( const nav_mesh_t *mesh );
static inline int32_t Nav_WorldToTileCoord( double value, double tile_world_size );

/**
*	Navigation Cluster Graph Routing:
*
*	Tile-route pre-pass can be either:
*		- Unweighted BFS (fewest tile hops)
*		- Weighted Dijkstra (flag-aware coarse costs)
*
*	The weighted mode keeps the hierarchical benefit but lets you bias away from
*	undesirable regions (water/lava/slime/stairs) before running fine A*.
**/
// Profiling/logging CVars
cvar_t *nav_profile_level = nullptr;
cvar_t *nav_log_file_enable = nullptr;
cvar_t *nav_log_file_name = nullptr;

/**
 *	@brief	Lightweight logging wrapper for navigation subsystem.
 *	@note	For now forwards to engine console printing; allows future redirection.
 */
/**
 *	@brief	Lightweight logging wrapper for navigation subsystem.
 *	@param	fmt printf-style format string.
 *	@note	Forwards to centralized Com_Printf so messages go through the global
 *		console/logging pipeline (and thus to the configured logfile). This keeps
 *		navigation logging consistent with the rest of the engine.
 */
void SVG_Nav_Log( const char *fmt, ... ) {
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
*	@brief	Extract tile XY from a world-space position.
*	@param	mesh	Navigation mesh.
*	@param	pos	World-space position.
*	@return	Tile key for the position.
**/
/**
*
*	Forward Declarations (Debug):
*
**/
/**
*   @brief  Refresh the runtime data for inline models (transforms only).
*           This updates the origin/angles of inline model runtime entries based on current entity state.
**/
void SVG_Nav_RefreshInlineModelRuntime( void );

/**
*	@brief	Performs a simple PMove-like step traversal test (3-trace).
*
*			This is intentionally conservative and is used only to validate edges in A*:
*			1) Try direct horizontal move.
*			2) If blocked, try stepping up <= max step, then horizontal move.
*			3) Trace down to find ground.
*
*	@return	True if the traversal is possible, false otherwise.
**/
const bool Nav_CanTraverseStep( const nav_mesh_t *mesh, const Vector3 &startPos, const Vector3 &endPos, const edict_ptr_t *clip_entity );
const bool Nav_CanTraverseStep_ExplicitBBox( const nav_mesh_t *mesh, const Vector3 &startPos, const Vector3 &endPos,
	const Vector3 &mins, const Vector3 &maxs, const edict_ptr_t *clip_entity, const svg_nav_path_policy_t *policy );

// Convenience overload: call explicit-bbox traversal test without a policy (defaults to mesh parameters).
static inline bool Nav_CanTraverseStep_ExplicitBBox( const nav_mesh_t *mesh, const Vector3 &startPos, const Vector3 &endPos,
	const Vector3 &mins, const Vector3 &maxs, const edict_ptr_t *clip_entity ) {
	return Nav_CanTraverseStep_ExplicitBBox( mesh, startPos, endPos, mins, maxs, clip_entity, nullptr );
}
// (Declaration present earlier) - definition follows below without a redundant default argument.



/**
*
*   CVars for Navigation Debug Drawing:
*
**/

//! Master on/off. 0 = off, 1 = on.
extern cvar_t *nav_debug_draw;
//! Only draw a specific BSP leaf index. -1 = any.
extern cvar_t *nav_debug_draw_leaf;
//! Only draw a specific tile (grid coords). Requires "x y". "*" disables.
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
*   Navigation Global Variables and CVars:
*
*
*
**/
/**
*	Navigation Debug CVar Variables:
**/
//cvar_t *nav_debug_draw_voxelmesh = nullptr;
//cvar_t *nav_debug_draw_walkable_sample_points = nullptr;
//cvar_t *nav_debug_draw_pathfinding = nullptr;

/**
*   @brief  Global navigation mesh instance (RAII owner).
*           Stores the complete navigation data for the current level.
*   @note   Automatically manages construction/destruction via RAII helper.
**/
nav_mesh_raii_t g_nav_mesh;

/**
*    @brief    Build a packed occupancy key from tile/cell/layer indices.
*/
static inline uint64_t Nav_OccupancyKey( const int32_t tileId, const int32_t cellIndex, const int32_t layerIndex ) {
	return ( ( ( uint64_t )tileId & 0xFFFF ) << 32 ) | ( ( ( uint64_t )cellIndex & 0xFFFF ) << 16 ) | ( ( uint64_t )layerIndex & 0xFFFF );
}

/**
*    @brief    Ensure occupancy map is stamped for the current frame.
*/
static inline void Nav_Occupancy_BeginFrame( nav_mesh_t *mesh ) {
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
*    @brief    Clear all dynamic occupancy records.
*/
void SVG_Nav_Occupancy_Clear( nav_mesh_t *mesh ) {
	/**
	*\tSanity: require mesh.
	*/
	if ( !mesh ) {
		return;
	}

	/**
	*\tReset occupancy counts.
	*/
	mesh->occupancy.clear();
	mesh->occupancy_frame = ( int32_t )level.frameNumber;
}

/**
*    @brief    Add occupancy for a tile/cell/layer.
*/
void SVG_Nav_Occupancy_Add( nav_mesh_t *mesh, int32_t tileId, int32_t cellIndex, int32_t layerIndex, int32_t cost, bool blocked ) {
	/**
	*\tSanity: require mesh and non-negative indices.
	*/
	if ( !mesh || tileId < 0 || cellIndex < 0 || layerIndex < 0 ) {
		return;
	}

	/**
	*\tRefresh occupancy for the current frame to avoid stale data.
	*/
	Nav_Occupancy_BeginFrame( mesh );

	/**
	*\tAccumulate soft-cost and hard-block flags.
	*/
	const uint64_t key = Nav_OccupancyKey( tileId, cellIndex, layerIndex );
	nav_occupancy_entry_t &entry = mesh->occupancy[ key ];
	entry.soft_cost += std::max( 1, cost );
	if ( blocked ) {
		entry.blocked = true;
	}
}

/**
 *    @brief    Query occupancy soft cost for a tile/cell/layer.
*/
int32_t SVG_Nav_Occupancy_SoftCost( const nav_mesh_t *mesh, int32_t tileId, int32_t cellIndex, int32_t layerIndex ) {
	/**
	*\tSanity: require mesh and valid indices.
	*/
	if ( !mesh || tileId < 0 || cellIndex < 0 || layerIndex < 0 ) {
		return 0;
	}

	/**
	*\tLookup occupancy soft cost.
	*/
	const uint64_t key = Nav_OccupancyKey( tileId, cellIndex, layerIndex );
	auto it = mesh->occupancy.find( key );
	if ( it == mesh->occupancy.end() ) {
		return 0;
	}

	return it->second.soft_cost;
}

/**
*    @brief    Query occupancy blocked flag for a tile/cell/layer.
*/
bool SVG_Nav_Occupancy_Blocked( const nav_mesh_t *mesh, int32_t tileId, int32_t cellIndex, int32_t layerIndex ) {
	if ( !mesh || tileId < 0 || cellIndex < 0 || layerIndex < 0 ) {
		return false;
	}

	const uint64_t key = Nav_OccupancyKey( tileId, cellIndex, layerIndex );
	auto it = mesh->occupancy.find( key );
	if ( it == mesh->occupancy.end() ) {
		return false;
	}

	return it->second.blocked;
}

/**
*   @brief  Navigation CVars for generation parameters.
**/
//! XY grid cell size in world units.
cvar_t *nav_cell_size_xy = nullptr;
//! Z-axis quantization step.
cvar_t *nav_z_quant = nullptr;
//! Number of cells per tile dimension.
cvar_t *nav_tile_size = nullptr;
//! Maximum step height (matches player PM_STEP_MAX_SIZE).
cvar_t *nav_max_step = nullptr;
//! Maximum allowed downward traversal drop.
cvar_t *nav_max_drop = nullptr;
	//! Cap applied when rejecting large drops during path validation.
cvar_t *nav_drop_cap = nullptr;
//! Maximum walkable slope in degrees.
cvar_t *nav_max_slope_deg = nullptr;
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
// Optional vertical tolerance (world units). If <= 0, auto-derived from mesh parameters.
cvar_t *nav_z_tolerance = nullptr;

// Runtime cost-tuning CVars for A* heuristics (defined here)
cvar_t *nav_cost_w_dist = nullptr;
cvar_t *nav_cost_jump_base = nullptr;
cvar_t *nav_cost_jump_height_weight = nullptr;
cvar_t *nav_cost_los_weight = nullptr;
cvar_t *nav_cost_dynamic_weight = nullptr;
cvar_t *nav_cost_failure_weight = nullptr;
cvar_t *nav_cost_failure_tau_ms = nullptr;
cvar_t *nav_cost_turn_weight = nullptr;
cvar_t *nav_cost_slope_weight = nullptr;
cvar_t *nav_cost_drop_weight = nullptr;
cvar_t *nav_cost_goal_z_blend_factor = nullptr;
cvar_t *nav_cost_min_cost_per_unit = nullptr;



/**
*
*
*
*
*	Utility Functions:
*
*
*
*
**/
/**
*   @brief  Set a presence bit for a cell index within a tile.
**/
static inline void Nav_SetPresenceBit( nav_tile_t *tile, int32_t cell_index ) {
	const int32_t word_index = cell_index >> 5;
	const int32_t bit_index = cell_index & 31;
	tile->presence_bits[ word_index ] |= ( 1u << bit_index );
}


/**
*
*
*
*   Navigation System Initialization/Shutdown:
*
*
*
**/
/**
*   @brief  Initialize navigation system and register cvars.
*           Called after entities have post-spawned to ensure inline models
*           have their proper modelindex set.
**/
void SVG_Nav_Init( void ) {
	/**
	*   Register grid and quantization CVars with sensible defaults:
	**/
	nav_cell_size_xy = gi.cvar( "nav_cell_size_xy", "4", 0 );
	nav_z_quant = gi.cvar( "nav_z_quant", "2", 0 );
	nav_tile_size = gi.cvar( "nav_tile_size", "8", 0 );

	// Tunable Z tolerance for layer selection and fallback. 0 = auto.
	nav_z_tolerance = gi.cvar( "nav_z_tolerance", "1", 0 );

	/**
	*   Register physics constraint CVars matching player movement:
	**/
	nav_max_step = gi.cvar( "nav_max_step", "18", 0 );
	nav_max_drop = gi.cvar( "nav_max_drop", "128", 0 );
	nav_max_slope_deg = gi.cvar( "nav_max_slope_deg", "70", 0 );

	/**
	*	Register physics constraints for specific actions.
	**/
	nav_drop_cap = gi.cvar( "nav_drop_cap", "128", 0 );
	// Drop cap used during path validation to reject excessive falls.
	nav_drop_cap = gi.cvar( "nav_drop_cap", "64", 0 );

	/**
	*   Register agent bounding box CVars:
	**/
	nav_agent_mins_x = gi.cvar( "nav_agent_mins_x", "-16", 0 );
	nav_agent_mins_y = gi.cvar( "nav_agent_mins_y", "-16", 0 );
	nav_agent_mins_z = gi.cvar( "nav_agent_mins_z", "-36", 0 );
	nav_agent_maxs_x = gi.cvar( "nav_agent_maxs_x", "16", 0 );
	nav_agent_maxs_y = gi.cvar( "nav_agent_maxs_y", "16", 0 );
	nav_agent_maxs_z = gi.cvar( "nav_agent_maxs_z", "36", 0 );

	/**
	*   Debug draw CVars:
	**/
	#if _DEBUG_NAV_MESH
	nav_debug_draw = gi.cvar( "nav_debug_draw", "1", 0 );
	nav_debug_draw_leaf = gi.cvar( "nav_debug_draw_leaf", "-1", 0 );
	nav_debug_draw_tile = gi.cvar( "nav_debug_draw_tile", "*", 0 );
	nav_debug_draw_max_segments = gi.cvar( "nav_debug_draw_max_segments", "4096", 0 );
	nav_debug_draw_max_dist = gi.cvar( "nav_debug_draw_max_dist", "1024", 0 );
	nav_debug_draw_tile_bounds = gi.cvar( "nav_debug_draw_tile_bounds", "0", 0 );
	nav_debug_draw_samples = gi.cvar( "nav_debug_draw_samples", "0", 0 );
	nav_debug_draw_path = gi.cvar( "nav_debug_draw_path", "1", 0 );
	// Temporary: enable diagnostic prints for failed nav lookups (tile/cell indices)
	nav_debug_show_failed_lookups = gi.cvar( "nav_debug_show_failed_lookups", "1", CVAR_CHEAT );
	#else
	nav_debug_draw = gi.cvar( "nav_debug_draw", "0", 0 );
	nav_debug_draw_leaf = gi.cvar( "nav_debug_draw_leaf", "-1", 0 );
	nav_debug_draw_tile = gi.cvar( "nav_debug_draw_tile", "*", 0 );
	nav_debug_draw_max_segments = gi.cvar( "nav_debug_draw_max_segments", "4096", 0 );
	nav_debug_draw_max_dist = gi.cvar( "nav_debug_draw_max_dist", "1024", 0 );
	nav_debug_draw_tile_bounds = gi.cvar( "nav_debug_draw_tile_bounds", "1", 0 );
	nav_debug_draw_samples = gi.cvar( "nav_debug_draw_samples", "1", 0 );
	nav_debug_draw_path = gi.cvar( "nav_debug_draw_path", "0", 0 );
	// Temporary: enable diagnostic prints for failed nav lookups (tile/cell indices)
	nav_debug_show_failed_lookups = gi.cvar( "nav_debug_show_failed_lookups", "0", 0 );
	#endif

	/**
	*    Register the reject debugging toggle.
	**/
	nav_debug_draw_rejects = gi.cvar( "nav_debug_draw_rejects", "0", CVAR_CHEAT );

	/**
	*	Cluster-route (tile graph) tuning CVars.
	*	These affect only the coarse pre-pass; fine A* still validates traversal.
	**/
	nav_cluster_route_weighted = gi.cvar( "nav_cluster_route_weighted", "1", 0 );
	// Per-flag penalties (added on top of base hop cost). Zero disables bias.
	nav_cluster_cost_stair = gi.cvar( "nav_cluster_cost_stair", "0", 0 );
	nav_cluster_cost_water = gi.cvar( "nav_cluster_cost_water", "2", 0 );
	nav_cluster_cost_lava = gi.cvar( "nav_cluster_cost_lava", "250", 0 );
	nav_cluster_cost_slime = gi.cvar( "nav_cluster_cost_slime", "8", 0 );
	// Per-flag hard exclusions (0 = allowed, 1 = forbidden).
	nav_cluster_forbid_stair = gi.cvar( "nav_cluster_forbid_stair", "0", 0 );
	nav_cluster_forbid_water = gi.cvar( "nav_cluster_forbid_water", "0", 0 );
	nav_cluster_forbid_lava = gi.cvar( "nav_cluster_forbid_lava", "0", 0 );
	nav_cluster_forbid_slime = gi.cvar( "nav_cluster_forbid_slime", "0", 0 );

	/**
	*    Cluster routing CVars.
	**/
	nav_cluster_enable = gi.cvar( "nav_cluster_enable", "1", 0 );
	nav_cluster_debug_draw = gi.cvar( "nav_cluster_debug_draw", "0", 0 );
	nav_cluster_debug_draw_graph = gi.cvar( "nav_cluster_debug_draw_graph", "0", 0 );

	// Profiling / logging control CVars
	nav_profile_level = gi.cvar( "nav_profile_level", "1", 0 ); // 0=off,1=phase,2=per-leaf,3=per-tile
	nav_log_file_enable = gi.cvar( "nav_log_file_enable", "0", 0 );
	nav_log_file_name = gi.cvar( "nav_log_file_name", "nav", 0 );

	/**
	*   Register runtime cost-tuning CVars for A* heuristics.
	*/
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

}

/**
*   @brief  Shutdown navigation system and free memory.
*           Called during game shutdown to clean up resources.
**/
void SVG_Nav_Shutdown( void ) {
	/**
	*    Cleanup: clear debug paths and free any loaded/generate mesh.
	**/
	NavDebug_ClearCachedPaths();
	SVG_Nav_FreeMesh();
}



/**
*
*
*
*   Navigation System "Voxel Mesh(-es)":
*
*
*
**/
/**
*   @brief  Free tile cell allocations (layers + arrays).
**/
static void Nav_FreeTileCells( nav_tile_t *tile, int32_t cells_per_tile ) {
	if ( !tile->cells ) {
		return;
	}

	for ( int32_t c = 0; c < cells_per_tile; c++ ) {
		if ( tile->cells[ c ].layers ) {
			gi.TagFree( tile->cells[ c ].layers );
		}
	}

	gi.TagFree( tile->cells );
	tile->cells = nullptr;
}

/**
*   @brief  Free the current navigation mesh.
*           Releases all memory allocated for navigation data using TAG_SVGAME_LEVEL.
**/
/**
*   @brief  Free the current navigation mesh.
*           Releases all memory allocated for navigation data using TAG_SVGAME_LEVEL.
**/
void SVG_Nav_FreeMesh( void ) {
	/**
	*	Early out if no mesh exists.
	**/
	if ( !g_nav_mesh ) {
		return;
	}

	/**
	*	Clear coarse routing graph: its pointers/indices become invalid once the mesh is freed.
	**/
	SVG_Nav_ClusterGraph_Clear();

	/**
	*	Clear dynamic occupancy map so unordered_map allocations are released before TagFree.
	**/
	SVG_Nav_Occupancy_Clear( g_nav_mesh.get() );

	/**
	*	Free canonical world tiles.
	**/
	{
		const int32_t cells_per_tile = g_nav_mesh->tile_size * g_nav_mesh->tile_size;
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
	*	Free per-leaf tile id arrays.
	**/
	if ( g_nav_mesh->leaf_data ) {
		for ( int32_t i = 0; i < g_nav_mesh->num_leafs; i++ ) {
			nav_leaf_data_t *leaf = &g_nav_mesh->leaf_data[ i ];
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
	*   Free inline model data:
	**/
	if ( g_nav_mesh->inline_model_data ) {
		// Iterate through all inline models.
		for ( int32_t i = 0; i < g_nav_mesh->num_inline_models; i++ ) {
			nav_inline_model_data_t *model = &g_nav_mesh->inline_model_data[ i ];

			// Free tiles in this model.
			if ( model->tiles ) {
				for ( int32_t t = 0; t < model->num_tiles; t++ ) {
					nav_tile_t *tile = &model->tiles[ t ];

					// Free presence bitset.
					if ( tile->presence_bits ) {
						gi.TagFree( tile->presence_bits );
					}

					// Free XY cells and their layers.
					if ( tile->cells ) {
						int32_t cells_per_tile = g_nav_mesh->tile_size * g_nav_mesh->tile_size;
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
	*	Free inline model runtime data:
	**/
	if ( g_nav_mesh->inline_model_runtime ) {
		gi.TagFree( g_nav_mesh->inline_model_runtime );
		g_nav_mesh->inline_model_runtime = nullptr;
		g_nav_mesh->num_inline_model_runtime = 0;
	}

	/**
	*	Reset the RAII owner, which will run the mesh destructor and free memory.
	*	This ensures STL members (vectors, maps) are properly destroyed.
	**/
	g_nav_mesh.reset();
}



/**
*
*
*
*   Navigation Helper Functions:
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
	const double default_slope = 70.0;

	profile.max_step_height = nav_max_step ? nav_max_step->value : default_step;
	profile.max_drop_height = nav_max_drop ? nav_max_drop->value : default_drop;
	profile.drop_cap = nav_drop_cap ? nav_drop_cap->value : profile.max_drop_height;
	if ( profile.drop_cap <= 0.0 ) {
		profile.drop_cap = default_drop_cap;
	}
	profile.max_slope_deg = nav_max_slope_deg ? nav_max_slope_deg->value : default_slope;

	return profile;
}
/**
*   @brief  Check if a surface normal is walkable based on slope.
*   @param  normal          Surface normal vector.
*   @param  max_slope_deg   Maximum walkable slope in degrees.
*   @return True if the slope is walkable, false otherwise.
**/
const bool IsWalkableSlope( const Vector3 &normal, double max_slope_deg ) {
	// Convert max_slope_deg to minimum normal Z component.
	double min_normal_z = cosf( max_slope_deg * DEG_TO_RAD );
	// Surface is walkable if normal Z is above the threshold.
	return normal[ 2 ] >= min_normal_z;
}

/**
*   @brief  Detect content flags from trace results.
*   @param  trace   Trace result containing content information.
*   @return Content flags (nav_layer_flags_t) for the traced surface.
**/
/**
*    Legacy voxelmesh generation helpers:
*        These functions are kept for reference only. The authoritative
*        generation implementation lives in `svg_nav_generate.cpp`.
*        Define `SVG_NAV_LEGACY_GENERATION` to compile these helpers.
**/
#if defined( SVG_NAV_LEGACY_GENERATION )
static uint8_t DetectContentFlags( const cm_trace_t &trace ) {
	// Start with walkable flag.
	uint8_t flags = NAV_FLAG_WALKABLE;

	// Check for water based on trace contents.
	if ( trace.contents & CONTENTS_WATER ) {
		flags |= NAV_FLAG_WATER;
	}
	// Check for lava based on trace contents.
	if ( trace.contents & CONTENTS_LAVA ) {
		flags |= NAV_FLAG_LAVA;
	}
	// Check for slime based on trace contents.
	if ( trace.contents & CONTENTS_SLIME ) {
		flags |= NAV_FLAG_SLIME;
	}
	// Check for ladder based on trace contents.
	if ( trace.contents & CONTENTS_LADDER ) {
		flags |= NAV_FLAG_LADDER;
	}

	return flags;
}

/**
 *   @brief  Deprecated helper for voxelmesh generation; use svg_nav_generate.cpp instead.
 *   @note   Performs downward traces to find walkable layers at an XY position.
 *   @param  xy_pos          XY position to trace at.
 *   @param  mins            Agent bounding box minimum.
 *   @param  maxs            Agent bounding box maximum.
 *   @param  z_min           Minimum Z height to search.
 *   @param  z_max           Maximum Z height to search.
 *   @param  max_step        Maximum step height for downward traces.
 *   @param  max_slope_deg   Maximum walkable slope in degrees.
 *   @param  z_quant         Z-axis quantization step.
 *   @param  out_layers      Output array of detected layers.
 *   @param  out_num_layers  Output number of layers found.
 *   @param  clip_entity     Optional entity to clip against (inline model), null for world.
 *   @note   This function is not thread-safe due to the static temp_layers array.
 *           It should only be called from a single thread (main game thread).
 **/
// Diagnostic counters for FindWalkableLayers
static int32_t s_precheck_fail_count = 0;
static int32_t s_trace_attempt_count = 0;
static int32_t s_trace_hit_count = 0;
static int32_t s_slope_reject_count = 0;

[[deprecated( "Use svg_nav_generate.cpp sampling helpers instead of FindWalkableLayers." )]]
static void FindWalkableLayers( const Vector3 &xy_pos, const Vector3 &mins, const Vector3 &maxs,
	double z_min, double z_max, double max_step, double max_slope_deg, double z_quant,
	nav_layer_t **out_layers, int32_t *out_num_layers,
	const edict_ptr_t *clip_entity ) {
// Maximum number of layers that can be found at a single XY position.
	const int32_t MAX_LAYERS = 16;
	// Note: Static array for efficiency, but limits thread-safety.
	static nav_layer_t temp_layers[ MAX_LAYERS ] = {};
	int32_t num_layers = 0;

	/**
	*	Fast precheck: if the entire XY column is empty (no solid content) within the
	*	requested Z slice, we can skip the expensive downward trace loop.
	*
	*	Note: This intentionally checks a conservative AABB that covers the search range.
	*	If any solid exists in the column we fall back to the trace-based sampler to
	*	recover accurate endpos + plane normal.
	**/
	{
		// Build an AABB spanning the vertical search range around this XY.
		Vector3 colMins = xy_pos;
		Vector3 colMaxs = xy_pos;
		colMins[ 2 ] = z_min;
		colMaxs[ 2 ] = z_max;

		// DO NOT expand by agent hull - z_min/z_max already include agent bbox offset.
		// Expanding XY slightly for conservative edge detection.
		colMins[ 0 ] += mins[ 0 ];
		colMins[ 1 ] += mins[ 1 ];
		colMaxs[ 0 ] += maxs[ 0 ];
		colMaxs[ 1 ] += maxs[ 1 ];

		bool hasSolid = true;
		if ( clip_entity ) {
			/**
			*	Inline model: prefer CM_BoxContents against the model BSP headnode when available.
			*	This avoids even the single clip trace.
			**/
			const char *modelStr = clip_entity->model;
			int32_t inlineIndex = -1;
			if ( modelStr && modelStr[ 0 ] == '*' ) {
				inlineIndex = atoi( modelStr + 1 );
			}

			mnode_t *headnode = nullptr;
			if ( inlineIndex >= 0 && gi.CM_InlineModelHeadnode ) {
				headnode = gi.CM_InlineModelHeadnode( inlineIndex );
			}

			if ( headnode ) {
				cm_contents_t contents = CONTENTS_NONE;
				gi.CM_BoxContents_headnode( &colMins[ 0 ], &colMaxs[ 0 ], &contents, nullptr, 0, headnode, nullptr );
				hasSolid = ( contents & CM_CONTENTMASK_SOLID ) != 0;
			} else {
				// Fallback: one conservative clip trace through the column.
				Vector3 start = xy_pos;
				start[ 2 ] = z_max;
				Vector3 end = xy_pos;
				end[ 2 ] = z_min;
				const cm_trace_t tr = gi.clip( clip_entity, &start, &mins, &maxs, &end, CM_CONTENTMASK_SOLID );
				hasSolid = ( tr.fraction < 1.0f ) || tr.startsolid || tr.allsolid;
			}
		} else {
			// World: a cheap contents query for this agent-shaped AABB.
			cm_contents_t contents = CONTENTS_NONE;
			gi.CM_BoxContents( &colMins[ 0 ], &colMaxs[ 0 ], &contents, nullptr, 0, nullptr );
			hasSolid = ( contents & CM_CONTENTMASK_SOLID ) != 0;
		}

		if ( !hasSolid ) {
			s_precheck_fail_count++;
			if ( s_precheck_fail_count <= 10 ) {
				gi.dprintf( "[ERROR][NavGen][FindWalkableLayers] Precheck failed: no solid in column at (%.1f,%.1f) z=[%.1f,%.1f]\n",
					xy_pos[ 0 ], xy_pos[ 1 ], z_min, z_max );
			}
			*out_layers = nullptr;
			*out_num_layers = 0;
			return;
		}
	}

	// Start at maximum Z and work downward.
	double current_z = z_max;
	// Search distance for each downward trace (2x max step).
	double step_down = max_step * 2.0f;

	/**
	*   Perform repeated downward traces to find all walkable layers:
	**/
	while ( current_z > z_min && num_layers < MAX_LAYERS ) {
		// Setup trace start position at current Z height.
		Vector3 start = xy_pos;
		start[ 2 ] = current_z;

		// Setup trace end position stepping down.
		Vector3 end = xy_pos;
		end[ 2 ] = current_z - step_down;

		// Perform the trace (world-only or full collision).
		cm_trace_t trace;
		if ( clip_entity ) {
			// Inline model collision using the entity-specific clip.
			trace = gi.clip( clip_entity, &start, &mins, &maxs, &end, CM_CONTENTMASK_SOLID );
		} else {
			// World-only collision for static world geometry.
			trace = gi.trace( &start, &mins, &maxs, &end, nullptr, CM_CONTENTMASK_SOLID );
		}

		s_trace_attempt_count++;

		/**
		*   Process trace results:
		**/
		// Check if we hit something and if the normal points upward.
		if ( trace.fraction < 1.0f && trace.plane.normal[ 2 ] > 0.0f ) {
			s_trace_hit_count++;
			// Check if the slope is walkable.
			if ( IsWalkableSlope( trace.plane.normal, max_slope_deg ) ) {
				// Found a walkable surface - record it.
				// Note: Assumes z_quant is non-zero (validated during generation).
				temp_layers[ num_layers ].z_quantized = ( int16_t )( trace.endpos[ 2 ] / z_quant );
				temp_layers[ num_layers ].flags = DetectContentFlags( trace );
				temp_layers[ num_layers ].clearance = 0; // TODO: Calculate clearance.
				num_layers++;

				// Continue searching below this surface.
				current_z = trace.endpos[ 2 ] - 1.0f;
			} else {
				// Hit a non-walkable surface (too steep), continue searching below.
				s_slope_reject_count++;
				if ( s_slope_reject_count <= 10 ) {
					gi.dprintf( "[WARNING][NavGen][FindWalkableLayers] Slope rejected: normal.z=%.3f at (%.1f,%.1f,%.1f) max_slope=%.1f\n",
						trace.plane.normal[ 2 ], xy_pos[ 0 ], xy_pos[ 1 ], trace.endpos[ 2 ], max_slope_deg );
				}
				current_z = trace.endpos[ 2 ] - 1.0f;
			}
		} else {
			// No hit in this trace, move down by the step distance.
			current_z -= step_down;
		}
	}

	/**
	*   Allocate and return layer data:
	**/
	if ( num_layers > 0 ) {
		// Allocate permanent storage for the layers.
		*out_layers = ( nav_layer_t * )gi.TagMallocz( sizeof( nav_layer_t ) * num_layers, TAG_SVGAME_LEVEL );
		memcpy( *out_layers, temp_layers, sizeof( nav_layer_t ) * num_layers );
		*out_num_layers = num_layers;
	} else {
		// No walkable layers found at this XY position.
		*out_layers = nullptr;
		*out_num_layers = 0;
	}
}



/**
*
*
*
*   Navigation Tile Generation:
*
*
*
**/
/**
 *   @brief  Deprecated helper for legacy mesh generation; prefer svg_nav_generate.
 *   @note   Builds a navigation tile by sampling walkable layers within the BSP leaf bounds.
 *   @return True if any walkable data was generated.
 **/
[[deprecated( "Use GenerateWorldMesh helpers in svg_nav_generate.cpp instead of Nav_BuildTile." )]]
static bool Nav_BuildTile( nav_mesh_t *mesh, nav_tile_t *tile, const Vector3 &leaf_mins, const Vector3 &leaf_maxs,
	double z_min, double z_max ) {
	const int32_t cells_per_tile = mesh->tile_size * mesh->tile_size;
	const int32_t presence_words = ( cells_per_tile + 31 ) / 32;
	const double tile_world_size = Nav_TileWorldSize( mesh );

	// Allocate tile storage.
	tile->presence_bits = ( uint32_t * )gi.TagMallocz( sizeof( uint32_t ) * presence_words, TAG_SVGAME_LEVEL );
	tile->cells = ( nav_xy_cell_t * )gi.TagMallocz( sizeof( nav_xy_cell_t ) * cells_per_tile, TAG_SVGAME_LEVEL );

	bool has_layers = false;

	const double tile_origin_x = tile->tile_x * tile_world_size;
	const double tile_origin_y = tile->tile_y * tile_world_size;

	for ( int32_t cell_y = 0; cell_y < mesh->tile_size; cell_y++ ) {
		for ( int32_t cell_x = 0; cell_x < mesh->tile_size; cell_x++ ) {
			const double world_x = tile_origin_x + ( ( double )cell_x + 0.5f ) * mesh->cell_size_xy;
			const double world_y = tile_origin_y + ( ( double )cell_y + 0.5f ) * mesh->cell_size_xy;

			// Skip cells outside the leaf bounds.
			if ( world_x < leaf_mins[ 0 ] || world_x > leaf_maxs[ 0 ] ||
				world_y < leaf_mins[ 1 ] || world_y > leaf_maxs[ 1 ] ) {
				continue;
			}

			Vector3 xy_pos = { world_x, world_y, 0.0 };
			nav_layer_t *layers = nullptr;
			int32_t num_layers = 0;

			FindWalkableLayers( xy_pos, mesh->agent_mins, mesh->agent_maxs,
				z_min, z_max, mesh->max_step, mesh->max_slope_deg, mesh->z_quant,
				&layers, &num_layers, nullptr );

			if ( num_layers > 0 ) {
				const int32_t cell_index = cell_y * mesh->tile_size + cell_x;
				tile->cells[ cell_index ].num_layers = num_layers;
				tile->cells[ cell_index ].layers = layers;
				Nav_SetPresenceBit( tile, cell_index );
				has_layers = true;
			}
		}
	}

	if ( !has_layers ) {
		Nav_FreeTileCells( tile, cells_per_tile );
		if ( tile->presence_bits ) {
			gi.TagFree( tile->presence_bits );
		}
	}

	return has_layers;
}

/**
*	@brief	Build a navigation tile for an inline model using entity clip collision.
*	@return	True if any walkable data was generated.
**/
static bool Nav_BuildInlineTile( nav_mesh_t *mesh, nav_tile_t *tile, const Vector3 &model_mins, const Vector3 &model_maxs,
	double z_min, double z_max, const edict_ptr_t *clip_entity ) {
	const int32_t cells_per_tile = mesh->tile_size * mesh->tile_size;
	const int32_t presence_words = ( cells_per_tile + 31 ) / 32;
	const double tile_world_size = Nav_TileWorldSize( mesh );

	// Allocate tile storage.
	tile->presence_bits = ( uint32_t * )gi.TagMallocz( sizeof( uint32_t ) * presence_words, TAG_SVGAME_LEVEL );
	tile->cells = ( nav_xy_cell_t * )gi.TagMallocz( sizeof( nav_xy_cell_t ) * cells_per_tile, TAG_SVGAME_LEVEL );

	bool has_layers = false;

	// Tile origins are in "model local space". Allow negative coords by using local mins as base.
	const double tile_origin_x = model_mins[ 0 ] + ( tile->tile_x * tile_world_size );
	const double tile_origin_y = model_mins[ 1 ] + ( tile->tile_y * tile_world_size );

	for ( int32_t cell_y = 0; cell_y < mesh->tile_size; cell_y++ ) {
		for ( int32_t cell_x = 0; cell_x < mesh->tile_size; cell_x++ ) {
			const double local_x = tile_origin_x + ( ( double )cell_x + 0.5 ) * mesh->cell_size_xy;
			const double local_y = tile_origin_y + ( ( double )cell_y + 0.5 ) * mesh->cell_size_xy;

			// Skip cells outside the model AABB.
			if ( local_x < model_mins[ 0 ] || local_x > model_maxs[ 0 ] ||
				local_y < model_mins[ 1 ] || local_y > model_maxs[ 1 ] ) {
				continue;
			}

			Vector3 xy_pos = { local_x, local_y, 0.0 };
			nav_layer_t *layers = nullptr;
			int32_t num_layers = 0;

			FindWalkableLayers( xy_pos, mesh->agent_mins, mesh->agent_maxs,
				z_min, z_max, mesh->max_step, mesh->max_slope_deg, mesh->z_quant,
				&layers, &num_layers, clip_entity );

			if ( num_layers > 0 ) {
				const int32_t cell_index = cell_y * mesh->tile_size + cell_x;
				tile->cells[ cell_index ].num_layers = num_layers;
				tile->cells[ cell_index ].layers = layers;
				Nav_SetPresenceBit( tile, cell_index );
				has_layers = true;
			}
		}
	}

	if ( !has_layers ) {
		Nav_FreeTileCells( tile, cells_per_tile );

		if ( tile->presence_bits ) {
			gi.TagFree( tile->presence_bits );
			tile->presence_bits = nullptr;
		}
	}

	return has_layers;
}



/**
*
*
*
*   Inline BSP model Mesh Generation:
*
*
*
**/



/**
*
*
*
*   Navigation Mesh Generation:
*
*
*
**/
/**
 *   @brief  Deprecated legacy world mesh generator; use svg_nav_generate.cpp instead.
 *           Creates tiles for each BSP leaf containing walkable surface samples.
 *   @param  mesh    Navigation mesh structure to populate.
 *   @note   Only compiled when SVG_NAV_LEGACY_GENERATION is enabled; new generation lives in svg_nav_generate.cpp.
 **/
[[deprecated( "Replace GenerateWorldMesh with the canonical svg_nav_generate.cpp implementation." )]]
static void GenerateWorldMesh( nav_mesh_t *mesh ) {
	/**
	*   Get BSP data from the world model:
	**/
	const cm_t *world_model = gi.GetCollisionModel();
	if ( !world_model || !world_model->cache ) {
		gi.cprintf( nullptr, PRINT_HIGH, "Failed to get world BSP data\n" );
		return;
	}
	// Get the pointer to the cached BSP data.    
	bsp_t *bsp = world_model->cache;
	// Get number of leafs in the BSP.
	int32_t num_leafs = bsp->numleafs;

	SVG_Nav_Log( "Generating world navmesh for %d leafs...\n", num_leafs );

	/**
	*   Allocate leaf data array:
	**/
	mesh->num_leafs = num_leafs;
	mesh->leaf_data = ( nav_leaf_data_t * )gi.TagMallocz( sizeof( nav_leaf_data_t ) * num_leafs, TAG_SVGAME_LEVEL );

	const double tile_world_size = Nav_TileWorldSize( mesh );

	/**
	*	Compute a global world Z sampling range from bsp model 0 bounds.
	*
	*	We keep XY tiling based on per-leaf bounds (for clustering and locality), but
	*	use a consistent vertical range derived from the world model to avoid cases
	*	where leaf Z extents are invalid/unhelpful for navigation sampling.
	**/
	const Vector3 world_mins = bsp->models[ 0 ].mins;
	const Vector3 world_maxs = bsp->models[ 0 ].maxs;
	const double world_z_min = ( double )world_mins[ 2 ] + ( double )mesh->agent_mins[ 2 ];
	const double world_z_max = ( double )world_maxs[ 2 ] + ( double )mesh->agent_maxs[ 2 ];

	/**
	*   Generate tiles for each leaf using BSP bounds:
	**/
	int32_t leaf_skipped_solid = 0;
	int32_t leaf_processed = 0;
	int32_t total_tile_attempts = 0;
	int32_t total_tile_success = 0;

	/**
	*	Legacy note:
	*
	*	World voxelmesh generation is implemented in `svg_nav_generate.cpp` and now uses
	*	canonical world tile storage (`mesh->world_tiles`) with leaf tile-id references.
	*
	*	This function remains only to print the older stats if it is still invoked in
	*	any legacy code paths.
	**/
	leaf_skipped_solid = leaf_skipped_solid;
	leaf_processed = leaf_processed;
	total_tile_attempts = total_tile_attempts;
	total_tile_success = total_tile_success;

	SVG_Nav_Log( "World mesh generation complete\n" );
	SVG_Nav_Log( "[NavGen] Leaf statistics: %d total, %d solid (skipped), %d processed\n", num_leafs, leaf_skipped_solid, leaf_processed );
	SVG_Nav_Log( "[NavGen] Tile statistics: %d attempted, %d succeeded (%.1f%% success rate)\n", total_tile_attempts, total_tile_success, total_tile_attempts > 0 ? ( 100.0f * total_tile_success / total_tile_attempts ) : 0.0f );
	if ( total_tile_success == 0 && total_tile_attempts > 0 ) {
		SVG_Nav_Log( "[ERROR][NavGen] ALL TILES FAILED! Possible causes:\n" );
		SVG_Nav_Log( "[ERROR][NavGen]   1. All surfaces too steep (check nav_max_slope_deg=%.1f)\n", mesh->max_slope_deg );
		SVG_Nav_Log( "[ERROR][NavGen]   2. Map has no valid walkable floors\n" );
		SVG_Nav_Log( "[ERROR][NavGen]   3. Agent bounding box too large for map geometry\n" );
		SVG_Nav_Log( "[ERROR][NavGen]   4. Bug in Nav_BuildTile surface sampling\n" );
		SVG_Nav_Log( "[ERROR][NavGen] See first 10 diagnostic messages above for details.\n" );
	}
}

/**
*   @brief  Generate navigation mesh for inline BSP models (brush entities).
*           Creates tiles for each inline model in local space for later transform application.
*   @param  mesh    Navigation mesh structure to populate.
**/
static void GenerateInlineModelMesh( nav_mesh_t *mesh ) {
	/**
	*   Get number of inline models from world model:
	**/
	const cm_t *world_model = gi.GetCollisionModel();
	if ( !world_model || !world_model->cache ) {
		gi.cprintf( nullptr, PRINT_HIGH, "Failed to get world BSP data\n" );
		return;
	}

	std::unordered_map<int32_t, svg_base_edict_t *> model_to_ent;
	Nav_CollectInlineModelEntities( model_to_ent );

	// Nothing to do if no brush entities are present/active.
	if ( model_to_ent.empty() ) {
		mesh->num_inline_models = 0;
		mesh->inline_model_data = nullptr;
		mesh->num_inline_model_runtime = 0;
		mesh->inline_model_runtime = nullptr;
		return;
	}

	gi.cprintf( nullptr, PRINT_HIGH, "Generating inline model navmesh' for %d inline models...\n", ( int32_t )model_to_ent.size() );

	// Allocate only for inline models that are actually referenced by entities.
	mesh->num_inline_models = ( int32_t )model_to_ent.size();
	mesh->inline_model_data = ( nav_inline_model_data_t * )gi.TagMallocz(
		sizeof( nav_inline_model_data_t ) * mesh->num_inline_models, TAG_SVGAME_LEVEL );

	// Build runtime mapping (transforms).
	Nav_BuildInlineModelRuntime( mesh, model_to_ent );

	const double tile_world_size = Nav_TileWorldSize( mesh );

	int32_t out_index = 0;
	for ( const auto &it : model_to_ent ) {
		const int32_t model_index = it.first;
		svg_base_edict_t *ent = it.second;

		nav_inline_model_data_t *out_model = &mesh->inline_model_data[ out_index ];
		out_model->model_index = model_index;
		out_model->num_tiles = 0;
		out_model->tiles = nullptr;

		if ( !ent ) {
			out_index++;
			continue;
		}

		const char *model_name = ent->model;
		const mmodel_t *mm = ( model_name ? gi.GetInlineModelDataForName( model_name ) : nullptr );
		if ( !mm ) {
			out_index++;
			continue;
		}

		const Vector3 model_mins = mm->mins;
		const Vector3 model_maxs = mm->maxs;

		const double z_min = model_mins[ 2 ] + mesh->agent_mins[ 2 ];
		const double z_max = model_maxs[ 2 ] + mesh->agent_maxs[ 2 ];

		// Compute tile range in model-local space with mins as the origin.
		const int32_t tile_min_x = 0;
		const int32_t tile_min_y = 0;
		const int32_t tile_max_x = Nav_WorldToTileCoord( ( model_maxs[ 0 ] - model_mins[ 0 ] ) - NAV_TILE_EPSILON, tile_world_size );
		const int32_t tile_max_y = Nav_WorldToTileCoord( ( model_maxs[ 1 ] - model_mins[ 1 ] ) - NAV_TILE_EPSILON, tile_world_size );

		std::vector<nav_tile_t> tiles;
		if ( tile_max_x >= tile_min_x && tile_max_y >= tile_min_y ) {
			const int32_t tile_count = ( tile_max_x - tile_min_x + 1 ) * ( tile_max_y - tile_min_y + 1 );
			tiles.reserve( tile_count );
		}

		for ( int32_t tile_y = tile_min_y; tile_y <= tile_max_y; tile_y++ ) {
			for ( int32_t tile_x = tile_min_x; tile_x <= tile_max_x; tile_x++ ) {
				nav_tile_t tile = {};
				tile.tile_x = tile_x;
				tile.tile_y = tile_y;

				if ( Nav_BuildInlineTile( mesh, &tile, model_mins, model_maxs, z_min, z_max, ent ) ) {
					tiles.push_back( tile );
				}
			}
		}

		if ( !tiles.empty() ) {
			out_model->num_tiles = ( int32_t )tiles.size();
			out_model->tiles = ( nav_tile_t * )gi.TagMallocz( sizeof( nav_tile_t ) * out_model->num_tiles, TAG_SVGAME_LEVEL );
			memcpy( out_model->tiles, tiles.data(), sizeof( nav_tile_t ) * out_model->num_tiles );
		}

		out_index++;
	}

	gi.cprintf( nullptr, PRINT_HIGH, "Inline models' navmesh generation complete\n" );
}
#endif // SVG_NAV_LEGACY_GENERATION

/**
*	@brief	Build runtime inline model data for navigation mesh.
*	@note	This allocates and initializes the runtime array. Per-frame updates should call
*			`SVG_Nav_RefreshInlineModelRuntime()` (no allocations).
**/
static void Nav_BuildInlineModelRuntime( nav_mesh_t *mesh, const std::unordered_map<int32_t, svg_base_edict_t *> &model_to_ent ) {
	/**
	*	Sanity checks / early outs.
	**/
	if ( !mesh ) {
		return;
	}

	/**
	*	Reset any existing runtime mapping and its lookup table.
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
	mesh->inline_model_runtime = ( nav_inline_model_runtime_t * )gi.TagMallocz(
		sizeof( nav_inline_model_runtime_t ) * mesh->num_inline_model_runtime, TAG_SVGAME_LEVEL );

	/**
	*	Reserve the lookup map up-front so inserts stay cheap.
	**/
	mesh->inline_model_runtime_index_of.reserve( ( size_t )mesh->num_inline_model_runtime );

	int32_t out_index = 0;
	for ( const auto &it : model_to_ent ) {
		const int32_t model_index = it.first;
		svg_base_edict_t *ent = it.second;

		nav_inline_model_runtime_t &rt = mesh->inline_model_runtime[ out_index ];
		rt.model_index = model_index;
		rt.owner_ent = ent;
		rt.owner_entnum = ent ? ent->s.number : 0;

		rt.origin = ent ? ent->currentOrigin : Vector3{};
		rt.angles = ent ? ent->currentAngles : Vector3{};
		rt.dirty = false;

		/**
		*	Populate owner_entnum -> runtime index map.
		*	We only emit entries for valid entity numbers.
		**/
		if ( rt.owner_entnum > 0 ) {
			mesh->inline_model_runtime_index_of.emplace( rt.owner_entnum, out_index );
		}

		out_index++;
	}
}

/**
*	@brief	Lookup an inline-model runtime entry by owning entity number.
*	@param	mesh	Navigation mesh.
*	@param	owner_entnum	Owning entity number (edict->s.number).
*	@return	Pointer to runtime entry if found, otherwise nullptr.
*	@note	Uses the cached `inline_model_runtime_index_of` map to avoid linear scans.
**/
const nav_inline_model_runtime_t *SVG_Nav_GetInlineModelRuntimeForOwnerEntNum( const nav_mesh_t *mesh, const int32_t owner_entnum ) {
	/**
	*	Sanity checks: validate mesh and runtime availability.
	**/
	if ( !mesh || owner_entnum <= 0 ) {
		return nullptr;
	}
	if ( !mesh->inline_model_runtime || mesh->num_inline_model_runtime <= 0 ) {
		return nullptr;
	}

	/**
	*	Resolve runtime index via the prebuilt lookup map.
	**/
	auto it = mesh->inline_model_runtime_index_of.find( owner_entnum );
	if ( it == mesh->inline_model_runtime_index_of.end() ) {
		return nullptr;
	}

	/**
	*	Bounds-check the resolved index before returning.
	**/
	const int32_t idx = it->second;
	if ( idx < 0 || idx >= mesh->num_inline_model_runtime ) {
		return nullptr;
	}

	return &mesh->inline_model_runtime[ idx ];
}

/**
*	@brief	Dump the inline-model runtime index map for debugging.
*	@param	mesh	Navigation mesh.
*	@note	Prints to console; intended for developer diagnostics.
**/
void SVG_Nav_Debug_PrintInlineModelRuntimeIndexMap( const nav_mesh_t *mesh ) {
	/**
	*	Sanity: validate mesh and ensure we have something to dump.
	**/
	if ( !mesh ) {
		gi.dprintf( "%s: mesh == nullptr\n", __func__ );
		return;
	}

	gi.dprintf( "[NavDebug] inline_model_runtime_index_of: %d entries (runtime_count=%d)\n",
		( int32_t )mesh->inline_model_runtime_index_of.size(), mesh->num_inline_model_runtime );

	/**
	*	Iterate the map and emit each mapping as owner_entnum -> runtime index.
	**/
	for ( const auto &kv : mesh->inline_model_runtime_index_of ) {
		const int32_t ownerEntNum = kv.first;
		const int32_t runtimeIndex = kv.second;
		gi.dprintf( "[NavDebug]   owner_entnum=%d -> runtime_index=%d\n", ownerEntNum, runtimeIndex );
	}
}

/**
*   @brief  Refresh the runtime data for inline models (transforms only).
*           This updates the origin/angles of inline model runtime entries based on current entity state.
**/
void SVG_Nav_RefreshInlineModelRuntime( void ) {
	if ( !g_nav_mesh || !g_nav_mesh->inline_model_runtime || g_nav_mesh->num_inline_model_runtime <= 0 ) {
		return;
	}

	for ( int32_t i = 0; i < g_nav_mesh->num_inline_model_runtime; i++ ) {
		nav_inline_model_runtime_t &rt = g_nav_mesh->inline_model_runtime[ i ];
		svg_base_edict_t *ent = rt.owner_ent;

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
*   Navigation Pathfinding Node Methods:
*
*
*
**/
/**
*	@brief	Select a navigation layer index from an XY cell for a desired world-space Z.
*	@param	mesh			Navigation mesh (provides `z_quant` and default step/tolerance inputs).
*	@param	cell			Navigation XY cell to select a layer from.
*	@param	desired_z	Desired world-space Z height to match (units).
*	@param	out_layer_index	[out] Selected layer index inside `cell->layers`.
*	@return	True if a suitable layer index was found.
*	@note	This selection is intentionally conservative and deterministic:
*			- First selects the closest layer within the Z tolerance gate.
*			- When the desired Z is within a small preference threshold of any layer,
*			  applies a top/bottom preference (defaults to top-most).
*			- If nothing is within tolerance, falls back to closest-by-Z.
*			Actual movement feasibility is validated later by step tests (e.g.,
*			`Nav_CanTraverseStep_ExplicitBBox`) when building/expanding paths.
**/
const bool Nav_SelectLayerIndex( const nav_mesh_t *mesh, const nav_xy_cell_t *cell, double desired_z,
	int32_t *out_layer_index ) {
	/**
	*    Sanity checks.
	**/
	if ( !mesh || !cell || cell->num_layers <= 0 || !out_layer_index ) {
		return false;
	}

	/**
	*	Z tolerance gate:
	*		Only layers within this tolerance are considered candidates for the primary selection.
	*		This reduces Z-jitter in multi-layer cells and keeps selection aligned with
	*		step capabilities.
	**/
	double z_tolerance = 0.0;
	if ( nav_z_tolerance && nav_z_tolerance->value > 0.0f ) {
		// Use explicit tuning when provided (world units).
		z_tolerance = nav_z_tolerance->value;
	} else {
		// Default: allow up to a step plus half a quant to handle common stair sampling.
		z_tolerance = mesh->max_step + ( mesh->z_quant * 0.5f );
	}
	const double selectTolerance = z_tolerance;

	/**
	*	Layer selection preference policy:
	*		The selection bias is currently derived from the default path policy.
	*		This keeps selection behavior consistent across the nav module without adding
	*		new cvars.
	*
	*	@note	The preference is only applied when the desired Z is within
	*			`layer_select_prefer_z_threshold` of any layer, and the candidate layer is still
	*			inside `selectTolerance`.
	**/
	svg_nav_path_policy_t defaultPolicy = {};
	// Select whether "top" means highest Z or lowest Z.
	const bool preferTop = defaultPolicy.layer_select_prefer_top;
	// Threshold (world units) to activate the preference behavior.
	const double preferThreshold = std::max( 0.0, defaultPolicy.layer_select_prefer_z_threshold );

	/**
	*	Pass 1: closest-by-Z selection.
	*		- Track a primary selection (closest within `selectTolerance`).
	*		- Track a fallback selection (closest overall) for when tolerance rejects all.
	**/
	int32_t best_index = -1;
	double best_delta = std::numeric_limits<double>::max();
	int32_t fallback_index = -1;
	double best_fallback_delta = std::numeric_limits<double>::max();

	for ( int32_t i = 0; i < cell->num_layers; i++ ) {
		// Convert from quantized Z to world-space Z for distance comparisons.
		const double layer_z = ( double )cell->layers[ i ].z_quantized * mesh->z_quant;
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
	*	Fallback:
	*		If no layers are inside the tolerance gate, still return the closest-by-Z layer.
	*		This allows the caller to attempt more expensive traversal checks (or fallback
	*		pathfinding) rather than failing immediately.
	**/
	if ( best_index < 0 ) {
		best_index = fallback_index;
	}
	if ( best_index < 0 ) {
		return false;
	}

	/**
	*	Pass 2: thresholded top/bottom preference.
	*		If the desired Z is within a small window of any layer, use this to stabilize
	*		selection in multi-layer cells by preferring a consistent extremum (top or bottom).
	**/
	if ( cell->num_layers > 1 && preferThreshold > 0.0 ) {
		/**
		*	Compute closest delta across all layers.
		*		This decides whether we are "close enough" to activate the preference behavior.
		**/
		double closest_delta = std::numeric_limits<double>::max();
		for ( int32_t i = 0; i < cell->num_layers; i++ ) {
			// Convert quantized Z to world Z.
			const double layer_z = ( double )cell->layers[ i ].z_quantized * mesh->z_quant;
			// Compute absolute Z delta.
			const double delta = std::fabs( layer_z - desired_z );
			// Track the closest delta for preference activation.
			closest_delta = std::min( closest_delta, delta );
		}

		if ( closest_delta <= preferThreshold ) {
			/**
			*	Preference selection:
			*		Choose the extremum (top or bottom) among layers that are still within
			*		`selectTolerance`. This prevents preferring a far-away level purely due to
			*		being "top"/"bottom".
			**/
			int32_t preferred_index = -1;
			double preferred_z = preferTop ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();

			for ( int32_t i = 0; i < cell->num_layers; i++ ) {
				// Convert quantized Z to world-space Z.
				const double layer_z = ( double )cell->layers[ i ].z_quantized * mesh->z_quant;
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
	*	Commit output.
	**/
	*out_layer_index = best_index;
	return true;
}

/**
*    @brief	Check whether a sparse tile cell is present in the tile bitset.
*    @param	tile		Tile holding the presence bitset.
*    @param	cell_index	Cell index inside the tile (0..cells_per_tile-1).
*    @return	True if the cell is marked as present, false otherwise.
*    @note	This helper guards accesses to sparse cell arrays.
**/
static inline bool Nav_CellPresent( const nav_tile_t *tile, const int32_t cell_index ) {
	/**
	*    Sanity checks: require a tile and valid presence bitset.
	**/
	if ( !tile || !tile->presence_bits ) {
		return false;
	}

	/**
	*    Compute bitset indices and test the cell presence bit.
	**/
	const int32_t word_index = cell_index >> 5;
	const int32_t bit_index = cell_index & 31;
	return ( tile->presence_bits[ word_index ] & ( 1u << bit_index ) ) != 0;
}

/**
*    @brief	Compute the world-space center position for a node in a tile cell/layer.
*    @param	mesh	Navigation mesh owning the tile.
*    @param	tile	Tile containing the cell.
*    @param	cell_index	Cell index inside the tile.
*    @param	layer	Layer data for the node (provides quantized Z).
*    @return	World-space center position for the node.
*    @note	Returns a zero vector if inputs are invalid.
**/
static Vector3 Nav_NodeWorldPosition( const nav_mesh_t *mesh, const nav_tile_t *tile, int32_t cell_index, const nav_layer_t *layer ) {
	/**
	*    Sanity checks: require mesh, tile, and layer data.
	**/
	if ( !mesh || !tile || !layer ) {
		return Vector3{};
	}

	/**
	*    Resolve tile origin and cell coordinates inside the tile grid.
	**/
	const double tile_world_size = Nav_TileWorldSize( mesh );
	const int32_t cell_x = cell_index % mesh->tile_size;
	const int32_t cell_y = cell_index / mesh->tile_size;

	/**
	*    Compute world-space coordinates for the cell center and layer height.
	**/
	const double world_x = ( ( double )tile->tile_x * tile_world_size ) + ( ( double )cell_x + 0.5 ) * mesh->cell_size_xy;
	const double world_y = ( ( double )tile->tile_y * tile_world_size ) + ( ( double )cell_y + 0.5 ) * mesh->cell_size_xy;
	const double world_z = ( double )layer->z_quantized * mesh->z_quant;

	/**
	*    Return the computed node position.
	**/
	return Vector3{ ( float )world_x, ( float )world_y, ( float )world_z };
}

/**
*    @brief	Resolve a node reference inside a specific tile based on world position.
*    @param	mesh	Navigation mesh owning the tile.
*    @param	tile	Tile to query for the node.
*    @param	tile_id	Canonical world tile id for the tile.
*    @param	leaf_index	BSP leaf index for the position (stored in node key).
*    @param	position	World-space position to query.
*    @param	desired_z	Desired Z for layer selection.
*    @param	out_node	[out] Node reference to populate on success.
*    @return	True if a node could be resolved from the tile.
**/
static bool Nav_TryResolveNodeInTile( const nav_mesh_t *mesh, const nav_tile_t *tile, const int32_t tile_id,
	const int32_t leaf_index, const Vector3 &position, double desired_z, nav_node_ref_t *out_node ) {
	/**
	*    Sanity checks: require mesh, tile, and output storage.
	**/
	if ( !mesh || !tile || !out_node ) {
		return false;
	}

	/**
	*    Compute tile origin and map the world position into a cell index.
	**/
	const double tile_world_size = Nav_TileWorldSize( mesh );
	const double tile_origin_x = ( double )tile->tile_x * tile_world_size;
	const double tile_origin_y = ( double )tile->tile_y * tile_world_size;
	const int32_t cell_x = Nav_WorldToCellCoord( position[ 0 ], tile_origin_x, mesh->cell_size_xy );
	const int32_t cell_y = Nav_WorldToCellCoord( position[ 1 ], tile_origin_y, mesh->cell_size_xy );

	/**
	*    Reject positions that map outside the tile's cell grid.
	**/
	if ( cell_x < 0 || cell_y < 0 || cell_x >= mesh->tile_size || cell_y >= mesh->tile_size ) {
		return false;
	}

	/**
	*    Resolve the cell entry and ensure it is present in the sparse tile.
	**/
	const int32_t cell_index = ( cell_y * mesh->tile_size ) + cell_x;
	if ( !Nav_CellPresent( tile, cell_index ) ) {
		return false;
	}

	const nav_xy_cell_t *cell = &tile->cells[ cell_index ];
	if ( cell->num_layers <= 0 || !cell->layers ) {
		return false;
	}

	/**
	*    Select the best layer index based on the desired Z value.
	**/
	int32_t layer_index = -1;
	if ( !Nav_SelectLayerIndex( mesh, cell, desired_z, &layer_index ) ) {
		return false;
	}

	/**
	*    Populate the node reference with keys and world-space position.
	**/
	const nav_layer_t *layer = &cell->layers[ layer_index ];
	out_node->key.leaf_index = leaf_index;
	out_node->key.tile_index = tile_id;
	out_node->key.cell_index = cell_index;
	out_node->key.layer_index = layer_index;
	out_node->position = Nav_NodeWorldPosition( mesh, tile, cell_index, layer );
	return true;
}

/**
*    @brief	Find a navigation node in a specific BSP leaf using its tile list.
*    @param	mesh	Navigation mesh holding world tiles.
*    @param	leaf_data	Leaf data containing tile ids to search.
*    @param	leaf_index	Leaf index to store in the output node key.
*    @param	position	World-space position to query.
*    @param	desired_z	Desired Z for layer selection.
*    @param	out_node	[out] Node reference to populate on success.
*    @param	allow_fallback	If true, allows a broader scan of leaf tiles.
*    @return	True if a node was found in this leaf, false otherwise.
**/
static bool Nav_FindNodeInLeaf( const nav_mesh_t *mesh, const nav_leaf_data_t *leaf_data, int32_t leaf_index,
	const Vector3 &position, double desired_z, nav_node_ref_t *out_node, bool allow_fallback ) {
	/**
	*    Sanity checks: require mesh, leaf data, and output storage.
	**/
	if ( !mesh || !leaf_data || !out_node ) {
		return false;
	}
	if ( leaf_data->num_tiles <= 0 || !leaf_data->tile_ids ) {
		return false;
	}

	/**
	*    Compute the target tile coordinate from the world position.
	**/
	const double tile_world_size = Nav_TileWorldSize( mesh );
	const int32_t target_tile_x = Nav_WorldToTileCoord( position[ 0 ], tile_world_size );
	const int32_t target_tile_y = Nav_WorldToTileCoord( position[ 1 ], tile_world_size );

	/**
	*    Iterate the leaf's tile list to resolve a node.
	**/
	for ( int32_t i = 0; i < leaf_data->num_tiles; i++ ) {
		// Resolve tile id and ensure it is within bounds.
		const int32_t tile_id = leaf_data->tile_ids[ i ];
		if ( tile_id < 0 || tile_id >= ( int32_t )mesh->world_tiles.size() ) {
			continue;
		}

		const nav_tile_t *tile = &mesh->world_tiles[ tile_id ];
		// Skip tiles that do not match the target coordinate unless fallback is allowed.
		if ( !allow_fallback && ( tile->tile_x != target_tile_x || tile->tile_y != target_tile_y ) ) {
			continue;
		}

		// Attempt to resolve a node within this tile.
		if ( Nav_TryResolveNodeInTile( mesh, tile, tile_id, leaf_index, position, desired_z, out_node ) ) {
			return true;
		}
	}

	return false;
}

/**
*    @brief	Select a layer index using a blend between start and goal Z heights.
*    @param	mesh	Navigation mesh used for quantization.
*    @param	cell	Navigation cell containing layers.
*    @param	start_z	Starting Z height (world units).
*    @param	goal_z	Goal Z height (world units).
*    @param	start_pos	Start position (for distance-based blending).
*    @param	goal_pos	Goal position (for distance-based blending).
*    @param	blend_start_dist	Distance at which blending begins.
*    @param	blend_full_dist	Distance at which blending fully favors the goal Z.
*    @param	out_layer_index	[out] Selected layer index.
*    @return	True if a suitable layer index was found.
**/
const bool Nav_SelectLayerIndex_BlendZ( const nav_mesh_t *mesh, const nav_xy_cell_t *cell, double start_z, double goal_z,
	const Vector3 &start_pos, const Vector3 &goal_pos, const double blend_start_dist, const double blend_full_dist, int32_t *out_layer_index ) {
	/**
	*    Sanity checks: require mesh, cell, and output storage.
	**/
	if ( !mesh || !cell || !out_layer_index ) {
		return false;
	}

	/**
	*    Compute horizontal distance to drive Z blending.
	**/
	const Vector3 delta = QM_Vector3Subtract( goal_pos, start_pos );
	const double dist2d = sqrtf( ( delta[ 0 ] * delta[ 0 ] ) + ( delta[ 1 ] * delta[ 1 ] ) );

	/**
	*    Derive a blend factor between start and goal Z based on the distance gates.
	**/
	double blend_factor = 0.0;
	if ( blend_full_dist > blend_start_dist ) {
		blend_factor = QM_Clamp( ( dist2d - blend_start_dist ) / ( blend_full_dist - blend_start_dist ), 0.0, 1.0 );
	} else if ( blend_full_dist > 0.0 ) {
		blend_factor = ( dist2d >= blend_full_dist ) ? 1.0 : 0.0;
	}

	/**
	*    Blend between start and goal Z and delegate to the standard selector.
	**/
	const double desired_z = ( ( 1.0 - blend_factor ) * start_z ) + ( blend_factor * goal_z );
	return Nav_SelectLayerIndex( mesh, cell, desired_z, out_layer_index );
}

/**
*    @brief	Find a navigation node using blended start/goal Z selection.
*    @param	mesh	Navigation mesh to query.
*    @param	position	World-space position to query.
*    @param	start_z	Seeker starting Z height.
*    @param	goal_z	Target goal Z height.
*    @param	start_pos	Seeker starting position.
*    @param	goal_pos	Target goal position.
*    @param	blend_start_dist	Distance at which blending starts.
*    @param	blend_full_dist	Distance at which blending fully favors goal Z.
*    @param	out_node	[out] Output node reference.
*    @param	allow_fallback	Allow fallback search if direct lookup fails.
*    @return	True if a node was found, false otherwise.
**/
const bool Nav_FindNodeForPosition_BlendZ( const nav_mesh_t *mesh, const Vector3 &position, double start_z, double goal_z,
	const Vector3 &start_pos, const Vector3 &goal_pos, const double blend_start_dist, const double blend_full_dist,
	nav_node_ref_t *out_node, bool allow_fallback ) {
	/**
	*    Sanity checks: require mesh and output storage.
	**/
	if ( !mesh || !out_node ) {
		return false;
	}

	/**
	*    Build a blended desired Z value based on horizontal distance.
	**/
	const Vector3 delta = QM_Vector3Subtract( goal_pos, start_pos );
	const double dist2d = sqrtf( ( delta[ 0 ] * delta[ 0 ] ) + ( delta[ 1 ] * delta[ 1 ] ) );
	const double denom = blend_full_dist - blend_start_dist;
	const double t = ( denom > 0.0 ) ? QM_Clamp( ( dist2d - blend_start_dist ) / denom, 0.0, 1.0 ) : ( dist2d >= blend_full_dist ? 1.0 : 0.0 );
	const double desired_z = ( ( 1.0 - t ) * start_z ) + ( t * goal_z );

	/**
	*    Attempt lookup with the blended Z preference first.
	**/
	if ( Nav_FindNodeForPosition( mesh, position, desired_z, out_node, allow_fallback ) ) {
		return true;
	}

	/**
	*    Fallback: retry with explicit start/goal Z values when allowed.
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
*    @brief	Find a navigation node for a world-space position using BSP leaf lookup.
*    @param	mesh	Navigation mesh to query.
*    @param	position	World-space position to query.
*    @param	desired_z	Desired Z height for layer selection.
*    @param	out_node	[out] Output node reference.
*    @param	allow_fallback	Allow fallback search if direct lookup fails.
*    @return	True if a node was found, false otherwise.
**/
const bool Nav_FindNodeForPosition( const nav_mesh_t *mesh, const Vector3 &position, double desired_z,
	nav_node_ref_t *out_node, bool allow_fallback ) {
	/**
	*    Sanity checks: require mesh and output storage.
	**/
	if ( !mesh || !out_node ) {
		return false;
	}

	/**
	*    Default to an unknown leaf index; leaf-specific data is optional for lookup.
	**/
	const int32_t leaf_index = -1;

	/**
	*    Attempt a direct lookup via the canonical world tile map.
	**/
	const double tile_world_size = Nav_TileWorldSize( mesh );
	const int32_t tile_x = Nav_WorldToTileCoord( position[ 0 ], tile_world_size );
	const int32_t tile_y = Nav_WorldToTileCoord( position[ 1 ], tile_world_size );
	const nav_world_tile_key_t key = { .tile_x = tile_x, .tile_y = tile_y };
	auto it = mesh->world_tile_id_of.find( key );
	if ( it != mesh->world_tile_id_of.end() ) {
		const int32_t tile_id = it->second;
		const nav_tile_t *tile = &mesh->world_tiles[ tile_id ];
		if ( Nav_TryResolveNodeInTile( mesh, tile, tile_id, leaf_index, position, desired_z, out_node ) ) {
			return true;
		}
	}

	/**
	*    Fallback: probe neighboring tiles to handle leaf seam mismatches.
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
				const nav_tile_t *nb_tile = &mesh->world_tiles[ nb_tile_id ];
				if ( Nav_TryResolveNodeInTile( mesh, nb_tile, nb_tile_id, leaf_index, position, desired_z, out_node ) ) {
					return true;
				}
			}
		}
	}

	return false;
}

/**
*	@brief  Advance along a navigation path based on current origin and waypoint radius.
*	@param  path            Navigation traversal path.
*	@param  current_origin  Current 3D position of the mover.
*	@param  waypoint_radius  Radius around waypoints to consider them "reached".
*	@param  policy          Optional traversal policy for per-agent constraints.
*	@param  inout_index     Input/output current waypoint index in the path.
*	@param  out_direction    Output direction vector towards the current waypoint.
*	@return True if there is a valid waypoint to move towards after advancement.
**/
const bool SVG_Nav_QueryMovementDirection_Advance2D_Output3D( const nav_traversal_path_t *path, const Vector3 &current_origin,
    double waypoint_radius, const svg_nav_path_policy_t *policy, int32_t *inout_index, Vector3 *out_direction ) {
    /**
    *    Sanity checks: validate inputs and ensure we have a usable path.
    **/
    if ( !path || !inout_index || !out_direction ) {
        return false;
    }
    if ( path->num_points <= 0 || !path->points ) {
        return false;
    }

    /**
    *    Clamp the waypoint radius to a minimum value.
    *    This avoids degenerate behavior where micro radii prevent waypoint completion
    *    when the mover cannot stand exactly on the path point due to collision.
    **/
    waypoint_radius = std::max( waypoint_radius, 8.0 );

    int32_t index = *inout_index;
    if ( index < 0 ) {
        index = 0;
    }

    const double waypoint_radius_sqr = waypoint_radius * waypoint_radius;

    // Advance using 2D distance so Z quantization / stairs don't prevent waypoint completion.
    while ( index < path->num_points ) {
        const Vector3 delta = QM_Vector3Subtract( path->points[ index ], current_origin );
        const double dist_sqr = ( delta[ 0 ] * delta[ 0 ] ) + ( delta[ 1 ] * delta[ 1 ] );
        if ( dist_sqr > waypoint_radius_sqr ) {
            break;
        }
        index++;
    }

    /**
    *    Stuck heuristic: if we are not making 2D progress towards the current waypoint,
    *    advance it after a few frames. This helps around corners where the exact waypoint
    *    position may be effectively unreachable due to collision/step resolution.
    *
    *    NOTE: This is intentionally conservative and only triggers when the waypoint is
    *    still relatively close in 2D (within a few radii).
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
            s_last_dist2d_sqr = ( d0[ 0 ] * d0[ 0 ] ) + ( d0[ 1 ] * d0[ 1 ] );
            s_no_progress_frames = 0;
        } else {
            // Compute current 2D distance to the waypoint.
            const Vector3 d1 = QM_Vector3Subtract( path->points[ index ], current_origin );
            const double dist2d_sqr = ( d1[ 0 ] * d1[ 0 ] ) + ( d1[ 1 ] * d1[ 1 ] );

            // Only consider this if we're already fairly close to the waypoint.
            const double near_sqr = waypoint_radius_sqr * 9.0; // within ~3 radii
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
        *inout_index = path->num_points;
        return false;
    }

    Vector3 direction = QM_Vector3Subtract( path->points[ index ], current_origin );

    // Clamp vertical component to avoid large up/down jumps.
    if ( g_nav_mesh ) {
		// Determine the effective step limit (policy overrides mesh defaults when present).
		double stepLimit = g_nav_mesh->max_step;
		if ( policy && policy->max_step_height > 0.0 ) {
			stepLimit = policy->max_step_height;
		}
        const double maxDz = stepLimit + g_nav_mesh->z_quant;
        if ( direction[ 2 ] > maxDz ) {
            direction[ 2 ] = maxDz;
        } else if ( direction[ 2 ] < -maxDz ) {
            direction[ 2 ] = -maxDz;
        }
    }

    const double length = ( double )QM_Vector3LengthDP( direction );
    if ( length <= std::numeric_limits<double>::epsilon() ) {
        return false;
    }

    *out_direction = QM_Vector3NormalizeDP( direction );
    *inout_index = index;
    return true;
}
