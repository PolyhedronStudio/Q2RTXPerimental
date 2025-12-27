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
#include "sharedgame/sg_entities.h"


/**
*   @brief  For debugging problems when out-of-date entity origin is referenced.
**/
#if USE_DEBUG
void CLG_CheckServerEntityPresent( const int32_t entityNumber, const char *what ) {
    if ( CLG_IsLocalClientEntity( entityNumber ) ) {
        return; // local client entity = current
    }

    // Get the entity for the number.
    centity_t *e = &clg_entities[ entityNumber ];

    // Check if the entity was present in the current frame.
    if ( e && e->serverframe == clgi.client->frame.number ) {
        return; // current
    }

    // Print debug info.
    if ( e && e->serverframe ) {
        clgi.Print( PRINT_ERROR, "SERVER BUG: %s on entity %d last seen %d frames ago\n", what, entityNumber, clgi.client->frame.number - e->serverframe );
    } else {
        clgi.Print( PRINT_ERROR, "SERVER BUG: %s on entity %d never seen before\n", what, entityNumber );
    }
}
#endif



/**
*
*
*
*	Entity Chase and View Bindings:
*
*
*
**/
#if 0
/**
*	@return		A pointer into clg_entities that matches to the client we're currently chasing. nullptr if not chasing anyone.
**/
centity_t *CLG_GetChaseBoundEntity( void ) {
	if ( clgi.client->frame.ps.stats[ STAT_CHASE ] > 0 && clgi.client->frame.ps.stats[ STAT_CHASE ] <= MAX_CLIENTS ) {
		return &clg_entities[ clgi.client->frame.ps.stats[ STAT_CHASE ] - CS_PLAYERSKINS - 1 ];
	} else {
		return nullptr;
	}
}
#endif
/**
*	@return		A pointer into clg_entities that matches to the received client frame its player_state_t we're currently chasing. 
* 				nullptr if not chasing anyone but the local client entity itself.
**/
centity_t *CLG_GetFrameClientEntity( void ) {
	// Sanity check.
	if ( clgi.client->frame.ps.clientNumber == clgi.client->clientNumber ) {
		// We're chasing ourselves.
		return nullptr;
	}
	// Return chased entity.
	return &clg_entities[ clgi.client->frame.ps.clientNumber + 1 ];
}
/**
*	@return		The local client entity pointer, which is a match with the entity in clg_entities
*				for the client number which we received at initial time of connection.
**/
centity_t *CLG_GetLocalClientEntity( void ) {
	// Sanity check.
	if ( clgi.client->clientNumber == -1 ) {
		clgi.Print( PRINT_DEVELOPER, "CLG_GetLocalClientEntity: No client number set yet(Value is CLIENTNUM_NONE(%d)).\n", CLIENTNUM_NONE );
		// Return a nullptr.
		return nullptr;
	}
	// Return the local client entity.
	return &clg_entities[ clgi.client->clientNumber + 1 ];
}
/**
*	@return		The predicted client entity pointer, which resides outside of clg_entities.
**/
centity_t *CLG_GetPredictedClientEntity( void ) {
	// Sanity check.
	if ( clgi.client->clientNumber == -1 ) {
		clgi.Print( PRINT_DEVELOPER, "CLG_GetPredictedClientEntity: No client number set yet(Value is CLIENTNUM_NONE(%d)).\n", CLIENTNUM_NONE );
		// Return a nullptr.
		return nullptr;
	}
	// Return the local client entity.
	return &game.predictedEntity;//&clg_entities[ clgi.client->clientNumber + 1 ];
}

#if 0
/**
*	@return		A pointer to the entity which our view has to be bound to. If STAT_CHASE is set, it'll point to the chased entity.
* 				Otherwise, it'll point to the local client entity.
*
*				Exception: If no client number is set yet, it'll return a nullptr and print a developer warning.
**/
centity_t *CLG_GetViewBoundEntity( const bool onlyPredictedClient = false ) {
	// Sanity check.
	if ( clgi.client->clientNumber == -1 ) {
		clgi.Print( PRINT_DEVELOPER, "CLG_GetViewBoundEntity: No client number set yet(Value is -1).\n" );
		// Return a nullptr.
		return nullptr;
	}
	// Get chase entity.
	centity_t *viewBoundEntity = CLG_GetFrame();
	// If not chasing anyone, assign it the local client entity.
	if ( viewBoundEntity == nullptr ) {
		if ( !cl_predict->integer || !onlyPredictedClient ) {
			viewBoundEntity = CLG_GetLocalClientEntity();
		} else {
			viewBoundEntity = CLG_GetPredictedClientEntity();
		}
	}
	// Return the view bound entity.
	return viewBoundEntity;
}
#endif
/**
*	@return		A pointer to the entity which our view is bound to. This can be the local client entity, the
*				actual chased entity(the frame's playerstate clientnumber != local client number) or the predicted entity.
*
*				Exception: If no client number is set yet, it'll return a nullptr and print a developer warning.
**/
centity_t *CLG_GetViewBoundEntity( const bool onlyPredictedClient ) {
	// Sanity check.
	if ( clgi.client->clientNumber == -1 ) {
		clgi.Print( PRINT_DEVELOPER, "%s: No client number set yet(Value is -1).\n", __func__ );
		// Return a nullptr.
		return nullptr;
	}
	// Get client frame's matching client entity.
	centity_t *viewBoundEntity = CLG_GetFrameClientEntity();
	// If not chasing anyone, assign it the local client entity.
	if ( viewBoundEntity == nullptr ) {
		if ( !cl_predict->integer || !onlyPredictedClient ) {
			viewBoundEntity = CLG_GetLocalClientEntity();
		} else {
			viewBoundEntity = CLG_GetPredictedClientEntity();
		}
	}
	// Return the view bound entity.
	return viewBoundEntity;
}

/**
*	@brief	Returns true if the entity state's number matches to the view entity number.
**/
const bool CLG_IsCurrentViewEntity( const centity_t *cent ) {
	if ( !cent ) {
		clgi.Print( PRINT_DEVELOPER, "%s: (nullptr) entity pointer.\n", __func__ );
		return false;
	}
	// Get client frame's matching client entity.
	if ( cent == CLG_GetFrameClientEntity() ) {
		return true;
	} else {
		// Not chasing anyone, check against local client entity.
		if ( cent == CLG_GetLocalClientEntity() ) {
			return true;
		}
		// Check against predicted client entity.
		if ( cent == CLG_GetPredictedClientEntity() ) {
			return true;
		}
	}

	// Not a match.
	return false;
}



/**
*
*
*
*   Entity Frame(s), Origin, and Angles (Linear/Extrapolation):
*
*
*
**/
/**
*   @brief  Animates the entity's frame from previousFrameTime to currentTime using the given animationHz.
*	@param previousFrameTime The previous frame time.
*	@param currentTime The current time.
*	@param animationHz The animation hertz to use. Default is BASE_FRAMERATE (40).
*	@return The backLerp fraction for the animation.
**/
const double CLG_GetEntityFrameBackLerp( const QMTime previousFrameTime, const QMTime currentTime, const double animationHz ) {
	// Calculate animation ms fraction per frame.
	const double animationMs = 1. / ( animationHz ) * 1000.;

	// Calculate back lerpfraction. (10hz.)
	//refreshEntity->backlerp = 1.0f - ( ( clgi.client->time - ( (float)packetEntity->frame_servertime - clgi.client->sv_frametime ) ) / (float)animationMs );
	// Calculate back lerpfraction using RealTime. (40hz.)
	//refreshEntity->backlerp = 1.0f - ( ( clgi.GetRealTime()  - ( (float)packetEntity->frame_realtime - clgi.client->sv_frametime ) ) / (float)animationMs );

	// Calculate back lerpfraction using clgi.client->time. (40hz.)
	return QM_Clamp( 1. - ( (double)( currentTime.Milliseconds() - ( previousFrameTime.Milliseconds() ) ) / animationMs ), 0.0, 1.);
}
/**
*   @brief  Handles the 'lerping' of the entity origins.
*	@param	previousFrameOrigin The previous frame origin.
* 	@param	currentFrameOrigin The current frame origin.
* 	@param	lerpFraction The lerp fraction to use for interpolation.
*	@return A Vector3 containing the lerped origin.
**/
const Vector3 CLG_GetEntityLerpOrigin( const Vector3 &previousFrameOrigin, const Vector3 &currentFrameOrigin, const double lerpFraction ) {
	return QM_Vector3Lerp( previousFrameOrigin, currentFrameOrigin, lerpFraction );
}
/**
*   @brief  Handles the 'lerping' of the packet and its corresponding refresh entity angles.
*	@param	previousFrameAngles The previous frame angles.
*	@param	currentFrameAngles The current frame angles.
*	@param	lerpFraction The lerp fraction to use for interpolation.
*	@return A Vector3 containing the lerped angles.
**/
const Vector3 CLG_GetEntityLerpAngles( const Vector3 &previousFrameAngles, const Vector3 &currentFrameAngles, const double lerpFraction ) {
	return QM_Vector3LerpAngles( previousFrameAngles, currentFrameAngles, lerpFraction );
}



/**
*
*
*
*   Entity Sound Spatialization (Linear/Extrapolation):
* 
* 
* 
**/
/**
*   @brief  Performs general linear interpolation for the entity origin inquired by sound spatialization.
**/
static const Vector3 CLG_LerpEntitySoundOrigin( const centity_t *ent, const double lerpFrac ) {
	return QM_Vector3Lerp( ent->prev.origin, ent->current.origin, lerpFrac );
}
/**
*   @brief  Beam entity specific entity sound origin. Use the closest point on 
*           the line between player and beam start/end points as sound source origin.
* 
*           NOTE: org actually has to already be set by the general LerpEntitySoundOrigin function.
*   @param  lerpedEntitySoundOrigin Needs to be set to the already lerped entity sound origin before calling this function.
*           This is because we need to calculate the beam line vector from the old to new origin.
*   @param  lerpFrac The lerp fraction to use for interpolation.
**/
static const Vector3 CLG_LerpBeamSoundOrigin( const centity_t *ent, const Vector3 lerpedEntitySoundOrigin, const double lerpFrac ) {
    // Entity old origin direction to new origin.
    const Vector3 oldOrg = QM_Vector3Lerp( ent->prev.old_origin, ent->current.old_origin, clgi.client->lerpfrac );
    const Vector3 vec = oldOrg - lerpedEntitySoundOrigin;

    // Listener origin and direction.
    const Vector3 playerEntityOrigin = clgi.client->playerEntityOrigin;
    const Vector3 p = playerEntityOrigin - lerpedEntitySoundOrigin;

    // Determine fraction of the closest point.
    const float t = QM_Clamp( QM_Vector3DotProduct( p, vec ) / QM_Vector3DotProduct( vec, vec ), 0.f, 1.f );

    // Calculate final the closest point.
    return QM_Vector3MultiplyAdd( vec, t, lerpedEntitySoundOrigin );
}
/**
*   @brief  Calculate origin used for BSP model by taking the closest point from the AABB to the 'listener'.
**/
static const Vector3 CLG_LerpBrushModelSoundOrigin( const centity_t *ent, const Vector3 lerpedEntitySoundOrigin, const double lerpFrac ) {
    mmodel_t *brushModel = clgi.client->model_clip[ ent->current.modelindex ];
    if ( brushModel ) {
        Vector3 absMin = lerpedEntitySoundOrigin, absMax = lerpedEntitySoundOrigin;
        absMin += brushModel->mins;
        absMax += brushModel->maxs;

        return QM_Vector3ClosestPointToBox( clgi.client->playerEntityOrigin, absMin, absMax );
    }
	// Fallback to regular lerped origin.
	return lerpedEntitySoundOrigin;
}

/**
*   @brief  The sound code makes callbacks to the client for entitiy position
*           information, so entities can be dynamically re-spatialized.
**/
const Vector3 CLG_GetEntitySoundOrigin( const int32_t entityNumber ) {
    if ( entityNumber < 0 || entityNumber >= MAX_EDICTS ) {
        Com_Error( ERR_DROP, "%s: bad entnum: %d", __func__, entityNumber );
    }

    if ( !entityNumber || entityNumber == clgi.client->listener_spatialize.entnum ) {
        // Should this ever happen?
        return clgi.client->listener_spatialize.origin;
    }

    // Get entity pointer.
    centity_t *ent = &clg_entities[ entityNumber ]; // ENTITY_FOR_NUMBER( entityNumber );

    // If for whatever reason it is invalid:
    if ( !ent ) {
        Com_Error( ERR_DROP, "%s: nullptr entity for entnum: %d", __func__, entityNumber );
        return {};
    }

    // Lerp fraction.
	const double lerpFrac = clgi.client->lerpfrac;
    // Regular lerp for entity origins:
    Vector3 soundOrigin = CLG_LerpEntitySoundOrigin( ent, lerpFrac );

    // Use the closest point on the line for beam entities:
    if ( ent->current.entityType == ET_BEAM || ent->current.renderfx & RF_BEAM ) {
        soundOrigin = CLG_LerpBeamSoundOrigin( ent, soundOrigin, lerpFrac );
    // Calculate origin for BSP models to be closest point from listener to the bmodel's aabb:
    } else if ( ent->current.solid == BOUNDS_BRUSHMODEL ) {
        soundOrigin = CLG_LerpBrushModelSoundOrigin( ent, soundOrigin, lerpFrac );
    }

    // Return the final lerped origin.
    return soundOrigin;

    // TODO: Determine whichever reverb effect is dominant for the current sound we're spatializing??    
}



/**
*
*
*
*	Entity Identification:
*
*
*
**/
/**
*	@brief	Returns true if the entity state's number matches to our local client entity number,
*			received at initial time of connection.
**/
const bool CLG_IsLocalClientEntity( const server_frame_t *frame ) {
    //if ( state && state->number > 0 ) {
    //	return CLG_IsLocalClientEntity( state->number );
    //}
    if ( frame ) {
        // Check for client number match.
        if ( frame->ps.clientNumber == clgi.client->clientNumber ) {
            return true;
        }
    }
    return false;
}
/**
*	@brief	Returns true if the entity state's number matches to our local client entity number,
*			received at initial time of connection.
**/
const bool CLG_IsLocalClientEntity( const entity_state_t *state ) {
    //if ( state && state->number > 0 ) {
    //	return CLG_IsLocalClientEntity( state->number );
    //}
    if ( state ) {
        // Decode skinnum.
        const encoded_skinnum_t encodedSkin = static_cast<const encoded_skinnum_t>( state->skinnum );
        // Check for client number match.
        if ( encodedSkin.clientNumber == clgi.client->clientNumber ) {
            return true;
        }
    }
    return false;
}
/**
*	@brief	Returns true if the entityNumber matches to our local connection received entity number.
**/
const bool CLG_IsLocalClientEntity( const int32_t entityNumber ) {
    if ( entityNumber < 0 ) {
        clgi.Print( PRINT_DEVELOPER, "%s: Invalid entity number (#%d) < (#0).\n", __func__, entityNumber );
        return false;
    }
    if ( entityNumber >= MAX_EDICTS ) {
        clgi.Print( PRINT_DEVELOPER, "%s: Entity number (#%d) exceeds MAX_EDICTS(#%d).\n", __func__, entityNumber, MAX_EDICTS );
        return false;
    }
    // Get the entity state.
    return CLG_IsLocalClientEntity( &clg_entities[ entityNumber ].current );
}






