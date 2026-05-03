#version 450
#extension GL_ARB_separate_shader_objects : enable

// ---------------------------------------------------------------------------
// debug_draw.vert
//
// Vertex shader for the 3D debug overlay.
//
// The CPU tessellates each queued primitive (segment / sphere) into ribbon or
// billboard quad vertices in Vulkan clip space BEFORE uploading them.  This
// shader therefore does no transformation at all — it simply passes the
// pre-computed clip-space position straight to gl_Position and forwards the
// remaining per-vertex attributes to the fragment shader as varyings.
// ---------------------------------------------------------------------------

// Per-vertex inputs (match vkpt_debug_draw_vertex_t in debug_draw.c).
layout( location = 0 ) in vec4  in_pos;           //!< Pre-computed Vulkan clip-space position.
layout( location = 1 ) in vec4  in_color;          //!< RGBA colour.
layout( location = 2 ) in vec2  in_uv;             //!< Ribbon UV: u=cross-axis [-1,+1], v=along-axis [0,1].
layout( location = 3 ) in float in_view_depth;     //!< Linearised view-space depth for depth comparison.
layout( location = 4 ) in float in_half_width_px;  //!< Half ribbon width in pixels (thickness/2 + outline).
layout( location = 5 ) in float in_outline_px;     //!< Outline border width in pixels.
layout( location = 6 ) in int   in_flags;          //!< VKPT_DEBUG_DRAW_FLAG_* bitmask.
layout( location = 7 ) in int   in_type;           //!< 0 = segment ribbon, 1 = sphere billboard.

// Per-fragment varyings.
layout( location = 0 ) out vec4       v_color;
layout( location = 1 ) out vec2       v_uv;
layout( location = 2 ) out float      v_view_depth;
layout( location = 3 ) out float      v_half_width_px;
layout( location = 4 ) out float      v_outline_px;
layout( location = 5 ) out flat int   v_flags;
layout( location = 6 ) out flat int   v_type;

void main() {
    // The clip-space position is fully pre-computed on the CPU, so just output it.
    gl_Position = in_pos;

    // The engine's projection matrix (P) is designed for analytical reprojection in shaders
    // and produces clip-space z values where NDC_z = clip_z/clip_w is always > 1 for any
    // valid in-frustum point (minimum ≈ (zfar+znear)/(zfar-znear) ≈ 1.0005 with znear=1,
    // zfar=4096). Vulkan's hardware depth clipper silently discards every primitive with
    // NDC_z outside [0,1] before rasterization, so nothing would ever reach the fragment
    // shader. Override z to place all overlay vertices at NDC_z = 0.5 (the mid-point of
    // the valid range). The debug overlay uses its own per-fragment depth test by sampling
    // TEX_PT_VIEW_DEPTH_A, so Vulkan's hardware depth has no role here.
    gl_Position.z = 0.5 * gl_Position.w;

    // Forward all per-vertex data to the fragment shader.
    v_color         = in_color;
    v_uv            = in_uv;
    v_view_depth    = in_view_depth;
    v_half_width_px = in_half_width_px;
    v_outline_px    = in_outline_px;
    v_flags         = in_flags;
    v_type          = in_type;
}
