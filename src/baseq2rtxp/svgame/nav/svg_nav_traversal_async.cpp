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

//! Hard node limit to prevent runaway searches from consuming memory.
static constexpr int32_t NAV_ASTAR_MAX_NODES = 8192;
//! Per-call time budget used to cap frame impact of incremental expansion.
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

static void Nav_AStar_ExpandNeighbors( nav_a_star_state_t *state, int32_t current_index ) {
	const nav_mesh_t *mesh = state->mesh;
	if ( !mesh ) {
		return;
	}
	nav_search_node_t &current = state->nodes[ current_index ];
	const Vector3 &agent_mins = state->agent_mins;
	const Vector3 &agent_maxs = state->agent_maxs;
	const std::vector<nav_tile_cluster_key_t> *tileRouteFilter = state->tileRouteFilter;
	const svg_nav_path_policy_t *policy = state->policy;

	for ( const Vector3 &offset_dir : s_nav_neighbor_offsets ) {
		state->neighbor_try_count++;
		Vector3 scaledOffset = offset_dir;
		scaledOffset[ 0 ] *= ( float )mesh->cell_size_xy;
		scaledOffset[ 1 ] *= ( float )mesh->cell_size_xy;
		scaledOffset[ 2 ] *= ( float )mesh->z_quant;
		const Vector3 neighbor_origin = QM_Vector3Add( current.node.position, scaledOffset );

		/**
		*    Skip nodes outside the optional hierarchical tile route discovered by the path process.
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
				state->tile_filter_reject_count++;
				continue;
			}
		}

		nav_node_ref_t neighbor_node = {};
		if ( !Nav_FindNodeForPosition( mesh, neighbor_origin, neighbor_origin[ 2 ], &neighbor_node, true ) ) {
			state->no_node_count++;
			continue;
		}

		const double neighbor_drop = current.node.position[ 2 ] - neighbor_node.position[ 2 ];
		if ( policy && neighbor_drop > 0.0 && neighbor_drop > policy->drop_cap ) {
			continue;
		}

		/**
		*    The PMove-derived step test covers slopes, hills, and stairs while respecting both
		*    agent hulls and slope/drop constraints.
		**/
		if ( !Nav_CanTraverseStep_ExplicitBBox( mesh, current.node.position, neighbor_node.position, agent_mins, agent_maxs, nullptr, policy ) ) {
			state->edge_reject_count++;
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
		if ( neighbor_node.key.leaf_index >= 0 && neighbor_node.key.leaf_index < mesh->num_leafs ) {
			if ( neighbor_node.key.tile_index >= 0 && neighbor_node.key.tile_index < ( int32_t )mesh->world_tiles.size() ) {
				const nav_tile_t *tile = &mesh->world_tiles[ neighbor_node.key.tile_index ];
				const nav_xy_cell_t *cell = &tile->cells[ neighbor_node.key.cell_index ];
				if ( neighbor_node.key.layer_index >= 0 && neighbor_node.key.layer_index < cell->num_layers ) {
					neighborLayer = &cell->layers[ neighbor_node.key.layer_index ];
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
	state->search_budget_ms = NAV_ASTAR_STEP_BUDGET_MS;

	if ( tileRoute && !tileRoute->empty() ) {
		state->tile_route_storage = *tileRoute;
		state->tileRouteFilter = &state->tile_route_storage;
	} else {
		state->tileRouteFilter = nullptr;
	}

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
	*        - `NAV_ASTAR_STEP_BUDGET_MS` throttles per-call time while allowing incremental continuation.
	**/
	state->step_start_ms = gi.Com_GetSystemMilliseconds();

	while ( expansions > 0 && state->status == nav_a_star_status_t::Running ) {
		if ( state->open_list.empty() ) {
			state->status = nav_a_star_status_t::Failed;
			break;
		}

		if ( ( int32_t )state->nodes.size() >= state->max_nodes ) {
			state->hit_stagnation_limit = true;
			state->status = nav_a_star_status_t::Failed;
			break;
		}

		const uint64_t now = gi.Com_GetSystemMilliseconds();
		if ( state->search_budget_ms > 0 && ( now - state->step_start_ms ) >= state->search_budget_ms ) {
			state->hit_time_budget = true;
			break;
		}

		int32_t best_open_index = 0;
		double best_f_cost = state->nodes[ state->open_list[ 0 ] ].f_cost;
		for ( int32_t i = 1; i < ( int32_t )state->open_list.size(); i++ ) {
			const double f_cost = state->nodes[ state->open_list[ i ] ].f_cost;
			if ( f_cost < best_f_cost ) {
				best_f_cost = f_cost;
				best_open_index = i;
			}
		}

		const int32_t current_index = state->open_list[ best_open_index ];
		state->open_list[ best_open_index ] = state->open_list.back();
		state->open_list.pop_back();

		nav_search_node_t &current = state->nodes[ current_index ];
		current.closed = true;

		if ( current.f_cost + 1e-3 < state->best_f_cost_seen ) {
			state->best_f_cost_seen = current.f_cost;
			state->stagnation_count = 0;
		} else {
			state->stagnation_count++;
		}
		if ( state->stagnation_count > 2048 && ( int32_t )state->nodes.size() >= ( state->max_nodes / 2 ) ) {
			state->hit_stagnation_limit = true;
			state->status = nav_a_star_status_t::Failed;
			break;
		}

		if ( current.node.key == state->goal_node.key ) {
			state->goal_index = current_index;
			state->status = nav_a_star_status_t::Completed;
			break;
		}

		Nav_AStar_ExpandNeighbors( state, current_index );
		expansions--;
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
