#pragma once

#include <vector>
#include <cmath>
#include <cfloat>
#include <algorithm>

namespace Collision {

// Simple, self-contained Vector3 implementation to hook seamlessly into any engine math layout.
// Using standard Q2 vector layout
struct Vector3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;

    Vector3() = default;
    Vector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
    Vector3(const ::Vector3& v) : x(v.x), y(v.y), z(v.z) {}

    Vector3 operator+(const Vector3& v) const { return {x + v.x, y + v.y, z + v.z}; }
    Vector3 operator-(const Vector3& v) const { return {x - v.x, y - v.y, z - v.z}; }
    Vector3 operator-() const { return {-x, -y, -z}; }
    Vector3 operator*(float s) const { return {x * s, y * s, z * s}; }
    
    float Dot(const Vector3& v) const { return x * v.x + y * v.y + z * v.z; }
    Vector3 Cross(const Vector3& v) const {
        return { y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x };
    }
    float LengthSq() const { return Dot(*this); }
    float Length() const { return std::sqrt(LengthSq()); }
    Vector3 Normalized() const {
        float len = Length();
        return len > FLT_EPSILON ? *this * (1.0f / len) : Vector3{0, 0, 0};
    }
};

// --- Shape Definitions and Support Functions ---

struct Sphere {
    Vector3 center;
    float radius;
    Vector3 GetSupport(const Vector3& dir) const {
        return center + dir.Normalized() * radius;
    }
};

struct Box {
    Vector3 center;
    Vector3 halfExtents; // Local aligned extents
    // Note: If rotated, multiply the dir by local matrix orientation beforehand
    Vector3 GetSupport(const Vector3& dir) const {
        return center + Vector3(
            (dir.x >= 0.0f ? halfExtents.x : -halfExtents.x),
            (dir.y >= 0.0f ? halfExtents.y : -halfExtents.y),
            (dir.z >= 0.0f ? halfExtents.z : -halfExtents.z)
        );
    }
};

struct Capsule {
    Vector3 base;
    Vector3 tip;
    float radius;
    Vector3 GetSupport(const Vector3& dir) const {
        Vector3 dNorm = dir.Normalized();
        float dotBase = base.Dot(dNorm);
        float dotTip = tip.Dot(dNorm);
        Vector3 furthestPoint = (dotTip > dotBase) ? tip : base;
        return furthestPoint + dNorm * radius;
    }
};

struct Cylinder {
    Vector3 center;
    float radius;
    float halfHeight; // Aligned along Z axis for Q2
    Vector3 GetSupport(const Vector3& dir) const {
        Vector3 dirXY(dir.x, dir.y, 0.0f);
        Vector3 planeDir = dirXY.Normalized();
        return center + Vector3(
            planeDir.x * radius,
            planeDir.y * radius,
            (dir.z >= 0.0f ? halfHeight : -halfHeight)
        );
    }
};

struct ConvexHull {
    const Vector3* vertices;
    size_t count;
    Vector3 GetSupport(const Vector3& dir) const {
        size_t maxIdx = 0;
        float maxDot = vertices[0].Dot(dir);
        for(size_t i = 1; i < count; ++i) {
            float dot = vertices[i].Dot(dir);
            if(dot > maxDot) {
                maxDot = dot;
                maxIdx = i;
            }
        }
        return vertices[maxIdx];
    }
};

// Generic structure matching the Minkowski Difference computation
template<typename ShapeA, typename ShapeB>
Vector3 GetMinkowskiSupport(const ShapeA& a, const ShapeB& b, const Vector3& dir) {
    return a.GetSupport(dir) - b.GetSupport(-dir);
}

// Result structure containing extraction information required for collision resolution
struct CollisionResult {
    bool intersecting = false;
    Vector3 normal;
    float penetrationDepth = 0.0f;
};

// --- GJK and EPA Engine Internals ---

struct Simplex {
    Vector3 points[4];
    int size = 0;

    void PushFront(const Vector3& point) {
        points[3] = points[2];
        points[2] = points[1];
        points[1] = points[0];
        points[0] = point;
        size = std::min(size + 1, 4);
    }
};

inline bool UpdateSimplex(Simplex& s, Vector3& d) {
    if (s.size == 2) { // Line segment
        Vector3 ab = s.points[1] - s.points[0];
        Vector3 ao = -s.points[0];
        if (ab.Dot(ao) > 0) {
            d = ab.Cross(ao).Cross(ab);
        } else {
            s.size = 1;
            d = ao;
        }
    } else if (s.size == 3) { // Triangle
        Vector3 ab = s.points[1] - s.points[0];
        Vector3 ac = s.points[2] - s.points[0];
        Vector3 abc = ab.Cross(ac);
        Vector3 ao = -s.points[0];

        if (abc.Cross(ac).Dot(ao) > 0) {
            if (ac.Dot(ao) > 0) {
                s.points[1] = s.points[2];
                s.size = 2;
                d = ac.Cross(ao).Cross(ac);
            } else {
                s.size = 2;
                s.points[2] = s.points[1]; // fallback line
                return UpdateSimplex(s, d);
            }
        } else {
            if (ab.Cross(abc).Dot(ao) > 0) {
                s.size = 2;
                return UpdateSimplex(s, d);
            } else {
                if (abc.Dot(ao) > 0) {
                    d = abc;
                } else {
                    std::swap(s.points[1], s.points[2]);
                    d = -abc;
                }
            }
        }
    } else if (s.size == 4) { // Tetrahedron
        Vector3 abc = (s.points[1] - s.points[0]).Cross(s.points[2] - s.points[0]);
        Vector3 acd = (s.points[2] - s.points[0]).Cross(s.points[3] - s.points[0]);
        Vector3 adb = (s.points[3] - s.points[0]).Cross(s.points[1] - s.points[0]);
        Vector3 ao = -s.points[0];

        if (abc.Dot(ao) > 0) {
            s.size = 3;
            return UpdateSimplex(s, d);
        }
        if (acd.Dot(ao) > 0) {
            s.points[1] = s.points[2];
            s.points[2] = s.points[3];
            s.size = 3;
            return UpdateSimplex(s, d);
        }
        if (adb.Dot(ao) > 0) {
            s.points[2] = s.points[1];
            s.points[1] = s.points[3];
            s.size = 3;
            return UpdateSimplex(s, d);
        }
        return true; // Overlap detected
    }
    return false;
}

struct Face {
    size_t a, b, c;
    Vector3 normal;
    float distance;
};

template<typename ShapeA, typename ShapeB>
CollisionResult RunEPA(const Simplex& simplex, const ShapeA& a, const ShapeB& b) {
    std::vector<Vector3> polytope(simplex.points, simplex.points + 4);
    std::vector<Face> faces = {
        {0, 1, 2, Vector3(), 0.0f}, {0, 2, 3, Vector3(), 0.0f},
        {0, 3, 1, Vector3(), 0.0f}, {1, 3, 2, Vector3(), 0.0f}
    };

    auto computeFaceNorm = [&](Face& f) {
        Vector3 ab = polytope[f.b] - polytope[f.a];
        Vector3 ac = polytope[f.c] - polytope[f.a];
        f.normal = ab.Cross(ac).Normalized();
        f.distance = f.normal.Dot(polytope[f.a]);
        if (f.distance < 0) {
            f.normal = -f.normal;
            f.distance = -f.distance;
        }
    };

    for (auto& f : faces) computeFaceNorm(f);

    size_t minFaceIdx = 0;
    const int MAX_EPA_ITERATIONS = 40;

    for (int iter = 0; iter < MAX_EPA_ITERATIONS; ++iter) {
        minFaceIdx = 0;
        float minDst = FLT_MAX;
        for (size_t i = 0; i < faces.size(); ++i) {
            if (faces[i].distance < minDst) {
                minDst = faces[i].distance;
                minFaceIdx = i;
            }
        }

        Vector3 searchDir = faces[minFaceIdx].normal;
        Vector3 p = GetMinkowskiSupport(a, b, searchDir);
        float d = p.Dot(searchDir);

        if (d - minDst < 0.001f) {
            return { true, faces[minFaceIdx].normal, minDst };
        }

        polytope.push_back(p);
        size_t pIdx = polytope.size() - 1;

        std::vector<std::pair<size_t, size_t>> uniqueEdges;
        auto addEdge = [&](size_t u, size_t v) {
            auto it = std::find_if(uniqueEdges.begin(), uniqueEdges.end(), [&](const std::pair<size_t, size_t>& edge) {
                return (edge.first == v && edge.second == u);
            });
            if (it != uniqueEdges.end()) uniqueEdges.erase(it);
            else uniqueEdges.push_back({u, v});
        };

        for (size_t i = 0; i < faces.size();) {
            if ((polytope[pIdx] - polytope[faces[i].a]).Dot(faces[i].normal) > 0) {
                addEdge(faces[i].a, faces[i].b);
                addEdge(faces[i].b, faces[i].c);
                addEdge(faces[i].c, faces[i].a);
                faces.erase(faces.begin() + i);
            } else {
                ++i;
            }
        }

        for (const auto& edge : uniqueEdges) {
            Face newFace{ edge.first, edge.second, pIdx, Vector3(), 0.0f };
            computeFaceNorm(newFace);
            faces.push_back(newFace);
        }
    }

    return { true, faces[minFaceIdx].normal, faces[minFaceIdx].distance };
}

template<typename ShapeA, typename ShapeB>
CollisionResult CheckCollision(const ShapeA& shapeA, const ShapeB& shapeB) {
    Simplex simplex;
    Vector3 dir(1.0f, 0.0f, 0.0f); // Default search axis
    Vector3 support = GetMinkowskiSupport(shapeA, shapeB, dir);
    simplex.PushFront(support);
    dir = -support;

    const int MAX_GJK_ITERATIONS = 32;
    for (int i = 0; i < MAX_GJK_ITERATIONS; ++i) {
        support = GetMinkowskiSupport(shapeA, shapeB, dir);
        if (support.Dot(dir) <= 0) {
            return { false, Vector3(0,0,0), 0.0f }; // No intersection
        }
        simplex.PushFront(support);
        if (UpdateSimplex(simplex, dir)) {
            return RunEPA(simplex, shapeA, shapeB);
        }
    }
    return { false, Vector3(0,0,0), 0.0f };
}

} // namespace Collision
