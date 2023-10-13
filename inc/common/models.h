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

#ifndef COMMON_MODELS_H
#define COMMON_MODELS_H

// WID: C++20: In case of C++ including this..
#ifdef __cplusplus
// We extern "C"
extern "C" {
#endif

//
// models.h -- common models manager
//

#include "system/hunk.h"
#include "common/error.h"

#define MOD_Malloc(size)    Hunk_TryAlloc(&model->hunk, size)

#define CHECK(x)    if (!(x)) { ret = Q_ERR(ENOMEM); goto fail; }

#define MAX_ALIAS_SKINS     32
#define MAX_ALIAS_VERTS     4096

typedef struct mspriteframe_s {
	int             width, height;
	int             origin_x, origin_y;
	struct image_s *image;
} mspriteframe_t;

typedef enum {
	MCLASS_REGULAR,
	MCLASS_EXPLOSION,
	MCLASS_FLASH,
	MCLASS_SMOKE,
	MCLASS_STATIC_LIGHT,
	MCLASS_FLARE
} model_class_t;

typedef struct {
	vec3_t translate;
	quat_t rotate;
	vec3_t scale;
} iqm_transform_t;

typedef struct {
	char name[ MAX_QPATH ];
	uint32_t first_frame;
	uint32_t num_frames;
	bool loop;
} iqm_anim_t;

// inter-quake-model
typedef struct {
	uint32_t num_vertexes;
	uint32_t num_triangles;
	uint32_t num_frames;
	uint32_t num_meshes;
	uint32_t num_joints;
	uint32_t num_poses;
	uint32_t num_animations;
	struct iqm_mesh_s *meshes;

	uint32_t *indices;

	// vertex arrays
	float *positions;
	float *texcoords;
	float *normals;
	float *tangents;
	byte *colors;
	byte *blend_indices; // byte4 per vertex
	byte *blend_weights; // byte4 per vertex

	char *jointNames;
	int *jointParents;
	float *bindJoints; // [num_joints * 12]
	float *invBindJoints; // [num_joints * 12]
	iqm_transform_t *poses; // [num_frames * num_poses]
	float *bounds;

	iqm_anim_t *animations;
} iqm_model_t;

// inter-quake-model mesh
typedef struct iqm_mesh_s {
	char name[ MAX_QPATH ];
	char material[ MAX_QPATH ];
	iqm_model_t *data;
	uint32_t first_vertex, num_vertexes;
	uint32_t first_triangle, num_triangles;
	uint32_t first_influence, num_influences;
} iqm_mesh_t;

typedef struct light_poly_s {
	float positions[ 9 ]; // 3x vec3_t
	vec3_t off_center;
	vec3_t color;
	struct pbr_material_s *material;
	int cluster;
	int style;
	float emissive_factor;
} light_poly_t;

typedef struct model_s {
	enum {
		MOD_FREE,
		MOD_ALIAS,
		MOD_SPRITE,
		MOD_EMPTY
	} type;
	char name[ MAX_QPATH ];
	int registration_sequence;
	memhunk_t hunk;

	// alias models
	int numframes;
	struct maliasframe_s *frames;
	#if USE_REF == REF_GL || USE_REF == REF_VKPT
	int nummeshes;
	struct maliasmesh_s *meshes;
	model_class_t model_class;
	#else
	int numskins;
	struct image_s *skins[ MAX_ALIAS_SKINS ];
	int numtris;
	struct maliastri_s *tris;
	int numsts;
	struct maliasst_s *sts;
	int numverts;
	int skinwidth;
	int skinheight;
	#endif

		// sprite models
	struct mspriteframe_s *spriteframes;
	bool sprite_vertical;

	iqm_model_t *iqmData;

	int num_light_polys;
	light_poly_t *light_polys;
} model_t;

// WID: C++20: In case of C++ including this..
#ifdef __cplusplus
};
#endif

#endif // COMMON_MODELS_H
