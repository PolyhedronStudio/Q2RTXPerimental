#include "svgame/svg_local.h"
#include "svgame/nav/nav_path.h"
#include "svgame/nav/nav_query.h"
#include "svgame/nav/nav_identity.h"
#include "svgame/nav/nav_route_context.h"
#include "svgame/nav/nav_thread.h"
#include "svgame/nav/nav_worker.h"
#include "svgame/nav/nav_clearance_geometry.h"
#include "svgame/nav/nav_perf.h"
#include "svgame/nav/nav_containers.h"
#include "svgame/nav/nav_core.h"
#include "svgame/nav/nav_generate.h" // For g_nav_nodes and g_nav_polys.
#include "svgame/nav/nav_sector_graph.h"
#include "svgame/svg_utils.h"
#include "svgame/crowd/svg_crowd_manager.h"
#include "svgame/entities/monster/svg_monster_base.h"
#include "svgame/entities/func/svg_func_door.h"
#include "svgame/entities/func/svg_func_door_rotating.h"
#include "svgame/entities/func/svg_func_wall.h"
#include "svgame/entities/func/svg_func_plat.h"
#include "svgame/entities/func/svg_func_areaportal.h"
#include "shared/math/qm_vector3.h"
#include "svgame/monsters/svg_mmove.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <queue>
#include <unordered_map>
#include <vector>
#include <thread>

int32_t Nav_FindClosestFaceInLeaf( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const Vector3DP &point );
int32_t Nav_FindFaceInLeafStrict( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const Vector3DP &point );
int32_t Nav_FindPolyInLeaf( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const Vector3DP &point );
int32_t Nav_FindClosestPolyGlobal( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const Vector3DP &point );
bool Nav_PointInsideFace2D( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const Vector3DP &point, const nav_face_t &face );
int32_t Nav_FindLeafNode( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const Vector3DP &point );

int32_t Nav_FindReachableFaceInLeaf( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const Vector3DP &point, const int32_t targetFace, const double agentRadius );
int32_t Nav_FindReachableFaceInLeaf( const Vector3DP &point, const int32_t targetFace, const double agentRadius );
int32_t Nav_GetFaceComponent( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const int32_t faceIdx );
int32_t Nav_GetFaceComponent( const int32_t faceIdx );
static bool Nav_ClipFaceLineInterval2D( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const nav_face_t &face, const Vector3DP &lineOrigin, const Vector3DP &lineDirection, const double lineLength, const int32_t excludedFaceIdx, const double minimumClearance, double *outMinT, double *outMaxT );
static bool Nav_ClipFaceLineInterval2D( const nav_face_t &face, const Vector3DP &lineOrigin, const Vector3DP &lineDirection, const double lineLength, const int32_t excludedFaceIdx, const double minimumClearance, double *outMinT, double *outMaxT );
bool Nav_GetPortalEndpoints( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, int32_t faceA, int32_t faceB, Vector3DP *outV0, Vector3DP *outV1 );
bool Nav_GetPortalEndpoints( int32_t faceA, int32_t faceB, Vector3DP *outV0, Vector3DP *outV1 );
static bool Nav_ComputePortalOverlapSegment( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const nav_halfedge_t &he, const nav_halfedge_t &twin, Vector3DP *outV0, Vector3DP *outV1, double *outWidth2D );

/**
*	@brief	Advance one query's spatial token without aliasing older visits on rollover.
**/
static uint32_t Nav_RouteNextStamp( uint32_t &stamp, svg_vector_t<uint32_t, TAG_SVGAME_NAVMESH> &tokens ) {
	// Only this context clears its visitation; no immutable snapshot record is written.
	if ( ++stamp == 0 ) {
		std::fill( tokens.begin(), tokens.end(), 0u );
		stamp = 1;
	}
	return stamp;
}

nav_segment_clearance_t Nav_TestSegmentClearance( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const Vector3DP &p0, const Vector3DP &p1, const double clearance );
bool Nav_HasGeometricLineOfSight2D( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const Vector3DP &p0, const Vector3DP &p1, const double clearanceMargin );
void Nav_RaycastRoomBoundary2D( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const Vector3DP &anchor, const double maxRadius, const int32_t numRays, double *outDoorYaw, bool *outHasDoor, double *outMinWallDist, double *outAvgWallDist, nav_zone_type_t *outZoneType, const double agentRadius );
bool Nav_RaycastHalfEdge2D( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const Vector3DP &start, const Vector3DP &dirNorm, const double maxDistance, nav_raycast_result_t *outResult );
bool Nav_CircleCastHalfEdge2D( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const Vector3DP &start, const Vector3DP &dirNorm, const double maxDistance, const double agentRadius, nav_raycast_result_t *outResult );

static void Nav_RepairUnsafeFlatLinks( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, std::vector<Vector3DP> &points, std::vector<bool> &forced, const double radius );
static void Nav_RepairUnsafeFlatLinks( svg_nav_route_context_t &context, std::vector<Vector3DP> &points, std::vector<bool> &forced, const double radius );
static void Nav_PruneUnsafeFlatPortals( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, std::vector<Vector3DP> &waypoints, std::vector<bool> *forced, const double radius );
static void Nav_PruneUnsafeFlatPortals( std::vector<Vector3DP> &waypoints, std::vector<bool> *forced, const double radius );
static bool Nav_ProjectOptionalFlatPortal( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const Vector3DP &previous, Vector3DP &current, const Vector3DP &next, const double radius );
static bool Nav_ProjectOptionalFlatPortal( const Vector3DP &previous, Vector3DP &current, const Vector3DP &next, const double radius );
static bool Nav_IsContinuousFlatSpan( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const Vector3DP &start, const Vector3DP &end );
static bool Nav_IsContinuousFlatSpan( const Vector3DP &start, const Vector3DP &end );
static void Nav_EnforceConvexCornerWaypoints( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const std::vector<int32_t> &path, const double agentRadius, std::vector<Vector3DP> &waypoints, std::vector<bool> *forcedWaypoints, const Vector3 &mins, const Vector3 &maxs, const int32_t shape, const nav_path_policy_t &policy, svg_base_edict_t *passEntity );
static void Nav_EnforceConvexCornerWaypoints( svg_nav_route_context_t &context, const std::vector<int32_t> &path, const double agentRadius, std::vector<Vector3DP> &waypoints, std::vector<bool> *forcedWaypoints, const Vector3 &mins, const Vector3 &maxs, const int32_t shape, const nav_path_policy_t &policy, svg_base_edict_t *passEntity );
static bool Nav_ClipPortalForAgentClearance( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const int32_t faceAIdx, const int32_t faceBIdx, const Vector3DP &portalLeft, const Vector3DP &portalRight, const double wallClearance, const double cornerClearance, Vector3DP *outLeft, Vector3DP *outRight, bool *outIsNarrowPortal );
static bool Nav_ClipPortalForAgentClearance( const int32_t faceAIdx, const int32_t faceBIdx, const Vector3DP &portalLeft, const Vector3DP &portalRight, const double wallClearance, const double cornerClearance, Vector3DP *outLeft, Vector3DP *outRight, bool *outIsNarrowPortal );

static bool Nav_RepairRefinementRoute( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const std::vector<Vector3DP> &original, const std::vector<bool> &originalForced,
	const svg_vector_t<nav_waypoint_anchor_t, TAG_SVGAME_NAVMESH> &portalAnchors,
	std::vector<Vector3DP> &output, std::vector<bool> &outputForced, const Vector3 &mins, const Vector3 &maxs,
	const int32_t shape, const nav_path_policy_t &policy, svg_base_edict_t *passEntity );
static bool Nav_RepairRefinementRoute( svg_nav_route_context_t &context, const std::vector<Vector3DP> &original, const std::vector<bool> &originalForced,
	const svg_vector_t<nav_waypoint_anchor_t, TAG_SVGAME_NAVMESH> &portalAnchors,
	std::vector<Vector3DP> &output, std::vector<bool> &outputForced, const Vector3 &mins, const Vector3 &maxs,
	const int32_t shape, const nav_path_policy_t &policy, svg_base_edict_t *passEntity );
nav_route_validation_t Nav_ValidateHullRouteNativeMainThread( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const std::span<const Vector3DP> points,
	const Vector3 &mins, const Vector3 &maxs, const int32_t shape, const nav_path_policy_t &policy, svg_base_edict_t *passEntity, const bool requireExactGoal = true, const uint32_t sampleBudget = 8192u );
nav_goal_support_t Nav_ResolveHullGoalSupport( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const Vector3DP &feet, const double radius, const int32_t shape );
static bool Nav_BuildWaypointRecords( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const std::vector<Vector3DP> &points, std::vector<bool> *forced,
	const double radius, svg_vector_t<svg_nav_waypoint_t, TAG_SVGAME_NAVMESH> &output );
static bool Nav_BuildWaypointRecords( svg_nav_route_context_t &context, const std::vector<Vector3DP> &points, std::vector<bool> *forced,
	const double radius, svg_vector_t<svg_nav_waypoint_t, TAG_SVGAME_NAVMESH> &output );

//! Bounded counters describing the most recent explicit path query.
static nav_path_diagnostics_t s_nav_last_path_diagnostics = {};

//! Query-local source anchors survive funnel/refinement cleanup until final records are assembled.
//! Owner-thread anchor scratch is cleared before every synchronous string-pull query.

/**
*	@brief	Retain exact source geometry for a generated portal or corner obligation.
*	@param	feet	Generated feet-space constraint position.
*	@param	flags	Combinable semantic obligations at that position.
*	@param	portal	Source half-edge, or -1 for a corner-only constraint.
*	@param	corner	Source corner-cache index, or -1 for a portal-only constraint.
*	@param	face	Supporting face when generation already knows it.
**/
static void Nav_RecordWaypointConstraint( svg_nav_route_context_t &context, const Vector3DP &feet, const uint32_t flags,
	const int32_t portal, const int32_t corner, const int32_t face ) {
	// Copy provenance; never retain a pointer into geometry/refinement scratch.
	context.anchors.push_back( { feet, { flags, portal, corner }, face } );
}

/**
*	@brief	Assemble authoritative semantic records after all point generation and cleanup.
*	@param	points	Retained exact route positions.
*	@param	forced	Legacy generation flags; output is derived from the completed records.
*	@param	radius	Unchanged native planar radius.
*	@param	output	Owned tagged waypoint records.
*	@return	False on source-obligation overflow; no incomplete contract is accepted.
*	@note	Coalescing preserves all source flags and IDs rather than keeping only one Boolean.
**/
static bool Nav_BuildWaypointRecords( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const std::vector<Vector3DP> &points, std::vector<bool> *forced,
	const double radius, svg_vector_t<svg_nav_waypoint_t, TAG_SVGAME_NAVMESH> &output ) {
	/**
	*	Build into owned records, preserving exact endpoints and indexed route constraints.
	**/
	output.clear();
	output.reserve( points.size() );
	// Each retained point receives its own event and supporting-face metadata.
	for ( size_t index = 0; index < points.size(); ++index ) {
		svg_nav_waypoint_t record;
		record.feet = points[ index ];
		record.face = Nav_FindClosestFaceInLeaf( mesh, context, record.feet );
		record.constraint_id = static_cast<uint32_t>( index );
		record.required_clearance = radius;
		// Start/final semantics preserve endpoint ownership independently of mandatory intermediates.
		if ( index == 0 ) {
			record.flags |= NAV_WAYPOINT_START;
		}
		if ( index + 1 == points.size() ) {
			record.flags |= NAV_WAYPOINT_FINAL_GOAL;
		}
		/**
		*	Merge provenance for exact or deliberately coalesced nearby generated constraints.
		**/
		// The cleanup's four-unit coalescing tolerance is metadata matching, never hull clearance reduction.
		for ( const auto &anchor : context.anchors ) {
			if ( QM_Vector3DistanceSqrDP( anchor.feet, record.feet ) > WAYPOINT_EPS_SQR ) {
				continue;
			}
			bool duplicate = false;
			// Multiple anchors from the same source union their flags instead of consuming more capacity.
			for ( uint32_t source = 0; source < record.constraint_count; ++source ) {
				auto &existing = record.constraints[ source ];
				if ( existing.portal == anchor.source.portal && existing.corner == anchor.source.corner ) {
					existing.flags |= anchor.source.flags;
					duplicate = true;
					break;
				}
			}
			// Defined overflow failure prevents silently dropping an obligation.
			if ( !duplicate ) {
				if ( record.constraint_count == record.constraints.size() ) {
					output.clear();
					return false;
				}
				record.constraints[ record.constraint_count++ ] = anchor.source;
			}
			record.flags |= anchor.source.flags;
		}
		// Preserve legacy generation obligations while every producer migrates to explicit sources.
		if ( forced != nullptr && index < forced->size() && ( *forced )[ index ] ) {
			record.flags |= NAV_WAYPOINT_MANDATORY;
		}
		/**
		*	Store immutable segment directions so progression does not infer an obligation from a Boolean.
		**/
		const auto direction = []( Vector3DP delta ) {
			delta.z = 0.0;
			const double length = QM_Vector3LengthDP( delta );
			return length > 0.000001 ? delta * ( 1.0 / length ) : Vector3DP{};
		};
		record.incoming = index > 0 ? direction( record.feet - points[ index - 1 ] ) : Vector3DP{};
		record.outgoing = index + 1 < points.size() ? direction( points[ index + 1 ] - record.feet ) : Vector3DP{};
		output.push_back( record );
	}
	/**
	*	Every generated mandatory source must remain owned after coalescing and cleanup.
	*	Checking only retained points could silently discard a portal obligation removed by generation.
	**/
	for ( const auto &anchor : context.anchors ) {
		// Optional entry/exit decorations may be decimated after their replacement link is proved.
		if ( ( anchor.source.flags & NAV_WAYPOINT_MANDATORY ) == 0 ) {
			continue;
		}
		bool owned = false;
		// Match source identity and all flags, not proximity alone or a legacy forced Boolean.
		for ( const auto &record : output ) {
			for ( uint32_t source = 0; source < record.constraint_count; ++source ) {
				const auto &retained = record.constraints[ source ];
				if ( retained.portal == anchor.source.portal && retained.corner == anchor.source.corner &&
					 ( retained.flags & anchor.source.flags ) == anchor.source.flags ) {
					owned = true;
					break;
				}
			}
			if ( owned ) {
				break;
			}
		}
		// Fail explicitly rather than installing a route with an omitted mandatory event.
		if ( !owned ) {
			output.clear();
			return false;
		}
	}
	/**
	*	Maintain compatibility drawing only as a derived view of authoritative semantics.
	**/
	if ( forced != nullptr ) {
		forced->assign( output.size(), false );
		// Derive each old flag after source unions are complete.
		for ( size_t index = 0; index < output.size(); ++index ) {
			( *forced )[ index ] = Nav_WaypointIsMandatory( output[ index ] );
		}
	}
	return true;
}

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

static bool Nav_ComputePortalOverlapSegment( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const nav_halfedge_t &he, const nav_halfedge_t &twin, Vector3DP *outV0, Vector3DP *outV1, double *outWidth2D ) {
	/**
	*	Sanity checks: reject topological twins whose owning faces cannot form a physical floor portal.
	**/
	const nav_face_t &face = mesh.faces[ he.face_idx ];
	const nav_face_t &neighbor_face = mesh.faces[ twin.face_idx ];
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
	const Vector3DP a0 = mesh.vertices[ he.vertex_idx ];
	const Vector3DP a1 = mesh.vertices[ mesh.halfedges[ he.next_idx ].vertex_idx ];
	const Vector3DP b0 = mesh.vertices[ twin.vertex_idx ];
	const Vector3DP b1 = mesh.vertices[ mesh.halfedges[ twin.next_idx ].vertex_idx ];

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
int32_t Nav_FindLeafNode( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const Vector3DP &point ) {
	/**
	*	Sanity check: reject empty trees immediately.
	**/
	if ( mesh.nodes.empty() ) {
		return -1;
	}

	/**
	*	Start at root node (index 0) and traverse internal nodes down to a leaf.
	**/
	int32_t nodeIdx = 0;
	while ( true ) {
		const nav_kdtree_node_t &node = mesh.nodes[ nodeIdx ];

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
bool Nav_PointInsideFace2D( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const Vector3DP &point, const nav_face_t &face ) {
	// Faces need at least three edges before they can contain a point.
	if ( face.num_edges < 3 ) {
		return false;
	}

	double windingSign = 0.0;
	for ( int32_t e = 0; e < face.num_edges; e++ ) {
		const nav_halfedge_t &he = mesh.halfedges[ face.first_edge_idx + e ];
		const Vector3DP a = mesh.vertices[ he.vertex_idx ];
		const Vector3DP b = mesh.vertices[ mesh.halfedges[ he.next_idx ].vertex_idx ];

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
int32_t Nav_FindClosestPolyGlobal( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const Vector3DP &point ) {
	nav_perf_scope_t global_scope( NAV_PERF_PHASE_GLOBAL_FALLBACKS );
	Nav_Perf_RecordGlobalFaceScan( static_cast<int32_t>( mesh.faces.size() ) );
	// Track the best face that actually contains the point in 2D.
	int32_t bestInsideFace = -1;
	double bestInsideDist = 64.0;

	// Track the nearest face as a fallback when no face contains the point.
	int32_t bestFallbackFace = -1;
	double bestFallbackDistSqr = 999999.0 * 999999.0;

	// Scan every face because this is the final catch-all lookup.
	for ( size_t i = 0; i < mesh.faces.size(); ++i ) {
		const nav_face_t &face = mesh.faces[ i ];

		// Discard faces flagged with CM_SURFACE_NO_NAVMESH or CONTENTS_NO_NAVMESH:
		if ( ( face.surface_flags & ( CM_SURFACE_NO_NAVMESH | CONTENTS_NO_NAVMESH ) ) != 0 ) {
			continue;
		}

		// Measure vertical distance to the face plane for the inside test.
		const Vector3DP v0 = mesh.vertices[ mesh.halfedges[ face.first_edge_idx ].vertex_idx ];
		const double plane_dist = QM_Vector3DotProductDP( v0, face.normal );
		const double d = std::fabs( QM_Vector3DotProductDP( point, face.normal ) - plane_dist );

		if ( d < bestInsideDist && Nav_PointInsideFace2D( mesh, context, point, face ) ) {
			bestInsideDist = d;
			bestInsideFace = static_cast< int32_t >( i );
		}

		// Also track the nearest face center as a fallback using squared distance.
		const double dx = point.x - face.center.x;
		const double dy = point.y - face.center.y;
		const double dz = point.z - face.center.z;
		const double centerDistSqr = ( dx * dx ) + ( dy * dy ) + ( dz * dz );
		if ( centerDistSqr < bestFallbackDistSqr ) {
			bestFallbackDistSqr = centerDistSqr;
			bestFallbackFace = static_cast< int32_t >( i );
		}
	}

	// Preserve the original containing-face preference and exhaustive fallback.
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
int32_t Nav_FindPolyInLeaf( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const Vector3DP &point ) {
	nav_perf_scope_t lookup_scope( NAV_PERF_PHASE_START_GOAL_LOOKUP );
	// Find the leaf first so we can prefer local faces over a global scan.
	const int32_t leafIdx = Nav_FindLeafNode( mesh, context, point );
	if ( leafIdx < 0 ) {
		return Nav_FindClosestPolyGlobal( mesh, context, point );
	}

	const nav_kdtree_node_t &leaf = mesh.nodes[ leafIdx ];
	const int32_t firstFaceIdx = leaf.first_face_id;
	if ( firstFaceIdx == -1 || firstFaceIdx >= static_cast< int32_t >( mesh.faces.size() ) ) {
		return Nav_FindClosestPolyGlobal( mesh, context, point );
	}

	Nav_Perf_RecordSpatialCandidate( 1, leaf.num_faces );

	// Search candidate faces in this leaf first.
	// Contiguous iteration [firstFaceIdx, firstFaceIdx + num_faces) visits each face exactly once.
	// Does not mutate global mesh face structures, preserving thread-safety and re-entrancy.
	int32_t bestInsideFace = -1;
	double bestInsideDist = 64.0;
	int32_t bestFallbackFace = -1;
	double bestFallbackDistSqr = 999999.0 * 999999.0;

	for ( int32_t i = 0; i < leaf.num_faces; ++i ) {
		const int32_t faceIdx = firstFaceIdx + i;
		if ( faceIdx >= static_cast< int32_t >( mesh.faces.size() ) ) {
			break;
		}

		const nav_face_t &face = mesh.faces[ faceIdx ];

		// Discard faces flagged with CM_SURFACE_NO_NAVMESH or CONTENTS_NO_NAVMESH:
		if ( ( face.surface_flags & ( CM_SURFACE_NO_NAVMESH | CONTENTS_NO_NAVMESH ) ) != 0 ) {
			continue;
		}

		const Vector3DP v0 = mesh.vertices[ mesh.halfedges[ face.first_edge_idx ].vertex_idx ];
		const double plane_dist = QM_Vector3DotProductDP( v0, face.normal );
		const double d = std::fabs( QM_Vector3DotProductDP( point, face.normal ) - plane_dist );

		if ( d < bestInsideDist && Nav_PointInsideFace2D( mesh, context, point, face ) ) {
			bestInsideDist = d;
			bestInsideFace = faceIdx;
		}

		const double dx = point.x - face.center.x;
		const double dy = point.y - face.center.y;
		const double dz = point.z - face.center.z;
		const double centerDistSqr = ( dx * dx ) + ( dy * dy ) + ( dz * dz );
		if ( centerDistSqr < bestFallbackDistSqr ) {
			bestFallbackDistSqr = centerDistSqr;
			bestFallbackFace = faceIdx;
		}
	}

	if ( bestInsideFace != -1 ) {
		return bestInsideFace;
	}

	// Perform global search if the candidate faces in the KD-tree leaf hint did not contain the point.
	return Nav_FindClosestPolyGlobal( mesh, context, point );
}

/**
*	@brief	Locate the navmesh polygon enclosing a world-space point strictly within its local KD-leaf.
*	@note	Unlike Nav_FindPolyInLeaf, this function strictly never falls back to Nav_FindClosestPolyGlobal.
*			If the point is not contained within any face of the resolved KD-tree leaf, it returns -1 immediately in O(log N).
*	@param	point	Query position in world space (Vector3DP).
*	@return	Face index if contained within a leaf face, or -1 otherwise.
**/
int32_t Nav_FindFaceInLeafStrict( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const Vector3DP &point ) {
	nav_perf_scope_t lookup_scope( NAV_PERF_PHASE_START_GOAL_LOOKUP );
	const int32_t leafIdx = Nav_FindLeafNode( mesh, context, point );
	if ( leafIdx < 0 ) {
		return -1;
	}

	const nav_kdtree_node_t &leaf = mesh.nodes[ leafIdx ];
	const int32_t firstFaceIdx = leaf.first_face_id;
	if ( firstFaceIdx == -1 || firstFaceIdx >= static_cast< int32_t >( mesh.faces.size() ) ) {
		return -1;
	}

	Nav_Perf_RecordSpatialCandidate( 1, leaf.num_faces );

	// Contiguous iteration visits each face in the leaf strictly once without mutating nav_face_t.
	int32_t bestInsideFace = -1;
	double bestInsideDist = 64.0;

	for ( int32_t i = 0; i < leaf.num_faces; ++i ) {
		const int32_t faceIdx = firstFaceIdx + i;
		if ( faceIdx >= static_cast< int32_t >( mesh.faces.size() ) ) {
			break;
		}

		const nav_face_t &face = mesh.faces[ faceIdx ];

		// Discard faces flagged with CM_SURFACE_NO_NAVMESH or CONTENTS_NO_NAVMESH:
		if ( ( face.surface_flags & ( CM_SURFACE_NO_NAVMESH | CONTENTS_NO_NAVMESH ) ) != 0 ) {
			continue;
		}

		const Vector3DP v0 = mesh.vertices[ mesh.halfedges[ face.first_edge_idx ].vertex_idx ];
		const double plane_dist = QM_Vector3DotProductDP( v0, face.normal );
		const double d = std::fabs( QM_Vector3DotProductDP( point, face.normal ) - plane_dist );

		if ( d < bestInsideDist && Nav_PointInsideFace2D( mesh, context, point, face ) ) {
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
bool Nav_GetPortalEndpoints( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, int32_t faceA, int32_t faceB, Vector3DP *outV0, Vector3DP *outV1 ) {
	/**
	*	Sanity checks: ensure valid output pointers and valid face indices.
	**/
	if ( outV0 == nullptr || outV1 == nullptr || faceA < 0 || faceB < 0 ||
		static_cast< size_t >( faceA ) >= mesh.faces.size() ||
		static_cast< size_t >( faceB ) >= mesh.faces.size() ) {
		return false;
	}

	const nav_face_t &fA = mesh.faces[ faceA ];
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
		const nav_halfedge_t &he = mesh.halfedges[ fA.first_edge_idx + e ];
		// Skip boundary edges without twins.
		if ( he.twin_idx == -1 ) {
			continue;
		}

		const nav_halfedge_t &twin = mesh.halfedges[ he.twin_idx ];
		// Skip edges that connect to a different neighbor face.
		if ( twin.face_idx != faceB ) {
			continue;
		}

		Vector3DP temp_left, temp_right;
		// Nav_ComputePortalOverlapSegment outputs temp_left (Left) and temp_right (Right) in CCW traversal space.
		if ( Nav_ComputePortalOverlapSegment( mesh, context, he, twin, &temp_left, &temp_right, nullptr ) ) {
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

//! Priority queue wrapper exposing underlying container for heap-free clearing and capacity reuse.
struct astar_priority_queue : public std::priority_queue<AStarNode, svg_vector_t<AStarNode, TAG_SVGAME_NAVMESH>, std::greater<AStarNode>> {
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

//! Connected component partition identifier per nav face for O(1) reachability rejection.
static svg_vector_t<int32_t, TAG_SVGAME_NAVMESH> s_nav_face_components;
//! Precomputed 2D portal width per half-edge for O(1) portal clearance checks.
static svg_vector_t<double, TAG_SVGAME_NAVMESH> s_nav_edge_portal_widths;

//! Pre-allocated flat A* node state per nav face to avoid dynamic heap allocations.
struct nav_astar_face_t {
	uint32_t queryId = 0;
	double gScore = 0.0;
	int32_t cameFrom = -1;
};
//! Persistent flat array of A* node states across all navigation faces.
static svg_vector_t<nav_astar_face_t, TAG_SVGAME_NAVMESH> s_nav_astar_faces;
//! Monotonically increasing query token counter for O(1) A* state invalidation without reallocations.
static uint32_t s_nav_astar_query_counter = 0;

/**
*	@brief	Retain the corrected M4 synchronous reference for parity and unsupported live policies.
*	@param	startFace	Starting face in the live owner-thread mesh.
*	@param	goalFace	Destination face in that mesh.
*	@param	outPath	Owned corridor output.
*	@param	policy	Traversal policy and optional live callback, confined to this phase.
*	@return	True when the reference found a complete corridor.
*	@note	This compatibility phase is explicitly excluded from the worker call graph.
**/
static bool Nav_FindPathLegacyMainThread( int32_t startFace, int32_t goalFace, std::vector<int32_t> &outPath, const nav_path_policy_t &policy ) {
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
		Nav_Perf_RecordAStarSearch( 0, 0, 0, true );
		return true;
	}

	Nav_EnsureObstacleCornersTable();

	// O(1) Connected Component Reachability Test:
	// If start and goal faces are in disconnected navmesh islands (e.g. roof vs floor or unreachable ledge),
	// reject immediately without performing an expensive exhaustive graph flood-fill!
	if ( static_cast<size_t>( startFace ) < s_nav_face_components.size() &&
		 static_cast<size_t>( goalFace ) < s_nav_face_components.size() ) {
		if ( s_nav_face_components[ startFace ] != s_nav_face_components[ goalFace ] ) {
			Nav_Perf_RecordAStarSearch( 0, 0, 0, false );
			return false;
		}
	}

	nav_perf_scope_t astar_scope( NAV_PERF_PHASE_ASTAR_SEARCH );
	int32_t astarExpansions = 0;
	int32_t astarStalePops = 0;
	size_t astarPeakHeap = 0;

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

	/**
	*	@brief	A* Heuristic function estimating remaining traversal cost to goal.
	*	@note	Scaled by face slope so A* favors searching along flat surfaces.
	*			Corridor hysteresis can apply up to 0.85 discount on committed corridors.
	*			This weighted search provides fast goal-directed corridor discovery
	*			with bounded expansion effort.
	**/
	auto Heuristic = []( int32_t a, int32_t b ) -> double {
		const double dist = QM_Vector3DistanceDP( g_nav_faces[ a ].center, g_nav_faces[ b ].center );
		const double slopeA = QM_Clamp( g_nav_faces[ a ].normal.z, 0.0, 1.0 );
		const double h = dist * ( 1.0 + ( 1.0 - slopeA ) * 4.0 );
		return ( std::isfinite( h ) && h >= 0.0 ) ? h : 0.0;
	};
	s_astar_open_set.push( { startFace, Heuristic( startFace, goalFace ) } );

	while ( !s_astar_open_set.empty() ) {
		if ( s_astar_open_set.size() > astarPeakHeap ) {
			astarPeakHeap = s_astar_open_set.size();
		}
		const auto topNode = s_astar_open_set.top();
		const int32_t current = topNode.polyIdx;
		s_astar_open_set.pop();

		// Detect and discard stale heap pops where current node was already closed or reached with a better gScore.
		if ( s_nav_astar_faces[ current ].queryId == s_nav_astar_query_counter ) {
			const double currentRecordedG = s_nav_astar_faces[ current ].gScore;
			const double expectedF = currentRecordedG + Heuristic( current, goalFace );
			if ( topNode.fScore > expectedF + 1e-4 ) {
				astarStalePops++;
				continue; // Stale heap-entry rejection!
			}
		}

		astarExpansions++;
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
			s_nav_last_path_diagnostics.graph_cost = s_nav_astar_faces[ goalFace ].gScore;
			Nav_Perf_RecordAStarSearch( astarExpansions, astarStalePops, astarPeakHeap, true );
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
			// Candidate topology is policy-independent; only the actual mover's allowed rise authorizes search traversal.
			if ( zDelta > policy.max_step_height ) {
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
			// Validate finite non-negative cost to preserve graph admissibility and heap ordering:
			if ( !std::isfinite( edgeCost ) || edgeCost < 0.0 ) {
				edgeCost = 0.0;
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

	Nav_Perf_RecordAStarSearch( astarExpansions, astarStalePops, astarPeakHeap, false );
	return false;
}

//! Owner-thread copied graph, replaced only without outstanding owner-side pins.
static svg_nav_mesh_snapshot_t s_nav_query_snapshot;
//! Published copy validity, independently retired before live mesh mutation.
static bool s_nav_query_snapshot_valid = false;
//! Owner-only lease count; workers receive const views, never modify this counter.
static uint32_t s_nav_query_snapshot_pins = 0;
//! Designated publication/allocation owner, bound at the first owner-side lifecycle call.
static std::thread::id s_nav_query_owner;
//! Retained synchronous context/request/result with no sampling-time engine allocations in the solver.
static svg_nav_query_context_t s_nav_query_context;
//! Owned synchronous payload; borrowed monster membership is copied before invoking the pure solver.
static svg_nav_request_t s_nav_query_request;
//! Owned graph reply; native waypoint validation and route commit remain main-thread phases.
static svg_nav_search_result_t s_nav_query_result;
//! Owner-issued namespace and epoch shared by request, actor and assignment revisions.
static uint64_t s_nav_identity_revision = 0, s_nav_identity_epoch = 1;
//! Opt-in parity controls and owner-thread counters; never written by the pure solver.
static bool s_nav_query_parity = false;
//! Counts of explicit reference comparisons and actual corridor/cost mismatches.
static uint64_t s_nav_query_compared = 0, s_nav_query_mismatches = 0;
//! Complete native route comparisons between independent contexts under the same pinned publication.
static uint64_t s_nav_route_compared = 0, s_nav_route_mismatches = 0;
//! Unsupported live-policy and preallocated-buffer limit events, reported only on request.
static uint64_t s_nav_query_excluded = 0, s_nav_query_resource_limits = 0;

/**
*	@brief	Check and bind the engine-safe publication/allocation owner.
*	@return	False for off-owner use; Debug also asserts the violated lifecycle invariant.
**/
static bool Nav_QueryIsOwnerThread( void ) {
	// Bind only through explicitly named main-thread APIs, never from the pure algorithm.
	const auto current = std::this_thread::get_id();
	if ( s_nav_query_owner == std::thread::id{} ) {
		s_nav_query_owner = current;
	}
	const bool owner = current == s_nav_query_owner;
	Q_assert( owner );
	return owner;
}

/**
*	@brief	Read the owner-issued service epoch used by route commit checks.
**/
uint64_t Nav_IdentityEpochMainThread( void ) {
	// Identity access is owner-only; workers receive a copied scalar instead.
	Q_assert( Nav_QueryIsOwnerThread() );
	return s_nav_identity_epoch;
}

/**
*	@brief	Issue a revision, draining the synchronous service before namespace rollover.
*	@note	No path worker exists in M5; M6 must extend this barrier to its queue and running reader.
**/
uint64_t Nav_IssueIdentityRevisionMainThread( void ) {
	// Do not publish identities from an unsupported execution owner.
	if ( !Nav_QueryIsOwnerThread() ) {
		return 0;
	}
	/**
	*	Overflow closes the old publication after all inline queries and generation have drained.
	*	Changing epoch makes every retained actor reply obsolete, even if a revision number repeats.
	**/
	if ( s_nav_identity_revision == UINT64_MAX ) {
		// Join the copied graph reader before checking/releasing the old epoch publication.
		Nav_StopQueryWorkerMainThread();
		Nav_DrainAsyncGenerationMainThread();
		Q_assert( s_nav_query_snapshot_pins == 0 );
		// Epoch exhaustion cannot safely reuse an old identity namespace.
		if ( s_nav_query_snapshot_pins != 0 || s_nav_identity_epoch == UINT64_MAX ) {
			gi.error( "Navigation identity namespace exhausted with live readers" );
			return 0;
		}
		Nav_RetireQuerySnapshotMainThread();
		++s_nav_identity_epoch;
		s_nav_identity_revision = 0;
	}
	return ++s_nav_identity_revision;
}

/**
*	@brief	Convert an engine vector into a pointer-free double-precision copied record.
*	@return	Exact component copy; endpoint coordinates are not snapped or projected.
**/
static nav_query_point_t Nav_QueryCopyPoint( const Vector3DP &point ) {
	// Retain double coordinates exactly across the ownership boundary.
	return { point.x, point.y, point.z };
}

/**
*	@brief	Prepare copied graph/lookup/portal geometry on the designated engine owner.
*	@param	snapshot	Exclusive unpublished storage; outstanding pins prohibit rebuilding.
*	@return	True only after complete index validation and publication identity assignment.
*	@note	Trace-dependent cache preparation occurs here, never inside Nav_QueryFindPath.
**/
bool Nav_BuildQuerySnapshotMainThread( svg_nav_mesh_snapshot_t &snapshot ) {
	/**
	*	Reject unsupported lifetime overlap before any live mesh read or tagged allocation.
	**/
	if ( !Nav_QueryIsOwnerThread() || s_nav_query_snapshot_pins != 0 || Nav_GetGenerationProgress().is_generating ) {
		return false;
	}
	// Incomplete collision preparation is pending publication, never a partial geometry snapshot.
	if ( !Nav_PrepareQueryInputsMainThread( 0 ) ) {
		return false;
	}
	Nav_EnsureObstacleCornersTable();
	snapshot.generation = snapshot.topology = 0;
	// All copied storage is allocated/resized on the engine owner before readers receive spans.
	snapshot.vertices.resize( g_nav_vertices.size() );
	snapshot.faces.resize( g_nav_faces.size() );
	snapshot.edges.resize( g_nav_halfedges.size() );
	snapshot.nodes.resize( g_nav_nodes.size() );
	/**
	*	Copy finite geometry and validate every polygon span before a query can index it.
	**/
	for ( size_t index = 0; index < g_nav_vertices.size(); ++index ) {
		const auto point = Nav_QueryCopyPoint( g_nav_vertices[ index ] );
		if ( !Nav_QueryFinite( point ) ) {
			return false;
		}
		snapshot.vertices[ index ] = point;
	}
	for ( size_t index = 0; index < g_nav_faces.size(); ++index ) {
		const auto &face = g_nav_faces[ index ];
		if ( face.first_edge_idx < 0 || face.num_edges < 0 ||
			 static_cast<size_t>( face.first_edge_idx ) + static_cast<size_t>( face.num_edges ) > g_nav_halfedges.size() ) {
			return false;
		}
		auto &copy = snapshot.faces[ index ];
		copy.first_edge = face.first_edge_idx;
		copy.num_edges = face.num_edges;
		copy.center = Nav_QueryCopyPoint( face.center );
		copy.normal = Nav_QueryCopyPoint( face.normal );
		copy.excluded = ( face.surface_flags & ( CM_SURFACE_NO_NAVMESH | CONTENTS_NO_NAVMESH ) ) != 0;
		copy.component = index < s_nav_face_components.size() ? s_nav_face_components[ index ] : -1;
		if ( !Nav_QueryFinite( copy.center ) || !Nav_QueryFinite( copy.normal ) ) {
			return false;
		}
	}
	/**
	*	Resolve adjacency and portal overlap into indexed scalar records while native topology is owned.
	**/
	for ( size_t index = 0; index < g_nav_halfedges.size(); ++index ) {
		const auto &edge = g_nav_halfedges[ index ];
		if ( edge.vertex_idx < 0 || edge.next_idx < 0 || static_cast<size_t>( edge.vertex_idx ) >= snapshot.vertices.size() ||
			 static_cast<size_t>( edge.next_idx ) >= g_nav_halfedges.size() || !std::isfinite( edge.z_diff ) ) {
			return false;
		}
		auto &copy = snapshot.edges[ index ];
		copy = {};
		copy.vertex = edge.vertex_idx;
		copy.next = edge.next_idx;
		copy.rise = edge.z_diff;
		copy.disabled = ( edge.flags & NAV_EDGE_DISABLED ) != 0;
		copy.width = index < s_nav_edge_portal_widths.size() ? s_nav_edge_portal_widths[ index ] : 0.0;
		// Boundary edges remain -1; adjacent records retain both directed disabled flags.
		if ( edge.twin_idx >= 0 ) {
			if ( static_cast<size_t>( edge.twin_idx ) >= g_nav_halfedges.size() ) {
				return false;
			}
			const auto &twin = g_nav_halfedges[ edge.twin_idx ];
			if ( twin.face_idx < 0 || static_cast<size_t>( twin.face_idx ) >= g_nav_faces.size() ) {
				return false;
			}
			copy.neighbor = twin.face_idx;
			copy.disabled = copy.disabled || ( twin.flags & NAV_EDGE_DISABLED ) != 0;
			Vector3DP left = {}, right = {};
			copy.has_portal = Nav_ComputePortalOverlapSegment( edge, twin, &left, &right, nullptr );
			copy.left = Nav_QueryCopyPoint( left );
			copy.right = Nav_QueryCopyPoint( right );
		}
	}
	/**
	*	Copy KD splits and publish identities only after every geometry record is complete.
	**/
	for ( size_t index = 0; index < g_nav_nodes.size(); ++index ) {
		const auto &node = g_nav_nodes[ index ];
		snapshot.nodes[ index ] = { node.first_face_id, node.num_faces, node.left_child, node.right_child, node.split_axis, node.split_pos };
	}
	Nav_CaptureRouteSnapshotMainThread( snapshot );
	snapshot.generation = g_nav_mesh_generation;
	snapshot.topology = g_nav_topology_revision;
	return true;
}

/**
*	@brief	Pin current immutable graph backing on its engine-safe owner.
*	@return	Const owner or nullptr while generation/publication cannot serve a safe new lease.
*	@note	Old pins keep copied geometry alive; they never alias mutating live mesh vectors.
**/
const svg_nav_mesh_snapshot_t *Nav_AcquireQuerySnapshotMainThread( void ) {
	/**
	*	Generation blocks new publication; an outstanding retired lease prevents storage replacement.
	**/
	if ( !Nav_QueryIsOwnerThread() || Nav_GetGenerationProgress().is_generating || s_nav_query_snapshot_pins == UINT32_MAX ) {
		return nullptr;
	}
	const auto view = s_nav_query_snapshot.View();
	if ( !s_nav_query_snapshot_valid || view.generation != g_nav_mesh_generation || view.topology != g_nav_topology_revision ) {
		if ( s_nav_query_snapshot_pins != 0 || !Nav_BuildQuerySnapshotMainThread( s_nav_query_snapshot ) ) {
			return nullptr;
		}
		s_nav_query_snapshot_valid = true;
	}
	// Only the owner changes lifetime accounting; the pure solver never pins or frees.
	++s_nav_query_snapshot_pins;
	return &s_nav_query_snapshot;
}

/**
*	@brief	Release a current or retired owner-side lease without worker-side destruction.
*	@param	snapshot	Exact previously acquired owner; nullptr means no lease.
**/
void Nav_ReleaseQuerySnapshotMainThread( const svg_nav_mesh_snapshot_t *snapshot ) {
	/**
	*	Enforce matched owner release; preserve backing while another context still holds a pin.
	**/
	if ( snapshot == nullptr ) {
		return;
	}
	if ( !Nav_QueryIsOwnerThread() ) {
		return;
	}
	Q_assert( snapshot == &s_nav_query_snapshot && s_nav_query_snapshot_pins != 0 );
	if ( snapshot != &s_nav_query_snapshot || s_nav_query_snapshot_pins == 0 ) {
		return;
	}
	--s_nav_query_snapshot_pins;
}

/**
*	@brief	Retire acquisition before live mesh revision replacement; preserve old copied reader data.
**/
void Nav_RetireQuerySnapshotMainThread( void ) {
	// Retirement never clears/free vectors while an independent pure context still reads them.
	if ( Nav_QueryIsOwnerThread() ) {
		// Close submission and join before this publication can be replaced.
		Nav_StopQueryWorkerMainThread();
		s_nav_query_snapshot_valid = false;
	}
}

/**
*	@brief	Keep an owner-only snapshot lease matched across early returns and allocation exceptions.
*	@note	Noncopyable/nonmovable guards never enter request payloads or move onto a worker.
**/
class nav_query_snapshot_scope_t {
public:
	/**
	*	Constructor / destructor.
	**/
	/**
	*	@brief	Acquire copied geometry on the designated engine-safe owner.
	**/
	nav_query_snapshot_scope_t() : snapshot( Nav_AcquireQuerySnapshotMainThread() ) {}
	/**
	*	@brief	Release an outstanding owner lease during normal return or exception unwinding.
	**/
	~nav_query_snapshot_scope_t() { Release(); }
	// Prevent duplicate ownership or a moved guard being destroyed on an arbitrary worker.
	nav_query_snapshot_scope_t( const nav_query_snapshot_scope_t & ) = delete;
	nav_query_snapshot_scope_t &operator=( const nav_query_snapshot_scope_t & ) = delete;
	/**
	*	Member functions.
	**/
	/**
	*	@brief	Expose the pinned const owner; a null result denotes unavailable publication.
	**/
	const svg_nav_mesh_snapshot_t *Get() const { return snapshot; }
	/**
	*	@brief	Release once and clear local ownership so a second/destructor release is harmless.
	**/
	void Release() {
		// Clearing our own pointer prevents early-release paths from decrementing the owner twice.
		Nav_ReleaseQuerySnapshotMainThread( snapshot );
		snapshot = nullptr;
	}
private:
	/**
	*	Member variables.
	**/
	//! Exact acquired owner; cleared as soon as the matching release completes.
	const svg_nav_mesh_snapshot_t *snapshot = nullptr;
};

/**
*	@brief	Copy the identical graph policy for synchronous and worker callers on the owner.
*	@param	mesh	Owner-pinned immutable publication used for indexed membership and face hints.
*	@return	True only when the captured identity still matches this publication.
*	@note	No live callback or entity pointer enters the copied request.
**/
bool Nav_PreparePathRequestMainThread( const nav_query_mesh_view_t &mesh, svg_nav_request_t &request,
	const int32_t start, const int32_t goal, const nav_path_policy_t &policy, const nav_query_cost_policy_t &cost,
	const std::span<const uint32_t> marks, const uint32_t stamp,
	const Vector3DP *start_feet, const Vector3DP *goal_feet, const nav_query_identity_t &identity ) {
	/**
	*	Copy exact endpoints, physical limits and the owned prior-corridor preference.
	**/
	request.start_face = start;
	request.goal_face = goal;
	request.start = start >= 0 && static_cast<size_t>( start ) < mesh.faces.size() ? mesh.faces[ start ].center : nav_query_point_t{};
	request.goal = goal >= 0 && static_cast<size_t>( goal ) < mesh.faces.size() ? mesh.faces[ goal ].center : nav_query_point_t{};
	// Full callers copy exact requested feet coordinates; the face-only diagnostic API uses face centers.
	if ( start_feet != nullptr ) {
		request.start = Nav_QueryCopyPoint( *start_feet );
	}
	if ( goal_feet != nullptr ) {
		request.goal = Nav_QueryCopyPoint( *goal_feet );
	}
	request.mins = Nav_QueryCopyPoint( Vector3DP( policy.agent_mins ) );
	request.maxs = Nav_QueryCopyPoint( Vector3DP( policy.agent_maxs ) );
	request.shape = policy.trace_shape;
	request.radius = policy.agent_radius;
	request.step = policy.max_step_height;
	request.drop = policy.max_drop_height;
	request.min_normal = policy.min_step_normal;
	request.ignore_disabled = policy.ignore_disabled_edges;
	request.cost = cost;
	request.identity = identity;
	// Preparation cannot relabel an actor request after epoch/publication invalidation.
	if ( identity.epoch != s_nav_identity_epoch || identity.generation != mesh.generation || identity.topology != mesh.topology ) {
		return false;
	}
	request.prior_faces.clear();
	if ( !marks.empty() && stamp != 0 ) {
		request.prior_faces.resize( mesh.faces.size(), 0 );
		for ( size_t face = 0; face < mesh.faces.size(); ++face ) {
			request.prior_faces[ face ] = face < marks.size() && marks[ face ] == stamp ? 1 : 0;
		}
	}
	return true;
}

/**
*	@brief	Execute the snapshot solver synchronously after owner-only request/capacity preparation.
*	@param	marks	Transient owner input, copied before the pure solve; never stored in the request.
*	@param	stamp	Nonzero prior-corridor membership stamp.
*	@return	True for a complete graph corridor; no partial/resource-limited route is installed.
*	@note	Unknown policies use Nav_FindPathLegacyMainThread explicitly; no worker is enabled.
**/
bool Nav_FindPathSnapshotCostsMainThread( const int32_t start, const int32_t goal,
	std::vector<int32_t> &output, const nav_path_policy_t &policy, const nav_query_cost_policy_t &cost,
	const std::span<const uint32_t> marks, const uint32_t stamp,
	const Vector3DP *start_feet, const Vector3DP *goal_feet,
	svg_nav_graph_workspace_t *workspace, svg_nav_result_t *route_result ) {
	/**
	*	Acquire copied geometry before touching retained request/context storage.
	**/
	output.clear();
	// Issue before pinning so controlled rollover never runs with this query's reader outstanding.
	const uint64_t sequence = route_result != nullptr && route_result->identity.sequence != 0 ?
		route_result->identity.sequence : Nav_IssueIdentityRevisionMainThread();
	nav_query_snapshot_scope_t lease;
	const auto *snapshot = lease.Get();
	if ( snapshot == nullptr ) {
		return false;
	}
	const auto mesh = snapshot->View();
	/**
	*	Copy physical and cost policy into the owned transient request, with no entity/callback pointers.
	**/
	// Explicit callers own independent buffers; face-only compatibility remains owner-serialized.
	auto &request = workspace != nullptr ? workspace->request : s_nav_query_request;
	auto &queryContext = workspace != nullptr ? workspace->context : s_nav_query_context;
	auto &queryResult = workspace != nullptr ? workspace->result : s_nav_query_result;
	nav_query_identity_t identity = route_result != nullptr ? route_result->identity : nav_query_identity_t{};
	identity.sequence = sequence;
	// Face-only owner diagnostics capture publication identity here; actors retain their original capture.
	if ( route_result == nullptr || route_result->identity.sequence == 0 ) {
		identity.epoch = s_nav_identity_epoch;
		identity.generation = mesh.generation;
		identity.topology = mesh.topology;
	}
	if ( !Nav_PreparePathRequestMainThread( mesh, request, start, goal, policy, cost, marks, stamp, start_feet, goal_feet, identity ) ) {
		return false;
	}
	// Preserve actor ownership through the native stage and compatibility diagnostics.
	if ( route_result != nullptr ) {
		route_result->identity = request.identity;
	}
	/**
	*	Prepare retained capacities only on the owner; the pure solver may not grow any buffer.
	**/
	queryContext.states.resize( mesh.faces.size() );
	queryContext.heap.reserve( std::max<size_t>( 512, mesh.edges.size() * 2 + 1 ) );
	queryResult.corridor.reserve( mesh.faces.size() );
	nav_query_status_t status;
	{
		nav_perf_scope_t scope( NAV_PERF_PHASE_ASTAR_SEARCH );
		status = Nav_QueryFindPath( mesh, queryContext, request, queryResult );
	}
	const bool found = status == nav_query_status_t::found;
	if ( status == nav_query_status_t::resource_limited ) {
		++s_nav_query_resource_limits;
	}
	if ( found ) {
		output.assign( queryResult.corridor.begin(), queryResult.corridor.end() );
	}
	/**
	*	Copy request-local counters into the compatibility debug record on the owner, outside the pure graph.
	**/
	const auto counter = []( const uint64_t value ) { return static_cast<int32_t>( std::min<uint64_t>( value, INT32_MAX ) ); };
	const auto &context = queryContext;
	nav_path_diagnostics_t diagnostics;
	diagnostics.start_face = start;
	diagnostics.goal_face = goal;
	diagnostics.query_valid = status != nav_query_status_t::invalid_input;
	diagnostics.route_found = found;
	diagnostics.graph_cost = queryResult.cost;
	diagnostics.expanded_faces = counter( context.expansions );
	diagnostics.accepted_transitions = counter( context.accepted );
	diagnostics.rejected_disabled = counter( context.disabled );
	diagnostics.rejected_narrow_portal = counter( context.narrow );
	diagnostics.rejected_step_height = counter( context.rises );
	diagnostics.rejected_drop_height = counter( context.drops );
	Nav_Perf_RecordAStarSearch( counter( context.expansions ), counter( context.stale_pops ), static_cast<size_t>( context.peak_heap ), found );
	/**
	*	Opt-in parity compares the actual legacy callback policy and full corridor/cost before restoring diagnostics.
	**/
	if ( s_nav_query_parity ) {
		std::vector<int32_t> reference;
		const bool old_found = Nav_FindPathLegacyMainThread( start, goal, reference, policy );
		const double old_cost = s_nav_last_path_diagnostics.graph_cost;
		const double tolerance = 1e-9 * std::max( 1.0, std::fabs( old_cost ) );
		bool equal = old_found == found && reference == output && std::fabs( old_cost - diagnostics.graph_cost ) <= tolerance;
		// Pure face/portal extraction also agrees with the corrected synchronous lookup and topology reference.
		if ( start >= 0 && static_cast<size_t>( start ) < mesh.faces.size() ) {
			const auto &point = g_nav_faces[ start ].center;
			equal = equal && Nav_QueryFindFace( mesh, Nav_QueryCopyPoint( point ), true ) == Nav_FindFaceInLeafStrict( point );
		}
		// Every selected corridor portal is compared before the native funnel receives the reference path.
		for ( size_t index = 1; index < output.size(); ++index ) {
			nav_query_point_t left = {}, right = {};
			Vector3DP old_left = {}, old_right = {};
			const bool pure_portal = Nav_QueryPortal( mesh, output[ index - 1 ], output[ index ], &left, &right );
			const bool old_portal = Nav_GetPortalEndpoints( output[ index - 1 ], output[ index ], &old_left, &old_right );
			equal = equal && pure_portal == old_portal;
			if ( pure_portal && old_portal ) {
				equal = equal && Nav_QueryDistance( left, Nav_QueryCopyPoint( old_left ) ) <= 1e-9 &&
					Nav_QueryDistance( right, Nav_QueryCopyPoint( old_right ) ) <= 1e-9;
			}
		}
		++s_nav_query_compared;
		if ( !equal ) {
			++s_nav_query_mismatches;
			gi.dprintf( "[nav query parity] FAIL start=%" PRId32 " goal=%" PRId32 " old=%d new=%d cost=%.17g/%.17g\n", start, goal, old_found, found, old_cost, diagnostics.graph_cost );
		}
	}
	// Publish the owned reply first, then mirror it for legacy console diagnostics.
	if ( route_result != nullptr ) {
		route_result->diagnostics = diagnostics;
	}
	s_nav_last_path_diagnostics = diagnostics;
	return found;
}

/**
*	@brief	Route default pure policies to copied queries; retain explicit owner-only virtual-policy compatibility.
*	@return	True when the selected synchronous phase found a complete corridor.
**/
bool Nav_FindPath( int32_t startFace, int32_t goalFace, std::vector<int32_t> &outPath, const nav_path_policy_t &policy ) {
	/**
	*	A live callback without a declared snapshot representation may never enter the pure solver.
	**/
	if ( policy.edge_cost_callback != nullptr ) {
		if ( s_nav_query_parity ) {
			++s_nav_query_excluded;
		}
		return Nav_FindPathLegacyMainThread( startFace, goalFace, outPath, policy );
	}
	return Nav_FindPathSnapshotCostsMainThread( startFace, goalFace, outPath, policy, {} );
}

/**
*	@brief	Control and inspect opt-in reference/context parity without a normal-gameplay double search.
*	@note	One explicit enable resets counters; a report does not reset or mutate accepted routes.
**/
void Nav_QueryParityCommand( void ) {
	/**
	*	Accept only explicit 0/1 controls; absent controls request a read-only status report.
	**/
	if ( gi.argc() > 2 ) {
		const char *value = gi.argv( 2 );
		if ( Q_stricmp( value, "0" ) != 0 && Q_stricmp( value, "1" ) != 0 ) {
			gi.dprintf( "Usage: sv nav_query_parity [0|1]\n" );
			return;
		}
		s_nav_query_parity = value[ 0 ] == '1';
		if ( s_nav_query_parity ) {
			s_nav_query_compared = s_nav_query_mismatches = s_nav_query_excluded = s_nav_query_resource_limits = 0;
			s_nav_route_compared = s_nav_route_mismatches = 0;
		}
	}
	const bool pass = s_nav_query_compared != 0 && s_nav_query_mismatches == 0 && s_nav_query_resource_limits == 0;
	gi.dprintf( "[nav query parity] enabled=%d compared=%" PRIu64 " mismatches=%" PRIu64 " excluded=%" PRIu64 " resource=%" PRIu64 " verdict=%s\n",
		s_nav_query_parity, s_nav_query_compared, s_nav_query_mismatches, s_nav_query_excluded, s_nav_query_resource_limits,
		pass ? "PASS" : ( s_nav_query_compared == 0 ? "PENDING" : "FAIL" ) );
	gi.dprintf( "[nav route parity] compared=%" PRIu64 " mismatches=%" PRIu64 " verdict=%s\n",
		s_nav_route_compared, s_nav_route_mismatches,
		s_nav_route_compared == 0 ? "PENDING" : ( s_nav_route_mismatches == 0 ? "PASS" : "FAIL" ) );
}

/**
*	@brief	Verify two owner pins, retired-copy reads, blocked replacement and fresh revision publication.
*	@note	Opt-in diagnostic changes only cache/topology revision; live geometry is not moved or shrunk.
**/
void Nav_QuerySnapshotCheckCommand( void ) {
	/**
	*	Hold two independent owner leases and prepare separate readers before retirement.
	**/
	nav_query_snapshot_scope_t first, second;
	if ( first.Get() == nullptr || second.Get() == nullptr ) {
		gi.dprintf( "[nav snapshot lifecycle] verdict=FAIL reason=unavailable\n" );
		return;
	}
	const auto mesh = first.Get()->View();
	if ( mesh.faces.empty() ) {
		gi.dprintf( "[nav snapshot lifecycle] verdict=FAIL reason=empty\n" );
		return;
	}
	svg_nav_query_context_t a, b;
	svg_nav_search_result_t ar, br;
	svg_nav_request_t request;
	a.states.resize( mesh.faces.size() ); b.states.resize( mesh.faces.size() );
	a.heap.reserve( std::max<size_t>( 512, mesh.edges.size() * 2 + 1 ) );
	b.heap.reserve( a.heap.capacity() );
	ar.corridor.reserve( mesh.faces.size() ); br.corridor.reserve( mesh.faces.size() );
	request.start_face = request.goal_face = 0;
	// Prefer a traversable directed pair so retained-copy reads exercise real adjacency and node state.
	bool pair_found = false;
	for ( size_t face = 0; face < mesh.faces.size() && !pair_found; ++face ) {
		const auto &from = mesh.faces[ face ];
		if ( from.excluded ) {
			continue;
		}
		for ( int32_t side = 0; side < from.num_edges; ++side ) {
			const auto &edge = mesh.edges[ from.first_edge + side ];
			if ( edge.neighbor < 0 || edge.disabled || edge.width < NAV_ABSOLUTE_MIN_PORTAL_PASSAGE_WIDTH ||
				 edge.rise > request.step || -edge.rise > request.drop || mesh.faces[ edge.neighbor ].excluded ) {
				continue;
			}
			request.start_face = static_cast<int32_t>( face );
			request.goal_face = edge.neighbor;
			pair_found = true;
			break;
		}
	}
	request.start = mesh.faces[ request.start_face ].center;
	request.goal = mesh.faces[ request.goal_face ].center;
	request.identity.generation = mesh.generation;
	request.identity.topology = mesh.topology;
	const auto initial = Nav_QueryFindPath( mesh, a, request, ar );
	/**
	*	Retire live topology while copied readers remain pinned; rebuilding must fail closed.
	**/
	Nav_InvalidateObstacleCornersTable();
	const auto *blocked_a = Nav_AcquireQuerySnapshotMainThread();
	const bool first_blocked = blocked_a == nullptr;
	Nav_ReleaseQuerySnapshotMainThread( blocked_a );
	const auto retained = Nav_QueryFindPath( mesh, b, request, br );
	const bool copied_read = initial == nav_query_status_t::found && retained == initial && ar.corridor == br.corridor && ar.cost == br.cost &&
		second.Get()->View().generation == mesh.generation && second.Get()->View().topology == mesh.topology;
	/**
	*	One release cannot reclaim another context's backing; the last release permits a fresh publication.
	**/
	first.Release();
	const auto *blocked_b = Nav_AcquireQuerySnapshotMainThread();
	const bool second_blocked = blocked_b == nullptr;
	Nav_ReleaseQuerySnapshotMainThread( blocked_b );
	second.Release();
	// This explicit lifecycle diagnostic drains bounded preparation batches before testing renewed publication.
	while ( !Nav_PrepareQueryInputsMainThread() ) {
		// Gameplay performs one batch per frame; this opt-in probe waits for complete readiness.
	}
	// Do not read the expired old spans after the final release and replacement.
	nav_query_snapshot_scope_t fresh;
	const bool renewed = fresh.Get() != nullptr && fresh.Get()->View().generation == g_nav_mesh_generation &&
		fresh.Get()->View().topology == g_nav_topology_revision;
	const bool pass = first_blocked && second_blocked && copied_read && renewed;
	gi.dprintf( "[nav snapshot lifecycle] verdict=%s pins=2 blocked=%d retained=%d renewed=%d\n",
		pass ? "PASS" : "FAIL", static_cast<int32_t>( first_blocked ) + static_cast<int32_t>( second_blocked ), copied_read, renewed );
}

/**
*	@brief	Exercise real actor revision setters and controlled service rollover on explicit request.
*	@note	The selected actor route is cancelled; no stale route or counter is restored afterwards.
**/
void Nav_QueryIdentityCheckCommand( void ) {
	/**
	*	Require an active monster; diagnostic identity -1 cannot satisfy a production commit contract.
	**/
	const int32_t number = gi.argc() == 3 ? std::atoi( gi.argv( 2 ) ) : -1;
	auto *actor = number >= 0 ? dynamic_cast<svg_monster_base_t *>( g_edict_pool.EdictForNumber( number ) ) : nullptr;
	if ( actor == nullptr || !SVG_Entity_IsActive( actor ) ) {
		gi.dprintf( "[nav identity lifecycle] verdict=FAIL reason=actor\n" );
		return;
	}
	uint32_t checks = 0;
	bool pass = true;
	const auto check = [&]( const bool ok ) { ++checks; pass = pass && ok; };
	/**
	*	Actual goal/state/crowd setters must obsolete a captured reply before new submission.
	**/
	auto old = actor->SVG_Nav_CaptureIdentity();
	check( actor->SVG_Nav_IsIdentityCurrent( old ) );
	actor->navigationGoalRevision = Nav_IssueIdentityRevisionMainThread();
	check( !actor->SVG_Nav_IsIdentityCurrent( old ) );
	old = actor->SVG_Nav_CaptureIdentity();
	actor->ResetNavigationPath();
	check( !actor->SVG_Nav_IsIdentityCurrent( old ) );
	old = actor->SVG_Nav_CaptureIdentity();
	const auto oldGoal = actor->crowd.assignedGoalOrigin;
	actor->crowd.SetNavigationAssignment( actor->crowd.assignedGoalOrigin, oldGoal + Vector3{ 1.0f, 0.0f, 0.0f } );
	check( !actor->SVG_Nav_IsIdentityCurrent( old ) );
	actor->crowd.SetNavigationAssignment( actor->crowd.assignedGoalOrigin, oldGoal );
	check( !actor->SVG_Nav_IsIdentityCurrent( old ) );
	const auto unchanged = actor->crowd.navigationRevision;
	actor->crowd.SetNavigationAssignment( actor->crowd.assignedGoalOrigin, oldGoal );
	check( actor->crowd.navigationRevision == unchanged );
	/**
	*	A reused incarnation and wrong entity slot cannot attach even with otherwise current revisions.
	**/
	old = actor->SVG_Nav_CaptureIdentity();
	auto stale = old;
	stale.incarnation = 0;
	check( !actor->SVG_Nav_IsIdentityCurrent( stale ) );
	stale = old;
	stale.entity = -1;
	check( !actor->SVG_Nav_IsIdentityCurrent( stale ) );
	/**
	*	Force the real rollover boundary without live readers; old replies must fail across the new epoch.
	**/
	const uint64_t epoch = Nav_IdentityEpochMainThread();
	s_nav_identity_revision = UINT64_MAX;
	const auto renewedRevision = Nav_IssueIdentityRevisionMainThread();
	check( renewedRevision == 1 && Nav_IdentityEpochMainThread() == epoch + 1 );
	check( !actor->SVG_Nav_IsIdentityCurrent( old ) );
	check( actor->SVG_Nav_IsIdentityCurrent( actor->SVG_Nav_CaptureIdentity() ) );
	actor->ResetNavigationPath();
	gi.dprintf( "[nav identity lifecycle] verdict=%s checks=%" PRIu32 " overflow=1 epoch=%" PRIu64 "\n", pass ? "PASS" : "FAIL", checks, Nav_IdentityEpochMainThread() );
}

/**
*	@brief	Print the most recent A* rejection counters for a failed debug query.
*	@note	This is deliberately separate from Nav_FindPath so normal gameplay
*			queries remain silent apart from their existing diagnostics.
**/
nav_path_diagnostics_t Nav_CopyLastPathDiagnosticsMainThread( void ) {
	// This transitional copy is owner-only and never read by pure graph or route algorithms.
	Q_assert( Nav_QueryIsOwnerThread() );
	return s_nav_last_path_diagnostics;
}

/**
*	@brief	Print the owner-side compatibility diagnostic copy on explicit request.
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
	// Explicit query diagnostics distinguish infeasibility from a deliberately bounded fallback search.
	gi.dprintf( "[nav repair] status=%" PRIu32 " nodes=%" PRIu32 " edges=%" PRIu32 " samples=%" PRIu32 " span=%" PRIu32 "..%" PRIu32 "\n",
		diagnostics.repair_status, diagnostics.repair_nodes, diagnostics.repair_edges, diagnostics.repair_samples,
		diagnostics.repair_first, diagnostics.repair_last );
	// Exact obligation endpoints expose infeasible hard points without mutating the rejected route.
	gi.dprintf( "[nav repair span] from=(%.6f %.6f %.6f) to=(%.6f %.6f %.6f)\n",
		diagnostics.repair_start.x, diagnostics.repair_start.y, diagnostics.repair_start.z,
		diagnostics.repair_goal.x, diagnostics.repair_goal.y, diagnostics.repair_goal.z );
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
static bool Nav_ClipFaceLineInterval2D( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const nav_face_t &face, const Vector3DP &lineOrigin, const Vector3DP &lineDirection, const double lineLength, const int32_t excludedFaceIdx, const double minimumClearance, double *outMinT, double *outMaxT ) {
	if ( outMinT == nullptr || outMaxT == nullptr || lineLength <= 0.0 ) {
		return false;
	}

	const Vector3DP faceCenter = face.center;
	double windingSign = 0.0;
	for ( int32_t edgeIndex = 0; edgeIndex < face.num_edges; edgeIndex++ ) {
		const nav_halfedge_t &halfEdge = mesh.halfedges[ face.first_edge_idx + edgeIndex ];
		const Vector3DP a = mesh.vertices[ halfEdge.vertex_idx ];
		const Vector3DP b = mesh.vertices[ mesh.halfedges[ halfEdge.next_idx ].vertex_idx ];
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
		const nav_halfedge_t &halfEdge = mesh.halfedges[ face.first_edge_idx + edgeIndex ];
		if ( halfEdge.twin_idx != -1 ) {
			const nav_halfedge_t &twin = mesh.halfedges[ halfEdge.twin_idx ];
			const bool isSharedPortal = ( excludedFaceIdx >= 0 && twin.face_idx == excludedFaceIdx );
			const bool isDisabledPortal = ( ( halfEdge.flags & NAV_EDGE_DISABLED ) != 0 || ( twin.flags & NAV_EDGE_DISABLED ) != 0 );
			const bool isImpassableStep = std::fabs( halfEdge.z_diff ) > NAV_MAX_STEP_HEIGHT;
			if ( isSharedPortal || ( !isDisabledPortal && !isImpassableStep ) ) {
				continue;
			}
		}
		const Vector3DP a = mesh.vertices[ halfEdge.vertex_idx ];
		const Vector3DP b = mesh.vertices[ mesh.halfedges[ halfEdge.next_idx ].vertex_idx ];
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

//! Precomputed cache of all convex obstacle corners in the active navigation mesh.
static svg_vector_t<nav_cached_corner_t, TAG_SVGAME_NAVMESH> s_nav_cached_corners;
//! Precomputed list of all solid obstacle boundary edges across the active navigation mesh.
static svg_vector_t<std::pair<Vector3DP, Vector3DP>, TAG_SVGAME_NAVMESH> s_nav_cached_solid_edges;
//! Whether each legacy cached edge is a strict XY wall; traversable risers/drop-offs need native support policy.
static svg_vector_t<uint8_t, TAG_SVGAME_NAVMESH> s_nav_cached_edge_blocks_xy;
//! Monotonically increasing navmesh generation identifier, bumped on map load, generation, and clear.
uint64_t g_nav_mesh_generation = 1;
//! Monotonically increasing topology revision counter, bumped on door/wall state changes and dynamic obstacle updates.
uint64_t g_nav_topology_revision = 1;

//! Cached generation and topology revision to detect when the navigation mesh is reloaded, regenerated, or dynamically modified.
static uint64_t s_nav_obstacle_cache_generation = 0;
static uint64_t s_nav_obstacle_cache_topology_revision = 0;
//! Explicit validity flag indicating whether s_nav_cached_corners represents the current mesh generation and topology.
static bool s_nav_obstacle_cache_valid = false;
//! Monotonically increasing counter of total obstacle cache rebuild operations performed.
static uint64_t s_nav_obstacle_cache_rebuild_count = 0;

/**
*	@brief	Invalidate the cached obstacle corners and boundary edges table.
**/
void Nav_InvalidateObstacleCornersTable( void ) {
	// Retire copied-query acquisition before exposing a new live topology revision.
	Nav_RetireQuerySnapshotMainThread();
	s_nav_obstacle_cache_valid = false;
	// Topology shares the controlled epoch namespace with actor and request identities.
	g_nav_topology_revision = Nav_IssueIdentityRevisionMainThread();
}

/**
*	@brief	Get total count of obstacle cache rebuild operations performed.
*	@return	Rebuild counter.
**/
uint64_t Nav_GetObstacleCacheRebuildCount( void ) {
	return s_nav_obstacle_cache_rebuild_count;
}

//! Pack a 2D integer cell coordinate into a unique 64-bit spatial hash key.
inline int64_t Nav_SpatialGridKey( const int32_t cellX, const int32_t cellY ) {
	return ( static_cast<int64_t>( cellX ) << 32 ) | ( static_cast<int64_t>( cellY ) & 0xFFFFFFFFLL );
}

//! Spatial grid mapping 2D cell keys to solid edge indices for O(1) line-of-sight tests.
static svg_unordered_map_t<int64_t, svg_vector_t<int32_t, TAG_SVGAME_NAVMESH>, std::hash<int64_t>, std::equal_to<int64_t>, TAG_SVGAME_NAVMESH> s_nav_spatial_grid_edges;
//! Spatial grid mapping 2D cell keys to corner indices for O(1) corner clearance queries.
static svg_unordered_map_t<int64_t, svg_vector_t<int32_t, TAG_SVGAME_NAVMESH>, std::hash<int64_t>, std::equal_to<int64_t>, TAG_SVGAME_NAVMESH> s_nav_spatial_grid_corners;
//! Timestamp / token counters for O(1) query deduplication across adjacent spatial grid cells.
//! Timestamp / token counters for corner deduplication across adjacent spatial grid cells.
//! Token used to deduplicate solid-edge candidates across spatial cells.
//! Token used to deduplicate corner candidates across spatial cells.


//! Owner-prepared boundary roles indexed by half-edge, never written by a route query worker.
static svg_vector_t<uint32_t, TAG_SVGAME_NAVMESH> s_nav_boundary_roles;
//! Preparation cursor and complete input identities for the current collision-dependent cache.
static size_t s_nav_boundary_cursor = 0;
//! Generation/topology whose immutable classification the owner is currently preparing.
static uint64_t s_nav_boundary_generation = 0, s_nav_boundary_topology = 0;

/**
*	@brief	Classify one unpaired boundary against the stable collision world.
*	@note	Called only by bounded owner preparation or the exclusive generation phase.
**/
static uint32_t Nav_ClassifyBoundaryTrace( const nav_halfedge_t &edge ) {
	// Copy edge geometry locally; no entity or mesh mutation occurs during the trace.
	const auto &a = g_nav_vertices[ edge.vertex_idx ];
	const auto &b = g_nav_vertices[ g_nav_halfedges[ edge.next_idx ].vertex_idx ];
	Vector3DP direction = b - a;
	direction.z = 0.0;
	const double length = QM_Vector3LengthDP( direction );
	// Degenerate edges cannot support a reliable outward probe and remain conservative walls.
	if ( length <= 0.001 ) {
		return NAV_EDGE_WALL;
	}
	direction = direction * ( 1.0 / length );
	const auto midpoint = ( a + b ) * 0.5;
	const Vector3 start( static_cast<float>( midpoint.x ), static_cast<float>( midpoint.y ), static_cast<float>( midpoint.z + NAV_MAX_STEP_HEIGHT ) );
	const Vector3 end = start + Vector3( static_cast<float>( direction.y * NAV_BOUNDARY_WALL_PROBE_DIST ), static_cast<float>( -direction.x * NAV_BOUNDARY_WALL_PROBE_DIST ), 0.0f );
	const auto trace = SVG_Trace( start, vec3_origin, vec3_origin, end, nullptr, CM_CONTENTMASK_SOLID );
	return trace.startsolid || trace.fraction < 1.0f ? NAV_EDGE_WALL : NAV_EDGE_DROPOFF;
}

/**
*	@brief	Prepare at most the requested number of boundary edges for owner publication.
*	@return	True only after all current boundary classifications have been copied.
**/
bool Nav_PrepareQueryInputsMainThread( const uint32_t edgeBudget ) {
	/**
	*	Generation exclusively owns live mesh/entity inputs; ordinary owner preparation must wait.
	**/
	if ( !Nav_QueryIsOwnerThread() || Nav_GetGenerationProgress().is_generating ) {
		return false;
	}
	// Restart indexed preparation before exposing inputs from a replaced mesh or dynamic topology.
	if ( s_nav_boundary_generation != g_nav_mesh_generation || s_nav_boundary_topology != g_nav_topology_revision ) {
		s_nav_boundary_roles.assign( g_nav_halfedges.size(), NAV_EDGE_NONE );
		s_nav_boundary_cursor = 0;
		s_nav_boundary_generation = g_nav_mesh_generation;
		s_nav_boundary_topology = g_nav_topology_revision;
	}
	/**
	*	Cap examinations as well as traces; internal edges cannot hide an unbounded scan in one tick.
	**/
	uint32_t examined = 0;
	while ( s_nav_boundary_cursor < g_nav_halfedges.size() && examined < edgeBudget ) {
		const size_t index = s_nav_boundary_cursor++;
		++examined;
		const auto &edge = g_nav_halfedges[ index ];
		// Paired roles depend only on copied adjacency and need no collision probe.
		if ( edge.twin_idx < 0 ) {
			s_nav_boundary_roles[ index ] = Nav_ClassifyBoundaryTrace( edge );
		}
	}
	return s_nav_boundary_cursor == g_nav_halfedges.size();
}

/**
*	@brief	Ensure the global precomputed table of convex obstacle corners and solid boundary edges is initialized.
*	@note	Owner-thread cache is rebuilt for mesh/topology changes; strict clearance queries still scan O(E) entries.
**/
void Nav_EnsureObstacleCornersTable() {
	/**
	*	Reuse only geometry that belongs to the current mesh and dynamic topology revision.
	**/
	if ( s_nav_obstacle_cache_valid &&
		 s_nav_obstacle_cache_generation == g_nav_mesh_generation &&
		 s_nav_obstacle_cache_topology_revision == g_nav_topology_revision ) {
		return;
	}

	// Generation has an exclusive collision lifetime barrier; ordinary routes need completed bounded owner inputs.
	const bool generationWorker = Nav_IsGenerationWorkerThread();
	if ( !generationWorker && !Nav_PrepareQueryInputsMainThread( 0 ) ) {
		return;
	}

	/**
	*	Discard every parallel geometry/role/token buffer before rebuilding a new owned cache.
	**/
	nav_perf_scope_t cache_scope( NAV_PERF_PHASE_CACHE_BUILD );
	s_nav_obstacle_cache_rebuild_count++;
	s_nav_cached_corners.clear();
	s_nav_cached_solid_edges.clear();
	// Rebuild strict role metadata in exact lockstep with the legacy boundary list.
	s_nav_cached_edge_blocks_xy.clear();
	s_nav_spatial_grid_edges.clear();
	s_nav_spatial_grid_corners.clear();


	// Empty mesh revisions own an empty cache; no old edges may survive this transition.
	if ( g_nav_faces.empty() || g_nav_halfedges.empty() || g_nav_vertices.empty() ) {
		s_nav_obstacle_cache_generation = g_nav_mesh_generation;
		s_nav_obstacle_cache_topology_revision = g_nav_topology_revision;
		s_nav_obstacle_cache_valid = true;
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
						// Collision-dependent roles are inputs prepared before owner publication.
						edgeFlags = generationWorker ? Nav_ClassifyBoundaryTrace( he ) :
							s_nav_boundary_roles[ static_cast<size_t>( face.first_edge_idx + e ) ];
					}

					solidEdges.push_back( { v0, v1, u, inNorm, idx0, idx1, edgeFlags } );
					s_nav_cached_solid_edges.push_back( { v0, v1 } );
					// Enabled elevation transitions are not extruded XY walls in either direction; native sweeps enforce actual step/drop limits.
					const bool twinDisabled = he.twin_idx >= 0 && ( g_nav_halfedges[ he.twin_idx ].flags & NAV_EDGE_DISABLED ) != 0;
					const bool blocksXY = he.twin_idx < 0 ? ( edgeFlags & NAV_EDGE_WALL ) != 0 :
						( he.flags & NAV_EDGE_DISABLED ) != 0 || twinDisabled;
					s_nav_cached_edge_blocks_xy.push_back( blocksXY ? uint8_t{ 1 } : uint8_t{ 0 } );
				}
			}
		}
	}

	// 2. Precalculate all convex obstacle corner geometries once for the map:
	// Replace O(E^2) all-pairs comparison with spatial endpoint incidence bucketing.
	// Two edges can only share a corner if their endpoints match horizontally within NAV_CORNER_VERTEX_EPSILON_SQR
	// and vertically within NAV_DROPOFF_ALLOWED_SIZE.
	constexpr double NAV_ENDPOINT_GRID_CELL_SIZE = 128.0;
	constexpr double NAV_ENDPOINT_GRID_INV_CELL_SIZE = 1.0 / NAV_ENDPOINT_GRID_CELL_SIZE;

	// Grid mapping 2D cell coordinate to edge indices that have an endpoint inside or touching that cell:
	svg_unordered_map_t<int64_t, svg_vector_t<int32_t, TAG_SVGAME_NAVMESH>, std::hash<int64_t>, std::equal_to<int64_t>, TAG_SVGAME_NAVMESH> endpoint_grid;

	auto HashEndpointToGrid = [&]( const Vector3DP &pt, const int32_t edge_idx ) {
		// Include 0.5 unit margin around point to account for NAV_CORNER_VERTEX_EPSILON (0.25):
		const int32_t minCX = static_cast<int32_t>( std::floor( ( pt.x - 0.5 ) * NAV_ENDPOINT_GRID_INV_CELL_SIZE ) );
		const int32_t maxCX = static_cast<int32_t>( std::floor( ( pt.x + 0.5 ) * NAV_ENDPOINT_GRID_INV_CELL_SIZE ) );
		const int32_t minCY = static_cast<int32_t>( std::floor( ( pt.y - 0.5 ) * NAV_ENDPOINT_GRID_INV_CELL_SIZE ) );
		const int32_t maxCY = static_cast<int32_t>( std::floor( ( pt.y + 0.5 ) * NAV_ENDPOINT_GRID_INV_CELL_SIZE ) );

		for ( int32_t cy = minCY; cy <= maxCY; ++cy ) {
			for ( int32_t cx = minCX; cx <= maxCX; ++cx ) {
				auto &bucket = endpoint_grid[ Nav_SpatialGridKey( cx, cy ) ];
				if ( bucket.empty() || bucket.back() != edge_idx ) {
					bucket.push_back( edge_idx );
				}
			}
		}
	};

	for ( size_t i = 0; i < solidEdges.size(); ++i ) {
		HashEndpointToGrid( solidEdges[ i ].v0, static_cast<int32_t>( i ) );
		HashEndpointToGrid( solidEdges[ i ].v1, static_cast<int32_t>( i ) );
	}

	svg_vector_t<uint32_t, TAG_SVGAME_NAVMESH> edgePairTokens( solidEdges.size(), 0 );
	uint32_t currentPairQuery = 0;

	auto PointsShareCorner = []( const Vector3DP &p1, const Vector3DP &p2 ) -> bool {
		const double dx = p1.x - p2.x;
		const double dy = p1.y - p2.y;
		return ( ( dx * dx + dy * dy ) <= NAV_CORNER_VERTEX_EPSILON_SQR && std::fabs( p1.z - p2.z ) <= static_cast<double>( NAV_DROPOFF_ALLOWED_SIZE ) );
	};

	for ( size_t i = 0; i < solidEdges.size(); ++i ) {
		const auto &e1 = solidEdges[ i ];
		currentPairQuery++;

		// Gather unique candidate partner edges from the spatial cells covering e1's endpoints:
		const Vector3DP endpoints[ 2 ] = { e1.v0, e1.v1 };
		for ( int32_t p = 0; p < 2; ++p ) {
			const int32_t cx = static_cast<int32_t>( std::floor( endpoints[ p ].x * NAV_ENDPOINT_GRID_INV_CELL_SIZE ) );
			const int32_t cy = static_cast<int32_t>( std::floor( endpoints[ p ].y * NAV_ENDPOINT_GRID_INV_CELL_SIZE ) );
			const auto it = endpoint_grid.find( Nav_SpatialGridKey( cx, cy ) );
			if ( it == endpoint_grid.end() ) {
				continue;
			}

			const auto &candidateIndices = it->second;
			for ( const int32_t candIdx : candidateIndices ) {
				// Only consider pairs where j > i to avoid duplicate pair processing:
				if ( candIdx <= static_cast<int32_t>( i ) ) {
					continue;
				}
				if ( edgePairTokens[ candIdx ] == currentPairQuery ) {
					continue;
				}
				edgePairTokens[ candIdx ] = currentPairQuery;

				const auto &e2 = solidEdges[ candIdx ];

				// Enforce structural edge compatibility:
				// An obstacle wall edge must only pair with another obstacle wall edge.
				// A drop-off ledge edge must only pair with another drop-off ledge edge.
				if ( ( e1.edgeFlags & ( NAV_EDGE_WALL | NAV_EDGE_DROPOFF ) ) != ( e2.edgeFlags & ( NAV_EDGE_WALL | NAV_EDGE_DROPOFF ) ) ) {
					continue;
				}

				// Find shared corner vertex between the two solid boundary edges.
				Vector3DP vCorner = {};
				Vector3DP farPt1 = {};
				Vector3DP farPt2 = {};
				bool sharesVertex = false;

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
	}

	// 3. Populate spatial hash grid buckets for O(1) solid edge lookups:

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
					// Components represent adjacency only; request-specific rise/drop limits remain in A*.
					// A permissive drop policy must never be rejected by a default-drop partition.
					s_nav_face_components[ neighborIdx ] = currentComponent;
					bfsQueue.push_back( neighborIdx );
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

	// 8. Commit cache generation, topology revision, and validity flag:
	s_nav_obstacle_cache_generation = g_nav_mesh_generation;
	s_nav_obstacle_cache_topology_revision = g_nav_topology_revision;
	s_nav_obstacle_cache_valid = true;
}

/**
*	@brief	Deep-copy prepared native route geometry into private snapshot backing.
*	@note	Owner-only container construction/destruction never runs in a query worker.
**/
void Nav_CaptureRouteSnapshotMainThread( svg_nav_mesh_snapshot_t &snapshot ) {
	// Every copied index resolves within these copied arrays rather than live mesh containers.
	snapshot.native_vertices.assign( g_nav_vertices.begin(), g_nav_vertices.end() );
	snapshot.native_halfedges.assign( g_nav_halfedges.begin(), g_nav_halfedges.end() );
	snapshot.native_faces.assign( g_nav_faces.begin(), g_nav_faces.end() );
	snapshot.native_nodes.assign( g_nav_nodes.begin(), g_nav_nodes.end() );
	snapshot.native_rooms.resize( g_nav_rooms.size() );
	// Room classification is copied without the room's nested live index vectors.
	for ( size_t index = 0; index < g_nav_rooms.size(); ++index ) {
		snapshot.native_rooms[ index ].zone_type = g_nav_rooms[ index ].zone_type;
	}
	// Classification is complete before publication; no query writes to these tables.
	snapshot.native_corners = s_nav_cached_corners;
	snapshot.native_solid_edges = s_nav_cached_solid_edges;
	snapshot.native_edge_blocks_xy = s_nav_cached_edge_blocks_xy;
	snapshot.native_components = s_nav_face_components;
	snapshot.native_widths = s_nav_edge_portal_widths;
	// Deep copy each nested bucket so later native cache rebuilds cannot mutate a pinned view.
	snapshot.native_grid_edges = s_nav_spatial_grid_edges;
	snapshot.native_grid_corners = s_nav_spatial_grid_corners;
	// Prepare nearest-boundary acceleration before publication; queries never mutate or allocate this tree.
	snapshot.native_clearance_tree_ready = Nav_BuildClearanceTree( snapshot.native_solid_edges, snapshot.native_edge_blocks_xy,
		snapshot.native_clearance_indices, snapshot.native_clearance_nodes );
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
bool Nav_CircleCastHalfEdge2D( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const Vector3DP &start, const Vector3DP &dirNorm, const double maxDistance, const double agentRadius, nav_raycast_result_t *outResult ) {
	if ( !outResult || maxDistance <= 0.0 ) {
		return false;
	}

	// Copied obstacle classification is immutable for this lease.

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
	int32_t startFaceIdx = Nav_FindFaceInLeafStrict( mesh, context, start );
	if ( startFaceIdx < 0 || startFaceIdx >= static_cast<int32_t>( mesh.faces.size() ) ) {
		Vector3DP feetStart = start;
		feetStart.z -= CROWD_SLOT_FEET_SNAP_OFFSET_Z;
		startFaceIdx = Nav_FindFaceInLeafStrict( mesh, context, feetStart );
	}
	if ( startFaceIdx >= 0 && startFaceIdx < static_cast<int32_t>( mesh.faces.size() ) ) {
		outResult->hitFaceIndex = startFaceIdx;
		const int32_t rId = mesh.faces[ startFaceIdx ].room_id;
		if ( rId >= 0 && rId < static_cast<int32_t>( mesh.rooms.size() ) ) {
			outResult->zone_type = mesh.rooms[ rId ].zone_type;
		}
	}

	// Phase 1: Direct half-edge polygon graph traversal from face to face:
	if ( startFaceIdx >= 0 && startFaceIdx < static_cast<int32_t>( mesh.faces.size() ) ) {
		int32_t currentFaceIdx = startFaceIdx;
		int32_t prevHalfedgeIdx = -1;
		double distTraversed = 0.0;
		Vector3DP currentPos = start;
		const int32_t currentRoomId = mesh.faces[ startFaceIdx ].room_id;

		//! Maximum topological face hops permitted per raycast to bound execution time in O(1).
		static constexpr int32_t MAX_RAYCAST_FACE_HOPS = 32;

		for ( int32_t hop = 0; hop < MAX_RAYCAST_FACE_HOPS && distTraversed < maxDistance; hop++ ) {
			const nav_face_t &face = mesh.faces[ currentFaceIdx ];
			int32_t bestExitEdge = -1;
			double bestExitT = 999999.0;
			Vector3DP bestHitPoint = {};
			Vector3DP bestHitNormal = {};

			int32_t curEdge = face.first_edge_idx;
			for ( int32_t e = 0; e < face.num_edges; e++ ) {
				if ( curEdge < 0 || curEdge >= static_cast<int32_t>( mesh.halfedges.size() ) ) {
					break;
				}
				if ( curEdge == prevHalfedgeIdx ) {
					curEdge = mesh.halfedges[ curEdge ].next_idx;
					continue;
				}

				const nav_halfedge_t &he = mesh.halfedges[ curEdge ];
				const Vector3DP &v0 = mesh.vertices[ he.vertex_idx ];
				const int32_t nextHeIdx = he.next_idx;
				const Vector3DP &v1 = ( nextHeIdx >= 0 && nextHeIdx < static_cast<int32_t>( mesh.halfedges.size() ) ) ? mesh.vertices[ mesh.halfedges[ nextHeIdx ].vertex_idx ] : v0;

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

			const nav_halfedge_t &exitHe = mesh.halfedges[ bestExitEdge ];
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
			const nav_halfedge_t &twinHe = mesh.halfedges[ exitHe.twin_idx ];
			const int32_t neighborFaceIdx = twinHe.face_idx;
			const int32_t neighborRoomId = ( neighborFaceIdx >= 0 && neighborFaceIdx < static_cast<int32_t>( mesh.faces.size() ) ) ? mesh.faces[ neighborFaceIdx ].room_id : -1;

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
			if ( effectiveRadius > 0.0 && static_cast<size_t>( bestExitEdge ) < mesh.widths.size() ) {
				const double portalWidth = mesh.widths[ bestExitEdge ];
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

	const uint32_t queryToken = Nav_RouteNextStamp( context.edge_stamp, context.edge_tokens );
	double closestHitDist = outResult->hitDistance;
	bool hitFound = false;

	for ( int32_t cy = minCY; cy <= maxCY; ++cy ) {
		for ( int32_t cx = minCX; cx <= maxCX; ++cx ) {
			const auto it = ( *mesh.grid_edges ).find( Nav_SpatialGridKey( cx, cy ) );
			if ( it == ( *mesh.grid_edges ).end() ) {
				continue;
			}

			for ( const int32_t edgeIdx : it->second ) {
				if ( context.edge_tokens[ edgeIdx ] == queryToken ) {
					continue;
				}
				context.edge_tokens[ edgeIdx ] = queryToken;

				const auto &edge = mesh.solid_edges[ edgeIdx ];
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
bool Nav_RaycastHalfEdge2D( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const Vector3DP &start, const Vector3DP &dirNorm, const double maxDistance, nav_raycast_result_t *outResult ) {
	return Nav_CircleCastHalfEdge2D( mesh, context, start, dirNorm, maxDistance, 0.0, outResult );
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
void Nav_RaycastRoomBoundary2D( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const Vector3DP &anchor, const double maxRadius, const int32_t numRays, double *outDoorYaw, bool *outHasDoor, double *outMinWallDist, double *outAvgWallDist, nav_zone_type_t *outZoneType, const double agentRadius ) {
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
		Nav_CircleCastHalfEdge2D( mesh, context, anchor, dir, maxRadius, agentRadius, &res );

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
bool Nav_HasGeometricLineOfSight2D( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const Vector3DP &p0, const Vector3DP &p1, const double clearanceMargin ) {

	// Copied obstacle classification is immutable for this lease.

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

	const uint32_t queryToken = Nav_RouteNextStamp( context.edge_stamp, context.edge_tokens );

	for ( int32_t cy = minCY; cy <= maxCY; ++cy ) {
		for ( int32_t cx = minCX; cx <= maxCX; ++cx ) {
			const auto it = ( *mesh.grid_edges ).find( Nav_SpatialGridKey( cx, cy ) );
			if ( it == ( *mesh.grid_edges ).end() ) {
				continue;
			}

			for ( const int32_t edgeIdx : it->second ) {
				if ( context.edge_tokens[ edgeIdx ] == queryToken ) {
					continue;
				}
				context.edge_tokens[ edgeIdx ] = queryToken;

				const auto &edge = mesh.solid_edges[ edgeIdx ];
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
*	@brief	Evaluate strict disk clearance against the current prepared boundary cache.
*	@param	p0	Exact start feet position.
*	@param	p1	Exact end feet position.
*	@param	clearance	Required planar radius including policy margin.
*	@return	Revision-tagged reason and closest boundary witness.
*	@note	Pure geometry after cache preparation; conservative broad-phase pruning preserves the exact nearest witness.
**/
nav_segment_clearance_t Nav_TestSegmentClearance( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const Vector3DP &p0, const Vector3DP &p1, const double clearance ) {
	/**
	*	Reject invalid geometry before arithmetic, and attach current revision identity.
	**/
	nav_segment_clearance_t result;
	result.mesh_generation = mesh.generation;
	result.topology_revision = mesh.topology;
	// Nonfinite coordinates and negative clearance cannot authorize traversal.
	if ( !std::isfinite( p0.x ) || !std::isfinite( p0.y ) || !std::isfinite( p0.z ) ||
		 !std::isfinite( p1.x ) || !std::isfinite( p1.y ) || !std::isfinite( p1.z ) ||
		 !std::isfinite( clearance ) || clearance < 0.0 ) {
		return result;
	}

	/**
	*	Refuse stale/unprepared geometry rather than rebuilding inside a movement query.
	**/
	// The lease pins publication identity; complete classification must match every copied boundary.
	if ( mesh.edge_blocks_xy.size() != mesh.solid_edges.size() ) {
		result.reason = nav_clearance_reason_t::cache_unavailable;
		return result;
	}

	/**
	*	Measure every relevant closed boundary segment, including stationary movement.
	**/
	const double minZ = std::min( p0.z, p1.z ) - NAV_MAX_STEP_HEIGHT;
	const double maxZ = std::max( p0.z, p1.z ) + NAV_MAX_STEP_HEIGHT;
	double minimumSqr = std::numeric_limits<double>::infinity();
	/**
	*	Search complete immutable bounds before falling back to the ordered reference scan.
	*	Exact distances, first-edge ties and elevation filtering are retained with stack-only scratch.
	**/
	int32_t treeWitness = -1;
	if ( mesh.clearance_tree_ready && Nav_QueryClearanceTree( mesh.clearance_nodes, mesh.solid_edges,
		p0, p1, minZ, maxZ, minimumSqr, treeWitness ) ) {
		result.witness_edge = treeWitness;
		// Empty eligible geometry retains infinite clearance without inventing a boundary witness.
		if ( treeWitness >= 0 ) {
			result.witness_start = mesh.solid_edges[ treeWitness ].first;
			result.witness_end = mesh.solid_edges[ treeWitness ].second;
		}
		result.minimum_clearance = std::sqrt( minimumSqr );
		result.reason = ( minimumSqr == 0.0 || result.minimum_clearance < clearance ) ?
			nav_clearance_reason_t::boundary_contact : nav_clearance_reason_t::clear;
		return result;
	}
	// Discard partial tree results before unsupported/corrupt geometry takes the exhaustive path.
	minimumSqr = std::numeric_limits<double>::infinity();
	// This linear reference avoids shared query stamps and spatial-grid omission risks.
	for ( size_t index = 0; index < mesh.solid_edges.size(); ++index ) {
		// Legacy refinement includes risers and ledges; strict flat walls exclude those native-policy transitions.
		if ( mesh.edge_blocks_xy[ index ] == 0 ) {
			continue;
		}
		const auto &edge = mesh.solid_edges[ index ];
		// Boundaries from unrelated elevations cannot constrain this floor's XY sweep.
		if ( std::max( edge.first.z, edge.second.z ) < minZ || std::min( edge.first.z, edge.second.z ) > maxZ ) {
			continue;
		}
		// Corrupt boundary coordinates or overflowed geometry cannot prove clearance.
		if ( !std::isfinite( edge.first.x ) || !std::isfinite( edge.first.y ) || !std::isfinite( edge.first.z ) ||
			 !std::isfinite( edge.second.x ) || !std::isfinite( edge.second.y ) || !std::isfinite( edge.second.z ) ) {
			return result;
		}
		// Distant boundaries cannot improve the exact witness; retain all near/tied and extreme cases.
		if ( !Nav_ClearanceSegmentMayImprove( p0, p1, edge.first, edge.second, minimumSqr ) ) {
			continue;
		}
		const double distanceSqr = Nav_ClearanceSegmentDistanceSqr( p0, p1, edge.first, edge.second );
		// Reject arithmetic failure rather than treating an unmeasured edge as clear.
		if ( !std::isfinite( distanceSqr ) ) {
			return result;
		}
		// Keep the actual nearest witness, even when an earlier edge already failed clearance.
		if ( distanceSqr < minimumSqr ) {
			minimumSqr = distanceSqr;
			result.witness_edge = static_cast<int32_t>( index );
			result.witness_start = edge.first;
			result.witness_end = edge.second;
		}
	}

	/**
	*	Report exact-contact as blocked at zero radius; positive clearance permits equality.
	**/
	result.minimum_clearance = std::sqrt( minimumSqr );
	result.reason = ( minimumSqr == 0.0 || result.minimum_clearance < clearance ) ?
		nav_clearance_reason_t::boundary_contact : nav_clearance_reason_t::clear;
	return result;
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

static bool Nav_ClipPortalForAgentClearance( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const int32_t faceAIdx, const int32_t faceBIdx, const Vector3DP &portalLeft, const Vector3DP &portalRight, const double wallClearance, const double cornerClearance, Vector3DP *outLeft, Vector3DP *outRight, bool *outIsNarrowPortal ) {
	/**
	*	Sanity checks on inputs.
	**/
	if ( outLeft == nullptr || outRight == nullptr || wallClearance < 0.0 || cornerClearance < 0.0 || faceAIdx < 0 || faceBIdx < 0 ||
		static_cast<size_t>( faceAIdx ) >= mesh.faces.size() || static_cast<size_t>( faceBIdx ) >= mesh.faces.size() ) {
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
	// Owner-classified obstacle geometry is immutable for this lease.

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

	const uint32_t cornerToken = Nav_RouteNextStamp( context.corner_stamp, context.corner_tokens );
	context.clip_local_corners.clear();
	if ( context.clip_local_corners.capacity() < 32 ) {
		context.clip_local_corners.reserve( 32 );
	}

	for ( int32_t cy = minCY; cy <= maxCY; ++cy ) {
		for ( int32_t cx = minCX; cx <= maxCX; ++cx ) {
			const auto it = ( *mesh.grid_corners ).find( Nav_SpatialGridKey( cx, cy ) );
			if ( it == ( *mesh.grid_corners ).end() ) {
				continue;
			}
			for ( const int32_t cIdx : it->second ) {
				if ( context.corner_tokens[ cIdx ] == cornerToken ) {
					continue;
				}
				context.corner_tokens[ cIdx ] = cornerToken;
				context.clip_local_corners.push_back( static_cast<uint32_t>( cIdx ) );
			}
		}
	}

	// Resolve only indices from the current pinned publication; retained scratch owns no mesh pointers.
	for ( const uint32_t cornerIndex : context.clip_local_corners ) {
		Q_assert( cornerIndex < mesh.corners.size() );
		const auto &c = mesh.corners[ cornerIndex ];
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
		const nav_face_t &face = mesh.faces[ fIdx ];
		for ( int32_t e = 0; e < face.num_edges; ++e ) {
			const nav_halfedge_t &he = mesh.halfedges[ face.first_edge_idx + e ];
			if ( he.twin_idx != -1 ) {
				const int32_t neighborFace = mesh.halfedges[ he.twin_idx ].face_idx;
				if ( neighborFace == faceAIdx || neighborFace == faceBIdx ) {
					continue; // Skip the portal edge connecting faceA and faceB
				}
			}

			const bool isSolid = ( he.twin_idx == -1 || ( he.flags & NAV_EDGE_DISABLED ) != 0 || std::fabs( he.z_diff ) > NAV_SOLID_EDGE_MIN_Z_DIFF );
			if ( isSolid ) {
				const Vector3DP &v0 = mesh.vertices[ he.vertex_idx ];
				const Vector3DP &v1 = mesh.vertices[ mesh.halfedges[ he.next_idx ].vertex_idx ];
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
int32_t Nav_FindClosestFaceInLeaf( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const Vector3DP &point ) {
	/**
	*	Prefer local KD-tree face lookup for stable corner progression.
	**/
	const int32_t leafFace = Nav_FindPolyInLeaf( mesh, context, point );
	if ( leafFace >= 0 && static_cast< size_t >( leafFace ) < mesh.faces.size() ) {
		return leafFace;
	}

	/**
	*	Fallback to global closest poly lookup only if leaf search completely failed.
	**/
	return Nav_FindClosestPolyGlobal( mesh, context, point );
}

/**
*	@brief	Get the connected component partition ID of a navigation face.
*	@param	faceIdx	Index of the nav face.
*	@return	Component identifier, or -1 if invalid.
**/
int32_t Nav_GetFaceComponent( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const int32_t faceIdx ) {
	if ( faceIdx >= 0 && static_cast<size_t>( faceIdx ) < mesh.components.size() ) {
		return mesh.components[ faceIdx ];
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
int32_t Nav_FindReachableFaceInLeaf( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const Vector3DP &point, const int32_t targetFace, const double agentRadius ) {
	const int32_t primaryFace = Nav_FindClosestFaceInLeaf( mesh, context, point );
	if ( primaryFace < 0 || targetFace < 0 ||
		 static_cast<size_t>( primaryFace ) >= mesh.components.size() ||
		 static_cast<size_t>( targetFace ) >= mesh.components.size() ) {
		return primaryFace;
	}

	const int32_t targetComp = mesh.components[ targetFace ];
	if ( targetComp >= 0 && mesh.components[ primaryFace ] == targetComp ) {
		return primaryFace;
	}

	// Primary face is either disconnected or on a different component (e.g. an isolated sliver polygon).
	// Search candidate faces in the local KD-tree leaf that are reachable to targetComp and overlap the agent's hull.
	const int32_t leafIdx = Nav_FindLeafNode( mesh, context, point );
	if ( leafIdx >= 0 && leafIdx < static_cast<int32_t>( mesh.nodes.size() ) ) {
		const nav_kdtree_node_t &leaf = mesh.nodes[ leafIdx ];
		const int32_t firstFaceIdx = leaf.first_face_id;
		const double maxFootprintRadius = ( agentRadius > 0.0 ) ? ( agentRadius * 2.0 ) : 32.0;
		const double maxFootprintDistSqr = maxFootprintRadius * maxFootprintRadius;

		int32_t bestReachableFace = -1;
		double bestDistSqr = 999999.0;

		for ( int32_t i = 0; i < leaf.num_faces; ++i ) {
			const int32_t candFaceIdx = firstFaceIdx + i;
			if ( candFaceIdx < 0 || static_cast<size_t>( candFaceIdx ) >= mesh.faces.size() ) {
				break;
			}
			if ( static_cast<size_t>( candFaceIdx ) < mesh.components.size() && mesh.components[ candFaceIdx ] == targetComp ) {
				const nav_face_t &candFace = mesh.faces[ candFaceIdx ];
				if ( ( candFace.surface_flags & ( CM_SURFACE_NO_NAVMESH | CONTENTS_NO_NAVMESH ) ) != 0 ) {
					continue;
				}
				if ( Nav_PointInsideFace2D( mesh, context, point, candFace ) ) {
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
	if ( primaryFace >= 0 && static_cast<size_t>( primaryFace ) < mesh.faces.size() ) {
		const nav_face_t &pf = mesh.faces[ primaryFace ];
		for ( int32_t e = 0; e < pf.num_edges; ++e ) {
			const nav_halfedge_t &he = mesh.halfedges[ pf.first_edge_idx + e ];
			if ( he.twin_idx != -1 ) {
				const int32_t nbrFace = mesh.halfedges[ he.twin_idx ].face_idx;
				if ( nbrFace >= 0 && static_cast<size_t>( nbrFace ) < mesh.components.size() ) {
					if ( mesh.components[ nbrFace ] == targetComp ) {
						return nbrFace;
					}
				}
			}
		}
	}

	return primaryFace;
}

//! Corner information structure extracted along the path corridor for obstacle avoidance.

//! Intersection record for an obstacle corner projected onto a path polyline segment.

//! Persistent static buffer for gathering obstacle corners along a path corridor.
//! Persistent static buffer for spatial grid corner queries along the corridor.
//! Persistent static buffer for refined path waypoints with obstacle standoff points.
//! Persistent static buffer for tracking forced waypoint status across corner refinements.
//! Persistent static buffer for corners intersecting a single path segment.

/**
*	@brief	Enforce strict agent physical clearance around convex obstacle corners.
*	@param	path			[in] Sequence of face indices describing the path corridor.
*	@param	agentRadius		[in] Radius of the agent's collision hull (mins/maxs half-width).
*	@param	waypoints		[in,out] Waypoints list to enforce convex corner clearance for.
*	@param	forcedWaypoints	[in,out] Parallel boolean flags matching waypoints that are strictly forced.
*	@param	mins	Native minimum bounds used for candidate links.
*	@param	maxs	Native maximum bounds used for candidate links.
*	@param	shape	Explicit native capsule/cylinder shape.
*	@param	policy	Unchanged traversal limits.
*	@note	Owner-thread reference uses spatial candidate gathering and complete native candidate-link proof; no O(1) query claim.
**/
static void Nav_EnforceConvexCornerWaypoints( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const std::vector<int32_t> &path, const double agentRadius, std::vector<Vector3DP> &waypoints, std::vector<bool> *forcedWaypoints, const Vector3 &mins, const Vector3 &maxs, const int32_t shape, const nav_path_policy_t &policy, svg_base_edict_t *passEntity ) {
	nav_perf_scope_t corner_scope( NAV_PERF_PHASE_CORNER_REFINEMENT );
	if ( waypoints.size() < 2 || path.empty() ) {
		return;
	}

	// Owner-classified obstacle geometry is immutable for this lease.
	if ( mesh.corners.empty() ) {
		return;
	}

	// Base standoff clearance distance scaled dynamically by entity bounding box radius (mins/maxs half-width).
	// Uses standard entity default (NAV_DEFAULT_AGENT_RADIUS) plus NAV_CORNER_CLEARANCE_MARGIN (12.0) = 28.0 units when radius is unspecified.
	const double effectiveAgentRadius = ( agentRadius > 0.0 ) ? agentRadius : NAV_DEFAULT_AGENT_RADIUS;
	const double requiredClearance = effectiveAgentRadius + NAV_CORNER_CLEARANCE_MARGIN;
	/**
	*	Candidate links share the production complete native hull/support contract.
	*	A ramp link cannot be proved by extruding distant XY boundaries through every elevation.
	**/
	const auto clearLink = [&]( const Vector3DP &from, const Vector3DP &to ) {
		// Intermediate nav-surface points need a contact probe; the retained endpoints themselves remain exact.
		const auto startSupport = Nav_ResolveHullGoalSupport( mesh, context, from, effectiveAgentRadius, shape );
		// Two-point probes need no heap/container allocation.
		const std::array<Vector3DP, 2> link{ startSupport.probe, to };
		return Nav_ValidateHullRouteNativeMainThread( mesh, context, link, mins, maxs, shape, policy, passEntity, false ).reason == nav_route_reason_t::clear;
	};


	// 1. Gather precomputed corners relevant to the path 3D bounding box using this request's exclusive context:
	context.corridor_corners.clear();
	if ( context.corridor_corners.capacity() < 128 ) {
		context.corridor_corners.reserve( 128 );
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

	const uint32_t cornerToken = Nav_RouteNextStamp( context.corner_stamp, context.corner_tokens );
	context.candidate_corners.clear();
	if ( context.candidate_corners.capacity() < 128 ) {
		context.candidate_corners.reserve( 128 );
	}

	for ( int32_t cy = minCY; cy <= maxCY; ++cy ) {
		for ( int32_t cx = minCX; cx <= maxCX; ++cx ) {
			const auto it = ( *mesh.grid_corners ).find( Nav_SpatialGridKey( cx, cy ) );
			if ( it == ( *mesh.grid_corners ).end() ) {
				continue;
			}
			for ( const int32_t cIdx : it->second ) {
				if ( context.corner_tokens[ cIdx ] == cornerToken ) {
					continue;
				}
				context.corner_tokens[ cIdx ] = cornerToken;
				context.candidate_corners.push_back( static_cast<uint32_t>( cIdx ) );
			}
		}
	}

	// Resolve only indices from the current pinned publication; retained scratch owns no mesh pointers.
	for ( const uint32_t cornerIndex : context.candidate_corners ) {
		Q_assert( cornerIndex < mesh.corners.size() );
		const auto &c = mesh.corners[ cornerIndex ];
		if ( c.vertex.x < wpMin.x || c.vertex.x > wpMax.x ||
			 c.vertex.y < wpMin.y || c.vertex.y > wpMax.y ||
			 c.maxZ < wpMin.z || c.minZ > wpMax.z ) {
			continue;
		}

		const double normalA = QM_Vector3DotProductDP( c.bisectorNorm, c.n1 );
		const double normalB = QM_Vector3DotProductDP( c.bisectorNorm, c.n2 );
		const double physicalMinimum = Nav_RequiredBisectorDistance( effectiveAgentRadius, 0.0, normalA, normalB );
		// No common inward side means this bisector cannot represent a safe hull location.
		if ( !std::isfinite( physicalMinimum ) ) {
			continue;
		}
		double standoffDist = Nav_RequiredBisectorDistance( effectiveAgentRadius, NAV_CORNER_CLEARANCE_MARGIN, normalA, normalB );

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

		const uint32_t rayToken = Nav_RouteNextStamp( context.edge_stamp, context.edge_tokens );
		for ( int32_t cy = rayMinCY; cy <= rayMaxCY; ++cy ) {
			for ( int32_t cx = rayMinCX; cx <= rayMaxCX; ++cx ) {
				const auto it = ( *mesh.grid_edges ).find( Nav_SpatialGridKey( cx, cy ) );
				if ( it == ( *mesh.grid_edges ).end() ) {
					continue;
				}
				for ( const int32_t edgeIdx : it->second ) {
					if ( context.edge_tokens[ edgeIdx ] == rayToken ) {
						continue;
					}
					context.edge_tokens[ edgeIdx ] = rayToken;

					const auto &edge = mesh.solid_edges[ edgeIdx ];
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
			const double minSafeDist = physicalMinimum + 0.0001;
			standoffDist = std::clamp( minHitDist * NAV_STANDOFF_SCALE_HALF, minSafeDist, standoffDist );
		}

		Vector3DP standoffMid = c.vertex + c.bisectorNorm * standoffDist;
		standoffMid.z = c.vertex.z;

		// Ensure the standoff point actually lies on a valid walkable navigation mesh face within the local leaf:
		int32_t faceIdx = Nav_FindClosestFaceInLeaf( mesh, context, standoffMid );
		if ( faceIdx == -1 || !Nav_PointInsideFace2D( mesh, context, standoffMid, mesh.faces[ faceIdx ] ) ) {
			// If standoff is slightly outside due to polygon boundary bevels or tight partitioning,
			// try a slightly contracted standoff distance (e.g. 0.75x or 0.5x) before giving up!
			bool foundFallback = false;
			for ( const double scale : { NAV_STANDOFF_SCALE_SEVENTY_FIVE, NAV_STANDOFF_SCALE_HALF } ) {
				const double testDist = standoffDist * scale;
				if ( testDist < physicalMinimum ) {
					break;
				}
				const Vector3DP testMid = c.vertex + c.bisectorNorm * testDist;
				const int32_t testFaceIdx = Nav_FindClosestFaceInLeaf( mesh, context, testMid );
				if ( testFaceIdx != -1 && Nav_PointInsideFace2D( mesh, context, testMid, mesh.faces[ testFaceIdx ] ) &&
					 Nav_TestSegmentClearance( mesh, context, testMid, testMid, effectiveAgentRadius ).reason == nav_clearance_reason_t::clear ) {
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
		if ( faceIdx >= 0 && static_cast<size_t>( faceIdx ) < mesh.faces.size() ) {
			const nav_face_t &face = mesh.faces[ faceIdx ];
			if ( std::fabs( face.normal.z ) > 0.001 ) {
				const Vector3DP v0 = mesh.vertices[ mesh.halfedges[ face.first_edge_idx ].vertex_idx ];
				const double planeD = QM_Vector3DotProductDP( v0, face.normal );
				standoffMid.z = ( planeD - face.normal.x * standoffMid.x - face.normal.y * standoffMid.y ) / face.normal.z;
			}
		}

		// Ensure standoff is connected to the corner through open space (not through a solid brush into another room):
		// This point ray classifies the free side of the corner vertex; it does not authorize hull movement.
		// Accepted snap/arc links below use complete closed segments at the unchanged physical radius.
		const Vector3DP testStart = c.vertex + c.bisectorNorm * 1.0;
		if ( !Nav_HasGeometricLineOfSight2D( mesh, context, testStart, standoffMid, 0.0 ) ) {
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

		context.corridor_corners.push_back( { c.vertex, standoffMid, standoffDist, c.bisectorNorm, c.u1, c.u2, c.n1, c.n2, c.minZ, c.maxZ, c.cornerType, static_cast<int32_t>( cornerIndex ) } );
	}

	if ( context.corridor_corners.empty() ) {
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

		for ( const auto &corner : context.corridor_corners ) {
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
					const double minDist = Nav_RequiredBisectorDistance( effectiveAgentRadius, 0.0, QM_Vector3DotProductDP( corner.bisectorNorm, corner.n1 ), QM_Vector3DotProductDP( corner.bisectorNorm, corner.n2 ) );
					if ( testDist < minDist ) {
						break;
					}

					Vector3DP candMid = corner.vertex + corner.bisectorNorm * testDist;
					candMid.z = waypoints[ i ].z;
					int32_t candFaceIdx = Nav_FindFaceInLeafStrict( mesh, context, candMid );
					if ( candFaceIdx < 0 ) {
						Vector3DP feetTest = candMid;
						feetTest.z -= NAV_STANDOFF_FEET_SNAP_OFFSET_Z;
						candFaceIdx = Nav_FindFaceInLeafStrict( mesh, context, feetTest );
					}
					if ( candFaceIdx < 0 ) {
						candFaceIdx = Nav_FindClosestFaceInLeaf( mesh, context, candMid );
						if ( candFaceIdx >= 0 && !Nav_PointInsideFace2D( mesh, context, candMid, mesh.faces[ candFaceIdx ] ) ) {
							candFaceIdx = -1;
						}
					}

					if ( candFaceIdx >= 0 && candFaceIdx < static_cast<int32_t>( mesh.faces.size() ) ) {
						const nav_face_t &face = mesh.faces[ candFaceIdx ];
						if ( std::fabs( face.normal.z ) > 0.001 ) {
							const Vector3DP v0 = mesh.vertices[ mesh.halfedges[ face.first_edge_idx ].vertex_idx ];
							const double planeD = QM_Vector3DotProductDP( v0, face.normal );
							candMid.z = ( planeD - face.normal.x * candMid.x - face.normal.y * candMid.y ) / face.normal.z;
						}
					} else {
						continue;
					}

					// Nav containment is point-only; require the unchanged hull radius against every prepared wall.
					if ( Nav_TestSegmentClearance( mesh, context, candMid, candMid, effectiveAgentRadius ).reason != nav_clearance_reason_t::clear ) {
						continue;
					}
					bool candSafe = true;
					if ( i > 0 ) {
						if ( !clearLink( waypoints[ i - 1 ], candMid ) ) {
							candSafe = false;
						}
					}
					if ( candSafe && i + 1 < waypoints.size() ) {
						if ( !clearLink( candMid, waypoints[ i + 1 ] ) ) {
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
					Nav_RecordWaypointConstraint( context, bestStandoff, NAV_WAYPOINT_CORNER_APEX | NAV_WAYPOINT_MANDATORY, -1, corner.sourceCorner, -1 );
					if ( forcedWaypoints != nullptr && i < forcedWaypoints->size() ) {
						( *forcedWaypoints )[ i ] = true;
					}
					break;
				}
			}
		}
	}

	// 4. Single-pass insertion of standoff waypoints for segments that violate corner clearance:
	context.refined_waypoints.clear();
	context.refined_forced.clear();
	if ( context.refined_waypoints.capacity() < ( waypoints.size() + context.corridor_corners.size() + 16 ) ) {
		context.refined_waypoints.reserve( waypoints.size() + context.corridor_corners.size() + 16 );
	}
	if ( forcedWaypoints != nullptr && context.refined_forced.capacity() < ( waypoints.size() + context.corridor_corners.size() + 16 ) ) {
		context.refined_forced.reserve( waypoints.size() + context.corridor_corners.size() + 16 );
	}

	for ( size_t i = 0; i < waypoints.size(); ++i ) {
		context.refined_waypoints.push_back( waypoints[ i ] );
		if ( forcedWaypoints != nullptr && i < forcedWaypoints->size() ) {
			context.refined_forced.push_back( ( *forcedWaypoints )[ i ] );
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

				context.segment_corners.clear();
				if ( context.segment_corners.capacity() < 32 ) {
					context.segment_corners.reserve( 32 );
				}

				// Retain stable indices instead of pointers that could outlive context buffer growth or copying.
				for ( uint32_t cornerIndex = 0; cornerIndex < context.corridor_corners.size(); ++cornerIndex ) {
					const auto &corner = context.corridor_corners[ cornerIndex ];
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
									context.segment_corners.push_back( { t, cornerIndex } );
								}
							}
						}
					}
				}

				if ( !context.segment_corners.empty() ) {
					// Sort all violated obstacle corners along the segment from start (t=0) to end (t=1).
					// For curved or circular sets of brushes, this automatically creates an ordered arc of waypoints
					// wrapping smoothly around the curve with guaranteed agent clearance at every step.
					std::sort( context.segment_corners.begin(), context.segment_corners.end(), []( const nav_segment_corner_t &a, const nav_segment_corner_t &b ) {
						return a.t < b.t;
					} );

					for ( size_t scIdx = 0; scIdx < context.segment_corners.size(); ++scIdx ) {
						const auto &sc = context.segment_corners[ scIdx ];
						// Resolve the current owned source only while this refinement context is stable.
						Q_assert( sc.corner < context.corridor_corners.size() );
						const auto &sourceCorner = context.corridor_corners[ sc.corner ];
						if ( context.refined_waypoints.empty() || QM_Vector3DistanceSqrDP( context.refined_waypoints.back(), sourceCorner.standoffMid ) > NAV_CORNER_DUPLICATE_TOLERANCE_SQR ) {
							Vector3DP bestStandoff = sourceCorner.standoffMid;
							bool insertSafe = false;

							// Target forward for line-of-sight test:
							// If there is another corner along this segment, verify line of sight to that next corner's standoff.
							// Otherwise, verify line of sight to the segment destination p1.
							const Vector3DP &targetForward = ( scIdx + 1 < context.segment_corners.size() )
								? context.corridor_corners[ context.segment_corners[ scIdx + 1 ].corner ].standoffMid
								: p1;

							for ( const double scale : { NAV_STANDOFF_SCALE_FULL, NAV_STANDOFF_SCALE_SEVENTY_FIVE, NAV_STANDOFF_SCALE_HALF } ) {
								const double testDist = sourceCorner.standoffDist * scale;
								const double minDist = Nav_RequiredBisectorDistance( effectiveAgentRadius, 0.0, QM_Vector3DotProductDP( sourceCorner.bisectorNorm, sourceCorner.n1 ), QM_Vector3DotProductDP( sourceCorner.bisectorNorm, sourceCorner.n2 ) );
								if ( testDist < minDist ) {
									break;
								}

								Vector3DP candMid = sourceCorner.vertex + sourceCorner.bisectorNorm * testDist;
								candMid.z = p0.z + ( p1.z - p0.z ) * sc.t;
								int32_t candFaceIdx = Nav_FindFaceInLeafStrict( mesh, context, candMid );
								if ( candFaceIdx < 0 ) {
									Vector3DP feetTest = candMid;
									feetTest.z -= NAV_STANDOFF_FEET_SNAP_OFFSET_Z;
									candFaceIdx = Nav_FindFaceInLeafStrict( mesh, context, feetTest );
								}
								if ( candFaceIdx < 0 ) {
									candFaceIdx = Nav_FindClosestFaceInLeaf( mesh, context, candMid );
									if ( candFaceIdx >= 0 && !Nav_PointInsideFace2D( mesh, context, candMid, mesh.faces[ candFaceIdx ] ) ) {
										candFaceIdx = -1;
									}
								}

								if ( candFaceIdx >= 0 && candFaceIdx < static_cast<int32_t>( mesh.faces.size() ) ) {
									const nav_face_t &face = mesh.faces[ candFaceIdx ];
									if ( std::fabs( face.normal.z ) > 0.001 ) {
										const Vector3DP v0 = mesh.vertices[ mesh.halfedges[ face.first_edge_idx ].vertex_idx ];
										const double planeD = QM_Vector3DotProductDP( v0, face.normal );
										candMid.z = ( planeD - face.normal.x * candMid.x - face.normal.y * candMid.y ) / face.normal.z;
									}
								} else {
									continue;
								}

								// Opposing walls constrain the complete radius, not merely the bisector's radial distance.
								if ( Nav_TestSegmentClearance( mesh, context, candMid, candMid, effectiveAgentRadius ).reason != nav_clearance_reason_t::clear ) {
									continue;
								}

								// Prove the complete incoming hull segment, including the previous endpoint; no endpoint nudging.
								if ( !clearLink( context.refined_waypoints.back(), candMid ) ) {
									continue;
								}

								// Prove the complete outgoing hull segment before inserting an obligatory corner.
								if ( !clearLink( candMid, targetForward ) ) {
									continue;
								}

								bestStandoff = candMid;
								insertSafe = true;
								break;
							}

							if ( insertSafe ) {
								// Check if obstacle corner represents an obstacle wall and a sharp turn (>= 45 deg) suitable for an arc:
								const double wallTurnDot = QM_Vector3DotProductDP( sourceCorner.u1, sourceCorner.u2 );
								if ( ( sourceCorner.cornerType & NAV_EDGE_WALL ) != 0 && wallTurnDot <= NAV_CORNER_ARC_MIN_TURN_DOT ) {
									// Determine approach vs departure wall based on incoming path vector:
									const Vector3DP toP0 = context.refined_waypoints.back() - sourceCorner.vertex;
									const double projWall1 = toP0.x * sourceCorner.u1.x + toP0.y * sourceCorner.u1.y;
									const double projWall2 = toP0.x * sourceCorner.u2.x + toP0.y * sourceCorner.u2.y;

									const Vector3DP &uEntry = ( projWall1 >= projWall2 ) ? sourceCorner.u1 : sourceCorner.u2;
									const Vector3DP &nEntry = ( projWall1 >= projWall2 ) ? sourceCorner.n1 : sourceCorner.n2;
									const Vector3DP &uExit = ( projWall1 >= projWall2 ) ? sourceCorner.u2 : sourceCorner.u1;
									const Vector3DP &nExit = ( projWall1 >= projWall2 ) ? sourceCorner.n2 : sourceCorner.n1;

									const double effectiveAgentRadius = ( agentRadius > 0.0 ) ? agentRadius : NAV_DEFAULT_AGENT_RADIUS;
									const double arcClearance = effectiveAgentRadius + NAV_CORNER_CLEARANCE_MARGIN;
									const double arcMinSepSqr = std::min( NAV_CORNER_ARC_MIN_SEPARATION_SQR, effectiveAgentRadius * effectiveAgentRadius );

									// Check for compound corner (closely spaced adjacent corners within NAV_COMPOUND_CORNER_MAX_SPACING):
									bool hasNextClose = false;
									if ( scIdx + 1 < context.segment_corners.size() ) {
										const double cdx = sourceCorner.vertex.x - context.corridor_corners[ context.segment_corners[ scIdx + 1 ].corner ].vertex.x;
										const double cdy = sourceCorner.vertex.y - context.corridor_corners[ context.segment_corners[ scIdx + 1 ].corner ].vertex.y;
										hasNextClose = ( ( cdx * cdx + cdy * cdy ) < NAV_COMPOUND_CORNER_MAX_SPACING_SQR );
									}
									bool hasPrevClose = false;
									if ( scIdx > 0 ) {
										const double cdx = sourceCorner.vertex.x - context.corridor_corners[ context.segment_corners[ scIdx - 1 ].corner ].vertex.x;
										const double cdy = sourceCorner.vertex.y - context.corridor_corners[ context.segment_corners[ scIdx - 1 ].corner ].vertex.y;
										hasPrevClose = ( ( cdx * cdx + cdy * cdy ) < NAV_COMPOUND_CORNER_MAX_SPACING_SQR );
									}

									// 1. Candidate Entry Tangent Waypoint (skip if coming directly from an adjacent compound corner):
									if ( !hasPrevClose ) {
										Vector3DP candEntry = sourceCorner.vertex + uEntry * NAV_CORNER_ARC_TANGENT_OFFSET + nEntry * arcClearance;
										candEntry.z = bestStandoff.z;
										int32_t entryFaceIdx = Nav_FindFaceInLeafStrict( mesh, context, candEntry );
										if ( entryFaceIdx < 0 ) {
											Vector3DP feetTest = candEntry;
											feetTest.z -= NAV_STANDOFF_FEET_SNAP_OFFSET_Z;
											entryFaceIdx = Nav_FindFaceInLeafStrict( mesh, context, feetTest );
										}
										if ( entryFaceIdx < 0 ) {
											entryFaceIdx = Nav_FindClosestFaceInLeaf( mesh, context, candEntry );
											if ( entryFaceIdx >= 0 && !Nav_PointInsideFace2D( mesh, context, candEntry, mesh.faces[ entryFaceIdx ] ) ) {
												entryFaceIdx = -1;
											}
										}

										if ( entryFaceIdx >= 0 && entryFaceIdx < static_cast<int32_t>( mesh.faces.size() ) ) {
											const nav_face_t &face = mesh.faces[ entryFaceIdx ];
											if ( std::fabs( face.normal.z ) > 0.001 ) {
												const Vector3DP v0 = mesh.vertices[ mesh.halfedges[ face.first_edge_idx ].vertex_idx ];
												const double planeD = QM_Vector3DotProductDP( v0, face.normal );
												candEntry.z = ( planeD - face.normal.x * candEntry.x - face.normal.y * candEntry.y ) / face.normal.z;
											}

											// Elevation tier guard: candEntry must be on the same vertical level as the corner standoff (within max step height)
											if ( std::fabs( candEntry.z - bestStandoff.z ) <= static_cast<double>( NAV_MAX_STEP_HEIGHT ) ) {
												// Verify minimum distance from previous waypoint and to apex standoff:
												if ( QM_Vector3DistanceSqrDP( context.refined_waypoints.back(), candEntry ) > arcMinSepSqr &&
													 QM_Vector3DistanceSqrDP( candEntry, bestStandoff ) > arcMinSepSqr ) {
													// Verify forward progress along segment:
													Vector3DP segProgress = candEntry - context.refined_waypoints.back();
													segProgress.z = 0.0;
													Vector3DP overallDir = bestStandoff - context.refined_waypoints.back();
													overallDir.z = 0.0;
													if ( QM_Vector3DotProductDP( segProgress, overallDir ) > 0.0 ) {
														if ( clearLink( context.refined_waypoints.back(), candEntry ) &&
															 clearLink( candEntry, bestStandoff ) ) {
															context.refined_waypoints.push_back( candEntry );
														Nav_RecordWaypointConstraint( context, candEntry, NAV_WAYPOINT_CORNER_ENTRY, -1, sourceCorner.sourceCorner, entryFaceIdx );
															if ( forcedWaypoints != nullptr ) {
																context.refined_forced.push_back( false );
															}
														}
													}
												}
											}
										}
									}

									// Insert apex standoff:
									context.refined_waypoints.push_back( bestStandoff );
										Nav_RecordWaypointConstraint( context, bestStandoff, NAV_WAYPOINT_CORNER_APEX | NAV_WAYPOINT_MANDATORY, -1, sourceCorner.sourceCorner, -1 );
									if ( forcedWaypoints != nullptr ) {
										context.refined_forced.push_back( true );
									}

									// 2. Candidate Exit Tangent Waypoint (skip if transitioning directly into an adjacent compound corner):
									if ( !hasNextClose ) {
										Vector3DP candExit = sourceCorner.vertex + uExit * NAV_CORNER_ARC_TANGENT_OFFSET + nExit * arcClearance;
										candExit.z = bestStandoff.z;
										int32_t exitFaceIdx = Nav_FindFaceInLeafStrict( mesh, context, candExit );
										if ( exitFaceIdx < 0 ) {
											Vector3DP feetTest = candExit;
											feetTest.z -= NAV_STANDOFF_FEET_SNAP_OFFSET_Z;
											exitFaceIdx = Nav_FindFaceInLeafStrict( mesh, context, feetTest );
										}
										if ( exitFaceIdx < 0 ) {
											exitFaceIdx = Nav_FindClosestFaceInLeaf( mesh, context, candExit );
											if ( exitFaceIdx >= 0 && !Nav_PointInsideFace2D( mesh, context, candExit, mesh.faces[ exitFaceIdx ] ) ) {
												exitFaceIdx = -1;
											}
										}

										if ( exitFaceIdx >= 0 && exitFaceIdx < static_cast<int32_t>( mesh.faces.size() ) ) {
											const nav_face_t &face = mesh.faces[ exitFaceIdx ];
											if ( std::fabs( face.normal.z ) > 0.001 ) {
												const Vector3DP v0 = mesh.vertices[ mesh.halfedges[ face.first_edge_idx ].vertex_idx ];
												const double planeD = QM_Vector3DotProductDP( v0, face.normal );
												candExit.z = ( planeD - face.normal.x * candExit.x - face.normal.y * candExit.y ) / face.normal.z;
											}

											// Directional guard: uExit must align with departure progress towards targetForward
											Vector3DP toForward = targetForward - sourceCorner.vertex;
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
														if ( clearLink( bestStandoff, candExit ) &&
															 clearLink( candExit, targetForward ) ) {
															context.refined_waypoints.push_back( candExit );
																Nav_RecordWaypointConstraint( context, candExit, NAV_WAYPOINT_CORNER_EXIT, -1, sourceCorner.sourceCorner, exitFaceIdx );
															if ( forcedWaypoints != nullptr ) {
																context.refined_forced.push_back( false );
															}
														}
													}
												}
											}
										}
									}
								} else {
									// Gentler turn: single apex standoff insertion
									context.refined_waypoints.push_back( bestStandoff );
										Nav_RecordWaypointConstraint( context, bestStandoff, NAV_WAYPOINT_CORNER_APEX | NAV_WAYPOINT_MANDATORY, -1, sourceCorner.sourceCorner, -1 );
									if ( forcedWaypoints != nullptr ) {
										context.refined_forced.push_back( true );
									}
								}
							}
						}
					}
				}
			}
		}
	}

	waypoints.assign( context.refined_waypoints.begin(), context.refined_waypoints.end() );
	if ( forcedWaypoints != nullptr ) {
		forcedWaypoints->assign( context.refined_forced.begin(), context.refined_forced.end() );
	}
}

//! Owner-thread flat-face interval scratch; reused only during synchronous route installation.

/**
*	@brief	Prove that an entire center segment lies on one continuous flat support plane.
*	@param	start	Exact retained feet-space start.
*	@param	end	Exact retained feet-space end.
*	@return	True only when convex flat-face intervals cover the closed segment without a gap.
*	@note	A ray with no wall hit does not prove flat support: an intervening ramp still needs local native samples.
**/
static bool Nav_IsContinuousFlatSpan( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const Vector3DP &start, const Vector3DP &end ) {
	/**
	*	Clip the segment against each compatible convex floor face, retaining only intervals on the same plane.
	**/
	context.flat_intervals.clear();
	const Vector3DP direction = end - start;
	// Inspect faces only at installation; bounded continuation remains the fallback for excessive topology.
	for ( const auto &face : mesh.faces ) {
		if ( face.normal.z < 0.999 || face.first_edge_idx < 0 || static_cast<size_t>( face.first_edge_idx ) >= mesh.halfedges.size() || face.num_edges < 3 ) {
			continue;
		}
		const int32_t firstVertex = mesh.halfedges[ face.first_edge_idx ].vertex_idx;
		if ( firstVertex < 0 || static_cast<size_t>( firstVertex ) >= mesh.vertices.size() ) {
			return false;
		}
		const Vector3DP vertex = mesh.vertices[ firstVertex ];
		const double surfaceStart = ( QM_Vector3DotProductDP( vertex, face.normal ) - face.normal.x * start.x - face.normal.y * start.y ) / face.normal.z;
		const double surfaceEnd = ( QM_Vector3DotProductDP( vertex, face.normal ) - face.normal.x * end.x - face.normal.y * end.y ) / face.normal.z;
		// Native flat feet may include collision slop; the actual plane may not hide a rise, slope or hole.
		if ( std::fabs( surfaceStart - start.z ) > NAV_MOVEMENT_ENDPOINT_TOLERANCE ||
			 std::fabs( surfaceEnd - end.z ) > NAV_MOVEMENT_ENDPOINT_TOLERANCE ) {
			continue;
		}
		double first = 0.0, last = 1.0;
		int32_t edgeIndex = face.first_edge_idx;
		bool intersects = true;
		// Every inward half-plane must contain the clipped interval; malformed/degenerate faces cannot certify support.
		for ( int32_t offset = 0; offset < face.num_edges; ++offset ) {
			// Topology corruption is a failed proof, never an out-of-bounds read or partial supported interval.
			if ( edgeIndex < 0 || static_cast<size_t>( edgeIndex ) >= mesh.halfedges.size() ) {
				return false;
			}
			const auto &edge = mesh.halfedges[ edgeIndex ];
			if ( edge.vertex_idx < 0 || static_cast<size_t>( edge.vertex_idx ) >= mesh.vertices.size() || edge.next_idx < 0 ||
				 static_cast<size_t>( edge.next_idx ) >= mesh.halfedges.size() || mesh.halfedges[ edge.next_idx ].vertex_idx < 0 ||
				 static_cast<size_t>( mesh.halfedges[ edge.next_idx ].vertex_idx ) >= mesh.vertices.size() ) {
				return false;
			}
			const Vector3DP &a = mesh.vertices[ edge.vertex_idx ];
			const Vector3DP &b = mesh.vertices[ mesh.halfedges[ edge.next_idx ].vertex_idx ];
			const double ex = b.x - a.x, ey = b.y - a.y;
			const double centerSide = ex * ( face.center.y - a.y ) - ey * ( face.center.x - a.x );
			const double sign = centerSide > 0.0 ? 1.0 : -1.0;
			const double originSide = sign * ( ex * ( start.y - a.y ) - ey * ( start.x - a.x ) );
			const double change = sign * ( ex * direction.y - ey * direction.x );
			// A zero-area face or exterior parallel segment has no provable supported interval.
			if ( std::fabs( centerSide ) <= 1e-12 || ( change == 0.0 && originSide < 0.0 ) ) {
				intersects = false;
				break;
			}
			// Clip entry/exit to the same affine half-plane; do not shift the segment or hull radius.
			if ( change > 0.0 ) {
				first = std::max( first, -originSide / change );
			} else if ( change < 0.0 ) {
				last = std::min( last, -originSide / change );
			}
			if ( first > last ) {
				intersects = false;
				break;
			}
			edgeIndex = edge.next_idx;
		}
		if ( intersects ) {
			// Excessive interval work falls back to local native continuation rather than accepting an incomplete proof.
			if ( context.flat_intervals.size() == 8192 ) {
				return false;
			}
			context.flat_intervals.emplace_back( first, last );
		}
	}
	/**
	*	Merge sorted convex intervals and reject any interior gap in same-plane support.
	**/
	std::sort( context.flat_intervals.begin(), context.flat_intervals.end() );
	double covered = 0.0;
	// Only roundoff-scale shared-vertex differences may coalesce; this is independent of physical hull clearance.
	for ( const auto &interval : context.flat_intervals ) {
		if ( interval.first > covered + 1e-9 ) {
			return false;
		}
		covered = std::max( covered, interval.second );
		if ( covered >= 1.0 ) {
			return true;
		}
	}
	return false;
}

/**
*	@brief	Resolve analytical slope support and bounded native contact epsilon for an exact goal.
*	@param	feet	Unmodified requested feet-space endpoint.
*	@param	radius	Native planar hull radius.
*	@param	shape	Explicit capsule/cylinder profile.
*	@return	Probe-only contact adjustment and expected native slope support offset.
*	@note	Native feet already above the surface are not offset twice; no collision decision is made here.
**/
nav_goal_support_t Nav_ResolveHullGoalSupport( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const Vector3DP &feet, const double radius, const int32_t shape ) {
	/**
	*	Reject malformed profile data before accessing the supporting face or calculating its plane.
	**/
	nav_goal_support_t result;
	result.probe = feet;
	// Invalid profiles receive no adjustment and still require the caller's fail-closed native check.
	if ( !std::isfinite( feet.x ) || !std::isfinite( feet.y ) || !std::isfinite( feet.z ) ||
		 !std::isfinite( radius ) || radius <= 0.0 || ( shape != MM_SHAPE_CAPSULE && shape != MM_SHAPE_CYLINDER ) ) {
		return result;
	}
	const int32_t faceIndex = Nav_FindClosestFaceInLeaf( mesh, context, feet );
	// Missing surface topology cannot authorize a destination height correction.
	if ( faceIndex < 0 || static_cast<size_t>( faceIndex ) >= mesh.faces.size() ) {
		return result;
	}
	const auto &face = mesh.faces[ faceIndex ];
	// Walkable support needs a positive vertical normal and a valid defining vertex.
	if ( face.normal.z <= 0.0 || face.first_edge_idx < 0 || static_cast<size_t>( face.first_edge_idx ) >= mesh.halfedges.size() ) {
		return result;
	}
	const int32_t vertexIndex = mesh.halfedges[ face.first_edge_idx ].vertex_idx;
	if ( vertexIndex < 0 || static_cast<size_t>( vertexIndex ) >= mesh.vertices.size() ) {
		return result;
	}
	/**
	*	Distinguish literal nav-surface coordinates from already supported native feet.
	**/
	const Vector3DP vertex = mesh.vertices[ vertexIndex ];
	const double surfaceZ = ( QM_Vector3DotProductDP( vertex, face.normal ) - face.normal.x * feet.x - face.normal.y * feet.y ) / face.normal.z;
	// Only a surface-coincident goal receives the analytical profile offset and bounded contact probe.
	if ( std::fabs( feet.z - surfaceZ ) <= NAV_MOVEMENT_ENDPOINT_TOLERANCE ) {
		result.surface = true;
		result.offset = shape == MM_SHAPE_CAPSULE ? radius * ( 1.0 / face.normal.z - 1.0 ) :
			radius * std::sqrt( std::max( 0.0, 1.0 - face.normal.z * face.normal.z ) ) / face.normal.z;
		// Apply native contact tolerance to the probe alone; route coordinates remain exact.
		result.probe.z += result.offset + NAV_MOVEMENT_ENDPOINT_TOLERANCE;
	}
	return result;
}

/**
*	@brief	Prove complete retained route geometry and native support before installation.
*	@param	points	Exact feet-space route after all generation/cleanup mutations.
*	@param	mins	Native bounds minimums.
*	@param	maxs	Native bounds maximums.
*	@param	shape	Explicit capsule/cylinder profile.
*	@param	policy	Unchanged traversal limits.
*	@return	Owned endpoint/link failure or complete static hull/support validation.
*	@param	requireExactGoal	Keep true for installation; intermediate candidate links prove supported XY continuation only.
*	@param	sampleBudget	Caller budget, capped at 8192; bounded repair supplies its remaining shared sample allowance.
*	@note	Native samples are bounded by 8192; this owner-thread correctness reference makes no speedup claim.
**/
nav_route_validation_t Nav_ValidateHullRouteNativeMainThread( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const std::span<const Vector3DP> points,
	const Vector3 &mins, const Vector3 &maxs, const int32_t shape, const nav_path_policy_t &policy, svg_base_edict_t *passEntity, const bool requireExactGoal, const uint32_t sampleBudget ) {
	/**
	*	Reject mismatched hulls, missing routes and malformed positions before preparing geometry.
	**/
	nav_route_validation_t result;
	if ( points.size() < 2 || !Nav_IsSupportedNativeHull( mins, maxs ) ||
		 !std::isfinite( policy.agent_radius ) || std::fabs( policy.agent_radius - maxs.x ) > 0.000001 ||
		 ( shape != MM_SHAPE_CAPSULE && shape != MM_SHAPE_CYLINDER ) ) {
		return result;
	}
	// Invalid values cannot authorize geometry or collision work.
	for ( const auto &point : points ) {
		if ( !std::isfinite( point.x ) || !std::isfinite( point.y ) || !std::isfinite( point.z ) ) {
			return result;
		}
	}
	// Immutable geometry is owner-prepared; native trace imports stay in this named phase.
	const double radius = maxs.x;
	const cm_contents_t staticMask = CM_CONTENTMASK_SOLID | CONTENTS_MONSTERCLIP;
	const auto native = [&]( const Vector3DP &start, const Vector3DP &end, const bool requireHeight = false ) {
		const auto hit = SVG_Nav_TestMovementHull( start, end, mins, maxs,
			static_cast<mm_trace_shape_t>( shape ), policy, passEntity, staticMask, requireHeight );
		result.traces += hit.trace_count;
		result.physical = hit.reason;
		return hit;
	};
	/**
	*	The actual start must already be physically supported; recovery is an explicit separate state.
	**/
	const auto start = native( points.front(), points.front(), true );
	if ( start.reason != svg_nav_movement_reason_t::clear ) {
		result.reason = nav_route_reason_t::invalid_start;
		return result;
	}
	/**
	*	Classify the exact goal before checking links; an invalid destination cannot become an incidental segment failure.
	**/
	const auto goalSupport = Nav_ResolveHullGoalSupport( mesh, context, points.back(), radius, shape );
	const double supportOffset = goalSupport.offset;
	const Vector3DP goalProbe = goalSupport.probe;
	// Intermediate nav points constrain XY continuation; native landing may be above their plane at a ramp/platform seam.
	// Full installed routes always require the exact supported final destination.
	if ( requireExactGoal ) {
		const auto goal = native( goalProbe, goalProbe, true );
		if ( goal.reason != svg_nav_movement_reason_t::clear ) {
			result.reason = nav_route_reason_t::invalid_goal;
			return result;
		}
	}
	/**
	*	Capture the current cleared lifetime's finite origin reservation on the owner thread.
	*	Directed repair links use this same proof, so an inaccessible corner cannot authorize a replacement.
	**/
	const svg_crowd_group_t *originGroup = passEntity != nullptr && SVG_Crowd_HasClearedEgress( passEntity )
		? SVG_Crowd_GetGroup( passEntity->crowd.crowdID ) : nullptr;
	const auto entersReservedOrigin = [&]( const Vector3DP &from, const Vector3DP &to ) {
		// Ordinary routes and uncleared departure owners retain their existing native proof.
		if ( originGroup == nullptr ) {
			return false;
		}
		const Vector3DP normal = originGroup->egressPortalOutward;
		const Vector3DP tangent{ -normal.y, normal.x, 0.0 };
		const Vector3DP relative = from - originGroup->egressPortalOrigin;
		const Vector3DP delta = to - from;
		const double depth = QM_Vector3DotProductDP( relative, normal );
		const double depthDelta = QM_Vector3DotProductDP( delta, normal );
		const double lateral = QM_Vector3DotProductDP( relative, tangent );
		const double lateralDelta = QM_Vector3DotProductDP( delta, tangent );
		const double stop = SVG_Crowd_GetEgressClearanceDepth( passEntity );
		const double footprint = std::max( { std::fabs( mins.x ), std::fabs( maxs.x ), std::fabs( mins.y ), std::fabs( maxs.y ) } );
		const double halfSpan = originGroup->egressPortalHalfWidth + footprint;
		double first = 0.0;
		double last = 1.0;
		// Clip the complete link to the closed lateral aperture, including entry through either side.
		if ( lateralDelta == 0.0 ) {
			if ( std::fabs( lateral ) > halfSpan ) {
				return false;
			}
		} else {
			const double a = ( -halfSpan - lateral ) / lateralDelta;
			const double b = ( halfSpan - lateral ) / lateralDelta;
			first = std::max( first, std::min( a, b ) );
			last = std::min( last, std::max( a, b ) );
		}
		// An empty lateral interval cannot enter this finite reservation; touching its stop plane is legal.
		return first <= last && std::min( depth + depthDelta * first, depth + depthDelta * last ) < stop;
	};
	Vector3DP supported = start.bbox_endpoint;
	supported.z += mins.z;
	/**
	*	Validate every complete flat link and simulate supported horizontal continuation through every elevation.
	**/
	for ( size_t index = 1; index < points.size(); ++index ) {
		result.segment = static_cast<int32_t>( index - 1 );
		const Vector3DP &previous = points[ index - 1 ];
		const Vector3DP &target = points[ index ];
		// Reject reserved geometry before traces; bounded repair must choose another fully supported link.
		if ( entersReservedOrigin( previous, target ) ) {
			result.reason = nav_route_reason_t::unsafe_segment;
			return result;
		}
		const int32_t face = Nav_FindClosestFaceInLeaf( mesh, context, previous );
		const bool flat = std::fabs( previous.z - target.z ) <= NAV_MOVEMENT_ENDPOINT_TOLERANCE &&
			face >= 0 && static_cast<size_t>( face ) < mesh.faces.size() && mesh.faces[ face ].normal.z >= 0.999;
		// Closed-link clearance proves actual hull radius, including arc chords and both endpoints.
		if ( flat ) {
			result.geometric = Nav_TestSegmentClearance( mesh, context, previous, target, radius );
			result.minimum_clearance = std::min( result.minimum_clearance, result.geometric.minimum_clearance );
			if ( result.geometric.reason != nav_clearance_reason_t::clear ) {
				result.reason = nav_route_reason_t::unsafe_segment;
				return result;
			}
		}
		Vector3DP remaining = target - supported;
		remaining.z = 0.0;
		const double length = QM_Vector3LengthDP( remaining );
		result.length += length;
		// Check the floating count before conversion so huge finite coordinates cannot overflow an integer.
		const double requestedSamples = std::ceil( length / 5.5 );
		if ( !std::isfinite( requestedSamples ) || requestedSamples > 8192.0 ) {
			result.reason = nav_route_reason_t::work_limit;
			return result;
		}
		/**
		*	A complete native sweep proves a flat continuous span without hundreds of identical floor probes.
		*	Topology crossings, ledges, stairs and ramps retain local continuation samples.
		**/
		// A footprint can remain on an earlier tread after the center crosses its portal.
		// Keep local descent samples until native support actually reaches this flat surface.
		const bool continuousFlat = flat && std::fabs( supported.z - previous.z ) <= NAV_MOVEMENT_ENDPOINT_TOLERANCE &&
			Nav_IsContinuousFlatSpan( mesh, context, previous, target );
		const uint32_t count = continuousFlat ? 1u : std::max( 1u, static_cast<uint32_t>( requestedSamples ) );
		// Bound reference work before a malformed or enormous route can stall installation indefinitely.
		if ( count > std::min( sampleBudget, 8192u ) || result.samples + count > std::min( sampleBudget, 8192u ) ) {
			result.reason = nav_route_reason_t::work_limit;
			return result;
		}
		// Retain only a local containment hint; crossings fall back to the unchanged nearest-face lookup.
		int32_t supportFace = face;
		// Each sample is a horizontal native command; landing selects actual support instead of jumping to nav Z.
		for ( uint32_t sample = 1; sample <= count; ++sample ) {
			Vector3DP end = previous + ( target - previous ) * ( static_cast<double>( sample ) / count );
			end.z = supported.z;
			const auto movement = native( supported, end );
			++result.samples;
			if ( movement.reason != svg_nav_movement_reason_t::clear ) {
				result.reason = nav_route_reason_t::native_failure;
				result.supported_endpoint = supported;
				return result;
			}
			supported = movement.bbox_endpoint;
			supported.z += mins.z;
			/**
			*	Prove containment locally before repeating a KD/global search for every small native step.
			*	An eligible containing face within the lookup's 64-unit plane threshold guarantees that its
			*	containment query would succeed; this Boolean check does not need the nearest face's identity.
			**/
			bool containedLocally = false;
			// Invalid hints and excluded faces cannot certify containment; no cached height is reused.
			if ( supportFace >= 0 && static_cast<size_t>( supportFace ) < mesh.faces.size() ) {
				const auto &support = mesh.faces[ supportFace ];
				const Vector3DP vertex = mesh.vertices[ mesh.halfedges[ support.first_edge_idx ].vertex_idx ];
				containedLocally = ( support.surface_flags & ( CM_SURFACE_NO_NAVMESH | CONTENTS_NO_NAVMESH ) ) == 0 &&
					std::fabs( QM_Vector3DotProductDP( supported, support.normal ) - QM_Vector3DotProductDP( vertex, support.normal ) ) < 64.0 &&
					Nav_PointInsideFace2D( mesh, context, supported, support );
			}
			// Crossings, unrelated elevations and unsupported hints retain the original exact lookup and rejection.
			if ( !containedLocally ) {
				supportFace = Nav_FindClosestFaceInLeaf( mesh, context, supported );
			}
			// A native supported point outside the intended navigation surface cannot prove route containment.
			if ( !containedLocally && ( supportFace < 0 || !Nav_PointInsideFace2D( mesh, context, supported, mesh.faces[ supportFace ] ) ) ) {
				result.reason = nav_route_reason_t::unsafe_segment;
				return result;
			}
		}
	}
	/**
	*	Require the requested goal floor; native analytical slope support may offset literal nav-surface feet.
	**/
	result.supported_endpoint = supported;
	if ( requireExactGoal && std::fabs( supported.z - points.back().z - supportOffset ) > NAV_MOVEMENT_ENDPOINT_TOLERANCE ) {
		result.reason = nav_route_reason_t::invalid_goal;
		return result;
	}
	result.reason = nav_route_reason_t::clear;
	result.segment = -1;
	return result;
}

/**
*	@brief	Project an optional flat portal into full-radius free space using exact nearest finite-edge witnesses.
*	@param	previous	Retained incoming waypoint.
*	@param	current	Optional portal candidate, updated only after all clearance/containment checks pass.
*	@param	next	Retained outgoing waypoint.
*	@param	radius	Unchanged native XY radius.
*	@return	True if a complete safe replacement exists within the bounded local projection search.
*	@note	At most three margin trials of eight corrections; this is an owner-thread O(E) reference repair.
**/
static bool Nav_ProjectOptionalFlatPortal( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const Vector3DP &previous, Vector3DP &current, const Vector3DP &next, const double radius ) {
	/**
	*	Keep the physical radius fixed; only optional additional wall margin varies in a narrow channel.
	**/
	// Prefer a small margin, then reduce margin if opposing boundaries leave less spare room.
	for ( const double margin : { 4.0, 2.0, 0.25 } ) {
		Vector3DP candidate = current;
		// Bound local projection work so contradictory wall constraints cannot cause an unbounded query.
		for ( uint32_t iteration = 0; iteration < 8; ++iteration ) {
			const auto clearance = Nav_TestSegmentClearance( mesh, context, candidate, candidate, radius + margin );
			/**
			*	A candidate needs surface containment and both complete closed links before replacing the portal.
			**/
			if ( clearance.reason == nav_clearance_reason_t::clear ) {
				const int32_t face = Nav_FindClosestFaceInLeaf( mesh, context, candidate );
				// A clearance-only point outside the mesh cannot become a route waypoint.
				if ( face >= 0 && static_cast<size_t>( face ) < mesh.faces.size() &&
					 mesh.faces[ face ].normal.z >= 0.999 && Nav_PointInsideFace2D( mesh, context, candidate, mesh.faces[ face ] ) &&
					 std::fabs( mesh.faces[ face ].center.z - current.z ) <= 0.125 &&
					 Nav_TestSegmentClearance( mesh, context, previous, candidate, radius ).reason == nav_clearance_reason_t::clear &&
					 Nav_TestSegmentClearance( mesh, context, candidate, next, radius ).reason == nav_clearance_reason_t::clear ) {
					current = candidate;
					return true;
				}
				break;
			}
			// Invalid/stale geometry is terminal; only a measured finite wall contact permits projection.
			if ( clearance.reason != nav_clearance_reason_t::boundary_contact || clearance.witness_edge < 0 ) {
				return false;
			}

			/**
			*	Move away from the exact closest point on the finite boundary, including its endpoints.
			**/
			Vector3DP edge = clearance.witness_end - clearance.witness_start;
			edge.z = 0.0;
			const double lengthSqr = QM_Vector3LengthSqrDP( edge );
			Vector3DP offset = candidate - clearance.witness_start;
			offset.z = 0.0;
			const double fraction = lengthSqr > 0.0 ? std::clamp( QM_Vector3DotProductDP( offset, edge ) / lengthSqr, 0.0, 1.0 ) : 0.0;
			const Vector3DP nearest = clearance.witness_start + edge * fraction;
			Vector3DP normal = candidate - nearest;
			normal.z = 0.0;
			const double distance = QM_Vector3LengthDP( normal );
			// A zero-distance point has no proven free side; do not guess across a wall.
			if ( distance <= 0.000001 ) {
				break;
			}
			candidate = candidate + normal * ( ( radius + margin + 0.0001 - distance ) / distance );
		}
	}
	return false;
}

/**
*	@brief	Repair an unsafe optional flat portal by a validated projection or a fully clear neighbor replacement.
*	@param	waypoints	Refined feet-space route, including immutable start and goal.
*	@param	forced	Parallel semantic/legacy forced flags; mandatory points are retained.
*	@param	radius	Unchanged physical XY radius.
*	@note	Repairs optional portal excursions without skipping elevation, door or corner obligations.
**/
static void Nav_PruneUnsafeFlatPortals( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, std::vector<Vector3DP> &waypoints, std::vector<bool> *forced, const double radius ) {
	/**
	*	Inspect interior optional points; start, goal and every forced waypoint remain authoritative.
	**/
	size_t index = 1;
	// Erasing an excursion exposes another neighbor pair, so recheck the same index before advancing.
	while ( index + 1 < waypoints.size() ) {
		const Vector3DP &previous = waypoints[ index - 1 ];
		const Vector3DP &current = waypoints[ index ];
		const Vector3DP &next = waypoints[ index + 1 ];
		const bool mandatory = forced == nullptr || index >= forced->size() || ( *forced )[ index ];
		const bool sameHeight = std::fabs( current.z - previous.z ) <= 0.125 &&
			std::fabs( next.z - current.z ) <= 0.125;
		// Repair an optional narrow-channel center without moving mandatory door/stair/corner points.
		if ( !mandatory && sameHeight && Nav_TestSegmentClearance( mesh, context, current, current, radius ).reason == nav_clearance_reason_t::boundary_contact ) {
			Vector3DP projected = current;
			// Commit only a surface-contained replacement with full incoming and outgoing hull clearance.
			if ( Nav_ProjectOptionalFlatPortal( mesh, context, previous, projected, next, radius ) ) {
				waypoints[ index ] = projected;
				++index;
				continue;
			}
		}
		// An unsafe optional point/link may be pruned only when the exact closed neighbor replacement is safe.
		if ( !mandatory && sameHeight &&
			 ( Nav_TestSegmentClearance( mesh, context, current, current, radius ).reason == nav_clearance_reason_t::boundary_contact ||
			   Nav_TestSegmentClearance( mesh, context, previous, current, radius ).reason == nav_clearance_reason_t::boundary_contact ||
			   Nav_TestSegmentClearance( mesh, context, current, next, radius ).reason == nav_clearance_reason_t::boundary_contact ) &&
			 Nav_TestSegmentClearance( mesh, context, previous, next, radius ).reason == nav_clearance_reason_t::clear ) {
			// Mutate route and flags together so consumers retain matching indices and mandatory events.
			waypoints.erase( waypoints.begin() + index );
			forced->erase( forced->begin() + index );
			continue;
		}
		++index;
	}
}

/**
*	@brief	Repair a retained flat corner chord that cuts the full-radius envelope of a finite wall endpoint.
*	@param	points	Exact retained route; existing endpoints and mandatory points remain unchanged.
*	@param	forced	Parallel obligations receiving each new required corner apex.
*	@param	radius	Unchanged physical radius.
*	@note	At most 64 insertions per query and three margin candidates per witnessed endpoint.
*			This geometry repair is provisional; complete native route proof still precedes installation.
**/
static void Nav_RepairUnsafeFlatLinks( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, std::vector<Vector3DP> &points, std::vector<bool> &forced, const double radius ) {
	/**
	*	Inspect closed flat links after all funnel/arc cleanup, including links between mandatory points.
	**/
	uint32_t inserted = 0;
	size_t index = 1;
	while ( index < points.size() && inserted < 64 ) {
		const Vector3DP a = points[ index - 1 ];
		const Vector3DP b = points[ index ];
		// Elevations use native local step continuation and cannot be repaired as extruded XY walls.
		if ( std::fabs( a.z - b.z ) > NAV_MOVEMENT_ENDPOINT_TOLERANCE ) {
			++index;
			continue;
		}
		const int32_t face = Nav_FindClosestFaceInLeaf( mesh, context, a );
		if ( face < 0 || static_cast<size_t>( face ) >= mesh.faces.size() || mesh.faces[ face ].normal.z < 0.999 ) {
			++index;
			continue;
		}
		const auto link = Nav_TestSegmentClearance( mesh, context, a, b, radius );
		// Only a measured corner cut between already safe endpoints admits this local repair.
		if ( link.reason != nav_clearance_reason_t::boundary_contact ||
			 Nav_TestSegmentClearance( mesh, context, a, a, radius ).reason != nav_clearance_reason_t::clear ||
			 Nav_TestSegmentClearance( mesh, context, b, b, radius ).reason != nav_clearance_reason_t::clear ) {
			++index;
			continue;
		}
		Vector3DP direction = b - a;
		direction.z = 0.0;
		const double lengthSqr = QM_Vector3LengthSqrDP( direction );
		bool repaired = false;
		/**
		*	Try both finite wall endpoints; long-wall interior crossings have no nearby endpoint repair.
		**/
		for ( const Vector3DP &corner : { link.witness_start, link.witness_end } ) {
			const double fraction = lengthSqr > 0.0 ? std::clamp( QM_Vector3DotProductDP( corner - a, direction ) / lengthSqr, 0.0, 1.0 ) : 0.0;
			const Vector3DP projected = a + direction * fraction;
			Vector3DP outward = projected - corner;
			outward.z = 0.0;
			const double distance = QM_Vector3LengthDP( outward );
			// A coincident crossing has no proven free side; an endpoint farther than R did not cause this corner cut.
			if ( distance <= 1e-6 || distance >= radius || fraction <= 0.0 || fraction >= 1.0 ) {
				continue;
			}
			outward = outward * ( 1.0 / distance );
			// Spare margin is variable; every candidate and both retained chords still require the full native radius.
			for ( const double margin : { 12.0, 8.0, 4.0 } ) {
				Vector3DP candidate = corner + outward * ( radius + margin );
				candidate.z = a.z;
				const int32_t candidateFace = Nav_FindClosestFaceInLeaf( mesh, context, candidate );
				if ( candidateFace < 0 || static_cast<size_t>( candidateFace ) >= mesh.faces.size() ||
					 mesh.faces[ candidateFace ].normal.z < 0.999 || !Nav_PointInsideFace2D( mesh, context, candidate, mesh.faces[ candidateFace ] ) ||
					 std::fabs( mesh.faces[ candidateFace ].center.z - a.z ) > NAV_MOVEMENT_ENDPOINT_TOLERANCE ||
					 Nav_TestSegmentClearance( mesh, context, a, candidate, radius ).reason != nav_clearance_reason_t::clear ||
					 Nav_TestSegmentClearance( mesh, context, candidate, b, radius ).reason != nav_clearance_reason_t::clear ) {
					continue;
				}
				/**
				*	Retain the original obligations and add owned corner provenance at the new safe chord junction.
				**/
				int32_t sourceCorner = -1;
				for ( size_t source = 0; source < mesh.corners.size(); ++source ) {
					if ( QM_Vector3Distance2DSqrDP( mesh.corners[ source ].vertex, corner ) <= 1e-8 ) {
						sourceCorner = static_cast<int32_t>( source );
						break;
					}
				}
				points.insert( points.begin() + index, candidate );
				forced.insert( forced.begin() + index, true );
				Nav_RecordWaypointConstraint( context, candidate, NAV_WAYPOINT_CORNER_APEX | NAV_WAYPOINT_MANDATORY, -1, sourceCorner, candidateFace );
				++inserted;
				repaired = true;
				break;
			}
			if ( repaired ) {
				break;
			}
		}
		// Both new chords were proven above; later native validation rejects any collision/support disagreement.
		index += repaired ? 2 : 1;
	}
}

//! Portal representation used during funnel algorithm string pulling.

//! Persistent static buffer for funnel portal sequence in Nav_StringPull.
//! Persistent static buffer for tracking portal indices associated with waypoints.
//! Persistent static buffer for cylindrical obstacle subdivision waypoints.
//! Persistent static buffer for cylindrical obstacle subdivision forced flags.
//! Persistent static buffer for sanitized path waypoints.
//! Persistent static buffer for sanitized path forced flags.
//! Persistent static buffer for collinear decimated path waypoints.
//! Persistent static buffer for collinear decimated path forced flags.
//! Persistent static buffer for double-precision conversion in Vector3 overload.


/**
*	@brief	Find a complete supported local alternative when independent corner refinement produces an invalid link.
*	@param	original	Unrefined corridor points with exact start/goal coordinates.
*	@param	originalForced	Authoritative portal obligations captured before corner proposals.
*	@param	portalAnchors	Owned provenance of original door/stair constraints.
*	@param	output	Candidate replacement, committed only after complete native proof.
*	@param	outputForced	Parallel generation flags for the replacement.
*	@param	mins	Native hull minimum bounds.
*	@param	maxs	Native hull maximum bounds.
*	@param	shape	Native capsule/cylinder shape.
*	@param	policy	Unchanged radius/elevation/support policy.
*	@return	True only when a whole replacement route is safe and retains every original mandatory portal.
*	@note	Owner-thread fallback: at most 64 nodes per obligation span, 1024 edge proofs and 32768 samples per query.
**/
static bool Nav_RepairRefinementRoute( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const std::vector<Vector3DP> &original, const std::vector<bool> &originalForced,
	const svg_vector_t<nav_waypoint_anchor_t, TAG_SVGAME_NAVMESH> &portalAnchors,
	std::vector<Vector3DP> &output, std::vector<bool> &outputForced, const Vector3 &mins, const Vector3 &maxs,
	const int32_t shape, const nav_path_policy_t &policy, svg_base_edict_t *passEntity ) {
	/**
	*	Validate span ownership and bound all search work before allocating query-local candidates.
	**/
	if ( original.size() < 2 || original.size() != originalForced.size() ) {
		return false;
	}
	struct node_t {
		//! Supported native feet used for complete directed-edge validation.
		Vector3DP feet = {};
		//! Corner provenance for newly selected apex constraints, or -1 for a corridor point.
		int32_t corner = -1;
	};
	std::vector<Vector3DP> repaired{ original.front() };
	std::vector<bool> repairedForced{ originalForced.front() };
	svg_vector_t<nav_waypoint_anchor_t, TAG_SVGAME_NAVMESH> repairedAnchors = portalAnchors;
	context.diagnostics.repair_status = 1;
	uint32_t edgeProofs = 0;
	uint32_t samples = 0;
	const cm_contents_t staticMask = CM_CONTENTMASK_SOLID | CONTENTS_MONSTERCLIP;
	/**
	*	Native landing resolves actual footprint support at ramp/platform seams without moving any requested endpoint.
	**/
	const auto supportedPoint = [&]( const Vector3DP &point, Vector3DP &feet ) {
		const auto expected = Nav_ResolveHullGoalSupport( mesh, context, point, policy.agent_radius, shape );
		Vector3DP above = expected.probe;
		above.z += policy.max_step_height + 2.0 - mins.z;
		Vector3DP below = expected.probe;
		below.z -= policy.max_step_height + 2.0 + mins.z;
		const auto support = SVG_MMove_Trace( above, mins, maxs, below, passEntity, staticMask, static_cast<mm_trace_shape_t>( shape ) );
		// Only local static walkable support can supply a candidate; an unrelated storey cannot be selected.
		if ( support.startsolid || support.allsolid || support.fraction >= 1.0 || support.entityNumber != 0 ||
			 support.plane.normal[ 2 ] < policy.min_step_normal ) {
			return false;
		}
		feet = Vector3DP( support.endpos );
		feet.z += mins.z;
		return std::fabs( feet.z - point.z - expected.offset ) <= policy.max_step_height + NAV_MOVEMENT_ENDPOINT_TOLERANCE;
	};
	size_t first = 0;
	// Each mandatory original portal partitions the search; no replacement can jump over a door or riser.
	while ( first + 1 < original.size() ) {
		size_t last = first + 1;
		while ( last + 1 < original.size() && !originalForced[ last ] ) {
			++last;
		}
		// Retain the exact original obligation span in diagnostics even when no safe connection exists.
		context.diagnostics.repair_first = static_cast<uint32_t>( first );
		context.diagnostics.repair_last = static_cast<uint32_t>( last );
		context.diagnostics.repair_start = original[ first ];
		context.diagnostics.repair_goal = original[ last ];
		std::vector<node_t> nodes;
		nodes.reserve( 64 );
		Vector3DP start = {};
		// The actual route start stays exact; later authored portal centers may have native seam support above nav Z.
		if ( first == 0 ) {
			start = original.front();
		} else if ( !supportedPoint( original[ first ], start ) ) {
			context.diagnostics.repair_status = 6;
			return false;
		}
		nodes.push_back( { start, -1 } );
		nodes.push_back( { original[ last ], -1 } );
		/**
		*	Retain safe original optional corridor points and nearby corner alternatives as search candidates.
		**/
		const auto appendNode = [&]( const Vector3DP &point, const int32_t corner ) {
			Vector3DP supported = {};
			// Node capacity and stationary native clearance are explicit feasibility bounds, never clearance exemptions.
			if ( nodes.size() == 64 || !supportedPoint( point, supported ) ) {
				return;
			}
			for ( const auto &node : nodes ) {
				if ( QM_Vector3DistanceSqrDP( node.feet, supported ) < 0.000001 ) {
					return;
				}
			}
			if ( SVG_Nav_TestMovementHull( supported, supported, mins, maxs, static_cast<mm_trace_shape_t>( shape ),
				 policy, passEntity, staticMask, true ).reason == svg_nav_movement_reason_t::clear ) {
				nodes.push_back( { supported, corner } );
			}
		};
		for ( size_t index = first + 1; index < last; ++index ) {
			appendNode( original[ index ], -1 );
		}
		// Restrict cached corners to the current obligation span's local footprint, not the whole map.
		Vector3DP lower = original[ first ], upper = lower;
		for ( size_t index = first + 1; index <= last; ++index ) {
			lower.x = std::min( lower.x, original[ index ].x );
			lower.y = std::min( lower.y, original[ index ].y );
			upper.x = std::max( upper.x, original[ index ].x );
			upper.y = std::max( upper.y, original[ index ].y );
		}
		for ( const auto &corner : context.corridor_corners ) {
			if ( corner.standoffMid.x >= lower.x - NAV_CORNER_SEARCH_PADDING_XY && corner.standoffMid.x <= upper.x + NAV_CORNER_SEARCH_PADDING_XY &&
				 corner.standoffMid.y >= lower.y - NAV_CORNER_SEARCH_PADDING_XY && corner.standoffMid.y <= upper.y + NAV_CORNER_SEARCH_PADDING_XY ) {
				appendNode( corner.standoffMid, corner.sourceCorner );
			}
		}
		/**
		*	Portal interiors provide corridor witnesses that a corner-only visibility graph can miss.
		*	Every added point still requires local native support and every selected edge receives full proof.
		**/
		for ( const auto &portal : context.portals ) {
			// Endpoints can be wall contacts; sample bounded interior fractions without changing mandatory portals.
			for ( const double fraction : { 0.25, 0.5, 0.75 } ) {
				const Vector3DP point = portal.left + ( portal.right - portal.left ) * fraction;
				// Keep candidates local to this obligation span instead of connecting unrelated map regions.
				if ( point.x >= lower.x - NAV_CORNER_SEARCH_PADDING_XY && point.x <= upper.x + NAV_CORNER_SEARCH_PADDING_XY &&
					 point.y >= lower.y - NAV_CORNER_SEARCH_PADDING_XY && point.y <= upper.y + NAV_CORNER_SEARCH_PADDING_XY ) {
					appendNode( point, -1 );
				}
			}
		}
		/**
		*	A bounded A* search chooses collision-free directed links at the unchanged hull radius.
		*	Euclidean XY distance is an admissible heuristic; only complete native proofs create edges.
		**/
		context.diagnostics.repair_nodes = std::max( context.diagnostics.repair_nodes, static_cast<uint32_t>( nodes.size() ) );
		std::array<double, 64> cost;
		cost.fill( std::numeric_limits<double>::infinity() );
		std::array<int32_t, 64> parent;
		parent.fill( -1 );
		std::array<bool, 64> closed{};
		cost[ 0 ] = 0.0;
		for ( size_t iteration = 0; iteration < nodes.size(); ++iteration ) {
			int32_t active = -1;
			double best = std::numeric_limits<double>::infinity();
			// Direct indexing avoids heap nodes and preserves deterministic tie order.
			for ( size_t index = 0; index < nodes.size(); ++index ) {
				const double estimate = cost[ index ] + QM_Vector3Distance2DDP( nodes[ index ].feet, nodes[ 1 ].feet );
				if ( !closed[ index ] && estimate < best ) {
					best = estimate;
					active = static_cast<int32_t>( index );
				}
			}
			if ( active < 0 || active == 1 ) {
				break;
			}
			closed[ active ] = true;
			std::vector<size_t> neighbors;
			neighbors.reserve( nodes.size() );
			for ( size_t index = 1; index < nodes.size(); ++index ) {
				if ( !closed[ index ] ) {
					neighbors.push_back( index );
				}
			}
			std::sort( neighbors.begin(), neighbors.end(), [&]( const size_t a, const size_t b ) {
				// Always try the exact span destination, then deterministic nearest local alternatives.
				if ( a == 1 || b == 1 ) {
					return a == 1 && b != 1;
				}
				const double da = QM_Vector3Distance2DSqrDP( nodes[ active ].feet, nodes[ a ].feet );
				const double db = QM_Vector3Distance2DSqrDP( nodes[ active ].feet, nodes[ b ].feet );
				return da != db ? da < db : a < b;
			} );
			// Nearby points across a wall can crowd out the necessary farther corner in a nearest-neighbor cutoff.
			// Evaluate the bounded local set; the shared edge/sample ceilings still cap total query work.
			for ( size_t rank = 0; rank < neighbors.size(); ++rank ) {
				const size_t next = neighbors[ rank ];
				const double proposal = cost[ active ] + QM_Vector3Distance2DDP( nodes[ active ].feet, nodes[ next ].feet );
				if ( proposal >= cost[ next ] ) {
					continue;
				}
				if ( ++edgeProofs > 1024 || samples >= 32768 ) {
					context.diagnostics.repair_status = 4;
					return false;
				}
				const std::array<Vector3DP, 2> link{ nodes[ active ].feet, nodes[ next ].feet };
				const auto proof = Nav_ValidateHullRouteNativeMainThread( mesh, context, link, mins, maxs, shape, policy, passEntity, next == 1 && last + 1 == original.size(), 32768u - samples );
				samples += proof.samples;
				context.diagnostics.repair_edges = edgeProofs;
				context.diagnostics.repair_samples = samples;
				if ( samples > 32768 ) {
					context.diagnostics.repair_status = 4;
					return false;
				}
				// Exhausting the remaining sample allowance is resource limitation, not geometric infeasibility.
				if ( proof.reason == nav_route_reason_t::work_limit ) {
					context.diagnostics.repair_status = 4;
					return false;
				}
				// A rejected native/geometry edge cannot enter the predecessor tree.
				if ( proof.reason == nav_route_reason_t::clear ) {
					cost[ next ] = proposal;
					parent[ next ] = active;
				}
			}
		}
		if ( parent[ 1 ] < 0 ) {
			context.diagnostics.repair_status = 3;
			return false;
		}
		/**
		*	Reconstruct forward candidates and preserve exact mandatory portal coordinates and provenance.
		**/
		std::vector<size_t> reverse;
		for ( int32_t index = 1; index != 0 && index >= 0; index = parent[ index ] ) {
			reverse.push_back( static_cast<size_t>( index ) );
		}
		for ( auto it = reverse.rbegin(); it != reverse.rend(); ++it ) {
			const auto &node = nodes[ *it ];
			const bool endpoint = *it == 1;
			const Vector3DP point = endpoint ? original[ last ] : node.feet;
			repaired.push_back( point );
			repairedForced.push_back( endpoint ? originalForced[ last ] : node.corner >= 0 );
			if ( !endpoint && node.corner >= 0 ) {
				repairedAnchors.push_back( { point, { NAV_WAYPOINT_CORNER_APEX | NAV_WAYPOINT_MANDATORY, -1, node.corner }, Nav_FindClosestFaceInLeaf( mesh, context, point ) } );
			}
		}
		first = last;
	}
	/**
	*	Commit only after the complete stitched route proves the actual start, all links and exact final destination.
	**/
	if ( Nav_ValidateHullRouteNativeMainThread( mesh, context, repaired, mins, maxs, shape, policy, passEntity ).reason != nav_route_reason_t::clear ) {
		context.diagnostics.repair_status = 5;
		return false;
	}
	context.diagnostics.repair_status = 2;
	output = std::move( repaired );
	outputForced = std::move( repairedForced );
	context.anchors = std::move( repairedAnchors );
	return true;
}

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
bool Nav_PrepareRouteNativeMainThread( const nav_route_mesh_view_t &mesh, svg_nav_route_context_t &context, const std::vector<int32_t> &path, const Vector3DP &startPos, const Vector3DP &goalPos, double agentRadius, std::vector<Vector3DP> &outWaypoints, std::vector<bool> *outForcedWaypoints, const Vector3 &agentMins, const Vector3 &agentMaxs, int32_t traceShape, svg_vector_t<svg_nav_waypoint_t, TAG_SVGAME_NAVMESH> *outWaypointRecords, const nav_path_policy_t *traversalPolicy, nav_route_validation_t *outValidation, svg_base_edict_t *passEntity ) {
	// Native traces and tagged scratch ownership stay on the server owner.
	Q_assert( Nav_QueryIsOwnerThread() );
	nav_perf_scope_t funnel_scope( NAV_PERF_PHASE_PORTAL_FUNNEL );
	/**
	*	Generation always owns its temporary mandatory flags, including callers requesting only semantic records.
	**/
	std::vector<bool> localForcedWaypoints;
	if ( outForcedWaypoints == nullptr ) {
		outForcedWaypoints = &localForcedWaypoints;
	}
	// Clear query-local source anchors before any early return or new generation.
	context.anchors.clear();
	if ( outWaypointRecords != nullptr ) {
		outWaypointRecords->clear();
	}
	outWaypoints.clear();
	outWaypoints.reserve( 128 );
	if ( outForcedWaypoints != nullptr ) {
		outForcedWaypoints->clear();
		outForcedWaypoints->reserve( 128 );
	}

	const Vector3DP startPosDP = startPos;
	const Vector3DP goalPosDP = goalPos;
	// Share one resolved policy across refinement candidates and final complete-route validation.
	nav_path_policy_t nativePolicy = traversalPolicy != nullptr ? *traversalPolicy : nav_path_policy_t{};
	nativePolicy.agent_radius = static_cast<float>( agentRadius > 0.0 ? agentRadius : NAV_DEFAULT_AGENT_RADIUS );
	std::vector<Vector3DP> unrefinedPoints;
	std::vector<bool> unrefinedForced;
	svg_vector_t<nav_waypoint_anchor_t, TAG_SVGAME_NAVMESH> portalAnchors;
	const auto finishRoute = [&]() {
		// Search and native verification use the same policy; default callers retain the reference policy.
		nav_path_policy_t policy = traversalPolicy != nullptr ? *traversalPolicy : nav_path_policy_t{};
		policy.agent_radius = static_cast<float>( agentRadius > 0.0 ? agentRadius : NAV_DEFAULT_AGENT_RADIUS );
		const auto validation = Nav_ValidateHullRouteNativeMainThread( mesh, context, outWaypoints, agentMins, agentMaxs, traceShape, policy, passEntity );
		if ( outValidation != nullptr ) {
			*outValidation = validation;
		}
		// Failed endpoints/links remain exact in diagnostics, but cannot be installed as a valid route.
		if ( validation.reason != nav_route_reason_t::clear ) {
			return false;
		}
		// Semantic overflow or lost source ownership is distinct from a geometrically valid route.
		if ( outWaypointRecords != nullptr && !Nav_BuildWaypointRecords( mesh, context, outWaypoints, outForcedWaypoints, policy.agent_radius, *outWaypointRecords ) ) {
			if ( outValidation != nullptr ) {
				outValidation->reason = nav_route_reason_t::semantic_failure;
			}
			return false;
		}
		return true;
	};

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
		/**
		*	A shared face does not imply swept-hull visibility near its boundary corners.
		*	Keep the direct fast path only after complete proof; failed links use normal corner refinement.
		**/
		if ( finishRoute() ) {
			return true;
		}
		// Invalid literal endpoints are never repaired by substituting a corner or snapped destination.
		if ( outValidation != nullptr && ( outValidation->reason == nav_route_reason_t::invalid_start ||
			 outValidation->reason == nav_route_reason_t::invalid_goal ) ) {
			return false;
		}
		unrefinedPoints = outWaypoints;
		unrefinedForced = *outForcedWaypoints;
		portalAnchors = context.anchors;
		Nav_EnforceConvexCornerWaypoints( mesh, context, path, agentRadius, outWaypoints, outForcedWaypoints, agentMins, agentMaxs, traceShape, nativePolicy, passEntity );
		if ( finishRoute() ) {
			return true;
		}
		return Nav_RepairRefinementRoute( mesh, context, unrefinedPoints, unrefinedForced, portalAnchors, outWaypoints,
			*outForcedWaypoints, agentMins, agentMaxs, traceShape, nativePolicy, passEntity ) && finishRoute();
	}

	context.portals.clear();
	if ( context.portals.capacity() < 512 ) {
		context.portals.reserve( 512 );
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
		if ( Nav_GetPortalEndpoints( mesh, context, face_idx, next_face_idx, &left, &right ) ) {
			const Vector3DP rawLeft = left;
			const Vector3DP rawRight = right;

			bool isDoor = false;
			int32_t door_entity_id = ENTITYNUM_NONE;
			double stepRise = 0.0;
			int32_t sourcePortal = -1;
			const nav_face_t &face = mesh.faces[ face_idx ];
			for ( int32_t e = 0; e < face.num_edges; e++ ) {
				const nav_halfedge_t &he = mesh.halfedges[ face.first_edge_idx + e ];
				if ( he.twin_idx != -1 && mesh.halfedges[ he.twin_idx ].face_idx == next_face_idx ) {
					stepRise = he.z_diff;
					sourcePortal = face.first_edge_idx + e;
					if ( he.edge_entity_id != ENTITYNUM_NONE ) {
						isDoor = true;
						door_entity_id = he.edge_entity_id;
					}
					break;
				}
			}

			const nav_face_t &nextFace = mesh.faces[ next_face_idx ];
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
				Nav_RecordWaypointConstraint( context, mid, NAV_WAYPOINT_DOOR_PORTAL | NAV_WAYPOINT_MANDATORY | ( isStepTransition ? NAV_WAYPOINT_STEP_RISER : 0u ), sourcePortal, -1, next_face_idx );
				context.portals.push_back( portal );
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
					const nav_halfedge_t &phe = mesh.halfedges[ face.first_edge_idx + pe ];
					if ( phe.twin_idx != -1 ) {
						const nav_face_t &twinFace = mesh.faces[ mesh.halfedges[ phe.twin_idx ].face_idx ];
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
					if ( Nav_PointInsideFace2D( mesh, context, approachPoint, face ) ) {
						funnel_portal_t approachPortal;
						approachPortal.left = approachPoint;
						approachPortal.right = approachPoint;
						approachPortal.force_waypoint = true;
						Nav_RecordWaypointConstraint( context, approachPoint, NAV_WAYPOINT_STEP_APPROACH | NAV_WAYPOINT_MANDATORY, sourcePortal, -1, face_idx );
						context.portals.push_back( approachPortal );
					}
				}

				// 2. Stair step transition (ascending or descending): lock portal to the exact midpoint of the step width at tread elevation.
				Vector3DP mid = ( rawLeft + rawRight ) * 0.5;
				mid.z = nextFace.center.z;
				portal.left = mid;
				portal.right = mid;
				portal.force_waypoint = true;
				Nav_RecordWaypointConstraint( context, mid, NAV_WAYPOINT_STEP_RISER | NAV_WAYPOINT_MANDATORY, sourcePortal, -1, next_face_idx );
				context.portals.push_back( portal );

				// 3. When exiting a stair flight onto a flat platform/room, insert a frontal runway landing waypoint.
				// This forces the agent to walk straight forward onto the solid platform before commencing any turn,
				// completely preventing lateral drift off the platform cliff edges.
				if ( portalLen > 0.001 ) {
					bool nextIsStep = false;
					for ( int32_t ne = 0; ne < nextFace.num_edges; ne++ ) {
						const nav_halfedge_t &nhe = mesh.halfedges[ nextFace.first_edge_idx + ne ];
						if ( nhe.twin_idx != -1 ) {
							const nav_face_t &twinFace = mesh.faces[ mesh.halfedges[ nhe.twin_idx ].face_idx ];
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
						if ( Nav_PointInsideFace2D( mesh, context, runwayPoint, nextFace ) ) {
							funnel_portal_t runwayPortal;
							runwayPortal.left = runwayPoint;
							runwayPortal.right = runwayPoint;
							runwayPortal.force_waypoint = true;
							Nav_RecordWaypointConstraint( context, runwayPoint, NAV_WAYPOINT_STEP_LANDING | NAV_WAYPOINT_MANDATORY, sourcePortal, -1, next_face_idx );
							context.portals.push_back( runwayPortal );
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
				if ( Nav_ClipPortalForAgentClearance( mesh, context, face_idx, next_face_idx, rawLeft, rawRight, wallClearance, cornerClearance, &clippedLeft, &clippedRight, &isNarrowPortal ) ) {
					portal.left = clippedLeft;
					portal.right = clippedRight;
					portal.force_waypoint = isNarrowPortal;
					// A collapsed narrow portal remains an owned funnel obligation, distinct from a door or stair.
					if ( isNarrowPortal ) {
						Nav_RecordWaypointConstraint( context, portal.left, NAV_WAYPOINT_FUNNEL | NAV_WAYPOINT_MANDATORY, sourcePortal, -1, next_face_idx );
					}
				} else {
					portal.left = rawLeft;
					portal.right = rawRight;
					portal.force_waypoint = false;
				}
				context.portals.push_back( portal );
			}
		} else {
			const Vector3DP center = mesh.faces[ face_idx ].center;
			context.portals.push_back( { center, center } );
		}
	}

	funnel_portal_t goalPortal;
	goalPortal.left = goalPosDP;
	goalPortal.right = goalPosDP;
	goalPortal.force_waypoint = false;
	context.portals.push_back( goalPortal );

	context.portal_indices.clear();
	if ( context.portal_indices.capacity() < 256 ) {
		context.portal_indices.reserve( 256 );
	}
	auto AppendWaypoint = [&]( const Vector3DP &waypoint, const int32_t pIdx, const bool forced ) {
		outWaypoints.push_back( waypoint );
		context.portal_indices.push_back( pIdx );
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

	for ( int32_t i = 0; i < ( int32_t )context.portals.size(); ++i ) {
		const Vector3DP &left = context.portals[ i ].left;
		const Vector3DP &right = context.portals[ i ].right;

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

		if ( context.portals[ i ].force_waypoint ) {
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

	AppendWaypoint( goalPosDP, static_cast<int32_t>( context.portals.size() ) - 1, false );

	/**
	*	Curved/Cylindrical Obstacle Subdivision:
	*	If any segment (W_i -> W_{i+1}) intersects world geometry (e.g. cutting through a curved/cylindrical brush),
	*	subdivide the segment by inserting the intermediate portal midpoints in strict sequential order.
	**/
	if ( outWaypoints.size() >= 2 && outWaypoints.size() == context.portal_indices.size() ) {
		context.sub_waypoints.clear();
		context.sub_forced.clear();
		if ( context.sub_waypoints.capacity() < ( outWaypoints.size() * 2 + 16 ) ) {
			context.sub_waypoints.reserve( outWaypoints.size() * 2 + 16 );
		}
		if ( outForcedWaypoints != nullptr && context.sub_forced.capacity() < ( outWaypoints.size() * 2 + 16 ) ) {
			context.sub_forced.reserve( outWaypoints.size() * 2 + 16 );
		}

		context.sub_waypoints.push_back( outWaypoints.front() );
		if ( outForcedWaypoints != nullptr && !outForcedWaypoints->empty() ) {
			context.sub_forced.push_back( outForcedWaypoints->front() );
		}

		for ( size_t i = 0; i + 1 < outWaypoints.size(); ++i ) {
			const Vector3DP &p0 = outWaypoints[ i ];
			const Vector3DP &p1 = outWaypoints[ i + 1 ];
			const int32_t portal0 = context.portal_indices[ i ];
			const int32_t portal1 = context.portal_indices[ i + 1 ];
			const bool isP1Forced = ( outForcedWaypoints != nullptr && i + 1 < outForcedWaypoints->size() && ( *outForcedWaypoints )[ i + 1 ] );

			const double clearance = ( agentRadius > 0.0 ) ? agentRadius : NAV_DEFAULT_AGENT_RADIUS;
			if ( !Nav_HasGeometricLineOfSight2D( mesh, context, p0, p1, clearance ) && ( portal1 > portal0 + 1 ) ) {
				// Subdivide along the intermediate portals of the corridor in strict forward order
				for ( int32_t p = portal0 + 1; p < portal1; ++p ) {
					const Vector3DP mid = ( context.portals[ p ].left + context.portals[ p ].right ) * 0.5;
					if ( QM_Vector3DistanceSqrDP( mid, context.sub_waypoints.back() ) >= ( 8.0 * 8.0 ) &&
						 QM_Vector3DistanceSqrDP( mid, p1 ) >= ( 8.0 * 8.0 ) ) {
						context.sub_waypoints.push_back( mid );
						if ( outForcedWaypoints != nullptr ) {
							context.sub_forced.push_back( false );
						}
					}
				}
			}

			context.sub_waypoints.push_back( p1 );
			if ( outForcedWaypoints != nullptr ) {
				context.sub_forced.push_back( isP1Forced );
			}
		}

		outWaypoints.assign( context.sub_waypoints.begin(), context.sub_waypoints.end() );
		if ( outForcedWaypoints != nullptr ) {
			outForcedWaypoints->assign( context.sub_forced.begin(), context.sub_forced.end() );
		}
	}

	/**
	*	Enforce strict agent clearance across all path segments by inserting convex corner standoff waypoints
	*	for any path polyline segment that passes within agent clearance of a solid obstacle corner.
	**/
	unrefinedPoints = outWaypoints;
	unrefinedForced = *outForcedWaypoints;
	portalAnchors = context.anchors;
	Nav_EnforceConvexCornerWaypoints( mesh, context, path, agentRadius, outWaypoints, outForcedWaypoints, agentMins, agentMaxs, traceShape, nativePolicy, passEntity );

	/**
	*	Sanitize output waypoints: remove collinear or near-duplicate consecutive points.
	**/
	if ( outWaypoints.size() >= 2 ) {
		context.clean_waypoints.clear();
		context.clean_forced.clear();
		if ( context.clean_waypoints.capacity() < ( outWaypoints.size() + 16 ) ) {
			context.clean_waypoints.reserve( outWaypoints.size() + 16 );
		}
		if ( outForcedWaypoints != nullptr && context.clean_forced.capacity() < ( outWaypoints.size() + 16 ) ) {
			context.clean_forced.reserve( outWaypoints.size() + 16 );
		}

		context.clean_waypoints.push_back( outWaypoints.front() );
		if ( outForcedWaypoints != nullptr ) {
			context.clean_forced.push_back( outForcedWaypoints->front() );
		}

		for ( size_t i = 1; i < outWaypoints.size(); ++i ) {
			const double distSqr = QM_Vector3DistanceSqrDP( outWaypoints[ i ], context.clean_waypoints.back() );
			const bool isForced = ( outForcedWaypoints != nullptr && i < outForcedWaypoints->size() && ( *outForcedWaypoints )[ i ] );
			const bool isLast = ( i == outWaypoints.size() - 1 );

			/**
			*	Corner refinement can create A -> unforced portal -> nearby A hairpins.
			*	Remove the unforced excursion only when the retained corner endpoints
			*	are within one hull radius and their connecting segment has full clearance.
			*	Both mandatory endpoints remain intact unless they coincide exactly.
			**/
			const size_t cleanCount = context.clean_waypoints.size();
			if ( cleanCount >= 2 && outForcedWaypoints != nullptr &&
				 !context.clean_forced.back() &&
				 QM_Vector3DistanceSqrDP( outWaypoints[ i ], context.clean_waypoints[ cleanCount - 2 ] ) <= agentRadius * agentRadius &&
				 Nav_HasGeometricLineOfSight2D( mesh, context, context.clean_waypoints[ cleanCount - 2 ], outWaypoints[ i ], agentRadius ) ) {
				context.clean_waypoints.pop_back();
				context.clean_forced.pop_back();
				// Coalesce only identical corners, preserving each distinct mandatory location.
				if ( QM_Vector3DistanceSqrDP( outWaypoints[ i ], context.clean_waypoints.back() ) < 0.000001 ) {
					context.clean_forced.back() = context.clean_forced.back() || isForced;
					// Keep the exact requested endpoint rather than its near-equal predecessor.
					if ( isLast ) {
						context.clean_waypoints.back() = outWaypoints[ i ];
					}
					continue;
				}
			}

			// Preserve waypoints that represent meaningful progression (>= 2.0 units), are forced portals, or are the final goal
			if ( distSqr >= 4.0 || isForced || isLast ) {
				context.clean_waypoints.push_back( outWaypoints[ i ] );
				if ( outForcedWaypoints != nullptr ) {
					context.clean_forced.push_back( isForced );
				}
			}
		}

		// Collinear decimation: remove redundant intermediate points along straight sections
		if ( context.clean_waypoints.size() >= 3 ) {
			context.simplified_waypoints.clear();
			context.simplified_forced.clear();
			if ( context.simplified_waypoints.capacity() < ( context.clean_waypoints.size() + 16 ) ) {
				context.simplified_waypoints.reserve( context.clean_waypoints.size() + 16 );
			}
			if ( context.simplified_forced.capacity() < ( context.clean_waypoints.size() + 16 ) ) {
				context.simplified_forced.reserve( context.clean_waypoints.size() + 16 );
			}

			context.simplified_waypoints.push_back( context.clean_waypoints.front() );
			context.simplified_forced.push_back( context.clean_forced.front() );

			for ( size_t i = 1; i + 1 < context.clean_waypoints.size(); ++i ) {
				const bool isForced = context.clean_forced[ i ];
				if ( isForced ) {
					context.simplified_waypoints.push_back( context.clean_waypoints[ i ] );
					context.simplified_forced.push_back( true );
					continue;
				}

				const Vector3DP &prev = context.simplified_waypoints.back();
				const Vector3DP &curr = context.clean_waypoints[ i ];
				const Vector3DP &next = context.clean_waypoints[ i + 1 ];

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
						if ( Nav_HasGeometricLineOfSight2D( mesh, context, prev, next, clearance ) ) {
							continue;
						}
					}
				}

				context.simplified_waypoints.push_back( curr );
				context.simplified_forced.push_back( false );
			}

			context.simplified_waypoints.push_back( context.clean_waypoints.back() );
			context.simplified_forced.push_back( context.clean_forced.back() );

			context.clean_waypoints = context.simplified_waypoints;
			context.clean_forced = context.simplified_forced;
		}

		outWaypoints.assign( context.clean_waypoints.begin(), context.clean_waypoints.end() );
		if ( outForcedWaypoints != nullptr ) {
			outForcedWaypoints->assign( context.clean_forced.begin(), context.clean_forced.end() );
		}
	}

	/**
	*	Final decimation must not leave optional portal centers inside the required wall clearance.
	*	Repair only a demonstrably safe flat neighbor link while preserving all mandatory points.
	**/
	Nav_PruneUnsafeFlatPortals( mesh, context, outWaypoints, outForcedWaypoints, agentRadius > 0.0 ? agentRadius : NAV_DEFAULT_AGENT_RADIUS );
	// Full-radius corner cuts can remain between mandatory points even when both stationary points are safe.
	Nav_RepairUnsafeFlatLinks( mesh, context, outWaypoints, *outForcedWaypoints, agentRadius > 0.0 ? agentRadius : NAV_DEFAULT_AGENT_RADIUS );

	// Source obligations are assembled only after every route mutation has completed.
	if ( finishRoute() ) {
		return true;
	}
	// Discard only rejected corner proposals; original mandatory portals remain exact in every alternative.
	return Nav_RepairRefinementRoute( mesh, context, unrefinedPoints, unrefinedForced, portalAnchors, outWaypoints,
		*outForcedWaypoints, agentMins, agentMaxs, traceShape, nativePolicy, passEntity ) && finishRoute();
}

/**
*	@brief	Compatibility wrapper owns one complete native route context through semantic assembly.
*	@note	No mutable funnel/refinement scratch is shared with another request.
**/
bool Nav_StringPull( const std::vector<int32_t> &path, const Vector3DP &startPos, const Vector3DP &goalPos, double agentRadius, std::vector<Vector3DP> &outWaypoints, std::vector<bool> *outForcedWaypoints, const Vector3 &agentMins, const Vector3 &agentMaxs, int32_t traceShape, svg_vector_t<svg_nav_waypoint_t, TAG_SVGAME_NAVMESH> *outWaypointRecords, const nav_path_policy_t *traversalPolicy, nav_route_validation_t *outValidation, svg_base_edict_t *passEntity, nav_path_diagnostics_t *diagnostics, svg_nav_route_context_t *workspace ) {
	nav_query_snapshot_scope_t routeLease;
	// A complete native route owns one coherent snapshot across every nested operation.
	if ( routeLease.Get() == nullptr ) {
		return false;
	}

	// Preserve the explicit A* diagnostic snapshot before route-local refinement.
	svg_nav_route_context_t localContext;
	auto &context = workspace != nullptr ? *workspace : localContext;
	context.diagnostics = diagnostics != nullptr ? *diagnostics : nav_path_diagnostics_t{};
	// Prepare native lookup visitation on the owner before entering the route phase.
	context.edge_tokens.resize( routeLease.Get()->RouteView().solid_edges.size() );
	context.corner_tokens.resize( routeLease.Get()->RouteView().corners.size() );
	const auto inputDiagnostics = context.diagnostics;
	const bool ok = Nav_PrepareRouteNativeMainThread( routeLease.Get()->RouteView(), context, path, startPos, goalPos, agentRadius, outWaypoints,
		outForcedWaypoints, agentMins, agentMaxs, traceShape, outWaypointRecords, traversalPolicy, outValidation, passEntity );
	/**
	*	Opt-in comparison executes the full native route through a second independent context.
	*	Compare exact endpoints, retained mandatory source records and route proof/cost, including rejected replies.
	**/
	if ( s_nav_query_parity ) {
		svg_nav_route_context_t referenceContext;
		referenceContext.diagnostics = inputDiagnostics;
		referenceContext.edge_tokens.resize( context.edge_tokens.size() );
		referenceContext.corner_tokens.resize( context.corner_tokens.size() );
		svg_nav_result_t reference;
		const bool referenceOk = Nav_PrepareRouteNativeMainThread( routeLease.Get()->RouteView(), referenceContext,
			path, startPos, goalPos, agentRadius, reference.waypoints, &reference.forced, agentMins, agentMaxs, traceShape,
			outWaypointRecords != nullptr ? &reference.records : nullptr, traversalPolicy,
			outValidation != nullptr ? &reference.validation : nullptr, passEntity );
		bool equal = ok == referenceOk && outWaypoints.size() == reference.waypoints.size() &&
			context.diagnostics.graph_cost == referenceContext.diagnostics.graph_cost;
		// Exact component comparisons preserve literal start/goal coordinates and every retained interior point.
		for ( size_t index = 0; equal && index < outWaypoints.size(); ++index ) {
			const auto &a = outWaypoints[ index ];
			const auto &b = reference.waypoints[ index ];
			equal = a.x == b.x && a.y == b.y && a.z == b.z;
		}
		if ( outForcedWaypoints != nullptr ) {
			equal = equal && *outForcedWaypoints == reference.forced;
		}
		if ( outWaypointRecords != nullptr ) {
			equal = equal && outWaypointRecords->size() == reference.records.size();
			for ( size_t index = 0; equal && index < reference.records.size(); ++index ) {
				equal = Nav_WaypointRecordsEqual( ( *outWaypointRecords )[ index ], reference.records[ index ] );
			}
		}
		if ( outValidation != nullptr ) {
			equal = equal && outValidation->reason == reference.validation.reason &&
				outValidation->segment == reference.validation.segment && outValidation->length == reference.validation.length &&
				outValidation->minimum_clearance == reference.validation.minimum_clearance &&
				outValidation->samples == reference.validation.samples && outValidation->traces == reference.validation.traces;
		}
		++s_nav_route_compared;
		if ( !equal ) {
			++s_nav_route_mismatches;
			gi.dprintf( "[nav route parity] FAIL points=%zu/%zu validity=%d/%d\n", outWaypoints.size(), reference.waypoints.size(), ok, referenceOk );
		}
	}
	// Compatibility logging receives a copy after the request finishes.
	// Explicit callers retain their own terminal diagnostics across unrelated queries.
	if ( diagnostics != nullptr ) {
		*diagnostics = context.diagnostics;
	}
	s_nav_last_path_diagnostics = context.diagnostics;
	return ok;
}


/**
*	@brief	Build a smoothed string-pulled path using the Funnel algorithm (single-precision convenience wrapper).
**/
bool Nav_StringPull( const std::vector<int32_t> &path, const Vector3 &startPos, const Vector3 &goalPos, float agentRadius, std::vector<Vector3> &outWaypoints, std::vector<bool> *outForcedWaypoints, const Vector3 &agentMins, const Vector3 &agentMaxs, int32_t traceShape, svg_vector_t<svg_nav_waypoint_t, TAG_SVGAME_NAVMESH> *outWaypointRecords, const nav_path_policy_t *traversalPolicy, nav_route_validation_t *outValidation, svg_base_edict_t *passEntity ) {
	// Conversion storage is exclusively owned by this native request.
	std::vector<Vector3DP> waypointsDP;
	waypointsDP.clear();
	if ( waypointsDP.capacity() < 256 ) {
		waypointsDP.reserve( 256 );
	}
	const bool ok = Nav_StringPull( path, Vector3DP( startPos ), Vector3DP( goalPos ), static_cast<double>( agentRadius ), waypointsDP, outForcedWaypoints, agentMins, agentMaxs, traceShape, outWaypointRecords, traversalPolicy, outValidation, passEntity );
	outWaypoints.clear();
	outWaypoints.reserve( waypointsDP.size() );
	for ( const Vector3DP &wp : waypointsDP ) {
		outWaypoints.push_back( static_cast<Vector3>( wp ) );
	}
	return ok;
}

//! Generation tag identifying which mesh generation g_nav_entity_edges was constructed for.
static uint64_t s_nav_entity_edges_generation = 0;

/**
*	@brief	Reconstruct the entity-to-halfedge mapping table from current mesh halfedges.
*	@note	Tags the registry with g_nav_mesh_generation so dynamic updates avoid full-mesh scans.
**/
void Nav_RebuildEntityEdgeRegistry( void ) {
	g_nav_entity_edges.clear();
	int32_t max_ent = 0;
	for ( size_t i = 0; i < g_nav_halfedges.size(); ++i ) {
		const int32_t ent = g_nav_halfedges[ i ].edge_entity_id;
		if ( ent > max_ent ) {
			max_ent = ent;
		}
	}
	if ( max_ent > 0 ) {
		g_nav_entity_edges.resize( static_cast<size_t>( max_ent ) + 1 );
		for ( size_t i = 0; i < g_nav_halfedges.size(); ++i ) {
			const int32_t ent = g_nav_halfedges[ i ].edge_entity_id;
			if ( ent > 0 ) {
				g_nav_entity_edges[ ent ].push_back( static_cast<int32_t>( i ) );
			}
		}
	}
	s_nav_entity_edges_generation = g_nav_mesh_generation;
}

/**
*	@brief	Globally enables or disables all nav mesh edges associated with a specific entity ID.
*	@details Updates edge flags, synchronizes twin edges to maintain halfedge topology invariants,
*			updates sector graph portal passability, and invalidates the obstacle corners table.
**/
void Nav_SetEntityEdgesState( int32_t entity_id, uint32_t flags, bool enable ) {
	if ( entity_id <= 0 ) {
		return;
	}

	bool state_changed = false;

	// Ensure entity edge registry is synchronized with the current mesh generation:
	if ( s_nav_entity_edges_generation != g_nav_mesh_generation ) {
		Nav_RebuildEntityEdgeRegistry();
	}

	if ( static_cast<size_t>( entity_id ) < g_nav_entity_edges.size() && !g_nav_entity_edges[ entity_id ].empty() ) {
		const std::vector<int32_t> &edges = g_nav_entity_edges[ entity_id ];
		for ( size_t i = 0; i < edges.size(); ++i ) {
			const int32_t edge_idx = edges[ i ];
			if ( edge_idx < 0 || edge_idx >= static_cast<int32_t>( g_nav_halfedges.size() ) ) {
				continue;
			}
			nav_halfedge_t &he = g_nav_halfedges[ edge_idx ];
			const uint32_t old_flags = he.flags;
			if ( enable ) {
				he.flags |= flags;
			} else {
				he.flags &= ~flags;
			}
			if ( he.flags != old_flags ) {
				state_changed = true;
			}

			// Update twin edge consistently to preserve half-edge topology invariants:
			if ( he.twin_idx >= 0 && he.twin_idx < static_cast<int32_t>( g_nav_halfedges.size() ) ) {
				nav_halfedge_t &twin = g_nav_halfedges[ he.twin_idx ];
				const uint32_t twin_old = twin.flags;
				if ( enable ) {
					twin.flags |= flags;
				} else {
					twin.flags &= ~flags;
				}
				if ( twin.flags != twin_old ) {
					state_changed = true;
				}
			}
		}
	} else {
		// Fallback: whole-edge sweep only if registry data is absent for this entity:
		for ( size_t edge_idx = 0; edge_idx < g_nav_halfedges.size(); ++edge_idx ) {
			nav_halfedge_t &halfedge = g_nav_halfedges[ edge_idx ];
			bool matches = ( halfedge.edge_entity_id == entity_id );
			if ( !matches && halfedge.face_idx >= 0 && halfedge.face_idx < static_cast<int32_t>( g_nav_faces.size() ) ) {
				const nav_face_t &face = g_nav_faces[ halfedge.face_idx ];
				if ( face.entity_id == entity_id || face.transition_entity_id == entity_id ) {
					matches = true;
				}
			}
			if ( matches ) {
				const uint32_t old_flags = halfedge.flags;
				if ( enable ) {
					halfedge.flags |= flags;
				} else {
					halfedge.flags &= ~flags;
				}
				if ( halfedge.flags != old_flags ) {
					state_changed = true;
				}

				if ( halfedge.twin_idx >= 0 && halfedge.twin_idx < static_cast<int32_t>( g_nav_halfedges.size() ) ) {
					nav_halfedge_t &twin = g_nav_halfedges[ halfedge.twin_idx ];
					const uint32_t twin_old = twin.flags;
					if ( enable ) {
						twin.flags |= flags;
					} else {
						twin.flags &= ~flags;
					}
					if ( twin.flags != twin_old ) {
						state_changed = true;
					}
				}
			}
		}
	}

	// Update sector graph portal passability if this entity is associated with any portals:
	if ( ( flags & NAV_EDGE_DISABLED ) != 0 ) {
		const bool isPassable = !enable;
		bool portalsChanged = false;
		for ( auto &portal : g_nav_portals ) {
			if ( portal.entity_number == entity_id && portal.is_passable != isPassable ) {
				portal.is_passable = isPassable;
				portalsChanged = true;
			}
		}
		if ( portalsChanged ) {
			Nav_SectorGraph_Build();
		}
	}

	if ( state_changed ) {
		Nav_InvalidateObstacleCornersTable();
	}
}

/**
*	@brief	Resolve a face query on owner-pinned copied geometry.
*	@note	The explicit overload carries immutable input and exclusive context.
**/
int32_t Nav_FindClosestFaceInLeaf( const Vector3DP &point ) {
	nav_query_snapshot_scope_t lease;
	const auto *snapshot = lease.Get();
	// An unavailable publication cannot authorize lookup in a partial live mesh.
	if ( snapshot == nullptr ) {
		return -1;
	}
	svg_nav_route_context_t context;
	return Nav_FindClosestFaceInLeaf( snapshot->RouteView(), context, point );
}

/**
*	@brief	Resolve a face query on owner-pinned copied geometry.
*	@note	The explicit overload carries immutable input and exclusive context.
**/
int32_t Nav_FindFaceInLeafStrict( const Vector3DP &point ) {
	nav_query_snapshot_scope_t lease;
	const auto *snapshot = lease.Get();
	// An unavailable publication cannot authorize lookup in a partial live mesh.
	if ( snapshot == nullptr ) {
		return -1;
	}
	svg_nav_route_context_t context;
	return Nav_FindFaceInLeafStrict( snapshot->RouteView(), context, point );
}

/**
*	@brief	Resolve a face query on owner-pinned copied geometry.
*	@note	The explicit overload carries immutable input and exclusive context.
**/
int32_t Nav_FindPolyInLeaf( const Vector3DP &point ) {
	nav_query_snapshot_scope_t lease;
	const auto *snapshot = lease.Get();
	// An unavailable publication cannot authorize lookup in a partial live mesh.
	if ( snapshot == nullptr ) {
		return -1;
	}
	svg_nav_route_context_t context;
	return Nav_FindPolyInLeaf( snapshot->RouteView(), context, point );
}

/**
*	@brief	Resolve a face query on owner-pinned copied geometry.
*	@note	The explicit overload carries immutable input and exclusive context.
**/
int32_t Nav_FindClosestPolyGlobal( const Vector3DP &point ) {
	nav_query_snapshot_scope_t lease;
	const auto *snapshot = lease.Get();
	// An unavailable publication cannot authorize lookup in a partial live mesh.
	if ( snapshot == nullptr ) {
		return -1;
	}
	svg_nav_route_context_t context;
	return Nav_FindClosestPolyGlobal( snapshot->RouteView(), context, point );
}

/**
*	@brief	Resolve a face query on owner-pinned copied geometry.
*	@note	The explicit overload carries immutable input and exclusive context.
**/
bool Nav_PointInsideFace2D( const Vector3DP &point, const nav_face_t &face ) {
	nav_query_snapshot_scope_t lease;
	const auto *snapshot = lease.Get();
	// An unavailable publication cannot authorize lookup in a partial live mesh.
	if ( snapshot == nullptr ) {
		return false;
	}
	svg_nav_route_context_t context;
	return Nav_PointInsideFace2D( snapshot->RouteView(), context, point, face );
}

/**
*	@brief	Resolve a face query on owner-pinned copied geometry.
*	@note	The explicit overload carries immutable input and exclusive context.
**/
int32_t Nav_FindLeafNode( const Vector3DP &point ) {
	nav_query_snapshot_scope_t lease;
	const auto *snapshot = lease.Get();
	// An unavailable publication cannot authorize lookup in a partial live mesh.
	if ( snapshot == nullptr ) {
		return -1;
	}
	svg_nav_route_context_t context;
	return Nav_FindLeafNode( snapshot->RouteView(), context, point );
}

/**
*	@brief	Owner-only geometry compatibility wrapper uses one immutable snapshot lease.
**/
int32_t Nav_FindReachableFaceInLeaf( const Vector3DP &point, const int32_t targetFace, const double agentRadius ) {
	nav_query_snapshot_scope_t lease;
	const auto *snapshot = lease.Get();
	// Retired or incomplete geometry cannot yield a valid portal or face.
	if ( snapshot == nullptr ) {
		return -1;
	}
	svg_nav_route_context_t context;
	return Nav_FindReachableFaceInLeaf( snapshot->RouteView(), context, point, targetFace, agentRadius );
}

/**
*	@brief	Owner-only geometry compatibility wrapper uses one immutable snapshot lease.
**/
int32_t Nav_GetFaceComponent( const int32_t faceIdx ) {
	nav_query_snapshot_scope_t lease;
	const auto *snapshot = lease.Get();
	// Retired or incomplete geometry cannot yield a valid portal or face.
	if ( snapshot == nullptr ) {
		return -1;
	}
	svg_nav_route_context_t context;
	return Nav_GetFaceComponent( snapshot->RouteView(), context, faceIdx );
}

/**
*	@brief	Owner-only geometry compatibility wrapper uses one immutable snapshot lease.
**/
static bool Nav_ClipFaceLineInterval2D( const nav_face_t &face, const Vector3DP &lineOrigin, const Vector3DP &lineDirection, const double lineLength, const int32_t excludedFaceIdx, const double minimumClearance, double *outMinT, double *outMaxT ) {
	nav_query_snapshot_scope_t lease;
	const auto *snapshot = lease.Get();
	// Retired or incomplete geometry cannot yield a valid portal or face.
	if ( snapshot == nullptr ) {
		return false;
	}
	svg_nav_route_context_t context;
	return Nav_ClipFaceLineInterval2D( snapshot->RouteView(), context, face, lineOrigin, lineDirection, lineLength, excludedFaceIdx, minimumClearance, outMinT, outMaxT );
}

/**
*	@brief	Owner-only geometry compatibility wrapper uses one immutable snapshot lease.
**/
bool Nav_GetPortalEndpoints( int32_t faceA, int32_t faceB, Vector3DP *outV0, Vector3DP *outV1 ) {
	nav_query_snapshot_scope_t lease;
	const auto *snapshot = lease.Get();
	// Retired or incomplete geometry cannot yield a valid portal or face.
	if ( snapshot == nullptr ) {
		return false;
	}
	svg_nav_route_context_t context;
	return Nav_GetPortalEndpoints( snapshot->RouteView(), context, faceA, faceB, outV0, outV1 );
}

/**
*	@brief	Owner wrapper prepares isolated geometry-query state and pins copied classifications.
*	@note	Pure overloads never publish diagnostics or call engine performance imports.
**/
nav_segment_clearance_t Nav_TestSegmentClearance( const Vector3DP &p0, const Vector3DP &p1, const double clearance ) {
	nav_query_snapshot_scope_t lease;
	const auto *snapshot = lease.Get();
	// No coherent publication means no geometric authorization.
	if ( snapshot == nullptr ) {
		nav_segment_clearance_t unavailable; unavailable.reason = nav_clearance_reason_t::cache_unavailable; return unavailable;
	}
	// Allocate query-local visitation only before solving, on the owner.
	svg_nav_route_context_t context;
	// The strict linear scan does not use visitation tokens; avoid a whole-mesh allocation per movement check.
	const auto result = Nav_TestSegmentClearance( snapshot->RouteView(), context, p0, p1, clearance );
	return result;
}

/**
*	@brief	Owner wrapper prepares isolated geometry-query state and pins copied classifications.
*	@note	Pure overloads never publish diagnostics or call engine performance imports.
**/
bool Nav_HasGeometricLineOfSight2D( const Vector3DP &p0, const Vector3DP &p1, const double clearanceMargin ) {
	nav_query_snapshot_scope_t lease;
	const auto *snapshot = lease.Get();
	// No coherent publication means no geometric authorization.
	if ( snapshot == nullptr ) {
		return false;
	}
	// Allocate query-local visitation only before solving, on the owner.
	svg_nav_route_context_t context;
	context.edge_tokens.resize( snapshot->RouteView().solid_edges.size() );
	const auto result = Nav_HasGeometricLineOfSight2D( snapshot->RouteView(), context, p0, p1, clearanceMargin );
	return result;
}

/**
*	@brief	Owner wrapper prepares isolated geometry-query state and pins copied classifications.
*	@note	Pure overloads never publish diagnostics or call engine performance imports.
**/
void Nav_RaycastRoomBoundary2D( const Vector3DP &anchor, const double maxRadius, const int32_t numRays, double *outDoorYaw, bool *outHasDoor, double *outMinWallDist, double *outAvgWallDist, nav_zone_type_t *outZoneType, const double agentRadius ) {
	nav_query_snapshot_scope_t lease;
	const auto *snapshot = lease.Get();
	// No coherent publication means no geometric authorization.
	if ( snapshot == nullptr ) {
		return;
	}
	// Allocate query-local visitation only before solving, on the owner.
	svg_nav_route_context_t context;
	context.edge_tokens.resize( snapshot->RouteView().solid_edges.size() );
	Nav_RaycastRoomBoundary2D( snapshot->RouteView(), context, anchor, maxRadius, numRays, outDoorYaw, outHasDoor, outMinWallDist, outAvgWallDist, outZoneType, agentRadius );
}

/**
*	@brief	Owner wrapper prepares isolated geometry-query state and pins copied classifications.
*	@note	Pure overloads never publish diagnostics or call engine performance imports.
**/
bool Nav_RaycastHalfEdge2D( const Vector3DP &start, const Vector3DP &dirNorm, const double maxDistance, nav_raycast_result_t *outResult ) {
	nav_query_snapshot_scope_t lease;
	const auto *snapshot = lease.Get();
	// No coherent publication means no geometric authorization.
	if ( snapshot == nullptr ) {
		return false;
	}
	// Allocate query-local visitation only before solving, on the owner.
	svg_nav_route_context_t context;
	context.edge_tokens.resize( snapshot->RouteView().solid_edges.size() );
	const auto result = Nav_RaycastHalfEdge2D( snapshot->RouteView(), context, start, dirNorm, maxDistance, outResult );
	return result;
}

/**
*	@brief	Owner wrapper prepares isolated geometry-query state and pins copied classifications.
*	@note	Pure overloads never publish diagnostics or call engine performance imports.
**/
bool Nav_CircleCastHalfEdge2D( const Vector3DP &start, const Vector3DP &dirNorm, const double maxDistance, const double agentRadius, nav_raycast_result_t *outResult ) {
	nav_query_snapshot_scope_t lease;
	const auto *snapshot = lease.Get();
	// No coherent publication means no geometric authorization.
	if ( snapshot == nullptr ) {
		return false;
	}
	// Allocate query-local visitation only before solving, on the owner.
	svg_nav_route_context_t context;
	context.edge_tokens.resize( snapshot->RouteView().solid_edges.size() );
	const auto result = Nav_CircleCastHalfEdge2D( snapshot->RouteView(), context, start, dirNorm, maxDistance, agentRadius, outResult );
	return result;
}

/**
*	@brief	Owner bridge preserves one pinned geometry revision through this refinement operation.
**/
static void Nav_RepairUnsafeFlatLinks( svg_nav_route_context_t &context, std::vector<Vector3DP> &points, std::vector<bool> &forced, const double radius ) {
	nav_query_snapshot_scope_t lease;
	const auto *snapshot = lease.Get();
	// Refuse unpublished geometry; native validation must decide whether the resulting route is usable.
	if ( snapshot == nullptr ) {
		return;
	}
	// Context-local visitation cannot modify copied obstacle caches or another query.
	context.edge_tokens.resize( snapshot->RouteView().solid_edges.size() );
	context.corner_tokens.resize( snapshot->RouteView().corners.size() );
	Nav_RepairUnsafeFlatLinks( snapshot->RouteView(), context, points, forced, radius );
}

/**
*	@brief	Owner bridge preserves one pinned geometry revision through this refinement operation.
**/
static void Nav_PruneUnsafeFlatPortals( std::vector<Vector3DP> &waypoints, std::vector<bool> *forced, const double radius ) {
	nav_query_snapshot_scope_t lease;
	const auto *snapshot = lease.Get();
	// Refuse unpublished geometry; native validation must decide whether the resulting route is usable.
	if ( snapshot == nullptr ) {
		return;
	}
	svg_nav_route_context_t context;
	// Context-local visitation cannot modify copied obstacle caches or another query.
	context.edge_tokens.resize( snapshot->RouteView().solid_edges.size() );
	context.corner_tokens.resize( snapshot->RouteView().corners.size() );
	Nav_PruneUnsafeFlatPortals( snapshot->RouteView(), context, waypoints, forced, radius );
}

/**
*	@brief	Owner bridge preserves one pinned geometry revision through this refinement operation.
**/
static bool Nav_ProjectOptionalFlatPortal( const Vector3DP &previous, Vector3DP &current, const Vector3DP &next, const double radius ) {
	nav_query_snapshot_scope_t lease;
	const auto *snapshot = lease.Get();
	// Refuse unpublished geometry; native validation must decide whether the resulting route is usable.
	if ( snapshot == nullptr ) {
		return false;
	}
	svg_nav_route_context_t context;
	// Context-local visitation cannot modify copied obstacle caches or another query.
	context.edge_tokens.resize( snapshot->RouteView().solid_edges.size() );
	context.corner_tokens.resize( snapshot->RouteView().corners.size() );
	const bool ok = Nav_ProjectOptionalFlatPortal( snapshot->RouteView(), context, previous, current, next, radius );
	return ok;
}

/**
*	@brief	Owner bridge preserves one pinned geometry revision through this refinement operation.
**/
static bool Nav_IsContinuousFlatSpan( const Vector3DP &start, const Vector3DP &end ) {
	nav_query_snapshot_scope_t lease;
	const auto *snapshot = lease.Get();
	// Refuse unpublished geometry; native validation must decide whether the resulting route is usable.
	if ( snapshot == nullptr ) {
		return false;
	}
	svg_nav_route_context_t context;
	// Context-local visitation cannot modify copied obstacle caches or another query.
	context.edge_tokens.resize( snapshot->RouteView().solid_edges.size() );
	context.corner_tokens.resize( snapshot->RouteView().corners.size() );
	const bool ok = Nav_IsContinuousFlatSpan( snapshot->RouteView(), context, start, end );
	return ok;
}

/**
*	@brief	Owner bridge preserves one pinned geometry revision through this refinement operation.
**/
static void Nav_EnforceConvexCornerWaypoints( svg_nav_route_context_t &context, const std::vector<int32_t> &path, const double agentRadius, std::vector<Vector3DP> &waypoints, std::vector<bool> *forcedWaypoints, const Vector3 &mins, const Vector3 &maxs, const int32_t shape, const nav_path_policy_t &policy, svg_base_edict_t *passEntity ) {
	nav_query_snapshot_scope_t lease;
	const auto *snapshot = lease.Get();
	// Refuse unpublished geometry; native validation must decide whether the resulting route is usable.
	if ( snapshot == nullptr ) {
		return;
	}
	// Context-local visitation cannot modify copied obstacle caches or another query.
	context.edge_tokens.resize( snapshot->RouteView().solid_edges.size() );
	context.corner_tokens.resize( snapshot->RouteView().corners.size() );
	Nav_EnforceConvexCornerWaypoints( snapshot->RouteView(), context, path, agentRadius, waypoints, forcedWaypoints, mins, maxs, shape, policy, passEntity );
}

/**
*	@brief	Owner bridge preserves one pinned geometry revision through this refinement operation.
**/
static bool Nav_ClipPortalForAgentClearance( const int32_t faceAIdx, const int32_t faceBIdx, const Vector3DP &portalLeft, const Vector3DP &portalRight, const double wallClearance, const double cornerClearance, Vector3DP *outLeft, Vector3DP *outRight, bool *outIsNarrowPortal ) {
	nav_query_snapshot_scope_t lease;
	const auto *snapshot = lease.Get();
	// Refuse unpublished geometry; native validation must decide whether the resulting route is usable.
	if ( snapshot == nullptr ) {
		return false;
	}
	svg_nav_route_context_t context;
	// Context-local visitation cannot modify copied obstacle caches or another query.
	context.edge_tokens.resize( snapshot->RouteView().solid_edges.size() );
	context.corner_tokens.resize( snapshot->RouteView().corners.size() );
	const bool ok = Nav_ClipPortalForAgentClearance( snapshot->RouteView(), context, faceAIdx, faceBIdx, portalLeft, portalRight, wallClearance, cornerClearance, outLeft, outRight, outIsNarrowPortal );
	return ok;
}

/**
*	@brief	Owner bridge pins exact copied route geometry through semantic/support validation.
*	@note	Physical traces are confined to Nav_ValidateHullRouteNativeMainThread.
**/
static bool Nav_RepairRefinementRoute( svg_nav_route_context_t &context, const std::vector<Vector3DP> &original, const std::vector<bool> &originalForced,
	const svg_vector_t<nav_waypoint_anchor_t, TAG_SVGAME_NAVMESH> &portalAnchors,
	std::vector<Vector3DP> &output, std::vector<bool> &outputForced, const Vector3 &mins, const Vector3 &maxs,
	const int32_t shape, const nav_path_policy_t &policy, svg_base_edict_t *passEntity ) {
	nav_query_snapshot_scope_t lease;
	const auto *snapshot = lease.Get();
	// Incomplete publication cannot prove a supported semantic route.
	if ( snapshot == nullptr ) {
		return false;
	}
	context.edge_tokens.resize( snapshot->RouteView().solid_edges.size() );
	context.corner_tokens.resize( snapshot->RouteView().corners.size() );
	return Nav_RepairRefinementRoute( snapshot->RouteView(), context, original, originalForced, portalAnchors, output, outputForced, mins, maxs, shape, policy, passEntity );
}

/**
*	@brief	Owner bridge pins exact copied route geometry through semantic/support validation.
*	@note	Physical traces are confined to Nav_ValidateHullRouteNativeMainThread.
**/
nav_route_validation_t Nav_ValidateHullRoute( const std::vector<Vector3DP> &points,
	const Vector3 &mins, const Vector3 &maxs, const int32_t shape, const nav_path_policy_t &policy, svg_base_edict_t *passEntity, const bool requireExactGoal, const uint32_t sampleBudget ) {
	nav_query_snapshot_scope_t lease;
	const auto *snapshot = lease.Get();
	// Incomplete publication cannot prove a supported semantic route.
	if ( snapshot == nullptr ) {
		nav_route_validation_t failed; failed.reason = nav_route_reason_t::semantic_failure; return failed;
	}
	svg_nav_route_context_t context;
	context.edge_tokens.resize( snapshot->RouteView().solid_edges.size() );
	context.corner_tokens.resize( snapshot->RouteView().corners.size() );
	return Nav_ValidateHullRouteNativeMainThread( snapshot->RouteView(), context, points, mins, maxs, shape, policy, passEntity, requireExactGoal, sampleBudget );
}

/**
*	@brief	Owner bridge pins exact copied route geometry through semantic/support validation.
*	@note	Physical traces are confined to Nav_ValidateHullRouteNativeMainThread.
**/
nav_goal_support_t Nav_ResolveHullGoalSupport( const Vector3DP &feet, const double radius, const int32_t shape ) {
	nav_query_snapshot_scope_t lease;
	const auto *snapshot = lease.Get();
	// Incomplete publication cannot prove a supported semantic route.
	if ( snapshot == nullptr ) {
		return {};
	}
	svg_nav_route_context_t context;
	context.edge_tokens.resize( snapshot->RouteView().solid_edges.size() );
	context.corner_tokens.resize( snapshot->RouteView().corners.size() );
	return Nav_ResolveHullGoalSupport( snapshot->RouteView(), context, feet, radius, shape );
}

/**
*	@brief	Owner bridge pins exact copied route geometry through semantic/support validation.
*	@note	Physical traces are confined to Nav_ValidateHullRouteNativeMainThread.
**/
static bool Nav_BuildWaypointRecords( svg_nav_route_context_t &context, const std::vector<Vector3DP> &points, std::vector<bool> *forced,
	const double radius, svg_vector_t<svg_nav_waypoint_t, TAG_SVGAME_NAVMESH> &output ) {
	nav_query_snapshot_scope_t lease;
	const auto *snapshot = lease.Get();
	// Incomplete publication cannot prove a supported semantic route.
	if ( snapshot == nullptr ) {
		return false;
	}
	context.edge_tokens.resize( snapshot->RouteView().solid_edges.size() );
	context.corner_tokens.resize( snapshot->RouteView().corners.size() );
	return Nav_BuildWaypointRecords( snapshot->RouteView(), context, points, forced, radius, output );
}

/**
*	@brief	Build the legacy obstacle classification once for an exclusive generation worker.
*	@return	A temporary view into generation-owned arrays and cache storage.
*	@note	The generation completion barrier must end all live readers before clearing these arrays.
**/
nav_route_mesh_view_t Nav_MakeGenerationRouteView() {
	/**
	*	Keep the preexisting generation-time obstacle classification while native
	*	queries transition to immutable published snapshots.
	**/
	Nav_EnsureObstacleCornersTable();
	nav_route_mesh_view_t view;
	view.vertices = std::span<const Vector3DP>( g_nav_vertices.data(), g_nav_vertices.size() );
	view.halfedges = std::span<const nav_halfedge_t>( g_nav_halfedges.data(), g_nav_halfedges.size() );
	view.faces = std::span<const nav_face_t>( g_nav_faces.data(), g_nav_faces.size() );
	view.nodes = std::span<const nav_kdtree_node_t>( g_nav_nodes.data(), g_nav_nodes.size() );
	view.corners = std::span<const nav_cached_corner_t>( s_nav_cached_corners.data(), s_nav_cached_corners.size() );
	view.solid_edges = std::span<const std::pair<Vector3DP, Vector3DP>>( s_nav_cached_solid_edges.data(), s_nav_cached_solid_edges.size() );
	view.edge_blocks_xy = std::span<const uint8_t>( s_nav_cached_edge_blocks_xy.data(), s_nav_cached_edge_blocks_xy.size() );
	view.grid_edges = &s_nav_spatial_grid_edges;
	view.grid_corners = &s_nav_spatial_grid_corners;
	return view;
}

/**
*	@brief	Copy and print the current local obstacle scene for explicit failed-route diagnosis.
*	@param	monster	Current live actor whose unresolved waypoint and local obstacles are inspected.
*	@note	This diagnostic makes no actor, order, policy or movement mutation.
**/
void Nav_DiagnoseActorDetourMainThread( const svg_monster_base_t &monster ) {
	/**
	*	Require a remaining mandatory point and one stable copied publication.
	**/
	if ( monster.stringPathPos >= monster.stringPulledPath.size() ) {
		return;
	}
	nav_query_snapshot_scope_t lease;
	const auto *snapshot = lease.Get();
	if ( snapshot == nullptr ) {
		return;
	}
	const Vector3DP start = SVG_GetEntityFeetOriginDP( &monster );
	const Vector3DP goal = monster.stringPulledPath[ monster.stringPathPos ];
	const auto mesh = snapshot->RouteView();
	/**
	*	Print the same bounded broadphase inputs as production, including native radii and floor roles.
	**/
	std::array<svg_base_edict_t *, 17> nearby{};
	const Vector3 lower = monster.currentOrigin + Vector3{ -256.0f, -256.0f, monster.mins.z };
	const Vector3 upper = monster.currentOrigin + Vector3{ 256.0f, 256.0f, monster.maxs.z };
	const int32_t count = gi.BoxEdicts( &lower, &upper, nearby.data(), static_cast<int32_t>( nearby.size() ), AREA_SOLID );
	gi.dprintf( "[nav detour scene] ent=%d start=(%.9f %.9f %.9f) goal=(%.9f %.9f %.9f) radius=%.9f handles=%d\n",
		monster.s.number, start.x, start.y, start.z, goal.x, goal.y, goal.z, monster.maxs.x, count );
	for ( int32_t index = 0; index < count; ++index ) {
		const auto *peer = nearby[ index ];
		// Ignore non-actor broadphase entries just as the proposal builder does.
		if ( peer == nullptr || peer == &monster || !SVG_Entity_IsActive( peer ) ||
			 !peer->GetTypeInfo()->IsSubClassType<svg_monster_base_t>() ) {
			continue;
		}
		const Vector3DP feet = SVG_GetEntityFeetOriginDP( peer );
		gi.dprintf( "[nav detour disk] ent=%d center=(%.9f %.9f %.9f) radius=%.9f parked=%d\n", peer->s.number,
			feet.x, feet.y, feet.z, monster.maxs.x + peer->maxs.x + NAV_STANDOFF_MIN_HULL_CLEARANCE_MARGIN, peer->crowd.reachedGoal ? 1 : 0 );
	}
	/**
	*	Print relevant immutable strict wall segments, allowing independent geometry replay.
	**/
	for ( uint32_t index = 0; index < mesh.solid_edges.size(); ++index ) {
		const auto &edge = mesh.solid_edges[ index ];
		// Bounding overlap keeps console diagnostics local without discarding crossing long edges.
		if ( mesh.edge_blocks_xy[ index ] == 0 || std::max( edge.first.x, edge.second.x ) < start.x - 256 ||
			 std::min( edge.first.x, edge.second.x ) > start.x + 256 || std::max( edge.first.y, edge.second.y ) < start.y - 256 ||
			 std::min( edge.first.y, edge.second.y ) > start.y + 256 ) {
			continue;
		}
		gi.dprintf( "[nav detour wall] a=(%.9f %.9f %.9f) b=(%.9f %.9f %.9f)\n", edge.first.x, edge.first.y,
			edge.first.z, edge.second.x, edge.second.y, edge.second.z );
	}
	/**
	*	Probe raw direct and opposite actor tangents on explicit diagnosis only; these never authorize a command.
	*	Separate geometric and native witnesses to distinguish a parked actor from the wall beside it.
	**/
	auto *self = g_edict_pool.EdictForNumber( monster.s.number );
	Vector3DP direct = goal - start;
	direct.z = 0.0;
	const double length = QM_Vector3LengthDP( direct );
	// Only a current pool object and meaningful horizontal continuation can define these bounded probes.
	if ( self != &monster || !SVG_Entity_IsActive( self ) || length <= 0.001 ) {
		return;
	}
	direct = direct * ( 1.0 / length );
	const double distance = std::min( length, 5.5 );
	const auto probe = [&]( const char *label, const Vector3DP &direction ) {
		const Vector3DP end = start + direction * distance;
		// Both proofs retain the actual native hull and supported feet coordinates; reservation projection is deliberately not emulated.
		const auto geometric = Nav_TestSegmentClearance( start, end, monster.maxs.x );
		const auto native = SVG_Nav_TestMovementCandidate( self, start, end, monster.pathNavigationState.policy, false );
		gi.dprintf( "[nav detour probe] ent=%" PRId32 " spawn=%" PRId32 " incarnation=%" PRIu64 " label=%s raw=1 authorization=0 end=(%.9f %.9f %.9f) geometric=%" PRId32 " clearance=%.9f native=%" PRId32 " actor=%" PRId32 " traces=%" PRIu32 "\n",
			monster.s.number, monster.spawn_count, monster.navigationIncarnation, label, end.x, end.y, end.z,
			static_cast<int32_t>( geometric.reason ), geometric.minimum_clearance, static_cast<int32_t>( native.reason ), native.blocking_actor, native.trace_count );
		return native;
	};
	const auto forward = probe( "direct", direct );
	// A wall-only rejection cannot supply an actor tangent; only the actual current full-hull blocker can.
	if ( forward.blocking_actor < 1 || forward.blocking_actor >= globals.edictPool->num_edicts ) {
		return;
	}
	const auto *peer = g_edict_pool.EdictForNumber( forward.blocking_actor );
	// A removed or replaced non-actor cannot define the same local avoidance geometry.
	if ( peer == nullptr || !SVG_Entity_IsActive( peer ) || !peer->GetTypeInfo()->IsSubClassType<svg_monster_base_t>() ) {
		return;
	}
	Vector3DP radial = start - SVG_GetEntityFeetOriginDP( peer );
	radial.z = 0.0;
	const double radialLength = QM_Vector3LengthDP( radial );
	// Coincident actors cannot define a safe tangent and remain an explicit diagnostic failure.
	if ( radialLength <= 0.001 ) {
		return;
	}
	radial = radial * ( 1.0 / radialLength );
	Vector3DP tangent{ -radial.y, radial.x, 0.0 };
	const double alignment = QM_Vector3DotProductDP( tangent, direct );
	// Match production's preferred orientation and parity tie break before probing the opposite side independently.
	if ( alignment < -0.000001 || ( std::fabs( alignment ) <= 0.000001 && ( monster.s.number & 1 ) != 0 ) ) {
		tangent = tangent * -1.0;
	}
	const Vector3DP preferred = tangent + radial * 0.25;
	const Vector3DP opposite = tangent * -1.0 + radial * 0.25;
	probe( "preferred_tangent", preferred * ( 1.0 / QM_Vector3LengthDP( preferred ) ) );
	probe( "opposite_tangent", opposite * ( 1.0 / QM_Vector3LengthDP( opposite ) ) );

}
