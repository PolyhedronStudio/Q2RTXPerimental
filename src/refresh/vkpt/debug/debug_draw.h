/*
Copyright (C) 2024

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#ifndef __VKPT_DEBUG_DRAW_H__
#define __VKPT_DEBUG_DRAW_H__

#include "../vkpt.h"

/**
*	@brief	Shared styling parameters for queued debug draw primitives.
*	@note	All helpers consume the same style payload so callers can consistently
*			control color, screen-space thickness, and optional outline width.
**/
typedef struct vkpt_debug_draw_style_s {
	vec4_t color;
	float thickness_px;
	float outline_thickness_px;
	uint32_t flags;
} vkpt_debug_draw_style_t;

/**
*	@brief	Primitive-specific flags used by the isolated debug renderer.
**/
enum {
	VKPT_DEBUG_DRAW_FLAG_NONE = 0,
	VKPT_DEBUG_DRAW_FLAG_DEPTH_TEST = 1 << 0,
	VKPT_DEBUG_DRAW_FLAG_OUTLINE = 1 << 1,
};

/**
*	@brief	Reset queued primitives for the current frame.
**/
void vkpt_debug_draw_clear_queue( void );

/**
*	@brief	Queue a world-space line segment.
**/
void vkpt_debug_draw_add_line( const vec3_t start, const vec3_t end, const vkpt_debug_draw_style_t *style );

/**
*	@brief	Queue a world-space arrow built from a shaft plus a head.
**/
void vkpt_debug_draw_add_arrow( const vec3_t start, const vec3_t end, float head_length, const vkpt_debug_draw_style_t *style );

/**
*	@brief	Queue a sphere approximation centered at the given world-space point.
**/
void vkpt_debug_draw_add_sphere( const vec3_t center, float radius, const vkpt_debug_draw_style_t *style );

/**
*	@brief	Queue a capsule defined by two endpoints and a radius.
**/
void vkpt_debug_draw_add_capsule( const vec3_t start, const vec3_t end, float radius, const vkpt_debug_draw_style_t *style );

/**
*	@brief	Queue a cylinder approximation defined by two endpoints and a radius.
**/
void vkpt_debug_draw_add_cylinder( const vec3_t start, const vec3_t end, float radius, const vkpt_debug_draw_style_t *style );

/**
*	@brief	Queue an axis-aligned box from mins/maxs bounds.
**/
void vkpt_debug_draw_add_box( const vec3_t mins, const vec3_t maxs, const vkpt_debug_draw_style_t *style );

/**
*	@brief	Fill a style struct with a useful default configuration.
**/
void vkpt_debug_draw_make_style( vkpt_debug_draw_style_t *style, float r, float g, float b, float a, float thickness_px, float outline_thickness_px, uint32_t flags );

#endif
