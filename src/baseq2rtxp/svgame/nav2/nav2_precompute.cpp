/********************************************************************
*
*
*	ServerGame: Nav2 Coarse Lower-Bound Precomputation - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_precompute.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>


/**
*
*
*	Nav2 Precompute Local Helpers:
*
*
**/
//! Immutable published cache used by coarse worker-safe heuristic queries.
static nav2_precompute_cache_t s_nav2_precompute_published_cache = {};
//! True when a published cache is available for query callers.
static bool s_nav2_precompute_has_published_cache = false;

/**
* @brief	Build a deterministic ordered 64-bit pair key from two signed 32-bit ids.
* @param	a	First id.
* @param	b	Second id.
* @return	Ordered 64-bit key suitable for pair maps.
**/
static inline uint64_t SVG_Nav2_Precompute_MakeOrderedPairKey( const int32_t a, const int32_t b ) {
	const int32_t minId = std::min( a, b );
	const int32_t maxId = std::max( a, b );
	const uint32_t ua = static_cast<uint32_t>( minId );
	const uint32_t ub = static_cast<uint32_t>( maxId );
	return ( static_cast<uint64_t>( ua ) << 32 ) | static_cast<uint64_t>( ub );
}

/**
* @brief	Sanitize one traversal-cost value for precompute graph construction.
* @param	baseCost	Base edge cost.
* @param	topologyPenalty	Topology penalty component.
* @return	Non-negative traversal cost.
**/
static inline double SVG_Nav2_Precompute_SanitizeTraversalCost( const double baseCost, const double topologyPenalty ) {
	const double combined = baseCost + topologyPenalty;
	if ( !std::isfinite( combined ) ) {
		return 0.0;
	}
	return std::max( 0.0, combined );
}

/**
* @brief	Compute one landmark-triangle lower bound between two nodes.
* @param	distanceByLandmark	Per-landmark distance vectors.
* @param	nodeA	Compact node index A.
* @param	nodeB	Compact node index B.
* @return	Admissible lower-bound estimate (>= 0).
**/
static double SVG_Nav2_Precompute_ComputeLandmarkLowerBound( const std::vector<std::vector<double>> &distanceByLandmark,
	const int32_t nodeA, const int32_t nodeB ) {
	/**
	*   Guard invalid indices and empty landmark sets.
	**/
	if ( nodeA < 0 || nodeB < 0 || distanceByLandmark.empty() ) {
		return 0.0;
	}

	double bestLowerBound = 0.0;
	for ( const std::vector<double> &distances : distanceByLandmark ) {
		if ( nodeA >= ( int32_t )distances.size() || nodeB >= ( int32_t )distances.size() ) {
			continue;
		}
		const double da = distances[ ( size_t )nodeA ];
		const double db = distances[ ( size_t )nodeB ];
		if ( !std::isfinite( da ) || !std::isfinite( db ) ) {
			continue;
		}
		bestLowerBound = std::max( bestLowerBound, std::fabs( da - db ) );
	}
	return std::max( 0.0, bestLowerBound );
}

/**
* @brief	Accumulate bounded summary values from a built cache.
* @param	cache	Built cache to summarize.
* @param	outSummary	[out] Summary output.
**/
static void SVG_Nav2_Precompute_BuildSummaryFromCache( const nav2_precompute_cache_t &cache, nav2_precompute_summary_t *outSummary ) {
	if ( !outSummary ) {
		return;
	}

	*outSummary = {};
	outSummary->success = cache.hierarchy_node_count > 0;
	outSummary->hierarchy_node_count = cache.hierarchy_node_count;
	outSummary->hierarchy_edge_count = cache.hierarchy_edge_count;
	outSummary->landmark_count = ( int32_t )cache.landmarks.size();
	outSummary->portal_pair_bound_count = ( int32_t )cache.portal_pair_bounds.size();
	outSummary->cluster_pair_bound_count = ( int32_t )cache.cluster_pair_bounds.size();
	outSummary->region_layer_edge_cost_count = ( int32_t )cache.region_layer_edge_costs.size();
	outSummary->connector_crossing_cost_count = ( int32_t )cache.connector_crossing_costs.size();

	uint64_t approxBytes = 0;
	approxBytes += ( uint64_t )cache.landmarks.size() * sizeof( nav2_precompute_landmark_t );
	approxBytes += ( uint64_t )cache.node_to_landmark_distances.size() * sizeof( double );
	approxBytes += ( uint64_t )cache.portal_pair_bounds.size() * sizeof( nav2_precompute_pair_bound_t );
	approxBytes += ( uint64_t )cache.cluster_pair_bounds.size() * sizeof( nav2_precompute_pair_bound_t );
	approxBytes += ( uint64_t )cache.region_layer_edge_costs.size() * sizeof( nav2_precompute_edge_cost_t );
	approxBytes += ( uint64_t )cache.connector_crossing_costs.size() * sizeof( nav2_precompute_edge_cost_t );
	outSummary->approx_bytes = approxBytes;
}


/**
*
*
*	Nav2 Precompute Public API:
*
*
**/
/**
* @brief	Reset one coarse lower-bound cache to deterministic defaults.
* @param	cache	[in,out] Cache to clear.
**/
void SVG_Nav2_Precompute_ClearCache( nav2_precompute_cache_t *cache ) {
	if ( !cache ) {
		return;
	}
	*cache = {};
}

/**
* @brief	Build coarse lower-bound precompute data from a hierarchy graph.
* @param	hierarchy	Hierarchy graph providing coarse traversal topology.
* @param	policy	Build policy controlling optional sections and memory caps.
* @param	staticNavVersion	Static-nav version stamp bound to this cache.
* @param	outCache	[out] Cache receiving built lower-bound data.
* @param	outSummary	[out] Optional bounded summary.
* @return	True when build produced a usable cache.
**/
const bool SVG_Nav2_Precompute_BuildFromHierarchy( const nav2_hierarchy_graph_t &hierarchy,
	const nav2_precompute_policy_t &policy, const uint32_t staticNavVersion,
	nav2_precompute_cache_t *outCache, nav2_precompute_summary_t *outSummary ) {
	/**
	*   Sanity-check required output storage.
	**/
	if ( !outCache ) {
		return false;
	}
	SVG_Nav2_Precompute_ClearCache( outCache );
	if ( outSummary ) {
		*outSummary = {};
	}

	/**
	*   Require a non-empty hierarchy graph for coarse precompute.
	**/
	if ( hierarchy.nodes.empty() ) {
		return false;
	}

	/**
	*   Copy policy/version metadata and coarse graph sizes.
	**/
	outCache->policy = policy;
	outCache->static_nav_version = staticNavVersion;
	outCache->hierarchy_node_count = ( int32_t )hierarchy.nodes.size();
	outCache->hierarchy_edge_count = ( int32_t )hierarchy.edges.size();
	outCache->feature_flags = NAV2_PRECOMPUTE_FEATURE_NONE;
	if ( policy.mark_serialization_ready ) {
		outCache->feature_flags |= NAV2_PRECOMPUTE_FEATURE_SERIALIZATION_READY;
	}

	/**
	*   Build stable node-index maps for compact distance arrays.
	**/
	std::unordered_map<int32_t, int32_t> compactIndexByNodeId = {};
	compactIndexByNodeId.reserve( hierarchy.nodes.size() );
	for ( size_t nodeIndex = 0; nodeIndex < hierarchy.nodes.size(); nodeIndex++ ) {
		compactIndexByNodeId[ hierarchy.nodes[ nodeIndex ].node_id ] = ( int32_t )nodeIndex;
	}

	/**
	*   Build an undirected shadow adjacency graph for admissible lower-bound computation.
	*   The shadow graph uses non-negative edge weights and intentionally ignores edge direction so
	*   landmark triangle bounds never over-estimate shortest directed path costs.
	**/
	std::vector<std::vector<std::pair<int32_t, double>>> adjacency = {};
	adjacency.resize( hierarchy.nodes.size() );
	for ( const nav2_hierarchy_edge_t &edge : hierarchy.edges ) {
		const auto fromIt = compactIndexByNodeId.find( edge.from_node_id );
		const auto toIt = compactIndexByNodeId.find( edge.to_node_id );
		if ( fromIt == compactIndexByNodeId.end() || toIt == compactIndexByNodeId.end() ) {
			continue;
		}
		const int32_t fromCompact = fromIt->second;
		const int32_t toCompact = toIt->second;
		if ( fromCompact < 0 || toCompact < 0 || fromCompact >= ( int32_t )adjacency.size() || toCompact >= ( int32_t )adjacency.size() ) {
			continue;
		}

		const double edgeCost = SVG_Nav2_Precompute_SanitizeTraversalCost( edge.base_cost, edge.topology_penalty );
		adjacency[ ( size_t )fromCompact ].push_back( { toCompact, edgeCost } );
		adjacency[ ( size_t )toCompact ].push_back( { fromCompact, edgeCost } );
	}

	/**
	*   Local Dijkstra helper used for landmark-distance generation.
	**/
	auto runDijkstraFrom = [&]( const int32_t startCompactIndex ) -> std::vector<double> {
		const size_t nodeCount = hierarchy.nodes.size();
		std::vector<double> distances( nodeCount, std::numeric_limits<double>::infinity() );
		if ( startCompactIndex < 0 || startCompactIndex >= ( int32_t )nodeCount ) {
			return distances;
		}

		using queue_entry_t = std::pair<double, int32_t>;
		std::priority_queue<queue_entry_t, std::vector<queue_entry_t>, std::greater<queue_entry_t>> frontier = {};
		distances[ ( size_t )startCompactIndex ] = 0.0;
		frontier.push( { 0.0, startCompactIndex } );

		while ( !frontier.empty() ) {
			const queue_entry_t current = frontier.top();
			frontier.pop();
			const double currentDist = current.first;
			const int32_t currentIndex = current.second;
			if ( currentIndex < 0 || currentIndex >= ( int32_t )nodeCount ) {
				continue;
			}
			if ( currentDist > distances[ ( size_t )currentIndex ] ) {
				continue;
			}

			for ( const std::pair<int32_t, double> &neighbor : adjacency[ ( size_t )currentIndex ] ) {
				const int32_t nextIndex = neighbor.first;
				const double stepCost = std::max( 0.0, neighbor.second );
				if ( nextIndex < 0 || nextIndex >= ( int32_t )nodeCount ) {
					continue;
				}
				const double candidate = currentDist + stepCost;
				if ( candidate >= distances[ ( size_t )nextIndex ] ) {
					continue;
				}
				distances[ ( size_t )nextIndex ] = candidate;
				frontier.push( { candidate, nextIndex } );
			}
		}

		return distances;
	};

	/**
	*   Select deterministic landmark nodes and compute per-landmark shortest-path distances.
	**/
	const int32_t desiredLandmarks = std::max( 1, std::min( policy.max_landmarks, ( int32_t )hierarchy.nodes.size() ) );
	std::vector<int32_t> selectedLandmarkCompactIndices = {};
	selectedLandmarkCompactIndices.reserve( ( size_t )desiredLandmarks );
	std::vector<std::vector<double>> distanceByLandmark = {};
	distanceByLandmark.reserve( ( size_t )desiredLandmarks );

	// Seed the first landmark from the lowest stable node id for deterministic selection.
	int32_t firstLandmarkCompact = 0;
	int32_t smallestNodeId = std::numeric_limits<int32_t>::max();
	for ( size_t i = 0; i < hierarchy.nodes.size(); i++ ) {
		if ( hierarchy.nodes[ i ].node_id < smallestNodeId ) {
			smallestNodeId = hierarchy.nodes[ i ].node_id;
			firstLandmarkCompact = ( int32_t )i;
		}
	}
	selectedLandmarkCompactIndices.push_back( firstLandmarkCompact );
	distanceByLandmark.push_back( runDijkstraFrom( firstLandmarkCompact ) );

	while ( ( int32_t )selectedLandmarkCompactIndices.size() < desiredLandmarks ) {
		int32_t bestCandidate = -1;
		double bestCoverageDistance = -1.0;

		for ( int32_t compactIndex = 0; compactIndex < ( int32_t )hierarchy.nodes.size(); compactIndex++ ) {
			if ( std::find( selectedLandmarkCompactIndices.begin(), selectedLandmarkCompactIndices.end(), compactIndex )
				!= selectedLandmarkCompactIndices.end() ) {
				continue;
			}

			double nearestDistance = std::numeric_limits<double>::infinity();
			for ( const std::vector<double> &distances : distanceByLandmark ) {
				if ( compactIndex >= ( int32_t )distances.size() ) {
					continue;
				}
				nearestDistance = std::min( nearestDistance, distances[ ( size_t )compactIndex ] );
			}

			if ( !std::isfinite( nearestDistance ) ) {
				nearestDistance = std::numeric_limits<double>::max() * 0.5;
			}
			if ( nearestDistance > bestCoverageDistance ) {
				bestCoverageDistance = nearestDistance;
				bestCandidate = compactIndex;
			}
		}

		if ( bestCandidate < 0 ) {
			break;
		}
		selectedLandmarkCompactIndices.push_back( bestCandidate );
		distanceByLandmark.push_back( runDijkstraFrom( bestCandidate ) );
	}

	/**
	*   Publish selected landmark descriptors and flattened node->landmark distances.
	**/
	outCache->landmarks.clear();
	outCache->node_to_landmark_distances.clear();
	outCache->landmarks.reserve( selectedLandmarkCompactIndices.size() );
	outCache->node_to_landmark_distances.reserve( hierarchy.nodes.size() * selectedLandmarkCompactIndices.size() );

	for ( size_t landmarkIndex = 0; landmarkIndex < selectedLandmarkCompactIndices.size(); landmarkIndex++ ) {
		const int32_t compactIndex = selectedLandmarkCompactIndices[ landmarkIndex ];
		if ( compactIndex < 0 || compactIndex >= ( int32_t )hierarchy.nodes.size() ) {
			continue;
		}
		const nav2_hierarchy_node_t &landmarkNode = hierarchy.nodes[ ( size_t )compactIndex ];
		nav2_precompute_landmark_t landmark = {};
		landmark.hierarchy_node_id = landmarkNode.node_id;
		landmark.landmark_index = ( int32_t )landmarkIndex;
		landmark.topology = landmarkNode.topology;
		landmark.preferred_z = landmarkNode.allowed_z_band.preferred_z;
		outCache->landmarks.push_back( landmark );
	}

	for ( size_t nodeIndex = 0; nodeIndex < hierarchy.nodes.size(); nodeIndex++ ) {
		for ( size_t landmarkIndex = 0; landmarkIndex < distanceByLandmark.size(); landmarkIndex++ ) {
			double distanceValue = std::numeric_limits<double>::infinity();
			if ( nodeIndex < distanceByLandmark[ landmarkIndex ].size() ) {
				distanceValue = distanceByLandmark[ landmarkIndex ][ nodeIndex ];
			}
			outCache->node_to_landmark_distances.push_back( distanceValue );
		}
	}
	if ( !outCache->landmarks.empty() ) {
		outCache->feature_flags |= NAV2_PRECOMPUTE_FEATURE_LANDMARKS;
	}

	/**
	*   Build optional portal-pair bounds using landmark triangle lower bounds.
	**/
	if ( policy.enable_portal_pair_bounds && policy.max_portal_pairs > 0 && !distanceByLandmark.empty() ) {
		std::vector<int32_t> portalNodeCompactIndices = {};
		for ( size_t i = 0; i < hierarchy.nodes.size(); i++ ) {
			if ( ( hierarchy.nodes[ i ].flags & NAV2_HIERARCHY_NODE_FLAG_HAS_PORTAL ) != 0 ) {
				portalNodeCompactIndices.push_back( ( int32_t )i );
			}
		}

		for ( size_t i = 0; i < portalNodeCompactIndices.size(); i++ ) {
			for ( size_t j = i + 1; j < portalNodeCompactIndices.size(); j++ ) {
				if ( ( int32_t )outCache->portal_pair_bounds.size() >= policy.max_portal_pairs ) {
					break;
				}

				const int32_t compactA = portalNodeCompactIndices[ i ];
				const int32_t compactB = portalNodeCompactIndices[ j ];
				const nav2_hierarchy_node_t &nodeA = hierarchy.nodes[ ( size_t )compactA ];
				const nav2_hierarchy_node_t &nodeB = hierarchy.nodes[ ( size_t )compactB ];
				const double lowerBound = SVG_Nav2_Precompute_ComputeLandmarkLowerBound( distanceByLandmark, compactA, compactB );

				nav2_precompute_pair_bound_t pair = {};
				pair.key_a = std::min( nodeA.node_id, nodeB.node_id );
				pair.key_b = std::max( nodeA.node_id, nodeB.node_id );
				pair.lower_bound = lowerBound;
				pair.source = nav2_precompute_lb_source_t::PortalPair;
				outCache->portal_pair_bounds.push_back( pair );
			}
			if ( ( int32_t )outCache->portal_pair_bounds.size() >= policy.max_portal_pairs ) {
				break;
			}
		}

		if ( !outCache->portal_pair_bounds.empty() ) {
			outCache->feature_flags |= NAV2_PRECOMPUTE_FEATURE_PORTAL_PAIR_BOUNDS;
		}
	}

	/**
	*   Build optional cluster-pair bounds from one deterministic representative per cluster.
	*   These are stored for future heuristic enrichments and diagnostics.
	**/
	if ( policy.enable_cluster_pair_bounds && policy.max_cluster_pairs > 0 && !distanceByLandmark.empty() ) {
		std::unordered_map<int32_t, int32_t> representativeByCluster = {};
		for ( size_t i = 0; i < hierarchy.nodes.size(); i++ ) {
			const int32_t clusterId = hierarchy.nodes[ i ].topology.cluster_id;
			if ( clusterId < 0 ) {
				continue;
			}
			const auto it = representativeByCluster.find( clusterId );
			if ( it == representativeByCluster.end() ) {
				representativeByCluster[ clusterId ] = ( int32_t )i;
				continue;
			}
			const nav2_hierarchy_node_t &oldRep = hierarchy.nodes[ ( size_t )it->second ];
			const nav2_hierarchy_node_t &newRep = hierarchy.nodes[ i ];
			if ( newRep.node_id < oldRep.node_id ) {
				representativeByCluster[ clusterId ] = ( int32_t )i;
			}
		}

		std::vector<int32_t> clusterIds = {};
		clusterIds.reserve( representativeByCluster.size() );
		for ( const auto &kvp : representativeByCluster ) {
			clusterIds.push_back( kvp.first );
		}
		std::sort( clusterIds.begin(), clusterIds.end() );

		for ( size_t i = 0; i < clusterIds.size(); i++ ) {
			for ( size_t j = i + 1; j < clusterIds.size(); j++ ) {
				if ( ( int32_t )outCache->cluster_pair_bounds.size() >= policy.max_cluster_pairs ) {
					break;
				}
				const int32_t clusterA = clusterIds[ i ];
				const int32_t clusterB = clusterIds[ j ];
				const int32_t repA = representativeByCluster[ clusterA ];
				const int32_t repB = representativeByCluster[ clusterB ];
				const double lowerBound = SVG_Nav2_Precompute_ComputeLandmarkLowerBound( distanceByLandmark, repA, repB );

				nav2_precompute_pair_bound_t pair = {};
				pair.key_a = std::min( clusterA, clusterB );
				pair.key_b = std::max( clusterA, clusterB );
				pair.lower_bound = lowerBound;
				pair.source = nav2_precompute_lb_source_t::ClusterPair;
				outCache->cluster_pair_bounds.push_back( pair );
			}
			if ( ( int32_t )outCache->cluster_pair_bounds.size() >= policy.max_cluster_pairs ) {
				break;
			}
		}

		if ( !outCache->cluster_pair_bounds.empty() ) {
			outCache->feature_flags |= NAV2_PRECOMPUTE_FEATURE_CLUSTER_PAIR_BOUNDS;
		}
	}

	/**
	*   Build optional region-layer edge cost cache.
	**/
	if ( policy.enable_region_layer_edge_costs ) {
		for ( const nav2_hierarchy_edge_t &edge : hierarchy.edges ) {
			if ( edge.region_layer_id < 0 ) {
				continue;
			}
			nav2_precompute_edge_cost_t entry = {};
			entry.from_node_id = edge.from_node_id;
			entry.to_node_id = edge.to_node_id;
			entry.base_cost = std::max( 0.0, edge.base_cost );
			entry.topology_penalty = std::max( 0.0, edge.topology_penalty );
			outCache->region_layer_edge_costs.push_back( entry );
		}
		if ( !outCache->region_layer_edge_costs.empty() ) {
			outCache->feature_flags |= NAV2_PRECOMPUTE_FEATURE_REGION_LAYER_EDGE_COSTS;
		}
	}

	/**
	*   Build optional connector crossing base-cost cache.
	**/
	if ( policy.enable_connector_crossing_costs ) {
		for ( const nav2_hierarchy_edge_t &edge : hierarchy.edges ) {
			if ( edge.connector_id < 0 && ( edge.flags & NAV2_HIERARCHY_EDGE_FLAG_REQUIRES_PORTAL ) == 0 ) {
				continue;
			}
			nav2_precompute_edge_cost_t entry = {};
			entry.from_node_id = edge.from_node_id;
			entry.to_node_id = edge.to_node_id;
			entry.base_cost = std::max( 0.0, edge.base_cost );
			entry.topology_penalty = std::max( 0.0, edge.topology_penalty );
			outCache->connector_crossing_costs.push_back( entry );
		}
		if ( !outCache->connector_crossing_costs.empty() ) {
			outCache->feature_flags |= NAV2_PRECOMPUTE_FEATURE_CONNECTOR_BASE_COSTS;
		}
	}

	/**
	*   Build final summary when requested.
	**/
	SVG_Nav2_Precompute_BuildSummaryFromCache( *outCache, outSummary );
	return outCache->hierarchy_node_count > 0 && !outCache->landmarks.empty();
}

/**
* @brief	Publish a built lower-bound cache for immutable shared queries.
* @param	cache	Cache to copy into published immutable storage.
* @return	True when publication succeeded.
**/
const bool SVG_Nav2_Precompute_PublishCache( const nav2_precompute_cache_t &cache ) {
	/**
	*   Require at least one hierarchy node and one landmark to publish a usable cache.
	**/
	if ( cache.hierarchy_node_count <= 0 || cache.landmarks.empty() ) {
		return false;
	}

	s_nav2_precompute_published_cache = cache;
	s_nav2_precompute_published_cache.feature_flags |= NAV2_PRECOMPUTE_FEATURE_IMMUTABLE_PUBLISHED;
	s_nav2_precompute_has_published_cache = true;
	return true;
}

/**
* @brief	Reset the published immutable lower-bound cache.
**/
void SVG_Nav2_Precompute_ClearPublishedCache( void ) {
	s_nav2_precompute_published_cache = {};
	s_nav2_precompute_has_published_cache = false;
}

/**
* @brief	Get the currently published immutable lower-bound cache.
* @return	Pointer to published cache, or `nullptr` when no cache is published.
**/
const nav2_precompute_cache_t *SVG_Nav2_Precompute_GetPublishedCache( void ) {
	if ( !s_nav2_precompute_has_published_cache ) {
		return nullptr;
	}
	return &s_nav2_precompute_published_cache;
}

/**
* @brief	Query a coarse lower bound between two hierarchy nodes from a specific cache.
* @param	cache	Cache to query.
* @param	fromNodeId	Stable source hierarchy node id.
* @param	toNodeId	Stable destination hierarchy node id.
* @param	outResult	[out] Lower-bound result payload.
* @return	True when the query executed.
**/
const bool SVG_Nav2_Precompute_QueryLowerBound( const nav2_precompute_cache_t &cache,
	const int32_t fromNodeId, const int32_t toNodeId, nav2_precompute_lb_result_t *outResult ) {
	/**
	*   Require output storage and reset deterministic defaults.
	**/
	if ( !outResult ) {
		return false;
	}
	*outResult = {};

	/**
	*   Resolve trivial equal-node case immediately.
	**/
	if ( fromNodeId == toNodeId && fromNodeId >= 0 ) {
		outResult->valid = true;
		outResult->lower_bound = 0.0;
		outResult->source = nav2_precompute_lb_source_t::FallbackHeuristic;
		return true;
	}

	/**
	*   Build fast maps from node ids to compact indices and landmark id->index for this query.
	**/
	if ( cache.hierarchy_node_count <= 0 || cache.landmarks.empty() ) {
		return true;
	}

	std::unordered_map<int32_t, int32_t> compactByNodeId = {};
	compactByNodeId.reserve( ( size_t )cache.hierarchy_node_count );
	for ( int32_t compactIndex = 0; compactIndex < cache.hierarchy_node_count; compactIndex++ ) {
		const int32_t nodeId = compactIndex + 1;
		compactByNodeId[ nodeId ] = compactIndex;
	}

	/**
	*   Try to derive compact indices from landmark metadata first, then fallback to dense id assumption.
	**/
	if ( compactByNodeId.find( fromNodeId ) == compactByNodeId.end() || compactByNodeId.find( toNodeId ) == compactByNodeId.end() ) {
		compactByNodeId.clear();
		for ( int32_t i = 0; i < cache.hierarchy_node_count; i++ ) {
			compactByNodeId[ i + 1 ] = i;
		}
	}

	const auto fromIt = compactByNodeId.find( fromNodeId );
	const auto toIt = compactByNodeId.find( toNodeId );
	if ( fromIt == compactByNodeId.end() || toIt == compactByNodeId.end() ) {
		return true;
	}
	const int32_t fromCompact = fromIt->second;
	const int32_t toCompact = toIt->second;

	/**
	*   Rebuild landmark distance vectors from flattened cache storage.
	**/
	const int32_t landmarkCount = ( int32_t )cache.landmarks.size();
	if ( landmarkCount <= 0 ) {
		return true;
	}
	if ( cache.node_to_landmark_distances.size() < ( size_t )cache.hierarchy_node_count * ( size_t )landmarkCount ) {
		return true;
	}

	std::vector<std::vector<double>> distanceByLandmark = {};
	distanceByLandmark.resize( ( size_t )landmarkCount );
	for ( int32_t landmarkIndex = 0; landmarkIndex < landmarkCount; landmarkIndex++ ) {
		distanceByLandmark[ ( size_t )landmarkIndex ].resize( ( size_t )cache.hierarchy_node_count, std::numeric_limits<double>::infinity() );
	}
	for ( int32_t nodeIndex = 0; nodeIndex < cache.hierarchy_node_count; nodeIndex++ ) {
		for ( int32_t landmarkIndex = 0; landmarkIndex < landmarkCount; landmarkIndex++ ) {
			const size_t flatIndex = ( size_t )nodeIndex * ( size_t )landmarkCount + ( size_t )landmarkIndex;
			distanceByLandmark[ ( size_t )landmarkIndex ][ ( size_t )nodeIndex ] = cache.node_to_landmark_distances[ flatIndex ];
		}
	}

	/**
	*   Evaluate landmark triangle lower bound.
	**/
	double bestLowerBound = SVG_Nav2_Precompute_ComputeLandmarkLowerBound( distanceByLandmark, fromCompact, toCompact );
	nav2_precompute_lb_source_t bestSource = nav2_precompute_lb_source_t::LandmarkTriangle;

	/**
	*   Evaluate exact portal-node pair cache when available.
	**/
	if ( !cache.portal_pair_bounds.empty() ) {
		const int32_t pairA = std::min( fromNodeId, toNodeId );
		const int32_t pairB = std::max( fromNodeId, toNodeId );
		for ( const nav2_precompute_pair_bound_t &pair : cache.portal_pair_bounds ) {
			if ( pair.key_a != pairA || pair.key_b != pairB ) {
				continue;
			}
			if ( pair.lower_bound > bestLowerBound ) {
				bestLowerBound = pair.lower_bound;
				bestSource = pair.source;
			}
			break;
		}
	}

	outResult->valid = true;
	outResult->lower_bound = std::max( 0.0, bestLowerBound );
	outResult->source = bestSource;
	return true;
}

/**
* @brief	Query a coarse lower bound between two hierarchy nodes using the published cache.
* @param	fromNodeId	Stable source hierarchy node id.
* @param	toNodeId	Stable destination hierarchy node id.
* @param	outResult	[out] Lower-bound result payload.
* @return	True when the query executed.
**/
const bool SVG_Nav2_Precompute_QueryPublishedLowerBound( const int32_t fromNodeId,
	const int32_t toNodeId, nav2_precompute_lb_result_t *outResult ) {
	if ( !outResult ) {
		return false;
	}
	*outResult = {};

	const nav2_precompute_cache_t *published = SVG_Nav2_Precompute_GetPublishedCache();
	if ( !published ) {
		return true;
	}
	return SVG_Nav2_Precompute_QueryLowerBound( *published, fromNodeId, toNodeId, outResult );
}

/**
* @brief	Emit bounded precompute summary diagnostics.
* @param	cache	Cache to inspect.
* @param	summary	Optional precomputed summary.
* @param	limit	Maximum number of record lines to print.
**/
void SVG_Nav2_Precompute_DebugPrint( const nav2_precompute_cache_t &cache,
	const nav2_precompute_summary_t *summary, const int32_t limit ) {
	if ( limit <= 0 ) {
		return;
	}

	nav2_precompute_summary_t localSummary = {};
	if ( summary ) {
		localSummary = *summary;
	} else {
		SVG_Nav2_Precompute_BuildSummaryFromCache( cache, &localSummary );
	}

	gi.dprintf( "[NAV2][Precompute] success=%d nodes=%d edges=%d landmarks=%d portal_pairs=%d cluster_pairs=%d region_edges=%d connector_edges=%d approx_bytes=%llu flags=0x%08x\n",
		localSummary.success ? 1 : 0,
		localSummary.hierarchy_node_count,
		localSummary.hierarchy_edge_count,
		localSummary.landmark_count,
		localSummary.portal_pair_bound_count,
		localSummary.cluster_pair_bound_count,
		localSummary.region_layer_edge_cost_count,
		localSummary.connector_crossing_cost_count,
		( unsigned long long )localSummary.approx_bytes,
		cache.feature_flags );

	const int32_t landmarkReportCount = std::min( ( int32_t )cache.landmarks.size(), limit );
	for ( int32_t i = 0; i < landmarkReportCount; i++ ) {
		const nav2_precompute_landmark_t &lm = cache.landmarks[ ( size_t )i ];
		gi.dprintf( "[NAV2][Precompute] landmark[%d] node=%d leaf=%d cluster=%d area=%d pref_z=%.1f\n",
			i,
			lm.hierarchy_node_id,
			lm.topology.leaf_index,
			lm.topology.cluster_id,
			lm.topology.area_id,
			lm.preferred_z );
	}
}

/**
* @brief	Keep the nav2 precompute module represented in the build.
**/
void SVG_Nav2_Precompute_ModuleAnchor( void ) {
}
