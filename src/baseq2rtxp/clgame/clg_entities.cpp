/********************************************************************
*
*
*	ClientGame: Entity Management.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_entities.h"
#include "sharedgame/sg_entity_events.h"

//// Needed for EAX spatialization to work:
//#include "clgame/clg_local_entities.h"
//#include "clgame/local_entities/clg_local_entity_classes.h"
//#include "clgame/local_entities/clg_local_env_sound.h"

#include "sharedgame/pmove/sg_pmove.h"



/**
*   @brief  For debugging problems when out-of-date entity origin is referenced.
**/
#if USE_DEBUG
void CLG_CheckEntityPresent( int32_t entityNumber, const char *what ) {
	server_frame_t *frame = &clgi.client->frame;

	if ( entityNumber == clgi.client->frame.clientNum + 1 ) {
		return; // player entity = current.
	}

	centity_t *e = &clg_entities[ entityNumber ]; //e = &cl_entities[entnum];
	if ( e && e->serverframe == frame->number ) {
		return; // current
	}

	if ( e && e->serverframe ) {
		clgi.Print( PRINT_DEVELOPER, "SERVER BUG: %s on entity %d last seen %d frames ago\n", what, entityNumber, frame->number - e->serverframe );
	} else {
        clgi.Print( PRINT_DEVELOPER, "SERVER BUG: %s on entity %d never seen before\n", what, entityNumber );
	}
}
#endif

/**
*   @brief  Performs general linear interpolation for the entity origin inquired by sound spatialization.
**/
void CLG_LerpEntitySoundOrigin( const centity_t *ent, vec3_t org ) {
    // Just regular lerp for most of all entities.
    LerpVector( ent->prev.origin, ent->current.origin, clgi.client->lerpfrac, org );
}
/**
*   @brief  Beam entity specific entity sound origin. Use the closest point on 
*           the line between player and beam start/end points as sound source origin.
* 
*           NOTE: org actually has to already be set by the general LerpEntitySoundOrigin function.
**/
void CLG_LerpBeamSoundOrigin( const centity_t *ent, vec3_t org ) {
    // Entity old origin direction to new origin.
    const Vector3 oldOrg = QM_Vector3Lerp( ent->prev.old_origin, ent->current.old_origin, clgi.client->lerpfrac );
    const Vector3 vec = oldOrg - org;

    // Listener origin and direction.
    const Vector3 playerEntityOrigin = clgi.client->playerEntityOrigin;
    const Vector3 p = playerEntityOrigin - org;

    // Determine fraction of the closest point.
    const float t = QM_Clamp( QM_Vector3DotProduct( p, vec ) / QM_Vector3DotProduct( vec, vec ), 0.f, 1.f );

    // Calculate final the closest point.
    const Vector3 closestPoint = QM_Vector3MultiplyAdd( vec, t, org );
    VectorCopy( closestPoint, org );
}
/**
*   @brief  Calculate origin used for BSP model by taking the closest point from the AABB to the 'listener'.
**/
void CLG_LerpBrushModelSoundOrigin( const centity_t *ent, vec3_t org ) {
    mmodel_t *brushModel = clgi.client->model_clip[ ent->current.modelindex ];
    if ( brushModel ) {
        Vector3 absmin = org, absmax = org;
        absmin += brushModel->mins;
        absmax += brushModel->maxs;

        const Vector3 closestPoint = QM_Vector3ClosestPointToBox( clgi.client->playerEntityOrigin, absmin, absmax );
        VectorCopy( closestPoint, org );
    }
}

/**
*   @brief  The sound code makes callbacks to the client for entitiy position
*           information, so entities can be dynamically re-spatialized.
**/
void PF_GetEntitySoundOrigin( const int32_t entityNumber, vec3_t org ) {
    if ( entityNumber < 0 || entityNumber >= MAX_EDICTS ) {
        Com_Error( ERR_DROP, "%s: bad entnum: %d", __func__, entityNumber );
    }

    if ( !entityNumber || entityNumber == clgi.client->listener_spatialize.entnum ) {
        // Should this ever happen?
        VectorCopy( clgi.client->listener_spatialize.origin, org );
        return;
    }

    // Get entity pointer.
    centity_t *ent = &clg_entities[ entityNumber ]; // ENTITY_FOR_NUMBER( entityNumber );

    // If for whatever reason it is invalid:
    if ( !ent ) {
        Com_Error( ERR_DROP, "%s: nullptr entity for entnum: %d", __func__, entityNumber );
        return;
    }

    // Just regular lerp for most of all entities.
    CLG_LerpEntitySoundOrigin( ent, org );
    //LerpVector( ent->prev.origin, ent->current.origin, clgi.client->lerpfrac, org );

    // Use the closest point on the line for beam entities:
    if ( ent->current.entityType == ET_BEAM || ent->current.renderfx & RF_BEAM ) {
        CLG_LerpBeamSoundOrigin( ent, org );
    // Calculate origin for BSP models to be closest point from listener to the bmodel's aabb:
    } else if ( ent->current.solid == BOUNDS_BRUSHMODEL ) {
        CLG_LerpBrushModelSoundOrigin( ent, org );
    }

    // TODO: Determine whichever reverb effect is dominant for the current sound we're spatializing??    
}



/**
*
*
*
*	Entity Receive and Update:
*
*	Callbacks for the client game when it receives entity states from the server,
*	to update them for when entering from new frame or updating from a deltaframe
*
*
*
**/
/**
*	@brief	Called when a new frame has been received that contains an entity
*			which was not present in the previous frame.
**/
void CLG_EntityState_FrameEnter( centity_t *ent, const entity_state_t *state, const vec_t *origin ) {
	// Assign a unique id to each entity for tracking it over multiple frames.
    static int64_t entity_ctr = 0;
    ent->id = ++entity_ctr;
    // For diminishing rocket / grenade trails.
    ent->trailcount = 1024;

    // Duplicate the current state so lerping doesn't hurt anything.
    ent->prev = *state;

    // <Q2RTXP>: WID: TODO: Do we still want/need this per se?
    // Handle proper lerping for animated entities by Hz.    
    #if 1
    // WID: 40hz
    // Update the animation frames and time.
    ent->current_frame = ent->last_frame = state->frame;
    ent->frame_servertime = clgi.client->servertime;
    ent->frame_realtime = clgi.GetRealTime();
    // WID: 40hz
    #endif

    // No lerping if teleported, or a BEAM effect entity.
    if ( state->event == EV_PLAYER_TELEPORT ||
        state->event == EV_OTHER_TELEPORT ||
        ( state->entityType == ET_BEAM || state->renderfx & RF_BEAM ) ) {
        // No lerping if teleported.
        VectorCopy( origin, ent->lerp_origin );
        return;
    }

    // old_origin is valid for new entities,
    // so use it as starting point for interpolating between
    VectorCopy( state->old_origin, ent->prev.origin );
    VectorCopy( state->old_origin, ent->lerp_origin );
}
/**
*	@brief	Called when a new frame has been received that contains an entity
*			already present in the previous frame.
**/
void CLG_EntityState_FrameUpdate( centity_t *ent, const entity_state_t *state, const vec_t *origin ) {
    const int32_t event = state->event;

    // <Q2RTXP>: WID: TODO: Do we still want/need this per se?
    // Handle proper lerping for animated entities by Hz.    
    #if 1
    if ( ent->current_frame != state->frame ) {
        if ( state->renderfx & RF_OLD_FRAME_LERP ) {
            ent->last_frame = ent->current.old_frame;
        } else {
            ent->last_frame = ent->current.frame;
        }
        ent->current_frame = state->frame;
        ent->frame_servertime = clgi.client->servertime;
        ent->frame_realtime = clgi.GetRealTime();
    }
    #endif

    // Set step height, and server time, if caught stair stepping.
    if ( state->renderfx & RF_STAIR_STEP ) {
        ent->step_height = state->origin[ 2 ] - ent->current.origin[ 2 ];
        ent->step_servertime = clgi.client->servertime;
        ent->step_realtime = clgi.GetRealTime();
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

        // <Q2RTXP>: WID: TODO: Do we still want/need this per se?
        // Handle proper lerping for animated entities by Hz.    
        #if 1
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
* 
* 
* 
*   Player State Duplication/Lerping:
* 
* 
* 
**/
/**
*   @brief  Duplicates old player state, into playerstate. Used as a utility function.
*   @param ps Pointer to the state that we want to copy data into.
*   @param ops Pointer to the state's data source.
*   @param debugID Debugging ID for what caused the duplication.
**/
static constexpr int32_t PS_DUP_DEBUG_OLDFRAME_INVALID = BIT( 0 );
static constexpr int32_t PS_DUP_DEBUG_OLDFRAME_NOT_LASTFRAME_NUMBER = BIT( 1 );
static constexpr int32_t PS_DUP_DEBUG_ORIGIN_OFFSET_LARGER_THAN_256 = BIT( 2 );
static constexpr int32_t PS_DUP_DEBUG_EVENT_TELEPORT = BIT( 3 );
static constexpr int32_t PS_DUP_DEBUG_TIME_TELEPORT = BIT( 4 );
static constexpr int32_t PS_DUP_DEBUG_CLIENTNUM_MISMATCH = BIT( 5 );
static constexpr int32_t PS_DUP_DEBUG_NOLERP = BIT( 6 );

static void duplicate_player_state( player_state_t *ps, player_state_t *ops, const int32_t debugID ) {
    *ops = *ps;

    // Debugging output for player state duplication.
    #if 0
    if ( debugID == PS_DUP_DEBUG_OLDFRAME_INVALID ) {
        Com_LPrintf( PRINT_DEVELOPER, "FRAME(#%d) PS_DUP_DEBUG_OLDFRAME_INVALID\n", cl.frame.number );
    } else if ( debugID == PS_DUP_DEBUG_OLDFRAME_NOT_LASTFRAME_NUMBER ) {
        Com_LPrintf( PRINT_DEVELOPER, "FRAME(#%d) PS_DUP_DEBUG_OLDFRAME_NOT_LASTFRAME_NUMBER \n", cl.frame.number );
    } else if ( debugID == PS_DUP_DEBUG_ORIGIN_OFFSET_LARGER_THAN_256 ) {
        Com_LPrintf( PRINT_DEVELOPER, "FRAME(#%d) PS_DUP_DEBUG_ORIGIN_OFFSET_LARGER_THAN_256 \n", cl.frame.number );
    } else if ( debugID == PS_DUP_DEBUG_EVENT_TELEPORT ) {
        Com_LPrintf( PRINT_DEVELOPER, "FRAME(#%d) PS_DUP_DEBUG_EVENT_TELEPORT \n", cl.frame.number );
    } else if ( debugID == PS_DUP_DEBUG_TIME_TELEPORT ) {
        Com_LPrintf( PRINT_DEVELOPER, "FRAME(#%d) PS_DUP_DEBUG_TIME_TELEPORT \n", cl.frame.number );
    } else if ( debugID == PS_DUP_DEBUG_CLIENTNUM_MISMATCH ) {
        Com_LPrintf( PRINT_DEVELOPER, "FRAME(#%d) PS_DUP_DEBUG_CLIENTNUM_MISMATCH \n", cl.frame.number );
    } else if ( debugID == PS_DUP_DEBUG_NOLERP ) {
        Com_LPrintf( PRINT_DEVELOPER, "FRAME(#%d) PS_DUP_DEBUG_NOLERP \n", cl.frame.number );
    } else {
        Com_LPrintf( PRINT_DEVELOPER, "FRAME(#%d) No player state duplicating. \n", cl.frame.number );
    }
    #endif
}
/**
*   Determine whether the player state has to lerp between the current and old frame,
*   or snap 'to'.
**/
void CLG_PlayerState_LerpOrSnap( server_frame_t *oldframe, server_frame_t *frame, const int32_t framediv ) {
    // Find player states to interpolate between
    player_state_t *ps = &frame->ps;
    player_state_t *ops = &oldframe->ps;

    // No lerping if previous frame was dropped or invalid.
    if ( !oldframe->valid ) {
        duplicate_player_state( ps, ops, PS_DUP_DEBUG_OLDFRAME_INVALID );
        return;
    }

    // Duplicate state in case the stored old frame number does not match to the
    // expected last frame number.
    const int32_t lastFrameNumber = frame->number - framediv;
    if ( oldframe->number != lastFrameNumber ) {
        duplicate_player_state( ps, ops, PS_DUP_DEBUG_OLDFRAME_NOT_LASTFRAME_NUMBER );
        return;
    }

    // No lerping if player entity was teleported (origin check).
    if ( fabsf( ops->pmove.origin[ 0 ] - ps->pmove.origin[ 0 ] ) > 256 || // * 8 || // WID: float-movement
        fabsf( ops->pmove.origin[ 1 ] - ps->pmove.origin[ 1 ] ) > 256 || // * 8 || // WID: float-movement
        fabsf( ops->pmove.origin[ 2 ] - ps->pmove.origin[ 2 ] ) > 256 ) {// * 8) { // WID: float-movement
        duplicate_player_state( ps, ops, PS_DUP_DEBUG_ORIGIN_OFFSET_LARGER_THAN_256 );
        return;
    }

    // No lerping if player entity was teleported (event check).
    centity_t *clent = &clg_entities[frame->clientNum + 1];

    // If the player entity was within the range of lastFrameNumber and frame->number,
    // and had any teleport events going on, duplicate the player state into the old player state,
    // to prevent it from lerping afar distance.
    if ( clent->serverframe > lastFrameNumber && clent->serverframe <= frame->number &&
        ( clent->current.event == EV_PLAYER_TELEPORT || clent->current.event == EV_OTHER_TELEPORT ) ) {
        duplicate_player_state( ps, ops, PS_DUP_DEBUG_EVENT_TELEPORT );
        return;
    }

    // No lerping if teleport bit was flipped.
    if ( ( ops->pmove.pm_flags ^ ps->pmove.pm_flags ) & PMF_TIME_TELEPORT ) {
        duplicate_player_state( ps, ops, PS_DUP_DEBUG_TIME_TELEPORT );
        return;
    }

    //if ( ( ops->rdflags ^ ps->rdflags ) & RDF_TELEPORT_BIT ) {
    //    duplicate_player_state( ps, ops );
    //    return;
    //}

    // No lerping if POV number changed.
    if ( oldframe->clientNum != frame->clientNum ) {
        duplicate_player_state( ps, ops, PS_DUP_DEBUG_CLIENTNUM_MISMATCH );
        return;
    }

    // No lerping in case of the enabled developer option.
    if ( cl_nolerp->integer == 1 ) {
        duplicate_player_state( ps, ops, PS_DUP_DEBUG_NOLERP );
        return;
    }
}