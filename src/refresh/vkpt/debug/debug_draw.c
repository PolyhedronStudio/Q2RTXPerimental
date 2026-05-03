/*
Copyright (C) 2024

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#include "debug_draw.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

extern cvar_t *cvar_pt_draw_debug_3d_geometry;

/**
*	@brief	CPU-side queued primitive instance written by the public helper functions.
*	@note	Each instance is tessellated into ribbon/billboard quads during record().
**/
typedef struct vkpt_debug_draw_instance_s {
	float    point_a[ 4 ];  //!< World-space endpoint A (w=1) or sphere center.
	float    point_b[ 4 ];  //!< World-space endpoint B; w stores capsule/sphere radius.
	float    color[ 4 ];    //!< RGBA color.
	float    params[ 4 ];   //!< [0]=outline_px, [1]=thickness_px.
	int32_t  type;          //!< VKPT_DEBUG_DRAW_PRIM_SEGMENT or VKPT_DEBUG_DRAW_PRIM_SPHERE.
	int32_t  flags;         //!< VKPT_DEBUG_DRAW_FLAG_* bitmask.
	int32_t  pad0;
	int32_t  pad1;
} vkpt_debug_draw_instance_t;

/**
*	@brief	Per-vertex data uploaded to the GPU for 3D ribbon/billboard geometry.
*	@note	pos is pre-computed Vulkan clip-space (after VP transform + frustum clip on CPU).
*			The total struct size is 64 bytes (aligned).
**/
typedef struct vkpt_debug_draw_vertex_s {
	float    pos[ 4 ];       //!< Clip-space position (passed directly to gl_Position).
	float    color[ 4 ];     //!< RGBA per-vertex colour.
	float    uv[ 2 ];        //!< u = cross-axis [-1,+1], v = along-axis [0,1] (or sphere 2D).
	float    view_depth;     //!< Linearised view-space depth for the per-fragment depth test.
	float    half_width_px;  //!< Half of the total ribbon width in pixels (thickness/2 + outline).
	float    outline_px;     //!< Outline border thickness in pixels.
	int32_t  flags;          //!< VKPT_DEBUG_DRAW_FLAG_* bitmask (flat).
	int32_t  type;           //!< VKPT_DEBUG_DRAW_PRIM_SEGMENT or VKPT_DEBUG_DRAW_PRIM_SPHERE (flat).
	int32_t  pad;            //!< Explicit padding — keeps the struct at 64 bytes.
} vkpt_debug_draw_vertex_t;

enum {
	VKPT_DEBUG_DRAW_PIPELINE_OVERLAY = 0,
	VKPT_DEBUG_DRAW_NUM_PIPELINES
};

enum {
	VKPT_DEBUG_DRAW_PRIM_SEGMENT = 0,
	VKPT_DEBUG_DRAW_PRIM_SPHERE  = 1
};

enum {
	//! Maximum number of queued debug instances per frame.
	VKPT_DEBUG_DRAW_MAX_INSTANCES = 4096,
	//! Number of cross-section steps for cylinder wireframes.
	VKPT_DEBUG_DRAW_CYLINDER_STEPS = 12,
	//! Maximum vertex count: 6 verts (2 triangles) per instance.
	VKPT_DEBUG_DRAW_MAX_VERTICES = VKPT_DEBUG_DRAW_MAX_INSTANCES * 6
};

static VkPipelineLayout  debug_draw_pipeline_layout;
static VkPipeline        debug_draw_pipelines[ VKPT_DEBUG_DRAW_NUM_PIPELINES ];
static VkRenderPass      debug_draw_render_pass;
static VkFramebuffer    *debug_draw_framebuffers = NULL;

//! Per-frame-in-flight vertex buffers (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, host-visible).
static BufferResource_t  debug_draw_vertex_buffer[ MAX_FRAMES_IN_FLIGHT ];

//! CPU-side queue used by helpers (line/arrow/sphere/capsule/cylinder/box).
static vkpt_debug_draw_instance_t debug_draw_queue[ VKPT_DEBUG_DRAW_MAX_INSTANCES ];
//! Number of queued debug instances in the current frame.
static uint32_t debug_draw_queue_count = 0;

/**
*	@brief	Create the isolated swapchain render pass for the debug overlay.
*	@note	The pass loads/stores the swapchain image so debug primitives blend on top.
**/
static void vkpt_debug_draw_create_render_pass( void ) {
	VkAttachmentDescription color_attachment = {
		.format = qvk.surf_format.format,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
	};

	VkAttachmentReference color_attachment_ref = {
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};

	VkSubpassDescription subpass = {
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.colorAttachmentCount = 1,
		.pColorAttachments = &color_attachment_ref,
	};

	VkSubpassDependency dependencies[] = {
		{
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = 0,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		},
	};

	VkRenderPassCreateInfo render_pass_info = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &color_attachment,
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = LENGTH( dependencies ),
		.pDependencies = dependencies,
	};

	_VK( vkCreateRenderPass( qvk.device, &render_pass_info, NULL, &debug_draw_render_pass ) );
	ATTACH_LABEL_VARIABLE( debug_draw_render_pass, RENDER_PASS );
}

/**
*	@brief	Create swapchain framebuffers consumed by the debug render pass.
**/
static void vkpt_debug_draw_create_framebuffers( void ) {
	debug_draw_framebuffers = (VkFramebuffer *)malloc( qvk.num_swap_chain_images * sizeof( *debug_draw_framebuffers ) );
	for ( uint32_t i = 0; i < qvk.num_swap_chain_images; i++ ) {
		VkImageView attachments[] = {
			qvk.swap_chain_image_views[ i ]
		};

		VkFramebufferCreateInfo fb_create_info = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = debug_draw_render_pass,
			.attachmentCount = 1,
			.pAttachments = attachments,
			.width = qvk.extent_unscaled.width,
			.height = qvk.extent_unscaled.height,
			.layers = 1,
		};

		_VK( vkCreateFramebuffer( qvk.device, &fb_create_info, NULL, debug_draw_framebuffers + i ) );
		ATTACH_LABEL_VARIABLE( debug_draw_framebuffers[ i ], FRAMEBUFFER );
	}
}

/**
*	@brief	Multiply a column-major 4×4 matrix by a vec4.
*	@param	M	Column-major 4×4 matrix stored as a flat float[16] array
*				(M[col*4 + row] = element at column col, row row).
*	@param	v	Input vec4.
*	@param	out	[out] Result vec4.
*	@note	Matches GLSL mat4 column-major convention so this computes `out = M * v`.
**/
static void vkpt_debug_mat4_mul_vec4( const float *M, const float v[ 4 ], float out[ 4 ] ) {
	// Compute each output row as the dot product of the corresponding matrix row with v.
	for ( int row = 0; row < 4; row++ ) {
		out[ row ] = 0.0f;
		// Accumulate contribution from each column.
		for ( int col = 0; col < 4; col++ ) {
			out[ row ] += M[ col * 4 + row ] * v[ col ];
		}
	}
}

/**
*	@brief	Clip a homogeneous line segment to the Vulkan view frustum using Liang-Barsky.
*	@param	a		Clip-space start point (vec4).
*	@param	b		Clip-space end point (vec4).
*	@param	t_start	[out] Parametric t at the visible start of the clipped segment.
*	@param	t_end	[out] Parametric t at the visible end of the clipped segment.
*	@return	true if any portion of the segment is visible, false if fully clipped.
*	@note	The engine's projection path for debug overlays relies on x/y screen mapping and
*			positive view-space forward depth (w), while clip-space z is not in standard
*			Vulkan [0,w] form for this matrix path. We therefore clip only against
*			left/right/top/bottom planes here and keep a positive-w sanity check later.
**/
static bool vkpt_debug_clip_segment( const float a[ 4 ], const float b[ 4 ],
                                     float *t_start, float *t_end ) {
	// Initialise the visible parametric interval to the full segment.
	float te = 0.0f;
	float tx = 1.0f;

	// Precompute direction deltas.
	const float dx = b[ 0 ] - a[ 0 ];
	const float dy = b[ 1 ] - a[ 1 ];
	const float dz = b[ 2 ] - a[ 2 ];
	const float dw = b[ 3 ] - a[ 3 ];

	// Macro: process one half-plane where f(t) = p + t*q >= 0 means "inside".
	// q < 0 → line exits the plane (update t_exit).
	// q > 0 → line enters the plane (update t_enter).
	// q = 0 and p < 0 → fully outside this plane → reject.
#define CLIP_PLANE( p_val, q_val ) \
	do { \
		const float p__ = (p_val); \
		const float q__ = (q_val); \
		if ( q__ < 0.0f ) { \
			const float t__ = -p__ / q__; \
			if ( t__ < tx ) tx = t__; \
		} else if ( q__ > 0.0f ) { \
			const float t__ = -p__ / q__; \
			if ( t__ > te ) te = t__; \
		} else if ( p__ < 0.0f ) { \
			return false; \
		} \
	} while ( 0 )

	// Left plane:   x + w >= 0
	CLIP_PLANE( a[ 0 ] + a[ 3 ], dx + dw );
	// Right plane:  w - x >= 0
	CLIP_PLANE( a[ 3 ] - a[ 0 ], dw - dx );
	// Bottom plane: y + w >= 0
	CLIP_PLANE( a[ 1 ] + a[ 3 ], dy + dw );
	// Top plane:    w - y >= 0
	CLIP_PLANE( a[ 3 ] - a[ 1 ], dw - dy );

#undef CLIP_PLANE

	// If enter > exit the segment is fully clipped.
	if ( te > tx ) {
		return false;
	}

	// Output the visible interval.
	*t_start = te;
	*t_end   = tx;
	return true;
}

/**
*	@brief	Compute Euclidean length for a vec3.
*	@param	v	Input vector.
*	@return	Vector length in world units.
**/
static float vkpt_debug_vec3_length( const vec3_t v ) {
	const float len_sq = ( v[ 0 ] * v[ 0 ] ) + ( v[ 1 ] * v[ 1 ] ) + ( v[ 2 ] * v[ 2 ] );
	if ( len_sq <= 0.0f ) {
		return 0.0f;
	}
	return sqrtf( len_sq );
}

/**
*	@brief	Normalize a vec3 with zero-length protection.
*	@param	v	Input vector.
*	@param	out	[out] Normalized result or zero when input is near zero.
*	@return	Original vector length.
**/
static float vkpt_debug_vec3_normalize( const vec3_t v, vec3_t out ) {
	const float len = vkpt_debug_vec3_length( v );
	if ( len <= 0.00001f ) {
		VectorClear( out );
		return 0.0f;
	}

	const float inv = 1.0f / len;
	out[ 0 ] = v[ 0 ] * inv;
	out[ 1 ] = v[ 1 ] * inv;
	out[ 2 ] = v[ 2 ] * inv;
	return len;
}

/**
*	@brief	Push an instance to the CPU queue when capacity allows.
*	@param	instance	Prepared instance payload.
*	@return	True when queued, false when rejected (null/full).
**/
static bool vkpt_debug_draw_enqueue_instance( const vkpt_debug_draw_instance_t *instance ) {
	if ( instance == NULL ) {
		return false;
	}
	if ( debug_draw_queue_count >= VKPT_DEBUG_DRAW_MAX_INSTANCES ) {
		return false;
	}

	debug_draw_queue[ debug_draw_queue_count++ ] = *instance;
	return true;
}

/**
*	@brief	Resolve nullable style into a fully initialized style payload.
*	@param	style	Optional caller style.
*	@param	resolved	[out] Style to use for queue encoding.
**/
static void vkpt_debug_draw_resolve_style( const vkpt_debug_draw_style_t *style, vkpt_debug_draw_style_t *resolved ) {
	if ( resolved == NULL ) {
		return;
	}

	resolved->color[ 0 ] = 1.0f;
	resolved->color[ 1 ] = 0.2f;
	resolved->color[ 2 ] = 0.1f;
	resolved->color[ 3 ] = 0.9f;
	resolved->thickness_px = 2.0f;
	resolved->outline_thickness_px = 0.0f;
	resolved->flags = VKPT_DEBUG_DRAW_FLAG_DEPTH_TEST;
	if ( style != NULL ) {
		*resolved = *style;
	}
}

/**
*	@brief	Encode one line/capsule segment instance into the queue.
**/
static void vkpt_debug_draw_enqueue_segment( const vec3_t start, const vec3_t end, float radius_px, const vkpt_debug_draw_style_t *style ) {
	vkpt_debug_draw_instance_t instance;
	memset( &instance, 0, sizeof( instance ) );

	VectorCopy( start, instance.point_a );
	instance.point_a[ 3 ] = 1.0f;
	VectorCopy( end, instance.point_b );
	instance.point_b[ 3 ] = max( 0.25f, radius_px );
	Vector4Copy( style->color, instance.color );

	instance.params[ 0 ] = max( 0.0f, style->outline_thickness_px );
	instance.params[ 1 ] = max( 0.25f, style->thickness_px );
	instance.type = VKPT_DEBUG_DRAW_PRIM_SEGMENT;
	instance.flags = (int32_t)style->flags;

	vkpt_debug_draw_enqueue_instance( &instance );
}

/**
*	@brief	Encode one sphere instance into the queue.
**/
static void vkpt_debug_draw_enqueue_sphere( const vec3_t center, float radius, const vkpt_debug_draw_style_t *style ) {
	vkpt_debug_draw_instance_t instance;
	memset( &instance, 0, sizeof( instance ) );

	VectorCopy( center, instance.point_a );
	instance.point_a[ 3 ] = 1.0f;
	VectorCopy( center, instance.point_b );
	instance.point_b[ 3 ] = max( 0.25f, radius );
	Vector4Copy( style->color, instance.color );

	instance.params[ 0 ] = max( 0.0f, style->outline_thickness_px );
	instance.params[ 1 ] = max( 0.25f, style->thickness_px );
	instance.type = VKPT_DEBUG_DRAW_PRIM_SPHERE;
	instance.flags = (int32_t)style->flags;

	vkpt_debug_draw_enqueue_instance( &instance );
}

/**
*	@brief	Build a stable orthonormal basis from a direction vector.
*	@param	direction	Normalized forward direction.
*	@param	right	[out] Right axis.
*	@param	up	[out] Up axis.
**/
static void vkpt_debug_draw_build_basis( const vec3_t direction, vec3_t right, vec3_t up ) {
	vec3_t fallback_axis = { 0.0f, 0.0f, 1.0f };
	if ( fabsf( direction[ 2 ] ) > 0.9f ) {
		VectorSet( fallback_axis, 1.0f, 0.0f, 0.0f );
	}

	CrossProduct( direction, fallback_axis, right );
	if ( vkpt_debug_vec3_normalize( right, right ) <= 0.0f ) {
		VectorSet( right, 1.0f, 0.0f, 0.0f );
	}

	CrossProduct( right, direction, up );
	if ( vkpt_debug_vec3_normalize( up, up ) <= 0.0f ) {
		VectorSet( up, 0.0f, 1.0f, 0.0f );
	}
}

/**
*	@brief	Check if the debug draw subsystem should emit draw work this frame.
*	@return	True when `pt_draw_debug_3d_geometry != 0`.
**/
bool vkpt_debug_draw_enabled( void ) {
	if ( cvar_pt_draw_debug_3d_geometry == NULL ) {
		return false;
	}
	return cvar_pt_draw_debug_3d_geometry->integer != 0;
}

/**
*	@brief	Reset queued primitives for the current frame.
**/
void vkpt_debug_draw_clear_queue( void ) {
	debug_draw_queue_count = 0;
}

/**
*	@brief	Build a style payload in one call.
**/
void vkpt_debug_draw_make_style( vkpt_debug_draw_style_t *style, float r, float g, float b, float a, float thickness_px, float outline_thickness_px, uint32_t flags ) {
	if ( style == NULL ) {
		return;
	}

	style->color[ 0 ] = r;
	style->color[ 1 ] = g;
	style->color[ 2 ] = b;
	style->color[ 3 ] = a;
	style->thickness_px = thickness_px;
	style->outline_thickness_px = outline_thickness_px;
	style->flags = flags;
}

/**
*	@brief	Queue a single world-space line segment.
**/
void vkpt_debug_draw_add_line( const vec3_t start, const vec3_t end, const vkpt_debug_draw_style_t *style ) {
	if ( !vkpt_debug_draw_enabled() ) {
		return;
	}

	vkpt_debug_draw_style_t resolved_style;
	vkpt_debug_draw_resolve_style( style, &resolved_style );
	vkpt_debug_draw_enqueue_segment( start, end, max( 0.25f, resolved_style.thickness_px * 0.5f ), &resolved_style );
}

/**
*	@brief	Queue an arrow composed from a shaft plus four head edges.
**/
void vkpt_debug_draw_add_arrow( const vec3_t start, const vec3_t end, float head_length, const vkpt_debug_draw_style_t *style ) {
	if ( !vkpt_debug_draw_enabled() ) {
		return;
	}

	vkpt_debug_draw_style_t resolved_style;
	vkpt_debug_draw_resolve_style( style, &resolved_style );

	vec3_t axis;
	VectorSubtract( end, start, axis );
	vec3_t direction;
	const float axis_len = vkpt_debug_vec3_normalize( axis, direction );
	if ( axis_len <= 0.0001f ) {
		return;
	}

	const float clamped_head_len = max( 1.0f, min( head_length, axis_len * 0.75f ) );
	const float head_width = clamped_head_len * 0.5f;

	vec3_t shaft_end;
	VectorMA( end, -clamped_head_len, direction, shaft_end );
	vkpt_debug_draw_enqueue_segment( start, shaft_end, max( 0.25f, resolved_style.thickness_px * 0.5f ), &resolved_style );

	vec3_t right;
	vec3_t up;
	vkpt_debug_draw_build_basis( direction, right, up );

	vec3_t head_point;
	vkpt_debug_draw_enqueue_segment( end, shaft_end, max( 0.25f, resolved_style.thickness_px * 0.5f ), &resolved_style );
	VectorMA( shaft_end, head_width, right, head_point );
	vkpt_debug_draw_enqueue_segment( end, head_point, max( 0.25f, resolved_style.thickness_px * 0.5f ), &resolved_style );
	VectorMA( shaft_end, -head_width, right, head_point );
	vkpt_debug_draw_enqueue_segment( end, head_point, max( 0.25f, resolved_style.thickness_px * 0.5f ), &resolved_style );
	VectorMA( shaft_end, head_width, up, head_point );
	vkpt_debug_draw_enqueue_segment( end, head_point, max( 0.25f, resolved_style.thickness_px * 0.5f ), &resolved_style );
	VectorMA( shaft_end, -head_width, up, head_point );
	vkpt_debug_draw_enqueue_segment( end, head_point, max( 0.25f, resolved_style.thickness_px * 0.5f ), &resolved_style );
}

/**
*	@brief	Queue a sphere primitive.
**/
void vkpt_debug_draw_add_sphere( const vec3_t center, float radius, const vkpt_debug_draw_style_t *style ) {
	if ( !vkpt_debug_draw_enabled() ) {
		return;
	}

	vkpt_debug_draw_style_t resolved_style;
	vkpt_debug_draw_resolve_style( style, &resolved_style );
	vkpt_debug_draw_enqueue_sphere( center, max( 0.25f, radius ), &resolved_style );
}

/**
*	@brief	Queue a capsule primitive represented as a thick segment.
**/
void vkpt_debug_draw_add_capsule( const vec3_t start, const vec3_t end, float radius, const vkpt_debug_draw_style_t *style ) {
	if ( !vkpt_debug_draw_enabled() ) {
		return;
	}

	vkpt_debug_draw_style_t resolved_style;
	vkpt_debug_draw_resolve_style( style, &resolved_style );
	vkpt_debug_draw_enqueue_segment( start, end, max( 0.25f, radius ), &resolved_style );
}

/**
*	@brief	Queue a cylinder wireframe approximation.
*	@note	Approximates circles with `VKPT_DEBUG_DRAW_CYLINDER_STEPS` segments.
**/
void vkpt_debug_draw_add_cylinder( const vec3_t start, const vec3_t end, float radius, const vkpt_debug_draw_style_t *style ) {
	if ( !vkpt_debug_draw_enabled() ) {
		return;
	}

	vkpt_debug_draw_style_t resolved_style;
	vkpt_debug_draw_resolve_style( style, &resolved_style );

	vec3_t axis;
	VectorSubtract( end, start, axis );
	vec3_t direction;
	if ( vkpt_debug_vec3_normalize( axis, direction ) <= 0.0001f ) {
		return;
	}

	vec3_t right;
	vec3_t up;
	vkpt_debug_draw_build_basis( direction, right, up );

	vec3_t top_prev = { 0.0f };
	vec3_t bot_prev = { 0.0f };
	for ( int32_t i = 0; i <= VKPT_DEBUG_DRAW_CYLINDER_STEPS; i++ ) {
		const float t = (float)i / (float)VKPT_DEBUG_DRAW_CYLINDER_STEPS;
		const float angle = t * 6.28318530718f;
		const float c = cosf( angle );
		const float s = sinf( angle );

		vec3_t offset;
		offset[ 0 ] = ( right[ 0 ] * c + up[ 0 ] * s ) * radius;
		offset[ 1 ] = ( right[ 1 ] * c + up[ 1 ] * s ) * radius;
		offset[ 2 ] = ( right[ 2 ] * c + up[ 2 ] * s ) * radius;

		vec3_t top_curr;
		vec3_t bot_curr;
		VectorAdd( start, offset, top_curr );
		VectorAdd( end, offset, bot_curr );

		if ( i > 0 ) {
			vkpt_debug_draw_enqueue_segment( top_prev, top_curr, max( 0.25f, resolved_style.thickness_px * 0.5f ), &resolved_style );
			vkpt_debug_draw_enqueue_segment( bot_prev, bot_curr, max( 0.25f, resolved_style.thickness_px * 0.5f ), &resolved_style );
			if ( ( i % max( 1, VKPT_DEBUG_DRAW_CYLINDER_STEPS / 4 ) ) == 0 ) {
				vkpt_debug_draw_enqueue_segment( top_curr, bot_curr, max( 0.25f, resolved_style.thickness_px * 0.5f ), &resolved_style );
			}
		}

		VectorCopy( top_curr, top_prev );
		VectorCopy( bot_curr, bot_prev );
	}
}

/**
*	@brief	Queue an axis-aligned box wireframe.
**/
void vkpt_debug_draw_add_box( const vec3_t mins, const vec3_t maxs, const vkpt_debug_draw_style_t *style ) {
	if ( !vkpt_debug_draw_enabled() ) {
		return;
	}

	vkpt_debug_draw_style_t resolved_style;
	vkpt_debug_draw_resolve_style( style, &resolved_style );

	vec3_t vertices[ 8 ] = {
		{ mins[ 0 ], mins[ 1 ], mins[ 2 ] },
		{ maxs[ 0 ], mins[ 1 ], mins[ 2 ] },
		{ maxs[ 0 ], maxs[ 1 ], mins[ 2 ] },
		{ mins[ 0 ], maxs[ 1 ], mins[ 2 ] },
		{ mins[ 0 ], mins[ 1 ], maxs[ 2 ] },
		{ maxs[ 0 ], mins[ 1 ], maxs[ 2 ] },
		{ maxs[ 0 ], maxs[ 1 ], maxs[ 2 ] },
		{ mins[ 0 ], maxs[ 1 ], maxs[ 2 ] },
	};

	const int32_t edges[ 12 ][ 2 ] = {
		{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
		{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
		{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
	};

	for ( int32_t i = 0; i < 12; i++ ) {
		vkpt_debug_draw_enqueue_segment( vertices[ edges[ i ][ 0 ] ], vertices[ edges[ i ][ 1 ] ], max( 0.25f, resolved_style.thickness_px * 0.5f ), &resolved_style );
	}
}

/**
*	@brief	Initialize the isolated debug draw renderer resources.
*	@return	`VK_SUCCESS` on success.
*	@note	Creates per-frame-in-flight vertex buffers (host-visible/coherent).
*			The SSBO and its descriptor pool/set are gone; vertex data is submitted
*			as a plain vertex buffer from the CPU tessellation in record().
**/
VkResult vkpt_debug_draw_initialize( void ) {
	memset( debug_draw_pipelines, 0, sizeof( debug_draw_pipelines ) );
	memset( debug_draw_vertex_buffer, 0, sizeof( debug_draw_vertex_buffer ) );

	debug_draw_pipeline_layout = VK_NULL_HANDLE;
	debug_draw_render_pass     = VK_NULL_HANDLE;
	debug_draw_framebuffers    = NULL;
	debug_draw_queue_count     = 0;

	// Create the swapchain render pass for the debug overlay.
	vkpt_debug_draw_create_render_pass();

	/**
	*	Allocate one vertex buffer per frame in flight.
	*	Each buffer can hold VKPT_DEBUG_DRAW_MAX_VERTICES vertices (6 per instance).
	**/
	for ( int32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
		// Create a host-visible, coherent vertex buffer large enough for all tessellated quads.
		_VK( buffer_create( &debug_draw_vertex_buffer[ i ],
			sizeof( vkpt_debug_draw_vertex_t ) * VKPT_DEBUG_DRAW_MAX_VERTICES,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ) );
	}

	return VK_SUCCESS;
}

/**
*	@brief	Release persistent debug draw renderer resources.
*	@return	`VK_SUCCESS`.
**/
VkResult vkpt_debug_draw_destroy( void ) {
	// Destroy per-frame vertex buffers.
	for ( int32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
		buffer_destroy( &debug_draw_vertex_buffer[ i ] );
	}

	// Destroy the render pass.
	if ( debug_draw_render_pass != VK_NULL_HANDLE ) {
		vkDestroyRenderPass( qvk.device, debug_draw_render_pass, NULL );
		debug_draw_render_pass = VK_NULL_HANDLE;
	}

	return VK_SUCCESS;
}

/**
*	@brief	Create debug draw graphics pipelines and swapchain framebuffers.
*	@return	`VK_SUCCESS` when creation succeeds.
*	@note	The pipeline layout uses only the global textures descriptor set (set 0)
*			for `TEX_PT_VIEW_DEPTH_A` depth sampling. The old SSBO descriptor set is gone;
*			vertex data is now submitted as a real vertex buffer.
**/
VkResult vkpt_debug_draw_create_pipelines( void ) {
	/**
	*	Pipeline layout: set 0 = global textures (for depth texture sampling).
	*	No global UBO or SSBO bindings are needed in the new shaders.
	**/
	VkDescriptorSetLayout desc_set_layouts[] = {
		qvk.desc_set_layout_textures,
	};

	CREATE_PIPELINE_LAYOUT( qvk.device, &debug_draw_pipeline_layout,
		.setLayoutCount = LENGTH( desc_set_layouts ),
		.pSetLayouts = desc_set_layouts
	);
	ATTACH_LABEL_VARIABLE( debug_draw_pipeline_layout, PIPELINE_LAYOUT );

	// Vertex and fragment shader modules.
	VkPipelineShaderStageCreateInfo shader_info[] = {
		SHADER_STAGE( QVK_MOD_DEBUG_DRAW_VERT, VK_SHADER_STAGE_VERTEX_BIT ),
		SHADER_STAGE( QVK_MOD_DEBUG_DRAW_FRAG, VK_SHADER_STAGE_FRAGMENT_BIT ),
	};

	/**
	*	Vertex input: one binding at stride 64 (sizeof vkpt_debug_draw_vertex_t),
	*	with 8 per-vertex attributes matching the struct layout exactly.
	**/
	VkVertexInputBindingDescription vertex_binding = {
		.binding   = 0,
		.stride    = sizeof( vkpt_debug_draw_vertex_t ),
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	};

	VkVertexInputAttributeDescription vertex_attrs[] = {
		// location 0: pos  (vec4, clip-space)
		{ .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof( vkpt_debug_draw_vertex_t, pos ) },
		// location 1: color (vec4, RGBA)
		{ .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof( vkpt_debug_draw_vertex_t, color ) },
		// location 2: uv (vec2)
		{ .location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT,       .offset = offsetof( vkpt_debug_draw_vertex_t, uv ) },
		// location 3: view_depth (float)
		{ .location = 3, .binding = 0, .format = VK_FORMAT_R32_SFLOAT,          .offset = offsetof( vkpt_debug_draw_vertex_t, view_depth ) },
		// location 4: half_width_px (float)
		{ .location = 4, .binding = 0, .format = VK_FORMAT_R32_SFLOAT,          .offset = offsetof( vkpt_debug_draw_vertex_t, half_width_px ) },
		// location 5: outline_px (float)
		{ .location = 5, .binding = 0, .format = VK_FORMAT_R32_SFLOAT,          .offset = offsetof( vkpt_debug_draw_vertex_t, outline_px ) },
		// location 6: flags (int)
		{ .location = 6, .binding = 0, .format = VK_FORMAT_R32_SINT,            .offset = offsetof( vkpt_debug_draw_vertex_t, flags ) },
		// location 7: type  (int)
		{ .location = 7, .binding = 0, .format = VK_FORMAT_R32_SINT,            .offset = offsetof( vkpt_debug_draw_vertex_t, type ) },
	};

	VkPipelineVertexInputStateCreateInfo vertex_input_info = {
		.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount   = 1,
		.pVertexBindingDescriptions      = &vertex_binding,
		.vertexAttributeDescriptionCount = LENGTH( vertex_attrs ),
		.pVertexAttributeDescriptions    = vertex_attrs,
	};

	// Triangle list: each ribbon quad is 2 triangles (6 vertices).
	VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {
		.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		.primitiveRestartEnable = VK_FALSE,
	};

	VkViewport viewport = {
		.x        = 0.0f,
		.y        = 0.0f,
		.width    = (float)qvk.extent_unscaled.width,
		.height   = (float)qvk.extent_unscaled.height,
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};

	VkRect2D scissor = {
		.offset = { 0, 0 },
		.extent = qvk.extent_unscaled,
	};

	VkPipelineViewportStateCreateInfo viewport_state = {
		.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.pViewports    = &viewport,
		.scissorCount  = 1,
		.pScissors     = &scissor,
	};

	VkPipelineRasterizationStateCreateInfo rasterizer_state = {
		.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable        = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode             = VK_POLYGON_MODE_FILL,
		.lineWidth               = 1.0f,
		.cullMode                = VK_CULL_MODE_NONE,
		.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.depthBiasEnable         = VK_FALSE,
	};

	VkPipelineMultisampleStateCreateInfo multisample_state = {
		.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
	};

	VkPipelineColorBlendAttachmentState color_blend_attachment = {
		.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
		.blendEnable         = VK_TRUE,
		.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.colorBlendOp        = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.alphaBlendOp        = VK_BLEND_OP_ADD,
	};

	VkPipelineColorBlendStateCreateInfo color_blend_state = {
		.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments    = &color_blend_attachment,
	};

	VkGraphicsPipelineCreateInfo pipeline_info = {
		.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount          = LENGTH( shader_info ),
		.pStages             = shader_info,
		.pVertexInputState   = &vertex_input_info,
		.pInputAssemblyState = &input_assembly_info,
		.pViewportState      = &viewport_state,
		.pRasterizationState = &rasterizer_state,
		.pMultisampleState   = &multisample_state,
		.pDepthStencilState  = NULL,
		.pColorBlendState    = &color_blend_state,
		.layout              = debug_draw_pipeline_layout,
		.renderPass          = debug_draw_render_pass,
		.subpass             = 0,
	};

	_VK( vkCreateGraphicsPipelines( qvk.device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &debug_draw_pipelines[ VKPT_DEBUG_DRAW_PIPELINE_OVERLAY ] ) );
	ATTACH_LABEL_VARIABLE( debug_draw_pipelines[ VKPT_DEBUG_DRAW_PIPELINE_OVERLAY ], PIPELINE );

	// Create swapchain framebuffers for the debug overlay render pass.
	vkpt_debug_draw_create_framebuffers();
	return VK_SUCCESS;
}

/**
*	@brief	Destroy debug draw pipelines and framebuffer wrappers.
*	@return	`VK_SUCCESS`.
**/
VkResult vkpt_debug_draw_destroy_pipelines( void ) {
	for ( int32_t i = 0; i < VKPT_DEBUG_DRAW_NUM_PIPELINES; i++ ) {
		if ( debug_draw_pipelines[ i ] != VK_NULL_HANDLE ) {
			vkDestroyPipeline( qvk.device, debug_draw_pipelines[ i ], NULL );
			debug_draw_pipelines[ i ] = VK_NULL_HANDLE;
		}
	}

	if ( debug_draw_pipeline_layout != VK_NULL_HANDLE ) {
		vkDestroyPipelineLayout( qvk.device, debug_draw_pipeline_layout, NULL );
		debug_draw_pipeline_layout = VK_NULL_HANDLE;
	}

	if ( debug_draw_framebuffers != NULL ) {
		for ( uint32_t i = 0; i < qvk.num_swap_chain_images; i++ ) {
			vkDestroyFramebuffer( qvk.device, debug_draw_framebuffers[ i ], NULL );
		}
		free( debug_draw_framebuffers );
		debug_draw_framebuffers = NULL;
	}

	return VK_SUCCESS;
}

/**
*	@brief	Upload the current queue and draw it on top of the current frame.
*	@param	cmd_buf	Command buffer used for frame-end debug overlay rendering.
*	@return	`VK_SUCCESS`.
*	@note	Performs CPU-side VP transform, Liang-Barsky frustum clipping, and ribbon
*			quad tessellation before uploading vertices to the GPU. Each queued segment
*			or sphere becomes at most 6 vertices (2 triangles). The fragment shader then
*			handles ribbon-edge feathering, optional outline, and depth testing against
*			TEX_PT_VIEW_DEPTH_A. No SSBO or fullscreen-quad dispatch is used.
**/
VkResult vkpt_debug_draw_record( VkCommandBuffer cmd_buf ) {
	if ( !vkpt_debug_draw_enabled() ) {
		vkpt_debug_draw_clear_queue();
		return VK_SUCCESS;
	}
	if ( debug_draw_queue_count == 0 ) {
		return VK_SUCCESS;
	}

	/**
	*	Map the per-frame vertex buffer for CPU write.
	**/
	BufferResource_t *vb = &debug_draw_vertex_buffer[ qvk.current_frame_index ];
	vkpt_debug_draw_vertex_t *verts = (vkpt_debug_draw_vertex_t *)buffer_map( vb );
	// If mapping fails, drop the queue gracefully.
	if ( verts == NULL ) {
		vkpt_debug_draw_clear_queue();
		return VK_SUCCESS;
	}

	/**
	*	Fetch view (V) and projection (P) matrices from the per-frame UBO mirror.
	*	Both are column-major mat4 (float[4][4]) matching the GLSL convention.
	**/
	const QVKUniformBuffer_t *ubo = &vkpt_refdef.uniform_buffer;
	// Viewport dimensions used for NDC ↔ pixel conversion.
	const float vp_w = (float)qvk.extent_unscaled.width;
	const float vp_h = (float)qvk.extent_unscaled.height;

	/**
	*	Tessellate each queued instance into ribbon/billboard quad vertices.
	**/
	uint32_t vertex_count = 0;

	// Iterate over every queued primitive.
	for ( uint32_t i = 0; i < debug_draw_queue_count; i++ ) {
		// Guard: ensure we cannot overflow the vertex buffer.
		if ( vertex_count + 6 > VKPT_DEBUG_DRAW_MAX_VERTICES ) {
			break;
		}

		const vkpt_debug_draw_instance_t *inst = &debug_draw_queue[ i ];

		// Build homogeneous world-space points (w=1).
		const float world_a[ 4 ] = { inst->point_a[ 0 ], inst->point_a[ 1 ], inst->point_a[ 2 ], 1.0f };
		const float world_b[ 4 ] = { inst->point_b[ 0 ], inst->point_b[ 1 ], inst->point_b[ 2 ], 1.0f };

		/**
		*	Transform world positions to view space via the view matrix V.
		*	view_depth = Euclidean distance from camera (used for the per-fragment depth test).
		**/
		float view_a[ 4 ], view_b[ 4 ];
		vkpt_debug_mat4_mul_vec4( (const float *)ubo->V, world_a, view_a );
		vkpt_debug_mat4_mul_vec4( (const float *)ubo->V, world_b, view_b );
		// Compute linear view-space depth (Euclidean distance).
		const float depth_a = sqrtf( view_a[ 0 ] * view_a[ 0 ] + view_a[ 1 ] * view_a[ 1 ] + view_a[ 2 ] * view_a[ 2 ] );
		const float depth_b = sqrtf( view_b[ 0 ] * view_b[ 0 ] + view_b[ 1 ] * view_b[ 1 ] + view_b[ 2 ] * view_b[ 2 ] );

		/**
		*	Transform view-space positions to clip space via the projection matrix P.
		**/
		float clip_a[ 4 ], clip_b[ 4 ];
		vkpt_debug_mat4_mul_vec4( (const float *)ubo->P, view_a, clip_a );
		vkpt_debug_mat4_mul_vec4( (const float *)ubo->P, view_b, clip_b );

		// Read style parameters stored by the enqueue helpers.
		// params[0] = outline_px, params[1] = thickness_px.
		const float outline_px    = inst->params[ 0 ];
		const float thickness_px  = inst->params[ 1 ];
		// Total ribbon half-width includes the outline border.
		const float half_width_px = thickness_px * 0.5f + outline_px;

		if ( inst->type == VKPT_DEBUG_DRAW_PRIM_SPHERE ) {
			/**
			*	Sphere primitive: generate a screen-aligned billboard quad.
			*	The sphere center is tested against the near plane; centers behind it are dropped.
			**/

			// Use clip_a as the sphere center (point_a = point_b for spheres).
			// Skip if the center is behind the camera or before the near plane.
			if ( clip_a[ 3 ] <= 0.001f || clip_a[ 2 ] < 0.0f ) {
				continue;
			}

			// Billboard radius: match original behaviour (max of world radius and thickness).
			const float radius_px = fmaxf( inst->point_b[ 3 ], half_width_px );

			// NDC-space offsets (converted to clip space by multiplying by w).
			const float ox = radius_px * ( 2.0f / vp_w ) * clip_a[ 3 ];
			const float oy = radius_px * ( 2.0f / vp_h ) * clip_a[ 3 ];

			// Four billboard corners in clip space.
			const float clip_corners[ 4 ][ 4 ] = {
				{ clip_a[ 0 ] - ox, clip_a[ 1 ] - oy, clip_a[ 2 ], clip_a[ 3 ] }, // bottom-left
				{ clip_a[ 0 ] + ox, clip_a[ 1 ] - oy, clip_a[ 2 ], clip_a[ 3 ] }, // bottom-right
				{ clip_a[ 0 ] - ox, clip_a[ 1 ] + oy, clip_a[ 2 ], clip_a[ 3 ] }, // top-left
				{ clip_a[ 0 ] + ox, clip_a[ 1 ] + oy, clip_a[ 2 ], clip_a[ 3 ] }, // top-right
			};
			// Corresponding UV coordinates for the billboard circle test.
			const float uv_corners[ 4 ][ 2 ] = { { -1.f, -1.f }, { +1.f, -1.f }, { -1.f, +1.f }, { +1.f, +1.f } };

			// Emit two triangles: (0,1,2) and (2,1,3) — counter-clockwise winding.
			const int32_t tris[ 6 ] = { 0, 1, 2, 2, 1, 3 };
			for ( int32_t j = 0; j < 6; j++ ) {
				const int32_t vi = tris[ j ];
				vkpt_debug_draw_vertex_t *v = &verts[ vertex_count++ ];
				memcpy( v->pos,   clip_corners[ vi ], sizeof( v->pos ) );
				memcpy( v->color, inst->color,        sizeof( v->color ) );
				v->uv[ 0 ]       = uv_corners[ vi ][ 0 ];
				v->uv[ 1 ]       = uv_corners[ vi ][ 1 ];
				v->view_depth    = depth_a;
				v->half_width_px = radius_px;
				v->outline_px    = outline_px;
				v->flags         = inst->flags;
				v->type          = VKPT_DEBUG_DRAW_PRIM_SPHERE;
				v->pad           = 0;
			}
		} else {
			/**
			*	Segment/capsule primitive: clip against the view frustum, then
			*	tessellate a camera-facing ribbon quad from the clipped endpoints.
			**/

			// Clip the segment to the view frustum using Liang-Barsky in homogeneous clip space.
			float t_enter = 0.0f, t_exit = 1.0f;
			if ( !vkpt_debug_clip_segment( clip_a, clip_b, &t_enter, &t_exit ) ) {
				// Segment is entirely outside the frustum — skip it.
				continue;
			}
			// Safety: degenerate interval after clip.
			if ( t_exit <= t_enter ) {
				continue;
			}

			/**
			*	Lerp the clipped endpoints in clip space and view-depth space.
			**/
			float ca[ 4 ], cb[ 4 ];
			for ( int32_t j = 0; j < 4; j++ ) {
				ca[ j ] = clip_a[ j ] + t_enter * ( clip_b[ j ] - clip_a[ j ] );
				cb[ j ] = clip_a[ j ] + t_exit  * ( clip_b[ j ] - clip_a[ j ] );
			}
			// Interpolated view depths at the clipped endpoints.
			const float da = depth_a + t_enter * ( depth_b - depth_a );
			const float db = depth_a + t_exit  * ( depth_b - depth_a );

			// Sanity check on the homogeneous w after clipping.
			if ( ca[ 3 ] <= 0.0001f || cb[ 3 ] <= 0.0001f ) {
				continue;
			}

			/**
			*	Project clipped endpoints to NDC to compute the screen-space
			*	segment direction and derive a pixel-correct ribbon perpendicular.
			**/
			const float ndc_ax = ca[ 0 ] / ca[ 3 ];
			const float ndc_ay = ca[ 1 ] / ca[ 3 ];
			const float ndc_bx = cb[ 0 ] / cb[ 3 ];
			const float ndc_by = cb[ 1 ] / cb[ 3 ];

			// Convert direction to screen-pixel space (accounts for non-square viewports).
			const float sdx = ( ndc_bx - ndc_ax ) * ( vp_w * 0.5f );
			const float sdy = ( ndc_by - ndc_ay ) * ( vp_h * 0.5f );
			const float slen = sqrtf( sdx * sdx + sdy * sdy );

			// Perpendicular direction in screen-pixel space (rotate 90°).
			float spx, spy;
			if ( slen > 0.0001f ) {
				// Normal case: segment has non-zero screen length.
				spx = -sdy / slen;
				spy =  sdx / slen;
			} else {
				// Degenerate case: segment is nearly point-on or depth-only — default to right.
				spx = 1.0f;
				spy = 0.0f;
			}

			/**
			*	Convert pixel-space perp to NDC-space, then to clip-space offsets.
			*	ox/oy are the clip-space offsets that produce a half_width_px pixel ribbon.
			**/
			const float npx = spx * ( 2.0f / vp_w ) * half_width_px; // NDC x offset
			const float npy = spy * ( 2.0f / vp_h ) * half_width_px; // NDC y offset
			// Convert NDC offsets to clip space (multiply by w for each endpoint).
			const float ox_a = npx * ca[ 3 ];
			const float oy_a = npy * ca[ 3 ];
			const float ox_b = npx * cb[ 3 ];
			const float oy_b = npy * cb[ 3 ];

			/**
			*	Build 4 ribbon vertices:
			*   v0 = A-left  ( u=-1, v=0 )     v1 = A-right ( u=+1, v=0 )
			*   v2 = B-left  ( u=-1, v=1 )     v3 = B-right ( u=+1, v=1 )
			**/
			const float clip_v[ 4 ][ 4 ] = {
				{ ca[ 0 ] - ox_a, ca[ 1 ] - oy_a, ca[ 2 ], ca[ 3 ] }, // v0: A-left
				{ ca[ 0 ] + ox_a, ca[ 1 ] + oy_a, ca[ 2 ], ca[ 3 ] }, // v1: A-right
				{ cb[ 0 ] - ox_b, cb[ 1 ] - oy_b, cb[ 2 ], cb[ 3 ] }, // v2: B-left
				{ cb[ 0 ] + ox_b, cb[ 1 ] + oy_b, cb[ 2 ], cb[ 3 ] }, // v3: B-right
			};
			const float uv_v[ 4 ][ 2 ] = { { -1.f, 0.f }, { +1.f, 0.f }, { -1.f, 1.f }, { +1.f, 1.f } };
			// View depths at each corner (same for both corners of each endpoint).
			const float dep_v[ 4 ] = { da, da, db, db };

			// Two triangles (CCW): (v0,v1,v2) and (v2,v1,v3).
			const int32_t tris[ 6 ] = { 0, 1, 2, 2, 1, 3 };
			for ( int32_t j = 0; j < 6; j++ ) {
				const int32_t vi = tris[ j ];
				vkpt_debug_draw_vertex_t *v = &verts[ vertex_count++ ];
				memcpy( v->pos,   clip_v[ vi ], sizeof( v->pos ) );
				memcpy( v->color, inst->color,  sizeof( v->color ) );
				v->uv[ 0 ]       = uv_v[ vi ][ 0 ];
				v->uv[ 1 ]       = uv_v[ vi ][ 1 ];
				v->view_depth    = dep_v[ vi ];
				v->half_width_px = half_width_px;
				v->outline_px    = outline_px;
				v->flags         = inst->flags;
				v->type          = VKPT_DEBUG_DRAW_PRIM_SEGMENT;
				v->pad           = 0;
			}
		}
	}

	// Unmap after CPU tessellation is complete.
	buffer_unmap( vb );

	// If nothing survived clipping, bail out early without starting a render pass.
	if ( vertex_count == 0 ) {
		vkpt_debug_draw_clear_queue();
		return VK_SUCCESS;
	}

	/**
	*	Begin the swapchain render pass and issue a single draw call for all tessellated geometry.
	**/
	VkRenderPassBeginInfo render_pass_info = {
		.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass               = debug_draw_render_pass,
		.framebuffer              = debug_draw_framebuffers[ qvk.current_swap_chain_image_index ],
		.renderArea.offset        = { 0, 0 },
		.renderArea.extent        = qvk.extent_unscaled,
	};

	// Only the global textures descriptor set (set 0) is needed — for TEX_PT_VIEW_DEPTH_A.
	VkDescriptorSet desc_sets[] = {
		qvk_get_current_desc_set_textures(),
	};

	vkCmdBeginRenderPass( cmd_buf, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE );

	vkCmdBindDescriptorSets( cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
		debug_draw_pipeline_layout, 0, LENGTH( desc_sets ), desc_sets, 0, NULL );

	vkCmdBindPipeline( cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
		debug_draw_pipelines[ VKPT_DEBUG_DRAW_PIPELINE_OVERLAY ] );

	// Bind the tessellated vertex buffer and draw all triangles.
	VkDeviceSize vb_offset = 0;
	vkCmdBindVertexBuffers( cmd_buf, 0, 1, &vb->buffer, &vb_offset );
	vkCmdDraw( cmd_buf, vertex_count, 1, 0, 0 );

	vkCmdEndRenderPass( cmd_buf );

	vkpt_debug_draw_clear_queue();
	return VK_SUCCESS;
}