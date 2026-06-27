#include "svgame/nav/nav_core.h"
#include "svgame/nav/nav_containers.h"
#include "svgame/nav/nav_types.h"
#include "svgame/nav/nav_generate.h"
#include "svgame/nav/nav_path.h"
#include "svgame/nav/nav_debug.h"

#include "svgame/nav/nav_debug_draw.h"
#include "svgame/svg_utils.h"

//! Cvar that toggles KD-tree node overlay rendering.
cvar_t *s_nav_debug_nodes = nullptr;
//! Cvar that toggles polygon edge overlay rendering.
cvar_t *s_nav_debug_polys = nullptr;

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
//! Whether cached test route is valid and should be rendered.
static bool s_nav_dbg_has_test_path = false;

/**
* @brief Register nav debug cvars for the node and polygon overlays.
**/
void Nav_DebugInit() {
    s_nav_debug_nodes = gi.cvar("nav_debug_nodes", "0", 0);
    s_nav_debug_polys = gi.cvar("nav_debug_polys", "1", 0);
}

/**
* @brief Resolve a player entity to use for nav debug command sampling.
* @return Player edict or nullptr.
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
* @brief Build a feet-origin sample from one actor origin.
* @param actor Actor used to derive the feet sample.
* @return Feet-origin world-space point.
**/
static Vector3 Nav_DebugBuildFeetOrigin( const svg_base_edict_t *actor ) {
	Vector3 feet = actor->currentOrigin;
	feet.z += actor->mins.z;
	return feet;
}

/**
* @brief Resolve a stable local face for debug queries, with a robust global fallback.
* @param point World-space query point.
* @return Face id or -1 when no nav face exists.
**/
static int32_t Nav_DebugFindClosestFaceInLeaf( const Vector3 &point ) {
	const int32_t leafFace = Nav_FindPolyInLeaf( point );
	if ( leafFace >= 0 && static_cast<size_t>( leafFace ) < g_nav_faces.size() ) {
		const nav_face_t &face = g_nav_faces[ leafFace ];
		if ( Nav_PointInsideFace2D( point, face ) ) {
			const Vector3 v0 = g_nav_vertices[ g_nav_halfedges[ face.first_edge_idx ].vertex_idx ];
			const float planeDist = static_cast<float>( QM_Vector3DotProduct( v0, face.normal ) );
			const float verticalDist = std::fabs( static_cast<float>( QM_Vector3DotProduct( point, face.normal ) ) - planeDist );
			if ( verticalDist <= 64.0f ) {
				return leafFace;
			}
		}
	}
	return Nav_FindClosestPolyGlobal( point );
}

/**
* @brief Draw cached goal markers and cached test route.
**/
static void Nav_DebugDrawTestRoute() {
	if ( !s_nav_debug_polys || s_nav_debug_polys->value == 0 ) {
		return;
	}

	if ( s_nav_dbg_has_goal_a ) {
		SVG_Nav_DebugDraw_AddSphere( s_nav_dbg_goal_a_origin, 6.0f, U32_GREEN, SG_SVC_DEBUG_DRAW_STYLE_FLAG_DEPTH_TEST );
	}
	if ( s_nav_dbg_has_goal_b ) {
		SVG_Nav_DebugDraw_AddSphere( s_nav_dbg_goal_b_origin, 6.0f, U32_RED, SG_SVC_DEBUG_DRAW_STYLE_FLAG_DEPTH_TEST );
	}

	if ( !s_nav_dbg_has_test_path || s_nav_dbg_test_path.empty() ) {
		return;
	}

	Vector3 prev = s_nav_dbg_goal_a_origin;
	if ( s_nav_dbg_goal_a_face >= 0 && static_cast<size_t>( s_nav_dbg_goal_a_face ) < g_nav_faces.size() ) {
		prev = g_nav_faces[ s_nav_dbg_goal_a_face ].center;
	}

	for ( size_t i = 0; i < s_nav_dbg_test_path.size(); i++ ) {
		const int32_t faceIdx = s_nav_dbg_test_path[ i ];
		if ( faceIdx < 0 || static_cast<size_t>( faceIdx ) >= g_nav_faces.size() ) {
			continue;
		}
		const Vector3 point = g_nav_faces[ faceIdx ].center;
		SVG_Nav_DebugDraw_AddSphere( point, 4.0f, U32_CYAN, SG_SVC_DEBUG_DRAW_STYLE_FLAG_DEPTH_TEST );
		SVG_Nav_DebugDraw_AddLine( prev, point, U32_MAGENTA, SG_SVC_DEBUG_DRAW_STYLE_FLAG_DEPTH_TEST );
		prev = point;
	}

	Vector3 endPoint = s_nav_dbg_goal_b_origin;
	if ( s_nav_dbg_goal_b_face >= 0 && static_cast<size_t>( s_nav_dbg_goal_b_face ) < g_nav_faces.size() ) {
		endPoint = g_nav_faces[ s_nav_dbg_goal_b_face ].center;
	}
	SVG_Nav_DebugDraw_AddLine( prev, endPoint, U32_MAGENTA, SG_SVC_DEBUG_DRAW_STYLE_FLAG_DEPTH_TEST );
}

void Nav_DebugSetGoalACommand( void ) {
	svg_base_edict_t *player = Nav_DebugGetCommandPlayer();
	if ( !player ) {
		gi.dprintf( "nav_dbg_goal_a: player not available\n" );
		return;
	}

	const Vector3 playerCenter = player->currentOrigin;
	s_nav_dbg_goal_a_origin = Nav_DebugBuildFeetOrigin( player );
	s_nav_dbg_goal_a_face = Nav_DebugFindClosestFaceInLeaf( s_nav_dbg_goal_a_origin );
	s_nav_dbg_has_goal_a = true;
	s_nav_dbg_has_test_path = false;
	s_nav_dbg_test_path.clear();
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
	s_nav_dbg_goal_b_face = Nav_DebugFindClosestFaceInLeaf( s_nav_dbg_goal_b_origin );
	s_nav_dbg_has_goal_b = true;
	s_nav_dbg_has_test_path = false;
	s_nav_dbg_test_path.clear();
	gi.dprintf( "nav_dbg_goal_b: center=(%.2f %.2f %.2f) feet=(%.2f %.2f %.2f) face=%d\n",
		playerCenter.x, playerCenter.y, playerCenter.z,
		s_nav_dbg_goal_b_origin.x, s_nav_dbg_goal_b_origin.y, s_nav_dbg_goal_b_origin.z, s_nav_dbg_goal_b_face );
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
	policy.max_drop_height = PM_DROPOFF_ALLOWED_SIZE;
	policy.enable_max_drop_height_cap = true;
	policy.max_drop_height_cap = PM_DROPOFF_MAX_SIZE;

	s_nav_dbg_test_path.clear();
	s_nav_dbg_has_test_path = Nav_FindPath( s_nav_dbg_goal_a_face, s_nav_dbg_goal_b_face, s_nav_dbg_test_path, policy );
	gi.dprintf( "nav_dbg_test: A=%d B=%d success=%d nodes=%d\n",
		s_nav_dbg_goal_a_face, s_nav_dbg_goal_b_face, s_nav_dbg_has_test_path ? 1 : 0, static_cast<int32_t>( s_nav_dbg_test_path.size() ) );
}

static void RecursiveDrawNodes(int32_t nodeIndex, const Vector3 &playerPos, float radius) {
    if (nodeIndex == -1 || nodeIndex >= g_nav_nodes.size()) return;
    
    const nav_kdtree_node_t &node = g_nav_nodes[nodeIndex];
    
    // Simple AABB vs AABB check
    if (node.mins.x > playerPos.x + radius || node.maxs.x < playerPos.x - radius ||
        node.mins.y > playerPos.y + radius || node.maxs.y < playerPos.y - radius ||
        node.mins.z > playerPos.z + radius || node.maxs.z < playerPos.z - radius) {
        return;
    }

    if (s_nav_debug_nodes && s_nav_debug_nodes->value != 0) {
        SVG_Nav_DebugDraw_AddAabb(node.mins, node.maxs, U32_CYAN);
    }
    
    // Draw polys by directly iterating over g_nav_polys later in SVG_Nav_DebugDraw.
    
    if (node.left_child != -1) RecursiveDrawNodes(node.left_child, playerPos, radius);
    if (node.right_child != -1) RecursiveDrawNodes(node.right_child, playerPos, radius);
}

/**
* @brief Draw the nav KD-tree and polygon overlays for the current frame.
**/
void SVG_Nav_DebugDraw() {
    if (!s_nav_debug_nodes || !s_nav_debug_polys) return;
    if (s_nav_debug_nodes->value == 0 && s_nav_debug_polys->value == 0) return;
    if (g_nav_nodes.size() == 0) return;

    svg_base_edict_t *player = g_edict_pool.EdictForNumber(1);
    if (!player || !player->inUse) return;

    RecursiveDrawNodes(0, player->currentOrigin, CM_MAX_WORLD_SIZE );

    if (s_nav_debug_polys && s_nav_debug_polys->value != 0) {
        for (int32_t i = 0; i < g_nav_faces.size(); i++) {
            const nav_face_t &face = g_nav_faces[i];
            
            // Simple distance check
            if (QM_Vector3DistanceSqr(face.center, player->currentOrigin) > ((float)CM_MAX_WORLD_SIZE * (float)CM_MAX_WORLD_SIZE)) {
                continue;
            }

            for (int32_t e = 0; e < face.num_edges; e++) {
                const nav_halfedge_t& he = g_nav_halfedges[face.first_edge_idx + e];
                
                Vector3 start = g_nav_vertices[he.vertex_idx];
                Vector3 end = g_nav_vertices[g_nav_halfedges[he.next_idx].vertex_idx];

                if (he.twin_idx != -1) {
                    // Draw internal twinned edges in BLUE so we can see the mesh grid cells!
                    SVG_Nav_DebugDraw_AddLine(start, end, U32_BLUE, SG_SVC_DEBUG_DRAW_STYLE_FLAG_DEPTH_TEST);
                } else {
                    // Draw boundary edges in YELLOW
                    SVG_Nav_DebugDraw_AddLine(start, end, U32_YELLOW, SG_SVC_DEBUG_DRAW_STYLE_FLAG_DEPTH_TEST);
                }
            }

        }
    }

	/**
	*	Render temporary route test debug primitives once per debug draw frame.
	**/
	Nav_DebugDrawTestRoute();
}
