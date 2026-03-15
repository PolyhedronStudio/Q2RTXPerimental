/** * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 
* 
* 
* 	SVGame: VoxelMesh Traversing
* 
* 
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"

#include "svgame/nav/svg_nav.h"
#include "svgame/nav/svg_nav_clusters.h"
#include "svgame/nav/svg_nav_debug.h"
#include "svgame/nav/svg_nav_generate.h"
#include "svgame/nav/svg_nav_path_process.h"
#include "svgame/nav/svg_nav_request.h"
#include "svgame/nav/svg_nav_traversal.h"
#include "svgame/nav/svg_nav_traversal_async.h"

#include "svgame/entities/svg_player_edict.h"

// <Q2RTXP>: TODO: Move to shared? Ehh.. 
// Common BSP access for navigation generation.
#include "common/bsp.h"
#include "common/files.h"

#include <algorithm>
#include <cmath>
#include <limits>

extern cvar_t *nav_max_step;
extern cvar_t *nav_max_drop_height_cap;
extern cvar_t *nav_debug_draw;
extern cvar_t *nav_debug_draw_path;

//! Cached stats snapshot for the most recent sync navigation query.
static nav_query_debug_stats_t s_nav_last_query_stats = {};

//! Non-owning pointer to the currently active sync query stats collector.
static nav_query_debug_stats_t * s_nav_active_query_stats = nullptr;

/** 
*	Targeted pathfinding diagnostics:
*	    These are intentionally gated behind `nav_debug_draw >= 2` to avoid
*	    spamming logs during normal gameplay.
**/
bool Nav_PathDiagEnabled( void ) {
	return nav_debug_draw && nav_debug_draw->integer >= 2;
}

/** 
*    @brief  Resolve the canonical world tile view for a node reference.
*    @param  mesh  Navigation mesh owning the canonical tile storage.
*    @param  node  Node reference whose tile should be resolved.
*    @return Pointer to the canonical tile, or nullptr when the node/tile is invalid.
*    @note   This keeps traversal hot paths from repeating tile-index bounds checks inline.
**/
const nav_tile_t * SVG_Nav_GetNodeTileView( const nav_mesh_t * mesh, const nav_node_ref_t &node ) {
	/** 
	*    Sanity checks: require mesh storage and a valid canonical tile index.
	**/
	if ( !mesh || node.key.tile_index < 0 || node.key.tile_index >= ( int32_t )mesh->world_tiles.size() ) {
		return nullptr;
	}

	return &mesh->world_tiles[ node.key.tile_index ];
}

/** 
*    @brief  Resolve the canonical XY cell view for a node reference.
*    @param  mesh  Navigation mesh owning the canonical tile storage.
*    @param  node  Node reference whose cell should be resolved.
*    @return Pointer to the canonical XY cell, or nullptr when the node/cell is invalid.
*    @note   This composes `SVG_Nav_GetNodeTileView()` with the tile cell accessor so traversal code can avoid duplicating both checks.
**/
const nav_xy_cell_t * SVG_Nav_GetNodeCellView( const nav_mesh_t * mesh, const nav_node_ref_t &node ) {
	/** 
	*    Resolve the owning canonical tile before touching cell storage.
	**/
	const nav_tile_t * tile = SVG_Nav_GetNodeTileView( mesh, node );
	if ( !tile ) {
		return nullptr;
	}

	/** 
	*    Resolve the sparse cell array and validate the node's cell index.
	**/
	auto cells_view = SVG_Nav_Tile_GetCells( mesh, tile );
	const nav_xy_cell_t * cells_ptr = cells_view.first;
	const int32_t cells_count = cells_view.second;
	if ( !cells_ptr || node.key.cell_index < 0 || node.key.cell_index >= cells_count ) {
		return nullptr;
	}

	return &cells_ptr[ node.key.cell_index ];
}

/** 
*    @brief  Map an XY neighbor offset onto the persisted per-edge direction index.
*    @param  cell_dx  Neighbor cell X offset.
*    @param  cell_dy  Neighbor cell Y offset.
*    @return Persisted edge-slot index, or -1 when the offset is not one of the 8 stored directions.
**/
static int32_t Nav_EdgeDirIndexForOffset( const int32_t cell_dx, const int32_t cell_dy ) {
	/** 
	*    Persist only the immediate 8-direction XY neighborhood.
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
*    @brief  Resolve a canonical layer pointer from a node reference.
*    @param  mesh  Navigation mesh.
*    @param  node  Node reference to resolve.
*    @return Pointer to the canonical layer or nullptr on failure.
**/
const nav_layer_t * SVG_Nav_GetNodeLayerView( const nav_mesh_t * mesh, const nav_node_ref_t &node ) {
	/** 
	*    Resolve the owning canonical cell before touching layer storage.
	**/
	const nav_xy_cell_t * cell = SVG_Nav_GetNodeCellView( mesh, node );
	if ( !cell ) {
		return nullptr;
	}

	/** 
	*    Resolve the target layer through the cell's safe layer view.
	**/
	auto layers_view = SVG_Nav_Cell_GetLayers( cell );
	const nav_layer_t * layers_ptr = layers_view.first;
	const int32_t layer_count = layers_view.second;
	if ( !layers_ptr || node.key.layer_index < 0 || node.key.layer_index >= layer_count ) {
		return nullptr;
	}

	return &layers_ptr[ node.key.layer_index ];
}

/** 
*    @brief  Query traversal feature bits for a canonical node.
*    @param  mesh  Navigation mesh.
*    @param  node  Node reference to inspect.
*    @return Traversal feature bits for the node, or `NAV_TRAVERSAL_FEATURE_NONE` on failure.
**/
uint32_t SVG_Nav_GetNodeTraversalFeatureBits( const nav_mesh_t * mesh, const nav_node_ref_t &node ) {
	/** 
	*    Resolve the canonical layer before reading its traversal metadata.
	**/
	const nav_layer_t * layer = SVG_Nav_GetNodeLayerView( mesh, node );
	return layer ? layer->traversal_feature_bits : NAV_TRAVERSAL_FEATURE_NONE;
}

/** 
*    @brief  Query explicit edge metadata for a canonical node and XY offset.
*    @param  mesh      Navigation mesh.
*    @param  node      Source node reference.
*    @param  cell_dx   Neighbor cell X offset in `[-1, 1]`.
*    @param  cell_dy   Neighbor cell Y offset in `[-1, 1]`.
*    @return Explicit edge feature bits, or `NAV_EDGE_FEATURE_NONE` when no persisted edge slot applies.
**/
uint32_t SVG_Nav_GetEdgeFeatureBitsForOffset( const nav_mesh_t * mesh, const nav_node_ref_t &node, const int32_t cell_dx, const int32_t cell_dy ) {
	/** 
	*    Resolve the persisted edge slot index for this XY neighbor offset.
	**/
	const int32_t edge_dir_index = Nav_EdgeDirIndexForOffset( cell_dx, cell_dy );
	if ( edge_dir_index < 0 ) {
		return NAV_EDGE_FEATURE_NONE;
	}

	/** 
	*    Resolve the canonical layer before reading the persisted edge metadata.
	**/
	const nav_layer_t * layer = SVG_Nav_GetNodeLayerView( mesh, node );
	if ( !layer ) {
		return NAV_EDGE_FEATURE_NONE;
	}

	/** 
	*    Return only explicitly persisted edge data.
	**/
	if ( ( layer->edge_valid_mask & ( 1u << edge_dir_index ) ) == 0 ) {
		return NAV_EDGE_FEATURE_NONE;
	}

	return layer->edge_feature_bits[ edge_dir_index ];
}

/** 
*    @brief  Query coarse tile summary bits for the tile owning a canonical node.
*    @param  mesh  Navigation mesh.
*    @param  node  Node reference whose tile should be inspected.
*    @return Tile summary bits, or `NAV_TILE_SUMMARY_NONE` on failure.
**/
uint32_t SVG_Nav_GetTileSummaryBitsForNode( const nav_mesh_t * mesh, const nav_node_ref_t &node ) {
	/** 
	*    Validate the canonical tile index before reading tile-level metadata.
	**/
	const nav_tile_t * tile = SVG_Nav_GetNodeTileView( mesh, node );
	if ( !tile ) {
		return NAV_TILE_SUMMARY_NONE;
	}

	return tile->traversal_summary_bits | tile->edge_summary_bits;
}

/** 
*	@brief	RAII helper used to bracket one sync query stats capture window.
*	@note	Only the outermost sync query activates collection so nested helper calls do not clobber totals.
**/
struct nav_query_debug_scope_t {
	//! Stats buffer being populated for the outermost active query.
	nav_query_debug_stats_t * stats = nullptr;
	//! Real-system timestamp captured when the query begins.
	uint64_t start_ms = 0;
	//! True when this scope became the outermost active query collector.
	bool activated = false;

	/** 
	*	@brief	Begin collecting sync query stats when no other collector is active.
	*	@param	in_stats	Stats buffer that should receive the query snapshot.
	**/
	explicit nav_query_debug_scope_t( nav_query_debug_stats_t * in_stats )
		: stats( in_stats ) {
		/** 
		*	Ignore nested scopes so only the public query entry point owns total timing.
		**/
		if ( !stats || s_nav_active_query_stats != nullptr ) {
			return;
		}

		/** 
		*	Reset the output buffer and activate it for downstream helper calls.
		**/
		*stats = {};
		start_ms = gi.GetRealSystemTime();
		s_nav_active_query_stats = stats;
		activated = true;
	}

	/** 
	*	@brief	Finalize total timing for the outermost active sync query.
	**/
	~nav_query_debug_scope_t() {
		/** 
		*	Only the active outermost scope should finalize cached stats.
		**/
		if ( !activated || !stats ) {
			return;
		}

		/** 
		*	Store the finished total duration and mark the snapshot valid.
		**/
		stats->total_ms = ( double )( gi.GetRealSystemTime() - start_ms );
		stats->valid = true;
		s_nav_active_query_stats = nullptr;
	}
};

/** 
*	@brief	Record one coarse-route timing sample into the active sync query stats.
*	@param	stats			Active query stats buffer.
*	@param	used_hierarchy_route	True when hierarchy coarse A*	produced the route filter.
*	@param	used_cluster_route	True when legacy cluster routing produced the route filter.
*	@param	coarse_expansions	Coarse expansion count reported for diagnostics.
*	@param	elapsed_ms		Measured coarse-stage duration.
**/
static void Nav_QueryStats_RecordCoarseStage( nav_query_debug_stats_t * stats, const bool used_hierarchy_route,
	const bool used_cluster_route, const int32_t coarse_expansions, const uint64_t elapsed_ms ) {
	/** 
	*	Ignore calls that do not have an active stats buffer.
	**/
	if ( !stats ) {
		return;
	}

	/** 
	*	Accumulate the coarse-route timing and current expansion proxy.
	**/
	stats->coarse_ms += ( double )elapsed_ms;
	if ( used_hierarchy_route ) {
		stats->used_hierarchy_route = true;
	}
	if ( used_cluster_route ) {
		stats->used_cluster_route = true;
	}
	if ( coarse_expansions > 0 ) {
		stats->coarse_expansions += coarse_expansions;
	}
}

/** 
*	@brief	Record fine-search counters from the shared incremental A*	state.
*	@param	stats		Active query stats buffer.
*	@param	state		Completed incremental A*	state.
*	@param	elapsed_ms	Measured fine-search duration.
**/
static void Nav_QueryStats_RecordFineStage( nav_query_debug_stats_t * stats, const nav_a_star_state_t &state, const uint64_t elapsed_ms ) {
	/** 
	*	Ignore calls that do not have an active stats buffer.
	**/
	if ( !stats ) {
		return;
	}

	/** 
	*	Accumulate lightweight timing and counter snapshots from the A*	state.
	**/
	stats->refine_ms += ( double )elapsed_ms;
	stats->leaf_expansions += state.node_expand_count;
	stats->open_pushes += state.open_push_count;
	stats->open_pops += state.open_pop_count;
   stats->occupancy_overlay_hits += state.occupancy_overlay_hit_count;
	stats->occupancy_soft_cost_hits += state.occupancy_soft_cost_hit_count;
	stats->occupancy_block_rejects += state.occupancy_block_reject_count;
   stats->corridor_buffer_radius = state.corridor_buffer_radius;
	stats->corridor_buffered_tile_count = state.corridor_buffered_tile_count;
}

/** 
*	@brief	Record that the active sync query used a fallback endpoint strategy.
*	@param	stats	Active query stats buffer.
**/
static void Nav_QueryStats_RecordFallbackUsage( nav_query_debug_stats_t * stats ) {
	/** 
	*	Ignore calls that do not have an active stats buffer.
	**/
	if ( !stats ) {
		return;
	}

	/** 
	*	Mark the snapshot so debug output can show that primary endpoint resolution was not enough.
	**/
	stats->fallback_used = true;
}

/** 
*	@brief    Record whether hierarchy routing was preferred and whether fallback occurred.
*	@param    stats                  Active query stats buffer.
*	@param    hierarchy_preferred    True when hierarchy routing was eligible and preferred for this query.
*	@param    used_hierarchy_route   True when hierarchy routing actually produced the corridor.
*	@param    has_refine_corridor    True when any coarse corridor was available for refinement.
**/
static void Nav_QueryStats_RecordHierarchyRoutingChoice( nav_query_debug_stats_t * stats, const bool hierarchy_preferred,
	const bool used_hierarchy_route, const bool has_refine_corridor ) {
	/** 
	*	Ignore calls that do not have an active stats buffer.
	**/
	if ( !stats ) {
		return;
	}

	/** 
	*	Publish the preferred-routing state separately from older endpoint fallback stats.
	**/
	stats->hierarchy_preferred = stats->hierarchy_preferred || hierarchy_preferred;
	if ( hierarchy_preferred && !used_hierarchy_route ) {
		stats->hierarchy_fallback_used = true;
	}
	if ( !has_refine_corridor ) {
		stats->unrestricted_refine_used = true;
	}
}

/** 
*	@brief    Return whether hierarchy routing should be treated as the preferred coarse path for this query.
* @param    mesh          Navigation mesh containing hierarchy data.
* @param    start_point   Start point for the current query in nav-center space.
* @param    goal_point    Goal point for the current query in nav-center space.
* @return   True when hierarchy routing is enabled, static hierarchy data is ready, and the query is long enough to justify the coarse pass.
**/
static bool Nav_HierarchyRoutingPreferredForQuery( const nav_mesh_t * mesh, const Vector3 &start_point, const Vector3 &goal_point ) {
	/** 
	*	Require an enabled, non-dirty hierarchy before treating hierarchy routing as the preferred path.
	**/
 if ( !mesh
		|| !nav_hierarchy_route_enable
		|| nav_hierarchy_route_enable->integer == 0
		|| mesh->hierarchy.dirty
		|| mesh->hierarchy.regions.empty() ) {
		return false;
	}

	/** 
	*	Skip hierarchy preference for very short local queries so the coarse pass stays focused on open-area or longer-range routing.
	**/
	const Vector3 delta = QM_Vector3Subtract( goal_point, start_point );
	const double distance2D = std::sqrt( ( delta.x* delta.x ) + ( delta.y* delta.y ) );
    const double minHierarchyDistance = nav_hierarchy_route_min_distance ? std::max( 0.0, ( double )nav_hierarchy_route_min_distance->value ) : 192.0;
	return distance2D >= minHierarchyDistance;
}

/** 
*	@brief	Record the final success state and waypoint count for the active sync query.
*	@param	stats		Active query stats buffer.
*	@param	path_found	True when the query produced a valid path.
*	@param	waypoint_count	Number of finalized waypoints.
**/
static void Nav_QueryStats_RecordResult( nav_query_debug_stats_t * stats, const bool path_found, const int32_t waypoint_count ) {
	/** 
	*	Ignore calls that do not have an active stats buffer.
	**/
	if ( !stats ) {
		return;
	}

	/** 
	*	Store the query result summary for structured output and benchmarks.
	**/
	stats->path_found = path_found;
	stats->waypoint_count = waypoint_count;
}

/** 
*	@brief    Record bounded refinement corridor metadata for the active sync query.
*	@param    stats      Active query stats buffer.
*	@param    corridor   Optional explicit refinement corridor used by fine A* .
**/
static void Nav_QueryStats_RecordRefineCorridor( nav_query_debug_stats_t * stats, const nav_refine_corridor_t * corridor ) {
	/** 
	*	Ignore calls that do not have an active stats buffer or corridor.
	**/
	if ( !stats || !corridor ) {
		return;
	}

	/** 
	*	Publish the bounded refinement corridor shape so benchmarks can report how tightly the search was constrained.
	**/
	stats->used_refine_corridor = corridor->HasExactTileRoute();
	stats->corridor_region_count = ( int32_t )corridor->region_path.size();
	stats->corridor_portal_count = ( int32_t )corridor->portal_path.size();
	stats->corridor_tile_count = ( int32_t )corridor->exact_tile_route.size();
	stats->portal_overlay_block_count = corridor->overlay_block_count;
	stats->portal_overlay_penalty_count = corridor->overlay_penalty_count;
}

/** 
*	@brief    Record one finalized-path simplification pass into the active sync query stats buffer.
*	@param    stats              Active query stats buffer.
*	@param    simplify_stats     Simplification attempt/success counters for this pass.
**/
static void Nav_QueryStats_RecordSimplification( nav_query_debug_stats_t * stats, const nav_path_simplify_stats_t &simplify_stats ) {
	/** 
	*	Ignore calls that do not have an active stats buffer.
	**/
	if ( !stats ) {
		return;
	}

	/** 
	*	Accumulate simplification counters so benchmark output can quantify post-refinement collapse work.
	**/
    stats->simplify_input_waypoint_count = simplify_stats.input_waypoint_count;
	stats->simplify_output_waypoint_count = simplify_stats.output_waypoint_count;
	stats->simplify_attempts += simplify_stats.attempts;
	stats->simplify_successes += simplify_stats.successes;
	stats->simplify_duplicate_prunes += simplify_stats.duplicate_prunes;
	stats->simplify_collinear_prunes += simplify_stats.collinear_prunes;
	stats->simplify_fallback_local_replans += simplify_stats.fallback_local_replan_count;
	stats->simplify_ms += simplify_stats.total_ms;
}

/** 
*	@brief    Record one LOS rejection caused by the sparse occupancy overlay.
*	@param    stats    Active query stats buffer.
**/
static void Nav_QueryStats_RecordOccupancyLosReject( nav_query_debug_stats_t * stats ) {
	/** 
	*	Ignore calls that do not have an active stats buffer.
	**/
	if ( !stats ) {
		return;
	}

	/** 
	*	Track how often the local dynamic overlay vetoed a direct shortcut or simplification span.
	**/
	stats->occupancy_los_rejects++;
}

/** 
*	@brief    Region-search node used by the initial hierarchy coarse A*	implementation.
**/
struct nav_coarse_region_search_node_t {
	//! Compact region id represented by this coarse search node.
	int32_t region_id = NAV_REGION_ID_NONE;
	//! Cost from the start region to this node.
	double g_cost = 0.0;
	//! Heuristic total used for open-list ordering.
	double f_cost = 0.0;
	//! Parent node index used to reconstruct the final region path.
	int32_t parent_node_index = -1;
	//! Portal used to reach this region from the parent.
	int32_t via_portal_id = NAV_PORTAL_ID_NONE;
	//! True once this node has been removed from the open list and fully processed.
	bool closed = false;
};

/** 
*	@brief    Resolve the owning region id for a canonical node reference.
*	@param    mesh            Navigation mesh containing hierarchy membership.
*	@param    node            Canonical node whose tile membership should be queried.
*	@param    out_region_id   [out] Resolved region id.
*	@return   True when the node maps to a tile with valid region membership.
**/
static bool Nav_Hierarchy_TryGetRegionIdForNode( const nav_mesh_t * mesh, const nav_node_ref_t &node, int32_t * out_region_id ) {
	/** 
	*	Sanity checks: require mesh and output storage.
	**/
	if ( !mesh || !out_region_id ) {
		return false;
	}

	/** 
	*	Validate the canonical tile index before reading hierarchy membership.
	**/
	if ( node.key.tile_index < 0 || node.key.tile_index >= ( int32_t )mesh->world_tiles.size() ) {
		return false;
	}

	/** 
	*	Read the tile's current region assignment and reject unassigned tiles.
	**/
	const int32_t region_id = mesh->world_tiles[ node.key.tile_index ].region_id;
	if ( region_id == NAV_REGION_ID_NONE || region_id < 0 || region_id >= ( int32_t )mesh->hierarchy.regions.size() ) {
		return false;
	}

	*out_region_id = region_id;
	return true;
}

/** 
* 	  @brief  Query the next 3D movement direction while advancing waypoints in 2D.
* 	  @param  path            Path to follow.
* 	  @param  current_origin  Current world-space origin.
* 	  @param  waypoint_radius Radius for 2D waypoint completion.
* 	  @param  policy          Optional traversal policy for per-agent step/drop limits.
* 	  @param  inout_index     Current waypoint index (updated on success).
* 	  @param  out_direction   Output normalized 3D movement direction.
* 	  @return True if a valid direction was produced, false if the path is invalid or complete.
**/
const bool SVG_Nav_QueryMovementDirection_Advance2D_Output3D( const nav_traversal_path_t * path, const Vector3 &current_origin,
	double waypoint_radius, const svg_nav_path_policy_t * policy, int32_t * inout_index, Vector3 * out_direction ) {
	/** 
	* 	Sanity checks: validate inputs and ensure we have a usable path.
	**/
	if ( !path || !inout_index || !out_direction ) {
		return false;
	}
	if ( path->num_points <= 0 || !path->points ) {
		return false;
	}

	/** 
	* 	Clamp the waypoint radius to a minimum value for 2D completion.
	**/
	waypoint_radius = std::max( waypoint_radius, 8.0 );

	int32_t index = * inout_index;
	if ( index < 0 ) {
		index = 0;
	}

	const double waypoint_radius_sqr = waypoint_radius * waypoint_radius;

	/** 
	* 	Advance waypoints using only horizontal distance so stair runs preserve their vertical anchors.
	**/
	while ( index < path->num_points ) {
		const Vector3 delta = QM_Vector3Subtract( path->points[ index ], current_origin );
		const double dist2d_sqr = ( delta[ 0 ] * delta[ 0 ] ) + ( delta[ 1 ] * delta[ 1 ] );
		if ( dist2d_sqr > waypoint_radius_sqr ) {
			break;
		}
		index++;
	}

	/** 
	* 	Return false once the caller has consumed all waypoints.
	**/
	if ( index >= path->num_points ) {
		*inout_index = path->num_points;
		return false;
	}

	/** 
	* 	Build a 3D direction toward the current waypoint while clamping extreme vertical components.
	**/
	Vector3 direction = QM_Vector3Subtract( path->points[ index ], current_origin );
	if ( g_nav_mesh ) {
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

	/** 
	* 	Validate the direction magnitude before normalizing.
	**/
	const double length = ( double )QM_Vector3LengthDP( direction );
	if ( length <= std::numeric_limits<double>::epsilon() ) {
		return false;
	}

	*out_direction = QM_Vector3NormalizeDP( direction );
	*inout_index = index;
	return true;
}

/** 
*	@brief    Forward declarations for conservative direct-shortcut gating helpers.
**/
static double Nav_LOS_ComputeSampleStep( const nav_mesh_t * mesh, const nav_los_request_t * request );
static int32_t Nav_LOS_ComputeSampleCount( const Vector3 &start_point, const Vector3 &goal_point, const double sample_step );
static bool Nav_LOS_NodeAllowed( const nav_mesh_t * mesh, const nav_node_ref_t &node, const svg_nav_path_policy_t * policy, const nav_los_request_t * request );
static const bool Nav_TryGetGlobalCellCoords( const nav_mesh_t * mesh, const nav_node_ref_t &node, int32_t * out_cell_x, int32_t * out_cell_y );
static double Nav_DirectLosShortcutMaxVerticalDelta( const nav_mesh_t * mesh, const svg_nav_path_policy_t * policy );
static bool Nav_DirectLosShortcutHasClearlyValidIntermediateHops( const nav_mesh_t * mesh, const nav_node_ref_t &start_node,
	const nav_node_ref_t &goal_node, const Vector3 &agent_mins, const Vector3 &agent_maxs,
	const svg_nav_path_policy_t * policy );
static double Nav_Hierarchy_ComputePortalTraversalCost( const nav_portal_t &portal, const nav_portal_overlay_t * overlay,
	const Vector3 &goal_point, const svg_nav_path_policy_t * policy );

/** 
*	  @brief	Try to build a direct two-point shortcut path between resolved canonical endpoints.
*	  @param	mesh		Navigation mesh containing the LOS/traversal data.
*	  @param	start_node	Resolved canonical start node.
*	  @param	goal_node	Resolved canonical goal node.
*	  @param	agent_mins	Agent bounding box minimums in nav-center space.
*	  @param	agent_maxs	Agent bounding box maximums in nav-center space.
*	  @param	policy		Optional traversal policy controlling hazards and occupancy.
*	  @param	out_points	[out] Direct nav-center waypoint list when the shortcut succeeds.
*	  @return	True when the query can skip coarse routing and fine A*	with a direct path.
**/
static const bool Nav_Traversal_TryBuildDirectShortcutPoints( const nav_mesh_t * mesh, const nav_node_ref_t &start_node,
	const nav_node_ref_t &goal_node, const Vector3 &agent_mins, const Vector3 &agent_maxs,
	const svg_nav_path_policy_t * policy, std::vector<Vector3> &out_points ) {
	/** 
	*	  	Sanity checks: require mesh storage before evaluating any direct shortcut.
	**/
	if ( !mesh ) {
		return false;
	}

	/** 
	*	  	Reset the output buffer so callers only see the direct two-point path on success.
	**/
	out_points.clear();

	/** 
	*	  	Trivial path: identical canonical endpoints already represent a direct path without invoking LOS.
	**/
	if ( start_node.key == goal_node.key ) {
		out_points.push_back( start_node.worldPosition );
		out_points.push_back( goal_node.worldPosition );
		return true;
	}

	/** 
	*	Only allow the early direct shortcut on low-risk vertical queries.
	*	If the total height delta is not small, require explicitly valid sampled
	*	intermediate hops; otherwise fall back to corridor routing plus the
	*	post-A* LOS simplifier.
	**/
	const double verticalDelta = std::fabs( ( double )goal_node.worldPosition.z - ( double )start_node.worldPosition.z );
	const double maxShortcutVerticalDelta = Nav_DirectLosShortcutMaxVerticalDelta( mesh, policy );
	const bool smallVerticalDelta = verticalDelta <= maxShortcutVerticalDelta;
	if ( !smallVerticalDelta
		&& !Nav_DirectLosShortcutHasClearlyValidIntermediateHops( mesh, start_node, goal_node, agent_mins, agent_maxs, policy ) ) {
		return false;
	}

	/** 
	*	Attempt the reusable LOS accelerator before coarse routing or fine A* .
	**/
	const nav_los_request_t losRequest = { .mode = nav_los_mode_t::Auto };
	if ( !SVG_Nav_HasLineOfSight( mesh, start_node, goal_node, agent_mins, agent_maxs, policy, &losRequest, nullptr ) ) {
		return false;
	}

	/** 
	*	Build the minimal direct path using nav-center endpoint positions.
	**/
	out_points.push_back( start_node.worldPosition );
	out_points.push_back( goal_node.worldPosition );
	return true;
}

/** 
*	@brief    Compute the Euclidean heuristic used by the initial hierarchy coarse A* .
*	@param    a   First representative point.
*	@param    b   Second representative point.
*	@return   Straight-line distance between the representative points.
**/
static double Nav_Hierarchy_CoarseHeuristic( const Vector3 &a, const Vector3 &b ) {
	/** 
	*	Compute the direct 3D distance so the coarse A*	retains an admissible geometric heuristic.
	**/
	const Vector3 delta = QM_Vector3Subtract( b, a );
	return std::sqrt( ( delta[ 0 ]* delta[ 0 ] ) + ( delta[ 1 ]* delta[ 1 ] ) + ( delta[ 2 ]* delta[ 2 ] ) );
}

/** 
*	@brief    Try to derive a representative 3D point for a canonical world tile.
*	@param    mesh            Navigation mesh containing the tile.
*	@param    tile_id         Canonical world-tile id.
*	@param    out_point       [out] Representative point sampled from the tile.
*	@return   True when a representative point could be resolved.
**/
static bool Nav_Hierarchy_TryGetTileRepresentativePoint( const nav_mesh_t * mesh, const int32_t tile_id, Vector3 * out_point ) {
	/** 
	*	Sanity checks: require mesh and output storage.
	**/
	if ( !mesh || !out_point ) {
		return false;
	}

	/** 
	*	Validate tile ownership before reading sparse cell storage.
	**/
	if ( tile_id < 0 || tile_id >= ( int32_t )mesh->world_tiles.size() ) {
		return false;
	}

	const nav_tile_t &tile = mesh->world_tiles[ tile_id ];
	auto cellsView = SVG_Nav_Tile_GetCells( mesh, &tile );
	const nav_xy_cell_t * cellsPtr = cellsView.first;
	const int32_t cellsCount = cellsView.second;
	if ( !cellsPtr || !tile.presence_bits ) {
		return false;
	}

	/** 
	*	Use the first populated cell/layer as a deterministic representative sample for coarse heuristics.
	**/
	for ( int32_t cellIndex = 0; cellIndex < cellsCount; cellIndex++ ) {
		const int32_t wordIndex = cellIndex >> 5;
		const int32_t bitIndex = cellIndex & 31;
		if ( ( tile.presence_bits[ wordIndex ] & ( 1u << bitIndex ) ) == 0 ) {
			continue;
		}

		const nav_xy_cell_t &cell = cellsPtr[ cellIndex ];
		auto layersView = SVG_Nav_Cell_GetLayers( &cell );
		const nav_layer_t * layersPtr = layersView.first;
		const int32_t layerCount = layersView.second;
		if ( !layersPtr || layerCount <= 0 ) {
			continue;
		}

		*out_point = Nav_NodeWorldPosition( mesh, &tile, cellIndex, &layersPtr[ 0 ] );
		return true;
	}

	return false;
}

/** 
*	@brief    Try to derive a representative 3D point for a hierarchy region.
*	@param    mesh            Navigation mesh containing the hierarchy.
*	@param    region_id       Compact region id.
*	@param    out_point       [out] Representative point sampled from the region.
*	@return   True when a representative point could be resolved.
**/
static bool Nav_Hierarchy_TryGetRegionRepresentativePoint( const nav_mesh_t * mesh, const int32_t region_id, Vector3 * out_point ) {
	/** 
	*	Sanity checks: require mesh and output storage.
	**/
	if ( !mesh || !out_point ) {
		return false;
	}

	/** 
	*	Validate the compact region id before reading the hierarchy arrays.
	**/
	if ( region_id < 0 || region_id >= ( int32_t )mesh->hierarchy.regions.size() ) {
		return false;
	}

	const nav_region_t &region = mesh->hierarchy.regions[ region_id ];

	/** 
	*	Prefer the stored debug anchor tile because it already follows deterministic region ordering.
	**/
	if ( Nav_Hierarchy_TryGetTileRepresentativePoint( mesh, region.debug_anchor_tile_id, out_point ) ) {
		return true;
	}

	/** 
	*	Fall back to the first region tile ref when the anchor tile is unavailable.
	**/
	if ( region.num_tile_refs > 0 ) {
		const int32_t tile_id = mesh->hierarchy.tile_refs[ region.first_tile_ref ].tile_id;
		return Nav_Hierarchy_TryGetTileRepresentativePoint( mesh, tile_id, out_point );
	}

	return false;
}

/** 
*	@brief    Check whether a coarse portal is allowed by the current traversal policy.
*	@param    portal   Portal candidate being evaluated by hierarchy coarse A* .
*	@param    policy   Optional traversal policy controlling hazardous-medium exclusions.
*	@return   True when the portal is allowed.
**/
static bool Nav_Hierarchy_PortalAllowedByPolicy( const nav_portal_t &portal, const svg_nav_path_policy_t * policy ) {
	/** 
	*	Without an explicit policy we keep the initial coarse route permissive and rely on fine A*	for final validation.
	**/
	if ( !policy ) {
		return true;
	}

	/** 
	*	Respect policy-driven hazardous-medium exclusions at the coarse stage when the metadata is available.
	**/
	if ( policy->forbid_water && ( portal.compatibility_bits & NAV_HIERARCHY_COMPAT_WATER ) != 0 ) {
		return false;
	}
	if ( policy->forbid_lava && ( portal.compatibility_bits & NAV_HIERARCHY_COMPAT_LAVA ) != 0 ) {
		return false;
	}
	if ( policy->forbid_slime && ( portal.compatibility_bits & NAV_HIERARCHY_COMPAT_SLIME ) != 0 ) {
		return false;
	}

	return true;
}

/** 
*	@brief    Return the maximum vertical delta that still qualifies as a low-risk early direct shortcut.
*	@param    mesh     Navigation mesh providing default step and quantization values.
*	@param    policy   Optional traversal policy overriding the step height.
*	@return   Positive world-space height delta that can bypass the intermediate-hop preflight.
**/
static double Nav_DirectLosShortcutMaxVerticalDelta( const nav_mesh_t * mesh, const svg_nav_path_policy_t * policy ) {
	/** 
	*	Prefer the configured LOS vertical-delta cap when available, but never drop below one step plus quantization slack.
	**/
	const double stepLimit = ( policy && policy->max_step_height > 0.0 )
		? ( double )policy->max_step_height
		: ( mesh ? ( double )mesh->max_step : NAV_DEFAULT_STEP_MAX_SIZE );
	const double zSlack = mesh ? ( double )mesh->z_quant : 0.0;
	const double configuredLimit = ( nav_cost_los_max_vertical_delta && nav_cost_los_max_vertical_delta->value > 0.0 )
		? ( double )nav_cost_los_max_vertical_delta->value
		: NAV_DEFAULT_LOS_MAX_VERTICAL_DELTA;
	return std::max( stepLimit + zSlack, configuredLimit );
}

/** 
*	@brief    Require large-vertical-delta shortcut candidates to prove their sampled hops are obviously valid.
*	@param    mesh         Navigation mesh containing the sampled canonical nodes.
*	@param    start_node   Canonical LOS start node.
*	@param    goal_node    Canonical LOS goal node.
*	@param    agent_mins   Agent bbox minimums in nav-center space.
*	@param    agent_maxs   Agent bbox maximums in nav-center space.
*	@param    policy       Optional traversal policy controlling hazards and occupancy.
*	@return   True when every sampled hop stays local and passes explicit step validation.
*	@note     This keeps risky stair/ramp queries on the corridor+A* path unless the direct span is clearly safe.
**/
static bool Nav_DirectLosShortcutHasClearlyValidIntermediateHops( const nav_mesh_t * mesh, const nav_node_ref_t &start_node,
	const nav_node_ref_t &goal_node, const Vector3 &agent_mins, const Vector3 &agent_maxs,
	const svg_nav_path_policy_t * policy ) {
	/** 
	*	Sanity checks: require mesh ownership before sampling intermediate shortcut hops.
	**/
	if ( !mesh ) {
		return false;
	}

	/** 
	*	Reuse the standard LOS node-acceptance policy so occupancy and hazard vetoes stay consistent.
	**/
	const nav_los_request_t losRequest = { .mode = nav_los_mode_t::LeafTraversal/*DDA*/ };
	if ( !Nav_LOS_NodeAllowed( mesh, start_node, policy, &losRequest ) || !Nav_LOS_NodeAllowed( mesh, goal_node, policy, &losRequest ) ) {
		return false;
	}

	/** 
	*	Only treat large vertical spans as clearly valid when we have actual intermediate samples to validate.
	**/
	const double sampleStep = Nav_LOS_ComputeSampleStep( mesh, &losRequest );
	const int32_t sampleCount = Nav_LOS_ComputeSampleCount( start_node.worldPosition, goal_node.worldPosition, sampleStep );
	if ( sampleCount <= 1 ) {
		return false;
	}

	/** 
	*	Local helper: every sampled hop must remain adjacent in the cell grid and pass explicit step validation.
	**/
	auto validateLocalHop = [&]( const nav_node_ref_t &from_node, const nav_node_ref_t &to_node ) -> bool {
		/** 
		*	Identical canonical nodes are trivially valid and do not need further checks.
		**/
		if ( from_node.key == to_node.key ) {
			return true;
		}

		/** 
		*	Require cell-local adjacency so the shortcut only survives when the sampled span is obviously walkable.
		**/
		int32_t from_cell_x = 0;
		int32_t from_cell_y = 0;
		int32_t to_cell_x = 0;
		int32_t to_cell_y = 0;
		if ( !Nav_TryGetGlobalCellCoords( mesh, from_node, &from_cell_x, &from_cell_y )
			|| !Nav_TryGetGlobalCellCoords( mesh, to_node, &to_cell_x, &to_cell_y ) ) {
			return false;
		}
		if ( std::abs( to_cell_x - from_cell_x ) > 1 || std::abs( to_cell_y - from_cell_y ) > 1 ) {
			return false;
		}

		/** 
		*	Preserve the existing step/drop validator so stairs, ramps, and local drops still share one authority.
		**/
		return Nav_CanTraverseStep_ExplicitBBox_NodeRefs( mesh, from_node, to_node, agent_mins, agent_maxs, nullptr, policy );
	};

	/** 
	*	Sample the straight span in DDA order and require each changed canonical hop to remain obviously valid.
	**/
	nav_node_ref_t previousNode = start_node;
	for ( int32_t sampleIndex = 1; sampleIndex < sampleCount; sampleIndex++ ) {
		const double t = ( double )sampleIndex / ( double )sampleCount;
		const Vector3 samplePoint = {
			( float )( start_node.worldPosition.x + ( goal_node.worldPosition.x - start_node.worldPosition.x )* t ),
			( float )( start_node.worldPosition.y + ( goal_node.worldPosition.y - start_node.worldPosition.y )* t ),
			( float )( start_node.worldPosition.z + ( goal_node.worldPosition.z - start_node.worldPosition.z )* t )
		};

		nav_node_ref_t sampleNode = {};
		if ( !Nav_FindNodeForPosition( mesh, samplePoint, samplePoint[ 2 ], &sampleNode, true ) ) {
			return false;
		}
		if ( !Nav_LOS_NodeAllowed( mesh, sampleNode, policy, &losRequest ) ) {
			return false;
		}
		if ( !validateLocalHop( previousNode, sampleNode ) ) {
			return false;
		}
		previousNode = sampleNode;
	}

	/** 
	*	Finish by validating the final sampled hop into the goal node.
	**/
	return validateLocalHop( previousNode, goal_node );
}

/** 
*	@brief    Query the local runtime overlay entry for a portal transition.
*	@param    mesh            Navigation mesh containing hierarchy portal overlays.
*	@param    portal_id       Stable compact portal id to query.
*	@param    out_overlay     [out] Runtime overlay snapshot for the portal.
*	@return   True when a local overlay snapshot was returned.
**/
static bool Nav_Hierarchy_TryGetPortalOverlayState( const nav_mesh_t * mesh, const int32_t portal_id, nav_portal_overlay_t * out_overlay ) {
	/** 
	*	Sanity checks: require mesh and output storage.
	**/
	if ( !mesh || !out_overlay ) {
		return false;
	}

	/** 
	*	Reuse the shared portal-overlay query helper so static hierarchy data stays immutable.
	**/
	return SVG_Nav_Hierarchy_TryGetPortalOverlay( mesh, portal_id, out_overlay );
}

/**
*	@brief	Try to conservatively shortcut from the current coarse portal into a neighboring portal using LOS.
*	@param	mesh	Navigation mesh containing hierarchy and canonical node data.
*	@param	current_portal	Current portal being expanded by hierarchy coarse A*.
*	@param	candidate_portal	Candidate neighboring portal owned by the adjacent region.
*	@param	goal_point	Representative point for the goal region, used to bias ladder-aware costs.
*	@param	policy	Optional traversal policy controlling hazards and ladder preference.
*	@param	out_shortcut_cost	[out] Traversal cost to use when the shortcut succeeds.
*	@return	True when the neighboring portal can be reached conservatively via LOS.
*	@note	This first pass is intentionally strict: it uses portal representative points, default mesh hull bounds,
*			respects portal compatibility and overlay blocks, and only accepts LOS when canonical endpoint resolution succeeds.
**/
static bool Nav_Hierarchy_TryPortalToPortalLosShortcut( const nav_mesh_t * mesh, const nav_portal_t &current_portal,
	const nav_portal_t &candidate_portal, const Vector3 &goal_point, const svg_nav_path_policy_t * policy, double * out_shortcut_cost ) {
	/**
	*	Sanity checks: require mesh and output storage before resolving any canonical endpoints.
	**/
	if ( !mesh || !out_shortcut_cost ) {
		return false;
	}

	/**
	*	Reject portal pairs that disagree with the active policy before any LOS work.
	**/
	if ( !Nav_Hierarchy_PortalAllowedByPolicy( current_portal, policy ) || !Nav_Hierarchy_PortalAllowedByPolicy( candidate_portal, policy ) ) {
		return false;
	}

	/**
	*	Resolve the candidate portal overlay and reject locally invalidated transitions.
	**/
	nav_portal_overlay_t candidate_overlay = {};
	if ( !Nav_Hierarchy_TryGetPortalOverlayState( mesh, candidate_portal.id, &candidate_overlay ) ) {
		candidate_overlay = {};
	}
	if ( ( candidate_overlay.flags & ( NAV_PORTAL_FLAG_INVALIDATED | NAV_PORTAL_FLAG_BLOCKED ) ) != 0 ) {
		return false;
	}

	/**
	*	Resolve canonical LOS endpoints from the two portal representative points.
	**/
	nav_node_ref_t start_node = {};
	nav_node_ref_t goal_node = {};
	if ( !Nav_FindNodeForPosition( mesh, current_portal.representative_point, current_portal.representative_point.z, &start_node, true )
		|| !Nav_FindNodeForPosition( mesh, candidate_portal.representative_point, candidate_portal.representative_point.z, &goal_node, true ) ) {
		return false;
	}

	/**
	*	Bias the reusable LOS accelerator toward the conservative backend for portal semantics.
	**/
	const nav_los_request_t los_request = { .mode = nav_los_mode_t::LeafTraversal };
	if ( !SVG_Nav_HasLineOfSight( mesh, start_node, goal_node, mesh->agent_mins, mesh->agent_maxs, policy, &los_request, nullptr ) ) {
		return false;
	}

	/**
	*	Publish the candidate portal traversal cost so coarse A* can enqueue the neighboring region with the cheaper shortcut edge.
	**/
	*out_shortcut_cost = Nav_Hierarchy_ComputePortalTraversalCost( candidate_portal, &candidate_overlay, goal_point, policy );
	return true;
}

/** 
*	@brief    Compute the coarse traversal cost for a portal transition.
*	@param    portal       Portal candidate being evaluated by hierarchy coarse A* .
*	@param    overlay      Optional local runtime overlay affecting portal cost.
*	@param    goal_point   Representative point for the goal region.
*	@param    policy       Optional traversal policy controlling ladder preference.
*	@return   Coarse traversal cost to step through this portal.
**/
static double Nav_Hierarchy_ComputePortalTraversalCost( const nav_portal_t &portal, const nav_portal_overlay_t * overlay,
	const Vector3 &goal_point, const svg_nav_path_policy_t * policy ) {
	/** 
	*	Start from the persisted coarse traversal estimate and keep it positive.
	**/
	double traversalCost = std::max( portal.traversal_cost, 1.0 );

	/** 
	*	Apply a simple ladder preference when the goal height is close to the portal's representative Z.
	**/
	if ( policy && policy->prefer_ladders && ( portal.flags & NAV_PORTAL_FLAG_SUPPORTS_LADDER ) != 0 ) {
		const double goalHeightDelta = std::fabs( ( double )goal_point.z - ( double )portal.representative_point.z );
		if ( goalHeightDelta <= policy->ladder_preferred_height_slack ) {
			traversalCost = std::max( 1.0, traversalCost - policy->ladder_preference_bias );
		}
	}

	/** 
	*	Apply any local runtime penalty after static portal heuristics so temporary invalidation can steer coarse routing.
	**/
	if ( overlay && overlay->additional_traversal_cost > 0.0 ) {
		traversalCost += overlay->additional_traversal_cost;
	}

	return traversalCost;
}

/** 
*	@brief    Forward declarations for clearance-aware LOS helpers used before their definitions later in the TU.
**/
static const bool Nav_TryGetNodeLayerView( const nav_mesh_t * mesh, const nav_node_ref_t &node, const nav_tile_t * * out_tile,
	const nav_xy_cell_t * * out_cell, const nav_layer_t * * out_layer );
static inline double Nav_GetLayerClearanceWorld( const nav_mesh_t * mesh, const nav_layer_t &layer );

/** 
*	@brief    Append a tile key only when it is not already present in the buffered coarse route.
*	@param    bufferedRoute    Materialized tile-key route under construction.
*	@param    key              Tile key to append.
**/
static void Nav_Hierarchy_AppendUniqueTileKey( std::vector<nav_tile_cluster_key_t> &bufferedRoute, const nav_tile_cluster_key_t &key ) {
	/** 
	*	Skip duplicates so the coarse tile filter stays deterministic and compact.
	**/
	if ( std::find( bufferedRoute.begin(), bufferedRoute.end(), key ) != bufferedRoute.end() ) {
		return;
	}

	bufferedRoute.push_back( key );
}

/** 
*	@brief    Run initial coarse A*	over the region/portal hierarchy.
*	@param    mesh                    Navigation mesh containing hierarchy storage.
*	@param    start_region_id         Region containing the canonical start node.
*	@param    goal_region_id          Region containing the canonical goal node.
*	@param    policy                  Optional traversal policy for coarse portal filtering.
*	@param    out_region_path         [out] Ordered compact region path from start to goal.
*	@param    out_portal_path         [out] Ordered compact portal path from start to goal.
*	@param    out_coarse_expansions   [out] Number of coarse region expansions performed.
*	@return   True when a region path was found.
**/
static bool Nav_Hierarchy_FindRegionPath( const nav_mesh_t * mesh, const int32_t start_region_id, const int32_t goal_region_id,
  const svg_nav_path_policy_t * policy, std::vector<int32_t> * out_region_path, std::vector<int32_t> * out_portal_path,
  int32_t * out_coarse_expansions, int32_t * out_overlay_block_count, int32_t * out_overlay_penalty_count ) {
	/** 
	*	Sanity checks: require mesh and output storage.
	**/
	if ( !mesh || !out_region_path ) {
		return false;
	}

	/** 
	*	Reset caller outputs before attempting any coarse search.
	**/
	out_region_path->clear();
	if ( out_portal_path ) {
		out_portal_path->clear();
	}
	if ( out_coarse_expansions ) {
		*out_coarse_expansions = 0;
	}
	if ( out_overlay_block_count ) {
		*out_overlay_block_count = 0;
	}
	if ( out_overlay_penalty_count ) {
		*out_overlay_penalty_count = 0;
	}

	/** 
	*	Validate region ids before accessing hierarchy storage.
	**/
	if ( start_region_id < 0 || goal_region_id < 0
		|| start_region_id >= ( int32_t )mesh->hierarchy.regions.size()
		|| goal_region_id >= ( int32_t )mesh->hierarchy.regions.size() ) {
		return false;
	}

	/** 
	*	Fast path: when both endpoints are already in the same region, the coarse route is trivial.
	**/
	if ( start_region_id == goal_region_id ) {
		out_region_path->push_back( start_region_id );
		return true;
	}

	Vector3 goalPoint = {};
	if ( !Nav_Hierarchy_TryGetRegionRepresentativePoint( mesh, goal_region_id, &goalPoint ) ) {
		return false;
	}

	/** 
	*	Initialize compact search storage keyed directly by region id for deterministic lookup.
	**/
	std::vector<nav_coarse_region_search_node_t> nodes;
	std::vector<int32_t> openList;
	std::vector<int32_t> nodeIndexOfRegion( mesh->hierarchy.regions.size(), -1 );
	nodes.reserve( mesh->hierarchy.regions.size() );
	openList.reserve( mesh->hierarchy.regions.size() );

	Vector3 startPoint = {};
	if ( !Nav_Hierarchy_TryGetRegionRepresentativePoint( mesh, start_region_id, &startPoint ) ) {
		return false;
	}

	/** 
	*	Seed the open list with the starting region.
	**/
	nav_coarse_region_search_node_t startNode = {};
	startNode.region_id = start_region_id;
	startNode.g_cost = 0.0;
	startNode.f_cost = Nav_Hierarchy_CoarseHeuristic( startPoint, goalPoint );
	nodes.push_back( startNode );
	openList.push_back( 0 );
	nodeIndexOfRegion[ start_region_id ] = 0;

	/** 
	*	Run a simple deterministic A*	over the compact region graph built from portal references.
	**/
	while ( !openList.empty() ) {
		int32_t bestOpenIndex = 0;
		for ( int32_t openIndex = 1; openIndex < ( int32_t )openList.size(); openIndex++ ) {
			const nav_coarse_region_search_node_t &bestNode = nodes[ openList[ bestOpenIndex ] ];
			const nav_coarse_region_search_node_t &candidateNode = nodes[ openList[ openIndex ] ];
			if ( candidateNode.f_cost < bestNode.f_cost
				|| ( candidateNode.f_cost == bestNode.f_cost && candidateNode.region_id < bestNode.region_id ) ) {
				bestOpenIndex = openIndex;
			}
		}

		const int32_t currentNodeIndex = openList[ bestOpenIndex ];
		openList.erase( openList.begin() + bestOpenIndex );
		nav_coarse_region_search_node_t &currentNode = nodes[ currentNodeIndex ];
		if ( currentNode.closed ) {
			continue;
		}

		currentNode.closed = true;
		if ( out_coarse_expansions ) {
			( * out_coarse_expansions )++;
		}

		/** 
		*	Stop once the goal region is closed, then reconstruct the compact region path.
		**/
		if ( currentNode.region_id == goal_region_id ) {
			std::vector<int32_t> reversePath;
            std::vector<int32_t> reversePortalPath;
			for ( int32_t nodeIndex = currentNodeIndex; nodeIndex >= 0; nodeIndex = nodes[ nodeIndex ].parent_node_index ) {
				reversePath.push_back( nodes[ nodeIndex ].region_id );
               if ( nodes[ nodeIndex ].via_portal_id != NAV_PORTAL_ID_NONE ) {
					reversePortalPath.push_back( nodes[ nodeIndex ].via_portal_id );
				}
			}
			out_region_path->assign( reversePath.rbegin(), reversePath.rend() );
            if ( out_portal_path ) {
				out_portal_path->assign( reversePortalPath.rbegin(), reversePortalPath.rend() );
			}
			return true;
		}

       /** 
		* 	Resolve the portal used to enter the current region so portal-to-portal LOS can
		* 	conservatively validate cross-region hand-offs without changing the fallback route.
		**/
		const nav_portal_t * currentEntryPortal = nullptr;
		if ( currentNode.via_portal_id != NAV_PORTAL_ID_NONE
			&& currentNode.via_portal_id >= 0
			&& currentNode.via_portal_id < ( int32_t )mesh->hierarchy.portals.size() ) {
			// Cache the current region entry portal once for all outgoing portal candidates.
			currentEntryPortal = &mesh->hierarchy.portals[ currentNode.via_portal_id ];
		}

		const nav_region_t &currentRegion = mesh->hierarchy.regions[ currentNode.region_id ];
		for ( int32_t portalRefIndex = 0; portalRefIndex < currentRegion.num_portal_refs; portalRefIndex++ ) {
			const int32_t portalId = mesh->hierarchy.region_portal_refs[ currentRegion.first_portal_ref + portalRefIndex ];
			if ( portalId < 0 || portalId >= ( int32_t )mesh->hierarchy.portals.size() ) {
				continue;
			}

			const nav_portal_t &portal = mesh->hierarchy.portals[ portalId ];
			if ( !Nav_Hierarchy_PortalAllowedByPolicy( portal, policy ) ) {
				continue;
			}

			nav_portal_overlay_t portalOverlay = {};
			Nav_Hierarchy_TryGetPortalOverlayState( mesh, portalId, &portalOverlay );

			/** 
			*	Skip locally invalidated or blocked portals without mutating the static portal graph.
			**/
			if ( ( portalOverlay.flags & ( NAV_PORTAL_FLAG_INVALIDATED | NAV_PORTAL_FLAG_BLOCKED ) ) != 0 ) {
				if ( out_overlay_block_count ) {
					( * out_overlay_block_count )++;
				}
				continue;
			}
			if ( out_overlay_penalty_count && portalOverlay.additional_traversal_cost > 0.0 ) {
				( * out_overlay_penalty_count )++;
			}

			const int32_t neighborRegionId = ( portal.region_a == currentNode.region_id ) ? portal.region_b : portal.region_a;
			if ( neighborRegionId < 0 || neighborRegionId >= ( int32_t )mesh->hierarchy.regions.size() ) {
				continue;
			}

			Vector3 neighborPoint = {};
			if ( !Nav_Hierarchy_TryGetRegionRepresentativePoint( mesh, neighborRegionId, &neighborPoint ) ) {
				continue;
			}

			//const double tentativeGCost = currentNode.g_cost + Nav_Hierarchy_ComputePortalTraversalCost( portal, &portalOverlay, goalPoint, policy );
			/** 
			* 	Start from the persisted candidate-portal traversal cost, then opportunistically
			* 	reuse the conservative portal-to-portal LOS shortcut when this region was entered
			* 	through another portal.
			**/
			double portalTraversalCost = Nav_Hierarchy_ComputePortalTraversalCost( portal, &portalOverlay, goalPoint, policy );

			/** 
			* 	Only evaluate the shortcut when the current region has a valid entry portal and the
			* 	outgoing candidate is a different portal. Failure must fall back to the existing
			* 	region expansion so hierarchy routing remains conservative rather than brittle.
			**/
			if ( currentEntryPortal && currentEntryPortal->id != portal.id ) {
				double shortcutTraversalCost = 0.0;
				if ( Nav_Hierarchy_TryPortalToPortalLosShortcut( mesh, * currentEntryPortal, portal, goalPoint, policy, &shortcutTraversalCost ) ) {
					// Prefer the cheaper conservative portal hand-off when LOS confirmed it explicitly.
					portalTraversalCost = std::min( portalTraversalCost, shortcutTraversalCost );
				}
			}

			const double tentativeGCost = currentNode.g_cost + portalTraversalCost;
			int32_t neighborNodeIndex = nodeIndexOfRegion[ neighborRegionId ];
			if ( neighborNodeIndex < 0 ) {
				nav_coarse_region_search_node_t neighborNode = {};
				neighborNode.region_id = neighborRegionId;
				neighborNode.g_cost = tentativeGCost;
				neighborNode.f_cost = tentativeGCost + Nav_Hierarchy_CoarseHeuristic( neighborPoint, goalPoint );
				neighborNode.parent_node_index = currentNodeIndex;
				neighborNode.via_portal_id = portalId;
				neighborNodeIndex = ( int32_t )nodes.size();
				nodes.push_back( neighborNode );
				nodeIndexOfRegion[ neighborRegionId ] = neighborNodeIndex;
				openList.push_back( neighborNodeIndex );
				continue;
			}

			nav_coarse_region_search_node_t &neighborNode = nodes[ neighborNodeIndex ];
			if ( neighborNode.closed || tentativeGCost >= neighborNode.g_cost ) {
				continue;
			}

			neighborNode.g_cost = tentativeGCost;
			neighborNode.f_cost = tentativeGCost + Nav_Hierarchy_CoarseHeuristic( neighborPoint, goalPoint );
			neighborNode.parent_node_index = currentNodeIndex;
			neighborNode.via_portal_id = portalId;
			openList.push_back( neighborNodeIndex );
		}
	}

	return false;
}

/** 
*	@brief    Materialize a compact region path into the existing fine-search tile-key route filter.
*	@param    mesh             Navigation mesh containing region tile refs.
*	@param    region_path      Ordered compact region path.
*	@param    out_tile_route   [out] Materialized tile-key route usable by fine A* .
*	@return   True when at least one tile key was produced.
**/
static bool Nav_Hierarchy_MaterializeRegionPathToTileRoute( const nav_mesh_t * mesh, const std::vector<int32_t> &region_path,
	std::vector<nav_tile_cluster_key_t> * out_tile_route ) {
	/** 
	*	Sanity checks: require mesh and output storage.
	**/
	if ( !mesh || !out_tile_route ) {
		return false;
	}

	/** 
	*	Reset any previous route materialization before appending tiles from the region path.
	**/
	out_tile_route->clear();
	if ( region_path.empty() ) {
		return false;
	}

	/** 
	*	Append every tile in each route region while preserving the region-path order.
	**/
	for ( const int32_t regionId : region_path ) {
		if ( regionId < 0 || regionId >= ( int32_t )mesh->hierarchy.regions.size() ) {
			continue;
		}

		const nav_region_t &region = mesh->hierarchy.regions[ regionId ];
		for ( int32_t tileRefIndex = 0; tileRefIndex < region.num_tile_refs; tileRefIndex++ ) {
			const int32_t tileId = mesh->hierarchy.tile_refs[ region.first_tile_ref + tileRefIndex ].tile_id;
			if ( tileId < 0 || tileId >= ( int32_t )mesh->world_tiles.size() ) {
				continue;
			}

			const nav_tile_t &tile = mesh->world_tiles[ tileId ];
			Nav_Hierarchy_AppendUniqueTileKey( * out_tile_route, nav_tile_cluster_key_t{ .tile_x = tile.tile_x, .tile_y = tile.tile_y } );
		}
	}

	return !out_tile_route->empty();
}

/** 
*	@brief    Materialize a hierarchy coarse route into an explicit bounded refinement corridor.
*	@param    mesh          Navigation mesh containing region tile refs.
*	@param    region_path   Ordered compact region path.
*	@param    portal_path   Ordered compact portal path.
*	@param    overlay_block_count   Number of local portal overlay rejections encountered while building the corridor.
*	@param    overlay_penalty_count Number of local portal overlay penalties encountered while building the corridor.
*	@param    out_corridor          [out] Explicit corridor describing the bounded local refinement space.
*	@return   True when the corridor contains an exact tile route.
**/
static bool Nav_Hierarchy_MaterializeRefineCorridor( const nav_mesh_t * mesh, const std::vector<int32_t> &region_path,
    const std::vector<int32_t> &portal_path, const int32_t overlay_block_count, const int32_t overlay_penalty_count,
	nav_refine_corridor_t * out_corridor ) {
	/** 
	*	Sanity checks: require mesh and output storage.
	**/
	if ( !mesh || !out_corridor ) {
		return false;
	}

	/** 
	*	Reset the caller-visible corridor before filling hierarchy-derived data.
	**/
	*out_corridor = {};
	out_corridor->source = nav_refine_corridor_source_t::Hierarchy;
	out_corridor->region_path = region_path;
	out_corridor->portal_path = portal_path;
    out_corridor->overlay_block_count = overlay_block_count;
	out_corridor->overlay_penalty_count = overlay_penalty_count;
	return Nav_Hierarchy_MaterializeRegionPathToTileRoute( mesh, region_path, &out_corridor->exact_tile_route );
}

/** 
*	@brief    Materialize a legacy cluster route into an explicit bounded refinement corridor.
*	@param    exact_tile_route  Ordered exact cluster route tiles.
*	@param    out_corridor      [out] Explicit corridor describing the bounded local refinement space.
*	@return   True when the corridor contains an exact tile route.
**/
static bool Nav_Cluster_MaterializeRefineCorridor( const std::vector<nav_tile_cluster_key_t> &exact_tile_route,
	nav_refine_corridor_t * out_corridor ) {
	/** 
	*	Sanity checks: require output storage.
	**/
	if ( !out_corridor ) {
		return false;
	}

	/** 
	*	Reset the corridor and tag it as cluster-derived before copying the exact tile route.
	**/
	*out_corridor = {};
	out_corridor->source = nav_refine_corridor_source_t::Cluster;
	out_corridor->exact_tile_route = exact_tile_route;
	out_corridor->overlay_block_count = 0;
	out_corridor->overlay_penalty_count = 0;
	return out_corridor->HasExactTileRoute();
}

/** 
*	@brief    Build an explicit bounded refinement corridor from hierarchy routing with legacy cluster fallback.
*	@param    mesh                    Navigation mesh containing hierarchy and cluster data.
*	@param    start_node              Resolved canonical start node.
*	@param    goal_node               Resolved canonical goal node.
*	@param    policy                  Optional traversal policy controlling coarse portal gating.
*	@param    out_corridor            [out] Explicit corridor describing the bounded local refinement space.
*	@param    out_used_hierarchy      [out] True when hierarchy routing produced the corridor.
*	@param    out_used_cluster        [out] True when legacy cluster routing produced the corridor.
*	@param    out_coarse_expansions   [out] Coarse expansion count reported for diagnostics.
*	@return   True when a bounded refinement corridor was produced.
**/
bool SVG_Nav_BuildRefineCorridor( const nav_mesh_t * mesh, const nav_node_ref_t &start_node, const nav_node_ref_t &goal_node,
	const svg_nav_path_policy_t * policy, nav_refine_corridor_t * out_corridor,
	bool * out_used_hierarchy, bool * out_used_cluster, int32_t * out_coarse_expansions ) {
	/** 
	*	Sanity checks: require mesh and output storage before attempting coarse routing.
	**/
	if ( !mesh || !out_corridor ) {
		return false;
	}

	/** 
	*	Reset all caller-visible outputs so fallback behavior remains explicit.
	**/
	*out_corridor = {};
	if ( out_used_hierarchy ) {
		*out_used_hierarchy = false;
	}
	if ( out_used_cluster ) {
		*out_used_cluster = false;
	}
	if ( out_coarse_expansions ) {
		*out_coarse_expansions = 0;
	}

	/** 
	*	Attempt the hierarchy corridor first when static hierarchy data is available.
	**/
	if ( nav_hierarchy_route_enable && nav_hierarchy_route_enable->integer != 0 && !mesh->hierarchy.dirty && !mesh->hierarchy.regions.empty() ) {
		int32_t startRegionId = NAV_REGION_ID_NONE;
		int32_t goalRegionId = NAV_REGION_ID_NONE;
		if ( Nav_Hierarchy_TryGetRegionIdForNode( mesh, start_node, &startRegionId )
			&& Nav_Hierarchy_TryGetRegionIdForNode( mesh, goal_node, &goalRegionId ) ) {
			std::vector<int32_t> regionPath;
			std::vector<int32_t> portalPath;
			int32_t coarseExpansions = 0;
			int32_t overlayBlockCount = 0;
			int32_t overlayPenaltyCount = 0;
			if ( Nav_Hierarchy_FindRegionPath( mesh, startRegionId, goalRegionId, policy, &regionPath, &portalPath, &coarseExpansions,
				&overlayBlockCount, &overlayPenaltyCount )
				&& Nav_Hierarchy_MaterializeRefineCorridor( mesh, regionPath, portalPath, overlayBlockCount, overlayPenaltyCount, out_corridor ) ) {
				if ( out_used_hierarchy ) {
					*out_used_hierarchy = true;
				}
				if ( out_coarse_expansions ) {
					*out_coarse_expansions = coarseExpansions;
				}
				return true;
			}
		}
	}

	/** 
	*	Fall back to the legacy cluster route pre-pass by wrapping it in the same explicit corridor representation.
	**/
	std::vector<nav_tile_cluster_key_t> clusterRoute;
	if ( SVG_Nav_ClusterGraph_FindRoute( mesh, start_node.worldPosition, goal_node.worldPosition, clusterRoute )
		&& Nav_Cluster_MaterializeRefineCorridor( clusterRoute, out_corridor ) ) {
		if ( out_used_cluster ) {
			*out_used_cluster = true;
		}
		if ( out_coarse_expansions ) {
			*out_coarse_expansions = ( int32_t )clusterRoute.size();
		}
		return true;
	}

	return false;
}

/** 
*	@brief    Build a coarse tile-route filter from hierarchy routing with legacy cluster fallback.
*	@param    mesh                    Navigation mesh containing hierarchy and cluster data.
*	@param    start_node              Resolved canonical start node.
*	@param    goal_node               Resolved canonical goal node.
*	@param    policy                  Optional traversal policy controlling coarse portal gating.
*	@param    out_tile_route          [out] Materialized tile-key route usable by fine A* .
*	@param    out_used_hierarchy      [out] True when hierarchy routing produced the returned route.
*	@param    out_used_cluster        [out] True when legacy cluster routing produced the returned route.
*	@param    out_coarse_expansions   [out] Coarse expansion count reported for diagnostics.
*	@return   True when a coarse route filter was produced.
**/
bool SVG_Nav_BuildCoarseRouteFilter( const nav_mesh_t * mesh, const nav_node_ref_t &start_node, const nav_node_ref_t &goal_node,
	const svg_nav_path_policy_t * policy, std::vector<nav_tile_cluster_key_t> * out_tile_route,
	bool * out_used_hierarchy, bool * out_used_cluster, int32_t * out_coarse_expansions ) {
	/** 
	*	Sanity checks: require mesh and output storage before attempting coarse routing.
	**/
	if ( !mesh || !out_tile_route ) {
		return false;
	}

	/** 
	*	Build the explicit refinement corridor first, then expose only its exact tile route for legacy callers.
	**/
    nav_refine_corridor_t corridor = {};
	if ( SVG_Nav_BuildRefineCorridor( mesh, start_node, goal_node, policy, &corridor,
		out_used_hierarchy, out_used_cluster, out_coarse_expansions ) ) {
		*out_tile_route = corridor.exact_tile_route;
		return !out_tile_route->empty();
	}

	return false;
}

/** 
*	@brief    Record one LOS attempt into the active sync query stats buffer.
*	@param    stats    Active query stats buffer.
*	@param    success  True when the LOS query succeeded.
**/
static void Nav_QueryStats_RecordLosResult( nav_query_debug_stats_t * stats, const bool success ) {
	/** 
	*	Ignore calls that do not have an active stats buffer.
	**/
	if ( !stats ) {
		return;
	}

	/** 
	*	Count every LOS query attempt and track successful direct traversals separately.
	**/
	stats->los_tests++;
	if ( success ) {
		stats->los_successes++;
	}
}

/** 
*	@brief    Compute the LOS sample spacing for the current mesh and request.
*	@param    mesh     Navigation mesh supplying the base cell size.
*	@param    request  Optional LOS request override.
*	@return   Positive world-space sample spacing used by LOS backends.
**/
static double Nav_LOS_ComputeSampleStep( const nav_mesh_t * mesh, const nav_los_request_t * request ) {
	/** 
	*	Start from the nav cell size because LOS should align with the existing fine navigation grid.
	**/
	const double baseStep = ( mesh && mesh->cell_size_xy > 0.0 ) ? mesh->cell_size_xy : 4.0;
	const double scale = ( request && request->sample_step_scale > 0.0 ) ? request->sample_step_scale : 1.0;
	return std::max( 1.0, baseStep* scale );
}

/** 
*	@brief    Compute a bounded sample count for LOS evaluation between two canonical points.
*	@param    start_point   LOS start point in nav-center space.
*	@param    goal_point    LOS goal point in nav-center space.
*	@param    sample_step   Positive world-space sample spacing.
*	@return   Number of interpolation samples to evaluate.
**/
static int32_t Nav_LOS_ComputeSampleCount( const Vector3 &start_point, const Vector3 &goal_point, const double sample_step ) {
	/** 
	*	Compute the straight-line segment length before converting it into a bounded sample count.
	**/
	const Vector3 delta = QM_Vector3Subtract( goal_point, start_point );
	const double distance = std::sqrt( ( delta[ 0 ]* delta[ 0 ] ) + ( delta[ 1 ]* delta[ 1 ] ) + ( delta[ 2 ]* delta[ 2 ] ) );
	if ( distance <= 0.001 ) {
		return 1;
	}

	/** 
	*	Bound the sample count so a malformed request cannot explode LOS work on long segments.
	**/
	const int32_t sampleCount = ( int32_t )std::ceil( distance / std::max( sample_step, 1.0 ) );
	return std::clamp( sampleCount, 1, 16384 );
}

/** 
*	@brief    Decide whether a direct LOS shortcut attempt is cheap enough to run for this query.
*	@param    mesh         Navigation mesh supplying the LOS sample spacing.
*	@param    start_node   Canonical LOS start node.
*	@param    goal_node    Canonical LOS goal node.
*	@return   True when the configured LOS sample budget allows the direct shortcut attempt.
**/
static bool Nav_ShouldAttemptDirectLosShortcut( const nav_mesh_t * mesh, const nav_node_ref_t &start_node, const nav_node_ref_t &goal_node ) {
	/** 
	*	Sanity checks: require mesh data before estimating LOS sample cost.
	**/
	if ( !mesh ) {
		return false;
	}

	/** 
	*	Treat zero or negative limits as "always attempt" so tuning can disable this gate explicitly.
	**/
	const int32_t maxSamples = nav_direct_los_attempt_max_samples ? nav_direct_los_attempt_max_samples->integer : NAV_DEFAULT_DIRECT_LOS_ATTEMPT_MAX_SAMPLES;
	if ( maxSamples <= 0 ) {
		return true;
	}

	/** 
	*	Estimate the direct LOS work using the same sample spacing logic as the DDA backend.
	**/
	const double sampleStep = Nav_LOS_ComputeSampleStep( mesh, nullptr );
	const int32_t sampleCount = Nav_LOS_ComputeSampleCount( start_node.worldPosition, goal_node.worldPosition, sampleStep );
	return sampleCount <= maxSamples;
}

/** 
*	@brief    Require additional clearance margin before allowing long LOS spans to collapse in narrow passages.
*	@param    mesh         Navigation mesh containing canonical layer clearance metadata.
*	@param    start_node   Canonical LOS start node.
*	@param    goal_node    Canonical LOS goal node.
*	@param    agent_mins   Agent bbox minimums in nav-center space.
*	@param    agent_maxs   Agent bbox maximums in nav-center space.
*	@return   True when the long LOS span has enough endpoint clearance margin, false when it should be treated as too narrow.
*	@note     Vertical traversal semantics are still enforced by the shared 3D LOS step validation; this helper only vetoes long narrow-passage collapses.
**/
static bool Nav_LOS_HasClearanceMarginForLongSpan( const nav_mesh_t * mesh, const nav_node_ref_t &start_node,
	const nav_node_ref_t &goal_node, const Vector3 &agent_mins, const Vector3 &agent_maxs ) {
	/** 
	*	Sanity checks: require mesh data before reading canonical layer clearance.
	**/
	if ( !mesh ) {
		return false;
	}

	/** 
	*	Short spans remain eligible because they do not meaningfully collapse long narrow corridors.
	**/
	const Vector3 horizontalDelta = {
		goal_node.worldPosition.x - start_node.worldPosition.x,
		goal_node.worldPosition.y - start_node.worldPosition.y,
		0.0f
	};
	const double horizontalDistance = std::sqrt( ( horizontalDelta.x* horizontalDelta.x ) + ( horizontalDelta.y* horizontalDelta.y ) );
    const double longSpanThreshold = nav_simplify_long_span_min_distance
		? std::max( 0.0, ( double )nav_simplify_long_span_min_distance->value )
		: std::max( 64.0, mesh->cell_size_xy* 3.0 );
	if ( horizontalDistance <= longSpanThreshold ) {
		return true;
	}

	/** 
	*	Resolve both endpoint layers so the long-span collapse can inspect actual vertical clearance.
	**/
	const nav_tile_t * start_tile = nullptr;
	const nav_xy_cell_t * start_cell = nullptr;
	const nav_layer_t * start_layer = nullptr;
	const nav_tile_t * goal_tile = nullptr;
	const nav_xy_cell_t * goal_cell = nullptr;
	const nav_layer_t * goal_layer = nullptr;
	if ( !Nav_TryGetNodeLayerView( mesh, start_node, &start_tile, &start_cell, &start_layer )
		|| !Nav_TryGetNodeLayerView( mesh, goal_node, &goal_tile, &goal_cell, &goal_layer ) ) {
		return false;
	}

	/** 
	*	Require a modest extra margin above the agent hull height before allowing a long LOS collapse.
	**/
	const double requiredClearance = std::max( 0.0, ( double )agent_maxs[ 2 ] - ( double )agent_mins[ 2 ] );
    const double clearanceMargin = nav_simplify_clearance_margin
		? std::max( 0.0, ( double )nav_simplify_clearance_margin->value )
		: std::max( ( double )PHYS_STEP_GROUND_DIST, std::max( mesh->z_quant, mesh->cell_size_xy* 0.5 ) );
	const double startClearance = Nav_GetLayerClearanceWorld( mesh, * start_layer );
	const double goalClearance = Nav_GetLayerClearanceWorld( mesh, * goal_layer );
	return startClearance >= ( requiredClearance + clearanceMargin )
		&& goalClearance >= ( requiredClearance + clearanceMargin );
}

/** 
*	@brief    Check whether a canonical LOS sample node satisfies policy and occupancy constraints.
*	@param    mesh     Navigation mesh owning the node.
*	@param    node     Canonical sample node to evaluate.
*	@param    policy   Optional traversal policy controlling hazard and occupancy behavior.
*	@param    request  Optional LOS request controlling occupancy participation.
*	@return   True when the node is acceptable for direct LOS traversal.
**/
static bool Nav_LOS_NodeAllowed( const nav_mesh_t * mesh, const nav_node_ref_t &node, const svg_nav_path_policy_t * policy, const nav_los_request_t * request ) {
	/** 
	*	Sanity checks: require mesh ownership before reading traversal metadata or occupancy overlays.
	**/
	if ( !mesh ) {
		return false;
	}

	/** 
	*	Respect hazard-forbid policy bits using the persisted traversal feature metadata on the canonical node.
	**/
	const uint32_t traversalBits = SVG_Nav_GetNodeTraversalFeatureBits( mesh, node );
	if ( policy ) {
		if ( policy->forbid_water && ( traversalBits & NAV_TRAVERSAL_FEATURE_WATER ) != 0 ) {
			return false;
		}
		if ( policy->forbid_lava && ( traversalBits & NAV_TRAVERSAL_FEATURE_LAVA ) != 0 ) {
			return false;
		}
		if ( policy->forbid_slime && ( traversalBits & NAV_TRAVERSAL_FEATURE_SLIME ) != 0 ) {
			return false;
		}
	}

	/** 
	*	Treat sparse occupancy as a direct-shortcut veto when dynamic occupancy is enabled.
	*			LOS cannot express soft local cost pressure, so any relevant overlay entry must fall back
	*			to corridor refinement where soft-cost steering can still influence the result.
	**/
	const bool consultDynamicOccupancy = ( !request || request->consult_dynamic_occupancy ) && ( !policy || policy->use_dynamic_occupancy );
  if ( consultDynamicOccupancy ) {
		nav_occupancy_entry_t occupancy = {};
		if ( SVG_Nav_Occupancy_TryGet( mesh, node.key.tile_index, node.key.cell_index, node.key.layer_index, &occupancy ) ) {
			const double occupancySoftScale = policy ? std::max( 0.0, policy->dynamic_occupancy_soft_cost_scale ) : 1.0;
			const int32_t effectiveSoftCost = occupancy.blocked ? std::max( occupancy.soft_cost, 1 ) : occupancy.soft_cost;

			/** 
			*	Hard-block requests always reject LOS immediately when the overlay marks the node blocked.
			**/
			if ( occupancy.blocked && ( !policy || policy->hard_block_dynamic_occupancy ) ) {
				Nav_QueryStats_RecordOccupancyLosReject( s_nav_active_query_stats );
				return false;
			}

			/** 
			*	Any remaining soft occupancy pressure must also veto LOS so refinement can honor the overlay cost locally.
			**/
			if ( effectiveSoftCost > 0 && occupancySoftScale > 0.0 ) {
				Nav_QueryStats_RecordOccupancyLosReject( s_nav_active_query_stats );
				return false;
			}
		}
	}

	return true;
}

/** 
*	@brief    Run nav-grid DDA LOS over canonical node samples.
*	@param    mesh         Navigation mesh containing the canonical world grid.
*	@param    start_node   Canonical LOS start node.
*	@param    goal_node    Canonical LOS goal node.
*	@param    agent_mins   Agent bbox minimums in nav-center space.
*	@param    agent_maxs   Agent bbox maximums in nav-center space.
*	@param    policy       Optional traversal policy controlling hazards and occupancy.
*	@param    request      Optional LOS request tuning.
*	@param    out_result   [out] LOS work summary being populated.
*	@return   True when the straight segment remains traversable under DDA sampling.
**/
static bool Nav_LOS_DDA( const nav_mesh_t * mesh, const nav_node_ref_t &start_node, const nav_node_ref_t &goal_node,
	const Vector3 &agent_mins, const Vector3 &agent_maxs, const svg_nav_path_policy_t * policy,
	const nav_los_request_t * request, nav_los_result_t * out_result ) {
	/** 
	*	Sanity checks: require mesh and result storage before evaluating DDA samples.
	**/
	if ( !mesh || !out_result ) {
		return false;
	}

	/** 
	*	Reject endpoints that already violate policy or occupancy semantics.
	**/
	if ( !Nav_LOS_NodeAllowed( mesh, start_node, policy, request ) || !Nav_LOS_NodeAllowed( mesh, goal_node, policy, request ) ) {
		return false;
	}

	/** 
	*	Walk the direct segment at nav-grid spacing and validate each canonical sample hop.
	**/
	const double sampleStep = Nav_LOS_ComputeSampleStep( mesh, request );
	const int32_t sampleCount = Nav_LOS_ComputeSampleCount( start_node.worldPosition, goal_node.worldPosition, sampleStep );
	nav_node_ref_t previousNode = start_node;
	for ( int32_t sampleIndex = 1; sampleIndex < sampleCount; sampleIndex++ ) {
		const double t = ( double )sampleIndex / ( double )sampleCount;
		const Vector3 samplePoint = {
			( float )( start_node.worldPosition.x + ( goal_node.worldPosition.x - start_node.worldPosition.x )* t ),
			( float )( start_node.worldPosition.y + ( goal_node.worldPosition.y - start_node.worldPosition.y )* t ),
			( float )( start_node.worldPosition.z + ( goal_node.worldPosition.z - start_node.worldPosition.z )* t )
		};
		out_result->sample_count++;

		nav_node_ref_t sampleNode = {};
		if ( !Nav_FindNodeForPosition( mesh, samplePoint, samplePoint[ 2 ], &sampleNode, true ) ) {
			return false;
		}
		out_result->node_resolve_count++;
		if ( !Nav_LOS_NodeAllowed( mesh, sampleNode, policy, request ) ) {
			return false;
		}

		/** 
		*	Only validate a step chunk when the canonical sample actually moved to a new node.
		**/
		if ( sampleNode.key != previousNode.key ) {
			if ( !Nav_CanTraverseStep_ExplicitBBox_NodeRefs( mesh, previousNode, sampleNode, agent_mins, agent_maxs, nullptr, policy ) ) {
				return false;
			}
			previousNode = sampleNode;
		}
	}

	/** 
	*	Finish by validating the final hop into the goal node when it differs from the last sample.
	**/
	if ( goal_node.key != previousNode.key ) {
		if ( !Nav_CanTraverseStep_ExplicitBBox_NodeRefs( mesh, previousNode, goal_node, agent_mins, agent_maxs, nullptr, policy ) ) {
			return false;
		}
	}

	return true;
}

/** 
*	@brief    Run conservative leaf-tracking LOS between canonical nodes.
*	@param    mesh         Navigation mesh containing the canonical leaf/tile linkage.
*	@param    start_node   Canonical LOS start node.
*	@param    goal_node    Canonical LOS goal node.
*	@param    agent_mins   Agent bbox minimums in nav-center space.
*	@param    agent_maxs   Agent bbox maximums in nav-center space.
*	@param    policy       Optional traversal policy controlling hazards and occupancy.
*	@param    request      Optional LOS request tuning.
*	@param    out_result   [out] LOS work summary being populated.
*	@return   True when the direct segment remains traversable under canonical leaf tracking.
**/
static bool Nav_LOS_LeafTraversal( const nav_mesh_t * mesh, const nav_node_ref_t &start_node, const nav_node_ref_t &goal_node,
	const Vector3 &agent_mins, const Vector3 &agent_maxs, const svg_nav_path_policy_t * policy,
	const nav_los_request_t * request, nav_los_result_t * out_result ) {
	/** 
	*	Sanity checks: require mesh and result storage before evaluating leaf samples.
	**/
	if ( !mesh || !out_result ) {
		return false;
	}

	/** 
	*	Reject endpoints that already violate policy or occupancy semantics.
	**/
	if ( !Nav_LOS_NodeAllowed( mesh, start_node, policy, request ) || !Nav_LOS_NodeAllowed( mesh, goal_node, policy, request ) ) {
		return false;
	}

	/** 
	*	First confirm the whole straight segment is physically traversable under the existing direct step validator.
	**/
	if ( !Nav_CanTraverseStep_ExplicitBBox_NodeRefs( mesh, start_node, goal_node, agent_mins, agent_maxs, nullptr, policy ) ) {
		return false;
	}

	/** 
	*	Sample canonical nodes along the segment and track their leaf ids, resolving work only when the sampled leaf changes.
	**/
	const double sampleStep = Nav_LOS_ComputeSampleStep( mesh, request );
	const int32_t sampleCount = Nav_LOS_ComputeSampleCount( start_node.worldPosition, goal_node.worldPosition, sampleStep );
	int32_t previousLeafIndex = -1;
	for ( int32_t sampleIndex = 0; sampleIndex <= sampleCount; sampleIndex++ ) {
		const double t = ( double )sampleIndex / ( double )sampleCount;
		const Vector3 samplePoint = {
			( float )( start_node.worldPosition.x + ( goal_node.worldPosition.x - start_node.worldPosition.x )* t ),
			( float )( start_node.worldPosition.y + ( goal_node.worldPosition.y - start_node.worldPosition.y )* t ),
			( float )( start_node.worldPosition.z + ( goal_node.worldPosition.z - start_node.worldPosition.z )* t )
		};
		out_result->sample_count++;

		nav_node_ref_t sampleNode = {};
		if ( !Nav_FindNodeForPosition( mesh, samplePoint, samplePoint[ 2 ], &sampleNode, true ) ) {
			return false;
		}
		out_result->node_resolve_count++;
		if ( !Nav_LOS_NodeAllowed( mesh, sampleNode, policy, request ) ) {
			return false;
		}

		const int32_t leafIndex = sampleNode.key.leaf_index;
		if ( leafIndex == previousLeafIndex ) {
			continue;
		}

		previousLeafIndex = leafIndex;
		out_result->leaf_visit_count++;
		if ( leafIndex < 0 || leafIndex >= mesh->num_leafs || mesh->leaf_data[ leafIndex ].num_tiles <= 0 ) {
			return false;
		}
	}

	return true;
}

/** 
*	@brief    Evaluate a reusable LOS accelerator query between two canonical nodes.
*	@param    mesh         Navigation mesh containing canonical world tiles and BSP linkage.
*	@param    start_node   Canonical LOS start node.
*	@param    goal_node    Canonical LOS goal node.
*	@param    agent_mins   Agent bbox minimums in nav-center space.
*	@param    agent_maxs   Agent bbox maximums in nav-center space.
*	@param    policy       Optional traversal policy controlling hazards and occupancy.
*	@param    request      Optional LOS mode/sample configuration.
*	@param    out_result   [out] Optional LOS result payload describing the backend work performed.
*	@return   True when the direct LOS segment is considered traversable.
*	@note     This is a reusable accelerator for future shortcutting phases, not a replacement pathfinder.
**/
bool SVG_Nav_HasLineOfSight( const nav_mesh_t * mesh, const nav_node_ref_t &start_node, const nav_node_ref_t &goal_node,
	const Vector3 &agent_mins, const Vector3 &agent_maxs, const svg_nav_path_policy_t * policy,
	const nav_los_request_t * request, nav_los_result_t * out_result ) {
	/** 
	*	Initialize the output payload eagerly so callers always receive a stable snapshot.
	**/
	nav_los_result_t localResult = {};
	if ( request ) {
		localResult.requested_mode = request->mode;
		localResult.resolved_mode = request->mode;
	}

	/** 
	*	Sanity checks: require mesh ownership before any LOS backend is selected.
	**/
	if ( !mesh ) {
		Nav_QueryStats_RecordLosResult( s_nav_active_query_stats, false );
		if ( out_result ) {
			*out_result = localResult;
		}
		return false;
	}

	/** 
	*	Treat identical canonical endpoints as trivially visible once policy and occupancy accept the node.
	**/
	if ( start_node.key == goal_node.key ) {
		const bool success = Nav_LOS_NodeAllowed( mesh, start_node, policy, request );
		localResult.has_line_of_sight = success;
		localResult.resolved_mode = request ? request->mode : nav_los_mode_t::Auto;
		Nav_QueryStats_RecordLosResult( s_nav_active_query_stats, success );
		if ( out_result ) {
			*out_result = localResult;
		}
		return success;
	}

	/** 
	*	Resolve the backend mode, preferring DDA when nav-grid data is available.
	**/
	const nav_los_mode_t requestedMode = request ? request->mode : nav_los_mode_t::Auto;
	nav_los_mode_t resolvedMode = requestedMode;
   if ( resolvedMode == nav_los_mode_t::Auto ) {
		resolvedMode = ( mesh->cell_size_xy > 0.0 && mesh->tile_size > 0 ) ? nav_los_mode_t::DDA : nav_los_mode_t::LeafTraversal;
	}

 // When callers left the mode as Auto but DDA was chosen, use a tunable
	// conservative heuristic to prefer LeafTraversal when the segment clearly
	// involves vertical transitions, ladders, or stair/portal semantics.
	if ( requestedMode == nav_los_mode_t::Auto && resolvedMode == nav_los_mode_t::DDA
		&& nav_los_auto_leaf_heuristics && nav_los_auto_leaf_heuristics->integer != 0 ) {
		bool preferLeaf = false;

		// Large vertical delta suggests the trace must respect per-leaf vertical
		// semantics (stairs, ladders, multi-level portals).
		const double verticalDelta = std::fabs( ( double )goal_node.worldPosition.z - ( double )start_node.worldPosition.z );
		const double maxShortcutVerticalDelta = Nav_DirectLosShortcutMaxVerticalDelta( mesh, policy );
		if ( verticalDelta > maxShortcutVerticalDelta ) {
			preferLeaf = true;
		}

		// If either endpoint advertises ladders or stair presence, prefer leaf traversal
		// so portal/leaf transitions and ladder endpoints are handled conservatively.
		const uint32_t startTraversal = SVG_Nav_GetNodeTraversalFeatureBits( mesh, start_node );
		const uint32_t goalTraversal = SVG_Nav_GetNodeTraversalFeatureBits( mesh, goal_node );
		const uint32_t traversalFlags = startTraversal | goalTraversal;
		if ( ( traversalFlags & ( NAV_TRAVERSAL_FEATURE_LADDER | NAV_TRAVERSAL_FEATURE_STAIR_PRESENCE ) ) != 0 ) {
			preferLeaf = true;
		}

		// Tile-level summaries can also indicate ladder/stair semantics that warrant
		// the conservative backend.
		const uint32_t startTileSummary = SVG_Nav_GetTileSummaryBitsForNode( mesh, start_node );
		const uint32_t goalTileSummary = SVG_Nav_GetTileSummaryBitsForNode( mesh, goal_node );
		if ( ( ( startTileSummary | goalTileSummary ) & ( NAV_TILE_SUMMARY_LADDER | NAV_TILE_SUMMARY_STAIR ) ) != 0 ) {
			preferLeaf = true;
		}

		if ( preferLeaf ) {
			resolvedMode = nav_los_mode_t::LeafTraversal;
		}
	}

	localResult.requested_mode = requestedMode;
	localResult.resolved_mode = resolvedMode;

	/** 
	*	Dispatch to the selected backend and then publish instrumentation/output state.
	**/
	bool success = false;
	if ( resolvedMode == nav_los_mode_t::LeafTraversal ) {
		success = Nav_LOS_LeafTraversal( mesh, start_node, goal_node, agent_mins, agent_maxs, policy, request, &localResult );
	} else {
		success = Nav_LOS_DDA( mesh, start_node, goal_node, agent_mins, agent_maxs, policy, request, &localResult );
	}

	localResult.has_line_of_sight = success;
	Nav_QueryStats_RecordLosResult( s_nav_active_query_stats, success );
	if ( out_result ) {
		*out_result = localResult;
	}
	return success;
}

/** 
*	@brief    Greedily simplify a finalized nav-center waypoint list using the shared LOS API.
*	@param    mesh            Navigation mesh used to resolve canonical waypoint nodes.
*	@param    agent_mins      Agent bbox minimums in nav-center space.
*	@param    agent_maxs      Agent bbox maximums in nav-center space.
*	@param    policy          Optional traversal policy controlling hazards and occupancy checks.
*	@param    inout_points    [in,out] Finalized nav-center waypoint list to simplify in place.
*	@param    out_stats       [out] Optional simplification attempt/success counters.
*	@return   True when the waypoint list was shortened, false when no safe collapse was found.
*	@note     This is a post-refinement path simplifier, not a replacement pathfinder.
**/
bool SVG_Nav_SimplifyPathPoints( const nav_mesh_t * mesh, const Vector3 &agent_mins, const Vector3 &agent_maxs,
	const svg_nav_path_policy_t * policy, std::vector<Vector3> * inout_points, const nav_path_simplify_options_t * options,
	nav_path_simplify_stats_t * out_stats ) {
	/**
	*	Initialize output counters eagerly so callers always receive a stable snapshot.
	**/
	nav_path_simplify_stats_t localStats = {};
	localStats.input_waypoint_count = inout_points ? ( int32_t )inout_points->size() : 0;
	if ( out_stats ) {
		*out_stats = {};
	}
	const uint64_t simplifyStartMs = gi.GetRealSystemTime();

	/**
	*	Build the effective simplification options, keeping sync defaults conservative when the caller does not override them.
	**/
	nav_path_simplify_options_t simplifyOptions = {};
	if ( options ) {
		simplifyOptions = *options;
	} else {
		simplifyOptions.max_los_tests = nav_simplify_sync_max_los_tests ? nav_simplify_sync_max_los_tests->integer : 2;
		simplifyOptions.max_elapsed_ms = ( uint64_t )( nav_simplify_sync_max_ms ? std::max( 0, nav_simplify_sync_max_ms->integer ) : 1 );
		simplifyOptions.collinear_angle_degrees = nav_simplify_collinear_angle_degrees ? ( double )nav_simplify_collinear_angle_degrees->value : 4.0;
		simplifyOptions.remove_duplicates = true;
		simplifyOptions.prune_collinear = true;
		simplifyOptions.aggressiveness = nav_path_simplify_aggressiveness_t::SyncConservative;
	}

	/**
	*	Sanity checks: require mesh and mutable waypoint storage before attempting simplification.
	**/
	if ( !mesh || !inout_points || inout_points->size() <= 2 ) {
		localStats.output_waypoint_count = localStats.input_waypoint_count;
		localStats.total_ms = ( double )( gi.GetRealSystemTime() - simplifyStartMs );
		if ( out_stats ) {
			*out_stats = localStats;
		}
		return false;
	}

	/**
	*	Start from a mutable working copy so cleanup stages can prune duplicates and near-collinear points before LOS tests.
	**/
	std::vector<Vector3> workingPoints = *inout_points;

	/**
	*	Remove consecutive duplicate waypoints first so later collinearity and LOS checks do not spend budget on trivial repeats.
	**/
	if ( simplifyOptions.remove_duplicates && workingPoints.size() > 1 ) {
		std::vector<Vector3> deduplicatedPoints;
		deduplicatedPoints.reserve( workingPoints.size() );
		deduplicatedPoints.push_back( workingPoints.front() );
		for ( size_t pointIndex = 1; pointIndex < workingPoints.size(); pointIndex++ ) {
			const Vector3 delta = QM_Vector3Subtract( workingPoints[ pointIndex ], deduplicatedPoints.back() );
			const double dist2 = ( delta.x * delta.x ) + ( delta.y * delta.y ) + ( delta.z * delta.z );
			if ( dist2 <= ( 0.001 * 0.001 ) ) {
				localStats.duplicate_prunes++;
				continue;
			}
			deduplicatedPoints.push_back( workingPoints[ pointIndex ] );
		}
		workingPoints.swap( deduplicatedPoints );
	}

	/**
	*	Prune nearly-collinear interior points next so the sync pass gets cheap waypoint reduction even before LOS collapsing.
	**/
	if ( simplifyOptions.prune_collinear && workingPoints.size() > 2 ) {
		std::vector<Vector3> prunedPoints;
		prunedPoints.reserve( workingPoints.size() );
		prunedPoints.push_back( workingPoints.front() );
		const double angleRadians = std::max( 0.0, simplifyOptions.collinear_angle_degrees ) * ( 3.14159265358979323846 / 180.0 );
		const double dotThreshold = std::cos( angleRadians );
		for ( size_t pointIndex = 1; pointIndex + 1 < workingPoints.size(); pointIndex++ ) {
			const Vector3 prevDelta = QM_Vector3Subtract( workingPoints[ pointIndex ], prunedPoints.back() );
			const Vector3 nextDelta = QM_Vector3Subtract( workingPoints[ pointIndex + 1 ], workingPoints[ pointIndex ] );
			const double prevLen = std::sqrt( ( prevDelta.x * prevDelta.x ) + ( prevDelta.y * prevDelta.y ) + ( prevDelta.z * prevDelta.z ) );
			const double nextLen = std::sqrt( ( nextDelta.x * nextDelta.x ) + ( nextDelta.y * nextDelta.y ) + ( nextDelta.z * nextDelta.z ) );
			if ( prevLen <= 0.001 || nextLen <= 0.001 ) {
				localStats.collinear_prunes++;
				continue;
			}
			const double dot = ( ( prevDelta.x * nextDelta.x ) + ( prevDelta.y * nextDelta.y ) + ( prevDelta.z * nextDelta.z ) ) / ( prevLen * nextLen );
			if ( dot >= dotThreshold ) {
				localStats.collinear_prunes++;
				continue;
			}
			prunedPoints.push_back( workingPoints[ pointIndex ] );
		}
		prunedPoints.push_back( workingPoints.back() );
		workingPoints.swap( prunedPoints );
	}

	/**
	*	Publish the cleanup-only result immediately when the path became trivial before any LOS collapsing was needed.
	**/
	if ( workingPoints.size() <= 2 ) {
		localStats.output_waypoint_count = ( int32_t )workingPoints.size();
		localStats.total_ms = ( double )( gi.GetRealSystemTime() - simplifyStartMs );
		if ( out_stats ) {
			*out_stats = localStats;
		}
		if ( workingPoints.size() < inout_points->size() ) {
			*inout_points = std::move( workingPoints );
			return true;
		}
		return false;
	}

	/**
	*	Cache canonical waypoint nodes so repeated greedy LOS tests do not repeatedly resolve the same points.
	**/
	std::vector<nav_node_ref_t> cachedNodes( workingPoints.size() );
	std::vector<uint8_t> cachedNodeValidity( workingPoints.size(), 0 );
	auto resolvePathPointNode = [&]( const size_t pointIndex, nav_node_ref_t *out_node ) -> bool {
		/**
		*	Reuse the cached node when this path point was already resolved earlier in the simplification pass.
		**/
		if ( pointIndex >= cachedNodes.size() || !out_node ) {
			return false;
		}
		if ( cachedNodeValidity[ pointIndex ] != 0 ) {
			*out_node = cachedNodes[ pointIndex ];
			return true;
		}

		/**
		*	Resolve the canonical node directly from the finalized nav-center waypoint position.
		**/
		nav_node_ref_t resolvedNode = {};
		if ( !Nav_FindNodeForPosition( mesh, workingPoints[ pointIndex ], workingPoints[ pointIndex ][ 2 ], &resolvedNode, true ) ) {
			return false;
		}

		cachedNodes[ pointIndex ] = resolvedNode;
		cachedNodeValidity[ pointIndex ] = 1;
		*out_node = resolvedNode;
		return true;
		};

		/**
		*	Greedily keep the farthest visible point from each anchor so intermediate nodes collapse whenever LOS permits.
		**/
	const nav_los_request_t losRequest = { .mode = nav_los_mode_t::DDA };
	std::vector<Vector3> simplifiedPoints;
	simplifiedPoints.reserve( workingPoints.size() );
	simplifiedPoints.push_back( workingPoints.front() );
	bool shortened = workingPoints.size() < inout_points->size();
	size_t anchorIndex = 0;
	const size_t pointCount = workingPoints.size();
	auto hasLosBudgetRemaining = [&]() -> bool {
		if ( simplifyOptions.max_los_tests > 0 && localStats.attempts >= simplifyOptions.max_los_tests ) {
			return false;
		}
		if ( simplifyOptions.max_elapsed_ms > 0 && ( gi.GetRealSystemTime() - simplifyStartMs ) >= simplifyOptions.max_elapsed_ms ) {
			return false;
		}
		return true;
		};
	auto tryLosCandidate = [&]( const nav_node_ref_t &anchorNode, const size_t candidateIndex ) -> bool {
		nav_node_ref_t candidateNode = {};
		if ( !resolvePathPointNode( candidateIndex, &candidateNode ) ) {
			return false;
		}

		/**
		*	Reject long narrow-passage collapses unless both endpoints provide enough clearance margin.
		**/
		if ( !Nav_LOS_HasClearanceMarginForLongSpan( mesh, anchorNode, candidateNode, agent_mins, agent_maxs ) ) {
			return false;
		}

		localStats.attempts++;
		if ( SVG_Nav_HasLineOfSight( mesh, anchorNode, candidateNode, agent_mins, agent_maxs, policy, &losRequest, nullptr ) ) {
			localStats.successes++;
			return true;
		}
		return false;
		};
	while ( anchorIndex + 1 < pointCount ) {
		/**
		*	Default to preserving the next waypoint unless a longer LOS span proves safe.
		**/
		size_t chosenIndex = anchorIndex + 1;
		nav_node_ref_t anchorNode = {};
		if ( resolvePathPointNode( anchorIndex, &anchorNode ) ) {
		 /**
			*	Always try the goal first when budget allows; this is the cheapest high-value simplification win.
			**/
			if ( pointCount - 1 > anchorIndex + 1 && hasLosBudgetRemaining() && tryLosCandidate( anchorNode, pointCount - 1 ) ) {
				chosenIndex = pointCount - 1;
				shortened = true;
			} else if ( simplifyOptions.aggressiveness == nav_path_simplify_aggressiveness_t::AsyncAggressive ) {
				/**
				*	Async mode uses an exponential farthest-jump search with bounded binary refinement to collapse more aggressively without blocking the query.
				**/
				size_t farthestVisible = anchorIndex + 1;
				size_t firstFailure = pointCount - 1;
				bool sawFailure = false;
				for ( size_t step = 2; anchorIndex + step < pointCount && hasLosBudgetRemaining(); step <<= 1 ) {
					const size_t candidateIndex = std::min( pointCount - 1, anchorIndex + step );
					if ( tryLosCandidate( anchorNode, candidateIndex ) ) {
						farthestVisible = candidateIndex;
						if ( candidateIndex == pointCount - 1 ) {
							break;
						}
					} else {
						firstFailure = candidateIndex;
						sawFailure = true;
						break;
					}
				}
				if ( sawFailure ) {
					size_t left = farthestVisible + 1;
					size_t right = ( firstFailure > 0 ) ? firstFailure - 1 : farthestVisible;
					while ( left <= right && hasLosBudgetRemaining() ) {
						const size_t mid = left + ( ( right - left ) / 2 );
						if ( tryLosCandidate( anchorNode, mid ) ) {
							farthestVisible = mid;
							left = mid + 1;
						} else {
							if ( mid == 0 ) {
								break;
							}
							right = mid - 1;
						}
					}
				}
				if ( farthestVisible > anchorIndex + 1 ) {
					chosenIndex = farthestVisible;
					shortened = true;
				}
			} else if ( pointCount - 1 > anchorIndex + 2 && hasLosBudgetRemaining() ) {
				/**
				*	Sync mode stays intentionally small: after the goal attempt, try at most one intermediate farthest-jump candidate.
				**/
				const size_t candidateIndex = anchorIndex + std::max<size_t>( 2, ( pointCount - 1 - anchorIndex ) / 2 );
				if ( candidateIndex > anchorIndex + 1 && tryLosCandidate( anchorNode, candidateIndex ) ) {
					chosenIndex = candidateIndex;
					shortened = true;
				}
			}
		}

		/**
		*	Commit the chosen next waypoint and continue simplifying from that new anchor.
		**/
		simplifiedPoints.push_back( workingPoints[ chosenIndex ] );
		anchorIndex = chosenIndex;
	}

	/**
	*	Publish counters for callers and apply the simplified path only when it is strictly shorter.
	**/
	localStats.output_waypoint_count = shortened ? ( int32_t )simplifiedPoints.size() : ( int32_t )workingPoints.size();
	localStats.total_ms = ( double )( gi.GetRealSystemTime() - simplifyStartMs );
	if ( out_stats ) {
		*out_stats = localStats;
	}
	if ( shortened && simplifiedPoints.size() < inout_points->size() ) {
		*inout_points = std::move( simplifiedPoints );
		return true;
	}
	if ( workingPoints.size() < inout_points->size() ) {
		*inout_points = std::move( workingPoints );
		return true;
	}

	return false;
}

/** 
*	@brief	Return the last recorded sync query stats snapshot.
*	@return	Const reference to the cached sync query stats.
**/
const nav_query_debug_stats_t &SVG_Nav_GetLastQueryStats( void ) {
	return s_nav_last_query_stats;
}

static void Nav_Debug_PrintNodeRef( const char * label, const nav_node_ref_t &n ) {
	if ( !label ) {
		label = "node";
	}

	gi.dprintf( "[DEBUG][NavPath][Diag] %s: pos(%.1f %.1f %.1f) key(leaf=%d tile=%d cell=%d layer=%d)\n",
		label,
		n.worldPosition.x, n.worldPosition.y, n.worldPosition.z,
		n.key.leaf_index, n.key.tile_index, n.key.cell_index, n.key.layer_index );
}

/** 
*	@brief	Convert an edge rejection enum value into a stable diagnostic string.
*	@param	reason	Edge rejection reason to stringify.
*	@return	Constant string suitable for debug logging.
**/
static const char * Nav_EdgeRejectReasonToString( const nav_edge_reject_reason_t reason ) {
	switch ( reason ) {
	case nav_edge_reject_reason_t::None: return "None";
	case nav_edge_reject_reason_t::TileRouteFilter: return "TileRouteFilter";
	case nav_edge_reject_reason_t::NoNode: return "NoNode";
	case nav_edge_reject_reason_t::DropCap: return "DropCap";
	case nav_edge_reject_reason_t::StepTest: return "StepTest";
	case nav_edge_reject_reason_t::ObstructionJump: return "ObstructionJump";
	case nav_edge_reject_reason_t::Occupancy: return "Occupancy";
	case nav_edge_reject_reason_t::EntersWater: return "EntersWater";
	case nav_edge_reject_reason_t::EntersLava: return "EntersLava";
	case nav_edge_reject_reason_t::EntersSlime: return "EntersSlime";
	case nav_edge_reject_reason_t::OptionalWalkOff: return "OptionalWalkOff";
	case nav_edge_reject_reason_t::ForbiddenWalkOff: return "ForbiddenWalkOff";
	case nav_edge_reject_reason_t::Other: return "Other";
	default: return "Unknown";
	}
}

/** 
*	@brief	Emit per-reason edge rejection counters for an A*	state.
*	@param	prefix	Diagnostic prefix/tag identifying the call site.
*	@param	state	A*	state containing reason counters.
**/
static void Nav_Debug_LogEdgeRejectReasonCounters( const char * prefix, const nav_a_star_state_t &state ) {
	/** 
	*	Iterate all known reject-reason slots so diagnostics remain complete
	*	even when only a subset of reasons is currently non-zero.
	**/
	for ( int32_t reasonIndex = ( int32_t )nav_edge_reject_reason_t::None;
		reasonIndex <= ( int32_t )nav_edge_reject_reason_t::Other;
		reasonIndex++ ) {
		const nav_edge_reject_reason_t reason = ( nav_edge_reject_reason_t )reasonIndex;
		gi.dprintf( "%s edge_reject_reason[%d]=%d (%s)\n",
			prefix ? prefix : "[NavPath][Diag]",
			reasonIndex,
			state.edge_reject_reason_counts[ reasonIndex ],
			Nav_EdgeRejectReasonToString( reason ) );
	}
}

/** 
*	@brief	Find the nearest Z-layer delta for the given node within its cell.
*	@param	mesh		Navigation mesh containing the node.
*	@param	node		Navigation node to inspect.
*	@param	out_delta	[out] Closest absolute Z delta to another layer in the same cell.
*	@param	out_layers	[out] Number of layers found in the cell (optional).
*	@return	True if a valid delta was found, false otherwise.
*	@note	This is a diagnostic helper used to report missing vertical connectivity.
**/
/** 
*	@brief    Find the nearest Z-layer delta for the given node within its cell.
*	@param    mesh        Navigation mesh containing the node.
*	@param    node        Navigation node to inspect.
*	@param    out_delta   [out] Closest absolute Z delta to another layer in the same cell.
*	@param    out_layers  [out] Number of layers found in the cell (optional).
*	@return   True if a valid delta was found, false otherwise.
*	@note     This is a diagnostic helper used to report missing vertical connectivity.
**/
static const bool Nav_Debug_FindNearestLayerDelta( const nav_mesh_t * mesh, const nav_node_ref_t &node, double * out_delta, int32_t * out_layers ) {
	/** 
	*	Sanity: require mesh and output pointer.
	**/
	if ( !mesh || !out_delta ) {
		return false;
	}

	/** 
	*	Validate tile/cell indices before inspecting layers.
	**/
	if ( node.key.tile_index < 0 || node.key.tile_index >= ( int32_t )mesh->world_tiles.size() ) {
		return false;
	}

	// Use safe tile cell accessor to avoid dereferencing a possibly-null
	// `tile.cells` pointer in sparse tiles.
	const nav_tile_t &tile = mesh->world_tiles[ node.key.tile_index ];
	auto cellsView = SVG_Nav_Tile_GetCells( mesh, const_cast< nav_tile_t* >( &tile ) );
	const nav_xy_cell_t * cellsPtr = cellsView.first;
	const int32_t cellsCount = cellsView.second;
	if ( !cellsPtr || node.key.cell_index < 0 || node.key.cell_index >= cellsCount ) {
		return false;
	}

	const nav_xy_cell_t &cell = cellsPtr[ node.key.cell_index ];
	if ( out_layers ) {
		*out_layers = cell.num_layers;
	}
	if ( cell.num_layers <= 1 || !cell.layers ) {
		return false;
	}

	/** 
	*	Scan all layers to find the closest Z delta.
	**/
	const double current_z = node.worldPosition.z;
	double best_delta = std::numeric_limits<double>::max();
	for ( int32_t li = 0; li < cell.num_layers; li++ ) {
		// Skip the current layer itself.
		if ( li == node.key.layer_index ) {
			continue;
		}
		const double layer_z = ( double )cell.layers[ li ].z_quantized* mesh->z_quant;
		const double dz = fabsf( ( float )( layer_z - current_z ) );
		if ( dz < best_delta ) {
			best_delta = dz;
		}
	}

	/** 
	*	Output the closest delta when a candidate was found.
	**/
	if ( best_delta == std::numeric_limits<double>::max() ) {
		return false;
	}
	*out_delta = best_delta;
	return true;
}

/** 
*	 @brief	Resolve the canonical tile, cell, and layer for a nav node reference.
*	 @param	mesh		Navigation mesh.
*	 @param	node		Node reference to inspect.
*	 @param	out_tile	[out] Resolved tile pointer.
*	 @param	out_cell	[out] Resolved cell pointer.
*	 @param	out_layer	[out] Resolved layer pointer.
*	 @return	True if the node maps to valid canonical nav storage.
**/
static const bool Nav_TryGetNodeLayerView( const nav_mesh_t * mesh, const nav_node_ref_t &node, const nav_tile_t * * out_tile, const nav_xy_cell_t * * out_cell, const nav_layer_t * * out_layer ) {
	/** 
	*	Sanity checks: require mesh and output storage.
	**/
	if ( !mesh || !out_tile || !out_cell || !out_layer ) {
		return false;
	}

	/** 
	*	Validate the canonical world-tile index before reading sparse storage.
	**/
	if ( node.key.tile_index < 0 || node.key.tile_index >= ( int32_t )mesh->world_tiles.size() ) {
		return false;
	}

	const nav_tile_t &tile = mesh->world_tiles[ node.key.tile_index ];
	auto cellsView = SVG_Nav_Tile_GetCells( mesh, &tile );
	const nav_xy_cell_t * cellsPtr = cellsView.first;
	const int32_t cellsCount = cellsView.second;
	if ( !cellsPtr || node.key.cell_index < 0 || node.key.cell_index >= cellsCount ) {
		return false;
	}

	const nav_xy_cell_t &cell = cellsPtr[ node.key.cell_index ];
	auto layersView = SVG_Nav_Cell_GetLayers( &cell );
	const nav_layer_t * layersPtr = layersView.first;
	const int32_t layerCount = layersView.second;
	if ( !layersPtr || node.key.layer_index < 0 || node.key.layer_index >= layerCount ) {
		return false;
	}

	/** 
	*	Return the resolved canonical views.
	**/
	*out_tile = &tile;
	*out_cell = &cell;
	*out_layer = &layersPtr[ node.key.layer_index ];
	return true;
}

/** 
*	 @brief	Convert a node reference into global cell-grid coordinates.
*	 @param	mesh		Navigation mesh.
*	 @param	node		Node reference to inspect.
*	 @param	out_cell_x	[out] Global cell X coordinate.
*	 @param	out_cell_y	[out] Global cell Y coordinate.
*	 @return	True if the node could be mapped to a valid grid cell.
**/
static const bool Nav_TryGetGlobalCellCoords( const nav_mesh_t * mesh, const nav_node_ref_t &node, int32_t * out_cell_x, int32_t * out_cell_y ) {
	/** 
	*	Sanity checks: require mesh and output storage.
	**/
	if ( !mesh || !out_cell_x || !out_cell_y ) {
		return false;
	}

	/** 
	*	Resolve the canonical tile first so we can derive local cell coordinates.
	**/
	if ( node.key.tile_index < 0 || node.key.tile_index >= ( int32_t )mesh->world_tiles.size() ) {
		return false;
	}

	const nav_tile_t &tile = mesh->world_tiles[ node.key.tile_index ];
	const int32_t local_cell_x = node.key.cell_index % mesh->tile_size;
	const int32_t local_cell_y = node.key.cell_index / mesh->tile_size;

	/** 
	*	Expand tile-local coordinates into world-grid coordinates.
	**/
	*out_cell_x = ( tile.tile_x* mesh->tile_size ) + local_cell_x;
	*out_cell_y = ( tile.tile_y* mesh->tile_size ) + local_cell_y;
	return true;
}

/** 
*	 @brief	Convert a global cell coordinate into a cell-center world position.
*	 @param	mesh		Navigation mesh.
*	 @param	cell_x		Global cell X coordinate.
*	 @param	cell_y		Global cell Y coordinate.
*	 @param	z		Desired world-space Z for the query point.
*	 @return	World-space cell center carrying the provided Z height.
**/
static inline Vector3 Nav_GlobalCellCenterToWorld( const nav_mesh_t * mesh, const int32_t cell_x, const int32_t cell_y, const double z ) {
	return Vector3{
		( float )( ( ( double )cell_x + 0.5 )* mesh->cell_size_xy ),
		( float )( ( ( double )cell_y + 0.5 )* mesh->cell_size_xy ),
		( float )z
	};
}

/** 
*	 @brief	Convert a quantized layer clearance value into world units.
*	 @param	mesh		Navigation mesh.
*	 @param	layer		Layer holding the quantized clearance.
*	 @return	World-space clearance above this layer's walkable floor sample.
**/
static inline double Nav_GetLayerClearanceWorld( const nav_mesh_t * mesh, const nav_layer_t &layer ) {
	return mesh ? ( ( double )layer.clearance* mesh->z_quant ) : 0.0;
}

/** 
*	 @brief	Compute floor-division for signed global cell coordinates.
*	 @param	value	Signed dividend.
*	 @param	divisor	Positive divisor.
*	 @return	Mathematical floor of `value / divisor`.
**/
static inline int32_t Nav_Traversal_FloorDiv( const int32_t value, const int32_t divisor ) {
	/** 
	*	Sanity checks: require a positive divisor.
	**/
	if ( divisor <= 0 ) {
		return 0;
	}

	/** 
	*	Use integer truncation for non-negative inputs.
	**/
	if ( value >= 0 ) {
		return value / divisor;
	}

	/** 
	*	Negative inputs need explicit floor semantics.
	**/
	return -( ( -value + divisor - 1 ) / divisor );
}

/** 
*	 @brief	Compute a positive modulo for signed global cell coordinates.
*	 @param	value	Signed input value.
*	 @param	modulus	Positive modulus.
*	 @return	Value wrapped into `[0, modulus)`.
**/
static inline int32_t Nav_Traversal_PosMod( const int32_t value, const int32_t modulus ) {
	/** 
	*	Sanity checks: require a positive modulus.
	**/
	if ( modulus <= 0 ) {
		return 0;
	}

	/** 
	*	Normalize the remainder into the tile-local range.
	**/
	const int32_t remainder = value % modulus;
	return ( remainder < 0 ) ? ( remainder + modulus ) : remainder;
}

/** 
*	 @brief	Resolve a canonical intermediate segment node directly from global cell coordinates.
*	 @param	mesh		Navigation mesh.
*	 @param	current_node	Current canonical segment start node.
*	 @param	target_cell_x	Global target cell X coordinate.
*	 @param	target_cell_y	Global target cell Y coordinate.
*	 @param	desired_z	Desired world-space Z for layer selection.
*	 @param	out_node	[out] Resolved canonical target node.
*	 @return	True if a walkable layer in the target cell could be resolved.
*	 @note	This keeps segmented XY bridge validation on exact walkable tile/cell storage instead of
*	 			falling back to a world-position lookup that can miss sparse but valid intermediate cells.
**/
static const bool Nav_TryResolveIntermediateSegmentNodeExact( const nav_mesh_t * mesh, const nav_node_ref_t &current_node,
	const int32_t target_cell_x, const int32_t target_cell_y, const double desired_z, nav_node_ref_t * out_node ) {
	/** 
	*	Sanity checks: require mesh storage, output storage, and a valid current tile reference.
	**/
	if ( !mesh || !out_node ) {
		return false;
	}
	if ( current_node.key.tile_index < 0 || current_node.key.tile_index >= ( int32_t )mesh->world_tiles.size() ) {
		return false;
	}

	/** 
	*	Map the requested global cell back into canonical tile-local addressing.
	**/
	const int32_t target_tile_x = Nav_Traversal_FloorDiv( target_cell_x, mesh->tile_size );
	const int32_t target_tile_y = Nav_Traversal_FloorDiv( target_cell_y, mesh->tile_size );
	const int32_t target_local_x = Nav_Traversal_PosMod( target_cell_x, mesh->tile_size );
	const int32_t target_local_y = Nav_Traversal_PosMod( target_cell_y, mesh->tile_size );
	const int32_t target_cell_index = ( target_local_y* mesh->tile_size ) + target_local_x;

	/** 
	*	Resolve the canonical tile and reject missing sparse cells before reading layer storage.
	**/
	const nav_world_tile_key_t target_tile_key = { .tile_x = target_tile_x, .tile_y = target_tile_y };
	auto tile_it = mesh->world_tile_id_of.find( target_tile_key );
	if ( tile_it == mesh->world_tile_id_of.end() ) {
		return false;
	}

	const int32_t target_tile_index = tile_it->second;
	if ( target_tile_index < 0 || target_tile_index >= ( int32_t )mesh->world_tiles.size() ) {
		return false;
	}

	const nav_tile_t * target_tile = &mesh->world_tiles[ target_tile_index ];
	if ( !target_tile || !target_tile->presence_bits ) {
		return false;
	}

	// Reject sparse cells that are not marked present in the tile bitset.
	const int32_t word_index = target_cell_index >> 5;
	const int32_t bit_index = target_cell_index & 31;
	if ( ( target_tile->presence_bits[ word_index ] & ( 1u << bit_index ) ) == 0 ) {
		return false;
	}

	/** 
	*	Resolve the target cell storage and select the best walkable layer for the requested step height.
	**/
	auto cellsView = SVG_Nav_Tile_GetCells( mesh, target_tile );
	const nav_xy_cell_t * cellsPtr = cellsView.first;
	const int32_t cellsCount = cellsView.second;
	if ( !cellsPtr || target_cell_index < 0 || target_cell_index >= cellsCount ) {
		return false;
	}

	const nav_xy_cell_t * target_cell = &cellsPtr[ target_cell_index ];
	if ( !target_cell || target_cell->num_layers <= 0 || !target_cell->layers ) {
		return false;
	}

	int32_t target_layer_index = -1;
	if ( !Nav_SelectLayerIndex( mesh, target_cell, desired_z, &target_layer_index ) ) {
		return false;
	}

	/** 
	*	Populate the resolved canonical node using the selected walkable layer.
	**/
	const nav_layer_t * target_layer = &target_cell->layers[ target_layer_index ];
	out_node->key.leaf_index = current_node.key.leaf_index;
	out_node->key.tile_index = target_tile_index;
	out_node->key.cell_index = target_cell_index;
	out_node->key.layer_index = target_layer_index;
	out_node->worldPosition = Nav_NodeWorldPosition( mesh, target_tile, target_cell_index, target_layer );
	return true;
}

/** 
*	 @brief	Forward declaration for the single-step traversal helper used by segmented ramps.
*	 @param	mesh		Navigation mesh.
*	 @param	start_node	Resolved start node.
*	 @param	end_node	Resolved end node.
*	 @param	mins		Agent bounding box minimums.
*	 @param	maxs		Agent bounding box maximums.
*	 @param	clip_entity	Entity to use for clipping (nullptr for world).
*	 @param	policy		Optional path policy for tuning step behavior.
*	 @return	True if the traversal is possible, false otherwise.
**/
static const bool Nav_CanTraverseStep_ExplicitBBox_Single( const nav_mesh_t * mesh, const nav_node_ref_t &start_node, const nav_node_ref_t &end_node, const Vector3 &mins, const Vector3 &maxs, const edict_ptr_t * clip_entity, const svg_nav_path_policy_t * policy, nav_edge_reject_reason_t * out_reason = nullptr, const cm_contents_t stepTraceMask = CM_CONTENTMASK_MONSTERSOLID );

/** 
*	 @brief	Performs a simple PMove-like step traversal test (3-trace) with explicit agent bbox.
*				This is intentionally conservative and is used only to validate edges in A* :
*				1) Try direct horizontal move.
*				2) If blocked, try stepping up <= max step, then horizontal move.
*				3) Trace down to find ground.
*	 @param	mesh			Navigation mesh.
*	 @param	startPos		Start world position.
*	 @param	endPos			End world position.
*	 @param	mins			Agent bounding box minimums.
*	 @param	maxs			Agent bounding box maximums.
*	 @param	clip_entity		Entity to use for clipping (nullptr for world).
*	 @param	policy			Optional path policy for tuning step behavior.
*	 @return	True if the traversal is possible, false otherwise.
**/
const bool Nav_CanTraverseStep_ExplicitBBox( const nav_mesh_t * mesh, const Vector3 &startPos, const Vector3 &endPos, const Vector3 &mins, const Vector3 &maxs, const edict_ptr_t * clip_entity, const svg_nav_path_policy_t * policy, nav_edge_reject_reason_t * out_reason, const cm_contents_t stepTraceMask ) {
	/** 
	*	Sanity checks: require a valid mesh.
	**/
	if ( !mesh ) {
		return false;
	}

	/** 
	*	Resolve canonical start/end nodes so edge validation can work from tile/cell/layer
	*	facts rather than generic world tracing.
	**/
	nav_node_ref_t start_node = {};
	nav_node_ref_t end_node = {};
	if ( !Nav_FindNodeForPosition( mesh, startPos, startPos[ 2 ], &start_node, true ) ||
		!Nav_FindNodeForPosition( mesh, endPos, endPos[ 2 ], &end_node, true ) ) {
		if ( out_reason ) {
			*out_reason = nav_edge_reject_reason_t::NoNode;
		}
		return false;
	}

	/** 
	*	Early out when both points already resolve to the same canonical node.
	**/
	if ( start_node.key == end_node.key ) {
		return true;
	}

	/** 
	*	Canonical-edge validation:
	*	    Reuse the already resolved node references so the validator does not
	*	    re-query the same walkable cell centers through `Nav_FindNodeForPosition()`.
	**/
	return Nav_CanTraverseStep_ExplicitBBox_NodeRefs( mesh, start_node, end_node, mins, maxs, clip_entity, policy, out_reason, stepTraceMask );
}

/** 
*	 @brief	Validate a traversal edge from canonical node references.
*	 @param	mesh		Navigation mesh.
*	 @param	start_node	Resolved canonical start node.
*	 @param	end_node	Resolved canonical end node.
*	 @param	mins		Agent bounding box minimums.
*	 @param	maxs		Agent bounding box maximums.
*	 @param	clip_entity	Entity to use for clipping (nullptr for world).
*	 @param	policy		Optional path policy for tuning step behavior.
*	 @param	out_reason	[out] Optional reject reason.
*	 @param	stepTraceMask	Reserved mask parameter kept aligned with the position overload.
*	 @return	True if the traversal is possible, false otherwise.
*	 @note		This keeps XY validation on the walkable canonical cells already selected by the caller and
*	 			limits additional Z probing to the segmented step path only when stepping requires it.
**/
const bool Nav_CanTraverseStep_ExplicitBBox_NodeRefs( const nav_mesh_t * mesh, const nav_node_ref_t &start_node, const nav_node_ref_t &end_node,
	const Vector3 &mins, const Vector3 &maxs, const edict_ptr_t * clip_entity, const svg_nav_path_policy_t * policy,
	nav_edge_reject_reason_t * out_reason, const cm_contents_t stepTraceMask ) {
	/** 
	*	Sanity checks: require a valid mesh.
	**/
	if ( !mesh ) {
		return false;
	}

	/** 
	*	Early out when both endpoints already reference the same canonical node.
	**/
	if ( start_node.key == end_node.key ) {
		return true;
	}

	/** 
	*	Compute the required climb to determine if segmentation is needed.
	**/
	const Vector3 &startPos = start_node.worldPosition;
	const Vector3 &endPos = end_node.worldPosition;
	const double desiredDz = ( double )endPos[ 2 ] - ( double )startPos[ 2 ];
	const double requiredUp = std::max( 0.0, desiredDz );
	const double stepSize = ( policy && policy->max_step_height > 0.0 ) ? ( double )policy->max_step_height : ( double )mesh->max_step;

	/** 
	*	Drop cap enforcement (total edge):
	*	    Ensure long downward transitions do not bypass the cap when we segment
	*	    by horizontal distance for multi-cell ramps.
	**/
	const double dropCap = ( policy ? ( double )policy->max_drop_height_cap : ( nav_max_drop_height_cap ? nav_max_drop_height_cap->value : 128.0f ) );
	const double requiredDown = std::max( 0.0, ( double )startPos[ 2 ] - ( double )endPos[ 2 ] );
	// Reject edges that drop farther than the configured cap.
	if ( requiredDown > 0.0 && dropCap >= 0.0 && requiredDown > dropCap ) {
		if ( out_reason ) * out_reason = nav_edge_reject_reason_t::DropCap;
		return false;
	}

	/** 
	*	Compute horizontal distance to decide if we should segment long, shallow ramps.
	*	    Multi-cell ramps can fail a single step-test even with a small Z delta, so
	*	    we segment by XY distance to validate each cell-scale portion.
	**/
	const Vector3 fullDelta = QM_Vector3Subtract( endPos, startPos );
	int32_t start_cell_x = 0;
	int32_t start_cell_y = 0;
	int32_t end_cell_x = 0;
	int32_t end_cell_y = 0;
	if ( !Nav_TryGetGlobalCellCoords( mesh, start_node, &start_cell_x, &start_cell_y ) ||
		!Nav_TryGetGlobalCellCoords( mesh, end_node, &end_cell_x, &end_cell_y ) ) {
		if ( out_reason ) {
			*out_reason = nav_edge_reject_reason_t::NoNode;
		}
		return false;
	}

	const int32_t delta_cell_x = end_cell_x - start_cell_x;
	const int32_t delta_cell_y = end_cell_y - start_cell_y;
	const int32_t edge_step_dx = ( delta_cell_x > 0 ) ? 1 : ( ( delta_cell_x < 0 ) ? -1 : 0 );
	const int32_t edge_step_dy = ( delta_cell_y > 0 ) ? 1 : ( ( delta_cell_y < 0 ) ? -1 : 0 );
	const uint32_t source_edge_bits = SVG_Nav_GetEdgeFeatureBitsForOffset( mesh, start_node, edge_step_dx, edge_step_dy );

	/** 
	*	Apply persisted edge semantics before the more expensive segmented validator runs.
	**/
	if ( source_edge_bits != NAV_EDGE_FEATURE_NONE ) {
		if ( policy && policy->forbid_water && ( source_edge_bits & NAV_EDGE_FEATURE_ENTERS_WATER ) != 0 ) {
			if ( out_reason ) {
				*out_reason = nav_edge_reject_reason_t::Other;
			}
			return false;
		}
		if ( policy && policy->forbid_lava && ( source_edge_bits & NAV_EDGE_FEATURE_ENTERS_LAVA ) != 0 ) {
			if ( out_reason ) {
				*out_reason = nav_edge_reject_reason_t::Other;
			}
			return false;
		}
		if ( policy && policy->forbid_slime && ( source_edge_bits & NAV_EDGE_FEATURE_ENTERS_SLIME ) != 0 ) {
			if ( out_reason ) {
				*out_reason = nav_edge_reject_reason_t::Other;
			}
			return false;
		}
		if ( policy && !policy->allow_optional_walk_off && ( source_edge_bits & NAV_EDGE_FEATURE_OPTIONAL_WALK_OFF ) != 0 ) {
			if ( out_reason ) {
				*out_reason = nav_edge_reject_reason_t::Other;
			}
			return false;
		}
		if ( policy && !policy->allow_forbidden_walk_off && ( source_edge_bits & NAV_EDGE_FEATURE_FORBIDDEN_WALK_OFF ) != 0 ) {
			if ( out_reason ) {
				*out_reason = nav_edge_reject_reason_t::Other;
			}
			return false;
		}
		if ( ( source_edge_bits & NAV_EDGE_FEATURE_HARD_WALL_BLOCKED ) != 0 && ( source_edge_bits & NAV_EDGE_FEATURE_PASSABLE ) == 0 ) {
			if ( out_reason ) {
				*out_reason = nav_edge_reject_reason_t::Other;
			}
			return false;
		}
	}
	// Determine how many segments we need for vertical climb and for multi-cell ramps.
	const int32_t stepCountVertical = ( requiredUp > stepSize && stepSize > 0.0 )
		? ( int32_t )std::ceil( requiredUp / stepSize )
		: 1;
	const int32_t stepCountHorizontal = std::max( std::abs( delta_cell_x ), std::abs( delta_cell_y ) );
	   // Use the most conservative segmentation to satisfy both climb and ramp constraints.
	const int32_t stepCount = std::max( 1, std::max( stepCountVertical, stepCountHorizontal ) );

	 /** 
	*Segmented ramp/stair handling:
	*	   Split long climbs into <= max_step_height sub-steps so
	*	   step validation mirrors PMove-style step constraints.
	 **/
	if ( stepCount > 1 ) {
		  /** 
		*	Segment by canonical cell hops instead of interpolated trace points.
		 * This keeps each sub-step aligned with actual nav cells/layers.
		  **/
		nav_node_ref_t segmentStartNode = start_node;
		   // Validate each sub-step in sequence.
		for ( int32_t stepIndex = 1; stepIndex <= stepCount; stepIndex++ ) {
			const double t = ( double )stepIndex / ( double )stepCount;
			const int32_t segment_cell_x = start_cell_x + ( int32_t )std::lround( ( double )delta_cell_x* t );
			const int32_t segment_cell_y = start_cell_y + ( int32_t )std::lround( ( double )delta_cell_y* t );

			nav_node_ref_t segmentEndNode = {};
			if ( stepIndex == stepCount ) {
				segmentEndNode = end_node;
			} else {
			 /** 
				*	Resolve the next segment from the current segment surface instead of the full-edge
				*	interpolated Z. Intermediate cells on stairs/ramps often do not contain a node at the
				*	synthetic interpolated height even though a valid step-sized continuation exists.
				**/
				// Compute the remaining vertical delta from this segment start toward the final end node.
				const double remaining_segment_dz = ( double )end_node.worldPosition[ 2 ] - ( double )segmentStartNode.worldPosition[ 2 ];
				// Clamp the desired rise/drop for this sub-step to one PMove-sized step.
				const double segment_step_dz = QM_Clamp( remaining_segment_dz, -stepSize, stepSize );
				// Use the clamped per-step surface target as the primary lookup height for this segment cell.
				const double segment_desired_z = ( double )segmentStartNode.worldPosition[ 2 ] + segment_step_dz;
				/** 
				*Resolve the intermediate segment from exact tile/cell storage so short XY bridge probes stay
					*	aligned with walkable surface cells instead of relying on a world-position lookup.
				**/
				if ( !Nav_TryResolveIntermediateSegmentNodeExact( mesh, segmentStartNode, segment_cell_x, segment_cell_y, segment_desired_z, &segmentEndNode ) ) {
				/** 
				*Conservative fallback: if the stepped desired Z does not map to a walkable layer in this
					*	exact cell, retry against the current surface height before rejecting the segment.
				**/
					if ( !Nav_TryResolveIntermediateSegmentNodeExact( mesh, segmentStartNode, segment_cell_x, segment_cell_y, segmentStartNode.worldPosition[ 2 ], &segmentEndNode ) ) {
						if ( out_reason ) {
							*out_reason = nav_edge_reject_reason_t::NoNode;
						}
						return false;
					}
				}
			}

			/** 
			*	Skip duplicate canonical nodes produced by rounding so segmentation stays monotonic.
			**/
			if ( segmentStartNode.key == segmentEndNode.key ) {
				continue;
			}

			/** 
			*	Validate the current sub-step using the single-step routine.
			*	Abort immediately if any sub-step fails.
			**/
			if ( !Nav_CanTraverseStep_ExplicitBBox_Single( mesh, segmentStartNode, segmentEndNode, mins, maxs, clip_entity, policy, out_reason, stepTraceMask ) ) {
				return false;
			}

			// Advance to the next segment.
			segmentStartNode = segmentEndNode;
		}

		// All sub-steps succeeded.
		return true;
	}

	/** 
	*	Fallback: use the single-step validation when no segmentation is needed.
	**/
	return Nav_CanTraverseStep_ExplicitBBox_Single( mesh, start_node, end_node, mins, maxs, clip_entity, policy, out_reason, stepTraceMask );
}

/** 
*	  @brief	Probe a small XY neighborhood to rescue sync endpoint node resolution on boundary origins.
*	  @param	mesh		Navigation mesh.
*	  @param	query_center	Endpoint center position that failed direct lookup.
*	  @param	start_center	Original request start center used by blend lookup.
*	  @param	goal_center	Original request goal center used by blend lookup.
*	  @param	query_is_goal	True when rescuing the goal endpoint, false for the start endpoint.
*	  @param	policy		Resolved path policy controlling blend behavior.
*	  @param	out_node	[out] Best rescued canonical node.
*	  @return	True when a nearby endpoint node was recovered.
*	  @note	This mirrors the async worker's boundary-origin hardening so sync path generation can recover
*	  		from sound or interaction points that land directly on tile or cell boundaries.
**/
static const bool Nav_Traversal_TryResolveBoundaryOriginNode( const nav_mesh_t * mesh, const Vector3 &query_center,
	const Vector3 &start_center, const Vector3 &goal_center, const bool query_is_goal,
	const svg_nav_path_policy_t &policy, nav_node_ref_t * out_node ) {
	/** 
	*	  	Sanity checks: require mesh storage and an output node reference.
	**/
	if ( !mesh || !out_node ) {
		return false;
	}

	/** 
	*	  	Build a compact probe ring around the failed endpoint.
	*	  		Use half-cell and one-cell offsets so boundary-origin path requests can snap onto a nearby
	*	  		valid walk surface without skipping across large gaps.
	**/
	const float mesh_cell_size = ( float )mesh->cell_size_xy;
	const float half_cell_raw = mesh_cell_size* 0.5f;
	const float half_cell = ( half_cell_raw > 1.0f ) ? half_cell_raw : 1.0f;
	const float full_cell = ( half_cell > mesh_cell_size ) ? half_cell : mesh_cell_size;
	const Vector3 probe_offsets[ ] = {
		{ half_cell, 0.0f, 0.0f },
		{ -half_cell, 0.0f, 0.0f },
		{ 0.0f, half_cell, 0.0f },
		{ 0.0f, -half_cell, 0.0f },
		{ half_cell, half_cell, 0.0f },
		{ half_cell, -half_cell, 0.0f },
		{ -half_cell, half_cell, 0.0f },
		{ -half_cell, -half_cell, 0.0f },
		{ full_cell, 0.0f, 0.0f },
		{ -full_cell, 0.0f, 0.0f },
		{ 0.0f, full_cell, 0.0f },
		{ 0.0f, -full_cell, 0.0f }
	};

	/** 
	*	  	Keep the closest rescued node to the original failed endpoint.
	**/
	bool found_node = false;
	double best_distance_sqr = std::numeric_limits<double>::infinity();
	nav_node_ref_t best_node = {};

	// Probe each nearby XY offset and keep the closest successfully resolved canonical node.
	for ( const Vector3 &probe_offset : probe_offsets ) {
		// Shift the failed endpoint by the current local probe offset.
		const Vector3 probe_center = QM_Vector3Add( query_center, probe_offset );
		nav_node_ref_t candidate_node = {};
		bool resolved = false;

		/** 
		*	  	Reuse the caller's configured lookup policy so boundary rescue stays aligned with the
		*	  	same blend and fallback behavior as the primary endpoint resolution path.
		**/
		if ( policy.enable_goal_z_layer_blend ) {
			resolved = Nav_FindNodeForPosition_BlendZ( mesh, probe_center, start_center.z, goal_center.z,
				start_center, goal_center, policy.blend_start_dist, policy.blend_full_dist, &candidate_node, true );
		} else {
			const double desired_z = query_is_goal ? goal_center.z : start_center.z;
			resolved = Nav_FindNodeForPosition( mesh, probe_center, desired_z, &candidate_node, true );
		}

		// Continue probing until we find at least one nearby canonical node.
		if ( !resolved ) {
			continue;
		}

		// Score the candidate by how closely its recovered world position matches the original endpoint.
		const double candidate_distance_sqr = QM_Vector3DistanceSqr( candidate_node.worldPosition, query_center );
		if ( !found_node || candidate_distance_sqr < best_distance_sqr ) {
			found_node = true;
			best_distance_sqr = candidate_distance_sqr;
			best_node = candidate_node;
		}
	}

	/** 
	*	  	Commit the closest rescued endpoint node when probing succeeded.
	**/
	if ( !found_node ) {
		return false;
	}

	*out_node = best_node;
	return true;
}

/** 
*	  @brief	Resolve one traversal endpoint using direct lookup, boundary rescue, and same-cell rescue.
*	  @param	mesh		Navigation mesh.
*	  @param	query_center	Endpoint center position to resolve.
*	  @param	start_center	Full request start center used by blended lookup.
*	  @param	goal_center	Full request goal center used by blended lookup.
*	  @param	query_is_goal	True when resolving the goal endpoint, false for the start endpoint.
*	  @param	policy		Resolved path policy used for endpoint lookup.
*	  @param	out_node	[out] Resolved canonical endpoint node.
*	  @return	True when endpoint resolution succeeded.
*	  @note	This keeps the sync traversal APIs aligned with the async worker's endpoint hardening.
**/
static const bool Nav_Traversal_TryResolveEndpointNode( const nav_mesh_t * mesh, const Vector3 &query_center,
	const Vector3 &start_center, const Vector3 &goal_center, const bool query_is_goal,
	const svg_nav_path_policy_t &policy, nav_node_ref_t * out_node ) {
	/** 
	*	  	Sanity checks: require mesh storage and an output node reference.
	**/
	if ( !mesh || !out_node ) {
		return false;
	}

	/** 
	*	  	Attempt direct endpoint lookup first using the requested blend policy.
	**/
	nav_node_ref_t resolved_node = {};
	bool resolved = false;
	if ( policy.enable_goal_z_layer_blend ) {
		resolved = Nav_FindNodeForPosition_BlendZ( mesh, query_center, start_center.z, goal_center.z,
			start_center, goal_center, policy.blend_start_dist, policy.blend_full_dist, &resolved_node, true );
	} else {
		const double desired_z = query_is_goal ? goal_center.z : start_center.z;
		resolved = Nav_FindNodeForPosition( mesh, query_center, desired_z, &resolved_node, true );
	}

	/** 
	*	  	Fallback to boundary-origin rescue when the raw endpoint center misses a walkable sample.
	**/
	if ( !resolved ) {
		resolved = Nav_Traversal_TryResolveBoundaryOriginNode( mesh, query_center, start_center, goal_center,
			query_is_goal, policy, &resolved_node );
	}

	/** 
	*	  	Early out when both direct lookup and boundary rescue failed.
	**/
	if ( !resolved ) {
		return false;
	}

	/** 
	*	  	Rescue isolated same-cell layers by preferring the best-connected layer variant in that cell.
	**/
	Nav_AStar_TrySelectConnectedSameCellLayer( mesh, resolved_node, &policy, &resolved_node );
	*out_node = resolved_node;
	return true;
}

/** 
*	  @brief	Resolve both traversal endpoints under one blend strategy.
*	  @param	mesh		Navigation mesh.
*	  @param	start_center	Request start in nav-center space.
*	  @param	goal_center	Request goal in nav-center space.
*	  @param	agent_mins	Agent bounding box minimums.
*	  @param	agent_maxs	Agent bounding box maximums.
*	  @param	policy		Optional path policy to copy and refine.
*	  @param	enable_goal_z_layer_blend	Whether this strategy should use blended endpoint lookup.
*	  @param	blend_start_dist	Distance at which goal-Z blending begins.
*	  @param	blend_full_dist	Distance at which goal-Z blending fully favors the goal layer.
*	  @param	out_start_node	[out] Resolved canonical start node.
*	  @param	out_goal_node	[out] Resolved canonical goal node.
*	  @return	True when both endpoints were resolved successfully.
**/
static const bool Nav_Traversal_TryResolveEndpointsForStrategy( const nav_mesh_t * mesh, const Vector3 &start_center,
	const Vector3 &goal_center, const Vector3 &agent_mins, const Vector3 &agent_maxs, const svg_nav_path_policy_t * policy,
	const bool enable_goal_z_layer_blend, const double blend_start_dist, const double blend_full_dist,
	nav_node_ref_t * out_start_node, nav_node_ref_t * out_goal_node ) {
	/** 
	*	  	Sanity checks: require mesh storage and output nodes.
	**/
	if ( !mesh || !out_start_node || !out_goal_node ) {
		return false;
	}

	/** 
	*	  	Build the resolved policy snapshot for this specific endpoint strategy.
	*	  		This keeps sync endpoint lookup aligned with the agent hull and caller policy.
	**/
	svg_nav_path_policy_t resolvedPolicy = policy ? * policy : svg_nav_path_policy_t{};
	resolvedPolicy.agent_mins = agent_mins;
	resolvedPolicy.agent_maxs = agent_maxs;
	resolvedPolicy.enable_goal_z_layer_blend = enable_goal_z_layer_blend;
	if ( enable_goal_z_layer_blend ) {
		resolvedPolicy.blend_start_dist = ( blend_start_dist > 0.0 ) ? blend_start_dist : resolvedPolicy.blend_start_dist;
		resolvedPolicy.blend_full_dist = ( blend_full_dist > resolvedPolicy.blend_start_dist )
			? blend_full_dist
			: std::max( resolvedPolicy.blend_start_dist + mesh->z_quant, resolvedPolicy.blend_full_dist );
	}

	/** 
	*	  	Resolve the start and goal endpoints under the same policy snapshot.
	**/
	if ( !Nav_Traversal_TryResolveEndpointNode( mesh, start_center, start_center, goal_center, false, resolvedPolicy, out_start_node ) ) {
		return false;
	}
	if ( !Nav_Traversal_TryResolveEndpointNode( mesh, goal_center, start_center, goal_center, true, resolvedPolicy, out_goal_node ) ) {
		return false;
	}

	return true;
}

/** 
*	  @brief	Forward declaration for the shared sync A*	search helper.
*	  @param	mesh		Navigation mesh containing the voxel tiles/cells.
*	  @param	start_node	Resolved canonical start node.
*	  @param	goal_node	Resolved canonical goal node.
*	  @param	agent_mins	Agent bounding box minimums in nav-center space.
*	  @param	agent_maxs	Agent bounding box maximums in nav-center space.
*	  @param	out_points	[out] Waypoints produced by the search.
*	  @param	policy		Optional traversal policy.
*	  @param	pathProcess	Optional per-entity path-process state.
*	  @param	tileRouteFilter	Optional coarse tile-route restriction.
*	  @return	True when a valid path was produced.
*	  @note	`Nav_Traversal_TryGeneratePointsWithEndpointFallbacks()` calls this helper before its full
*	  		definition appears later in the translation unit, so we declare it here explicitly.
**/
static bool Nav_AStarSearch( const nav_mesh_t * mesh, const nav_node_ref_t &start_node, const nav_node_ref_t &goal_node,
	const Vector3 &agent_mins, const Vector3 &agent_maxs, std::vector<Vector3> &out_points, const svg_nav_path_policy_t * policy = nullptr,
	const struct svg_nav_path_process_t * pathProcess = nullptr, const nav_refine_corridor_t * refineCorridor = nullptr );

/** 
*	  @brief	Generate traversal waypoints by trying primary and fallback endpoint-resolution strategies.
*	  @param	mesh		Navigation mesh.
*	  @param	start_center	Request start in nav-center space.
*	  @param	goal_center	Request goal in nav-center space.
*	  @param	agent_mins	Agent bounding box minimums.
*	  @param	agent_maxs	Agent bounding box maximums.
*	  @param	policy		Optional path policy for traversal and endpoint tuning.
*	  @param	pathProcess	Optional path-process state for failure penalties.
*	  @param	prefer_blended_lookup	True to try blended endpoint lookup first, false to try plain lookup first.
*	  @param	blend_start_dist	Distance at which goal-Z blending begins.
*	  @param	blend_full_dist	Distance at which goal-Z blending fully favors the goal layer.
*	  @param	out_points	[out] Final traversal waypoints.
*	  @return	True when any endpoint strategy produced a valid traversal path.
*	  @note	This helper stops relying on a single layer-selection guess by trying both blended and unblended
*	  		endpoint strategies before giving up.
**/
static const bool Nav_Traversal_TryGeneratePointsWithEndpointFallbacks( const nav_mesh_t * mesh, const Vector3 &start_center,
	const Vector3 &goal_center, const Vector3 &agent_mins, const Vector3 &agent_maxs, const svg_nav_path_policy_t * policy,
	const svg_nav_path_process_t * pathProcess, const bool prefer_blended_lookup, const double blend_start_dist,
	const double blend_full_dist, std::vector<Vector3> &out_points ) {
	nav_query_debug_stats_t * const queryStats = s_nav_active_query_stats;

	   /** 
	  *	 	Sanity checks: require mesh storage.
	   **/
	if ( !mesh ) {
		return false;
	}

	/** 
	*	  	Prepare the primary and secondary endpoint strategies.
	*	  		The secondary strategy flips blend usage so sync traversal is not trapped by one layer guess.
	**/
	const bool strategyBlendModes[ ] = { prefer_blended_lookup, !prefer_blended_lookup };
	const int32_t strategyCount = ( strategyBlendModes[ 0 ] == strategyBlendModes[ 1 ] ) ? 1 : 2;

	/** 
	*	  	Evaluate each endpoint strategy until one produces a valid path.
	**/
	for ( int32_t strategyIndex = 0; strategyIndex < strategyCount; strategyIndex++ ) {
		const bool strategyUsesBlend = strategyBlendModes[ strategyIndex ];

		/** 
		*	Record when the sync query falls back to a secondary endpoint strategy.
		**/
		if ( strategyIndex > 0 ) {
			Nav_QueryStats_RecordFallbackUsage( queryStats );
		}

		nav_node_ref_t start_node = {};
		nav_node_ref_t goal_node = {};
		if ( !Nav_Traversal_TryResolveEndpointsForStrategy( mesh, start_center, goal_center, agent_mins, agent_maxs,
			policy, strategyUsesBlend, blend_start_dist, blend_full_dist, &start_node, &goal_node ) ) {
			continue;
		}

     /** 
		*	  	Try the Phase 7 direct goal shortcut before any coarse routing or fine A*	work.
		**/
      if ( Nav_ShouldAttemptDirectLosShortcut( mesh, start_node, goal_node )
			&& Nav_Traversal_TryBuildDirectShortcutPoints( mesh, start_node, goal_node, agent_mins, agent_maxs, policy, out_points ) ) {
			return true;
		}

		/** 
	*	 	Compute the coarse route from the resolved canonical endpoints so A*	stays aligned
		*	  	with the exact surfaces that endpoint resolution recovered while preserving cluster fallback.
		**/
      const bool hierarchyPreferred = Nav_HierarchyRoutingPreferredForQuery( mesh, start_node.worldPosition, goal_node.worldPosition );
		nav_refine_corridor_t refineCorridor = {};
		bool usedHierarchyRoute = false;
		bool usedClusterRoute = false;
		int32_t coarseExpansions = 0;
		const uint64_t coarseStartMs = queryStats ? gi.GetRealSystemTime() : 0;
		const bool hasRefineCorridor = SVG_Nav_BuildRefineCorridor( mesh, start_node, goal_node, policy, &refineCorridor,
			&usedHierarchyRoute, &usedClusterRoute, &coarseExpansions );
		if ( queryStats ) {
			Nav_QueryStats_RecordCoarseStage( queryStats, usedHierarchyRoute, usedClusterRoute, coarseExpansions, gi.GetRealSystemTime() - coarseStartMs );
			Nav_QueryStats_RecordRefineCorridor( queryStats, hasRefineCorridor ? &refineCorridor : nullptr );
			Nav_QueryStats_RecordHierarchyRoutingChoice( queryStats, hierarchyPreferred, usedHierarchyRoute, hasRefineCorridor );
		}

		/** 
		*	  	Attempt A*	using the currently resolved endpoint pair.
		*	  		If this fails, fall through and let the alternate endpoint strategy try again.
		**/
       if ( Nav_AStarSearch( mesh, start_node, goal_node, agent_mins, agent_maxs, out_points, policy, pathProcess,
			hasRefineCorridor ? &refineCorridor : nullptr ) ) {
			return true;
		}
	}

	return false;
}

/** 
* 
* 
* 
*	  Navigation Pathfinding:
* 
* 
* 
**/
/** 
* 	@brief	Perform A*	search between two navigation nodes.
* 	@param	mesh			Navigation mesh containing the voxel tiles/cells.
* 	@param	start_node	Start navigation node reference.
* 	@param	goal_node	Goal navigation node reference.
* 	@param	agent_mins	Agent bounding box mins (in nav-center space).
* 	@param	agent_maxs	Agent bounding box maxs (in nav-center space).
* 	@param	out_points	[out] Waypoints (node positions) from start to goal.
* 	@param	policy		Optional per-agent traversal tuning (step/drop/jump constraints).
* 	@param	pathProcess	Optional per-entity path process used for failure backoff penalties.
* 	@param	tileRouteFilter	Optional hierarchical filter restricting expansion to a coarse tile route.
* 	@return	True if a path was found, false otherwise.
* 	@note	This search validates each candidate edge using `Nav_CanTraverseStep_ExplicitBBox`.
* 			It is therefore safe to consider additional neighbor offsets (e.g., 2-cell hops)
* 			because collision/step feasibility is still enforced.
**/
/** 
* 	@brief	Perform a traversal search while reusing the async helper state machine.
* 	@note	The shared helper documents neighbor offsets, drop limits, and chunked expansion so both sync and async callers stay aligned.
**/
static bool Nav_AStarSearch( const nav_mesh_t * mesh, const nav_node_ref_t &start_node, const nav_node_ref_t &goal_node,
	const Vector3 &agent_mins, const Vector3 &agent_maxs, std::vector<Vector3> &out_points, const svg_nav_path_policy_t * policy,
	const struct svg_nav_path_process_t * pathProcess, const nav_refine_corridor_t * refineCorridor ) {
	nav_query_debug_stats_t * const queryStats = s_nav_active_query_stats;
	const uint64_t refineStartMs = queryStats ? gi.GetRealSystemTime() : 0;

	/** 
	*	Prepare the incremental A*	state so we share the traversal rules with the async helpers.
	**/
	nav_a_star_state_t state = {};
   if ( !Nav_AStar_Init( &state, mesh, start_node, goal_node, agent_mins, agent_maxs, policy, refineCorridor, pathProcess ) ) {
		return false;
	}

	/** 
	*	Synchronous rebuild budget:
	*	    Disable per-step time limits so the helper can run to completion without per-step cap.
	**/
	state.search_budget_ms = 0;
	state.hit_time_budget = false;

	const int32_t expansionBudget = std::max( 1, SVG_Nav_GetAsyncRequestExpansions() );

	/** 
	*	Step the helper repeatedly so it can honor its chunked expansion and time budgets.
	**/
	while ( state.status == nav_a_star_status_t::Running ) {
		Nav_AStar_Step( &state, expansionBudget );
	}
	if ( queryStats ) {
		Nav_QueryStats_RecordFineStage( queryStats, state, gi.GetRealSystemTime() - refineStartMs );
	}

	/** 
	*	Finalize the path when the search completes successfully.
	**/
	if ( state.status == nav_a_star_status_t::Completed ) {
      const bool ok = Nav_AStar_Finalize( &state, &out_points );
		if ( ok && mesh && out_points.size() > 2 ) {
			/** 
			*	Apply the small sync simplification pass: cleanup, tiny LOS budget, and strict sync time cap.
			**/
			nav_path_simplify_options_t simplifyOptions = {};
			simplifyOptions.max_los_tests = nav_simplify_sync_max_los_tests ? nav_simplify_sync_max_los_tests->integer : 2;
			simplifyOptions.max_elapsed_ms = ( uint64_t )( nav_simplify_sync_max_ms ? std::max( 0, nav_simplify_sync_max_ms->integer ) : 1 );
			simplifyOptions.collinear_angle_degrees = nav_simplify_collinear_angle_degrees ? ( double )nav_simplify_collinear_angle_degrees->value : 4.0;
			simplifyOptions.remove_duplicates = true;
			simplifyOptions.prune_collinear = true;
			simplifyOptions.aggressiveness = nav_path_simplify_aggressiveness_t::SyncConservative;
			nav_path_simplify_stats_t simplifyStats = {};
            SVG_Nav_SimplifyPathPoints( mesh, agent_mins, agent_maxs, policy, &out_points, &simplifyOptions, &simplifyStats );
			if ( queryStats ) {
				Nav_QueryStats_RecordSimplification( queryStats, simplifyStats );
			}
		}
		Nav_AStar_Reset( &state );
		return ok;
	}

	/** 
	*	Cache diagnostic toggle so we can gate extra logging below.
	**/
	const bool navDiag = Nav_PathDiagEnabled();

	gi.dprintf( "[WARNING][NavPath][A* ] Pathfinding failed: No path found from (%.1f,%.1f,%.1f) to (%.1f,%.1f,%.1f)\n",
		start_node.worldPosition.x, start_node.worldPosition.y, start_node.worldPosition.z,
		goal_node.worldPosition.x, goal_node.worldPosition.y, goal_node.worldPosition.z );
	gi.dprintf( "[WARNING][NavPath][A* ] Search stats: explored=%d nodes, max_nodes=%d, open_list_empty=%s\n",
		( int32_t )state.nodes.size(),
		state.max_nodes,
		state.open_list.empty() ? "true" : "false" );

	/** 
	*	Emit detailed diagnostics only when explicitly requested.
	**/
	if ( navDiag ) {
		// Rate-limit verbose diagnostics to avoid overflowing the net/console buffer
		// when many A*	failures occur in quick succession. This keeps logs
		// useful while preventing spam that would drop messages.
		static uint64_t s_last_navpath_diag_ms = 0;
		const uint64_t nowMs = gi.GetRealSystemTime();
		const uint64_t diagCooldownMs = 200; // min interval between verbose prints
		if ( nowMs - s_last_navpath_diag_ms >= diagCooldownMs ) {
			s_last_navpath_diag_ms = nowMs;
			gi.dprintf( "[DEBUG][NavPath][Diag] A*	summary: neighbor_tries=%d, no_node=%d, edge_reject=%d, tile_filter_reject=%d, pass_through_prune=%d, stagnation=%d\n",
				state.neighbor_try_count, state.no_node_count, state.edge_reject_count, state.tile_filter_reject_count, state.pass_through_prune_count, state.stagnation_count );
			gi.dprintf( "[DEBUG][NavPath][Diag] A*	inputs: max_step=%.1f max_drop=%.1f cell_size_xy=%.1f z_quant=%.1f\n",
				nav_max_step ? nav_max_step->value : -1.0f,
				nav_max_drop_height_cap ? nav_max_drop_height_cap->value : -1.0f,
				mesh ? ( float )mesh->cell_size_xy : -1.0f,
				mesh ? ( float )mesh->z_quant : -1.0f );
			Nav_Debug_PrintNodeRef( "start_node", start_node );
			Nav_Debug_PrintNodeRef( "goal_node", goal_node );

			if ( !state.saw_vertical_neighbor ) {
				double nearestDelta = 0.0;
				int32_t layerCount = 0;
				if ( Nav_Debug_FindNearestLayerDelta( mesh, start_node, &nearestDelta, &layerCount ) ) {
					gi.dprintf( "[DEBUG][NavPath][Diag] No vertical neighbors; nearest layer delta=%.1f (layers=%d)\n",
						nearestDelta, layerCount );
				} else {
					gi.dprintf( "[DEBUG][NavPath][Diag] No vertical neighbors; no alternate layers found in start cell.\n" );
				}
			}

			const std::vector<nav_tile_cluster_key_t> * routeFilter = state.tileRouteFilter;
			if ( routeFilter && !routeFilter->empty() ) {
				gi.dprintf( "[DEBUG][NavPath][Diag] cluster route enforced: %d tiles\n",
					( int32_t )routeFilter->size() );
			} else {
				gi.dprintf( "[DEBUG][NavPath][Diag] cluster route: (none)\n" );
			}
		} // rate limit
	}

	/** 
	*	Print a concise failure reason based on the observed abort conditions.
	**/
	if ( state.hit_stagnation_limit ) {
		gi.dprintf( "[WARNING][NavPath][A* ] Failure reason: Search failed due to excessive stagnation after %d iterations.\n", state.stagnation_count );
	} else if ( ( int32_t )state.nodes.size() >= state.max_nodes ) {
		gi.dprintf( "[WARNING][NavPath][A* ] Failure reason: Search node limit reached (%d nodes). The navmesh may be too large or the goal is very far.\n", state.max_nodes );
	} else if ( state.open_list.empty() ) {
		gi.dprintf( "[WARNING][NavPath][A* ] Failure reason: Open list exhausted. This typically means:\n" );
		gi.dprintf( "[WARNING][NavPath][A* ]   1. No navmesh connectivity exists between start and goal Z-levels\n" );
		gi.dprintf( "[WARNING][NavPath][A* ]   2. All potential paths are blocked by obstacles\n" );
		gi.dprintf( "[WARNING][NavPath][A* ]   3. Edge validation (Nav_CanTraverseStep) rejected all connections\n" );
		gi.dprintf( "[WARNING][NavPath][A* ] Suggestions:\n" );
		gi.dprintf( "[WARNING][NavPath][A* ]   - Check nav_max_step (current: %.1f) and nav_max_drop_height_cap (current: %.1f)\n",
			nav_max_step ? nav_max_step->value : -1.0f,
			nav_max_drop_height_cap ? nav_max_drop_height_cap->value : -1.0f );
		gi.dprintf( "[WARNING][NavPath][A* ]   - Verify navmesh has stairs/ramps connecting the Z-levels\n" );
		gi.dprintf( "[WARNING][NavPath][A* ]   - Use 'nav_debug_draw 1' to visualize the navmesh\n" );
	} else {
		gi.dprintf( "[WARNING][NavPath][A* ] Failure reason: Unknown (status=%d).\n", ( int32_t )state.status );
	}

  // Emit extra diagnostic payload when A*	fails so callers can inspect node mapping
	// and candidate rejection counts. This log is gated by the global async stats
	// cvar to avoid overwhelming release logs.
	if ( Nav_PathDiagEnabled() ) {
		gi.dprintf( "[NavPath][Diag] neighbor_tries=%d no_node=%d edge_reject=%d tile_filter_reject=%d pass_through_prune=%d stagnation=%d\n",
			state.neighbor_try_count, state.no_node_count, state.edge_reject_count, state.tile_filter_reject_count, state.pass_through_prune_count, state.stagnation_count );
	   // Emit per-reason counters to show exactly which rejection paths dominated.
		Nav_Debug_LogEdgeRejectReasonCounters( "[NavPath][Diag]", state );
	}

	Nav_AStar_Reset( &state );
	return false;
}



//////////////////////////////////////////////////////////////////////////
/** 
* 
* 
* 
*	  Navigation System "Traversal Operations":
* 
* 
* 
**/

/** 
*	  @brief  Generate a traversal path between two world-space origins.
*	       Uses the navigation voxelmesh and A*	search to produce waypoints.
*	  @param  start_origin    World-space starting origin.
*	  @param  goal_origin     World-space destination origin.
*	  @param  out_path        Output path result (caller must free).
*	  @return True if a path was found, false otherwise.
**/
const bool SVG_Nav_GenerateTraversalPathForOrigin( const Vector3 &start_origin, const Vector3 &goal_origin, nav_traversal_path_t * out_path ) {
	nav_query_debug_scope_t queryStatsScope( &s_nav_last_query_stats );
	nav_query_debug_stats_t * const queryStats = s_nav_active_query_stats;

	if ( !out_path ) {
		return false;
	}

	SVG_Nav_FreeTraversalPath( out_path );

	if ( !g_nav_mesh ) {
		return false;
	}

	// Mesh pointer used for lookup/conversions.
	const nav_mesh_t * mesh = g_nav_mesh.get();

	nav_node_ref_t start_node = {};
	nav_node_ref_t goal_node = {};

	// Public API accepts feet-origin (z at feet). Convert to nav-center
	// space by applying the mesh agent center offset so node lookups use the
	// same reference as the async path preparer.
	const Vector3 start_center = SVG_Nav_ConvertFeetToCenter( mesh, start_origin );
	const Vector3 goal_center = SVG_Nav_ConvertFeetToCenter( mesh, goal_origin );

	if ( !Nav_FindNodeForPosition( mesh, start_center, start_center[ 2 ], &start_node, true ) ) {
		return false;
	}

	if ( !Nav_FindNodeForPosition( mesh, goal_center, goal_center[ 2 ], &goal_node, true ) ) {
		return false;
	}

	std::vector<Vector3> points;
 if ( Nav_ShouldAttemptDirectLosShortcut( mesh, start_node, goal_node )
		&& Nav_Traversal_TryBuildDirectShortcutPoints( mesh, start_node, goal_node, g_nav_mesh->agent_mins, g_nav_mesh->agent_maxs, nullptr, points ) ) {
	} else {
		/** 
	   * 	Hierarchical pre-pass: compute a coarse route filter and restrict A*	expansions
		*	to tiles on that route.
		**/
      const bool hierarchyPreferred = Nav_HierarchyRoutingPreferredForQuery( g_nav_mesh.get(), start_node.worldPosition, goal_node.worldPosition );
		nav_refine_corridor_t refineCorridor = {};
		bool usedHierarchyRoute = false;
		bool usedClusterRoute = false;
		int32_t coarseExpansions = 0;
		const uint64_t coarseStartMs = queryStats ? gi.GetRealSystemTime() : 0;
	 /** 
		*	Boundary-origin hardening:
		*	    Use resolved canonical node positions for the coarse route query so tile-route selection
		*	    stays aligned with the surfaces that node lookup actually recovered.
		**/
     const bool hasRefineCorridor = SVG_Nav_BuildRefineCorridor( g_nav_mesh.get(), start_node, goal_node, nullptr, &refineCorridor,
			&usedHierarchyRoute, &usedClusterRoute, &coarseExpansions );
		if ( queryStats ) {
			Nav_QueryStats_RecordCoarseStage( queryStats, usedHierarchyRoute, usedClusterRoute, coarseExpansions, gi.GetRealSystemTime() - coarseStartMs );
           Nav_QueryStats_RecordRefineCorridor( queryStats, hasRefineCorridor ? &refineCorridor : nullptr );
           Nav_QueryStats_RecordHierarchyRoutingChoice( queryStats, hierarchyPreferred, usedHierarchyRoute, hasRefineCorridor );
		}

     if ( !Nav_AStarSearch( g_nav_mesh.get(), start_node, goal_node, g_nav_mesh->agent_mins, g_nav_mesh->agent_maxs, points, nullptr, nullptr,
			hasRefineCorridor ? &refineCorridor : nullptr ) ) {
			return false;
		}
	}

	if ( points.empty() ) {
		return false;
	}

	out_path->num_points = ( int32_t )points.size();
	out_path->points = ( Vector3* )gi.TagMallocz( sizeof( Vector3 )* out_path->num_points, TAG_SVGAME_NAVMESH );
	memcpy( out_path->points, points.data(), sizeof( Vector3 )* out_path->num_points );
	Nav_QueryStats_RecordResult( queryStats, true, out_path->num_points );

	// Cache for always-on debug draw.
	NavDebug_PurgeCachedPaths();

	nav_debug_cached_path_t cached = {};
	cached.path.num_points = out_path->num_points;
	cached.path.points = ( Vector3* )gi.TagMallocz( sizeof( Vector3 )* out_path->num_points, TAG_SVGAME_NAVMESH );
	memcpy( cached.path.points, out_path->points, sizeof( Vector3 )* out_path->num_points );
	cached.expireTime = level.time + NAV_DEBUG_PATH_RETENTION;
	s_nav_debug_cached_paths.push_back( cached );

	/** 
	*	Debug draw: path segments.
	**/
	if ( NavDebug_Enabled() && nav_debug_draw_path && nav_debug_draw_path->integer != 0 ) {
		const int32_t segmentCount = std::max( 0, out_path->num_points - 1 );
		for ( int32_t i = 0; i < segmentCount; i++ ) {
			if ( !NavDebug_CanEmitSegments( 1 ) ) {
				break;
			}

			const Vector3 &a = out_path->points[ i ];
			const Vector3 &b = out_path->points[ i + 1 ];

			// Distance filter on segment midpoint.
			const Vector3 mid = { ( a[ 0 ] + b[ 0 ] )* 0.5f, ( a[ 1 ] + b[ 1 ] )* 0.5f, ( a[ 2 ] + b[ 2 ] )* 0.5f };
			if ( !NavDebug_PassesDistanceFilter( mid ) ) {
				continue;
			}

			SVG_DebugDrawLine_TE( a, b, MULTICAST_PVS, false );
			NavDebug_ConsumeSegments( 1 );
		}
	}

	return true;
}

const bool SVG_Nav_GenerateTraversalPathForOriginEx( const Vector3 &start_origin, const Vector3 &goal_origin, nav_traversal_path_t * out_path,
	const bool enable_goal_z_layer_blend, const double blend_start_dist, const double blend_full_dist ) {
	nav_query_debug_scope_t queryStatsScope( &s_nav_last_query_stats );
	nav_query_debug_stats_t * const queryStats = s_nav_active_query_stats;

	if ( !out_path ) {
		return false;
	}

	SVG_Nav_FreeTraversalPath( out_path );

	if ( !g_nav_mesh ) {
		return false;
	}
	const nav_mesh_t * mesh = g_nav_mesh.get();

	nav_node_ref_t start_node = {};
	nav_node_ref_t goal_node = {};
	bool startResolved = false;
	bool goalResolved = false;

	if ( enable_goal_z_layer_blend ) {
		// Convert caller feet-origin to nav-center using mesh agent hull.
		const Vector3 start_center = SVG_Nav_ConvertFeetToCenter( mesh, start_origin );
		const Vector3 goal_center = SVG_Nav_ConvertFeetToCenter( mesh, goal_origin );

		if ( !Nav_FindNodeForPosition_BlendZ( mesh, start_center, start_center[ 2 ], goal_center[ 2 ], start_center, goal_center,
			blend_start_dist, blend_full_dist, &start_node, true ) ) {
			return false;
		}
		// Use blend for goal selection as well to prefer the goal's Z layer when far away.
		if ( !Nav_FindNodeForPosition_BlendZ( mesh, goal_center, start_center[ 2 ], goal_center[ 2 ], start_center, goal_center,
			blend_start_dist, blend_full_dist, &goal_node, true ) ) {
			return false;
		}
	} else {
		// Convert caller feet-origin to nav-center using provided agent bbox.
		const Vector3 start_center = SVG_Nav_ConvertFeetToCenter( mesh, start_origin );
		const Vector3 goal_center = SVG_Nav_ConvertFeetToCenter( mesh, goal_origin );

		if ( !Nav_FindNodeForPosition( mesh, start_center, start_center[ 2 ], &start_node, true ) ) {
			return false;
		}
		if ( !Nav_FindNodeForPosition( mesh, goal_center, goal_center[ 2 ], &goal_node, true ) ) {
			return false;
		}
	}

	std::vector<Vector3> points;
 if ( Nav_ShouldAttemptDirectLosShortcut( mesh, start_node, goal_node )
		&& Nav_Traversal_TryBuildDirectShortcutPoints( mesh, start_node, goal_node, mesh->agent_mins, mesh->agent_maxs, nullptr, points ) ) {
	} else {
		/** 
	   * 	Hierarchical pre-pass: compute a coarse route filter and restrict A*	expansions
		*	to tiles on that route.
		**/
      const bool hierarchyPreferred = Nav_HierarchyRoutingPreferredForQuery( mesh, start_node.worldPosition, goal_node.worldPosition );
		nav_refine_corridor_t refineCorridor = {};
		bool usedHierarchyRoute = false;
		bool usedClusterRoute = false;
		int32_t coarseExpansions = 0;
		const uint64_t coarseStartMs = queryStats ? gi.GetRealSystemTime() : 0;
	 /** 
		*	Boundary-origin hardening:
		*	    Use resolved canonical node positions for the coarse route query so tile-route selection
		*	    stays aligned with the surfaces that node lookup actually recovered.
		**/
     const bool hasRefineCorridor = SVG_Nav_BuildRefineCorridor( g_nav_mesh.get(), start_node, goal_node, nullptr, &refineCorridor,
			&usedHierarchyRoute, &usedClusterRoute, &coarseExpansions );
		if ( queryStats ) {
			Nav_QueryStats_RecordCoarseStage( queryStats, usedHierarchyRoute, usedClusterRoute, coarseExpansions, gi.GetRealSystemTime() - coarseStartMs );
           Nav_QueryStats_RecordRefineCorridor( queryStats, hasRefineCorridor ? &refineCorridor : nullptr );
           Nav_QueryStats_RecordHierarchyRoutingChoice( queryStats, hierarchyPreferred, usedHierarchyRoute, hasRefineCorridor );
		}

     if ( !Nav_AStarSearch( mesh, start_node, goal_node, mesh->agent_mins, mesh->agent_maxs, points, nullptr, nullptr,
			hasRefineCorridor ? &refineCorridor : nullptr ) ) {
			return false;
		}
	}

	if ( points.empty() ) {
		return false;
	}

	out_path->num_points = ( int32_t )points.size();
	out_path->points = ( Vector3* )gi.TagMallocz( sizeof( Vector3 )* out_path->num_points, TAG_SVGAME_NAVMESH );
	memcpy( out_path->points, points.data(), sizeof( Vector3 )* out_path->num_points );
	Nav_QueryStats_RecordResult( queryStats, true, out_path->num_points );

	// Cache for always-on debug draw.
	NavDebug_PurgeCachedPaths();

	nav_debug_cached_path_t cached = {};
	cached.path.num_points = out_path->num_points;
	cached.path.points = ( Vector3* )gi.TagMallocz( sizeof( Vector3 )* out_path->num_points, TAG_SVGAME_NAVMESH );
	memcpy( cached.path.points, out_path->points, sizeof( Vector3 )* out_path->num_points );
	cached.expireTime = level.time + NAV_DEBUG_PATH_RETENTION;
	s_nav_debug_cached_paths.push_back( cached );

 /** 
	*	  	Emit optional debug draw segments for the cached path.
	**/
	if ( NavDebug_Enabled() && nav_debug_draw_path && nav_debug_draw_path->integer != 0 ) {
		const int32_t segmentCount = std::max( 0, out_path->num_points - 1 );
		for ( int32_t i = 0; i < segmentCount; i++ ) {
			if ( !NavDebug_CanEmitSegments( 1 ) ) {
				break;
			}

			const Vector3 &a = out_path->points[ i ];
			const Vector3 &b = out_path->points[ i + 1 ];

			const Vector3 mid = { ( a[ 0 ] + b[ 0 ] )* 0.5f, ( a[ 1 ] + b[ 1 ] )* 0.5f, ( a[ 2 ] + b[ 2 ] )* 0.5f };
			if ( !NavDebug_PassesDistanceFilter( mid ) ) {
				continue;
			}

			SVG_DebugDrawLine_TE( a, b, MULTICAST_PVS, false );
			NavDebug_ConsumeSegments( 1 );
		}
	}

	return true;
}

const bool SVG_Nav_GenerateTraversalPathForOrigin_WithAgentBBox( const Vector3 &start_origin, const Vector3 &goal_origin, nav_traversal_path_t * out_path,
	const Vector3 &agent_mins, const Vector3 &agent_maxs, const svg_nav_path_policy_t * policy ) {
	nav_query_debug_scope_t queryStatsScope( &s_nav_last_query_stats );
	nav_query_debug_stats_t * const queryStats = s_nav_active_query_stats;

   /** 
	*	Sanity checks: require output storage.
	**/
	if ( !out_path ) {
		return false;
	}

   /** 
	*	Reset any previous path contents owned by the caller.
	**/
	SVG_Nav_FreeTraversalPath( out_path );

 /** 
	*	Require a loaded navmesh before attempting sync traversal path generation.
	**/
	if ( !g_nav_mesh ) {
		return false;
	}

	/** 
 * Cache the navmesh pointer and convert public feet-origin inputs into nav-center space.
	**/
	const nav_mesh_t * mesh = g_nav_mesh.get();
	if ( !mesh ) {
		return false;
	}

	const Vector3 start_center = SVG_Nav_ConvertFeetToCenter( mesh, start_origin, &agent_mins, &agent_maxs );
	const Vector3 goal_center = SVG_Nav_ConvertFeetToCenter( mesh, goal_origin, &agent_mins, &agent_maxs );

	/** 
	*	Boundary diagnostics:
	*	    This overload accepts feet-origin inputs and must convert exactly once
	*	    before node resolution.
	**/
	if ( Nav_PathDiagEnabled() ) {
		gi.dprintf( "[DEBUG][NavPath][Boundary] WithAgentBBox feet->center start=(%.1f %.1f %.1f)->(%.1f %.1f %.1f) goal=(%.1f %.1f %.1f)->(%.1f %.1f %.1f)\n",
			start_origin.x, start_origin.y, start_origin.z,
			start_center.x, start_center.y, start_center.z,
			goal_origin.x, goal_origin.y, goal_origin.z,
			goal_center.x, goal_center.y, goal_center.z );
	}

   /** 
	*	Generate traversal waypoints while honoring any caller-supplied policy blend preference first.
	**/
	std::vector<Vector3> points;
	const bool prefer_blended_lookup = policy ? policy->enable_goal_z_layer_blend : false;
	const double preferred_blend_start = policy ? policy->blend_start_dist : NAV_DEFAULT_BLEND_DIST_START;
	const double preferred_blend_full = policy ? policy->blend_full_dist : NAV_DEFAULT_BLEND_DIST_FULL;
	if ( !Nav_Traversal_TryGeneratePointsWithEndpointFallbacks( mesh, start_center, goal_center,
		agent_mins, agent_maxs, policy, nullptr, prefer_blended_lookup,
		preferred_blend_start, preferred_blend_full, points ) ) {
		return false;
	}

 /** 
	*	Reject empty waypoint output before allocating the public path buffer.
	**/
	if ( points.empty() ) {
		return false;
	}

	/** 
	*	Copy the generated waypoints into the caller-owned traversal path.
	**/
	out_path->num_points = ( int32_t )points.size();
	out_path->points = ( Vector3* )gi.TagMallocz( sizeof( Vector3 )* out_path->num_points, TAG_SVGAME_NAVMESH );
	memcpy( out_path->points, points.data(), sizeof( Vector3 )* out_path->num_points );
	Nav_QueryStats_RecordResult( queryStats, true, out_path->num_points );
	return true;
}

const bool SVG_Nav_GenerateTraversalPathForOriginEx_WithAgentBBox( const Vector3 &start_origin, const Vector3 &goal_origin, nav_traversal_path_t * out_path,
	const Vector3 &agent_mins, const Vector3 &agent_maxs, const svg_nav_path_policy_t * policy, const bool enable_goal_z_layer_blend, const double blend_start_dist, const double blend_full_dist,
	const struct svg_nav_path_process_t * pathProcess ) {
	nav_query_debug_scope_t queryStatsScope( &s_nav_last_query_stats );
	nav_query_debug_stats_t * const queryStats = s_nav_active_query_stats;

	if ( !out_path ) {
		return false;
	}

	SVG_Nav_FreeTraversalPath( out_path );

	if ( !g_nav_mesh ) {
		return false;
	}

	nav_node_ref_t start_node = {};
	nav_node_ref_t goal_node = {};
	bool startResolved = false;
	bool goalResolved = false;

	if ( enable_goal_z_layer_blend ) {
		// Convert caller feet-origin to nav-center using provided agent bbox.
		const nav_mesh_t * mesh = g_nav_mesh.get();
		if ( !mesh ) {
			return false;
		}
		const Vector3 start_center = SVG_Nav_ConvertFeetToCenter( mesh, start_origin, &agent_mins, &agent_maxs );
		const Vector3 goal_center = SVG_Nav_ConvertFeetToCenter( mesh, goal_origin, &agent_mins, &agent_maxs );
		startResolved = Nav_FindNodeForPosition_BlendZ( mesh, start_center, start_center[ 2 ], goal_center[ 2 ], start_center, goal_center,
			blend_start_dist, blend_full_dist, &start_node, true );
		goalResolved = Nav_FindNodeForPosition_BlendZ( mesh, goal_center, start_center[ 2 ], goal_center[ 2 ], start_center, goal_center,
			blend_start_dist, blend_full_dist, &goal_node, true );
	} else {
		const nav_mesh_t * mesh = g_nav_mesh.get();
		if ( !mesh ) {
			return false;
		}
		const Vector3 start_center = SVG_Nav_ConvertFeetToCenter( mesh, start_origin, &agent_mins, &agent_maxs );
		const Vector3 goal_center = SVG_Nav_ConvertFeetToCenter( mesh, goal_origin, &agent_mins, &agent_maxs );
		startResolved = Nav_FindNodeForPosition( mesh, start_center, start_center[ 2 ], &start_node, true );
		goalResolved = Nav_FindNodeForPosition( mesh, goal_center, goal_center[ 2 ], &goal_node, true );
	}

	/** 
	*	Targeted diagnostics: node resolution.
	*	    This is the quickest way to separate the "no nodes" case from true A*	failures.
	**/
	if ( Nav_PathDiagEnabled() ) {
		gi.dprintf( "[DEBUG][NavPath][Diag] Resolve nodes: start_ok=%d goal_ok=%d blend=%d\n",
			startResolved ? 1 : 0, goalResolved ? 1 : 0, enable_goal_z_layer_blend ? 1 : 0 );
		if ( startResolved ) {
			Nav_Debug_PrintNodeRef( "start_node", start_node );
		}
		if ( goalResolved ) {
			Nav_Debug_PrintNodeRef( "goal_node", goal_node );
		}
	}

	// Early out: we cannot pathfind without both endpoints.
	if ( !startResolved || !goalResolved ) {
		if ( Nav_PathDiagEnabled() ) {
			gi.dprintf( "[WARNING][NavPath][Diag] Node resolution failed: start_ok=%d goal_ok=%d. Likely: missing nav tiles near endpoint(s) or bad Z-layer selection.\n",
				startResolved ? 1 : 0, goalResolved ? 1 : 0 );
		}
		return false;
	}

	std::vector<Vector3> points;
  if ( Nav_ShouldAttemptDirectLosShortcut( g_nav_mesh.get(), start_node, goal_node )
		&& Nav_Traversal_TryBuildDirectShortcutPoints( g_nav_mesh.get(), start_node, goal_node, agent_mins, agent_maxs, policy, points ) ) {
	} else {
		/** 
	   * 	Hierarchical pre-pass: compute a coarse route filter and restrict A*	expansions
		*	to tiles on that route.
		*
		*	We still pass `pathProcess` through so the fine A*	can apply per-entity
		*	failure penalties.
		**/
      const bool hierarchyPreferred = Nav_HierarchyRoutingPreferredForQuery( g_nav_mesh.get(), start_node.worldPosition, goal_node.worldPosition );
		nav_refine_corridor_t refineCorridor = {};
		bool usedHierarchyRoute = false;
		bool usedClusterRoute = false;
		int32_t coarseExpansions = 0;
		const uint64_t coarseStartMs = queryStats ? gi.GetRealSystemTime() : 0;
	 /** 
		*	Boundary-origin hardening:
		*	    Use resolved canonical node positions for the coarse route query so tile-route selection
		*	    stays aligned with the surfaces that node lookup actually recovered.
		**/
      const bool hasRefineCorridor = SVG_Nav_BuildRefineCorridor( g_nav_mesh.get(), start_node, goal_node, policy, &refineCorridor,
			&usedHierarchyRoute, &usedClusterRoute, &coarseExpansions );
		if ( queryStats ) {
			Nav_QueryStats_RecordCoarseStage( queryStats, usedHierarchyRoute, usedClusterRoute, coarseExpansions, gi.GetRealSystemTime() - coarseStartMs );
           Nav_QueryStats_RecordRefineCorridor( queryStats, hasRefineCorridor ? &refineCorridor : nullptr );
           Nav_QueryStats_RecordHierarchyRoutingChoice( queryStats, hierarchyPreferred, usedHierarchyRoute, hasRefineCorridor );
		}

      if ( !Nav_AStarSearch( g_nav_mesh.get(), start_node, goal_node, agent_mins, agent_maxs, points, policy, pathProcess,
			hasRefineCorridor ? &refineCorridor : nullptr ) ) {
			return false;
		}
	}

	if ( points.empty() ) {
		return false;
	}

	out_path->num_points = ( int32_t )points.size();
	out_path->points = ( Vector3* )gi.TagMallocz( sizeof( Vector3 )* out_path->num_points, TAG_SVGAME_NAVMESH );
	memcpy( out_path->points, points.data(), sizeof( Vector3 )* out_path->num_points );
	Nav_QueryStats_RecordResult( queryStats, true, out_path->num_points );
	return true;
}

/** 
*	  @brief  Free a traversal path allocated by SVG_Nav_GenerateTraversalPathForOrigin.
*	  @param  path    Path structure to free.
**/
void SVG_Nav_FreeTraversalPath( nav_traversal_path_t * path ) {
	if ( !path ) {
		return;
	}

	if ( path->points ) {
		gi.TagFree( path->points );
	}

	path->points = nullptr;
	path->num_points = 0;
}

/** 
*	  @brief  Query movement direction along a traversal path.
*	       Advances the waypoint index as the caller reaches waypoints.
*	  @param  path            Path to follow.
*	  @param  current_origin  Current world-space origin.
*	  @param  waypoint_radius Radius for waypoint completion.
*	  @param  policy          Optional traversal policy for per-agent constraints.
*	  @param  inout_index     Current waypoint index (updated on success).
*	  @param  out_direction   Output normalized movement direction.
*	  @return True if a valid direction was produced, false if path is complete/invalid.
**/
const bool SVG_Nav_QueryMovementDirection( const nav_traversal_path_t * path, const Vector3 &current_origin,
	double waypoint_radius, const svg_nav_path_policy_t * policy, int32_t * inout_index, Vector3 * out_direction ) {
/** 
* 	Sanity checks: validate inputs and ensure we have a usable path.
**/
	if ( !path || !inout_index || !out_direction ) {
		return false;
	}
	if ( path->num_points <= 0 || !path->points ) {
		return false;
	}

	/** 
	*	Clamp the waypoint radius to a minimum value.
	*	This avoids degenerate behavior where micro radii prevent waypoint completion
	*	when the mover cannot stand exactly on the path point due to collision.
	**/
	waypoint_radius = std::max( waypoint_radius, 8.0 );

	int32_t index = * inout_index;
	if ( index < 0 ) {
		index = 0;
	}

	const double waypoint_radius_sqr = waypoint_radius* waypoint_radius;

	// Advance waypoints using 3D distance so stairs count toward completion.
	while ( index < path->num_points ) {
		const Vector3 delta = QM_Vector3Subtract( path->points[ index ], current_origin );
		const double dist_sqr = ( delta[ 0 ]* delta[ 0 ] ) + ( delta[ 1 ]* delta[ 1 ] ) + ( delta[ 2 ]* delta[ 2 ] );
		if ( dist_sqr > waypoint_radius_sqr ) {
			break;
		}
		index++;
	}

	/** 
	*	Stuck heuristic (3D query): if we are not making 2D progress towards the current waypoint,
	*	advance it after a few frames. This is primarily for cornering; we intentionally keep
	*	it based on XY so stairs still work with the 2D based Advance2D variant.
	**/
	if ( index < path->num_points ) {
		thread_local int32_t s_last_index = -1;
		thread_local double s_last_dist2d_sqr = 0.0f;
		thread_local int32_t s_no_progress_frames = 0;
		thread_local int32_t s_last_frame = -1;

		if ( s_last_frame != ( int32_t )level.frameNumber || s_last_index != index ) {
			s_last_frame = ( int32_t )level.frameNumber;
			s_last_index = index;
			const Vector3 d0 = QM_Vector3Subtract( path->points[ index ], current_origin );
			s_last_dist2d_sqr = ( d0[ 0 ]* d0[ 0 ] ) + ( d0[ 1 ]* d0[ 1 ] );
			s_no_progress_frames = 0;
		} else {
			const Vector3 d1 = QM_Vector3Subtract( path->points[ index ], current_origin );
			const double dist2d_sqr = ( d1[ 0 ]* d1[ 0 ] ) + ( d1[ 1 ]* d1[ 1 ] );
			const double near_sqr = waypoint_radius_sqr* 9.0f;
			if ( dist2d_sqr <= near_sqr ) {
				const double improve_eps = 1.0f;
				if ( dist2d_sqr >= ( s_last_dist2d_sqr - improve_eps ) ) {
					s_no_progress_frames++;
				} else {
					s_no_progress_frames = 0;
				}
				if ( s_no_progress_frames >= 6 && ( index + 1 ) < path->num_points ) {
					index++;
					s_no_progress_frames = 0;
				}
			}
			s_last_dist2d_sqr = dist2d_sqr;
			s_last_frame = ( int32_t )level.frameNumber;
		}
	}

	if ( index >= path->num_points ) {
		*inout_index = path->num_points;
		return false;
	}

	// Produce a 3D direction. Clamp vertical component to avoid large up/down jumps.
	Vector3 direction = QM_Vector3Subtract( path->points[ index ], current_origin );

	// clamp Z to a reasonable step height if the nav mesh is available
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

/** 
* 
* 
* 
* 	Navigation Movement Tests:
* 
* 
* 
**/
/** 
* 	@brief	Low-level trace wrapper that supports world or inline-model clip.
**/
const inline svg_trace_t Nav_Trace( const Vector3 &start, const Vector3 &mins, const Vector3 &maxs, const Vector3 &end,
	const edict_ptr_t * clip_entity, const cm_contents_t mask ) {
	/** 
	*	`Inline-model` traversal:
	*	    Treat only `non-world` entities as explicit `inline-model` clips.
	*	    World clipping is handled by the dedicated `world-only` branch below.
	**/
	if ( clip_entity && clip_entity->s.number != ENTITYNUM_WORLD ) {
		// Note that `gi.clip` treats a `nullptr` entity as a world trace, so we can directly
		// pass the `clip_entity` through without special handling.

		// <Q2RTXP>: WID: TODO: Should perform a lifted retry here as well to filter startsolid/allsolid artifacts on inline models, 
		// but that is not currently a known issue so defer until we have repro cases.
		return gi.clip( clip_entity, &start, &mins, &maxs, &end, mask );
	}

 /** 
	*	World traversal policy for nav edge validation:
	*	    Keep nav-owned collision semantics here and clip only against the
	*	    world or explicit inline-model entity. A*	step testing should not
	*	    depend on the generic scene trace path because that would pull all
	*	    world entities into a validator that is supposed to reason about nav
	*	    tiles, cells, and layers.
	*
	*	    @note	When the initial world clip starts in solid, perform a small
	*	            lifted retry to filter floor-boundary precision cases that can
	*	            otherwise appear as false startsolid/allsolid hits.
	**/
	const svg_trace_t worldTrace = gi.clip( nullptr, &start, &mins, &maxs, &end, mask );

	/** 
	*	Fast path: keep the primary world trace when it is already valid.
	**/
	if ( !worldTrace.startsolid && !worldTrace.allsolid ) {
		return worldTrace;
	}

	/** 
	*	Boundary hardening:
	*	    Retry once with a tiny vertical lift to avoid contact-on-plane
	*	    precision artifacts that can mark traces as startsolid/allsolid.
	**/
	Vector3 retryStart = start;
	Vector3 retryEnd = end;
	constexpr float kWorldRetryLift = 0.125f;
	retryStart[ 2 ] += kWorldRetryLift;
	retryEnd[ 2 ] += kWorldRetryLift;

	svg_trace_t retryTrace = gi.clip( nullptr, &retryStart, &mins, &maxs, &retryEnd, mask );

	/** 
	*	Prefer the retry only when it clearly resolves solid-start artifacts.
	**/
	if ( !retryTrace.startsolid && !retryTrace.allsolid &&
		( retryTrace.fraction >= worldTrace.fraction || worldTrace.fraction <= 0.0f ) ) {
		// Reproject the lifted endpos back to the original Z frame for callers.
		retryTrace.endpos[ 2 ] -= kWorldRetryLift;
		return retryTrace;
	}

	// Fall back to the original world trace when retry did not improve confidence.
	return worldTrace;
}

/** 
* 	@brief	Low-level trace wrapper that supports world or inline-model clip.
**/
/** 
* 	@brief	Performs a simple PMove-like step traversal test (3-trace).
* 
* 			This is intentionally conservative and is used only to validate edges in A* :
* 			1) Try direct horizontal move.
* 			2) If blocked, try stepping up <= max step, then horizontal move.
* 			3) Trace down to find ground.
* 	@param	mesh			Navigation mesh.
*		@param	startPos		Start world position.
*		@param	endPos			End world position.
*		@param	clip_entity		Entity to use for clipping (nullptr for world).
* 
* 	@return	True if the traversal is possible, false otherwise.
**/
const bool Nav_CanTraverseStep( const nav_mesh_t * mesh, const Vector3 &startPos, const Vector3 &endPos, const edict_ptr_t * clip_entity, svg_nav_path_policy_t &policy ) {
	if ( !mesh ) {
		return false;
	}

	// Ignore Z from caller; we compute step behavior ourselves.
	Vector3 start = startPos;
	Vector3 goal = endPos;
	goal[ 2 ] = start[ 2 ];

	const Vector3 mins = mesh->agent_mins;
	const Vector3 maxs = mesh->agent_maxs;

	return Nav_CanTraverseStep_ExplicitBBox( mesh, startPos, endPos, mins, maxs, clip_entity, &policy );
}

/** 
*	 @brief	Performs a simple PMove-like step traversal test (3-trace) with explicit agent bbox.
*				This is intentionally conservative and is used only to validate edges in A* :
*				1) Try direct horizontal move.
*				2) If blocked, try stepping up <= max step, then horizontal move.
*				3) Trace down to find ground.
*		@param	mesh			Navigation mesh.
*		@param	startPos		Start world position.
*		@param	endPos			End world position.
*		@param	mins			Agent bounding box minimums.
*		@param	maxs			Agent bounding box maximums.
*		@param	clip_entity		Entity to use for clipping (nullptr for world).
*		@param	policy			Optional path policy for tuning step behavior.
*		@return	True if the traversal is possible, false otherwise.
**/
/** 
*	 @brief	Internal single-step traversal validation used by the segmented ramp helper.
*	 @param	mesh			Navigation mesh.
*	 @param	startPos	Start world position.
*	 @param	endPos		End world position.
*	 @param	mins		Agent bounding box minimums.
*	 @param	maxs		Agent bounding box maximums.
*	 @param	clip_entity	Entity to use for clipping (nullptr for world).
*	 @param	policy		Optional path policy for tuning step behavior.
*	 @return	True if the traversal is possible, false otherwise.
*	 @note	This function assumes any segmented-ramp handling has already been applied.
**/
static const bool Nav_CanTraverseStep_ExplicitBBox_Single( const nav_mesh_t * mesh, const nav_node_ref_t &start_node, const nav_node_ref_t &end_node, const Vector3 &mins, const Vector3 &maxs, const edict_ptr_t * clip_entity, const svg_nav_path_policy_t * policy, nav_edge_reject_reason_t * out_reason, const cm_contents_t stepTraceMask ) {
  /** 
	*	Sanity checks: require a valid mesh.
	**/
	if ( !mesh ) {
		return false;
	}
	( void )clip_entity;
	( void )stepTraceMask;

	const Vector3 &startPos = start_node.worldPosition;
	const Vector3 &endPos = end_node.worldPosition;

	//! Tracks the last frame we reported a step-test failure.
	static int32_t s_nav_step_test_failure_frame = -1;
	//! Indicates whether we already printed a failure reason this frame.
	static bool s_nav_step_test_failure_logged = false;
	auto ReportStepTestFailureOnce = [startPos, endPos]( const char * reason ) {
		const int32_t currentFrame = ( int32_t )level.frameNumber;
		if ( s_nav_step_test_failure_frame != currentFrame ) {
			s_nav_step_test_failure_frame = currentFrame;
			s_nav_step_test_failure_logged = false;
		}
		if ( s_nav_step_test_failure_logged ) {
			return;
		}
		s_nav_step_test_failure_logged = true;
		if ( !nav_debug_draw_rejects || nav_debug_draw_rejects->integer == 0 ) {
			return;
		}
		gi.dprintf( "[DEBUG][NavPath][StepTest] Edge from (%.1f %.1f %.1f) to (%.1f %.1f %.1f) failed: %s\n",
			startPos[ 0 ], startPos[ 1 ], startPos[ 2 ],
			endPos[ 0 ], endPos[ 1 ], endPos[ 2 ],
			reason ? reason : "unspecified" );
		};

		/** 
		*	Resolve canonical tile/cell/layer views for both endpoints.
		**/
	const nav_tile_t * start_tile = nullptr;
	const nav_xy_cell_t * start_cell = nullptr;
	const nav_layer_t * start_layer = nullptr;
	const nav_tile_t * end_tile = nullptr;
	const nav_xy_cell_t * end_cell = nullptr;
	const nav_layer_t * end_layer = nullptr;
	if ( !Nav_TryGetNodeLayerView( mesh, start_node, &start_tile, &start_cell, &start_layer ) ||
		!Nav_TryGetNodeLayerView( mesh, end_node, &end_tile, &end_cell, &end_layer ) ) {
		if ( out_reason ) {
			*out_reason = nav_edge_reject_reason_t::NoNode;
		}
		return false;
	}

	/** 
	*	Resolve global cell-grid coordinates so the validator can reason about local hops.
	**/
	int32_t start_cell_x = 0;
	int32_t start_cell_y = 0;
	int32_t end_cell_x = 0;
	int32_t end_cell_y = 0;
	if ( !Nav_TryGetGlobalCellCoords( mesh, start_node, &start_cell_x, &start_cell_y ) ||
		!Nav_TryGetGlobalCellCoords( mesh, end_node, &end_cell_x, &end_cell_y ) ) {
		if ( out_reason ) {
			*out_reason = nav_edge_reject_reason_t::NoNode;
		}
		return false;
	}

	/** 
	*	Compute local traversal limits and vertical deltas.
	**/
	const double desiredDz = ( double )endPos[ 2 ] - ( double )startPos[ 2 ];
	const double requiredUp = std::max( 0.0, desiredDz );
	const double requiredDown = std::max( 0.0, -desiredDz );
	const double stepSize = ( policy && policy->max_step_height > 0.0 ) ? ( double )policy->max_step_height : ( double )mesh->max_step;
	const double dropCap = ( policy && policy->max_drop_height_cap > 0.0 )
		? ( double )policy->max_drop_height_cap
		: ( nav_max_drop_height_cap ? nav_max_drop_height_cap->value : 128.0f );
	const double sameLevelTolerance = std::max( ( double )PHYS_STEP_GROUND_DIST, ( double )mesh->z_quant* 0.5 );
	const double requiredClearance = std::max( 0.0, ( double )maxs[ 2 ] - ( double )mins[ 2 ] );

	/** 
	*	Reject non-local single-step edges.
	*	    Segmented callers should already have split longer moves into adjacent hops.
	**/
	const int32_t delta_cell_x = end_cell_x - start_cell_x;
	const int32_t delta_cell_y = end_cell_y - start_cell_y;
	if ( std::abs( delta_cell_x ) > 1 || std::abs( delta_cell_y ) > 1 ) {
		if ( out_reason ) {
			*out_reason = nav_edge_reject_reason_t::StepTest;
		}
		ReportStepTestFailureOnce( "non-local single-step edge" );
		return false;
	}

	/** 
	*	Ensure both endpoint layers are walkable and have enough clearance for the agent hull.
	**/
	if ( ( start_layer->flags & NAV_FLAG_WALKABLE ) == 0 || ( end_layer->flags & NAV_FLAG_WALKABLE ) == 0 ) {
		if ( out_reason ) {
			*out_reason = nav_edge_reject_reason_t::StepTest;
		}
		ReportStepTestFailureOnce( "layer is not marked walkable" );
		return false;
	}

	const double startClearance = Nav_GetLayerClearanceWorld( mesh, * start_layer );
	const double endClearance = Nav_GetLayerClearanceWorld( mesh, * end_layer );
	if ( startClearance + PHYS_STEP_GROUND_DIST < requiredClearance ) {
		NavDebug_RecordReject( startPos, endPos, NAV_DEBUG_REJECT_REASON_CLEARANCE );
		if ( out_reason ) {
			*out_reason = nav_edge_reject_reason_t::StepTest;
		}
		ReportStepTestFailureOnce( "start layer clearance below agent hull height" );
		return false;
	}
	if ( endClearance + PHYS_STEP_GROUND_DIST < requiredClearance ) {
		NavDebug_RecordReject( startPos, endPos, NAV_DEBUG_REJECT_REASON_CLEARANCE );
		if ( out_reason ) {
			*out_reason = nav_edge_reject_reason_t::StepTest;
		}
		ReportStepTestFailureOnce( "end layer clearance below agent hull height" );
		return false;
	}

	/** 
	*	Same-level transitions are valid once local adjacency and clearance are satisfied.
	**/
	if ( std::fabs( desiredDz ) <= sameLevelTolerance ) {
		return true;
	}

	/** 
	*	Uphill transitions must fit within the configured step height.
	**/
	if ( requiredUp > 0.0 ) {
		if ( requiredUp > stepSize ) {
			if ( out_reason ) {
				*out_reason = nav_edge_reject_reason_t::StepTest;
			}
			ReportStepTestFailureOnce( "required climb exceeds configured step" );
			return false;
		}
		return true;
	}

	/** 
	*	Downward transitions must stay within the configured drop cap.
	**/
	if ( requiredDown > dropCap ) {
		NavDebug_RecordReject( startPos, endPos, NAV_DEBUG_REJECT_REASON_DROP_CAP );
		if ( out_reason ) {
			*out_reason = nav_edge_reject_reason_t::DropCap;
		}
		ReportStepTestFailureOnce( "drop cap exceeded" );
		return false;
	}

	/** 
	*	Conservative success case:
	*	    This canonical local hop is adjacent, walkable, and satisfies clearance plus
	*	    step/drop limits, so no generic tracing is needed for A*	edge validation.
	**/
	return true;
}