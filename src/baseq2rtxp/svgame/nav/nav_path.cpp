#include "svgame/nav/nav_path.h"
#include "svgame/nav/nav_containers.h"
#include "svgame/nav/nav_core.h"
#include "svgame/nav/nav_generate.h" // For g_nav_nodes and g_nav_polys.
#include "svgame/svg_utils.h"
#include "svgame/entities/func/svg_func_door.h"
#include "svgame/entities/func/svg_func_door_rotating.h"
#include "svgame/entities/func/svg_func_wall.h"
#include "svgame/entities/func/svg_func_areaportal.h"
#include "shared/math/qm_vector3.h"
#include "svgame/monsters/svg_mmove.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_map>
#include <vector>

/**
* @brief Compute the true overlapping portal segment between two twinned edges.
* @param he Edge on the current face.
* @param twin Opposite edge on the neighbor face.
* @param outV0 Optional output first overlap endpoint.
* @param outV1 Optional output second overlap endpoint.
* @param outWidth2D Optional output overlap width in XY.
* @return True when a non-degenerate overlap segment was found.
**/
static bool Nav_ComputePortalOverlapSegment( const nav_halfedge_t &he, const nav_halfedge_t &twin, Vector3 *outV0, Vector3 *outV1, float *outWidth2D ) {
	/**
	* Reject topological twins whose owning faces cannot be a physical floor portal.
	**/
	const nav_face_t &face = g_nav_faces[ he.face_idx ];
	const nav_face_t &neighbor_face = g_nav_faces[ twin.face_idx ];
	if ( face.entity_id != neighbor_face.entity_id && face.entity_id != ENTITYNUM_NONE && neighbor_face.entity_id != ENTITYNUM_NONE ) {
		// Keep unrelated dynamic fragments from acting like a real traversable portal.
		return false;
	}
	const float face_normal_length = QM_Vector3Length( face.normal );
	const float neighbor_normal_length = QM_Vector3Length( neighbor_face.normal );
	if ( face.normal.z < NAV_MIN_WALKABLE_Z || neighbor_face.normal.z < NAV_MIN_WALKABLE_Z ||
		face_normal_length <= 0.001f || neighbor_normal_length <= 0.001f ) {
		return false;
	}
	const float normal_alignment = QM_Vector3DotProduct( face.normal, neighbor_face.normal ) /
		( face_normal_length * neighbor_normal_length );
	if ( normal_alignment < 0.0f ) {
		return false;
	}

	const Vector3 a0 = g_nav_vertices[ he.vertex_idx ];
	const Vector3 a1 = g_nav_vertices[ g_nav_halfedges[ he.next_idx ].vertex_idx ];
	const Vector3 b0 = g_nav_vertices[ twin.vertex_idx ];
	const Vector3 b1 = g_nav_vertices[ g_nav_halfedges[ twin.next_idx ].vertex_idx ];

	Vector3 aDir2D = a1 - a0;
	aDir2D.z = 0.0f;
	const float aLen = QM_Vector3Length( aDir2D );
	if ( aLen <= 0.0001f ) {
		return false;
	}
	aDir2D = aDir2D * ( 1.0f / aLen );
	const float lateral0 = std::fabs( aDir2D.x * ( b0.y - a0.y ) - aDir2D.y * ( b0.x - a0.x ) );
	const float lateral1 = std::fabs( aDir2D.x * ( b1.y - a0.y ) - aDir2D.y * ( b1.x - a0.x ) );
	if ( lateral0 > 0.5f || lateral1 > 0.5f ) {
		return false;
	}

	Vector3 bDir2D = b1 - b0;
	bDir2D.z = 0.0f;
	const float bLen = QM_Vector3Length( bDir2D );
	if ( bLen <= 0.0001f ) {
		return false;
	}
	bDir2D = bDir2D * ( 1.0f / bLen );
	const float edge_direction_alignment = QM_Vector3DotProduct( aDir2D, bDir2D );
	if ( std::fabs( edge_direction_alignment ) < 0.95f ) {
		return false;
	}

	auto projectOnA = [&]( const Vector3 &p ) -> float {
		Vector3 ap = p - a0;
		ap.z = 0.0f;
		return static_cast< float >( QM_Vector3DotProduct( ap, aDir2D ) );
		};

	const float u0 = projectOnA( b0 );
	const float u1 = projectOnA( b1 );
	const float bMin = std::min( u0, u1 );
	const float bMax = std::max( u0, u1 );
	const float overlapStart = std::max( 0.0f, bMin );
	const float overlapEnd = std::min( aLen, bMax );
	const float overlapLen = overlapEnd - overlapStart;
	if ( overlapLen < 0.1f ) {
		return false;
	}

	const float t0 = QM_Clamp( overlapStart / aLen, 0.0f, 1.0f );
	const float t1 = QM_Clamp( overlapEnd / aLen, 0.0f, 1.0f );
	Vector3 seg0 = QM_Vector3MultiplyAdd( a0, t0, ( a1 - a0 ) );
	Vector3 seg1 = QM_Vector3MultiplyAdd( a0, t1, ( a1 - a0 ) );

	const Vector3 bMinPoint = ( u0 <= u1 ) ? b0 : b1;
	const Vector3 bMaxPoint = ( u0 <= u1 ) ? b1 : b0;
	const float bSpan = std::max( 0.0001f, bMax - bMin );
	const float bt0 = QM_Clamp( ( overlapStart - bMin ) / bSpan, 0.0f, 1.0f );
	const float bt1 = QM_Clamp( ( overlapEnd - bMin ) / bSpan, 0.0f, 1.0f );
	const Vector3 bSeg0 = QM_Vector3MultiplyAdd( bMinPoint, bt0, ( bMaxPoint - bMinPoint ) );
	const Vector3 bSeg1 = QM_Vector3MultiplyAdd( bMinPoint, bt1, ( bMaxPoint - bMinPoint ) );

	// Portal queries are directional.  Use the destination edge's height so
	// stair transitions steer toward the receiving walk surface instead of
	// flattening the portal to the highest adjacent vertex.
	seg0.z = bSeg0.z;
	seg1.z = bSeg1.z;

	if ( outV0 ) {
		*outV0 = seg0;
	}
	if ( outV1 ) {
		*outV1 = seg1;
	}
	if ( outWidth2D ) {
		*outWidth2D = overlapLen;
	}
	return true;
}

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
* 	@brief	Reapply runtime door/wall nav edge state after the navmesh is regenerated.
* 	@note	The nav generator clears and rebuilds the edge registry, so current mover state must be pushed back in.
**/
void Nav_ResyncDynamicEntityEdges() {
	for ( int32_t i = 1; i < g_edict_pool.num_edicts; ++i ) {
		svg_base_edict_t *ent = g_edict_pool.EdictForNumber( i );
		if ( !ent || !SVG_Entity_IsActive( ent ) || !ent->GetTypeInfo() ) {
			continue;
		}

		bool disableEdges = false;
		bool isDynamicNavEntity = false;

		if ( ent->GetTypeInfo()->IsSubClassType<svg_func_door_t>() || ent->GetTypeInfo()->IsSubClassType<svg_func_door_rotating_t>() ) {
			const svg_func_door_t *door = static_cast<const svg_func_door_t *>( ent );
			isDynamicNavEntity = true;
			disableEdges = ( door->pushMoveInfo.state == svg_func_door_t::DOOR_STATE_CLOSED || door->pushMoveInfo.state == svg_func_door_t::DOOR_STATE_MOVING_TO_CLOSED_STATE );
		} else if ( ent->GetTypeInfo()->IsSubClassType<svg_func_wall_t>() ) {
			const svg_func_wall_t *wall = static_cast<const svg_func_wall_t *>( ent );
			isDynamicNavEntity = true;
			disableEdges = ( wall->solid == SOLID_BSP );
		} else if ( ent->GetTypeInfo()->IsSubClassType<svg_func_areaportal_t>() ) {
			const svg_func_areaportal_t *areaportal = static_cast<const svg_func_areaportal_t *>( ent );
			isDynamicNavEntity = true;
			disableEdges = ( areaportal->count <= 0 );
		}

		if ( !isDynamicNavEntity ) {
			continue;
		}

		Nav_SetEntityEdgesState( ent->s.number, NAV_EDGE_DISABLED, disableEdges );
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

		// Check every edge in the face winding using a winding-independent 2D
		// cross-product test.  Extracted windings can be reversed by CSG clipping;
		// assuming one winding direction makes one stair orientation unqueryable.
	float windingSign = 0.0f;
	for ( int32_t e = 0; e < face.num_edges; e++ ) {
		const nav_halfedge_t &he = g_nav_halfedges[ face.first_edge_idx + e ];
		const Vector3 a = g_nav_vertices[ he.vertex_idx ];
		const Vector3 b = g_nav_vertices[ g_nav_halfedges[ he.next_idx ].vertex_idx ];

		// Compare the point against the edge in XY space.
		const Vector3 edge = b - a;
		const Vector3 toPoint = point - a;
		const float cross2d = edge.x * toPoint.y - edge.y * toPoint.x;

		// Keep a tolerance so small wall-adjacent offsets still count as inside.
		const float edgeLen = std::sqrt( edge.x * edge.x + edge.y * edge.y );
		if ( std::fabs( cross2d ) <= 24.0f * edgeLen ) {
			continue;
		}
		const float edgeSign = ( cross2d > 0.0f ) ? 1.0f : -1.0f;
		if ( windingSign == 0.0f ) {
			windingSign = edgeSign;
		} else if ( edgeSign != windingSign ) {
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
		const float plane_dist = static_cast< float >( QM_Vector3DotProduct( v0, face.normal ) );
		const float d = std::fabs( static_cast< float >( QM_Vector3DotProduct( point, face.normal ) ) - plane_dist );

		if ( d < bestInsideDist && Nav_PointInsideFace2D( point, face ) ) {
			bestInsideDist = d;
			bestInsideFace = static_cast< int32_t >( i );
		}

		// Also track the nearest face center as a fallback.
		const float dx = point.x - face.center.x;
		const float dy = point.y - face.center.y;
		const float dz = point.z - face.center.z;
		const float centerDist = std::sqrt( dx * dx + dy * dy + dz * dz );
		if ( centerDist < bestFallbackDist ) {
			bestFallbackDist = centerDist;
			bestFallbackFace = static_cast< int32_t >( i );
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
	if ( firstFaceIdx == -1 || firstFaceIdx >= static_cast< int32_t >( g_nav_faces.size() ) ) {
		return Nav_FindClosestPolyGlobal( point );
	}

	// Search only the candidate faces in this leaf first.
	int32_t bestInsideFace = -1;
	float bestInsideDist = 64.0f;
	int32_t bestFallbackFace = -1;
	float bestFallbackDist = 999999.0f;

	for ( int32_t i = 0; i < leaf.num_faces; ++i ) {
		const int32_t faceIdx = firstFaceIdx + i;
		if ( faceIdx >= static_cast< int32_t >( g_nav_faces.size() ) ) {
			break;
		}

		const nav_face_t &face = g_nav_faces[ faceIdx ];
		const Vector3 v0 = g_nav_vertices[ g_nav_halfedges[ face.first_edge_idx ].vertex_idx ];
		const float plane_dist = static_cast< float >( QM_Vector3DotProduct( v0, face.normal ) );
		const float d = std::fabs( static_cast< float >( QM_Vector3DotProduct( point, face.normal ) ) - plane_dist );

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
		static_cast< size_t >( faceA ) >= g_nav_faces.size() ||
		static_cast< size_t >( faceB ) >= g_nav_faces.size() ) {
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

		// Use only the true overlap span so runtime steering matches twin-link topology.
		if ( Nav_ComputePortalOverlapSegment( he, twin, outV0, outV1, nullptr ) ) {
			return true;
		}

		// A portal is usable only when the two half-edges have a real overlap.
		// Returning the full source edge here makes path width and steering disagree
		// with the twin topology, especially on fragmented stair edges.
		return false;
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
	if ( startFace < 0 || goalFace < 0 || startFace >= static_cast< int32_t >( g_nav_faces.size() ) || goalFace >= static_cast< int32_t >( g_nav_faces.size() ) ) {
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
		return static_cast< float >( QM_Vector3Distance( g_nav_faces[ a ].center, g_nav_faces[ b ].center ) );
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

			const nav_halfedge_t &twin = g_nav_halfedges[ he.twin_idx ];
			const int32_t neighbor = twin.face_idx;

		/**
		*	Door state is stored symmetrically on both directed portal edges. Reject the
		*	transition before width or vertical checks when either side is disabled.
		*	Ordinary world, slope, and stair portals retain zero flags and are unaffected.
		**/
		if ( !policy.ignore_disabled_edges ) {
			if ( ( he.flags & NAV_EDGE_DISABLED ) != 0 || ( twin.flags & NAV_EDGE_DISABLED ) != 0 ) {
				continue;
			}
		}

			const nav_face_t &neighborFace = g_nav_faces[ neighbor ];

			// Enforce physical traversability (not just topological adjacency) for this edge transition.
			float portalWidth2D = 0.0f;
			if ( !Nav_ComputePortalOverlapSegment( he, twin, nullptr, nullptr, &portalWidth2D ) ) {
				continue;
			}
			const float minRequiredPortalWidth = policy.min_portal_width;
			if ( portalWidth2D < minRequiredPortalWidth ) {
				if ( portalWidth2D < 0.1f ) {
					// Degenerate narrow portal – allow transition.
					// No action needed.
				} else {
					gi.dprintf( "[NAV DEBUG] Edge rejected: width %.2f < %.2f (face %d -> %d)\n", portalWidth2D, minRequiredPortalWidth, current, neighbor );
					continue;
				}
			}

// Vertical clearance is limited so the path prefers walkable transitions.
const float dz = he.z_diff;
const float allowedStep = policy.max_step_height + policy.step_clearance;
if ( dz > allowedStep ) {
	gi.dprintf("[NAV DEBUG] Edge rejected: dz %.2f > allowed %.2f (face %d -> %d)\n", dz, allowedStep, current, neighbor);
	continue;
}
if ( dz < -policy.max_drop_height ) {
	gi.dprintf("[NAV DEBUG] Edge rejected: drop %.2f > max_drop %.2f (face %d -> %d)\n", -dz, policy.max_drop_height, current, neighbor);
	continue;
}

// Penalize vertical motion so flat routes stay preferred when they are reasonable.
const float dist2D = static_cast<float>( QM_Vector2Distance( face.center, neighborFace.center ) );
const float distZ = std::abs( face.center.z - neighborFace.center.z );
	const float tentative_gScore = gScore[ current ] + dist2D + distZ;

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

/**
*	@brief	Build a smoothed string-pulled path using the Funnel algorithm.
*	@param	path		The sequence of face IDs to traverse.
*	@param	startPos	The exact starting position (e.g. agent's current position).
*	@param	goalPos		The exact ending position.
*	@param	agentRadius	The collision radius to steer clear of walls.
*	@param	outWaypoints	Output sequence of 3D points.
*	@return	True if a valid corridor and string-pull could be generated.
**/
inline float Nav_TriArea2D( const Vector3 &a, const Vector3 &b, const Vector3 &c ) {
	const float ax = b.x - a.x;
	const float ay = b.y - a.y;
	const float bx = c.x - a.x;
	const float by = c.y - a.y;
	return bx * ay - ax * by;
}

/**
*	@brief	Calculate 2D cross product of 3 vectors, ignoring Z component
**/
static float QM_Vector2CrossProduct( const Vector3 &a, const Vector3 &b, const Vector3 &c ) {
	return ( b.x - a.x ) * ( c.y - a.y ) - ( c.x - a.x ) * ( b.y - a.y );
}

bool Nav_StringPull( const std::vector<int32_t> &path, const Vector3 &startPos, const Vector3 &goalPos, float agentRadius, std::vector<Vector3> &outWaypoints ) {
	outWaypoints.clear();
	if ( path.empty() ) {
		return false;
	}

	if ( path.size() == 1 ) {
		outWaypoints.push_back( startPos );
		outWaypoints.push_back( goalPos );
		return true;
	}

	struct funnel_portal_t {
		Vector3 left, right;
	};
	std::vector<funnel_portal_t> portals;

	for ( size_t i = 0; i + 1 < path.size(); ++i ) {
		const int32_t face_idx = path[ i ];
		const int32_t next_face_idx = path[ i + 1 ];
		
		Vector3 right, left;
		// Nav_GetPortalEndpoints returns the right endpoint in the first out param, left in the second.
		if ( Nav_GetPortalEndpoints( face_idx, next_face_idx, &right, &left ) ) {
			
			// Determine if this portal belongs to a physical door
			bool isDoor = false;
			int32_t door_entity_id = ENTITYNUM_NONE;
			const nav_face_t &face = g_nav_faces[ face_idx ];
			for ( int32_t e = 0; e < face.num_edges; e++ ) {
				const nav_halfedge_t &he = g_nav_halfedges[ face.first_edge_idx + e ];
				if ( he.twin_idx != -1 && g_nav_halfedges[ he.twin_idx ].face_idx == next_face_idx ) {
					if ( he.edge_entity_id != ENTITYNUM_NONE ) {
						isDoor = true;
						door_entity_id = he.edge_entity_id;
					}
					break;
				}
			}

			// Natively expand door portals to their full physical BSP width
			if ( isDoor && door_entity_id > 0 && door_entity_id < g_edict_pool.num_edicts ) {
				svg_base_edict_t *door = g_edicts[ door_entity_id ];
				if ( door ) {
					// Reconstruct the original, unrotated generation-time bounding box.
					// We use pos1 (closed origin) if available, otherwise fallback to s.origin.
					// Wait, pushmovers use pos1 for the closed position. 
					// s.origin updates dynamically, pos1 is static.
					Vector3 closedMin = QM_Vector3Add( door->pos1, door->mins );
					Vector3 closedMax = QM_Vector3Add( door->pos1, door->maxs );
					
					Vector3 edgeDir = QM_Vector3Subtract( left, right );
					edgeDir.z = 0.0f;
					
					if ( std::abs( edgeDir.x ) > std::abs( edgeDir.y ) ) {
						// Portal runs along the X axis
						if ( edgeDir.x > 0.0f ) {
							left.x = closedMax.x;
							right.x = closedMin.x;
						} else {
							left.x = closedMin.x;
							right.x = closedMax.x;
						}
					} else {
						// Portal runs along the Y axis
						if ( edgeDir.y > 0.0f ) {
							left.y = closedMax.y;
							right.y = closedMin.y;
						} else {
							left.y = closedMin.y;
							right.y = closedMax.y;
						}
					}
				}
			}

			// Shrink the portal by the agent radius so the agent doesn't graze the wall.
			Vector3 edgeDir = QM_Vector3Subtract( left, right );
			edgeDir.z = 0.0f;
			const float edgeLen = QM_Vector3Length( edgeDir );
			if ( edgeLen > 0.001f ) {
				edgeDir = edgeDir * ( 1.0f / edgeLen );
				
				// Provide a 2.0f unit padding to prevent floating point/step grazing
				float shrinkDist = std::min( agentRadius + 2.0f, edgeLen * 0.5f );
				
				right = QM_Vector3Add( right, edgeDir * shrinkDist );
				left = QM_Vector3Subtract( left, edgeDir * shrinkDist );
			}
			portals.push_back( { left, right } );
		} else {
			// Fallback: If no portal connects them, use the face center.
			const Vector3 center = g_nav_faces[ face_idx ].center;
			portals.push_back( { center, center } );
		}
	}

	// Add the goal as the final portal.
	portals.push_back( { goalPos, goalPos } );

	outWaypoints.push_back( startPos );

	Vector3 portalApex = startPos;
	Vector3 portalLeft = startPos;
	Vector3 portalRight = startPos;

	int32_t apexIndex = 0;
	int32_t leftIndex = 0;
	int32_t rightIndex = 0;

	for ( int32_t i = 0; i < ( int32_t )portals.size(); ++i ) {
		const Vector3 &left = portals[ i ].left;
		const Vector3 &right = portals[ i ].right;

		// Update the right bound.
		if ( Nav_TriArea2D( portalApex, portalRight, right ) <= 0.0f ) {
			float distSqRight = ( portalApex.x - portalRight.x ) * ( portalApex.x - portalRight.x ) + ( portalApex.y - portalRight.y ) * ( portalApex.y - portalRight.y );
			if ( distSqRight < 0.001f || Nav_TriArea2D( portalApex, portalLeft, right ) > 0.0f ) {
				// Tighten the funnel.
				portalRight = right;
				rightIndex = i;
			} else {
				// Right crossed left, so the left bound is a corner.
				outWaypoints.push_back( portalLeft );
				portalApex = portalLeft;
				apexIndex = leftIndex;
				// Reset funnel bounds.
				portalLeft = portalApex;
				portalRight = portalApex;
				leftIndex = apexIndex;
				rightIndex = apexIndex;
				// Restart the scan.
				i = apexIndex;
				continue;
			}
		}

		// Update the left bound.
		if ( Nav_TriArea2D( portalApex, portalLeft, left ) >= 0.0f ) {
			float distSqLeft = ( portalApex.x - portalLeft.x ) * ( portalApex.x - portalLeft.x ) + ( portalApex.y - portalLeft.y ) * ( portalApex.y - portalLeft.y );
			if ( distSqLeft < 0.001f || Nav_TriArea2D( portalApex, portalRight, left ) < 0.0f ) {
				// Tighten the funnel.
				portalLeft = left;
				leftIndex = i;
			} else {
				// Left crossed right, so the right bound is a corner.
				outWaypoints.push_back( portalRight );
				portalApex = portalRight;
				apexIndex = rightIndex;
				// Reset funnel bounds.
				portalLeft = portalApex;
				portalRight = portalApex;
				leftIndex = apexIndex;
				rightIndex = apexIndex;
				// Restart the scan.
				i = apexIndex;
				continue;
			}
		}
	}

	outWaypoints.push_back( goalPos );

	return true;
}

/**
*	@brief	Globally enables or disables all nav mesh edges associated with a specific entity ID.
**/
void Nav_SetEntityEdgesState( int32_t entity_id, uint32_t flags, bool enable ) {
	if ( entity_id <= 0 ) {
		return;
	}

	bool applied_any = false;
	if ( g_nav_entity_edges.empty() || entity_id >= static_cast<int32_t>( g_nav_entity_edges.size() ) ) {
		// Fall through to the full scan below.
	} else {
		const std::vector<int32_t> &edges = g_nav_entity_edges[ entity_id ];
		for ( size_t i = 0; i < edges.size(); ++i ) {
			const int32_t edge_idx = edges[ i ];
			// Ignore stale or corrupt registrations instead of indexing outside the current half-edge array.
			if ( edge_idx < 0 || edge_idx >= static_cast<int32_t>( g_nav_halfedges.size() ) ) {
				continue;
			}
			if ( enable ) {
				g_nav_halfedges[ edge_idx ].flags |= flags;
			} else {
				g_nav_halfedges[ edge_idx ].flags &= ~flags;
			}
			applied_any = true;
		}
	}

	// Defensive fallback: if the entity registry missed some edges, update by face/edge ownership too.
	for ( size_t edge_idx = 0; edge_idx < g_nav_halfedges.size(); ++edge_idx ) {
		nav_halfedge_t &halfedge = g_nav_halfedges[ edge_idx ];
		if ( halfedge.edge_entity_id != entity_id ) {
			const nav_face_t &face = g_nav_faces[ halfedge.face_idx ];
			if ( face.entity_id != entity_id && face.transition_entity_id != entity_id ) {
				continue;
			}
		}

		if ( enable ) {
			halfedge.flags |= flags;
		} else {
			halfedge.flags &= ~flags;
		}
		applied_any = true;
	}

	(void)applied_any;
}
