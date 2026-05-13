/********************************************************************
*
*
*	VKPT:	Debug drawing utilities for rendering simple world-space 
*			primitives mostly to assist with development and 
*			debugging.
*
*
********************************************************************/
#ifndef __VKPT_DEBUG_DRAW_H__
#define __VKPT_DEBUG_DRAW_H__

#include "../vkpt.h"

/**
*	@brief	Shared styling parameters for queued debug draw primitives.
*	@note	All helpers consume the same style payload so callers can consistently
*			control color, screen-space thickness, and optional outline width.
**/
typedef struct vkpt_debug_draw_style_s {
	//! RGBA color for the primitive, with each channel in the [0.0, 1.0] range. The alpha channel controls the transparency of the primitive, where 0.0 is fully transparent and 1.0 is fully opaque. This allows for creating effects like ghosted or highlighted primitives by adjusting the alpha value accordingly.
	vec4_t color;
	//! Thickness of the primitive in screen-space pixels. For lines, this controls how thick the line appears on the screen regardless of its distance from the camera. For other primitives like spheres or boxes, this can be used to control the thickness of the edges when rendered in wireframe mode. A larger value will make the primitive more visible, while a smaller value will make it thinner and potentially harder to see.
	float thickness_px;
	//! When the outline flag is enabled, this controls the thickness of the outline in screen-space pixels. The outline is typically rendered as a separate pass around the main primitive, using a darker color to create contrast and enhance visibility. This can be especially useful for thin primitives like lines or wireframes that might otherwise be hard to see against complex backgrounds. A larger value will create a thicker outline, while a smaller value will create a thinner outline.
	float outline_thickness_px;
	//! Bitfield of primitive-specific flags that control rendering behavior, such as depth testing and outlining. These flags allow for fine-tuning how each primitive is rendered, enabling features like always-on-top rendering or enhanced visibility through outlines. The specific flags and their effects are defined in the enum below.
	uint32_t flags;
} vkpt_debug_draw_style_t;

/**
*	@brief	Primitive-specific flags used by the isolated debug renderer.
**/
enum {
	//! No special rendering flags, just draw the primitive as-is.
	VKPT_DEBUG_DRAW_FLAG_NONE = 0,
	//! Enable depth testing for the primitive, so it will be occluded by world geometry and other primitives as expected. When disabled, the primitive will always render on top of everything else, which can be useful for visualizing things like entity origins or bounding boxes that might otherwise be hidden inside geometry.
	VKPT_DEBUG_DRAW_FLAG_DEPTH_TEST = 1 << 0,
	//! When enabled, the primitive will be drawn with an additional outline to enhance visibility. The outline's thickness is determined by the 'outline_thickness_px' field in the style struct, and its color is typically a darker version of the main color to create contrast. This can be especially helpful for thin primitives like lines or wireframes that might be hard to see against complex backgrounds.
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
