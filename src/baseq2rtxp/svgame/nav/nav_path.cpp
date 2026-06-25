#include "svgame/nav/nav_path.h"
#include "svgame/nav/nav_containers.h"
#include "svgame/nav/nav_core.h"
#include "svgame/nav/nav_generate.h" // For g_nav_nodes and g_nav_polys.
#include "svgame/svg_utils.h"
#include "shared/math/qm_vector3.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_map>
#include <vector>

/**
* @brief Legacy no-op placeholder for adjacency graph construction.
* @note The half-edge mesh already stores adjacency during generation.
**/
void Nav_BuildAdjacencyGraph() {
// The graph is built explicitly via Nav_BuildHalfEdgeMesh, so no work is needed here.
}

/**
* @brief Walk the KD-tree to locate the leaf node that contains a point.
* @param point World-space position to query.
* @return Index of the leaf node, or -1 when the point lies outside the tree.
**/
int32_t Nav_FindLeafNode( const Vector3 &point ) {
// Reject empty trees immediately.
if ( g_nav_nodes.empty() ) {
return -1;
}

// Start at the root and descend until a leaf is found.
int32_t nodeIdx = 0;
while ( true ) {
const nav_kdtree_node_t &node = g_nav_nodes[ nodeIdx ];

// Leaf nodes have no children, so the current index is the answer.
if ( node.left_child == -1 && node.right_child == -1 ) {
return nodeIdx;
}

// Choose the child that contains the query point on the split axis.
bool leftSide = false;
if ( node.left_child != -1 ) {
const nav_kdtree_node_t &leftChild = g_nav_nodes[ node.left_child ];
if ( node.split_axis == 0 ) {
leftSide = ( point.x <= leftChild.maxs.x );
} else if ( node.split_axis == 1 ) {
leftSide = ( point.y <= leftChild.maxs.y );
} else if ( node.split_axis == 2 ) {
leftSide = ( point.z <= leftChild.maxs.z );
}
}

nodeIdx = leftSide ? node.left_child : node.right_child;
if ( nodeIdx == -1 ) {
return -1;
}
}
}

/**
* @brief Check if a point lies inside the 2D projection of a face.
* @param point World-space position.
* @param face Face to test against.
* @return True if the point is inside the face outline.
**/
bool Nav_PointInsideFace2D( const Vector3 &point, const nav_face_t &face ) {
// Faces need at least three edges before they can contain a point.
if ( face.num_edges < 3 ) {
return false;
}

// Check every edge in the face winding using a 2D cross-product test.
for ( int32_t e = 0; e < face.num_edges; e++ ) {
const nav_halfedge_t &he = g_nav_halfedges[ face.first_edge_idx + e ];
const Vector3 a = g_nav_vertices[ he.vertex_idx ];
const Vector3 b = g_nav_vertices[ g_nav_halfedges[ he.next_idx ].vertex_idx ];

// Compare the point against the edge in XY space.
const Vector3 edge = QM_Vector3Subtract( b, a );
const Vector3 toPoint = QM_Vector3Subtract( point, a );
const float cross2d = edge.x * toPoint.y - edge.y * toPoint.x;

// Keep a tolerance so small wall-adjacent offsets still count as inside.
const float edgeLen = std::sqrt( edge.x * edge.x + edge.y * edge.y );
if ( cross2d > 24.0f * edgeLen ) {
return false;
}
}

return true;
}

/**
* @brief Fallback that finds the closest face globally by 3D distance.
* @param point World-space position to search from.
* @return Index of the closest face, or -1 if none exist.
**/
int32_t Nav_FindClosestPolyGlobal( const Vector3 &point ) {
// Track the best face that actually contains the point in 2D.
int32_t bestInsideFace = -1;
float bestInsideDist = 64.0f;

// Track the nearest face as a fallback when no face contains the point.
int32_t bestFallbackFace = -1;
float bestFallbackDist = 999999.0f;

// Scan every face because this is the final catch-all lookup.
for ( size_t i = 0; i < g_nav_faces.size(); ++i ) {
const nav_face_t &face = g_nav_faces[ i ];

// Measure vertical distance to the face plane for the inside test.
const Vector3 v0 = g_nav_vertices[ g_nav_halfedges[ face.first_edge_idx ].vertex_idx ];
const float plane_dist = static_cast<float>( QM_Vector3DotProduct( v0, face.normal ) );
const float d = std::fabs( static_cast<float>( QM_Vector3DotProduct( point, face.normal ) ) - plane_dist );

if ( d < bestInsideDist && Nav_PointInsideFace2D( point, face ) ) {
bestInsideDist = d;
bestInsideFace = static_cast<int32_t>( i );
}

// Also track the nearest face center as a fallback.
const float dx = point.x - face.center.x;
const float dy = point.y - face.center.y;
const float dz = point.z - face.center.z;
const float centerDist = std::sqrt( dx * dx + dy * dy + dz * dz );
if ( centerDist < bestFallbackDist ) {
bestFallbackDist = centerDist;
bestFallbackFace = static_cast<int32_t>( i );
}
}

if ( bestInsideFace != -1 ) {
return bestInsideFace;
}
return bestFallbackFace;
}

/**
* @brief Return the nav face that contains a point using the KD-tree leaf as a hint.
* @param point World-space position.
* @return Index of the face, or a global fallback if the leaf lookup fails.
**/
int32_t Nav_FindPolyInLeaf( const Vector3 &point ) {
// Find the leaf first so we can prefer local faces over a global scan.
const int32_t leafIdx = Nav_FindLeafNode( point );
if ( leafIdx < 0 ) {
return Nav_FindClosestPolyGlobal( point );
}

const nav_kdtree_node_t &leaf = g_nav_nodes[ leafIdx ];
const int32_t firstFaceIdx = leaf.first_face_id;
if ( firstFaceIdx == -1 || firstFaceIdx >= static_cast<int32_t>( g_nav_faces.size() ) ) {
return Nav_FindClosestPolyGlobal( point );
}

// Search only the candidate faces in this leaf first.
int32_t bestInsideFace = -1;
float bestInsideDist = 64.0f;
int32_t bestFallbackFace = -1;
float bestFallbackDist = 999999.0f;

for ( int32_t i = 0; i < leaf.num_faces; ++i ) {
const int32_t faceIdx = firstFaceIdx + i;
if ( faceIdx >= static_cast<int32_t>( g_nav_faces.size() ) ) {
break;
}

const nav_face_t &face = g_nav_faces[ faceIdx ];
const Vector3 v0 = g_nav_vertices[ g_nav_halfedges[ face.first_edge_idx ].vertex_idx ];
const float plane_dist = static_cast<float>( QM_Vector3DotProduct( v0, face.normal ) );
const float d = std::fabs( static_cast<float>( QM_Vector3DotProduct( point, face.normal ) ) - plane_dist );

if ( d < bestInsideDist && Nav_PointInsideFace2D( point, face ) ) {
bestInsideDist = d;
bestInsideFace = faceIdx;
}

const float dx = point.x - face.center.x;
const float dy = point.y - face.center.y;
const float dz = point.z - face.center.z;
const float centerDist = std::sqrt( dx * dx + dy * dy + dz * dz );
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
* @brief Calculate the portal segment endpoints between two adjacent nav polygons.
* @param faceA Index of the first polygon.
* @param faceB Index of the second polygon.
* @param outV0 Output receiving the first endpoint.
* @param outV1 Output receiving the second endpoint.
* @return True if a shared edge was identified.
**/
bool Nav_GetPortalEndpoints( int32_t faceA, int32_t faceB, Vector3 *outV0, Vector3 *outV1 ) {
// Validate inputs before touching the mesh arrays.
if ( outV0 == nullptr || outV1 == nullptr || faceA < 0 || faceB < 0 ||
static_cast<size_t>( faceA ) >= g_nav_faces.size() ||
static_cast<size_t>( faceB ) >= g_nav_faces.size() ) {
return false;
}

const nav_face_t &fA = g_nav_faces[ faceA ];
for ( int32_t e = 0; e < fA.num_edges; ++e ) {
const nav_halfedge_t &he = g_nav_halfedges[ fA.first_edge_idx + e ];
if ( he.twin_idx == -1 ) {
continue;
}

const nav_halfedge_t &twin = g_nav_halfedges[ he.twin_idx ];
if ( twin.face_idx != faceB ) {
continue;
}

// Capture the shared edge and lift it to the highest Z so stairs are treated consistently.
*outV0 = g_nav_vertices[ he.vertex_idx ];
*outV1 = g_nav_vertices[ g_nav_halfedges[ he.next_idx ].vertex_idx ];
float maxZ = std::max( outV0->z, outV1->z );
const Vector3 tv0 = g_nav_vertices[ twin.vertex_idx ];
const Vector3 tv1 = g_nav_vertices[ g_nav_halfedges[ twin.next_idx ].vertex_idx ];
maxZ = std::max( std::max( maxZ, tv0.z ), tv1.z );
outV0->z = maxZ;
outV1->z = maxZ;
return true;
}

return false;
}

/**
* @brief A* node entry used by Nav_FindPath.
**/
struct AStarNode {
int32_t polyIdx = -1;
float fScore = 0.0f;

bool operator>( const AStarNode &other ) const {
return fScore > other.fScore;
}
};

/**
* @brief Compute a path using A* from the start face to the goal face.
* @param startFace Index of the starting face.
* @param goalFace Index of the goal face.
* @param outPath Output vector that receives the face sequence.
* @param policy Traversal policy used to bound vertical movement.
* @return True when a valid path was found.
**/
bool Nav_FindPath( int32_t startFace, int32_t goalFace, std::vector<int32_t> &outPath, const nav_path_policy_t &policy ) {
// Start with a clean output path.
outPath.clear();

// Reject invalid indices up front.
if ( startFace < 0 || goalFace < 0 || startFace >= static_cast<int32_t>( g_nav_faces.size() ) || goalFace >= static_cast<int32_t>( g_nav_faces.size() ) ) {
return false;
}

// A zero-length path is valid when the start already matches the goal.
if ( startFace == goalFace ) {
outPath.push_back( startFace );
return true;
}

// Open set ordered by estimated total cost.
std::priority_queue<AStarNode, std::vector<AStarNode>, std::greater<AStarNode>> openSet;
std::unordered_map<int32_t, int32_t> cameFrom;
std::unordered_map<int32_t, float> gScore;

// Seed the search from the start face.
gScore[ startFace ] = 0.0f;
auto Heuristic = []( int32_t a, int32_t b ) {
return static_cast<float>( QM_Vector3Distance( g_nav_faces[ a ].center, g_nav_faces[ b ].center ) );
};
openSet.push( { startFace, Heuristic( startFace, goalFace ) } );

// Explore the graph until the open set is exhausted or the goal is found.
while ( !openSet.empty() ) {
const int32_t current = openSet.top().polyIdx;
openSet.pop();

if ( current == goalFace ) {
// Reconstruct the path by walking the predecessor map backwards.
int32_t curr = current;
while ( curr != startFace ) {
outPath.push_back( curr );
curr = cameFrom[ curr ];
}
outPath.push_back( startFace );
std::reverse( outPath.begin(), outPath.end() );
return true;
}

const nav_face_t &face = g_nav_faces[ current ];
for ( int32_t e = 0; e < face.num_edges; ++e ) {
const nav_halfedge_t &he = g_nav_halfedges[ face.first_edge_idx + e ];
if ( he.twin_idx == -1 ) {
continue;
}

const int32_t neighbor = g_nav_halfedges[ he.twin_idx ].face_idx;
const nav_face_t &neighborFace = g_nav_faces[ neighbor ];

// Vertical clearance is limited so the path prefers walkable transitions.
const float dz = he.z_diff;
const float allowedStep = policy.max_step_height + policy.step_clearance;
if ( dz > allowedStep ) {
continue;
}
if ( dz < -policy.max_drop_height ) {
continue;
}

// Penalize vertical motion so flat routes stay preferred when they are reasonable.
const float dist2D = static_cast<float>( QM_Vector2Distance( face.center, neighborFace.center ) );
const float distZ = std::abs( face.center.z - neighborFace.center.z );
const float tentative_gScore = gScore[ current ] + dist2D + ( distZ * 5.0f );

const auto it = gScore.find( neighbor );
if ( it == gScore.end() || tentative_gScore < it->second ) {
cameFrom[ neighbor ] = current;
gScore[ neighbor ] = tentative_gScore;
const float h = Heuristic( neighbor, goalFace );
openSet.push( { neighbor, tentative_gScore + h } );
}
}
}

// No exact route was found.
outPath.clear();
return false;
}
