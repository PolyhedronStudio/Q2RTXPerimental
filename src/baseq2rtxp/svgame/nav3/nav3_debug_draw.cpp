/********************************************************************
*
*
*    ServerGame: Nav3 Debug Draw Queue + Stream API - Implementation
*
*
********************************************************************/
#include "svgame/nav3/nav3_debug_draw.h"

#include <algorithm>


/**
*
*
*    Nav3 Debug Draw Constants:
*
*
**/
//! Absolute safety cap for in-memory queued debug primitives.
static constexpr int32_t NAV3_DEBUG_DRAW_QUEUE_HARD_LIMIT = 8192;
//! Default configured queue cap for one server frame.
static constexpr int32_t NAV3_DEBUG_DRAW_DEFAULT_MAX_PRIMITIVES = 2048;
//! Bounded primitive chunk count serialized into one `svc_debug_draw` message.
static constexpr int32_t NAV3_DEBUG_DRAW_MAX_PRIMS_PER_MESSAGE = 64;
//! Bounded minimum size for sphere and arrow heads to avoid degenerate payloads.
static constexpr float NAV3_DEBUG_DRAW_MIN_DIMENSION = 0.1f;


/**
*
*
*    Nav3 Debug Draw Local Types:
*
*
**/
/**
*    @brief  Compact queued primitive payload for one debug draw command entry.
**/
struct nav3_debug_draw_primitive_t {
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
*
*
*    Nav3 Debug Draw Local State:
*
*
**/
//! Top-level nav3 debug cvar used to gate all queue submission and streaming.
static cvar_t *s_nav3_debug = nullptr;
//! Configurable queue cap cvar for one frame.
static cvar_t *s_nav3_debug_max_primitives = nullptr;
//! Per-subsystem detail-level cvars matching `NAV3_DEBUG_DRAW_SUBSYSTEM_TABLE`.
static cvar_t *s_nav3_debug_subsystem_cvars[ static_cast<int32_t>( nav3_debug_draw_subsystem_t::Count ) ] = {};

//! Fixed-capacity per-frame primitive queue.
static nav3_debug_draw_primitive_t s_nav3_debug_draw_queue[ NAV3_DEBUG_DRAW_QUEUE_HARD_LIMIT ] = {};
//! Number of queued primitives in `s_nav3_debug_draw_queue`.
static int32_t s_nav3_debug_draw_queue_count = 0;
//! Number of primitives dropped this frame because queue budget was exhausted.
static int32_t s_nav3_debug_draw_dropped_count = 0;
//! Ensures budget overflow warning prints at most once per frame.
static bool s_nav3_debug_draw_budget_warning_emitted = false;


/**
*
*
*    Nav3 Debug Draw Local Helpers:
*
*
**/
/**
*    @brief  Convert subsystem enum to validated table index.
*    @param  subsystem  Subsystem enum value to convert.
*    @return Valid table index or `-1` when out-of-range.
**/
static int32_t Nav3_DebugDraw_SubsystemToIndex( const nav3_debug_draw_subsystem_t subsystem ) {
    const int32_t index = static_cast<int32_t>( subsystem );
    if ( index < 0 || index >= static_cast<int32_t>( nav3_debug_draw_subsystem_t::Count ) ) {
        return -1;
    }
    return index;
}

/**
*    @brief  Return configured per-frame queue cap clamped to safe bounds.
**/
static int32_t Nav3_DebugDraw_GetConfiguredQueueCap( void ) {
    int32_t queueCap = NAV3_DEBUG_DRAW_DEFAULT_MAX_PRIMITIVES;
    if ( s_nav3_debug_max_primitives ) {
        queueCap = s_nav3_debug_max_primitives->integer;
    }

    queueCap = std::max( 1, queueCap );
    queueCap = std::min( queueCap, NAV3_DEBUG_DRAW_QUEUE_HARD_LIMIT );
    return queueCap;
}

/**
*    @brief  Register top-level and per-subsystem debug draw cvars.
**/
static void Nav3_DebugDraw_RegisterCvars( void ) {
    if ( !s_nav3_debug ) {
        s_nav3_debug = gi.cvar( "nav3_debug", "0", 0 );
    }
    if ( !s_nav3_debug_max_primitives ) {
        s_nav3_debug_max_primitives = gi.cvar( "nav3_debug_max_primitives", "2048", 0 );
    }

    const int32_t subsystemCount = static_cast<int32_t>( nav3_debug_draw_subsystem_t::Count );
    for ( int32_t i = 0; i < subsystemCount; i++ ) {
        if ( !s_nav3_debug_subsystem_cvars[ i ] ) {
            s_nav3_debug_subsystem_cvars[ i ] = gi.cvar( NAV3_DEBUG_DRAW_SUBSYSTEM_TABLE[ i ].cvar_name, "0", 0 );
        }
    }
}

/**
*    @brief  Return true when the given player has opted into debug draw stream traffic.
*    @param  playerEntity  Candidate player entity.
**/
static const bool Nav3_DebugDraw_ClientIsOptedIn( const svg_base_edict_t *playerEntity ) {
    if ( !playerEntity || !playerEntity->client ) {
        return false;
    }

    const char *userInfo = playerEntity->client->pers.userinfo;
    if ( !userInfo || userInfo[ 0 ] == '\0' ) {
        return false;
    }

    const char *optInValue = Info_ValueForKey( userInfo, SG_SVC_DEBUG_DRAW_CLIENT_CVAR_NAME );
    return optInValue && atoi( optInValue ) != 0;
}

/**
*    @brief  Write one vector to the active game-import message stream as three floats.
*    @param  value  Vector to serialize.
**/
static void Nav3_DebugDraw_WriteVector3( const Vector3 &value ) {
    gi.WriteFloat( value.x );
    gi.WriteFloat( value.y );
    gi.WriteFloat( value.z );
}

/**
*    @brief  Serialize one queued primitive payload into the active message stream.
*    @param  primitive  Queued primitive payload to serialize.
**/
static void Nav3_DebugDraw_SerializePrimitive( const nav3_debug_draw_primitive_t &primitive ) {
    // Write primitive header shared by all primitive payloads.
    gi.WriteUint8( static_cast<int32_t>( primitive.type ) );
    gi.WriteInt32( static_cast<int32_t>( primitive.color ) );
    gi.WriteUint16( primitive.style_flags );
    gi.WriteFloat( primitive.thickness_px );
    gi.WriteFloat( primitive.outline_thickness_px );

    // Serialize primitive-specific payload.
    switch ( primitive.type ) {
    case sg_svc_debug_draw_primitive_type_t::Line:
    case sg_svc_debug_draw_primitive_type_t::Aabb:
        Nav3_DebugDraw_WriteVector3( primitive.p0 );
        Nav3_DebugDraw_WriteVector3( primitive.p1 );
        break;
    case sg_svc_debug_draw_primitive_type_t::Obb:
        Nav3_DebugDraw_WriteVector3( primitive.p0 );
        Nav3_DebugDraw_WriteVector3( primitive.p1 );
        Nav3_DebugDraw_WriteVector3( primitive.p2 );
        Nav3_DebugDraw_WriteVector3( primitive.p3 );
        Nav3_DebugDraw_WriteVector3( primitive.p4 );
        break;
    case sg_svc_debug_draw_primitive_type_t::Sphere:
        Nav3_DebugDraw_WriteVector3( primitive.p0 );
        gi.WriteFloat( primitive.radius );
        break;
    case sg_svc_debug_draw_primitive_type_t::Arrow:
        Nav3_DebugDraw_WriteVector3( primitive.p0 );
        Nav3_DebugDraw_WriteVector3( primitive.p1 );
        gi.WriteFloat( primitive.head_length );
        break;
    case sg_svc_debug_draw_primitive_type_t::Text:
        Nav3_DebugDraw_WriteVector3( primitive.p0 );
        gi.WriteString( primitive.text );
        break;
    case sg_svc_debug_draw_primitive_type_t::Count:
    default:
        // Keep stream deterministic by writing a harmless fallback payload.
        Nav3_DebugDraw_WriteVector3( Vector3{} );
        Nav3_DebugDraw_WriteVector3( Vector3{} );
        break;
    }
}

/**
*    @brief  Send all queued debug draw primitives to one opted-in client in bounded chunks.
*    @param  playerEntity  Destination player entity.
**/
static void Nav3_DebugDraw_SendQueueToClient( svg_base_edict_t *playerEntity ) {
    int32_t queueIndex = 0;
    while ( queueIndex < s_nav3_debug_draw_queue_count ) {
        const int32_t remaining = s_nav3_debug_draw_queue_count - queueIndex;
        const int32_t chunkCount = std::min( remaining, NAV3_DEBUG_DRAW_MAX_PRIMS_PER_MESSAGE );

        // Begin one bounded svc_debug_draw message.
        gi.WriteUint8( svc_debug_draw );
        gi.WriteUint8( SG_SVC_DEBUG_DRAW_VERSION );
        gi.WriteUint16( chunkCount );

        // Serialize chunk payload.
        for ( int32_t i = 0; i < chunkCount; i++ ) {
            Nav3_DebugDraw_SerializePrimitive( s_nav3_debug_draw_queue[ queueIndex ] );
            queueIndex++;
        }

        // Unreliable by default so debug traffic cannot stall gameplay replication.
        gi.unicast( playerEntity, false );
    }
}

/**
*    @brief  Queue one primitive payload while enforcing per-frame queue cap.
*    @param  primitive  Primitive payload to enqueue.
*    @return True when the payload was queued.
**/
static const bool Nav3_DebugDraw_EnqueuePrimitive( const nav3_debug_draw_primitive_t &primitive ) {
    if ( !SVG_Nav3_DebugDraw_IsEnabled() ) {
        return false;
    }

    const int32_t queueCap = Nav3_DebugDraw_GetConfiguredQueueCap();
    if ( s_nav3_debug_draw_queue_count >= queueCap ) {
        s_nav3_debug_draw_dropped_count++;
        if ( !s_nav3_debug_draw_budget_warning_emitted ) {
            gi.cprintf( nullptr, PRINT_HIGH,
                "nav3_debug_draw: queue cap reached (%d). Dropping additional primitives this frame.\n",
                queueCap );
            s_nav3_debug_draw_budget_warning_emitted = true;
        }
        return false;
    }

    if ( s_nav3_debug_draw_queue_count >= NAV3_DEBUG_DRAW_QUEUE_HARD_LIMIT ) {
        return false;
    }

    s_nav3_debug_draw_queue[ s_nav3_debug_draw_queue_count ] = primitive;
    s_nav3_debug_draw_queue_count++;
    return true;
}

/**
*    @brief  Build one baseline primitive payload with shared style fields filled.
**/
static nav3_debug_draw_primitive_t Nav3_DebugDraw_MakePrimitiveBase(
    const sg_svc_debug_draw_primitive_type_t primitiveType,
    const uint32_t color,
    const uint16_t styleFlags,
    const float thicknessPx,
    const float outlineThicknessPx ) {
    nav3_debug_draw_primitive_t primitive = {};
    primitive.type = primitiveType;
    primitive.color = color;
    primitive.style_flags = styleFlags;
    primitive.thickness_px = thicknessPx;
    primitive.outline_thickness_px = outlineThicknessPx;
    return primitive;
}


/**
*
*
*    Nav3 Debug Draw Public API:
*
*
**/
/**
*    @brief  Register debug draw cvars and initialize queue state.
**/
void SVG_Nav3_DebugDraw_Init( void ) {
    Nav3_DebugDraw_RegisterCvars();
    s_nav3_debug_draw_queue_count = 0;
    s_nav3_debug_draw_dropped_count = 0;
    s_nav3_debug_draw_budget_warning_emitted = false;
}

/**
*    @brief  Shutdown nav3 debug draw runtime state.
**/
void SVG_Nav3_DebugDraw_Shutdown( void ) {
    s_nav3_debug_draw_queue_count = 0;
    s_nav3_debug_draw_dropped_count = 0;
    s_nav3_debug_draw_budget_warning_emitted = false;
}

/**
*    @brief  Begin a new server-frame debug draw submission window.
**/
void SVG_Nav3_DebugDraw_BeginFrame( void ) {
    s_nav3_debug_draw_budget_warning_emitted = false;
    s_nav3_debug_draw_dropped_count = 0;
}

/**
*    @brief  Flush queued primitives to opted-in clients and clear the frame queue.
**/
void SVG_Nav3_DebugDraw_FlushFrame( void ) {
    if ( !SVG_Nav3_DebugDraw_IsEnabled() ) {
        s_nav3_debug_draw_queue_count = 0;
        s_nav3_debug_draw_dropped_count = 0;
        return;
    }

    if ( s_nav3_debug_draw_queue_count <= 0 ) {
        return;
    }

    // Walk connected client entities and stream to opted-in recipients only.
    for ( int32_t i = 1; i <= game.maxclients; i++ ) {
        svg_base_edict_t *playerEntity = g_edict_pool.EdictForNumber( i );
        if ( !SVG_Entity_IsActive( playerEntity ) ) {
            continue;
        }
        if ( !playerEntity->client ) {
            continue;
        }
        if ( !Nav3_DebugDraw_ClientIsOptedIn( playerEntity ) ) {
            continue;
        }

        Nav3_DebugDraw_SendQueueToClient( playerEntity );
    }

    // Queue lifetime is one frame only.
    s_nav3_debug_draw_queue_count = 0;
    s_nav3_debug_draw_dropped_count = 0;
}

/**
*    @brief  Return true when top-level nav3 debug draw is enabled.
**/
const bool SVG_Nav3_DebugDraw_IsEnabled( void ) {
    return s_nav3_debug && ( s_nav3_debug->integer != 0 );
}

/**
*    @brief  Return configured detail level for one subsystem cvar.
*    @param  subsystem  Subsystem detail selector.
*    @return Integer detail level (`0` = off).
**/
const int32_t SVG_Nav3_DebugDraw_GetSubsystemLevel( const nav3_debug_draw_subsystem_t subsystem ) {
    const int32_t subsystemIndex = Nav3_DebugDraw_SubsystemToIndex( subsystem );
    if ( subsystemIndex < 0 ) {
        return 0;
    }

    const cvar_t *subsystemCvar = s_nav3_debug_subsystem_cvars[ subsystemIndex ];
    if ( !subsystemCvar ) {
        return 0;
    }

    return subsystemCvar->integer;
}

/**
*    @brief  Return true when the subsystem cvar is at least `minLevel` and top-level debug is on.
*    @param  subsystem  Subsystem detail selector.
*    @param  minLevel   Minimum accepted detail level.
**/
const bool SVG_Nav3_DebugDraw_IsSubsystemEnabled( const nav3_debug_draw_subsystem_t subsystem, const int32_t minLevel ) {
    if ( !SVG_Nav3_DebugDraw_IsEnabled() ) {
        return false;
    }
    return SVG_Nav3_DebugDraw_GetSubsystemLevel( subsystem ) >= minLevel;
}

/**
*    @brief  Return the number of primitives currently queued for the active frame.
**/
const int32_t SVG_Nav3_DebugDraw_GetQueuedPrimitiveCount( void ) {
    return s_nav3_debug_draw_queue_count;
}

/**
*    @brief  Queue one world-space line segment.
**/
void SVG_Nav3_DebugDraw_AddLine( const Vector3 &start, const Vector3 &end, const uint32_t color,
    const uint16_t styleFlags,
    const float thicknessPx,
    const float outlineThicknessPx ) {
    nav3_debug_draw_primitive_t primitive = Nav3_DebugDraw_MakePrimitiveBase(
        sg_svc_debug_draw_primitive_type_t::Line,
        color,
        styleFlags,
        thicknessPx,
        outlineThicknessPx );
    primitive.p0 = start;
    primitive.p1 = end;
    Nav3_DebugDraw_EnqueuePrimitive( primitive );
}

/**
*    @brief  Queue one axis-aligned world-space box.
**/
void SVG_Nav3_DebugDraw_AddAabb( const Vector3 &mins, const Vector3 &maxs, const uint32_t color,
    const uint16_t styleFlags,
    const float thicknessPx,
    const float outlineThicknessPx ) {
    nav3_debug_draw_primitive_t primitive = Nav3_DebugDraw_MakePrimitiveBase(
        sg_svc_debug_draw_primitive_type_t::Aabb,
        color,
        styleFlags,
        thicknessPx,
        outlineThicknessPx );
    primitive.p0 = mins;
    primitive.p1 = maxs;
    Nav3_DebugDraw_EnqueuePrimitive( primitive );
}

/**
*    @brief  Queue one oriented world-space box.
*    @param  center     Box center.
*    @param  extents    Half-size extents along each basis axis.
*    @param  axisX      Local X basis vector (normalized expected).
*    @param  axisY      Local Y basis vector (normalized expected).
*    @param  axisZ      Local Z basis vector (normalized expected).
**/
void SVG_Nav3_DebugDraw_AddObb( const Vector3 &center, const Vector3 &extents, const Vector3 &axisX, const Vector3 &axisY, const Vector3 &axisZ,
    const uint32_t color,
    const uint16_t styleFlags,
    const float thicknessPx,
    const float outlineThicknessPx ) {
    nav3_debug_draw_primitive_t primitive = Nav3_DebugDraw_MakePrimitiveBase(
        sg_svc_debug_draw_primitive_type_t::Obb,
        color,
        styleFlags,
        thicknessPx,
        outlineThicknessPx );
    primitive.p0 = center;
    primitive.p1 = extents;
    primitive.p2 = axisX;
    primitive.p3 = axisY;
    primitive.p4 = axisZ;
    Nav3_DebugDraw_EnqueuePrimitive( primitive );
}

/**
*    @brief  Queue one world-space sphere primitive.
**/
void SVG_Nav3_DebugDraw_AddSphere( const Vector3 &center, const float radius, const uint32_t color,
    const uint16_t styleFlags,
    const float thicknessPx,
    const float outlineThicknessPx ) {
    nav3_debug_draw_primitive_t primitive = Nav3_DebugDraw_MakePrimitiveBase(
        sg_svc_debug_draw_primitive_type_t::Sphere,
        color,
        styleFlags,
        thicknessPx,
        outlineThicknessPx );
    primitive.p0 = center;
    primitive.radius = std::max( NAV3_DEBUG_DRAW_MIN_DIMENSION, radius );
    Nav3_DebugDraw_EnqueuePrimitive( primitive );
}

/**
*    @brief  Queue one world-space arrow primitive.
**/
void SVG_Nav3_DebugDraw_AddArrow( const Vector3 &start, const Vector3 &end, const float headLength, const uint32_t color,
    const uint16_t styleFlags,
    const float thicknessPx,
    const float outlineThicknessPx ) {
    nav3_debug_draw_primitive_t primitive = Nav3_DebugDraw_MakePrimitiveBase(
        sg_svc_debug_draw_primitive_type_t::Arrow,
        color,
        styleFlags,
        thicknessPx,
        outlineThicknessPx );
    primitive.p0 = start;
    primitive.p1 = end;
    primitive.head_length = std::max( NAV3_DEBUG_DRAW_MIN_DIMENSION, headLength );
    Nav3_DebugDraw_EnqueuePrimitive( primitive );
}

/**
*    @brief  Queue one world-space text label primitive.
**/
void SVG_Nav3_DebugDraw_AddText( const Vector3 &origin, const char *text, const uint32_t color,
    const uint16_t styleFlags,
    const float thicknessPx,
    const float outlineThicknessPx ) {
    nav3_debug_draw_primitive_t primitive = Nav3_DebugDraw_MakePrimitiveBase(
        sg_svc_debug_draw_primitive_type_t::Text,
        color,
        styleFlags,
        thicknessPx,
        outlineThicknessPx );
    primitive.p0 = origin;
    Q_strlcpy( primitive.text, text ? text : "", sizeof( primitive.text ) );
    Nav3_DebugDraw_EnqueuePrimitive( primitive );
}

/**
*    @brief  Queue a bounded synthetic validation payload for `sv nav3_debug_draw_validate`.
**/
void SVG_Nav3_DebugDraw_QueueValidationPrimitives( void ) {
    if ( !SVG_Nav3_DebugDraw_IsEnabled() ) {
        return;
    }

    // Anchor validation primitives around first active client origin when available.
    Vector3 anchor = { 0.0f, 0.0f, 48.0f };
    for ( int32_t i = 1; i <= game.maxclients; i++ ) {
        svg_base_edict_t *playerEntity = g_edict_pool.EdictForNumber( i );
        if ( !SVG_Entity_IsActive( playerEntity ) ) {
            continue;
        }
        if ( !playerEntity->client ) {
            continue;
        }

        anchor = playerEntity->currentOrigin;
        anchor.z += 24.0f;
        break;
    }

    // Primitive 1: baseline line segment.
    SVG_Nav3_DebugDraw_AddLine(
        anchor,
        Vector3{ anchor.x + 96.0f, anchor.y, anchor.z },
        U32_GREEN );

    // Primitive 2: axis-aligned box around anchor.
    SVG_Nav3_DebugDraw_AddAabb(
        Vector3{ anchor.x - 16.0f, anchor.y - 16.0f, anchor.z - 16.0f },
        Vector3{ anchor.x + 16.0f, anchor.y + 16.0f, anchor.z + 16.0f },
        U32_YELLOW );

    // Primitive 3: oriented box with fixed rotated XY basis.
    SVG_Nav3_DebugDraw_AddObb(
        Vector3{ anchor.x, anchor.y + 96.0f, anchor.z },
        Vector3{ 20.0f, 10.0f, 16.0f },
        Vector3{ 0.8660f, 0.5000f, 0.0f },
        Vector3{ -0.5000f, 0.8660f, 0.0f },
        Vector3{ 0.0f, 0.0f, 1.0f },
        U32_CYAN );

    // Primitive 4: sphere marker.
    SVG_Nav3_DebugDraw_AddSphere(
        Vector3{ anchor.x - 96.0f, anchor.y, anchor.z },
        18.0f,
        U32_MAGENTA );

    // Primitive 5: upward arrow.
    SVG_Nav3_DebugDraw_AddArrow(
        anchor,
        Vector3{ anchor.x, anchor.y, anchor.z + 96.0f },
        18.0f,
        U32_BLUE );

    // Primitive 6: text label anchor.
    SVG_Nav3_DebugDraw_AddText(
        Vector3{ anchor.x + 32.0f, anchor.y + 32.0f, anchor.z + 32.0f },
        "nav3_debug_draw_validate",
        U32_WHITE );
}
