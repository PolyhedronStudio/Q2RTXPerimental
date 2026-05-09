/********************************************************************
*
*
*    ServerGame: Nav3 Debug Draw Queue + Stream API
*
*
********************************************************************/
#pragma once

#include "svgame/nav3/nav3_types.h"
#include "sharedgame/sg_cmd_messages.h"


/**
*
*
*    Nav3 Debug Draw Subsystem Labels + Palette:
*
*
**/
/**
*    @brief  Per-subsystem debug draw channel identifiers.
**/
enum class nav3_debug_draw_subsystem_t : uint8_t {
    Spans = 0,
    Filters,
    Octree,
    Cover,
    Edges,
    Jump,
    Movers,
    Dynamic,
    Agents,
    Path,
    Count
};

/**
*    @brief  Shared subsystem label + cvar + color tuple.
*    @note   Keep this table centralized so all nav3 subsystems use consistent labels and colors.
**/
struct nav3_debug_draw_subsystem_desc_t {
    nav3_debug_draw_subsystem_t subsystem;
    const char *label;
    const char *cvar_name;
    uint32_t color;
};

inline constexpr nav3_debug_draw_subsystem_desc_t NAV3_DEBUG_DRAW_SUBSYSTEM_TABLE[] = {
    { nav3_debug_draw_subsystem_t::Spans, "spans", "nav3_debug_spans", U32_GREEN },
    { nav3_debug_draw_subsystem_t::Filters, "filters", "nav3_debug_filters", U32_RED },
    { nav3_debug_draw_subsystem_t::Octree, "octree", "nav3_debug_octree", U32_BLUE },
    { nav3_debug_draw_subsystem_t::Cover, "cover", "nav3_debug_cover", U32_YELLOW },
    { nav3_debug_draw_subsystem_t::Edges, "edges", "nav3_debug_edges", U32_CYAN },
    { nav3_debug_draw_subsystem_t::Jump, "jump", "nav3_debug_jump", U32_MAGENTA },
    { nav3_debug_draw_subsystem_t::Movers, "movers", "nav3_debug_movers", U32_WHITE },
    { nav3_debug_draw_subsystem_t::Dynamic, "dynamic", "nav3_debug_dynamic", U32_RED },
    { nav3_debug_draw_subsystem_t::Agents, "agents", "nav3_debug_agents", U32_CYAN },
    { nav3_debug_draw_subsystem_t::Path, "path", "nav3_debug_path", U32_YELLOW },
};


/**
*
*
*    Nav3 Debug Draw Runtime API:
*
*
**/
/**
*    @brief  Register debug draw cvars and initialize queue state.
**/
void SVG_Nav3_DebugDraw_Init( void );

/**
*    @brief  Shutdown nav3 debug draw runtime state.
**/
void SVG_Nav3_DebugDraw_Shutdown( void );

/**
*    @brief  Begin a new server-frame debug draw submission window.
**/
void SVG_Nav3_DebugDraw_BeginFrame( void );

/**
*    @brief  Flush queued primitives to opted-in clients and clear the frame queue.
**/
void SVG_Nav3_DebugDraw_FlushFrame( void );

/**
*    @brief  Return true when top-level nav3 debug draw is enabled.
**/
const bool SVG_Nav3_DebugDraw_IsEnabled( void );

/**
*    @brief  Return configured detail level for one subsystem cvar.
*    @param  subsystem  Subsystem detail selector.
*    @return Integer detail level (`0` = off).
**/
const int32_t SVG_Nav3_DebugDraw_GetSubsystemLevel( const nav3_debug_draw_subsystem_t subsystem );

/**
*    @brief  Return true when the subsystem cvar is at least `minLevel` and top-level debug is on.
*    @param  subsystem  Subsystem detail selector.
*    @param  minLevel   Minimum accepted detail level.
**/
const bool SVG_Nav3_DebugDraw_IsSubsystemEnabled( const nav3_debug_draw_subsystem_t subsystem, const int32_t minLevel = 1 );

/**
*    @brief  Return the number of primitives currently queued for the active frame.
**/
const int32_t SVG_Nav3_DebugDraw_GetQueuedPrimitiveCount( void );

/**
*    @brief  Queue one world-space line segment.
**/
void SVG_Nav3_DebugDraw_AddLine( const Vector3 &start, const Vector3 &end, const uint32_t color,
    const uint16_t styleFlags = SG_SVC_DEBUG_DRAW_STYLE_FLAG_DEPTH_TEST,
    const float thicknessPx = 2.0f,
    const float outlineThicknessPx = 0.0f );

/**
*    @brief  Queue one axis-aligned world-space box.
**/
void SVG_Nav3_DebugDraw_AddAabb( const Vector3 &mins, const Vector3 &maxs, const uint32_t color,
    const uint16_t styleFlags = SG_SVC_DEBUG_DRAW_STYLE_FLAG_DEPTH_TEST,
    const float thicknessPx = 2.0f,
    const float outlineThicknessPx = 0.0f );

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
    const uint16_t styleFlags = SG_SVC_DEBUG_DRAW_STYLE_FLAG_DEPTH_TEST,
    const float thicknessPx = 2.0f,
    const float outlineThicknessPx = 0.0f );

/**
*    @brief  Queue one world-space sphere primitive.
**/
void SVG_Nav3_DebugDraw_AddSphere( const Vector3 &center, const float radius, const uint32_t color,
    const uint16_t styleFlags = SG_SVC_DEBUG_DRAW_STYLE_FLAG_DEPTH_TEST,
    const float thicknessPx = 2.0f,
    const float outlineThicknessPx = 0.0f );

/**
*    @brief  Queue one world-space arrow primitive.
**/
void SVG_Nav3_DebugDraw_AddArrow( const Vector3 &start, const Vector3 &end, const float headLength, const uint32_t color,
    const uint16_t styleFlags = SG_SVC_DEBUG_DRAW_STYLE_FLAG_DEPTH_TEST,
    const float thicknessPx = 2.0f,
    const float outlineThicknessPx = 0.0f );

/**
*    @brief  Queue one world-space text label primitive.
**/
void SVG_Nav3_DebugDraw_AddText( const Vector3 &origin, const char *text, const uint32_t color,
    const uint16_t styleFlags = SG_SVC_DEBUG_DRAW_STYLE_FLAG_DEPTH_TEST,
    const float thicknessPx = 2.0f,
    const float outlineThicknessPx = 0.0f );

/**
*    @brief  Queue a bounded synthetic validation payload for `sv nav3_debug_draw_validate`.
**/
void SVG_Nav3_DebugDraw_QueueValidationPrimitives( void );
