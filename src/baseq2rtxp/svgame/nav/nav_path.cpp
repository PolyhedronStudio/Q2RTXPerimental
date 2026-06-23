#include "svgame/nav/nav_path.h"
#include "svgame/nav/nav_containers.h"
#include "svgame/nav/nav_core.h"
#include "svgame/nav/nav_generate.h" // For g_nav_nodes, g_nav_polys
#include "svgame/svg_utils.h"
#include "shared/math/qm_vector3.h"

#include <queue>
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include <vector>

// Half-edge graph is already built in generation phase! No lazy building needed.
void Nav_BuildAdjacencyGraph() {
    // No-op. The graph is built explicitly via Nav_BuildHalfEdgeMesh.
}

/**
*   @brief  Walk the KD-tree to locate the leaf node that contains @p point.
*   @param  point   World-space position to query.
*   @return Index of the leaf node (-1 if the point lies outside the world bounds).
**/
int32_t Nav_FindLeafNode( const Vector3 &point ) {
    if (g_nav_nodes.empty()) return -1;
    
    const Vector3& mins = g_nav_nodes[0].mins;
    const Vector3& maxs = g_nav_nodes[0].maxs;

    // Early-out: if the point is outside the world AABB we reject it.
    // Allow points outside the exact bounding box to still traverse the KD-Tree
    // so they can snap to the nearest boundary leaf.

    int32_t nodeIdx = 0; // start at the root
    
    while ( true ) {
        const nav_kdtree_node_t &node = g_nav_nodes[nodeIdx];

        // Leaf? (leaf nodes have both children == -1)
        if ( node.left_child == -1 && node.right_child == -1 ) {
            return nodeIdx;
        }

        bool leftSide = false;
        if (node.left_child != -1) {
            const nav_kdtree_node_t &leftChild = g_nav_nodes[node.left_child];
            if (node.split_axis == 0) {
                leftSide = ( point.x <= leftChild.maxs.x );
            } else if (node.split_axis == 1) {
                leftSide = ( point.y <= leftChild.maxs.y );
            } else if (node.split_axis == 2) {
                leftSide = ( point.z <= leftChild.maxs.z );
            }
        }

        nodeIdx = leftSide ? node.left_child : node.right_child;
        if ( nodeIdx == -1 ) return -1;
    }
}

/**
*   @brief  Check if a point lies within the 2D projection of a polygon.
*   @param  point   World-space position.
*   @param  poly    Polygon to check against.
*   @return True if the point is inside.
**/
bool Nav_PointInsideFace2D( const Vector3 &point, const nav_face_t &face ) {
    if ( face.num_edges < 3 ) return false;

    // Project onto XY plane
    for ( int e = 0; e < face.num_edges; e++ ) {
        const nav_halfedge_t& he = g_nav_halfedges[face.first_edge_idx + e];
        Vector3 a = g_nav_vertices[he.vertex_idx];
        Vector3 b = g_nav_vertices[g_nav_halfedges[he.next_idx].vertex_idx];

        Vector3 edge = QM_Vector3Subtract( b, a );
        Vector3 toPoint = QM_Vector3Subtract( point, a );

        float cross2d = edge.x * toPoint.y - edge.y * toPoint.x;
        
        // The NavMesh polygons are not shrunk by entity radius.
        // If an entity is sliding against a wall, its origin is 16 units outside the poly.
        // We allow a tolerance of 24 units.
        float edgeLen = std::sqrt( edge.x * edge.x + edge.y * edge.y );
        if ( cross2d > 24.0f * edgeLen ) {
            return false;
        }
    }
    return true;
}

int32_t Nav_FindClosestPolyGlobal( const Vector3 &point ) {
    int32_t bestInsideFace = -1;
    float bestInsideDist = 64.0f; // vertical threshold for being "inside"

    int32_t bestFallbackFace = -1;
    float bestFallbackDist = 999999.0f;
    
    for ( size_t i = 0; i < g_nav_faces.size(); ++i ) {
        const nav_face_t &face = g_nav_faces[i];
        
        Vector3 v0 = g_nav_vertices[g_nav_halfedges[face.first_edge_idx].vertex_idx];
        const float plane_dist = static_cast<float>(QM_Vector3DotProduct( v0, face.normal ));
        const float d = std::fabs(static_cast<float>(QM_Vector3DotProduct( point, face.normal )) - plane_dist);
        
        if ( d < bestInsideDist ) {
            if ( Nav_PointInsideFace2D( point, face ) ) {
                bestInsideDist = d;
                bestInsideFace = static_cast<int32_t>(i);
            }
        }

        const float dx = point.x - face.center.x;
        const float dy = point.y - face.center.y;
        const float dz = point.z - face.center.z;
        const float centerDist = std::sqrt(dx*dx + dy*dy + dz*dz);
        
        if ( centerDist < bestFallbackDist ) {
            bestFallbackDist = centerDist;
            bestFallbackFace = static_cast<int32_t>(i);
        }
    }
    
    if ( bestInsideFace != -1 ) {
        return bestInsideFace;
    }
    return bestFallbackFace;
}

int32_t Nav_FindPolyInLeaf( const Vector3 &point ) {
    const int32_t leafIdx = Nav_FindLeafNode( point );
    if ( leafIdx < 0 ) return Nav_FindClosestPolyGlobal( point );

    const nav_kdtree_node_t &leaf = g_nav_nodes[leafIdx];
    int32_t firstFaceIdx = leaf.first_face_id;
    if (firstFaceIdx == -1 || firstFaceIdx >= static_cast<int32_t>(g_nav_faces.size())) {
        return Nav_FindClosestPolyGlobal( point );
    }

    int32_t bestInsideFace = -1;
    float bestInsideDist = 64.0f;
    
    int32_t bestFallbackFace = -1;
    float bestFallbackDist = 999999.0f;

    for ( int32_t i = 0; i < leaf.num_faces; ++i ) {
        const int32_t faceIdx = firstFaceIdx + i;
        if (faceIdx >= static_cast<int32_t>(g_nav_faces.size())) break;
        
        const nav_face_t &face = g_nav_faces[faceIdx];

        Vector3 v0 = g_nav_vertices[g_nav_halfedges[face.first_edge_idx].vertex_idx];
        const float plane_dist = static_cast<float>(QM_Vector3DotProduct( v0, face.normal ));
        const float d = std::fabs(static_cast<float>(QM_Vector3DotProduct( point, face.normal )) - plane_dist);
        
        if ( d < bestInsideDist ) {
            if ( Nav_PointInsideFace2D( point, face ) ) {
                bestInsideDist = d;
                bestInsideFace = faceIdx;
            }
        }

        const float dx = point.x - face.center.x;
        const float dy = point.y - face.center.y;
        const float dz = point.z - face.center.z;
        const float centerDist = std::sqrt(dx*dx + dy*dy + dz*dz);
        
        if ( centerDist < bestFallbackDist ) {
            bestFallbackDist = centerDist;
            bestFallbackFace = faceIdx;
        }
    }

    if ( bestInsideFace != -1 ) {
        return bestInsideFace;
    }
    if ( bestFallbackFace == -1 ) {
        return Nav_FindClosestPolyGlobal( point );
    }
    return bestFallbackFace;
}

/**
*   @brief  Calculate the portal segment endpoints between two adjacent nav polygons.
*   @details  Considers vertical height differences for stairs (≤ NAV_MAX_STEP_SIZE) and drop‑offs.
*   @param   faceA       Index of the first polygon.
*   @param   faceB       Index of the second polygon.
*   @param   outV0       Output vector receiving the first endpoint of the portal.
*   @param   outV1       Output vector receiving the second endpoint of the portal.
*   @return  True if a shared edge was identified (including stairs/drop‑offs). Returns false on invalid indices.
*/
bool Nav_GetPortalEndpoints( int32_t faceA, int32_t faceB, Vector3 *outV0, Vector3 *outV1 ) {
	if ( outV0 == nullptr || outV1 == nullptr || faceA < 0 || faceB < 0 ||
		static_cast<size_t>(faceA) >= g_nav_faces.size() ||
		static_cast<size_t>(faceB) >= g_nav_faces.size() ) {
		return false;
	}

	const nav_face_t &fA = g_nav_faces[faceA];
	for ( int32_t e = 0; e < fA.num_edges; ++e ) {
		const nav_halfedge_t &he = g_nav_halfedges[fA.first_edge_idx + e];
		if ( he.twin_idx != -1 ) {
			const nav_halfedge_t &twin = g_nav_halfedges[he.twin_idx];
			if ( twin.face_idx == faceB ) {
				*outV0 = g_nav_vertices[he.vertex_idx];
				*outV1 = g_nav_vertices[g_nav_halfedges[he.next_idx].vertex_idx];
                
                // BUGFIX: Set Z height to the maximum Z of both faces that share this twin edge
                // so that NextWaypoint correctly detects that a vertical step-up is required.
                float maxZ = std::max(outV0->z, outV1->z);
                Vector3 tv0 = g_nav_vertices[twin.vertex_idx];
                Vector3 tv1 = g_nav_vertices[g_nav_halfedges[twin.next_idx].vertex_idx];
                maxZ = std::max({ maxZ, tv0.z, tv1.z });
                
                outV0->z = maxZ;
                outV1->z = maxZ;

                return true;
			}
		}
	}

	return false;
}


/**
*   AStar Node definition
**/
struct AStarNode {
    int32_t polyIdx;
    float fScore;
    
    bool operator>(const AStarNode& other) const {
        return fScore > other.fScore;
    }
};

/**
*   @brief  Compute a path using A* from start to goal polygon.
**/
bool Nav_FindPath( int32_t startFace, int32_t goalFace, std::vector<int32_t> &outPath, const nav_path_policy_t &policy ) {
    outPath.clear();
    
    if ( startFace < 0 || goalFace < 0 || startFace >= static_cast<int32_t>(g_nav_faces.size()) || goalFace >= static_cast<int32_t>(g_nav_faces.size()) ) {
        return false;
    }

    if ( startFace == goalFace ) {
        outPath.push_back( startFace );
        return true;
    }

    std::priority_queue<AStarNode, std::vector<AStarNode>, std::greater<AStarNode>> openSet;
    std::unordered_map<int32_t, int32_t> cameFrom;
    std::unordered_map<int32_t, float> gScore;

    gScore[startFace] = 0.0f;
    
    auto Heuristic = [](int32_t a, int32_t b) {
        return static_cast<float>(QM_Vector3Distance(g_nav_faces[a].center, g_nav_faces[b].center));
    };

    openSet.push({startFace, Heuristic(startFace, goalFace)});

    int32_t closestPoly = startFace;
    float closestDist = Heuristic(startFace, goalFace);

    while (!openSet.empty()) {
        int32_t current = openSet.top().polyIdx;
        openSet.pop();

        if (current == goalFace) {
            // Reconstruct path
            int32_t curr = current;
            while (curr != startFace) {
                outPath.push_back(curr);
                curr = cameFrom[curr];
            }
            outPath.push_back(startFace);
            std::reverse(outPath.begin(), outPath.end());
            return true;
        }

        const nav_face_t &face = g_nav_faces[current];
        for (int32_t e = 0; e < face.num_edges; ++e) {
            const nav_halfedge_t& he = g_nav_halfedges[face.first_edge_idx + e];
            if (he.twin_idx != -1) {
                int32_t neighbor = g_nav_halfedges[he.twin_idx].face_idx;
                
                const nav_face_t &neighborFace = g_nav_faces[neighbor];
                const nav_halfedge_t &twin = g_nav_halfedges[he.twin_idx];

                // Horizontal clearance check removed from AStar graph traversal.
                // Graph connectivity should not be broken just because a polygon's center is close to its edge.
                // Steering/Clearance is handled dynamically during actual movement.

                float dz = he.z_diff;

                // Allow a vertical clearance buffer before attempting a step. This gives the agent a little extra space to approach the stair edge.
                float allowedStep = policy.max_step_height + policy.step_clearance;
                if ( dz > allowedStep ) {
                    continue;
                }
                if ( dz < -policy.max_drop_height ) {
                    continue;
                }

                float dist2D = static_cast<float>(QM_Vector2Distance(face.center, neighborFace.center));
                float distZ = std::abs(face.center.z - neighborFace.center.z);
                
                // Heavily penalize vertical movement so monsters prefer walking around pyramids/obstacles 
                // instead of climbing them like mountain goats, unless the flat path is significantly longer.
                float tentative_gScore = gScore[current] + dist2D + (distZ * 5.0f);
                
                auto it = gScore.find(neighbor);
                if (it == gScore.end() || tentative_gScore < it->second) {
                    cameFrom[neighbor] = current;
                    gScore[neighbor] = tentative_gScore;
                    float h = Heuristic(neighbor, goalFace);
                    if ( h < closestDist ) {
                        closestDist = h;
                        closestPoly = neighbor;
                    }
                    openSet.push({neighbor, tentative_gScore + h});
                }
            }
        }
    }

    /**
    *	No path to the exact goal was found.
    **/
    // Reject partial closest-poly routes so callers do not steer away from the real target.
    outPath.clear();
    return false;
}
