//! Tactical cover point debug rendering and visualization routines.
/********************************************************************
*
*
*	ServerGame: NavMesh Cover Debug Visualizer
*
*	Draws positions, outward normal arrows, wall heights, peeking flags,
*	and dynamic parent mover links for all precalculated cover points.
*
*
********************************************************************/
#include "nav_cover_debug.h"
#include "nav_cover_types.h"
#include "nav_cover_query.h"
#include "nav_debug_draw.h"
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"
#include <vector>

//! External reference to the global list of generated cover points.
extern std::vector<nav_cover_point_t> g_nav_cover_points;

//! Console variable enabling tactical cover visualization.
static cvar_t *s_nav_debug_cover = nullptr;

//! Debug Colors for Cover Points:
//! Low cover color (crouch / waist-high).
static const uint32_t COLOR_COVER_LOW = MakeColor( 251, 242, 54, 255 );
//! High cover color (standing / full-height).
static const uint32_t COLOR_COVER_HIGH = MakeColor( 106, 190, 48, 255 );
//! Claimed / reserved cover color.
static const uint32_t COLOR_COVER_CLAIMED = MakeColor( 217, 87, 99, 255 );
//! Directional normal arrow color.
static const uint32_t COLOR_COVER_NORMAL = MakeColor( 95, 205, 228, 255 );
//! Peeking tangent indicator color.
static const uint32_t COLOR_COVER_PEEK = MakeColor( 155, 173, 183, 255 );
//! Dynamic mover link line color.
static const uint32_t COLOR_COVER_MOVER_LINK = MakeColor( 143, 86, 59, 255 );

/**
*	@brief	Initialize tactical cover debug cvars.
**/
void Nav_Cover_DebugInit( void ) {
	if ( !s_nav_debug_cover ) {
		s_nav_debug_cover = gi.cvar( "nav_debug_cover", "0", 0 );
	}
}

/**
*	@brief	Draw tactical cover debug overlays for the active server frame.
**/
void Nav_Cover_DebugDraw( void ) {
	/**
	*	Sanity checks: Verify cvar is enabled, debug drawing is active, and cover points exist.
	**/
	if ( !s_nav_debug_cover || s_nav_debug_cover->value == 0 ) {
		return;
	}
	if ( !SVG_Nav_DebugDraw_IsEnabled() || g_nav_cover_points.empty() ) {
		return;
	}

	/**
	*	Iterate all generated cover points and render geometric indicators.
	**/
	const int32_t num_points = static_cast<int32_t>( g_nav_cover_points.size() );
	for ( int32_t i = 0; i < num_points; i++ ) {
		const nav_cover_point_t &cp = g_nav_cover_points[ i ];

		// Resolve current world-space coordinates (handles moving parent entities).
		Vector3 world_pos = {}, world_normal = {}, world_tangent = {};
		if ( !Nav_GetCoverPointWorld( cp, &world_pos, &world_normal, &world_tangent ) ) {
			continue;
		}

		/**
		*	Determine display color based on claim status and posture type.
		**/
		const bool is_claimed = Nav_IsCoverPointClaimed( i );
		uint32_t sphere_color = COLOR_COVER_LOW;
		if ( is_claimed ) {
			sphere_color = COLOR_COVER_CLAIMED;
		} else if ( cp.cover_type == NAV_COVER_HIGH ) {
			sphere_color = COLOR_COVER_HIGH;
		}

		// 1. Draw base feet position sphere.
		SVG_Nav_DebugDraw_AddSphere( world_pos, 4.0f, sphere_color, SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE );

		// 2. Draw outward protective normal arrow (pointing away from the wall).
		const Vector3 normal_end = QM_Vector3Add( world_pos, QM_Vector3Scale( world_normal, 16.0f ) );
		SVG_Nav_DebugDraw_AddArrow( world_pos, normal_end, 4.0f, COLOR_COVER_NORMAL, SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE );

		// 3. Draw vertical wall height probe line.
		const Vector3 height_top = QM_Vector3Add( world_pos, Vector3{ 0.0f, 0.0f, cp.wall_height } );
		SVG_Nav_DebugDraw_AddLine( world_pos, height_top, sphere_color, SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE, 1.5f );

		// 4. Draw peeking tangent indicators:
		if ( ( cp.peek_flags & NAV_COVER_PEEK_LEFT ) != 0 ) {
			const Vector3 peek_left_end = QM_Vector3Add( world_pos, QM_Vector3Scale( world_tangent, -12.0f ) );
			SVG_Nav_DebugDraw_AddLine( world_pos, peek_left_end, COLOR_COVER_PEEK, SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE, 2.0f );
		}
		if ( ( cp.peek_flags & NAV_COVER_PEEK_RIGHT ) != 0 ) {
			const Vector3 peek_right_end = QM_Vector3Add( world_pos, QM_Vector3Scale( world_tangent, 12.0f ) );
			SVG_Nav_DebugDraw_AddLine( world_pos, peek_right_end, COLOR_COVER_PEEK, SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE, 2.0f );
		}

		// 5. Draw dynamic mover parent link line:
		if ( cp.parent_entity_id > 0 && cp.parent_entity_id < g_edict_pool.num_edicts ) {
			const svg_base_edict_t *parent_ent = g_edicts[ cp.parent_entity_id ];
			if ( parent_ent && parent_ent->inUse ) {
				SVG_Nav_DebugDraw_AddLine( parent_ent->currentOrigin, world_pos, COLOR_COVER_MOVER_LINK, SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE, 1.0f );
			}
		}
	}
}
