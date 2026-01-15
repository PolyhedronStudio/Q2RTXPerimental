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

//
// cl_precache.c
//

#include "cl_client.h"
#include "common/collisionmodel.h"

/**
*   @brief  Breaks up playerskin into name (optional), model and skin components.
*           If model or skin are found to be invalid, replaces them with sane defaults.
**/
void CL_ParsePlayerSkin(char *name, char *model, char *skin, const char *s)
{
    clge->ParsePlayerSkin( name, model, skin, s );
}

/**
*   @brief
**/
void CL_LoadClientinfo( clientinfo_t *ci, const char *s ) {
    clge->PrecacheClientInfo( ci, s );
}

/**
*   @brief 
**/
void CL_RegisterSounds(void)
{
    int i;
    char    *s;

    S_BeginRegistration();
    clge->PrecacheClientSounds();
    for (i = 1; i < MAX_SOUNDS; i++) {
        s = cl.configstrings[CS_SOUNDS + i];
        if (!s[0])
            break;
        cl.sound_precache[i] = S_RegisterSound(s);
    }
    S_EndRegistration();
}

/**
*   @brief  Registers main BSP file and inline models
**/
void CL_RegisterBspModels(void) {
	// Fetch the map name from the configstrings.
    char *name = cl.configstrings[ CS_MODELS + 1 ];
	// Ensure a map name is actually set.
    if ( !name[ 0 ] ) {
        Com_Error( ERR_DROP, "%s: no map set", __func__ );
		return;
    }
    
	// Load the map data for the client's collision model.
    const int32_t ret = CM_LoadMap( &cl.collisionModel, name );   //ret = BSP_Load( name, &cl.collisionModel->cache );
	// Check for errors.
    if ( cl.collisionModel.cache == nullptr ) {
        Com_Error( ERR_DROP, "Couldn't load %s: %s", name, BSP_ErrorString( ret ) );
		return;
    }
	// Verify the map checksum.
    if ( cl.collisionModel.cache->checksum != atoi( cl.configstrings[ CS_MAPCHECKSUM ] ) ) {
		// Different map version.
		if ( cls.demo.playback ) {
			Com_WPrintf( "Local map version differs from demo: %i != %s\n",
				cl.collisionModel.cache->checksum, cl.configstrings[ CS_MAPCHECKSUM ] );
		} else {
			Com_Error( ERR_DROP, "Local map version differs from server: %i != %s",
				cl.collisionModel.cache->checksum, cl.configstrings[ CS_MAPCHECKSUM ] );
		}
		return;
    }

	/**
	*	Valid map, now iterate all inline models and register them.
	**/
    for ( int32_t i = 1; i < MAX_MODELS; i++ ) {
		// Fetch the model name from the configstrings.
        name = cl.configstrings[CS_MODELS + i];
		// Break as soon as we found an emtpty string, except for the player model slot.
        if ( !name[ 0 ] && i != MODELINDEX_PLAYER ) {
            break;
        }
		// If it starts with a '*', it's an inline model (Contained within the BSP).
		if ( name[ 0 ] == '*' ) {
			cl.model_clip[ i ] = BSP_InlineModel( cl.collisionModel.cache, name );
		} else {
			cl.model_clip[ i ] = nullptr;
		}
    }
}

/**
*	Builds a list of visual weapon models
**/
void CL_PrecacheViewModels(void)
{
    // The client game is in control of these.
    clge->PrecacheViewModels();
}

/**
*	Setup the sky properly.
**/

void CL_SetSky(void)
{
    float       rotate = 0;
    int         autorotate = 1;
    vec3_t      axis;

	// Parse sky rotation from configstrings.
	ssize_t size = sscanf( cl.configstrings[ CS_SKYROTATE ], "%f %d", &rotate, &autorotate );
	// Check sky axis.from configstrings.
	if ( sscanf( cl.configstrings[ CS_SKYAXIS ], "%f %f %f", &axis[ 0 ], &axis[ 1 ], &axis[ 2 ] ) != 3 ) {
		// Debug info.
		Com_DPrintf( "Couldn't parse CS_SKYAXIS\n" );
		// Default axis.
		VectorClear( axis );
	}
	// Set the sky in the renderer.
	R_SetSky( cl.configstrings[ CS_SKY ], rotate, autorotate, axis );
}

/**
*   @brief  Called before entering a new level, or after changing dlls
**/
void CL_PrepRefresh(void) {

	// Ensure the renderer is initialized.
	if ( !cls.ref_initialized ) {
		return;
	}
	// No map loaded?
	if ( !cl.mapname[ 0 ] ) {
		return;     // no map loaded
	}

	/**
	*	(Re-)Register models, pics, and skins.
	**/
	// Begin a new registration.sequence with the given map name.
	R_BeginRegistration( cl.mapname );
	// Switch loading state to models.
	CL_LoadState( LOAD_MODELS );
	// First register the main BSP model and inline models.
	clge->PrecacheClientModels();
	// Update the screen to make sure the changes are visible.
	SCR_UpdateScreen();

	// If the user specified a test model, load it now.
	if ( cl_testmodel->string && cl_testmodel->string[ 0 ] ) {
		cl_testmodel_handle = R_RegisterModel( cl_testmodel->string );
		if ( cl_testmodel_handle ) {
			Com_Printf( "Loaded the test model: %s\n", cl_testmodel->string );
		} else {
			Com_WPrintf( "Failed to load the test model from %s\n", cl_testmodel->string );
		}
	} else {
		cl_testmodel_handle = -1;
	}

	// Now iterate all other models and register them.
	for ( int32_t i = 2; i < MAX_MODELS; i++ ) {
		// Fetch the model name from the configstrings.
		const char *name = cl.configstrings[ CS_MODELS + i ];
		// Break as soon as we found an emtpty string, except for the player model slot.
		if ( !name[ 0 ] && i != MODELINDEX_PLAYER ) {
			break;
		}
		// Ignore view models here.
		if ( name[ 0 ] == '#' ) {
			continue;
		}
		// And register the inline model.
		cl.model_draw[ i ] = R_RegisterModel( name );
			// Update the screen to make sure the changes are visible.
		SCR_UpdateScreen();
	}

	/**
	*	Load all game images.
	**/
	// Engage image loading state.
	CL_LoadState( LOAD_IMAGES );
	// Iterate all images in the configstrings and register them.
	for ( int32_t i = 1; i < MAX_IMAGES; i++ ) {
		// Fetch the image name from the configstrings.
		const char *name = cl.configstrings[ CS_IMAGES + i ];
		// If we found an empty string, break.
		if ( !name[ 0 ] ) {
			break;
		}
		// Otherwise register the image.
		cl.image_precache[ i ] = R_RegisterImage( name, IT_PIC, IF_SRGB );//R_RegisterPic2(name);
			// Update the screen to make sure the changes are visible.
		SCR_UpdateScreen();
	}

	/**
	*	Engage into client and weapon view model precaching.
	**/
	CL_LoadState( LOAD_CLIENTS );
	for ( int32_t i = 0; i < MAX_CLIENTS; i++ ) {
		// Get the playerskin name from the configstrings.
		const char *name = cl.configstrings[ CS_PLAYERSKINS + i ];
		// Skip empty names.
		if ( !name[ 0 ] ) {
			CL_LoadClientinfo( &cl.baseclientinfo, "unnamed\\playerdummy" );
			continue;
		}
		// Load the matching specific clientinfo.
		CL_LoadClientinfo( &cl.clientinfo[ i ], name );
			// Update the screen to make sure the changes are visible.
		SCR_UpdateScreen();
	}
	// Also load the base dummy clientinfo.
	CL_LoadClientinfo( &cl.baseclientinfo, "unnamed\\playerdummy" );

	/**
	*	Set sky textures and speed.
	**/
	CL_SetSky();

	/**
	*	The renderer can now free any unneeded stuff.
	*	(Which was loaded in a previous map load.)
	**/
	R_EndRegistration();

	// Clear any lines of console text that were around.
	Con_ClearNotificationTexts_f();
	// Update the screen to make sure the changes are visible.
	SCR_UpdateScreen();

    /**
	*	Start the cd track (if any.)
	**/
    OGG_Play();
}

/*
=================
CL_UpdateConfigstring

A configstring update has been parsed.
=================
*/
void CL_UpdateConfigstring( const int32_t index ) {
    const char *s = cl.configstrings[index];

    // Allow the client game to interscept the config string.
    if ( clge->UpdateConfigString( index ) ) {
        return;
    }

    // Parse any max client changes.
    if (index == CS_MAXCLIENTS) {
        cl.maxclients = atoi(s);
        return;
    }

    // Parse the actual map name.
    if (index == CS_MODELS + 1) {
        if ( !Com_ParseMapName( cl.mapname, s, sizeof( cl.mapname ) ) ) {
            Com_Error( ERR_DROP, "%s: bad world model: %s", __func__, s );
        }
        return;
    }

    // Moved to Client Game: Return for safety reasons, in case the client game 
    // forgot to return 'true' for intersception.
    if ( index == CS_AIRACCEL ) {
        //cl.pmp.airaccelerate = cl.pmp.qwmode || atoi(s);
        return;
    }
    // Moved to Client Game: Return for safety reasons, in case the client game 
    // forgot to return 'true' for intersception.
    if ( index >= CS_LIGHTS && index < CS_LIGHTS + MAX_LIGHTSTYLES ) {
        //CL_SetLightStyle(index - CS_LIGHTS, s);
        return;
    }

    // Any updates below are for 'in-game' state where data was precached,
    // but the CONFIGSTRING has changed by the 'server game' module.
	if ( cls.state < ca_precached ) {
		return;
	}

    // Reload the refresh model data, and in case of an inline brush model
    // also load its clipping model.
    if (index >= CS_MODELS + 2 && index < CS_MODELS + MAX_MODELS ) {
		// Calculate the model array index.
        const int32_t i = index - CS_MODELS;
		// Register the model for drawing.
		cl.model_draw[ i ] = R_RegisterModel( s );
		// Load the inline model for clipping too.
		if ( *s == '*' ) {
			cl.model_clip[ i ] = BSP_InlineModel( cl.collisionModel.cache, s );
		} else {
			// Unset it in the clipping model array, because it has now been replaced by
			// a non-inline BSP model. It is now a regular entity model.
			cl.model_clip[ i ] = nullptr;
		}
        return;
    }

    // Reload the sound ConfigString.
    if (index >= CS_SOUNDS && index < CS_SOUNDS + MAX_SOUNDS) {
        cl.sound_precache[index - CS_SOUNDS] = S_RegisterSound(s);
        return;
    }

    // Reload the image ConfigString.
    if (index >= CS_IMAGES && index < CS_IMAGES + MAX_IMAGES) {
        cl.image_precache[index - CS_IMAGES] = R_RegisterPic2(s);
        return;
    }

    // Reload the client info ConfigString.
    if (index >= CS_PLAYERSKINS && index < CS_PLAYERSKINS + MAX_CLIENTS) {
        CL_LoadClientinfo(&cl.clientinfo[index - CS_PLAYERSKINS], s);
        return;
    }

    #if 0 // <Q2RTXP>: WID: We dun support this, so disable it.
    if (index == CS_CDTRACK) {
        OGG_Play();
        return;
    }
    #endif
}
