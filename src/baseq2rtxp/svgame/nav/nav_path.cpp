#include "svgame/svg_local.h"
#include "svgame/nav/nav_path.h"
#include "svgame/nav/nav_containers.h"
#include "svgame/nav/nav_core.h"
#include "svgame/nav/nav_generate.h" // For g_nav_nodes and g_nav_polys.
#include "svgame/svg_utils.h"
#include "svgame/entities/func/svg_func_door.h"
#include "svgame/entities/func/svg_func_door_rotating.h"
#include "svgame/entities/func/svg_func_wall.h"
#include "svgame/entities/func/svg_func_plat.h"
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

	/**
	*	In a counter-clockwise half-edge loop with upward normal, the outward traversal
	*	normal points to the right of (a0 -> a1). Looking forward in traversal direction:
	*	a1 (seg1_dp) is strictly on the LEFT, and a0 (seg0_dp) is strictly on the RIGHT.
	**/
	if ( outV0 ) {
		*outV0 = seg1_dp;
	}
	if ( outV1 ) {
		*outV1 = seg0_dp;
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

		// Discard faces flagged with CM_SURFACE_NO_NAVMESH or CONTENTS_NO_NAVMESH:
		if ( ( face.surface_flags & ( CM_SURFACE_NO_NAVMESH | CONTENTS_NO_NAVMESH ) ) != 0 ) {
			continue;
		}

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

		// Discard faces flagged with CM_SURFACE_NO_NAVMESH or CONTENTS_NO_NAVMESH:
		if ( ( face.surface_flags & ( CM_SURFACE_NO_NAVMESH | CONTENTS_NO_NAVMESH ) ) != 0 ) {
			continue;
		}

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
*	@brief	Locate the navmesh polygon enclosing a world-space point strictly within its local KD-leaf.
*	@note	Unlike Nav_FindPolyInLeaf, this function strictly never falls back to Nav_FindClosestPolyGlobal.
*			If the point is not contained within any face of the resolved KD-tree leaf, it returns -1 immediately in O(log N).
*	@param	point	Query position in world space (Vector3DP).
*	@return	Face index if contained within a leaf face, or -1 otherwise.
**/
int32_t Nav_FindFaceInLeafStrict( const Vector3DP &point ) {
	const int32_t leafIdx = Nav_FindLeafNode( point );
	if ( leafIdx < 0 ) {
		return -1;
	}

	const nav_kdtree_node_t &leaf = g_nav_nodes[ leafIdx ];
	const int32_t firstFaceIdx = leaf.first_face_id;
	if ( firstFaceIdx == -1 || firstFaceIdx >= static_cast< int32_t >( g_nav_faces.size() ) ) {
		return -1;
	}

	//! Monotonically incrementing query identifier for mailboxing deduplication.
	static uint32_t s_strict_query_id = 1;
	const uint32_t current_query_id = ++s_strict_query_id;
	if ( s_strict_query_id == 0 ) {
		s_strict_query_id = 1;
	}

	int32_t bestInsideFace = -1;
	double bestInsideDist = 64.0;

	for ( int32_t i = 0; i < leaf.num_faces; ++i ) {
		const int32_t faceIdx = firstFaceIdx + i;
		if ( faceIdx >= static_cast< int32_t >( g_nav_faces.size() ) ) {
			break;
		}

		const nav_face_t &face = g_nav_faces[ faceIdx ];

		// Discard faces flagged with CM_SURFACE_NO_NAVMESH or CONTENTS_NO_NAVMESH:
		if ( ( face.surface_flags & ( CM_SURFACE_NO_NAVMESH | CONTENTS_NO_NAVMESH ) ) != 0 ) {
			continue;
		}

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
	}

	return bestInsideFace;
}

/**
*	@brief	Calculate the portal segment endpoints between two adjacent nav polygons.
*	@param	faceA	Index of the first polygon.
*	@param	faceB	Index of the second polygon.
*	@param	outV0	Output receiving the Left endpoint relative to traversal.
*	@param	outV1	Output receiving the Right endpoint relative to traversal.
*	@return	True if a shared edge was identified.
**/
bool Nav_GetPortalEndpoints( int32_t faceA, int32_t faceB, Vector3DP *outV0, Vector3DP *outV1 ) {
	/**
	*	Sanity checks: ensure valid output pointers and valid face indices.
	**/
	if ( outV0 == nullptr || outV1 == nullptr || faceA < 0 || faceB < 0 ||
		static_cast< size_t >( faceA ) >= g_nav_faces.size() ||
		static_cast< size_t >( faceB ) >= g_nav_faces.size() ) {
		return false;
	}

	const nav_face_t &fA = g_nav_faces[ faceA ];
	bool found_any = false;
	Vector3DP base_left, base_right;
	Vector3DP merged_left, merged_right;
	Vector3DP line_dir;
	double min_t = 0.0;
	double max_t = 0.0;

	/**
	*	Find the half-edges linking Face A to Face B and compute the overlap segment.
	**/
	for ( int32_t e = 0; e < fA.num_edges; ++e ) {
		const nav_halfedge_t &he = g_nav_halfedges[ fA.first_edge_idx + e ];
		// Skip boundary edges without twins.
		if ( he.twin_idx == -1 ) {
			continue;
		}

		const nav_halfedge_t &twin = g_nav_halfedges[ he.twin_idx ];
		// Skip edges that connect to a different neighbor face.
		if ( twin.face_idx != faceB ) {
			continue;
		}

		Vector3DP temp_left, temp_right;
		// Nav_ComputePortalOverlapSegment outputs temp_left (Left) and temp_right (Right) in CCW traversal space.
		if ( Nav_ComputePortalOverlapSegment( he, twin, &temp_left, &temp_right, nullptr ) ) {
			if ( !found_any ) {
				found_any = true;
				base_left = temp_left;
				base_right = temp_right;
				merged_left = temp_left;
				merged_right = temp_right;

				// line_dir points from Right to Left along the CCW half-edge vector.
				line_dir = base_left - base_right;
				const double lenSqr = line_dir.x * line_dir.x + line_dir.y * line_dir.y + line_dir.z * line_dir.z;
				if ( lenSqr > 0.0001 ) {
					const double len = std::sqrt( lenSqr );
					line_dir = line_dir * ( 1.0 / len );
					min_t = 0.0;
					max_t = len;
				} else {
					line_dir = Vector3DP( 1.0, 0.0, 0.0 );
					min_t = 0.0;
					max_t = 0.0;
				}
			} else {
				// Project additional collinear subsegments onto the edge axis.
				const double t_left = QM_Vector3DotProductDP( temp_left - base_right, line_dir );
				const double t_right = QM_Vector3DotProductDP( temp_right - base_right, line_dir );

				min_t = std::min( min_t, std::min( t_left, t_right ) );
				max_t = std::max( max_t, std::max( t_left, t_right ) );

				merged_right = base_right + line_dir * min_t;
				merged_left = base_right + line_dir * max_t;
			}
		}
	}

	/**
	*	If a valid portal was found, assign topological Left to outV0 and Right to outV1.
	**/
	if ( found_any ) {
		*outV0 = merged_left;
		*outV1 = merged_right;
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

//! Priority queue wrapper exposing underlying container for heap-free clearing and capacity reuse.
struct astar_priority_queue : public std::priority_queue<AStarNode, std::vector<AStarNode>, std::greater<AStarNode>> {
	void clear() {
		this->c.clear();
	}
	void reserve( const size_t capacity ) {
		this->c.reserve( capacity );
	}
	size_t capacity() const {
		return this->c.capacity();
	}
};
//! Persistent priority queue for A* graph exploration to eliminate per-search heap allocations.
static astar_priority_queue s_astar_open_set;

/**
*	@brief	Test whether a portal retains a usable agent-center corridor using double precision.
**/
static bool Nav_ClipPortalForAgentClearance( const int32_t faceAIdx, const int32_t faceBIdx, const Vector3DP &portalLeft, const Vector3DP &portalRight, const double wallClearance, const double cornerClearance, Vector3DP *outLeft, Vector3DP *outRight, bool *outIsNarrowPortal = nullptr );

/**
*	@brief	Ensure the global precomputed table of convex obstacle corners and solid boundary edges is initialized.
**/
static void Nav_EnsureObstacleCornersTable();

//! Connected component partition identifier per nav face for O(1) reachability rejection.
static std::vector<int32_t> s_nav_face_components;
//! Precomputed 2D portal width per half-edge for O(1) portal clearance checks.
static std::vector<double> s_nav_edge_portal_widths;

//! Pre-allocated flat A* node state per nav face to avoid dynamic heap allocations.
struct nav_astar_face_t {
	uint32_t queryId = 0;
	double gScore = 0.0;
	int32_t cameFrom = -1;
};
//! Persistent flat array of A* node states across all navigation faces.
static std::vector<nav_astar_face_t> s_nav_astar_faces;
//! Monotonically increasing query token counter for O(1) A* state invalidation without reallocations.
static uint32_t s_nav_astar_query_counter = 0;

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

	Nav_EnsureObstacleCornersTable();

	// O(1) Connected Component Reachability Test:
	// If start and goal faces are in disconnected navmesh islands (e.g. roof vs floor or unreachable ledge),
	// reject immediately without performing an expensive exhaustive graph flood-fill!
	if ( static_cast<size_t>( startFace ) < s_nav_face_components.size() &&
		 static_cast<size_t>( goalFace ) < s_nav_face_components.size() ) {
		if ( s_nav_face_components[ startFace ] != s_nav_face_components[ goalFace ] ) {
			return false;
		}
	}

	s_astar_open_set.clear();
	if ( s_astar_open_set.capacity() < 512 ) {
		s_astar_open_set.reserve( 512 );
	}

	// Ensure persistent flat A* node state array matches mesh face count
	if ( s_nav_astar_faces.size() < g_nav_faces.size() ) {
		s_nav_astar_faces.resize( g_nav_faces.size() );
	}

	// Advance query counter for O(1) node invalidation across searches without memory reallocations
	uint32_t currentQueryToken = ++s_nav_astar_query_counter;
	if ( currentQueryToken == 0 ) {
		for ( auto &node : s_nav_astar_faces ) {
			node.queryId = 0;
		}
		s_nav_astar_query_counter = 1;
	}

	s_nav_astar_faces[ startFace ].queryId = s_nav_astar_query_counter;
	s_nav_astar_faces[ startFace ].gScore = 0.0;
	s_nav_astar_faces[ startFace ].cameFrom = -1;

	auto Heuristic = []( int32_t a, int32_t b ) {
		const double dist = QM_Vector3DistanceDP( g_nav_faces[ a ].center, g_nav_faces[ b ].center );
		const double slopeA = QM_Clamp( g_nav_faces[ a ].normal.z, 0.0, 1.0 );
		return dist * ( 1.0 + ( 1.0 - slopeA ) * 4.0 );
	};
	s_astar_open_set.push( { startFace, Heuristic( startFace, goalFace ) } );

	while ( !s_astar_open_set.empty() ) {
		const int32_t current = s_astar_open_set.top().polyIdx;
		s_astar_open_set.pop();
		s_nav_last_path_diagnostics.expanded_faces++;

		if ( current == goalFace ) {
			int32_t curr = goalFace;
			while ( curr != startFace ) {
				outPath.push_back( curr );
				curr = s_nav_astar_faces[ curr ].cameFrom;
			}
			outPath.push_back( startFace );
			std::reverse( outPath.begin(), outPath.end() );
			s_nav_last_path_diagnostics.route_found = true;
			return true;
		}

		const nav_face_t &faceCurrent = g_nav_faces[ current ];
		double currentGScore = s_nav_astar_faces[ current ].gScore;

		for ( int32_t e = 0; e < faceCurrent.num_edges; e++ ) {
			const int32_t heIdx = faceCurrent.first_edge_idx + e;
			const nav_halfedge_t &he = g_nav_halfedges[ heIdx ];
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

			// Reject neighbor faces flagged with CM_SURFACE_NO_NAVMESH or CONTENTS_NO_NAVMESH:
			if ( ( faceNeighbor.surface_flags & ( CM_SURFACE_NO_NAVMESH | CONTENTS_NO_NAVMESH ) ) != 0 ) {
				continue;
			}

			const double zDelta = he.z_diff;
			if ( zDelta > NAV_MAX_STEP_HEIGHT ) {
				s_nav_last_path_diagnostics.rejected_step_height++;
				continue;
			}
			if ( -zDelta > policy.max_drop_height ) {
				s_nav_last_path_diagnostics.rejected_drop_height++;
				continue;
			}

			// O(1) Precomputed Portal Width Lookup:
			double portalWidth2D = ( static_cast<size_t>( heIdx ) < s_nav_edge_portal_widths.size() )
				? s_nav_edge_portal_widths[ heIdx ]
				: 0.0;

			// Reject portals narrower than the absolute physical passage threshold (20.0 units for a 32-unit capsule):
			// an agent cannot physically traverse a narrow sliver without penetrating adjacent world geometry.
			if ( portalWidth2D < NAV_ABSOLUTE_MIN_PORTAL_PASSAGE_WIDTH && !policy.ignore_disabled_edges ) {
				s_nav_last_path_diagnostics.rejected_narrow_portal++;
				continue;
			}

			// Penalize transitions across non-horizontal sloped faces (pyramids, roof slopes)
			// so A* strongly prefers flat ground paths over climbing/traversing sloped surfaces.
			const double neighborSlope = QM_Clamp( faceNeighbor.normal.z, 0.0, 1.0 );
			const double slopePenalty = 1.0 + ( 1.0 - neighborSlope ) * 4.0;

			// Penalize narrow / constricted portals that cannot provide full agent clearance (e.g. slivers along walls)
			// so A* strongly favors wide open polygon corridors away from small brush corners.
			double clearancePenalty = 1.0;
			const double effectiveAgentRadius = ( policy.agent_radius > 0.0f ) ? static_cast<double>( policy.agent_radius ) : NAV_DEFAULT_AGENT_RADIUS;
			const double minDesiredWidth = effectiveAgentRadius * 2.0 + NAV_DEFAULT_AGENT_RADIUS;
			const double minPassableWidth = effectiveAgentRadius * 2.0 - NAV_STEP_HEIGHT_PADDING;

			if ( portalWidth2D < minPassableWidth && !policy.ignore_disabled_edges ) {
				// Heavily penalize sliver portals narrower than the agent's physical collision hull
				// so A* will never choose a tight CSG sliver along an obstacle wall when an open route exists.
				const double deficit = minPassableWidth - portalWidth2D;
				clearancePenalty = 50.0 + ( deficit / minPassableWidth ) * 150.0;
			} else if ( portalWidth2D < minDesiredWidth && !policy.ignore_disabled_edges ) {
				const double deficit = minDesiredWidth - portalWidth2D;
				clearancePenalty = 2.0 + ( deficit / minDesiredWidth ) * 10.0;
			}

			const double edgeDistance = QM_Vector3DistanceDP( faceCurrent.center, faceNeighbor.center );
			double edgeCost = edgeDistance * slopePenalty * clearancePenalty;
			// Allow entity-specific edge cost customization (stair preference, corridor hysteresis)
			if ( policy.edge_cost_callback != nullptr ) {
				edgeCost = policy.edge_cost_callback( current, neighborIdx, he, edgeCost, policy.edge_cost_monster );
			}
			const double tentativeGScore = currentGScore + edgeCost;

			bool neighborVisited = ( s_nav_astar_faces[ neighborIdx ].queryId == s_nav_astar_query_counter );
			if ( !neighborVisited || tentativeGScore < s_nav_astar_faces[ neighborIdx ].gScore ) {
				s_nav_astar_faces[ neighborIdx ].queryId = s_nav_astar_query_counter;
				s_nav_astar_faces[ neighborIdx ].cameFrom = current;
				s_nav_astar_faces[ neighborIdx ].gScore = tentativeGScore;
				s_astar_open_set.push( { neighborIdx, tentativeGScore + Heuristic( neighborIdx, goalFace ) } );
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
			const bool isImpassableStep = std::fabs( halfEdge.z_diff ) > NAV_MAX_STEP_HEIGHT;
			if ( isSharedPortal || ( !isDisabledPortal && !isImpassableStep ) ) {
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
			const double midT = ( minT + maxT ) * 0.5;
			minT = midT;
			maxT = midT;
		}
	}

	*outMinT = minT;
	*outMaxT = maxT;
	return true;
}

//! Global table caching whether each vertex in g_nav_vertices touches a solid obstacle/boundary.
static std::vector<uint8_t> s_nav_vertex_is_boundary;

/**
*	@brief	Rebuild the global vertex boundary classification table for instant O(1) portal insetting.
**/
static void Nav_EnsureVertexBoundaryTable() {
	if ( s_nav_vertex_is_boundary.size() == g_nav_vertices.size() ) {
		return;
	}
	s_nav_vertex_is_boundary.assign( g_nav_vertices.size(), 0 );
	for ( const auto &he : g_nav_halfedges ) {
		const bool isSolid = ( he.twin_idx == -1 || ( he.flags & NAV_EDGE_DISABLED ) != 0 || std::fabs( he.z_diff ) > NAV_MAX_STEP_HEIGHT );
		if ( isSolid ) {
			if ( he.vertex_idx >= 0 && static_cast<size_t>( he.vertex_idx ) < s_nav_vertex_is_boundary.size() ) {
				s_nav_vertex_is_boundary[ he.vertex_idx ] = 1;
			}
			const int32_t nextVIdx = g_nav_halfedges[ he.next_idx ].vertex_idx;
			if ( nextVIdx >= 0 && static_cast<size_t>( nextVIdx ) < s_nav_vertex_is_boundary.size() ) {
				s_nav_vertex_is_boundary[ nextVIdx ] = 1;
			}
		}
	}
}


/**
*	@brief	Compute squared 2D perpendicular distance from point p to line segment (a -> b).
*	@param	p	Query point in world space.
*	@param	a	First segment endpoint in world space.
*	@param	b	Second segment endpoint in world space.
*	@return	Squared 2D Euclidean distance between p and the closest point on segment ab.
**/
double Nav_DistancePointToSegment2DSqr( const Vector3DP &p, const Vector3DP &a, const Vector3DP &b ) {
	Vector3DP ab = b - a;
	ab.z = 0.0;
	const double abLen2 = ab.x * ab.x + ab.y * ab.y;
	if ( abLen2 <= 0.0001 ) {
		const double dx = p.x - a.x;
		const double dy = p.y - a.y;
		return dx * dx + dy * dy;
	}
	Vector3DP ap = p - a;
	ap.z = 0.0;
	const double t = QM_Clamp( ( ap.x * ab.x + ap.y * ab.y ) / abLen2, 0.0, 1.0 );
	const Vector3DP proj = a + ab * t;
	const double dx = p.x - proj.x;
	const double dy = p.y - proj.y;
	return dx * dx + dy * dy;
}

/**
*	@brief	Calculate 2D cross product of 3 Vector3DP points for string pulling.
**/
static double Nav_TriArea2D( const Vector3DP &a, const Vector3DP &b, const Vector3DP &c ) {
	return ( b.x - a.x ) * ( c.y - a.y ) - ( c.x - a.x ) * ( b.y - a.y );
}

/**
*	@brief	Check 2D line segment intersection between (p1->p2) and (p3->p4).
**/
static bool Nav_SegmentsIntersect2D( const Vector3DP &p1, const Vector3DP &p2, const Vector3DP &p3, const Vector3DP &p4 ) {
	const double d1 = Nav_TriArea2D( p3, p4, p1 );
	const double d2 = Nav_TriArea2D( p3, p4, p2 );
	const double d3 = Nav_TriArea2D( p1, p2, p3 );
	const double d4 = Nav_TriArea2D( p1, p2, p4 );
	return ( ( ( d1 > 0.001 && d2 < -0.001 ) || ( d1 < -0.001 && d2 > 0.001 ) ) &&
	         ( ( d3 > 0.001 && d4 < -0.001 ) || ( d3 < -0.001 && d4 > 0.001 ) ) );
}

/**
*	@brief	Compute squared 2D minimum distance between segment (a1 -> a2) and segment (b1 -> b2).
**/
static double Nav_DistanceSegmentToSegment2DSqr( const Vector3DP &a1, const Vector3DP &a2, const Vector3DP &b1, const Vector3DP &b2 ) {
	if ( Nav_SegmentsIntersect2D( a1, a2, b1, b2 ) ) {
		return 0.0;
	}
	const double d1 = Nav_DistancePointToSegment2DSqr( a1, b1, b2 );
	const double d2 = Nav_DistancePointToSegment2DSqr( a2, b1, b2 );
	const double d3 = Nav_DistancePointToSegment2DSqr( b1, a1, a2 );
	const double d4 = Nav_DistancePointToSegment2DSqr( b2, a1, a2 );
	return std::min( std::min( d1, d2 ), std::min( d3, d4 ) );
}

//! Cached precomputed obstacle corner representation for fast O(1) runtime queries.
struct nav_cached_corner_t {
	Vector3DP vertex = {};
	Vector3DP u1 = {};
	Vector3DP u2 = {};
	Vector3DP n1 = {};
	Vector3DP n2 = {};
	Vector3DP bisectorNorm = {};
	double bisectorCosHalf = 1.0;
	double minZ = 0.0;
	double maxZ = 0.0;
	uint32_t cornerType = NAV_EDGE_WALL;
};

//! Precomputed cache of all convex obstacle corners in the active navigation mesh.
static std::vector<nav_cached_corner_t> s_nav_cached_corners;
//! Precomputed list of all solid obstacle boundary edges across the active navigation mesh.
static std::vector<std::pair<Vector3DP, Vector3DP>> s_nav_cached_solid_edges;
//! Generation key to detect when the navigation mesh is reloaded or regenerated.
static size_t s_nav_cached_mesh_fingerprint = 0;

//! Pack a 2D integer cell coordinate into a unique 64-bit spatial hash key.
inline int64_t Nav_SpatialGridKey( const int32_t cellX, const int32_t cellY ) {
	return ( static_cast<int64_t>( cellX ) << 32 ) | ( static_cast<int64_t>( cellY ) & 0xFFFFFFFFLL );
}

//! Spatial grid mapping 2D cell keys to solid edge indices for O(1) line-of-sight tests.
static std::unordered_map<int64_t, std::vector<int32_t>> s_nav_spatial_grid_edges;
//! Spatial grid mapping 2D cell keys to corner indices for O(1) corner clearance queries.
static std::unordered_map<int64_t, std::vector<int32_t>> s_nav_spatial_grid_corners;
//! Timestamp / token counters for O(1) query deduplication across adjacent spatial grid cells.
static std::vector<uint32_t> s_nav_cached_edge_query_tokens;
static std::vector<uint32_t> s_nav_cached_corner_query_tokens;
static uint32_t s_nav_spatial_query_counter = 0;
static uint32_t s_nav_spatial_corner_counter = 0;


/**
*	@brief	Ensure the global precomputed table of convex obstacle corners and solid boundary edges is initialized.
*	@note	Constructed once per map load in O(E) and reused across all runtime path queries in O(1).
**/
static void Nav_EnsureObstacleCornersTable() {
	const size_t currentFingerprint = g_nav_faces.size() ^ ( g_nav_halfedges.size() << 16 ) ^ ( g_nav_vertices.size() << 31 );
	if ( !s_nav_cached_corners.empty() && s_nav_cached_mesh_fingerprint == currentFingerprint ) {
		return;
	}

	s_nav_cached_mesh_fingerprint = currentFingerprint;
	s_nav_cached_corners.clear();
	s_nav_cached_solid_edges.clear();
	s_nav_spatial_grid_edges.clear();
	s_nav_spatial_grid_corners.clear();
	s_nav_cached_edge_query_tokens.clear();
	s_nav_cached_corner_query_tokens.clear();

	if ( g_nav_faces.empty() || g_nav_halfedges.empty() || g_nav_vertices.empty() ) {
		return;
	}

	struct solid_edge_t {
		Vector3DP v0, v1;
		Vector3DP edgeDirNorm;
		Vector3DP inwardNorm;
		int32_t v0_idx = -1;
		int32_t v1_idx = -1;
		uint32_t edgeFlags = NAV_EDGE_NONE;
	};
	std::vector<solid_edge_t> solidEdges;

	// 1. Gather all solid boundary halfedges across the entire navigation mesh:
	for ( size_t f = 0; f < g_nav_faces.size(); ++f ) {
		const nav_face_t &face = g_nav_faces[ f ];

		for ( int32_t e = 0; e < face.num_edges; e++ ) {
			const nav_halfedge_t &he = g_nav_halfedges[ face.first_edge_idx + e ];
			const bool isSolid = ( he.twin_idx == -1 || ( he.flags & NAV_EDGE_DISABLED ) != 0 || std::fabs( he.z_diff ) > NAV_SOLID_EDGE_MIN_Z_DIFF );
			if ( isSolid ) {
				const int32_t idx0 = he.vertex_idx;
				const int32_t idx1 = g_nav_halfedges[ he.next_idx ].vertex_idx;
				const Vector3DP &v0 = g_nav_vertices[ idx0 ];
				const Vector3DP &v1 = g_nav_vertices[ idx1 ];

				Vector3DP d = v1 - v0;
				d.z = 0.0;
				const double len = QM_Vector3LengthDP( d );
				if ( len > 0.001 ) {
					const Vector3DP u = d * ( 1.0 / len );
					// In a CCW winding face, the 2D left-normal of edge direction u points strictly into the face interior.
					// Avoid using (face.center - v0) because for acute sliver polygons, the center vector can form an
					// obtuse angle with the edge normal, causing erroneous normal inversion into solid obstacles.
					Vector3DP inNorm{ -u.y, u.x, 0.0 };

					// Classify structural role: ground obstacle wall vs elevated drop-off ledge:
					uint32_t edgeFlags = NAV_EDGE_NONE;
					if ( he.twin_idx != -1 ) {
						if ( he.z_diff > NAV_MAX_STEP_HEIGHT ) {
							edgeFlags |= NAV_EDGE_WALL;
						} else if ( he.z_diff < -NAV_MAX_STEP_HEIGHT ) {
							edgeFlags |= NAV_EDGE_DROPOFF;
						} else {
							edgeFlags |= NAV_EDGE_WALL;
						}
					} else {
						// Boundary edge without twin: probe collision model to determine whether
						// this edge bounds a solid wall rising above ground or an open-air drop-off.
						const Vector3DP mid = ( v0 + v1 ) * 0.5;
						const Vector3DP outNorm = inNorm * -1.0;
						const Vector3 probeStart( static_cast<float>( mid.x ), static_cast<float>( mid.y ), static_cast<float>( mid.z + NAV_MAX_STEP_HEIGHT ) );
						const Vector3 probeEnd = probeStart + static_cast<Vector3>( outNorm * NAV_BOUNDARY_WALL_PROBE_DIST );
						const svg_trace_t tr = SVG_Trace( probeStart, vec3_origin, vec3_origin, probeEnd, nullptr, CM_CONTENTMASK_SOLID );
						if ( tr.startsolid || tr.fraction < 1.0f ) {
							edgeFlags |= NAV_EDGE_WALL;
						} else {
							edgeFlags |= NAV_EDGE_DROPOFF;
						}
					}

					solidEdges.push_back( { v0, v1, u, inNorm, idx0, idx1, edgeFlags } );
					s_nav_cached_solid_edges.push_back( { v0, v1 } );
				}
			}
		}
	}

	// 2. Precalculate all convex obstacle corner geometries once for the map:
	for ( size_t i = 0; i < solidEdges.size(); ++i ) {
		const auto &e1 = solidEdges[ i ];
		for ( size_t j = i + 1; j < solidEdges.size(); ++j ) {
			const auto &e2 = solidEdges[ j ];

			// Enforce structural edge compatibility:
			// An obstacle wall edge must only pair with another obstacle wall edge.
			// A drop-off ledge edge must only pair with another drop-off ledge edge.
			// This maintains NAV_DROPOFF_ALLOWED_SIZE (128.0 units) for drop-offs without cross-pairing ground walls with platform roofs.
			if ( ( e1.edgeFlags & ( NAV_EDGE_WALL | NAV_EDGE_DROPOFF ) ) != ( e2.edgeFlags & ( NAV_EDGE_WALL | NAV_EDGE_DROPOFF ) ) ) {
				continue;
			}

			// Find shared corner vertex between the two solid boundary edges.
			// Matches horizontally with tolerance up to NAV_DROPOFF_ALLOWED_SIZE vertically to handle slopes and curbs.
			Vector3DP vCorner = {};
			Vector3DP farPt1 = {};
			Vector3DP farPt2 = {};
			bool sharesVertex = false;

			auto PointsShareCorner = []( const Vector3DP &p1, const Vector3DP &p2 ) -> bool {
				const double dx = p1.x - p2.x;
				const double dy = p1.y - p2.y;
				return ( ( dx * dx + dy * dy ) <= NAV_CORNER_VERTEX_EPSILON_SQR && std::fabs( p1.z - p2.z ) <= static_cast<double>( NAV_DROPOFF_ALLOWED_SIZE ) );
			};

			if ( e1.v1_idx == e2.v0_idx || PointsShareCorner( e1.v1, e2.v0 ) ) {
				vCorner = ( e1.v1 + e2.v0 ) * 0.5;
				farPt1 = e1.v0;
				farPt2 = e2.v1;
				sharesVertex = true;
			} else if ( e1.v1_idx == e2.v1_idx || PointsShareCorner( e1.v1, e2.v1 ) ) {
				vCorner = ( e1.v1 + e2.v1 ) * 0.5;
				farPt1 = e1.v0;
				farPt2 = e2.v0;
				sharesVertex = true;
			} else if ( e1.v0_idx == e2.v0_idx || PointsShareCorner( e1.v0, e2.v0 ) ) {
				vCorner = ( e1.v0 + e2.v0 ) * 0.5;
				farPt1 = e1.v1;
				farPt2 = e2.v1;
				sharesVertex = true;
			} else if ( e1.v0_idx == e2.v1_idx || PointsShareCorner( e1.v0, e2.v1 ) ) {
				vCorner = ( e1.v0 + e2.v1 ) * 0.5;
				farPt1 = e1.v1;
				farPt2 = e2.v0;
				sharesVertex = true;
			}

			if ( !sharesVertex ) {
				continue;
			}

			Vector3DP d1 = farPt1 - vCorner;
			Vector3DP d2 = farPt2 - vCorner;
			d1.z = 0.0;
			d2.z = 0.0;
			const double len1 = QM_Vector3LengthDP( d1 );
			const double len2 = QM_Vector3LengthDP( d2 );
			if ( len1 <= 0.001 || len2 <= 0.001 ) {
				continue;
			}
			const Vector3DP u1 = d1 * ( 1.0 / len1 );
			const Vector3DP u2 = d2 * ( 1.0 / len2 );

			// Check if two solid edges form a true turn (not a continuous straight wall, turn angle >= 31.8 deg):
			const double wallTurnDot = QM_Vector3DotProductDP( u1, u2 );
			if ( wallTurnDot < NAV_CORNER_STRAIGHT_WALL_MAX_DOT ) {
				continue; // Straight continuous wall
			}

			const Vector3DP &n1 = e1.inwardNorm;
			const Vector3DP &n2 = e2.inwardNorm;

			Vector3DP bisector = n1 + n2;
			bisector.z = 0.0;
			const double bisectorLen = QM_Vector3LengthDP( bisector );
			if ( bisectorLen <= 0.001 ) {
				continue;
			}
			const Vector3DP bisectorNorm = bisector * ( 1.0 / bisectorLen );

			// Convexity Invariant: The angle bisector of a convex obstacle corner points OUT into the open floor,
			// which is opposite to the boundary edges (b . u1 <= 0 and b . u2 <= 0).
			// Concave interior room corners have the bisector pointing into the corner cone (b . u > 0) and must be excluded.
			if ( QM_Vector3DotProductDP( bisectorNorm, u1 ) > 0.001 || QM_Vector3DotProductDP( bisectorNorm, u2 ) > 0.001 ) {
				continue; // Concave room corner or flat wall
			}

			const double dotN = std::clamp( QM_Vector3DotProductDP( n1, n2 ), -1.0, 1.0 );
			const double cosHalf = std::sqrt( std::max( 0.001, ( 1.0 + dotN ) * 0.5 ) );

			const double cornerMinZ = std::min( { e1.v0.z, e1.v1.z, e2.v0.z, e2.v1.z } );
			const double cornerMaxZ = std::max( { e1.v0.z, e1.v1.z, e2.v0.z, e2.v1.z } );
			const uint32_t cornerType = ( e1.edgeFlags & ( NAV_EDGE_WALL | NAV_EDGE_DROPOFF ) );

			// Deduplicate corners spatially while maintaining total vertical coverage and matching corner type:
			bool duplicate = false;
			for ( auto &existing : s_nav_cached_corners ) {
				if ( existing.cornerType != cornerType ) {
					continue;
				}
				const double dx = existing.vertex.x - vCorner.x;
				const double dy = existing.vertex.y - vCorner.y;
				if ( ( dx * dx + dy * dy ) < NAV_CORNER_DEDUPLICATION_RADIUS_SQR &&
					 vCorner.z >= ( existing.minZ - static_cast<double>( NAV_MAX_STEP_HEIGHT ) ) &&
					 vCorner.z <= ( existing.maxZ + static_cast<double>( NAV_MAX_STEP_HEIGHT ) ) ) {
					existing.minZ = std::min( existing.minZ, cornerMinZ );
					existing.maxZ = std::max( existing.maxZ, cornerMaxZ );
					duplicate = true;
					break;
				}
			}
			if ( !duplicate ) {
				s_nav_cached_corners.push_back( { vCorner, u1, u2, n1, n2, bisectorNorm, cosHalf, cornerMinZ, cornerMaxZ, cornerType } );
			}
		}
	}

	// 3. Populate spatial hash grid buckets for O(1) solid edge lookups:
	s_nav_cached_edge_query_tokens.assign( s_nav_cached_solid_edges.size(), 0 );
	for ( size_t k = 0; k < s_nav_cached_solid_edges.size(); ++k ) {
		const auto &edge = s_nav_cached_solid_edges[ k ];
		const int32_t minCX = static_cast<int32_t>( std::floor( std::min( edge.first.x, edge.second.x ) * NAV_SPATIAL_GRID_INV_CELL_SIZE ) );
		const int32_t maxCX = static_cast<int32_t>( std::floor( std::max( edge.first.x, edge.second.x ) * NAV_SPATIAL_GRID_INV_CELL_SIZE ) );
		const int32_t minCY = static_cast<int32_t>( std::floor( std::min( edge.first.y, edge.second.y ) * NAV_SPATIAL_GRID_INV_CELL_SIZE ) );
		const int32_t maxCY = static_cast<int32_t>( std::floor( std::max( edge.first.y, edge.second.y ) * NAV_SPATIAL_GRID_INV_CELL_SIZE ) );

		for ( int32_t cy = minCY; cy <= maxCY; ++cy ) {
			for ( int32_t cx = minCX; cx <= maxCX; ++cx ) {
				s_nav_spatial_grid_edges[ Nav_SpatialGridKey( cx, cy ) ].push_back( static_cast<int32_t>( k ) );
			}
		}
	}

	// 4. Populate spatial hash grid buckets for O(1) corner queries:
	s_nav_cached_corner_query_tokens.assign( s_nav_cached_corners.size(), 0 );
	for ( size_t k = 0; k < s_nav_cached_corners.size(); ++k ) {
		const auto &c = s_nav_cached_corners[ k ];
		const int32_t cx = static_cast<int32_t>( std::floor( c.vertex.x * NAV_SPATIAL_GRID_INV_CELL_SIZE ) );
		const int32_t cy = static_cast<int32_t>( std::floor( c.vertex.y * NAV_SPATIAL_GRID_INV_CELL_SIZE ) );
		s_nav_spatial_grid_corners[ Nav_SpatialGridKey( cx, cy ) ].push_back( static_cast<int32_t>( k ) );
	}

	// 5. Precompute connected component partition across all faces for O(1) reachability testing:
	s_nav_face_components.assign( g_nav_faces.size(), -1 );
	int32_t componentCount = 0;
	std::vector<int32_t> bfsQueue;
	bfsQueue.reserve( g_nav_faces.size() );

	for ( size_t f = 0; f < g_nav_faces.size(); ++f ) {
		if ( s_nav_face_components[ f ] != -1 ) {
			continue;
		}

		const int32_t currentComponent = componentCount++;
		bfsQueue.clear();
		bfsQueue.push_back( static_cast<int32_t>( f ) );
		s_nav_face_components[ f ] = currentComponent;

		size_t head = 0;
		while ( head < bfsQueue.size() ) {
			const int32_t currFaceIdx = bfsQueue[ head++ ];
			const nav_face_t &currFace = g_nav_faces[ currFaceIdx ];

			for ( int32_t e = 0; e < currFace.num_edges; ++e ) {
				const nav_halfedge_t &he = g_nav_halfedges[ currFace.first_edge_idx + e ];
				if ( he.twin_idx == -1 || static_cast<size_t>( he.twin_idx ) >= g_nav_halfedges.size() ) {
					continue;
				}
				const nav_halfedge_t &twin = g_nav_halfedges[ he.twin_idx ];
				const int32_t neighborIdx = twin.face_idx;
				if ( neighborIdx < 0 || static_cast<size_t>( neighborIdx ) >= g_nav_faces.size() ) {
					continue;
				}

				if ( s_nav_face_components[ neighborIdx ] == -1 ) {
					// Check vertical delta to preserve physical island boundaries:
					// Transitions exceeding max drop height are disconnected.
					if ( std::fabs( he.z_diff ) <= static_cast<double>( NAV_PROBE_DEFAULT_MAX_DROP_HEIGHT ) ) {
						s_nav_face_components[ neighborIdx ] = currentComponent;
						bfsQueue.push_back( neighborIdx );
					}
				}
			}
		}
	}

	// 6. Precompute 2D portal widths across all half-edges:
	s_nav_edge_portal_widths.assign( g_nav_halfedges.size(), 0.0 );
	for ( size_t e = 0; e < g_nav_halfedges.size(); ++e ) {
		const nav_halfedge_t &he = g_nav_halfedges[ e ];
		if ( he.twin_idx == -1 || static_cast<size_t>( he.twin_idx ) >= g_nav_halfedges.size() ) {
			continue;
		}
		const nav_halfedge_t &twin = g_nav_halfedges[ he.twin_idx ];
		Vector3DP pLeft{}, pRight{};
		double width = 0.0;
		if ( Nav_ComputePortalOverlapSegment( he, twin, &pLeft, &pRight, &width ) ) {
			s_nav_edge_portal_widths[ e ] = width;
		}
	}

	// 7. Ensure persistent flat A* node state array matches mesh face count:
	s_nav_astar_faces.assign( g_nav_faces.size(), nav_astar_face_t{} );
}

/**
*	@brief	Compute intersection distance t >= 0 along a 2D ray (origin + dir*t) with a 2D segment (v0 -> v1).
*	@param	origin	Ray origin.
*	@param	dir		Normalized ray direction.
*	@param	v0		Segment start.
*	@param	v1		Segment end.
*	@param	outT	[out] Ray parameter t if intersection occurs.
*	@return	True if ray intersects segment at t >= 0.
**/
static bool Nav_RayIntersectSegment2D( const Vector3DP &origin, const Vector3DP &dir, const Vector3DP &v0, const Vector3DP &v1, double *outT ) {
	const Vector3DP seg = v1 - v0;
	const double cross = dir.x * seg.y - dir.y * seg.x;
	if ( std::fabs( cross ) < NAV_RAY_PARALLEL_EPSILON ) {
		return false;
	}

	const Vector3DP d = v0 - origin;
	const double t = ( d.x * seg.y - d.y * seg.x ) / cross;
	const double s = ( d.x * dir.y - d.y * dir.x ) / cross;

	if ( t >= 0.0 && s >= 0.0 && s <= 1.0 ) {
		if ( outT != nullptr ) {
			*outT = t;
		}
		return true;
	}
	return false;
}

/**
*	@brief	Continuous 2D circle-cast intersection against a line segment.
*	@param	origin		Center of the moving circle at t = 0.
*	@param	dir			Normalized movement direction vector.
*	@param	radius		Radius of the moving circle.
*	@param	v0			Start vertex of the line segment.
*	@param	v1			End vertex of the line segment.
*	@param	outT		[out] Time of impact (distance traveled) along dir.
*	@param	outNormal	[out] Contact normal pointing back toward the circle center.
*	@return	True if the moving circle impacts the segment at t >= 0.
**/
static bool Nav_CircleIntersectSegment2D( const Vector3DP &origin, const Vector3DP &dir, const double radius, const Vector3DP &v0, const Vector3DP &v1, double *outT, Vector3DP *outNormal ) {
	if ( radius <= 0.001 ) {
		const Vector3DP seg = v1 - v0;
		if ( outNormal != nullptr ) {
			const double segLen = std::sqrt( seg.x * seg.x + seg.y * seg.y );
			if ( segLen > 0.001 ) {
				const Vector3DP segUnit = seg * ( 1.0 / segLen );
				*outNormal = Vector3DP{ -segUnit.y, segUnit.x, 0.0 };
			} else {
				*outNormal = -dir;
			}
		}
		return Nav_RayIntersectSegment2D( origin, dir, v0, v1, outT );
	}

	const Vector3DP seg = v1 - v0;
	const double segLen = std::sqrt( seg.x * seg.x + seg.y * seg.y );
	if ( segLen <= 0.001 ) {
		// Degenerate segment: test moving circle against point v0
		const Vector3DP d = origin - v0;
		const double b = ( d.x * dir.x + d.y * dir.y );
		const double c = ( d.x * d.x + d.y * d.y ) - ( radius * radius );
		if ( c <= 0.0 ) {
			if ( outT ) *outT = 0.0;
			if ( outNormal ) *outNormal = -dir;
			return true;
		}
		const double discr = b * b - c;
		if ( discr >= 0.0 ) {
			const double t = -b - std::sqrt( discr );
			if ( t >= 0.0 ) {
				if ( outT ) *outT = t;
				const Vector3DP hitPos = origin + ( dir * t );
				if ( outNormal ) *outNormal = QM_Vector3NormalizeDP( hitPos - v0 );
				return true;
			}
		}
		return false;
	}

	const Vector3DP segUnit = seg * ( 1.0 / segLen );
	const Vector3DP segNorm{ -segUnit.y, segUnit.x, 0.0 }; // Left-facing normal

	double bestT = 999999.0;
	Vector3DP bestNorm = {};
	bool hit = false;

	// 1. Test front face of the segment (offset line by +radius along normal):
	const double dirDotNorm = ( dir.x * segNorm.x + dir.y * segNorm.y );
	if ( dirDotNorm < -0.0001 ) {
		const Vector3DP d = v0 - origin;
		const double distToLine = ( d.x * segNorm.x + d.y * segNorm.y );
		const double tLine = ( distToLine + radius ) / dirDotNorm;
		if ( tLine >= 0.0 && tLine < bestT ) {
			const Vector3DP hitCenter = origin + ( dir * tLine );
			const double s = ( ( hitCenter.x - v0.x ) * segUnit.x + ( hitCenter.y - v0.y ) * segUnit.y );
			if ( s >= -0.001 && s <= ( segLen + 0.001 ) ) {
				bestT = tLine;
				bestNorm = segNorm;
				hit = true;
			}
		}
	}

	// 2. Test start vertex cap (v0):
	{
		const Vector3DP d = origin - v0;
		const double b = ( d.x * dir.x + d.y * dir.y );
		const double c = ( d.x * d.x + d.y * d.y ) - ( radius * radius );
		if ( c <= 0.0 ) {
			if ( 0.0 < bestT ) {
				bestT = 0.0;
				bestNorm = -dir;
				hit = true;
			}
		} else {
			const double discr = b * b - c;
			if ( discr >= 0.0 ) {
				const double t = -b - std::sqrt( discr );
				if ( t >= 0.0 && t < bestT ) {
					bestT = t;
					const Vector3DP hitPos = origin + ( dir * t );
					bestNorm = QM_Vector3NormalizeDP( hitPos - v0 );
					hit = true;
				}
			}
		}
	}

	// 3. Test end vertex cap (v1):
	{
		const Vector3DP d = origin - v1;
		const double b = ( d.x * dir.x + d.y * dir.y );
		const double c = ( d.x * d.x + d.y * d.y ) - ( radius * radius );
		if ( c <= 0.0 ) {
			if ( 0.0 < bestT ) {
				bestT = 0.0;
				bestNorm = -dir;
				hit = true;
			}
		} else {
			const double discr = b * b - c;
			if ( discr >= 0.0 ) {
				const double t = -b - std::sqrt( discr );
				if ( t >= 0.0 && t < bestT ) {
					bestT = t;
					const Vector3DP hitPos = origin + ( dir * t );
					bestNorm = QM_Vector3NormalizeDP( hitPos - v1 );
					hit = true;
				}
			}
		}
	}

	if ( hit ) {
		if ( outT ) *outT = bestT;
		if ( outNormal ) *outNormal = bestNorm;
		return true;
	}
	return false;
}

/**
*	@brief	Perform a 2D circle cast (continuous cylinder/capsule sweep) across the navigation mesh half-edge graph.
*	@param	start			Sweep origin in double-precision world coordinates.
*	@param	dirNorm			Normalized 2D sweep direction vector (dirNorm.z = 0.0).
*	@param	maxDistance		Maximum sweep distance in world units.
*	@param	agentRadius		Agent cylinder/capsule collision radius in world units (e.g. 16.0).
*	@param	outResult		[out] Pointer to result struct storing hit distance, normal, zone type, and portal metadata.
*	@return	True if an intersection occurred within maxDistance.
**/
bool Nav_CircleCastHalfEdge2D( const Vector3DP &start, const Vector3DP &dirNorm, const double maxDistance, const double agentRadius, nav_raycast_result_t *outResult ) {
	if ( !outResult || maxDistance <= 0.0 ) {
		return false;
	}

	Nav_EnsureObstacleCornersTable();

	// Normalize 2D sweep direction:
	Vector3DP rayDir = dirNorm;
	rayDir.z = 0.0;
	const double dirLen = QM_Vector3LengthDP( rayDir );
	if ( dirLen > 0.001 ) {
		rayDir = rayDir * ( 1.0 / dirLen );
	} else {
		rayDir = Vector3DP{ 1.0, 0.0, 0.0 };
	}

	const double effectiveRadius = std::max( 0.0, agentRadius );

	// Initialize default result assuming unbounded traversal:
	outResult->hitSolidWall = false;
	outResult->hitPortal = false;
	outResult->hitDropoff = false;
	outResult->hitDistance = maxDistance;
	outResult->hitPoint = start + ( rayDir * maxDistance );
	outResult->hitNormal = -rayDir;
	outResult->hitFaceIndex = -1;
	outResult->hitHalfedgeIndex = -1;
	outResult->zone_type = ZONE_TYPE_OPEN_SPACE;
	outResult->portal_type = PORTAL_TYPE_OPEN_APERTURE;
	outResult->targetRoomId = -1;
	outResult->portalEntityNumber = ENTITYNUM_NONE;

	// Locate start face across local KD-leaf or topological floor:
	int32_t startFaceIdx = Nav_FindFaceInLeafStrict( start );
	if ( startFaceIdx < 0 || startFaceIdx >= static_cast<int32_t>( g_nav_faces.size() ) ) {
		Vector3DP feetStart = start;
		feetStart.z -= CROWD_SLOT_FEET_SNAP_OFFSET_Z;
		startFaceIdx = Nav_FindFaceInLeafStrict( feetStart );
	}
	if ( startFaceIdx >= 0 && startFaceIdx < static_cast<int32_t>( g_nav_faces.size() ) ) {
		outResult->hitFaceIndex = startFaceIdx;
		const int32_t rId = g_nav_faces[ startFaceIdx ].room_id;
		if ( rId >= 0 && rId < static_cast<int32_t>( g_nav_rooms.size() ) ) {
			outResult->zone_type = g_nav_rooms[ rId ].zone_type;
		}
	}

	// Phase 1: Direct half-edge polygon graph traversal from face to face:
	if ( startFaceIdx >= 0 && startFaceIdx < static_cast<int32_t>( g_nav_faces.size() ) ) {
		int32_t currentFaceIdx = startFaceIdx;
		int32_t prevHalfedgeIdx = -1;
		double distTraversed = 0.0;
		Vector3DP currentPos = start;
		const int32_t currentRoomId = g_nav_faces[ startFaceIdx ].room_id;

		//! Maximum topological face hops permitted per raycast to bound execution time in O(1).
		static constexpr int32_t MAX_RAYCAST_FACE_HOPS = 32;

		for ( int32_t hop = 0; hop < MAX_RAYCAST_FACE_HOPS && distTraversed < maxDistance; hop++ ) {
			const nav_face_t &face = g_nav_faces[ currentFaceIdx ];
			int32_t bestExitEdge = -1;
			double bestExitT = 999999.0;
			Vector3DP bestHitPoint = {};
			Vector3DP bestHitNormal = {};

			int32_t curEdge = face.first_edge_idx;
			for ( int32_t e = 0; e < face.num_edges; e++ ) {
				if ( curEdge < 0 || curEdge >= static_cast<int32_t>( g_nav_halfedges.size() ) ) {
					break;
				}
				if ( curEdge == prevHalfedgeIdx ) {
					curEdge = g_nav_halfedges[ curEdge ].next_idx;
					continue;
				}

				const nav_halfedge_t &he = g_nav_halfedges[ curEdge ];
				const Vector3DP &v0 = g_nav_vertices[ he.vertex_idx ];
				const int32_t nextHeIdx = he.next_idx;
				const Vector3DP &v1 = ( nextHeIdx >= 0 && nextHeIdx < static_cast<int32_t>( g_nav_halfedges.size() ) ) ? g_nav_vertices[ g_nav_halfedges[ nextHeIdx ].vertex_idx ] : v0;

				Vector3DP edgeVec = v1 - v0;
				edgeVec.z = 0.0;
				const double cross = ( rayDir.x * edgeVec.y ) - ( rayDir.y * edgeVec.x );

				if ( std::fabs( cross ) > 0.00001 ) {
					Vector3DP startToV0 = v0 - currentPos;
					startToV0.z = 0.0;
					const double tRay = ( ( startToV0.x * edgeVec.y ) - ( startToV0.y * edgeVec.x ) ) / cross;
					const double tEdge = ( ( startToV0.x * rayDir.y ) - ( startToV0.y * rayDir.x ) ) / cross;

					if ( tRay >= 0.001 && tRay <= ( maxDistance - distTraversed ) && tEdge >= -0.001 && tEdge <= 1.001 ) {
						if ( tRay < bestExitT ) {
							bestExitT = tRay;
							bestExitEdge = curEdge;
							bestHitPoint = currentPos + ( rayDir * tRay );
							bestHitNormal = QM_Vector3NormalizeDP( Vector3DP{ -edgeVec.y, edgeVec.x, 0.0 } );
						}
					}
				}
				curEdge = he.next_idx;
			}

			// If no exit edge was intersected on this face, the ray terminates within the face bounds:
			if ( bestExitEdge < 0 ) {
				break;
			}

			const nav_halfedge_t &exitHe = g_nav_halfedges[ bestExitEdge ];
			const double totalHitDist = distTraversed + bestExitT;

			// 1. Check if exit edge is a solid boundary wall (twin_idx < 0):
			if ( exitHe.twin_idx < 0 ) {
				const double contactDist = std::max( 0.0, totalHitDist - effectiveRadius );
				outResult->hitSolidWall = true;
				outResult->hitDistance = contactDist;
				outResult->hitPoint = start + ( rayDir * contactDist );
				outResult->hitNormal = bestHitNormal;
				outResult->hitFaceIndex = currentFaceIdx;
				outResult->hitHalfedgeIndex = bestExitEdge;
				return true;
			}

			// 2. Check if exit edge is a vertical step-up obstacle (> step height):
			if ( exitHe.z_diff > NAV_MAX_STEP_HEIGHT ) {
				const double contactDist = std::max( 0.0, totalHitDist - effectiveRadius );
				outResult->hitSolidWall = true;
				outResult->hitDistance = contactDist;
				outResult->hitPoint = start + ( rayDir * contactDist );
				outResult->hitNormal = bestHitNormal;
				outResult->hitFaceIndex = currentFaceIdx;
				outResult->hitHalfedgeIndex = bestExitEdge;
				return true;
			}

			// 3. Check if exit edge is a vertical drop-off ledge (<-step height):
			if ( exitHe.z_diff < -NAV_MAX_STEP_HEIGHT ) {
				const double contactDist = std::max( 0.0, totalHitDist - effectiveRadius );
				outResult->hitDropoff = true;
				outResult->hitDistance = contactDist;
				outResult->hitPoint = start + ( rayDir * contactDist );
				outResult->hitNormal = bestHitNormal;
				outResult->hitFaceIndex = currentFaceIdx;
				outResult->hitHalfedgeIndex = bestExitEdge;
				return true;
			}

			// 4. Check if exit edge is a transition portal (doorway / dynamic entity / inter-zone boundary):
			const nav_halfedge_t &twinHe = g_nav_halfedges[ exitHe.twin_idx ];
			const int32_t neighborFaceIdx = twinHe.face_idx;
			const int32_t neighborRoomId = ( neighborFaceIdx >= 0 && neighborFaceIdx < static_cast<int32_t>( g_nav_faces.size() ) ) ? g_nav_faces[ neighborFaceIdx ].room_id : -1;

			if ( exitHe.edge_entity_id != ENTITYNUM_NONE || ( currentRoomId >= 0 && neighborRoomId >= 0 && currentRoomId != neighborRoomId ) ) {
				outResult->hitPortal = true;
				outResult->hitDistance = totalHitDist;
				outResult->hitPoint = bestHitPoint;
				outResult->hitNormal = bestHitNormal;
				outResult->hitFaceIndex = currentFaceIdx;
				outResult->hitHalfedgeIndex = bestExitEdge;
				outResult->targetRoomId = neighborRoomId;
				outResult->portalEntityNumber = exitHe.edge_entity_id;

				if ( exitHe.edge_entity_id != ENTITYNUM_NONE ) {
					const svg_base_edict_t *ent = g_edict_pool.EdictForNumber( exitHe.edge_entity_id );
					if ( ent != nullptr ) {
						if ( ent->GetTypeInfo()->IsSubClassType<svg_func_wall_t>() ) {
							outResult->portal_type = PORTAL_TYPE_FUNC_WALL;
						} else if ( ent->GetTypeInfo()->IsSubClassType<svg_func_door_t>() || ent->GetTypeInfo()->IsSubClassType<svg_func_door_rotating_t>() ) {
							outResult->portal_type = PORTAL_TYPE_DOOR_ENTITY;
						} else if ( ent->GetTypeInfo()->IsSubClassType<svg_func_plat_t>() ) {
							outResult->portal_type = PORTAL_TYPE_ELEVATOR_PLAT;
						}
					}
				}
				return true;
			}

			// 5. Check if aperture width across this twin edge is narrower than agent cylinder:
			if ( effectiveRadius > 0.0 && static_cast<size_t>( bestExitEdge ) < s_nav_edge_portal_widths.size() ) {
				const double portalWidth = s_nav_edge_portal_widths[ bestExitEdge ];
				if ( portalWidth > 0.0 && portalWidth < ( effectiveRadius * 2.0 ) ) {
					const double contactDist = std::max( 0.0, totalHitDist - effectiveRadius );
					outResult->hitSolidWall = true;
					outResult->hitDistance = contactDist;
					outResult->hitPoint = start + ( rayDir * contactDist );
					outResult->hitNormal = bestHitNormal;
					outResult->hitFaceIndex = currentFaceIdx;
					outResult->hitHalfedgeIndex = bestExitEdge;
					return true;
				}
			}

			// Advance to neighbor face:
			currentFaceIdx = neighborFaceIdx;
			prevHalfedgeIdx = exitHe.twin_idx;
			distTraversed = totalHitDist;
			currentPos = bestHitPoint;
		}

		// Phase 1 successfully traversed the topological half-edge mesh to maxDistance or terminated within open face:
		return false;
	}

	// Phase 2: Fallback 2D spatial grid boundary intersection for off-mesh queries (startFaceIdx < 0):
	const Vector3DP rayEnd = start + ( rayDir * maxDistance );
	const double minZ = std::min( start.z, rayEnd.z ) - NAV_MAX_STEP_HEIGHT;
	const double maxZ = std::max( start.z, rayEnd.z ) + NAV_MAX_STEP_HEIGHT;

	const double segMinX = std::min( start.x, rayEnd.x ) - effectiveRadius;
	const double segMaxX = std::max( start.x, rayEnd.x ) + effectiveRadius;
	const double segMinY = std::min( start.y, rayEnd.y ) - effectiveRadius;
	const double segMaxY = std::max( start.y, rayEnd.y ) + effectiveRadius;

	const int32_t minCX = static_cast<int32_t>( std::floor( segMinX * NAV_SPATIAL_GRID_INV_CELL_SIZE ) );
	const int32_t maxCX = static_cast<int32_t>( std::floor( segMaxX * NAV_SPATIAL_GRID_INV_CELL_SIZE ) );
	const int32_t minCY = static_cast<int32_t>( std::floor( segMinY * NAV_SPATIAL_GRID_INV_CELL_SIZE ) );
	const int32_t maxCY = static_cast<int32_t>( std::floor( segMaxY * NAV_SPATIAL_GRID_INV_CELL_SIZE ) );

	const uint32_t queryToken = ++s_nav_spatial_query_counter;
	double closestHitDist = outResult->hitDistance;
	bool hitFound = false;

	for ( int32_t cy = minCY; cy <= maxCY; ++cy ) {
		for ( int32_t cx = minCX; cx <= maxCX; ++cx ) {
			const auto it = s_nav_spatial_grid_edges.find( Nav_SpatialGridKey( cx, cy ) );
			if ( it == s_nav_spatial_grid_edges.end() ) {
				continue;
			}

			for ( const int32_t edgeIdx : it->second ) {
				if ( s_nav_cached_edge_query_tokens[ edgeIdx ] == queryToken ) {
					continue;
				}
				s_nav_cached_edge_query_tokens[ edgeIdx ] = queryToken;

				const auto &edge = s_nav_cached_solid_edges[ edgeIdx ];
				const double edgeMinZ = std::min( edge.first.z, edge.second.z );
				const double edgeMaxZ = std::max( edge.first.z, edge.second.z );
				if ( edgeMaxZ < minZ || edgeMinZ > maxZ ) {
					continue;
				}

				double hitT = 0.0;
				Vector3DP hitNorm = {};
				if ( Nav_CircleIntersectSegment2D( start, rayDir, effectiveRadius, edge.first, edge.second, &hitT, &hitNorm ) ) {
					if ( hitT >= 0.0 && hitT < closestHitDist ) {
						closestHitDist = hitT;
						outResult->hitSolidWall = true;
						outResult->hitDistance = hitT;
						outResult->hitPoint = start + ( rayDir * hitT );
						outResult->hitNormal = hitNorm;
						hitFound = true;
					}
				}
			}
		}
	}

	return hitFound || outResult->hitSolidWall || outResult->hitPortal || outResult->hitDropoff;
}

/**
*	@brief	Perform a high-performance 2D raycast across the navigation mesh half-edge graph.
*	@param	start			Ray origin in double-precision world coordinates.
*	@param	dirNorm			Normalized 2D ray direction vector (dirNorm.z = 0.0).
*	@param	maxDistance		Maximum ray traversal distance in world units.
*	@param	outResult		[out] Pointer to result struct storing hit distance, normal, zone type, and portal metadata.
*	@return	True if an intersection occurred within maxDistance.
**/
bool Nav_RaycastHalfEdge2D( const Vector3DP &start, const Vector3DP &dirNorm, const double maxDistance, nav_raycast_result_t *outResult ) {
	return Nav_CircleCastHalfEdge2D( start, dirNorm, maxDistance, 0.0, outResult );
}

/**
*	@brief	Omnidirectional 2D room profile scanner utilizing half-edge raycasting.
*	@param	anchor			Room center anchor origin in double-precision world coordinates.
*	@param	maxRadius		Maximum search radius for boundary walls in world units.
*	@param	numRays			Number of radial angular probes (e.g. 16 or 24).
*	@param	outDoorYaw		[out] Discovered primary doorway orientation yaw in degrees.
*	@param	outHasDoor		[out] True if a valid doorway portal / aperture was identified.
*	@param	outMinWallDist	[out] Minimum solid perimeter wall distance in world units.
*	@param	outAvgWallDist	[out] Average solid perimeter wall distance in world units.
*	@param	outZoneType		[out] Topological zone type of the room/zone.
*	@param	agentRadius		Optional agent collision radius for hull-swept boundary clearance.
**/
void Nav_RaycastRoomBoundary2D( const Vector3DP &anchor, const double maxRadius, const int32_t numRays, double *outDoorYaw, bool *outHasDoor, double *outMinWallDist, double *outAvgWallDist, nav_zone_type_t *outZoneType, const double agentRadius ) {
	const int32_t rayCount = std::clamp( numRays, 8, 64 );
	double minWall = maxRadius;
	double wallSum = 0.0;
	int32_t wallCount = 0;
	double maxRayDist = 0.0;
	double doorAngleDeg = 0.0;
	bool foundPortal = false;
	nav_zone_type_t detectedZone = ZONE_TYPE_OPEN_SPACE;

	for ( int32_t i = 0; i < rayCount; i++ ) {
		const double angle = static_cast<double>( i ) * ( ( 2.0 * QM_PI ) / static_cast<double>( rayCount ) );
		const Vector3DP dir{ std::cos( angle ), std::sin( angle ), 0.0 };

		nav_raycast_result_t res = {};
		Nav_CircleCastHalfEdge2D( anchor, dir, maxRadius, agentRadius, &res );

		if ( res.zone_type != ZONE_TYPE_OPEN_SPACE ) {
			detectedZone = res.zone_type;
		}

		if ( res.hitSolidWall ) {
			minWall = std::min( minWall, res.hitDistance );
			wallSum += res.hitDistance;
			wallCount++;
		} else if ( res.hitPortal ) {
			foundPortal = true;
			doorAngleDeg = angle * ( 180.0 / QM_PI );
		}

		if ( res.hitDistance > maxRayDist ) {
			maxRayDist = res.hitDistance;
			if ( !foundPortal ) {
				doorAngleDeg = angle * ( 180.0 / QM_PI );
			}
		}
	}

	const double avgWall = ( wallCount > 0 ) ? ( wallSum / static_cast<double>( wallCount ) ) : minWall;
	const bool hasAperture = foundPortal || ( wallCount >= 4 && maxRayDist > ( avgWall * 1.35 ) );

	if ( outDoorYaw ) *outDoorYaw = doorAngleDeg;
	if ( outHasDoor ) *outHasDoor = hasAperture;
	if ( outMinWallDist ) *outMinWallDist = minWall;
	if ( outAvgWallDist ) *outAvgWallDist = avgWall;
	if ( outZoneType ) *outZoneType = detectedZone;
}

/**
*	@brief	Test if a 2D segment has unobstructed geometric line-of-sight through the navmesh
*			without intersecting or penetrating any solid boundary obstacle edges.
*	@param	p0					Segment start position in double precision.
*	@param	p1					Segment end position in double precision.
*	@param	clearanceMargin		Required clearance distance from obstacle edges (agentRadius + margin).
*	@return	True if segment maintains clearance from all solid boundary edges.
**/
bool Nav_HasGeometricLineOfSight2D( const Vector3DP &p0, const Vector3DP &p1, const double clearanceMargin ) {
	Nav_EnsureObstacleCornersTable();

	Vector3DP segDir = p1 - p0;
	segDir.z = 0.0;
	const double segLen = QM_Vector3LengthDP( segDir );
	if ( segLen <= 0.001 ) {
		return true;
	}
	const Vector3DP segDirNorm = segDir * ( 1.0 / segLen );

	const double minZ = std::min( p0.z, p1.z ) - NAV_MAX_STEP_HEIGHT;
	const double maxZ = std::max( p0.z, p1.z ) + NAV_MAX_STEP_HEIGHT;
	const double clearanceSqr = clearanceMargin * clearanceMargin;

	const double segMinX = std::min( p0.x, p1.x ) - clearanceMargin;
	const double segMaxX = std::max( p0.x, p1.x ) + clearanceMargin;
	const double segMinY = std::min( p0.y, p1.y ) - clearanceMargin;
	const double segMaxY = std::max( p0.y, p1.y ) + clearanceMargin;

	const int32_t minCX = static_cast<int32_t>( std::floor( segMinX * NAV_SPATIAL_GRID_INV_CELL_SIZE ) );
	const int32_t maxCX = static_cast<int32_t>( std::floor( segMaxX * NAV_SPATIAL_GRID_INV_CELL_SIZE ) );
	const int32_t minCY = static_cast<int32_t>( std::floor( segMinY * NAV_SPATIAL_GRID_INV_CELL_SIZE ) );
	const int32_t maxCY = static_cast<int32_t>( std::floor( segMaxY * NAV_SPATIAL_GRID_INV_CELL_SIZE ) );

	const uint32_t queryToken = ++s_nav_spatial_query_counter;

	for ( int32_t cy = minCY; cy <= maxCY; ++cy ) {
		for ( int32_t cx = minCX; cx <= maxCX; ++cx ) {
			const auto it = s_nav_spatial_grid_edges.find( Nav_SpatialGridKey( cx, cy ) );
			if ( it == s_nav_spatial_grid_edges.end() ) {
				continue;
			}

			for ( const int32_t edgeIdx : it->second ) {
				if ( s_nav_cached_edge_query_tokens[ edgeIdx ] == queryToken ) {
					continue;
				}
				s_nav_cached_edge_query_tokens[ edgeIdx ] = queryToken;

				const auto &edge = s_nav_cached_solid_edges[ edgeIdx ];
				const double edgeMinZ = std::min( edge.first.z, edge.second.z );
				const double edgeMaxZ = std::max( edge.first.z, edge.second.z );
				if ( edgeMaxZ < minZ || edgeMinZ > maxZ ) {
					continue;
				}

				// 1. Direct 2D segment intersection test (exact cross-product orientation)
				if ( Nav_SegmentsIntersect2D( p0, p1, edge.first, edge.second ) ) {
					return false;
				}

				// 2. Clearance distance test against intermediate parts of the segment:
				if ( clearanceMargin > 0.0 ) {
					const double distSqr = Nav_DistanceSegmentToSegment2DSqr( p0, p1, edge.first, edge.second );
					if ( distSqr < clearanceSqr ) {
						// Only reject if the violation occurs along the travel segment (not merely at the endpoints
						// where the agent may start or end while standing close to a wall).
						Vector3DP toE1 = edge.first - p0;
						toE1.z = 0.0;
						const double t1 = ( toE1.x * segDirNorm.x + toE1.y * segDirNorm.y ) / segLen;

						Vector3DP toE2 = edge.second - p0;
						toE2.z = 0.0;
						const double t2 = ( toE2.x * segDirNorm.x + toE2.y * segDirNorm.y ) / segLen;

						if ( ( t1 > NAV_CORNER_SEGMENT_MIN_T && t1 < NAV_CORNER_SEGMENT_MAX_T ) || ( t2 > NAV_CORNER_SEGMENT_MIN_T && t2 < NAV_CORNER_SEGMENT_MAX_T ) ) {
							return false;
						}
					}
				}
			}
		}
	}

	return true;
}

/**
*	@brief	Clip a shared portal to the agent-clearance corridor of both adjacent faces in double precision.
*	@param	faceAIdx			Index of first adjacent face.
*	@param	faceBIdx			Index of second adjacent face.
*	@param	portalLeft			Input left portal endpoint relative to traversal.
*	@param	portalRight			Input right portal endpoint relative to traversal.
*	@param	wallClearance		Required clearance distance from solid straight walls.
*	@param	cornerClearance		Required clearance distance from convex obstacle corners (arc radius).
*	@param	outLeft				[out] Output receiving the clipped left endpoint.
*	@param	outRight			[out] Output receiving the clipped right endpoint.
*	@param	outIsNarrowPortal	[out,optional] Output set to true if portal was clamped to midpoint due to narrowness.
*	@return	True if a valid clipped portal segment was produced.
**/
//! Persistent static buffer for local obstacle corner queries during portal endpoint clipping.
static std::vector<const nav_cached_corner_t *> s_nav_clip_local_corners;

static bool Nav_ClipPortalForAgentClearance( const int32_t faceAIdx, const int32_t faceBIdx, const Vector3DP &portalLeft, const Vector3DP &portalRight, const double wallClearance, const double cornerClearance, Vector3DP *outLeft, Vector3DP *outRight, bool *outIsNarrowPortal ) {
	/**
	*	Sanity checks on inputs.
	**/
	if ( outLeft == nullptr || outRight == nullptr || wallClearance < 0.0 || cornerClearance < 0.0 || faceAIdx < 0 || faceBIdx < 0 ||
		static_cast<size_t>( faceAIdx ) >= g_nav_faces.size() || static_cast<size_t>( faceBIdx ) >= g_nav_faces.size() ) {
		return false;
	}

	if ( outIsNarrowPortal != nullptr ) {
		*outIsNarrowPortal = false;
	}

	/**
	*	Compute 2D portal length and direction vector pointing from right to left:
	*	P(t) = portalRight + portalDirection * t, where t in [0, portalLength].
	**/
	Vector3DP portalDirection = portalLeft - portalRight;
	portalDirection.z = 0.0;
	const double portalLength = QM_Vector3LengthDP( portalDirection );
	if ( portalLength <= 0.001 ) {
		return false;
	}
	portalDirection = portalDirection * ( 1.0 / portalLength );

	const double portalZ = ( portalLeft.z + portalRight.z ) * 0.5;

	// Parametric clipping interval along the portal: t in [minT, maxT]
	double minT = 0.0;
	double maxT = portalLength;

	// Ensure precomputed obstacle corners and solid edges table is populated
	Nav_EnsureObstacleCornersTable();

	const double cornerClearanceSqr = cornerClearance * cornerClearance;

	/**
	*	1. Exact Circular Clearance Arc Insetting for Convex Obstacle Corners:
	*	Retrieve nearby obstacle corners in O(1) expected time via the 2D spatial hash grid.
	*	If a portal endpoint falls within the clearance disk of radius `cornerClearance` centered at C,
	*	analytically solve the ray-circle exit intersection and advance the portal endpoint onto the clearance arc.
	**/
	const double boxMinX = std::min( portalRight.x, portalLeft.x ) - cornerClearance;
	const double boxMaxX = std::max( portalRight.x, portalLeft.x ) + cornerClearance;
	const double boxMinY = std::min( portalRight.y, portalLeft.y ) - cornerClearance;
	const double boxMaxY = std::max( portalRight.y, portalLeft.y ) + cornerClearance;

	const int32_t minCX = static_cast<int32_t>( std::floor( boxMinX * NAV_SPATIAL_GRID_INV_CELL_SIZE ) );
	const int32_t maxCX = static_cast<int32_t>( std::floor( boxMaxX * NAV_SPATIAL_GRID_INV_CELL_SIZE ) );
	const int32_t minCY = static_cast<int32_t>( std::floor( boxMinY * NAV_SPATIAL_GRID_INV_CELL_SIZE ) );
	const int32_t maxCY = static_cast<int32_t>( std::floor( boxMaxY * NAV_SPATIAL_GRID_INV_CELL_SIZE ) );

	const uint32_t cornerToken = ++s_nav_spatial_corner_counter;
	s_nav_clip_local_corners.clear();
	if ( s_nav_clip_local_corners.capacity() < 32 ) {
		s_nav_clip_local_corners.reserve( 32 );
	}

	for ( int32_t cy = minCY; cy <= maxCY; ++cy ) {
		for ( int32_t cx = minCX; cx <= maxCX; ++cx ) {
			const auto it = s_nav_spatial_grid_corners.find( Nav_SpatialGridKey( cx, cy ) );
			if ( it == s_nav_spatial_grid_corners.end() ) {
				continue;
			}
			for ( const int32_t cIdx : it->second ) {
				if ( s_nav_cached_corner_query_tokens[ cIdx ] == cornerToken ) {
					continue;
				}
				s_nav_cached_corner_query_tokens[ cIdx ] = cornerToken;
				s_nav_clip_local_corners.push_back( &s_nav_cached_corners[ cIdx ] );
			}
		}
	}

	for ( const auto *cornerPtr : s_nav_clip_local_corners ) {
		const auto &c = *cornerPtr;
		// Check vertical proximity within step height
		if ( std::fabs( c.vertex.z - portalZ ) > ( NAV_MAX_STEP_HEIGHT + NAV_STEP_HEIGHT_PADDING ) ) {
			continue;
		}

		if ( c.vertex.x < boxMinX || c.vertex.x > boxMaxX || c.vertex.y < boxMinY || c.vertex.y > boxMaxY ) {
			continue;
		}

		// Vector from corner to portalRight (t = 0)
		Vector3DP deltaR = portalRight - c.vertex;
		deltaR.z = 0.0;
		const double distRSqr = QM_Vector3LengthSqrDP( deltaR );

		// If portalRight is inside the forbidden clearance circle around corner C:
		if ( distRSqr < cornerClearanceSqr ) {
			// Solve t^2 + 2*(deltaR . u)*t + (distRSqr - cornerClearanceSqr) = 0
			// The ray starts inside the circle, so the positive root gives the exit point along the portal.
			const double b = QM_Vector3DotProductDP( deltaR, portalDirection );
			const double disc = b * b + cornerClearanceSqr - distRSqr;
			if ( disc >= 0.0 ) {
				const double tExit = -b + std::sqrt( disc );
				minT = std::max( minT, tExit );
			}
		}

		// Vector from corner to portalLeft (t = portalLength)
		Vector3DP deltaL = portalLeft - c.vertex;
		deltaL.z = 0.0;
		const double distLSqr = QM_Vector3LengthSqrDP( deltaL );

		// If portalLeft is inside the forbidden clearance circle around corner C:
		if ( distLSqr < cornerClearanceSqr ) {
			// Ray going backwards from portalLeft (-portalDirection):
			// Solve s^2 + 2*(deltaL . (-u))*s + (distLSqr - cornerClearanceSqr) = 0
			const double bL = QM_Vector3DotProductDP( deltaL, portalDirection * -1.0 );
			const double discL = bL * bL + cornerClearanceSqr - distLSqr;
			if ( discL >= 0.0 ) {
				const double sExit = -bL + std::sqrt( discL );
				maxT = std::min( maxT, portalLength - sExit );
			}
		}
	}

	/**
	*	2. Perpendicular Wall Clearance Insetting for Solid Boundary Edges:
	*	Check solid boundary edges belonging to faceA, faceB, and cached obstacle edges.
	*	If an endpoint is within clearance of a solid wall line, advance it along the portal
	*	direction until its perpendicular distance into open space equals the required clearance.
	**/
	const double wallClearanceSqr = wallClearance * wallClearance;

	auto CheckSolidEdgeClearance = [&]( const Vector3DP &v0, const Vector3DP &v1, const Vector3DP &inwardNorm ) {
		const double edgeMinZ = std::min( v0.z, v1.z ) - ( NAV_MAX_STEP_HEIGHT + NAV_STEP_HEIGHT_PADDING );
		const double edgeMaxZ = std::max( v0.z, v1.z ) + ( NAV_MAX_STEP_HEIGHT + NAV_STEP_HEIGHT_PADDING );
		if ( portalZ < edgeMinZ || portalZ > edgeMaxZ ) {
			return;
		}

		// Test portalRight proximity to edge
		const double distR2DSqr = Nav_DistancePointToSegment2DSqr( portalRight, v0, v1 );
		if ( distR2DSqr < wallClearanceSqr ) {
			Vector3DP toR = portalRight - v0;
			toR.z = 0.0;
			const double perpDistR = QM_Vector3DotProductDP( toR, inwardNorm );
			if ( perpDistR < wallClearance ) {
				const double projR = QM_Vector3DotProductDP( portalDirection, inwardNorm );
				if ( projR > NAV_PORTAL_MIN_NORMAL_PROJECTION ) {
					const double tNeed = ( wallClearance - perpDistR ) / projR;
					minT = std::max( minT, tNeed );
				} else {
					minT = std::max( minT, wallClearance );
				}
			}
		}

		// Test portalLeft proximity to edge
		const double distL2DSqr = Nav_DistancePointToSegment2DSqr( portalLeft, v0, v1 );
		if ( distL2DSqr < wallClearanceSqr ) {
			Vector3DP toL = portalLeft - v0;
			toL.z = 0.0;
			const double perpDistL = QM_Vector3DotProductDP( toL, inwardNorm );
			if ( perpDistL < wallClearance ) {
				const double projL = QM_Vector3DotProductDP( portalDirection * -1.0, inwardNorm );
				if ( projL > NAV_PORTAL_MIN_NORMAL_PROJECTION ) {
					const double sNeed = ( wallClearance - perpDistL ) / projL;
					maxT = std::min( maxT, portalLength - sNeed );
				} else {
					maxT = std::min( maxT, portalLength - wallClearance );
				}
			}
		}
	};

	// Check solid boundary edges of faceA and faceB:
	for ( const int32_t fIdx : { faceAIdx, faceBIdx } ) {
		const nav_face_t &face = g_nav_faces[ fIdx ];
		for ( int32_t e = 0; e < face.num_edges; ++e ) {
			const nav_halfedge_t &he = g_nav_halfedges[ face.first_edge_idx + e ];
			if ( he.twin_idx != -1 ) {
				const int32_t neighborFace = g_nav_halfedges[ he.twin_idx ].face_idx;
				if ( neighborFace == faceAIdx || neighborFace == faceBIdx ) {
					continue; // Skip the portal edge connecting faceA and faceB
				}
			}

			const bool isSolid = ( he.twin_idx == -1 || ( he.flags & NAV_EDGE_DISABLED ) != 0 || std::fabs( he.z_diff ) > NAV_SOLID_EDGE_MIN_Z_DIFF );
			if ( isSolid ) {
				const Vector3DP &v0 = g_nav_vertices[ he.vertex_idx ];
				const Vector3DP &v1 = g_nav_vertices[ g_nav_halfedges[ he.next_idx ].vertex_idx ];
				Vector3DP d = v1 - v0;
				d.z = 0.0;
				const double len = QM_Vector3LengthDP( d );
				if ( len > 0.001 ) {
					const Vector3DP u = d * ( 1.0 / len );
					// In a CCW winding face, the 2D left-normal of edge direction u points strictly into the face interior.
					Vector3DP inNorm{ -u.y, u.x, 0.0 };
					CheckSolidEdgeClearance( v0, v1, inNorm );
				}
			}
		}
	}

	/**
	*	3. Commit clipped endpoints or collapse narrow portals to centered midpoint:
	**/
	const bool rightConstrained = ( minT > 0.001 );
	const bool leftConstrained = ( maxT < ( portalLength - 0.001 ) );

	if ( minT >= maxT ) {
		if ( rightConstrained && leftConstrained ) {
			// Constricted passage between two distinct opposing obstacle boundaries (true doorway / narrow opening):
			// center the portal on its midpoint between the opposing obstacles and mark as a forced narrow portal.
			const double midT = portalLength * 0.5;
			*outRight = portalRight + portalDirection * midT;
			*outLeft = *outRight;
			if ( outIsNarrowPortal != nullptr ) {
				*outIsNarrowPortal = true;
			}
		} else if ( rightConstrained ) {
			// Only the right side is constrained by an obstacle/corner; the left side opens into open terrain.
			// Clamp to the open left end of the portal to maximize obstacle standoff, and do NOT force a doorway waypoint.
			*outRight = portalRight + portalDirection * portalLength;
			*outLeft = *outRight;
			if ( outIsNarrowPortal != nullptr ) {
				*outIsNarrowPortal = false;
			}
		} else if ( leftConstrained ) {
			// Only the left side is constrained by an obstacle/corner; the right side opens into open terrain.
			// Clamp to the open right end of the portal to maximize obstacle standoff, and do NOT force a doorway waypoint.
			*outRight = portalRight;
			*outLeft = *outRight;
			if ( outIsNarrowPortal != nullptr ) {
				*outIsNarrowPortal = false;
			}
		} else {
			// Unconstrained degenerate portal: default to midpoint without forcing a doorway constraint.
			const double midT = portalLength * 0.5;
			*outRight = portalRight + portalDirection * midT;
			*outLeft = *outRight;
			if ( outIsNarrowPortal != nullptr ) {
				*outIsNarrowPortal = false;
			}
		}
		return true;
	}

	*outRight = portalRight + portalDirection * minT;
	*outLeft = portalRight + portalDirection * maxT;
	return true;
}


/**
*	@brief	Find closest nav face in the current BSP leaf with fallback to global KD-tree.
*	@param	point	Query position in feet-origin space.
*	@return	Index of closest nav face or -1.
**/
int32_t Nav_FindClosestFaceInLeaf( const Vector3DP &point ) {
	/**
	*	Prefer local KD-tree face lookup for stable corner progression.
	**/
	const int32_t leafFace = Nav_FindPolyInLeaf( point );
	if ( leafFace >= 0 && static_cast< size_t >( leafFace ) < g_nav_faces.size() ) {
		return leafFace;
	}

	/**
	*	Fallback to global closest poly lookup only if leaf search completely failed.
	**/
	return Nav_FindClosestPolyGlobal( point );
}

/**
*	@brief	Get the connected component partition ID of a navigation face.
*	@param	faceIdx	Index of the nav face.
*	@return	Component identifier, or -1 if invalid.
**/
int32_t Nav_GetFaceComponent( const int32_t faceIdx ) {
	if ( faceIdx >= 0 && static_cast<size_t>( faceIdx ) < s_nav_face_components.size() ) {
		return s_nav_face_components[ faceIdx ];
	}
	return -1;
}

/**
*	@brief	Find the closest nav face in the local KD-leaf that shares the connected component of targetFace.
*	@details	If the face directly under point is an isolated sliver or disconnected polygon, searches
*				candidate faces in the leaf (and neighboring connected faces) that overlap the agent's
*				capsule footprint, preventing agents from becoming stranded on degenerate boundary slivers.
*	@param	point		Query position in feet-origin space.
*	@param	targetFace	Target/goal navigation face to match component reachability against.
*	@param	agentRadius	Physical hull radius of the agent (mins/maxs half-width).
*	@return	Index of closest reachable nav face, or standard closest face if no component match found.
**/
int32_t Nav_FindReachableFaceInLeaf( const Vector3DP &point, const int32_t targetFace, const double agentRadius ) {
	const int32_t primaryFace = Nav_FindClosestFaceInLeaf( point );
	if ( primaryFace < 0 || targetFace < 0 ||
		 static_cast<size_t>( primaryFace ) >= s_nav_face_components.size() ||
		 static_cast<size_t>( targetFace ) >= s_nav_face_components.size() ) {
		return primaryFace;
	}

	const int32_t targetComp = s_nav_face_components[ targetFace ];
	if ( targetComp >= 0 && s_nav_face_components[ primaryFace ] == targetComp ) {
		return primaryFace;
	}

	// Primary face is either disconnected or on a different component (e.g. an isolated sliver polygon).
	// Search candidate faces in the local KD-tree leaf that are reachable to targetComp and overlap the agent's hull.
	const int32_t leafIdx = Nav_FindLeafNode( point );
	if ( leafIdx >= 0 && leafIdx < static_cast<int32_t>( g_nav_nodes.size() ) ) {
		const nav_kdtree_node_t &leaf = g_nav_nodes[ leafIdx ];
		const int32_t firstFaceIdx = leaf.first_face_id;
		const double maxFootprintRadius = ( agentRadius > 0.0 ) ? ( agentRadius * 2.0 ) : 32.0;
		const double maxFootprintDistSqr = maxFootprintRadius * maxFootprintRadius;

		int32_t bestReachableFace = -1;
		double bestDistSqr = 999999.0;

		for ( int32_t i = 0; i < leaf.num_faces; ++i ) {
			const int32_t candFaceIdx = firstFaceIdx + i;
			if ( candFaceIdx < 0 || static_cast<size_t>( candFaceIdx ) >= g_nav_faces.size() ) {
				break;
			}
			if ( static_cast<size_t>( candFaceIdx ) < s_nav_face_components.size() && s_nav_face_components[ candFaceIdx ] == targetComp ) {
				const nav_face_t &candFace = g_nav_faces[ candFaceIdx ];
				if ( ( candFace.surface_flags & ( CM_SURFACE_NO_NAVMESH | CONTENTS_NO_NAVMESH ) ) != 0 ) {
					continue;
				}
				if ( Nav_PointInsideFace2D( point, candFace ) ) {
					return candFaceIdx;
				}
				const double dx = point.x - candFace.center.x;
				const double dy = point.y - candFace.center.y;
				const double distSqr = dx * dx + dy * dy;
				if ( distSqr <= maxFootprintDistSqr && distSqr < bestDistSqr ) {
					bestDistSqr = distSqr;
					bestReachableFace = candFaceIdx;
				}
			}
		}

		if ( bestReachableFace != -1 ) {
			return bestReachableFace;
		}
	}

	// Secondary check: half-edge neighbors of primaryFace that belong to targetComp.
	if ( primaryFace >= 0 && static_cast<size_t>( primaryFace ) < g_nav_faces.size() ) {
		const nav_face_t &pf = g_nav_faces[ primaryFace ];
		for ( int32_t e = 0; e < pf.num_edges; ++e ) {
			const nav_halfedge_t &he = g_nav_halfedges[ pf.first_edge_idx + e ];
			if ( he.twin_idx != -1 ) {
				const int32_t nbrFace = g_nav_halfedges[ he.twin_idx ].face_idx;
				if ( nbrFace >= 0 && static_cast<size_t>( nbrFace ) < s_nav_face_components.size() ) {
					if ( s_nav_face_components[ nbrFace ] == targetComp ) {
						return nbrFace;
					}
				}
			}
		}
	}

	return primaryFace;
}

//! Corner information structure extracted along the path corridor for obstacle avoidance.
struct nav_corridor_corner_t {
	Vector3DP vertex;
	Vector3DP standoffMid;
	double standoffDist = 0.0;
	Vector3DP bisectorNorm;
	Vector3DP u1;
	Vector3DP u2;
	Vector3DP n1;
	Vector3DP n2;
	double minZ = 0.0;
	double maxZ = 0.0;
	uint32_t cornerType = NAV_EDGE_WALL;
};

//! Intersection record for an obstacle corner projected onto a path polyline segment.
struct nav_segment_corner_t {
	double t = 0.0;
	const nav_corridor_corner_t *corner = nullptr;
};

//! Persistent static buffer for gathering obstacle corners along a path corridor.
static std::vector<nav_corridor_corner_t> s_nav_corridor_corners;
//! Persistent static buffer for spatial grid corner queries along the corridor.
static std::vector<const nav_cached_corner_t *> s_nav_candidate_corners;
//! Persistent static buffer for refined path waypoints with obstacle standoff points.
static std::vector<Vector3DP> s_nav_refined_waypoints;
//! Persistent static buffer for tracking forced waypoint status across corner refinements.
static std::vector<bool> s_nav_refined_forced;
//! Persistent static buffer for corners intersecting a single path segment.
static std::vector<nav_segment_corner_t> s_nav_segment_corners;

/**
*	@brief	Enforce strict agent physical clearance around convex obstacle corners.
*	@param	path			[in] Sequence of face indices describing the path corridor.
*	@param	agentRadius		[in] Radius of the agent's collision hull (mins/maxs half-width).
*	@param	waypoints		[in,out] Waypoints list to enforce convex corner clearance for.
*	@param	forcedWaypoints	[in,out] Parallel boolean flags matching waypoints that are strictly forced.
*	@note	Queries the precomputed obstacle corners cache in O(1), inserting a 3-point standoff arc
*			around any convex corner vertex to prevent agents from colliding with solid obstacle edges.
**/
static void Nav_EnforceConvexCornerWaypoints( const std::vector<int32_t> &path, const double agentRadius, std::vector<Vector3DP> &waypoints, std::vector<bool> *forcedWaypoints ) {
	if ( waypoints.size() < 2 || path.empty() ) {
		return;
	}

	Nav_EnsureObstacleCornersTable();
	if ( s_nav_cached_corners.empty() ) {
		return;
	}

	// Base standoff clearance distance scaled dynamically by entity bounding box radius (mins/maxs half-width).
	// Uses standard entity default (NAV_DEFAULT_AGENT_RADIUS) plus NAV_CORNER_CLEARANCE_MARGIN (12.0) = 28.0 units when radius is unspecified.
	const double effectiveAgentRadius = ( agentRadius > 0.0 ) ? agentRadius : NAV_DEFAULT_AGENT_RADIUS;
	const double requiredClearance = effectiveAgentRadius + NAV_CORNER_CLEARANCE_MARGIN;

	// 1. Gather precomputed corners relevant to the path 3D bounding box using persistent static buffers:
	s_nav_corridor_corners.clear();
	if ( s_nav_corridor_corners.capacity() < 128 ) {
		s_nav_corridor_corners.reserve( 128 );
	}

	Vector3DP wpMin = waypoints.front();
	Vector3DP wpMax = waypoints.front();
	for ( const auto &wp : waypoints ) {
		wpMin.x = std::min( wpMin.x, wp.x );
		wpMin.y = std::min( wpMin.y, wp.y );
		wpMin.z = std::min( wpMin.z, wp.z );
		wpMax.x = std::max( wpMax.x, wp.x );
		wpMax.y = std::max( wpMax.y, wp.y );
		wpMax.z = std::max( wpMax.z, wp.z );
	}
	wpMin = wpMin - Vector3DP{ NAV_CORNER_SEARCH_PADDING_XY, NAV_CORNER_SEARCH_PADDING_XY, NAV_CORNER_SEARCH_PADDING_Z };
	wpMax = wpMax + Vector3DP{ NAV_CORNER_SEARCH_PADDING_XY, NAV_CORNER_SEARCH_PADDING_XY, NAV_CORNER_SEARCH_PADDING_Z };

	const int32_t minCX = static_cast<int32_t>( std::floor( wpMin.x * NAV_SPATIAL_GRID_INV_CELL_SIZE ) );
	const int32_t maxCX = static_cast<int32_t>( std::floor( wpMax.x * NAV_SPATIAL_GRID_INV_CELL_SIZE ) );
	const int32_t minCY = static_cast<int32_t>( std::floor( wpMin.y * NAV_SPATIAL_GRID_INV_CELL_SIZE ) );
	const int32_t maxCY = static_cast<int32_t>( std::floor( wpMax.y * NAV_SPATIAL_GRID_INV_CELL_SIZE ) );

	const uint32_t cornerToken = ++s_nav_spatial_corner_counter;
	s_nav_candidate_corners.clear();
	if ( s_nav_candidate_corners.capacity() < 128 ) {
		s_nav_candidate_corners.reserve( 128 );
	}

	for ( int32_t cy = minCY; cy <= maxCY; ++cy ) {
		for ( int32_t cx = minCX; cx <= maxCX; ++cx ) {
			const auto it = s_nav_spatial_grid_corners.find( Nav_SpatialGridKey( cx, cy ) );
			if ( it == s_nav_spatial_grid_corners.end() ) {
				continue;
			}
			for ( const int32_t cIdx : it->second ) {
				if ( s_nav_cached_corner_query_tokens[ cIdx ] == cornerToken ) {
					continue;
				}
				s_nav_cached_corner_query_tokens[ cIdx ] = cornerToken;
				s_nav_candidate_corners.push_back( &s_nav_cached_corners[ cIdx ] );
			}
		}
	}

	for ( const auto *cornerPtr : s_nav_candidate_corners ) {
		const auto &c = *cornerPtr;
		if ( c.vertex.x < wpMin.x || c.vertex.x > wpMax.x ||
			 c.vertex.y < wpMin.y || c.vertex.y > wpMax.y ||
			 c.maxZ < wpMin.z || c.minZ > wpMax.z ) {
			continue;
		}

		double standoffDist = std::min( requiredClearance * NAV_CORNER_MAX_BISECTOR_RATIO, requiredClearance / c.bisectorCosHalf );

		/**
		*	Check geometric clearance along bisectorNorm to detect narrow corridors:
		*	If the corner faces an opposite obstacle boundary within standoff range, adaptively center
		*	the standoff between the corner and the opposite obstacle to guarantee safe clearance on both sides.
		**/
		const double maxProbeDist = standoffDist * NAV_CORNER_BISECTOR_PROBE_RATIO;
		double minHitDist = maxProbeDist;

		const Vector3DP rayEnd = c.vertex + c.bisectorNorm * maxProbeDist;
		const int32_t rayMinCX = static_cast<int32_t>( std::floor( std::min( c.vertex.x, rayEnd.x ) * NAV_SPATIAL_GRID_INV_CELL_SIZE ) );
		const int32_t rayMaxCX = static_cast<int32_t>( std::floor( std::max( c.vertex.x, rayEnd.x ) * NAV_SPATIAL_GRID_INV_CELL_SIZE ) );
		const int32_t rayMinCY = static_cast<int32_t>( std::floor( std::min( c.vertex.y, rayEnd.y ) * NAV_SPATIAL_GRID_INV_CELL_SIZE ) );
		const int32_t rayMaxCY = static_cast<int32_t>( std::floor( std::max( c.vertex.y, rayEnd.y ) * NAV_SPATIAL_GRID_INV_CELL_SIZE ) );

		const uint32_t rayToken = ++s_nav_spatial_query_counter;
		for ( int32_t cy = rayMinCY; cy <= rayMaxCY; ++cy ) {
			for ( int32_t cx = rayMinCX; cx <= rayMaxCX; ++cx ) {
				const auto it = s_nav_spatial_grid_edges.find( Nav_SpatialGridKey( cx, cy ) );
				if ( it == s_nav_spatial_grid_edges.end() ) {
					continue;
				}
				for ( const int32_t edgeIdx : it->second ) {
					if ( s_nav_cached_edge_query_tokens[ edgeIdx ] == rayToken ) {
						continue;
					}
					s_nav_cached_edge_query_tokens[ edgeIdx ] = rayToken;

					const auto &edge = s_nav_cached_solid_edges[ edgeIdx ];
					const double edgeMinZ = std::min( edge.first.z, edge.second.z );
					const double edgeMaxZ = std::max( edge.first.z, edge.second.z );
					if ( edgeMaxZ < ( c.minZ - static_cast<double>( NAV_MAX_STEP_HEIGHT ) ) || edgeMinZ > ( c.maxZ + static_cast<double>( NAV_MAX_STEP_HEIGHT ) ) ) {
						continue;
					}
					double t = 0.0;
					if ( Nav_RayIntersectSegment2D( c.vertex, c.bisectorNorm, edge.first, edge.second, &t ) ) {
						if ( t > NAV_RAY_MIN_HIT_DISTANCE && t < minHitDist ) {
							minHitDist = t;
						}
					}
				}
			}
		}
		if ( minHitDist < ( standoffDist * NAV_CORNER_NARROW_CHANNEL_RATIO ) ) {
			const double minSafeDist = ( agentRadius > 0.0 ) ? ( agentRadius + NAV_SEGMENT_ENDPOINT_NUDGE ) : ( NAV_DEFAULT_AGENT_RADIUS + NAV_SEGMENT_ENDPOINT_NUDGE );
			standoffDist = std::clamp( minHitDist * NAV_STANDOFF_SCALE_HALF, minSafeDist, standoffDist );
		}

		Vector3DP standoffMid = c.vertex + c.bisectorNorm * standoffDist;
		standoffMid.z = c.vertex.z;

		// Ensure the standoff point actually lies on a valid walkable navigation mesh face within the local leaf:
		int32_t faceIdx = Nav_FindClosestFaceInLeaf( standoffMid );
		if ( faceIdx == -1 || !Nav_PointInsideFace2D( standoffMid, g_nav_faces[ faceIdx ] ) ) {
			// If standoff is slightly outside due to polygon boundary bevels or tight partitioning,
			// try a slightly contracted standoff distance (e.g. 0.75x or 0.5x) before giving up!
			bool foundFallback = false;
			for ( const double scale : { NAV_STANDOFF_SCALE_SEVENTY_FIVE, NAV_STANDOFF_SCALE_HALF } ) {
				const double testDist = standoffDist * scale;
				if ( testDist < ( ( agentRadius > 0.0 ) ? ( agentRadius * NAV_STANDOFF_SCALE_SEVENTY_FIVE ) : ( NAV_DEFAULT_AGENT_RADIUS * NAV_STANDOFF_SCALE_SEVENTY_FIVE ) ) ) {
					break;
				}
				const Vector3DP testMid = c.vertex + c.bisectorNorm * testDist;
				const int32_t testFaceIdx = Nav_FindClosestFaceInLeaf( testMid );
				if ( testFaceIdx != -1 && Nav_PointInsideFace2D( testMid, g_nav_faces[ testFaceIdx ] ) ) {
					standoffDist = testDist;
					standoffMid = testMid;
					faceIdx = testFaceIdx;
					foundFallback = true;
					break;
				}
			}
			if ( !foundFallback ) {
				continue; // Standoff is outside the walkable navigation mesh (e.g. over a cliff or inside a wall)
			}
		}

		// Project standoffMid.z precisely onto the walkable surface plane (essential for sloped/inclined ramps):
		if ( faceIdx >= 0 && static_cast<size_t>( faceIdx ) < g_nav_faces.size() ) {
			const nav_face_t &face = g_nav_faces[ faceIdx ];
			if ( std::fabs( face.normal.z ) > 0.001 ) {
				const Vector3DP v0 = g_nav_vertices[ g_nav_halfedges[ face.first_edge_idx ].vertex_idx ];
				const double planeD = QM_Vector3DotProductDP( v0, face.normal );
				standoffMid.z = ( planeD - face.normal.x * standoffMid.x - face.normal.y * standoffMid.y ) / face.normal.z;
			}
		}

		// Ensure standoff is connected to the corner through open space (not through a solid brush into another room):
		// Offset test start by 1.0 unit along bisectorNorm so the test does not self-intersect the corner vertex edges.
		const Vector3DP testStart = c.vertex + c.bisectorNorm * 1.0;
		if ( !Nav_HasGeometricLineOfSight2D( testStart, standoffMid, 0.0 ) ) {
			continue;
		}

		// Verify the corner is relevant to the active path corridor:
		// Either a waypoint lies near the corner, or a path segment passes within standoff range of the corner.
		bool cornerNearPath = false;
		for ( size_t i = 0; i < waypoints.size(); ++i ) {
			const double dx = waypoints[ i ].x - c.vertex.x;
			const double dy = waypoints[ i ].y - c.vertex.y;
			if ( ( dx * dx + dy * dy ) <= NAV_CORNER_SNAP_RADIUS_SQR &&
				 waypoints[ i ].z >= ( c.minZ - static_cast<double>( NAV_MAX_STEP_HEIGHT ) ) &&
				 waypoints[ i ].z <= ( c.maxZ + static_cast<double>( NAV_MAX_STEP_HEIGHT ) ) ) {
				cornerNearPath = true;
				break;
			}
			if ( i + 1 < waypoints.size() ) {
				const Vector3DP &p0 = waypoints[ i ];
				const Vector3DP &p1 = waypoints[ i + 1 ];
				Vector3DP segDir = p1 - p0;
				segDir.z = 0.0;
				const double segLen = QM_Vector3LengthDP( segDir );
				if ( segLen > 0.001 ) {
					Vector3DP toC = c.vertex - p0;
					toC.z = 0.0;
					const double t = ( toC.x * segDir.x + toC.y * segDir.y ) / ( segLen * segLen );
					if ( t >= 0.0 && t <= 1.0 ) {
						const Vector3DP proj = p0 + segDir * t;
						const double d2 = ( c.vertex.x - proj.x ) * ( c.vertex.x - proj.x ) + ( c.vertex.y - proj.y ) * ( c.vertex.y - proj.y );
						if ( d2 <= ( standoffDist * standoffDist * NAV_CORNER_CORRIDOR_PROXIMITY_RATIO_SQR ) &&
							 proj.z >= ( c.minZ - static_cast<double>( NAV_MAX_STEP_HEIGHT ) ) &&
							 proj.z <= ( c.maxZ + static_cast<double>( NAV_MAX_STEP_HEIGHT ) ) ) {
							cornerNearPath = true;
							break;
						}
					}
				}
			}
		}

		if ( !cornerNearPath ) {
			continue;
		}

		s_nav_corridor_corners.push_back( { c.vertex, standoffMid, standoffDist, c.bisectorNorm, c.u1, c.u2, c.n1, c.n2, c.minZ, c.maxZ, c.cornerType } );
	}

	if ( s_nav_corridor_corners.empty() ) {
		return;
	}

	// 3. Adjust only intermediate funnel corners. The physical start and requested
	// destination are authoritative: replacing either silently changes the path contract.
	for ( size_t i = 1; i + 1 < waypoints.size(); ++i ) {
		const bool isForced = ( forcedWaypoints != nullptr && i < forcedWaypoints->size() && ( *forcedWaypoints )[ i ] );

		// Forced waypoints (stair step portals, approach/landing runways, doorways) represent authoritative
		// flight/corridor centerlines and must NEVER be snapped or distorted by obstacle corner standoffs:
		if ( isForced ) {
			continue;
		}

		for ( const auto &corner : s_nav_corridor_corners ) {
			const double dx = waypoints[ i ].x - corner.vertex.x;
			const double dy = waypoints[ i ].y - corner.vertex.y;
			if ( ( dx * dx + dy * dy ) < NAV_CORNER_SNAP_RADIUS_SQR &&
				 waypoints[ i ].z >= ( corner.minZ - static_cast<double>( NAV_MAX_STEP_HEIGHT ) ) &&
				 waypoints[ i ].z <= ( corner.maxZ + static_cast<double>( NAV_MAX_STEP_HEIGHT ) ) ) {
				// Line-of-sight verification: ensure snapping does not place the waypoint across a solid brush
				Vector3DP bestStandoff = corner.standoffMid;
				bool snapSafe = false;

				// Test the initial standoff midpoint first, then try contracted standoffs if constrained by an opposite wall
				for ( const double scale : { NAV_STANDOFF_SCALE_FULL, NAV_STANDOFF_SCALE_SEVENTY_FIVE, NAV_STANDOFF_SCALE_HALF } ) {
					const double testDist = corner.standoffDist * scale;
					const double minDist = ( agentRadius > 0.0 ) ? ( agentRadius + NAV_STANDOFF_MIN_HULL_CLEARANCE_MARGIN ) : ( NAV_DEFAULT_AGENT_RADIUS + NAV_STANDOFF_MIN_HULL_CLEARANCE_MARGIN );
					if ( testDist < minDist ) {
						break;
					}

					Vector3DP candMid = corner.vertex + corner.bisectorNorm * testDist;
					candMid.z = waypoints[ i ].z;
					int32_t candFaceIdx = Nav_FindFaceInLeafStrict( candMid );
					if ( candFaceIdx < 0 ) {
						Vector3DP feetTest = candMid;
						feetTest.z -= NAV_STANDOFF_FEET_SNAP_OFFSET_Z;
						candFaceIdx = Nav_FindFaceInLeafStrict( feetTest );
					}
					if ( candFaceIdx < 0 ) {
						candFaceIdx = Nav_FindClosestFaceInLeaf( candMid );
						if ( candFaceIdx >= 0 && !Nav_PointInsideFace2D( candMid, g_nav_faces[ candFaceIdx ] ) ) {
							candFaceIdx = -1;
						}
					}

					if ( candFaceIdx >= 0 && candFaceIdx < static_cast<int32_t>( g_nav_faces.size() ) ) {
						const nav_face_t &face = g_nav_faces[ candFaceIdx ];
						if ( std::fabs( face.normal.z ) > 0.001 ) {
							const Vector3DP v0 = g_nav_vertices[ g_nav_halfedges[ face.first_edge_idx ].vertex_idx ];
							const double planeD = QM_Vector3DotProductDP( v0, face.normal );
							candMid.z = ( planeD - face.normal.x * candMid.x - face.normal.y * candMid.y ) / face.normal.z;
						}
					} else {
						continue;
					}

					bool candSafe = true;
					if ( i > 0 ) {
						Vector3DP segDir = candMid - waypoints[ i - 1 ];
						segDir.z = 0.0;
						const double segLen = QM_Vector3LengthDP( segDir );
						const Vector3DP testStart = ( segLen > NAV_SEGMENT_NUDGE_MIN_LENGTH )
							? ( waypoints[ i - 1 ] + segDir * ( NAV_PROBE_START_NUDGE / segLen ) )
							: waypoints[ i - 1 ];
						if ( !Nav_HasGeometricLineOfSight2D( testStart, candMid, 0.0 ) ) {
							candSafe = false;
						}
					}
					if ( candSafe && i + 1 < waypoints.size() ) {
						Vector3DP segDir = waypoints[ i + 1 ] - candMid;
						segDir.z = 0.0;
						const double segLen = QM_Vector3LengthDP( segDir );
						const Vector3DP testEnd = ( segLen > NAV_SEGMENT_NUDGE_MIN_LENGTH )
							? ( waypoints[ i + 1 ] - segDir * ( NAV_PROBE_START_NUDGE / segLen ) )
							: waypoints[ i + 1 ];
						if ( !Nav_HasGeometricLineOfSight2D( candMid, testEnd, 0.0 ) ) {
							candSafe = false;
						}
					}

					if ( candSafe ) {
						bestStandoff = candMid;
						snapSafe = true;
						break;
					}
				}

				if ( snapSafe ) {
					waypoints[ i ] = bestStandoff;
					if ( forcedWaypoints != nullptr && i < forcedWaypoints->size() ) {
						( *forcedWaypoints )[ i ] = true;
					}
					break;
				}
			}
		}
	}

	// 4. Single-pass insertion of standoff waypoints for segments that violate corner clearance:
	s_nav_refined_waypoints.clear();
	s_nav_refined_forced.clear();
	if ( s_nav_refined_waypoints.capacity() < ( waypoints.size() + s_nav_corridor_corners.size() + 16 ) ) {
		s_nav_refined_waypoints.reserve( waypoints.size() + s_nav_corridor_corners.size() + 16 );
	}
	if ( forcedWaypoints != nullptr && s_nav_refined_forced.capacity() < ( waypoints.size() + s_nav_corridor_corners.size() + 16 ) ) {
		s_nav_refined_forced.reserve( waypoints.size() + s_nav_corridor_corners.size() + 16 );
	}

	for ( size_t i = 0; i < waypoints.size(); ++i ) {
		s_nav_refined_waypoints.push_back( waypoints[ i ] );
		if ( forcedWaypoints != nullptr && i < forcedWaypoints->size() ) {
			s_nav_refined_forced.push_back( ( *forcedWaypoints )[ i ] );
		}

		if ( i + 1 < waypoints.size() ) {
			const Vector3DP &p0 = waypoints[ i ];
			const Vector3DP &p1 = waypoints[ i + 1 ];

			// Condition check: Only bypass corner standoffs if BOTH endpoints are forced waypoints
			// (e.g. consecutive steps on a staircase or door portal centerline pairs).
			// Transition segments entering a forced waypoint from open space (!isP0Forced && isP1Forced)
			// or exiting into open space (isP0Forced && !isP1Forced) must retain corner standoffs
			// so agents rounding an outside corner before stepping up do not hug or clip the corner wall.
			const bool isP0Forced = ( forcedWaypoints != nullptr && i < forcedWaypoints->size() && ( *forcedWaypoints )[ i ] );
			const bool isP1Forced = ( forcedWaypoints != nullptr && i + 1 < forcedWaypoints->size() && ( *forcedWaypoints )[ i + 1 ] );
			if ( isP0Forced && isP1Forced ) {
				continue;
			}

			Vector3DP segDir = p1 - p0;
			segDir.z = 0.0;
			const double segLen = QM_Vector3LengthDP( segDir );

			if ( segLen > 0.001 ) {
				const Vector3DP segDirNorm = segDir * ( 1.0 / segLen );

				s_nav_segment_corners.clear();
				if ( s_nav_segment_corners.capacity() < 32 ) {
					s_nav_segment_corners.reserve( 32 );
				}

				for ( const auto &corner : s_nav_corridor_corners ) {
					const double segMinZ = std::min( p0.z, p1.z ) - static_cast<double>( NAV_MAX_STEP_HEIGHT );
					const double segMaxZ = std::max( p0.z, p1.z ) + static_cast<double>( NAV_MAX_STEP_HEIGHT );
					if ( corner.maxZ < segMinZ || corner.minZ > segMaxZ ) {
						continue; // Corner is on a completely different vertical level (e.g. roof or lower floor)
					}

					Vector3DP toCorner = corner.vertex - p0;
					toCorner.z = 0.0;
					const double t = ( toCorner.x * segDirNorm.x + toCorner.y * segDirNorm.y ) / segLen;

					if ( t > NAV_CORNER_SEGMENT_START_MIN_T && t < NAV_CORNER_SEGMENT_MAX_T ) {
						const Vector3DP proj = p0 + segDirNorm * ( t * segLen );
						const double dist2DSqr = ( corner.vertex.x - proj.x ) * ( corner.vertex.x - proj.x ) +
												 ( corner.vertex.y - proj.y ) * ( corner.vertex.y - proj.y );

						if ( dist2DSqr < ( corner.standoffDist * corner.standoffDist ) ) {
							// Only insert standoff if the segment actually cuts on the obstacle side of the standoff
							const Vector3DP cornerToProj = proj - corner.vertex;
							const double projAlongBisector = QM_Vector3DotProductDP( cornerToProj, corner.bisectorNorm );
							if ( projAlongBisector < corner.standoffDist ) {
								if ( QM_Vector3DistanceSqrDP( p0, corner.standoffMid ) > NAV_CORNER_DUPLICATE_TOLERANCE_SQR &&
									 QM_Vector3DistanceSqrDP( p1, corner.standoffMid ) > NAV_CORNER_DUPLICATE_TOLERANCE_SQR ) {
									s_nav_segment_corners.push_back( { t, &corner } );
								}
							}
						}
					}
				}

				if ( !s_nav_segment_corners.empty() ) {
					// Sort all violated obstacle corners along the segment from start (t=0) to end (t=1).
					// For curved or circular sets of brushes, this automatically creates an ordered arc of waypoints
					// wrapping smoothly around the curve with guaranteed agent clearance at every step.
					std::sort( s_nav_segment_corners.begin(), s_nav_segment_corners.end(), []( const nav_segment_corner_t &a, const nav_segment_corner_t &b ) {
						return a.t < b.t;
					} );

					for ( size_t scIdx = 0; scIdx < s_nav_segment_corners.size(); ++scIdx ) {
						const auto &sc = s_nav_segment_corners[ scIdx ];
						if ( s_nav_refined_waypoints.empty() || QM_Vector3DistanceSqrDP( s_nav_refined_waypoints.back(), sc.corner->standoffMid ) > NAV_CORNER_DUPLICATE_TOLERANCE_SQR ) {
							Vector3DP bestStandoff = sc.corner->standoffMid;
							bool insertSafe = false;

							// Target forward for line-of-sight test:
							// If there is another corner along this segment, verify line of sight to that next corner's standoff.
							// Otherwise, verify line of sight to the segment destination p1.
							const Vector3DP &targetForward = ( scIdx + 1 < s_nav_segment_corners.size() )
								? s_nav_segment_corners[ scIdx + 1 ].corner->standoffMid
								: p1;

							for ( const double scale : { NAV_STANDOFF_SCALE_FULL, NAV_STANDOFF_SCALE_SEVENTY_FIVE, NAV_STANDOFF_SCALE_HALF } ) {
								const double testDist = sc.corner->standoffDist * scale;
								const double minDist = ( agentRadius > 0.0 ) ? ( agentRadius + NAV_STANDOFF_MIN_HULL_CLEARANCE_MARGIN ) : ( NAV_DEFAULT_AGENT_RADIUS + NAV_STANDOFF_MIN_HULL_CLEARANCE_MARGIN );
								if ( testDist < minDist ) {
									break;
								}

								Vector3DP candMid = sc.corner->vertex + sc.corner->bisectorNorm * testDist;
								candMid.z = p0.z + ( p1.z - p0.z ) * sc.t;
								int32_t candFaceIdx = Nav_FindFaceInLeafStrict( candMid );
								if ( candFaceIdx < 0 ) {
									Vector3DP feetTest = candMid;
									feetTest.z -= NAV_STANDOFF_FEET_SNAP_OFFSET_Z;
									candFaceIdx = Nav_FindFaceInLeafStrict( feetTest );
								}
								if ( candFaceIdx < 0 ) {
									candFaceIdx = Nav_FindClosestFaceInLeaf( candMid );
									if ( candFaceIdx >= 0 && !Nav_PointInsideFace2D( candMid, g_nav_faces[ candFaceIdx ] ) ) {
										candFaceIdx = -1;
									}
								}

								if ( candFaceIdx >= 0 && candFaceIdx < static_cast<int32_t>( g_nav_faces.size() ) ) {
									const nav_face_t &face = g_nav_faces[ candFaceIdx ];
									if ( std::fabs( face.normal.z ) > 0.001 ) {
										const Vector3DP v0 = g_nav_vertices[ g_nav_halfedges[ face.first_edge_idx ].vertex_idx ];
										const double planeD = QM_Vector3DotProductDP( v0, face.normal );
										candMid.z = ( planeD - face.normal.x * candMid.x - face.normal.y * candMid.y ) / face.normal.z;
									}
								} else {
									continue;
								}

								// Line-of-sight verification: ensure candMid is cleanly reachable from previous waypoint.
								Vector3DP segFromPrev = candMid - s_nav_refined_waypoints.back();
								segFromPrev.z = 0.0;
								const double segFromPrevLen = QM_Vector3LengthDP( segFromPrev );
								const Vector3DP testStart = ( segFromPrevLen > NAV_SEGMENT_NUDGE_MIN_LENGTH )
									? ( s_nav_refined_waypoints.back() + segFromPrev * ( NAV_PROBE_START_NUDGE / segFromPrevLen ) )
									: s_nav_refined_waypoints.back();
								if ( !Nav_HasGeometricLineOfSight2D( testStart, candMid, 0.0 ) ) {
									continue;
								}

								// Forward line-of-sight verification: ensure candMid connects to next target (next corner or p1).
								Vector3DP segToNext = targetForward - candMid;
								segToNext.z = 0.0;
								const double segToNextLen = QM_Vector3LengthDP( segToNext );
								const Vector3DP testEnd = ( segToNextLen > NAV_SEGMENT_NUDGE_MIN_LENGTH )
									? ( targetForward - segToNext * ( NAV_PROBE_START_NUDGE / segToNextLen ) )
									: targetForward;
								if ( !Nav_HasGeometricLineOfSight2D( candMid, testEnd, 0.0 ) ) {
									continue;
								}

								bestStandoff = candMid;
								insertSafe = true;
								break;
							}

							if ( insertSafe ) {
								// Check if obstacle corner represents an obstacle wall and a sharp turn (>= 45 deg) suitable for an arc:
								const double wallTurnDot = QM_Vector3DotProductDP( sc.corner->u1, sc.corner->u2 );
								if ( ( sc.corner->cornerType & NAV_EDGE_WALL ) != 0 && wallTurnDot <= NAV_CORNER_ARC_MIN_TURN_DOT ) {
									// Determine approach vs departure wall based on incoming path vector:
									const Vector3DP toP0 = s_nav_refined_waypoints.back() - sc.corner->vertex;
									const double projWall1 = toP0.x * sc.corner->u1.x + toP0.y * sc.corner->u1.y;
									const double projWall2 = toP0.x * sc.corner->u2.x + toP0.y * sc.corner->u2.y;

									const Vector3DP &uEntry = ( projWall1 >= projWall2 ) ? sc.corner->u1 : sc.corner->u2;
									const Vector3DP &nEntry = ( projWall1 >= projWall2 ) ? sc.corner->n1 : sc.corner->n2;
									const Vector3DP &uExit = ( projWall1 >= projWall2 ) ? sc.corner->u2 : sc.corner->u1;
									const Vector3DP &nExit = ( projWall1 >= projWall2 ) ? sc.corner->n2 : sc.corner->n1;

									const double effectiveAgentRadius = ( agentRadius > 0.0 ) ? agentRadius : NAV_DEFAULT_AGENT_RADIUS;
									const double arcClearance = effectiveAgentRadius + NAV_CORNER_CLEARANCE_MARGIN;
									const double arcMinSepSqr = std::min( NAV_CORNER_ARC_MIN_SEPARATION_SQR, effectiveAgentRadius * effectiveAgentRadius );

									// Check for compound corner (closely spaced adjacent corners within NAV_COMPOUND_CORNER_MAX_SPACING):
									bool hasNextClose = false;
									if ( scIdx + 1 < s_nav_segment_corners.size() ) {
										const double cdx = sc.corner->vertex.x - s_nav_segment_corners[ scIdx + 1 ].corner->vertex.x;
										const double cdy = sc.corner->vertex.y - s_nav_segment_corners[ scIdx + 1 ].corner->vertex.y;
										hasNextClose = ( ( cdx * cdx + cdy * cdy ) < NAV_COMPOUND_CORNER_MAX_SPACING_SQR );
									}
									bool hasPrevClose = false;
									if ( scIdx > 0 ) {
										const double cdx = sc.corner->vertex.x - s_nav_segment_corners[ scIdx - 1 ].corner->vertex.x;
										const double cdy = sc.corner->vertex.y - s_nav_segment_corners[ scIdx - 1 ].corner->vertex.y;
										hasPrevClose = ( ( cdx * cdx + cdy * cdy ) < NAV_COMPOUND_CORNER_MAX_SPACING_SQR );
									}

									// 1. Candidate Entry Tangent Waypoint (skip if coming directly from an adjacent compound corner):
									if ( !hasPrevClose ) {
										Vector3DP candEntry = sc.corner->vertex + uEntry * NAV_CORNER_ARC_TANGENT_OFFSET + nEntry * arcClearance;
										candEntry.z = bestStandoff.z;
										int32_t entryFaceIdx = Nav_FindFaceInLeafStrict( candEntry );
										if ( entryFaceIdx < 0 ) {
											Vector3DP feetTest = candEntry;
											feetTest.z -= NAV_STANDOFF_FEET_SNAP_OFFSET_Z;
											entryFaceIdx = Nav_FindFaceInLeafStrict( feetTest );
										}
										if ( entryFaceIdx < 0 ) {
											entryFaceIdx = Nav_FindClosestFaceInLeaf( candEntry );
											if ( entryFaceIdx >= 0 && !Nav_PointInsideFace2D( candEntry, g_nav_faces[ entryFaceIdx ] ) ) {
												entryFaceIdx = -1;
											}
										}

										if ( entryFaceIdx >= 0 && entryFaceIdx < static_cast<int32_t>( g_nav_faces.size() ) ) {
											const nav_face_t &face = g_nav_faces[ entryFaceIdx ];
											if ( std::fabs( face.normal.z ) > 0.001 ) {
												const Vector3DP v0 = g_nav_vertices[ g_nav_halfedges[ face.first_edge_idx ].vertex_idx ];
												const double planeD = QM_Vector3DotProductDP( v0, face.normal );
												candEntry.z = ( planeD - face.normal.x * candEntry.x - face.normal.y * candEntry.y ) / face.normal.z;
											}

											// Elevation tier guard: candEntry must be on the same vertical level as the corner standoff (within max step height)
											if ( std::fabs( candEntry.z - bestStandoff.z ) <= static_cast<double>( NAV_MAX_STEP_HEIGHT ) ) {
												// Verify minimum distance from previous waypoint and to apex standoff:
												if ( QM_Vector3DistanceSqrDP( s_nav_refined_waypoints.back(), candEntry ) > arcMinSepSqr &&
													 QM_Vector3DistanceSqrDP( candEntry, bestStandoff ) > arcMinSepSqr ) {
													// Verify forward progress along segment:
													Vector3DP segProgress = candEntry - s_nav_refined_waypoints.back();
													segProgress.z = 0.0;
													Vector3DP overallDir = bestStandoff - s_nav_refined_waypoints.back();
													overallDir.z = 0.0;
													if ( QM_Vector3DotProductDP( segProgress, overallDir ) > 0.0 ) {
														Vector3DP segToEntry = candEntry - s_nav_refined_waypoints.back();
														segToEntry.z = 0.0;
														const double segToEntryLen = QM_Vector3LengthDP( segToEntry );
														const Vector3DP testEntryStart = ( segToEntryLen > NAV_SEGMENT_NUDGE_MIN_LENGTH )
															? ( s_nav_refined_waypoints.back() + segToEntry * ( NAV_PROBE_START_NUDGE / segToEntryLen ) )
															: s_nav_refined_waypoints.back();

														if ( Nav_HasGeometricLineOfSight2D( testEntryStart, candEntry, 0.0 ) &&
															 Nav_HasGeometricLineOfSight2D( candEntry, bestStandoff, 0.0 ) ) {
															s_nav_refined_waypoints.push_back( candEntry );
															if ( forcedWaypoints != nullptr ) {
																s_nav_refined_forced.push_back( false );
															}
														}
													}
												}
											}
										}
									}

									// Insert apex standoff:
									s_nav_refined_waypoints.push_back( bestStandoff );
									if ( forcedWaypoints != nullptr ) {
										s_nav_refined_forced.push_back( true );
									}

									// 2. Candidate Exit Tangent Waypoint (skip if transitioning directly into an adjacent compound corner):
									if ( !hasNextClose ) {
										Vector3DP candExit = sc.corner->vertex + uExit * NAV_CORNER_ARC_TANGENT_OFFSET + nExit * arcClearance;
										candExit.z = bestStandoff.z;
										int32_t exitFaceIdx = Nav_FindFaceInLeafStrict( candExit );
										if ( exitFaceIdx < 0 ) {
											Vector3DP feetTest = candExit;
											feetTest.z -= NAV_STANDOFF_FEET_SNAP_OFFSET_Z;
											exitFaceIdx = Nav_FindFaceInLeafStrict( feetTest );
										}
										if ( exitFaceIdx < 0 ) {
											exitFaceIdx = Nav_FindClosestFaceInLeaf( candExit );
											if ( exitFaceIdx >= 0 && !Nav_PointInsideFace2D( candExit, g_nav_faces[ exitFaceIdx ] ) ) {
												exitFaceIdx = -1;
											}
										}

										if ( exitFaceIdx >= 0 && exitFaceIdx < static_cast<int32_t>( g_nav_faces.size() ) ) {
											const nav_face_t &face = g_nav_faces[ exitFaceIdx ];
											if ( std::fabs( face.normal.z ) > 0.001 ) {
												const Vector3DP v0 = g_nav_vertices[ g_nav_halfedges[ face.first_edge_idx ].vertex_idx ];
												const double planeD = QM_Vector3DotProductDP( v0, face.normal );
												candExit.z = ( planeD - face.normal.x * candExit.x - face.normal.y * candExit.y ) / face.normal.z;
											}

											// Directional guard: uExit must align with departure progress towards targetForward
											Vector3DP toForward = targetForward - sc.corner->vertex;
											toForward.z = 0.0;
											const bool exitAlignsWithTarget = ( QM_Vector3DotProductDP( toForward, uExit ) > 0.0 );

											// Elevation tier guard: candExit must be on the same vertical level as the corner standoff (within max step height)
											const bool exitElevationSafe = ( std::fabs( candExit.z - bestStandoff.z ) <= static_cast<double>( NAV_MAX_STEP_HEIGHT ) );

											if ( exitAlignsWithTarget && exitElevationSafe ) {
												// Verify minimum distance from apex standoff and to forward target:
												if ( QM_Vector3DistanceSqrDP( bestStandoff, candExit ) > arcMinSepSqr &&
													 QM_Vector3DistanceSqrDP( candExit, targetForward ) > arcMinSepSqr ) {
													// Verify forward progress along segment:
													Vector3DP segProgress = candExit - bestStandoff;
													segProgress.z = 0.0;
													Vector3DP overallDir = targetForward - bestStandoff;
													overallDir.z = 0.0;
													if ( QM_Vector3DotProductDP( segProgress, overallDir ) > 0.0 ) {
														Vector3DP segFromExit = targetForward - candExit;
														segFromExit.z = 0.0;
														const double segFromExitLen = QM_Vector3LengthDP( segFromExit );
														const Vector3DP testExitEnd = ( segFromExitLen > NAV_SEGMENT_NUDGE_MIN_LENGTH )
															? ( targetForward - segFromExit * ( NAV_PROBE_START_NUDGE / segFromExitLen ) )
															: targetForward;

														if ( Nav_HasGeometricLineOfSight2D( bestStandoff, candExit, 0.0 ) &&
															 Nav_HasGeometricLineOfSight2D( candExit, testExitEnd, 0.0 ) ) {
															s_nav_refined_waypoints.push_back( candExit );
															if ( forcedWaypoints != nullptr ) {
																s_nav_refined_forced.push_back( false );
															}
														}
													}
												}
											}
										}
									}
								} else {
									// Gentler turn: single apex standoff insertion
									s_nav_refined_waypoints.push_back( bestStandoff );
									if ( forcedWaypoints != nullptr ) {
										s_nav_refined_forced.push_back( true );
									}
								}
							}
						}
					}
				}
			}
		}
	}

	waypoints = s_nav_refined_waypoints;
	if ( forcedWaypoints != nullptr ) {
		*forcedWaypoints = s_nav_refined_forced;
	}
}

//! Portal representation used during funnel algorithm string pulling.
struct funnel_portal_t {
	Vector3DP left;
	Vector3DP right;
	bool force_waypoint = false;
};

//! Persistent static buffer for funnel portal sequence in Nav_StringPull.
static std::vector<funnel_portal_t> s_nav_stringpull_portals;
//! Persistent static buffer for tracking portal indices associated with waypoints.
static std::vector<int32_t> s_nav_stringpull_portal_indices;
//! Persistent static buffer for cylindrical obstacle subdivision waypoints.
static std::vector<Vector3DP> s_nav_stringpull_sub_waypoints;
//! Persistent static buffer for cylindrical obstacle subdivision forced flags.
static std::vector<bool> s_nav_stringpull_sub_forced;
//! Persistent static buffer for sanitized path waypoints.
static std::vector<Vector3DP> s_nav_stringpull_clean_waypoints;
//! Persistent static buffer for sanitized path forced flags.
static std::vector<bool> s_nav_stringpull_clean_forced;
//! Persistent static buffer for collinear decimated path waypoints.
static std::vector<Vector3DP> s_nav_stringpull_simplified_waypoints;
//! Persistent static buffer for collinear decimated path forced flags.
static std::vector<bool> s_nav_stringpull_simplified_forced;
//! Persistent static buffer for double-precision conversion in Vector3 overload.
static std::vector<Vector3DP> s_nav_stringpull_waypoints_dp;

/**
*	@brief	Build a smoothed string-pulled path using the Funnel algorithm in full double precision.
*	@param	path The sequence of face IDs to traverse.
*	@param	startPos The exact starting position in double precision.
*	@param	goalPos The exact ending position in double precision.
*	@param	agentRadius The collision radius to steer clear of walls.
*	@param	outWaypoints Output sequence of 3D double-precision points.
*	@param	outForcedWaypoints Optional output flags parallel to outWaypoints.
*	@param	agentMins Bounding box minimums for full physical swept volume verification.
*	@param	agentMaxs Bounding box maximums for full physical swept volume verification.
*	@param	traceShape Analytical collision shape (0 = Auto, 1 = Capsule, 2 = Cylinder).
*	@return	True if a valid corridor and string-pull could be generated.
*	@note	Upward stair transitions may add a mandatory approach waypoint before the portal.
**/
bool Nav_StringPull( const std::vector<int32_t> &path, const Vector3DP &startPos, const Vector3DP &goalPos, double agentRadius, std::vector<Vector3DP> &outWaypoints, std::vector<bool> *outForcedWaypoints, const Vector3 &agentMins, const Vector3 &agentMaxs, int32_t traceShape ) {
	outWaypoints.clear();
	outWaypoints.reserve( 128 );
	if ( outForcedWaypoints != nullptr ) {
		outForcedWaypoints->clear();
		outForcedWaypoints->reserve( 128 );
	}

	const Vector3DP startPosDP = startPos;
	const Vector3DP goalPosDP = goalPos;

	if ( path.empty() ) {
		return false;
	}

	if ( path.size() == 1 ) {
		outWaypoints.push_back( startPosDP );
		outWaypoints.push_back( goalPosDP );
		if ( outForcedWaypoints != nullptr ) {
			outForcedWaypoints->push_back( false );
			outForcedWaypoints->push_back( false );
		}
		return true;
	}

	s_nav_stringpull_portals.clear();
	if ( s_nav_stringpull_portals.capacity() < 512 ) {
		s_nav_stringpull_portals.reserve( 512 );
	}

	constexpr double NAV_CORNER_STANDOFF_MARGIN = 10.0;
	constexpr double SQRT2 = 1.41421356237309504880;
	const double agentClearance = ( agentRadius > 0.0 )
		? ( ( static_cast<double>( agentRadius ) + NAV_CORNER_STANDOFF_MARGIN ) * SQRT2 )
		: 0.0;

	for ( size_t i = 0; i + 1 < path.size(); ++i ) {
		const int32_t face_idx = path[ i ];
		const int32_t next_face_idx = path[ i + 1 ];
		
		Vector3DP left{}, right{};
		if ( Nav_GetPortalEndpoints( face_idx, next_face_idx, &left, &right ) ) {
			const Vector3DP rawLeft = left;
			const Vector3DP rawRight = right;

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

			const nav_face_t &nextFace = g_nav_faces[ next_face_idx ];
			const bool isRampTransition = ( face.normal.z < NAV_RAMP_MAX_NORMAL_Z || nextFace.normal.z < NAV_RAMP_MAX_NORMAL_Z );
			const double verticalDelta = isRampTransition ? 0.0 : std::max( std::fabs( stepRise ), std::fabs( nextFace.center.z - face.center.z ) );
			const bool isStepTransition = !isRampTransition && ( verticalDelta >= NAV_STEP_MIN_VERTICAL_DELTA && verticalDelta <= NAV_MAX_STEP_HEIGHT );
			const bool isDynamicPortal = face.entity_id != ENTITYNUM_NONE || face.transition_entity_id != ENTITYNUM_NONE ||
				nextFace.entity_id != ENTITYNUM_NONE || nextFace.transition_entity_id != ENTITYNUM_NONE;

			funnel_portal_t portal;
			if ( isDoor || isDynamicPortal ) {
				const Vector3DP mid = ( rawLeft + rawRight ) * 0.5;
				portal.left = mid;
				portal.right = mid;
				portal.force_waypoint = true;
				s_nav_stringpull_portals.push_back( portal );
			} else if ( isStepTransition ) {
				Vector3DP portalDir = rawLeft - rawRight;
				portalDir.z = 0.0;
				const double portalLen = QM_Vector3LengthDP( portalDir );
				Vector3DP fwd = { 0.0, 1.0, 0.0 };
				if ( portalLen > 0.001 ) {
					portalDir = portalDir * ( 1.0 / portalLen );
					fwd = Vector3DP{ -portalDir.y, portalDir.x, 0.0 };
					Vector3DP toNext = nextFace.center - face.center;
					toNext.z = 0.0;
					if ( QM_Vector3DotProductDP( fwd, toNext ) < 0.0 ) {
						fwd = fwd * -1.0;
					}
				}

				const double effectiveAgentRadius = ( agentRadius > 0.0 ) ? agentRadius : NAV_DEFAULT_AGENT_RADIUS;
				const double runwayDist = effectiveAgentRadius + NAV_STEP_RUNWAY_MARGIN;

				// 1. Check if entering a stair flight from a flat platform/room/ground:
				// If face is NOT preceded by another stair step, insert a frontal approach runway waypoint on face.
				// This aligns the agent perpendicularly to the bottom riser before starting the flight.
				bool prevIsStep = false;
				for ( int32_t pe = 0; pe < face.num_edges; pe++ ) {
					const nav_halfedge_t &phe = g_nav_halfedges[ face.first_edge_idx + pe ];
					if ( phe.twin_idx != -1 ) {
						const nav_face_t &twinFace = g_nav_faces[ g_nav_halfedges[ phe.twin_idx ].face_idx ];
						const bool neighborIsRamp = ( face.normal.z < NAV_RAMP_MAX_NORMAL_Z || twinFace.normal.z < NAV_RAMP_MAX_NORMAL_Z );
						const double neighborDelta = neighborIsRamp ? 0.0 : std::max( std::fabs( phe.z_diff ), std::fabs( twinFace.center.z - face.center.z ) );
						if ( !neighborIsRamp && neighborDelta >= NAV_STEP_LANDING_MIN_DELTA && neighborDelta <= NAV_MAX_STEP_HEIGHT ) {
							prevIsStep = true;
							break;
						}
					}
				}

				if ( !prevIsStep && portalLen > 0.001 ) {
					Vector3DP approachPoint = ( rawLeft + rawRight ) * 0.5 - fwd * runwayDist;
					approachPoint.z = face.center.z;
					if ( Nav_PointInsideFace2D( approachPoint, face ) ) {
						funnel_portal_t approachPortal;
						approachPortal.left = approachPoint;
						approachPortal.right = approachPoint;
						approachPortal.force_waypoint = true;
						s_nav_stringpull_portals.push_back( approachPortal );
					}
				}

				// 2. Stair step transition (ascending or descending): lock portal to the exact midpoint of the step width at tread elevation.
				Vector3DP mid = ( rawLeft + rawRight ) * 0.5;
				mid.z = nextFace.center.z;
				portal.left = mid;
				portal.right = mid;
				portal.force_waypoint = true;
				s_nav_stringpull_portals.push_back( portal );

				// 3. When exiting a stair flight onto a flat platform/room, insert a frontal runway landing waypoint.
				// This forces the agent to walk straight forward onto the solid platform before commencing any turn,
				// completely preventing lateral drift off the platform cliff edges.
				if ( portalLen > 0.001 ) {
					bool nextIsStep = false;
					for ( int32_t ne = 0; ne < nextFace.num_edges; ne++ ) {
						const nav_halfedge_t &nhe = g_nav_halfedges[ nextFace.first_edge_idx + ne ];
						if ( nhe.twin_idx != -1 ) {
							const nav_face_t &twinFace = g_nav_faces[ g_nav_halfedges[ nhe.twin_idx ].face_idx ];
							const bool neighborIsRamp = ( nextFace.normal.z < NAV_RAMP_MAX_NORMAL_Z || twinFace.normal.z < NAV_RAMP_MAX_NORMAL_Z );
							const double neighborDelta = neighborIsRamp ? 0.0 : std::max( std::fabs( nhe.z_diff ), std::fabs( twinFace.center.z - nextFace.center.z ) );
							if ( !neighborIsRamp && neighborDelta >= NAV_STEP_LANDING_MIN_DELTA && neighborDelta <= NAV_MAX_STEP_HEIGHT ) {
								nextIsStep = true;
								break;
							}
						}
					}

					if ( !nextIsStep ) {
						Vector3DP runwayPoint = ( rawLeft + rawRight ) * 0.5 + fwd * runwayDist;
						runwayPoint.z = nextFace.center.z;

						// Only insert runway landing if the point physically lies within the landing face polygon
						if ( Nav_PointInsideFace2D( runwayPoint, nextFace ) ) {
							funnel_portal_t runwayPortal;
							runwayPortal.left = runwayPoint;
							runwayPortal.right = runwayPoint;
							runwayPortal.force_waypoint = true;
							s_nav_stringpull_portals.push_back( runwayPortal );
						}
					}
				}
			} else {
				/**
				*	Raw mesh portal: clip portal endpoints against solid obstacle boundaries
				*	to ensure the funnel corridor maintains clearance from walls.
				**/
				Vector3DP clippedLeft = rawLeft;
				Vector3DP clippedRight = rawRight;
				bool isNarrowPortal = false;
				const double wallClearance = ( agentRadius > 0.0 ) ? ( agentRadius + NAV_PORTAL_CLEARANCE_MARGIN ) : ( NAV_DEFAULT_AGENT_RADIUS + NAV_PORTAL_CLEARANCE_MARGIN );
				const double cornerClearance = ( agentRadius > 0.0 ) ? ( agentRadius + NAV_CORNER_CLEARANCE_MARGIN ) : ( NAV_DEFAULT_AGENT_RADIUS + NAV_CORNER_CLEARANCE_MARGIN );
				if ( Nav_ClipPortalForAgentClearance( face_idx, next_face_idx, rawLeft, rawRight, wallClearance, cornerClearance, &clippedLeft, &clippedRight, &isNarrowPortal ) ) {
					portal.left = clippedLeft;
					portal.right = clippedRight;
					portal.force_waypoint = isNarrowPortal;
				} else {
					portal.left = rawLeft;
					portal.right = rawRight;
					portal.force_waypoint = false;
				}
				s_nav_stringpull_portals.push_back( portal );
			}
		} else {
			const Vector3DP center = g_nav_faces[ face_idx ].center;
			s_nav_stringpull_portals.push_back( { center, center } );
		}
	}

	funnel_portal_t goalPortal;
	goalPortal.left = goalPosDP;
	goalPortal.right = goalPosDP;
	goalPortal.force_waypoint = false;
	s_nav_stringpull_portals.push_back( goalPortal );

	s_nav_stringpull_portal_indices.clear();
	if ( s_nav_stringpull_portal_indices.capacity() < 256 ) {
		s_nav_stringpull_portal_indices.reserve( 256 );
	}
	auto AppendWaypoint = [&]( const Vector3DP &waypoint, const int32_t pIdx, const bool forced ) {
		outWaypoints.push_back( waypoint );
		s_nav_stringpull_portal_indices.push_back( pIdx );
		if ( outForcedWaypoints != nullptr ) {
			outForcedWaypoints->push_back( forced );
		}
	};

	AppendWaypoint( startPosDP, 0, false );

	Vector3DP portalApex = startPosDP;
	Vector3DP portalLeft = startPosDP;
	Vector3DP portalRight = startPosDP;

	int32_t apexIndex = 0;
	int32_t leftIndex = 0;
	int32_t rightIndex = 0;

	for ( int32_t i = 0; i < ( int32_t )s_nav_stringpull_portals.size(); ++i ) {
		const Vector3DP &left = s_nav_stringpull_portals[ i ].left;
		const Vector3DP &right = s_nav_stringpull_portals[ i ].right;

		// Tighten the right side of the funnel (right is to the left of / narrower than current right ray)
		if ( Nav_TriArea2D( portalApex, portalRight, right) >= 0.0 ) {
			double distSqRight = ( portalApex.x - portalRight.x ) * ( portalApex.x - portalRight.x ) + ( portalApex.y - portalRight.y ) * ( portalApex.y - portalRight.y );
			if ( distSqRight < 0.001 || Nav_TriArea2D( portalApex, portalLeft, right ) <= 0.0 ) {
				portalRight = right;
				rightIndex = i;
			} else {
				// Right edge crossed over left ray: portalLeft is a true corner waypoint
				AppendWaypoint( portalLeft, leftIndex, false );
				portalApex = portalLeft;
				apexIndex = leftIndex;
				portalLeft = portalApex;
				portalRight = portalApex;
				leftIndex = apexIndex;
				rightIndex = apexIndex;
				i = apexIndex;
				continue;
			}
		}

		// Tighten the left side of the funnel (left is to the right of / narrower than current left ray)
		if ( Nav_TriArea2D( portalApex, portalLeft, left ) <= 0.0 ) {
			double distSqLeft = ( portalApex.x - portalLeft.x ) * ( portalApex.x - portalLeft.x ) + ( portalApex.y - portalLeft.y ) * ( portalApex.y - portalLeft.y );
			if ( distSqLeft < 0.001 || Nav_TriArea2D( portalApex, portalRight, left ) >= 0.0 ) {
				portalLeft = left;
				leftIndex = i;
			} else {
				// Left edge crossed over right ray: portalRight is a true corner waypoint
				AppendWaypoint( portalRight, rightIndex, false );
				portalApex = portalRight;
				apexIndex = rightIndex;
				portalLeft = portalApex;
				portalRight = portalApex;
				leftIndex = apexIndex;
				rightIndex = apexIndex;
				i = apexIndex;
				continue;
			}
		}

		if ( s_nav_stringpull_portals[ i ].force_waypoint ) {
			if ( QM_Vector3DistanceSqrDP( portalApex, left ) > static_cast<double>( WAYPOINT_EPS_SQR ) ) {
				AppendWaypoint( left, i, true );
			}
			portalApex = left;
			apexIndex = i;
			portalLeft = portalApex;
			portalRight = portalApex;
			leftIndex = apexIndex;
			rightIndex = apexIndex;
		}
	}

	AppendWaypoint( goalPosDP, static_cast<int32_t>( s_nav_stringpull_portals.size() ) - 1, false );

	/**
	*	Curved/Cylindrical Obstacle Subdivision:
	*	If any segment (W_i -> W_{i+1}) intersects world geometry (e.g. cutting through a curved/cylindrical brush),
	*	subdivide the segment by inserting the intermediate portal midpoints in strict sequential order.
	**/
	if ( outWaypoints.size() >= 2 && outWaypoints.size() == s_nav_stringpull_portal_indices.size() ) {
		s_nav_stringpull_sub_waypoints.clear();
		s_nav_stringpull_sub_forced.clear();
		if ( s_nav_stringpull_sub_waypoints.capacity() < ( outWaypoints.size() * 2 + 16 ) ) {
			s_nav_stringpull_sub_waypoints.reserve( outWaypoints.size() * 2 + 16 );
		}
		if ( outForcedWaypoints != nullptr && s_nav_stringpull_sub_forced.capacity() < ( outWaypoints.size() * 2 + 16 ) ) {
			s_nav_stringpull_sub_forced.reserve( outWaypoints.size() * 2 + 16 );
		}

		s_nav_stringpull_sub_waypoints.push_back( outWaypoints.front() );
		if ( outForcedWaypoints != nullptr && !outForcedWaypoints->empty() ) {
			s_nav_stringpull_sub_forced.push_back( outForcedWaypoints->front() );
		}

		for ( size_t i = 0; i + 1 < outWaypoints.size(); ++i ) {
			const Vector3DP &p0 = outWaypoints[ i ];
			const Vector3DP &p1 = outWaypoints[ i + 1 ];
			const int32_t portal0 = s_nav_stringpull_portal_indices[ i ];
			const int32_t portal1 = s_nav_stringpull_portal_indices[ i + 1 ];
			const bool isP1Forced = ( outForcedWaypoints != nullptr && i + 1 < outForcedWaypoints->size() && ( *outForcedWaypoints )[ i + 1 ] );

			const double clearance = ( agentRadius > 0.0 ) ? agentRadius : NAV_DEFAULT_AGENT_RADIUS;
			if ( !Nav_HasGeometricLineOfSight2D( p0, p1, clearance ) && ( portal1 > portal0 + 1 ) ) {
				// Subdivide along the intermediate portals of the corridor in strict forward order
				for ( int32_t p = portal0 + 1; p < portal1; ++p ) {
					const Vector3DP mid = ( s_nav_stringpull_portals[ p ].left + s_nav_stringpull_portals[ p ].right ) * 0.5;
					if ( QM_Vector3DistanceSqrDP( mid, s_nav_stringpull_sub_waypoints.back() ) >= ( 8.0 * 8.0 ) &&
						 QM_Vector3DistanceSqrDP( mid, p1 ) >= ( 8.0 * 8.0 ) ) {
						s_nav_stringpull_sub_waypoints.push_back( mid );
						if ( outForcedWaypoints != nullptr ) {
							s_nav_stringpull_sub_forced.push_back( false );
						}
					}
				}
			}

			s_nav_stringpull_sub_waypoints.push_back( p1 );
			if ( outForcedWaypoints != nullptr ) {
				s_nav_stringpull_sub_forced.push_back( isP1Forced );
			}
		}

		outWaypoints = s_nav_stringpull_sub_waypoints;
		if ( outForcedWaypoints != nullptr ) {
			*outForcedWaypoints = s_nav_stringpull_sub_forced;
		}
	}

	/**
	*	Enforce strict agent clearance across all path segments by inserting convex corner standoff waypoints
	*	for any path polyline segment that passes within agent clearance of a solid obstacle corner.
	**/
	Nav_EnforceConvexCornerWaypoints( path, agentRadius, outWaypoints, outForcedWaypoints );

	/**
	*	Sanitize output waypoints: remove collinear or near-duplicate consecutive points.
	**/
	if ( outWaypoints.size() >= 2 ) {
		s_nav_stringpull_clean_waypoints.clear();
		s_nav_stringpull_clean_forced.clear();
		if ( s_nav_stringpull_clean_waypoints.capacity() < ( outWaypoints.size() + 16 ) ) {
			s_nav_stringpull_clean_waypoints.reserve( outWaypoints.size() + 16 );
		}
		if ( outForcedWaypoints != nullptr && s_nav_stringpull_clean_forced.capacity() < ( outWaypoints.size() + 16 ) ) {
			s_nav_stringpull_clean_forced.reserve( outWaypoints.size() + 16 );
		}

		s_nav_stringpull_clean_waypoints.push_back( outWaypoints.front() );
		if ( outForcedWaypoints != nullptr ) {
			s_nav_stringpull_clean_forced.push_back( outForcedWaypoints->front() );
		}

		for ( size_t i = 1; i < outWaypoints.size(); ++i ) {
			const double distSqr = QM_Vector3DistanceSqrDP( outWaypoints[ i ], s_nav_stringpull_clean_waypoints.back() );
			const bool isForced = ( outForcedWaypoints != nullptr && i < outForcedWaypoints->size() && ( *outForcedWaypoints )[ i ] );
			const bool isLast = ( i == outWaypoints.size() - 1 );

			/**
			*	Corner refinement can create A -> unforced portal -> nearby A hairpins.
			*	Remove the unforced excursion only when the retained corner endpoints
			*	are within one hull radius and their connecting segment has full clearance.
			*	Both mandatory endpoints remain intact unless they coincide exactly.
			**/
			const size_t cleanCount = s_nav_stringpull_clean_waypoints.size();
			if ( cleanCount >= 2 && outForcedWaypoints != nullptr &&
				 !s_nav_stringpull_clean_forced.back() &&
				 QM_Vector3DistanceSqrDP( outWaypoints[ i ], s_nav_stringpull_clean_waypoints[ cleanCount - 2 ] ) <= agentRadius * agentRadius &&
				 Nav_HasGeometricLineOfSight2D( s_nav_stringpull_clean_waypoints[ cleanCount - 2 ], outWaypoints[ i ], agentRadius ) ) {
				s_nav_stringpull_clean_waypoints.pop_back();
				s_nav_stringpull_clean_forced.pop_back();
				// Coalesce only identical corners, preserving each distinct mandatory location.
				if ( QM_Vector3DistanceSqrDP( outWaypoints[ i ], s_nav_stringpull_clean_waypoints.back() ) < 0.000001 ) {
					s_nav_stringpull_clean_forced.back() = s_nav_stringpull_clean_forced.back() || isForced;
					// Keep the exact requested endpoint rather than its near-equal predecessor.
					if ( isLast ) {
						s_nav_stringpull_clean_waypoints.back() = outWaypoints[ i ];
					}
					continue;
				}
			}

			// Preserve waypoints that represent meaningful progression (>= 2.0 units), are forced portals, or are the final goal
			if ( distSqr >= 4.0 || isForced || isLast ) {
				s_nav_stringpull_clean_waypoints.push_back( outWaypoints[ i ] );
				if ( outForcedWaypoints != nullptr ) {
					s_nav_stringpull_clean_forced.push_back( isForced );
				}
			}
		}

		// Collinear decimation: remove redundant intermediate points along straight sections
		if ( s_nav_stringpull_clean_waypoints.size() >= 3 ) {
			s_nav_stringpull_simplified_waypoints.clear();
			s_nav_stringpull_simplified_forced.clear();
			if ( s_nav_stringpull_simplified_waypoints.capacity() < ( s_nav_stringpull_clean_waypoints.size() + 16 ) ) {
				s_nav_stringpull_simplified_waypoints.reserve( s_nav_stringpull_clean_waypoints.size() + 16 );
			}
			if ( s_nav_stringpull_simplified_forced.capacity() < ( s_nav_stringpull_clean_waypoints.size() + 16 ) ) {
				s_nav_stringpull_simplified_forced.reserve( s_nav_stringpull_clean_waypoints.size() + 16 );
			}

			s_nav_stringpull_simplified_waypoints.push_back( s_nav_stringpull_clean_waypoints.front() );
			s_nav_stringpull_simplified_forced.push_back( s_nav_stringpull_clean_forced.front() );

			for ( size_t i = 1; i + 1 < s_nav_stringpull_clean_waypoints.size(); ++i ) {
				const bool isForced = s_nav_stringpull_clean_forced[ i ];
				if ( isForced ) {
					s_nav_stringpull_simplified_waypoints.push_back( s_nav_stringpull_clean_waypoints[ i ] );
					s_nav_stringpull_simplified_forced.push_back( true );
					continue;
				}

				const Vector3DP &prev = s_nav_stringpull_simplified_waypoints.back();
				const Vector3DP &curr = s_nav_stringpull_clean_waypoints[ i ];
				const Vector3DP &next = s_nav_stringpull_clean_waypoints[ i + 1 ];

				Vector3DP d1 = curr - prev;
				Vector3DP d2 = next - curr;
				d1.z = 0.0;
				d2.z = 0.0;
				const double len1 = QM_Vector3LengthDP( d1 );
				const double len2 = QM_Vector3LengthDP( d2 );

				if ( len1 > 0.001 && len2 > 0.001 ) {
					const Vector3DP u1 = d1 * ( 1.0 / len1 );
					const Vector3DP u2 = d2 * ( 1.0 / len2 );
					const double dot = QM_Vector3DotProductDP( u1, u2 );

					// If path continues in virtually the same direction (within ~10 deg), verify that the direct
					// segment (prev -> next) maintains unobstructed geometric clearance through obstacle geometry before pruning:
					if ( dot > NAV_COLLINEAR_MAX_DOT ) {
						const double clearance = ( agentRadius > 0.0 ) ? agentRadius : NAV_DEFAULT_AGENT_RADIUS;
						if ( Nav_HasGeometricLineOfSight2D( prev, next, clearance ) ) {
							continue;
						}
					}
				}

				s_nav_stringpull_simplified_waypoints.push_back( curr );
				s_nav_stringpull_simplified_forced.push_back( false );
			}

			s_nav_stringpull_simplified_waypoints.push_back( s_nav_stringpull_clean_waypoints.back() );
			s_nav_stringpull_simplified_forced.push_back( s_nav_stringpull_clean_forced.back() );

			s_nav_stringpull_clean_waypoints = s_nav_stringpull_simplified_waypoints;
			s_nav_stringpull_clean_forced = s_nav_stringpull_simplified_forced;
		}

		outWaypoints = s_nav_stringpull_clean_waypoints;
		if ( outForcedWaypoints != nullptr ) {
			*outForcedWaypoints = s_nav_stringpull_clean_forced;
		}
	}

	return true;
}

/**
*	@brief	Build a smoothed string-pulled path using the Funnel algorithm (single-precision convenience wrapper).
**/
bool Nav_StringPull( const std::vector<int32_t> &path, const Vector3 &startPos, const Vector3 &goalPos, float agentRadius, std::vector<Vector3> &outWaypoints, std::vector<bool> *outForcedWaypoints, const Vector3 &agentMins, const Vector3 &agentMaxs, int32_t traceShape ) {
	s_nav_stringpull_waypoints_dp.clear();
	if ( s_nav_stringpull_waypoints_dp.capacity() < 256 ) {
		s_nav_stringpull_waypoints_dp.reserve( 256 );
	}
	const bool ok = Nav_StringPull( path, Vector3DP( startPos ), Vector3DP( goalPos ), static_cast<double>( agentRadius ), s_nav_stringpull_waypoints_dp, outForcedWaypoints, agentMins, agentMaxs, traceShape );
	outWaypoints.clear();
	outWaypoints.reserve( s_nav_stringpull_waypoints_dp.size() );
	for ( const Vector3DP &wp : s_nav_stringpull_waypoints_dp ) {
		outWaypoints.push_back( static_cast<Vector3>( wp ) );
	}
	return ok;
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
