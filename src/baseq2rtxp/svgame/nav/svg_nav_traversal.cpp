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
#include "svgame/nav/svg_nav_traversal.h"

#include "svgame/entities/svg_player_edict.h"

// <Q2RTXP>: TODO: Move to shared? Ehh.. 
// Common BSP access for navigation generation.
#include "common/bsp.h"
#include "common/files.h"

extern cvar_t *nav_max_step;
extern cvar_t *nav_max_drop;
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
static bool Nav_AStarSearch( const nav_mesh_t *mesh, const nav_node_ref_t &start_node, const nav_node_ref_t &goal_node,
	const Vector3 &agent_mins, const Vector3 &agent_maxs, std::vector<Vector3> &out_points, const svg_nav_path_policy_t *policy = nullptr,
	const struct svg_nav_path_process_t *pathProcess = nullptr, const std::vector<nav_tile_cluster_key_t> *tileRouteFilter = nullptr ) {
	constexpr int32_t MAX_SEARCH_NODES = 8192;
	/**
	*	Neighbor expansion offsets in grid-cell units.
	*
	*	We include both 1-cell and 2-cell offsets:
	*		- 1-cell offsets cover normal adjacency.
	*		- 2-cell offsets help route around single-voxel-thick obstructions where
	*		  the immediate neighbor cell may be non-walkable/absent.
	*
	*	Edge feasibility is still enforced by `Nav_CanTraverseStep_ExplicitBBox`, so
	*	adding these candidates does not allow walking through walls.
	**/
	static constexpr Vector3 neighbor_offsets[ ] = {
		{ 1.0f, 0.0f, 0.0f },
		{ -1.0f, 0.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f },
		{ 0.0f, -1.0f, 0.0f },
		{ 1.0f, 1.0f, 0.0f },
		{ 1.0f, -1.0f, 0.0f },
		{ -1.0f, 1.0f, 0.0f },
		{ -1.0f, -1.0f, 0.0f },

		{ 2.0f, 0.0f, 0.0f },
		{ -2.0f, 0.0f, 0.0f },
		{ 0.0f, 2.0f, 0.0f },
		{ 0.0f, -2.0f, 0.0f },
		{ 2.0f, 2.0f, 0.0f },
		{ 2.0f, -2.0f, 0.0f },
		{ -2.0f, 2.0f, 0.0f },
		{ -2.0f, -2.0f, 0.0f }
	};

	/**
	*    Simple Euclidean heuristic. Kept local for ease of modification.
	**/
	auto heuristic = []( const Vector3 &a, const Vector3 &b ) -> double {
		const Vector3 delta = QM_Vector3Subtract( b, a );
		return sqrtf( ( delta[ 0 ] * delta[ 0 ] ) + ( delta[ 1 ] * delta[ 1 ] ) + ( delta[ 2 ] * delta[ 2 ] ) );
		};

	std::vector<nav_search_node_t> nodes;
	nodes.reserve( 256 );

	std::vector<int32_t> open_list;
	open_list.reserve( 256 );

	std::unordered_map<nav_node_key_t, int32_t, nav_node_key_hash_t> node_lookup;
	node_lookup.reserve( 256 );

	nav_search_node_t start_search = {
		.node = start_node,
		.g_cost = 0.0f,
		.f_cost = heuristic( start_node.position, goal_node.position ),
		.parent_index = -1,
		.closed = false
	};

	nodes.push_back( start_search );
	open_list.push_back( 0 );
	node_lookup.emplace( start_node.key, 0 );

	/**
	*    Diagnostics counters:
	*        These help classify why expansion fails without dumping per-edge spam.
	*        - noNodeCount: neighbor positions had no resolvable nav node.
	*        - tileFilterRejectCount: hierarchical tile route filter rejected neighbor.
	*        - edgeRejectCount: node existed but step/edge validation rejected traversal.
	**/
	int32_t noNodeCount = 0;
	int32_t tileFilterRejectCount = 0;
	int32_t edgeRejectCount = 0;
	int32_t neighborTryCount = 0;

	while ( !open_list.empty() && ( int32_t )nodes.size() < MAX_SEARCH_NODES ) {
		int32_t best_open_index = 0;
		double best_f_cost = nodes[ open_list[ 0 ] ].f_cost;

		for ( int32_t i = 1; i < ( int32_t )open_list.size(); i++ ) {
			const double f_cost = nodes[ open_list[ i ] ].f_cost;
			if ( f_cost < best_f_cost ) {
				best_f_cost = f_cost;
				best_open_index = i;
			}
		}

		const int32_t current_index = open_list[ best_open_index ];
		open_list[ best_open_index ] = open_list.back();
		open_list.pop_back();

		nav_search_node_t &current = nodes[ current_index ];
		current.closed = true;

		if ( current.node.key == goal_node.key ) {
			std::vector<Vector3> reversed_points;
			reversed_points.reserve( 64 );

			int32_t trace_index = current_index;
			while ( trace_index >= 0 ) {
				reversed_points.push_back( nodes[ trace_index ].node.position );
				trace_index = nodes[ trace_index ].parent_index;
			}

			out_points.assign( reversed_points.rbegin(), reversed_points.rend() );
			return true;
		}

		for ( const Vector3 &offset_dir : neighbor_offsets ) {
			neighborTryCount++;
			/**
			*	Compute neighbor query position.
			*		Neighbor offsets are expressed in grid-cell units.
			*		- X/Y use `cell_size_xy`.
			*		- Z uses `z_quant` (layer step).
			*
			*	Note:
			*		Previously we scaled the full 3D offset by `cell_size_xy`, which made any
			*		future vertical neighbor offsets incorrect and (more importantly) made it too
			*		easy to accidentally query Z positions that don't correspond to quantized nav
			*		layers. Using the correct axis scales increases the chance `Nav_FindNodeForPosition`
			*		resolves adjacent nodes, especially on stairs/ramps.
			**/
			Vector3 scaledOffset = offset_dir;
			scaledOffset[ 0 ] *= (float)mesh->cell_size_xy;
			scaledOffset[ 1 ] *= (float)mesh->cell_size_xy;
			scaledOffset[ 2 ] *= (float)mesh->z_quant;
			const Vector3 neighbor_origin = QM_Vector3Add( current.node.position, scaledOffset );

			/**
			*	Optional hierarchical filter: only expand nodes whose XY position falls on
			*	tiles included in the precomputed tile route.
			**/
			if ( tileRouteFilter && !tileRouteFilter->empty() ) {
				const nav_tile_cluster_key_t nk = SVG_Nav_GetTileKeyForPosition( mesh, neighbor_origin );
				bool allowed = false;
				for ( const nav_tile_cluster_key_t &k : *tileRouteFilter ) {
					if ( k == nk ) {
						allowed = true;
						break;
					}
				}
				if ( !allowed ) {
					tileFilterRejectCount++;
					continue;
				}
			}

			nav_node_ref_t neighbor_node = {};
			// Neighbor lookups must be robust across BSP leaf seams and sparse leaf->tile mappings.
			// We still validate physical feasibility with `Nav_CanTraverseStep_ExplicitBBox`, so
			// enabling fallback here does not allow walking through walls; it only prevents
			// premature A* expansion failure due to lookup bookkeeping.
			if ( !Nav_FindNodeForPosition( mesh, neighbor_origin, current.node.position[ 2 ], &neighbor_node, true ) ) {
				noNodeCount++;
				continue;
			}
			// Validate edge traversal using the PMove-like step test. Pass the policy through
			// so traversal checks can consult step/drop/jump thresholds when available.
			if ( !Nav_CanTraverseStep_ExplicitBBox( mesh, current.node.position, neighbor_node.position, agent_mins, agent_maxs, nullptr, policy ) ) {
				edgeRejectCount++;
				continue;
			}

			// Base distance cost (Euclidean between nodes).
			const double baseDist = heuristic( current.node.position, neighbor_node.position );

			/**
			*	Read runtime cost-tuning CVars.
			*		These weights shape A* scoring but do not affect edge feasibility.
			**/
			const double w_dist = nav_cost_w_dist ? nav_cost_w_dist->value : 1.0f;
			const double jumpBase = nav_cost_jump_base ? nav_cost_jump_base->value : 8.0f;
			const double jumpHeightWeight = nav_cost_jump_height_weight ? nav_cost_jump_height_weight->value : 2.0f;
			const double losWeight = nav_cost_los_weight ? nav_cost_los_weight->value : 1.0f;
			const double dynamicWeight = nav_cost_dynamic_weight ? nav_cost_dynamic_weight->value : 1.5f;
			const double failureWeight = nav_cost_failure_weight ? nav_cost_failure_weight->value : 10.0f;
			const double failureTauMs = nav_cost_failure_tau_ms ? nav_cost_failure_tau_ms->value : 5000.0f;
			const double turnWeight = nav_cost_turn_weight ? nav_cost_turn_weight->value : 0.8f;
			const double slopeWeight = nav_cost_slope_weight ? nav_cost_slope_weight->value : 0.5f;
			const double dropWeight = nav_cost_drop_weight ? nav_cost_drop_weight->value : 4.0f;
			/**
			*	Reserved: goal Z blend factor.
			*		This tuning value currently influences layer selection during start/goal node lookup
			*		(via BlendZ). We keep the cvar visible here to make it obvious that it is not part of
			*		per-edge scoring yet.
			**/
			( void )nav_cost_goal_z_blend_factor;
			const double minCostPerUnit = nav_cost_min_cost_per_unit ? nav_cost_min_cost_per_unit->value : 1.0f;

			// Start composing extra penalties/bonuses beyond base distance.
			double extraCost = 0.0f;

			// Inspect neighbor layer flags for content-based penalties (water/lava/etc).
			const nav_layer_t *neighborLayer = nullptr;
			if ( neighbor_node.key.leaf_index >= 0 && neighbor_node.key.leaf_index < mesh->num_leafs ) {
				if ( neighbor_node.key.tile_index >= 0 && neighbor_node.key.tile_index < ( int32_t )mesh->world_tiles.size() ) {
					const nav_tile_t *tile = &mesh->world_tiles[ neighbor_node.key.tile_index ];
					const nav_xy_cell_t *cell = &tile->cells[ neighbor_node.key.cell_index ];
					if ( neighbor_node.key.layer_index >= 0 && neighbor_node.key.layer_index < cell->num_layers ) {
						neighborLayer = &cell->layers[ neighbor_node.key.layer_index ];
					}
				}
			}

			// Content penalties
			if ( neighborLayer ) {
				if ( ( neighborLayer->flags & NAV_FLAG_WATER ) != 0 ) {
					extraCost += 1.0f; // mild penalty for water
				}
				if ( ( neighborLayer->flags & NAV_FLAG_LAVA ) != 0 ) {
					extraCost += 100.0f; // strong penalty for lava
				}
				if ( ( neighborLayer->flags & NAV_FLAG_SLIME ) != 0 ) {
					extraCost += 4.0f;
				}
			}

			// Vertical change -> slope/step/drop costs
			const double dz = neighbor_node.position.z - current.node.position.z;
			const double horizontal = sqrtf( ( neighbor_node.position.x - current.node.position.x ) * ( neighbor_node.position.x - current.node.position.x ) +
				( neighbor_node.position.y - current.node.position.y ) * ( neighbor_node.position.y - current.node.position.y ) );
			const double slope = ( horizontal > 0.0001f ) ? ( fabsf( dz ) / horizontal ) : 0.0f;
			// Quadratic slope penalty
			extraCost += slopeWeight * slope * slope * baseDist;

			// Step up cost (positive dz)
			if ( dz > 0.0f ) {
				const double stepLimit = ( policy ? ( double )policy->max_step_height : mesh->max_step );
				if ( dz > stepLimit ) {
					// This is larger than a normal step; scale as climb cost.
					extraCost += ( dz / std::max( stepLimit, 1.0 ) ) * 2.0;
				}
			} else {
				// Drop cost: penalize large drops to prefer safer routes.
				const double drop = -dz;
				const double maxDrop = ( policy ? ( double )policy->max_drop_height : ( nav_max_drop ? nav_max_drop->value : 128.0f ) );
				if ( drop > 0.0f ) {
					extraCost += dropWeight * ( drop / std::max( maxDrop, 1.0 ) );
				}
			}

			// Small-obstruction jump cost: if climb required beyond normal step but within obstruction jump limit.
			if ( policy && policy->allow_small_obstruction_jump ) {
				const double hj = std::max( 0.0, dz - ( double )policy->max_step_height );
				if ( hj > 0.0f ) {
					// Disallow if beyond allowed obstruction jump height (should be filtered by Nav_CanTraverseStep already).
					if ( hj > ( double )policy->max_obstruction_jump_height ) {
						continue; // infeasible neighbor
					}
					const double jumpCost = jumpBase + jumpHeightWeight * ( hj / ( double )policy->max_obstruction_jump_height );
					extraCost += jumpCost;
				}
			}

			// Line-of-sight bonus/penalty relative to goal: prefer nodes that keep LOS to goal.
			bool hasLOS = false;
			{
				// Trace from neighbor position to goal node position to determine visibility.
				cm_trace_t losTr = gi.trace( &neighbor_node.position, &mesh->agent_mins, &mesh->agent_maxs, &goal_node.position, nullptr, CM_CONTENTMASK_SOLID );
				if ( losTr.fraction >= 1.0f && !losTr.startsolid && !losTr.allsolid ) {
					hasLOS = true;
				}
			}
			if ( hasLOS ) {
				extraCost -= losWeight; // prefer LOS
			}

			// Dynamic occupancy: hard blocks skip expansion, soft cost biases against crowded cells.
			if ( SVG_Nav_Occupancy_Blocked( mesh, neighbor_node.key.tile_index, neighbor_node.key.cell_index, neighbor_node.key.layer_index ) ) {
				continue;
			}
			const int32_t occSoftCost = SVG_Nav_Occupancy_SoftCost( mesh, neighbor_node.key.tile_index, neighbor_node.key.cell_index, neighbor_node.key.layer_index );
			if ( occSoftCost > 0 ) {
				extraCost += dynamicWeight * ( double )occSoftCost;
			}

			// Failure/backoff penalty: if the caller provided a pathProcess we
			// apply a time-decaying penalty for recent failures from that entity.
			if ( pathProcess ) {
				const QMTime now = level.time;
				const QMTime lastFail = pathProcess->last_failure_time;
				if ( lastFail > 0_ms ) {
					const double tauMs = failureTauMs > 0.0f ? failureTauMs : 5000.0f;
					const double dt = ( double )( ( now - lastFail ).Milliseconds() );
					const double factor = exp( -dt / tauMs );
					// For debug builds print the computed failure penalty contribution.
					if ( nav_debug_draw && nav_debug_draw->integer != 0 ) {
						gi.dprintf( "[DEBUG][NavPath][A*] Applying failure penalty factor=%.3f -> penalty=%.3f (dt=%.1f ms)\n", factor, failureWeight * factor, dt );
					}
					extraCost += failureWeight * ( double )factor;

					// Additional: if neighbor is near the last failing goal position or matches failing yaw,
					// apply a stronger penalty to avoid repeating the same failing approach.
					const Vector3 toLastFail = QM_Vector3Subtract( neighbor_node.position, pathProcess->last_failure_pos );
					const double distToFail = QM_Vector3LengthDP( toLastFail );
					const double failPosRadius = 64.0f; // tunable radius for signature matching
					if ( distToFail <= failPosRadius ) {
						// Apply multiplicative penalty based on proximity.
						const double posFactor = 1.0f - ( distToFail / failPosRadius );
						const double sigPenalty = failureWeight * ( double )( 0.75f * posFactor );
						extraCost += sigPenalty;
						if ( nav_debug_draw && nav_debug_draw->integer != 0 ) {
							gi.dprintf( "[DEBUG][NavPath][A*] Applying signature position penalty=%.3f (dist=%.1f)", sigPenalty, distToFail );
						}
					}
					// Yaw matching: prefer to penalize directions that approach from similar yaw.
					const double neighborYaw = QM_Vector3ToYaw( QM_Vector3Subtract( neighbor_node.position, current.node.position ) );
					double yawDelta = fabsf( neighborYaw - pathProcess->last_failure_yaw );
					yawDelta = fmodf( yawDelta + 180.0f, 360.0f ) - 180.0f; // normalize to [-180,180]
					const double yawThresh = 45.0f; // degrees
					if ( fabsf( yawDelta ) <= yawThresh ) {
						const double yawFactor = 1.0f - ( fabsf( yawDelta ) / yawThresh );
						const double yawPenalty = failureWeight * ( double )( 0.5f * yawFactor );
						extraCost += yawPenalty;
						if ( nav_debug_draw && nav_debug_draw->integer != 0 ) {
							gi.dprintf( "[DEBUG][NavPath][A*] Applying signature yaw penalty=%.3f (yawDelta=%.1f)\n", yawPenalty, yawDelta );
						}
					}
				}
			} else if ( policy ) {
				// Fallback: small static penalty based on policy tuning.
				extraCost += failureWeight * ( double )policy->fail_backoff_max_pow * 0.01f;
			}

			// Turning cost: prefer straighter routes. If current has a parent, compute turning angle.
			if ( current.parent_index >= 0 ) {
				const nav_search_node_t &parentNode = nodes[ current.parent_index ];
				Vector3 fromDir = QM_Vector3NormalizeDP( QM_Vector3Subtract( current.node.position, parentNode.node.position ) );
				Vector3 toDir = QM_Vector3NormalizeDP( QM_Vector3Subtract( neighbor_node.position, current.node.position ) );
				const double dot = QM_Vector3DotProductDP( fromDir, toDir );
				const double clamped = QM_Clamp( dot, -1.0, 1.0 );
				const double ang = acosf( clamped );
				extraCost += turnWeight * ( ang / ( double )M_PI );
			}

			// For debug: optionally print per-edge cost breakdown when nav_debug_draw is enabled.
			if ( nav_debug_draw && nav_debug_draw->integer != 0 ) {
				gi.dprintf( "[DEBUG][NavPath][A*] edge from (%.1f %.1f %.1f) to (%.1f %.1f %.1f): base=%.3f extras=%.3f total_step=%.3f\n",
					current.node.position.x, current.node.position.y, current.node.position.z,
					neighbor_node.position.x, neighbor_node.position.y, neighbor_node.position.z,
					baseDist, extraCost, baseDist * w_dist * minCostPerUnit + extraCost );
			}

			// Compose tentative g using weighted base distance and all extras.
			const double tentative_g = current.g_cost + std::max( baseDist * w_dist * minCostPerUnit, 0.0 ) + extraCost;

			auto lookup_it = node_lookup.find( neighbor_node.key );
			if ( lookup_it == node_lookup.end() ) {
				nav_search_node_t neighbor_search = {
					.node = neighbor_node,
					.g_cost = tentative_g,
					.f_cost = tentative_g + heuristic( neighbor_node.position, goal_node.position ),
					.parent_index = current_index,
					.closed = false
				};

				nodes.push_back( neighbor_search );
				const int32_t neighbor_index = ( int32_t )nodes.size() - 1;
				open_list.push_back( neighbor_index );
				node_lookup.emplace( neighbor_node.key, neighbor_index );
				continue;
			}

			const int32_t neighbor_index = lookup_it->second;
			nav_search_node_t &neighbor_search = nodes[ neighbor_index ];
			if ( neighbor_search.closed ) {
				continue;
			}

			if ( tentative_g < neighbor_search.g_cost ) {
				neighbor_search.g_cost = tentative_g;
				neighbor_search.f_cost = tentative_g + heuristic( neighbor_node.position, goal_node.position );
				neighbor_search.parent_index = current_index;
			}
		}
	}

	/**
	*	A* search exhausted without finding a path to the goal.
	*	Provide detailed diagnostics to help understand why the path failed.
	**/
	gi.dprintf( "[WARNING][NavPath][A*] Pathfinding failed: No path found from (%.1f,%.1f,%.1f) to (%.1f,%.1f,%.1f)\n",
		start_node.position.x, start_node.position.y, start_node.position.z,
		goal_node.position.x, goal_node.position.y, goal_node.position.z );
	gi.dprintf( "[WARNING][NavPath][A*] Search stats: explored=%d nodes, max_nodes=%d, open_list_empty=%s\n",
		( int32_t )nodes.size(), MAX_SEARCH_NODES, open_list.empty() ? "true" : "false" );

	/**
	*    One-shot debug summary:
	*        Emit a compact breakdown so we can determine whether this is:
	*            1) no nodes found near start/goal or during expansion,
	*            2) connectivity exists but edge validation rejects all neighbors,
	*            3) coarse tile-route filter blocks expansion.
	**/
	if ( Nav_PathDiagEnabled() ) {
		gi.dprintf( "[DEBUG][NavPath][Diag] A* summary: neighbor_tries=%d, no_node=%d, edge_reject=%d, tile_filter_reject=%d\n",
			neighborTryCount, noNodeCount, edgeRejectCount, tileFilterRejectCount );
		gi.dprintf( "[DEBUG][NavPath][Diag] A* inputs: max_step=%.1f max_drop=%.1f cell_size_xy=%.1f z_quant=%.1f\n",
			nav_max_step ? nav_max_step->value : -1.0f,
			nav_max_drop ? nav_max_drop->value : -1.0f,
			mesh ? ( float )mesh->cell_size_xy : -1.0f,
			mesh ? ( float )mesh->z_quant : -1.0f );
		Nav_Debug_PrintNodeRef( "start_node", start_node );
		Nav_Debug_PrintNodeRef( "goal_node", goal_node );
		if ( tileRouteFilter ) {
			gi.dprintf( "[DEBUG][NavPath][Diag] tile_route_filter: %d tiles\n", ( int32_t )tileRouteFilter->size() );
		} else {
			gi.dprintf( "[DEBUG][NavPath][Diag] tile_route_filter: (none)\n" );
		}
	}

	// Diagnose reason for failure
	if ( ( int32_t )nodes.size() >= MAX_SEARCH_NODES ) {
		gi.dprintf( "[WARNING][NavPath][A*] Failure reason: Search node limit reached (%d nodes). The navmesh may be too large or the goal is very far.\n", MAX_SEARCH_NODES );
	} else if ( open_list.empty() ) {
		gi.dprintf( "[WARNING][NavPath][A*] Failure reason: Open list exhausted. This typically means:\n" );
		gi.dprintf( "[WARNING][NavPath][A*]   1. No navmesh connectivity exists between start and goal Z-levels\n" );
		gi.dprintf( "[WARNING][NavPath][A*]   2. All potential paths are blocked by obstacles\n" );
		gi.dprintf( "[WARNING][NavPath][A*]   3. Edge validation (Nav_CanTraverseStep) rejected all connections\n" );
		gi.dprintf( "[WARNING][NavPath][A*] Suggestions:\n" );
		gi.dprintf( "[WARNING][NavPath][A*]   - Check nav_max_step (current: %.1f) and nav_max_drop (current: %.1f)\n",
			nav_max_step ? nav_max_step->value : -1.0f,
			nav_max_drop ? nav_max_drop->value : -1.0f );
		gi.dprintf( "[WARNING][NavPath][A*]   - Verify navmesh has stairs/ramps connecting the Z-levels\n" );
		gi.dprintf( "[WARNING][NavPath][A*]   - Use 'nav_debug_draw 1' to visualize the navmesh\n" );
	}

	return false;
}



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

		if ( Nav_PathDiagEnabled() ) {
			gi.dprintf( "[DEBUG][NavPath][Diag] Cluster route: has=%d tiles=%d\n", hasTileRoute ? 1 : 0, ( int32_t )tileRoute.size() );
		}

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
*   @param  inout_index     Current waypoint index (updated on success).
*   @param  out_direction   Output normalized movement direction.
*   @return True if a valid direction was produced, false if path is complete/invalid.
**/
const bool SVG_Nav_QueryMovementDirection( const nav_traversal_path_t *path, const Vector3 &current_origin,
	double waypoint_radius, int32_t *inout_index, Vector3 *out_direction ) {
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
		const double maxDz = g_nav_mesh->max_step + g_nav_mesh->z_quant;
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
const bool Nav_CanTraverseStep_ExplicitBBox( const nav_mesh_t *mesh, const Vector3 &startPos, const Vector3 &endPos, const Vector3 &mins, const Vector3 &maxs, const edict_ptr_t *clip_entity, const svg_nav_path_policy_t *policy ) {
	if ( !mesh ) {
		return false;
	}

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

	// Compute the vertical travel we must be able to step up to reach endPos.
	const double requiredUp = std::max( 0.0, desiredDz );

	// If the required climb is larger than our allowed step height, this edge is infeasible.
	if ( requiredUp > stepSize ) {
		return false;
	}

	// If policy wants to enforce a minimum step height, only apply it to actual "step up" edges.
	if ( requiredUp > 0.0 && requiredUp < minStep ) {
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
			return false;
		}

		// Ensure we actually achieved (at least) the required step height.
		const double actualUp = upTr.endpos[ 2 ] - start[ 2 ];
		if ( actualUp + eps < requiredUp ) {
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
						// Check normal Z threshold from policy if available.
						if ( policy ) {
							if ( downTr2.plane.normal[ 2 ] <= 0.0f || downTr2.plane.normal[ 2 ] < policy->min_step_normal ) {
								return false;
							}
						} else {
							if ( downTr2.plane.normal[ 2 ] <= 0.0f || !IsWalkableSlope( downTr2.plane.normal, mesh->max_slope_deg ) ) {
								return false;
							}
						}
						// Optionally cap drop height.
						if ( policy && policy->cap_drop_height ) {
							const double drop = start[ 2 ] - downTr2.endpos[ 2 ];
							if ( drop > ( double )policy->max_drop_height ) {
								return false;
							}
						}
						return true;
					}
				}
			}
		}
		return false;
	}

	/**
	*	Step down to find supporting ground.
	*		Trace down far enough to re-acquire the ground after a step/flat/down edge.
	*		When climbing, drop the probe by the amount we stepped up; otherwise still probe
	*		by a small amount to ensure we're not floating.
	**/
	Vector3 down = fwdTr.endpos;
	Vector3 downEnd = down;
	downEnd[ 2 ] -= ( std::max( requiredUp, eps ) + eps );

	cm_trace_t downTr = Nav_Trace( down, mins, maxs, downEnd, clip_entity, CM_CONTENTMASK_SOLID );
	if ( downTr.allsolid || downTr.startsolid ) {
		return false;
	}

	// Must land on something (not floating).
	if ( downTr.fraction >= 1.0f ) {
		return false;
	}

	// Walkable-ish contact: ensure the surface normal is acceptable. Use policy threshold if provided.
	if ( downTr.plane.normal[ 2 ] <= 0.0f ) {
		return false;
	}
	if ( policy ) {
		if ( downTr.plane.normal[ 2 ] < policy->min_step_normal ) {
			return false;
		}
	} else {
		if ( !IsWalkableSlope( downTr.plane.normal, mesh->max_slope_deg ) ) {
			return false;
		}
	}

	// Enforce drop-limit safety from policy if requested.
	if ( policy && policy->cap_drop_height ) {
		const double drop = start[ 2 ] - downTr.endpos[ 2 ];
		if ( drop > ( double )policy->max_drop_height ) {
			return false;
		}
	}

	return true;
}