/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2019, NVIDIA CORPORATION. All rights reserved.

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

#ifndef REFRESH_SHARED_TYPES_H
#define REFRESH_SHARED_TYPES_H

// WID: C++20: In case of C++ including this..
#ifdef __cplusplus
// We extern "C"
extern "C" {
#endif

#define MAX_DLIGHTS 32
#define MAX_ENTITIES 8192 // == MAX_PACKET_ENTITIES * 2
#define MAX_PARTICLES 16384
#define MAX_LIGHTSTYLES 256

#define POWERSUIT_SCALE 4.0f
#define WEAPONSHELL_SCALE 0.5f

#define SHELL_RED_COLOR 0xF2
#define SHELL_GREEN_COLOR 0xD0
#define SHELL_BLUE_COLOR 0xF3

#define SHELL_RG_COLOR 0xDC
// #define SHELL_RB_COLOR        0x86
#define SHELL_RB_COLOR 0x68
#define SHELL_BG_COLOR 0x78

// ROGUE
#define SHELL_DOUBLE_COLOR 0xDF // 223
#define SHELL_HALF_DAM_COLOR 0x90
#define SHELL_CYAN_COLOR 0x72
// ROGUE

#define SHELL_WHITE_COLOR 0xD7

#define RF_SHELL_MASK       (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | \
                            RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM)

#define DLIGHT_CUTOFF 64

typedef struct entity_s {
    qhandle_t model; // opaque type outside refresh
    vec3_t angles;

    /*
    ** most recent data
    */
    vec3_t origin; // also used as RF_BEAM's "from"
    int frame; // also used as RF_BEAM's diameter

    /*
    ** previous data for lerping
    */
    vec3_t oldorigin; // also used as RF_BEAM's "to"
    int oldframe;

    /*
    ** misc
    */
    float backlerp; // 0.0 = current, 1.0 = old
    int skinnum; // also used as RF_BEAM's palette index,
    // -1 => use rgba

    float alpha; // ignore if RF_TRANSLUCENT isn't set
    color_t rgba;

    qhandle_t skin; // NULL for inline skin
    int flags;

    int id;

    int tent_type;

    float scale;
} entity_t;

typedef enum dlight_type_e {
    DLIGHT_SPHERE = 0,
    DLIGHT_SPOT
} dlight_type;

typedef enum dlight_spot_emission_profile_e {
    DLIGHT_SPOT_EMISSION_PROFILE_FALLOFF = 0,
    DLIGHT_SPOT_EMISSION_PROFILE_AXIS_ANGLE_TEXTURE
} dlight_spot_emission_profile;

typedef struct dlight_s {
    vec3_t origin;
    #if USE_REF == REF_GL
    vec3_t transformed;
    #endif
    vec3_t color;
    float intensity;
    float radius;

    // VKPT light types support
    dlight_type light_type;
    // Spotlight options
    struct {
        // Spotlight emission profile
        dlight_spot_emission_profile emission_profile;
        // Spotlight direction
        vec3_t direction;
        union {
            // Options for DLIGHT_SPOT_EMISSION_PROFILE_FALLOFF
            struct {
                // Cosine of angle of spotlight cone width (no emission beyond that)
                float cos_total_width;
                // Cosine of angle of start of falloff (full emission below that)
                float cos_falloff_start;
            };
            // Options for DLIGHT_SPOT_EMISSION_PROFILE_AXIS_ANGLE_TEXTURE
            struct {
                // Angle of spotlight cone width (no emission beyond that), in radians
                float total_width;
                // Emission profile texture, indexed by 'angle / total_width'
                qhandle_t texture;
            };
        };
    } spot;
} dlight_t;

typedef struct particle_s {
    vec3_t origin;
    int color; // -1 => use rgba
    float alpha;
    color_t rgba;
    float brightness;
    float radius;
} particle_t;

typedef struct lightstyle_s {
    float           white;          // highest of RGB
    vec3_t          rgb;            // 0.0 - 2.0
} lightstyle_t;

#define MAX_DECALS 50
typedef struct decal_s {
    vec3_t pos;
    vec3_t dir;
    float spread;
    float length;
    float dummy;
} decal_t;

// passes information back from the RTX renderer to the engine for various development maros
typedef struct ref_feedback_s {
    int viewcluster;
    int lookatcluster;
    int num_light_polys;
    int resolution_scale;

    char view_material[ MAX_QPATH ];
    char view_material_override[ MAX_QPATH ];
    int view_material_index;

    vec3_t hdr_color;
    float adapted_luminance;
} ref_feedback_t;

typedef struct refdef_s {
    int x, y, width, height; // in virtual screen coordinates
    float fov_x, fov_y;
    vec3_t vieworg;
    vec3_t viewangles;
    vec4_t screen_blend; // rgba 0-1 full screen blend
    //vec4_t damageblend; // rgba 0-1 full screen damage blend
    float time; // time is uesed to auto animate
    int rdflags; // RDF_UNDERWATER, etc

    byte *areabits; // if not NULL, only areas with set bits will be drawn

    lightstyle_t *lightstyles; // [MAX_LIGHTSTYLES]

    int num_entities;
    entity_t *entities;

    int num_dlights;
    dlight_t *dlights;

    int num_particles;
    particle_t *particles;

    int decal_beg;
    int decal_end;
    decal_t decal[ MAX_DECALS ];

    ref_feedback_t feedback;
} refdef_t;

typedef struct {
    int     colorbits;
    int     depthbits;
    int     stencilbits;
    int     multisamples;
    bool    debug;
} r_opengl_config_t;

typedef enum {
    QVF_ACCELERATED = ( 1 << 0 ),
    QVF_GAMMARAMP = ( 1 << 1 ),
    QVF_FULLSCREEN = ( 1 << 2 )
} vidFlags_t;

typedef struct {
    int width;
    int height;
    vidFlags_t flags;
} refcfg_t;

typedef struct {
    int left, right, top, bottom;
} clipRect_t;

typedef enum {
    IF_NONE = 0,
    IF_PERMANENT = ( 1 << 0 ),
    IF_TRANSPARENT = ( 1 << 1 ),
    IF_PALETTED = ( 1 << 2 ),
    IF_UPSCALED = ( 1 << 3 ),
    IF_SCRAP = ( 1 << 4 ),
    IF_TURBULENT = ( 1 << 5 ),
    IF_REPEAT = ( 1 << 6 ),
    IF_NEAREST = ( 1 << 7 ),
    IF_OPAQUE = ( 1 << 8 ),
    IF_SRGB = ( 1 << 9 ),
    IF_FAKE_EMISSIVE = ( 1 << 10 ),
    IF_EXACT = ( 1 << 11 ),
    IF_NORMAL_MAP = ( 1 << 12 ),
    IF_BILERP = ( 1 << 13 ), // always lerp, independent of bilerp_pics cvar

    // Image source indicator/requirement flags
    IF_SRC_BASE = ( 0x1 << 16 ),
    IF_SRC_GAME = ( 0x2 << 16 ),
    IF_SRC_MASK = ( 0x3 << 16 ),
} imageflags_t;

// Shift amount for storing fake emissive synthesis threshold
#define IF_FAKE_EMISSIVE_THRESH_SHIFT 20

typedef enum {
    IT_PIC,
    IT_FONT,
    IT_SKIN,
    IT_SPRITE,
    IT_WALL,
    IT_SKY,

    IT_MAX
} imagetype_t;

typedef enum ref_type_e {
    REF_TYPE_NONE = 0,
    REF_TYPE_GL,
    REF_TYPE_VKPT
} ref_type_t;

// WID: C++20: In case of C++ including this..
#ifdef __cplusplus
};
#endif

#endif // REFRESH_H
