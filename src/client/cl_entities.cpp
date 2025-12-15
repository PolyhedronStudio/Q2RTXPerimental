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
// cl_ents.c -- entity parsing and management

#include "cl_client.h"



/**
*   @brief  For debugging problems when out-of-date entity origin is referenced.
**/
#if USE_DEBUG
void CL_CheckEntityPresent( const int32_t entityNumber, const char *what ) {
    if ( entityNumber == cl.frame.ps.clientNumber + 1 ) {
        return; // player entity = current
    }

    centity_t *e = ENTITY_FOR_NUMBER( entityNumber ); //e = &cl_entities[entnum];

    if ( e && e->serverframe == cl.frame.number ) {
        return; // current
    }

    if ( e && e->serverframe ) {
        Com_LPrintf( PRINT_DEVELOPER, "SERVER BUG: %s on entity %d last seen %d frames ago\n", what, entityNumber, cl.frame.number - e->serverframe );
    } else {
        Com_LPrintf( PRINT_DEVELOPER, "SERVER BUG: %s on entity %d never seen before\n", what, entityNumber );
    }
}
#endif



/*(
*
*
*
*   Frame Parsing:
* 
*
**/
/**
*   @brief  Prepares the client state for 'activation', also setting the predicted values
*           to those of the initially received first valid frame.
**/
static void CL_SetInitialServerFrame(void)
{
    cls.state = ca_active;

    // Delta  
    cl.serverdelta = Q_align(cl.frame.number, 1);
    // Set time, needed for demos
    cl.time = cl.extrapolatedTime = 0; 
    // Initialize oldframe in a way that the lerp to come, won't distort anything.
    cl.oldframe.valid = false;
    cl.oldframe.ps = cl.frame.ps;

    // Determine the current delta frame's server time.
    cl.servertime = cl.frame.number * CL_FRAMETIME;

	// Set frameflags.
    cl.frameflags = FF_NONE;
	// Set initial sequence.
    cl.initialSeq = cls.netchan.outgoing_sequence;

	// Set the client prediction to the initial frame.
    clge->Frame_SetInitialServerFrame();

    //! Get rid of loading plaque.
    SCR_EndLoadingPlaque();     
    SCR_LagClear();
    //! Get rid of connection screen.
    Con_Close(false);           

    // Check for pause.
    CL_CheckForPause();
    // Update frame times.
    CL_UpdateFrameTimes();

    // Fire a local trigger in case it is not a demo playback.
    if (!cls.demo.playback) {
        EXEC_TRIGGER(cl_beginmapcmd);
        Cmd_ExecTrigger("#cl_enterlevel");
    }
}

/**
*   @brief  Called after finished parsing the frame data. All entity states and the
*           player state will be transitioned and/or reset as needed, to make sure
*           the client has a proper view of the current server frame. It does the following:
*               - Will switch the clientstatic state to 'ca_active' if it is the first
*                 parsed valid frame and the client is done precaching all data.
*               - Sets the client servertime.
*               - Rebuilds the solid entity list for this frame.
*               - Updates all entity states for this frame.
*               - Fires any entity events for this frame.
*               - Updates the predicted frame.
*               - Initializes the player's own entity position from the playerstate.
*               - Lerps or snaps the playerstate between the old and current frame.
*               - Checks for prediction errors.
**/
void CL_TransitionServerFrames( void ) {
    // Getting a valid frame message ends the connection process
    if ( cl.frame.valid && cls.state == ca_precached ) {
        CL_SetInitialServerFrame();
    }

    // Determine the current delta frame's server time.
    int64_t framenum = cl.frame.number - cl.serverdelta;
    cl.servertime = framenum * CL_FRAMETIME;

	// Let the client game handle the frame transition.
    clge->Frame_TransitionToNext();
}

/**
*   @brief  The sound code makes callbacks to the client for entitiy position
*           information, so entities can be dynamically re-spatialized.
**/
void CL_GetEntitySoundOrigin( const int32_t entityNumber, vec3_t org ) {
    clge->GetEntitySoundOrigin( entityNumber, org );
}

/**
*   @brief  Used by the sound code in order to determine the reverb effect to apply for the entity's origin.
**/
qhandle_t CL_GetEAXBySoundOrigin( const int32_t entityNumber, vec3_t org ) {
    return clge->GetEAXBySoundOrigin( entityNumber, org );
}