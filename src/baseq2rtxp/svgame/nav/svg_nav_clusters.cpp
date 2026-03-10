/** * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 
* 
* 
*  SVGame: Navigation Cluster Graph Helpers - Implementation
* 
* 
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"

#include "svgame/nav/svg_nav.h"
#include "svgame/nav/svg_nav_clusters.h"


/** 
*  @brief    Cluster routing configuration CVars exposed for tuning.
**/
//! Toggles whether the cluster router uses feature-weighted traversal costs.
cvar_t *nav_cluster_route_weighted = nullptr;
//! Configured extra cost applied when traversing stair tiles within cluster routing.
cvar_t *nav_cluster_cost_stair = nullptr;
//! Configured extra cost applied when traversing water tiles within cluster routing.
cvar_t *nav_cluster_cost_water = nullptr;
//! Configured extra cost applied when traversing lava tiles within cluster routing.
cvar_t *nav_cluster_cost_lava = nullptr;
//! Configured extra cost applied when traversing slime tiles within cluster routing.
cvar_t *nav_cluster_cost_slime = nullptr;
//! When enabled, forbids stair tiles from being queued by the cluster router.
cvar_t *nav_cluster_forbid_stair = nullptr;
//! When enabled, forbids water tiles from being queued by the cluster router.
cvar_t *nav_cluster_forbid_water = nullptr;
//! When enabled, forbids lava tiles from being queued by the cluster router.
cvar_t *nav_cluster_forbid_lava = nullptr;
//! When enabled, forbids slime tiles from being queued by the cluster router.
cvar_t *nav_cluster_forbid_slime = nullptr;
//! Master switch that turns the cluster graph routing subsystem on or off.
cvar_t *nav_cluster_enable = nullptr;
//! Enables drawing of the computed cluster route for debugging purposes.
cvar_t *nav_cluster_debug_draw = nullptr;
//! Enables visualization of the cluster graph topology for debugging.
cvar_t *nav_cluster_debug_draw_graph = nullptr;


/**
*	Cached last tile - route for debug drawing and diagnostics.
**/
//! Cache of the most recent cluster tile sequence computed by the routing subsystem.
static std::vector<nav_tile_cluster_key_t> s_nav_cluster_last_route;
//! Expiration timestamp guarding the debug route visualization cache.
static QMTime s_nav_cluster_last_route_expire = 0_ms;
//! Last computed route cost used for diagnostics and logging.
static double s_nav_cluster_last_route_cost = 0.0;
//! Hop count of the most recent cached cluster route.
static int32_t s_nav_cluster_last_route_hops = 0;

//! Tile-cluster graph storage (nodes + neighbor indices) reconstructed from the nav mesh.
static nav_tile_cluster_graph_t s_nav_tile_cluster_graph = {};

//! Goal-height slack used when deciding whether a tile's ladder anchors align with the coarse route objective.
static constexpr double NAV_CLUSTER_LADDER_GOAL_SLACK = 96.0;
//! Maximum weighted-route bonus granted to ladder tiles whose explicit anchors align with the route's vertical objective.
static constexpr double NAV_CLUSTER_LADDER_INTENT_BONUS = 0.75;

static inline nav_tile_cluster_key_t Nav_ClusterKey( const int32_t tile_x, const int32_t tile_y ) {
	nav_tile_cluster_key_t k;
	k.tile_x = tile_x;
	k.tile_y = tile_y;
	return k;
}

static void Nav_ClusterGraph_Clear( void ) {
	s_nav_tile_cluster_graph.idOf.clear();
	s_nav_tile_cluster_graph.nodes.clear();
}

/** 
*  @brief		Detect coarse cluster flags for a tile by inspecting its cells/layers.
*  @param		mesh    Navigation mesh pointer (used for tile_size and z_quant).
*  @param		tile    Tile to inspect for relevant cluster flags.
*  @return	Bitmask of nav tile cluster flags (stair/water/lava/slime).
*  @note		Uses safe accessors to avoid dereferencing possibly-null sparse
* 				tile storage. Early-outs when mesh or cell storage is missing.
**/
static inline uint8_t Nav_ClusterGraph_DetectTileFlags( const nav_mesh_t * mesh, const nav_tile_t &tile ) {
	uint8_t flags = NAV_TILE_CLUSTER_FLAG_NONE;
	// Early out when mesh is missing.
	if ( !mesh ) {
		return flags;
	}

	/** 
	*  Prefer the persisted coarse summaries when available so the cluster branch consumes
	*  the same metadata already derived from the tile's fine node and edge data.
	**/
	const uint32_t summary_bits = tile.traversal_summary_bits | tile.edge_summary_bits;
	if ( summary_bits != NAV_TILE_SUMMARY_NONE ) {
		if ( ( summary_bits & NAV_TILE_SUMMARY_STAIR ) != 0 ) {
			flags |= NAV_TILE_CLUSTER_FLAG_STAIR;
		}
		if ( ( summary_bits & NAV_TILE_SUMMARY_WATER ) != 0 ) {
			flags |= NAV_TILE_CLUSTER_FLAG_WATER;
		}
		if ( ( summary_bits & NAV_TILE_SUMMARY_LAVA ) != 0 ) {
			flags |= NAV_TILE_CLUSTER_FLAG_LAVA;
		}
		if ( ( summary_bits & NAV_TILE_SUMMARY_SLIME ) != 0 ) {
			flags |= NAV_TILE_CLUSTER_FLAG_SLIME;
		}
		if ( ( summary_bits & NAV_TILE_SUMMARY_LADDER ) != 0 ) {
			flags |= NAV_TILE_CLUSTER_FLAG_LADDER;
		}
		if ( ( summary_bits & NAV_TILE_SUMMARY_WALK_OFF ) != 0 ) {
			flags |= NAV_TILE_CLUSTER_FLAG_WALKOFF;
		}
		return flags;
	}

	constexpr uint8_t relevantMask = NAV_TILE_CLUSTER_FLAG_STAIR |
		NAV_TILE_CLUSTER_FLAG_WATER |
		NAV_TILE_CLUSTER_FLAG_LAVA |
        NAV_TILE_CLUSTER_FLAG_SLIME |
		NAV_TILE_CLUSTER_FLAG_LADDER;

	// Use safe accessor to obtain pointer/count for the tile's cells so we
	// avoid direct dereference of internal C-style arrays which may be null.
	auto cellsView = SVG_Nav_Tile_GetCells( mesh, const_cast<nav_tile_t * >( &tile ) );
	const nav_xy_cell_t * cellsPtr = cellsView.first;
	const int32_t actualCells = cellsView.second;
	if ( !cellsPtr || actualCells <= 0 ) {
		return flags;
	}

	// Iterate cells using the accessor-provided count.
	for ( int32_t ci = 0; ci < actualCells; ++ci ) {
		const nav_xy_cell_t &cell = cellsPtr[ ci ];

		// Quick sanity: skip empty cells.
		if ( cell.num_layers <= 0 || !cell.layers ) {
			continue;
		}

		// Multiple layers implies stairs/vertical adjacency inside the cell.
		if ( cell.num_layers > 1 ) {
			flags |= NAV_TILE_CLUSTER_FLAG_STAIR;
		}

		// Use safe accessor for layers to avoid raw pointer indexing elsewhere.
		auto layersView = SVG_Nav_Cell_GetLayers( &cell );
		const nav_layer_t * layersPtr = layersView.first;
		const int32_t layerCount = layersView.second;
		if ( !layersPtr || layerCount <= 0 ) {
			continue;
		}

		for ( int32_t li = 0; li < layerCount; ++li ) {
			const nav_layer_t &layer = layersPtr[ li ];
          if ( ( layer.traversal_feature_bits & NAV_TRAVERSAL_FEATURE_LADDER ) != 0 ) {
				flags |= NAV_TILE_CLUSTER_FLAG_LADDER;
			}
			if ( ( layer.flags & NAV_FLAG_WATER ) != 0 ) {
				flags |= NAV_TILE_CLUSTER_FLAG_WATER;
			}
			if ( ( layer.flags & NAV_FLAG_LAVA ) != 0 ) {
				flags |= NAV_TILE_CLUSTER_FLAG_LAVA;
			}
			if ( ( layer.flags & NAV_FLAG_SLIME ) != 0 ) {
				flags |= NAV_TILE_CLUSTER_FLAG_SLIME;
			}
		}

		// If we've found all relevant flags, no need to scan further.
		if ( ( flags & relevantMask ) == relevantMask ) {
			break;
		}
	}

	return flags;
}

static nav_tile_cluster_key_t Nav_GetTileKeyForPosition( const nav_mesh_t * mesh, const Vector3 &pos ) {
	const double tileWorldSize = Nav_TileWorldSize( mesh );
	return Nav_ClusterKey( Nav_WorldToTileCoord( pos[ 0 ], tileWorldSize ), Nav_WorldToTileCoord( pos[ 1 ], tileWorldSize ) );
}

/** 
*  @brief	Resolve the coarse ladder anchor range for a tile from fine endpoint metadata.
*  @param	mesh	Navigation mesh containing the tile.
*  @param	key	Tile key to inspect.
*  @param	out_bottom_z	[out] Lowest explicit ladder startpoint height found in the tile.
*  @param	out_top_z	[out] Highest explicit ladder endpoint height found in the tile.
*  @return	True when both bottom and top ladder anchors were found.
*  @note	This derives coarse ladder intent from the same explicit start/top semantics used by the fine async traversal layer.
**/
static const bool Nav_ClusterGraph_TryGetTileLadderAnchorRange( const nav_mesh_t * mesh, const nav_tile_cluster_key_t &key, double * out_bottom_z, double * out_top_z ) {
	/** 
	*  Sanity checks: require mesh storage and output storage.
	**/
	if ( !mesh || !out_bottom_z || !out_top_z ) {
		return false;
	}

	/** 
	*  Resolve the canonical world tile owning this coarse tile key.
	**/
	const nav_world_tile_key_t worldTileKey = { .tile_x = key.tile_x, .tile_y = key.tile_y };
	auto tileIt = mesh->world_tile_id_of.find( worldTileKey );
	if ( tileIt == mesh->world_tile_id_of.end() ) {
		return false;
	}

	const int32_t tileIndex = tileIt->second;
	if ( tileIndex < 0 || tileIndex >= ( int32_t )mesh->world_tiles.size() ) {
		return false;
	}

	const nav_tile_t * tile = &mesh->world_tiles[ tileIndex ];
	auto cellsView = SVG_Nav_Tile_GetCells( mesh, tile );
	const nav_xy_cell_t * cellsPtr = cellsView.first;
	const int32_t cellsCount = cellsView.second;
	if ( !cellsPtr || cellsCount <= 0 ) {
		return false;
	}

	/** 
	*  Scan every layer and keep the outermost explicit ladder anchors.
	**/
	double bestBottomZ = std::numeric_limits<double>::infinity();
	double bestTopZ = -std::numeric_limits<double>::infinity();
	bool foundBottom = false;
	bool foundTop = false;
	for ( int32_t cellIndex = 0; cellIndex < cellsCount; cellIndex++ ) {
		const nav_xy_cell_t &cell = cellsPtr[ cellIndex ];
		auto layersView = SVG_Nav_Cell_GetLayers( &cell );
		const nav_layer_t * layersPtr = layersView.first;
		const int32_t layerCount = layersView.second;
		if ( !layersPtr || layerCount <= 0 ) {
			continue;
		}

		// Inspect each layer for explicit ladder bottom/top endpoint semantics.
		for ( int32_t layerIndex = 0; layerIndex < layerCount; layerIndex++ ) {
			const nav_layer_t &layer = layersPtr[ layerIndex ];
			if ( ( layer.ladder_endpoint_flags & NAV_LADDER_ENDPOINT_STARTPOINT ) != 0 ) {
				bestBottomZ = std::min( bestBottomZ, ( double )layer.ladder_start_z_quantized* mesh->z_quant );
				foundBottom = true;
			}
			if ( ( layer.ladder_endpoint_flags & NAV_LADDER_ENDPOINT_ENDPOINT ) != 0 ) {
				bestTopZ = std::max( bestTopZ, ( double )layer.ladder_end_z_quantized* mesh->z_quant );
				foundTop = true;
			}
		}
	}

	/** 
	*  Commit the anchor range only when both explicit ladder endpoints exist.
	**/
	if ( !foundBottom || !foundTop ) {
		return false;
	}

	* out_bottom_z = bestBottomZ;
	* out_top_z = bestTopZ;
	return true;
}

static void Nav_ClusterGraph_BuildFromMesh_Runtime( const nav_mesh_t * mesh ) {
	Nav_ClusterGraph_Clear();
	if ( !mesh || mesh->num_leafs <= 0 || !mesh->leaf_data ) {
		return;
	}

	auto DetectTileClusterFlags = [mesh]( const nav_tile_t &tile ) -> uint8_t {
		return Nav_ClusterGraph_DetectTileFlags( mesh, tile );
		};

	for ( const nav_tile_t &tile : mesh->world_tiles ) {
		const nav_tile_cluster_key_t key = Nav_ClusterKey( tile.tile_x, tile.tile_y );
		if ( s_nav_tile_cluster_graph.idOf.find( key ) != s_nav_tile_cluster_graph.idOf.end() ) {
			continue;
		}

		const int32_t id = ( int32_t )s_nav_tile_cluster_graph.nodes.size();
		s_nav_tile_cluster_graph.idOf.emplace( key, id );
		s_nav_tile_cluster_graph.nodes.push_back( nav_tile_cluster_node_t{ .key = key, .flags = DetectTileClusterFlags( tile ) } );
	}

	static constexpr int32_t dx[ 4 ] = { 1, -1, 0, 0 };
	static constexpr int32_t dy[ 4 ] = { 0, 0, 1, -1 };
	const int32_t n = ( int32_t )s_nav_tile_cluster_graph.nodes.size();
	for ( int32_t id = 0; id < n; id++ ) {
		nav_tile_cluster_node_t &node = s_nav_tile_cluster_graph.nodes[ id ];
		const nav_tile_cluster_key_t k = node.key;
		for ( int32_t i = 0; i < 4; i++ ) {
			const nav_tile_cluster_key_t nk = Nav_ClusterKey( k.tile_x + dx[ i ], k.tile_y + dy[ i ] );
			auto it = s_nav_tile_cluster_graph.idOf.find( nk );
			node.neighbors[ i ] = ( it == s_nav_tile_cluster_graph.idOf.end() ) ? -1 : it->second;
		}
	}
}

static const bool Nav_ClusterGraph_FindRoute( const nav_mesh_t * mesh, const Vector3 &startPos, const Vector3 &goalPos,
	std::vector<nav_tile_cluster_key_t> &outRoute ) {
	outRoute.clear();
	if ( nav_cluster_enable && nav_cluster_enable->integer == 0 ) {
		return false;
	}
	if ( !mesh || s_nav_tile_cluster_graph.nodes.empty() ) {
		return false;
	}

	const nav_tile_cluster_key_t startKey = Nav_GetTileKeyForPosition( mesh, startPos );
	const nav_tile_cluster_key_t goalKey = Nav_GetTileKeyForPosition( mesh, goalPos );

	auto itS = s_nav_tile_cluster_graph.idOf.find( startKey );
	auto itG = s_nav_tile_cluster_graph.idOf.find( goalKey );
	if ( itS == s_nav_tile_cluster_graph.idOf.end() || itG == s_nav_tile_cluster_graph.idOf.end() ) {
		return false;
	}

	const int32_t startId = itS->second;
	const int32_t goalId = itG->second;
	if ( startId == goalId ) {
		outRoute.push_back( startKey );
		return true;
	}

	const int32_t n = ( int32_t )s_nav_tile_cluster_graph.nodes.size();
	std::vector<int32_t> parent( ( size_t )n, -1 );

	auto IsForbidden = [startId, goalId]( const int32_t id ) -> bool {
		if ( id == startId || id == goalId ) {
			return false;
		}

		const nav_tile_cluster_node_t &node = s_nav_tile_cluster_graph.nodes[ id ];
		const uint8_t f = node.flags;
		if ( nav_cluster_forbid_stair && nav_cluster_forbid_stair->integer != 0 && ( f & NAV_TILE_CLUSTER_FLAG_STAIR ) != 0 ) {
			return true;
		}
		if ( nav_cluster_forbid_water && nav_cluster_forbid_water->integer != 0 && ( f & NAV_TILE_CLUSTER_FLAG_WATER ) != 0 ) {
			return true;
		}
		if ( nav_cluster_forbid_lava && nav_cluster_forbid_lava->integer != 0 && ( f & NAV_TILE_CLUSTER_FLAG_LAVA ) != 0 ) {
			return true;
		}
		if ( nav_cluster_forbid_slime && nav_cluster_forbid_slime->integer != 0 && ( f & NAV_TILE_CLUSTER_FLAG_SLIME ) != 0 ) {
			return true;
		}
		return false;
		};

    auto TilePenalty = [mesh, &startPos, &goalPos]( const nav_tile_cluster_node_t &node ) -> double {
		const uint8_t flags = node.flags;
		double cost = 0.0;
		if ( nav_cluster_cost_stair && ( flags & NAV_TILE_CLUSTER_FLAG_STAIR ) != 0 ) {
			cost += nav_cluster_cost_stair->value;
		}
		if ( nav_cluster_cost_water && ( flags & NAV_TILE_CLUSTER_FLAG_WATER ) != 0 ) {
			cost += nav_cluster_cost_water->value;
		}
		if ( nav_cluster_cost_lava && ( flags & NAV_TILE_CLUSTER_FLAG_LAVA ) != 0 ) {
			cost += nav_cluster_cost_lava->value;
		}
		if ( nav_cluster_cost_slime && ( flags & NAV_TILE_CLUSTER_FLAG_SLIME ) != 0 ) {
			cost += nav_cluster_cost_slime->value;
		}

		/** 
		*  Favor ladder-capable tiles when their explicit anchors align with a meaningful vertical objective.
		**/
		if ( mesh && ( flags & NAV_TILE_CLUSTER_FLAG_LADDER ) != 0 ) {
			const double routeGoalDeltaZ = std::fabs( ( double )goalPos.z - ( double )startPos.z );
			const double minimumMeaningfulVerticalGoal = mesh->max_step + mesh->z_quant;
			if ( routeGoalDeltaZ > minimumMeaningfulVerticalGoal ) {
				double ladderBottomZ = 0.0;
				double ladderTopZ = 0.0;
				if ( Nav_ClusterGraph_TryGetTileLadderAnchorRange( mesh, node.key, &ladderBottomZ, &ladderTopZ ) ) {
					const bool preferUpwardLadder = goalPos.z >= startPos.z;
					const double ladderAnchorZ = preferUpwardLadder ? ladderTopZ : ladderBottomZ;
					const double anchorGoalDelta = std::fabs( ladderAnchorZ - ( double )goalPos.z );
					if ( anchorGoalDelta <= NAV_CLUSTER_LADDER_GOAL_SLACK ) {
						const double proximityFactor = 1.0 - QM_Clamp( anchorGoalDelta / NAV_CLUSTER_LADDER_GOAL_SLACK, 0.0, 1.0 );
						cost -= NAV_CLUSTER_LADDER_INTENT_BONUS* proximityFactor;
					}
				}
			}
		}

		return cost;
		};

	const bool useWeighted = ( nav_cluster_route_weighted && nav_cluster_route_weighted->integer != 0 );
	const double INF = std::numeric_limits<double>::infinity();
	std::vector<double> dist( ( size_t )n, INF );
	std::vector<uint8_t> closed( ( size_t )n, 0 );

	std::vector<int32_t> open;
	open.reserve( 128 );

	dist[ startId ] = 0.0;
	open.push_back( startId );

	while ( !open.empty() ) {
		int32_t bestOpenIdx = 0;
		double bestCost = dist[ open[ 0 ] ];
		for ( int32_t i = 1; i < ( int32_t )open.size(); i++ ) {
			const double c = dist[ open[ i ] ];
			if ( c < bestCost ) {
				bestCost = c;
				bestOpenIdx = i;
			}
		}

		const int32_t cur = open[ bestOpenIdx ];
		open[ bestOpenIdx ] = open.back();
		open.pop_back();

		if ( closed[ cur ] != 0 ) {
			continue;
		}
		closed[ cur ] = 1;

		if ( cur == goalId ) {
			break;
		}

		for ( int32_t ni = 0; ni < 4; ni++ ) {
			const int32_t nb = s_nav_tile_cluster_graph.nodes[ cur ].neighbors[ ni ];
			if ( nb < 0 ) {
				continue;
			}
			if ( closed[ nb ] != 0 ) {
				continue;
			}
			if ( IsForbidden( nb ) ) {
				continue;
			}

			double stepCost = 1.0;
			if ( useWeighted ) {
              stepCost = std::max( 0.1, stepCost + TilePenalty( s_nav_tile_cluster_graph.nodes[ nb ] ) );
			}

			const double nd = dist[ cur ] + stepCost;
			if ( nd < dist[ nb ] ) {
				dist[ nb ] = nd;
				parent[ nb ] = cur;
				open.push_back( nb );
			}
		}
	}

	if ( parent[ goalId ] < 0 ) {
		return false;
	}

	s_nav_cluster_last_route.clear();
	s_nav_cluster_last_route_cost = dist[ goalId ];
	s_nav_cluster_last_route_hops = 0;

	for ( int32_t v = goalId; v >= 0; v = parent[ v ] ) {
		s_nav_cluster_last_route.push_back( s_nav_tile_cluster_graph.nodes[ v ].key );
		s_nav_cluster_last_route_hops++;
		if ( v == startId ) {
			break;
		}
	}
	if ( !s_nav_cluster_last_route.empty() && s_nav_cluster_last_route.back() == Nav_GetTileKeyForPosition( mesh, startPos ) ) {
		s_nav_cluster_last_route.assign( s_nav_cluster_last_route.rbegin(), s_nav_cluster_last_route.rend() );
		s_nav_cluster_last_route_expire = level.time + 1500_ms;
		if ( nav_cluster_debug_draw && nav_cluster_debug_draw->integer != 0 ) {
			gi.dprintf( "[NavCluster] Route found: %d hops, cost %.2f (weighted=%d)\n",
				s_nav_cluster_last_route_hops,
				s_nav_cluster_last_route_cost,
				( nav_cluster_route_weighted && nav_cluster_route_weighted->integer != 0 ) ? 1 : 0 );
		}
	} else {
		s_nav_cluster_last_route.clear();
	}

	std::vector<nav_tile_cluster_key_t> rev;
	for ( int32_t v = goalId; v >= 0; v = parent[ v ] ) {
		rev.push_back( s_nav_tile_cluster_graph.nodes[ v ].key );
		if ( v == startId ) {
			break;
		}
	}
	if ( rev.empty() || !( rev.back() == startKey ) ) {
		return false;
	}

	outRoute.assign( rev.rbegin(), rev.rend() );
	return true;
}

void SVG_Nav_ClusterGraph_BuildFromMesh_World( const nav_mesh_t * mesh ) {
   Nav_ClusterGraph_BuildFromMesh_Runtime( mesh );
}

/** 
*  @brief    Clear cached cluster graph state and adjacency data.
**/
void SVG_Nav_ClusterGraph_Clear( void ) {
	Nav_ClusterGraph_Clear();
}

/** 
*  @brief    Find a tile-route between start and goal positions via the cluster graph.
*  @param    mesh        Navigation mesh.
*  @param    startPos    World-space start.
*  @param    goalPos     World-space goal.
*  @param    outRoute    [out] Tile keys forming a route from start to goal.
*  @return   True if a route was found.
**/
const bool SVG_Nav_ClusterGraph_FindRoute( const nav_mesh_t * mesh, const Vector3 &startPos, const Vector3 &goalPos,
	std::vector<nav_tile_cluster_key_t> &outRoute ) {
	return Nav_ClusterGraph_FindRoute( mesh, startPos, goalPos, outRoute );
}

/** 
* 	@brief    Get the tile key for a given world position.
**/
nav_tile_cluster_key_t SVG_Nav_GetTileKeyForPosition( const nav_mesh_t * mesh, const Vector3 &pos ) {
	return Nav_GetTileKeyForPosition( mesh, pos );
}