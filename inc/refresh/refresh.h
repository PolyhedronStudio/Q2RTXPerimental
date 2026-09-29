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
#pragma once


#include "common/cvar.h"
#include "common/error.h"

// Include the with client/client-game shared types.
#include "shared_types.h"

QEXTERN_C_OPEN

extern refcfg_t r_config;

// called when the library is loaded
extern ref_type_t (*R_Init)(bool total);

// called before the library is unloaded
extern void (*R_Shutdown)(bool total);

// All data that will be used in a level should be
// registered before rendering any frames to prevent disk hits,
// but they can still be registered at a later time
// if necessary.
//
// EndRegistration will free any remaining data that wasn't registered.
// Any model_s or skin_s pointers from before the BeginRegistration
// are no longer valid after EndRegistration.
//
// Skins and images need to be differentiated, because skins
// are flood filled to eliminate mip map edge errors, and pics have
// an implicit "pics/" prepended to the name. (a pic name that starts with a
// slash will not use the "pics/" prefix or the ".pcx" postfix)
extern void (*R_BeginRegistration)(const char* map);
qhandle_t R_RegisterModel(const char* name);
qhandle_t R_RegisterImage(const char* name, imagetype_t type,
    imageflags_t flags);
qhandle_t R_RegisterRawImage(const char* name, int width, int height, byte* pic, imagetype_t type,
    imageflags_t flags);
void R_UnregisterImage(qhandle_t handle);

extern void (*R_SetSky)(const char* name, float rotate, int autorotate, const vec3_t axis);
extern void (*R_EndRegistration)(void);


#define R_RegisterPic(name) R_RegisterImage(name, IT_PIC, IF_PERMANENT | IF_SRGB)
#define R_RegisterPic2(name) R_RegisterImage(name, IT_PIC, IF_SRGB)
#define R_RegisterFont(name) R_RegisterImage(name, IT_FONT, IF_PERMANENT | IF_SRGB)
#define R_RegisterSkin(name) R_RegisterImage(name, IT_SKIN, IF_SRGB)

extern void (*R_RenderFrame)(refdef_t* fd);
extern void (*R_LightPoint)(const vec3_t origin, vec3_t light);
extern void    (*R_ClearColor)(void);
extern void    (*R_SetAlpha)(float clpha);
extern void    (*R_SetAlphaScale)(float alpha);
extern void    (*R_SetColor)(uint32_t color);
extern void    (*R_SetClipRect)(const clipRect_t *clip);
float   R_ClampScale(cvar_t *var);
extern void    (*R_SetScale)(float scale);

enum {
	STYLE_FLAG_NONE                   = 0,
	STYLE_FLAG_OUTLINE                = BIT( 0 ),
	STYLE_FLAG_OUTER_GLOW             = BIT( 1 ),
	STYLE_FLAG_INNER_GLOW             = BIT( 2 ),
	STYLE_FLAG_SILHOUETTE_SDF         = BIT( 3 ),
	STYLE_FLAG_SDF_MTSDF              = BIT( 4 ),       //!< Multi-channel MTSDF texture active

	// Stroke alignment flags
	STYLE_FLAG_STROKE_ALIGN_CENTER    = 0,              //!< Centered [-W/2, +W/2] (Default)
	STYLE_FLAG_STROKE_ALIGN_INSET     = BIT( 5 ),       //!< Inset [-W, 0]
	STYLE_FLAG_STROKE_ALIGN_OUTSET    = BIT( 6 ),       //!< Outset [0, +W]
	STYLE_FLAG_STROKE_ALIGN_MASK      = ( BIT( 5 ) | BIT( 6 ) ),

	// Falloff & blending flags
	STYLE_FLAG_GLOW_FALLOFF_HERMITE   = 0,              //!< Cubic Hermite smoothstep (Default)
	STYLE_FLAG_GLOW_FALLOFF_EXP       = BIT( 7 ),       //!< Exponential / Neon curve
	STYLE_FLAG_GLOW_BLEND_ADDITIVE    = BIT( 8 ),       //!< Additive blending for HDR bloom

	// Per-edge enable masks
	STYLE_FLAG_EDGE_TOP               = BIT( 9 ),
	STYLE_FLAG_EDGE_RIGHT             = BIT( 10 ),
	STYLE_FLAG_EDGE_BOTTOM            = BIT( 11 ),
	STYLE_FLAG_EDGE_LEFT              = BIT( 12 ),
	STYLE_FLAG_EDGE_ALL               = ( BIT( 9 ) | BIT( 10 ) | BIT( 11 ) | BIT( 12 ) ),

	// Primitive & 3D flags
	STYLE_FLAG_PRIMITIVE_LINE2D       = BIT( 13 ),      //!< Capsule line segment SDF
	STYLE_FLAG_CORNER_RADIUS          = BIT( 14 ),      //!< Rounded corners active
	STYLE_FLAG_DEPTH_TEST             = BIT( 15 ),      //!< 3D linear depth test against TEX_PT_VIEW_DEPTH_A
};

// Universal stroke configuration API
extern void (*R_SetStroke)( const uint32_t color, const float thickness );
extern void (*R_SetStrokeThickness)( const float thickness );
extern void (*R_SetStrokeThickness4)( const float top, const float right, const float bottom, const float left );
extern void (*R_SetStrokeColors4)( const uint32_t top, const uint32_t right, const uint32_t bottom, const uint32_t left );
extern void (*R_SetStrokeEx)( const uint32_t color, const float thickness, const uint32_t flags );
extern void (*R_SetStroke4Ex)( const uint32_t colors[ 4 ], const float thickness[ 4 ], const uint32_t flags );

// Universal outer glow configuration API
extern void (*R_SetOuterGlow)( const uint32_t color, const float radius );
extern void (*R_SetOuterGlowRadius4)( const float top, const float right, const float bottom, const float left );
extern void (*R_SetOuterGlowColors4)( const uint32_t top, const uint32_t right, const uint32_t bottom, const uint32_t left );
extern void (*R_SetOuterGlowEdges)( const uint32_t edge_mask );
extern void (*R_SetOuterGlowEx)( const uint32_t color, const float radius, const uint32_t flags );
extern void (*R_SetOuterGlow4Ex)( const uint32_t colors[ 4 ], const float radii[ 4 ], const uint32_t flags );

// Universal inner glow configuration API
extern void (*R_SetInnerGlow)( const uint32_t color, const float radius );
extern void (*R_SetInnerGlowColors4)( const uint32_t top, const uint32_t right, const uint32_t bottom, const uint32_t left );
extern void (*R_SetInnerGlowRadius4)( const float top, const float right, const float bottom, const float left );
extern void (*R_SetInnerGlowEx)( const uint32_t color, const float radius, const uint32_t flags );

// Universal corner radius configuration API
extern void (*R_SetCornerRadius)( const float radius );
extern void (*R_SetCornerRadius4)( const float top_left, const float top_right, const float bottom_right, const float bottom_left );

// Reset all renderer styles to default (no stroke, no glow, no corner radius)
extern void (*R_ClearStyle)( void );

// Dedicated 2D glowing line primitive
extern void (*R_DrawLine2D)( const float x1, const float y1, const float x2, const float y2, const float thickness, const uint32_t color );

// TrueType Font & 3D text API
extern qhandle_t (*R_RegisterFontTTF)( const char *path, const float pixel_height );
/**
*	@brief	Global helper to load pregenerated `.mtsdf` font binary cache files if present on disk;
*			falls back to full TTF distance-field registration and auto-caching if no cached `.mtsdf` file exists.
*	@param	fontPath	Relative path to TrueType font file (e.g. "fonts/segoeui.ttf").
*	@param	fontSizePx	Target font pixel height.
*	@return	Valid font handle on success, otherwise 0.
**/
qhandle_t R_LoadOrRegisterFontTTF( const char *fontPath, const float fontSizePx );
extern float (*R_GetFontEffectSpacing)( void );
extern void (*R_DrawString3DOccluded)( const vec3_t origin, const vec3_t angles, const float scale, const char *text, const qhandle_t font, const uint32_t color );
extern void (*R_DrawString3DNonOccluded)( const vec3_t origin, const vec3_t angles, const float scale, const char *text, const qhandle_t font, const uint32_t color );
extern void (*R_DrawString3D)( const vec3_t origin, const vec3_t angles, const float scale, const char *text, const qhandle_t font, const uint32_t color );

// Non-breaking 2D compatibility aliases
#define R_SetStroke2D            R_SetStroke
#define R_SetStrokeThickness2D   R_SetStrokeThickness
#define R_SetStrokeThickness2D4  R_SetStrokeThickness4
#define R_SetStrokeColors2D4     R_SetStrokeColors4
#define R_SetStroke2DEx          R_SetStrokeEx
#define R_SetStroke2D4Ex         R_SetStroke4Ex
#define R_SetOuterGlow2D         R_SetOuterGlow
#define R_SetOuterGlowRadius2D4  R_SetOuterGlowRadius4
#define R_SetOuterGlowColors2D4  R_SetOuterGlowColors4
#define R_SetOuterGlowEdges2D    R_SetOuterGlowEdges
#define R_SetOuterGlow2DEx       R_SetOuterGlowEx
#define R_SetOuterGlow2D4Ex      R_SetOuterGlow4Ex
#define R_SetInnerGlow2D         R_SetInnerGlow
#define R_SetInnerGlowColors2D4  R_SetInnerGlowColors4
#define R_SetInnerGlowRadius2D4  R_SetInnerGlowRadius4
#define R_SetInnerGlow2DEx       R_SetInnerGlowEx
#define R_SetCornerRadius2D      R_SetCornerRadius
#define R_SetCornerRadius2D4     R_SetCornerRadius4
#define R_ClearStyle2D           R_ClearStyle

extern void    (*R_DrawChar)(int x, int y, int flags, int ch, qhandle_t font);
extern int     (*R_DrawString)(int x, int y, int flags, size_t maxChars,
                     const char *string, qhandle_t font);  // returns advanced x coord
bool R_GetPicSize(int *w, int *h, qhandle_t pic);   // returns transparency bit
extern void    (*R_DrawPic)(int x, int y, qhandle_t pic);
extern void    ( *R_DrawPicEx )( double destX, double destY, double destW, double destH, qhandle_t pic,
    double srcX, double srcY, double srcW, double srcH );
extern void    (*R_DrawStretchPic)(int x, int y, int w, int h, qhandle_t pic);
extern void    (*R_DrawRotateStretchPic)( int x, int y, int w, int h, float angle, int pivot_x, int pivot_y, qhandle_t pic );
extern void    (*R_DrawKeepAspectPic)(int x, int y, int w, int h, qhandle_t pic);
extern void    (*R_DrawStretchRaw)(int x, int y, int w, int h);
extern void    (*R_TileClear)(int x, int y, int w, int h, qhandle_t pic);
extern void    (*R_DrawFill8)(int x, int y, int w, int h, int c);
extern void    (*R_DrawFill32)(int x, int y, int w, int h, uint32_t color);
// <Q2RTXP> For crosshair. WID: TODO: The other Draw calls floatify.
extern void    ( *R_DrawFill8f )( float x, float y, float w, float  h, int32_t c );
extern void    ( *R_DrawFill32f )( float x, float y, float w, float h, uint32_t color );
extern void    ( *R_DrawDebugBox )( const vec3_t mins, const vec3_t maxs, uint32_t color, const float thickness, const float outline_thickness, const uint16_t style_flags );
extern void    ( *R_DrawDebugLine )( const vec3_t start, const vec3_t end, uint32_t color, const float thickness, const float outline_thickness, const uint16_t style_flags );
extern void    ( *R_DrawDebugArrow )( const vec3_t start, const vec3_t end, float head_length, uint32_t color, const float thickness, const float outline_thickness, const uint16_t style_flags );
extern void    ( *R_DrawDebugSphere )( const vec3_t center, float radius, uint32_t color, const float thickness, const float outline_thickness, const uint16_t style_flags );
extern void    ( *R_DrawDebugCapsule )( const vec3_t start, const vec3_t end, float radius, uint32_t color, const float thickness, const float outline_thickness, const uint16_t style_flags );
extern void    ( *R_DrawDebugCylinder )( const vec3_t start, const vec3_t end, float radius, uint32_t color, const float thickness, const float outline_thickness, const uint16_t style_flags );
// </Q2RTXP>
extern void    (*R_UpdateRawPic)(int pic_w, int pic_h, const uint32_t *pic);
extern void    (*R_DiscardRawPic)(void);

// video mode and refresh state management entry points
extern void    (*R_BeginFrame)(void);
extern void    (*R_EndFrame)(void);
extern void    (*R_ModeChanged)(int width, int height, int flags);

// add decal to ring buffer
extern void (*R_AddDecal)(decal_t* d);
// add pre-clipped decal mesh triangles
extern void (*R_AddDecalMesh)(const decal_mesh_vertex_t *vertices, int32_t vertexCount, const vec3_t albedo, float alpha, uint32_t materialHash, float lifeSeconds);
// clear all renderer-side decal submissions
extern void (*R_ClearDecals)(void);
// clear renderer-side transient state (entity history, decals, etc.)
extern void ( *R_ClearState )( void );
// clear all renderer-side decal material mappings
extern void (*R_ClearDecalMaterialMappings)(void);
// configure one renderer-side decal material mapping
extern void (*R_SetDecalMaterialMapping)(uint32_t materialHash, const char *materialName);
// configure the renderer-side decal render mode
extern void (*R_SetDecalRenderMode)(int32_t renderMode);
// dump renderer-side decal material mappings for runtime diagnostics
extern void (*R_DumpDecalMaterialMappings)(void);

extern bool (*R_InterceptKey)(unsigned key, bool down);
extern bool (*R_IsHDR)(void);

#if REF_GL
void R_RegisterFunctionsGL(void);
#endif
#if REF_VKPT
void R_RegisterFunctionsRTX(void);
#endif


/**
*   @brief
**/
typedef struct {
    int     colorbits;
    int     depthbits;
    int     stencilbits;
    int     multisamples;
    qboolean debug;
} r_opengl_config_t;

r_opengl_config_t *R_GetGLConfig( void );

// Extern C
QEXTERN_C_CLOSE
