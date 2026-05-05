#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

// ---------------------------------------------------------------------------
// debug_draw.frag
//
// Fragment shader for the 3D debug overlay.
//
// The CPU tessellates each primitive into real ribbon/billboard quad geometry
// in clip space before uploading.  This shader receives per-vertex varyings
// (no SSBO reads, no world-space projection) and is responsible for:
//
//   1. Ribbon edge feathering  — smoothstep alpha falloff at u = ±1.
//   2. Optional outline border — dark band near the edge when outline_px > 0.
//   3. Sphere circle clip      — discard outside the unit circle for spheres.
//   4. Optional depth test     — compare interpolated view depth against the
//                                scene path-tracer depth (TEX_PT_VIEW_DEPTH_A)
//                                when VKPT_DEBUG_DRAW_FLAG_DEPTH_TEST is set.
//
// Descriptor set 0 = global textures (for TEX_PT_VIEW_DEPTH_A depth sampling).
// No global UBO binding is required — near/far constants are inlined.
// ---------------------------------------------------------------------------

// Use the global textures descriptor set at binding set 0.
#define GLOBAL_TEXTURES_DESC_SET_IDX 0
#include "../shader/global_textures.h"

// ---------------------------------------------------------------------------
// Per-fragment inputs (match debug_draw.vert outputs).
// ---------------------------------------------------------------------------
layout( location = 0 ) in vec4       v_color;
layout( location = 1 ) in vec2       v_uv;           //!< u=cross-axis [-1,+1], v=along-axis [0,1].
layout( location = 2 ) in float      v_view_depth;   //!< Interpolated linearised view-space depth.
layout( location = 3 ) in float      v_half_width_px;//!< Half ribbon/sphere width in pixels.
layout( location = 4 ) in float      v_outline_px;   //!< Outline border in pixels.
layout( location = 5 ) in flat int   v_flags;
layout( location = 6 ) in flat int   v_type;

layout( location = 0 ) out vec4 out_color;

// ---------------------------------------------------------------------------
// Constants.
// ---------------------------------------------------------------------------
const int DEBUG_DRAW_PRIM_SEGMENT  = 0;
const int DEBUG_DRAW_PRIM_SPHERE   = 1;
const int DEBUG_DRAW_FLAG_DEPTH_TEST = 1;

// ---------------------------------------------------------------------------
// Helpers.
// ---------------------------------------------------------------------------

/**
*   Sample the scene path-tracer depth texture at the integer pixel coordinate
*   of the current fragment and return the raw half-float depth value.
*   This texture stores linear view distance directly (not NDC depth).
**/
float sample_scene_depth( ivec2 pixel ) {
    return texelFetch( TEX_PT_VIEW_DEPTH_A, pixel, 0 ).r;
}

// ---------------------------------------------------------------------------
// Main.
// ---------------------------------------------------------------------------
void main() {
    /**
    *   Compute the pixel coordinate for depth texture sampling.
    *   gl_FragCoord.xy gives the exact pixel centre in window space.
    **/
    ivec2 pixel = ivec2( gl_FragCoord.xy );

    /**
    *   Per-type alpha and outline computation.
    *
    *   For SEGMENT ribbons:
    *     v_uv.x is the signed cross-axis position in [-1, +1].
    *     The outer edge of the ribbon (including outline) is at |u| = 1.
    *     edge_dist_px measures how many pixels we are away from that outer edge.
    *
    *   For SPHERE billboards:
    *     v_uv.xy is a 2D position in the unit square [-1,1]×[-1,1].
    *     We clip to the unit circle and feather the edge.
    **/
    float alpha      = 1.0;
    bool  in_outline = false;

    if ( v_type == DEBUG_DRAW_PRIM_SPHERE ) {
        // Radial distance from billboard centre in UV space.
        float dist_uv = length( v_uv );
        // Pixel distance from the outer edge of the sphere circle.
        float edge_dist_px = ( 1.0 - dist_uv ) * v_half_width_px;
        // Sphere ring thickness (in pixels) provided by the CPU path.
        float ring_width_px = max( 1.0, v_outline_px );

        // Discard fragments completely outside the sphere circle.
        if ( edge_dist_px < -0.5 ) {
            discard;
        }

        // Discard interior pixels so spheres render as wireframe rings.
        if ( edge_dist_px > ring_width_px + 1.5 ) {
            discard;
        }

        // Anti-aliased ring: smooth at the outside and inside edges.
        float outer_alpha = smoothstep( 0.0, 1.5, edge_dist_px );
        float inner_fade = smoothstep( ring_width_px, ring_width_px + 1.5, edge_dist_px );
        alpha = outer_alpha * ( 1.0 - inner_fade );
    } else {
        // Signed cross-axis distance from ribbon centre in pixels.
        float cross_dist_px = abs( v_uv.x ) * v_half_width_px;
        // Pixel distance from the outer edge.
        float edge_dist_px  = v_half_width_px - cross_dist_px;

        // Discard fragments outside the ribbon extents.
        if ( edge_dist_px < -0.5 ) {
            discard;
        }

        // Smooth feathering at the ribbon boundary (1-pixel anti-alias band).
        alpha = smoothstep( 0.0, 1.5, edge_dist_px );

        // Outline zone: within outline_px pixels of the outer edge.
        if ( v_outline_px > 0.0 && edge_dist_px < v_outline_px ) {
            in_outline = true;
        }
    }

    /**
    *   Resolve the final colour — outline overrides to solid black.
    **/
    vec4 color = v_color;
    if ( in_outline ) {
        // Solid dark outline regardless of primitive colour.
        color.rgb = vec3( 0.0 );
        color.a   = max( color.a, 1.0 );
    }

    /**
    *   Optional per-fragment depth test against TEX_PT_VIEW_DEPTH_A.
    *   v_view_depth is the interpolated linearised view depth computed on the CPU
    *   and stored per-vertex, so it correctly tracks position along the ribbon.
    **/
    if ( ( v_flags & DEBUG_DRAW_FLAG_DEPTH_TEST ) != 0 ) {
        float scene_raw = sample_scene_depth( pixel );
        // Reflection/refraction passes can store negative depth markers; compare by magnitude.
        float scene_view_depth = abs( scene_raw );
        // Use a small distance-scaled bias to reduce edge sparkles at grazing angles.
        float depth_bias = max( 1.0, scene_view_depth * 0.002 );
        if ( scene_view_depth > 0.0 && v_view_depth > scene_view_depth + depth_bias ) {
            discard;
        }
    }

    // Output the final blended colour.
    out_color = vec4( color.rgb, color.a * alpha );
}
