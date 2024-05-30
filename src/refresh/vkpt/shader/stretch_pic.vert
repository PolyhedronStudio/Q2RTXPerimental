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
// Vertex shader for UI rendering.
// The engine accumulates all UI rendering tasks in an array of StretchPic
// structures, which are passed to this shader. See `src/refresh/vkpt/draw.c`
// for more information.
// ========================================================================== //

#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable

#include "utils.glsl"

out gl_PerVertex {
	vec4 gl_Position;
};

layout(location = 0) out vec4 color;
layout(location = 1) out flat uint tex_id;
layout(location = 2) out vec2 tex_coord;

struct StretchPic {
	float x, y;
	float w, h;
	float s, t;
	float w_s, h_t;
	uint color, tex_handle;
	float	pivot_x, pivot_y;
	float	angle, pad01;
	float	pad02, pad03;
	mat4 matTransform;
};

layout(set = 0, binding = 0) buffer SBO {
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
	// Create 2D rotation matrix.
	mat2 m = mat2( c, s, -s, c );
	// Rotate around ( v - pivot ) and then 
	// translate back to ( rotated_v + pivot ).
	return ( m * ( v - pivot ) ) + pivot;
}

void
main()
{
	// Acquire stretch pic structure instance reference we're processing.
	StretchPic sp = stretch_pics[ gl_InstanceIndex ];
	
	// Position is model-space, so the quad vertices stored in positions are first fetched.
	vec2 pos = positions[ gl_VertexIndex ];
	// Get pivot, convert to model space.
	vec2 model_space_pivot = vec2(
		( 1.0 / sp.w ) * sp.pivot_x,
		( 1.0 / sp.h ) * sp.pivot_y
	);
	// Rotate 'pos' around the pivot in 'model' space.
	pos = rotate( pos, model_space_pivot, sp.angle );
	// Then scaled from their unit normal up to the specified width and height
	// defining our screen-space 'model' space.
	pos *= vec2(sp.w, sp.h);
	// Translate to desired screen-space position.
	pos += vec2(sp.x, sp.y);

	// Acquire color to use.
	color = unpackUnorm4x8(sp.color);
	color = pow(color, vec4(2.4));

	// Setup texture information.
	tex_coord	= vec2(sp.s, sp.t) + positions[gl_VertexIndex] * vec2(sp.w_s, sp.h_t);
	tex_id		= sp.tex_handle;

	// Output the final screen-space vertex position.
	gl_Position = sp.matTransform * vec4(pos, 1.0, 1.0);
}

