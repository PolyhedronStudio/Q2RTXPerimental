#pragma once

#include "svgame/nav/nav_types.h"
#include "sharedgame/sg_cmd_messages.h"

/**
*	@brief	Register debug draw cvars and initialize queue state for the nav system.
**/
void SVG_Nav_DebugDraw_Init( void );

/**
*	@brief	Begin a new server-frame debug draw submission window.
**/
void SVG_Nav_DebugDraw_BeginFrame( void );

/**
*	@brief	Flush queued primitives to opted-in clients and clear the frame queue.
**/
void SVG_Nav_DebugDraw_FlushFrame( void );

/**
*	@brief	Return true when top-level nav debug draw is enabled via nav_debug_draw cvar.
**/
const bool SVG_Nav_DebugDraw_IsEnabled( void );

/**
*	@brief	Return the number of primitives currently queued for the active frame.
**/
const int32_t SVG_Nav_DebugDraw_GetQueuedPrimitiveCount( void );

/**
*	@brief	Queue one world-space line segment.
**/
void SVG_Nav_DebugDraw_AddLine( const Vector3 &start, const Vector3 &end, const uint32_t color,
	const uint16_t styleFlags = SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE,
	const float thicknessPx = 2.0f,
	const float outlineThicknessPx = 0.0f );

/**
*	@brief	Queue one axis-aligned world-space box.
**/
void SVG_Nav_DebugDraw_AddAabb( const Vector3 &mins, const Vector3 &maxs, const uint32_t color,
	const uint16_t styleFlags = SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE,
	const float thicknessPx = 2.0f,
	const float outlineThicknessPx = 0.0f );

/**
*	@brief	Queue one world-space sphere.
**/
void SVG_Nav_DebugDraw_AddSphere( const Vector3 &center, const float radius, const uint32_t color,
	const uint16_t styleFlags = SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE,
	const float thicknessPx = 2.0f,
	const float outlineThicknessPx = 0.0f );

/**
*	@brief	Queue one world-space arrow.
**/
void SVG_Nav_DebugDraw_AddArrow( const Vector3 &start, const Vector3 &end, const float headLength, const uint32_t color,
	const uint16_t styleFlags = SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE,
	const float thicknessPx = 2.0f,
	const float outlineThicknessPx = 0.0f );

/**
*	@brief	Queue one world-space capsule.
**/
void SVG_Nav_DebugDraw_AddCapsule( const Vector3 &start, const Vector3 &end, const float radius, const uint32_t color,
	const uint16_t styleFlags = SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE,
	const float thicknessPx = 2.0f,
	const float outlineThicknessPx = 0.0f );

/**
*	@brief	Queue one world-space cylinder.
**/
void SVG_Nav_DebugDraw_AddCylinder( const Vector3 &start, const Vector3 &end, const float radius, const uint32_t color,
	const uint16_t styleFlags = SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE,
	const float thicknessPx = 2.0f,
	const float outlineThicknessPx = 0.0f );
