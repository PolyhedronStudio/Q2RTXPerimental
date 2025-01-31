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
#include "client/sound/sound.h"
#include "refresh/models.h"

extern qhandle_t cl_mod_powerscreen;
extern qhandle_t cl_mod_laser;
extern qhandle_t cl_mod_dmspot;
extern qhandle_t cl_sfx_footsteps[4];

/*
=========================================================================

FRAME PARSING

=========================================================================
*/

// returns true if origin/angles update has been optimized out
static inline bool entity_is_optimized(const entity_state_t *state)
{
    //return cls.serverProtocol == PROTOCOL_VERSION_Q2PRO
    //    && state->number == cl.frame.clientNum + 1
    //    && cl.frame.ps.pmove.pm_type < PM_DEAD;
	// WID: net-protocol2: We don't want this anyway, should get rid of it?
    //if (cls.serverProtocol != PROTOCOL_VERSION_Q2PRO)
    if ( state->number != cl.frame.clientNum + 1 ) {
        return false;
    }

    if ( cl.frame.ps.pmove.pm_type >= PM_DEAD ) {
        return false;
    }

    // WID: TODO: It does WORK with this, but do we want to do this?
    // using the entity origin for the player, wouldn't that mean
    // it is a slower update rate than the actual player is?
    if ( cls.serverProtocol == PROTOCOL_VERSION_Q2RTXPERIMENTAL ) {
        return true;
    }

    return true;
}

/**
*   @brief  Will update a new 'in-frame' entity and initialize it according to that.
**/
static inline void entity_update_new( centity_t *ent, const entity_state_t *state, const vec_t *origin ) {
    static int entity_ctr;
    ent->id = ++entity_ctr;
    ent->trailcount = 1024;     // for diminishing rocket / grenade trails

    // duplicate the current state so lerping doesn't hurt anything
    ent->prev = *state;

    #if 0
    // WID: 40hz
    // Update the animation frames and time.
    ent->current_frame = ent->last_frame = state->frame;
    ent->frame_servertime = cl.servertime;
    ent->frame_realtime = cls.realtime;
    // WID: 40hz
    #endif

    // No lerping if teleported, or a BEAM effect entity.
    if ( state->event == EV_PLAYER_TELEPORT ||
        state->event == EV_OTHER_TELEPORT ||
        ( state->entityType == ET_BEAM || state->renderfx & RF_BEAM ) ) {
        // no lerping if teleported
        VectorCopy( origin, ent->lerp_origin );
        return;
    }

    // old_origin is valid for new entities,
    // so use it as starting point for interpolating between
    VectorCopy( state->old_origin, ent->prev.origin );
    VectorCopy( state->old_origin, ent->lerp_origin );
}

/**
*   @brief  Will update an entity which was in the previous frame also.
**/
static inline void entity_update_old( centity_t *ent, const entity_state_t *state, const vec_t *origin ) {
    const int32_t event = state->event;

    // Handle proper lerping for animated entities by Hz.
    #if 0
    if ( ent->current_frame != state->frame ) {
        if ( state->renderfx & RF_OLD_FRAME_LERP ) {
            ent->last_frame = ent->current.old_frame;
        } else {
            ent->last_frame = ent->current.frame;
        }
        ent->current_frame = state->frame;
        ent->frame_servertime = cl.servertime;
        ent->frame_realtime = cls.realtime;
    }
    #endif

    // Set step height, and server time, if caught stair stepping.
    if ( state->renderfx & RF_STAIR_STEP ) {
        ent->step_height = state->origin[ 2 ] - ent->current.origin[ 2 ];
        ent->step_servertime = cl.servertime;
        ent->step_realtime = cls.realtime;
    }

    if ( state->entityType != ent->current.entityType
        || state->modelindex != ent->current.modelindex
        || state->modelindex2 != ent->current.modelindex2
        || state->modelindex3 != ent->current.modelindex3
        || state->modelindex4 != ent->current.modelindex4
        || event == EV_PLAYER_TELEPORT
        || event == EV_OTHER_TELEPORT
        || fabsf( origin[ 0 ] - ent->current.origin[ 0 ] ) > 64//512
        || fabsf( origin[ 1 ] - ent->current.origin[ 1 ] ) > 64//512
        || fabsf( origin[ 2 ] - ent->current.origin[ 2 ] ) > 64//512
        || cl_nolerp->integer == 1 ) {
        // some data changes will force no lerping
        ent->trailcount = 1024;     // for diminishing rocket / grenade trails

        // duplicate the current state so lerping doesn't hurt anything
        ent->prev = *state;

        #if 0
        // WID: 40hz
        ent->last_frame = state->frame;
        // WID: 40hz
        #endif
        // no lerping if teleported or morphed
        VectorCopy( origin, ent->lerp_origin );
        return;
    }

    // Shuffle the last 'current' state to previous
    ent->prev = ent->current;
}

/**
*   @return True if the SAME entity was NOT IN the PREVIOUS frame.
**/
static inline bool entity_is_new(const centity_t *ent) {
    // Last received frame was invalid, so this entity is new to the current frame.
    if ( !cl.oldframe.valid ) {
        return true;
    }
    // Wasn't in last received frame, so this entity is new to the current frame.
    if ( ent->serverframe != cl.oldframe.number ) {
        return true;
    }
    // Developer option, always new.
    if ( cl_nolerp->integer == 2 ) {
        return true;
    }
    //! Developer option, lerp from last received frame.
    if ( cl_nolerp->integer == 3 ) {
        return false;
    }
    //! Previous server frame was dropped, so we're new
    if ( cl.oldframe.number != cl.frame.number - 1 ) {
        return true;
    }

    return false;
}

static void parse_entity_update(const entity_state_t *state)
{
    centity_t *ent = ENTITY_FOR_NUMBER( state->number );
    const vec_t *origin;
    vec3_t origin_v;

    // If entity is solid, and not our client entity, add it to the solid entity list.
    if ( state->solid && state->number != cl.frame.clientNum + 1 && cl.numSolidEntities < MAX_PACKET_ENTITIES ) {
        // Add it to the solids entity list.
        cl.solidEntities[ cl.numSolidEntities++ ] = ent;
    }

    // If not a brush model, acquire the bounds from the state. (It will use the clip brush node its bounds otherwise.)
    if ( state->solid && state->solid != BOUNDS_BRUSHMODEL ) {
        // If not a brush bsp entity, decode its mins and maxs.
        MSG_UnpackBoundsUint32( bounds_packed_t{ .u = state->bounds }, ent->mins, ent->maxs );
    // Clear out the mins and maxs.
    } else {
        VectorClear( ent->mins );
        VectorClear( ent->maxs );
    }

    // Work around Q2PRO server bandwidth optimization.
    if ( entity_is_optimized( state ) ) {
        VectorCopy( cl.frame.ps.pmove.origin, origin_v );
        origin = origin_v;
    } else {
        origin = state->origin;
    }

    // See if the entity is new for the current serverframe or not and base our next move on that.
    if ( entity_is_new( ent ) ) {
        // Wasn't in last update, so initialize some things.
        entity_update_new( ent, state, origin );
    } else {
        // Was around, sp update some things.
        entity_update_old( ent, state, origin );
    }

    // Assign last received server frame.
    ent->serverframe = cl.frame.number;
    // Assign new state.
    ent->current = *state;

    // Work around Q2PRO server bandwidth optimization.
    if ( entity_is_optimized( state ) ) {
        Com_PlayerToEntityState( &cl.frame.ps, &ent->current );
    }
}

/**
*   @brief  An entity has just been parsed that has an event value.
**/
static void parse_entity_event(const int32_t entityNumber ) {
    clge->ParseEntityEvent( entityNumber );
}

/**
*   @brief  Prepares the client state for 'activation', also setting the predicted values
*           to those of the initially received first valid frame.
**/
static void CL_SetActiveState(void)
{
    cls.state = ca_active;

    // Delta  
    cl.serverdelta = Q_align(cl.frame.number, 1);
    // Set time, needed for demos
    cl.time = cl.servertime = 0; 

    // Initialize oldframe so lerping doesn't hurt anything.
    cl.oldframe.valid = false;
    cl.oldframe.ps = cl.frame.ps;

    cl.frameflags = 0;
    cl.initialSeq = cls.netchan.outgoing_sequence;

    if (cls.demo.playback) {
        // init some demo things
        CL_FirstDemoFrame();
    } else {
        // Set the initial client predicted state values.
        cl.predictedState.currentPs = cl.frame.ps;
        cl.predictedState.lastPs = cl.frame.ps;
        //VectorCopy(cl.frame.ps.pmove.origin, cl.predictedState.view.origin);//VectorScale(cl.frame.ps.pmove.origin, 0.125f, cl.predicted_origin); // WID: float-movement
        //VectorCopy(cl.frame.ps.pmove.velocity, cl.predictedState.view.velocity);//VectorScale(cl.frame.ps.pmove.velocity, 0.125f, cl.predicted_velocity); // WID: float-movement
        if (cl.frame.ps.pmove.pm_type < PM_DEAD &&
            cls.serverProtocol >= PROTOCOL_VERSION_Q2RTXPERIMENTAL) {
            // enhanced servers don't send viewangles
            CL_PredictAngles();
        } else {
            // just use what server provided
            VectorCopy( cl.frame.ps.viewangles, cl.predictedState.currentPs.viewangles );
            VectorCopy( cl.frame.ps.viewangles, cl.predictedState.lastPs.viewangles );
        }
    }

    // Reset local (view-)transitions.
    cl.predictedState.transition = {};
    //cl.predictedState.time.height_changed = 0;
    //cl.predictedState.time.step_changed = 0;

    // Reset ground information.
    cl.predictedState.ground = {};
    cl.predictedState.liquid = {};

    // Fire the ClientBegin callback of the client game module.
    clge->ClientBegin();

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
*   @brief  Duplicates old player state, into playerstate. Used as a utility function.
*   @param ps Pointer to the state that we want to copy data into.
*   @param ops Pointer to the state's data source.
**/
static void duplicate_player_state( player_state_t *ps, player_state_t *ops ) {
    *ops = *ps;
}

/**
*   Determine whether the player state has to lerp between the current and old frame,
*   or snap 'to'.
**/
static void CL_LerpOrSnapPlayerState( server_frame_t *oldframe, server_frame_t *frame, const int32_t framediv) {
    // Find player states to interpolate between
    player_state_t *ps = &frame->ps;
    player_state_t *ops = &oldframe->ps;

    // No lerping if previous frame was dropped or invalid.
    if ( !oldframe->valid ) {
        duplicate_player_state( ps, ops );
        return;
    }

    // Duplicate state in case the stored old frame number does not match to the
    // expected last frame number.
    const int32_t lastFrameNumber = frame->number - framediv;
    if ( oldframe->number != lastFrameNumber ) {
        duplicate_player_state( ps, ops );
        return;
    }

    // No lerping if player entity was teleported (origin check).
    if ( fabsf( ops->pmove.origin[ 0 ] - ps->pmove.origin[ 0 ] ) > 256 || // * 8 || // WID: float-movement
        fabsf( ops->pmove.origin[ 1 ] - ps->pmove.origin[ 1 ] ) > 256 || // * 8 || // WID: float-movement
        fabsf( ops->pmove.origin[ 2 ] - ps->pmove.origin[ 2 ] ) > 256 ) {// * 8) { // WID: float-movement
        duplicate_player_state( ps, ops );
        return;
    }

    // No lerping if player entity was teleported (event check).
    centity_t *clent = ENTITY_FOR_NUMBER( frame->clientNum + 1 );//ent = &cl_entities[frame->clientNum + 1];

    // If the player entity was within the range of lastFrameNumber and frame->number,
    // and had any teleport events going on, duplicate the player state into the old player state,
    // to prevent it from lerping afar distance.
    if ( clent->serverframe > lastFrameNumber && clent->serverframe <= frame->number &&
        ( clent->current.event == EV_PLAYER_TELEPORT || clent->current.event == EV_OTHER_TELEPORT ) ) {
        duplicate_player_state( ps, ops );
        return;
    }

    // No lerping if teleport bit was flipped.
    //if ((ops->pmove.pm_flags ^ ps->pmove.pm_flags) & PMF_TELEPORT_BIT){
    //    duplicate_player_state( ps, ops );
    //    return;
    //}
    //if ( ( ops->rdflags ^ ps->rdflags ) & RDF_TELEPORT_BIT ) {
    //    duplicate_player_state( ps, ops );
    //    return;
    //}

    // No lerping if POV number changed.
    if ( oldframe->clientNum != frame->clientNum ) {
        duplicate_player_state( ps, ops );
        return;
    }

    // No lerping in case of the enabled developer option.
    if ( cl_nolerp->integer == 1 ) {
        duplicate_player_state( ps, ops );
        return;
    }
}

/**
*   @brief  Called after finished parsing the frame data. All entity states and the
*           player state will be updated, and checked for 'snapping to' their new state,
*           or to smoothly lerp into it. It'll check for any prediction errors afterwards
*           also and calculate its correction value.
*           
*           Will switch the clientstatic state to 'ca_active' if it is the first
*           parsed valid frame and the client is done precaching all data.
**/
void CL_DeltaFrame(void)
{
    // Getting a valid frame message ends the connection process
    //if ( cls.state == ca_precached ) {
    if ( cl.frame.valid && cls.state == ca_precached ) {
        CL_SetActiveState();
    }

    // Determine the current delta frame's server time.
    int64_t framenum = cl.frame.number - cl.serverdelta;
    cl.servertime = framenum * CL_FRAMETIME;

    // Rebuild the list of solid entities for this frame
    cl.numSolidEntities = 0;

    // Initialize position of the player's own entity from playerstate.
    // this is needed in situations when player entity is invisible, but
    // server sends an effect referencing it's origin (such as MZ_LOGIN, etc).
    centity_t *clent = ENTITY_FOR_NUMBER( cl.frame.clientNum + 1 );
    Com_PlayerToEntityState( &cl.frame.ps, &clent->current );

    // Iterate over the current frame entity states and update them accordingly.
    for ( int32_t i = 0; i < cl.frame.numEntities; i++ ) {
        int32_t j = ( cl.frame.firstEntity + i ) & PARSE_ENTITIES_MASK;
        entity_state_t *state = &cl.entityStates[ j ];

        // Set the current and previous entity state.
        parse_entity_update( state );
    }

    // Re-Iterate over the current frame entity states in order to safely launch their events.
    // (Events might need data of other entities, which if we called them in the previous iteration, might not have been updated yet.)
    for ( int32_t i = 0; i < cl.frame.numEntities; i++ ) {
        int32_t j = ( cl.frame.firstEntity + i ) & PARSE_ENTITIES_MASK;
        entity_state_t *state = &cl.entityStates[ j ];
        
        // Fire any needed entity events.
        parse_entity_event( state->number );
    }

    // If we're recording a demo, make sure to store this frame into the demo data.
    if ( cls.demo.recording && !cls.demo.paused && !cls.demo.seeking ) {
        CL_EmitDemoFrame();
    }

    if ( cls.demo.playback ) {
        // this delta has nothing to do with local viewangles,
        // clear it to avoid interfering with demo freelook hack
        VectorClear( cl.frame.ps.pmove.delta_angles );
    }

    // Grab mouse in case of player move type changes between frames.
    // TODO: I have no clue why this was here to begin with, it wasn't me.
    if ( cl.oldframe.ps.pmove.pm_type != cl.frame.ps.pmove.pm_type ) {
        IN_Activate();
    }

    // Determine whether the player state has to lerp between the current and old frame,
    // or snap 'to'.
    CL_LerpOrSnapPlayerState( &cl.oldframe, &cl.frame, 1 );

    if ( cls.demo.playback ) {
        // TODO: Proper stair smoothing.

        // Record time of changing and adjusting viewheight if it differs from previous time.
        CL_AdjustViewHeight( cl.frame.ps.pmove.viewheight );
    }

    // See if we had any prediction errors.
    CL_CheckPredictionError();

    // Notiy screen about the delta frame, so it may adjust some data if needed.
    SCR_DeltaFrame();
}

/**
*   @brief  For debugging problems when out-of-date entity origin is referenced.
**/
#if USE_DEBUG
void CL_CheckEntityPresent( const int32_t entityNumber, const char *what)
{
    if ( entityNumber == cl.frame.clientNum + 1 ) {
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


/**
*   @brief  Sets cl.refdef view values and sound spatialization params.
*           Usually called from CL_PrepareViewEntities, but may be directly called from the main
*           loop if rendering is disabled but sound is running.
**/
void CL_CalculateViewValues( void ) {
    // Update view values.
    clge->CalculateViewValues();
}

/**
*   @brief  Prepares the current frame's view scene for the refdef by 
*           emitting all frame data(entities, particles, dynamic lights, lightstyles,
*           and temp entities) to the refresh definition.
**/
void CL_PrepareViewEntities(void) {
    // Let the Client Game prepare view entities.
    clge->PrepareViewEntities();

    // WID: disable LOC: LOC_AddLocationsToScene();
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