/*
Copyright (C) 2018 Christoph Schied

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

// ========================================================================== //
// Vertex shader for 2D UI and 3D text rendering with stroke & glow expansion.
// The engine accumulates all UI rendering tasks in an array of StretchPic
// structures, which are passed to this shader via SSBO Set 0 Binding 0.
// ========================================================================== //

#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable

#include "utils.glsl"

const uint STYLE_FLAG_NONE                   = 0u;
const uint STYLE_FLAG_OUTLINE                = (1u << 0);
const uint STYLE_FLAG_OUTER_GLOW             = (1u << 1);
const uint STYLE_FLAG_INNER_GLOW             = (1u << 2);
const uint STYLE_FLAG_SILHOUETTE_SDF         = (1u << 3);
const uint STYLE_FLAG_SDF_MTSDF              = (1u << 4);

const uint STYLE_FLAG_STROKE_ALIGN_CENTER    = 0u;
const uint STYLE_FLAG_STROKE_ALIGN_INSET     = (1u << 5);
const uint STYLE_FLAG_STROKE_ALIGN_OUTSET    = (1u << 6);
const uint STYLE_FLAG_STROKE_ALIGN_MASK      = ((1u << 5) | (1u << 6));

const uint STYLE_FLAG_GLOW_FALLOFF_HERMITE   = 0u;
const uint STYLE_FLAG_GLOW_FALLOFF_EXP       = (1u << 7);
const uint STYLE_FLAG_GLOW_BLEND_ADDITIVE    = (1u << 8);

const uint STYLE_FLAG_EDGE_TOP               = (1u << 9);
const uint STYLE_FLAG_EDGE_RIGHT             = (1u << 10);
const uint STYLE_FLAG_EDGE_BOTTOM            = (1u << 11);
const uint STYLE_FLAG_EDGE_LEFT              = (1u << 12);
const uint STYLE_FLAG_EDGE_ALL               = ((1u << 9) | (1u << 10) | (1u << 11) | (1u << 12));

const uint STYLE_FLAG_PRIMITIVE_LINE2D       = (1u << 13);
const uint STYLE_FLAG_CORNER_RADIUS          = (1u << 14);
const uint STYLE_FLAG_DEPTH_TEST             = (1u << 15);

out gl_PerVertex {
	vec4 gl_Position;
};

layout(location = 0) out vec4 color;
layout(location = 1) out flat uint tex_id;
layout(location = 2) out vec2 tex_coord;
layout(location = 3) out flat uint v_style_flags;
layout(location = 4) out flat uint v_instance_id;
layout(location = 5) out vec2 v_local_pos;
layout(location = 6) out vec2 v_rect_size;
layout(location = 7) out float v_view_depth;

// 256-byte StretchPic payload, perfectly aligned to 16 bytes for std430.
struct StretchPic {
	float x, y;
	float w, h;
	float s, t;
	float w_s, h_t;
	uint color, tex_handle;
	float pivot_x, pivot_y;
	float angle, view_depth;
	float pad02, pad03;
	mat4 matTransform;

	// Extended Style Payload (128 bytes):
	uvec4 stroke_colors;      // Top, Right, Bottom, Left
	vec4  stroke_thickness;   // Top, Right, Bottom, Left
	uvec4 outer_glow_colors;  // Top, Right, Bottom, Left
	vec4  outer_glow_radius;  // Top, Right, Bottom, Left
	uvec4 inner_glow_colors;  // Top, Right, Bottom, Left
	vec4  inner_glow_radius;  // Top, Right, Bottom, Left
	vec4  corner_radii;       // TL, TR, BR, BL
	uint  style_flags;
	uint  sdf_tex_handle;
	float sdf_pixel_range;
	float pad_style;
};

layout(set = 0, binding = 0, std430) readonly buffer SBO {
	StretchPic stretch_pics[];
};

vec2 positions[4] = vec2[](
	vec2(0.0, 1.0),
	vec2(0.0, 0.0),
	vec2(1.0, 1.0),
	vec2(1.0, 0.0)
);

vec2 rotate(vec2 v, vec2 pivot, float a) {
	float rangle = -radians( a );
	float s = sin( rangle );
	float c = cos( rangle );
	mat2 m = mat2( c, s, -s, c );
	return ( m * ( v - pivot ) ) + pivot;
}

void main()
{
	// Acquire stretch pic structure instance reference we're processing.
	StretchPic sp = stretch_pics[ gl_InstanceIndex ];

	// Pass instance index and style flags to the fragment shader.
	v_instance_id = gl_InstanceIndex;
	v_style_flags = sp.style_flags;
	v_view_depth  = sp.view_depth;
	v_rect_size   = vec2( sp.w, sp.h );

	// Unit quad vertex in [0, 1] x [0, 1]
	vec2 u = positions[ gl_VertexIndex ];

	// Acquire color to use.
	color = unpackUnorm4x8( sp.color );
	color = pow( color, vec4( 2.4 ) );

	tex_id = sp.tex_handle;

	// Branch: 3D perspective projection vs 2D screen projection
	if ( sp.view_depth > 0.0 ) {
		// 3D Text: Quad is positioned and oriented by sp.matTransform (MVP * CharMatrix).
		// Unit position is mapped directly into clip space.
		v_local_pos = u * vec2( sp.w, sp.h );
		tex_coord   = vec2( sp.s, sp.t ) + u * vec2( sp.w_s, sp.h_t );
		gl_Position = sp.matTransform * vec4( u, 0.0, 1.0 );
		return;
	}

	// 2D UI / Rectangles / Shapes / Lines / MTSDF Distance Field Glyphs
	if ( sp.style_flags == 0u || ( sp.style_flags & STYLE_FLAG_SDF_MTSDF ) != 0u ) {
		// ZERO OVERHEAD PATH / DISTANCE FIELD FONT PATH:
		// MTSDF font glyphs already contain baked distance padding within their atlas cells;
		// quad vertices and atlas UV coordinates must not be expanded outward to prevent sampling
		// neighboring glyphs in the packed atlas.
		vec2 model_space_pivot = vec2(
			( 1.0 / sp.w ) * sp.pivot_x,
			( 1.0 / sp.h ) * sp.pivot_y
		);
		vec2 pos = rotate( u, model_space_pivot, sp.angle );
		pos *= vec2( sp.w, sp.h );
		pos += vec2( sp.x, sp.y );

		v_local_pos = u * vec2( sp.w, sp.h );
		tex_coord   = vec2( sp.s, sp.t ) + u * vec2( sp.w_s, sp.h_t );
		gl_Position = sp.matTransform * vec4( pos, 1.0, 1.0 );
		return;
	}

	// STYLED PATH: Compute asymmetric quad expansion margins (Top, Right, Bottom, Left)
	// Outer glow margins:
	vec4 G = vec4( 0.0 );
	if ( ( sp.style_flags & STYLE_FLAG_OUTER_GLOW ) != 0u ) {
		if ( ( sp.style_flags & STYLE_FLAG_EDGE_TOP ) != 0u )    { G.x = max( 0.0, sp.outer_glow_radius.x ); }
		if ( ( sp.style_flags & STYLE_FLAG_EDGE_RIGHT ) != 0u )  { G.y = max( 0.0, sp.outer_glow_radius.y ); }
		if ( ( sp.style_flags & STYLE_FLAG_EDGE_BOTTOM ) != 0u ) { G.z = max( 0.0, sp.outer_glow_radius.z ); }
		if ( ( sp.style_flags & STYLE_FLAG_EDGE_LEFT ) != 0u )   { G.w = max( 0.0, sp.outer_glow_radius.w ); }
	}

	// Stroke outline margins:
	vec4 S = vec4( 0.0 );
	if ( ( sp.style_flags & STYLE_FLAG_OUTLINE ) != 0u ) {
		float align_scale = 0.5; // Centered default [-W/2, +W/2]
		if ( ( sp.style_flags & STYLE_FLAG_STROKE_ALIGN_OUTSET ) != 0u ) {
			align_scale = 1.0;
		} else if ( ( sp.style_flags & STYLE_FLAG_STROKE_ALIGN_INSET ) != 0u ) {
			align_scale = 0.0;
		}
		S = max( vec4( 0.0 ), sp.stroke_thickness * align_scale );
	}

	// Total margin per edge: M = max(Glow, Stroke) -> (Top, Right, Bottom, Left)
	vec4 M = max( G, S );

	// Asymmetrically expanded local coordinate:
	// x in [-M_left, w + M_right], y in [-M_top, h + M_bottom]
	vec2 local_pos;
	local_pos.x = u.x * ( sp.w + M.w + M.y ) - M.w;
	local_pos.y = u.y * ( sp.h + M.x + M.z ) - M.x;

	v_local_pos = local_pos;

	// Rotate around pivot in unexpanded coordinates
	vec2 pos = rotate( local_pos, vec2( sp.pivot_x, sp.pivot_y ), sp.angle );
	pos += vec2( sp.x, sp.y );

	// Base UVs map smoothly to original [0, w] x [0, h] space
	vec2 uv_norm = local_pos / vec2( sp.w, sp.h );
	tex_coord    = vec2( sp.s, sp.t ) + uv_norm * vec2( sp.w_s, sp.h_t );

	gl_Position = sp.matTransform * vec4( pos, 1.0, 1.0 );
}
