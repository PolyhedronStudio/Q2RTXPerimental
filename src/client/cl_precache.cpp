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
    char *name = cl.configstrings[ CS_MODELS + 1 ];
    int i, ret;

    if ( !name[ 0 ] ) {
        Com_Error( ERR_DROP, "%s: no map set", __func__ );
    }
    
    ret = CM_LoadMap( &cl.collisionModel, name );   //ret = BSP_Load( name, &cl.collisionModel->cache );

    if ( cl.collisionModel.cache == nullptr ) {
        Com_Error( ERR_DROP, "Couldn't load %s: %s", name, BSP_ErrorString( ret ) );
    }

    if ( cl.collisionModel.cache->checksum != atoi( cl.configstrings[ CS_MAPCHECKSUM ] ) ) {
        if (cls.demo.playback) {
            Com_WPrintf("Local map version differs from demo: %i != %s\n",
                        cl.collisionModel.cache->checksum, cl.configstrings[CS_MAPCHECKSUM]);
        } else {
            Com_Error(ERR_DROP, "Local map version differs from server: %i != %s",
                      cl.collisionModel.cache->checksum, cl.configstrings[CS_MAPCHECKSUM]);
        }
    }

    for (i = 1; i < MAX_MODELS; i++) {
        name = cl.configstrings[CS_MODELS + i];
        if ( !name[ 0 ] && i != MODELINDEX_PLAYER ) {
            break;
        }
        if (name[0] == '*')
            cl.model_clip[i] = BSP_InlineModel( cl.collisionModel.cache, name );
        else
            cl.model_clip[i] = NULL;
    }
}

/*
=================
CL_PrecacheViewModels

Builds a list of visual weapon models
=================
*/
void CL_PrecacheViewModels(void)
{
    // The client game is in control of these.
    clge->PrecacheViewModels();
}

/*
=================
CL_SetSky

=================
*/
void CL_SetSky(void)
{
    float       rotate = 0;
    int         autorotate = 1;
    vec3_t      axis;

    sscanf(cl.configstrings[CS_SKYROTATE], "%f %d", &rotate, &autorotate);
    if (sscanf(cl.configstrings[CS_SKYAXIS], "%f %f %f",
               &axis[0], &axis[1], &axis[2]) != 3) {
        Com_DPrintf("Couldn't parse CS_SKYAXIS\n");
        VectorClear(axis);
    }

    R_SetSky(cl.configstrings[CS_SKY], rotate, autorotate, axis);
}

/**
*   @brief  Called before entering a new level, or after changing dlls
**/
void CL_PrepRefresh(void)
{
    int         i;
    char        *name;

    if (!cls.ref_initialized)
        return;
    if (!cl.mapname[0])
        return;     // no map loaded

    // register models, pics, and skins
    R_BeginRegistration(cl.mapname);

    CL_LoadState(LOAD_MODELS);

    clge->PrecacheClientModels();

	if (cl_testmodel->string && cl_testmodel->string[0])
	{
		cl_testmodel_handle = R_RegisterModel(cl_testmodel->string);
		if (cl_testmodel_handle)
			Com_Printf("Loaded the test model: %s\n", cl_testmodel->string);
		else
			Com_WPrintf("Failed to load the test model from %s\n", cl_testmodel->string);
	}
	else
		cl_testmodel_handle = -1;

    for (i = 2; i < MAX_MODELS; i++) {
        name = cl.configstrings[CS_MODELS + i];
        // Ignore MODELINDEX_PLAYER slot.
        if ( !name[ 0 ] && i != MODELINDEX_PLAYER ) {
            break;
        }
        // Ignore view models here.
        if ( name[ 0 ] == '#' ) {
            continue;
        }
        cl.model_draw[i] = R_RegisterModel(name);
    }


    CL_LoadState(LOAD_IMAGES);
    for (i = 1; i < MAX_IMAGES; i++) {
        name = cl.configstrings[CS_IMAGES + i];
        if (!name[0]) {
            break;
        }
        cl.image_precache[i] = R_RegisterPic2(name);
    }

    CL_LoadState(LOAD_CLIENTS);
    for (i = 0; i < MAX_CLIENTS; i++) {
        name = cl.configstrings[CS_PLAYERSKINS + i];
        if (!name[0]) {
            continue;
        }
        CL_LoadClientinfo(&cl.clientinfo[i], name);
    }

    CL_LoadClientinfo(&cl.baseclientinfo, "unnamed\\playerdummy");

    // set sky textures and speed
    CL_SetSky();

    // the renderer can now free unneeded stuff
    R_EndRegistration();

    // clear any lines of console text
    Con_ClearNotify_f();

    SCR_UpdateScreen();

    // start the cd track
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
    // but has changed by the 'server game' module.
    if (cls.state < ca_precached) {
        return;
    }

    // Reload the refresh model data, and in case of an inline brush model
    // also load its clipping model.
    if (index >= CS_MODELS + 2 && index < CS_MODELS + MAX_MODELS) {
        int i = index - CS_MODELS;

        cl.model_draw[i] = R_RegisterModel(s);
        if (*s == '*')
            cl.model_clip[i] = BSP_InlineModel( cl.collisionModel.cache, s);
        else
            cl.model_clip[i] = NULL;
        return;
    }

    // Reload the sound.
    if (index >= CS_SOUNDS && index < CS_SOUNDS + MAX_SOUNDS) {
        cl.sound_precache[index - CS_SOUNDS] = S_RegisterSound(s);
        return;
    }

    // Reload the image.
    if (index >= CS_IMAGES && index < CS_IMAGES + MAX_IMAGES) {
        cl.image_precache[index - CS_IMAGES] = R_RegisterPic2(s);
        return;
    }

    // Reload the client info.
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
