#include "svgame/nav/nav_core.h"
#include "svgame/nav/nav_containers.h"
#include "svgame/nav/nav_types.h"
#include "svgame/nav/nav_generate.h"
#include "svgame/nav/nav_path.h"
#include "svgame/nav/nav_debug.h"

#include "svgame/nav/nav_debug_draw.h"
#include "svgame/svg_utils.h"


/**
*
*
*
*	Debug constants and related:
*
*
*
**/
//! 1 == Triangle Fan -- 2 == Ear Clipping
#define DEBUG_NAV_DRAW_NGON_TRIANGLES_METHOD 1


/**
*	Debug Overlay Colors:
**/
//! The color of the KD-tree node overlay.
static const uint32_t KDTREE_COLOR = MakeColor( 95, 205, 228, 255 );

//! For when edges are too far apart to be considered a twin edge.
static const uint32_t EDGE_NO_TWIN_COLOR = MakeColor( 172, 50, 50, 255 );
//! For when edges are close enough to be considered a twin edge.
static const uint32_t EDGE_TWIN_COLOR = MakeColor( 223, 113, 38, 255 );

//! Entity edge overlay color when the edge is enabled (e.g., a door).
static const uint32_t EDGE_ENABLED_COLOR = MakeColor( 106, 190, 48, 255 );
//! Entity edge overlay color when the edge is disabled (e.g., a door).
static const uint32_t EDGE_DISABLED_COLOR = MakeColor( 63, 63, 116, 255 );

//! The color of the navmesh polygon overlay.
static const uint32_t TRIS_COLOR = MakeColor( 238, 195, 154, 255 );

// <WID>: TODO: Rename these to be more descriptive and less generic, since they are used for debug overlays and not actual gameplay colors.
// Used for debug path.
//! For debug start/end point sphere.
static const uint32_t DEBUG_ROUTE_SPHERE_COLOR = MakeColor( 223, 113, 38, 255 );
//! Color used for string-pulled route segments and ordinary waypoints.
static const uint32_t DEBUG_FUNNEL_ROUTE_COLOR = MakeColor( 95, 205, 228, 255 );
//! Color used for mandatory stair approach and crossing waypoints.
static const uint32_t DEBUG_FORCED_WAYPOINT_COLOR = MakeColor( 106, 190, 48, 255 );


/**
*
*
*
*	Static cached data for nav (debug-) path testing.
*
*
*
**/
//! Cvar that toggles KD-tree node overlay rendering.
cvar_t *s_nav_debug_nodes = nullptr;
//! Cvar that toggles polygon edge overlay rendering.
cvar_t *s_nav_debug_polys = nullptr;
//! Cvar that toggles triangle edge debug rendering.
cvar_t *s_nav_debug_tris = nullptr;
//! Cvar that highlights the retained debug route endpoint KD leaves.
cvar_t *s_nav_debug_query_leaves = nullptr;
//! Cvar that sets the maximum radius for nav debug overlay rendering.
cvar_t *s_nav_debug_draw_radius = nullptr;

//! Cached goal A world position for nav debug path testing.
static Vector3 s_nav_dbg_goal_a_origin = {};
//! Cached goal B world position for nav debug path testing.
static Vector3 s_nav_dbg_goal_b_origin = {};
//! Cached goal A face id.
static int32_t s_nav_dbg_goal_a_face = -1;
//! Cached goal B face id.
static int32_t s_nav_dbg_goal_b_face = -1;
//! Whether goal A has been set.
static bool s_nav_dbg_has_goal_a = false;
//! Whether goal B has been set.
static bool s_nav_dbg_has_goal_b = false;
//! Cached test route between goal A and goal B.
static std::vector<int32_t> s_nav_dbg_test_path = {};
//! Cached string-pulled route rendered beside the raw A* face-center chain.
static std::vector<Vector3> s_nav_dbg_test_waypoints = {};
//! Flags parallel to s_nav_dbg_test_waypoints for mandatory stair constraints.
static std::vector<bool> s_nav_dbg_test_forced_waypoints = {};
//! Whether cached test route is valid and should be rendered.
static bool s_nav_dbg_has_test_path = false;




/**
* @brief Register nav debug cvars for the node and polygon overlays.
**/
void Nav_DebugInit() {
	s_nav_debug_nodes = gi.cvar( "nav_debug_nodes", "0", 0 );
	s_nav_debug_polys = gi.cvar( "nav_debug_polys", "1", 0 );
	s_nav_debug_tris = gi.cvar( "nav_debug_tris", "0", 0 );
	s_nav_debug_query_leaves = gi.cvar( "nav_debug_query_leaves", "0", 0 );
	s_nav_debug_draw_radius = gi.cvar( "nav_debug_draw_radius", std::to_string( CM_MAX_WORLD_HALF_SIZE ).c_str(), 0 );
}

/**
*	@brief	Resolve a player entity to use for nav debug command sampling.
*	@return	Player edict or nullptr.
**/
static svg_base_edict_t *Nav_DebugGetCommandPlayer() {
	for ( int32_t i = 1; i <= game.maxclients; i++ ) {
		svg_base_edict_t *player = g_edict_pool.EdictForNumber( i );
		if ( !player || !player->inUse || !player->client ) {
			continue;
		}
		return player;
	}
	return nullptr;
}

/**
*	@brief	Build a feet-origin sample from one actor origin.
*	@param	actor Actor used to derive the feet sample.
*	@return	Feet-origin world-space point.
**/
static Vector3 Nav_DebugBuildFeetOrigin( const svg_base_edict_t *actor ) {
	Vector3 feet = actor->currentOrigin;
	feet.z += actor->mins.z;
	return feet;
}

/**
*	@brief	Resolve a stable local face for debug queries, with a robust global fallback.
*	@param	point World-space query point.
*	@return	Face id or -1 when no nav face exists.
**/
static int32_t Nav_DebugFindClosestFaceInLeaf( const Vector3DP &point ) {
const int32_t leafFace = Nav_FindPolyInLeaf( static_cast<Vector3>( point ) );
	if ( leafFace >= 0 && static_cast< size_t >( leafFace ) < g_nav_faces.size() ) {
		const nav_face_t &face = g_nav_faces[ leafFace ];
			if ( Nav_PointInsideFace2D( static_cast<Vector3>( point ), face ) ) {
			const Vector3DP v0 = g_nav_vertices[ g_nav_halfedges[ face.first_edge_idx ].vertex_idx ];
			const double planeDist = ( QM_Vector3DotProductDP( v0, face.normal ) );
			const double verticalDist = std::abs( ( QM_Vector3DotProductDP( point, face.normal ) ) - planeDist );
			if ( verticalDist <= 64.0 ) {
				return leafFace;
			}
		}
	}
	return Nav_FindClosestPolyGlobal( static_cast<Vector3>( point ) );
}

/**
*	@brief	Draw cached goal markers and cached test route.
**/
static void Nav_DebugDrawTestRoute() {
	if ( !s_nav_debug_polys || s_nav_debug_polys->value == 0 ) {
		return;
	}

	if ( s_nav_dbg_has_goal_a ) {
		SVG_Nav_DebugDraw_AddSphere( s_nav_dbg_goal_a_origin, 6.0f, U32_GREEN, SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE );
	}
	if ( s_nav_dbg_has_goal_b ) {
		SVG_Nav_DebugDraw_AddSphere( s_nav_dbg_goal_b_origin, 6.0f, U32_RED, SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE );
	}

	if ( !s_nav_dbg_has_test_path || s_nav_dbg_test_path.empty() ) {
		return;
	}

	Vector3 prev = s_nav_dbg_goal_a_origin;
	if ( s_nav_dbg_goal_a_face >= 0 && static_cast< size_t >( s_nav_dbg_goal_a_face ) < g_nav_faces.size() ) {
		prev = QM_Vector3FromDP( g_nav_faces[ s_nav_dbg_goal_a_face ].center );
	}

	for ( size_t i = 0; i < s_nav_dbg_test_path.size(); i++ ) {
		const int32_t faceIdx = s_nav_dbg_test_path[ i ];
		if ( faceIdx < 0 || static_cast< size_t >( faceIdx ) >= g_nav_faces.size() ) {
			continue;
		}
		const Vector3 point = QM_Vector3FromDP( g_nav_faces[ faceIdx ].center );
		SVG_Nav_DebugDraw_AddSphere( point, 4.0f, DEBUG_ROUTE_SPHERE_COLOR, SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE );
		SVG_Nav_DebugDraw_AddLine( prev, point, U32_MAGENTA, SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE );
		prev = point;
	}

	Vector3 endPoint = s_nav_dbg_goal_b_origin;
	if ( s_nav_dbg_goal_b_face >= 0 && static_cast< size_t >( s_nav_dbg_goal_b_face ) < g_nav_faces.size() ) {
		endPoint = QM_Vector3FromDP( g_nav_faces[ s_nav_dbg_goal_b_face ].center );
	}
	SVG_Nav_DebugDraw_AddLine( prev, endPoint, U32_MAGENTA, SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE );

	/**
	*	Render the actual funnel result separately from the A* face-center chain.
	*	This makes the mandatory lower-tread approach and portal-crossing points
	*	visible instead of implying that face centers are movement targets.
	**/
	if ( s_nav_dbg_test_waypoints.size() >= 2 ) {
		Vector3 funnelPrevious = s_nav_dbg_test_waypoints[ 0 ];
		for ( size_t waypointIndex = 0; waypointIndex < s_nav_dbg_test_waypoints.size(); waypointIndex++ ) {
			const Vector3 &waypoint = s_nav_dbg_test_waypoints[ waypointIndex ];
			const bool forced = waypointIndex < s_nav_dbg_test_forced_waypoints.size() && s_nav_dbg_test_forced_waypoints[ waypointIndex ];
			const uint32_t waypointColor = forced ? DEBUG_FORCED_WAYPOINT_COLOR : DEBUG_FUNNEL_ROUTE_COLOR;
			SVG_Nav_DebugDraw_AddSphere( waypoint, forced ? 5.0f : 3.0f, waypointColor, SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE );
			if ( waypointIndex > 0 ) {
				SVG_Nav_DebugDraw_AddLine( funnelPrevious, waypoint, DEBUG_FUNNEL_ROUTE_COLOR, SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE );
			}
			funnelPrevious = waypoint;
		}
	}
}

void Nav_DebugSetGoalACommand( void ) {
	svg_base_edict_t *player = Nav_DebugGetCommandPlayer();
	if ( !player ) {
		gi.dprintf( "nav_dbg_goal_a: player not available\n" );
		return;
	}

	const Vector3 playerCenter = player->currentOrigin;
	s_nav_dbg_goal_a_origin = Nav_DebugBuildFeetOrigin( player );
	s_nav_dbg_goal_a_face = Nav_DebugFindClosestFaceInLeaf( Vector3DP( s_nav_dbg_goal_a_origin ) );
	s_nav_dbg_has_goal_a = true;
	s_nav_dbg_has_test_path = false;
	s_nav_dbg_test_path.clear();
	s_nav_dbg_test_waypoints.clear();
	s_nav_dbg_test_forced_waypoints.clear();
	gi.dprintf( "nav_dbg_goal_a: center=(%.2f %.2f %.2f) feet=(%.2f %.2f %.2f) face=%d\n",
		playerCenter.x, playerCenter.y, playerCenter.z,
		s_nav_dbg_goal_a_origin.x, s_nav_dbg_goal_a_origin.y, s_nav_dbg_goal_a_origin.z, s_nav_dbg_goal_a_face );
}

void Nav_DebugSetGoalBCommand( void ) {
	svg_base_edict_t *player = Nav_DebugGetCommandPlayer();
	if ( !player ) {
		gi.dprintf( "nav_dbg_goal_b: player not available\n" );
		return;
	}

	const Vector3 playerCenter = player->currentOrigin;
	s_nav_dbg_goal_b_origin = Nav_DebugBuildFeetOrigin( player );
	s_nav_dbg_goal_b_face = Nav_DebugFindClosestFaceInLeaf( Vector3DP( s_nav_dbg_goal_b_origin ) );
	s_nav_dbg_has_goal_b = true;
	s_nav_dbg_has_test_path = false;
	s_nav_dbg_test_path.clear();
	s_nav_dbg_test_waypoints.clear();
	s_nav_dbg_test_forced_waypoints.clear();
	gi.dprintf( "nav_dbg_goal_b: center=(%.2f %.2f %.2f) feet=(%.2f %.2f %.2f) face=%d\n",
		playerCenter.x, playerCenter.y, playerCenter.z,
		s_nav_dbg_goal_b_origin.x, s_nav_dbg_goal_b_origin.y, s_nav_dbg_goal_b_origin.z, s_nav_dbg_goal_b_face );
}

/**
*	@brief	Print one bounded diagnostic snapshot for the currently cached debug route.
*	@param	policy	Path policy used to create the cached route.
*	@note	This runs only from `nav_dbg_test`; it retains deterministic goal samples
*			without adding movement-frame or per-edge generation logging.
**/
static void Nav_DebugLogTestPathDiagnostics( const nav_path_policy_t &policy ) {
	/**
	*	Report the frozen endpoint localization and active policy before walking the route.
	**/
	const int32_t start_leaf = Nav_FindLeafNode( s_nav_dbg_goal_a_origin );
	const int32_t goal_leaf = Nav_FindLeafNode( s_nav_dbg_goal_b_origin );
	gi.dprintf( "nav_dbg_test_diag: start=(%.2f %.2f %.2f) face=%d leaf=%d goal=(%.2f %.2f %.2f) face=%d leaf=%d radius=%.2f step=%.2f drop=%.2f\n",
		s_nav_dbg_goal_a_origin.x, s_nav_dbg_goal_a_origin.y, s_nav_dbg_goal_a_origin.z,
		s_nav_dbg_goal_a_face, start_leaf,
		s_nav_dbg_goal_b_origin.x, s_nav_dbg_goal_b_origin.y, s_nav_dbg_goal_b_origin.z,
		s_nav_dbg_goal_b_face, goal_leaf,
		policy.agent_radius, policy.max_step_height, policy.max_drop_height );

	/**
	*	Stop after endpoint data when A* did not produce a corridor to inspect.
	**/
	if ( !s_nav_dbg_has_test_path || s_nav_dbg_test_path.empty() ) {
		gi.dprintf( "nav_dbg_test_diag: no route was produced\n" );
		return;
	}

	/**
	*	Report each stable face index/ID and each physical portal in the cached corridor.
	**/
	for ( size_t path_index = 0; path_index < s_nav_dbg_test_path.size(); path_index++ ) {
		const int32_t face_index = s_nav_dbg_test_path[ path_index ];
		if ( face_index < 0 || face_index >= static_cast< int32_t >( g_nav_faces.size() ) ) {
			gi.dprintf( "nav_dbg_test_diag: invalid cached face index=%d at path_index=%d\n", face_index, static_cast< int32_t >( path_index ) );
			return;
		}

		const nav_face_t &face = g_nav_faces[ face_index ];
		gi.dprintf( "nav_dbg_test_diag: path[%d] face=%d id=%d center=(%.2f %.2f %.2f) edges=%d\n",
			static_cast< int32_t >( path_index ), face_index, face.face_id,
			face.center.x, face.center.y, face.center.z, face.num_edges );
		if ( path_index + 1 >= s_nav_dbg_test_path.size() ) {
			continue;
		}

		const int32_t next_face_index = s_nav_dbg_test_path[ path_index + 1 ];
		Vector3 portal_left = {};
		Vector3 portal_right = {};
		const bool has_portal = Nav_GetPortalEndpoints( face_index, next_face_index, &portal_left, &portal_right );
		int32_t portal_edge = -1;
		int32_t portal_twin = -1;
		for ( int32_t edge_offset = 0; edge_offset < face.num_edges; edge_offset++ ) {
			const int32_t edge_index = face.first_edge_idx + edge_offset;
			const nav_halfedge_t &edge = g_nav_halfedges[ edge_index ];
			if ( edge.twin_idx != -1 && g_nav_halfedges[ edge.twin_idx ].face_idx == next_face_index ) {
				portal_edge = edge_index;
				portal_twin = edge.twin_idx;
				break;
			}
		}

		if ( !has_portal || portal_edge == -1 ) {
			gi.dprintf( "nav_dbg_test_diag: transition=%d->%d has_portal=%d edge=%d\n", face_index, next_face_index, has_portal ? 1 : 0, portal_edge );
			continue;
		}

		const nav_halfedge_t &edge = g_nav_halfedges[ portal_edge ];
		const nav_halfedge_t &twin = g_nav_halfedges[ portal_twin ];
		const float delta_z = static_cast< float >( g_nav_faces[ next_face_index ].center.z - face.center.z );
		gi.dprintf( "nav_dbg_test_diag: transition=%d->%d edge=%d twin=%d flags=0x%08x/0x%08x entity=%d dz=%.2f portalL=(%.2f %.2f %.2f) portalR=(%.2f %.2f %.2f)\n",
			face_index, next_face_index, portal_edge, portal_twin,
			edge.flags, twin.flags, edge.edge_entity_id, delta_z,
			portal_left.x, portal_left.y, portal_left.z,
			portal_right.x, portal_right.y, portal_right.z );
	}

	/**
	*	Run the same funnel stage used by NPC movement and retain its finite waypoint output.
	**/
	s_nav_dbg_test_waypoints.clear();
	s_nav_dbg_test_forced_waypoints.clear();
	const bool string_pull_ok = Nav_StringPull( s_nav_dbg_test_path, s_nav_dbg_goal_a_origin, s_nav_dbg_goal_b_origin, policy.agent_radius, s_nav_dbg_test_waypoints, &s_nav_dbg_test_forced_waypoints );
	gi.dprintf( "nav_dbg_test_diag: funnel_success=%d waypoint_count=%d\n", string_pull_ok ? 1 : 0, static_cast< int32_t >( s_nav_dbg_test_waypoints.size() ) );
	for ( size_t waypoint_index = 0; waypoint_index < s_nav_dbg_test_waypoints.size(); waypoint_index++ ) {
		const Vector3 &waypoint = s_nav_dbg_test_waypoints[ waypoint_index ];
		const bool forced = waypoint_index < s_nav_dbg_test_forced_waypoints.size() && s_nav_dbg_test_forced_waypoints[ waypoint_index ];
		gi.dprintf( "nav_dbg_test_diag: waypoint[%d] forced=%d pos=(%.2f %.2f %.2f)\n", static_cast< int32_t >( waypoint_index ), forced ? 1 : 0, waypoint.x, waypoint.y, waypoint.z );
	}
}

void Nav_DebugTestPathCommand( void ) {
	if ( !s_nav_dbg_has_goal_a || !s_nav_dbg_has_goal_b ) {
		gi.dprintf( "nav_dbg_test: set both goals first (sv nav_dbg_goal_a / sv nav_dbg_goal_b)\n" );
		return;
	}
	if ( s_nav_dbg_goal_a_face < 0 || s_nav_dbg_goal_b_face < 0 ) {
		gi.dprintf( "nav_dbg_test: invalid goal face(s): A=%d B=%d\n", s_nav_dbg_goal_a_face, s_nav_dbg_goal_b_face );
		s_nav_dbg_has_test_path = false;
		s_nav_dbg_test_path.clear();
		return;
	}

	nav_path_policy_t policy = {};
	policy.max_step_height = NAV_MAX_STEP_SIZE;
	policy.max_drop_height = NAV_DROPOFF_ALLOWED_SIZE;
	policy.enable_max_drop_height_cap = true;
	policy.max_drop_height_cap = NAV_DROPOFF_MAX_SIZE;

	s_nav_dbg_test_path.clear();
	s_nav_dbg_test_waypoints.clear();
	s_nav_dbg_test_forced_waypoints.clear();
	s_nav_dbg_has_test_path = Nav_FindPath( s_nav_dbg_goal_a_face, s_nav_dbg_goal_b_face, s_nav_dbg_test_path, policy );
	gi.dprintf( "nav_dbg_test: A=%d B=%d success=%d nodes=%d\n",
		s_nav_dbg_goal_a_face, s_nav_dbg_goal_b_face, s_nav_dbg_has_test_path ? 1 : 0, static_cast< int32_t >( s_nav_dbg_test_path.size() ) );
	if ( !s_nav_dbg_has_test_path ) {
		// Print the exact one-shot rejection totals for this frozen A/B query.
		Nav_LogLastPathDiagnostics();
	}
	Nav_DebugLogTestPathDiagnostics( policy );
}

static void RecursiveDrawNodes( int32_t nodeIndex, const Vector3 &playerPos, float radius ) {
	if ( nodeIndex == -1 || nodeIndex >= g_nav_nodes.size() ) return;

	const nav_kdtree_node_t &node = g_nav_nodes[ nodeIndex ];

	// Simple AABB vs AABB check
	if ( node.mins.x > playerPos.x + radius || node.maxs.x < playerPos.x - radius ||
		node.mins.y > playerPos.y + radius || node.maxs.y < playerPos.y - radius ||
		node.mins.z > playerPos.z + radius || node.maxs.z < playerPos.z - radius ) {
		return;
	}

	if ( s_nav_debug_nodes && s_nav_debug_nodes->value != 0 ) {
		// Only draw leaf nodes to avoid overlapping AABB slivers from parent volumes
		if ( node.left_child == -1 && node.right_child == -1 ) {
			SVG_Nav_DebugDraw_AddAabb( static_cast<Vector3>( node.mins ), static_cast<Vector3>( node.maxs ), KDTREE_COLOR );
		}
	}

	// Draw polys by directly iterating over g_nav_polys later in SVG_Nav_DebugDraw.

	if ( node.left_child != -1 ) RecursiveDrawNodes( node.left_child, playerPos, radius );
	if ( node.right_child != -1 ) RecursiveDrawNodes( node.right_child, playerPos, radius );
}

/**
*	@brief	Highlight the fixed endpoint leaves used by the retained debug path test.
*	@note	This intentionally draws only command-captured leaves, making KD candidate
*			inspection independent from transient player movement and node-AABB clutter.
**/
static void Nav_DebugDrawQueryLeaves( void ) {
	/**
	*	Require an enabled overlay and two captured endpoint samples.
	**/
	if ( !s_nav_debug_query_leaves || s_nav_debug_query_leaves->value == 0 || !s_nav_dbg_has_goal_a || !s_nav_dbg_has_goal_b ) {
		return;
	}

	/**
	*	Resolve each retained endpoint independently because both can occupy one leaf.
	**/
	const int32_t endpoint_leaves[ 2 ] = {
		Nav_FindLeafNode( s_nav_dbg_goal_a_origin ),
		Nav_FindLeafNode( s_nav_dbg_goal_b_origin )
	};
	for ( int32_t endpoint_index = 0; endpoint_index < 2; endpoint_index++ ) {
		const int32_t leaf_index = endpoint_leaves[ endpoint_index ];
		if ( leaf_index < 0 || leaf_index >= static_cast< int32_t >( g_nav_nodes.size() ) ) {
			continue;
		}

		const nav_kdtree_node_t &leaf = g_nav_nodes[ leaf_index ];
		SVG_Nav_DebugDraw_AddAabb( static_cast<Vector3>( leaf.mins ), static_cast<Vector3>( leaf.maxs ), endpoint_index == 0 ? U32_GREEN : U32_RED );
	}
}

#ifdef DEBUG_NAV_DRAW_NGON_TRIANGLES_METHOD
#if DEBUG_NAV_DRAW_NGON_TRIANGLES_METHOD == 1
/**
* @brief Triangulate and draw a navmesh face using ear clipping.
* @param face The nav face to draw.
**/
static void Nav_DebugDraw_TriangleFanForFace( const nav_face_t &face ) {
	if ( face.num_edges < 3 ) {
		return;
	}

	// Keep nav edge overlays visible even when they overlap world geometry.
	sg_svc_debug_draw_style_flags_t styleFlags = SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE;

	// Triangulate the face as a fan from the first vertex (v0)
	const Vector3 v0 = static_cast<Vector3>( g_nav_vertices[ g_nav_halfedges[ face.first_edge_idx ].vertex_idx ] );
	for ( int32_t e = 1; e < face.num_edges - 1; e++ ) {
		const nav_halfedge_t &he = g_nav_halfedges[ face.first_edge_idx + e ];
		const Vector3 v1 = static_cast<Vector3>( g_nav_vertices[ he.vertex_idx ] );
		const Vector3 v2 = static_cast<Vector3>( g_nav_vertices[ g_nav_halfedges[ he.next_idx ].vertex_idx ] );
		SVG_Nav_DebugDraw_AddLine( v0, v1, TRIS_COLOR, styleFlags, 1, 1 );
		SVG_Nav_DebugDraw_AddLine( v1, v2, TRIS_COLOR, styleFlags, 1, 1 );
		SVG_Nav_DebugDraw_AddLine( v2, v0, TRIS_COLOR, styleFlags, 1, 1 );
	}
}
#else
/**
* @brief Triangulate and draw a navmesh face using ear clipping.
* @param face The nav face to draw.
**/
static void Nav_DebugDraw_EarClippingTrianglesForFace( const nav_face_t &face ) {
	if ( face.num_edges < 3 ) {
		return;
	}

	std::vector<Vector3> verts;
	verts.reserve( face.num_edges );
	for ( int32_t e = 0; e < face.num_edges; e++ ) {
		const nav_halfedge_t &he = g_nav_halfedges[ face.first_edge_idx + e ];
		verts.push_back( g_nav_vertices[ he.vertex_idx ] );
	}

	std::vector<int32_t> indices( verts.size() );
	for ( size_t i = 0; i < indices.size(); ++i ) {
		indices[ i ] = ( int32_t )i;
	}

	auto PointInTriangle = []( const Vector3 &p, const Vector3 &a, const Vector3 &b, const Vector3 &c ) {
		Vector3 v0 = c - a;
		Vector3 v1 = b - a;
		Vector3 v2 = p - a;

		float dot00 = QM_Vector3DotProduct( v0, v0 );
		float dot01 = QM_Vector3DotProduct( v0, v1 );
		float dot02 = QM_Vector3DotProduct( v0, v2 );
		float dot11 = QM_Vector3DotProduct( v1, v1 );
		float dot12 = QM_Vector3DotProduct( v1, v2 );

		float invDenom = 1.0f / ( dot00 * dot11 - dot01 * dot01 );
		float u = ( dot11 * dot02 - dot01 * dot12 ) * invDenom;
		float v = ( dot00 * dot12 - dot01 * dot02 ) * invDenom;

		// Add a small epsilon to avoid floating point precision issues on collinear points.
		return ( u >= -0.001f ) && ( v >= -0.001f ) && ( u + v <= 1.001f );
		};

	int32_t infiniteLoopGuard = face.num_edges * 2;
	while ( indices.size() > 3 && infiniteLoopGuard-- > 0 ) {
		bool earFound = false;
		for ( size_t i = 0; i < indices.size(); ++i ) {
			size_t prev = ( i == 0 ) ? indices.size() - 1 : i - 1;
			size_t next = ( i == indices.size() - 1 ) ? 0 : i + 1;

			Vector3 p_prev = verts[ indices[ prev ] ];
			Vector3 p_curr = verts[ indices[ i ] ];
			Vector3 p_next = verts[ indices[ next ] ];

			Vector3 edge1 = p_curr - p_prev;
			Vector3 edge2 = p_next - p_curr;
			Vector3 cross = QM_Vector3CrossProduct( edge1, edge2 );

			// Check if the angle is convex using the face normal
			// Note: Windings in this engine are clockwise, so cross product points opposite to normal for convex corners.
			if ( QM_Vector3DotProduct( cross, face.normal ) >= 0.0f ) {
				continue;
			}

			// Check if any other vertex is inside this triangle
			bool isEar = true;
			for ( size_t j = 0; j < indices.size(); ++j ) {
				if ( j == prev || j == i || j == next ) {
					continue;
				}
				if ( PointInTriangle( verts[ indices[ j ] ], p_prev, p_curr, p_next ) ) {
					isEar = false;
					break;
				}
			}

			if ( isEar ) {
				SVG_Nav_DebugDraw_AddLine( p_prev, p_curr, TRIS_COLOR, SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE, 2, 0 );
				SVG_Nav_DebugDraw_AddLine( p_curr, p_next, TRIS_COLOR, SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE, 2, 0 );
				SVG_Nav_DebugDraw_AddLine( p_next, p_prev, TRIS_COLOR, SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE, 2, 0 );

				indices.erase( indices.begin() + i );
				earFound = true;
				break;
			}
		}

		if ( !earFound ) {
			break;
		}
	}

	// Draw the remaining triangles if any
	if ( indices.size() == 3 ) {
		SVG_Nav_DebugDraw_AddLine( verts[ indices[ 0 ] ], verts[ indices[ 1 ] ], TRIS_COLOR, SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE, 2, 0 );
		SVG_Nav_DebugDraw_AddLine( verts[ indices[ 1 ] ], verts[ indices[ 2 ] ], TRIS_COLOR, SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE, 2, 0 );
		SVG_Nav_DebugDraw_AddLine( verts[ indices[ 2 ] ], verts[ indices[ 0 ] ], TRIS_COLOR, SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE, 2, 0 );
	} else if ( indices.size() > 3 ) {
		// Fallback for complex/self-intersecting degenerate polygons: just draw a simple fan from the first vertex.
		const Vector3 v0 = verts[ indices[ 0 ] ];
		for ( size_t i = 1; i < indices.size() - 1; i++ ) {
			const Vector3 v1 = verts[ indices[ i ] ];
			const Vector3 v2 = verts[ indices[ i + 1 ] ];
			SVG_Nav_DebugDraw_AddLine( v0, v1, TRIS_COLOR, SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE, 2, 0 );
			SVG_Nav_DebugDraw_AddLine( v1, v2, TRIS_COLOR, SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE, 2, 0 );
			SVG_Nav_DebugDraw_AddLine( v2, v0, TRIS_COLOR, SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE, 2, 0 );
		}
	}
}
#endif
#endif // DEBUG_NAV_DRAW_NGON_TRIANGLES_METHOD != 0
/**
* @brief Draw the nav KD-tree and polygon overlays for the current frame.
**/
void SVG_Nav_DebugDraw() {
	// Skip debug drawing if the nav debug cvars are not initialized or disabled, or if there are no nav nodes to draw.
	if ( !s_nav_debug_nodes || !s_nav_debug_polys ) {
		return;
	}
	// Skip debug drawing if both nav debug cvars are disabled, or if there are no nav nodes to draw.
	if ( s_nav_debug_nodes->value == 0 && s_nav_debug_polys->value == 0 ) {
		return;
	}
	// Skip debug drawing if there are no nav nodes to draw.
	if ( g_nav_nodes.size() == 0 ) {
		return;
	}

	/**
	*	Ensure properly valid player edict for debug drawing. If not valid, we skip the debug draw for this frame.
	**/
	svg_base_edict_t *player = g_edict_pool.EdictForNumber( 1 );
	if ( !player || !player->inUse ) {
		return;
	}

	/**
	*	Recursively draw the KD-tree nodes starting from the root node (index 0),
	*	using the player's current position as the center of the debug draw radius.
	**/
	RecursiveDrawNodes( 0, player->currentOrigin, s_nav_debug_draw_radius->value );
	Nav_DebugDrawQueryLeaves();

	/**
	*	Debug draw the navmesh half-edge mesh N-gons, their edges, and if enabled possibly their triangles too.
	**/
	if ( s_nav_debug_polys && s_nav_debug_polys->value != 0 ) {
		// Iterate over the N-gon faces in the half-edge mesh and draw them if they are within the debug draw radius of the player.
		for ( int32_t i = 0; i < g_nav_faces.size(); i++ ) {
			// Get the current face from the global nav faces vector.
			const nav_face_t &face = g_nav_faces[ i ];

			// Simple distance check
			if ( QM_Vector3DistanceSqrDP( face.center, Vector3DP( player->currentOrigin ) ) > ( s_nav_debug_draw_radius->value * s_nav_debug_draw_radius->value ) ) {
				continue;
			}
			/**
			*	Render triangle edges if enabled.
			**/
			if ( s_nav_debug_tris && s_nav_debug_tris->value != 0 && face.num_edges >= 3 ) {
				#ifdef DEBUG_NAV_DRAW_NGON_TRIANGLES_METHOD
					#if DEBUG_NAV_DRAW_NGON_TRIANGLES_METHOD == 1
						Nav_DebugDraw_TriangleFanForFace( face );
					#elif DEBUG_NAV_DRAW_NGON_TRIANGLES_METHOD == 2
						Nav_DebugDraw_EarClippingTrianglesForFace( face );
					#endif // #if DEBUG_NAV_DRAW_NGON_TRIANGLES_METHOD == 1
				#else
					// Default to triangle fan if no method is defined.
					Nav_DebugDraw_TriangleFanForFace( face );
				#endif // #ifdef DEBUG_NAV_DRAW_NGON_TRIANGLES_METHOD
			}

			/**
			*	Iterate over all edges of the face and draw them with appropriate colors based on:
			*		- Whether they have a twin edge or not. (Tahiti Gold and "Brown" in Krita PixelArt32 palette).
			*		- Whether they are "opened" or "closed" (disabled) edges.
			**/
			for ( int32_t e = 0; e < face.num_edges; e++ ) {
				// Get the half-edge for this edge of the face.
				const nav_halfedge_t &he = g_nav_halfedges[ face.first_edge_idx + e ];

				// Store the start and end vertices of the edge for drawing.
				Vector3 start = static_cast<Vector3>(g_nav_vertices[ he.vertex_idx ]);
				Vector3 end = static_cast<Vector3>(g_nav_vertices[ g_nav_halfedges[ he.next_idx ].vertex_idx ]);

				// Determine whether to draw with depth testing.
				//const bool depthTest = ( s_nav_debug_tris && s_nav_debug_tris->value >= 2 );
				sg_svc_debug_draw_style_flags_t styleFlags = SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE;//( depthTest ? SG_SVC_DEBUG_DRAW_STYLE_FLAG_DEPTH_TEST : SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE );

				/**
				*	Matching transition-owned edge found via direct edge ownership or face transition ownership,
				*	so we draw it with the door state color.
				**/
				if ( he.edge_entity_id != ENTITYNUM_NONE || face.entity_id != ENTITYNUM_NONE || face.transition_entity_id != ENTITYNUM_NONE ) {
					// Dynamic transition edges retain their state color whether or not a matching twin was found.
					const uint32_t edgeColor = ( he.flags & NAV_EDGE_DISABLED ) != 0 ? EDGE_DISABLED_COLOR : EDGE_ENABLED_COLOR;
					SVG_Nav_DebugDraw_AddLine( start, end, edgeColor, styleFlags, 2, 0 );
				} else if ( he.twin_idx != -1 ) {
					// Draw ordinary twinned edges using the polygon face debug color.
					SVG_Nav_DebugDraw_AddLine( start, end, EDGE_TWIN_COLOR, styleFlags, 2, 0 );
				/**
				*	No matching twin edge means this is a boundary edge, so we draw it with a distinct color.
				**/
				} else {
					// Draw edges without twins (boundary edges) using a distinct color.
					SVG_Nav_DebugDraw_AddLine( start, end, EDGE_NO_TWIN_COLOR, styleFlags, 4, 0 );
				}
			}

		}
	}

	/**
	*	Render temporary route test debug primitives once per debug draw frame.
	**/
	Nav_DebugDrawTestRoute();
}
