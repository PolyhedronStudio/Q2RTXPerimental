/********************************************************************
*
*
*	ClientGame: Entity Management.
*
*
********************************************************************/
#include "clg_local.h"
#include "clg_entities.h"

//// Needed for EAX spatialization to work:
//#include "clg_local_entities.h"
//#include "local_entities/clg_local_entity_classes.h"
//#include "local_entities/clg_local_env_sound.h"



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
    const float t = QM_Clampf( QM_Vector3DotProduct( p, vec ) / QM_Vector3DotProduct( vec, vec ), 0.f, 1.f );

    // Calculate final the closest point.
    const Vector3 closestPoint = QM_Vector3MultiplyAdd( org, t, vec );
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
    if ( ent->current.renderfx & RF_BEAM ) {
        CLG_LerpBeamSoundOrigin( ent, org );
    // Calculate origin for BSP models to be closest point from listener to the bmodel's aabb:
    } else if ( ent->current.solid == BOUNDS_BRUSHMODEL ) {
        CLG_LerpBrushModelSoundOrigin( ent, org );
    }

    // TODO: Determine whichever reverb effect is dominant for the current sound we're spatializing??    
}
/**
*   @brief  The sound code makes callbacks to the client for entitiy position
*           information, so entities can be dynamically re-spatialized.
**/
qhandle_t PF_GetEAXBySoundOrigin( const int32_t entityNumber, vec3_t org ) {
    //if ( entityNumber < 0 || entityNumber >= MAX_EDICTS ) {
    //    Com_Error( ERR_DROP, "%s: bad entnum: %d", __func__, entityNumber );
    //}

    //if ( !entityNumber || entityNumber == clgi.client->listener_spatialize.entnum ) {
    //    // Should this ever happen?
    //    VectorCopy( clgi.client->listener_spatialize.origin, org );
    //    return SOUND_EAX_EFFECT_DEFAULT;
    //}

    //// Get entity pointer.
    //centity_t *ent = &clg_entities[ entityNumber ]; // ENTITY_FOR_NUMBER( entityNumber );

    //// If for whatever reason it is invalid:
    //if ( !ent ) {
    //    Com_Error( ERR_DROP, "%s: nullptr entity for entnum: %d", __func__, entityNumber );
    //    return SOUND_EAX_EFFECT_DEFAULT;
    //}

    //return reverb_eax_id;
    return 0;
}

/**
*	@return		A pointer to the entity bound to the client game's view. Unless STAT_CHASE is set to
*               a specific client number the current received frame, this'll point to the entity that
*               is of the local client player himself.
**/
centity_t *CLG_ViewBoundEntity( void ) {
    // Default to clgi.client->clientNumberl.
	int32_t index = clgi.client->clientNumber;

    // Chase entity.
	if ( clgi.client->frame.ps.stats[ STAT_CHASE ] ) {
		index = clgi.client->frame.ps.stats[ STAT_CHASE ] - CS_PLAYERSKINS;
	}

    // + 1, because 0 == world.
	return &clg_entities[ index + 1 ];
}
