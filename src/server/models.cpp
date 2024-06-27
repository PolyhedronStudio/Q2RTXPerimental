/********************************************************************
*
*
*	Server Based Model Loading:
*
*
********************************************************************/
#include "server.h"

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

#include "shared/shared.h"
#include "shared/util_list.h"

#include "common/common.h"
#include "common/files.h"

#include "system/hunk.h"
//#include "format/md2.h"
//#if USE_MD3
//#include "format/md3.h"
//#endif
//#include "format/sp2.h"
#include "format/iqm.h"

#include "refresh/images.h"
#include "refresh/shared_types.h"

#include "common/skeletalmodels/cm_skm_configuration.h"



/**
*
* 
*
*   Server "placeholder" data types that were normally refresh related structures.
* 
* 
* 
**/
//! Model type load callback.
typedef int ( *mod_load_callback_t )( model_t *, const void *, size_t, const char * );

//! PBR Material replacement structure.
typedef struct sv_model_material_s {
    char name[ MAX_QPATH ];
} sv_model_material_t;

//! Aliasmesh replacement structure.
typedef struct maliasmesh_s {
    int             numverts;
    int             numtris;
    int             numindices;
    int             numskins;
    int             tri_offset; /* offset in vertex buffer on device */
    int *indices;
    vec3_t *positions;
    vec3_t *normals;
    vec2_t *tex_coords;
    vec3_t *tangents;
    uint32_t *blend_indices; // iqm only
    uint32_t *blend_weights; // iqm only
    sv_model_material_t **materials;
    bool            handedness;
} maliasmesh_t;

// Error Checking.
#define CHECK(x)    if (!(x)) { ret = Q_ERR(ENOMEM); goto fail; }

/**
*   SV MOD_Malloc 
**/
static inline void *MOD_Malloc( memhunk_t *hunk, const size_t size ) {
    return Hunk_TryAlloc( hunk, size );
}

// During registration it is possible to have more models than could actually
// be referenced during gameplay, because we don't want to free anything until
// we are sure we won't need it.
#define MAX_SVMODELS     (MAX_MODELS * 2)

//! Actual server model cache.
static model_t sv_models[ MAX_SVMODELS ] = {};
//! Current number of loaded up models.
static int32_t sv_numModels = 0;

//! The server's own registration sequence for loading models.
int32_t sv_model_registration_sequence = 0;

// Resides in /refresh/model_iqm.c ...
extern "C" {
    int MOD_LoadIQM_Base( model_t *model, const void *rawdata, size_t length, const char *mod_name );
}

/**
*   @brief  
**/
static model_t *SV_Models_Alloc( void ) {
    model_t *model = nullptr;
    int32_t i = 0;

    // Iterate up to the first free model(has type unset( == 0 )).
    for ( i = 0, model = sv_models; i < sv_numModels; i++, model++ ) {
        if ( !model->type ) {
            break;
        }
    }

    // Increment num models if we're exceeding it with the last free model slot.
    if ( i == sv_numModels ) {
        // Prevent exceeding limits, return nullptr.
        if ( sv_numModels == MAX_SVMODELS ) {
            return nullptr;
        }
        // Increment.
        sv_numModels++;
    }
    // Return pointer to newly allocated model.
    return model;
}
/**
*   @brief  Pointer to model data matching the name, otherwise a (nullptr) on failure.
**/
model_t *SV_Models_Find( const char *name ) {
    model_t *model;
    int i;

    for ( i = 0, model = sv_models; i < sv_numModels; i++, model++ ) {
        if ( !model->type ) {
            continue;
        }
        if ( !FS_pathcmp( model->name, name ) ) {
            return model;
        }
    }

    return NULL;
}
/**
*   @return Pointer to model data matching the resource handle, otherwise a (nullptr) on failure.
**/
model_t *SV_Models_ForHandle( qhandle_t h ) {
    // Invalid handle.
    if ( !h ) {
        return nullptr;
    }
    // Assert.
    Q_assert( h > 0 && h <= sv_numModels );
    // Get pointer to model handle.
    model_t *model = &sv_models[ h - 1 ];
    // Type == 0 == uninitialized == invalid handle.
    if ( !model->type ) {
        return nullptr;
    }

    // Valid pointer handle.
    return model;
}

/**
*   @brief
**/
static void SV_Models_List_f( void ) {
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

    Com_Printf( "------------------\n" );
    bytes = count = 0;

    for ( i = 0, model = sv_models; i < sv_numModels; i++, model++ ) {
        if ( !model->type ) {
            continue;
        }
        Com_Printf( "(Type: %s) %8zu : %s\n", types[ model->type ],
            model->hunk.mapped, model->name );
        bytes += model->hunk.mapped;
        count++;
    }
    Com_Printf( "Total models: %d (out of %d slots)\n", count, sv_numModels );
    Com_Printf( "Total resident: %zu\n", bytes );
}

/**
*   @brief
**/
void SV_Models_FreeUnused( void ) {
    model_t *model;
    int i;

    for ( i = 0, model = sv_models; i < sv_numModels; i++, model++ ) {
        if ( !model->type ) {
            continue;
        }
        if ( model->registration_sequence == sv_model_registration_sequence ) {
            // make sure it is paged in
            Com_PageInMemory( model->hunk.base, model->hunk.cursize );
        } else {
            // don't need this model
            Hunk_Free( &model->hunk );
            memset( model, 0, sizeof( *model ) );
        }
    }
}

/**
*   @brief
**/
void SV_Models_FreeAll( void ) {
    model_t *model = nullptr;
    int32_t i = 0;

    for ( i = 0, model = sv_models; i < sv_numModels; i++, model++ ) {
        if ( !model->type ) {
            continue;
        }

        Hunk_Free( &model->hunk );
        memset( model, 0, sizeof( *model ) );
    }

    sv_numModels = 0;
}

/**
*   @brief
**/
//int SV_Models_ValidateMD2( dmd2header_t *header, size_t length ) {
//    size_t end;
//
//    // check ident and version
//    if ( header->ident != MD2_IDENT )
//        return Q_ERR_UNKNOWN_FORMAT;
//    if ( header->version != MD2_VERSION )
//        return Q_ERR_UNKNOWN_FORMAT;
//
//    // check triangles
//    if ( header->num_tris < 1 )
//        return Q_ERR_TOO_FEW;
//    if ( header->num_tris > TESS_MAX_INDICES / 3 )
//        return Q_ERR_TOO_MANY;
//
//    end = header->ofs_tris + sizeof( dmd2triangle_t ) * header->num_tris;
//    if ( header->ofs_tris < sizeof( *header ) || end < header->ofs_tris || end > length )
//        return Q_ERR_BAD_EXTENT;
//    if ( header->ofs_tris % q_alignof( dmd2triangle_t ) )
//        return Q_ERR_BAD_ALIGN;
//
//    // check st
//    if ( header->num_st < 3 )
//        return Q_ERR_TOO_FEW;
//    if ( header->num_st > INT_MAX / sizeof( dmd2stvert_t ) )
//        return Q_ERR_TOO_MANY;
//
//    end = header->ofs_st + sizeof( dmd2stvert_t ) * header->num_st;
//    if ( header->ofs_st < sizeof( *header ) || end < header->ofs_st || end > length )
//        return Q_ERR_BAD_EXTENT;
//    if ( header->ofs_st % q_alignof( dmd2stvert_t ) )
//        return Q_ERR_BAD_ALIGN;
//
//    // check xyz and frames
//    if ( header->num_xyz < 3 )
//        return Q_ERR_TOO_FEW;
//    if ( header->num_xyz > MD2_MAX_VERTS )
//        return Q_ERR_TOO_MANY;
//    if ( header->num_frames < 1 )
//        return Q_ERR_TOO_FEW;
//    if ( header->num_frames > MD2_MAX_FRAMES )
//        return Q_ERR_TOO_MANY;
//
//    end = sizeof( dmd2frame_t ) + ( header->num_xyz - 1 ) * sizeof( dmd2trivertx_t );
//    if ( header->framesize < end || header->framesize > MD2_MAX_FRAMESIZE )
//        return Q_ERR_BAD_EXTENT;
//    if ( header->framesize % q_alignof( dmd2frame_t ) )
//        return Q_ERR_BAD_ALIGN;
//
//    end = header->ofs_frames + (size_t)header->framesize * header->num_frames;
//    if ( header->ofs_frames < sizeof( *header ) || end < header->ofs_frames || end > length )
//        return Q_ERR_BAD_EXTENT;
//    if ( header->ofs_frames % q_alignof( dmd2frame_t ) )
//        return Q_ERR_BAD_ALIGN;
//
//    // check skins
//    if ( header->num_skins ) {
//        if ( header->num_skins > MD2_MAX_SKINS )
//            return Q_ERR_TOO_MANY;
//
//        end = header->ofs_skins + (size_t)MD2_MAX_SKINNAME * header->num_skins;
//        if ( header->ofs_skins < sizeof( *header ) || end < header->ofs_skins || end > length )
//            return Q_ERR_BAD_EXTENT;
//    }
//
//    if ( header->skinwidth < 1 || header->skinwidth > MD2_MAX_SKINWIDTH )
//        return Q_ERR_INVALID_FORMAT;
//    if ( header->skinheight < 1 || header->skinheight > MD2_MAX_SKINHEIGHT )
//        return Q_ERR_INVALID_FORMAT;
//
//    return Q_ERR_SUCCESS;
//}

/**
*   @brief
**/
static model_class_t
get_model_class( const char *name ) {
    //if ( !strcmp( name, "models/objects/explode/tris.md2" ) )
    //    return MCLASS_EXPLOSION;
    //else if ( !strcmp( name, "models/objects/r_explode/tris.md2" ) )
    //    return MCLASS_EXPLOSION;
    //else if ( !strcmp( name, "models/objects/flash/tris.md2" ) )
    //    return MCLASS_FLASH;
    //else if ( !strcmp( name, "models/objects/smoke/tris.md2" ) )
    //    return MCLASS_SMOKE;
    //else if ( !strcmp( name, "models/objects/minelite/light2/tris.md2" ) )
    //    return MCLASS_STATIC_LIGHT;
    //else if ( !strcmp( name, "models/objects/flare/tris.md2" ) )
    //    return MCLASS_FLARE;
    //else
        return MCLASS_REGULAR;
}

/**
*   @brief  
**/
int SV_Models_LoadIQM( model_t *model, const void *rawdata, size_t length, const char *mod_name ) {
    Hunk_Begin( &model->hunk, 0x4000000 );
    model->type = model_t::MOD_ALIAS;

    int res = MOD_LoadIQM_Base( model, rawdata, length, mod_name ), ret;

    if ( res != Q_ERR_SUCCESS ) {
        Hunk_Free( &model->hunk );
        return res;
    }

    // Parse out base model path for alias name.
    char base_path[ MAX_OSPATH ] = {};
    COM_FilePath( mod_name, base_path, sizeof( base_path ) );

    // Extend the base model path to check on a skeletal model configuration file.
    char skm_config_path[ MAX_OSPATH ];
    memset( skm_config_path, 0, sizeof( skm_config_path ) );
    const int32_t len = Q_snprintf( skm_config_path, MAX_OSPATH, "%s/tris.skc", base_path );

    // If it had a succesfull length, head on to parsing the actual config file.
    if ( len > 4 ) {
        char *skm_config_file_buffer = NULL;
        int32_t skc_load_result = 0;
        skm_config_file_buffer = SKM_LoadConfigurationFile( model, skm_config_path, &skc_load_result );
        // If succesfully loaded, parse the buffer.
        if ( skc_load_result ) {
            // Move on to parsing the actual configuration file and storing the needed data into the model_t struct.
            SKM_ParseConfigurationBuffer( model, skm_config_path, skm_config_file_buffer );
        }

        // Make sure to free up the buffer now we're done parsing the skeletal model config script.
        Z_Free( skm_config_file_buffer );
    }

    CHECK( model->meshes = static_cast<maliasmesh_t*>( MOD_Malloc( &model->hunk, sizeof( maliasmesh_t ) * model->iqmData->num_meshes ) ) );
    model->nummeshes = (int)model->iqmData->num_meshes;
    model->numframes = 1; // these are baked frames, so that the VBO uploader will only make one copy of the vertices

    for ( unsigned model_idx = 0; model_idx < model->iqmData->num_meshes; model_idx++ ) {
        iqm_mesh_t *iqm_mesh = &model->iqmData->meshes[ model_idx ];
        maliasmesh_t *mesh = &model->meshes[ model_idx ];

        mesh->indices = iqm_mesh->data->indices ? (int *)iqm_mesh->data->indices + iqm_mesh->first_triangle * 3 : NULL;
        mesh->positions = iqm_mesh->data->positions ? (vec3_t *)( iqm_mesh->data->positions + iqm_mesh->first_vertex * 3 ) : NULL;
        mesh->normals = iqm_mesh->data->normals ? (vec3_t *)( iqm_mesh->data->normals + iqm_mesh->first_vertex * 3 ) : NULL;
        mesh->tex_coords = iqm_mesh->data->texcoords ? (vec2_t *)( iqm_mesh->data->texcoords + iqm_mesh->first_vertex * 2 ) : NULL;
        mesh->tangents = iqm_mesh->data->tangents ? (vec3_t *)( iqm_mesh->data->tangents + iqm_mesh->first_vertex * 3 ) : NULL;
        mesh->blend_indices = iqm_mesh->data->blend_indices ? (uint32_t *)( iqm_mesh->data->blend_indices + iqm_mesh->first_vertex * 4 ) : NULL;
        mesh->blend_weights = iqm_mesh->data->blend_weights ? (uint32_t *)( iqm_mesh->data->blend_weights + iqm_mesh->first_vertex * 4 ) : NULL;

        mesh->numindices = (int)( iqm_mesh->num_triangles * 3 );
        mesh->numverts = (int)iqm_mesh->num_vertexes;
        mesh->numtris = (int)iqm_mesh->num_triangles;

        // convert the indices from IQM global space to mesh-local space; fix winding order.
        for ( unsigned triangle_idx = 0; triangle_idx < iqm_mesh->num_triangles; triangle_idx++ ) {
            int tri[ 3 ];
            tri[ 0 ] = mesh->indices[ triangle_idx * 3 + 0 ];
            tri[ 1 ] = mesh->indices[ triangle_idx * 3 + 1 ];
            tri[ 2 ] = mesh->indices[ triangle_idx * 3 + 2 ];

            mesh->indices[ triangle_idx * 3 + 0 ] = tri[ 2 ] - (int)iqm_mesh->first_vertex;
            mesh->indices[ triangle_idx * 3 + 1 ] = tri[ 1 ] - (int)iqm_mesh->first_vertex;
            mesh->indices[ triangle_idx * 3 + 2 ] = tri[ 0 ] - (int)iqm_mesh->first_vertex;
        }

        char filename[ MAX_QPATH ];
        Q_snprintf( filename, sizeof( filename ), "%s/%s.pcx", base_path, iqm_mesh->material );
        // WID: TODO: Implement?
        //pbr_material_t *mat = MAT_Find( filename, IT_SKIN, IF_NONE );
        //assert( mat ); // it's either found or created

        // WID: Each mesh, has a string name of its "material", these should be listed and counted in order
        // to acquire the number of materials.
        // WID: It is never allocated elsewhere!
        //CHECK( mesh->materials = static_cast<sv_model_material_t**>( MOD_Malloc( &model->hunk, sizeof( sv_model_material_t * ) /* * model->iqmData->num_materials*/ ) ) );
        //Q_snprintf( mesh->materials[ 0 ]->name, MAX_QPATH, "%s", filename );
        // WID: TODO: Implement?
        //mesh->materials[ 0 ] = mat;
        mesh->numskins = 1; // looks like IQM only supports one skin?
    }

    //compute_missing_model_tangents( model );

    //extract_model_lights( model );

    Hunk_End( &model->hunk );

    return Q_ERR_SUCCESS;

fail:
    return ret;
}

///**
//*   @brief
//**/
//static int SV_Models_LoadSP2( model_t *model, const void *rawdata, size_t length, const char *mod_name ) {
//    dsp2header_t header;
//    dsp2frame_t *src_frame;
//    mspriteframe_t *dst_frame;
//    char buffer[ SP2_MAX_FRAMENAME ];
//    int i, ret;
//
//    if ( length < sizeof( header ) )
//        return Q_ERR_FILE_TOO_SMALL;
//
//    // byte swap the header
//    LittleBlock( &header, rawdata, sizeof( header ) );
//
//    if ( header.ident != SP2_IDENT )
//        return Q_ERR_UNKNOWN_FORMAT;
//    if ( header.version != SP2_VERSION )
//        return Q_ERR_UNKNOWN_FORMAT;
//    if ( header.numframes < 1 ) {
//        // empty models draw nothing
//        model->type = SV_Models_EMPTY;
//        return Q_ERR_SUCCESS;
//    }
//    if ( header.numframes > SP2_MAX_FRAMES )
//        return Q_ERR_TOO_MANY;
//    if ( sizeof( dsp2header_t ) + sizeof( dsp2frame_t ) * header.numframes > length )
//        return Q_ERR_BAD_EXTENT;
//
//    Hunk_Begin( &model->hunk, sizeof( mspriteframe_t ) * header.numframes );
//    model->type = SV_Models_SPRITE;
//
//    CHECK( model->spriteframes = SV_Models_Malloc( sizeof( mspriteframe_t ) * header.numframes ) );
//    model->numframes = header.numframes;
//
//    src_frame = (dsp2frame_t *)( (byte *)rawdata + sizeof( dsp2header_t ) );
//    dst_frame = model->spriteframes;
//    for ( i = 0; i < header.numframes; i++ ) {
//        dst_frame->width = (int32_t)LittleLong( src_frame->width );
//        dst_frame->height = (int32_t)LittleLong( src_frame->height );
//
//        dst_frame->origin_x = (int32_t)LittleLong( src_frame->origin_x );
//        dst_frame->origin_y = (int32_t)LittleLong( src_frame->origin_y );
//
//        if ( !Q_memccpy( buffer, src_frame->name, 0, sizeof( buffer ) ) ) {
//            Com_WPrintf( "%s has bad frame name\n", model->name );
//            dst_frame->image = R_NOTEXTURE;
//        } else {
//            FS_NormalizePath( buffer );
//            dst_frame->image = IMG_Find( buffer, IT_SPRITE, IF_SRGB );
//        }
//
//        src_frame++;
//        dst_frame++;
//    }
//
//    Hunk_End( &model->hunk );
//
//    return Q_ERR_SUCCESS;
//
//fail:
//    return ret;
//}

/**
*   @brief  Increments registration sequence for loading.
**/
void SV_Models_IncrementRegistrationSequence() {
    sv_model_registration_sequence++;
}

void SV_Models_Reference( model_t *model ) {
    int32_t mesh_idx = 0;
    int32_t skin_idx = 0;
    int32_t frame_idx = 0;

    // register any images used by the models
    switch ( model->type ) {
    case model_s::MOD_ALIAS:
        //for ( mesh_idx = 0; mesh_idx < model->nummeshes; mesh_idx++ ) {
        //    maliasmesh_t *mesh = &model->meshes[ mesh_idx ];
        //    for ( skin_idx = 0; skin_idx < mesh->numskins; skin_idx++ ) {
        //        MAT_UpdateRegistration( mesh->materials[ skin_idx ] );
        //    }
        //}
        break;
    case model_s::MOD_SPRITE:
        //for ( frame_idx = 0; frame_idx < model->numframes; frame_idx++ ) {
        //    model->spriteframes[ frame_idx ].image->registration_sequence = registration_sequence;
        //}
        break;
    case model_s::MOD_EMPTY:
        break;
    default:
        Q_assert( !"bad model type" );
    }

    model->registration_sequence = sv_model_registration_sequence;
}

#define TRY_MODEL_SRC_GAME      1
#define TRY_MODEL_SRC_BASE      0

/**
*   @brief
**/
qhandle_t SV_RegisterModel( const char *name ) {
    char normalized[ MAX_QPATH ] = {};
    qhandle_t index = 0;
    size_t namelen = 0;
    int filelen = 0;
    model_t *model = nullptr;
    byte *rawdata = nullptr;
    uint32_t ident = 0;
    mod_load_callback_t load = 0;
    int ret = 0;

    // empty names are legal, silently ignore them
    if ( !*name ) {
        return 0;
    }

    // Inline BSP model:
    if ( *name == '*' ) {
        // Index number comes after the asterix in the string.
        index = atoi( name + 1 );
        return ~index;
    }

    // Normalize the path.
    namelen = FS_NormalizePathBuffer( normalized, name, MAX_QPATH );

    // This should never happen.
    if ( namelen >= MAX_QPATH ) {
        Com_Error( ERR_DROP, "%s: oversize name", __func__ );
    }

    // Normalized to empty name?
    if ( namelen == 0 ) {
        Com_DPrintf( "%s: empty name\n", __func__ );
        return 0;
    }

    char *extension = normalized + namelen - 4;
    if ( namelen > 4 && ( strcmp( extension, ".iqm" ) != 0 ) ) {
        Com_DPrintf( "%s: non-iqm file, \"%s\"\n", __func__, normalized );
        return 0;
    }
    // See if it's already loaded.
    model = SV_Models_Find( normalized );
    if ( model ) {
        SV_Models_Reference( model );
        goto done;
    }

    // Always prefer models from the game dir, even if format might be 'inferior'
    for ( int try_location = Q_stricmp( fs_game->string, BASEGAME ) ? TRY_MODEL_SRC_GAME : TRY_MODEL_SRC_BASE;
        try_location >= TRY_MODEL_SRC_BASE; try_location-- ) 
    {
        int fs_flags = 0;
        if ( try_location > 0 ) {
            fs_flags = try_location == TRY_MODEL_SRC_GAME ? FS_PATH_GAME : FS_PATH_BASE;
        }

        char *extension = normalized + namelen - 4;
        bool try_md3 = false; // cls.ref_type == REF_TYPE_VKPT || ( cls.ref_type == REF_TYPE_GL && gl_use_hd_assets->integer );
        //if ( namelen > 4 && ( strcmp( extension, ".md2" ) == 0 ) && try_md3 ) {
        //    memcpy( extension, ".md3", 4 );

        //    filelen = FS_LoadFileFlags( normalized, (void **)&rawdata, fs_flags );

        //    memcpy( extension, ".md2", 4 );
        //}
        if ( !rawdata ) {
            filelen = FS_LoadFileFlags( normalized, (void **)&rawdata, fs_flags );
        }
        if ( rawdata )
            break;
    }

    if ( !rawdata ) {
        filelen = FS_LoadFile( normalized, (void **)&rawdata );
        if ( !rawdata ) {
            // don't spam about missing models
            if ( filelen == Q_ERR( ENOENT ) ) {
                return 0;
            }

            ret = filelen;
            goto fail1;
        }
    }

    if ( filelen < 4 ) {
        ret = Q_ERR_FILE_TOO_SMALL;
        goto fail2;
    }

    // Check ident
    ident = LittleLong( *(uint32_t *)rawdata );
    switch ( ident ) {
    //case MD2_IDENT:
    //    load = SV_Models_LoadMD2;
    //    break;
    //    #if USE_MD3
    //case MD3_IDENT:
    //    load = SV_Models_LoadMD3;
    //    break;
    //    #endif
    //case SP2_IDENT:
    //    load = SV_Models_LoadSP2;
    //    break;
    case IQM_IDENT:
        load = SV_Models_LoadIQM;
        break;
    default:
        ret = Q_ERR_SUCCESS;
        goto silent_exit;
    }

    if ( !load ) {
        ret = Q_ERR_UNKNOWN_FORMAT;
        goto fail2;
    }

    model = SV_Models_Alloc();
    if ( !model ) {
        ret = Q_ERR_OUT_OF_SLOTS;
        goto fail2;
    }

    memcpy( model->name, normalized, namelen + 1 );
    model->registration_sequence = sv_model_registration_sequence;

    ret = load( model, rawdata, filelen, name );

    FS_FreeFile( rawdata );

    if ( ret ) {
        memset( model, 0, sizeof( *model ) );
        goto fail1;
    }

    model->model_class = get_model_class( model->name );

done:
    index = ( model - sv_models ) + 1;
    return index;
// silent_exit: Used for exit case where the model format is not needed to be loaded by the server.
silent_exit:
    FS_FreeFile( rawdata );
    return Q_ERR_SUCCESS;
fail2:
    FS_FreeFile( rawdata );
fail1:
    Com_EPrintf( "Couldn't load %s: %s\n", normalized, Q_ErrorString( ret ) );
    return 0;
}

/**
*   @brief  Initialize server models memory cache.
**/
void SV_Models_Init( void ) {
    // Prevent initializing twice, thus requiring a shutdown to occure before calling this function.
    Q_assert( !sv_numModels );
    // Add server model list cmd.
    Cmd_AddCommand( "sv_modellist", SV_Models_List_f );

    // Initial registration sequence.
    SV_Models_IncrementRegistrationSequence();
}

/**
*   @brief  Shutdown and free server models memory cache.
**/
void SV_Models_Shutdown( void ) {
    // Free all models from memory.
    SV_Models_FreeAll();

    // Unregister command(s).
    Cmd_RemoveCommand( "sv_modellist" );
}

