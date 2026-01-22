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

/********************************************************************
*
*	Module Name: Model Management System
*
*	Detailed explanation of module purpose and functionality.
*
*	This module manages loading, caching, and lifetime of 3D models
*	for the Quake II RTX Perimental renderer. It supports multiple
*	model formats (MD2, MD3, IQM, SP2) and handles format detection,
*	model registration, reference counting, and memory management.
*
*	Model Registration Flow:
*	1. R_RegisterModel() is called with a model path
*	2. Path normalization and inline BSP model detection
*	3. Check if model is already loaded (cache lookup)
*	4. Search game directory first, then base directory
*	5. Try HD assets (.md3) before standard assets (.md2) when appropriate
*	6. Detect format via magic number (ident field)
*	7. Call format-specific loader (MD2, MD3, IQM, SP2, SPJ)
*	8. Store model in hunk memory with registration sequence stamp
*	9. Return handle (1-based index into r_models array)
*
*	Supported Formats:
*	- MD2 (ident: IDP2): Quake II alias models (vertex-animated)
*	- MD3 (ident: IDP3): Quake III models (mesh-based, higher quality)
*	- IQM (ident: INTERQUAKEMODEL): Skeletal animation models
*	- SP2 (ident: IDS2): Sprite models (binary format)
*	- SPJ (ident: JSPRITE): Sprite models (JSON text format)
*
*	Memory Management:
*	- Models are allocated from hunk memory (Hunk_Begin/End)
*	- Each model has a registration_sequence stamp
*	- MOD_FreeUnused() removes models not used in current registration
*	- Inline BSP models (*N) return negative handles and aren't cached
*
*	Model Classification:
*	- Hardcoded special effect classes (explosion, flash, smoke, etc.)
*	- Used for renderer-specific optimizations and behaviors
*	- Classification occurs after successful model load
*
*	Test Model System:
*	- cl_testmodel cvar: Model path to display at test position
*	- cl_testfps: Animation speed for test model
*	- cl_testalpha: Transparency for test model
*	- puttest command: Places test model at current view position
*
********************************************************************/

#include "shared/shared.h"
#include "shared/util/util_list.h"
#include "common/common.h"
#include "common/files.h"
#include "system/hunk.h"
#include "format/md2.h"
#if USE_MD3
#include "format/md3.h"
#endif
#include "format/sp2.h"
#include "format/iqm.h"
#include "refresh/images.h"
#include "refresh/models.h"
#include "../client/cl_client.h"
#include "gl/gl.h"

// During registration it is possible to have more models than could actually
// be referenced during gameplay, because we don't want to free anything until
// we are sure we won't need it.
#define MAX_RMODELS     (MAX_MODELS * 2)

// Global model pool - models remain loaded until explicitly freed or registration sequence advances
model_t      r_models[MAX_RMODELS];
int          r_numModels;

// Test model cvars for debugging and model preview
cvar_t    *cl_testmodel;		// Path to model file to display
cvar_t    *cl_testfps;			// Animation frames per second
cvar_t    *cl_testalpha;		// Alpha transparency (0-1)
qhandle_t  cl_testmodel_handle = -1;
vec3_t     cl_testmodel_position;


/**
*
*
*
*	Model Slot Management
*
*
*
**/

/**
*	@brief	Allocate a free model slot from the global model pool.
*
*	Searches the r_models array for an unused slot (model->type == 0).
*	If all existing slots are occupied, expands the pool by incrementing
*	r_numModels, up to the MAX_RMODELS limit.
*
*	@return	Pointer to an available model_t slot, or NULL if pool is full.
*
*	@note	Model slots are reused after MOD_FreeUnused() or MOD_FreeAll().
*	@note	Does not initialize the returned slot - caller must set type and other fields.
**/
static model_t *MOD_Alloc(void)
{
    model_t *model;
    int i;

    for (i = 0, model = r_models; i < r_numModels; i++, model++) {
        if (!model->type) {
            break;
        }
    }

    if (i == r_numModels) {
        if (r_numModels == MAX_RMODELS) {
            return NULL;
        }
        r_numModels++;
    }

    return model;
}


/**
*
*
*
*	Model Lookup and Registration
*
*
*
**/

/**
*	@brief	Look up a model by its normalized path name.
*
*	Performs a linear search through the r_models array to find a model
*	with a matching name. Uses case-insensitive path comparison that
*	handles forward/backward slashes equivalently.
*
*	@param	name	Normalized model path to search for
*
*	@return	Pointer to the model_t if found, NULL otherwise.
*
*	@note	Name should be pre-normalized with FS_NormalizePath().
*	@note	Only searches models with type != 0 (loaded models).
**/
model_t *MOD_ForName(const char *name) {
    model_t *model;
    int i;

    for (i = 0, model = r_models; i < r_numModels; i++, model++) {
        if (!model->type) {
            continue;
        }
        if (!FS_pathcmp(model->name, name)) {
            return model;
        }
    }

    return NULL;
}

/**
*	@brief	Convert a model handle to a model pointer.
*
*	Model handles are 1-based indices into the r_models array.
*	This function performs bounds checking and validates that the
*	model slot is actually loaded.
*
*	@param	h	Model handle (1-based index)
*
*	@return	Pointer to the model_t if handle is valid, NULL otherwise.
*
*	@note	Handle 0 is reserved for "no model" and returns NULL.
*	@note	Negative handles represent inline BSP models and are not handled here.
**/
model_t *MOD_ForHandle( qhandle_t h ) {
    model_t *model;

    if ( !h ) {
        return NULL;
    }

    Q_assert( h > 0 && h <= r_numModels );
    model = &r_models[ h - 1 ];
    if ( !model->type ) {
        return NULL;
    }

    return model;
}

/**
*	@brief	Console command to list all loaded models.
*
*	Displays a table of all loaded models with their type, memory usage,
*	and file path. Shows total model count and total memory consumption.
*
*	@note	Console command: r_modellist
*	@note	Model types: Free (unused), Alias (MD2/MD3/IQM), Sprite (SP2/SPJ), Empty (no geometry)
**/
static void MOD_List_f(void)
{
    static const char *types[ 5 ] =
    {
        "Free",
        "Alias",
        "Sprite",
        "Empty"
    };

    int     i, count;
    model_t *model;
    size_t  bytes;

    Com_Printf("------------------\n");
    bytes = count = 0;

    for (i = 0, model = r_models; i < r_numModels; i++, model++) {
        if (!model->type) {
            continue;
        }
        Com_Printf("(Type: %s) %8zu : %s\n", types[model->type],
                   model->hunk.mapped, model->name);
        bytes += model->hunk.mapped;
        count++;
    }
    Com_Printf("Total models: %d (out of %d slots)\n", count, r_numModels);
    Com_Printf("Total resident: %zu\n", bytes);
}

/**
*	@brief	Free models not used in the current registration sequence.
*
*	Called at the end of map loading to remove models from the previous
*	map that aren't needed anymore. Models with a registration_sequence
*	matching the current global registration_sequence are kept and paged
*	into memory; all others are freed.
*
*	@note	Paging in ensures recently-used models are memory resident.
*	@note	This implements automatic model cache management.
**/
void MOD_FreeUnused(void)
{
    model_t *model;
    int i;

    for (i = 0, model = r_models; i < r_numModels; i++, model++) {
        if (!model->type) {
            continue;
        }
        if (model->registration_sequence == registration_sequence) {
            // make sure it is paged in
            Com_PageInMemory(model->hunk.base, model->hunk.cursize);
        } else {
            // don't need this model
            Hunk_Free(&model->hunk);
            memset(model, 0, sizeof(*model));
        }
    }
}

/**
*	@brief	Free all loaded models unconditionally.
*
*	Called during renderer shutdown or when switching renderers.
*	Releases all hunk memory and resets the model count to zero.
*
*	@note	After calling this, all model handles become invalid.
*	@note	Resets r_numModels to 0, completely clearing the model pool.
**/
void MOD_FreeAll(void)
{
    model_t *model;
    int i;

    for (i = 0, model = r_models; i < r_numModels; i++, model++) {
        if (!model->type) {
            continue;
        }

        Hunk_Free(&model->hunk);
        memset(model, 0, sizeof(*model));
    }

    r_numModels = 0;
}


/**
*
*
*
*	MD2 Format Validation
*
*
*
**/

/**
*	@brief	Validate the structure and bounds of an MD2 model file.
*
*	Performs comprehensive validation of MD2 header fields, checking:
*	- Magic number (ident) and version
*	- Triangle count and array bounds
*	- Texture coordinate (st) count and bounds
*	- Vertex (xyz) count and frame data layout
*	- Skin count and skin dimensions
*	- Memory alignment requirements
*	- Array offsets and extents relative to file size
*
*	@param	header	Pointer to MD2 header (byte-swapped to native endian)
*	@param	length	Total file size in bytes
*
*	@return	Q_ERR_SUCCESS if valid, or error code:
*			Q_ERR_UNKNOWN_FORMAT - Bad ident or version
*			Q_ERR_TOO_FEW - Insufficient triangles/vertices/frames
*			Q_ERR_TOO_MANY - Exceeds maximum limits
*			Q_ERR_BAD_EXTENT - Offset/size extends beyond file
*			Q_ERR_BAD_ALIGN - Misaligned data structure
*			Q_ERR_INVALID_FORMAT - Invalid skin dimensions
*
*	@note	MD2_IDENT = "IDP2" (0x32504449)
*	@note	MD2_VERSION = 8
*	@note	Maximum limits prevent buffer overflows and resource exhaustion.
**/
int MOD_ValidateMD2(dmd2header_t *header, size_t length)
{
    size_t end;

    // Check magic number (ident) and format version
    if (header->ident != MD2_IDENT)
        return Q_ERR_UNKNOWN_FORMAT;
    if (header->version != MD2_VERSION)
        return Q_ERR_UNKNOWN_FORMAT;

    // Validate triangle count and verify array bounds
    if (header->num_tris < 1)
        return Q_ERR_TOO_FEW;
    if (header->num_tris > TESS_MAX_INDICES / 3)
        return Q_ERR_TOO_MANY;

    end = header->ofs_tris + sizeof(dmd2triangle_t) * header->num_tris;
    if (header->ofs_tris < sizeof(*header) || end < header->ofs_tris || end > length)
        return Q_ERR_BAD_EXTENT;
    if (header->ofs_tris % q_alignof(dmd2triangle_t))
        return Q_ERR_BAD_ALIGN;

    // Validate texture coordinate (st) count and array bounds
    if (header->num_st < 3)
        return Q_ERR_TOO_FEW;
    if (header->num_st > INT_MAX / sizeof(dmd2stvert_t))
        return Q_ERR_TOO_MANY;

    end = header->ofs_st + sizeof(dmd2stvert_t) * header->num_st;
    if (header->ofs_st < sizeof(*header) || end < header->ofs_st || end > length)
        return Q_ERR_BAD_EXTENT;
    if (header->ofs_st % q_alignof(dmd2stvert_t))
        return Q_ERR_BAD_ALIGN;

    // Validate vertex count, frame count, and frame data layout
    if (header->num_xyz < 3)
        return Q_ERR_TOO_FEW;
    if (header->num_xyz > MD2_MAX_VERTS)
        return Q_ERR_TOO_MANY;
    if (header->num_frames < 1)
        return Q_ERR_TOO_FEW;
    if (header->num_frames > MD2_MAX_FRAMES)
        return Q_ERR_TOO_MANY;

    end = sizeof(dmd2frame_t) + (header->num_xyz - 1) * sizeof(dmd2trivertx_t);
    if (header->framesize < end || header->framesize > MD2_MAX_FRAMESIZE)
        return Q_ERR_BAD_EXTENT;
    if (header->framesize % q_alignof(dmd2frame_t))
        return Q_ERR_BAD_ALIGN;

    end = header->ofs_frames + (size_t)header->framesize * header->num_frames;
    if (header->ofs_frames < sizeof(*header) || end < header->ofs_frames || end > length)
        return Q_ERR_BAD_EXTENT;
    if (header->ofs_frames % q_alignof(dmd2frame_t))
        return Q_ERR_BAD_ALIGN;

    // Validate skin count and skin dimensions
    if (header->num_skins) {
        if (header->num_skins > MD2_MAX_SKINS)
            return Q_ERR_TOO_MANY;

        end = header->ofs_skins + (size_t)MD2_MAX_SKINNAME * header->num_skins;
        if (header->ofs_skins < sizeof(*header) || end < header->ofs_skins || end > length)
            return Q_ERR_BAD_EXTENT;
    }

    if (header->skinwidth < 1 || header->skinwidth > MD2_MAX_SKINWIDTH)
        return Q_ERR_INVALID_FORMAT;
    if (header->skinheight < 1 || header->skinheight > MD2_MAX_SKINHEIGHT)
        return Q_ERR_INVALID_FORMAT;

    return Q_ERR_SUCCESS;
}


/**
*
*
*
*	Model Classification
*
*
*
**/

/**
*	@brief	Classify model by hardcoded name for special rendering behavior.
*
*	Certain models require special handling in the renderer (lighting,
*	transparency, billboarding, etc.). This function uses a hardcoded
*	name lookup table to assign model classifications.
*
*	@param	name	Normalized model path
*
*	@return	Model class enum value:
*			MCLASS_EXPLOSION - Explosion sprites (no lighting)
*			MCLASS_FLASH - Muzzle flash (additive blending)
*			MCLASS_SMOKE - Smoke effects (alpha blending)
*			MCLASS_STATIC_LIGHT - Static light source models
*			MCLASS_FLARE - Lens flare effects
*			MCLASS_REGULAR - Standard model (default)
*
*	@note	Classification is based on exact string match, not pattern.
*	@note	Hardcoded table allows renderer-specific optimizations.
**/
static model_class_t
get_model_class(const char *name)
{
	if (!strcmp(name, "models/objects/explode/tris.md2"))
		return MCLASS_EXPLOSION;
    else if ( !strcmp( name, "sprites/explo00/explo00.spj" ) )
        return MCLASS_EXPLOSION;
    else if ( !strcmp( name, "sprites/explo01/explo01.spj" ) )
        return MCLASS_EXPLOSION;
	else if (!strcmp(name, "models/objects/r_explode/tris.md2"))
		return MCLASS_EXPLOSION;
	else if (!strcmp(name, "models/objects/flash/tris.md2"))
		return MCLASS_FLASH;
	else if (!strcmp(name, "models/objects/smoke/tris.md2"))
		return MCLASS_SMOKE;
	else if (!strcmp(name, "models/objects/minelite/light2/tris.md2"))
        return MCLASS_STATIC_LIGHT;
    //else if ( !strcmp( name, "models/objects/barrels/tris.md2" ) )
    //    return MCLASS_STATIC_LIGHT;
    else if (!strcmp(name, "models/objects/flare/tris.md2"))
        return MCLASS_FLARE;
	else
		return MCLASS_REGULAR;
}


/**
*
*
*
*	SP2 Sprite Loading
*
*
*
**/

/**
*	@brief	Load an SP2 (Sprite2) format sprite model.
*
*	SP2 is Quake II's binary sprite format for 2D billboard effects.
*	Each frame references an external image file and specifies dimensions
*	and origin offset for positioning.
*
*	File Format:
*	- Header: ident, version, frame count
*	- Frame array: width, height, origin_x, origin_y, image path
*
*	@param	model		Model structure to populate
*	@param	rawdata		Raw file data (little-endian)
*	@param	length		File size in bytes
*	@param	mod_name	Original model name (for error messages)
*
*	@return	Q_ERR_SUCCESS on success, or error code:
*			Q_ERR_FILE_TOO_SMALL - File smaller than header
*			Q_ERR_UNKNOWN_FORMAT - Bad ident or version
*			Q_ERR_TOO_MANY - Exceeds SP2_MAX_FRAMES
*			Q_ERR_BAD_EXTENT - Frame array extends beyond file
*
*	@note	SP2_IDENT = "IDS2" (0x32534449)
*	@note	SP2_VERSION = 2
*	@note	Empty models (numframes=0) are marked MOD_EMPTY and draw nothing.
*	@note	Allocates spriteframes array from hunk memory.
**/
static int MOD_LoadSP2(model_t *model, const void *rawdata, size_t length, const char* mod_name)
{
    dsp2header_t header;
    dsp2frame_t *src_frame;
    mspriteframe_t *dst_frame;
    char buffer[SP2_MAX_FRAMENAME];
    int i, ret;

    if (length < sizeof(header))
        return Q_ERR_FILE_TOO_SMALL;

    // Byte-swap header from little-endian to native byte order
    LittleBlock(&header, rawdata, sizeof(header));

    // Validate SP2 magic number and version
    if (header.ident != SP2_IDENT)
        return Q_ERR_UNKNOWN_FORMAT;
    if (header.version != SP2_VERSION)
        return Q_ERR_UNKNOWN_FORMAT;
    if (header.numframes < 1) {
        // Empty models draw nothing (used for placeholder entities)
        model->type = MOD_EMPTY;
        return Q_ERR_SUCCESS;
    }
    if (header.numframes > SP2_MAX_FRAMES)
        return Q_ERR_TOO_MANY;
    if (sizeof(dsp2header_t) + sizeof(dsp2frame_t) * header.numframes > length)
        return Q_ERR_BAD_EXTENT;

    // Allocate sprite frame array from hunk memory
    Hunk_Begin(&model->hunk, sizeof(mspriteframe_t) * header.numframes);
    model->type = MOD_SPRITE;

    CHECK(model->spriteframes = MOD_Malloc(sizeof(mspriteframe_t) * header.numframes));
    model->numframes = header.numframes;

    // Parse frame data: dimensions, origin, and image path
    src_frame = (dsp2frame_t *)((byte *)rawdata + sizeof(dsp2header_t));
    dst_frame = model->spriteframes;
    for (i = 0; i < header.numframes; i++) {
        dst_frame->width = (int32_t)LittleLong(src_frame->width);
        dst_frame->height = (int32_t)LittleLong(src_frame->height);

        dst_frame->origin_x = (int32_t)LittleLong(src_frame->origin_x);
        dst_frame->origin_y = (int32_t)LittleLong(src_frame->origin_y);

        // Load image for this frame; normalize path and register with image manager
        if (!Q_memccpy(buffer, src_frame->name, 0, sizeof(buffer))) {
            Com_WPrintf("%s has bad frame name\n", model->name);
            dst_frame->image = R_NOTEXTURE;
        } else {
            FS_NormalizePath(buffer);
            dst_frame->image = IMG_Find(buffer, IT_SPRITE, IF_SRGB);
        }

        src_frame++;
        dst_frame++;
    }

    Hunk_End(&model->hunk);

    return Q_ERR_SUCCESS;

fail:
    return ret;
}


/**
*
*
*
*	Model Loading Pipeline
*
*
*
**/

// Directory search order: game directory first, then base directory
#define TRY_MODEL_SRC_GAME      1
#define TRY_MODEL_SRC_BASE      0

// Publicly exposed to refresh modules so, extern "C".
int MOD_LoadSP2Json( model_t *model, const void *rawdata, size_t length, const char *mod_name );

/**
*	@brief	Register and load a model by name, returning a handle.
*
*	This is the main entry point for model loading. It handles:
*	- Path normalization and inline BSP model detection
*	- Model cache lookup (returns existing if already loaded)
*	- Directory search priority (game dir > base dir)
*	- HD asset preference (.md3 over .md2 when supported)
*	- Format detection via magic number (ident field)
*	- Delegation to format-specific loaders
*	- Model classification for special effects
*	- Registration sequence stamping for automatic cleanup
*
*	Directory Search Order:
*	1. If fs_game != "baseq2", search game directory first
*	2. Always search base directory as fallback
*	3. Within each directory, try HD asset variant first (.md3)
*	4. Then try standard asset (.md2, .spj, etc.)
*
*	Format Detection:
*	- SPJ (text): Detected by .spj extension, sets ident manually
*	- MD2/MD3/SP2/IQM (binary): Detected by reading 4-byte ident field
*
*	@param	name	Model path (e.g., "models/weapons/v_bfg/tris.md2")
*
*	@return	Model handle (1-based index), or:
*			0 - Failed to load or empty name
*			~N (negative) - Inline BSP model index (for "*N" names)
*
*	@note	Inline BSP models (prefixed with '*') reference brush model indices from the map.
*	@note	Empty names ("") are silently ignored and return 0.
*	@note	Failed loads print error message to console.
*	@note	HD assets (.md3) are only tried when using VKPT renderer or gl_use_hd_assets is enabled.
**/
qhandle_t R_RegisterModel(const char *name)
{
    char normalized[MAX_QPATH];
    qhandle_t index;
    size_t namelen;
    int filelen = 0;
    model_t *model;
    byte *rawdata = NULL;
    uint32_t ident = 0;
    mod_load_t load;
    int ret;

    // Empty names are legal, silently ignore them
    if (!*name)
        return 0;

    // Inline BSP models (e.g., "*1", "*2") reference brush geometry from the map
    if (*name == '*') {
        index = atoi(name + 1);
        return ~index;
    }

    // Normalize path (forward slashes, lowercase, remove redundant separators)
    namelen = FS_NormalizePathBuffer(normalized, name, MAX_QPATH);

    // Path normalization should never produce oversize names
    if (namelen >= MAX_QPATH)
        Com_Error(ERR_DROP, "%s: oversize name", __func__);

    // Normalized to empty name (invalid path)?
    if (namelen == 0) {
        Com_DPrintf("%s: empty name\n", __func__);
        return 0;
    }

    // Check if model is already loaded (cache lookup)
    model = MOD_ForName(normalized);
    if (model) {
        MOD_Reference(model);
        goto done;
    }

    // Directory search: game dir first (if not baseq2), then base dir
    // Always prefer models from the game dir, even if format might be 'inferior'
    for (int try_location = Q_stricmp(fs_game->string, BASEGAME) ? TRY_MODEL_SRC_GAME : TRY_MODEL_SRC_BASE;
         try_location >= TRY_MODEL_SRC_BASE;
         try_location--)
    {
        int fs_flags = 0;
        if (try_location > 0)
            fs_flags = try_location == TRY_MODEL_SRC_GAME ? FS_PATH_GAME : FS_PATH_BASE;

        // Try HD asset variant (.md3) before standard asset (.md2) when supported
        char* extension = normalized + namelen - 4;
        bool try_md3 = cls.ref_type == REF_TYPE_VKPT || (cls.ref_type == REF_TYPE_GL && gl_use_hd_assets->integer);
        if (namelen > 4 && (strcmp(extension, ".md2") == 0) && try_md3)
        {
            // Try loading .md3 variant (higher quality)
            memcpy(extension, ".md3", 4);

            filelen = FS_LoadFileFlags(normalized, (void **)&rawdata, fs_flags);

            memcpy(extension, ".md2", 4);
        }
        // Load SPJ (JSON sprite) format - text-based sprite definition
        if ( namelen > 4 && ( strcmp( extension, ".spj" ) == 0 ) ) {
			//memcpy( extension, ".sp2", 4 );
			filelen = FS_LoadFileFlags( normalized, (void **)&rawdata, fs_flags );
            if ( filelen > 0 ) {
                // SPJ is text format, manually set ident for format detection
                ident = SPJ_IDENT;
            }
			//memcpy( extension, ".spj", 4 );
        }
        // Try loading standard asset in current directory
        if (!rawdata)
        {
            filelen = FS_LoadFileFlags(normalized, (void **)&rawdata, fs_flags);
        }
        if (rawdata)
            break;
    }

    // Fallback to unscoped search (all directories)
	if (!rawdata)
	{
		filelen = FS_LoadFile(normalized, (void **)&rawdata);
		if (!rawdata) {
			// Don't spam console about missing models (common during development)
			if (filelen == Q_ERR(ENOENT)) {
				return 0;
			}

			ret = filelen;
			goto fail1;
		}
	}

    if (filelen < 4) {
        ret = Q_ERR_FILE_TOO_SMALL;
        goto fail2;
    }

    // Detect format via magic number (ident field)
    // Check ident from binary file rawdata if it was not set before (which it would be for text formats like SPJ)
    if ( !ident ) {
        ident = LittleLong( *(uint32_t *)rawdata );
    }
    
    // Dispatch to format-specific loader based on magic number
    switch (ident) {
    case MD2_IDENT:		// "IDP2" - Quake II alias models
        load = MOD_LoadMD2;
        break;
#if USE_MD3
    case MD3_IDENT:		// "IDP3" - Quake III models
        load = MOD_LoadMD3;
        break;
#endif
    case SP2_IDENT:		// "IDS2" - Sprite models (binary)
        load = MOD_LoadSP2;
        break;
    case SPJ_IDENT:		// "JSPRITE" - Sprite models (JSON text)
		load = MOD_LoadSP2Json;
		break;
    case IQM_IDENT:		// "INTERQUAKEMODEL" - Skeletal animation models
        load = MOD_LoadIQM;
        break;
    default:
        ret = Q_ERR_UNKNOWN_FORMAT;
        goto fail2;
    }

    if (!load)
    {
        ret = Q_ERR_UNKNOWN_FORMAT;
        goto fail2;
    }

    model = MOD_Alloc();
    if (!model) {
        ret = Q_ERR_OUT_OF_SLOTS;
        goto fail2;
    }

    memcpy(model->name, normalized, namelen + 1);
    model->registration_sequence = registration_sequence;	// Stamp for automatic cleanup

    // Call format-specific loader
    ret = load(model, rawdata, filelen, name);

    FS_FreeFile(rawdata);

    if (ret) {
        memset(model, 0, sizeof(*model));
        goto fail1;
    }

    // Classify model for special rendering behaviors (explosion, flash, etc.)
	model->model_class = get_model_class(model->name);

done:
    // Return 1-based handle (index + 1)
    index = (model - r_models) + 1;
    return index;

fail2:
    FS_FreeFile(rawdata);
fail1:
    Com_EPrintf("Couldn't load %s: %s\n", normalized, Q_ErrorString(ret));
    return 0;
}


/**
*
*
*
*	Console Commands
*
*
*
**/

/**
*	@brief	Console command to place test model at current view position.
*
*	Sets the test model position to the player's current view origin,
*	adjusted downward by eye-level height (46.12 units) to place the
*	model on the ground.
*
*	@note	Console command: puttest
*	@note	Used in conjunction with cl_testmodel cvar.
**/
static void MOD_PutTest_f(void)
{
	CL_RefExport_GetViewOrigin( cl_testmodel_position );
    cl_testmodel_position[2] -= 46.12f; // Adjust for player eye-level height
}


/**
*
*
*
*	Initialization and Shutdown
*
*
*
**/

/**
*	@brief	Initialize the model management system.
*
*	Registers console commands and creates cvars for test model system.
*	Called during renderer initialization.
*
*	@note	r_numModels must be 0 on entry (verified by assertion).
**/
void MOD_Init(void)
{
    Q_assert(!r_numModels);
    Cmd_AddCommand("r_modellist", MOD_List_f);
    Cmd_AddCommand("puttest", MOD_PutTest_f);

    // Test model cvars for debugging and model preview
    // Path to the test model - can be an .md2, .md3 or .iqm file
    cl_testmodel = Cvar_Get("cl_testmodel", "", 0);

    // Test model animation frames per second, can be adjusted at runtime
    cl_testfps = Cvar_Get("cl_testfps", "10", 0);

    // Test model alpha, 0-1
    cl_testalpha = Cvar_Get("cl_testalpha", "1", 0);
}

/**
*	@brief	Shutdown the model management system.
*
*	Frees all loaded models and unregisters console commands.
*	Called during renderer shutdown or when switching renderers.
**/
void MOD_Shutdown(void)
{
    MOD_FreeAll();
    Cmd_RemoveCommand("r_modellist");
    Cmd_RemoveCommand("puttest");
}

