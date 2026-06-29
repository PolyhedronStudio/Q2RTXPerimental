#include "svgame/nav/nav_debug_draw.h"
#include "svgame/svg_local.h"
#include <algorithm>
#include "svgame/svg_utils.h"

/**
*	Nav Debug Draw Local State
**/
//! Top-level nav debug cvar used to gate all drawing.
static cvar_t *s_nav_debug_draw = nullptr;

void SVG_Nav_DebugDraw_Init( void ) {
	if ( !s_nav_debug_draw ) {
		s_nav_debug_draw = gi.cvar( "nav_debug_draw", "1", 0 );
	}
}

void SVG_Nav_DebugDraw_BeginFrame( void ) {
	// Drawing messages removed. Handled via gi.R_ interfaces directly.
}

void SVG_Nav_DebugDraw_FlushFrame( void ) {
	// Drawing messages removed. Handled via gi.R_ interfaces directly.
}

const bool SVG_Nav_DebugDraw_IsEnabled( void ) {
	return s_nav_debug_draw && s_nav_debug_draw->value != 0;
}

const int32_t SVG_Nav_DebugDraw_GetQueuedPrimitiveCount( void ) {
	return 0; // Queue removed.
}

void SVG_Nav_DebugDraw_AddLine( const Vector3 &start, const Vector3 &end, const uint32_t color, const uint16_t styleFlags, const float thicknessPx, const float outlineThicknessPx ) {
	if ( !SVG_Nav_DebugDraw_IsEnabled() ) {
		return;
	}
	if ( gi.R_DrawDebugLine ) {
		SVG_DebugDraw_Line( start, end, color );
	}
}

void SVG_Nav_DebugDraw_AddAabb( const Vector3 &mins, const Vector3 &maxs, const uint32_t color, const uint16_t styleFlags, const float thicknessPx, const float outlineThicknessPx ) {
	if ( !SVG_Nav_DebugDraw_IsEnabled() ) {
		return;
	}
	if ( gi.R_DrawDebugBox ) {
		SVG_DebugDraw_Box( mins, maxs, color );
	}
}

void SVG_Nav_DebugDraw_AddSphere( const Vector3 &center, const float radius, const uint32_t color, const uint16_t styleFlags, const float thicknessPx, const float outlineThicknessPx ) {
	if ( !SVG_Nav_DebugDraw_IsEnabled() ) {
		return;
	}
	if ( gi.R_DrawDebugSphere ) {
		SVG_DebugDraw_Sphere( center, radius, color );
	}
}

void SVG_Nav_DebugDraw_AddArrow( const Vector3 &start, const Vector3 &end, const float headLength, const uint32_t color, const uint16_t styleFlags, const float thicknessPx, const float outlineThicknessPx ) {
	if ( !SVG_Nav_DebugDraw_IsEnabled() ) {
		return;
	}
	if ( gi.R_DrawDebugArrow ) {
		SVG_DebugDraw_Arrow( start, end, headLength, color );
	}
}

void SVG_Nav_DebugDraw_AddCapsule( const Vector3 &start, const Vector3 &end, const float radius, const uint32_t color, const uint16_t styleFlags, const float thicknessPx, const float outlineThicknessPx ) {
	if ( !SVG_Nav_DebugDraw_IsEnabled() ) {
		return;
	}
	if ( gi.R_DrawDebugCapsule ) {
		SVG_DebugDraw_Capsule( start, end, radius, color );
	}
}

void SVG_Nav_DebugDraw_AddCylinder( const Vector3 &start, const Vector3 &end, const float radius, const uint32_t color, const uint16_t styleFlags, const float thicknessPx, const float outlineThicknessPx ) {
	if ( !SVG_Nav_DebugDraw_IsEnabled() ) {
		return;
	}
	if ( gi.R_DrawDebugCylinder ) {
		SVG_DebugDraw_Cylinder( start, end, radius, color );
	}
}

