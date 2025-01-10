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

#ifndef REFRESH_H
#define REFRESH_H

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
extern void    (*R_DrawChar)(int x, int y, int flags, int ch, qhandle_t font);
extern int     (*R_DrawString)(int x, int y, int flags, size_t maxChars,
                     const char *string, qhandle_t font);  // returns advanced x coord
bool R_GetPicSize(int *w, int *h, qhandle_t pic);   // returns transparency bit
extern void    (*R_DrawPic)(int x, int y, qhandle_t pic);
extern void    (*R_DrawStretchPic)(int x, int y, int w, int h, qhandle_t pic);
extern void    (*R_DrawRotateStretchPic)( int x, int y, int w, int h, float angle, int pivot_x, int pivot_y, qhandle_t pic );
extern void    (*R_DrawKeepAspectPic)(int x, int y, int w, int h, qhandle_t pic);
extern void    (*R_DrawStretchRaw)(int x, int y, int w, int h);
extern void    (*R_TileClear)(int x, int y, int w, int h, qhandle_t pic);
extern void    (*R_DrawFill8)(int x, int y, int w, int h, int c);
extern void    (*R_DrawFill32)(int x, int y, int w, int h, uint32_t color);
extern void    (*R_UpdateRawPic)(int pic_w, int pic_h, const uint32_t *pic);
extern void    (*R_DiscardRawPic)(void);
extern void    (*R_DrawFill32)(int x, int y, int w, int h, uint32_t color);

// video mode and refresh state management entry points
extern void    (*R_BeginFrame)(void);
extern void    (*R_EndFrame)(void);
extern void    (*R_ModeChanged)(int width, int height, int flags);

// add decal to ring buffer
extern void (*R_AddDecal)(decal_t* d);

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

#endif // REFRESH_H
