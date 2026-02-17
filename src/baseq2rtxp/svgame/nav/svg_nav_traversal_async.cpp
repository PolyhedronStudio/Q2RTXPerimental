/********************************************************************
*
*
*    SVGame: Navigation Traversal Async Helpers (implementation)
*
*
********************************************************************/

#include "svgame/svg_local.h"
#include "svgame/nav/svg_nav_clusters.h"
#include "svgame/nav/svg_nav_traversal.h"
#include "svgame/nav/svg_nav_traversal_async.h"
#include "svgame/nav/svg_nav_path_process.h"
#include "svgame/nav/svg_nav_request.h" // used to size reserve heuristics

#include <algorithm>
#include <cmath>
#include <limits>

extern cvar_t *nav_max_step;
extern cvar_t *nav_drop_cap;
extern cvar_t *nav_cost_w_dist;
extern cvar_t *nav_cost_jump_base;
extern cvar_t *nav_cost_jump_height_weight;
extern cvar_t *nav_cost_los_weight;
extern cvar_t *nav_cost_dynamic_weight;
extern cvar_t *nav_cost_failure_weight;
extern cvar_t *nav_cost_failure_tau_ms;
extern cvar_t *nav_cost_turn_weight;
extern cvar_t *nav_cost_slope_weight;
extern cvar_t *nav_cost_drop_weight;
extern cvar_t *nav_cost_min_cost_per_unit;

//! Optional runtime cvar to tune per-call A* step budget (milliseconds).
extern cvar_t *nav_astar_step_budget_ms;

inline bool Nav_PathDiagEnabled( void );

//! Hard node limit to prevent runaway searches from consuming memory.
static constexpr int32_t NAV_ASTAR_MAX_NODES = 8192;
//! Default per-call time budget used to cap frame impact of incremental expansion (fallback when cvar not registered).
static constexpr uint64_t NAV_ASTAR_STEP_BUDGET_MS = 8;

//! Neighbor offsets (XY/Z in grid-cell units) considered by the search.
static constexpr Vector3 s_nav_neighbor_offsets[] = {
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
	{ -2.0f, -2.0f, 0.0f },

	{ 0.0f, 0.0f, 1.0f },
	{ 0.0f, 0.0f, -1.0f },

	{ 1.0f, 0.0f, 1.0f },
	{ -1.0f, 0.0f, 1.0f },
	{ 0.0f, 1.0f, 1.0f },
	{ 0.0f, -1.0f, 1.0f },
	{ 1.0f, 1.0f, 1.0f },
	{ 1.0f, -1.0f, 1.0f },
	{ -1.0f, 1.0f, 1.0f },
	{ -1.0f, -1.0f, 1.0f },

	{ 1.0f, 0.0f, -1.0f },
	{ -1.0f, 0.0f, -1.0f },
	{ 0.0f, 1.0f, -1.0f },
	{ 0.0f, -1.0f, -1.0f },
	{ 1.0f, 1.0f, -1.0f },
	{ 1.0f, -1.0f, -1.0f },
	{ -1.0f, 1.0f, -1.0f },
	{ -1.0f, -1.0f, -1.0f },

	{ 2.0f, 0.0f, 1.0f },
	{ -2.0f, 0.0f, 1.0f },
	{ 0.0f, 2.0f, 1.0f },
	{ 0.0f, -2.0f, 1.0f },
	{ 2.0f, 2.0f, 1.0f },
	{ 2.0f, -2.0f, 1.0f },
	{ -2.0f, 2.0f, 1.0f },
	{ -2.0f, -2.0f, 1.0f },
	{ 2.0f, 0.0f, -1.0f },
	{ -2.0f, 0.0f, -1.0f },
	{ 0.0f, 2.0f, -1.0f },
	{ 0.0f, -2.0f, -1.0f },
	{ 2.0f, 2.0f, -1.0f },
	{ 2.0f, -2.0f, -1.0f },
	{ -2.0f, 2.0f, -1.0f },
	{ -2.0f, -2.0f, -1.0f }
};

static inline double Nav_AStar_Heuristic( const Vector3 &a, const Vector3 &b ) {
	const Vector3 delta = QM_Vector3Subtract( b, a );
	return sqrtf( ( delta[ 0 ] * delta[ 0 ] ) + ( delta[ 1 ] * delta[ 1 ] ) + ( delta[ 2 ] * delta[ 2 ] ) );
}

/**
*	@brief	Expand neighbor nodes for the given `current_index`.
*	@param	state		A* state.
*	@param	current_index	Index of the node to expand.
*	@note	This function performs heavy work and is invoked from `Nav_AStar_Step`.
*			To enforce strict per-call time budgets this function checks the
*			A* per-call search budget periodically and returns early when exceeded.
**/
static void Nav_AStar_ExpandNeighbors( nav_a_star_state_t *state, int32_t current_index ) {
	const nav_mesh_t *mesh = state->mesh;
	if ( !mesh ) {
		return;
	}
	nav_search_node_t &current = state->nodes[ current_index ];
	const Vector3 &agent_mins = state->agent_mins;
	const Vector3 &agent_maxs = state->agent_maxs;
    // Use the safe accessor to obtain a view of the optional tile-route filter.
	const std::vector<nav_tile_cluster_key_t> &tileRouteView = state->GetTileRouteFilterView();
	const svg_nav_path_policy_t *policy = state->policy;

	// Local counter to make an internal periodic budget check every N neighbor iterations.
	int32_t innerExpansionCounter = 0;
	const int32_t innerCheckInterval = 8; // check budget every 8 neighbor evaluations

	for ( const Vector3 &offset_dir : s_nav_neighbor_offsets ) {
		/**
		*	Check per-call time budget periodically to ensure a single A* step
		*	does not run for too long even if many neighbors are evaluated.
		**/
		if ( state->search_budget_ms > 0 && ( ( innerExpansionCounter & ( innerCheckInterval - 1 ) ) == 0 ) ) {
			const uint64_t now = gi.GetRealSystemTime();
			if ( ( now - state->step_start_ms ) >= state->search_budget_ms ) {
				// Mark the budget hit and return early so the async worker can yield.
				state->hit_time_budget = true;
				return;
			}
		}
		innerExpansionCounter++;

        // Track this neighbor attempt for diagnostics and tuning.
		state->neighbor_try_count++;
		Vector3 scaledOffset = offset_dir;
		scaledOffset[ 0 ] *= ( float )mesh->cell_size_xy;
		scaledOffset[ 1 ] *= ( float )mesh->cell_size_xy;
		scaledOffset[ 2 ] *= ( float )mesh->z_quant;
		const Vector3 neighbor_origin = QM_Vector3Add( current.node.position, scaledOffset );

		/**
		*    Skip nodes outside the optional hierarchical tile route discovered by the path process.
		**/
        if ( !tileRouteView.empty() ) {
			const nav_tile_cluster_key_t nk = SVG_Nav_GetTileKeyForPosition( mesh, neighbor_origin );
			bool allowed = false;
			for ( const nav_tile_cluster_key_t &k : tileRouteView ) {
				if ( k == nk ) {
					allowed = true;
					break;
				}
			}
			if ( !allowed ) {
          // Neighbor rejected due to tile-route filter.
			state->tile_filter_reject_count++;
			if ( Nav_PathDiagEnabled() ) {
				gi.dprintf( "[DEBUG][NavPath][Diag] Neighbor rejected by tile-route filter at pos=(%.1f %.1f %.1f)\n",
					neighbor_origin.x, neighbor_origin.y, neighbor_origin.z );
			}
				continue;
			}
		}

		nav_node_ref_t neighbor_node = {};
		if ( !Nav_FindNodeForPosition( mesh, neighbor_origin, neighbor_origin[ 2 ], &neighbor_node, true ) ) {
         // No node exists at this neighbor position.
			state->no_node_count++;
			if ( Nav_PathDiagEnabled() ) {
				gi.dprintf( "[DEBUG][NavPath][Diag] No node at neighbor pos=(%.1f %.1f %.1f)\n",
					neighbor_origin.x, neighbor_origin.y, neighbor_origin.z );
			}
			continue;
		}

		const double neighbor_drop = current.node.position[ 2 ] - neighbor_node.position[ 2 ];
      if ( policy && neighbor_drop > 0.0 && neighbor_drop > policy->drop_cap ) {
			// Neighbor rejected due to drop cap.
			if ( Nav_PathDiagEnabled() ) {
				gi.dprintf( "[DEBUG][NavPath][Diag] Neighbor rejected by drop cap: drop=%.1f cap=%.1f pos=(%.1f %.1f %.1f)\n",
					neighbor_drop, policy->drop_cap, neighbor_node.position.x, neighbor_node.position.y, neighbor_node.position.z );
			}
			continue;
		}

		/**
		*    The PMove-derived step test covers slopes, hills, and stairs while respecting both
		*    agent hulls and slope/drop constraints.
		**/
      if ( !Nav_CanTraverseStep_ExplicitBBox( mesh, current.node.position, neighbor_node.position, agent_mins, agent_maxs, nullptr, policy ) ) {
			// Edge validation failed for this neighbor.
			state->edge_reject_count++;
			if ( Nav_PathDiagEnabled() ) {
				double dz = neighbor_node.position.z - current.node.position.z;
				gi.dprintf( "[DEBUG][NavPath][Diag] Edge rejected between (%.1f %.1f %.1f) -> (%.1f %.1f %.1f) dz=%.1f\n",
					current.node.position.x, current.node.position.y, current.node.position.z,
					neighbor_node.position.x, neighbor_node.position.y, neighbor_node.position.z,
					dz );
			}
			continue;
		}

		if ( neighbor_node.position.z != current.node.position.z ) {
			state->saw_vertical_neighbor = true;
		}

		const double baseDist = Nav_AStar_Heuristic( current.node.position, neighbor_node.position );

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
		const double minCostPerUnit = nav_cost_min_cost_per_unit ? nav_cost_min_cost_per_unit->value : 1.0f;

		double extraCost = 0.0;

        const nav_layer_t *neighborLayer = nullptr;
		/**
		*    Resolve neighbor layer flags safely.
		*    Use the public tile/cell accessors to avoid dereferencing potentially
		*    null or corrupted `world_tiles`/`tile->cells` arrays on sparse tiles.
		**/
		if ( neighbor_node.key.leaf_index >= 0 && neighbor_node.key.leaf_index < mesh->num_leafs ) {
			if ( neighbor_node.key.tile_index >= 0 && neighbor_node.key.tile_index < ( int32_t )mesh->world_tiles.size() ) {
				// Obtain const reference to the canonical world tile.
				const nav_tile_t &tile = mesh->world_tiles[ neighbor_node.key.tile_index ];

				// Safe accessor: get cells pointer/count for this tile (const overload).
				auto cellsView = SVG_Nav_Tile_GetCells( mesh, &tile );
				const nav_xy_cell_t *cellsPtr = cellsView.first;
				const int32_t cellsCount = cellsView.second;
				if ( cellsPtr && neighbor_node.key.cell_index >= 0 && neighbor_node.key.cell_index < cellsCount ) {
					// Use safe accessor for layers to avoid dangling pointer derefs.
					auto layersView = SVG_Nav_Cell_GetLayers( &cellsPtr[ neighbor_node.key.cell_index ] );
					const nav_layer_t *layersPtr = layersView.first;
					const int32_t layerCount = layersView.second;
					if ( layersPtr && neighbor_node.key.layer_index >= 0 && neighbor_node.key.layer_index < layerCount ) {
						neighborLayer = &layersPtr[ neighbor_node.key.layer_index ];
					}
				}
			}
		}

		if ( neighborLayer ) {
			if ( ( neighborLayer->flags & NAV_FLAG_WATER ) != 0 ) {
				extraCost += 1.0f;
			}
			if ( ( neighborLayer->flags & NAV_FLAG_LAVA ) != 0 ) {
				extraCost += 100.0f;
			}
			if ( ( neighborLayer->flags & NAV_FLAG_SLIME ) != 0 ) {
				extraCost += 4.0f;
			}
		}

		const double dz = neighbor_node.position.z - current.node.position.z;
		const double horizontal = sqrtf( ( neighbor_node.position.x - current.node.position.x ) * ( neighbor_node.position.x - current.node.position.x ) +
			( neighbor_node.position.y - current.node.position.y ) * ( neighbor_node.position.y - current.node.position.y ) );
		const double slope = ( horizontal > 0.0001f )
			? ( fabsf( dz ) / horizontal )
			: 0.0f;
		extraCost += slopeWeight * slope * slope * baseDist;

		if ( dz > 0.0 ) {
			const double stepLimit = policy ? policy->max_step_height : ( nav_max_step ? nav_max_step->value : 18.0f );
			if ( dz > stepLimit ) {
				extraCost += ( dz / std::max( stepLimit, 1.0 ) ) * 2.0;
			}
		} else {
			const double drop = -dz;
			const double maxDrop = policy ? policy->max_drop_height : ( nav_drop_cap ? nav_drop_cap->value : 128.0f );
			if ( drop > 0.0 ) {
				extraCost += dropWeight * ( drop / std::max( maxDrop, 1.0 ) );
			}
		}

		if ( policy && policy->allow_small_obstruction_jump ) {
			const double hj = std::max( 0.0, dz - ( double )policy->max_step_height );
			if ( hj > 0.0f ) {
				if ( hj > ( double )policy->max_obstruction_jump_height ) {
					continue;
				}
				const double jumpCost = jumpBase + jumpHeightWeight * ( hj / ( double )policy->max_obstruction_jump_height );
				extraCost += jumpCost;
			}
		}

		bool hasLOS = false;
		{
			cm_trace_t losTr = gi.trace( &neighbor_node.position, &mesh->agent_mins, &mesh->agent_maxs, &state->goal_node.position, nullptr, CM_CONTENTMASK_SOLID );
			if ( losTr.fraction >= 1.0f && !losTr.startsolid && !losTr.allsolid ) {
				hasLOS = true;
			}
		}
		if ( hasLOS ) {
			extraCost -= losWeight;
		}

		if ( SVG_Nav_Occupancy_Blocked( mesh, neighbor_node.key.tile_index, neighbor_node.key.cell_index, neighbor_node.key.layer_index ) ) {
			continue;
		}
		const int32_t occSoftCost = SVG_Nav_Occupancy_SoftCost( mesh, neighbor_node.key.tile_index, neighbor_node.key.cell_index, neighbor_node.key.layer_index );
		if ( occSoftCost > 0 ) {
			extraCost += dynamicWeight * ( double )occSoftCost;
		}

		if ( state->pathProcess ) {
			const QMTime now = level.time;
			const QMTime lastFail = state->pathProcess->last_failure_time;
			if ( lastFail > 0_ms ) {
				const double tauMs = failureTauMs > 0.0f ? failureTauMs : 5000.0f;
				const double dt = ( double )( ( now - lastFail ).Milliseconds() );
				const double factor = std::exp( -dt / tauMs );
				extraCost += failureWeight * ( double )factor;

				const Vector3 toLastFail = QM_Vector3Subtract( neighbor_node.position, state->pathProcess->last_failure_pos );
				const double distToFail = QM_Vector3LengthDP( toLastFail );
				const double failPosRadius = 64.0f;
				if ( distToFail <= failPosRadius ) {
					const double posFactor = 1.0f - ( distToFail / failPosRadius );
					const double sigPenalty = failureWeight * ( double )( 0.75f * posFactor );
					extraCost += sigPenalty;
				}

				const double neighborYaw = QM_Vector3ToYaw( QM_Vector3Subtract( neighbor_node.position, current.node.position ) );
				double yawDelta = fabsf( neighborYaw - state->pathProcess->last_failure_yaw );
				yawDelta = fmodf( yawDelta + 180.0f, 360.0f ) - 180.0f;
				const double yawThresh = 45.0f;
				if ( fabsf( yawDelta ) <= yawThresh ) {
					const double yawFactor = 1.0f - ( fabsf( yawDelta ) / yawThresh );
					const double yawPenalty = failureWeight * ( double )( 0.5f * yawFactor );
					extraCost += yawPenalty;
				}
			}
		} else if ( policy ) {
			extraCost += failureWeight * ( double )policy->fail_backoff_max_pow * 0.01f;
		}

		if ( current.parent_index >= 0 ) {
			const nav_search_node_t &parentNode = state->nodes[ current.parent_index ];
			Vector3 fromDir = QM_Vector3NormalizeDP( QM_Vector3Subtract( current.node.position, parentNode.node.position ) );
			Vector3 toDir = QM_Vector3NormalizeDP( QM_Vector3Subtract( neighbor_node.position, current.node.position ) );
			const double dot = QM_Vector3DotProductDP( fromDir, toDir );
			const double clamped = QM_Clamp( dot, -1.0, 1.0 );
			const double ang = acosf( clamped );
			extraCost += turnWeight * ( ang / ( double )M_PI );
		}

		const double tentative_g = current.g_cost + std::max( baseDist * w_dist * minCostPerUnit, 0.0 ) + extraCost;

		auto lookup_it = state->node_lookup.find( neighbor_node.key );
		if ( lookup_it == state->node_lookup.end() ) {
			nav_search_node_t neighbor_search = {
				.node = neighbor_node,
				.g_cost = tentative_g,
				.f_cost = tentative_g + Nav_AStar_Heuristic( neighbor_node.position, state->goal_node.position ),
				.parent_index = current_index,
				.closed = false
			};

			state->nodes.push_back( neighbor_search );
			const int32_t neighbor_index = ( int32_t )state->nodes.size() - 1;
			state->open_list.push_back( neighbor_index );
			state->node_lookup.emplace( neighbor_node.key, neighbor_index );
			continue;
		}

		const int32_t neighbor_index = lookup_it->second;
		nav_search_node_t &neighbor_search = state->nodes[ neighbor_index ];
		if ( neighbor_search.closed ) {
			continue;
		}

		if ( tentative_g < neighbor_search.g_cost ) {
			neighbor_search.g_cost = tentative_g;
			neighbor_search.f_cost = tentative_g + Nav_AStar_Heuristic( neighbor_node.position, state->goal_node.position );
			neighbor_search.parent_index = current_index;
		}
	}
}

/**
*	@brief	Initialize an A* state.
*	@note	Sets up internal containers and applies the configured per-call search budget.
**/
bool Nav_AStar_Init( nav_a_star_state_t *state, const nav_mesh_t *mesh, const nav_node_ref_t &start_node,
	const nav_node_ref_t &goal_node, const Vector3 &agent_mins, const Vector3 &agent_maxs,
	const svg_nav_path_policy_t *policy, const std::vector<nav_tile_cluster_key_t> *tileRoute,
	const svg_nav_path_process_t *pathProcess ) {
	if ( !state || !mesh ) {
		return false;
	}

	Nav_AStar_Reset( state );

	state->mesh = mesh;
	state->agent_mins = agent_mins;
	state->agent_maxs = agent_maxs;
	state->start_node = start_node;
	state->goal_node = goal_node;
	state->policy = policy;
	state->pathProcess = pathProcess;
	state->max_nodes = NAV_ASTAR_MAX_NODES;

	// Apply configured search budget: prefer runtime cvar when available so budget can be tuned without rebuilds.
	if ( nav_astar_step_budget_ms ) {
		state->search_budget_ms = ( uint64_t )std::max( 0.0f, nav_astar_step_budget_ms->value );
	} else {
		state->search_budget_ms = NAV_ASTAR_STEP_BUDGET_MS;
	}

	// Reserve containers to avoid repeated reallocations during incremental searches.
	// Use a heuristic driven by async expansion budget to pick a sensible reserve size.
	// We prefer a modest default of 512 to reduce transient allocations for common short paths.
	// If async expansions are configured larger, use that as a hint (clamped by max_nodes).
	int32_t asyncExpansionsHint = SVG_Nav_GetAsyncRequestExpansions(); // >= 1
	const size_t heuristic = ( size_t )std::max( asyncExpansionsHint, 512 );
	const size_t reserveNodes = std::min<size_t>( heuristic, ( size_t )state->max_nodes );

	state->nodes.reserve( reserveNodes );
	state->open_list.reserve( reserveNodes / 2 );
	state->node_lookup.reserve( reserveNodes * 2 );

	if ( tileRoute && !tileRoute->empty() ) {
		state->tile_route_storage = *tileRoute;
		state->tileRouteFilter = &state->tile_route_storage;
	} else {
		state->tileRouteFilter = nullptr;
	}
	// Debug assertion: ensure non-owning pointer either null or points into our storage.
	Q_assert( state->tileRouteFilter == nullptr || state->tileRouteFilter == &state->tile_route_storage );

	nav_search_node_t start_search = {
		.node = start_node,
		.g_cost = 0.0,
		.f_cost = Nav_AStar_Heuristic( start_node.position, goal_node.position ),
		.parent_index = -1,
		.closed = false
	};

	state->nodes.push_back( start_search );
	state->open_list.push_back( 0 );
	state->node_lookup.emplace( start_node.key, 0 );
	state->best_f_cost_seen = start_search.f_cost;
	state->stagnation_count = 0;
	state->hit_stagnation_limit = false;
	state->hit_time_budget = false;
	state->goal_index = -1;
	state->status = nav_a_star_status_t::Running;

	if ( start_node.key == goal_node.key ) {
		state->goal_index = 0;
		state->status = nav_a_star_status_t::Completed;
	}

	return true;
}

/**
*	@brief	Advance the A* search by up to `expansions` node pops.
*	@param	state		A* state to step.
*	@param	expansions	Maximum node expansions to perform this call.
*	@return	Current search status after stepping.
*	@note	This function enforces both an expansions budget and a strict per-call time budget.
**/
nav_a_star_status_t Nav_AStar_Step( nav_a_star_state_t *state, int32_t expansions ) {
	if ( !state || state->status != nav_a_star_status_t::Running || expansions <= 0 ) {
		return state ? state->status : nav_a_star_status_t::Failed;
	}
	if ( !state->mesh ) {
		state->status = nav_a_star_status_t::Failed;
		return state->status;
	}

	/**
	*    Expansion budget guard:
	*        - `expansions` limits work per call to keep frame time predictable.
	*        - `NAV_ASTAR_MAX_NODES` prevents unbounded node growth on pathological meshes.
	*        - `search_budget_ms` throttles per-call time while allowing incremental continuation.
	**/
	state->step_start_ms = gi.GetRealSystemTime();

	int32_t expansionCounter = 0;
	while ( expansions > 0 && state->status == nav_a_star_status_t::Running ) {
		// If the open list is empty, we failed to find a path.
		if ( state->open_list.empty() ) {
			state->status = nav_a_star_status_t::Failed;
			break;
		}

		// Protect against excessive node growth.
		if ( ( int32_t )state->nodes.size() >= state->max_nodes ) {
			state->hit_stagnation_limit = true;
			state->status = nav_a_star_status_t::Failed;
			break;
		}

        // Check time budget periodically (every 8 expansions) to avoid calling the clock too often.
		if ( state->search_budget_ms > 0 && ( ( expansionCounter & 0x7 ) == 0 ) ) {
			const uint64_t now = gi.GetRealSystemTime();
			if ( ( now - state->step_start_ms ) >= state->search_budget_ms ) {
				state->hit_time_budget = true;
				break;
			}
		}

		auto best_it = std::min_element( state->open_list.begin(), state->open_list.end(),
			[ &state ]( int32_t a, int32_t b ) {
				return state->nodes[ a ].f_cost < state->nodes[ b ].f_cost;
			} );

		const int32_t current_index = *best_it;
		*best_it = state->open_list.back();
		state->open_list.pop_back();

		nav_search_node_t &current = state->nodes[ current_index ];
		current.closed = true;

		if ( current.node.key == state->goal_node.key ) {
			state->goal_index = current_index;
			state->status = nav_a_star_status_t::Completed;
			break;
		}

        // Expand neighbors; this function will itself return early if the per-call time budget is hit.
        Nav_AStar_ExpandNeighbors( state, current_index );
		// If ExpandNeighbors set hit_time_budget, break early.
		if ( state->hit_time_budget ) {
			break;
		}

		expansions--;
		expansionCounter++;
	}

	return state->status;
}

bool Nav_AStar_Finalize( nav_a_star_state_t *state, std::vector<Vector3> *out_points ) {
	if ( !state || !out_points ) {
		return false;
	}
	if ( state->status != nav_a_star_status_t::Completed ) {
		return false;
	}

	/**
	*    Walk the parent chain from the solved node to recover the waypoint list.
	**/
	out_points->clear();
	if ( state->start_node.key == state->goal_node.key ) {
		out_points->push_back( state->start_node.position );
		out_points->push_back( state->goal_node.position );
		return true;
	}

	if ( state->goal_index < 0 || state->goal_index >= ( int32_t )state->nodes.size() ) {
		return false;
	}

	std::vector<Vector3> reversed;
	reversed.reserve( 64 );
	int32_t trace_index = state->goal_index;
	while ( trace_index >= 0 ) {
		reversed.push_back( state->nodes[ trace_index ].node.position );
		trace_index = state->nodes[ trace_index ].parent_index;
	}

	out_points->assign( reversed.rbegin(), reversed.rend() );
	return true;
}

void Nav_AStar_Reset( nav_a_star_state_t *state ) {
	if ( !state ) {
		return;
	}

	state->status = nav_a_star_status_t::Idle;
	state->mesh = nullptr;
	state->agent_mins = {};
	state->agent_maxs = {};
	state->start_node = {};
	state->goal_node = {};
	state->nodes.clear();
	state->open_list.clear();
	state->node_lookup.clear();
	state->tile_route_storage.clear();
	state->tileRouteFilter = nullptr;
	state->policy = nullptr;
	state->pathProcess = nullptr;
	state->goal_index = -1;
	state->best_f_cost_seen = std::numeric_limits<double>::infinity();
	state->stagnation_count = 0;
	state->hit_stagnation_limit = false;
	state->hit_time_budget = false;
	state->saw_vertical_neighbor = false;
	state->neighbor_try_count = 0;
	state->no_node_count = 0;
	state->tile_filter_reject_count = 0;
	state->edge_reject_count = 0;
	state->step_start_ms = 0;
}
