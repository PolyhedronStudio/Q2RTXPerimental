/********************************************************************
*
*
*	ServerGame: KD-Tree Acceleration Structure Builder
*				using Adaptive Hybrid SAH (64-bin & Exact Edge-Events),
*				dynamic depth scaling, and exact leaf localization.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "nav_kdtree_builder.h"
#include "nav_generate.h"
#include "nav_thread.h"
#include <algorithm>
#include <cmath>
#include <vector>

/**
*	@brief	Compute surface area of an axis-aligned bounding box.
*	@param	mins	[in] Minimum corner of the AABB.
*	@param	maxs	[in] Maximum corner of the AABB.
*	@return	Total surface area (2 * (xy + yz + zx)).
*	@note	Surface Area Heuristic (SAH) calculates the probability of spatial ray/point intersection
*			proportional to bounding box surface area.
**/
double SurfaceArea( const Vector3DP &mins, const Vector3DP &maxs ) {
	// Compute spatial extents along X, Y, Z axes.
	Vector3DP ext = maxs - mins;
	// Calculate surface area formula: SA = 2 * (dx*dy + dy*dz + dx*dz).
	return 2.0 * ( static_cast< double >( ext.x ) * static_cast< double >( ext.y ) +
				   static_cast< double >( ext.y ) * static_cast< double >( ext.z ) +
				   static_cast< double >( ext.x ) * static_cast< double >( ext.z ) );
}

/**
*	@brief	Recursively build one KD-tree node for a face span using Adaptive Hybrid SAH and Spatial Splits.
*	@param	firstFaceIdx	First face index in the current span inside g_nav_faces.
*	@param	faceCount		Number of faces in the current span to partition.
*	@param	depth			Current recursion depth level in the spatial tree.
*	@return	Created node index in g_nav_nodes, or -1 on early exit / failure.
*	@note	Uses Binned SAH (64 bins) for top-level nodes (N >= 1024) for linear O(N) performance,
*			and Exact Edge-Event SAH for subtrees (N < 1024) for optimal node tightness.
*			Calculates dynamic max depth scaling via k * log2(N) + 16 (k = 1.3).
*			Stores the authoritative split coordinate in node.split_pos for exact leaf localization.
**/
static int32_t BuildKDNode( int32_t firstFaceIdx, int32_t faceCount, int32_t depth ) {
	/**
	*	Sanity checks and allocation safety guards.
	**/
	// Check if the input face count is zero to prevent building empty nodes.
	if ( faceCount == 0 ) {
		// Return invalid index for empty face spans.
		return -1;
	}
	// Safety check: ensure total KD-tree node allocations do not exceed maximum capacity limit.
	if ( g_nav_nodes.size() >= MAX_NAV_KDTREE_NODES ) {
		// Log warning to server console if capacity limit is reached.
		gi.dprintf( "WARNING: MAX_NAV_KDTREE_NODES reached!\n" );
		// Return invalid index on allocation overflow to protect memory integrity.
		return -1;
	}

	/**
	*	Initialize new KD-tree node record inside global storage vector.
	**/
	// Record index of the newly allocated node.
	int32_t nodeIdx = static_cast< int32_t >( g_nav_nodes.size() );
	// Clear node structure memory.
	nav_kdtree_node_t node = {};
	// Default left child index to unassigned (-1).
	node.left_child = -1;
	// Default right child index to unassigned (-1).
	node.right_child = -1;
	// Default first face ID to unassigned (-1) for internal nodes.
	node.first_face_id = -1;
	// Initialize face count to zero.
	node.num_faces = 0;
	// Initialize BSP leaf association ID.
	node.bsp_leaf_id = -1;
	// Push uninitialized node into global storage vector.
	g_nav_nodes.push_back( node );

	/**
	*	Compute tight AABB bounding box for all faces in the current span.
	**/
	// Fetch first vertex of the first face to initialize node minimum and maximum bounds.
	const Vector3DP &first_v = g_nav_vertices[ g_nav_halfedges[ g_nav_faces[ firstFaceIdx ].first_edge_idx ].vertex_idx ];
	// Initialize minimum bounding box corner.
	Vector3DP nodeMins = first_v;
	// Initialize maximum bounding box corner.
	Vector3DP nodeMaxs = first_v;

	// Loop over all faces in the current span.
	for ( int32_t i = 0; i < faceCount; ++i ) {
		// Reference candidate face.
		const nav_face_t &face = g_nav_faces[ firstFaceIdx + i ];
		// Iterate over all edges of the face loop.
		for ( int32_t e = 0; e < face.num_edges; ++e ) {
			// Fetch vertex coordinates for the edge origin.
			const Vector3DP &v = g_nav_vertices[ g_nav_halfedges[ face.first_edge_idx + e ].vertex_idx ];
			// Expand minimum and maximum bounds across all 3 axes.
			for ( int32_t k = 0; k < 3; ++k ) {
				if ( v[ k ] < nodeMins[ k ] ) nodeMins[ k ] = v[ k ];
				if ( v[ k ] > nodeMaxs[ k ] ) nodeMaxs[ k ] = v[ k ];
			}
		}
	}
	// Commit node minimum bounding box.
	g_nav_nodes[ nodeIdx ].mins = static_cast< Vector3DP >( nodeMins );
	// Commit node maximum bounding box.
	g_nav_nodes[ nodeIdx ].maxs = static_cast< Vector3DP >( nodeMaxs );

	/**
	*	Evaluate termination criteria using dynamic Max Depth formula: k * log2(N) + 16.
	*	Target Leaf Size: 4 to 8 faces per leaf node.
	**/
	// Calculate dynamic max depth based on total face count to handle varying map complexities.
	const int32_t dynamic_max_depth = std::max< int32_t >( 24, static_cast< int32_t >( 1.3 * std::log2( std::max< int32_t >( 2, faceCount ) ) + 16.0 ) );
	// Target leaf face count threshold.
	constexpr int32_t TARGET_LEAF_FACES = 4;

	// Stop splitting if face count drops to target leaf size or if dynamic max depth is reached.
	if ( faceCount <= TARGET_LEAF_FACES || depth >= dynamic_max_depth ) {
		// Assign face span start index to leaf node.
		g_nav_nodes[ nodeIdx ].first_face_id = firstFaceIdx;
		// Assign face count to leaf node.
		g_nav_nodes[ nodeIdx ].num_faces = faceCount;
		// Store BSP leaf association ID.
		g_nav_nodes[ nodeIdx ].bsp_leaf_id = g_nav_faces[ firstFaceIdx ].bsp_leaf_id;
		// Return created leaf node index.
		return nodeIdx;
	}

	/**
	*	Calculate surface area of parent bounding box for Surface Area Heuristic (SAH).
	**/
	// Calculate parent node AABB surface area.
	double parentSA = SurfaceArea( nodeMins, nodeMaxs );
	// Prevent division by zero if parent surface area is microscopic.
	if ( parentSA == 0.0 ) parentSA = 1.0;
	// Initialize best cost to baseline unsplit leaf cost: C_leaf = SA_P * N.
	double bestCost = parentSA * static_cast< double >( faceCount );
	// Initialize best split axis to unassigned (-1).
	int32_t bestAxis = -1;
	// Initialize best split position coordinate.
	double bestSplitPos = 0.0;

	// Calculate bounding box spatial extents along X, Y, Z.
	Vector3DP extents = nodeMaxs - nodeMins;

	/**
	*	Adaptive SAH Split Selection Strategy:
	*		- Binned SAH (64 bins) for N >= 1024 faces for O(N) linear performance.
	*		- Exact Edge-Event SAH for N < 1024 faces to achieve micro-level structural precision.
	**/
	if ( faceCount >= 1024 ) {
		/**
		*	Binned SAH Evaluation Pass (64 bins across 3 axes).
		**/
		// Number of spatial evaluation bins.
		constexpr int32_t NUM_BINS = 64;
		// Iterate over all 3 spatial axes (X, Y, Z).
		for ( int32_t axis = 0; axis < 3; ++axis ) {
			// Skip degenerate axes with microscopic extent (< 0.1 units) to avoid division by zero.
			if ( extents[ axis ] < 0.1 ) continue;

			// Bin structure to accumulate bounding box bounds and primitive counts.
			struct Bin {
				Vector3DP mins{};
				Vector3DP maxs{};
				int32_t   count = 0;
				bool      initialized = false;
				void Expand( const nav_face_t &f ) {
					for ( int32_t e = 0; e < f.num_edges; ++e ) {
						const Vector3DP &v = g_nav_vertices[ g_nav_halfedges[ f.first_edge_idx + e ].vertex_idx ];
						if ( !initialized ) {
							mins = v;
							maxs = v;
							initialized = true;
						} else {
							for ( int32_t k = 0; k < 3; ++k ) {
								if ( v[ k ] < mins[ k ] ) mins[ k ] = v[ k ];
								if ( v[ k ] > maxs[ k ] ) maxs[ k ] = v[ k ];
							}
						}
					}
					++count;
				}
			};

			// Allocate bin array.
			std::vector< Bin > bins( NUM_BINS );
			// Distribute faces into bins based on face centroid coordinate along active axis.
			for ( int32_t i = 0; i < faceCount; ++i ) {
				const nav_face_t &face = g_nav_faces[ firstFaceIdx + i ];
				double c = face.center[ axis ];
				int32_t b = static_cast< int32_t >( NUM_BINS * ( c - nodeMins[ axis ] ) / extents[ axis ] );
				if ( b < 0 ) b = 0;
				if ( b >= NUM_BINS ) b = NUM_BINS - 1;
				bins[ b ].Expand( face );
			}

			// Evaluate SAH cost across all candidate bin boundaries.
			for ( int32_t i = 0; i < NUM_BINS - 1; ++i ) {
				Vector3DP leftMins{}, leftMaxs{};
				int32_t leftCount = 0;
				bool leftInit = false;
				for ( int32_t j = 0; j <= i; ++j ) {
					if ( bins[ j ].count == 0 || !bins[ j ].initialized ) continue;
					if ( !leftInit ) {
						leftMins = bins[ j ].mins;
						leftMaxs = bins[ j ].maxs;
						leftInit = true;
					} else {
						for ( int32_t k = 0; k < 3; ++k ) {
							if ( bins[ j ].mins[ k ] < leftMins[ k ] ) leftMins[ k ] = bins[ j ].mins[ k ];
							if ( bins[ j ].maxs[ k ] > leftMaxs[ k ] ) leftMaxs[ k ] = bins[ j ].maxs[ k ];
						}
					}
					leftCount += bins[ j ].count;
				}

				Vector3DP rightMins{}, rightMaxs{};
				int32_t rightCount = 0;
				bool rightInit = false;
				for ( int32_t j = i + 1; j < NUM_BINS; ++j ) {
					if ( bins[ j ].count == 0 || !bins[ j ].initialized ) continue;
					if ( !rightInit ) {
						rightMins = bins[ j ].mins;
						rightMaxs = bins[ j ].maxs;
						rightInit = true;
					} else {
						for ( int32_t k = 0; k < 3; ++k ) {
							if ( bins[ j ].mins[ k ] < rightMins[ k ] ) rightMins[ k ] = bins[ j ].mins[ k ];
							if ( bins[ j ].maxs[ k ] > rightMaxs[ k ] ) rightMaxs[ k ] = bins[ j ].maxs[ k ];
						}
					}
					rightCount += bins[ j ].count;
				}

				// Skip empty child candidate splits to avoid unpopulated subtrees.
				if ( leftCount == 0 || rightCount == 0 ) continue;

				// Compute surface areas for left and right child candidate AABBs.
				double leftSA = SurfaceArea( leftMins, leftMaxs );
				double rightSA = SurfaceArea( rightMins, rightMaxs );
				// Compute SAH cost formula: C = 1.0 + (SA_L / SA_P) * N_L + (SA_R / SA_P) * N_R.
				double cost = 1.0 + ( leftSA / parentSA ) * leftCount + ( rightSA / parentSA ) * rightCount;

				// Update best split parameters if candidate cost is strictly lower.
				if ( cost < bestCost ) {
					bestCost = cost;
					bestAxis = axis;
					bestSplitPos = nodeMins[ axis ] + extents[ axis ] * ( i + 1 ) / static_cast< double >( NUM_BINS );
				}
			}
		}
	} else {
		/**
		*	Exact Edge-Event SAH Evaluation Pass for subtrees (N < 1024).
		*	Sorts primitive AABB start and end events along each axis to evaluate exact split costs.
		**/
		// Iterate over all 3 spatial axes (X, Y, Z).
		for ( int32_t axis = 0; axis < 3; ++axis ) {
			// Skip degenerate axes.
			if ( extents[ axis ] < 0.1 ) continue;

			// Edge event structure tracking event position and event type (-1 = END, 0 = PLANAR, 1 = START).
			struct EdgeEvent {
				double pos = 0.0;
				int32_t type = 0;
			};
			std::vector< EdgeEvent > events;
			events.reserve( faceCount * 2 );

			// Generate start, planar, and end events for all faces in the current span.
			for ( int32_t i = 0; i < faceCount; ++i ) {
				const nav_face_t &face = g_nav_faces[ firstFaceIdx + i ];
				double f_min = g_nav_vertices[ g_nav_halfedges[ face.first_edge_idx ].vertex_idx ][ axis ];
				double f_max = f_min;
				for ( int32_t e = 1; e < face.num_edges; ++e ) {
					double v_coord = g_nav_vertices[ g_nav_halfedges[ face.first_edge_idx + e ].vertex_idx ][ axis ];
					if ( v_coord < f_min ) f_min = v_coord;
					if ( v_coord > f_max ) f_max = v_coord;
				}
				// Classify planar vs volumetric face bounds.
				if ( std::abs( f_max - f_min ) < 0.001 ) {
					events.push_back( EdgeEvent{ f_min, 0 } );
				} else {
					events.push_back( EdgeEvent{ f_min, 1 } );
					events.push_back( EdgeEvent{ f_max, -1 } );
				}
			}

			// Sort edge events in ascending order along the active axis.
			std::sort( events.begin(), events.end(), []( const EdgeEvent &a, const EdgeEvent &b ) {
				if ( a.pos != b.pos ) return a.pos < b.pos;
				return a.type < b.type;
			} );

			// Running primitive counts for left, planar, and right sub-regions.
			int32_t n_left = 0;
			int32_t n_flat = 0;
			int32_t n_right = faceCount;

			// Sweep through sorted edge events to evaluate exact SAH cost at every boundary.
			for ( size_t i = 0; i < events.size(); ++i ) {
				const EdgeEvent &ev = events[ i ];
				if ( ev.type == -1 ) --n_right;
				if ( ev.type == 0 ) ++n_flat;
				if ( ev.type == 1 ) ++n_left;

				// Evaluate split cost if event position falls strictly inside node bounding box.
				if ( ev.pos > nodeMins[ axis ] && ev.pos < nodeMaxs[ axis ] ) {
					double leftSA = ( ev.pos - nodeMins[ axis ] );
					double rightSA = ( nodeMaxs[ axis ] - ev.pos );
					double cost = 1.0 + ( leftSA / extents[ axis ] ) * n_left + ( rightSA / extents[ axis ] ) * ( n_right + n_flat );
					if ( cost < bestCost ) {
						bestCost = cost;
						bestAxis = axis;
						bestSplitPos = ev.pos;
					}
				}
			}
		}
	}

	// Child partition element counters.
	int32_t leftCount = 0;
	int32_t rightCount = 0;

	/**
	*	Partition face array across bestSplitPos.
	**/
	if ( bestAxis == -1 ) {
		// Fallback: Median split along longest axis if SAH failed to find split.
		bestAxis = 0;
		if ( extents.y > extents.x ) bestAxis = 1;
		if ( extents.z > extents[ bestAxis ] ) bestAxis = 2;

		// Perform median split using std::nth_element.
		std::nth_element( g_nav_faces.begin() + firstFaceIdx,
			g_nav_faces.begin() + firstFaceIdx + faceCount / 2,
			g_nav_faces.begin() + firstFaceIdx + faceCount,
			[bestAxis]( const nav_face_t &a, const nav_face_t &b ) {
				return a.center[ bestAxis ] < b.center[ bestAxis ];
			} );

		leftCount = faceCount / 2;
		rightCount = faceCount - leftCount;
		bestSplitPos = g_nav_faces[ firstFaceIdx + leftCount ].center[ bestAxis ];
	} else {
		// Partition faces across bestSplitPos using std::partition.
		auto it = std::partition( g_nav_faces.begin() + firstFaceIdx,
			g_nav_faces.begin() + firstFaceIdx + faceCount,
			[bestAxis, bestSplitPos]( const nav_face_t &f ) {
				return f.center[ bestAxis ] <= bestSplitPos;
			}
		);
		leftCount = static_cast< int32_t >( it - ( g_nav_faces.begin() + firstFaceIdx ) );
		rightCount = faceCount - leftCount;

		// Fallback for one-sided partitions to enforce balanced tree splitting.
		if ( leftCount == 0 || rightCount == 0 ) {
			std::nth_element( g_nav_faces.begin() + firstFaceIdx,
				g_nav_faces.begin() + firstFaceIdx + faceCount / 2,
				g_nav_faces.begin() + firstFaceIdx + faceCount,
				[bestAxis]( const nav_face_t &a, const nav_face_t &b ) {
					return a.center[ bestAxis ] < b.center[ bestAxis ];
				} );
			leftCount = faceCount / 2;
			rightCount = faceCount - leftCount;
			bestSplitPos = g_nav_faces[ firstFaceIdx + leftCount ].center[ bestAxis ];
		}
	}

	/**
	*	Store split axis and exact split position in the internal node record.
	**/
	g_nav_nodes[ nodeIdx ].split_axis = bestAxis;
	g_nav_nodes[ nodeIdx ].split_pos = bestSplitPos;

	/**
	*	Recurse into left and right child subtrees.
	**/
	int32_t left = BuildKDNode( firstFaceIdx, leftCount, depth + 1 );
	int32_t right = BuildKDNode( firstFaceIdx + leftCount, rightCount, depth + 1 );

	// Assign left child node index.
	g_nav_nodes[ nodeIdx ].left_child = left;
	// Assign right child node index.
	g_nav_nodes[ nodeIdx ].right_child = right;

	// Return created internal node index.
	return nodeIdx;
}

/**
*	@brief	Build the KD-tree used for spatial nav queries.
*	@note	This is the final generation stage after the half-edge mesh is ready.
*			Repairs half-edge face back-pointers after `std::partition` shuffles face indices.
**/
void Nav_BuildKDTree() {
	// Set initial KD-tree generation progress stage.
	Nav_SetGenerationProgress( 0.90f );

	/**
	*	Reset global KD-node storage vector.
	**/
	g_nav_nodes.clear();
	// Early return if no nav faces exist to build tree.
	if ( g_nav_faces.empty() ) return;

	/**
	*	Build recursive KD-tree spatial index starting from root.
	**/
	BuildKDNode( 0, static_cast< int32_t >( g_nav_faces.size() ), 0 );
	Nav_SetGenerationProgress( 0.95f );

	/**
	*	`CRITICAL FIX`: `BuildKDNode` calls `std::partition`, which physically shuffles `g_nav_faces`.
	*	We `MUST` update the back-pointers in the half-edges so they point to the NEW shuffled face index!
	*	Otherwise, Nav_FindPath's A* will jump to random, completely unconnected faces!
	**/
	for ( int32_t i = 0; i < static_cast< int32_t >( g_nav_faces.size() ); i++ ) {
		// Update KD-tree post-build face repair progress (mapping to 0.95f..0.99f).
		if ( !g_nav_faces.empty() ) {
			const float pct = 0.95f + 0.04f * ( static_cast< float >( i ) / static_cast< float >( g_nav_faces.size() ) );
			Nav_SetGenerationProgress( pct );
		}

		// Update face ID back-pointer on face structure.
		g_nav_faces[ i ].face_id = i;

		// Update face back-pointer on all half-edges belonging to this face loop.
		for ( int32_t e = 0; e < g_nav_faces[ i ].num_edges; ++e ) {
			g_nav_halfedges[ g_nav_faces[ i ].first_edge_idx + e ].face_idx = i;
		}
	}

	// Validate the repaired ownership and final KD leaf face spans.
	Nav_ValidateTopology( "KDTree" );

	// Log tree build completion summary to server console.
	gi.dprintf( "NavMesh KD-Tree Generation Completed. %d nodes created.\n", static_cast< int32_t >( g_nav_nodes.size() ) );
}
