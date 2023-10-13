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

#ifndef COMMON_IMAGES_H
#define COMMON_IMAGES_H

#include <shared/list.h>

// WID: C++20: In case of C++ including this..
#ifdef __cplusplus
// We extern "C"
extern "C" {
#endif

typedef enum {
	IM_PCX,
	IM_WAL,
	IM_TGA,
	IM_JPG,
	IM_PNG,
	IM_MAX
} imageformat_t;

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

typedef enum {
	IT_PIC,
	IT_FONT,
	IT_SKIN,
	IT_SPRITE,
	IT_WALL,
	IT_SKY,

	IT_MAX
} imagetype_t;

// Format of data in image_t.pix_data
typedef enum {
	PF_R8G8B8A8_UNORM = 0,
	PF_R16_UNORM
} pixelformat_t;

typedef struct image_s {
	list_t          entry;
	char            name[ MAX_QPATH ]; // game path
	int             baselen; // without extension
	imagetype_t     type;
	imageflags_t    flags;
	int             width, height; // source image
	int             upload_width, upload_height; // after power of two and picmip
	int             registration_sequence; // 0 = free
	char            filepath[ MAX_QPATH ]; // actual path loaded, with correct format extension
	int             is_srgb;
	uint64_t        last_modified;
	#if REF_GL
	unsigned        texnum; // gl texture binding
	float           sl, sh, tl, th;
	#endif
	#if REF_VKPT
	byte *pix_data; // todo: add miplevels
	pixelformat_t   pixel_format; // pixel format (only supported by VKPT renderer)
	vec3_t          light_color; // use this color if this is a light source
	vec2_t          min_light_texcoord;
	vec2_t          max_light_texcoord;
	bool            entire_texture_emissive;
	bool            processing_complete;
	#else
	byte *pixels[ 4 ]; // mip levels
	#endif
} image_t;

#ifdef __cplusplus
};
#endif

#endif // COMMON_IMAGES_H