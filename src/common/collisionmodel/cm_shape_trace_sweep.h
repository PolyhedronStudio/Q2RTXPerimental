// Included by cm_shape_trace.cpp

#include <algorithm>

static constexpr float SWEEP_DIST_EPSILON = 0.03125f;


inline bool CM_IsBrushAxialBox( const mbrush_t *brush, Vector3 &mins, Vector3 &maxs ) {
    if ( brush->numsides != 6 ) return false;
    int32_t matched = 0;
    for ( int32_t i = 0; i < brush->numsides; i++ ) {
        const cm_plane_t *p = brush->firstbrushside[ i ].plane;
        if ( p->normal[0] == 1.0f && p->normal[1] == 0.0f && p->normal[2] == 0.0f ) { maxs.x = p->dist; matched |= 1; }
        else if ( p->normal[0] == -1.0f && p->normal[1] == 0.0f && p->normal[2] == 0.0f ) { mins.x = -p->dist; matched |= 2; }
        else if ( p->normal[0] == 0.0f && p->normal[1] == 1.0f && p->normal[2] == 0.0f ) { maxs.y = p->dist; matched |= 4; }
        else if ( p->normal[0] == 0.0f && p->normal[1] == -1.0f && p->normal[2] == 0.0f ) { mins.y = -p->dist; matched |= 8; }
        else if ( p->normal[0] == 0.0f && p->normal[1] == 0.0f && p->normal[2] == 1.0f ) { maxs.z = p->dist; matched |= 16; }
        else if ( p->normal[0] == 0.0f && p->normal[1] == 0.0f && p->normal[2] == -1.0f ) { mins.z = -p->dist; matched |= 32; }
    }
    return matched == 63;
}

static void CM_TestFace(float planeDist, float rayStart, float rayDir, int axis, float min1, float max1, float min2, float max2, float p1_1, float v_1, float p1_2, float v_2, Vector3 n, float& tHit, Vector3& nHit, bool& hit) {
    if (rayDir * n[axis] < 0.0f) {
        float t = (planeDist - rayStart) / rayDir;
        if (t >= 0.0f && t < tHit) {
            float h1 = p1_1 + v_1 * t;
            float h2 = p1_2 + v_2 * t;
            if (h1 >= min1 && h1 <= max1 && h2 >= min2 && h2 <= max2) {
                tHit = t; nHit = n; hit = true;
            }
        }
    }
}

static void CM_TestCylinderZFace(float planeDist, float rayStart, float rayDir, const Vector3& bMins, const Vector3& bMaxs, float R, const Vector3& p1, const Vector3& V, Vector3 n, float& tHit, Vector3& nHit, bool& hit) {
    if (rayDir * n.z < 0.0f) {
        float t = (planeDist - rayStart) / rayDir;
        if (t >= 0.0f && t < tHit) {
            float hx = p1.x + V.x * t;
            float hy = p1.y + V.y * t;
            float cx = std::max(bMins.x, std::min(hx, bMaxs.x));
            float cy = std::max(bMins.y, std::min(hy, bMaxs.y));
            float dx = hx - cx;
            float dy = hy - cy;
            if (dx*dx + dy*dy <= R*R + 0.0001f) {
                tHit = t; nHit = n; hit = true;
            }
        }
    }
}

static void CM_TestEdge(float cx, float cy, float rayStart1, float rayDir1, float rayStart2, float rayDir2, float rayStart3, float rayDir3, float min3, float max3, float R, int axis1, int axis2, float& tHit, Vector3& nHit, bool& hit) {
    float dx = rayStart1 - cx;
    float dy = rayStart2 - cy;
    float a = rayDir1 * rayDir1 + rayDir2 * rayDir2;
    if (a < 0.00001f) return;
    float b = 2.0f * (dx * rayDir1 + dy * rayDir2);
    float c = dx * dx + dy * dy - R * R;
    float d = b * b - 4.0f * a * c;
    if (d >= 0.0f) {
        float t = (-b - std::sqrt(d)) / (2.0f * a);
        if (t >= 0.0f && t < tHit) {
            float h3 = rayStart3 + rayDir3 * t;
            if (h3 >= min3 && h3 <= max3) {
                float h1 = rayStart1 + rayDir1 * t;
                float h2 = rayStart2 + rayDir2 * t;
                Vector3 n = {0,0,0};
                n[axis1] = h1 - cx;
                n[axis2] = h2 - cy;
                if (n[axis1] * rayDir1 + n[axis2] * rayDir2 < 0.0f) {
                    float len = std::sqrt(n[axis1]*n[axis1] + n[axis2]*n[axis2]);
                    if (len > 0.0001f) {
                        tHit = t; nHit = {n.x/len, n.y/len, n.z/len}; hit = true;
                    }
                }
            }
        }
    }
}

static void CM_TestCorner(float cx, float cy, float cz, const Vector3& p1, const Vector3& V, float R, float& tHit, Vector3& nHit, bool& hit) {
    float dx = p1.x - cx;
    float dy = p1.y - cy;
    float dz = p1.z - cz;
    float a = V.x * V.x + V.y * V.y + V.z * V.z;
    if (a < 0.00001f) return;
    float b = 2.0f * (dx * V.x + dy * V.y + dz * V.z);
    float c = dx * dx + dy * dy + dz * dz - R * R;
    float d = b * b - 4.0f * a * c;
    if (d >= 0.0f) {
        float t = (-b - std::sqrt(d)) / (2.0f * a);
        if (t >= 0.0f && t < tHit) {
            float hx = p1.x + V.x * t;
            float hy = p1.y + V.y * t;
            float hz = p1.z + V.z * t;
            Vector3 n = {hx - cx, hy - cy, hz - cz};
            if (n.x * V.x + n.y * V.y + n.z * V.z < 0.0f) {
                float len = std::sqrt(n.x*n.x + n.y*n.y + n.z*n.z);
                if (len > 0.0001f) {
                    tHit = t; nHit = {n.x/len, n.y/len, n.z/len}; hit = true;
                }
            }
        }
    }
}

static bool CM_SweepShapeVsAxialBox( const Vector3& p1, const Vector3& p2, 
                                     float R, float H, cm_trace_shape_type_t shapeType, 
                                     const Vector3& boxMins, const Vector3& boxMaxs,
                                     float& out_tHit, Vector3& out_nHit, bool& startsolid )
{
    Vector3 bMins = boxMins;
    Vector3 bMaxs = boxMaxs;
    if (shapeType != SHAPE_SPHERE) {
        bMins.z -= H;
        bMaxs.z += H;
    }

    // Check startsolid
    Vector3 closest = Vector3{
        std::max(bMins.x, std::min(p1.x, bMaxs.x)),
        std::max(bMins.y, std::min(p1.y, bMaxs.y)),
        std::max(bMins.z, std::min(p1.z, bMaxs.z))
    };
    
    float r_shrunk = std::max(0.0f, R - SWEEP_DIST_EPSILON);
    float r_shrunk_sq = r_shrunk * r_shrunk;

    const auto IsInside = [&]( const Vector3 &point ) {
        const Vector3 pointClosest = Vector3{
            std::max(bMins.x, std::min(point.x, bMaxs.x)),
            std::max(bMins.y, std::min(point.y, bMaxs.y)),
            std::max(bMins.z, std::min(point.z, bMaxs.z))
        };

        if ( shapeType == SHAPE_CYLINDER ) {
            const float dx = point.x - pointClosest.x;
            const float dy = point.y - pointClosest.y;
            return dx * dx + dy * dy < r_shrunk_sq &&
                point.z > bMins.z + SWEEP_DIST_EPSILON &&
                point.z < bMaxs.z - SWEEP_DIST_EPSILON;
        }

        const Vector3 delta = point - pointClosest;
        return delta.x * delta.x + delta.y * delta.y + delta.z * delta.z < r_shrunk_sq;
    };

    if ( IsInside( p1 ) ) {
        // Starting inside is not necessarily allsolid: the mover may be able to
        // leave this brush during the sweep. Only report allsolid when the end
        // point remains inside the expanded brush as well.
        if ( IsInside( p2 ) ) {
            startsolid = true;
            out_tHit = 0.0f;
            return true;
        }

        // The sweep exits the brush, so report startsolid without converting
        // the trace into an allsolid stop. There is no blocking impact plane
        // for a sweep that ends outside the brush.
        startsolid = true;
        out_tHit = 1.0f;
        return true;
    }

    float tHit = 1.0f;
    bool hit = false;
    Vector3 nHit = {0,0,0};
    Vector3 V = p2 - p1;

    // Test Faces
    CM_TestFace(bMaxs.x + R, p1.x, V.x, 0, bMins.y, bMaxs.y, bMins.z, bMaxs.z, p1.y, V.y, p1.z, V.z, Vector3{1,0,0}, tHit, nHit, hit);
    CM_TestFace(bMins.x - R, p1.x, V.x, 0, bMins.y, bMaxs.y, bMins.z, bMaxs.z, p1.y, V.y, p1.z, V.z, Vector3{-1,0,0}, tHit, nHit, hit);
    CM_TestFace(bMaxs.y + R, p1.y, V.y, 1, bMins.x, bMaxs.x, bMins.z, bMaxs.z, p1.x, V.x, p1.z, V.z, Vector3{0,1,0}, tHit, nHit, hit);
    CM_TestFace(bMins.y - R, p1.y, V.y, 1, bMins.x, bMaxs.x, bMins.z, bMaxs.z, p1.x, V.x, p1.z, V.z, Vector3{0,-1,0}, tHit, nHit, hit);
    
    if (shapeType == SHAPE_CYLINDER) {
        CM_TestCylinderZFace(bMaxs.z, p1.z, V.z, bMins, bMaxs, R, p1, V, Vector3{0,0,1}, tHit, nHit, hit);
        CM_TestCylinderZFace(bMins.z, p1.z, V.z, bMins, bMaxs, R, p1, V, Vector3{0,0,-1}, tHit, nHit, hit);
    } else {
        CM_TestFace(bMaxs.z + R, p1.z, V.z, 2, bMins.x, bMaxs.x, bMins.y, bMaxs.y, p1.x, V.x, p1.y, V.y, Vector3{0,0,1}, tHit, nHit, hit);
        CM_TestFace(bMins.z - R, p1.z, V.z, 2, bMins.x, bMaxs.x, bMins.y, bMaxs.y, p1.x, V.x, p1.y, V.y, Vector3{0,0,-1}, tHit, nHit, hit);
    }

    // Test Z Edges (for all shapes)
    CM_TestEdge(bMaxs.x, bMaxs.y, p1.x, V.x, p1.y, V.y, p1.z, V.z, bMins.z, bMaxs.z, R, 0, 1, tHit, nHit, hit);
    CM_TestEdge(bMins.x, bMaxs.y, p1.x, V.x, p1.y, V.y, p1.z, V.z, bMins.z, bMaxs.z, R, 0, 1, tHit, nHit, hit);
    CM_TestEdge(bMaxs.x, bMins.y, p1.x, V.x, p1.y, V.y, p1.z, V.z, bMins.z, bMaxs.z, R, 0, 1, tHit, nHit, hit);
    CM_TestEdge(bMins.x, bMins.y, p1.x, V.x, p1.y, V.y, p1.z, V.z, bMins.z, bMaxs.z, R, 0, 1, tHit, nHit, hit);

    if (shapeType == SHAPE_SPHERE || shapeType == SHAPE_CAPSULE) {
        // Test X Edges
        CM_TestEdge(bMaxs.y, bMaxs.z, p1.y, V.y, p1.z, V.z, p1.x, V.x, bMins.x, bMaxs.x, R, 1, 2, tHit, nHit, hit);
        CM_TestEdge(bMins.y, bMaxs.z, p1.y, V.y, p1.z, V.z, p1.x, V.x, bMins.x, bMaxs.x, R, 1, 2, tHit, nHit, hit);
        CM_TestEdge(bMaxs.y, bMins.z, p1.y, V.y, p1.z, V.z, p1.x, V.x, bMins.x, bMaxs.x, R, 1, 2, tHit, nHit, hit);
        CM_TestEdge(bMins.y, bMins.z, p1.y, V.y, p1.z, V.z, p1.x, V.x, bMins.x, bMaxs.x, R, 1, 2, tHit, nHit, hit);

        // Test Y Edges
        CM_TestEdge(bMaxs.x, bMaxs.z, p1.x, V.x, p1.z, V.z, p1.y, V.y, bMins.y, bMaxs.y, R, 0, 2, tHit, nHit, hit);
        CM_TestEdge(bMins.x, bMaxs.z, p1.x, V.x, p1.z, V.z, p1.y, V.y, bMins.y, bMaxs.y, R, 0, 2, tHit, nHit, hit);
        CM_TestEdge(bMaxs.x, bMins.z, p1.x, V.x, p1.z, V.z, p1.y, V.y, bMins.y, bMaxs.y, R, 0, 2, tHit, nHit, hit);
        CM_TestEdge(bMins.x, bMins.z, p1.x, V.x, p1.z, V.z, p1.y, V.y, bMins.y, bMaxs.y, R, 0, 2, tHit, nHit, hit);

        // Test Corners
        CM_TestCorner(bMaxs.x, bMaxs.y, bMaxs.z, p1, V, R, tHit, nHit, hit);
        CM_TestCorner(bMins.x, bMaxs.y, bMaxs.z, p1, V, R, tHit, nHit, hit);
        CM_TestCorner(bMaxs.x, bMins.y, bMaxs.z, p1, V, R, tHit, nHit, hit);
        CM_TestCorner(bMins.x, bMins.y, bMaxs.z, p1, V, R, tHit, nHit, hit);
        CM_TestCorner(bMaxs.x, bMaxs.y, bMins.z, p1, V, R, tHit, nHit, hit);
        CM_TestCorner(bMins.x, bMaxs.y, bMins.z, p1, V, R, tHit, nHit, hit);
        CM_TestCorner(bMaxs.x, bMins.y, bMins.z, p1, V, R, tHit, nHit, hit);
        CM_TestCorner(bMins.x, bMins.y, bMins.z, p1, V, R, tHit, nHit, hit);
    }

    if (hit) {
        out_tHit = tHit;
        out_nHit = nHit;
        return true;
    }
    return false;
}
