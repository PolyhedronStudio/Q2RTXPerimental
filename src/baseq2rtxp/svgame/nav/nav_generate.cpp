/********************************************************************
*
*
*	ServerGame: Navigation edge-mesh generation and KD-tree construction.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "nav_generate.h"
#include "nav_path.h"
#include "nav_thread.h"
#include <algorithm>
#include <unordered_map>
#include <utility>
#include "shared/cm/cm_model.h"
#include "shared/formats/format_bsp.h"

// Entity includes for IsSubClassType checking
#include "svgame/entities/func/svg_func_door.h"
#include "svgame/entities/func/svg_func_door_rotating.h"
#include "svgame/entities/func/svg_func_wall.h"



/**
*	Global navmesh data containers.
**/
//! Global navmesh polygon data (temporary during build).
nav_vector_t<nav_poly_t> g_nav_polys;

//! Half-edge mesh global vertices data
std::vector<Vector3> g_nav_vertices;
//! Half-edge mesh global half-edges data
std::vector<nav_halfedge_t> g_nav_halfedges;
//! Half-edge mesh global faces data
std::vector<nav_face_t> g_nav_faces;
//! Entity graph traversal edge sets
std::vector<std::vector<int32_t>> g_nav_entity_edges;

//! Global navmesh kdtree node data.
nav_vector_t<nav_kdtree_node_t> g_nav_nodes;
//! Global navmesh leaf link data.
nav_vector_t<nav_leaf_link_t> g_nav_leaf_links;
//! Global navmesh leaf polygon ID data.
nav_vector_t<int32_t> g_nav_leaf_poly_ids;

static constexpr int32_t KDTREE_MAX_BIN = 1024;

//! Node result for negative space during BSP tree construction.
static constexpr int32_t NODE_NEGATIVE = -1;

//! Maximum number of points in a winding (polygon) for navmesh generation.
static constexpr int32_t MAX_WINDING_POINTS = 1024;

/**
*	@brief	 A winding_t represents a polygon defined by a set of points, used during the navmesh generation process.
**/
struct winding_t {
	//! The number of points in the winding (polygon).
	int32_t num_points = 0;
	//! The array of points that define the winding (polygon). The maximum number of points is defined by MAX_WINDING_POINTS.
	Vector3 points[ MAX_WINDING_POINTS ] = {};
	//! Entity ID this polygon belongs to (e.g. for doors), or ENTITYNUM_NONE if world.
	int32_t entity_id = ENTITYNUM_NONE;
};



/**
*
*
*
*	Navigation Mesh Core:
* 
* 
* 
**/
/**
*	@brief	 Generate the navmesh asynchronously.
**/
/**
* @brief Generate the navmesh asynchronously from the console command path.
* @note This only submits the worker job; the actual extraction runs off-thread.
**/
void Nav_GenerateCommand() {
    /**
	*	Start the async generation.
	**/
    Nav_StartAsyncGeneration();
}

/**
*	@brief	Deallocate all navmesh data and clear the global containers.
**/
/**
* @brief Clear all global navmesh containers and release the current build state.
**/
void Nav_Clear() {
	/**
	*	Just clear all navmesh data containers.
	**/
	g_nav_polys.clear();
	g_nav_vertices.clear();
	g_nav_halfedges.clear();
	g_nav_faces.clear();
	g_nav_nodes.clear();
	g_nav_leaf_links.clear();
	g_nav_leaf_poly_ids.clear();

	// We've cleared all navmesh data, so we can also clear any async generation state if needed.
	gi.dprintf( "NavMesh memory cleared.\n" );
}

/**
*	@brief	Print the current standalone nav generation status to the server console.
*	@note	This keeps the nav system self-contained so the command path does not need nav2/nav3 helpers.
**/
/**
* @brief Print the current nav generation status to the server console.
* @note This reports the worker progress snapshot without depending on older nav helpers.
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
*
*
*
*	Tools for Winding (Polygon) Manipulation:
*
*
*
**/
/**
* @brief Construct a large base winding for a collision plane.
* @param p Plane to expand into a temporary polygon.
* @return Four-point winding centered on the plane.
**/
static winding_t BaseWindingForPlane( const cm_plane_t *p ) {
    winding_t w;
    w.num_points = 4;
    
    int32_t max = -1;
    float maxv = -1;
    for (int32_t i=0; i<3; i++) {
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

/**
* @brief Clip a winding against one plane in place.
* @param in Polygon to clip.
* @param split Plane to clip against.
* @param epsilon Tolerance used to classify points near the plane.
* @return True when the winding survives the clip.
**/
static bool ChopWindingInPlace( winding_t *in, const cm_plane_t *split, float epsilon ) {
    float dists[MAX_WINDING_POINTS + 4];
    int32_t sides[MAX_WINDING_POINTS + 4];
    int32_t counts[3] = {0, 0, 0};
    
    for (int32_t i=0; i<in->num_points; i++) {
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
    
    for (int32_t i=0; i<in->num_points; i++) {
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
        for (int32_t j=0; j<3; j++) {
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
/**
* @brief Split a winding into front and back fragments against a plane.
* @param in Source polygon.
* @param split Plane used to split the polygon.
* @param epsilon Tolerance used for plane-side classification.
* @param poly_normal Original polygon normal used to classify coplanar fragments.
* @param front Output front fragment.
* @param back Output back fragment.
**/
static void SplitWinding( const winding_t *in, const cm_plane_t *split, float epsilon, const Vector3 &poly_normal, winding_t *front, winding_t *back ) {
    float dists[MAX_WINDING_POINTS + 4];
    int32_t sides[MAX_WINDING_POINTS + 4];
    int32_t counts[3] = {0, 0, 0};
    
    for (int32_t i=0; i<in->num_points; i++) {
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
    
    for (int32_t i=0; i<in->num_points; i++) {
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
        for (int32_t j=0; j<3; j++) {
            if (split->normal[j] == 1) mid[j] = split->dist;
            else if (split->normal[j] == -1) mid[j] = -split->dist;
            else mid[j] = p1[j] + dot * (p2[j] - p1[j]);
        }
        
        front->points[front->num_points++] = mid;
        back->points[back->num_points++] = mid;
    }
}

/**
* @brief Shifts a plane by a given 3D offset.
**/
static cm_plane_t GetShiftedPlane(const cm_plane_t* plane, const Vector3& offset) {
    cm_plane_t p = *plane;
    p.dist += QM_Vector3DotProduct(Vector3(p.normal[0], p.normal[1], p.normal[2]), offset);
    return p;
}

/**
* @brief Computes the maximum Z height of a brush by physically building its geometry.
* @param b The brush to analyze.
* @param offset The world offset to apply to the brush's planes.
* @return The maximum Z height of the brush, or 999999.0f if the brush could not be constructed.
**/
static float GetBrushMaxZ( const mbrush_t *b, const Vector3& offset ) {
	float max_z = -999999.0f;
	
    // Try to build a face for every plane of the brush
	for ( int32_t i = 0; i < b->numsides; i++ ) {
        mbrushside_t* side = &b->firstbrushside[i];
        cm_plane_t p = GetShiftedPlane(side->plane, offset);
        
        winding_t w = BaseWindingForPlane( &p );
        bool valid = true;
        
        for ( int32_t j = 0; j < b->numsides && valid; j++ ) {
            if ( i == j ) continue;
            cm_plane_t clip = GetShiftedPlane(b->firstbrushside[j].plane, offset);
            if ( !ChopWindingInPlace( &w, &clip, 0.1f ) ) {
                valid = false;
            }
        }
        
        // If this face is valid, check its vertices for the maximum Z coordinate.
        if ( valid ) {
            for ( int32_t pt = 0; pt < w.num_points; pt++ ) {
                if ( w.points[pt].z > max_z ) {
                    max_z = w.points[pt].z;
                }
            }
        }
	}
    
    // If the brush geometry could not be constructed for some reason, return a high Z value to treat it as a wall.
	return (max_z == -999999.0f) ? 999999.0f : max_z;
}

/**
* @brief Computes which planes of a brush are actually part of its physical surface (not redundant).
**/
static std::vector<bool> GetBrushActivePlanes( const mbrush_t *b, const Vector3& offset ) {
    std::vector<bool> active(b->numsides, false);
    for ( int32_t i = 0; i < b->numsides; i++ ) {
        cm_plane_t p = GetShiftedPlane(b->firstbrushside[i].plane, offset);
        winding_t w = BaseWindingForPlane( &p );
        bool valid = true;
        for ( int32_t j = 0; j < b->numsides && valid; j++ ) {
            if ( i == j ) continue;
            cm_plane_t clip = GetShiftedPlane(b->firstbrushside[j].plane, offset);
            if ( !ChopWindingInPlace( &w, &clip, 0.1f ) ) {
                valid = false;
            }
        }
        if ( valid && w.num_points >= 3 ) {
            active[i] = true;
        }
    }
    return active;
}

/**
* @brief Check whether a fragment is completely outside one brush.
* @param frag Candidate polygon fragment.
* @param b Brush to test against.
* @param offset The world offset to apply to the brush's planes.
* @param plane_active Precomputed boolean array of which planes are non-redundant.
* @return True when the fragment can be kept without further clipping.
**/
static bool IsFragmentCompletelyOutsideBrush(const winding_t* frag, const mbrush_t* b, const Vector3& offset, const std::vector<bool>& plane_active, float expand = 0.0f) {
    for (int32_t j = 0; j < b->numsides; j++) {
        if (!plane_active[j]) continue;

        mbrushside_t* side = &b->firstbrushside[j];
        cm_plane_t p = GetShiftedPlane(side->plane, offset);
        p.dist += expand;
        
        // If all points of the fragment are strictly in front of this plane,
        // the entire fragment is outside the convex brush.
        bool completely_outside = true;
        for (int32_t i = 0; i < frag->num_points; i++) {
            float d = QM_Vector3DotProduct(frag->points[i], Vector3(p.normal[0], p.normal[1], p.normal[2])) - p.dist;
            // 0.1f tolerance to account for floating point inaccuracies
            if (d <= 0.1f) {
                completely_outside = false;
                break;
            }
        }
        if (completely_outside) {
            return true;
        }
    }
    return false;
}

/**
*	@brief Subtracts a brush from a list of polygon fragments, replacing them with the resulting smaller fragments.
**/
/**
* @brief Subtract one blocking brush from a set of polygon fragments.
* @param fragments Fragments that will be clipped in place.
* @param b Blocking brush to subtract.
* @param offset The world offset to apply to the brush's planes.
* @param poly_normal Original polygon normal used for coplanar tests.
**/
static void SubtractBrushFromWindings( std::vector<winding_t> &fragments, const mbrush_t *b, const Vector3& offset, const Vector3 &poly_normal ) {
	std::vector<winding_t> next_fragments;
	std::vector<bool> plane_active = GetBrushActivePlanes(b, offset);
	
	for ( const winding_t& frag : fragments ) {
		// Optimization: If the fragment is entirely in front of any plane of the brush, it is completely outside the brush.
		// We can skip splitting it entirely, saving massive amounts of fragmentation!
		if (IsFragmentCompletelyOutsideBrush(&frag, b, offset, plane_active, 0.0f)) {
			next_fragments.push_back(frag);
			continue;
		}

		winding_t inside_part = frag;
		bool entirely_inside = true;
		
		// Slicing against every plane of the brush
		for ( int32_t j = 0; j < b->numsides; j++ ) {
			if (!plane_active[j]) continue;
			
			mbrushside_t* side = &b->firstbrushside[j];
			cm_plane_t p = GetShiftedPlane(side->plane, offset);
			
			winding_t front = {};
			winding_t back = {};
			SplitWinding(&inside_part, &p, 0.1f, poly_normal, &front, &back);
			
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
/**
* @brief Attempt to merge two coplanar convex windings.
* @param w1 First winding.
* @param w2 Second winding.
* @param normal Plane normal shared by both windings.
* @param out Output merged winding.
* @return True when a valid merged polygon could be produced.
**/
static bool TryMergeWindings(const winding_t* w1, const winding_t* w2, const Vector3& normal, winding_t* out) {
    if (!w1 || !w2 || !out) return false;
    
    // Do not merge polygons that belong to different entities!
    // This preserves the special door boundary edges for the nav graph.
    if (w1->entity_id != w2->entity_id) {
        return false;
    }
    int32_t match_i1 = -1, match_i2 = -1;
    for (int32_t i1 = 0; i1 < w1->num_points; i1++) {
        Vector3 a1 = w1->points[i1];
        Vector3 b1 = w1->points[(i1 + 1) % w1->num_points];

        for (int32_t i2 = 0; i2 < w2->num_points; i2++) {
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
    int32_t n1 = w1->num_points;
    int32_t n2 = w2->num_points;

    for (int32_t j = 0; j < n1; j++) {
        out->points[out->num_points++] = w1->points[(match_i1 + 1 + j) % n1];
    }
    for (int32_t j = 0; j < n2 - 2; j++) {
        out->points[out->num_points++] = w2->points[(match_i2 + 2 + j) % n2];
    }

    // 3. Check convexity and simplify
    winding_t simple = *out;
    for (int32_t j = 0; j < simple.num_points; j++) {
        Vector3 p1 = simple.points[j];
        Vector3 p2 = simple.points[(j + 1) % simple.num_points];
        Vector3 p3 = simple.points[(j + 2) % simple.num_points];
        Vector3 dir1 = QM_Vector3Subtract(p2, p1);
        Vector3 dir2 = QM_Vector3Subtract(p3, p2);
        
        Vector3 cross = QM_Vector3CrossProduct(dir1, dir2);
        float dot = QM_Vector3DotProduct(cross, normal);

        if (dot < -0.01f) {
            return false; // Concave, cannot merge
        }
        if (std::abs(dot) < 0.01f) {
            // Collinear points, remove the middle one
            for (int32_t k = j + 1; k < simple.num_points - 1; k++) {
                simple.points[k] = simple.points[k + 1];
            }
            simple.num_points--;
            j--;
        }
    }
    
    if (simple.num_points < 3 || simple.num_points > MAX_WINDING_POINTS) return false; // Too complex or invalid

    *out = simple;
    return true;
}

/**
* @brief Checks whether the given node pointer securely falls within the loaded bsp nodes or leafs boundaries.
**/
static bool IsNodeValid(const bsp_t* bsp, const mnode_t* node) {
    if (!node || !bsp) return false;
    
    // Check if it safely falls within the bsp->nodes array.
    if (node >= bsp->nodes && node < bsp->nodes + bsp->numnodes) {
        return true;
    }
    
    // Check if it safely falls within the bsp->leafs array.
    const mleaf_t* leaf = reinterpret_cast<const mleaf_t*>(node);
    if (leaf >= bsp->leafs && leaf < bsp->leafs + bsp->numleafs) {
        return true;
    }
    
    // Likely uninitialized memory, or synthetic hulls. We don't process these here.
    return false;
}

/**
* @brief Recursively traverse a BSP tree node and collect all brush indices that belong to it.
**/
static void CollectModelBrushes(bsp_t* bsp, mnode_t* node, int32_t entity_id, const Vector3& offset, std::vector<int32_t>& brush_entity_ids, std::vector<Vector3>& brush_offsets, std::vector<bool>* brush_is_active = nullptr) {
    /**
    *   Sanity check to prevent out-of-bounds pointer reads if a map happens to have corrupted inline trees
    *   or synthetic engine hulls that point outside normal geometry pools.
    **/
    if (!IsNodeValid(bsp, node)) {
        return;
    }
    
    // If it's a leaf, process its brushes
    if (node->plane == nullptr) {
        mleaf_t* leaf = (mleaf_t*)node;
        
        // For each leaf, process its brushes
        for (int32_t i = 0; i < leaf->numleafbrushes; i++) {
            mbrush_t* b = leaf->firstleafbrush[i];
            int32_t brush_num = b - bsp->brushes;
            
            if (brush_num >= 0 && brush_num < bsp->numbrushes) {
                brush_entity_ids[brush_num] = entity_id;
                brush_offsets[brush_num] = offset;
                if (brush_is_active != nullptr) {
                    (*brush_is_active)[brush_num] = true;
                }
            }
        }
        return;
    }
    
    // Recurse into children.
    CollectModelBrushes(bsp, node->children[0], entity_id, offset, brush_entity_ids, brush_offsets, brush_is_active);
    CollectModelBrushes(bsp, node->children[1], entity_id, offset, brush_entity_ids, brush_offsets, brush_is_active);
}

/**
* @brief Split fragments against a door brush. Keeps the outside parts (unchanged) and the inside part (updated to the door's entity ID).
**/
static void SplitWindingsByEntityBrush(std::vector<winding_t>& fragments, const mbrush_t* b, int32_t entity_id, const Vector3& offset, float expand = 4.0f) {
    std::vector<winding_t> next_fragments;
    std::vector<bool> plane_active = GetBrushActivePlanes(b, offset);
    
    for (const winding_t& frag : fragments) {
        if (IsFragmentCompletelyOutsideBrush(&frag, b, offset, plane_active, expand)) {
            next_fragments.push_back(frag);
            continue;
        }

        winding_t inside_part = frag;
        bool entirely_inside = true;
        
        for (int32_t j = 0; j < b->numsides; j++) {
            if (!plane_active[j]) continue;
            
            mbrushside_t* side = &b->firstbrushside[j];
            cm_plane_t plane = GetShiftedPlane(side->plane, offset);
            plane.dist += expand; // Expand splitting planes outwards by a small amount to prevent slivers
            
            winding_t front = {};
            winding_t back = {};
            SplitWinding(&inside_part, &plane, 0.1f, Vector3(0,0,1) /* not used */, &front, &back);
            
            if (front.num_points >= 3) {
                next_fragments.push_back(front); // This part is outside, it keeps its original entity_id
                entirely_inside = false;
            }
            inside_part = back; // The back part is inside this plane, keep checking it against other planes
        }
        
        // Whatever is left in inside_part after checking all planes is completely inside the brush!
        if (inside_part.num_points >= 3) {
            inside_part.entity_id = entity_id;
            next_fragments.push_back(inside_part);
        }
    }
    
    fragments = next_fragments;
}

/**
*	@brief Extract walkable polygons from the current map's collision model and store them in g_nav_polys.
**/
/**
* @brief Extract walkable polygons from the current map collision model.
* @note This is the first stage of navmesh generation and fills g_nav_polys.
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

    
    // Parse the runtime edicts to find active bmodels.
    // We maintain a boolean array to ONLY extract brushes that are actively instantiated!
    std::vector<int32_t> brush_entity_ids(bsp->numbrushes, ENTITYNUM_NONE);
    std::vector<Vector3> brush_offsets(bsp->numbrushes, Vector3(0.0f, 0.0f, 0.0f));
    std::vector<bool> brush_is_active(bsp->numbrushes, false);
    
    // World geometry is always active, and has no offset.
    if (bsp->models != nullptr && bsp->nummodels > 0 && bsp->models[0].headnode != nullptr) {
        CollectModelBrushes(bsp, bsp->models[0].headnode, ENTITYNUM_NONE, Vector3(0.0f, 0.0f, 0.0f), brush_entity_ids, brush_offsets, &brush_is_active);
    }
    
    for (int32_t i = 1; i < g_edict_pool.num_edicts; i++) {
        svg_base_edict_t* edict = g_edicts[i];
        if ( !SVG_Entity_IsActive( edict ) ) {
            continue;
        }

        if (edict->model.ptr != nullptr && edict->model.size() >= 2 && edict->model[ 0 ] == '*' ) {
            int32_t model_num = gi.modelindex(edict->model.ptr);
            if (model_num > 0 && model_num < bsp->nummodels && bsp->models != nullptr) {
                if (bsp->models[model_num].headnode != nullptr) {
                    Vector3 offset = Vector3(edict->s.origin[0], edict->s.origin[1], edict->s.origin[2]);
                    
                    int32_t assigned_ent_id = ENTITYNUM_NONE;
                    if (edict->GetTypeInfo()->IsSubClassType<svg_func_door_t>() ||
                        edict->GetTypeInfo()->IsSubClassType<svg_func_door_rotating_t>() ||
                        edict->GetTypeInfo()->IsSubClassType<svg_func_wall_t>()) {
                        assigned_ent_id = i;
                    }
                    
                    CollectModelBrushes(bsp, bsp->models[model_num].headnode, assigned_ent_id, offset, brush_entity_ids, brush_offsets, &brush_is_active);
                }
            }
        }
    }
    
    int32_t solid_brushes = 0;
    int32_t playerclip_brushes = 0;
    int32_t walkable_sides = 0;
    int32_t sliver_pruned_fragments = 0;

	// Iterate through all brushes in the BSP to extract walkable polygons
	for ( int32_t i = 0; i < bsp->numbrushes; i++ ) {
        // Skip brushes that are part of an unused bmodel
        if (!brush_is_active[i]) {
            continue;
        }
        
		// Get the current brush
		mbrush_t *b = &bsp->brushes[ i ];
		// Skip brushes that are not walk-blocking contributors.
		if ( !( b->contents & ( CONTENTS_SOLID | CONTENTS_DETAIL | CONTENTS_MONSTERCLIP ) ) ) {
			continue;
		}
		// Track brush class split for diagnostics.
        if ( ( b->contents & CONTENTS_MONSTERCLIP ) != 0 && ( b->contents & ( CONTENTS_SOLID | CONTENTS_DETAIL ) ) == 0 ) {
            playerclip_brushes++;
        } else {
            solid_brushes++;
        }

		// Discard brushes that belong to the sky
		bool is_sky_brush = false;
		for ( int32_t j = 0; j < b->numsides; j++ ) {
			mbrushside_t *side = &b->firstbrushside[ j ];
			if ( side->texinfo && ( side->texinfo->c.flags & CM_SURFACE_FLAG_SKY ) ) {
				is_sky_brush = true;
				break;
			}
		}
		if ( is_sky_brush ) {
			continue;
		}

		//! Iterate through each side of the brush to find walkable surfaces
		for ( int32_t j = 0; j < b->numsides; j++ ) {
			// Get the current brush side
			mbrushside_t *side = &b->firstbrushside[ j ];
			
			// Discard sides that belong to sky surfaces (we don't want to walk on the skybox).
			if ( side->texinfo && ( side->texinfo->c.flags & CM_SURFACE_FLAG_SKY ) ) {
				continue;
			}
			
			// Discard sides that are not walkable based on their normal's Z component
			if ( side->plane->normal[ 2 ] < NAV_MIN_WALKABLE_Z ) {
				continue;
			}
            walkable_sides++;

			// Create a base winding (polygon) for the current brush side's plane
            cm_plane_t shifted_plane = GetShiftedPlane(side->plane, brush_offsets[i]);
			winding_t w = BaseWindingForPlane( &shifted_plane );
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
                cm_plane_t clip_plane = GetShiftedPlane(clip->plane, brush_offsets[i]);
				// Clip the winding in place against the plane. If it fails, mark the winding as invalid.
				if ( !ChopWindingInPlace( &w, &clip_plane, 0.1f ) ) {
					valid = false;
				}
			}
			// If the winding is still valid and has at least 3 points, create a nav_poly_t and add it to the global navmesh polygon container.
			if ( !valid || w.num_points < 3 ) {
				continue;
			}
            w.entity_id = brush_entity_ids[i];

			std::vector<winding_t> fragments;
				fragments.push_back(w);
				
				// Calculate the highest Z of the floor polygon we are currently subtracting from
				float floor_max_z = -999999.0f;
				for ( int32_t p = 0; p < w.num_points; p++ ) {
					if ( w.points[ p ].z > floor_max_z ) {
						floor_max_z = w.points[ p ].z;
					}
				}

				// Subtract all other solid brushes from this walkable surface
				Vector3 normal(shifted_plane.normal[0], shifted_plane.normal[1], shifted_plane.normal[2]);
				for ( int32_t other_idx = 0; other_idx < bsp->numbrushes; other_idx++ ) {
					if ( other_idx == i ) {
						continue; // Skip self
					}
                    
                    if ( !brush_is_active[other_idx] ) {
                        continue; // Skip unused/inactive brushes
                    }
					
					mbrush_t* other_b = &bsp->brushes[ other_idx ];
					if ( !( other_b->contents & ( CONTENTS_SOLID | CONTENTS_DETAIL | CONTENTS_MONSTERCLIP ) ) ) {
						continue; // Only subtract blocking geometry
					}
					
					// Determine if other_b is a stair step that the agent can walk onto.
					// We must not subtract steps from the walkable surface if they are low enough.
					float other_max_z = GetBrushMaxZ( other_b, brush_offsets[other_idx] );
					
					// NAV_MAX_STEP_SIZE is typically 18.25f. 
                    const bool isLowAscendingStep = other_max_z > floor_max_z && other_max_z <= floor_max_z + 18.25f;
                    if ( isLowAscendingStep ) {
                        // Skip subtracting step heights so agent can path onto them
                        continue;
					}
					
                    int32_t other_ent_id = brush_entity_ids[other_idx];
                    if ( other_ent_id != ENTITYNUM_NONE ) {
                        // This is a door brush! We DO NOT subtract it to block movement.
                        // Instead, we split the fragments with 0 expansion, and any fragment 
                        // that falls INSIDE the door gets its entity_id updated!
                        SplitWindingsByEntityBrush(fragments, other_b, other_ent_id, brush_offsets[other_idx]);
                    } else {
                        // Normal solid obstacle subtraction
					    SubtractBrushFromWindings(fragments, other_b, brush_offsets[other_idx], normal);
                    }
					
					if ( fragments.empty() ) {
						break; // Entire surface was swallowed by other brushes
					}
				}
				// Merge fragments to reduce unnecessary fragmentation and keep polygon counts low
#if 1
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
					// --- SLIVER PRUNING ---
					// Calculate polygon area and AABB to prune slivers (very long and thin polygons).
					if ( frag.num_points >= 3 ) {
						float area = 0.0f;
						Vector3 mins = frag.points[ 0 ];
						Vector3 maxs = frag.points[ 0 ];
						for ( int32_t v = 0; v < frag.num_points; v++ ) {
							Vector3 p = frag.points[ v ];
							mins.x = std::min( mins.x, p.x );
							mins.y = std::min( mins.y, p.y );
							mins.z = std::min( mins.z, p.z );
							maxs.x = std::max( maxs.x, p.x );
							maxs.y = std::max( maxs.y, p.y );
							maxs.z = std::max( maxs.z, p.z );
							
							if ( v >= 2 ) {
								Vector3 cross = QM_Vector3CrossProduct( frag.points[ v - 1 ] - frag.points[ 0 ], p - frag.points[ 0 ] );
								area += 0.5f * QM_Vector3Length( cross );
							}
						}
						
						float dx = maxs.x - mins.x;
						float dy = maxs.y - mins.y;
						float longest = std::max( dx, dy );
						
						// If the average width (area / longest edge) is less than 2 units, it's a useless sliver!
						// This perfectly culls floating-point errors without destroying valid narrow pathways or creating false insets.
						if ( area < 1.0f || ( longest > 0.001f && ( area / longest ) < 2.0f ) ) {
							sliver_pruned_fragments++;
							continue; // Prune sliver
						}
					}
					// ----------------------

					// Create a new nav_poly_t and populate its fields.
					nav_poly_t poly = {};
					// Assign a unique polygon ID based on the current size of the global navmesh polygon container.
					poly.poly_id = g_nav_polys.size();
					// Clamp the number of vertices to 8, as nav_poly_t supports a maximum of 8 vertices.
					poly.num_vertices = std::min( frag.num_points, MAX_WINDING_POINTS );
					// Calculate the center of the polygon by averaging its vertices.
					Vector3 center( 0, 0, 0 );
					// Copy the vertices from the winding to the nav_poly_t and accumulate the center.
					for ( int32_t v = 0; v < poly.num_vertices; v++ ) {
						// Copy the vertex position from the winding to the polygon.
						poly.vertices[ v ] = frag.points[ v ];
						// Accumulate the vertex positions to compute the center.
						center = center + poly.vertices[ v ];
					}
					// Average the accumulated vertex positions to find the center of the polygon.
					poly.center = center / static_cast<float>( poly.num_vertices );
					// Store the normal of the polygon plane, derived from the original brush side.
					poly.normal = normal;
                    poly.entity_id = frag.entity_id;
					// Save the ID of the BSP leaf that contributed this polygon (used for leaf-local pathfinding lookups).
					poly.bsp_leaf_id = 0;
					// Submit the polygon to the global navmesh polygon container.
					g_nav_polys.push_back( poly );
				}
			}
		}

	// Print a summary of the extraction process to the server console for debugging and verification.
    gi.dprintf("Nav_DoExtractionWork: Checked %d brushes. Found %d solid/detail brushes, %d playerclip-only brushes, %d walkable sides. Extracted %d polys, pruned %d sliver fragments.\n",
        bsp->numbrushes, solid_brushes, playerclip_brushes, walkable_sides, (int)g_nav_polys.size(), sliver_pruned_fragments);
}

/**
*	@brief Calculate the surface area of an axis-aligned bounding box defined by mins and maxs.	
*	@param mins The minimum corner of the AABB.
*	@param maxs The maximum corner of the AABB.
**/
/**
* @brief Compute the surface area of an axis-aligned bounding box.
* @param mins Minimum corner.
* @param maxs Maximum corner.
* @return AABB surface area.
**/
static double SurfaceArea( const Vector3 &mins, const Vector3 &maxs ) {
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
/**
* @brief Recursively build one KD-tree node for a face span.
* @param firstFaceIdx First face index in the current span.
* @param faceCount Number of faces in the current span.
* @param depth Current tree depth.
* @return Created node index, or -1 on failure.
**/
static int32_t BuildKDNode( int32_t firstFaceIdx, int32_t faceCount, int32_t depth ) {
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

	const int32_t NUM_BINS = KDTREE_MAX_BIN;
	double bestCost = 9.999e9;
	int32_t    bestAxis = -1;
	double bestSplitPos = 0.0;

	Vector3 extents = nodeMaxs - nodeMins;
	double parentSA = SurfaceArea( nodeMins, nodeMaxs );
	if ( parentSA == 0.0 ) parentSA = 1.0;

	for ( int32_t axis = 0; axis < 3; ++axis ) {
		if ( extents[ axis ] < 0.1f ) continue;

		struct Bin {
			Vector3 mins{ 99999, 99999, 99999 };
			Vector3 maxs{ -99999, -99999, -99999 };
			int32_t     count = 0;
			void Expand( const nav_face_t &f ) {
                for (int32_t e = 0; e < f.num_edges; ++e) {
                    const Vector3& v = g_nav_vertices[g_nav_halfedges[f.first_edge_idx + e].vertex_idx];
					for ( int32_t k = 0; k < 3; ++k ) {
						if ( v[ k ] < mins[ k ] ) mins[ k ] = v[ k ];
						if ( v[ k ] > maxs[ k ] ) maxs[ k ] = v[ k ];
					}
				}
				++count;
			}
		};
		// Use vector for bins to avoid massive stack allocation with 1024 bins
		std::vector<Bin> bins(NUM_BINS);

		for ( int32_t i = 0; i < faceCount; ++i ) {
			const nav_face_t &face = g_nav_faces[ firstFaceIdx + i ];
			double c = face.center[ axis ];
			int32_t b = static_cast< int32_t >( NUM_BINS * ( c - nodeMins[ axis ] ) / extents[ axis ] );
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
/**
* @brief Recursively partition polygons using axis-aligned split planes.
* @param polys Polygon container to split.
* @param mins Current region minimum bounds.
* @param maxs Current region maximum bounds.
* @param depth Current recursion depth.
**/
/**
* @brief Recursively partition polygons using axis-aligned split planes.
* @param polys Polygon container to split.
* @param mins Current region minimum bounds.
* @param maxs Current region maximum bounds.
* @param depth Current recursion depth.
**/
static void PartitionPolygonsRecursive( PolyContainer &polys, const Vector3 &mins, const Vector3 &maxs, int32_t depth ) {
    if (polys.empty()) return;

    static int32_t recurse_count = 0;
    if (depth == 0) recurse_count = 0;
    recurse_count++;
    if (recurse_count % 10000 == 0) {
        gi.dprintf("PartitionPolygonsRecursive: count=%d, depth=%d, polys=%d, extents=(%.1f, %.1f, %.1f)\n", 
            recurse_count, depth, (int)polys.size(), maxs.x - mins.x, maxs.y - mins.y, maxs.z - mins.z);
    }
    
    Vector3 extents = maxs - mins;
    
    // Pick the longest axis to split (only X or Y for 2.5D NavMesh!)
    int32_t primary_axis = (extents.y > extents.x) ? 1 : 0;
    
    // "Obstacle-Aware KD-Tree Splitting"
    // Find the polygon vertex coordinate along the split axis that is closest to the geometric center.
    // This perfectly aligns the split plane with the edges of crates/stairs.
    int32_t split_axis = -1;
    float split_dist = 0.0f;
    
    // Try the primary (longest) axis first
    std::vector<float> candidates;
    for (const auto& poly : polys) {
        for (int32_t i = 0; i < poly.num_vertices; i++) {
            float v = poly.vertices[i][primary_axis];
            // Enforce a KD-Node size limit to prevent tiny sliver polygons, but allow splitting stairs!
            if (v > mins[primary_axis] + 1.0f && v < maxs[primary_axis] - 1.0f) {
                candidates.push_back(v);
            }
        }
    }
    
    if (!candidates.empty()) {
        std::sort(candidates.begin(), candidates.end());
        split_dist = candidates[candidates.size() / 2];
        split_axis = primary_axis;
    } else {
        // Try secondary axis
        int32_t secondary_axis = 1 - primary_axis;
        for (const auto& poly : polys) {
            for (int32_t i = 0; i < poly.num_vertices; i++) {
                float v = poly.vertices[i][secondary_axis];
                if (v > mins[secondary_axis] + 1.0f && v < maxs[secondary_axis] - 1.0f) {
                    candidates.push_back(v);
                }
            }
        }
        if (!candidates.empty()) {
            std::sort(candidates.begin(), candidates.end());
            split_dist = candidates[candidates.size() / 2];
            split_axis = secondary_axis;
        }
    }
    
    // If still no internal vertex, the region is perfectly empty of obstacles. 
    if (split_axis == -1) {
        // Enforce a maximum cell size to ensure a "few more extra and proper splits" 
        // to prevent gigantic polygons, but only if they are truly massive.
        if (extents.x > 256.0f || extents.y > 256.0f) {
            split_axis = primary_axis;
            split_dist = mins[split_axis] + extents[split_axis] * 0.5f;
        } else {
            return; // Cell is empty and reasonably sized, stop subdividing.
        }
    }
    
    cm_plane_t plane = {};
    plane.normal[split_axis] = 1.0f;
    plane.dist = split_dist;
    
    std::vector<nav_poly_t> front_polys;
    std::vector<nav_poly_t> back_polys;
    
    for (const auto& poly : polys) {
        float poly_min = 99999.0f;
        float poly_max = -99999.0f;
        for (int32_t i = 0; i < poly.num_vertices; i++) {
            poly_min = std::min(poly_min, poly.vertices[i][split_axis]);
            poly_max = std::max(poly_max, poly.vertices[i][split_axis]);
        }
        
        // We no longer skip splitting based on 32.0f slivers.
        // Doing so breaks stair steps which are often 16 or 24 units wide!
        
        winding_t w = {};
        w.num_points = poly.num_vertices;
        for (int32_t i = 0; i < poly.num_vertices; i++) {
            w.points[i] = poly.vertices[i];
        }
        
        winding_t front = {};
        winding_t back = {};
        
        // Split winding using the axis-aligned plane
        SplitWinding(&w, &plane, 0.1f, poly.normal, &front, &back);
        
        if (front.num_points >= 3) {
            nav_poly_t p = poly;
            p.num_vertices = front.num_points;
            for (int32_t i = 0; i < front.num_points; i++) p.vertices[i] = front.points[i];
            
            // Recompute center
            Vector3 center(0, 0, 0);
            for (int32_t i = 0; i < p.num_vertices; i++) center = center + p.vertices[i];
            p.center = center / (float)p.num_vertices;
            
            front_polys.push_back(p);
        }
        if (back.num_points >= 3) {
            nav_poly_t p = poly;
            p.num_vertices = back.num_points;
            for (int32_t i = 0; i < back.num_points; i++) p.vertices[i] = back.points[i];
            
            // Recompute center
            Vector3 center(0, 0, 0);
            for (int32_t i = 0; i < p.num_vertices; i++) center = center + p.vertices[i];
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
/**
* @brief Spatially partition the extracted nav polygons.
* @note This prepares the data for half-edge construction and later twin linking.
**/
static void Nav_PartitionPolygons() {
    if (g_nav_polys.empty()) return;
    
    Vector3 mins(99999, 99999, 99999);
    Vector3 maxs(-99999, -99999, -99999);
    
    for (const auto& p : g_nav_polys) {
        for (int32_t i = 0; i < p.num_vertices; i++) {
            mins.x = std::min(mins.x, p.vertices[i].x);
            mins.y = std::min(mins.y, p.vertices[i].y);
            mins.z = std::min(mins.z, p.vertices[i].z);
            maxs.x = std::max(maxs.x, p.vertices[i].x);
            maxs.y = std::max(maxs.y, p.vertices[i].y);
            maxs.z = std::max(maxs.z, p.vertices[i].z);
        }
    }
    
    int32_t before_count = (int)g_nav_polys.size();
    PartitionPolygonsRecursive(g_nav_polys, mins, maxs, 0);
    gi.dprintf("NavMesh: Spatial Partitioning expanded %d polys into %d grid cells.\n", before_count, (int)g_nav_polys.size());
}

/**
*	@brief	Resolves micro-gaps and T-Junctions by splicing missing vertices directly into the edges of adjacent polygons.
*			This perfectly guarantees the 1-to-1 Twin Half-Edge constraint without destroying the faces.
**/
/**
*	@brief	Resolve T-junctions by splicing missing vertices into adjacent polygons.
*	@note	This keeps the later half-edge mesh conformal.
**/
static void Nav_ResolveTJunctionsByEdgeSplicing() {
    if (g_nav_polys.empty()) return;

    struct PolyAABB {
        float min_x, min_y, min_z, max_x, max_y, max_z;
    };

	static constexpr float zHeight = 24.0f;
    std::vector<PolyAABB> aabbs(g_nav_polys.size());
    auto UpdateAABB = [](const nav_poly_t& p, PolyAABB& aabb) {
        float min_x = 99999, min_y = 99999, min_z = 99999, max_x = -99999, max_y = -99999, max_z = -99999;
        for (int32_t k = 0; k < p.num_vertices; k++) {
            min_x = std::min(min_x, p.vertices[k].x);
            min_y = std::min(min_y, p.vertices[k].y);
            min_z = std::min(min_z, p.vertices[k].z);
            max_x = std::max(max_x, p.vertices[k].x);
            max_y = std::max(max_y, p.vertices[k].y);
            max_z = std::max(max_z, p.vertices[k].z);
        }
        aabb = {min_x - 4.0f, min_y - 4.0f, min_z - zHeight, max_x + 4.0f, max_y + 4.0f, max_z + zHeight };
    };

    struct GridCell {
        std::vector<int32_t> polys;
    };
    std::unordered_map<int64_t, GridCell> grid;
    static constexpr float cell_size = 256.0f;

    for (size_t i = 0; i < g_nav_polys.size(); i++) {
        UpdateAABB(g_nav_polys[i], aabbs[i]);
        int32_t min_cx = (int)std::floor(aabbs[i].min_x / cell_size);
        int32_t max_cx = (int)std::floor(aabbs[i].max_x / cell_size);
        int32_t min_cy = (int)std::floor(aabbs[i].min_y / cell_size);
        int32_t max_cy = (int)std::floor(aabbs[i].max_y / cell_size);
        int32_t min_cz = (int)std::floor(aabbs[i].min_z / cell_size);
        int32_t max_cz = (int)std::floor(aabbs[i].max_z / cell_size);
        for (int32_t cx = min_cx; cx <= max_cx; cx++) {
            for (int32_t cy = min_cy; cy <= max_cy; cy++) {
                for (int32_t cz = min_cz; cz <= max_cz; cz++) {
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
            
            Vector3 edgeVec = pB - pA;
            float edgeLenSqr = QM_Vector3LengthSqr(edgeVec);
            
            if (edgeLenSqr < 1.0f) continue;
            
            Vector3 pA_2d = { pA.x, pA.y, 0.0f };
            Vector3 pB_2d = { pB.x, pB.y, 0.0f };
            Vector3 edgeVec_2d = pB_2d - pA_2d;
            float edgeLenSqr_2d = QM_Vector3LengthSqr(edgeVec_2d);
            
            if (edgeLenSqr_2d < 0.001f) continue;
            
            int32_t min_cx = (int)std::floor(aabbs[i].min_x / cell_size);
            int32_t max_cx = (int)std::floor(aabbs[i].max_x / cell_size);
            int32_t min_cy = (int)std::floor(aabbs[i].min_y / cell_size);
            int32_t max_cy = (int)std::floor(aabbs[i].max_y / cell_size);
            int32_t min_cz = (int)std::floor(aabbs[i].min_z / cell_size);
            int32_t max_cz = (int)std::floor(aabbs[i].max_z / cell_size);
            
            std::vector<int32_t> checked_j; // Simple deduplication for cell overlap
            
            for (int32_t cx = min_cx; cx <= max_cx && !poly_modified; cx++) {
                for (int32_t cy = min_cy; cy <= max_cy && !poly_modified; cy++) {
                    for (int32_t cz = min_cz; cz <= max_cz && !poly_modified; cz++) {
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
                                
                                Vector3 toC_2d = vC_2d - pA_2d;
                                float t = QM_Vector3DotProduct(toC_2d, edgeVec_2d) / edgeLenSqr_2d;
                                
                                if (t > 0.0f && t < 1.0f) {
                                    Vector3 projC_2d = QM_Vector3MultiplyAdd( pA_2d, t, edgeVec_2d );
                                    
                                    if (QM_Vector3DistanceSqr(vC_2d, projC_2d) < 16.0f) { 
                                        Vector3 projC_3d = QM_Vector3MultiplyAdd( pA, t, edgeVec );
                                        float dz = std::abs(vC.z - projC_3d.z);
                                        
                                        if (dz <= zHeight) {
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

#if 0
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
#endif

/**
*	@brief	Builds the half-edge mesh from the extracted and partitioned polygons.
**/
/**
* @brief Build the half-edge mesh from the extracted nav polygons.
* @note This is the second stage of generation and produces the pathfinding graph.
**/
void Nav_BuildHalfEdgeMesh() {
	/**
	*	Ensure these are cleared out before we start building the half-edge mesh.
	**/
    g_nav_vertices.clear();
    g_nav_halfedges.clear();
    g_nav_faces.clear();
	// Cancel early if there are no polygons to process.
	if ( g_nav_polys.empty() ) {
		return;
	}

	/**
	*	Partition the massive extracted polys into a structured sub-polygon grid.
	**/
	Nav_PartitionPolygons();

	/**
	*	No prune pass needed here.
	*	Since we enabled full sloped CSG subtraction in Nav_DoExtractionWork,
	*	overlapping brushes are perfectly clipped and there are no buried phantom polygons.
	**/

	// Resolve any remaining T-junctions by splicing (mostly non-grid aligned or dynamic cuts).
	Nav_ResolveTJunctionsByEdgeSplicing();

	/**
	*	Triangulation is disabled because the KD-tree sub-polygons are naturally planar N-gons.
	*	Triangulating them creates unnecessary extra geometry and internal spokes that confuse pathing.
	*	Nav_TriangulatePolygons();
	**/

	// This is a spatial hash grid to deduplicate vertices and prevent T-junctions.
	std::unordered_map<int64_t, std::vector<int32_t>> vertex_grid;
	int32_t first_pass_twin_links = 0;
	int32_t second_pass_twin_links = 0;

	/**
	*	@brief	Hash a vertex position into the deduplication grid.
	*	@details	We enforce exact 3D matching (with a tiny floating point epsilon) to prevent
	*			"canonical vertex drift" where an entire staircase merges into a single vertex.
	**/
	auto GetVertexIndex = [&]( const Vector3 &p ) -> int32_t {
		// Use a 4-unit grid to hash vertices, allowing for very close proximity without merging.
		static constexpr float GRID_SIZE = 4.0f;
		int64_t cx = (int64_t)std::floor( p.x / GRID_SIZE );
		int64_t cy = (int64_t)std::floor( p.y / GRID_SIZE );
		int64_t cz = (int64_t)std::floor( p.z / GRID_SIZE );

		// Check neighboring grid cells to find an existing vertex that is very close to p.
		for ( int64_t ox = -1; ox <= 1; ox++ ) {
			// Check neighboring cells in X, Y, Z directions.
			for ( int64_t oy = -1; oy <= 1; oy++ ) {
				// Check neighboring cells in Z direction.
				for ( int64_t oz = -1; oz <= 1; oz++ ) {
					// Compute a unique hash key for the neighboring cell.
					int64_t key = ( ( cx + ox ) * 73856093 ) ^ ( ( cy + oy ) * 19349663 ) ^ ( ( cz + oz ) * 83492791 );
					// Look up the cell in the vertex grid
					auto it = vertex_grid.find( key );
					// If the cell exists, check each vertex index in that cell.
					if ( it != vertex_grid.end() ) {
						// Compare the position of each vertex in the cell to p.
						for ( int32_t idx : it->second ) {
							// Calculate the distance for each axis between the existing vertex and p.
							float dx = g_nav_vertices[ idx ].x - p.x;
							float dy = g_nav_vertices[ idx ].y - p.y;
							float dz = g_nav_vertices[ idx ].z - p.z;
							// If the squared distance is less than 0.01, consider it a match and return the index.
							if ( dx * dx + dy * dy + dz * dz < 0.01f ) {
								return idx;
							}
						}
					}
				}
			}
		}

		// If no existing vertex is found, add p as a new vertex and return its index.
		int32_t new_idx = (int32_t)g_nav_vertices.size();
		// Add the new vertex to the global vertex list.
		g_nav_vertices.push_back( p );

		// Generate a unique hash key for the new vertex's grid cell.
		int64_t key = ( cx * 73856093 ) ^ ( cy * 19349663 ) ^ ( cz * 83492791 );
		// Add the new vertex index to the appropriate cell in the vertex grid.
		vertex_grid[ key ].push_back( new_idx );

		// Return the index of the newly added vertex.
		return new_idx;
    };

	/**
	*	Now we build the half-edge mesh from the partitioned polygons.
	*	To do this we:
	*		- 1:	Deduplicate vertices and store them in g_nav_vertices
	*		- 2:	Create half-edges for each polygon edge, linking them to their vertices and faces
	*		- 3:	Compute the clearance for each face (distance from center to nearest edge)
	*		- 4:	Store the half-edges and faces in g_nav_halfedges and g_nav_faces
	*		- 5:	After all half-edges are created, we perform twin linking to connect adjacent edges across polygons
	*				This ensures a fully conformal half-edge graph for pathfinding.
	**/
    for (size_t i = 0; i < g_nav_polys.size(); i++) {
		// For each polygon, we create a nav_face_t and its associated half-edges.
        const nav_poly_t& poly = g_nav_polys[i];
        
		/**
		*	Create a new face for this polygon
		**/
		// Initialize a new face structure
        nav_face_t face = {};
		// Assign the face ID to the current index
        face.face_id = i;
        face.num_edges = poly.num_vertices;
        face.center = poly.center;
        face.normal = poly.normal;
        face.entity_id = poly.entity_id;
        face.bsp_leaf_id = poly.bsp_leaf_id;
		// Store the index of the first half-edge for this face
        face.first_edge_idx = g_nav_halfedges.size();

		/**
		*	Check for deduplicated vertices and store their indices for the half-edges.
		**/
		// Deduplicated vertice storage for the polygon's vertices
        std::vector<int32_t> v_indices(poly.num_vertices);
		// For each vertex in the polygon, get its index in the global vertex list
        for (int32_t v = 0; v < poly.num_vertices; v++) {
			// Deduplicate the vertex and get its index
            v_indices[v] = GetVertexIndex(poly.vertices[v]);
        }

        /**
		*	Now we compute the clearance for this face, which is the distance from the center to the nearest edge.
		**/
        float min_dist = 99999.0f;
		// For each edge of the polygon, compute the distance from the face center to the edge
        for (int32_t v = 0; v < poly.num_vertices; v++) {
			// Get the two vertices that define the edge.
            Vector3 a = poly.vertices[v];
            Vector3 b = poly.vertices[(v + 1) % poly.num_vertices];
            
            // distance from center to line segment a-b
            Vector3 edge = b - a;
            Vector3 toCenter = face.center - a;
			// Compute the squared length of the edge to avoid unnecessary square root calculations.
            float edgeLenSq = QM_Vector3LengthSqr(edge);
			// Default distance is 0.0f, will be updated if edge length is significant.
            float dist = 0.0f;
			// If the edge length is significant, compute the projection of the center onto the edge.
            if (edgeLenSq > 0.0001f) {
				// Compute the projection factor t of the center onto the edge, clamped between 0 and 1.
                float t = QM_Vector3DotProduct(toCenter, edge) / edgeLenSq;
				// Clamp t to the range [0, 1] to ensure the projection lies on the edge segment.
                t = std::max(0.0f, std::min(1.0f, t));
				// Compute the projected point on the edge using the clamped t value.
                Vector3 proj = QM_Vector3MultiplyAdd( a, t, edge );
				// Compute the distance from the face center to the projected point on the edge.
                dist = QM_Vector3Distance(face.center, proj);
			// If the edge length is too small, fall back to the distance from the center to one of the edge vertices.
            } else {
				// Edge is too small, use distance to vertex a
                dist = QM_Vector3Distance(face.center, a);
            }
			// Update the minimum distance if the current distance is smaller.
			if ( dist < min_dist ) {
				min_dist = dist;
			}
        }
		// Last but not least, store the computed clearance in the face structure.
        face.clearance = min_dist;

		/**
		*	For each edge of the polygon, create a half-edge and link it to the face and its vertices.
		**/
        for (int32_t v = 0; v < poly.num_vertices; v++) {
			// Get the current and next vertex indices for the half-edge
            int32_t curr_v = v_indices[v];

			// Create a new half-edge structure
            nav_halfedge_t he = {};
            he.vertex_idx = curr_v;
            he.face_idx = face.face_id;
            he.twin_idx = -1; // Default to boundary
			he.edge_entity_id = ENTITYNUM_NONE; // Default to no entity
			he.wall_offset = 16.0f; // Metadata for runtime inspection: boundary edges were expanded by 16.0f during CSG.

			// The next half-edge index is the next edge in the polygon, wrapping around to the first edge.
            he.next_idx = face.first_edge_idx + ((v + 1) % poly.num_vertices);
            
			// Add the half-edge to the global half-edge list
            g_nav_halfedges.push_back(he);
        }

        g_nav_faces.push_back(face);
    }
    
    /**
	*	Twin Linking using Z-tolerant 2D overlap check
    *	This connects stair steps that are physically separated by up to 18 units vertically,
    *	ensuring the half-edge graph remains fully conformal across varying height terrain.
	**/
	// The twin linking is done in two passes. The first pass links edges that are very close in 2D and have a small Z difference.
	// The second pass links edges that overlap in 2D but have a strict Z-tolerance to prevent linking catwalks to floors below them.
    std::unordered_map<int64_t, std::vector<int32_t>> twin_grid;
	// Build a spatial hash grid of half-edges based on their next vertex position to facilitate efficient twin linking.
	for (size_t j = 0; j < g_nav_halfedges.size(); j++) {
		// Get the second vertex of the half-edge (the next vertex in the polygon).
		Vector3 b2 = g_nav_vertices[g_nav_halfedges[g_nav_halfedges[j].next_idx].vertex_idx];
		// Compute the grid cell coordinates for the vertex position, using a 16-unit grid size.
		static constexpr float GRID_SIZE = 16.0f;
        int64_t cx = (int64_t)std::floor(b2.x / GRID_SIZE );
        int64_t cy = (int64_t)std::floor(b2.y / GRID_SIZE );
        int64_t key = (cx * 73856093) ^ (cy * 19349663);
		// Store the half-edge index in the corresponding grid cell for later twin linking.
        twin_grid[key].push_back((int32_t)j);
    }
    /**
	*	Now we iterate through all half-edges and attempt to find their twins by checking neighboring grid cells for potential matches.
	*	A half-edge is considered a twin if it is very close in 2D and has a small Z difference, allowing for slight vertical offsets.
	**/
    for (uint64_t i = 0; i < g_nav_halfedges.size(); i++) {
		// Skip half-edges that already have a twin assigned.
		if ( g_nav_halfedges[ i ].twin_idx != -1 ) {
			continue;
		}

		// Get the current half-edge and its vertices.
        nav_halfedge_t& heA = g_nav_halfedges[i];
        Vector3 a1 = g_nav_vertices[heA.vertex_idx];
        Vector3 a2 = g_nav_vertices[g_nav_halfedges[heA.next_idx].vertex_idx];
		// Initialize variables to track the best twin candidate based on Z difference.
        float bestZ = 99999.0f;
        int64_t bestTwin = -1;

		// Compute the grid cell coordinates for the first vertex of the half-edge, using a 16-unit grid size.
		static constexpr float GRID_SIZE = 16.0f;
		int64_t cx = ( int64_t )std::floor( a1.x / GRID_SIZE );
        int64_t cy = (int64_t)std::floor(a1.y / GRID_SIZE );
        
		// Check neighboring grid cells in a 3x3 area to find potential twin half-edges.
        for (int64_t ox = -1; ox <= 1; ox++) {
            for (int64_t oy = -1; oy <= 1; oy++) {
				// Compute the hash key for the neighboring grid cell to look for potential twin half-edges.
                int64_t key = ((cx + ox) * 73856093) ^ ((cy + oy) * 19349663);
                auto it = twin_grid.find(key);
                if (it == twin_grid.end()) continue;
                
                for (int32_t j : it->second) {
					if ( i == (size_t)j ) {
						continue; // Don't twin with self!
					}
					if ( g_nav_halfedges[ j ].twin_idx != -1 ) {
						continue;
					}

					// Get the candidate half-edge and its vertices for comparison.
                    nav_halfedge_t& heB = g_nav_halfedges[j];
                    if ( heA.face_idx == heB.face_idx ) {
                        continue;
                    }
					// Get the vertices of the candidate half-edge.
                    Vector3 b1 = g_nav_vertices[heB.vertex_idx];
                    Vector3 b2 = g_nav_vertices[g_nav_halfedges[heB.next_idx].vertex_idx];

                    /**
                    * Endpoint proximity alone is not sufficient for a portal.  Adjacent
                    * stair risers can have reversed endpoints within the T-junction
                    * tolerance while being laterally separated from the actual shared
                    * edge.  Require opposing, collinear spans before accepting a twin.
                    **/
                    Vector3 edgeA2D = a2 - a1;
                    edgeA2D.z = 0.0f;
                    Vector3 edgeB2D = b2 - b1;
                    edgeB2D.z = 0.0f;
                    const float edgeALen = QM_Vector3Length( edgeA2D );
                    const float edgeBLen = QM_Vector3Length( edgeB2D );
                    if ( edgeALen <= 0.1f || edgeBLen <= 0.1f ) {
                        continue;
                    }
                    edgeA2D = edgeA2D * ( 1.0f / edgeALen );
                    edgeB2D = edgeB2D * ( 1.0f / edgeBLen );
                    if ( QM_Vector3DotProduct( edgeA2D, edgeB2D ) > -0.95f ) {
                        continue;
                    }

                    const float lateralB1 = std::fabs( edgeA2D.x * ( b1.y - a1.y ) - edgeA2D.y * ( b1.x - a1.x ) );
                    const float lateralB2 = std::fabs( edgeA2D.x * ( b2.y - a1.y ) - edgeA2D.y * ( b2.x - a1.x ) );
                    static constexpr float MAX_TWIN_LATERAL_SEPARATION = 0.5f;
                    if ( lateralB1 > MAX_TWIN_LATERAL_SEPARATION || lateralB2 > MAX_TWIN_LATERAL_SEPARATION ) {
                        continue;
                    }

					// Compute the horizontal (X, Y) and vertical (Z) differences between the half-edges to determine if they are close enough to be considered twins.
                    float dx1 = a1.x - b2.x;
                    float dy1 = a1.y - b2.y;
                    float dz1 = std::abs(a1.z - b2.z);

                    float dx2 = a2.x - b1.x;
                    float dy2 = a2.y - b1.y;
                    float dz2 = std::abs(a2.z - b1.z);

                    // Tolerate up to a 4 unit horizontal gap (16.0f squared) to handle BSP T-junctions
                    static constexpr float MAX_DIST_SQR = 16.0f; // 4 units squared
					static constexpr float MAX_Z_DIFF = NAV_MAX_STEP_SIZE + 4.0f; // Keep twin linking aligned with default step + clearance policy.
                    if (dx1 * dx1 + dy1 * dy1 < MAX_DIST_SQR && dz1 <= MAX_Z_DIFF &&
                        dx2 * dx2 + dy2 * dy2 < MAX_DIST_SQR && dz2 <= MAX_Z_DIFF ) {
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

		// If a suitable twin half-edge was found, link them together and compute the Z difference for pathfinding.
        if (bestTwin != -1) {
			// Link the twin half-edges together by setting their twin indices.
            g_nav_halfedges[i].twin_idx = bestTwin;
            g_nav_halfedges[bestTwin].twin_idx = static_cast<int32_t>(i);
			
			// Clear wall offset metadata since this is now an internal connected edge
			g_nav_halfedges[i].wall_offset = 0.0f;
			g_nav_halfedges[bestTwin].wall_offset = 0.0f;

			first_pass_twin_links++;
            
            // Use the actual Z height of the shared edge vertices, not the face centers!
            // Face centers are wildly inaccurate for long slopes or ramps.
            float z1 = (g_nav_vertices[g_nav_halfedges[i].vertex_idx].z + g_nav_vertices[g_nav_halfedges[g_nav_halfedges[i].next_idx].vertex_idx].z) * 0.5f;
            float z2 = (g_nav_vertices[g_nav_halfedges[bestTwin].vertex_idx].z + g_nav_vertices[g_nav_halfedges[g_nav_halfedges[bestTwin].next_idx].vertex_idx].z) * 0.5f;
            g_nav_halfedges[i].z_diff = z2 - z1;
            g_nav_halfedges[bestTwin].z_diff = z1 - z2;
            
            // --- DOOR METADATA ASSIGNMENT ---
            int32_t ent_a = g_nav_faces[g_nav_halfedges[i].face_idx].entity_id;
            int32_t ent_b = g_nav_faces[g_nav_halfedges[bestTwin].face_idx].entity_id;

            if (ent_a != ent_b) {
                g_nav_halfedges[i].edge_entity_id = ent_b;
                g_nav_halfedges[bestTwin].edge_entity_id = ent_a;
            } else {
                g_nav_halfedges[i].edge_entity_id = ENTITYNUM_NONE;
                g_nav_halfedges[bestTwin].edge_entity_id = ENTITYNUM_NONE;
            }
        }
    }

    /**
	*	Secondary Twin Linking pass for T - Junctions and Overlaps
    *	Only links edges that overlap in 2D but have a strict Z-tolerance (<= 18.0f)
    *	to prevent accidentally linking catwalks to floors below them.
	**/
	// Build a spatial hash grid of half-edges based on their center position to facilitate efficient twin linking for overlapping edges.
    std::unordered_map<int64_t, std::vector<int32_t>> overlap_grid;
	// For each half-edge, compute its center point and store it in the overlap grid for later twin linking.
    for (size_t j = 0; j < g_nav_halfedges.size(); j++) {
		// Get the half-edge and its vertices to compute the center point.
        nav_halfedge_t& heB = g_nav_halfedges[j];
		// Compute the center point of the half-edge by averaging its two vertices.
        Vector3 b1 = g_nav_vertices[heB.vertex_idx];
        Vector3 b2 = g_nav_vertices[g_nav_halfedges[heB.next_idx].vertex_idx];
		// Compute the center point of the half-edge in 3D space.
        Vector3 center = ( b1 + b2 ) * 0.5f;
        
		// Compute the grid cell coordinates for the center point, using a 128-unit grid size to group nearby edges together.
		static constexpr float GRID_SIZE = 128.0f;
		int64_t cx = (int64_t)std::floor(center.x / GRID_SIZE );
        int64_t cy = (int64_t)std::floor(center.y / GRID_SIZE );
		// Compute a unique hash key for the grid cell based on its coordinates.
        int64_t key = (cx * 73856093) ^ (cy * 19349663);
		// Store the half-edge index in the corresponding grid cell for later twin linking of overlapping edges.
        overlap_grid[key].push_back((int32_t)j);
    }
    
	/**
	*	Now we iterate through all half-edges and attempt to find their twins by checking neighboring grid cells for potential matches.
	**/
	for (size_t i = 0; i < g_nav_halfedges.size(); i++) {
		if ( g_nav_halfedges[ i ].twin_idx != -1 ) {
			continue;
		}

		// Get the current half-edge and its vertices.
        nav_halfedge_t& heA = g_nav_halfedges[i];
		// Get the two vertices of the half-edge to compute its direction and length.
        Vector3 a1 = g_nav_vertices[heA.vertex_idx];
		// Get the next vertex of the half-edge to compute its direction and length.
        Vector3 a2 = g_nav_vertices[g_nav_halfedges[heA.next_idx].vertex_idx];

		// Compute the direction vector of the half-edge in 2D (ignoring Z) and its length.
        Vector3 dA = a2 - a1;
        dA.z = 0.0f;
        float lenA = QM_Vector3Length(dA);
        if (lenA < 0.1f) continue;
        Vector3 dirA = dA * ( 1.0f / lenA );

        // Initialize variables to track the best overlapping twin candidate based on overlap length.
        float bestOverlap = -1.0f;
        int32_t bestTwin = -1;

		// Compute the center point of the half-edge to use for spatial hashing in the overlap grid.
        Vector3 centerA = ( a1 + a2 ) * 0.5f;
		static constexpr float GRID_SIZE = 128.0f;
        int64_t cx = (int64_t)std::floor(centerA.x / GRID_SIZE );
        int64_t cy = (int64_t)std::floor(centerA.y / GRID_SIZE );
        
        for (int64_t ox = -1; ox <= 1; ox++) {
            for (int64_t oy = -1; oy <= 1; oy++) {
				// Compute the hash key for the neighboring grid cell to look for potential overlapping half-edges.
                int64_t key = ((cx + ox) * 73856093) ^ ((cy + oy) * 19349663);
                auto it = overlap_grid.find(key);
				// If the grid cell has no half-edges, skip to the next cell.
				if ( it == overlap_grid.end() ) {
					continue;
				}
				// For each half-edge in the neighboring grid cell, check if it overlaps with the current half-edge in 2D and has a small Z difference.
                for (int32_t j : it->second) {
					// Skip self and already linked half-edges.
					if ( i == j ) {
						continue;
					}
					// Skip half-edges that already have a twin assigned.
					if ( g_nav_halfedges[ j ].twin_idx != -1 ) {
						continue;
					}

					// Get the candidate half-edge and its vertices for comparison.
                    nav_halfedge_t& heB = g_nav_halfedges[j];
                    if ( heA.face_idx == heB.face_idx ) {
                        continue;
                    }
                    Vector3 b1 = g_nav_vertices[heB.vertex_idx];
                    Vector3 b2 = g_nav_vertices[g_nav_halfedges[heB.next_idx].vertex_idx];

					// Compute the direction vector of the candidate half-edge in 2D (ignoring Z) and its length.
                    Vector3 dB = b2 - b1;
                    dB.z = 0.0f;
                    float lenB = QM_Vector3Length(dB);
					// Skip candidate half-edges that are too short to be considered for twin linking.
					if ( lenB < 0.1f ) {
						continue;
					}
                    Vector3 dirB = dB * ( 1.0f / lenB );
					// Check if the two half-edges are nearly parallel in 2D by computing the dot product of their direction vectors.
					if ( QM_Vector3DotProduct( dirA, dirB ) > -0.95f ) {
						continue;
					}

                    // Endpoint projection alone is insufficient: two parallel stair
                    // edges on adjacent risers can project onto one another while
                    // remaining several units apart.  Such a false twin creates a
                    // traversable-looking L-turn portal that the capsule cannot cross.
                    const float lateralSeparation = std::fabs(
                        dirA.x * ( b1.y - a1.y ) - dirA.y * ( b1.x - a1.x ) );
                    static constexpr float MAX_LATERAL_SEPARATION = 0.5f;
                    if ( lateralSeparation > MAX_LATERAL_SEPARATION ) {
                        continue;
                    }

					// Project the candidate half-edge's first vertex onto the current half-edge's direction to find the closest point and check for overlap.
                    Vector3 a1_to_b1 = b1 - a1;
                    a1_to_b1.z = 0.0f;
                    float proj = QM_Vector3DotProduct(a1_to_b1, dirA);
                    Vector3 closestPt = QM_Vector3MultiplyAdd( a1, proj, dirA );
                    closestPt.z = 0.0f;
                    Vector3 b1_2d = b1; b1_2d.z = 0.0f;
					// If the closest point is more than 4 units away from b1 in 2D, skip this candidate half-edge.
					if ( QM_Vector3DistanceSqr( b1_2d, closestPt ) > 4.0f ) {
						continue;
					}

					// Compute the projection of the candidate half-edge's second vertex onto the current half-edge's direction to find the overlap range.
                    float u1 = proj;
                    Vector3 a1_to_b2 = b2 - a1;
                    a1_to_b2.z = 0.0f;
                    float u2 = QM_Vector3DotProduct(a1_to_b2, dirA);

					// Compute the overlap range between the two half-edges in 2D and check if it exceeds 1.0f units.
                    float minU = std::min(u1, u2);
                    float maxU = std::max(u1, u2);
                    float overlapStart = std::max(0.0f, minU);
                    float overlapEnd = std::min(lenA, maxU);
                    float overlapLen = overlapEnd - overlapStart;

					// Require a minimally usable overlap so we do not link paper-thin portals that path traversal later rejects.
                    if (overlapLen >= 2.0f) {
                        float dz = std::abs((a1.z + a2.z) * 0.5f - (b1.z + b2.z) * 0.5f);
                        // STRICT Z tolerance to prevent vertical scrambling!
						static constexpr float MAX_Z_DIFF = NAV_MAX_STEP_SIZE + 4.0f; // Keep twin linking aligned with default step + clearance policy.
                        if ( dz <= MAX_Z_DIFF ) {
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
			
			// Clear wall offset metadata since this is now an internal connected edge
			g_nav_halfedges[i].wall_offset = 0.0f;
			g_nav_halfedges[bestTwin].wall_offset = 0.0f;

			second_pass_twin_links++;
            
            // Use the actual Z height of the shared edge vertices, not the face centers!
            float z1 = (g_nav_vertices[g_nav_halfedges[i].vertex_idx].z + g_nav_vertices[g_nav_halfedges[g_nav_halfedges[i].next_idx].vertex_idx].z) * 0.5f;
            float z2 = (g_nav_vertices[g_nav_halfedges[bestTwin].vertex_idx].z + g_nav_vertices[g_nav_halfedges[g_nav_halfedges[bestTwin].next_idx].vertex_idx].z) * 0.5f;
            g_nav_halfedges[i].z_diff = z2 - z1;
            g_nav_halfedges[bestTwin].z_diff = z1 - z2;

            // --- DOOR METADATA ASSIGNMENT ---
            int32_t ent_a = g_nav_faces[g_nav_halfedges[i].face_idx].entity_id;
            int32_t ent_b = g_nav_faces[g_nav_halfedges[bestTwin].face_idx].entity_id;

            if (ent_a != ent_b) {
                g_nav_halfedges[i].edge_entity_id = ent_b;
                g_nav_halfedges[bestTwin].edge_entity_id = ent_a;
            } else {
                g_nav_halfedges[i].edge_entity_id = ENTITYNUM_NONE;
                g_nav_halfedges[bestTwin].edge_entity_id = ENTITYNUM_NONE;
            }
        }
    }

	/**
	*	Emit topology diagnostics so we can detect fragmented regions and weak links.
	**/
	auto Compute2DOverlapLen = [&]( const nav_halfedge_t &a, const nav_halfedge_t &b ) -> float {
		const Vector3 a0 = g_nav_vertices[ a.vertex_idx ];
		const Vector3 a1 = g_nav_vertices[ g_nav_halfedges[ a.next_idx ].vertex_idx ];
		const Vector3 b0 = g_nav_vertices[ b.vertex_idx ];
		const Vector3 b1 = g_nav_vertices[ g_nav_halfedges[ b.next_idx ].vertex_idx ];
        Vector3 aDir = a1 - a0;
		aDir.z = 0.0f;
		const float aLen = QM_Vector3Length( aDir );
		if ( aLen <= 0.0001f ) {
			return 0.0f;
		}
        aDir = aDir * ( 1.0f / aLen );
        Vector3 a0b0 = b0 - a0;
        Vector3 a0b1 = b1 - a0;
		a0b0.z = 0.0f;
		a0b1.z = 0.0f;
		const float u0 = static_cast<float>( QM_Vector3DotProduct( a0b0, aDir ) );
		const float u1 = static_cast<float>( QM_Vector3DotProduct( a0b1, aDir ) );
		const float bMin = std::min( u0, u1 );
		const float bMax = std::max( u0, u1 );
		const float overlapStart = std::max( 0.0f, bMin );
		const float overlapEnd = std::min( aLen, bMax );
		return std::max( 0.0f, overlapEnd - overlapStart );
	};

	int32_t boundary_edges = 0;
	int32_t twinned_edges = 0;
	int32_t twin_reverse_mismatch = 0;
	int32_t tiny_overlap_twins = 0;
	int32_t short_boundary_edges = 0;
	for ( size_t i = 0; i < g_nav_halfedges.size(); i++ ) {
		const nav_halfedge_t &he = g_nav_halfedges[ i ];
		const Vector3 e0 = g_nav_vertices[ he.vertex_idx ];
		const Vector3 e1 = g_nav_vertices[ g_nav_halfedges[ he.next_idx ].vertex_idx ];
		const float edgeLen2D = static_cast<float>( QM_Vector2Distance( e0, e1 ) );
		if ( he.twin_idx == -1 ) {
			boundary_edges++;
			if ( edgeLen2D < 4.0f ) {
				short_boundary_edges++;
			}
			continue;
		}
		twinned_edges++;
		if ( static_cast<size_t>( he.twin_idx ) >= g_nav_halfedges.size() || g_nav_halfedges[ he.twin_idx ].twin_idx != static_cast<int32_t>( i ) ) {
			twin_reverse_mismatch++;
			continue;
		}
		const float overlapLen = Compute2DOverlapLen( he, g_nav_halfedges[ he.twin_idx ] );
		if ( overlapLen < 2.0f ) {
			tiny_overlap_twins++;
		}
	}

    gi.dprintf("NavMesh Half-Edge Generation Completed. %d vertices, %d half-edges, %d faces.\n", 
               g_nav_vertices.size(), g_nav_halfedges.size(), g_nav_faces.size());
	gi.dprintf("NavMesh Topology Diagnostics: firstPassLinks=%d secondPassLinks=%d twinnedEdges=%d boundaryEdges=%d shortBoundaryEdges=%d twinReverseMismatch=%d tinyOverlapTwins=%d\n",
		first_pass_twin_links, second_pass_twin_links, twinned_edges, boundary_edges, short_boundary_edges, twin_reverse_mismatch, tiny_overlap_twins );
}

/**
*	@brief	Wrapper function to build the KD-Tree for the navmesh faces.
**/
/**
* @brief Build the KD-tree used for spatial nav queries.
* @note This is the final generation stage after the half-edge mesh is ready.
**/
void Nav_BuildKDTree() {
    g_nav_nodes.clear();
	if ( g_nav_faces.empty() ) return;
	
    BuildKDNode(0, g_nav_faces.size(), 0);

	for (int32_t i = 0; i < g_nav_faces.size(); i++) {
		g_nav_faces[i].face_id = i;
        
		/**
        *	`CRITICAL FIX`: `BuildKDNode` calls `std::partition`, which physically shuffles `g_nav_faces`.
        *	We `MUST` update the back-pointers in the half-edges so they point to the NEW shuffled face index!
        *	Otherwise, Nav_FindPath's A* will jump to random, completely unconnected faces!
		**/
        for (int32_t e = 0; e < g_nav_faces[i].num_edges; ++e) {
            g_nav_halfedges[g_nav_faces[i].first_edge_idx + e].face_idx = i;
        }
	}

    gi.dprintf("NavMesh KD-Tree Generation Completed. %d nodes created.\n", g_nav_nodes.size());
}
 
 
