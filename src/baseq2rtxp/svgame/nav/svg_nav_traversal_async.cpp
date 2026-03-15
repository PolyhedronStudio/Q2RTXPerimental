/** * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
* 
* 
*	SVGame: Navigation Traversal Async Helpers (implementation)
* 
* 
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/

#include "svgame/svg_local.h"
#include "svgame/nav/svg_nav_clusters.h"
#include "svgame/nav/svg_nav_traversal.h"
#include "svgame/nav/svg_nav_traversal_async.h"
#include "svgame/nav/svg_nav_path_process.h"
#include "svgame/nav/svg_nav_request.h" // used to size reserve heuristics

#include <algorithm>
#include <cmath>
#include <limits>

//! Optional runtime cvars to tune the tile-route corridor buffering applied to the fine search.
extern cvar_t *nav_refine_corridor_mid_tiles;
extern cvar_t *nav_refine_corridor_far_tiles;
extern cvar_t *nav_refine_corridor_radius_near;
extern cvar_t *nav_refine_corridor_radius_mid;
extern cvar_t *nav_refine_corridor_radius_far;

//! Optional runtime cvar to tune per-call A*	step budget (milliseconds).
extern cvar_t *nav_astar_step_budget_ms;

// Local debug control cvars for neighbor expansion diagnostics.
// These are lazily created on first use to avoid ordering/init dependencies.
static cvar_t *s_nav_expand_diag_enable = nullptr;
static cvar_t *s_nav_expand_diag_cooldown_ms = nullptr;

/**
*	  Hard node limit to prevent runaway searches from consuming memory.
**/
static constexpr int32_t NAV_ASTAR_MAX_NODES = 65536 * 4;
//! Default per-call time budget used to cap frame impact of incremental expansion (fallback when cvar not registered).
static constexpr uint64_t NAV_ASTAR_STEP_BUDGET_MS = 25;

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


// Forward declare small diagnostics helper used by gated prints below.
static inline bool Nav_PathDiagEnabled( void ) { return true; }

// Helper: stringify rejection reason for diagnostics/telemetry.
static const char *Nav_EdgeRejectReasonToString( nav_edge_reject_reason_t r ) {
	switch ( r ) {
	case nav_edge_reject_reason_t::None: return "None";
	case nav_edge_reject_reason_t::TileRouteFilter: return "TileRouteFilter";
	case nav_edge_reject_reason_t::NoNode: return "NoNode";
	case nav_edge_reject_reason_t::DropCap: return "DropCap";
	case nav_edge_reject_reason_t::StepTest: return "StepTest";
	case nav_edge_reject_reason_t::ObstructionJump: return "ObstructionJump";
	case nav_edge_reject_reason_t::Occupancy: return "Occupancy";
	case nav_edge_reject_reason_t::Other: return "Other";
	default: return "Unknown";
	}
}

/**
*	@brief	Determine whether an async neighbor offset represents a distinct cell-hop probe.
*	@param	offset_dir	Neighbor offset in cell/layer units.
*	@return	True when the offset should be evaluated by async expansion.
*	@note	Adjacent-cell layer selection already resolves the best reachable layer in the target XY cell,
* 			s o z-biased variants and long-hop variants only multiply search states without adding a stable
*				local traversal step. Pure vertical probes alias back onto the current node and are also skipped.
**/
static inline bool Nav_AStar_ShouldProbeNeighborOffset( const Vector3 &offset_dir ) {
	/** 
	*	Skip all probes that do not represent a distinct XY cell hop.
	**/
	if ( offset_dir.x == 0.0f && offset_dir.y == 0.0f ) {
		return false;
	}

    /** 
	*	Restore short XY bridge probes while keeping expansion planar.
	*	    The last known-good state produced full paths with a broader XY neighborhood available.
	*	    Re-allow up to 2-cell planar hops so async expansion can bridge sparse walkable cells,
	*	    but keep all non-step Z variants disabled so the search still extends only across XY.
	**/
	if ( std::fabs( offset_dir.x ) > 2.0f || std::fabs( offset_dir.y ) > 2.0f ) {
		return false;
	}

	/** 
	*	Skip redundant z-biased variants for the same XY hop because adjacent-cell layer selection
	*	already chooses the best target layer from the destination cell.
	**/
	if ( offset_dir.z != 0.0f ) {
		return false;
	}

	return true;
}

/** 
*	@brief	Apply sparse dynamic occupancy policy to a candidate async neighbor.
*	@param	state	A*	state receiving diagnostic counters.
*	@param	mesh	Navigation mesh consulted for sparse occupancy data.
*	@param	neighbor_node	Candidate canonical neighbor.
*	@param	policy	Traversal policy controlling whether occupancy is ignored, softened, or hard-blocked.
*	@param	dynamic_weight	Base runtime cost multiplier already used by the async scorer.
*	@param	inout_extra_cost	[in,out] Accumulated neighbor cost updated with any occupancy soft cost.
*	@return	True when the neighbor should be rejected due to hard occupancy blocking.
*	@note	This keeps sparse occupancy local and policy-driven so callers can prefer soft-cost steering before hard blocking.
**/
static bool Nav_AStar_ApplyDynamicOccupancyPolicy( nav_a_star_state_t * state, const nav_mesh_t * mesh, const nav_node_ref_t &neighbor_node,
	const svg_nav_path_policy_t * policy, const double dynamic_weight, double * inout_extra_cost ) {
	/** 
	*	Sanity checks: require mesh and output storage before consulting occupancy overlays.
	**/
	if ( !mesh || !inout_extra_cost ) {
		return false;
	}

	/** 
	*	Allow callers to disable sparse occupancy participation entirely for targeted policy experiments.
	**/
	if ( policy && !policy->use_dynamic_occupancy ) {
		return false;
	}

	/** 
  * Read the authoritative sparse occupancy overlay entry once so blocked and soft-cost semantics stay consistent.
	**/
	nav_occupancy_entry_t occupancy = {};
	if ( !SVG_Nav_Occupancy_TryGet( mesh, neighbor_node.key.tile_index, neighbor_node.key.cell_index, neighbor_node.key.layer_index, &occupancy ) ) {
		return false;
	}
	if ( state ) {
		state->occupancy_overlay_hit_count++;
	}

	const bool occupancyBlocked = occupancy.blocked;
	const int32_t occupancySoftCost = occupancy.soft_cost;
	const double occupancySoftScale = policy ? std::max( 0.0, policy->dynamic_occupancy_soft_cost_scale ) : 1.0;

	/** 
	*	Prefer soft-cost steering first unless the policy explicitly requests hard blocking.
	**/
	if ( occupancyBlocked && ( !policy || policy->hard_block_dynamic_occupancy ) ) {
		if ( state ) {
			state->occupancy_block_reject_count++;
			state->edge_reject_reason_counts[(int)nav_edge_reject_reason_t::Occupancy]++;
		}
		return true;
	}

	/** 
	*	Convert sparse occupancy pressure into local additional traversal cost when enabled.
	**/
	const int32_t effectiveSoftCost = occupancyBlocked ? std::max( occupancySoftCost, 1 ) : occupancySoftCost;
	if ( effectiveSoftCost > 0 && occupancySoftScale > 0.0 ) {
     if ( state ) {
			state->occupancy_soft_cost_hit_count++;
		}
		*inout_extra_cost += dynamic_weight* occupancySoftScale* ( double )effectiveSoftCost;
	}

	return false;
}

/** 
*	@brief	Append a tile-route key only when it is not already present in the buffered filter.
*	@param	bufferedRoute	Buffered route under construction.
*	@param	key		Tile key to append.
*	@note	Keeps the widened coarse-route corridor stable without requiring hash support for the tile key type.
**/
static void Nav_AStar_AppendUniqueRouteTile( std::vector<nav_tile_cluster_key_t> &bufferedRoute, const nav_tile_cluster_key_t &key ) {
	/** 
	*	Skip duplicate keys so the buffered filter stays compact and deterministic.
	**/
	if ( std::find( bufferedRoute.begin(), bufferedRoute.end(), key ) != bufferedRoute.end() ) {
		return;
	}

	// Append the newly discovered tile key.
	bufferedRoute.push_back( key );
}

/** 
*	@brief	Choose an adaptive tile-route corridor radius from coarse route length.
*	@param	routeTileCount	Number of tiles in the exact coarse route.
*	@return	Chebyshev corridor radius in tiles used to buffer the fine-search allow-list.
*	@note	Short routes should stay narrow for performance, while long routes need wider detour freedom
* 			to avoid getting trapped by an overly strict tile-route spine.
**/
static int32_t Nav_AStar_ComputeRouteBufferRadius( const size_t routeTileCount ) {
	/** 
	*	Keep short routes tight and expand only when the coarse route becomes long enough to justify it.
	**/
	//! <Q2RTXP>: WID: Was `std::max( 0, nav_refine_corridor_mid_tiles->integer ) : 12`
	const int32_t midTileThreshold = nav_refine_corridor_mid_tiles ? std::max( 0, nav_refine_corridor_mid_tiles->integer ) : 12;
	//! <Q2RTXP>: WID: Was `std::max( 0, nav_refine_corridor_far_tiles->integer ) : 24`
	const int32_t farTileThreshold = nav_refine_corridor_far_tiles ? std::max( 0, nav_refine_corridor_far_tiles->integer ) : 24;
	//! <Q2RTXP>: WID: Was `std::clamp( nav_refine_corridor_radius_far->integer, 0, 8 ) : 1;`
	const int32_t nearRadius = nav_refine_corridor_radius_near ? std::clamp( nav_refine_corridor_radius_near->integer, 0, 8 ) : 1;
	//! <Q2RTXP>: WID: Was `std::clamp( nav_refine_corridor_radius_far->integer, 0, 8 ) : 2;`
	const int32_t midRadius = nav_refine_corridor_radius_mid ? std::clamp( nav_refine_corridor_radius_mid->integer, 0, 8 ) : 2;
	//! <Q2RTXP>: WID: Was `std::clamp( nav_refine_corridor_radius_far->integer, 0, 8 ) : 3;`
	const int32_t farRadius = nav_refine_corridor_radius_far ? std::clamp( nav_refine_corridor_radius_far->integer, 0, 8 ) : 3;
	if ( routeTileCount >= (size_t)farTileThreshold ) {
		return farRadius;
	}
	if ( routeTileCount >= (size_t)midTileThreshold ) {
		return midRadius;
	}

   // Default to the tuned near-route detour budget for short routes.
	return nearRadius;
}

/**
 *	@brief    Build a widened fine-search corridor from the coarse cluster route.
 *	@param    exactRoute      Exact tile sequence returned by the cluster graph.
 *	@param    outBufferedRoute    [out] Buffered tile allow-list used by fine A* .
 *	@return   Chebyshev corridor radius in tiles used to buffer the fine-search allow-list.
 **/
static int32_t Nav_AStar_BuildBufferedTileRouteFilter( const std::vector<nav_tile_cluster_key_t> &exactRoute,
	std::vector<nav_tile_cluster_key_t> *outBufferedRoute ) {
	if ( !outBufferedRoute ) return 0;
	outBufferedRoute->clear();
	if ( exactRoute.empty() ) return 0;
	const int32_t routeBufferRadius = Nav_AStar_ComputeRouteBufferRadius( exactRoute.size() );
	const int32_t corridorWidth = ( routeBufferRadius * 2 ) + 1;
	outBufferedRoute->reserve( exactRoute.size() * corridorWidth * corridorWidth );
	for ( const nav_tile_cluster_key_t &routeKey : exactRoute ) {
		Nav_AStar_AppendUniqueRouteTile( *outBufferedRoute, routeKey );
		for ( int32_t dy = -routeBufferRadius; dy <= routeBufferRadius; dy++ ) {
			for ( int32_t dx = -routeBufferRadius; dx <= routeBufferRadius; dx++ ) {
				if ( dx == 0 && dy == 0 ) continue;
				const nav_tile_cluster_key_t widenedKey = { .tile_x = routeKey.tile_x + dx, .tile_y = routeKey.tile_y + dy };
				Nav_AStar_AppendUniqueRouteTile( *outBufferedRoute, widenedKey );
			}
		}
	}
	return routeBufferRadius;
}



static inline double Nav_AStar_Heuristic( const Vector3 &a, const Vector3 &b ) {
	const Vector3 delta = QM_Vector3Subtract( b, a );
	return sqrtf( ( delta[ 0 ]* delta[ 0 ] ) + ( delta[ 1 ]* delta[ 1 ] ) + ( delta[ 2 ]* delta[ 2 ] ) );
}

/** 
*	@brief	Compute floor-division for signed global cell coordinates.
*	@param	value	Signed dividend.
*	@param	divisor	Positive divisor.
*	@return	Mathematical floor of `value / divisor`.
*	@note	Used to map global cell coordinates back into tile coordinates without
* 		boundary-origin collapse on negative coordinates.
**/
static inline int32_t Nav_AStar_FloorDiv( const int32_t value, const int32_t divisor ) {
	/** 
	*	Sanity check: require a positive divisor.
	**/
	if ( divisor <= 0 ) {
		return 0;
	}

	/** 
	*	Use integer fast-path for non-negative values.
	**/
	if ( value >= 0 ) {
		return value / divisor;
	}

	/** 
	*	Negative coordinates need explicit floor semantics rather than truncation.
	**/
	return -( ( -value + divisor - 1 ) / divisor );
}

/** 
*	@brief	Compute a positive modulo for signed global cell coordinates.
*	@param	value	Signed input value.
*	@param	modulus	Positive modulus.
*	@return	Value wrapped into `[0, modulus)`.
**/
static inline int32_t Nav_AStar_PosMod( const int32_t value, const int32_t modulus ) {
	/** 
	*	Sanity check: require a positive modulus.
	**/
	if ( modulus <= 0 ) {
		return 0;
	}

	/** 
	*	Normalize the remainder into a positive tile-local range.
	**/
	const int32_t remainder = value % modulus;
	return ( remainder < 0 ) ? ( remainder + modulus ) : remainder;
}

/** 
*	@brief	Check whether a sparse tile cell is marked present.
*	@param	tile	Tile to query.
*	@param	cell_index	Cell index inside the tile.
*	@return	True if the sparse presence bit is set.
**/
static inline const bool Nav_AStar_CellPresent( const nav_tile_t * tile, const int32_t cell_index ) {
	/** 
	*	Sanity checks: require tile storage and a non-negative cell index.
	**/
	if ( !tile || !tile->presence_bits || cell_index < 0 ) {
		return false;
	}

	/** 
	*	Read the sparse presence bit for this tile cell.
	**/
	const int32_t word_index = cell_index >> 5;
	const int32_t bit_index = cell_index & 31;
	return ( tile->presence_bits[ word_index ] & ( 1u << bit_index ) ) != 0;
}

/** 
*	@brief	Compute a canonical node world position from tile, cell, and layer storage.
*	@param	mesh	Navigation mesh.
*	@param	tile	Canonical world tile.
*	@param	cell_index	Cell index inside the tile.
*	@param	layer	Selected layer inside the cell.
*	@return	World-space center position for the node.
**/
static inline Vector3 Nav_AStar_NodeWorldPosition( const nav_mesh_t * mesh, const nav_tile_t * tile, const int32_t cell_index, const nav_layer_t * layer ) {
	/** 
	*	Sanity checks: require canonical mesh storage.
	**/
	if ( !mesh || !tile || !layer ) {
		return Vector3{};
	}

	/** 
	*	Derive tile-local XY coordinates and convert them back to world-space cell centers.
	**/
	const double tile_world_size = ( double )mesh->tile_size* mesh->cell_size_xy;
	const int32_t cell_x = cell_index % mesh->tile_size;
	const int32_t cell_y = cell_index / mesh->tile_size;
	const double world_x = ( ( double )tile->tile_x* tile_world_size ) + ( ( double )cell_x + 0.5 )* mesh->cell_size_xy;
	const double world_y = ( ( double )tile->tile_y* tile_world_size ) + ( ( double )cell_y + 0.5 )* mesh->cell_size_xy;
	const double world_z = ( double )layer->z_quantized* mesh->z_quant;
	return Vector3{ ( float )world_x, ( float )world_y, ( float )world_z };
}

/** 
*	@brief	Select the best neighboring layer for adjacent-cell expansion.
*	@param	mesh	Navigation mesh.
*	@param	cell	Target XY cell containing one or more candidate layers.
*	@param	current_node	Currently expanded canonical node.
*	@param	offset_dir	Neighbor offset in cell/layer units.
*	@param	policy	Traversal policy controlling step and jump allowances.
*	@param	out_layer_index	[out] Selected target layer index.
*	@return	True if a compatible neighboring layer was found.
*	@note	This is stricter than generic `Nav_SelectLayerIndex()` because adjacent-cell
* 			expansion should prefer the walk surface that continues from the current node,
* 			not just the closest-by-Z layer inside the target cell.
**/
static bool Nav_AStar_SelectNeighborLayerIndex( const nav_mesh_t * mesh, const nav_xy_cell_t * cell, const nav_node_ref_t &current_node,
	const Vector3 &offset_dir, const svg_nav_path_policy_t * policy, int32_t * out_layer_index ) {
	/** 
	*	Sanity checks: require mesh, cell storage, and output storage.
	**/
	if ( !mesh || !cell || cell->num_layers <= 0 || !cell->layers || !out_layer_index ) {
		return false;
	}

	/** 
	*	Build the same desired Z used for the current neighbor probe.
	**/
	const double current_z = current_node.worldPosition[ 2 ];
	const double desired_z = current_z + ( ( double )offset_dir.z* mesh->z_quant );

	/** 
	*	Rebuild the local Z tolerance used by strict neighbor expansion.
	**/
	const double z_tolerance = mesh->max_step + ( mesh->z_quant* 0.5 );

	/** 
	*	Derive the local step/jump thresholds used only for candidate ordering.
	*	    Do not hard-reject layers solely from raw Z delta here because the authoritative
	*	    segmented step validator already handles ramps and stairs later in expansion.
	**/
	const double step_limit = ( policy && policy->max_step_height > 0.0 ) ? ( double )policy->max_step_height : ( double )mesh->max_step;
	const double jump_limit = ( policy && policy->allow_small_obstruction_jump ) ? ( double )policy->max_obstruction_jump_height : 0.0;
	const double max_allowed_rise = step_limit + jump_limit;

	/** 
	*	  Score candidates so we prefer step-compatible walk surfaces before jump-only or extreme layers.
	*	  Use two passes:
	*	      1) Strict desired-Z matching for normal adjacent-cell continuation.
	*	      2) Conservative fallback using only traversal compatibility when the desired-Z gate is too strict.
	*	  @note	This selector intentionally ranks large rises last instead of hard-rejecting them.
	* 		The later `Nav_CanTraverseStep_ExplicitBBox()` call is the authoritative gate for
	* 		ram p s, stairs, and segmented multi-cell climbs.
	**/
	int32_t best_index = -1;
	for ( int32_t pass_index = 0; pass_index < 2 && best_index < 0; pass_index++ ) {
		/** 
		*	Reset pass-local scoring state before evaluating candidates.
		**/
		int32_t pass_best_band = std::numeric_limits<int32_t>::max();
		double pass_best_up_cost = std::numeric_limits<double>::infinity();
		double pass_best_desired_delta = std::numeric_limits<double>::infinity();
		double pass_best_drop_cost = std::numeric_limits<double>::infinity();

		// The first pass keeps the normal desired-Z gate; the fallback pass relaxes it.
		const bool allow_outside_desired_tolerance = ( pass_index != 0 );

		// Iterate over all layers in the target cell and keep the most traversal-compatible candidate.
		for ( int32_t i = 0; i < cell->num_layers; i++ ) {
			// Convert the quantized layer height into world-space units.
			const double layer_z = ( double )cell->layers[ i ].z_quantized* mesh->z_quant;
			// Compare the candidate against the desired probe height.
			const double desired_delta = std::fabs( layer_z - desired_z );

			// During the strict pass, reject layers that are too far away from the intended probe height.
			if ( !allow_outside_desired_tolerance && desired_delta > z_tolerance ) {
				continue;
			}

            // Measure vertical delta from the current node to this candidate layer.
			const double dz_from_current = layer_z - current_z;

         // Prefer ordinary step-compatible or flat/downward neighbors over jump-only or extreme-rise neighbors.
			const int32_t candidate_band = ( dz_from_current > max_allowed_rise ) ? 2 : ( ( dz_from_current > step_limit ) ? 1 : 0 );
			// Smaller upward rise is better because it stays closer to the current walk surface.
			const double up_cost = std::max( 0.0, dz_from_current );
			// Smaller drops are preferred when upward cost and desired delta tie.
			const double drop_cost = std::max( 0.0, -dz_from_current );

			// Keep the lexicographically best candidate for adjacent-cell continuation.
			if ( best_index < 0
				|| candidate_band < pass_best_band
				|| ( candidate_band == pass_best_band && up_cost < pass_best_up_cost )
				|| ( candidate_band == pass_best_band && up_cost == pass_best_up_cost && desired_delta < pass_best_desired_delta )
				|| ( candidate_band == pass_best_band && up_cost == pass_best_up_cost && desired_delta == pass_best_desired_delta && drop_cost < pass_best_drop_cost ) ) {
				best_index = i;
				pass_best_band = candidate_band;
				pass_best_up_cost = up_cost;
				pass_best_desired_delta = desired_delta;
				pass_best_drop_cost = drop_cost;
			}
		}

		/** 
		*	If the strict pass found nothing, clear the provisional index so the fallback pass can retry.
		**/
		if ( pass_index == 0 && best_index >= 0 && pass_best_desired_delta > z_tolerance ) {
			best_index = -1;
		}
	}

	/** 
	*	Commit the best compatible neighboring layer, if any.
	**/
	if ( best_index < 0 ) {
		return false;
	}

	* out_layer_index = best_index;
	return true;
}

/** 
*	@brief	Resolve an exact neighboring canonical node from tile and cell offsets.
*	@param	mesh	Navigation mesh.
*	@param	current_node	Currently expanded canonical node.
*	@param	offset_dir	Neighbor offset in cell and layer units.
*	@param	out_node	[out] Resolved neighboring node.
*	@return	True if a concrete neighboring node was found.
*	@note	This bypasses world-position lookup for neighbor expansion so boundary-origin
* 			cell centers cannot collapse back onto the current node through coordinate rounding.
**/
static bool Nav_AStar_TryResolveNeighborNodeExact( nav_a_star_state_t * state, const nav_mesh_t * mesh, const nav_node_ref_t &current_node, const Vector3 &offset_dir,
	const svg_nav_path_policy_t * policy, nav_node_ref_t * out_node ) {
	/** 
	*	Sanity checks: require mesh, output storage, and a valid current tile reference.
	**/
	if ( !mesh || !out_node ) {
		return false;
	}
  /** 
	*	Vertical same-cell probes intentionally resolve back onto the current node.
	*	    This keeps alias accounting stable and avoids treating pure layer probes as missing neighbors.
	**/
	if ( offset_dir.x == 0.0f && offset_dir.y == 0.0f ) {
		* out_node = current_node;
		return true;
	}

	// Reject immediately when the current node does not reference a valid canonical tile.
	if ( current_node.key.tile_index < 0 || current_node.key.tile_index >= ( int32_t )mesh->world_tiles.size() ) {
       if ( state ) {
			state->no_node_invalid_current_tile_count++;
		}
		return false;
	}

	/** 
	*	Resolve the current node's global cell-grid coordinates.
	**/
	const nav_tile_t &current_tile = mesh->world_tiles[ current_node.key.tile_index ];
	const int32_t current_local_x = current_node.key.cell_index % mesh->tile_size;
	const int32_t current_local_y = current_node.key.cell_index / mesh->tile_size;
	const int32_t current_global_x = ( current_tile.tile_x* mesh->tile_size ) + current_local_x;
	const int32_t current_global_y = ( current_tile.tile_y* mesh->tile_size ) + current_local_y;

	/** 
	*	Apply the requested cell offsets in canonical grid space.
	**/
	const int32_t offset_cell_x = ( int32_t )offset_dir.x;
	const int32_t offset_cell_y = ( int32_t )offset_dir.y;
	const int32_t target_global_x = current_global_x + offset_cell_x;
	const int32_t target_global_y = current_global_y + offset_cell_y;

	/** 
	*	Map the global cell coordinates back to tile-local addressing.
	**/
	const int32_t target_tile_x = Nav_AStar_FloorDiv( target_global_x, mesh->tile_size );
	const int32_t target_tile_y = Nav_AStar_FloorDiv( target_global_y, mesh->tile_size );
	const int32_t target_local_x = Nav_AStar_PosMod( target_global_x, mesh->tile_size );
	const int32_t target_local_y = Nav_AStar_PosMod( target_global_y, mesh->tile_size );
	const int32_t target_cell_index = ( target_local_y* mesh->tile_size ) + target_local_x;

	/** 
	*	Resolve the canonical world tile for the target cell.
	**/
	const nav_world_tile_key_t target_tile_key = { .tile_x = target_tile_x, .tile_y = target_tile_y };
	auto tile_it = mesh->world_tile_id_of.find( target_tile_key );
	if ( tile_it == mesh->world_tile_id_of.end() ) {
       if ( state ) {
			state->no_node_target_tile_count++;
		}
		return false;
	}

	const int32_t target_tile_index = tile_it->second;
	if ( target_tile_index < 0 || target_tile_index >= ( int32_t )mesh->world_tiles.size() ) {
       if ( state ) {
			state->no_node_target_tile_count++;
		}
		return false;
	}
	const nav_tile_t * target_tile = &mesh->world_tiles[ target_tile_index ];

	/** 
	*	Reject missing sparse cells before touching cell storage.
	**/
	if ( !Nav_AStar_CellPresent( target_tile, target_cell_index ) ) {
       if ( state ) {
			state->no_node_presence_count++;
		}
		return false;
	}

	/** 
	*	Resolve the cell storage and choose the closest acceptable layer by desired Z.
	**/
	auto cellsView = SVG_Nav_Tile_GetCells( mesh, target_tile );
	const nav_xy_cell_t * cellsPtr = cellsView.first;
	const int32_t cellsCount = cellsView.second;
	if ( !cellsPtr || target_cell_index < 0 || target_cell_index >= cellsCount ) {
       if ( state ) {
			state->no_node_cell_view_count++;
		}
		return false;
	}

	const nav_xy_cell_t * target_cell = &cellsPtr[ target_cell_index ];
	if ( !target_cell || target_cell->num_layers <= 0 || !target_cell->layers ) {
       if ( state ) {
			state->no_node_cell_view_count++;
		}
		return false;
	}

	int32_t target_layer_index = -1;
   if ( !Nav_AStar_SelectNeighborLayerIndex( mesh, target_cell, current_node, offset_dir, policy, &target_layer_index ) ) {
		if ( state ) {
			state->no_node_layer_select_count++;
		}
		return false;
	}

	/** 
	*	Populate the resolved canonical node reference.
	**/
	const nav_layer_t * target_layer = &target_cell->layers[ target_layer_index ];
	out_node->key.leaf_index = current_node.key.leaf_index;
	out_node->key.tile_index = target_tile_index;
	out_node->key.cell_index = target_cell_index;
	out_node->key.layer_index = target_layer_index;
	out_node->worldPosition = Nav_AStar_NodeWorldPosition( mesh, target_tile, target_cell_index, target_layer );
	return true;
}

/** 
*	@brief	Count how many adjacent probes resolve to distinct neighboring nodes for a candidate layer.
*	@param	mesh	Navigation mesh.
*	@param	candidate_node	Candidate node to evaluate.
*	@param	policy	Traversal policy used by exact neighbor resolution.
*	@return	Number of non-alias neighboring nodes resolved around this candidate.
*	@note	This is used only for same-cell start/goal rescue so we can prefer a locally connected
* 			layer when the first generic lookup lands on an isolated variant.
**/
static int32_t Nav_AStar_CountResolvableNeighborVariants( const nav_mesh_t * mesh, const nav_node_ref_t &candidate_node, const svg_nav_path_policy_t * policy ) {
	/** 
	*	Sanity checks: require a mesh and a valid canonical tile reference.
	**/
	if ( !mesh || candidate_node.key.tile_index < 0 || candidate_node.key.tile_index >= ( int32_t )mesh->world_tiles.size() ) {
		return 0;
	}

	/** 
	*	Probe all configured offsets and count only distinct, non-alias neighbors.
	**/
	int32_t resolved_count = 0;
	for ( const Vector3 &offset_dir : s_nav_neighbor_offsets ) {
       // Skip offsets that do not represent a distinct async cell hop.
		if ( !Nav_AStar_ShouldProbeNeighborOffset( offset_dir ) ) {
			continue;
		}

		nav_node_ref_t neighbor_node = {};
		if ( !Nav_AStar_TryResolveNeighborNodeExact( nullptr, mesh, candidate_node, offset_dir, policy, &neighbor_node ) ) {
			continue;
		}
		if ( neighbor_node.key == candidate_node.key ) {
			continue;
		}

		resolved_count++;
	}

	return resolved_count;
}

/** 
*	@brief	Try to rescue a node onto a better-connected same-cell layer variant.
*	@param	mesh	Navigation mesh.
*	@param	seed_node	Initially resolved node in the target XY cell.
*	@param	policy	Traversal policy used for exact neighbor compatibility checks.
*	@param	out_node	[out] Best connected same-cell layer variant.
*	@return	True when evaluation succeeded and `out_node` was populated.
*	@note	This keeps the original XY cell fixed while scanning only its layer stack. It is intended
* 			to repair start or goal node picks that landed on an isolated layer inside a multi-layer cell.
**/
bool Nav_AStar_TrySelectConnectedSameCellLayer( const nav_mesh_t * mesh, const nav_node_ref_t &seed_node, const svg_nav_path_policy_t * policy, nav_node_ref_t * out_node ) {
	/** 
	*	Sanity checks: require mesh, output storage, and a valid canonical tile reference.
	**/
	if ( !mesh || !out_node ) {
		return false;
	}
	if ( seed_node.key.tile_index < 0 || seed_node.key.tile_index >= ( int32_t )mesh->world_tiles.size() ) {
		return false;
	}

	/** 
	*	Resolve the owning tile and cell for the seed node.
	**/
	const nav_tile_t * tile = &mesh->world_tiles[ seed_node.key.tile_index ];
	auto cellsView = SVG_Nav_Tile_GetCells( mesh, tile );
	const nav_xy_cell_t * cellsPtr = cellsView.first;
	const int32_t cellsCount = cellsView.second;
	if ( !cellsPtr || seed_node.key.cell_index < 0 || seed_node.key.cell_index >= cellsCount ) {
		return false;
	}

	const nav_xy_cell_t * cell = &cellsPtr[ seed_node.key.cell_index ];
	if ( !cell || cell->num_layers <= 0 || !cell->layers ) {
		return false;
	}

	/** 
	*	Start from the original resolved node and score each same-cell layer variant.
	**/
	nav_node_ref_t best_node = seed_node;
	int32_t best_neighbor_count = Nav_AStar_CountResolvableNeighborVariants( mesh, seed_node, policy );
	double best_z_delta = 0.0;

	// Evaluate every same-cell layer and keep the variant with the strongest local connectivity.
	for ( int32_t layer_index = 0; layer_index < cell->num_layers; layer_index++ ) {
		nav_node_ref_t candidate_node = seed_node;
		candidate_node.key.layer_index = layer_index;
		candidate_node.worldPosition = Nav_AStar_NodeWorldPosition( mesh, tile, seed_node.key.cell_index, &cell->layers[ layer_index ] );

		// Count how many non-alias adjacent neighbors this candidate can resolve.
		const int32_t neighbor_count = Nav_AStar_CountResolvableNeighborVariants( mesh, candidate_node, policy );
		// Measure how far this candidate is from the originally resolved world-space Z.
		const double z_delta = std::fabs( candidate_node.worldPosition.z - seed_node.worldPosition.z );

		// Prefer candidates with more resolvable neighbors; break ties by staying closest to the original Z.
		if ( neighbor_count > best_neighbor_count
			|| ( neighbor_count == best_neighbor_count && z_delta < best_z_delta ) ) {
			best_node = candidate_node;
			best_neighbor_count = neighbor_count;
			best_z_delta = z_delta;
		}
	}

	/** 
	*	Commit the best same-cell layer variant.
	**/
	* out_node = best_node;
	return true;
}

/** 
*	@brief	Apply policy-driven fast rejection to persisted edge metadata.
*	@param	state	Optional A*	state used to record rejection diagnostics.
*	@param	policy	Traversal policy controlling hazards and walk-off permissions.
*	@param	edge_bits	Persisted edge metadata to inspect.
*	@param	record_reject_reason	If true, increment the async rejection counter when the policy rejects the edge.
*	@return	True when the edge should be rejected before deeper neighbor resolution.
*	@note	This centralizes the hazard and walk-off policy gate so the async hot path does not duplicate the same checks in multiple places.
**/
static bool Nav_AStar_ShouldRejectEdgeByPolicy( nav_a_star_state_t * state, const svg_nav_path_policy_t * policy, const uint32_t edge_bits, const bool record_reject_reason ) {
	/** 
	*	Empty metadata never causes a policy rejection by itself.
	**/
	if ( edge_bits == NAV_EDGE_FEATURE_NONE ) {
		return false;
	}

	/** 
	*	Reject edges entering hazards forbidden by the active traversal policy.
	**/
	if ( policy && policy->forbid_water && ( edge_bits & NAV_EDGE_FEATURE_ENTERS_WATER ) != 0 ) {
		if ( record_reject_reason && state ) {
			state->edge_reject_reason_counts[(int)nav_edge_reject_reason_t::Other]++;
		}
		return true;
	}
	if ( policy && policy->forbid_lava && ( edge_bits & NAV_EDGE_FEATURE_ENTERS_LAVA ) != 0 ) {
		if ( record_reject_reason && state ) {
			state->edge_reject_reason_counts[(int)nav_edge_reject_reason_t::Other]++;
		}
		return true;
	}
	if ( policy && policy->forbid_slime && ( edge_bits & NAV_EDGE_FEATURE_ENTERS_SLIME ) != 0 ) {
		if ( record_reject_reason && state ) {
			state->edge_reject_reason_counts[(int)nav_edge_reject_reason_t::Other]++;
		}
		return true;
	}

	/** 
	*	Reject walk-off edges unless the active traversal policy explicitly allows them.
	**/
	if ( policy && !policy->allow_optional_walk_off && ( edge_bits & NAV_EDGE_FEATURE_OPTIONAL_WALK_OFF ) != 0 ) {
		if ( record_reject_reason && state ) {
			state->edge_reject_reason_counts[(int)nav_edge_reject_reason_t::Other]++;
		}
		return true;
	}
	if ( policy && !policy->allow_forbidden_walk_off && ( edge_bits & NAV_EDGE_FEATURE_FORBIDDEN_WALK_OFF ) != 0 ) {
		if ( record_reject_reason && state ) {
			state->edge_reject_reason_counts[(int)nav_edge_reject_reason_t::Other]++;
		}
		return true;
	}

	/** 
	*	Treat pure hard-wall blocks as policy rejections before any deeper neighbor work.
	**/
	if ( ( edge_bits & NAV_EDGE_FEATURE_HARD_WALL_BLOCKED ) != 0 && ( edge_bits & NAV_EDGE_FEATURE_PASSABLE ) == 0 ) {
		if ( record_reject_reason && state ) {
			state->edge_reject_reason_counts[(int)nav_edge_reject_reason_t::Other]++;
		}
		return true;
	}

	return false;
}

/**
*	@brief	Determine whether a long-hop probe is redundant because the full one-cell chain already exists.
*	@param	mesh	Navigation mesh.
*	@param	current_node	Currently expanded canonical node.
*	@param	offset_dir	Candidate neighbor offset.
*	@param	policy	Traversal policy used for exact neighbor resolution.
*	@return	True when the long-hop probe should be skipped in favor of the intermediate chain.
*	@note	This is intentionally conservative: it only prunes long hops when the full one-cell chain, including the final destination, resolves through explicit one-cell steps without relying on fallback behavior.
**/
static bool Nav_AStar_ShouldSkipPassedThroughProbe( const nav_mesh_t *mesh, const nav_node_ref_t &current_node, const Vector3 &offset_dir, const svg_nav_path_policy_t *policy ) {
	/**
	*	Sanity checks: require mesh storage before evaluating long-hop redundancy.
	**/
	if ( !mesh ) {
		return false;
	}

	/**
	*	Only long-hop XY probes are candidates for pass-through pruning.
	**/
	const int32_t offset_cell_x = ( int32_t )offset_dir.x;
	const int32_t offset_cell_y = ( int32_t )offset_dir.y;
	const int32_t hop_count = std::max( std::abs( offset_cell_x ), std::abs( offset_cell_y ) );
	if ( hop_count <= 1 ) {
		return false;
	}

	/**
	*	Step one cell at a time along the long-hop ray and require the full chained destination to resolve cleanly.
	**/
	const int32_t unit_step_x = ( offset_cell_x > 0 ) ? 1 : ( ( offset_cell_x < 0 ) ? -1 : 0 );
	const int32_t unit_step_y = ( offset_cell_y > 0 ) ? 1 : ( ( offset_cell_y < 0 ) ? -1 : 0 );
	const Vector3 unit_step = { ( float )unit_step_x, ( float )unit_step_y, 0.0f };
	nav_node_ref_t segment_start = current_node;
	for ( int32_t hop_index = 1; hop_index <= hop_count; hop_index++ ) {
		const uint32_t edge_bits = SVG_Nav_GetEdgeFeatureBitsForOffset( mesh, segment_start, unit_step_x, unit_step_y );
		if ( edge_bits == NAV_EDGE_FEATURE_NONE ) {
			return false;
		}

		/**
		*	Abort pruning when the one-cell chain crosses an edge forbidden by the active policy.
		**/
		if ( Nav_AStar_ShouldRejectEdgeByPolicy( nullptr, policy, edge_bits, false ) ) {
			return false;
		}

		nav_node_ref_t intermediate_node = {};
		if ( !Nav_AStar_TryResolveNeighborNodeExact( nullptr, mesh, segment_start, unit_step, policy, &intermediate_node ) ) {
			return false;
		}
		if ( intermediate_node.key == segment_start.key ) {
			return false;
		}

		segment_start = intermediate_node;
	}

	return true;
}

/** 
*	  @brief    Expand all neighbor nodes for the given `current_index`.
*	  @param    state	A*	state.
*	  @param    current_index	Index of the node to expand.
*	  @note	This function intentionally expands a popped node coherently in one pass.
*			Per-call budgeting is enforced by `Nav_AStar_Step()` between node pops so we do not
*			mark a node closed and then abandon the remainder of its neighbor list without a
*			resume mechanism.
**/
static void Nav_AStar_ExpandNeighbors( nav_a_star_state_t * state, int32_t current_index ) {
	const nav_mesh_t * mesh = state->mesh;
	if ( !mesh ) {
		return;
	}
 // Snapshot the expanding node before any local pushes can reallocate `state->nodes`.
	const nav_search_node_t current_snapshot = state->nodes[ current_index ];
	const Vector3 &agent_mins = state->agent_mins;
	const Vector3 &agent_maxs = state->agent_maxs;
	const svg_nav_path_policy_t * policy = state->policy;
	/**
	*	Capture the expanding node's immutable descriptors before local pushes
	*	may reallocate `state->nodes`, invalidating references.
	**/
   const nav_node_ref_t current_node_ref = current_snapshot.node;
	const Vector3 current_node_world_pos = current_node_ref.worldPosition;
   const double current_g_cost = current_snapshot.g_cost;
	const int32_t current_parent_index = current_snapshot.parent_index;

	// Rate-limit verbose per-neighbor diagnostics so enabling high debug levels
	// does not flood the network/message buffer. This is intentionally coarse
	// and shared across neighbors to reduce log volume when many entities are
	// expanding simultaneously.
	static uint64_t s_last_navexpand_diag_ms = 0;
	const uint64_t navExpandDiagCooldownMs = 200; // ms

	for ( const Vector3 &offset_dir : s_nav_neighbor_offsets ) {
      /** 
		*	Skip offsets that do not represent a distinct async cell hop.
		**/
		if ( !Nav_AStar_ShouldProbeNeighborOffset( offset_dir ) ) {
			continue;
		}

        // Track this neighbor attempt for diagnostics and tuning.
		state->neighbor_try_count++;
		const int32_t edge_step_dx = ( offset_dir.x > 0.0f ) ? 1 : ( ( offset_dir.x < 0.0f ) ? -1 : 0 );
		const int32_t edge_step_dy = ( offset_dir.y > 0.0f ) ? 1 : ( ( offset_dir.y < 0.0f ) ? -1 : 0 );
		const uint32_t sourceEdgeBits = SVG_Nav_GetEdgeFeatureBitsForOffset( mesh, current_node_ref, edge_step_dx, edge_step_dy );
		Vector3 scaledOffset = offset_dir;
		scaledOffset[ 0 ] = scaledOffset[ 0 ] * ( float )mesh->cell_size_xy;
		scaledOffset[ 1 ] = scaledOffset[ 1 ] * ( float )mesh->cell_size_xy;
		scaledOffset[ 2 ] = scaledOffset[ 2 ] * ( float )mesh->z_quant;
		const Vector3 neighbor_origin = QM_Vector3Add( current_node_world_pos, scaledOffset );

		/** 
		*	Apply persisted edge metadata as a cheap policy gate before canonical neighbor lookup or step validation.
		**/
        if ( Nav_AStar_ShouldRejectEdgeByPolicy( state, policy, sourceEdgeBits, true ) ) {
			continue;
		}

        /**
		*	Skip redundant long-hop probes when every passed-through intermediate cell is already predictably reachable.
		**/
		if ( ( !policy || policy->enable_pass_through_pruning ) && Nav_AStar_ShouldSkipPassedThroughProbe( mesh, current_node_ref, offset_dir, policy ) ) {
           state->pass_through_prune_count++;
			continue;
		}

     /** 
		*	Skip nodes outside the optional hierarchical tile route discovered by the path process.
		*	    Use the prebuilt lookup table so the hot path does not linearly scan the buffered route.
		**/
		if ( !state->tile_route_lookup.empty() ) {
			const nav_tile_cluster_key_t nk = SVG_Nav_GetTileKeyForPosition( mesh, neighbor_origin );
           if ( state->tile_route_lookup.find( nk ) == state->tile_route_lookup.end() ) {
				// Neighbor rejected due to tile-route filter.
				state->tile_filter_reject_count++;
				// Track reason-specific count and mark as already counted.
				state->edge_reject_reason_counts[(int)nav_edge_reject_reason_t::TileRouteFilter]++;
				if ( s_nav_expand_diag_enable && s_nav_expand_diag_enable->integer != 0 && Nav_PathDiagEnabled() ) {
					const uint64_t nowMs = gi.GetRealSystemTime();
					const int cooldown = s_nav_expand_diag_cooldown_ms ? s_nav_expand_diag_cooldown_ms->integer : 200;
					if ( nowMs - s_last_navexpand_diag_ms >= ( uint64_t )cooldown ) {
						s_last_navexpand_diag_ms = nowMs;
						gi.dprintf( "[DEBUG][NavPath][Diag] Neighbor rejected by tile-route filter at pos=(%.1f %.1f %.1f)\n",
							neighbor_origin.x, neighbor_origin.y, neighbor_origin.z );
					}
				}
				continue;
			}
		}

		/** 
		*	Resolve exact local neighbors using canonical tile-cell addressing.
		*		This avoids boundary-origin collapse from world-space re-lookup while keeping
		*		strict layer selection for adjacent-cell expansion.
		**/
		nav_node_ref_t neighbor_node = {};
		/** 
		*	No-node handling:
		*		If canonical neighbor addressing cannot resolve a valid sparse target cell/layer,
		*		treat this offset as a true missing neighbor.
		**/
       if ( !Nav_AStar_TryResolveNeighborNodeExact( state, mesh, current_node_ref, offset_dir, policy, &neighbor_node ) ) {
			// No node exists at this neighbor position.
			state->no_node_count++;
			// Track reason-specific count and mark as counted.
			state->edge_reject_reason_counts[(int)nav_edge_reject_reason_t::NoNode]++;
			if ( s_nav_expand_diag_enable && s_nav_expand_diag_enable->integer != 0 && Nav_PathDiagEnabled() ) {
				const uint64_t nowMs = gi.GetRealSystemTime();
				const int cooldown = s_nav_expand_diag_cooldown_ms ? s_nav_expand_diag_cooldown_ms->integer : 200;
				if ( nowMs - s_last_navexpand_diag_ms >= ( uint64_t )cooldown ) {
					s_last_navexpand_diag_ms = nowMs;
					gi.dprintf( "[DEBUG][NavPath][Diag] No node at neighbor pos=(%.1f %.1f %.1f)\n",
						neighbor_origin.x, neighbor_origin.y, neighbor_origin.z );
				}
			}
			continue;
		}

		/** 
		*	Track neighbor aliasing explicitly when canonical neighbor resolution still maps back onto the current node.
		**/
      if ( neighbor_node.key == current_node_ref.key ) {
			state->same_node_alias_count++;
			continue;
		}

        const double neighbor_drop = current_node_world_pos[ 2 ] - neighbor_node.worldPosition[ 2 ];
		if ( policy && neighbor_drop > 0.0 && neighbor_drop > policy->max_drop_height_cap ) {
			// Neighbor rejected due to drop cap.
			state->edge_reject_reason_counts[(int)nav_edge_reject_reason_t::DropCap]++;
			if ( s_nav_expand_diag_enable && s_nav_expand_diag_enable->integer != 0 && Nav_PathDiagEnabled() ) {
				const uint64_t nowMs = gi.GetRealSystemTime();
				const int cooldown = s_nav_expand_diag_cooldown_ms ? s_nav_expand_diag_cooldown_ms->integer : 200;
				if ( nowMs - s_last_navexpand_diag_ms >= ( uint64_t )cooldown ) {
					s_last_navexpand_diag_ms = nowMs;
					gi.dprintf( "[DEBUG][NavPath][Diag] Neighbor rejected by drop cap: drop=%.1f cap=%.1f pos=(%.1f %.1f %.1f)\n",
						neighbor_drop, policy->max_drop_height_cap, neighbor_node.worldPosition.x, neighbor_node.worldPosition.y, neighbor_node.worldPosition.z );
				}
			}
			continue;
		}

		/** 
		*	Cheap vertical prefilter:
		*	    Skip expensive step validation when the resolved neighbor already sits above any plausible
		*	    step or small-obstruction rise allowed by the active traversal policy.
		**/
		const double upwardDelta = neighbor_node.worldPosition[ 2 ] - current_node_world_pos[ 2 ];
		const double stepRiseLimit = ( policy && policy->max_step_height > 0.0 ) ? policy->max_step_height : mesh->max_step;
		const double obstructionRiseLimit = ( policy && policy->allow_small_obstruction_jump )
			? std::max( policy->max_obstruction_jump_height, stepRiseLimit )
			: stepRiseLimit;
		const double verticalPrefilterSlack = std::max( 1.0, ( double )mesh->z_quant );
		const double verticalPrefilterLimit = obstructionRiseLimit + verticalPrefilterSlack;
		if ( upwardDelta > verticalPrefilterLimit ) {
			state->edge_reject_count++;
			state->edge_reject_reason_counts[(int)nav_edge_reject_reason_t::StepTest]++;
			continue;
		}

		/** 
		*	The PMove-derived step test covers slopes, hills, and stairs while respecting both
		*	agent hulls and slope/drop constraints.
		**/
        // Ask the step validator for a detailed rejection reason when available.
		nav_edge_reject_reason_t stepReason = nav_edge_reject_reason_t::None;
       if ( !Nav_CanTraverseStep_ExplicitBBox_NodeRefs( mesh, current_node_ref, neighbor_node, agent_mins, agent_maxs, nullptr, policy, &stepReason ) ) {
			// Edge validation failed for this neighbor.
			state->edge_reject_count++;
			// Increment the returned reason if present, otherwise attribute to StepTest.
			const int reasonIndex = ( stepReason != nav_edge_reject_reason_t::None ) ? ( int )stepReason : ( int )nav_edge_reject_reason_t::StepTest;
			state->edge_reject_reason_counts[ reasonIndex ]++;
			if ( s_nav_expand_diag_enable && s_nav_expand_diag_enable->integer != 0 && Nav_PathDiagEnabled() ) {
				const uint64_t nowMs = gi.GetRealSystemTime();
				const int cooldown = s_nav_expand_diag_cooldown_ms ? s_nav_expand_diag_cooldown_ms->integer : 200;
				if ( nowMs - s_last_navexpand_diag_ms >= ( uint64_t )cooldown ) {
					s_last_navexpand_diag_ms = nowMs;
                   double dz = neighbor_node.worldPosition.z - current_node_world_pos.z;
                    gi.dprintf( "[DEBUG][NavPath][Diag] Edge rejected between (%.1f %.1f %.1f) -> (%.1f %.1f %.1f) dz=%.1f reason=%d (%s)\n",
                       current_node_world_pos.x, current_node_world_pos.y, current_node_world_pos.z,
						neighbor_node.worldPosition.x, neighbor_node.worldPosition.y, neighbor_node.worldPosition.z,
						dz, ( int )stepReason, Nav_EdgeRejectReasonToString( stepReason ) );
				}
			}
			continue;
		}

      if ( neighbor_node.worldPosition.z != current_node_world_pos.z ) {
			state->saw_vertical_neighbor = true;
		}

     const double baseDist = Nav_AStar_Heuristic( current_node_world_pos, neighbor_node.worldPosition );

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

     /** 
		*	Resolve neighbor layer flags through the shared safe node-view helper.
		*	    This keeps tile/cell/layer bounds checks centralized for hot-path callers.
		**/
		const nav_layer_t * neighborLayer = SVG_Nav_GetNodeLayerView( mesh, neighbor_node );

      if ( neighborLayer ) {
			if ( ( neighborLayer->traversal_feature_bits & NAV_TRAVERSAL_FEATURE_WATER ) != 0 ) {
				extraCost += 1.0f;
			}
          if ( ( neighborLayer->traversal_feature_bits & NAV_TRAVERSAL_FEATURE_LAVA ) != 0 ) {
				extraCost += 100.0f;
			}
         if ( ( neighborLayer->traversal_feature_bits & NAV_TRAVERSAL_FEATURE_SLIME ) != 0 ) {
				extraCost += 4.0f;
			}

			/** 
			*	Prefer ladders by default when the ladder endpoint reaches the goal height more directly than a long stair route.
			**/
     if ( policy && policy->prefer_ladders && ( sourceEdgeBits & NAV_EDGE_FEATURE_LADDER_PASS ) != 0 ) {
               /** 
				*	Use the explicit ladder endpoint semantics so bias is strongest at the meaningful top/bottom ladder anchors.
				**/
         const bool prefer_upward_ladder = state->goal_node.worldPosition.z >= current_node_world_pos.z;
				const bool is_ladder_top = ( neighborLayer->ladder_endpoint_flags & NAV_LADDER_ENDPOINT_ENDPOINT ) != 0;
				const bool is_ladder_bottom = ( neighborLayer->ladder_endpoint_flags & NAV_LADDER_ENDPOINT_STARTPOINT ) != 0;
				const double ladder_anchor_z = prefer_upward_ladder
					? ( double )neighborLayer->ladder_end_z_quantized* mesh->z_quant
					: ( double )neighborLayer->ladder_start_z_quantized* mesh->z_quant;
				const bool endpoint_matches_goal_direction = prefer_upward_ladder ? is_ladder_top : is_ladder_bottom;
				const double ladder_goal_delta = std::fabs( ladder_anchor_z - state->goal_node.worldPosition.z );
				const double node_goal_delta = std::fabs( ( double )neighbor_node.worldPosition.z - ( double )state->goal_node.worldPosition.z );
				if ( endpoint_matches_goal_direction && ladder_goal_delta <= policy->ladder_preferred_height_slack && ladder_goal_delta <= node_goal_delta ) {
					extraCost -= policy->ladder_preference_bias;
				}
			}
		}

     const double dz = neighbor_node.worldPosition.z - current_node_world_pos.z;
		const double horizontal = sqrtf( ( neighbor_node.worldPosition.x - current_node_world_pos.x )* ( neighbor_node.worldPosition.x - current_node_world_pos.x ) +
			( neighbor_node.worldPosition.y - current_node_world_pos.y )* ( neighbor_node.worldPosition.y - current_node_world_pos.y ) );
		const double slope = ( horizontal > 0.0001f )
			? ( fabsf( dz ) / horizontal )
			: 0.0f;
		extraCost += slopeWeight* slope* slope* baseDist;

		if ( dz > 0.0 ) {
			const double stepLimit = policy ? policy->max_step_height : ( nav_max_step ? nav_max_step->value : NAV_DEFAULT_STEP_MAX_SIZE );
			if ( dz > stepLimit ) {
				extraCost += ( dz / std::max( stepLimit, 1.0 ) )* 2.0;
			}
		} else {
			const double drop = -dz;
			const double maxDrop = policy ? policy->max_drop_height : ( nav_max_drop_height_cap ? nav_max_drop_height_cap->value : NAV_DEFAULT_MAX_DROP_HEIGHT_CAP );
			if ( drop > 0.0 ) {
				extraCost += dropWeight* ( drop / std::max( maxDrop, 1.0 ) );
			}
		}

		if ( policy && policy->allow_small_obstruction_jump ) {
			const double hj = std::max( 0.0, dz - ( double )policy->max_step_height );
			if ( hj > 0.0f ) {
				 /** 
				* Apply additional jump cost after authoritative step validation succeeds.
				*	    Keep this as a soft preference rather than a second hard veto so segmented
				*	    stair/ramp traversal accepted by `Nav_CanTraverseStep_ExplicitBBox()` can still expand.
				**/
				if ( hj > ( double )policy->max_obstruction_jump_height ) {
				   // Penalize climbs that exceed the preferred small-obstruction jump envelope.
					const double overflow = hj - ( double )policy->max_obstruction_jump_height;
					extraCost += jumpBase + jumpHeightWeight + overflow* 0.5;
				} else {
					// Apply the configured cost penalty for a small but still allowed obstruction jump.
					const double jumpCost = jumpBase + jumpHeightWeight* ( hj / ( double )policy->max_obstruction_jump_height );
					extraCost += jumpCost;
				}
			}
		}

		bool hasLOS = false;
		{
			cm_trace_t losTr = gi.trace( &neighbor_node.worldPosition, &mesh->agent_mins, &mesh->agent_maxs, &state->goal_node.worldPosition, nullptr, CM_CONTENTMASK_SOLID );
			if ( losTr.fraction >= 1.0f && !losTr.startsolid && !losTr.allsolid ) {
				hasLOS = true;
			}
		}
		if ( hasLOS ) {
            const double vz = std::fabs( neighbor_node.worldPosition.z - current_node_world_pos.z );
			const double maxVz = nav_cost_los_max_vertical_delta ? nav_cost_los_max_vertical_delta->value : NAV_DEFAULT_LOS_MAX_VERTICAL_DELTA;
			if ( vz > maxVz ) {
				hasLOS = false;
			}
		}
		if ( hasLOS ) {
			extraCost -= losWeight;
		}

       /** 
		*	Apply sparse dynamic occupancy after static traversal scoring so policy can choose soft-cost steering or hard blocking.
		**/
		if ( Nav_AStar_ApplyDynamicOccupancyPolicy( state, mesh, neighbor_node, policy, dynamicWeight, &extraCost ) ) {
			continue;
		}

		if ( state->pathProcess ) {
			const QMTime now = level.time;
			const QMTime lastFail = state->pathProcess->last_failure_time;
			if ( lastFail > 0_ms ) {
				const double tauMs = failureTauMs > 0.0f ? failureTauMs : 5000.0f;
				const double dt = ( double )( ( now - lastFail ).Milliseconds() );
				const double factor = std::exp( -dt / tauMs );
				extraCost += failureWeight* ( double )factor;

				const Vector3 toLastFail = QM_Vector3Subtract( neighbor_node.worldPosition, state->pathProcess->last_failure_pos );
				const double distToFail = QM_Vector3LengthDP( toLastFail );
				const double failPosRadius = 64.0f;
				if ( distToFail <= failPosRadius ) {
					const double posFactor = 1.0f - ( distToFail / failPosRadius );
					const double sigPenalty = failureWeight* ( double )( 0.75f* posFactor );
					extraCost += sigPenalty;
				}

                const double neighborYaw = QM_Vector3ToYaw( QM_Vector3Subtract( neighbor_node.worldPosition, current_node_world_pos ) );
				double yawDelta = fabsf( neighborYaw - state->pathProcess->last_failure_yaw );
				yawDelta = fmodf( yawDelta + 180.0f, 360.0f ) - 180.0f;
				const double yawThresh = 45.0f;
				if ( fabsf( yawDelta ) <= yawThresh ) {
					const double yawFactor = 1.0f - ( fabsf( yawDelta ) / yawThresh );
					const double yawPenalty = failureWeight* ( double )( 0.5f* yawFactor );
					extraCost += yawPenalty;
				}
			}
		} else if ( policy ) {
			extraCost += failureWeight* ( double )policy->fail_backoff_max_pow* 0.01f;
		}

      // Re-read the parent from the vector by index instead of keeping a stale node reference alive.
		if ( current_parent_index >= 0 ) {
			const nav_search_node_t &parentNode = state->nodes[ current_parent_index ];
			Vector3 fromDir = QM_Vector3NormalizeDP( QM_Vector3Subtract( current_node_world_pos, parentNode.node.worldPosition ) );
			Vector3 toDir = QM_Vector3NormalizeDP( QM_Vector3Subtract( neighbor_node.worldPosition, current_node_world_pos ) );
			const double dot = QM_Vector3DotProductDP( fromDir, toDir );
			const double clamped = QM_Clamp( dot, -1.0, 1.0 );
			const double ang = acosf( clamped );
			extraCost += turnWeight* ( ang / ( double )M_PI );
		}

		const double tentative_g = current_g_cost + std::max( baseDist* w_dist* minCostPerUnit, 0.0 ) + extraCost;

		auto lookup_it = state->node_lookup.find( neighbor_node.key );
		if ( lookup_it == state->node_lookup.end() ) {
			nav_search_node_t neighbor_search = {
				.node = neighbor_node,
				.g_cost = tentative_g,
				.f_cost = tentative_g + Nav_AStar_Heuristic( neighbor_node.worldPosition, state->goal_node.worldPosition ),
				.parent_index = current_index,
				.closed = false
			};

			state->nodes.push_back( neighbor_search );
			const int32_t neighbor_index = ( int32_t )state->nodes.size() - 1;
			state->open_list.push_back( neighbor_index );
            state->open_push_count++;
			state->node_lookup.emplace( neighbor_node.key, neighbor_index );
			continue;
		}

		const int32_t neighbor_index = lookup_it->second;
		nav_search_node_t &neighbor_search = state->nodes[ neighbor_index ];
		if ( neighbor_search.closed ) {
           /** 
			*	Track closed-duplicate resolutions separately from hard rejects.
			**/
			state->closed_duplicate_count++;
			continue;
		}

		if ( tentative_g < neighbor_search.g_cost ) {
			neighbor_search.g_cost = tentative_g;
			neighbor_search.f_cost = tentative_g + Nav_AStar_Heuristic( neighbor_node.worldPosition, state->goal_node.worldPosition );
			neighbor_search.parent_index = current_index;
		}
	}
}

/** 
*	@brief	Initialize an A* state.
*	@note	Sets up internal containers and applies the configured per-call search budget.
**/
bool Nav_AStar_Init( nav_a_star_state_t * state, const nav_mesh_t * mesh, const nav_node_ref_t &start_node,
	const nav_node_ref_t &goal_node, const Vector3 &agent_mins, const Vector3 &agent_maxs,
	const svg_nav_path_policy_t * policy, const nav_refine_corridor_t * refineCorridor,
	const svg_nav_path_process_t * pathProcess ) {
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
	state->refine_corridor = refineCorridor ? * refineCorridor : nav_refine_corridor_t{};

	/** 
	*	Emit an opt-in diagnostic when both endpoints land in the same XY cell but on different layers.
	*	    This is a strong signal that a caller supplied an inter-floor or mid-air goal that required Z projection.
	**/
	if ( s_nav_expand_diag_enable && s_nav_expand_diag_enable->integer != 0 && Nav_PathDiagEnabled()
		&& start_node.key.tile_index == goal_node.key.tile_index
		&& start_node.key.cell_index == goal_node.key.cell_index
		&& start_node.key.layer_index != goal_node.key.layer_index ) {
		gi.dprintf( "[NavAsync][Init] same-cell multi-layer endpoints start_z=%.1f goal_z=%.1f start_layer=%d goal_layer=%d\n",
			start_node.worldPosition.z,
			goal_node.worldPosition.z,
			start_node.key.layer_index,
			goal_node.key.layer_index );
	}

	// Lazy-register expansion diagnostic cvars.
	if ( !s_nav_expand_diag_enable ) {
		s_nav_expand_diag_enable = gi.cvar( "nav_expand_diag_enable", "1", 0 );
		s_nav_expand_diag_cooldown_ms = gi.cvar( "nav_expand_diag_cooldown_ms", "200", 0 );
	}

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
	state->node_lookup.reserve( reserveNodes* 2 );
	state->edge_validation_cache.reserve( reserveNodes* 4 );

   if ( state->refine_corridor.HasExactTileRoute() ) {
		/** 
		*	Widen the explicit refinement corridor into a buffered fine-search tile allow-list.
		*	    Exact-route filtering is too restrictive for longer searches because fine A*	sometimes must step
		*	    one tile off the coarse spine to realize a valid local traversal around geometry.
		**/
		state->corridor_buffer_radius = Nav_AStar_BuildBufferedTileRouteFilter( state->refine_corridor.exact_tile_route, &state->tile_route_storage );
		state->corridor_buffered_tile_count = ( int32_t )state->tile_route_storage.size();
		state->tileRouteFilter = &state->tile_route_storage;
        state->tile_route_lookup.clear();
		state->tile_route_lookup.reserve( state->tile_route_storage.size() );

		/** 
		*	Mirror the buffered route into a constant-time membership table so neighbor expansion
		*	does not linearly scan the full corridor for every tile probe.
		**/
		for ( const nav_tile_cluster_key_t &routeKey : state->tile_route_storage ) {
			state->tile_route_lookup.insert( routeKey );
		}
	} else {
       state->corridor_buffer_radius = 0;
		state->corridor_buffered_tile_count = 0;
		state->tileRouteFilter = nullptr;
		state->tile_route_lookup.clear();
	}
	// Debug assertion: ensure non-owning pointer either null or points into our storage.
	Q_assert( state->tileRouteFilter == nullptr || state->tileRouteFilter == &state->tile_route_storage );

	nav_search_node_t start_search = {
		.node = start_node,
		.g_cost = 0.0,
		.f_cost = Nav_AStar_Heuristic( start_node.worldPosition, goal_node.worldPosition ),
		.parent_index = -1,
		.closed = false
	};

	state->nodes.push_back( start_search );
	state->open_list.push_back( 0 );
    state->open_push_count = 1;
    state->occupancy_overlay_hit_count = 0;
	state->occupancy_soft_cost_hit_count = 0;
	state->occupancy_block_reject_count = 0;
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
*	  @brief	Advance the A*	search by up to `expansions` node pops.
*	  @param	state	A*	state to step.
*	  @param	expansions	Maximum node expansions to perform this call.
*	  @return	Current search status after stepping.
*	  @note	This function enforces both an expansions budget and a strict per-call time budget.
**/
nav_a_star_status_t Nav_AStar_Step( nav_a_star_state_t * state, int32_t expansions ) {
	if ( !state || state->status != nav_a_star_status_t::Running || expansions <= 0 ) {
		return state ? state->status : nav_a_star_status_t::Failed;
	}
	if ( !state->mesh ) {
		state->status = nav_a_star_status_t::Failed;
		return state->status;
	}

	/** 
	*	Reset per-call budget state before this incremental slice begins.
	*	    `hit_time_budget` is only meant to describe the current `Nav_AStar_Step()` invocation.
	*	    If it persists across frames, later step calls will keep bailing early even when the
	*	    new per-call budget has not actually been exhausted yet.
	**/
	state->hit_time_budget = false;

	/** 
	*	Expansion budget guard:
	*	    - `expansions` limits work per call to keep frame time predictable.
	*	    - `NAV_ASTAR_MAX_NODES` prevents unbounded node growth on pathological meshes.
	*	    - `search_budget_ms` throttles per-call time while allowing incremental continuation.
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

		const int32_t current_index = * best_it;
		* best_it = state->open_list.back();
		state->open_list.pop_back();
		state->open_pop_count++;

		nav_search_node_t &current = state->nodes[ current_index ];
		current.closed = true;
		state->node_expand_count++;

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

/** 
*	  @brief	Finalize the A*	search and extract the resulting path if successful.
*	  @return	Returns true if a valid path was found and extracted, false otherwise.
**/
const bool Nav_AStar_Finalize( nav_a_star_state_t * state, std::vector<Vector3> * out_points ) {
	if ( !state || !out_points ) {
		return false;
	}
	if ( state->status != nav_a_star_status_t::Completed ) {
		return false;
	}

	/** 
	*	Walk the parent chain from the solved node to recover the waypoint list.
	**/
	out_points->clear();
	if ( state->start_node.key == state->goal_node.key ) {
		out_points->push_back( state->start_node.worldPosition );
		out_points->push_back( state->goal_node.worldPosition );
		return true;
	}

	if ( state->goal_index < 0 || state->goal_index >= ( int32_t )state->nodes.size() ) {
		return false;
	}

	std::vector<Vector3> reversed;
	reversed.reserve( 64 );
	int32_t trace_index = state->goal_index;
	while ( trace_index >= 0 ) {
		reversed.push_back( state->nodes[ trace_index ].node.worldPosition );
		trace_index = state->nodes[ trace_index ].parent_index;
	}

	out_points->assign( reversed.rbegin(), reversed.rend() );
	return true;
}

/** 
*	  @brief	Reconstruct an ordered waypoint list from one explored search-node index.
*	  @param	state		A*	state owning the explored node storage.
*	  @param	node_index	Index of the frontier/search node to walk backward from.
*	  @param	out_points	[out] Ordered waypoint list reaching the chosen node.
*	  @return	True when the parent chain produced a usable path with at least two points.
**/
static const bool Nav_AStar_BuildPathFromNodeIndex( const nav_a_star_state_t * state, const int32_t node_index, std::vector<Vector3> * out_points ) {
	/** 
	*	  Sanity checks: require state storage, output storage, and a valid node index.
	**/
	if ( !state || !out_points || node_index < 0 || node_index >= ( int32_t )state->nodes.size() ) {
		return false;
	}

	/** 
	*	  Walk the parent chain from the chosen node back toward the start node.
	**/
	out_points->clear();
	std::vector<Vector3> reversed;
	reversed.reserve( 64 );
	int32_t trace_index = node_index;
	while ( trace_index >= 0 ) {
		reversed.push_back( state->nodes[ trace_index ].node.worldPosition );
		trace_index = state->nodes[ trace_index ].parent_index;
	}

	/** 
	*	  Require at least a start and one frontier point before exposing a partial path.
	**/
	if ( reversed.size() < 2 ) {
		return false;
	}

	/** 
	*	  Return the ordered start->frontier chain to the caller.
	**/
	out_points->assign( reversed.rbegin(), reversed.rend() );
	return true;
}

/** 
*	  @brief	Build the best currently-known provisional path from an in-flight A*	search.
*	  @param	state		Running or partially explored A*	state.
*	  @param	out_points	[out] Ordered waypoint list reaching the best explored frontier node.
*	  @return	True when a usable provisional path was reconstructed.
**/
const bool Nav_AStar_BuildBestPartialPath( const nav_a_star_state_t * state, std::vector<Vector3> * out_points ) {
	/** 
	*	  Sanity checks: require a live state, output storage, and explored nodes.
	**/
	if ( !state || !out_points || state->nodes.empty() ) {
		return false;
	}

	/** 
	*	  Reuse the finalized path when the search already reached the goal.
	**/
	if ( state->status == nav_a_star_status_t::Completed ) {
		return Nav_AStar_BuildPathFromNodeIndex( state, state->goal_index, out_points );
	}

	/** 
	*	  Choose the explored node that currently appears closest to the goal while preferring real progress away from the start.
	**/
	int32_t best_node_index = -1;
	double best_goal_distance_sqr = std::numeric_limits<double>::infinity();
	double best_f_cost = std::numeric_limits<double>::infinity();
	double best_g_cost = -std::numeric_limits<double>::infinity();
	for ( int32_t node_index = 0; node_index < ( int32_t )state->nodes.size(); node_index++ ) {
		const nav_search_node_t &search_node = state->nodes[ node_index ];

		// Skip the pure start node when we have not yet discovered any outward progress.
		if ( search_node.parent_index < 0 ) {
			continue;
		}

		// Score candidates by direct distance to the goal, then by A*	total cost, then by depth/progress.
		const double goal_distance_sqr = QM_Vector3DistanceSqr( search_node.node.worldPosition, state->goal_node.worldPosition );
		const bool better_distance = goal_distance_sqr + 0.001 < best_goal_distance_sqr;
		const bool equal_distance = std::fabs( goal_distance_sqr - best_goal_distance_sqr ) <= 0.001;
		const bool better_f_cost = search_node.f_cost + 0.001 < best_f_cost;
		const bool equal_f_cost = std::fabs( search_node.f_cost - best_f_cost ) <= 0.001;
		const bool better_progress = search_node.g_cost > best_g_cost + 0.001;
		if ( best_node_index < 0
			|| better_distance
			|| ( equal_distance && better_f_cost )
			|| ( equal_distance && equal_f_cost && better_progress ) ) {
			best_node_index = node_index;
			best_goal_distance_sqr = goal_distance_sqr;
			best_f_cost = search_node.f_cost;
			best_g_cost = search_node.g_cost;
		}
	}

	/** 
	*	  Reconstruct the provisional start->frontier chain when a candidate was found.
	**/
	if ( best_node_index < 0 ) {
		return false;
	}

	return Nav_AStar_BuildPathFromNodeIndex( state, best_node_index, out_points );
}

void Nav_AStar_Reset( nav_a_star_state_t * state ) {
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
	state->edge_validation_cache.clear();
	state->tile_route_storage.clear();
	state->tile_route_lookup.clear();
	state->tileRouteFilter = nullptr;
	state->policy = nullptr;
	state->pathProcess = nullptr;
	state->goal_index = -1;
	state->best_f_cost_seen = std::numeric_limits<double>::infinity();
	state->stagnation_count = 0;
	state->hit_stagnation_limit = false;
	state->hit_time_budget = false;
	state->saw_vertical_neighbor = false;
	state->node_expand_count = 0;
	state->open_push_count = 0;
	state->open_pop_count = 0;
	state->neighbor_try_count = 0;
	state->no_node_count = 0;
    state->no_node_invalid_current_tile_count = 0;
	state->no_node_target_tile_count = 0;
	state->no_node_presence_count = 0;
	state->no_node_cell_view_count = 0;
	state->no_node_layer_select_count = 0;
	state->tile_filter_reject_count = 0;
	state->edge_reject_count = 0;
	state->same_node_alias_count = 0;
	state->closed_duplicate_count = 0;
	state->pass_through_prune_count = 0;

	// Clear per-reason rejection counts.
	state->edge_reject_reason_counts.fill( 0 );
	state->step_start_ms = 0;
}
