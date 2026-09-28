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
// Pixel shader for 2D UI and 3D text rendering with stroke, glow, and corners.
// ========================================================================== //

#version 450
#extension GL_GOOGLE_include_directive    : enable
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier    : enable

layout(constant_id = 0) const uint spec_tone_mapping_hdr = 0;

#define GLOBAL_TEXTURES_DESC_SET_IDX 1
#include "global_textures.h"
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

layout(set = 2, binding = 2, std140) uniform UBO {
	float ui_hdr_nits;
	float tm_hdr_saturation_scale;
	float screen_width;
	float screen_height;
};

// 256-byte StretchPic layout matching vertex shader and host C struct
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

layout(location = 0) in vec4 color;
layout(location = 1) in flat uint tex_id;
layout(location = 2) in vec2 tex_coord;
layout(location = 3) in flat uint v_style_flags;
layout(location = 4) in flat uint v_instance_id;
layout(location = 5) in vec2 v_local_pos;
layout(location = 6) in vec2 v_rect_size;
layout(location = 7) in float v_view_depth;

layout(location = 0) out vec4 outColor;

void main()
{
	// Linear depth occlusion check for 3D world-space text
	if ( ( v_style_flags & STYLE_FLAG_DEPTH_TEST ) != 0u && v_view_depth > 0.0 ) {
		ivec2 depth_size = textureSize( TEX_PT_VIEW_DEPTH_A, 0 );
		vec2 screen_extent = vec2( max( 1.0, screen_width ), max( 1.0, screen_height ) );
		ivec2 depth_coord = ivec2( gl_FragCoord.xy * vec2( depth_size ) / screen_extent );
		depth_coord = clamp( depth_coord, ivec2( 0 ), depth_size - ivec2( 1 ) );
		float scene_raw = texelFetch( TEX_PT_VIEW_DEPTH_A, depth_coord, 0 ).r;
		float scene_depth = abs( scene_raw );
		float depth_bias = max( 1.0, scene_depth * 0.002 );
		if ( scene_depth > 0.0 && v_view_depth > scene_depth + depth_bias ) {
			discard;
		}
	}

	// ZERO-OVERHEAD EARLY RETURN FOR UNSTYLED ELEMENTS:
	// No SSBO reads, no extra math, executes exact baseline instructions.
	if ( v_style_flags == 0u ) {
		vec4 c = color;
		if ( tex_id != ~0u ) {
			c *= global_textureLod( tex_id, tex_coord, 0 );
		}
		if ( spec_tone_mapping_hdr != 0 ) {
			c.rgb *= ui_hdr_nits / 80.0;
			c.rgb = apply_saturation_scale( c.rgb, tm_hdr_saturation_scale * 0.01 );
		}
		outColor = c;
		return;
	}

	// STYLED ELEMENT EXECUTION: Read instance style payload from SBO
	StretchPic sp = stretch_pics[ v_instance_id ];

	// BRANCH A: MTSDF TrueType font glyph or sprite silhouette distance field
	if ( ( v_style_flags & ( STYLE_FLAG_SDF_MTSDF | STYLE_FLAG_SILHOUETTE_SDF ) ) != 0u ) {
		uint sdf_id = ( sp.sdf_tex_handle != 0u ) ? sp.sdf_tex_handle : tex_id;
		vec4 samp = global_textureLod( sdf_id, tex_coord, 0 );

		float sd;
		float sd_glow;
		if ( ( v_style_flags & STYLE_FLAG_SDF_MTSDF) != 0u ) {
			// Median of RGB channels for razor-sharp corners:
			sd = max( min( samp.r, samp.g ), min( max( samp.r, samp.g ), samp.b ) );
			// True isotropic Euclidean distance in alpha channel for glows:
			sd_glow = samp.a;
		} else {
			sd = samp.r;
			sd_glow = samp.r;
		}

		float range = ( sp.sdf_pixel_range > 0.0 ) ? sp.sdf_pixel_range : 8.0;
		float dist_px = ( sd - 0.5 ) * ( 2.0 * range );
		float glow_dist_px = ( sd_glow - 0.5 ) * ( 2.0 * range );

		// Sub-pixel antialiased contour edge coverage in screen-pixel units
		float dist_screen = dist_px / max( fwidth( dist_px ), 0.0001 );
		float alpha_edge = clamp( dist_screen + 0.5, 0.0, 1.0 );

		vec4 base_color = color;
		if ( tex_id != ~0u && ( v_style_flags & STYLE_FLAG_SDF_MTSDF ) == 0u ) {
			base_color *= global_textureLod( tex_id, tex_coord, 0 );
		}
		base_color.a *= alpha_edge;

		vec4 accum_color = base_color;

		// Outer Glow for distance field
		if ( ( v_style_flags & STYLE_FLAG_OUTER_GLOW ) != 0u ) {
			float glow_radius = sp.outer_glow_radius.x;
			if ( glow_radius > 0.0 && glow_dist_px < 0.0 ) {
				float glow_t = clamp( -glow_dist_px / glow_radius, 0.0, 1.0 );
				float falloff = ( ( v_style_flags & STYLE_FLAG_GLOW_FALLOFF_EXP ) != 0u )
					? exp( -3.0 * glow_t )
					: ( 1.0 - smoothstep( 0.0, 1.0, glow_t ) );

				vec4 glow_col = unpackUnorm4x8( sp.outer_glow_colors.x );
				glow_col = pow( glow_col, vec4( 2.4 ) );
				float glow_alpha = glow_col.a * falloff * ( 1.0 - alpha_edge );

				if ( ( v_style_flags & STYLE_FLAG_GLOW_BLEND_ADDITIVE ) != 0u ) {
					accum_color.rgb += glow_col.rgb * glow_alpha;
				} else {
					accum_color.rgb = mix( glow_col.rgb, accum_color.rgb, accum_color.a );
					accum_color.a = max( accum_color.a, glow_alpha );
				}
			}
		}

		// Stroke Outline for distance field
		if ( ( v_style_flags & STYLE_FLAG_OUTLINE ) != 0u ) {
			float stroke_w = sp.stroke_thickness.x;
			if ( stroke_w > 0.0 ) {
				float stroke_dist = abs( dist_px ) - ( stroke_w * 0.5 );
				float stroke_dist_screen = stroke_dist / max( fwidth( stroke_dist ), 0.0001 );
				float stroke_alpha = 1.0 - clamp( stroke_dist_screen + 0.5, 0.0, 1.0 );
				vec4 stroke_col = unpackUnorm4x8( sp.stroke_colors.x );
				stroke_col = pow( stroke_col, vec4( 2.4 ) );
				accum_color = mix( accum_color, stroke_col, stroke_alpha * stroke_col.a );
			}
		}

		if ( spec_tone_mapping_hdr != 0 ) {
			accum_color.rgb *= ui_hdr_nits / 80.0;
			accum_color.rgb = apply_saturation_scale( accum_color.rgb, tm_hdr_saturation_scale * 0.01 );
		}
		outColor = accum_color;
		return;
	}

	// BRANCH B: Analytic 2D Rounded Rectangle, Lines, Panels, and Border Geometry
	vec2 W = v_rect_size;
	vec2 p = v_local_pos; // Local coordinate where [0, W.x] x [0, W.y] is original rectangle
	vec2 b = W * 0.5;
	vec2 q = p - b;

	// Determine corner quadrant and fetch corresponding corner radius
	// Quadrants: TL (q.x < 0, q.y < 0), TR (q.x >= 0, q.y < 0), BR (q.x >= 0, q.y >= 0), BL (q.x < 0, q.y >= 0)
	float r = 0.0;
	int corner_idx = 0;
	if ( q.x < 0.0 && q.y < 0.0 ) {
		r = sp.corner_radii.x; // TL
		corner_idx = 0;
	} else if ( q.x >= 0.0 && q.y < 0.0 ) {
		r = sp.corner_radii.y; // TR
		corner_idx = 1;
	} else if ( q.x >= 0.0 && q.y >= 0.0 ) {
		r = sp.corner_radii.z; // BR
		corner_idx = 2;
	} else {
		r = sp.corner_radii.w; // BL
		corner_idx = 3;
	}
	r = min( r, min( b.x, b.y ) );

	// Analytic signed distance to rounded box:
	// d < 0 inside box, d = 0 on contour, d > 0 outside box
	vec2 d_vec = abs( q ) - b + vec2( r );
	float d = length( max( d_vec, vec2( 0.0 ) ) ) + min( max( d_vec.x, d_vec.y ), 0.0 ) - r;

	// Sub-pixel antialiased coverage for the interior fill:
	float inner_coverage = 1.0 - smoothstep( -0.5, +0.5, d );

	// Per-edge Voronoi distance calculation:
	// Distances to top, right, bottom, left borders
	float d_T = p.y;
	float d_R = W.x - p.x;
	float d_B = W.y - p.y;
	float d_L = p.x;

	// Voronoi edge selector with exact 45-degree miter corner joints
	int edge_idx = 0;
	float min_d = d_T;
	if ( d_R < min_d ) { min_d = d_R; edge_idx = 1; }
	if ( d_B < min_d ) { min_d = d_B; edge_idx = 2; }
	if ( d_L < min_d ) { min_d = d_L; edge_idx = 3; }

	// Stroke parameters for dominant edge
	float stroke_w = ( edge_idx == 0 ) ? sp.stroke_thickness.x :
	                 ( edge_idx == 1 ) ? sp.stroke_thickness.y :
	                 ( edge_idx == 2 ) ? sp.stroke_thickness.z : sp.stroke_thickness.w;

	uint stroke_u32 = ( edge_idx == 0 ) ? sp.stroke_colors.x :
	                  ( edge_idx == 1 ) ? sp.stroke_colors.y :
	                  ( edge_idx == 2 ) ? sp.stroke_colors.z : sp.stroke_colors.w;

	vec4 stroke_col = unpackUnorm4x8( stroke_u32 );
	stroke_col = pow( stroke_col, vec4( 2.4 ) );

	// Base interior color
	vec4 base_col = color;
	if ( tex_id != ~0u ) {
		base_col *= global_textureLod( tex_id, tex_coord, 0 );
	}
	base_col.a *= inner_coverage;

	vec4 result_col = base_col;

	// INNER GLOW: Evaluates inside the element contour (d < 0)
	if ( ( v_style_flags & STYLE_FLAG_INNER_GLOW ) != 0u && d < 0.0 ) {
		float in_r = ( edge_idx == 0 ) ? sp.inner_glow_radius.x :
		             ( edge_idx == 1 ) ? sp.inner_glow_radius.y :
		             ( edge_idx == 2 ) ? sp.inner_glow_radius.z : sp.inner_glow_radius.w;

		if ( in_r > 0.0 ) {
			float in_t = clamp( -d / in_r, 0.0, 1.0 );
			float in_falloff = ( ( v_style_flags & STYLE_FLAG_GLOW_FALLOFF_EXP ) != 0u )
				? exp( -3.0 * in_t )
				: ( 1.0 - smoothstep( 0.0, 1.0, in_t ) );

			uint in_glow_u32 = ( edge_idx == 0 ) ? sp.inner_glow_colors.x :
			                   ( edge_idx == 1 ) ? sp.inner_glow_colors.y :
			                   ( edge_idx == 2 ) ? sp.inner_glow_colors.z : sp.inner_glow_colors.w;

			vec4 in_glow_col = unpackUnorm4x8( in_glow_u32 );
			in_glow_col = pow( in_glow_col, vec4( 2.4 ) );
			float in_alpha = in_glow_col.a * in_falloff * inner_coverage;

			if ( ( v_style_flags & STYLE_FLAG_GLOW_BLEND_ADDITIVE ) != 0u ) {
				result_col.rgb += in_glow_col.rgb * in_alpha;
			} else {
				result_col.rgb = mix( result_col.rgb, in_glow_col.rgb, in_alpha );
			}
		}
	}

	// OUTER GLOW: Evaluates outside the element contour (d > 0)
	if ( ( v_style_flags & STYLE_FLAG_OUTER_GLOW ) != 0u && d > -0.5 ) {
		vec4 glow_col;
		float glow_radius = 0.0;
		bool edge_active = false;

		// Corner quadrant with angular slerp between adjacent edges:
		if ( d_vec.x > 0.0 && d_vec.y > 0.0 ) {
			float theta = atan( d_vec.y, d_vec.x ); // in [0, pi/2]
			float k = theta / ( 0.5 * M_PI );

			vec4 c_adj1, c_adj2;
			float r_adj1, r_adj2;
			bool act1, act2;

			if ( corner_idx == 0 ) {
				// TL: Left (3) -> Top (0)
				c_adj1 = unpackUnorm4x8( sp.outer_glow_colors.w );
				c_adj2 = unpackUnorm4x8( sp.outer_glow_colors.x );
				r_adj1 = sp.outer_glow_radius.w;
				r_adj2 = sp.outer_glow_radius.x;
				act1 = ( ( sp.style_flags & STYLE_FLAG_EDGE_LEFT ) != 0u );
				act2 = ( ( sp.style_flags & STYLE_FLAG_EDGE_TOP ) != 0u );
			} else if ( corner_idx == 1 ) {
				// TR: Top (0) -> Right (1)
				c_adj1 = unpackUnorm4x8( sp.outer_glow_colors.x );
				c_adj2 = unpackUnorm4x8( sp.outer_glow_colors.y );
				r_adj1 = sp.outer_glow_radius.x;
				r_adj2 = sp.outer_glow_radius.y;
				act1 = ( ( sp.style_flags & STYLE_FLAG_EDGE_TOP ) != 0u );
				act2 = ( ( sp.style_flags & STYLE_FLAG_EDGE_RIGHT ) != 0u );
			} else if ( corner_idx == 2 ) {
				// BR: Right (1) -> Bottom (2)
				c_adj1 = unpackUnorm4x8( sp.outer_glow_colors.y );
				c_adj2 = unpackUnorm4x8( sp.outer_glow_colors.z );
				r_adj1 = sp.outer_glow_radius.y;
				r_adj2 = sp.outer_glow_radius.z;
				act1 = ( ( sp.style_flags & STYLE_FLAG_EDGE_RIGHT ) != 0u );
				act2 = ( ( sp.style_flags & STYLE_FLAG_EDGE_BOTTOM ) != 0u );
			} else {
				// BL: Bottom (2) -> Left (3)
				c_adj1 = unpackUnorm4x8( sp.outer_glow_colors.z );
				c_adj2 = unpackUnorm4x8( sp.outer_glow_colors.w );
				r_adj1 = sp.outer_glow_radius.z;
				r_adj2 = sp.outer_glow_radius.w;
				act1 = ( ( sp.style_flags & STYLE_FLAG_EDGE_BOTTOM ) != 0u );
				act2 = ( ( sp.style_flags & STYLE_FLAG_EDGE_LEFT ) != 0u );
			}

			c_adj1 = pow( c_adj1, vec4( 2.4 ) );
			c_adj2 = pow( c_adj2, vec4( 2.4 ) );
			glow_col = mix( c_adj1, c_adj2, k );
			glow_radius = mix( r_adj1, r_adj2, k );
			edge_active = ( ( act1 && ( 1.0 - k > 0.05 ) ) || ( act2 && ( k > 0.05 ) ) );
		} else {
			// Straight edge:
			uint out_glow_u32 = ( edge_idx == 0 ) ? sp.outer_glow_colors.x :
			                    ( edge_idx == 1 ) ? sp.outer_glow_colors.y :
			                    ( edge_idx == 2 ) ? sp.outer_glow_colors.z : sp.outer_glow_colors.w;

			glow_col = unpackUnorm4x8( out_glow_u32 );
			glow_col = pow( glow_col, vec4( 2.4 ) );

			glow_radius = ( edge_idx == 0 ) ? sp.outer_glow_radius.x :
			              ( edge_idx == 1 ) ? sp.outer_glow_radius.y :
			              ( edge_idx == 2 ) ? sp.outer_glow_radius.z : sp.outer_glow_radius.w;

			edge_active = ( edge_idx == 0 ) ? ( ( sp.style_flags & STYLE_FLAG_EDGE_TOP ) != 0u ) :
			              ( edge_idx == 1 ) ? ( ( sp.style_flags & STYLE_FLAG_EDGE_RIGHT ) != 0u ) :
			              ( edge_idx == 2 ) ? ( ( sp.style_flags & STYLE_FLAG_EDGE_BOTTOM ) != 0u ) :
			                                  ( ( sp.style_flags & STYLE_FLAG_EDGE_LEFT ) != 0u );
		}

		if ( edge_active && glow_radius > 0.0 && d < glow_radius ) {
			float glow_t = clamp( max( 0.0, d ) / glow_radius, 0.0, 1.0 );
			float glow_falloff = ( ( v_style_flags & STYLE_FLAG_GLOW_FALLOFF_EXP ) != 0u )
				? exp( -3.0 * glow_t )
				: ( 1.0 - smoothstep( 0.0, 1.0, glow_t ) );

			float glow_alpha = glow_col.a * glow_falloff * ( 1.0 - inner_coverage );

			if ( ( v_style_flags & STYLE_FLAG_GLOW_BLEND_ADDITIVE ) != 0u ) {
				result_col.rgb += glow_col.rgb * glow_alpha;
			} else {
				result_col.rgb = mix( glow_col.rgb, result_col.rgb, result_col.a );
				result_col.a = max( result_col.a, glow_alpha );
			}
		}
	}

	// STROKE OUTLINE: Evaluates along the contour with alignment offset
	if ( ( v_style_flags & STYLE_FLAG_OUTLINE ) != 0u && stroke_w > 0.0 ) {
		float align_offset = 0.0; // Centered default [-W/2, +W/2]
		if ( ( sp.style_flags & STYLE_FLAG_STROKE_ALIGN_OUTSET ) != 0u ) {
			align_offset = stroke_w * 0.5;
		} else if ( ( sp.style_flags & STYLE_FLAG_STROKE_ALIGN_INSET ) != 0u ) {
			align_offset = -stroke_w * 0.5;
		}

		float stroke_delta = abs( d - align_offset ) - ( stroke_w * 0.5 );
		float stroke_coverage = 1.0 - smoothstep( -0.5, +0.5, stroke_delta );

		result_col = mix( result_col, stroke_col, stroke_coverage * stroke_col.a );
	}

	// If fragment ended up completely transparent outside the element, discard
	if ( result_col.a <= 0.001 ) {
		discard;
	}

	// Apply HDR tonemapping saturation and brightness scaling if active
	if ( spec_tone_mapping_hdr != 0 ) {
		result_col.rgb *= ui_hdr_nits / 80.0;
		result_col.rgb = apply_saturation_scale( result_col.rgb, tm_hdr_saturation_scale * 0.01 );
	}

	outColor = result_col;
}
