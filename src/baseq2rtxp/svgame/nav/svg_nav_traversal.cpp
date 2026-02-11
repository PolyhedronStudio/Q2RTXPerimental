/********************************************************************
*
*
*	SVGame: VoxelMesh Traversing
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
#include "svgame/nav/svg_nav_request.h"
#include "svgame/nav/svg_nav_traversal.h"
#include "svgame/nav/svg_nav_traversal_async.h"

#include "svgame/entities/svg_player_edict.h"

// <Q2RTXP>: TODO: Move to shared? Ehh.. 
// Common BSP access for navigation generation.
#include "common/bsp.h"
#include "common/files.h"

#include <cmath>

extern cvar_t *nav_max_step;
extern cvar_t *nav_drop_cap;
extern cvar_t *nav_debug_draw;
extern cvar_t *nav_debug_draw_path;
/**
*    Targeted pathfinding diagnostics:
*        These are intentionally gated behind `nav_debug_draw >= 2` to avoid
*        spamming logs during normal gameplay.
**/
static inline bool Nav_PathDiagEnabled( void ) {
	return nav_debug_draw && nav_debug_draw->integer >= 2;
}

static void Nav_Debug_PrintNodeRef( const char *label, const nav_node_ref_t &n ) {
	if ( !label ) {
		label = "node";
	}

	gi.dprintf( "[DEBUG][NavPath][Diag] %s: pos(%.1f %.1f %.1f) key(leaf=%d tile=%d cell=%d layer=%d)\n",
		label,
		n.position.x, n.position.y, n.position.z,
		n.key.leaf_index, n.key.tile_index, n.key.cell_index, n.key.layer_index );
}

/**
*    @brief	Find the nearest Z-layer delta for the given node within its cell.
*    @param	mesh		Navigation mesh containing the node.
*    @param	node		Navigation node to inspect.
*    @param	out_delta	[out] Closest absolute Z delta to another layer in the same cell.
*    @param	out_layers	[out] Number of layers found in the cell (optional).
*    @return	True if a valid delta was found, false otherwise.
*    @note	This is a diagnostic helper used to report missing vertical connectivity.
**/
static const bool Nav_Debug_FindNearestLayerDelta( const nav_mesh_t *mesh, const nav_node_ref_t &node, double *out_delta, int32_t *out_layers ) {
	/**
	*    Sanity: require mesh and output pointer.
	**/
	if ( !mesh || !out_delta ) {
		return false;
	}

	/**
	*    Validate tile/cell indices before inspecting layers.
	**/
	if ( node.key.tile_index < 0 || node.key.tile_index >= ( int32_t )mesh->world_tiles.size() ) {
		return false;
	}
	const nav_tile_t &tile = mesh->world_tiles[ node.key.tile_index ];
	if ( node.key.cell_index < 0 || node.key.cell_index >= ( mesh->tile_size * mesh->tile_size ) ) {
		return false;
	}
	const nav_xy_cell_t &cell = tile.cells[ node.key.cell_index ];
	if ( out_layers ) {
		*out_layers = cell.num_layers;
	}
	if ( cell.num_layers <= 1 || !cell.layers ) {
		return false;
	}

	/**
	*    Scan all layers to find the closest Z delta.
	**/
	const double current_z = node.position.z;
	double best_delta = std::numeric_limits<double>::max();
	for ( int32_t li = 0; li < cell.num_layers; li++ ) {
		// Skip the current layer itself.
		if ( li == node.key.layer_index ) {
			continue;
		}
		const double layer_z = ( double )cell.layers[ li ].z_quantized * mesh->z_quant;
		const double dz = fabsf( ( float )( layer_z - current_z ) );
		if ( dz < best_delta ) {
			best_delta = dz;
		}
	}

	/**
	*    Output the closest delta when a candidate was found.
	**/
	if ( best_delta == std::numeric_limits<double>::max() ) {
		return false;
	}
	*out_delta = best_delta;
	return true;
}

/**
*  @brief	Forward declaration for the single-step traversal helper used by segmented ramps.
*  @param	mesh			Navigation mesh.
*  @param	startPos	Start world position.
*  @param	endPos		End world position.
*  @param	mins		Agent bounding box minimums.
*  @param	maxs		Agent bounding box maximums.
*  @param	clip_entity	Entity to use for clipping (nullptr for world).
*  @param	policy		Optional path policy for tuning step behavior.
*  @return	True if the traversal is possible, false otherwise.
**/
static const bool Nav_CanTraverseStep_ExplicitBBox_Single( const nav_mesh_t *mesh, const Vector3 &startPos, const Vector3 &endPos, const Vector3 &mins, const Vector3 &maxs, const edict_ptr_t *clip_entity, const svg_nav_path_policy_t *policy );

/**
*  @brief	Performs a simple PMove-like step traversal test (3-trace) with explicit agent bbox.
* 			This is intentionally conservative and is used only to validate edges in A*:
* 			1) Try direct horizontal move.
* 			2) If blocked, try stepping up <= max step, then horizontal move.
* 			3) Trace down to find ground.
*  @param	mesh			Navigation mesh.
*  @param	startPos		Start world position.
*  @param	endPos			End world position.
*  @param	mins			Agent bounding box minimums.
*  @param	maxs			Agent bounding box maximums.
*  @param	clip_entity		Entity to use for clipping (nullptr for world).
*  @param	policy			Optional path policy for tuning step behavior.
*  @return	True if the traversal is possible, false otherwise.
**/
const bool Nav_CanTraverseStep_ExplicitBBox( const nav_mesh_t *mesh, const Vector3 &startPos, const Vector3 &endPos, const Vector3 &mins, const Vector3 &maxs, const edict_ptr_t *clip_entity, const svg_nav_path_policy_t *policy ) {
	/**
	*    Sanity checks: require a valid mesh.
	**/
	if ( !mesh ) {
		return false;
	}

	/**
	*    Compute the required climb to determine if segmentation is needed.
	**/
	const double desiredDz = ( double )endPos[ 2 ] - ( double )startPos[ 2 ];
	const double requiredUp = std::max( 0.0, desiredDz );
	const double stepSize = ( policy ? ( double )policy->max_step_height : mesh->max_step );

	/**
	*    Drop cap enforcement (total edge):
	*        Ensure long downward transitions do not bypass the cap when we segment
	*        by horizontal distance for multi-cell ramps.
	**/
	const double dropCap = ( policy ? ( double )policy->drop_cap : ( nav_drop_cap ? nav_drop_cap->value : 128.0f ) );
	const double requiredDown = std::max( 0.0, ( double )startPos[ 2 ] - ( double )endPos[ 2 ] );
	// Reject edges that drop farther than the configured cap.
	if ( requiredDown > 0.0 && dropCap >= 0.0 && requiredDown > dropCap ) {
		return false;
	}

	/**
	*    Compute horizontal distance to decide if we should segment long, shallow ramps.
	*        Multi-cell ramps can fail a single step-test even with a small Z delta, so
	*        we segment by XY distance to validate each cell-scale portion.
	**/
	const Vector3 fullDelta = QM_Vector3Subtract( endPos, startPos );
	const double horizontalDist = sqrtf( ( fullDelta[ 0 ] * fullDelta[ 0 ] ) + ( fullDelta[ 1 ] * fullDelta[ 1 ] ) );
	const double cellSize = ( mesh->cell_size_xy > 0.0 ) ? mesh->cell_size_xy : 0.0;
	// Determine how many segments we need for vertical climb and for multi-cell ramps.
	const int32_t stepCountVertical = ( requiredUp > stepSize && stepSize > 0.0 )
		? ( int32_t )std::ceil( requiredUp / stepSize )
		: 1;
	const int32_t stepCountHorizontal = ( cellSize > 0.0 && horizontalDist > cellSize )
		? ( int32_t )std::ceil( horizontalDist / cellSize )
		: 1;
	// Use the most conservative segmentation to satisfy both climb and ramp constraints.
	const int32_t stepCount = std::max( stepCountVertical, stepCountHorizontal );

	/**
	*    Segmented ramp/stair handling:
	*        Split long climbs into <= max_step_height sub-steps so
	*        step validation mirrors PMove-style step constraints.
	**/
	if ( stepCount > 1 ) {
		/**
		*    Segment by the combined climb/XY requirement to validate multi-cell ramps
		*    in smaller chunks and avoid early step-test failures.
		**/
		// Track the starting point of the current segment.
		Vector3 segmentStart = startPos;
		// Validate each sub-step in sequence.
		for ( int32_t stepIndex = 1; stepIndex <= stepCount; stepIndex++ ) {
			// Compute the fractional progress for this segment.
			const double t = ( double )stepIndex / ( double )stepCount;
			// Build the segment end position along the climb.
			Vector3 segmentEnd = {
				startPos[ 0 ] + ( float )( fullDelta[ 0 ] * t ),
				startPos[ 1 ] + ( float )( fullDelta[ 1 ] * t ),
				startPos[ 2 ] + ( float )( fullDelta[ 2 ] * t )
			};

			/**
			*    Validate the current sub-step using the single-step routine.
			*    Abort immediately if any sub-step fails.
			**/
			if ( !Nav_CanTraverseStep_ExplicitBBox_Single( mesh, segmentStart, segmentEnd, mins, maxs, clip_entity, policy ) ) {
				return false;
			}

			// Advance to the next segment.
			segmentStart = segmentEnd;
		}

		// All sub-steps succeeded.
		return true;
	}

	/**
	*    Fallback: use the single-step validation when no segmentation is needed.
	**/
	return Nav_CanTraverseStep_ExplicitBBox_Single( mesh, startPos, endPos, mins, maxs, clip_entity, policy );
}

/**
*
*
*
*   Navigation Pathfinding:
*
*
*
**/
/**
*	@brief	Perform A* search between two navigation nodes.
*	@param	mesh			Navigation mesh containing the voxel tiles/cells.
*	@param	start_node	Start navigation node reference.
*	@param	goal_node	Goal navigation node reference.
*	@param	agent_mins	Agent bounding box mins (in nav-center space).
*	@param	agent_maxs	Agent bounding box maxs (in nav-center space).
*	@param	out_points	[out] Waypoints (node positions) from start to goal.
*	@param	policy		Optional per-agent traversal tuning (step/drop/jump constraints).
*	@param	pathProcess	Optional per-entity path process used for failure backoff penalties.
*	@param	tileRouteFilter	Optional hierarchical filter restricting expansion to a coarse tile route.
*	@return	True if a path was found, false otherwise.
*	@note	This search validates each candidate edge using `Nav_CanTraverseStep_ExplicitBBox`.
*			It is therefore safe to consider additional neighbor offsets (e.g., 2-cell hops)
*			because collision/step feasibility is still enforced.
**/
/**
*	@brief	Perform a traversal search while reusing the async helper state machine.
*	@note	The shared helper documents neighbor offsets, drop limits, and chunked expansion so both sync and async callers stay aligned.
**/
static bool Nav_AStarSearch( const nav_mesh_t *mesh, const nav_node_ref_t &start_node, const nav_node_ref_t &goal_node,
	const Vector3 &agent_mins, const Vector3 &agent_maxs, std::vector<Vector3> &out_points, const svg_nav_path_policy_t *policy = nullptr,
	const struct svg_nav_path_process_t *pathProcess = nullptr, const std::vector<nav_tile_cluster_key_t> *tileRouteFilter = nullptr ) {
	/**
	*\tPrepare the incremental A* state so we share the traversal rules with the async helpers.
	**/
	nav_a_star_state_t state = {};
	if ( !Nav_AStar_Init( &state, mesh, start_node, goal_node, agent_mins, agent_maxs, policy, tileRouteFilter, pathProcess ) ) {
		return false;
	}

 /**
	*    Synchronous rebuild budget:
	*        Disable per-step time limits so the helper can run to completion without per-step cap.
	**/
	state.search_budget_ms = 0;
	state.hit_time_budget = false;

	const int32_t expansionBudget = std::max( 1, SVG_Nav_GetAsyncRequestExpansions() );

	/**
	*    Step the helper repeatedly so it can honor its chunked expansion and time budgets.
	**/
	while ( state.status == nav_a_star_status_t::Running ) {
		Nav_AStar_Step( &state, expansionBudget );
 }

	/**
	*    Finalize the path when the search completes successfully.
	**/
	if ( state.status == nav_a_star_status_t::Completed ) {
		const bool ok = Nav_AStar_Finalize( &state, &out_points );
		Nav_AStar_Reset( &state );
		return ok;
	}

	/**
	*    Cache diagnostic toggle so we can gate extra logging below.
	**/
	const bool navDiag = Nav_PathDiagEnabled();

	gi.dprintf( "[WARNING][NavPath][A*] Pathfinding failed: No path found from (%.1f,%.1f,%.1f) to (%.1f,%.1f,%.1f)\n",
		start_node.position.x, start_node.position.y, start_node.position.z,
		goal_node.position.x, goal_node.position.y, goal_node.position.z );
	gi.dprintf( "[WARNING][NavPath][A*] Search stats: explored=%d nodes, max_nodes=%d, open_list_empty=%s\n",
		( int32_t )state.nodes.size(),
		state.max_nodes,
		state.open_list.empty() ? "true" : "false" );

	/**
	*    Emit detailed diagnostics only when explicitly requested.
	**/
	if ( navDiag ) {
		gi.dprintf( "[DEBUG][NavPath][Diag] A* summary: neighbor_tries=%d, no_node=%d, edge_reject=%d, tile_filter_reject=%d, stagnation=%d\n",
			state.neighbor_try_count, state.no_node_count, state.edge_reject_count, state.tile_filter_reject_count, state.stagnation_count );
		gi.dprintf( "[DEBUG][NavPath][Diag] A* inputs: max_step=%.1f max_drop=%.1f cell_size_xy=%.1f z_quant=%.1f\n",
			nav_max_step ? nav_max_step->value : -1.0f,
			nav_drop_cap ? nav_drop_cap->value : -1.0f,
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

		const std::vector<nav_tile_cluster_key_t> *routeFilter = state.tileRouteFilter;
		if ( routeFilter && !routeFilter->empty() ) {
			gi.dprintf( "[DEBUG][NavPath][Diag] cluster route enforced: %d tiles\n",
				( int32_t )routeFilter->size() );
		} else {
			gi.dprintf( "[DEBUG][NavPath][Diag] cluster route: (none)\n" );
		}
	}

 /**
	*    Print a concise failure reason based on the observed abort conditions.
	**/
	if ( state.hit_stagnation_limit ) {
		gi.dprintf( "[WARNING][NavPath][A*] Failure reason: Search failed due to excessive stagnation after %d iterations.\n", state.stagnation_count );
	} else if ( ( int32_t )state.nodes.size() >= state.max_nodes ) {
		gi.dprintf( "[WARNING][NavPath][A*] Failure reason: Search node limit reached (%d nodes). The navmesh may be too large or the goal is very far.\n", state.max_nodes );
	} else if ( state.open_list.empty() ) {
		gi.dprintf( "[WARNING][NavPath][A*] Failure reason: Open list exhausted. This typically means:\n" );
		gi.dprintf( "[WARNING][NavPath][A*]   1. No navmesh connectivity exists between start and goal Z-levels\n" );
		gi.dprintf( "[WARNING][NavPath][A*]   2. All potential paths are blocked by obstacles\n" );
		gi.dprintf( "[WARNING][NavPath][A*]   3. Edge validation (Nav_CanTraverseStep) rejected all connections\n" );
		gi.dprintf( "[WARNING][NavPath][A*] Suggestions:\n" );
		gi.dprintf( "[WARNING][NavPath][A*]   - Check nav_max_step (current: %.1f) and nav_drop_cap (current: %.1f)\n",
			nav_max_step ? nav_max_step->value : -1.0f,
			nav_drop_cap ? nav_drop_cap->value : -1.0f );
		gi.dprintf( "[WARNING][NavPath][A*]   - Verify navmesh has stairs/ramps connecting the Z-levels\n" );
		gi.dprintf( "[WARNING][NavPath][A*]   - Use 'nav_debug_draw 1' to visualize the navmesh\n" );
	} else {
		gi.dprintf( "[WARNING][NavPath][A*] Failure reason: Unknown (status=%d).\n", ( int32_t )state.status );
	}

	Nav_AStar_Reset( &state );
	return false;
}



//////////////////////////////////////////////////////////////////////////
/**
*
*
*
*   Navigation System "Traversal Operations":
*
*
*
**/

/**
*   @brief  Generate a traversal path between two world-space origins.
*           Uses the navigation voxelmesh and A* search to produce waypoints.
*   @param  start_origin    World-space starting origin.
*   @param  goal_origin     World-space destination origin.
*   @param  out_path        Output path result (caller must free).
*   @return True if a path was found, false otherwise.
**/
const bool SVG_Nav_GenerateTraversalPathForOrigin( const Vector3 &start_origin, const Vector3 &goal_origin, nav_traversal_path_t *out_path ) {
	if ( !out_path ) {
		return false;
	}

	SVG_Nav_FreeTraversalPath( out_path );

	if ( !g_nav_mesh ) {
		return false;
	}

	nav_node_ref_t start_node = {};
	nav_node_ref_t goal_node = {};

	if ( !Nav_FindNodeForPosition( g_nav_mesh.get(), start_origin, start_origin[ 2 ], &start_node, true ) ) {
		return false;
	}

	if ( !Nav_FindNodeForPosition( g_nav_mesh.get(), goal_origin, goal_origin[ 2 ], &goal_node, true ) ) {
		return false;
	}

	std::vector<Vector3> points;
	if ( start_node.key == goal_node.key ) {
		points.push_back( start_node.position );
		points.push_back( goal_node.position );
	} else {
		/**
		*	Hierarchical pre-pass: compute a coarse tile route and restrict A* expansions
		*	to tiles on that route.
		**/
		std::vector<nav_tile_cluster_key_t> tileRoute;
		const bool hasTileRoute = SVG_Nav_ClusterGraph_FindRoute( g_nav_mesh.get(), start_origin, goal_origin, tileRoute );
		const std::vector<nav_tile_cluster_key_t> *routeFilter = hasTileRoute ? &tileRoute : nullptr;

		if ( !Nav_AStarSearch( g_nav_mesh.get(), start_node, goal_node, g_nav_mesh->agent_mins, g_nav_mesh->agent_maxs, points, nullptr, nullptr, routeFilter ) ) {
			return false;
		}
	}

	if ( points.empty() ) {
		return false;
	}

	out_path->num_points = ( int32_t )points.size();
	out_path->points = ( Vector3 * )gi.TagMallocz( sizeof( Vector3 ) * out_path->num_points, TAG_SVGAME_LEVEL );
	memcpy( out_path->points, points.data(), sizeof( Vector3 ) * out_path->num_points );

	// Cache for always-on debug draw.
	NavDebug_PurgeCachedPaths();

	nav_debug_cached_path_t cached = {};
	cached.path.num_points = out_path->num_points;
	cached.path.points = ( Vector3 * )gi.TagMallocz( sizeof( Vector3 ) * out_path->num_points, TAG_SVGAME_LEVEL );
	memcpy( cached.path.points, out_path->points, sizeof( Vector3 ) * out_path->num_points );
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
			const Vector3 mid = { ( a[ 0 ] + b[ 0 ] ) * 0.5f, ( a[ 1 ] + b[ 1 ] ) * 0.5f, ( a[ 2 ] + b[ 2 ] ) * 0.5f };
			if ( !NavDebug_PassesDistanceFilter( mid ) ) {
				continue;
			}

			SVG_DebugDrawLine_TE( a, b, MULTICAST_PVS, false );
			NavDebug_ConsumeSegments( 1 );
		}
	}

	return true;
}

const bool SVG_Nav_GenerateTraversalPathForOriginEx( const Vector3 &start_origin, const Vector3 &goal_origin, nav_traversal_path_t *out_path,
	const bool enable_goal_z_layer_blend, const double blend_start_dist, const double blend_full_dist ) {
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
		if ( !Nav_FindNodeForPosition_BlendZ( g_nav_mesh.get(), start_origin, start_origin[ 2 ], goal_origin[ 2 ], start_origin, goal_origin,
			blend_start_dist, blend_full_dist, &start_node, true ) ) {
			return false;
		}
		// Use blend for goal selection as well to prefer the goal's Z layer when far away.
		if ( !Nav_FindNodeForPosition_BlendZ( g_nav_mesh.get(), goal_origin, start_origin[ 2 ], goal_origin[ 2 ], start_origin, goal_origin,
			blend_start_dist, blend_full_dist, &goal_node, true ) ) {
			return false;
		}
	} else {
		if ( !Nav_FindNodeForPosition( g_nav_mesh.get(), start_origin, start_origin[ 2 ], &start_node, true ) ) {
			return false;
		}
		if ( !Nav_FindNodeForPosition( g_nav_mesh.get(), goal_origin, goal_origin[ 2 ], &goal_node, true ) ) {
			return false;
		}
	}

	std::vector<Vector3> points;
	if ( start_node.key == goal_node.key ) {
		points.push_back( start_node.position );
		points.push_back( goal_node.position );
	} else {
		/**
		*	Hierarchical pre-pass: compute a coarse tile route and restrict A* expansions
		*	to tiles on that route.
		**/
		std::vector<nav_tile_cluster_key_t> tileRoute;
		const bool hasTileRoute = SVG_Nav_ClusterGraph_FindRoute( g_nav_mesh.get(), start_origin, goal_origin, tileRoute );
		const std::vector<nav_tile_cluster_key_t> *routeFilter = hasTileRoute ? &tileRoute : nullptr;

		if ( !Nav_AStarSearch( g_nav_mesh.get(), start_node, goal_node, g_nav_mesh->agent_mins, g_nav_mesh->agent_maxs, points, nullptr, nullptr, routeFilter ) ) {
			return false;
		}
	}

	if ( points.empty() ) {
		return false;
	}

	out_path->num_points = ( int32_t )points.size();
	out_path->points = ( Vector3 * )gi.TagMallocz( sizeof( Vector3 ) * out_path->num_points, TAG_SVGAME_LEVEL );
	memcpy( out_path->points, points.data(), sizeof( Vector3 ) * out_path->num_points );

	// Cache for always-on debug draw.
	NavDebug_PurgeCachedPaths();

	nav_debug_cached_path_t cached = {};
	cached.path.num_points = out_path->num_points;
	cached.path.points = ( Vector3 * )gi.TagMallocz( sizeof( Vector3 ) * out_path->num_points, TAG_SVGAME_LEVEL );
	memcpy( cached.path.points, out_path->points, sizeof( Vector3 ) * out_path->num_points );
	cached.expireTime = level.time + NAV_DEBUG_PATH_RETENTION;
	s_nav_debug_cached_paths.push_back( cached );

	if ( NavDebug_Enabled() && nav_debug_draw_path && nav_debug_draw_path->integer != 0 ) {
		const int32_t segmentCount = std::max( 0, out_path->num_points - 1 );
		for ( int32_t i = 0; i < segmentCount; i++ ) {
			if ( !NavDebug_CanEmitSegments( 1 ) ) {
				break;
			}

			const Vector3 &a = out_path->points[ i ];
			const Vector3 &b = out_path->points[ i + 1 ];

			const Vector3 mid = { ( a[ 0 ] + b[ 0 ] ) * 0.5f, ( a[ 1 ] + b[ 1 ] ) * 0.5f, ( a[ 2 ] + b[ 2 ] ) * 0.5f };
			if ( !NavDebug_PassesDistanceFilter( mid ) ) {
				continue;
			}

			SVG_DebugDrawLine_TE( a, b, MULTICAST_PVS, false );
			NavDebug_ConsumeSegments( 1 );
		}
	}

	return true;
}

const bool SVG_Nav_GenerateTraversalPathForOrigin_WithAgentBBox( const Vector3 &start_origin, const Vector3 &goal_origin, nav_traversal_path_t *out_path,
	const Vector3 &agent_mins, const Vector3 &agent_maxs, const svg_nav_path_policy_t *policy ) {
	if ( !out_path ) {
		return false;
	}

	SVG_Nav_FreeTraversalPath( out_path );

	if ( !g_nav_mesh ) {
		return false;
	}

	nav_node_ref_t start_node = {};
	nav_node_ref_t goal_node = {};

	if ( !Nav_FindNodeForPosition( g_nav_mesh.get(), start_origin, start_origin[ 2 ], &start_node, true ) ) {
		return false;
	}

	if ( !Nav_FindNodeForPosition( g_nav_mesh.get(), goal_origin, goal_origin[ 2 ], &goal_node, true ) ) {
		return false;
	}

	std::vector<Vector3> points;
	if ( start_node.key == goal_node.key ) {
		points.push_back( start_node.position );
		points.push_back( goal_node.position );
	} else {
		/**
		*	Hierarchical pre-pass: compute a coarse tile route and restrict A* expansions
		*	to tiles on that route.
		**/
		std::vector<nav_tile_cluster_key_t> tileRoute;
		const bool hasTileRoute = SVG_Nav_ClusterGraph_FindRoute( g_nav_mesh.get(), start_origin, goal_origin, tileRoute );
		const std::vector<nav_tile_cluster_key_t> *routeFilter = hasTileRoute ? &tileRoute : nullptr;

		if ( !Nav_AStarSearch( g_nav_mesh.get(), start_node, goal_node, agent_mins, agent_maxs, points, policy, nullptr, routeFilter ) ) {
			return false;
		}
	}

	if ( points.empty() ) {
		return false;
	}

	out_path->num_points = ( int32_t )points.size();
	out_path->points = ( Vector3 * )gi.TagMallocz( sizeof( Vector3 ) * out_path->num_points, TAG_SVGAME_LEVEL );
	memcpy( out_path->points, points.data(), sizeof( Vector3 ) * out_path->num_points );
	return true;
}

const bool SVG_Nav_GenerateTraversalPathForOriginEx_WithAgentBBox( const Vector3 &start_origin, const Vector3 &goal_origin, nav_traversal_path_t *out_path,
	const Vector3 &agent_mins, const Vector3 &agent_maxs, const svg_nav_path_policy_t *policy, const bool enable_goal_z_layer_blend, const double blend_start_dist, const double blend_full_dist,
	const struct svg_nav_path_process_t *pathProcess ) {
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
		startResolved = Nav_FindNodeForPosition_BlendZ( g_nav_mesh.get(), start_origin, start_origin[ 2 ], goal_origin[ 2 ], start_origin, goal_origin,
			blend_start_dist, blend_full_dist, &start_node, true );
		goalResolved = Nav_FindNodeForPosition_BlendZ( g_nav_mesh.get(), goal_origin, start_origin[ 2 ], goal_origin[ 2 ], start_origin, goal_origin,
			blend_start_dist, blend_full_dist, &goal_node, true );
	} else {
		startResolved = Nav_FindNodeForPosition( g_nav_mesh.get(), start_origin, start_origin[ 2 ], &start_node, true );
		goalResolved = Nav_FindNodeForPosition( g_nav_mesh.get(), goal_origin, goal_origin[ 2 ], &goal_node, true );
	}

	/**
	*    Targeted diagnostics: node resolution.
	*        This is the quickest way to separate the "no nodes" case from true A* failures.
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
	if ( start_node.key == goal_node.key ) {
		points.push_back( start_node.position );
		points.push_back( goal_node.position );
	} else {
		/**
		*	Hierarchical pre-pass: compute a coarse tile route and restrict A* expansions
		*	to tiles on that route.
		*
		*	We still pass `pathProcess` through so the fine A* can apply per-entity
		*	failure penalties.
		**/
		std::vector<nav_tile_cluster_key_t> tileRoute;
		const bool hasTileRoute = SVG_Nav_ClusterGraph_FindRoute( g_nav_mesh.get(), start_origin, goal_origin, tileRoute );
		const std::vector<nav_tile_cluster_key_t> *routeFilter = hasTileRoute ? &tileRoute : nullptr;

		if ( !Nav_AStarSearch( g_nav_mesh.get(), start_node, goal_node, agent_mins, agent_maxs, points, policy, pathProcess, routeFilter ) ) {
			return false;
		}
	}

	if ( points.empty() ) {
		return false;
	}

	out_path->num_points = ( int32_t )points.size();
	out_path->points = ( Vector3 * )gi.TagMallocz( sizeof( Vector3 ) * out_path->num_points, TAG_SVGAME_LEVEL );
	memcpy( out_path->points, points.data(), sizeof( Vector3 ) * out_path->num_points );
	return true;
}

/**
*   @brief  Free a traversal path allocated by SVG_Nav_GenerateTraversalPathForOrigin.
*   @param  path    Path structure to free.
**/
void SVG_Nav_FreeTraversalPath( nav_traversal_path_t *path ) {
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
*   @brief  Query movement direction along a traversal path.
*           Advances the waypoint index as the caller reaches waypoints.
*   @param  path            Path to follow.
*   @param  current_origin  Current world-space origin.
*   @param  waypoint_radius Radius for waypoint completion.
*   @param  policy          Optional traversal policy for per-agent constraints.
*   @param  inout_index     Current waypoint index (updated on success).
*   @param  out_direction   Output normalized movement direction.
*   @return True if a valid direction was produced, false if path is complete/invalid.
**/
const bool SVG_Nav_QueryMovementDirection( const nav_traversal_path_t *path, const Vector3 &current_origin,
	double waypoint_radius, const svg_nav_path_policy_t *policy, int32_t *inout_index, Vector3 *out_direction ) {
/**
*	Sanity checks: validate inputs and ensure we have a usable path.
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

	int32_t index = *inout_index;
	if ( index < 0 ) {
		index = 0;
	}

	const double waypoint_radius_sqr = waypoint_radius * waypoint_radius;

	// Advance waypoints using 3D distance so stairs count toward completion.
	while ( index < path->num_points ) {
		const Vector3 delta = QM_Vector3Subtract( path->points[ index ], current_origin );
		const double dist_sqr = ( delta[ 0 ] * delta[ 0 ] ) + ( delta[ 1 ] * delta[ 1 ] ) + ( delta[ 2 ] * delta[ 2 ] );
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
			s_last_dist2d_sqr = ( d0[ 0 ] * d0[ 0 ] ) + ( d0[ 1 ] * d0[ 1 ] );
			s_no_progress_frames = 0;
		} else {
			const Vector3 d1 = QM_Vector3Subtract( path->points[ index ], current_origin );
			const double dist2d_sqr = ( d1[ 0 ] * d1[ 0 ] ) + ( d1[ 1 ] * d1[ 1 ] );
			const double near_sqr = waypoint_radius_sqr * 9.0f;
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
*	Navigation Movement Tests:
*
*
*
**/
/**
*	@brief	Low-level trace wrapper that supports world or inline-model clip.
**/
const inline cm_trace_t Nav_Trace( const Vector3 &start, const Vector3 &mins, const Vector3 &maxs, const Vector3 &end,
	const edict_ptr_t *clip_entity, const cm_contents_t mask ) {
	if ( clip_entity ) {
		return gi.clip( clip_entity, &start, &mins, &maxs, &end, mask );
	}

	return gi.trace( &start, &mins, &maxs, &end, nullptr, mask );
}

/**
*	@brief	Low-level trace wrapper that supports world or inline-model clip.
**/
/**
*	@brief	Performs a simple PMove-like step traversal test (3-trace).
*
*			This is intentionally conservative and is used only to validate edges in A*:
*			1) Try direct horizontal move.
*			2) If blocked, try stepping up <= max step, then horizontal move.
*			3) Trace down to find ground.
*	@param	mesh			Navigation mesh.
* 	@param	startPos		Start world position.
* 	@param	endPos			End world position.
* 	@param	clip_entity		Entity to use for clipping (nullptr for world).
*
*	@return	True if the traversal is possible, false otherwise.
**/
const bool Nav_CanTraverseStep( const nav_mesh_t *mesh, const Vector3 &startPos, const Vector3 &endPos, const edict_ptr_t *clip_entity, svg_nav_path_policy_t &policy ) {
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
*  @brief	Performs a simple PMove-like step traversal test (3-trace) with explicit agent bbox.
* 			This is intentionally conservative and is used only to validate edges in A*:
* 			1) Try direct horizontal move.
* 			2) If blocked, try stepping up <= max step, then horizontal move.
* 			3) Trace down to find ground.
* 	@param	mesh			Navigation mesh.
* 	@param	startPos		Start world position.
* 	@param	endPos			End world position.
* 	@param	mins			Agent bounding box minimums.
* 	@param	maxs			Agent bounding box maximums.
* 	@param	clip_entity		Entity to use for clipping (nullptr for world).
* 	@param	policy			Optional path policy for tuning step behavior.
* 	@return	True if the traversal is possible, false otherwise.
**/
/**
*  @brief	Internal single-step traversal validation used by the segmented ramp helper.
*  @param	mesh			Navigation mesh.
*  @param	startPos	Start world position.
*  @param	endPos		End world position.
*  @param	mins		Agent bounding box minimums.
*  @param	maxs		Agent bounding box maximums.
*  @param	clip_entity	Entity to use for clipping (nullptr for world).
*  @param	policy		Optional path policy for tuning step behavior.
*  @return	True if the traversal is possible, false otherwise.
*  @note	This function assumes any segmented-ramp handling has already been applied.
**/
static const bool Nav_CanTraverseStep_ExplicitBBox_Single( const nav_mesh_t *mesh, const Vector3 &startPos, const Vector3 &endPos, const Vector3 &mins, const Vector3 &maxs, const edict_ptr_t *clip_entity, const svg_nav_path_policy_t *policy ) {
	if ( !mesh ) {
		return false;
	}

	//! Tracks the last frame we reported a step-test failure.
	static int32_t s_nav_step_test_failure_frame = -1;
	//! Indicates whether we already printed a failure reason this frame.
	static bool s_nav_step_test_failure_logged = false;
	auto ReportStepTestFailureOnce = [ startPos, endPos ]( const char *reason ) {
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
			reason );
	};


	/**
	*	Set up a PMove-like, conservative step test for A* edge validation.
	*		We must respect the caller-provided Z delta between nodes because the navmesh
	*		nodes already encode vertical transitions (stairs/ramps/steps). We only flatten
	*		Z for the horizontal trace portions, but we still compute the allowed climb/drop
	*		based on the desired endpoint Z.
	**/
	// Keep the original Z delta between nodes.
	const double desiredDz = ( double )endPos[ 2 ] - ( double )startPos[ 2 ];

	// Small cushion to avoid precision issues on floors.
	const double eps = 0.25;

	// Determine step size: prefer policy override if provided, otherwise use mesh parameter.
	const double stepSize = ( policy ? ( double )policy->max_step_height : mesh->max_step );
	const double minStep = ( policy ? ( double )policy->min_step_height : eps );

	/**
	*    Drop cap enforcement: respect the caller's policy before attempting any traces
	*    so we can early-out on overly deep downward transitions.
	**/
	const double dropCap = ( policy ? ( double )policy->drop_cap : ( nav_drop_cap ? nav_drop_cap->value : 128.0f ) );
	const double requiredDown = std::max( 0.0, ( double )startPos[ 2 ] - ( double )endPos[ 2 ] );
	/**
	*    Reject edges that attempt to drop farther than the configured cap before tracing.
	**/
	if ( requiredDown > 0.0 && dropCap >= 0.0 && requiredDown > dropCap ) {
		NavDebug_RecordReject( startPos, endPos, NAV_DEBUG_REJECT_REASON_DROP_CAP );
		ReportStepTestFailureOnce( "drop cap exceeded before tracing" );
		return false;
	}

	/**
	*    We must know how much upward travel is required before continuing.
	**/
	const double requiredUp = std::max( 0.0, desiredDz );

	// If the required climb is larger than our allowed step height, this edge is infeasible.
	if ( requiredUp > stepSize ) {
		ReportStepTestFailureOnce( "required climb exceeds configured step" );
		return false;
	}

	// If policy wants to enforce a minimum step height, only apply it to actual "step up" edges.
	if ( requiredUp > 0.0 && requiredUp < minStep ) {
		ReportStepTestFailureOnce( "step height below minimum" );
		return false;
	}

	// Flatten Z for horizontal movement tests.
	Vector3 start = startPos;
	Vector3 goal = endPos;
	goal[ 2 ] = start[ 2 ];

	// 1) Direct horizontal move.
	{
		cm_trace_t tr = Nav_Trace( start, mins, maxs, goal, clip_entity, CM_CONTENTMASK_SOLID );
		if ( tr.fraction >= 1.0f && !tr.allsolid && !tr.startsolid ) {
			return true;
		}
	}

	/**
	*	Step-up phase:
	*		Only attempt the step-up if this edge actually needs to climb.
	*		For flat/down edges we keep the start Z and just do a down-trace later.
	**/
	Vector3 steppedStart = start;
	if ( requiredUp > 0.0 ) {
		// Step up by the required amount (plus a tiny cushion).
		Vector3 up = start;
		up[ 2 ] += ( requiredUp + eps );

		cm_trace_t upTr = Nav_Trace( start, mins, maxs, up, clip_entity, CM_CONTENTMASK_SOLID );
		if ( upTr.allsolid || upTr.startsolid ) {
			ReportStepTestFailureOnce( "step-up trace blocked" );
			return false;
		}

				// Ensure we actually achieved (at least) the required step height.
		const double actualUp = upTr.endpos[ 2 ] - start[ 2 ];
		if ( actualUp + eps < requiredUp ) {
			NavDebug_RecordReject( start, upTr.endpos, NAV_DEBUG_REJECT_REASON_CLEARANCE );
			ReportStepTestFailureOnce( "insufficient clearance" );
			return false;
		}

		steppedStart = upTr.endpos;
	}

	/**
	*	Horizontal move from stepped-up position.
	**/
	Vector3 steppedGoal = goal;
	steppedGoal[ 2 ] = steppedStart[ 2 ];

	cm_trace_t fwdTr = Nav_Trace( steppedStart, mins, maxs, steppedGoal, clip_entity, CM_CONTENTMASK_SOLID );
	if ( fwdTr.allsolid || fwdTr.startsolid || fwdTr.fraction < 1.0f ) {
		// If allowed, attempt a small obstruction 'jump' to climb a low obstacle and try again.
		if ( policy && policy->allow_small_obstruction_jump ) {
			// Try a small jump up to the configured obstruction height and then forward.
			Vector3 jumpUp = start;
			jumpUp[ 2 ] += ( double )policy->max_obstruction_jump_height;
			cm_trace_t jumpUpTr = Nav_Trace( start, mins, maxs, jumpUp, clip_entity, CM_CONTENTMASK_SOLID );
			if ( !jumpUpTr.allsolid && !jumpUpTr.startsolid ) {
				Vector3 jumpStart = jumpUpTr.endpos;
				Vector3 jumpGoal = goal;
				jumpGoal[ 2 ] = jumpStart[ 2 ];
				cm_trace_t jumpFwdTr = Nav_Trace( jumpStart, mins, maxs, jumpGoal, clip_entity, CM_CONTENTMASK_SOLID );
				if ( !jumpFwdTr.allsolid && !jumpFwdTr.startsolid && jumpFwdTr.fraction >= 1.0f ) {
					// Down-trace from landing position to ensure there's ground and slope is acceptable.
					Vector3 down = jumpFwdTr.endpos;
					Vector3 downEnd = down;
					downEnd[ 2 ] -= ( std::max( requiredUp, 0.0 ) + eps );
					cm_trace_t downTr2 = Nav_Trace( down, mins, maxs, downEnd, clip_entity, CM_CONTENTMASK_SOLID );
					if ( !downTr2.allsolid && !downTr2.startsolid && downTr2.fraction < 1.0f ) {
						// Check normal Z threshold using policy's step-normal minimum when available.
						const double minStepNormal = policy
							? ( double )policy->min_step_normal
							: cos( ( double )mesh->max_slope_deg * ( double )DEG_TO_RAD );
						if ( downTr2.plane.normal[ 2 ] <= 0.0f || downTr2.plane.normal[ 2 ] < minStepNormal ) {
							NavDebug_RecordReject( jumpFwdTr.endpos, downTr2.endpos, NAV_DEBUG_REJECT_REASON_SLOPE );
							return false;
						}
						// Optionally cap drop height.
						if ( policy && policy->cap_drop_height ) {
							const double drop = start[ 2 ] - downTr2.endpos[ 2 ];
							if ( drop > ( double )policy->max_drop_height ) {
								NavDebug_RecordReject( start, downTr2.endpos, NAV_DEBUG_REJECT_REASON_DROP_CAP );
								return false;
							}
						}
						return true;
					}
				}

			}
		}
	}

	/**
	*	Step down to find supporting ground.
	*		Trace down far enough to re-acquire the ground after a step/flat/down edge.
	*		When climbing, drop the probe by the amount we stepped up; otherwise still probe
	*		by a small amount to ensure we're not floating.
	**/
	Vector3 down = fwdTr.endpos;
	Vector3 downStart = down;
	// Step slightly above the surface to ensure the downward probe doesn't begin inside the floor hull.
	downStart[ 2 ] += eps;
	Vector3 downEnd = downStart;
	/**
	*    Compute the downward probe distance:
	*        - Uphill edges need to drop at least the climb amount to re-acquire ground.
	*        - Downhill edges must drop by the required descent to detect ramps/slopes.
	*        - Always include a small epsilon to avoid precision misses.
	**/
	const double probeDrop = std::max( std::max( requiredUp, requiredDown ), eps ) + eps;
	// Extend the down trace to the computed probe depth.
	downEnd[ 2 ] -= probeDrop;

	cm_trace_t downTr = Nav_Trace( downStart, mins, maxs, downEnd, clip_entity, CM_CONTENTMASK_SOLID );
	if ( downTr.allsolid || downTr.startsolid ) {
		ReportStepTestFailureOnce( "down trace blocked" );
		return false;
	}

	// Must land on something (not floating).
	if ( downTr.fraction >= 1.0f ) {
		ReportStepTestFailureOnce( "floating after step" );
		return false;
	}

	// Walkable-ish contact: ensure the surface normal respects the policy's step-normal threshold.
	if ( downTr.plane.normal[ 2 ] <= 0.0f ) {
		ReportStepTestFailureOnce( "non-positive ground normal" );
		return false;
	}
	const double minStepNormal = policy
		? ( double )policy->min_step_normal
		: cosf( ( float )mesh->max_slope_deg * ( float )DEG_TO_RAD );
	if ( downTr.plane.normal[ 2 ] < minStepNormal ) {
		ReportStepTestFailureOnce( "ground slope too steep" );
		return false;
	}

	// Drop cap after step
	if ( policy && policy->cap_drop_height ) {
		const double drop = start[ 2 ] - downTr.endpos[ 2 ];
		if ( drop > ( double )policy->max_drop_height ) {
			NavDebug_RecordReject( start, downTr.endpos, NAV_DEBUG_REJECT_REASON_DROP_CAP );
			ReportStepTestFailureOnce( "drop cap exceeded after step" );
			return false;
		}
	}

	return true;
}