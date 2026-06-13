#include "nav_generate.h"
#include "nav_thread.h"

nav_vector_t<nav_poly_t> g_nav_polys;
nav_vector_t<nav_kdtree_node_t> g_nav_nodes;
nav_vector_t<nav_leaf_link_t> g_nav_leaf_links;
nav_vector_t<int32_t> g_nav_leaf_poly_ids;

void Nav_GenerateCommand() {
    // Start the async generation
    Nav_StartAsyncGeneration();
}

void Nav_Clear() {
    g_nav_polys.clear();
    g_nav_nodes.clear();
    g_nav_leaf_links.clear();
    g_nav_leaf_poly_ids.clear();
    gi.dprintf("NavMesh memory cleared.\n");
}

#include "shared/cm/cm_model.h"
#include "shared/formats/format_bsp.h"

#define MAX_WINDING_POINTS 64

struct winding_t {
    int num_points;
    Vector3 points[MAX_WINDING_POINTS];
};

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
    QM_Vector3NormalizeLength(right);
    up = QM_Vector3CrossProduct(p_normal, right);
    QM_Vector3NormalizeLength(up);
    
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

void Nav_DoExtractionWork() {
    cm_t* cm = gi.GetCollisionModel();
    if (!cm || !cm->cache) return;
    bsp_t* bsp = cm->cache;

    g_nav_polys.clear();
    
    for (int i=0; i<bsp->numbrushes; i++) {
        mbrush_t* b = &bsp->brushes[i];
        if (!(b->contents & CONTENTS_SOLID)) continue;
        
        for (int j=0; j<b->numsides; j++) {
            mbrushside_t* side = &b->firstbrushside[j];
			if ( side->plane->normal[ 2 ] < NAV_MIN_WALKABLE_Z ) continue;
            
            winding_t w = BaseWindingForPlane(side->plane);
            bool valid = true;
            for (int k=0; k<b->numsides && valid; k++) {
                if (j == k) continue;
                mbrushside_t* clip = &b->firstbrushside[k];
                cm_plane_t flip;
                flip.normal[0] = -clip->plane->normal[0];
                flip.normal[1] = -clip->plane->normal[1];
                flip.normal[2] = -clip->plane->normal[2];
                flip.dist = -clip->plane->dist;
                if (!ChopWindingInPlace(&w, &flip, 0.1f)) {
                    valid = false;
                }
            }
            if (valid && w.num_points >= 3) {
                nav_poly_t poly = {};
                poly.poly_id = g_nav_polys.size();
                poly.num_vertices = std::min(w.num_points, 8);
                Vector3 center(0,0,0);
                for (int v=0; v<poly.num_vertices; v++) {
                    poly.vertices[v] = w.points[v];
                    center = center + w.points[v];
                }
                poly.center = center / (float)poly.num_vertices;
                poly.normal.x = side->plane->normal[0];
                poly.normal.y = side->plane->normal[1];
                poly.normal.z = side->plane->normal[2];
                g_nav_polys.push_back(poly);
            }
        }
    }
}
