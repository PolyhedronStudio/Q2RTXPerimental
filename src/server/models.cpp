/*
Copyright (C) 1997-2001 Id Software, Inc.

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

#include "server.h"

#include "format/md2.h"
#if USE_MD3
#include "format/md3.h"
#endif
#include "format/sp2.h"
#include "format/iqm.h"


/**
*	@brief	sv_modellist implementation.
**/
static void MOD_SV_List_f( void ) {
	static const char types[ 5 ] = "FASE";
	int     i, count;
	model_t *model;
	size_t  bytes;

	Com_Printf( "------------------\n" );
	bytes = count = 0;

	for ( i = 0, model = sv_precache.models; i < sv_precache.numModels; i++, model++ ) {
		if ( !model->type ) {
			continue;
		}
		Com_Printf( "%c %8zu : %s\n", types[ model->type ],
				   model->hunk.mapped, model->name );
		bytes += model->hunk.mapped;
		count++;
	}
	Com_Printf( "Total server models: %d (out of %d slots)\n", count, sv_precache.numModels );
	Com_Printf( "Total server resident bytes: %zu\n", bytes );
}

void MOD_SV_Init( void ) {
	Q_assert( !sv_precache.numModels );
	Cmd_AddCommand( "sv_modellist", MOD_SV_List_f );
}

/**
*	@brief	SV Specific model shutdown.
**/
void MOD_SV_Shutdown( void ) {
	MOD_FreeAll( &sv_precache );
	Cmd_RemoveCommand( "sv_modellist" );
}

/**
*	@brief	Server specific "Touch" model reference implementation.
**/
void MOD_SV_Reference( model_t *model ) {
	// WID: For now we won't be needing this, if ever at all. But let's keep it around just in case.
	// register any images used by the models
	//switch ( model->type ) {
	//	case model_s::MOD_ALIAS:
	//		//for ( mesh_idx = 0; mesh_idx < model->nummeshes; mesh_idx++ ) {
	//		//	maliasmesh_t *mesh = &model->meshes[ mesh_idx ];
	//		//	for ( skin_idx = 0; skin_idx < mesh->numskins; skin_idx++ ) {
	//		//		MAT_UpdateRegistration( mesh->materials[ skin_idx ] );
	//		//	}
	//		//}
	//		break;
	//	case model_s::MOD_SPRITE:
	//		//for ( frame_idx = 0; frame_idx < model->numframes; frame_idx++ ) {
	//		//	model->spriteframes[ frame_idx ].image->registration_sequence = cl_precache.registration_sequence;
	//		//}
	//		break;
	//	case model_s::MOD_EMPTY:
	//		break;
	//	default:
	//		Q_assert( !"bad model type" );
	//}

	// Update its registration sequence.
	model->registration_sequence = sv_precache.registration_sequence;
}

/**
*	@brief	We're only interested in skeletal models on the server side. Therefor, always return success on other formats.
**/
int MOD_LoadSTUB_SV( model_t *model, const void *rawdata, size_t length, const char *mod_name ) {
	return Q_ERR_SUCCESS;
}

/**
*	@brief	Server-Side IQM Load Callback.
**/
typedef struct maliasmesh_s {
	int             numverts;
	int             numtris;
	int             numindices;
	int             tri_offset; /* offset in vertex buffer on device */
	int	*indices;
	vec3_t *positions;
	vec3_t *normals;
	vec2_t *tex_coords;
	vec3_t *tangents;
	uint32_t *blend_indices; // iqm only
	uint32_t *blend_weights; // iqm only
	//struct pbr_material_s *materials[ MAX_ALIAS_SKINS ];
	int             numskins;
	bool            handedness;
} maliasmesh_t;

int MOD_LoadIQM_SV( model_t *model, const void *rawdata, size_t length, const char *mod_name ) {
	Hunk_Begin( &model->hunk, 0x4000000 );
	model->type = model_s::MOD_ALIAS;

	int res = MOD_LoadIQM_Base( model, rawdata, length, mod_name ), ret;

	if ( res != Q_ERR_SUCCESS ) {
		Hunk_Free( &model->hunk );
		return res;
	}

	char base_path[ MAX_QPATH ];
	COM_FilePath( mod_name, base_path, sizeof( base_path ) );

	CHECK( model->meshes = (maliasmesh_t*)MOD_Malloc( sizeof( maliasmesh_t ) * model->iqmData->num_meshes ) );
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
	}

	Hunk_End( &model->hunk );

	return Q_ERR_SUCCESS;

fail:
	return ret;
}


/**
*	@brief	Server Side version of R_RegisterModel.
**/
#define TRY_MODEL_SRC_GAME      1
#define TRY_MODEL_SRC_BASE      0

qhandle_t MOD_SV_RegisterModel( const char *name ) {
	char normalized[ MAX_QPATH ];
	qhandle_t index;
	size_t namelen;
	int filelen = 0;
	model_t *model;
	byte *rawdata = NULL;
	uint32_t ident;
	mod_load_t load;
	int ret;

	// empty names are legal, silently ignore them
	if ( !*name )
		return 0;

	if ( *name == '*' ) {
		// inline bsp model
		index = atoi( name + 1 );
		return ~index;
	}

	// normalize the path
	namelen = FS_NormalizePathBuffer( normalized, name, MAX_QPATH );

	// this should never happen
	if ( namelen >= MAX_QPATH )
		Com_Error( ERR_DROP, "%s: oversize name", __func__ );

	// normalized to empty name?
	if ( namelen == 0 ) {
		Com_DPrintf( "%s: empty name\n", __func__ );
		return 0;
	}

	// see if it's already loaded
	model = MOD_Find( &sv_precache, normalized );
	if ( model ) {
		MOD_SV_Reference( model );
		goto done;
	}

	// Always prefer models from the game dir, even if format might be 'inferior'
	for ( int try_location = Q_stricmp( fs_game->string, BASEGAME ) ? TRY_MODEL_SRC_GAME : TRY_MODEL_SRC_BASE;
		 try_location >= TRY_MODEL_SRC_BASE;
		 try_location-- ) {
		int fs_flags = 0;
		if ( try_location > 0 )
			fs_flags = try_location == TRY_MODEL_SRC_GAME ? FS_PATH_GAME : FS_PATH_BASE;

		char *extension = normalized + namelen - 4;
		bool try_md3 = true; //cls.ref_type == REF_TYPE_VKPT || ( cls.ref_type == REF_TYPE_GL && gl_use_hd_assets->integer );
		if ( namelen > 4 && ( strcmp( extension, ".md2" ) == 0 ) && try_md3 ) {
			memcpy( extension, ".md3", 4 );

			filelen = FS_LoadFileFlags( normalized, (void **)&rawdata, fs_flags );

			memcpy( extension, ".md2", 4 );
		}
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

	// check ident
	ident = LittleLong( *(uint32_t *)rawdata );
	switch ( ident ) {
		case MD2_IDENT:
			load = MOD_LoadSTUB_SV;
			break;
			#if USE_MD3
		case MD3_IDENT:
			load = MOD_LoadSTUB_SV;
			break;
			#endif
		case SP2_IDENT:
			load = MOD_LoadSTUB_SV;
			break;
		case IQM_IDENT:
			load = MOD_LoadIQM_SV;
			break;
		default:
			ret = Q_ERR_UNKNOWN_FORMAT;
			goto fail2;
	}

	if ( !load ) {
		ret = Q_ERR_UNKNOWN_FORMAT;
		goto fail2;
	}

	model = MOD_Alloc( &sv_precache );
	if ( !model ) {
		ret = Q_ERR_OUT_OF_SLOTS;
		goto fail2;
	}

	memcpy( model->name, normalized, namelen + 1 );
	model->registration_sequence = sv_precache.registration_sequence;

	ret = load( model, rawdata, filelen, name );

	FS_FreeFile( rawdata );

	if ( ret ) {
		memset( model, 0, sizeof( *model ) );
		goto fail1;
	}

	// WID: Do not need this here.
	//model->model_class = get_model_class( model->name );

done:
	index = ( model - sv_precache.models ) + 1;
	return index;

fail2:
	FS_FreeFile( rawdata );
fail1:
	Com_EPrintf( "Server couldn't load %s: %s\n", normalized, Q_ErrorString( ret ) );
	return 0;
}