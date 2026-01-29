/********************************************************************
*
*
*    SVGame: Navigation Cluster Graph Helpers - Implementation
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"

#include "svgame/nav/svg_nav_clusters.h"


/**
*    @brief    Cluster routing configuration CVars exposed for tuning.
**/
cvar_t *nav_cluster_route_weighted = nullptr;
cvar_t *nav_cluster_cost_stair = nullptr;
cvar_t *nav_cluster_cost_water = nullptr;
cvar_t *nav_cluster_cost_lava = nullptr;
cvar_t *nav_cluster_cost_slime = nullptr;
cvar_t *nav_cluster_forbid_stair = nullptr;
cvar_t *nav_cluster_forbid_water = nullptr;
cvar_t *nav_cluster_forbid_lava = nullptr;
cvar_t *nav_cluster_forbid_slime = nullptr;
cvar_t *nav_cluster_enable = nullptr;
cvar_t *nav_cluster_debug_draw = nullptr;
cvar_t *nav_cluster_debug_draw_graph = nullptr;


// Cached last tile-route for debug drawing and diagnostics.
static std::vector<nav_tile_cluster_key_t> s_nav_cluster_last_route;
static QMTime s_nav_cluster_last_route_expire = 0_ms;
static double s_nav_cluster_last_route_cost = 0.0;
static int32_t s_nav_cluster_last_route_hops = 0;

static nav_tile_cluster_graph_t s_nav_tile_cluster_graph = {};

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

static inline uint8_t Nav_ClusterGraph_DetectTileFlags( const nav_mesh_t *mesh, const nav_tile_t &tile ) {
	uint8_t flags = NAV_TILE_CLUSTER_FLAG_NONE;
	if ( !mesh || !tile.cells ) {
		return flags;
	}

	const int32_t cellsPerTile = mesh->tile_size * mesh->tile_size;
	constexpr uint8_t relevantMask = NAV_TILE_CLUSTER_FLAG_STAIR |
		NAV_TILE_CLUSTER_FLAG_WATER |
		NAV_TILE_CLUSTER_FLAG_LAVA |
		NAV_TILE_CLUSTER_FLAG_SLIME;

	for ( int32_t ci = 0; ci < cellsPerTile; ci++ ) {
		const nav_xy_cell_t &cell = tile.cells[ ci ];
		if ( cell.num_layers <= 0 || !cell.layers ) {
			continue;
		}

		if ( cell.num_layers > 1 ) {
			flags |= NAV_TILE_CLUSTER_FLAG_STAIR;
		}

		for ( int32_t li = 0; li < cell.num_layers; li++ ) {
			const nav_layer_t &layer = cell.layers[ li ];
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

		if ( ( flags & relevantMask ) == relevantMask ) {
			break;
		}
	}

	return flags;
}

static nav_tile_cluster_key_t Nav_GetTileKeyForPosition( const nav_mesh_t *mesh, const Vector3 &pos ) {
	const double tileWorldSize = Nav_TileWorldSize( mesh );
	return Nav_ClusterKey( Nav_WorldToTileCoord( pos[ 0 ], tileWorldSize ), Nav_WorldToTileCoord( pos[ 1 ], tileWorldSize ) );
}

static void Nav_ClusterGraph_BuildFromMesh_World( const nav_mesh_t *mesh ) {
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

static const bool Nav_ClusterGraph_FindRoute( const nav_mesh_t *mesh, const Vector3 &startPos, const Vector3 &goalPos,
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

	auto TilePenalty = []( const uint8_t flags ) -> double {
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
				stepCost += TilePenalty( s_nav_tile_cluster_graph.nodes[ nb ].flags );
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

void SVG_Nav_ClusterGraph_BuildFromMesh_World( const nav_mesh_t *mesh ) {
	Nav_ClusterGraph_BuildFromMesh_World( mesh );
}

/**
*    @brief    Clear cached cluster graph state and adjacency data.
**/
void SVG_Nav_ClusterGraph_Clear( void ) {
	Nav_ClusterGraph_Clear();
}

/**
*    @brief    Find a tile-route between start and goal positions via the cluster graph.
*    @param    mesh        Navigation mesh.
*    @param    startPos    World-space start.
*    @param    goalPos     World-space goal.
*    @param    outRoute    [out] Tile keys forming a route from start to goal.
*    @return   True if a route was found.
**/
const bool SVG_Nav_ClusterGraph_FindRoute( const nav_mesh_t *mesh, const Vector3 &startPos, const Vector3 &goalPos,
	std::vector<nav_tile_cluster_key_t> &outRoute ) {
	return Nav_ClusterGraph_FindRoute( mesh, startPos, goalPos, outRoute );
}

/**
*	@brief    Get the tile key for a given world position.
**/
nav_tile_cluster_key_t SVG_Nav_GetTileKeyForPosition( const nav_mesh_t *mesh, const Vector3 &pos ) {
	return Nav_GetTileKeyForPosition( mesh, pos );
}