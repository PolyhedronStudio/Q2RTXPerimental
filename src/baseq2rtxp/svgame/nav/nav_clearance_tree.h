#pragma once
#include "nav_clearance_geometry.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>

/**
*	@brief	Immutable balanced boundary-tree node; leaves retain original ordered edge identities.
**/
struct nav_clearance_tree_node_t {
	//! Conservative XY/elevation bounds of every descendant boundary.
	double min_x = 0.0, min_y = 0.0, min_z = 0.0, max_x = 0.0, max_y = 0.0, max_z = 0.0;
	//! Child indices for branches, or the original edge index for a leaf.
	int32_t left = -1, right = -1, edge = -1;
};

/**
*	@brief	Build balanced nearest-boundary geometry before immutable snapshot publication.
*	@param	edges	Original ordered finite boundary pairs.
*	@param	blocks	Strict XY-blocking classification for each edge.
*	@param	indices	Owner-prepared construction scratch, retained for snapshot accounting.
*	@param	nodes	Owner-owned output node storage.
*	@return	False keeps the exhaustive fallback for malformed/extreme coordinates.
*	@note	Only publication performs allocation; median partitioning bounds query stack depth.
**/
template<typename Edges, typename Blocks, typename Indices, typename Nodes>
bool Nav_BuildClearanceTree( const Edges &edges, const Blocks &blocks, Indices &indices, Nodes &nodes ) {
	/**
	*	Refuse geometry outside the conservative scalar pruning contract; retain exact linear behavior there.
	**/
	indices.clear();
	nodes.clear();
	if ( edges.size() != blocks.size() || edges.size() > static_cast<size_t>( INT32_MAX ) / 2 ) {
		return false;
	}
	// Only strict wall boundaries belong to this index; ledges/risers retain native support policy.
	for ( size_t index = 0; index < edges.size(); ++index ) {
		if ( blocks[ index ] == 0 ) {
			continue;
		}
		const auto &edge = edges[ index ];
		// Validate before constructing any bounds that could suppress an exact geometry evaluation.
		for ( const auto *point : { &edge.first, &edge.second } ) {
			if ( !std::isfinite( point->x ) || !std::isfinite( point->y ) || !std::isfinite( point->z ) ||
				std::fabs( point->x ) > 32768.0 || std::fabs( point->y ) > 32768.0 ) {
				nodes.clear();
				return false;
			}
		}
		indices.push_back( static_cast<int32_t>( index ) );
	}
	// Empty eligible geometry is a complete index whose nearest distance is infinity.
	if ( indices.empty() ) {
		return true;
	}
	nodes.reserve( indices.size() * 2 - 1 );
	/**
	*	Partition leaves by their largest spatial extent; all node bounds are unions of exact endpoints.
	**/
	const auto build = [&]( auto &&self, const size_t first, const size_t last ) -> int32_t {
		nav_clearance_tree_node_t node;
		node.min_x = node.min_y = node.min_z = std::numeric_limits<double>::infinity();
		node.max_x = node.max_y = node.max_z = -std::numeric_limits<double>::infinity();
		// Every descendant endpoint contributes to the conservative node envelope.
		for ( size_t cursor = first; cursor < last; ++cursor ) {
			const auto &edge = edges[ indices[ cursor ] ];
			for ( const auto *point : { &edge.first, &edge.second } ) {
				node.min_x = std::min( node.min_x, point->x ); node.max_x = std::max( node.max_x, point->x );
				node.min_y = std::min( node.min_y, point->y ); node.max_y = std::max( node.max_y, point->y );
				node.min_z = std::min( node.min_z, point->z ); node.max_z = std::max( node.max_z, point->z );
			}
		}
		const int32_t result = static_cast<int32_t>( nodes.size() );
		nodes.push_back( node );
		// Leaf identity is the original index, so equal-distance witnesses retain the exhaustive first-edge rule.
		if ( last - first == 1 ) {
			nodes[ result ].edge = indices[ first ];
			return result;
		}
		const bool split_x = node.max_x - node.min_x >= node.max_y - node.min_y;
		const size_t middle = first + ( last - first ) / 2;
		std::nth_element( indices.begin() + first, indices.begin() + middle, indices.begin() + last,
			[&]( const int32_t a, const int32_t b ) {
				const auto &ea = edges[ a ]; const auto &eb = edges[ b ];
				const double ca = split_x ? ea.first.x + ea.second.x : ea.first.y + ea.second.y;
				const double cb = split_x ? eb.first.x + eb.second.x : eb.first.y + eb.second.y;
				return ca != cb ? ca < cb : a < b;
			} );
		const int32_t left = self( self, first, middle );
		const int32_t right = self( self, middle, last );
		nodes[ result ].left = left;
		nodes[ result ].right = right;
		return result;
	};
	build( build, 0, indices.size() );
	return true;
}

/**
*	@brief	Find the exact nearest eligible boundary using immutable bounds and stack-only scratch.
*	@param	nodes	Complete balanced tree prepared for these edge records.
*	@param	minimumSqr	[out] Exact squared nearest distance, infinity if no eligible edges.
*	@param	witness	[out] Original first edge index for an equal minimum, or -1.
*	@return	False requests the exhaustive fallback; partial tree results must never authorize traversal.
*	@note	Uses the same exact distance evaluator and conservative cushion as the exhaustive reference.
**/
template<typename Point, typename Edges>
bool Nav_QueryClearanceTree( const std::span<const nav_clearance_tree_node_t> nodes, const Edges &edges,
	const Point &from, const Point &to, const double min_z, const double max_z, double &minimumSqr, int32_t &witness ) {
	/**
	*	Extreme query coordinates retain the exhaustive overflow/failure behavior.
	**/
	minimumSqr = std::numeric_limits<double>::infinity();
	witness = -1;
	if ( std::fabs( from.x ) > 32768.0 || std::fabs( from.y ) > 32768.0 ||
		std::fabs( to.x ) > 32768.0 || std::fabs( to.y ) > 32768.0 ) {
		return false;
	}
	// No indexed boundaries is a complete clear result.
	if ( nodes.empty() ) {
		return true;
	}
	std::array<int32_t, 64> stack;
	size_t pending = 1;
	stack[ 0 ] = 0;
	struct bounds_point_t { double x, y; };
	const bounds_point_t a{ from.x, from.y }, b{ to.x, to.y };
	/**
	*	Reject distant subtrees by closed bounds; every candidate leaf receives the original exact evaluator.
	**/
	while ( pending != 0 ) {
		const int32_t index = stack[ --pending ];
		// Malformed index/depth is a failed optimization, never a partial clearance proof.
		if ( index < 0 || static_cast<size_t>( index ) >= nodes.size() ) {
			return false;
		}
		const auto &node = nodes[ index ];
		// An unrelated elevation or conservative planar lower bound cannot improve the current exact minimum.
		if ( node.max_z < min_z || node.min_z > max_z ||
			!Nav_ClearanceSegmentMayImprove( a, b, bounds_point_t{ node.min_x, node.min_y }, bounds_point_t{ node.max_x, node.max_y }, minimumSqr ) ) {
			continue;
		}
		if ( node.edge >= 0 ) {
			if ( static_cast<size_t>( node.edge ) >= edges.size() ) {
				return false;
			}
			const auto &edge = edges[ node.edge ];
			const double distance = Nav_ClearanceSegmentDistanceSqr( from, to, edge.first, edge.second );
			// Nonfinite arithmetic must retain the reference's fail-closed behavior.
			if ( !std::isfinite( distance ) ) {
				return false;
			}
			if ( distance < minimumSqr || ( distance == minimumSqr && ( witness < 0 || node.edge < witness ) ) ) {
				minimumSqr = distance;
				witness = node.edge;
			}
		} else {
			if ( pending + 2 > stack.size() ) {
				return false;
			}
			stack[ pending++ ] = node.right;
			stack[ pending++ ] = node.left;
		}
	}
	return true;
}
