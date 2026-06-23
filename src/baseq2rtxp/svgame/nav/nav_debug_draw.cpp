#include "svgame/nav/nav_debug_draw.h"
#include "svgame/svg_local.h"
#include <algorithm>
#include "svgame/svg_utils.h"

/**
*	Nav Debug Draw Constants
**/
static constexpr int32_t NAV_DEBUG_DRAW_QUEUE_HARD_LIMIT = 32768;
static constexpr int32_t NAV_DEBUG_DRAW_DEFAULT_MAX_PRIMITIVES = 16384;
static constexpr int32_t NAV_DEBUG_DRAW_MAX_PRIMS_PER_MESSAGE = 64;

/**
*	Compact queued primitive payload for one debug draw command entry.
**/
struct nav_debug_draw_primitive_t {
	sg_svc_debug_draw_primitive_type_t type = sg_svc_debug_draw_primitive_type_t::Line;
	uint32_t color = U32_WHITE;
	uint16_t style_flags = SG_SVC_DEBUG_DRAW_STYLE_FLAG_DEPTH_TEST;
	float thickness_px = 2.0f;
	float outline_thickness_px = 0.0f;
	Vector3 p0 = {};
	Vector3 p1 = {};
	Vector3 p2 = {};
	Vector3 p3 = {};
	Vector3 p4 = {};
	float radius = 0.0f;
	float head_length = 0.0f;
	char text[ SG_SVC_DEBUG_DRAW_MAX_LABEL_CHARS ] = {};
};

/**
*	Nav Debug Draw Local State
**/
//! Top-level nav debug cvar used to gate all queue submission and streaming.
static cvar_t *s_nav_debug_draw = nullptr;
static cvar_t *s_nav_debug_max_primitives = nullptr;

//! Fixed-capacity per-frame primitive queue.
static nav_debug_draw_primitive_t s_nav_debug_draw_queue[ NAV_DEBUG_DRAW_QUEUE_HARD_LIMIT ] = {};
static int32_t s_nav_debug_draw_queue_count = 0;
static int32_t s_nav_debug_draw_dropped_count = 0;
static bool s_nav_debug_draw_budget_warning_emitted = false;

/**
*	Return configured per-frame queue cap clamped to safe bounds.
**/
static int32_t Nav_DebugDraw_GetConfiguredQueueCap( void ) {
	int32_t queueCap = NAV_DEBUG_DRAW_DEFAULT_MAX_PRIMITIVES;
	if ( s_nav_debug_max_primitives ) {
		queueCap = s_nav_debug_max_primitives->integer;
	}
	queueCap = std::max( 1, queueCap );
	queueCap = std::min( queueCap, NAV_DEBUG_DRAW_QUEUE_HARD_LIMIT );
	return queueCap;
}

/**
*	Return true when the given player has opted into debug draw stream traffic.
**/
static const bool Nav_DebugDraw_ClientIsOptedIn( const svg_base_edict_t *playerEntity ) {
	if ( !playerEntity || !playerEntity->client ) {
		return false;
	}
	return true; // We don't require client userinfo opt-in for the base nav debug anymore!
}

/**
*	Write one vector to the active game-import message stream as three floats.
**/
static void Nav_DebugDraw_WriteVector3( const Vector3 &value ) {
	gi.WriteFloat( value.x );
	gi.WriteFloat( value.y );
	gi.WriteFloat( value.z );
}

/**
*	Serialize one queued primitive payload into the active message stream.
**/
static void Nav_DebugDraw_SerializePrimitive( const nav_debug_draw_primitive_t &primitive ) {
	// Write primitive header
	gi.WriteUint8( static_cast<int32_t>( primitive.type ) );
	gi.WriteInt32( static_cast<int32_t>( primitive.color ) );
	gi.WriteUint16( primitive.style_flags );
	gi.WriteFloat( primitive.thickness_px );
	gi.WriteFloat( primitive.outline_thickness_px );

	// Serialize primitive-specific payload.
	switch ( primitive.type ) {
	case sg_svc_debug_draw_primitive_type_t::Line:
	case sg_svc_debug_draw_primitive_type_t::Aabb:
		Nav_DebugDraw_WriteVector3( primitive.p0 );
		Nav_DebugDraw_WriteVector3( primitive.p1 );
		break;
	case sg_svc_debug_draw_primitive_type_t::Obb:
		Nav_DebugDraw_WriteVector3( primitive.p0 );
		Nav_DebugDraw_WriteVector3( primitive.p1 );
		Nav_DebugDraw_WriteVector3( primitive.p2 );
		Nav_DebugDraw_WriteVector3( primitive.p3 );
		Nav_DebugDraw_WriteVector3( primitive.p4 );
		break;
	case sg_svc_debug_draw_primitive_type_t::Sphere:
		Nav_DebugDraw_WriteVector3( primitive.p0 );
		gi.WriteFloat( primitive.radius );
		break;
	case sg_svc_debug_draw_primitive_type_t::Arrow:
		Nav_DebugDraw_WriteVector3( primitive.p0 );
		Nav_DebugDraw_WriteVector3( primitive.p1 );
		gi.WriteFloat( primitive.head_length );
		break;
	case sg_svc_debug_draw_primitive_type_t::Text:
		Nav_DebugDraw_WriteVector3( primitive.p0 );
		gi.WriteString( primitive.text );
		break;
	case sg_svc_debug_draw_primitive_type_t::Count:
	default:
		// Fallback
		Nav_DebugDraw_WriteVector3( Vector3{} );
		Nav_DebugDraw_WriteVector3( Vector3{} );
		break;
	}
}

/**
*	Send all queued debug draw primitives to one opted-in client in bounded chunks.
**/
static void Nav_DebugDraw_SendQueueToClient( svg_base_edict_t *playerEntity ) {
	int32_t queueIndex = 0;
	while ( queueIndex < s_nav_debug_draw_queue_count ) {
		const int32_t remaining = s_nav_debug_draw_queue_count - queueIndex;
		const int32_t chunkCount = std::min( remaining, NAV_DEBUG_DRAW_MAX_PRIMS_PER_MESSAGE );

		// Begin one bounded svc_debug_draw message.
		gi.WriteUint8( svc_debug_draw );
		gi.WriteUint8( SG_SVC_DEBUG_DRAW_VERSION );
		gi.WriteUint16( chunkCount );

		// Serialize chunk payload.
		for ( int32_t i = 0; i < chunkCount; i++ ) {
			Nav_DebugDraw_SerializePrimitive( s_nav_debug_draw_queue[ queueIndex ] );
			queueIndex++;
		}

		// Unreliable by default so debug traffic cannot stall gameplay replication.
		gi.unicast( playerEntity, false );
	}
}

/**
*	Queue one primitive payload while enforcing per-frame queue cap.
**/
static const bool Nav_DebugDraw_EnqueuePrimitive( const nav_debug_draw_primitive_t &primitive ) {
	if ( !SVG_Nav_DebugDraw_IsEnabled() ) {
		return false;
	}

	const int32_t queueCap = Nav_DebugDraw_GetConfiguredQueueCap();
	if ( s_nav_debug_draw_queue_count >= queueCap ) {
		s_nav_debug_draw_dropped_count++;
		if ( !s_nav_debug_draw_budget_warning_emitted ) {
			gi.cprintf( nullptr, PRINT_HIGH,
				"nav_debug_draw: queue cap reached (%d). Dropping additional primitives this frame.\n",
				queueCap );
			s_nav_debug_draw_budget_warning_emitted = true;
		}
		return false;
	}

	if ( s_nav_debug_draw_queue_count >= NAV_DEBUG_DRAW_QUEUE_HARD_LIMIT ) {
		return false;
	}

	s_nav_debug_draw_queue[ s_nav_debug_draw_queue_count ] = primitive;
	s_nav_debug_draw_queue_count++;
	return true;
}

/**
*	Build one baseline primitive payload with shared style fields filled.
**/
static nav_debug_draw_primitive_t Nav_DebugDraw_MakePrimitiveBase(
	const sg_svc_debug_draw_primitive_type_t primitiveType,
	const uint32_t color,
	const uint16_t styleFlags,
	const float thicknessPx,
	const float outlineThicknessPx ) {
	nav_debug_draw_primitive_t primitive = {};
	primitive.type = primitiveType;
	primitive.color = color;
	primitive.style_flags = styleFlags;
	primitive.thickness_px = thicknessPx;
	primitive.outline_thickness_px = outlineThicknessPx;
	return primitive;
}

void SVG_Nav_DebugDraw_Init( void ) {
	if ( !s_nav_debug_draw ) {
		s_nav_debug_draw = gi.cvar( "nav_debug_draw", "0", 0 );
	}
	if ( !s_nav_debug_max_primitives ) {
		s_nav_debug_max_primitives = gi.cvar( "nav_debug_max_primitives", "16384", 0 );
	}
	s_nav_debug_draw_queue_count = 0;
	s_nav_debug_draw_dropped_count = 0;
	s_nav_debug_draw_budget_warning_emitted = false;
}

void SVG_Nav_DebugDraw_BeginFrame( void ) {
	s_nav_debug_draw_queue_count = 0;
	s_nav_debug_draw_dropped_count = 0;
	s_nav_debug_draw_budget_warning_emitted = false;
}

void SVG_Nav_DebugDraw_FlushFrame( void ) {
	if ( s_nav_debug_draw_queue_count <= 0 ) {
		return;
	}

	// Stream primitives directly to all active clients that have opted in.
	for ( int32_t i = 1; i <= game.maxclients; i++ ) {
		svg_base_edict_t *ent = g_edict_pool.EdictForNumber( i );
		if ( !ent || !ent->inUse || !ent->client ) {
			continue;
		}

		if ( Nav_DebugDraw_ClientIsOptedIn( ent ) ) {
			Nav_DebugDraw_SendQueueToClient( ent );
		}
	}

	s_nav_debug_draw_queue_count = 0;
}

const bool SVG_Nav_DebugDraw_IsEnabled( void ) {
	return s_nav_debug_draw && s_nav_debug_draw->value != 0;
}

const int32_t SVG_Nav_DebugDraw_GetQueuedPrimitiveCount( void ) {
	return s_nav_debug_draw_queue_count;
}

void SVG_Nav_DebugDraw_AddLine( const Vector3 &start, const Vector3 &end, const uint32_t color, const uint16_t styleFlags, const float thicknessPx, const float outlineThicknessPx ) {
	if ( !SVG_Nav_DebugDraw_IsEnabled() ) {
		return;
	}
	if ( gi.R_DrawDebugLine ) {
		SVG_DebugDraw_Line( start, end, color );
	} else {
		nav_debug_draw_primitive_t primitive = Nav_DebugDraw_MakePrimitiveBase(
			sg_svc_debug_draw_primitive_type_t::Line, color, styleFlags, thicknessPx, outlineThicknessPx );
		primitive.p0 = start;
		primitive.p1 = end;
		Nav_DebugDraw_EnqueuePrimitive( primitive );
	}
}

void SVG_Nav_DebugDraw_AddAabb( const Vector3 &mins, const Vector3 &maxs, const uint32_t color, const uint16_t styleFlags, const float thicknessPx, const float outlineThicknessPx ) {
	if ( !SVG_Nav_DebugDraw_IsEnabled() ) {
		return;
	}
	if ( gi.R_DrawDebugBox ) {
		SVG_DebugDraw_Box( mins, maxs, color );
	} else {
		nav_debug_draw_primitive_t primitive = Nav_DebugDraw_MakePrimitiveBase(
			sg_svc_debug_draw_primitive_type_t::Aabb, color, styleFlags, thicknessPx, outlineThicknessPx );
		primitive.p0 = mins;
		primitive.p1 = maxs;
		Nav_DebugDraw_EnqueuePrimitive( primitive );
	}
}
