#include "nav_generate.h"
#include "nav_thread.h"
#include <algorithm>
#include "shared/cm/cm_model.h"
#include "shared/formats/format_bsp.h"

/**
*	Global navmesh data containers.
**/
//! Global navmesh polygon data (temporary during build).
nav_vector_t<nav_poly_t> g_nav_polys;

//! Half-edge mesh global data
std::vector<Vector3> g_nav_vertices;
std::vector<nav_halfedge_t> g_nav_halfedges;
std::vector<nav_face_t> g_nav_faces;

//! Global navmesh kdtree node data.
nav_vector_t<nav_kdtree_node_t> g_nav_nodes;
//! Global navmesh leaf link data.
nav_vector_t<nav_leaf_link_t> g_nav_leaf_links;
//! Global navmesh leaf polygon ID data.
nav_vector_t<int32_t> g_nav_leaf_poly_ids;


//! Node result for negative space during BSP tree construction.
static constexpr int32_t NODE_NEGATIVE = -1;

//! Maximum number of points in a winding (polygon) for navmesh generation.
static constexpr int32_t MAX_WINDING_POINTS = 1024;
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
    g_nav_vertices.clear();
    g_nav_halfedges.clear();
    g_nav_faces.clear();
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
*	@brief Splits a winding by a plane into two separate windings.
**/
static void SplitWinding(const winding_t* in, const cm_plane_t* split, float epsilon, const Vector3& poly_normal, winding_t* front, winding_t* back) {
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
    
    front->num_points = 0;
    back->num_points = 0;

    if (counts[2] == 0 && counts[1] == 0) { // all on
        // Coplanar! If normals point in same direction, it's on the front of the brush. 
        // If normals are opposite, it's resting underneath the brush, so it goes to back (inside).
        Vector3 split_normal(split->normal[0], split->normal[1], split->normal[2]);
        if (QM_Vector3DotProduct(poly_normal, split_normal) > 0.0f) {
            *front = *in;
        } else {
            *back = *in;
        }
        return;
    }

    if (counts[2] == 0) { // all front (and on)
        *front = *in;
        return;
    }
    if (counts[1] == 0) { // all back (and on)
        *back = *in;
        return;
    }
    
    for (int i=0; i<in->num_points; i++) {
        Vector3 p1 = in->points[i];
        
        if (sides[i] == 0) {
            front->points[front->num_points++] = p1;
            back->points[back->num_points++] = p1;
            continue;
        }
        
        if (sides[i] == 1) {
            front->points[front->num_points++] = p1;
        } else {
            back->points[back->num_points++] = p1;
        }
        
        if (sides[i+1] == 0 || sides[i+1] == sides[i]) {
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
        
        front->points[front->num_points++] = mid;
        back->points[back->num_points++] = mid;
    }
}

static bool IsFragmentCompletelyOutsideBrush(const winding_t* frag, const mbrush_t* b) {
	for (int32_t j = 0; j < b->numsides; j++) {
		mbrushside_t* side = &b->firstbrushside[j];
		bool strictly_in_front = true;
		for (int p = 0; p < frag->num_points; p++) {
			float dot = frag->points[p].x * side->plane->normal[0] + 
						frag->points[p].y * side->plane->normal[1] + 
						frag->points[p].z * side->plane->normal[2];
			if (dot - side->plane->dist <= 0.1f) {
				strictly_in_front = false;
				break;
			}
		}
		if (strictly_in_front) {
			return true; // The fragment is entirely in front of or on this plane, so it doesn't intersect the brush volume
		}
	}
	return false;
}

/**
*	@brief Subtracts a brush from a list of polygon fragments, replacing them with the resulting smaller fragments.
**/
static void SubtractBrushFromWindings(std::vector<winding_t>& fragments, const mbrush_t* b, const Vector3& poly_normal) {
	std::vector<winding_t> next_fragments;
	
	for ( const winding_t& frag : fragments ) {
		// Optimization: If the fragment is entirely in front of any plane of the brush, it is completely outside the brush.
		// We can skip splitting it entirely, saving massive amounts of fragmentation!
		if (IsFragmentCompletelyOutsideBrush(&frag, b)) {
			next_fragments.push_back(frag);
			continue;
		}

		winding_t inside_part = frag;
		bool entirely_inside = true;
		
		// Slicing against every plane of the brush
		for (int32_t j = 0; j < b->numsides; j++) {
			mbrushside_t* side = &b->firstbrushside[j];
			
			// Split with all planes, including sloped ones, so that overlapping brushes
			// correctly form intersection edges that can be Twin Linked later.
			
			winding_t front = {};
			winding_t back = {};
			SplitWinding(&inside_part, side->plane, 0.1f, poly_normal, &front, &back);
			
			// Anything in front of an outward-facing plane is strictly OUTSIDE the brush. We save it.
			if (front.num_points >= 3) {
				next_fragments.push_back(front);
				entirely_inside = false;
			}
			
			// Anything behind the plane might still be inside the brush, so we keep chopping it.
			inside_part = back;
			if (inside_part.num_points < 3) {
				entirely_inside = false;
				break; // It's no longer inside the brush
			}
		}
		
		// If inside_part still has points after checking all planes, it is completely inside the brush.
		// Since this is a subtraction, we simply discard it! (Do not add it to next_fragments).
	}
	
	fragments = next_fragments;
}

/**
*	@brief Attempts to merge two coplanar convex polygons. If successful, stores the merged strictly-convex polygon in out.
**/
static bool TryMergeWindings(const winding_t* w1, const winding_t* w2, const Vector3& normal, winding_t* out) {
    // 1. Find shared edge
    int match_i1 = -1, match_i2 = -1;
    for (int i1 = 0; i1 < w1->num_points; i1++) {
        Vector3 a1 = w1->points[i1];
        Vector3 b1 = w1->points[(i1 + 1) % w1->num_points];

        for (int i2 = 0; i2 < w2->num_points; i2++) {
            Vector3 a2 = w2->points[i2];
            Vector3 b2 = w2->points[(i2 + 1) % w2->num_points];

            // Match b2 == a1 and a2 == b1 (reverse order)
            if (QM_Vector3DistanceSqr(a1, b2) < 0.1f && QM_Vector3DistanceSqr(b1, a2) < 0.1f) {
                match_i1 = i1;
                match_i2 = i2;
                break;
            }
        }
        if (match_i1 != -1) break;
    }

    if (match_i1 == -1) return false;

    // 2. Build merged polygon
    out->num_points = 0;
    int n1 = w1->num_points;
    int n2 = w2->num_points;

    for (int j = 0; j < n1; j++) {
        out->points[out->num_points++] = w1->points[(match_i1 + 1 + j) % n1];
    }
    for (int j = 0; j < n2 - 2; j++) {
        out->points[out->num_points++] = w2->points[(match_i2 + 2 + j) % n2];
    }

    // 3. Check convexity and simplify
    winding_t simple = *out;
    simple.num_points = 0;

    for (int i = 0; i < out->num_points; i++) {
        Vector3 prev = out->points[(i - 1 + out->num_points) % out->num_points];
        Vector3 curr = out->points[i];
        Vector3 next = out->points[(i + 1) % out->num_points];

        Vector3 d1 = QM_Vector3Subtract(curr, prev);
        Vector3 d2 = QM_Vector3Subtract(next, curr);
        
        // Ensure non-zero length to avoid normalization issues
        if (QM_Vector3DotProduct(d1, d1) < 0.001f || QM_Vector3DotProduct(d2, d2) < 0.001f) {
            continue; // Skip very close points
        }
        
        d1 = QM_Vector3Normalize(d1);
        d2 = QM_Vector3Normalize(d2);
        
        Vector3 cross = QM_Vector3CrossProduct(d1, d2);
        float dot = QM_Vector3DotProduct(cross, normal);

        // Quake base windings are clockwise relative to the normal.
        // This means for a convex corner, the cross product of (d1 x d2) points OPPOSITE to the normal.
        // So dot < -0.01f is strictly convex.
        // dot > 0.01f is concave (interior angle > 180).
        if (dot > 0.01f) return false; // Concave

        simple.points[simple.num_points++] = curr; // Keep all convex and collinear points
    }
    
    if (simple.num_points < 3 || simple.num_points > MAX_WINDING_POINTS) return false; // Too complex or invalid

    *out = simple;
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
        gi.dprintf("Nav_DoExtractionWork: No collision model or cache!\n");
		return;
	}
	// Get the BSP data from the collision model's cache
    bsp_t* bsp = cm->cache;

	// Now we can safely clear the global navmesh polygon container before starting the extraction process.
    g_nav_polys.clear();
    
    int solid_brushes = 0;
    int walkable_sides = 0;

	// Iterate through all brushes in the BSP to extract walkable polygons
	for ( int32_t i = 0; i < bsp->numbrushes; i++ ) {
		// Get the current brush
		mbrush_t *b = &bsp->brushes[ i ];
		// Skip brushes that are not solid or detail, as they cannot contribute to walkable surfaces.
		if ( !( b->contents & ( CONTENTS_SOLID | CONTENTS_DETAIL | CONTENTS_PLAYERCLIP ) ) ) {
			continue;
		}
        solid_brushes++;

		//! Iterate through each side of the brush to find walkable surfaces
		for ( int32_t j = 0; j < b->numsides; j++ ) {
			// Get the current brush side
			mbrushside_t *side = &b->firstbrushside[ j ];
			// Discard sides that are not walkable based on their normal's Z component
			if ( side->plane->normal[ 2 ] < NAV_MIN_WALKABLE_Z ) {
				continue;
			}
            walkable_sides++;

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
				// Clip the winding in place against the plane. If it fails, mark the winding as invalid.
				if ( !ChopWindingInPlace( &w, clip->plane, 0.1f ) ) {
					valid = false;
				}
			}
			// If the winding is still valid and has at least 3 points, create a nav_poly_t and add it to the global navmesh polygon container.
			if ( valid && w.num_points >= 3 ) {
				std::vector<winding_t> fragments;
				fragments.push_back(w);
				
				// Subtract all other solid brushes from this walkable surface
				Vector3 normal(side->plane->normal[0], side->plane->normal[1], side->plane->normal[2]);
				for ( int32_t other_idx = 0; other_idx < bsp->numbrushes; other_idx++ ) {
					if ( other_idx == i ) {
						continue; // Skip self
					}
					
					mbrush_t* other_b = &bsp->brushes[ other_idx ];
					if ( !( other_b->contents & ( CONTENTS_SOLID | CONTENTS_DETAIL ) ) ) {
						continue; // Only subtract blocking geometry
					}
					
					SubtractBrushFromWindings(fragments, other_b, normal);
					
					if ( fragments.empty() ) {
						break; // Entire surface was swallowed by other brushes
					}
				}
				// Merge fragments to reduce unnecessary fragmentation and keep polygon counts low
#if 0
				bool merged = true;
				while (merged) {
					merged = false;
					for (size_t f1 = 0; f1 < fragments.size() && !merged; f1++) {
						for (size_t f2 = f1 + 1; f2 < fragments.size(); f2++) {
							winding_t merged_w;
							if (TryMergeWindings(&fragments[f1], &fragments[f2], normal, &merged_w)) {
								// Replace f1 with merged, remove f2
								fragments[f1] = merged_w;
								fragments.erase(fragments.begin() + f2);
								merged = true; // Restart the scan because indices have changed
								break;
							}
						}
					}
				}
#endif
				
				// Add surviving fragments
				for ( const winding_t& frag : fragments ) {
					// Create a new nav_poly_t and populate its fields.
					nav_poly_t poly = {};
					// Assign a unique polygon ID based on the current size of the global navmesh polygon container.
					poly.poly_id = g_nav_polys.size();
					// Clamp the number of vertices to 8, as nav_poly_t supports a maximum of 8 vertices.
					poly.num_vertices = std::min( frag.num_points, 1024 );
					// Calculate the center of the polygon by averaging its vertices.
					Vector3 center( 0, 0, 0 );
					// Copy the vertices from the winding to the nav_poly_t and accumulate the center.
					for ( int v = 0; v < poly.num_vertices; v++ ) {
						// Copy the vertex position from the winding to the polygon.
						poly.vertices[ v ] = frag.points[ v ];
						// Accumulate the vertex positions to compute the center.
						center = center + frag.points[ v ];
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
    gi.dprintf("Nav_DoExtractionWork: Checked %d brushes. Found %d solid brushes, %d walkable sides. Extracted %d polys.\n",
        bsp->numbrushes, solid_brushes, walkable_sides, (int)g_nav_polys.size());
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
*	@brief Recursively build a KD-tree node for the navmesh faces.
*	@param firstFaceIdx	The index of the first face in g_nav_faces that belongs to this node.
*	@param faceCount The number of faces in this node.
*	@param depth The current depth in the tree, used for termination conditions.
*	@return The index of the created node in g_nav_nodes, or -1 on failure.
**/
static int32_t BuildKDNode( int32_t firstFaceIdx, int32_t faceCount, int depth ) {
	if ( faceCount == 0 ) return -1;
	if ( g_nav_nodes.size() >= MAX_NAV_KDTREE_NODES ) {
		gi.dprintf( "WARNING: MAX_NAV_KDTREE_NODES reached!\n" );
		return -1;
	}

	int32_t nodeIdx = static_cast< int32_t >( g_nav_nodes.size() );
	nav_kdtree_node_t node = {};
	node.left_child = -1;
	node.right_child = -1;
	node.first_face_id = -1;      // Not a leaf yet.
	node.num_faces = 0;
	node.bsp_leaf_id = -1;        // Will be filled for leaf nodes.
	g_nav_nodes.push_back( node );

	Vector3 nodeMins( 99999, 99999, 99999 );
	Vector3 nodeMaxs( -99999, -99999, -99999 );
	for ( int32_t i = 0; i < faceCount; ++i ) {
		const nav_face_t &face = g_nav_faces[ firstFaceIdx + i ];
        for (int32_t e = 0; e < face.num_edges; ++e) {
            const Vector3& v = g_nav_vertices[g_nav_halfedges[face.first_edge_idx + e].vertex_idx];
			for ( int32_t k = 0; k < 3; ++k ) {
				if ( v[ k ] < nodeMins[ k ] ) nodeMins[ k ] = v[ k ];
				if ( v[ k ] > nodeMaxs[ k ] ) nodeMaxs[ k ] = v[ k ];
			}
		}
	}
	g_nav_nodes[ nodeIdx ].mins = nodeMins;
	g_nav_nodes[ nodeIdx ].maxs = nodeMaxs;

	const int32_t MAX_FACES_PER_LEAF = 32;
	if ( faceCount <= MAX_FACES_PER_LEAF ) {
		g_nav_nodes[ nodeIdx ].first_face_id = firstFaceIdx;
		g_nav_nodes[ nodeIdx ].num_faces = faceCount;
		g_nav_nodes[ nodeIdx ].bsp_leaf_id = g_nav_faces[ firstFaceIdx ].bsp_leaf_id;
		return nodeIdx;
	}

	const int NUM_BINS = 1024;
	double bestCost = 9.999e9;
	int    bestAxis = -1;
	double bestSplitPos = 0.0;

	Vector3 extents = nodeMaxs - nodeMins;
	double parentSA = SurfaceArea( nodeMins, nodeMaxs );
	if ( parentSA == 0.0 ) parentSA = 1.0;

	for ( int axis = 0; axis < 3; ++axis ) {
		if ( extents[ axis ] < 0.1f ) continue;

		struct Bin {
			Vector3 mins{ 99999, 99999, 99999 };
			Vector3 maxs{ -99999, -99999, -99999 };
			int     count = 0;
			void Expand( const nav_face_t &f ) {
                for (int32_t e = 0; e < f.num_edges; ++e) {
                    const Vector3& v = g_nav_vertices[g_nav_halfedges[f.first_edge_idx + e].vertex_idx];
					for ( int k = 0; k < 3; ++k ) {
						if ( v[ k ] < mins[ k ] ) mins[ k ] = v[ k ];
						if ( v[ k ] > maxs[ k ] ) maxs[ k ] = v[ k ];
					}
				}
				++count;
			}
		};
		// Use vector for bins to avoid massive stack allocation with 1024 bins
		std::vector<Bin> bins(NUM_BINS);

		for ( int i = 0; i < faceCount; ++i ) {
			const nav_face_t &face = g_nav_faces[ firstFaceIdx + i ];
			double c = face.center[ axis ];
			int32_t b = static_cast< int >( NUM_BINS * ( c - nodeMins[ axis ] ) / extents[ axis ] );
			if ( b < 0 ) b = 0;
			if ( b >= NUM_BINS ) b = NUM_BINS - 1;
			bins[ b ].Expand( face );
		}

		for ( int32_t i = 0; i < NUM_BINS - 1; ++i ) {
			Vector3 leftMins( 99999, 99999, 99999 );
			Vector3 leftMaxs( -99999, -99999, -99999 );
			int32_t leftCount = 0;
			for ( int32_t j = 0; j <= i; ++j ) {
				if ( bins[ j ].count == 0 ) continue;
				for ( int32_t k = 0; k < 3; ++k ) {
					if ( bins[ j ].mins[ k ] < leftMins[ k ] ) leftMins[ k ] = bins[ j ].mins[ k ];
					if ( bins[ j ].maxs[ k ] > leftMaxs[ k ] ) leftMaxs[ k ] = bins[ j ].maxs[ k ];
				}
				leftCount += bins[ j ].count;
			}

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

			if ( leftCount == 0 || rightCount == 0 ) continue;

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

	int32_t leftCount = 0;
	int32_t rightCount = 0;

	if ( bestAxis == -1 ) {
		// Fallback: Median split along longest axis if SAH failed to find any split
		bestAxis = 0;
		if ( extents.y > extents.x ) bestAxis = 1;
		if ( extents.z > extents[ bestAxis ] ) bestAxis = 2;

		std::nth_element( g_nav_faces.begin() + firstFaceIdx,
						  g_nav_faces.begin() + firstFaceIdx + faceCount / 2,
						  g_nav_faces.begin() + firstFaceIdx + faceCount,
						  [ bestAxis ] ( const nav_face_t &a, const nav_face_t &b ) {
							  return a.center[ bestAxis ] < b.center[ bestAxis ];
						  } );

		leftCount = faceCount / 2;
		rightCount = faceCount - leftCount;
	} else {
		// Normal SAH split
		auto it = std::partition( g_nav_faces.begin() + firstFaceIdx,
								g_nav_faces.begin() + firstFaceIdx + faceCount,
								[ bestAxis, bestSplitPos ] ( const nav_face_t &f ) {
									return f.center[ bestAxis ] <= bestSplitPos;
								} 
		);
		leftCount = static_cast< int32_t >( it - ( g_nav_faces.begin() + firstFaceIdx ) );
		rightCount = faceCount - leftCount;

		// Edge case: if partition put everything on one side despite SAH, force median split
		if ( leftCount == 0 || rightCount == 0 ) {
			std::nth_element( g_nav_faces.begin() + firstFaceIdx,
							  g_nav_faces.begin() + firstFaceIdx + faceCount / 2,
							  g_nav_faces.begin() + firstFaceIdx + faceCount,
							  [ bestAxis ] ( const nav_face_t &a, const nav_face_t &b ) {
								  return a.center[ bestAxis ] < b.center[ bestAxis ];
							  } );
			leftCount = faceCount / 2;
			rightCount = faceCount - leftCount;
		}
	}

	g_nav_nodes[ nodeIdx ].split_axis = bestAxis;

	int32_t left = BuildKDNode( firstFaceIdx, leftCount, depth + 1 );
	int32_t right = BuildKDNode( firstFaceIdx + leftCount, rightCount, depth + 1 );

	g_nav_nodes[ nodeIdx ].left_child = left;
	g_nav_nodes[ nodeIdx ].right_child = right;

	return nodeIdx;
}

/**
*	@brief Recursively partitions a list of polygons into a grid using spatial split planes.
**/
template <typename PolyContainer>
static void PartitionPolygonsRecursive(PolyContainer& polys, const Vector3& mins, const Vector3& maxs, int depth) {
    if (polys.empty()) return;

    static int recurse_count = 0;
    if (depth == 0) recurse_count = 0;
    recurse_count++;
    if (recurse_count % 10000 == 0) {
        gi.dprintf("PartitionPolygonsRecursive: count=%d, depth=%d, polys=%d, extents=(%.1f, %.1f, %.1f)\n", 
            recurse_count, depth, (int)polys.size(), maxs.x - mins.x, maxs.y - mins.y, maxs.z - mins.z);
    }
    
    Vector3 extents = maxs - mins;
    if (extents.x <= 256.0f && extents.y <= 256.0f) {
        // Grid cell is small enough in 2D dimensions, stop subdividing.
        return;
    }
    
    // Pick the longest axis to split (only X or Y for 2.5D NavMesh!)
    int split_axis = 0;
    if (extents.y > extents.x) split_axis = 1;
    
    float split_dist = mins[split_axis] + extents[split_axis] * 0.5f;
    
    // "Obstacle-Aware KD-Tree Splitting"
    // Find the polygon vertex coordinate along the split axis that is closest to the geometric center.
    // This perfectly aligns the split plane with the edges of crates/stairs instead of arbitrary halfway points.
    float best_diff = extents[split_axis];
    for (const auto& poly : polys) {
        for (int i = 0; i < poly.num_vertices; i++) {
            float v = poly.vertices[i][split_axis];
            // Only consider vertices that are not on the absolute boundary of the bounding box
            if (v > mins[split_axis] + 1.0f && v < maxs[split_axis] - 1.0f) {
                float diff = std::abs(v - (mins[split_axis] + extents[split_axis] * 0.5f));
                if (diff < best_diff) {
                    best_diff = diff;
                    split_dist = v;
                }
            }
        }
    }
    
    cm_plane_t plane = {};
    plane.normal[split_axis] = 1.0f;
    plane.dist = split_dist;
    
    std::vector<nav_poly_t> front_polys;
    std::vector<nav_poly_t> back_polys;
    
    for (const auto& poly : polys) {
        winding_t w = {};
        w.num_points = poly.num_vertices;
        for (int i = 0; i < poly.num_vertices; i++) {
            w.points[i] = poly.vertices[i];
        }
        
        winding_t front = {};
        winding_t back = {};
        
        // Split winding using the axis-aligned plane
        SplitWinding(&w, &plane, 0.1f, poly.normal, &front, &back);
        
        if (front.num_points >= 3) {
            nav_poly_t p = poly;
            p.num_vertices = front.num_points;
            for (int i = 0; i < front.num_points; i++) p.vertices[i] = front.points[i];
            
            // Recompute center
            Vector3 center(0, 0, 0);
            for (int i = 0; i < p.num_vertices; i++) center = center + p.vertices[i];
            p.center = center / (float)p.num_vertices;
            
            front_polys.push_back(p);
        }
        if (back.num_points >= 3) {
            nav_poly_t p = poly;
            p.num_vertices = back.num_points;
            for (int i = 0; i < back.num_points; i++) p.vertices[i] = back.points[i];
            
            // Recompute center
            Vector3 center(0, 0, 0);
            for (int i = 0; i < p.num_vertices; i++) center = center + p.vertices[i];
            p.center = center / (float)p.num_vertices;
            
            back_polys.push_back(p);
        }
    }
    
    // Update bounding boxes for children
    Vector3 front_mins = mins;
    Vector3 front_maxs = maxs;
    front_mins[split_axis] = split_dist;
    
    Vector3 back_mins = mins;
    Vector3 back_maxs = maxs;
    back_maxs[split_axis] = split_dist;
    
    PartitionPolygonsRecursive(front_polys, front_mins, front_maxs, depth + 1);
    PartitionPolygonsRecursive(back_polys, back_mins, back_maxs, depth + 1);
    
    // Reconstruct the polys array from the leaves
    polys.clear();
    for (const auto& p : front_polys) polys.push_back(p);
    for (const auto& p : back_polys) polys.push_back(p);
}

/**
*	@brief	KD-Tree driven spatial partitioning of the NavMesh polygons.
*			Creates detailed sub-polygon areas to allow more organic AI movement paths,
*			while ensuring zero T-Junctions by splitting adjacent polys with infinite planes.
**/
static void Nav_PartitionPolygons() {
    if (g_nav_polys.empty()) return;
    
    Vector3 mins(99999, 99999, 99999);
    Vector3 maxs(-99999, -99999, -99999);
    
    for (const auto& p : g_nav_polys) {
        for (int i = 0; i < p.num_vertices; i++) {
            mins.x = std::min(mins.x, p.vertices[i].x);
            mins.y = std::min(mins.y, p.vertices[i].y);
            mins.z = std::min(mins.z, p.vertices[i].z);
            maxs.x = std::max(maxs.x, p.vertices[i].x);
            maxs.y = std::max(maxs.y, p.vertices[i].y);
            maxs.z = std::max(maxs.z, p.vertices[i].z);
        }
    }
    
    int before_count = (int)g_nav_polys.size();
    PartitionPolygonsRecursive(g_nav_polys, mins, maxs, 0);
    gi.dprintf("NavMesh: Spatial Partitioning expanded %d polys into %d grid cells.\n", before_count, (int)g_nav_polys.size());
}

#include <unordered_map>
#include <utility>

/**
*	@brief Resolves micro-gaps and T-Junctions by splicing missing vertices directly into the edges of adjacent polygons.
*          This perfectly guarantees the 1-to-1 Twin Half-Edge constraint without destroying the faces.
**/
static void Nav_ResolveTJunctionsByEdgeSplicing() {
    if (g_nav_polys.empty()) return;

    struct PolyAABB {
        float min_x, min_y, min_z, max_x, max_y, max_z;
    };

    std::vector<PolyAABB> aabbs(g_nav_polys.size());
    auto UpdateAABB = [](const nav_poly_t& p, PolyAABB& aabb) {
        float min_x = 99999, min_y = 99999, min_z = 99999, max_x = -99999, max_y = -99999, max_z = -99999;
        for (int k = 0; k < p.num_vertices; k++) {
            min_x = std::min(min_x, p.vertices[k].x);
            min_y = std::min(min_y, p.vertices[k].y);
            min_z = std::min(min_z, p.vertices[k].z);
            max_x = std::max(max_x, p.vertices[k].x);
            max_y = std::max(max_y, p.vertices[k].y);
            max_z = std::max(max_z, p.vertices[k].z);
        }
        aabb = {min_x - 4.0f, min_y - 4.0f, min_z - 24.0f, max_x + 4.0f, max_y + 4.0f, max_z + 24.0f};
    };

    struct GridCell {
        std::vector<int32_t> polys;
    };
    std::unordered_map<int64_t, GridCell> grid;
    float cell_size = 256.0f;

    for (size_t i = 0; i < g_nav_polys.size(); i++) {
        UpdateAABB(g_nav_polys[i], aabbs[i]);
        int min_cx = (int)std::floor(aabbs[i].min_x / cell_size);
        int max_cx = (int)std::floor(aabbs[i].max_x / cell_size);
        int min_cy = (int)std::floor(aabbs[i].min_y / cell_size);
        int max_cy = (int)std::floor(aabbs[i].max_y / cell_size);
        int min_cz = (int)std::floor(aabbs[i].min_z / cell_size);
        int max_cz = (int)std::floor(aabbs[i].max_z / cell_size);
        for (int cx = min_cx; cx <= max_cx; cx++) {
            for (int cy = min_cy; cy <= max_cy; cy++) {
                for (int cz = min_cz; cz <= max_cz; cz++) {
                    int64_t key = (cx * 73856093) ^ (cy * 19349663) ^ (cz * 83492791);
                    grid[key].polys.push_back((int32_t)i);
                }
            }
        }
    }

    int32_t totalSplices = 0;

    for (size_t i = 0; i < g_nav_polys.size(); i++) {
        bool poly_modified = false;
        
        for (int32_t e = 0; e < g_nav_polys[i].num_vertices && !poly_modified; e++) {
            Vector3 pA = g_nav_polys[i].vertices[e];
            Vector3 pB = g_nav_polys[i].vertices[(e + 1) % g_nav_polys[i].num_vertices];
            
            Vector3 edgeVec = QM_Vector3Subtract(pB, pA);
            float edgeLenSqr = QM_Vector3LengthSqr(edgeVec);
            
            if (edgeLenSqr < 1.0f) continue;
            
            Vector3 pA_2d = { pA.x, pA.y, 0.0f };
            Vector3 pB_2d = { pB.x, pB.y, 0.0f };
            Vector3 edgeVec_2d = QM_Vector3Subtract(pB_2d, pA_2d);
            float edgeLenSqr_2d = QM_Vector3LengthSqr(edgeVec_2d);
            
            if (edgeLenSqr_2d < 0.001f) continue;
            
            int min_cx = (int)std::floor(aabbs[i].min_x / cell_size);
            int max_cx = (int)std::floor(aabbs[i].max_x / cell_size);
            int min_cy = (int)std::floor(aabbs[i].min_y / cell_size);
            int max_cy = (int)std::floor(aabbs[i].max_y / cell_size);
            int min_cz = (int)std::floor(aabbs[i].min_z / cell_size);
            int max_cz = (int)std::floor(aabbs[i].max_z / cell_size);
            
            std::vector<int32_t> checked_j; // Simple deduplication for cell overlap
            
            for (int cx = min_cx; cx <= max_cx && !poly_modified; cx++) {
                for (int cy = min_cy; cy <= max_cy && !poly_modified; cy++) {
                    for (int cz = min_cz; cz <= max_cz && !poly_modified; cz++) {
                        int64_t key = (cx * 73856093) ^ (cy * 19349663) ^ (cz * 83492791);
                        auto it = grid.find(key);
                        if (it == grid.end()) continue;

                        for (int32_t j : it->second.polys) {
                            if (poly_modified) break;
                            if (i == j) continue;
                            
                            bool already_checked = false;
                            for (int32_t c : checked_j) {
                                if (c == j) { already_checked = true; break; }
                            }
                            if (already_checked) continue;
                            checked_j.push_back(j);
                            
                            if (aabbs[j].max_x < aabbs[i].min_x || aabbs[j].min_x > aabbs[i].max_x ||
                                aabbs[j].max_y < aabbs[i].min_y || aabbs[j].min_y > aabbs[i].max_y ||
                                aabbs[j].max_z < aabbs[i].min_z || aabbs[j].min_z > aabbs[i].max_z) {
                                continue;
                            }

                            for (int32_t vj = 0; vj < g_nav_polys[j].num_vertices && !poly_modified; vj++) {
                                Vector3 vC = g_nav_polys[j].vertices[vj];
                                Vector3 vC_2d = { vC.x, vC.y, 0.0f };
                                
                                Vector3 toC_2d = QM_Vector3Subtract(vC_2d, pA_2d);
                                float t = QM_Vector3DotProduct(toC_2d, edgeVec_2d) / edgeLenSqr_2d;
                                
                                if (t > 0.0f && t < 1.0f) {
                                    Vector3 projC_2d = QM_Vector3Add(pA_2d, QM_Vector3Scale(edgeVec_2d, t));
                                    
                                    if (QM_Vector3DistanceSqr(vC_2d, projC_2d) < 16.0f) { 
                                        Vector3 projC_3d = QM_Vector3Add(pA, QM_Vector3Scale(edgeVec, t));
                                        float dz = std::abs(vC.z - projC_3d.z);
                                        
                                        if (dz <= 24.0f) {
                                            if (QM_Vector3DistanceSqr(pA_2d, projC_2d) <= 1.0f) {
                                                // Snap vC to corner pA
                                                g_nav_polys[j].vertices[vj].x = pA.x;
                                                g_nav_polys[j].vertices[vj].y = pA.y;
                                                totalSplices++;
                                            } else if (QM_Vector3DistanceSqr(pB_2d, projC_2d) <= 1.0f) {
                                                // Snap vC to corner pB
                                                g_nav_polys[j].vertices[vj].x = pB.x;
                                                g_nav_polys[j].vertices[vj].y = pB.y;
                                                totalSplices++;
                                            } else {
                                                nav_poly_t newPoly = g_nav_polys[i];
                                                if (newPoly.num_vertices < MAX_WINDING_POINTS) {
                                                    for (int32_t k = newPoly.num_vertices; k > e + 1; k--) {
                                                        newPoly.vertices[k] = newPoly.vertices[k - 1];
                                                    }
                                                    newPoly.vertices[e + 1] = projC_3d;
                                                    newPoly.num_vertices++;

                                                    g_nav_polys[i] = newPoly;
                                                    
                                                    // Snap vC to perfectly align with the new spliced point in 2D to guarantee TwinLinking
                                                    g_nav_polys[j].vertices[vj].x = projC_3d.x;
                                                    g_nav_polys[j].vertices[vj].y = projC_3d.y;

                                                    poly_modified = true;
                                                    totalSplices++;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        
        if (totalSplices > 10000) {
            gi.dprintf("NavMesh WARNING: Aborting Edge Splicing due to infinite loop! (Splices > 10000)\n");
            break;
        }
        
        if (poly_modified) {
            i--; // Re-process poly i to check if more vertices need splicing
        }
    }
    
    if (totalSplices > 0) {
        gi.dprintf("NavMesh: Spliced %d T-Junction vertices.\n", totalSplices);
    }
}

/**
*	@brief Triangulates all polygons in the navmesh by connecting their center (meridian mid-point) to their outer vertices.
*	@note This guarantees all faces are strictly planar triangles, eliminating precision issues and non-convex splicing artifacts.
**/
static void Nav_TriangulatePolygons() {
    std::vector<nav_poly_t> new_polys;
    new_polys.reserve(g_nav_polys.size() * 4); // Roughly 4 triangles per quad

    for (const auto& poly : g_nav_polys) {
        if (poly.num_vertices < 3) continue;

        // The "meridian mid-point" is the calculated center
        Vector3 meridian = poly.center;

        for (int32_t i = 0; i < poly.num_vertices; i++) {
            Vector3 v1 = poly.vertices[i];
            Vector3 v2 = poly.vertices[(i + 1) % poly.num_vertices];

            nav_poly_t tri = {};
            tri.poly_id = new_polys.size();
            tri.num_vertices = 3;
            tri.vertices[0] = v1;
            tri.vertices[1] = v2;
            tri.vertices[2] = meridian;
            
            // The center of this triangle
            tri.center = (v1 + v2 + meridian) / 3.0f;
            tri.normal = poly.normal;
            tri.bsp_leaf_id = poly.bsp_leaf_id;

            new_polys.push_back(tri);
        }
    }

    // Transfer triangulated polygons into the engine's nav_vector container.
    g_nav_polys.clear();
    for (const auto &tri : new_polys) {
        g_nav_polys.push_back(tri);
    }
    gi.dprintf("NavMesh: Triangulated into %zu faces.\n", g_nav_polys.size());
}

void Nav_BuildHalfEdgeMesh() {
    g_nav_vertices.clear();
    g_nav_halfedges.clear();
    g_nav_faces.clear();

    if (g_nav_polys.empty()) return;

    // Partition the massive extracted polys into a structured sub-polygon grid
    Nav_PartitionPolygons();

    // No prune pass needed here. 
    // Since we enabled full sloped CSG subtraction in Nav_DoExtractionWork, 
    // overlapping brushes are perfectly clipped and there are no buried phantom polygons.

    // Resolve any remaining T-Junctions by splicing (mostly non-grid aligned or dynamic cuts)
    Nav_ResolveTJunctionsByEdgeSplicing();

    // Triangulation is disabled because the KD-Tree sub-polygons are naturally planar N-gons.
    // Triangulating them creates unnecessary extra geometry and internal spokes that confuse pathing.
    // Nav_TriangulatePolygons();

    std::unordered_map<int64_t, std::vector<int32_t>> vertex_grid;
    
    // Hash function to deduplicate vertices.
    // We enforce exact 3D matching (with a tiny floating point epsilon) to prevent
    // 'canonical vertex drift' where an entire staircase merges into a single vertex,
    // which breaks the twin linking and causes isolated geometry islands.
    auto GetVertexIndex = [&](const Vector3& p) -> int32_t {
        int64_t cx = (int64_t)std::floor(p.x / 4.0f);
        int64_t cy = (int64_t)std::floor(p.y / 4.0f);
        int64_t cz = (int64_t)std::floor(p.z / 4.0f);
        
        for (int64_t ox = -1; ox <= 1; ox++) {
            for (int64_t oy = -1; oy <= 1; oy++) {
                for (int64_t oz = -1; oz <= 1; oz++) {
                    int64_t key = ((cx + ox) * 73856093) ^ ((cy + oy) * 19349663) ^ ((cz + oz) * 83492791);
                    auto it = vertex_grid.find(key);
                    if (it != vertex_grid.end()) {
                        for (int32_t idx : it->second) {
                            float dx = g_nav_vertices[idx].x - p.x;
                            float dy = g_nav_vertices[idx].y - p.y;
                            float dz = g_nav_vertices[idx].z - p.z;
                            if (dx * dx + dy * dy + dz * dz < 0.01f) {
                                return idx;
                            }
                        }
                    }
                }
            }
        }
        
        int32_t new_idx = (int32_t)g_nav_vertices.size();
        g_nav_vertices.push_back(p);
        
        int64_t key = (cx * 73856093) ^ (cy * 19349663) ^ (cz * 83492791);
        vertex_grid[key].push_back(new_idx);
        
        return new_idx;
    };

    for (size_t i = 0; i < g_nav_polys.size(); i++) {
        const nav_poly_t& poly = g_nav_polys[i];
        
        nav_face_t face = {};
        face.face_id = i;
        face.num_edges = poly.num_vertices;
        face.center = poly.center;
        face.normal = poly.normal;
        face.bsp_leaf_id = poly.bsp_leaf_id;
        face.first_edge_idx = g_nav_halfedges.size();

        std::vector<int32_t> v_indices(poly.num_vertices);
        for (int v = 0; v < poly.num_vertices; v++) {
            v_indices[v] = GetVertexIndex(poly.vertices[v]);
        }

        // Compute clearance
        float min_dist = 99999.0f;
        for (int v = 0; v < poly.num_vertices; v++) {
            Vector3 a = poly.vertices[v];
            Vector3 b = poly.vertices[(v + 1) % poly.num_vertices];
            
            // distance from center to line segment a-b
            Vector3 edge = QM_Vector3Subtract(b, a);
            Vector3 toCenter = QM_Vector3Subtract(face.center, a);
            float edgeLenSq = QM_Vector3LengthSqr(edge);
            float dist = 0.0f;
            if (edgeLenSq > 0.0001f) {
                float t = QM_Vector3DotProduct(toCenter, edge) / edgeLenSq;
                t = std::max(0.0f, std::min(1.0f, t));
                Vector3 proj = QM_Vector3MultiplyAdd(a, t, edge);
                dist = QM_Vector3Distance(face.center, proj);
            } else {
                dist = QM_Vector3Distance(face.center, a);
            }
            if (dist < min_dist) min_dist = dist;
        }
        face.clearance = min_dist;

        for (int v = 0; v < poly.num_vertices; v++) {
            int32_t curr_v = v_indices[v];
            int32_t next_v = v_indices[(v + 1) % poly.num_vertices];

            nav_halfedge_t he = {};
            he.vertex_idx = curr_v;
            he.face_idx = face.face_id;
            he.twin_idx = -1; // Default to boundary

            int32_t he_idx = g_nav_halfedges.size();
            he.next_idx = face.first_edge_idx + ((v + 1) % poly.num_vertices);
            
            g_nav_halfedges.push_back(he);
        }

        g_nav_faces.push_back(face);
    }
    
    // Twin Linking using Z-tolerant 2D overlap check
    // This connects stair steps that are physically separated by up to 18 units vertically,
    // ensuring the half-edge graph remains fully conformal across varying height terrain.
    
    std::unordered_map<int64_t, std::vector<int32_t>> twin_grid;
    for (size_t j = 0; j < g_nav_halfedges.size(); j++) {
        Vector3 b2 = g_nav_vertices[g_nav_halfedges[g_nav_halfedges[j].next_idx].vertex_idx];
        int64_t cx = (int64_t)std::floor(b2.x / 16.0f);
        int64_t cy = (int64_t)std::floor(b2.y / 16.0f);
        int64_t key = (cx * 73856093) ^ (cy * 19349663);
        twin_grid[key].push_back((int32_t)j);
    }
    
    for (size_t i = 0; i < g_nav_halfedges.size(); i++) {
        if (g_nav_halfedges[i].twin_idx != -1) continue;

        nav_halfedge_t& heA = g_nav_halfedges[i];
        Vector3 a1 = g_nav_vertices[heA.vertex_idx];
        Vector3 a2 = g_nav_vertices[g_nav_halfedges[heA.next_idx].vertex_idx];

        float bestZ = 99999.0f;
        int32_t bestTwin = -1;

        int64_t cx = (int64_t)std::floor(a1.x / 16.0f);
        int64_t cy = (int64_t)std::floor(a1.y / 16.0f);
        
        for (int64_t ox = -1; ox <= 1; ox++) {
            for (int64_t oy = -1; oy <= 1; oy++) {
                int64_t key = ((cx + ox) * 73856093) ^ ((cy + oy) * 19349663);
                auto it = twin_grid.find(key);
                if (it == twin_grid.end()) continue;
                
                for (int32_t j : it->second) {
                    if (i == j) continue; // Don't twin with self!
                    if (g_nav_halfedges[j].twin_idx != -1) continue;

                    nav_halfedge_t& heB = g_nav_halfedges[j];
                    Vector3 b1 = g_nav_vertices[heB.vertex_idx];
                    Vector3 b2 = g_nav_vertices[g_nav_halfedges[heB.next_idx].vertex_idx];

                    float dx1 = a1.x - b2.x;
                    float dy1 = a1.y - b2.y;
                    float dz1 = std::abs(a1.z - b2.z);

                    float dx2 = a2.x - b1.x;
                    float dy2 = a2.y - b1.y;
                    float dz2 = std::abs(a2.z - b1.z);

                    // Tolerate up to a 4 unit horizontal gap (16.0f squared) to handle BSP T-junctions
                    if (dx1 * dx1 + dy1 * dy1 < 16.0f && dz1 <= 24.0f &&
                        dx2 * dx2 + dy2 * dy2 < 16.0f && dz2 <= 24.0f) {
                        
                        // If there are multiple overlapping edges, pick the one with the closest Z match.
                        float totalZ = dz1 + dz2;
                        if (totalZ < bestZ) {
                            bestZ = totalZ;
                            bestTwin = static_cast<int32_t>(j);
                        }
                    }
                }
            }
        }

        if (bestTwin != -1) {
            g_nav_halfedges[i].twin_idx = bestTwin;
            g_nav_halfedges[bestTwin].twin_idx = static_cast<int32_t>(i);
            
            // Use the actual Z height of the shared edge vertices, not the face centers!
            // Face centers are wildly inaccurate for long slopes or ramps.
            float z1 = (g_nav_vertices[g_nav_halfedges[i].vertex_idx].z + g_nav_vertices[g_nav_halfedges[g_nav_halfedges[i].next_idx].vertex_idx].z) * 0.5f;
            float z2 = (g_nav_vertices[g_nav_halfedges[bestTwin].vertex_idx].z + g_nav_vertices[g_nav_halfedges[g_nav_halfedges[bestTwin].next_idx].vertex_idx].z) * 0.5f;
            g_nav_halfedges[i].z_diff = z2 - z1;
            g_nav_halfedges[bestTwin].z_diff = z1 - z2;
        }
    }

    // Secondary Twin Linking pass for T-Junctions and Overlaps
    // Only links edges that overlap in 2D but have a strict Z-tolerance (<= 24.0f)
    // to prevent accidentally linking catwalks to floors below them.
    
    std::unordered_map<int64_t, std::vector<int32_t>> overlap_grid;
    for (size_t j = 0; j < g_nav_halfedges.size(); j++) {
        nav_halfedge_t& heB = g_nav_halfedges[j];
        Vector3 b1 = g_nav_vertices[heB.vertex_idx];
        Vector3 b2 = g_nav_vertices[g_nav_halfedges[heB.next_idx].vertex_idx];
        Vector3 center = QM_Vector3Scale(QM_Vector3Add(b1, b2), 0.5f);
        
        int64_t cx = (int64_t)std::floor(center.x / 128.0f);
        int64_t cy = (int64_t)std::floor(center.y / 128.0f);
        int64_t key = (cx * 73856093) ^ (cy * 19349663);
        overlap_grid[key].push_back((int32_t)j);
    }
    
    for (size_t i = 0; i < g_nav_halfedges.size(); i++) {
        if (g_nav_halfedges[i].twin_idx != -1) continue;

        nav_halfedge_t& heA = g_nav_halfedges[i];
        Vector3 a1 = g_nav_vertices[heA.vertex_idx];
        Vector3 a2 = g_nav_vertices[g_nav_halfedges[heA.next_idx].vertex_idx];

        Vector3 dA = QM_Vector3Subtract(a2, a1);
        dA.z = 0.0f;
        float lenA = QM_Vector3Length(dA);
        if (lenA < 0.1f) continue;
        Vector3 dirA = QM_Vector3Scale(dA, 1.0f / lenA);

        float bestOverlap = -1.0f;
        int32_t bestTwin = -1;

        Vector3 centerA = QM_Vector3Scale(QM_Vector3Add(a1, a2), 0.5f);
        int64_t cx = (int64_t)std::floor(centerA.x / 128.0f);
        int64_t cy = (int64_t)std::floor(centerA.y / 128.0f);
        
        for (int64_t ox = -1; ox <= 1; ox++) {
            for (int64_t oy = -1; oy <= 1; oy++) {
                int64_t key = ((cx + ox) * 73856093) ^ ((cy + oy) * 19349663);
                auto it = overlap_grid.find(key);
                if (it == overlap_grid.end()) continue;
                
                for (int32_t j : it->second) {
                    if (i == j) continue;
                    if (g_nav_halfedges[j].twin_idx != -1) continue;

                    nav_halfedge_t& heB = g_nav_halfedges[j];
                    Vector3 b1 = g_nav_vertices[heB.vertex_idx];
                    Vector3 b2 = g_nav_vertices[g_nav_halfedges[heB.next_idx].vertex_idx];

                    Vector3 dB = QM_Vector3Subtract(b2, b1);
                    dB.z = 0.0f;
                    float lenB = QM_Vector3Length(dB);
                    if (lenB < 0.1f) continue;
                    Vector3 dirB = QM_Vector3Scale(dB, 1.0f / lenB);

                    if (QM_Vector3DotProduct(dirA, dirB) > -0.95f) continue;

                    Vector3 a1_to_b1 = QM_Vector3Subtract(b1, a1);
                    a1_to_b1.z = 0.0f;
                    float proj = QM_Vector3DotProduct(a1_to_b1, dirA);
                    Vector3 closestPt = QM_Vector3Add(a1, QM_Vector3Scale(dirA, proj));
                    closestPt.z = 0.0f;
                    Vector3 b1_2d = b1; b1_2d.z = 0.0f;
                    
                    if (QM_Vector3DistanceSqr(b1_2d, closestPt) > 4.0f) continue;

                    float u1 = proj;
                    Vector3 a1_to_b2 = QM_Vector3Subtract(b2, a1);
                    a1_to_b2.z = 0.0f;
                    float u2 = QM_Vector3DotProduct(a1_to_b2, dirA);

                    float minU = std::min(u1, u2);
                    float maxU = std::max(u1, u2);
                    float overlapStart = std::max(0.0f, minU);
                    float overlapEnd = std::min(lenA, maxU);
                    float overlapLen = overlapEnd - overlapStart;

                    if (overlapLen > 1.0f) {
                        float dz = std::abs((a1.z + a2.z) * 0.5f - (b1.z + b2.z) * 0.5f);
                        // STRICT Z tolerance to prevent vertical scrambling!
                        if (dz <= 24.0f) {
                            if (overlapLen > bestOverlap) {
                                bestOverlap = overlapLen;
                                bestTwin = static_cast<int32_t>(j);
                            }
                        }
                    }
                }
            }
        }

        if (bestTwin != -1) {
            g_nav_halfedges[i].twin_idx = bestTwin;
            g_nav_halfedges[bestTwin].twin_idx = static_cast<int32_t>(i);
            
            // Use the actual Z height of the shared edge vertices, not the face centers!
            float z1 = (g_nav_vertices[g_nav_halfedges[i].vertex_idx].z + g_nav_vertices[g_nav_halfedges[g_nav_halfedges[i].next_idx].vertex_idx].z) * 0.5f;
            float z2 = (g_nav_vertices[g_nav_halfedges[bestTwin].vertex_idx].z + g_nav_vertices[g_nav_halfedges[g_nav_halfedges[bestTwin].next_idx].vertex_idx].z) * 0.5f;
            g_nav_halfedges[i].z_diff = z2 - z1;
            g_nav_halfedges[bestTwin].z_diff = z1 - z2;
        }
    }

    gi.dprintf("NavMesh Half-Edge Generation Completed. %d vertices, %d half-edges, %d faces.\n", 
               g_nav_vertices.size(), g_nav_halfedges.size(), g_nav_faces.size());
}

/**
*	@brief	Wrapper function to build the KD-Tree for the navmesh faces.
**/
void Nav_BuildKDTree() {
    g_nav_nodes.clear();
	if ( g_nav_faces.empty() ) return;
	
    BuildKDNode(0, g_nav_faces.size(), 0);

	for (int32_t i = 0; i < g_nav_faces.size(); i++) {
		g_nav_faces[i].face_id = i;
        
        // CRITICAL FIX: BuildKDNode calls std::partition, which physically shuffles g_nav_faces.
        // We must update the back-pointers in the half-edges so they point to the NEW shuffled face index!
        // Otherwise, Nav_FindPath's A* will jump to random, completely unconnected faces!
        for (int32_t e = 0; e < g_nav_faces[i].num_edges; ++e) {
            g_nav_halfedges[g_nav_faces[i].first_edge_idx + e].face_idx = i;
        }
	}

    gi.dprintf("NavMesh KD-Tree Generation Completed. %d nodes created.\n", g_nav_nodes.size());
}
 
 
