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

#ifndef MODELS_H
#define MODELS_H

//
// models.h -- common models manager
//

#include "system/hunk.h"
#include "common/error.h"

#include "refresh/shared_types.h"

#define MOD_Malloc(size)    Hunk_TryAlloc(&model->hunk, size)

#define CHECK(x)    if (!(x)) { ret = Q_ERR(ENOMEM); goto fail; }

// Extern C
QEXTERN_C_OPEN

extern model_t      r_models[];
extern int          r_numModels;

extern int registration_sequence;

typedef struct entity_s entity_t;

// these are implemented in r_models.c
void MOD_FreeUnused(void);
void MOD_FreeAll(void);
void MOD_Init(void);
void MOD_Shutdown(void);

model_t *MOD_ForName( const char *name );
model_t *MOD_ForHandle(qhandle_t h);
qhandle_t R_RegisterModel(const char *name);

struct dmd2header_s;
int MOD_ValidateMD2(struct dmd2header_s *header, size_t length);

int MOD_LoadIQM_Base(model_t* mod, const void* rawdata, size_t length, const char* mod_name);

/**
*	@brief	Lerp the bone pose transforms and compute the pose matrices for this model as [model->num_poses] 3x4 matrices
*			in the (pose_matrices) array. This uses the entity's frame/oldframe for Lerping, unless
*			the refresh entity has its bonePoses array set, in which case it'll use that for computing the
*			local space matrices with.
*
*	@note	(In other words it acts like a regular .md2/.md3 alias mesh, unless bonePoses were set.)
**/
bool R_ComputePoseTransforms( const model_t *model, const entity_t *entity, float *pose_matrices );

// these are implemented in [gl,sw]_models.c
typedef int (*mod_load_t)(model_t *, const void *, size_t, const char*);
extern int (*MOD_LoadMD2)(model_t *model, const void *rawdata, size_t length, const char* mod_name);
#if USE_MD3
extern int (*MOD_LoadMD3)(model_t *model, const void *rawdata, size_t length, const char* mod_name);
#endif
extern int(*MOD_LoadIQM)(model_t* model, const void* rawdata, size_t length, const char* mod_name);
extern void (*MOD_Reference)(model_t *model);

// Extern C
QEXTERN_C_CLOSE

#endif // MODELS_H
