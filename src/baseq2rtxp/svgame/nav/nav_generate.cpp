#include "nav_generate.h"
#include "nav_thread.h"
#include <algorithm>
#include "shared/cm/cm_model.h"
#include "shared/formats/format_bsp.h"

/**
*	Global navmesh data containers.
**/
//! Global navmesh polygon data.
nav_vector_t<nav_poly_t> g_nav_polys;
//! Global navmesh kdtree node data.
nav_vector_t<nav_kdtree_node_t> g_nav_nodes;
//! Global navmesh leaf link data.
nav_vector_t<nav_leaf_link_t> g_nav_leaf_links;
//! Global navmesh leaf polygon ID data.
nav_vector_t<int32_t> g_nav_leaf_poly_ids;


//! Node result for negative space during BSP tree construction.
static constexpr int32_t NODE_NEGATIVE = -1;

//! Maximum number of points in a winding (polygon) for navmesh generation.
static constexpr int32_t MAX_WINDING_POINTS = 64;
/**
*	@brief	 A winding_t represents a polygon defined by a set of points, used during the navmesh generation process.
**/
struct winding_t {
	//! The number of points in the winding (polygon).
	int32_t num_points;
	//! The array of points that define the winding (polygon). The maximum number of points is defined by MAX_WINDING_POINTS.
	Vector3 points[ MAX_WINDING_POINTS ];
};


/**
*
*
*
*	
* 
* 
* 
**/
/**
*	@brief	 Generate the navmesh asynchronously.
**/
void Nav_GenerateCommand() {
    // Start the async generation
    Nav_StartAsyncGeneration();
}

/**
*	@brief	Print the current standalone nav generation status to the server console.
*	@note	This keeps the nav system self-contained so the command path does not need nav2/nav3 helpers.
**/
void Nav_StatusCommand( void ) {
    /**
    *	Collect the latest progress snapshot from the nav generation worker.
    **/
    const nav_gen_progress_t &progress = Nav_GetGenerationProgress();

    /**
    *	Print a stable summary line so server operators can see whether generation is active and how far it has progressed.
    **/
    gi.dprintf(
        "NavMesh status: generating=%d progress=%.2f%% elapsed=%u ms remaining=%u ms\n",
        progress.is_generating ? 1 : 0,
        progress.progress_pct * 100.0f,
        progress.time_taken_ms,
        progress.estimated_time_left_ms );
}

/**
*	@brief	Deallocate all navmesh data and clear the global containers.
**/
void Nav_Clear() {
    g_nav_polys.clear();
    g_nav_nodes.clear();
    g_nav_leaf_links.clear();
    g_nav_leaf_poly_ids.clear();
    gi.dprintf("NavMesh memory cleared.\n");
}



/**
*
*
*
*
*
*
*
**/
static winding_t BaseWindingForPlane(const cm_plane_t* p) {
    winding_t w;
    w.num_points = 4;
    
    int max = -1;
    float maxv = -1;
    for (int i=0; i<3; i++) {
        float v = fabs(p->normal[i]);
        if (v > maxv) {
            max = i;
            maxv = v;
        }
    }
    
    Vector3 up(0, 0, 0);
    if (max == 2) {
        up.x = 1;
    } else {
        up.z = 1;
    }
    
    Vector3 p_normal(p->normal[0], p->normal[1], p->normal[2]);
    Vector3 right = QM_Vector3CrossProduct(up, p_normal);
    const double lengthA = QM_Vector3NormalizeLength(right);
    up = QM_Vector3CrossProduct(p_normal, right);
	const double lengthB = QM_Vector3NormalizeLength(up);
    
    Vector3 org = p_normal * p->dist;
    
    float BIGNUMBER = 99999.0f;
    Vector3 vright = right * BIGNUMBER;
    Vector3 vup = up * BIGNUMBER;
    
    w.points[0] = org - vright + vup;
    w.points[1] = org + vright + vup;
    w.points[2] = org + vright - vup;
    w.points[3] = org - vright - vup;
    return w;
}

static bool ChopWindingInPlace(winding_t* in, const cm_plane_t* split, float epsilon) {
    float dists[MAX_WINDING_POINTS + 4];
    int sides[MAX_WINDING_POINTS + 4];
    int counts[3] = {0, 0, 0};
    
    for (int i=0; i<in->num_points; i++) {
        float dot = in->points[i].x * split->normal[0] + 
                    in->points[i].y * split->normal[1] + 
                    in->points[i].z * split->normal[2];
        dists[i] = dot - split->dist;
        if (dists[i] > epsilon) {
            sides[i] = 1; // front
        } else if (dists[i] < -epsilon) {
            sides[i] = 2; // back
        } else {
            sides[i] = 0; // on
        }
        counts[sides[i]]++;
    }
    sides[in->num_points] = sides[0];
    dists[in->num_points] = dists[0];
    
    if (counts[2] == 0) return false; // all front
    if (counts[1] == 0) return true; // all back
    
    winding_t out;
    out.num_points = 0;
    
    for (int i=0; i<in->num_points; i++) {
        Vector3 p1 = in->points[i];
        if (sides[i] != 1) { // back or on
            out.points[out.num_points++] = p1;
        }
        if (sides[i] == 0 || sides[i] == sides[i+1]) {
            continue;
        }
        if (sides[i+1] == 0) {
            continue;
        }
        // split
        Vector3 p2 = in->points[(i+1)%in->num_points];
        float dot = dists[i] / (dists[i] - dists[i+1]);
        Vector3 mid;
        for (int j=0; j<3; j++) {
            if (split->normal[j] == 1) mid[j] = split->dist;
            else if (split->normal[j] == -1) mid[j] = -split->dist;
            else mid[j] = p1[j] + dot * (p2[j] - p1[j]);
        }
        out.points[out.num_points++] = mid;
    }
    
    *in = out;
    return true;
}



/**
*
*
*
*	Generate the navmesh polygons from the current map's collision model:
*
*
*
**/
/**
*	@brief Extract walkable polygons from the current map's collision model and store them in g_nav_polys.
**/
void Nav_DoExtractionWork() {
	// Get the collision model from the game interface
    cm_t* cm = gi.GetCollisionModel();
	// Safety check: Ensure the collision model and its cache are valid before proceeding.
	if ( !cm || !cm->cache ) {
		return;
	}
	// Get the BSP data from the collision model's cache
    bsp_t* bsp = cm->cache;

	// Now we can safely clear the global navmesh polygon container before starting the extraction process.
    g_nav_polys.clear();
    
	// Iterate through all brushes in the BSP to extract walkable polygons
	for ( int32_t i = 0; i < bsp->numbrushes; i++ ) {
		// Get the current brush
		mbrush_t *b = &bsp->brushes[ i ];
		// Skip brushes that are not solid, as they cannot contribute to walkable surfaces.
		if ( !( b->contents & CONTENTS_SOLID ) ) {
			continue;
		}

		//! Iterate through each side of the brush to find walkable surfaces
		for ( int32_t j = 0; j < b->numsides; j++ ) {
			// Get the current brush side
			mbrushside_t *side = &b->firstbrushside[ j ];
			// Discard sides that are not walkable based on their normal's Z component
			if ( side->plane->normal[ 2 ] < NAV_MIN_WALKABLE_Z ) {
				continue;
			}

			// Create a base winding (polygon) for the current brush side's plane
			winding_t w = BaseWindingForPlane( side->plane );
			// Store a flag to track if the winding remains valid after clipping against other brush sides
			bool valid = true;
			//! Clip the winding against all other sides of the brush to ensure it fits within the brush's volume
			for ( int32_t k = 0; k < b->numsides && valid; k++ ) {
				// Skip clipping against the same side
				if ( j == k ) {
					continue;
				}
				// Get the brush side to clip against
				mbrushside_t *clip = &b->firstbrushside[ k ];
				// Create a flipped plane for clipping, as we want to keep the part of the winding that is inside the brush.
				cm_plane_t flip;
				flip.normal[ 0 ] = -clip->plane->normal[ 0 ];
				flip.normal[ 1 ] = -clip->plane->normal[ 1 ];
				flip.normal[ 2 ] = -clip->plane->normal[ 2 ];
				flip.dist = -clip->plane->dist;
				// Clip the winding in place against the flipped plane. If it fails, mark the winding as invalid.
				if ( !ChopWindingInPlace( &w, &flip, 0.1f ) ) {
					valid = false;
				}
			}
			// If the winding is still valid and has at least 3 points, create a nav_poly_t and add it to the global navmesh polygon container.
			if ( valid && w.num_points >= 3 ) {
				// Create a new nav_poly_t and populate its fields.
				nav_poly_t poly = {};
				// Assign a unique polygon ID based on the current size of the global navmesh polygon container.
				poly.poly_id = g_nav_polys.size();
				// Clamp the number of vertices to 8, as nav_poly_t supports a maximum of 8 vertices.
				poly.num_vertices = std::min( w.num_points, 8 );
				// Calculate the center of the polygon by averaging its vertices.
				Vector3 center( 0, 0, 0 );
				// Copy the vertices from the winding to the nav_poly_t and accumulate the center.
				for ( int v = 0; v < poly.num_vertices; v++ ) {
					// Copy the vertex position from the winding to the polygon.
					poly.vertices[ v ] = w.points[ v ];
					// Accumulate the vertex positions to compute the center.
					center = center + w.points[ v ];
				}
				// Finalize the center by dividing by the number of vertices.
				poly.center = center / ( float )poly.num_vertices;
				// Set the normal of the polygon to match the plane normal of the brush side.
				poly.normal.x = side->plane->normal[ 0 ];
				poly.normal.y = side->plane->normal[ 1 ];
				poly.normal.z = side->plane->normal[ 2 ];
				// Submit the polygon to the global navmesh polygon container.
				g_nav_polys.push_back( poly );
			}
		}
	}
}

/**
*	@brief Calculate the surface area of an axis-aligned bounding box defined by mins and maxs.	
*	@param mins The minimum corner of the AABB.
*	@param maxs The maximum corner of the AABB.
**/
static double SurfaceArea(const Vector3& mins, const Vector3& maxs) {
    Vector3 ext = maxs - mins;
    return 2.0 * (double)( ( double )ext.x * ( double )ext.y + ( double )ext.y * ( double )ext.z + ( double )ext.x * ( double )ext.z);
}

/**
*	@brief Recursively build a KD-tree node for the navmesh polygons.
*	@param firstPolyIdx	The index of the first polygon in g_nav_polys that belongs to this node.
*	@param polyCount The number of polygons in this node.
*	@param depth The current depth in the tree, used for termination conditions.
*	@return The index of the created node in g_nav_nodes, or -1 on failure.
**/
//--------------------------------------------------------------------------
// BuildKDNode
//--------------------------------------------------------------------------
// Recursively builds a KD‑tree node for a range of navmesh polygons.
// Parameters:
//   firstPolyIdx – index of the first polygon in g_nav_polys belonging to this node.
//   polyCount    – number of polygons covered by this node.
//   depth        – current recursion depth (used for termination).
// Returns:
//   Index of the created node in g_nav_nodes, or -1 on failure.
static int32_t BuildKDNode( int32_t firstPolyIdx, int32_t polyCount, int depth ) {
	/**
	*	Base case: no polygons to place.
	**/
	if ( polyCount == 0 ) {
		return -1; // Invalid node.
	}

	/**
	*	Guard against exceeding the maximum allowed node count.
	**/
	if ( g_nav_nodes.size() >= MAX_NAV_KDTREE_NODES ) {
		gi.dprintf( "WARNING: MAX_NAV_KDTREE_NODES reached!\n" );
		return -1;
	}

	/**
	*	Allocate a new node and initialise its fields to sentinel values.
	**/
	// The index of the new node will be the current size of g_nav_nodes, as we will push it back after initialization.
	int32_t nodeIdx = static_cast< int32_t >( g_nav_nodes.size() );
	// Initialize the new node with default values. Left and right children are set to -1 (indicating no children), and poly_id is set to -1 to indicate it's not a leaf yet. The BSP leaf ID is also initialized to -1 and will be filled in for leaf nodes.
	nav_kdtree_node_t node = {};
	node.left_child = -1;
	node.right_child = -1;
	node.poly_id = -1;        // Not a leaf yet.
	node.bsp_leaf_id = -1;        // Will be filled for leaf nodes.
	// Add the new node to the global container.
	g_nav_nodes.push_back( node );

	/**
	*	Compute the axis‑aligned bounding box (AABB) that encloses all polygons
	*	belonging to this node. The result is stored in nodeMins / nodeMaxs.
	**/
	// Initialize mins and maxs to extreme values so that they will be correctly updated when we iterate through the polygons.
	Vector3 nodeMins( 99999, 99999, 99999 );
	Vector3 nodeMaxs( -99999, -99999, -99999 );
	// Iterate through all polygons in this node to compute the AABB.
	for ( int32_t i = 0; i < polyCount; ++i ) {
		// Get the current polygon from the global container.
		const nav_poly_t &poly = g_nav_polys[ firstPolyIdx + i ];
		// Update the node's mins and maxs to include the vertices of the current polygon.
		for ( int32_t v = 0; v < poly.num_vertices; ++v ) {
			// Update mins and maxs for each vertex of the polygon.
			for ( int32_t k = 0; k < 3; ++k ) {
				// Update the minimum corner of the AABB if the current vertex is smaller in this dimension.
				if ( poly.vertices[ v ][ k ] < nodeMins[ k ] ) {
					nodeMins[ k ] = poly.vertices[ v ][ k ];
				}
				// Update the maximum corner of the AABB if the current vertex is larger in this dimension.
				if ( poly.vertices[ v ][ k ] > nodeMaxs[ k ] ) {
					nodeMaxs[ k ] = poly.vertices[ v ][ k ];
				}
			}
		}
	}
	// Store the computed AABB in the node.
	g_nav_nodes[ nodeIdx ].mins = nodeMins;
	g_nav_nodes[ nodeIdx ].maxs = nodeMaxs;

	/**
	*	Leaf termination criteria.
	*		* Only one polygon – makes a leaf that directly references the polygon.
	*		* Depth limit – prevents pathological recursion (chosen as 20 levels).
	**/
	if ( polyCount == 1 || depth >= 20 ) {
		g_nav_nodes[ nodeIdx ].poly_id = g_nav_polys[ firstPolyIdx ].poly_id;
		g_nav_nodes[ nodeIdx ].bsp_leaf_id = g_nav_polys[ firstPolyIdx ].bsp_leaf_id;
		return nodeIdx;
	}

	/**
	*	Surface‑Area Heuristic (SAH) based splitting.
	*	We evaluate a small set of candidate split positions along each axis using
	*	a binning approach (NUM_BINS bins). The split with the lowest estimated
	*	traversal cost is chosen.
	**/
	const int NUM_BINS = 8;
	double bestCost = 9.999e9; // Large initial value.
	int    bestAxis = -1;      // Axis index: 0 = X, 1 = Y, 2 = Z.
	double bestSplitPos = 0.0;    // Position along bestAxis.

	Vector3 extents = nodeMaxs - nodeMins;
	double parentSA = SurfaceArea( nodeMins, nodeMaxs );
	if ( parentSA == 0.0 )
		parentSA = 1.0; // Guard against division by zero.

	// Evaluate each axis independently.
	for ( int axis = 0; axis < 3; ++axis ) {
		// Skip degenerate dimensions – no meaningful split possible.
		if ( extents[ axis ] < 0.1f )
			continue;

		// ----- Bin definition ------------------------------------------------
		struct Bin {
			Vector3 mins{ 99999, 99999, 99999 };
			Vector3 maxs{ -99999, -99999, -99999 };
			int     count = 0;
			// Expand the bin to include the given polygon.
			void Expand( const nav_poly_t &p ) {
				for ( int v = 0; v < p.num_vertices; ++v ) {
					for ( int k = 0; k < 3; ++k ) {
						if ( p.vertices[ v ][ k ] < mins[ k ] )
							mins[ k ] = p.vertices[ v ][ k ];
						if ( p.vertices[ v ][ k ] > maxs[ k ] )
							maxs[ k ] = p.vertices[ v ][ k ];
					}
				}
				++count;
			}
		};
		Bin bins[ NUM_BINS ];

		/**
		*	Populate bins based on polygon centroids.
		**/
		for ( int i = 0; i < polyCount; ++i ) {
			// Get the current polygon from the global container.
			const nav_poly_t &poly = g_nav_polys[ firstPolyIdx + i ];
			// Determine the bin index for this polygon based on its centroid along the current axis.
			double c = poly.center[ axis ];
			// Calculate the bin index. The formula maps the centroid position to a bin index in the range [0, NUM_BINS-1].
			int32_t b = static_cast< int >( NUM_BINS * ( c - nodeMins[ axis ] ) / extents[ axis ] );
			// Clamp the bin index to ensure it falls within valid bounds.
			if ( b < 0 ) b = 0;
			if ( b >= NUM_BINS ) b = NUM_BINS - 1;
			// Expand the corresponding bin to include this polygon.
			bins[ b ].Expand( poly );
		}

		/**
		*	Compute SAH cost for each possible split between bins.
		**/
		for ( int32_t i = 0; i < NUM_BINS - 1; ++i ) {
			// Left side aggregates.
			Vector3 leftMins( 99999, 99999, 99999 );
			Vector3 leftMaxs( -99999, -99999, -99999 );
			int32_t     leftCount = 0;
			for ( int32_t j = 0; j <= i; ++j ) {
				if ( bins[ j ].count == 0 ) continue;
				for ( int32_t k = 0; k < 3; ++k ) {
					if ( bins[ j ].mins[ k ] < leftMins[ k ] ) leftMins[ k ] = bins[ j ].mins[ k ];
					if ( bins[ j ].maxs[ k ] > leftMaxs[ k ] ) leftMaxs[ k ] = bins[ j ].maxs[ k ];
				}
				leftCount += bins[ j ].count;
			}

			// Right side aggregates.
			Vector3 rightMins( 99999, 99999, 99999 );
			Vector3 rightMaxs( -99999, -99999, -99999 );
			int32_t rightCount = 0;
			for ( int32_t j = i + 1; j < NUM_BINS; ++j ) {
				if ( bins[ j ].count == 0 ) continue;
				for ( int32_t k = 0; k < 3; ++k ) {
					if ( bins[ j ].mins[ k ] < rightMins[ k ] ) rightMins[ k ] = bins[ j ].mins[ k ];
					if ( bins[ j ].maxs[ k ] > rightMaxs[ k ] ) rightMaxs[ k ] = bins[ j ].maxs[ k ];
				}
				rightCount += bins[ j ].count;
			}

			// If either side is empty, the split is invalid.
			if ( leftCount == 0 || rightCount == 0 )
				continue;

			double leftSA = SurfaceArea( leftMins, leftMaxs );
			double rightSA = SurfaceArea( rightMins, rightMaxs );
			double cost = 1.0 + ( leftSA / parentSA ) * leftCount + ( rightSA / parentSA ) * rightCount;

			if ( cost < bestCost ) {
				bestCost = cost;
				bestAxis = axis;
				bestSplitPos = nodeMins[ axis ] + extents[ axis ] * ( i + 1 ) / static_cast< double >( NUM_BINS );
			}
		}
	}

	/**
	*	If no valid split was found, fall back to a leaf node.
	**/
	if ( bestAxis == -1 ) {
		g_nav_nodes[ nodeIdx ].poly_id = g_nav_polys[ firstPolyIdx ].poly_id;
		g_nav_nodes[ nodeIdx ].bsp_leaf_id = g_nav_polys[ firstPolyIdx ].bsp_leaf_id;
		return nodeIdx;
	}

	/**
	*	Partition the polygon array around the selected split plane.
	*	Polygons with centroid <= split position go to the left child.
	**/
	auto it = std::partition( g_nav_polys.get_data() + firstPolyIdx,
							g_nav_polys.get_data() + firstPolyIdx + polyCount,
							[ bestAxis, bestSplitPos ] ( const nav_poly_t &p ) {
								return p.center[ bestAxis ] <= bestSplitPos;
							} 
	);
	int32_t leftCount = static_cast< int32_t >( it - ( g_nav_polys.get_data() + firstPolyIdx ) );
	int32_t rightCount = polyCount - leftCount;

	// Guard against pathological partitioning that leaves one side empty.
	if ( leftCount == 0 || rightCount == 0 ) {
		g_nav_nodes[ nodeIdx ].poly_id = g_nav_polys[ firstPolyIdx ].poly_id;
		g_nav_nodes[ nodeIdx ].bsp_leaf_id = g_nav_polys[ firstPolyIdx ].bsp_leaf_id;
		return nodeIdx;
	}

	// Record the splitting axis for debugging / traversal purposes.
	g_nav_nodes[ nodeIdx ].split_axis = bestAxis;

	// Recursively build child sub‑trees.
	int32_t left = BuildKDNode( firstPolyIdx, leftCount, depth + 1 );
	int32_t right = BuildKDNode( firstPolyIdx + leftCount, rightCount, depth + 1 );

	// Store child indices.
	g_nav_nodes[ nodeIdx ].left_child = left;
	g_nav_nodes[ nodeIdx ].right_child = right;

	return nodeIdx;
}

/**
*	@brief	Wrapper function to build the KD-Tree for the navmesh polygons.
**/
void Nav_BuildKDTree() {
	// Clear any existing KD-Tree nodes before building a new one
    g_nav_nodes.clear();
	//! Check if there are any navmesh polygons to build the KD-Tree from. If not, return early.
	if ( g_nav_polys.empty() ) {
		return;
	}
	
	//! Start building the KD-Tree from the root node, which encompasses all navmesh polygons. The initial depth is set to 0.
    BuildKDNode(0, g_nav_polys.size(), 0);

	//! Log the completion of the KD-Tree generation and the total number of nodes created in the global navmesh node container.
    gi.dprintf("NavMesh KD-Tree Generation Completed. %d nodes created.\n", g_nav_nodes.size());
}
