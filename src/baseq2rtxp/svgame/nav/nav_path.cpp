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

//! Bounded counters describing the most recent explicit path query.
static nav_path_diagnostics_t s_nav_last_path_diagnostics = {};

/**
*	@brief	Compute the true overlapping portal segment between two twinned edges.
*	@param	he			[in] Half-edge on the current face.
*	@param	twin		[in] Opposite half-edge on the neighbor face.
*	@param	outV0		[out] Optional output first overlap endpoint.
*	@param	outV1		[out] Optional output second overlap endpoint.
*	@param	outWidth2D	[out] Optional output overlap width in XY.
*	@return	True when a non-degenerate, traversable portal overlap segment was found; false otherwise.
*	@note	Enforces strict 0.5-unit 2D lateral proximity and NAV_MAX_STEP_HEIGHT vertical step limits.
*			Allows angled step edge alignments (dot <= -0.70) for valid stair transitions.
**/
static bool Nav_ComputePortalOverlapSegment( const nav_halfedge_t &he, const nav_halfedge_t &twin, Vector3DP *outV0, Vector3DP *outV1, double *outWidth2D ) {
	/**
	*	Sanity checks: reject topological twins whose owning faces cannot form a physical floor portal.
	**/
	const nav_face_t &face = g_nav_faces[ he.face_idx ];
	const nav_face_t &neighbor_face = g_nav_faces[ twin.face_idx ];
	if ( face.entity_id != neighbor_face.entity_id && face.entity_id != ENTITYNUM_NONE && neighbor_face.entity_id != ENTITYNUM_NONE ) {
		// Keep unrelated dynamic fragments from acting like a real traversable portal.
		return false;
	}
	const double face_normal_length = QM_Vector3LengthDP( face.normal );
	const double neighbor_normal_length = QM_Vector3LengthDP( neighbor_face.normal );
	if ( face.normal.z < NAV_MIN_WALKABLE_Z || neighbor_face.normal.z < NAV_MIN_WALKABLE_Z ||
		face_normal_length <= 0.001 || neighbor_normal_length <= 0.001 ) {
		return false;
	}
	const double normal_alignment = QM_Vector3DotProductDP( face.normal, neighbor_face.normal ) /
		( face_normal_length * neighbor_normal_length );
	if ( normal_alignment < 0.0 ) {
		return false;
	}

	/**
	*	Fetch edge endpoints and calculate 2D direction vectors and lateral separation.
	**/
	const Vector3DP a0 = g_nav_vertices[ he.vertex_idx ];
	const Vector3DP a1 = g_nav_vertices[ g_nav_halfedges[ he.next_idx ].vertex_idx ];
	const Vector3DP b0 = g_nav_vertices[ twin.vertex_idx ];
	const Vector3DP b1 = g_nav_vertices[ g_nav_halfedges[ twin.next_idx ].vertex_idx ];

	Vector3DP aDir2D = a1 - a0;
	aDir2D.z = 0.0;
	const double aLen = QM_Vector3LengthDP( aDir2D );
	if ( aLen <= 0.0001 ) {
		return false;
	}
	aDir2D = aDir2D * ( 1.0 / aLen );
	const double lateral0 = std::fabs( aDir2D.x * ( b0.y - a0.y ) - aDir2D.y * ( b0.x - a0.x ) );
	const double lateral1 = std::fabs( aDir2D.x * ( b1.y - a0.y ) - aDir2D.y * ( b1.x - a0.x ) );
	// Allow up to 4.0 units lateral deviation for twin edges from high-poly BSP splitting
	// to prevent flat ground seams with minor vertex drift from being falsely rejected as impassable portals.
	static constexpr double NAV_MAX_PORTAL_LATERAL_DEVIATION = 4.0;
	if ( lateral0 > NAV_MAX_PORTAL_LATERAL_DEVIATION || lateral1 > NAV_MAX_PORTAL_LATERAL_DEVIATION ) {
		return false;
	}

	/**
	*	Evaluate edge direction alignment and anti-parallel portal orientation.
	**/
	Vector3DP bDir2D = b1 - b0;
	bDir2D.z = 0.0;
	const double bLen = QM_Vector3LengthDP( bDir2D );
	if ( bLen <= 0.0001 ) {
		return false;
	}
	bDir2D = bDir2D * ( 1.0 / bLen );
	const double edge_direction_alignment = QM_Vector3DotProductDP( aDir2D, bDir2D );
	// Require anti-parallel edge alignment across shared seam (dot <= -0.70 for angled stair steps, dot < 0.0 for portal validity);
	// same-direction twin edges (dot > 0) produce reversed portal endpoints that collapse the funnel.
	const double min_alignment_threshold = ( std::fabs( he.z_diff ) <= NAV_MAX_STEP_HEIGHT ) ? -0.70 : -0.90;
	if ( edge_direction_alignment > min_alignment_threshold ) {
		return false;
	}

	/**
	*	Project twin endpoints onto edge A to measure 2D overlap range.
	**/
	auto projectOnA = [&]( const Vector3DP &p ) -> double {
		Vector3DP ap = p - a0;
		ap.z = 0.0;
		return QM_Vector3DotProductDP( ap, aDir2D );
		};

	const double u0 = projectOnA( b0 );
	const double u1 = projectOnA( b1 );
	const double bMin = std::min( u0, u1 );
	const double bMax = std::max( u0, u1 );
	const double overlapStart = std::max( 0.0, bMin );
	const double overlapEnd = std::min( aLen, bMax );
	const double overlapLen = overlapEnd - overlapStart;
	if ( overlapLen < 0.1 ) {
		return false;
	}

	/**
	*	Construct output 3D portal endpoints and width.
	**/
	const double t0 = QM_Clamp( overlapStart / aLen, 0.0, 1.0 );
	const double t1 = QM_Clamp( overlapEnd / aLen, 0.0, 1.0 );
	Vector3DP seg0_dp = a0 + ( a1 - a0 ) * t0;
	Vector3DP seg1_dp = a0 + ( a1 - a0 ) * t1;

	const Vector3DP bMinPoint = ( u0 <= u1 ) ? b0 : b1;
	const Vector3DP bMaxPoint = ( u0 <= u1 ) ? b1 : b0;
	const double bSpan = std::max( 0.0001, bMax - bMin );
	const double bt0 = QM_Clamp( ( overlapStart - bMin ) / bSpan, 0.0, 1.0 );
	const double bt1 = QM_Clamp( ( overlapEnd - bMin ) / bSpan, 0.0, 1.0 );
	Vector3DP bSeg0_dp = bMinPoint + ( bMaxPoint - bMinPoint ) * bt0;
	Vector3DP bSeg1_dp = bMinPoint + ( bMaxPoint - bMinPoint ) * bt1;

	// Portal queries are directional.  Use the destination edge's height so
	// stair transitions steer toward the receiving walk surface instead of
	// flattening the portal to the highest adjacent vertex.
	seg0_dp.z = bSeg0_dp.z;
	seg1_dp.z = bSeg1_dp.z;

	if ( outV0 ) {
		*outV0 = seg0_dp;
	}
	if ( outV1 ) {
		*outV1 = seg1_dp;
	}
	if ( outWidth2D ) {
		*outWidth2D = overlapLen;
	}
	return true;
}

/**
*	@brief	Legacy no-op placeholder for adjacency graph construction.
*	@note	The half-edge mesh already stores adjacency during generation.
**/
void Nav_BuildAdjacencyGraph() {
// The graph is built explicitly via Nav_BuildHalfEdgeMesh, so no work is needed here.
}

/**
*	@brief	Walk the KD-tree to locate the leaf node that contains a point using authoritative split_pos.
*	@param	point	[in] World-space position to query.
*	@return	Index of the leaf node in g_nav_nodes, or -1 when the tree is empty.
*	@note	Uses node.split_pos for exact, deterministic child branch selection.
**/
int32_t Nav_FindLeafNode( const Vector3DP &point ) {
	/**
	*	Sanity check: reject empty trees immediately.
	**/
	if ( g_nav_nodes.empty() ) {
		return -1;
	}

	/**
	*	Start at root node (index 0) and traverse internal nodes down to a leaf.
	**/
	int32_t nodeIdx = 0;
	while ( true ) {
		const nav_kdtree_node_t &node = g_nav_nodes[ nodeIdx ];

		// Leaf nodes have no children, so the current index is the answer.
		if ( node.left_child == -1 && node.right_child == -1 ) {
			return nodeIdx;
		}

		// Choose the child that contains the query point on the split axis using exact split_pos.
		bool leftSide = false;
		if ( node.left_child != -1 && node.right_child != -1 ) {
			const int32_t axis = node.split_axis;
			leftSide = ( point[ axis ] <= node.split_pos );
		} else if ( node.left_child != -1 ) {
			leftSide = true;
		} else {
			leftSide = false;
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
* @brief Check if a point lies inside the 2D projection of a face using double precision.
* @param point World-space position.
* @param face Face to test against.
* @return True if the point is inside the face outline.
**/
bool Nav_PointInsideFace2D( const Vector3DP &point, const nav_face_t &face ) {
	// Faces need at least three edges before they can contain a point.
	if ( face.num_edges < 3 ) {
		return false;
	}

	double windingSign = 0.0;
	for ( int32_t e = 0; e < face.num_edges; e++ ) {
		const nav_halfedge_t &he = g_nav_halfedges[ face.first_edge_idx + e ];
		const Vector3DP a = g_nav_vertices[ he.vertex_idx ];
		const Vector3DP b = g_nav_vertices[ g_nav_halfedges[ he.next_idx ].vertex_idx ];

		// Compare the point against the edge in XY space.
		const Vector3DP edge = b - a;
		const Vector3DP toPoint = point - a;
		const double cross2d = edge.x * toPoint.y - edge.y * toPoint.x;

		// Treat only genuine edge-contact rounding as inside; a 24-unit band overlaps adjacent stair faces.
		const double edgeLen = std::sqrt( edge.x * edge.x + edge.y * edge.y );
		static constexpr double POINT_ON_EDGE_TOLERANCE = 0.5;
		if ( std::fabs( cross2d ) <= POINT_ON_EDGE_TOLERANCE * edgeLen ) {
			continue;
		}
		const double edgeSign = ( cross2d > 0.0 ) ? 1.0 : -1.0;
		if ( windingSign == 0.0 ) {
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
int32_t Nav_FindClosestPolyGlobal( const Vector3DP &point ) {
	// Track the best face that actually contains the point in 2D.
	int32_t bestInsideFace = -1;
	double bestInsideDist = 64.0;

	// Track the nearest face as a fallback when no face contains the point.
	int32_t bestFallbackFace = -1;
	double bestFallbackDist = 999999.0;

	// Scan every face because this is the final catch-all lookup.
	for ( size_t i = 0; i < g_nav_faces.size(); ++i ) {
		const nav_face_t &face = g_nav_faces[ i ];

		// Measure vertical distance to the face plane for the inside test.
		const Vector3DP v0 = g_nav_vertices[ g_nav_halfedges[ face.first_edge_idx ].vertex_idx ];
		const double plane_dist = QM_Vector3DotProductDP( v0, face.normal );
		const double d = std::fabs( QM_Vector3DotProductDP( point, face.normal ) - plane_dist );

		if ( d < bestInsideDist && Nav_PointInsideFace2D( point, face ) ) {
			bestInsideDist = d;
			bestInsideFace = static_cast< int32_t >( i );
		}

		// Also track the nearest face center as a fallback.
		const double dx = point.x - face.center.x;
		const double dy = point.y - face.center.y;
		const double dz = point.z - face.center.z;
		const double centerDist = std::sqrt( dx * dx + dy * dy + dz * dz );
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
int32_t Nav_FindPolyInLeaf( const Vector3DP &point ) {
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

	//! Monotonically incrementing global query identifier for Ray ID Mailboxing deduplication.
	static uint32_t s_global_query_id = 1;
	const uint32_t current_query_id = ++s_global_query_id;
	if ( s_global_query_id == 0 ) {
		s_global_query_id = 1;
	}

	// Search candidate faces in this leaf first, skipping already-tested faces via query_id mailboxing.
	int32_t bestInsideFace = -1;
	double bestInsideDist = 64.0;
	int32_t bestFallbackFace = -1;
	double bestFallbackDist = 999999.0;

	for ( int32_t i = 0; i < leaf.num_faces; ++i ) {
		const int32_t faceIdx = firstFaceIdx + i;
		if ( faceIdx >= static_cast< int32_t >( g_nav_faces.size() ) ) {
			break;
		}

		const nav_face_t &face = g_nav_faces[ faceIdx ];

		// Ray ID Mailboxing: skip redundant narrow-phase test if this face was already tested during the current query.
		if ( face.last_query_id == current_query_id ) {
			continue;
		}
		face.last_query_id = current_query_id;

		const Vector3DP v0 = g_nav_vertices[ g_nav_halfedges[ face.first_edge_idx ].vertex_idx ];
		const double plane_dist = QM_Vector3DotProductDP( v0, face.normal );
		const double d = std::fabs( QM_Vector3DotProductDP( point, face.normal ) - plane_dist );

		if ( d < bestInsideDist && Nav_PointInsideFace2D( point, face ) ) {
			bestInsideDist = d;
			bestInsideFace = faceIdx;
		}

		const double dx = point.x - face.center.x;
		const double dy = point.y - face.center.y;
		const double dz = point.z - face.center.z;
		const double centerDist = std::sqrt( dx * dx + dy * dy + dz * dz );
		if ( centerDist < bestFallbackDist ) {
			bestFallbackDist = centerDist;
			bestFallbackFace = faceIdx;
		}
	}

	if ( bestInsideFace != -1 ) {
		return bestInsideFace;
	}
	// Perform global search if the candidate faces in the KD-tree leaf hint did not contain the point.
	return Nav_FindClosestPolyGlobal( point );
}

/**
* @brief Calculate the portal segment endpoints between two adjacent nav polygons.
* @param faceA Index of the first polygon.
* @param faceB Index of the second polygon.
* @param outV0 Output receiving the first endpoint.
* @param outV1 Output receiving the second endpoint.
* @return True if a shared edge was identified.
**/
bool Nav_GetPortalEndpoints( int32_t faceA, int32_t faceB, Vector3DP *outV0, Vector3DP *outV1 ) {
	if ( outV0 == nullptr || outV1 == nullptr || faceA < 0 || faceB < 0 ||
		static_cast< size_t >( faceA ) >= g_nav_faces.size() ||
		static_cast< size_t >( faceB ) >= g_nav_faces.size() ) {
		return false;
	}

	const nav_face_t &fA = g_nav_faces[ faceA ];
	bool found_any = false;
	Vector3DP base_v0, base_v1;
	Vector3DP merged_v0, merged_v1;
	Vector3DP line_dir;
	double min_t = 0.0;
	double max_t = 0.0;

	for ( int32_t e = 0; e < fA.num_edges; ++e ) {
		const nav_halfedge_t &he = g_nav_halfedges[ fA.first_edge_idx + e ];
		if ( he.twin_idx == -1 ) {
			continue;
		}

		const nav_halfedge_t &twin = g_nav_halfedges[ he.twin_idx ];
		if ( twin.face_idx != faceB ) {
			continue;
		}

		Vector3DP temp_v0, temp_v1;
		if ( Nav_ComputePortalOverlapSegment( he, twin, &temp_v0, &temp_v1, nullptr ) ) {
			if ( !found_any ) {
				found_any = true;
				base_v0 = temp_v0;
				base_v1 = temp_v1;
				merged_v0 = temp_v0;
				merged_v1 = temp_v1;

				line_dir = base_v1 - base_v0;
				double lenSqr = line_dir.x * line_dir.x + line_dir.y * line_dir.y + line_dir.z * line_dir.z;
				if ( lenSqr > 0.0001 ) {
					line_dir = line_dir * ( 1.0 / std::sqrt( lenSqr ) );
					max_t = std::sqrt( lenSqr );
				} else {
					line_dir = Vector3DP( 1.0, 0.0, 0.0 );
					max_t = 0.0;
				}
			} else {
				double t0 = QM_Vector3DotProductDP( temp_v0 - base_v0, line_dir );
				double t1 = QM_Vector3DotProductDP( temp_v1 - base_v0, line_dir );

				if ( t0 < min_t ) min_t = t0;
				if ( t0 > max_t ) max_t = t0;
				if ( t1 < min_t ) min_t = t1;
				if ( t1 > max_t ) max_t = t1;

				merged_v0 = base_v0 + line_dir * min_t;
				merged_v1 = base_v0 + line_dir * max_t;
			}
		}
	}

	if ( found_any ) {
		Vector3DP travel = g_nav_faces[ faceB ].center - fA.center;
		travel.z = 0.0;
		double travel_len = std::sqrt( travel.x * travel.x + travel.y * travel.y );
		if ( travel_len < 0.001 ) {
			Vector3DP mid = ( merged_v0 + merged_v1 ) * 0.5;
			travel = mid - fA.center;
			travel.z = 0.0;
			travel_len = std::sqrt( travel.x * travel.x + travel.y * travel.y );
		}

		Vector3DP edge_vec = merged_v1 - merged_v0;
		edge_vec.z = 0.0;

		double cross2d = travel.x * edge_vec.y - travel.y * edge_vec.x;

		if ( cross2d >= 0.0 ) {
			*outV0 = merged_v1;
			*outV1 = merged_v0;
		} else {
			*outV0 = merged_v0;
			*outV1 = merged_v1;
		}
		return true;
	}

	return false;
}

/**
* @brief A* node entry used by Nav_FindPath.
**/
struct AStarNode {
	int32_t polyIdx = -1;
	double fScore = 0.0;

	bool operator>( const AStarNode &other ) const {
		return fScore > other.fScore;
	}
};

/**
* @brief Test whether a portal retains a usable agent-center corridor using double precision.
**/
static bool Nav_ClipPortalForAgentClearance( const int32_t faceAIdx, const int32_t faceBIdx, const Vector3DP &portalLeft, const Vector3DP &portalRight, const double clearance, Vector3DP *outLeft, Vector3DP *outRight );

/**
 * @brief Compute a path using A* from the start face to the goal face.
* @param startFace Index of the starting face.
* @param goalFace Index of the goal face.
* @param outPath Output vector that receives the face sequence.
* @param policy Traversal policy used to bound vertical movement.
* @return True when a valid path was found.
**/
bool Nav_FindPath( int32_t startFace, int32_t goalFace, std::vector<int32_t> &outPath, const nav_path_policy_t &policy ) {
	s_nav_last_path_diagnostics = {};
	s_nav_last_path_diagnostics.start_face = startFace;
	s_nav_last_path_diagnostics.goal_face = goalFace;

	outPath.clear();

	if ( startFace < 0 || goalFace < 0 || startFace >= static_cast< int32_t >( g_nav_faces.size() ) || goalFace >= static_cast< int32_t >( g_nav_faces.size() ) ) {
		return false;
	}
	s_nav_last_path_diagnostics.query_valid = true;

	if ( startFace == goalFace ) {
		outPath.push_back( startFace );
		s_nav_last_path_diagnostics.route_found = true;
		return true;
	}

	std::priority_queue<AStarNode, std::vector<AStarNode>, std::greater<AStarNode>> openSet;
	std::unordered_map<int32_t, int32_t> cameFrom;
	std::unordered_map<int32_t, double> gScore;
	std::unordered_map<int32_t, bool> expandedFaces;

	gScore[ startFace ] = 0.0;
	auto Heuristic = []( int32_t a, int32_t b ) {
		const double dist = QM_Vector3DistanceDP( g_nav_faces[ a ].center, g_nav_faces[ b ].center );
		const double slopeA = QM_Clamp( g_nav_faces[ a ].normal.z, 0.0, 1.0 );
		return dist * ( 1.0 + ( 1.0 - slopeA ) * 4.0 );
		};
	openSet.push( { startFace, Heuristic( startFace, goalFace ) } );

	while ( !openSet.empty() ) {
		const int32_t current = openSet.top().polyIdx;
		openSet.pop();
		if ( expandedFaces.find( current ) == expandedFaces.end() ) {
			expandedFaces[ current ] = true;
			s_nav_last_path_diagnostics.expanded_faces++;
		}

		if ( current == goalFace ) {
			int32_t curr = goalFace;
			while ( curr != startFace ) {
				outPath.push_back( curr );
				curr = cameFrom[ curr ];
			}
			outPath.push_back( startFace );
			std::reverse( outPath.begin(), outPath.end() );
			s_nav_last_path_diagnostics.route_found = true;
			return true;
		}

		const nav_face_t &faceCurrent = g_nav_faces[ current ];
		for ( int32_t e = 0; e < faceCurrent.num_edges; e++ ) {
			const nav_halfedge_t &he = g_nav_halfedges[ faceCurrent.first_edge_idx + e ];
			if ( he.twin_idx == -1 ) continue;
			const nav_halfedge_t &twin = g_nav_halfedges[ he.twin_idx ];
			const int32_t neighborIdx = twin.face_idx;

			if ( ( he.flags & NAV_EDGE_DISABLED ) != 0 || ( twin.flags & NAV_EDGE_DISABLED ) != 0 ) {
				if ( !policy.ignore_disabled_edges ) {
					s_nav_last_path_diagnostics.rejected_disabled++;
					continue;
				}
			}

			const nav_face_t &faceNeighbor = g_nav_faces[ neighborIdx ];
			const double zDelta = he.z_diff;
			if ( zDelta > NAV_MAX_STEP_HEIGHT ) {
				s_nav_last_path_diagnostics.rejected_step_height++;
				continue;
			}
			if ( -zDelta > policy.max_drop_height ) {
				s_nav_last_path_diagnostics.rejected_drop_height++;
				continue;
			}

			Vector3DP portalLeft{}, portalRight{};
			double portalWidth2D = 0.0;
			if ( !Nav_ComputePortalOverlapSegment( he, twin, &portalLeft, &portalRight, &portalWidth2D ) ) {
				s_nav_last_path_diagnostics.rejected_no_portal++;
				continue;
			}

			if ( portalWidth2D < 0.1 ) {
				s_nav_last_path_diagnostics.rejected_narrow_portal++;
				continue;
			}

			// Penalize transitions across non-horizontal sloped faces (pyramids, roof slopes)
			// so A* strongly prefers flat ground paths over climbing/traversing sloped surfaces.
			const double neighborSlope = QM_Clamp( faceNeighbor.normal.z, 0.0, 1.0 );
			const double slopePenalty = 1.0 + ( 1.0 - neighborSlope ) * 4.0;
			const double edgeDistance = QM_Vector3DistanceDP( faceCurrent.center, faceNeighbor.center );
			const double tentativeGScore = gScore[ current ] + edgeDistance * slopePenalty;

			if ( gScore.find( neighborIdx ) == gScore.end() || tentativeGScore < gScore[ neighborIdx ] ) {
				cameFrom[ neighborIdx ] = current;
				gScore[ neighborIdx ] = tentativeGScore;
				openSet.push( { neighborIdx, tentativeGScore + Heuristic( neighborIdx, goalFace ) } );
				s_nav_last_path_diagnostics.accepted_transitions++;
			}
		}
	}

	return false;
}

/**
*	@brief	Print the most recent A* rejection counters for a failed debug query.
*	@note	This is deliberately separate from Nav_FindPath so normal gameplay
*			queries remain silent apart from their existing diagnostics.
**/
void Nav_LogLastPathDiagnostics( void ) {
	const nav_path_diagnostics_t &diagnostics = s_nav_last_path_diagnostics;
	gi.dprintf( "nav_dbg_path_diag: start=%" PRId32 " goal=%" PRId32 " valid=%d found=%d expanded=%" PRId32 " accepted=%" PRId32 " disabled=%" PRId32 " no_portal=%" PRId32 " narrow=%" PRId32 " no_agent_corridor=%" PRId32 " step=%" PRId32 " drop=%" PRId32 "\n",
		diagnostics.start_face,
		diagnostics.goal_face,
		diagnostics.query_valid ? 1 : 0,
		diagnostics.route_found ? 1 : 0,
		diagnostics.expanded_faces,
		diagnostics.accepted_transitions,
		diagnostics.rejected_disabled,
		diagnostics.rejected_no_portal,
		diagnostics.rejected_narrow_portal,
		diagnostics.rejected_no_agent_corridor,
		diagnostics.rejected_step_height,
		diagnostics.rejected_drop_height );
}


/**
*	@brief	Calculate 2D cross product of 3 Vector3DP vectors, ignoring Z component.
**/
static double QM_Vector2CrossProductDP( const Vector3DP &a, const Vector3DP &b, const Vector3DP &c ) {
	return ( b.x - a.x ) * ( c.y - a.y ) - ( c.x - a.x ) * ( b.y - a.y );
}

/**
*	@brief	Clip a 2D line interval against the convex boundary of a nav face using double precision.
**/
static bool Nav_ClipFaceLineInterval2D( const nav_face_t &face, const Vector3DP &lineOrigin, const Vector3DP &lineDirection, const double lineLength, const int32_t excludedFaceIdx, const double minimumClearance, double *outMinT, double *outMaxT ) {
	if ( outMinT == nullptr || outMaxT == nullptr || lineLength <= 0.0 ) {
		return false;
	}

	const Vector3DP faceCenter = face.center;
	double windingSign = 0.0;
	for ( int32_t edgeIndex = 0; edgeIndex < face.num_edges; edgeIndex++ ) {
		const nav_halfedge_t &halfEdge = g_nav_halfedges[ face.first_edge_idx + edgeIndex ];
		const Vector3DP a = g_nav_vertices[ halfEdge.vertex_idx ];
		const Vector3DP b = g_nav_vertices[ g_nav_halfedges[ halfEdge.next_idx ].vertex_idx ];
		const double cross = ( b.x - a.x ) * ( faceCenter.y - a.y ) - ( b.y - a.y ) * ( faceCenter.x - a.x );
		if ( std::fabs( cross ) > 0.001 ) {
			windingSign = ( cross > 0.0 ) ? 1.0 : -1.0;
			break;
		}
	}
	if ( windingSign == 0.0 ) {
		return false;
	}

	double minT = 0.0;
	double maxT = lineLength;
	for ( int32_t edgeIndex = 0; edgeIndex < face.num_edges; edgeIndex++ ) {
		const nav_halfedge_t &halfEdge = g_nav_halfedges[ face.first_edge_idx + edgeIndex ];
		if ( halfEdge.twin_idx != -1 ) {
			const nav_halfedge_t &twin = g_nav_halfedges[ halfEdge.twin_idx ];
			const bool isSharedPortal = ( excludedFaceIdx >= 0 && twin.face_idx == excludedFaceIdx );
			const bool isDisabledPortal = ( ( halfEdge.flags & NAV_EDGE_DISABLED ) != 0 || ( twin.flags & NAV_EDGE_DISABLED ) != 0 );
			if ( isSharedPortal || !isDisabledPortal ) {
				continue;
			}
		}
		const Vector3DP a = g_nav_vertices[ halfEdge.vertex_idx ];
		const Vector3DP b = g_nav_vertices[ g_nav_halfedges[ halfEdge.next_idx ].vertex_idx ];
		const double edgeX = b.x - a.x;
		const double edgeY = b.y - a.y;
		const double edgeLength = std::sqrt( edgeX * edgeX + edgeY * edgeY );
		if ( edgeLength <= 0.001 ) {
			continue;
		}

		const double originCross = edgeX * ( lineOrigin.y - a.y ) - edgeY * ( lineOrigin.x - a.x );
		const double directionCross = edgeX * lineDirection.y - edgeY * lineDirection.x;
		const double interiorValue = windingSign * originCross;
		const double interiorSlope = windingSign * directionCross;
		const double minimumCross = minimumClearance * edgeLength;
		if ( std::fabs( interiorSlope ) <= 0.000001 ) {
			if ( interiorValue < minimumCross ) {
				return false;
			}
			continue;
		}

		const double boundaryT = ( minimumCross - interiorValue ) / interiorSlope;
		if ( interiorSlope > 0.0 ) {
			minT = std::max( minT, boundaryT );
		} else {
			maxT = std::min( maxT, boundaryT );
		}
		if ( minT > maxT ) {
			if ( maxT >= minT - 1.0 ) {
				maxT = minT;
			} else {
				return false;
			}
		}
	}

	*outMinT = minT;
	*outMaxT = maxT;
	return true;
}

/**
 *	@brief	Clip a shared portal to the agent-clearance corridor of both adjacent faces in double precision.
 **/
static bool Nav_ClipPortalForAgentClearance( const int32_t faceAIdx, const int32_t faceBIdx, const Vector3DP &portalLeft, const Vector3DP &portalRight, const double clearance, Vector3DP *outLeft, Vector3DP *outRight ) {
	if ( outLeft == nullptr || outRight == nullptr || clearance < 0.0 || faceAIdx < 0 || faceBIdx < 0 ||
		static_cast<size_t>( faceAIdx ) >= g_nav_faces.size() || static_cast<size_t>( faceBIdx ) >= g_nav_faces.size() ) {
		return false;
	}

	const nav_face_t &faceA = g_nav_faces[ faceAIdx ];
	const nav_face_t &faceB = g_nav_faces[ faceBIdx ];

	Vector3DP portalDirection = portalLeft - portalRight;
	portalDirection.z = 0.0;
	const double portalLength = QM_Vector3LengthDP( portalDirection );
	if ( portalLength <= 0.001 ) {
		return false;
	}
	portalDirection = portalDirection * ( 1.0 / portalLength );

	double minT = 0.0;
	double maxT = portalLength;
	double faceMinT = 0.0;
	double faceMaxT = 0.0;
	if ( !Nav_ClipFaceLineInterval2D( faceA, portalRight, portalDirection, portalLength, faceBIdx, clearance, &faceMinT, &faceMaxT ) ) {
		return false;
	}
	minT = std::max( minT, faceMinT );
	maxT = std::min( maxT, faceMaxT );

	if ( !Nav_ClipFaceLineInterval2D( faceB, portalRight, portalDirection, portalLength, faceAIdx, clearance, &faceMinT, &faceMaxT ) ) {
		return false;
	}
	minT = std::max( minT, faceMinT );
	maxT = std::min( maxT, faceMaxT );

	static constexpr double PORTAL_INTERVAL_EPSILON = 0.001;
	if ( maxT < minT - PORTAL_INTERVAL_EPSILON ) {
		return false;
	}
	if ( maxT < minT ) {
		maxT = minT;
	}

	*outRight = portalRight + portalDirection * minT;
	*outLeft = portalRight + portalDirection * maxT;
	return true;
}

/**
 *	@brief	Find a front-facing approach point in the widest valid section of an upward stair tread in double precision.
 **/
static bool Nav_ComputeStepApproachPoint( const nav_face_t &face, const Vector3DP &portalLeft, const Vector3DP &portalRight, const double agentRadius, Vector3DP *outPoint ) {
	if ( outPoint == nullptr ) {
		return false;
	}

	Vector3DP portalTangent = portalLeft - portalRight;
	portalTangent.z = 0.0;
	const double portalLength = QM_Vector3LengthDP( portalTangent );
	if ( portalLength <= 0.001 ) {
		return false;
	}
	portalTangent = portalTangent * ( 1.0 / portalLength );
	Vector3DP inwardNormal = { -portalTangent.y, portalTangent.x, 0.0 };
	const Vector3DP portalMidpoint = ( portalLeft + portalRight ) * 0.5;
	Vector3DP toFaceCenter = face.center - portalMidpoint;
	toFaceCenter.z = 0.0;
	if ( QM_Vector3DotProductDP( inwardNormal, toFaceCenter ) < 0.0 ) {
		inwardNormal = inwardNormal * -1.0;
	}

	const double preferredOffset = std::max( 4.0, agentRadius + 2.0 );
	const Vector3DP portalBase = portalRight;
	auto ClipAtOffset = [&]( const double offset, double *outMinT, double *outMaxT ) -> bool {
		const Vector3DP lineOrigin = portalBase + inwardNormal * offset;
		return Nav_ClipFaceLineInterval2D( face, lineOrigin, portalTangent, portalLength, -1, 0.5, outMinT, outMaxT );
	};

	double validOffset = preferredOffset;
	double minT = 0.0;
	double maxT = 0.0;
	if ( !ClipAtOffset( validOffset, &minT, &maxT ) ) {
		const double minimumApproachOffset = 0.5;
		double lowerOffset = minimumApproachOffset;
		double upperOffset = preferredOffset;
		if ( !ClipAtOffset( lowerOffset, &minT, &maxT ) ) {
			return false;
		}
		for ( int32_t iteration = 0; iteration < 10; iteration++ ) {
			const double testOffset = ( lowerOffset + upperOffset ) * 0.5;
			double testMinT = 0.0;
			double testMaxT = 0.0;
			if ( ClipAtOffset( testOffset, &testMinT, &testMaxT ) ) {
				lowerOffset = testOffset;
				minT = testMinT;
				maxT = testMaxT;
			} else {
				upperOffset = testOffset;
			}
		}
		validOffset = lowerOffset;
	}

	const double approachT = ( minT + maxT ) * 0.5;
	*outPoint = portalBase + inwardNormal * validOffset + portalTangent * approachT;

	const nav_halfedge_t &firstEdge = g_nav_halfedges[ face.first_edge_idx ];
	const Vector3DP planePoint = g_nav_vertices[ firstEdge.vertex_idx ];
	const Vector3DP planeNormal = face.normal;
	if ( std::fabs( planeNormal.z ) <= 0.001 ) {
		return false;
	}
	const double planeOffsetX = outPoint->x - planePoint.x;
	const double planeOffsetY = outPoint->y - planePoint.y;
	const double planeHeight = planePoint.z - ( planeNormal.x * planeOffsetX + planeNormal.y * planeOffsetY ) / planeNormal.z;
	outPoint->z = planeHeight;
	return true;
}

/**
*	@brief	Calculate 2D cross product of 3 Vector3DP points for string pulling.
**/
static double Nav_TriArea2D( const Vector3DP &a, const Vector3DP &b, const Vector3DP &c ) {
	return ( b.x - a.x ) * ( c.y - a.y ) - ( c.x - a.x ) * ( b.y - a.y );
}

/**
*	@brief	Build a smoothed string-pulled path using the Funnel algorithm in double precision.
*	@param	path	The sequence of face IDs to traverse.
*	@param	startPos	The exact starting position.
*	@param	goalPos	The exact ending position.
*	@param	agentRadius	The collision radius to steer clear of walls.
*	@param	outWaypoints	Output sequence of 3D points.
*	@param	outForcedWaypoints Optional output flags parallel to outWaypoints.
*	@return	True if a valid corridor and string-pull could be generated.
*	@note	Upward stair transitions may add a mandatory approach waypoint before the portal.
**/
bool Nav_StringPull( const std::vector<int32_t> &path, const Vector3 &startPos, const Vector3 &goalPos, float agentRadius, std::vector<Vector3> &outWaypoints, std::vector<bool> *outForcedWaypoints ) {
	outWaypoints.clear();
	if ( outForcedWaypoints != nullptr ) {
		outForcedWaypoints->clear();
	}

	const Vector3DP startPosDP = Vector3DP( startPos );
	const Vector3DP goalPosDP = Vector3DP( goalPos );

	if ( path.empty() ) {
		return false;
	}

	if ( path.size() == 1 ) {
		outWaypoints.push_back( startPos );
		outWaypoints.push_back( goalPos );
		if ( outForcedWaypoints != nullptr ) {
			outForcedWaypoints->push_back( false );
			outForcedWaypoints->push_back( false );
		}
		return true;
	}

	struct funnel_portal_t {
		Vector3DP left, right;
		bool force_waypoint = false;
	};
	std::vector<funnel_portal_t> portals;

	for ( size_t i = 0; i + 1 < path.size(); ++i ) {
		const int32_t face_idx = path[ i ];
		const int32_t next_face_idx = path[ i + 1 ];
		
		Vector3DP left{}, right{};
		// Nav_GetPortalEndpoints returns the directed portal's LEFT endpoint in outV0 and RIGHT in outV1.
		if ( Nav_GetPortalEndpoints( face_idx, next_face_idx, &left, &right ) ) {
			
			// Determine if this portal belongs to a physical door and get its step rise
			bool isDoor = false;
			int32_t door_entity_id = ENTITYNUM_NONE;
			double stepRise = 0.0;
			const nav_face_t &face = g_nav_faces[ face_idx ];
			for ( int32_t e = 0; e < face.num_edges; e++ ) {
				const nav_halfedge_t &he = g_nav_halfedges[ face.first_edge_idx + e ];
				if ( he.twin_idx != -1 && g_nav_halfedges[ he.twin_idx ].face_idx == next_face_idx ) {
					stepRise = he.z_diff;
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
					Vector3DP closedMin = Vector3DP( QM_Vector3Add( door->pos1, door->mins ) );
					Vector3DP closedMax = Vector3DP( QM_Vector3Add( door->pos1, door->maxs ) );
					
					Vector3DP edgeDir = left - right;
					edgeDir.z = 0.0;
					
					if ( std::abs( edgeDir.x ) > std::abs( edgeDir.y ) ) {
						if ( edgeDir.x > 0.0 ) {
							left.x = closedMax.x;
							right.x = closedMin.x;
						} else {
							left.x = closedMin.x;
							right.x = closedMax.x;
						}
					} else {
						if ( edgeDir.y > 0.0 ) {
							left.y = closedMax.y;
							right.y = closedMin.y;
						} else {
							left.y = closedMin.y;
							right.y = closedMax.y;
						}
					}
				}
			}

			const nav_face_t &nextFace = g_nav_faces[ next_face_idx ];
			const bool isDynamicPortal = face.entity_id != ENTITYNUM_NONE || face.transition_entity_id != ENTITYNUM_NONE ||
				nextFace.entity_id != ENTITYNUM_NONE || nextFace.transition_entity_id != ENTITYNUM_NONE;

			/**
			* Match A*'s static-corridor check by clipping every static portal to both
			* adjacent eroded face interiors.  This prevents the funnel from emitting a
			* waypoint beside a flat exterior corner that the monster capsule cannot clear.
			* Dynamic transitions keep their established door-width handling.
			**/
			if ( !isDynamicPortal ) {
				Vector3DP clearanceLeft = {};
				Vector3DP clearanceRight = {};
				const double agentClearance = std::max( static_cast<double>( agentRadius ), 0.0 );
				if ( Nav_ClipPortalForAgentClearance( face_idx, next_face_idx, left, right, agentClearance, &clearanceLeft, &clearanceRight ) ) {
					left = clearanceLeft;
					right = clearanceRight;
				} else {
					// Proportional clearance shrink fallback: Rather than collapsing to a single zero-width midpoint
					// (which breaks funnel string-pulling and emits artificial zigzag waypoints), shrink the portal
					// endpoints symmetrically toward the portal center up to 40% of portal length.
					Vector3DP edgeDir = left - right;
					edgeDir.z = 0.0;
					const double edgeLen = QM_Vector3LengthDP( edgeDir );
					if ( edgeLen > 0.001 ) {
						edgeDir = edgeDir * ( 1.0 / edgeLen );
						const double maxShrink = std::max( 0.0, ( edgeLen * 0.40 ) );
						const double shrinkDist = std::min( static_cast<double>( agentRadius ), maxShrink );
						right = right + edgeDir * shrinkDist;
						left = left - edgeDir * shrinkDist;
					}
				}
			} else {
				Vector3DP edgeDir = left - right;
				edgeDir.z = 0.0;
				const double edgeLen = QM_Vector3LengthDP( edgeDir );
				if ( edgeLen > 0.001 ) {
					edgeDir = edgeDir * ( 1.0 / edgeLen );
					const double maxShrink = std::max( 0.0, ( edgeLen * 0.5 ) - 0.1 );
					const double shrinkDist = std::min( static_cast<double>( agentRadius ) + 2.0, maxShrink );
					right = right + edgeDir * shrinkDist;
					left = left - edgeDir * shrinkDist;
				}
			}

			const double edgeLen = QM_Vector3LengthDP( left - right );
			if ( edgeLen > 0.001 ) {
				/**
				* Add a mandatory, flat approach point for upward transitions. The approach
				* point keeps the funnel from cutting toward a later L-turn before the mover
				* has reached the stair front.
				**/
				if ( next_face_idx != -1 ) {
					const nav_face_t &currentFace = g_nav_faces[ face_idx ];
					static constexpr double NAV_MIN_UPWARD_APPROACH_RISE = 6.0;

					if ( stepRise > NAV_MIN_UPWARD_APPROACH_RISE ) {
						Vector3DP approachPoint = {};
						if ( Nav_ComputeStepApproachPoint( currentFace, left, right, static_cast<double>( agentRadius ), &approachPoint ) ) {
							// Force the funnel to retain a front-facing approach before the upward portal.
							portals.push_back( { approachPoint, approachPoint, true } );
							// Force a centered crossing target so movement cannot turn into the next riser early.
							const Vector3DP crossingPoint = ( left + right ) * 0.5;
							portals.push_back( { crossingPoint, crossingPoint, true } );
						}
					}
				}
			}
			portals.push_back( { left, right } );
		} else {
			// Fallback: If no portal connects them, use the face center.
			const Vector3DP center = g_nav_faces[ face_idx ].center;
			portals.push_back( { center, center } );
		}
	}

	// Add the goal as the final portal.
	portals.push_back( { goalPosDP, goalPosDP } );

	auto AppendWaypoint = [&]( const Vector3DP &waypoint, const bool forced ) {
		outWaypoints.push_back( static_cast<Vector3>( waypoint ) );
		if ( outForcedWaypoints != nullptr ) {
			outForcedWaypoints->push_back( forced );
		}
	};

	AppendWaypoint( startPosDP, false );

	Vector3DP portalApex = startPosDP;
	Vector3DP portalLeft = startPosDP;
	Vector3DP portalRight = startPosDP;

	int32_t apexIndex = 0;
	int32_t leftIndex = 0;
	int32_t rightIndex = 0;

	for ( int32_t i = 0; i < ( int32_t )portals.size(); ++i ) {
		const Vector3DP &left = portals[ i ].left;
		const Vector3DP &right = portals[ i ].right;

		// Update the right bound.
		if ( Nav_TriArea2D( portalApex, portalRight, right ) <= 0.0 ) {
			double distSqRight = ( portalApex.x - portalRight.x ) * ( portalApex.x - portalRight.x ) + ( portalApex.y - portalRight.y ) * ( portalApex.y - portalRight.y );
			if ( distSqRight < 0.001 || Nav_TriArea2D( portalApex, portalLeft, right ) > 0.0 ) {
				// Tighten the funnel.
				portalRight = right;
				rightIndex = i;
			} else {
				// Right crossed left, so the left bound is a corner.
				AppendWaypoint( portalLeft, false );
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
		if ( Nav_TriArea2D( portalApex, portalLeft, left ) >= 0.0 ) {
			double distSqLeft = ( portalApex.x - portalLeft.x ) * ( portalApex.x - portalLeft.x ) + ( portalApex.y - portalLeft.y ) * ( portalApex.y - portalLeft.y );
			if ( distSqLeft < 0.001 || Nav_TriArea2D( portalApex, portalRight, left ) < 0.0 ) {
				// Tighten the funnel.
				portalLeft = left;
				leftIndex = i;
			} else {
				// Left crossed right, so the right bound is a corner.
				AppendWaypoint( portalRight, false );
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

		/**
		* Commit a stair-front approach only after the ordinary funnel tests have
		* accepted it, so the forced waypoint cannot bypass an earlier corner.
		**/
		if ( portals[ i ].force_waypoint ) {
			if ( QM_Vector3DistanceSqrDP( portalApex, left ) > static_cast<double>( WAYPOINT_EPS_SQR ) ) {
				AppendWaypoint( left, true );
			}
			portalApex = left;
			apexIndex = i;
			portalLeft = portalApex;
			portalRight = portalApex;
			leftIndex = apexIndex;
			rightIndex = apexIndex;
		}
	}

	AppendWaypoint( goalPosDP, false );

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
