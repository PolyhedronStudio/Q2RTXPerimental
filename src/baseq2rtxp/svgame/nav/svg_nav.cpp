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
//! Feature flag controlling whether hierarchy coarse routing is attempted before tile-cluster fallback.
cvar_t *nav_hierarchy_route_enable = nullptr;
//! Maximum LOS sample count allowed before direct-shortcut LOS is skipped for tuning.
cvar_t *nav_direct_los_attempt_max_samples = nullptr;
//! Tuning knob controlling whether Auto LOS mode may promote DDA spans to LeafTraversal heuristically.
cvar_t *nav_los_auto_leaf_heuristics = nullptr;
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
//! Maximum vertical delta allowed for line-of-sight checks (used to prevent long LOS spans that go over tall obstacles).
cvar_t *nav_cost_los_max_vertical_delta = nullptr;

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
	return Nav_CanTraverseStep_ExplicitBBox( mesh, startPos, endPos, mins, maxs, clip_entity, nullptr, nullptr, CM_CONTENTMASK_MONSTERSOLID );
}
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
 *  Navigation occupancy and hierarchy helper implementations have been moved to
 *  dedicated translation units to improve modularity:
 *    - svgame/nav/svg_nav_occupancy.{h,cpp}
 *    - svgame/nav/svg_nav_hierarchy.{h,cpp}
 *    - svgame/nav/svg_nav_regions.{h,cpp}
 *    - svgame/nav/svg_nav_portals.{h,cpp}
 *
 *  The functions previously implemented inline in this file are now defined
 *  in those units. Include their headers below so remaining callers continue
 *  to compile against the public API.
 **/

#include "svgame/nav/svg_nav_occupancy.h"
#include "svgame/nav/svg_nav_hierarchy.h"
#include "svgame/nav/svg_nav_regions.h"
#include "svgame/nav/svg_nav_portals.h"


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
	#if 0 // <Q2RTXP>: For now, use more conservative defaults to keep cell counts (and thus memory usage) down. Revisit after testing.
		nav_cell_size_xy = gi.cvar( "nav_cell_size_xy", "4", 0 );
		nav_z_quant = gi.cvar( "nav_z_quant", "2", 0 );
		nav_tile_size = gi.cvar( "nav_tile_size", "32", 0 );
	#else 
		// X/Y grid cell size in world units.
		nav_cell_size_xy = gi.cvar( "nav_cell_size_xy", "4", 0 );
		// Z-axis quantization step.
		nav_z_quant = gi.cvar( "nav_z_quant", "8", 0 );
		//! Number of cells per tile dimension.
		nav_tile_size = gi.cvar( "nav_tile_size", "16", 0 );
	#endif // 
	// Optional Tunable Z vertical tolerance (world units) for layer selection and fallback.
	// If <= 0, auto-derived from mesh parameters.
	nav_z_tolerance = gi.cvar( "nav_z_tolerance", "0", 0 );

	/** 
	*    Register physics constraint CVars matching player movement:
	**/
	nav_max_step = gi.cvar( "nav_max_step", std::to_string( NAV_DEFAULT_STEP_MAX_SIZE ).c_str(), 0 );
	nav_max_drop = gi.cvar( "nav_max_drop", std::to_string( NAV_DEFAULT_MAX_DROP_HEIGHT ).c_str(), 0 );
	nav_max_slope_normal_z = gi.cvar( "nav_max_slope_normal_z", std::to_string( PHYS_MAX_SLOPE_NORMAL ).c_str(), 0);
	// LOS vertical delta cap is used to prevent long LOS spans that go over tall obstacles, which can cause performance issues and unrealistic paths. The default is set to a reasonable value based on typical level geometry, but can be tuned for specific maps or gameplay needs.
	nav_cost_los_max_vertical_delta = gi.cvar( "nav_cost_los_max_vertical_delta", std::to_string( NAV_DEFAULT_LOS_MAX_VERTICAL_DELTA ).c_str(), 0 );
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
		nav_debug_draw = gi.cvar( "nav_debug_draw", "0", 0 );
		nav_debug_draw_leaf = gi.cvar( "nav_debug_draw_leaf", "-1", 0 );
		nav_debug_draw_tile = gi.cvar( "nav_debug_draw_tile", "* ", 0 );
		nav_debug_draw_max_segments = gi.cvar( "nav_debug_draw_max_segments", "128", 0 );
		nav_debug_draw_max_dist = gi.cvar( "nav_debug_draw_max_dist", "8192", 0 );
		nav_debug_draw_tile_bounds = gi.cvar( "nav_debug_draw_tile_bounds", "0", 0 );
		nav_debug_draw_samples = gi.cvar( "nav_debug_draw_samples", "0", 0 );
		nav_debug_draw_path = gi.cvar( "nav_debug_draw_path", "0", 0 );
		// Temporary: enable diagnostic prints for failed nav lookups (tile/cell indices)
		nav_debug_show_failed_lookups = gi.cvar( "nav_debug_show_failed_lookups", "0", CVAR_CHEAT );
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
	**/
	//! Feature flag controlling whether hierarchy coarse routing is attempted before tile-cluster fallback.
	nav_hierarchy_route_enable = gi.cvar( "nav_hierarchy_route_enable", "1", 0 );
	//! Maximum LOS sample count allowed before direct-shortcut LOS is skipped for tuning.
	nav_direct_los_attempt_max_samples = gi.cvar( "nav_direct_los_attempt_max_samples", std::to_string( NAV_DEFAULT_DIRECT_LOS_ATTEMPT_MAX_SAMPLES ).c_str(), 0);
    //! Auto LOS heuristic control: 0 keeps Auto on DDA whenever available, 1 enables conservative DDA->LeafTraversal promotion.
	nav_los_auto_leaf_heuristics = gi.cvar( "nav_los_auto_leaf_heuristics", "1", 0 );
	//! Minimum 2D query distance required before hierarchy routing is preferred over local-only refinement.
	nav_hierarchy_route_min_distance = gi.cvar( "nav_hierarchy_route_min_distance", "512", 0 );
	//! Maximum LOS tests allowed during the small sync simplification pass.
	nav_simplify_sync_max_los_tests = gi.cvar( "nav_simplify_sync_max_los_tests", "32", 0 ); // <Q2RTXP>: WID: Was 8
	//! Maximum LOS tests allowed during the more aggressive async simplification pass.
	nav_simplify_async_max_los_tests = gi.cvar( "nav_simplify_async_max_los_tests", "256", 0 ); // <Q2RTXP>: WID: Was 8
	//! Maximum wall-clock milliseconds the sync simplification pass may spend per query.
	nav_simplify_sync_max_ms = gi.cvar( "nav_simplify_sync_max_ms", "4", 0 ); // <Q2RTXP>: WID: Was 4
	//! Exact corridor tile-count threshold where refinement widening switches from near to mid radius.
	nav_refine_corridor_mid_tiles = gi.cvar( "nav_refine_corridor_mid_tiles", "3", 0 ); // <Q2RTXP>: WID:was12 Was 6
	//! Exact corridor tile-count threshold where refinement widening switches from mid to far radius.
	nav_refine_corridor_far_tiles = gi.cvar( "nav_refine_corridor_far_tiles", "6", 0 ); // <Q2RTXP>: WID:was24 Was 12
	//! Buffered refinement radius used for short exact corridors.
	nav_refine_corridor_radius_near = gi.cvar( "nav_refine_corridor_radius_near", "1", 0 );
	//! Buffered refinement radius used for mid-length exact corridors.
	nav_refine_corridor_radius_mid = gi.cvar( "nav_refine_corridor_radius_mid", "2", 0 );
	//! Buffered refinement radius used for long exact corridors.
	nav_refine_corridor_radius_far = gi.cvar( "nav_refine_corridor_radius_far", "3", 0 );
	//! Minimum 2D LOS span treated as a long simplification collapse for narrow-passage clearance tuning.
	nav_simplify_long_span_min_distance = gi.cvar( "nav_simplify_long_span_min_distance", "128", 0 );
	//! Additional clearance margin required before allowing long LOS simplification spans.
	nav_simplify_clearance_margin = gi.cvar( "nav_simplify_clearance_margin", "256", 0 );
	//! Angle threshold used when pruning nearly collinear simplified waypoints.
	nav_simplify_collinear_angle_degrees = gi.cvar( "nav_simplify_collinear_angle_degrees", "45", 0 );


	/** 
	*    Register runtime cost-tuning CVars for A*  heuristics.
	**/
	// Register runtime tunable per-call A*  search budget so async worker behavior can be tuned at runtime.
	nav_astar_step_budget_ms = gi.cvar( "nav_astar_step_budget_ms", "25", 0 );

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
*	@brief	Check whether a sparse tile cell is present in the tile bitset.
*	@param	tile		Tile holding the presence bitset.
*	@param	cell_index	Cell index inside the tile (0..cells_per_tile-1).
*	@return	True if the cell is marked as present, false otherwise.
*	@note	This helper guards accesses to sparse cell arrays and validates the
*			presence_bits pointer to prevent dereferencing null pointers on
*			sparsely-populated tiles.
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
	*	Resolve the cell index and obtain a safe pointer/count via the public helper.
	*	This avoids directly dereferencing the tile's `cells` pointer which may be
	*	null for sparse tiles.
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
*    @brief	Project a feet-origin point onto the nearest walkable nav-layer Z in its current XY cell.
*    @param	mesh		Navigation mesh used for tile/cell lookup.
*    @param	feet_origin	Feet-origin point supplied by the caller.
*    @param	agent_mins	Feet-origin agent mins used for center conversion.
*    @param	agent_maxs	Feet-origin agent maxs used for center conversion.
*    @param	out_origin	[out] Feet-origin point with the projected walkable Z when lookup succeeds.
*    @return	True when a walkable layer was found for the query XY cell.
*    @note	This preserves the caller's XY while snapping only the feet-origin Z to the best walkable layer.
**/
const bool SVG_Nav_TryProjectFeetOriginToWalkableZ( const nav_mesh_t * mesh, const Vector3 &feet_origin,
	const Vector3 &agent_mins, const Vector3 &agent_maxs, Vector3 * out_origin ) {
	/**
	*    Sanity checks: require mesh storage, output storage, and a valid agent hull.
	**/
	if ( !mesh || !out_origin ) {
		return false;
	}
	if ( !( agent_maxs.x > agent_mins.x ) || !( agent_maxs.y > agent_mins.y ) || !( agent_maxs.z > agent_mins.z ) ) {
		return false;
	}

	/**
	*    Convert the feet-origin query into nav-center space for tile/cell lookup.
	**/
	const Vector3 center_origin = SVG_Nav_ConvertFeetToCenter( mesh, feet_origin, &agent_mins, &agent_maxs );
	const nav_tile_cluster_key_t tile_key = SVG_Nav_GetTileKeyForPosition( mesh, center_origin );
	const nav_world_tile_key_t world_tile_key = { .tile_x = tile_key.tile_x, .tile_y = tile_key.tile_y };
	const auto tile_it = mesh->world_tile_id_of.find( world_tile_key );
	if ( tile_it == mesh->world_tile_id_of.end() ) {
		return false;
	}

	/**
	*    Resolve the tile-local XY cell that owns the query point.
	**/
	const nav_tile_t * tile = &mesh->world_tiles[ tile_it->second ];
	const auto cells_view = SVG_Nav_Tile_GetCells( mesh, tile );
	const nav_xy_cell_t * cells = cells_view.first;
	const int32_t cell_count = cells_view.second;
	if ( !cells || cell_count <= 0 ) {
		return false;
	}

	const double tile_world_size = ( double )mesh->tile_size * mesh->cell_size_xy;
	const double tile_origin_x = ( double )tile->tile_x * tile_world_size;
	const double tile_origin_y = ( double )tile->tile_y * tile_world_size;
	const double local_x = center_origin.x - tile_origin_x;
	const double local_y = center_origin.y - tile_origin_y;
	if ( local_x < 0.0 || local_y < 0.0 ) {
		return false;
	}

	const int32_t cell_x = std::clamp( ( int32_t )std::floor( local_x / mesh->cell_size_xy ), 0, mesh->tile_size - 1 );
	const int32_t cell_y = std::clamp( ( int32_t )std::floor( local_y / mesh->cell_size_xy ), 0, mesh->tile_size - 1 );
	const int32_t cell_index = ( cell_y * mesh->tile_size ) + cell_x;
	if ( cell_index < 0 || cell_index >= cell_count ) {
		return false;
	}

	/**
	*    Select the best walkable layer in the resolved cell using the existing selector.
	**/
	const nav_xy_cell_t * cell = &cells[ cell_index ];
	if ( !cell || cell->num_layers <= 0 || !cell->layers ) {
		return false;
	}

	int32_t layer_index = -1;
	if ( !Nav_SelectLayerIndex( mesh, cell, center_origin.z, &layer_index ) || layer_index < 0 || layer_index >= cell->num_layers ) {
		return false;
	}

	/**
	*    Convert the selected walkable layer back into feet-origin space for the caller.
	**/
	const float center_offset_z = ( agent_mins.z + agent_maxs.z ) * 0.5f;
	const double projected_center_z = ( double )cell->layers[ layer_index ].z_quantized * mesh->z_quant;
	*out_origin = feet_origin;
	out_origin->z = ( float )( projected_center_z - center_offset_z );
	return true;
}

/** 
* 	@brief	Get layers pointer and count for a nav XY cell (const overload).
* 	@param	cell	Pointer to the XY cell to query.
* 	@return	A std::pair of (const nav_layer_t* layers, int32_t num_layers).
**/
std::pair<const nav_layer_t *, int32_t> SVG_Nav_Cell_GetLayers( const nav_xy_cell_t * cell ) {
	/** 
	* 	Return a defensive empty view when the cell or its layers are missing.
	**/
	if ( !cell || !cell->layers || cell->num_layers <= 0 ) {
		return { nullptr, 0 };
	}

	return { cell->layers, cell->num_layers };
}

/** 
* 	@brief	Get layers pointer and count for a nav XY cell (mutable overload).
* 	@param	cell	Pointer to the XY cell to query.
* 	@return	A std::pair of (nav_layer_t* layers, int32_t num_layers).
**/
std::pair<nav_layer_t *, int32_t> SVG_Nav_Cell_GetLayers( nav_xy_cell_t * cell ) {
	/** 
	* 	Return a defensive empty view when the cell or its layers are missing.
	**/
	if ( !cell || !cell->layers || cell->num_layers <= 0 ) {
		return { nullptr, 0 };
	}

	return { cell->layers, cell->num_layers };
}

/** 
* 	@brief	Get cell array and count for a tile (mutable overload).
* 	@param	mesh	Pointer to the nav mesh.
* 	@param	tile	Pointer to the tile to query.
* 	@return	A std::pair of (nav_xy_cell_t* cells, int32_t count).
**/
std::pair<nav_xy_cell_t *, int32_t> SVG_Nav_Tile_GetCells( const nav_mesh_t * mesh, nav_tile_t * tile ) {
	/** 
	* 	Return a defensive empty view when mesh or tile storage is missing.
	**/
	if ( !mesh || !tile || !tile->cells || mesh->tile_size <= 0 ) {
		return { nullptr, 0 };
	}

	return { tile->cells, mesh->tile_size * mesh->tile_size };
}

/** 
* 	@brief	Get cell array and count for a tile (const overload).
* 	@param	mesh	Pointer to the nav mesh.
* 	@param	tile	Const pointer to the tile to query.
* 	@return	A std::pair of (const nav_xy_cell_t* cells, int32_t count).
**/
std::pair<const nav_xy_cell_t *, int32_t> SVG_Nav_Tile_GetCells( const nav_mesh_t * mesh, const nav_tile_t * tile ) {
	/** 
	* 	Return a defensive empty view when mesh or tile storage is missing.
	**/
	if ( !mesh || !tile || !tile->cells || mesh->tile_size <= 0 ) {
		return { nullptr, 0 };
	}

	return { tile->cells, mesh->tile_size * mesh->tile_size };
}

/** 
* 	@brief	Get tile id array and count for a leaf (mutable overload).
* 	@param	leaf	Pointer to the leaf data to query.
* 	@return	A std::pair of (int32_t* tile_ids, int32_t num_tiles).
**/
std::pair<int32_t *, int32_t> SVG_Nav_Leaf_GetTileIds( nav_leaf_data_t * leaf ) {
	/** 
	* 	Return a defensive empty view when the leaf or its tile ids are missing.
	**/
	if ( !leaf || !leaf->tile_ids || leaf->num_tiles <= 0 ) {
		return { nullptr, 0 };
	}

	return { leaf->tile_ids, leaf->num_tiles };
}

/** 
* 	@brief	Get tile id array and count for a leaf (const overload).
* 	@param	leaf	Const pointer to the leaf data to query.
* 	@return	A std::pair of (const int32_t* tile_ids, int32_t num_tiles).
**/
std::pair<const int32_t *, int32_t> SVG_Nav_Leaf_GetTileIds( const nav_leaf_data_t * leaf ) {
	/** 
	* 	Return a defensive empty view when the leaf or its tile ids are missing.
	**/
	if ( !leaf || !leaf->tile_ids || leaf->num_tiles <= 0 ) {
		return { nullptr, 0 };
	}

	return { leaf->tile_ids, leaf->num_tiles };
}

/** 
* 	@brief	Get region id array and count for a leaf (mutable overload).
* 	@param	leaf	Pointer to the leaf data to query.
* 	@return	A std::pair of (int32_t* region_ids, int32_t num_regions).
**/
std::pair<int32_t *, int32_t> SVG_Nav_Leaf_GetRegionIds( nav_leaf_data_t * leaf ) {
	/** 
	* 	Return a defensive empty view when the leaf or its region ids are missing.
	**/
	if ( !leaf || !leaf->region_ids || leaf->num_regions <= 0 ) {
		return { nullptr, 0 };
	}

	return { leaf->region_ids, leaf->num_regions };
}

/** 
* 	@brief	Get region id array and count for a leaf (const overload).
* 	@param	leaf	Const pointer to the leaf data to query.
* 	@return	A std::pair of (const int32_t* region_ids, int32_t num_regions).
**/
std::pair<const int32_t *, int32_t> SVG_Nav_Leaf_GetRegionIds( const nav_leaf_data_t * leaf ) {
	/** 
	* 	Return a defensive empty view when the leaf or its region ids are missing.
	**/
	if ( !leaf || !leaf->region_ids || leaf->num_regions <= 0 ) {
		return { nullptr, 0 };
	}

	return { leaf->region_ids, leaf->num_regions };
}

/** 
* 	@brief	Get inline-model runtime array and count from mesh (mutable overload).
* 	@param	mesh	Pointer to the navigation mesh.
* 	@return	A std::pair of (nav_inline_model_runtime_t* entries, int32_t count).
**/
std::pair<nav_inline_model_runtime_t *, int32_t> SVG_Nav_Mesh_GetInlineModelRuntime( nav_mesh_t * mesh ) {
	/** 
	* 	Return a defensive empty view when mesh or runtime storage is missing.
	**/
	if ( !mesh || !mesh->inline_model_runtime || mesh->num_inline_model_runtime <= 0 ) {
		return { nullptr, 0 };
	}

	return { mesh->inline_model_runtime, mesh->num_inline_model_runtime };
}

/** 
* 	@brief	Get inline-model runtime array and count from mesh (const overload).
* 	@param	mesh	Const pointer to the navigation mesh.
* 	@return	A std::pair of (const nav_inline_model_runtime_t* entries, int32_t count).
**/
std::pair<const nav_inline_model_runtime_t *, int32_t> SVG_Nav_Mesh_GetInlineModelRuntime( const nav_mesh_t * mesh ) {
	/** 
	* 	Return a defensive empty view when mesh or runtime storage is missing.
	**/
	if ( !mesh || !mesh->inline_model_runtime || mesh->num_inline_model_runtime <= 0 ) {
		return { nullptr, 0 };
	}

	return { mesh->inline_model_runtime, mesh->num_inline_model_runtime };
}
