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
	float x, y, w,   h;
	float s, t, w_s, h_t;
	float angle, pivot_x, pivot_y;
	uint color, tex_handle;
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

void
main()
{
	// Acquire stretch pic structure instance reference we're processing.
	StretchPic sp = stretch_pics[ gl_InstanceIndex ];
	// Position is screen-space, so the quad vertices stored in positions are first fetched.
	vec2 pos      = positions[ gl_VertexIndex ];
	// Then scaled from their unit normal up to the specified width and height.
	pos *= vec2(sp.w, sp.h);
	// And then translated to the specified x and y coordinates.
	pos += vec2(sp.x, sp.y);

	//
	// Rotating (ALMOST WORKS)
	//

	// Pivot point.
	vec2 pivot = vec2(sp.pivot_x, sp.pivot_y);
	// Offset by the pivot point out of reference frame.	
	pos -= pivot;

	// Rotate the position.
	float s = sin( sp.angle );
	float c = sin( sp.angle );
	pos.x += (pos.x * c - pos.y * s);// + sp.x;
	pos.y += (pos.x * s + pos.y * c);// + sp.y;
    //*(dst_vert + 0) = (vert_x * c - vert_y * s) + x;
    //*(dst_vert + 1) = (vert_x * s + vert_y * c) + y;
	// Translate it back into its old frame of reference origin.
	pos += pivot;
	//
	//
	//

	// Acquire color to use.
	color         = unpackUnorm4x8(sp.color);
	color = pow(color, vec4(2.4));

	// Setup texture information.
	tex_coord     = vec2(sp.s, sp.t) + positions[gl_VertexIndex] * vec2(sp.w_s, sp.h_t);
	tex_id        = sp.tex_handle;

	// Output the final screen-space vertex position.
	gl_Position = vec4(pos, 0.0, 1.0);
}

