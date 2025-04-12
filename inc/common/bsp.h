/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2008 Andrey Nazarov
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
/**
*
*
*   The BSP format is now part of shared data. Certain game module specific
*   pieces of code require knowledge of its data structure.
*
*	See: /shared/format_bsp.h
*
**/

#ifndef BSP_H
#define BSP_H

#include "shared/util/util_list.h"
#include "common/error.h"
#include "system/hunk.h"

// Extern C
QEXTERN_C_OPEN

int BSP_Load(const char *name, bsp_t **bsp_p);
void BSP_Free(bsp_t *bsp);
const char *BSP_ErrorString(int err);

#if USE_REF
// Also moved to shared/format_bsp.h
//typedef struct {
//    mface_t     *surf;
//    cm_plane_t    plane;
//    float       s, t;
//    float       fraction;
//} lightpoint_t;

void BSP_LightPoint(lightpoint_t *point, const vec3_t start, const vec3_t end, mnode_t *headnode);
void BSP_TransformedLightPoint(lightpoint_t *point, const vec3_t start, const vec3_t end,
                               mnode_t *headnode, const vec3_t origin, const vec3_t angles);
#endif

byte *BSP_ClusterVis(bsp_t *bsp, byte *mask, int cluster, int vis);
mleaf_t *BSP_PointLeaf( mnode_t *node, const vec3_t p );
mmodel_t *BSP_InlineModel(bsp_t *bsp, const char *name);

byte* BSP_GetPvs(bsp_t *bsp, int cluster);
byte* BSP_GetPvs2(bsp_t *bsp, int cluster);

bool BSP_SavePatchedPVS(bsp_t *bsp);

void BSP_Init(void);

// Extern C
QEXTERN_C_CLOSE

#endif // BSP_H
