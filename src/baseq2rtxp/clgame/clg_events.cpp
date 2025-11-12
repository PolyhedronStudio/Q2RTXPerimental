/********************************************************************
*
*
*	ClientGame: (Entity/Player State) -Events:
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_entities.h"
#include "clgame/clg_effects.h"
#include "clgame/clg_events.h"

// Needed:
#include "sharedgame/sg_entity_events.h"
#include "sharedgame/sg_entity_flags.h"
#include "sharedgame/sg_entity_types.h"
#include "sharedgame/sg_muzzleflashes.h"



/**
*   Default Sound Properties for when not specified by the event:
**/
static constexpr float defaultSoundVolume = 1.f;
static constexpr float defaultSoundAttenuation = 1.f;
static constexpr float defaultSoundTimeOffset = 0.f;



/**
*
*
*
*   Commonly Used/Utility Functions:
*
*
*
**/
/**
*   @brief  Validates the given sound index, and errors out if invalid.
**/
static const bool ValidSoundIndexOrError( const qhandle_t soundIndex ) {
    // Ensure valid sound index.
    if ( soundIndex < 0 ) {
        Com_Error( ERR_DROP, "%s: bad sound resource index: %d < 0", __func__, soundIndex );
        return false;
    }
    if ( soundIndex >= MAX_SOUNDS ) {
        Com_Error( ERR_DROP, "%s: bad sound resource index: %d >= MAX_SOUNDS(%d)", __func__, soundIndex, MAX_SOUNDS );
        return false;
    }

    // Valid.
    return true;
}
/**
*   @brief  Retreives and validates the sound handle for the given sound index.
**/
static inline qhandle_t GetSoundResourceHandle( const int32_t soundIndex ) {
    // Sanity check.
    if ( !ValidSoundIndexOrError( soundIndex ) ) {
        return 0;
    }
    // Get the sound handle.
    qhandle_t handle = clgi.client->sound_precache[ soundIndex ];
    if ( !handle ) {
        clgi.Print( PRINT_DEVELOPER, "%s: NULL sound handle for index: %d\n", __func__, soundIndex );
        return 0;
    }
    // Return it.
    return handle;
}

/**
*   @brief  Determines the 'fire' animation to play for the given primary fire event.
**/
static const std::string PrimaryFireEvent_DetermineAnimation( const player_state_t *ops, const player_state_t *ps, const int32_t playerStateEvent, const Vector3 &lerpOrigin ) {
    // Are we ducked?
    const bool isDucked = ( ps->pmove.pm_flags & PMF_DUCKED ? true : false );

    // Default animation
    std::string animationName = "fire_stand_pistol";

    if ( !ps->animation.isIdle ) {
        if ( ps->animation.isCrouched ) {
            //animationName = "fire_crouch_pistol";
            animationName = "fire_crouch_pistol";
        } else if ( ps->animation.isWalking ) {
            //animationName = "fire_walk_pistol";
            animationName = "fire_run_pistol";
        } else {
            // Only if not strafing though.
            bool isStrafing = true;
            if ( ( !( ps->animation.moveDirection & PM_MOVEDIRECTION_FORWARD ) && !( ps->animation.moveDirection & PM_MOVEDIRECTION_BACKWARD ) )
                && ( ( ps->animation.moveDirection & PM_MOVEDIRECTION_LEFT ) || ( ps->animation.moveDirection & PM_MOVEDIRECTION_RIGHT ) ) ) {
                animationName = "fire_stand_pistol";
            } else {
                animationName = "fire_run_pistol";
            }
        }
    } else {
        if ( ps->animation.isCrouched ) {
            //animationName = "fire_crouch_pistol";
            animationName = "fire_crouch_pistol";
        } else {
            animationName = "fire_stand_pistol";
        }
    }
    return animationName;
}



/**
*
*
*
*   Sound Event Implementations:
* 
* 
* 
**/
/**
*	@brief	Handle the (temp-) entity event to let the client play a general sound on the entity
*           with the passed in parameters.
*   @note   Always players at full volume.
*           Normal attenuation, and 0 sound offset.
*	@param	channel	The sound channel to play the sound on.
*	@param	soundResourceIndex	The sound resource index to play.
**/
static void CLG_EntityEvent_GeneralSound( centity_t *cent, const int32_t channel, const qhandle_t soundIndex ) {
    #if USE_DEBUG
    if ( developer->integer ) {
        CLG_CheckEntityPresent( cent->current.otherEntityNumber, __func__ );
    }
    #endif

	// Get the sound handle.
    qhandle_t soundResourceHandle = GetSoundResourceHandle( soundIndex );
    if ( !soundResourceHandle ) {
        return;
    }
    // Play the sound on the entity.
    if ( ( cent->current.entityFlags & EF_OTHER_ENTITY_EVENT ) != 0 && cent->current.otherEntityNumber > 0 ) {
        // Get the other entity.
        clgi.S_StartSound( nullptr, cent->current.otherEntityNumber, channel, soundResourceHandle, defaultSoundVolume, defaultSoundAttenuation, defaultSoundTimeOffset );
    } else {
        clgi.S_StartSound( nullptr, cent->current.number, channel, soundResourceHandle, defaultSoundVolume, defaultSoundAttenuation, defaultSoundTimeOffset );
    }
}
/**
*	@brief	Handle the (temp-) entity event to let the client play a general sound on the entity
*           with the passed in parameters.
*   @note   Will pack the attenuation parameter together with the channel parameter into the eventParameter.
*	@param	channel	The sound channel to play the sound on.
*	@param	attenuation	The sound attenuation to use.
*	@param	soundResourceIndex	The sound resource index to play.
**/
static void CLG_EntityEvent_GeneralSoundEx( centity_t *cent, const int32_t channel, const qhandle_t soundIndex ) {
    #if USE_DEBUG
    if ( developer->integer ) {
        CLG_CheckEntityPresent( cent->current.otherEntityNumber, __func__ );
    }
    #endif

    // Get the sound handle.
    qhandle_t soundResourceHandle = GetSoundResourceHandle( soundIndex );
    if ( !soundResourceHandle ) {
        return;
    }

	// Decode the channel and the attenuation from the channel parameter.
    int32_t decodedChannel = (uint8_t)( channel & 0xFF );
	// Stored as 0-3 in the event. <Q2RTXP>: TODO: (0-10 if we implement all attenuation levels)
	const float decodedAttenuation = static_cast<float>( ( channel & 0xFFFF ) >> 8 ) / 64.f;

	// Clear the no phs and reliable flags from the channel.
	// They are intend for svc_sound server commands only.
	constexpr int32_t removeChannelMask = ( CHAN_NO_PHS_ADD | CHAN_RELIABLE );
    decodedChannel &= ~removeChannelMask;

	// Play the sound on the entity.
    if ( ( cent->current.entityFlags & EF_OTHER_ENTITY_EVENT ) != 0 && cent->current.otherEntityNumber ) {
        // Get the other entity.
        clgi.S_StartSound( nullptr, cent->current.otherEntityNumber, decodedChannel, soundResourceHandle, defaultSoundVolume, decodedAttenuation, defaultSoundTimeOffset );
    } else {
        clgi.S_StartSound( nullptr, cent->current.number, decodedChannel, soundResourceHandle, defaultSoundVolume, decodedAttenuation, defaultSoundTimeOffset );
    }
}

/**
*	@brief	Handle the (temp-) entity event at the specified origin to let the 
*           client play a positioned general sound.
*   @note   Always players at full volume.
*           Normal attenuation, and 0 sound offset.
*	@param	channel	The sound channel to play the sound on.
*	@param	soundResourceIndex	The sound resource index to play.
**/
static void CLG_EntityEvent_PositionedSound( const Vector3 &origin, const int32_t channel, const qhandle_t soundIndex ) {
    // Get the sound handle.
    qhandle_t handle = GetSoundResourceHandle( soundIndex );
    if ( !handle ) {
        return;
    }
    // Play the sound at the given origin.
    clgi.S_StartSound( &origin.x, ENTITYNUM_WORLD, channel, soundIndex, defaultSoundVolume, defaultSoundAttenuation, defaultSoundTimeOffset );
}

/**
*	@brief	Handle the (temp-) entity event that plays a global sound for all clients
* 		    at their 'head' so it never diminishes.
*   @note   Always players at full volume.
*           Normal attenuation, and 0 sound offset.
*	@param	soundResourceIndex	The sound resource index to play.
**/
static void CLG_EntityEvent_GlobalSound( const qhandle_t soundIndex ) {
    // Get the sound handle.
    qhandle_t handle = GetSoundResourceHandle( soundIndex );
    if ( !handle ) {
        return;
    }
    // Play the sound at the given origin.
    clgi.S_StartSound( &clgi.client->frame.ps.pmove.origin.x, ENTITYNUM_WORLD, CHAN_AUTO, soundIndex, defaultSoundVolume, defaultSoundAttenuation, defaultSoundTimeOffset );
}




/**
*
*
*
*   Impact Event Implementations:
*
*
*
**/




/**
*
*
*
*   Core Entity Event Handling:
*
*
*
**/
/**
*   @brief  Processes the given entity event.
**/
static void ProcessEntityEvent( const int32_t eventValue, const Vector3 &lerpOrigin, centity_t *cent, const int32_t entityNumber, const int32_t clientNumber, clientinfo_t *clientInfo ) {

    // <Q2RTXP>: TODO: Fix so it doesn't do the teleporter at incorrect spawn origin.
    const Vector3 effectOrigin = lerpOrigin; // cent->current.origin 


    // EF_TELEPORTER acts like an event, but is not cleared each frame
    if ( ( cent->current.entityFlags & EF_TELEPORTER ) ) {
        CLG_TeleporterParticles( &effectOrigin.x );
    }

    // Handle the event.
    switch ( eventValue ) {
        /**
        *   FootStep Events:
        **/
        case EV_OTHER_FOOTSTEP:
            CLG_FootstepEvent( entityNumber );
            break;
		// General ladder footstep event:
        case EV_FOOTSTEP_LADDER:
            CLG_FootstepLadderEvent( entityNumber );
            break;

        /**
        *   Sound Events:
        **/
        // General entity sound event:
        case EV_GENERAL_SOUND:
            CLG_EntityEvent_GeneralSound( cent, cent->current.eventParm0, cent->current.eventParm1 );
		    break;
        case EV_GENERAL_SOUND_EX:
            CLG_EntityEvent_GeneralSoundEx( cent, cent->current.eventParm0, cent->current.eventParm1 );
            break;
        // General positioned sound event:
        case EV_POSITIONED_SOUND:
            CLG_EntityEvent_PositionedSound( lerpOrigin, cent->current.eventParm0, cent->current.eventParm1 );
            break;
        // General global sound event:
        case EV_GLOBAL_SOUND:
            CLG_EntityEvent_GlobalSound( cent->current.eventParm0 );
			break;

        ///**
        //*   Fall and Landing Events:
        //**/
        //case EV_FALL_MEDIUM:
        //    clgi.S_StartSound( NULL, entityNumber, CHAN_AUTO, clgi.S_RegisterSound( "player/land01.wav" ), 1, ATTN_NORM, 0 );
        //    break;
        //case EV_FALL_SHORT:
        //    clgi.S_StartSound( NULL, entityNumber, CHAN_AUTO, clgi.S_RegisterSound( "player/fall02.wav" ), 1, ATTN_NORM, 0 );
        //    break;
        //case EV_FALL_FAR:
        //    clgi.S_StartSound( NULL, entityNumber, CHAN_AUTO, clgi.S_RegisterSound( "player/fall01.wav" ), 1, ATTN_NORM, 0 );
        //    break;

        ///**
        //*   Teleport Events:
        //**/
        //case EV_PLAYER_TELEPORT:
        //    clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, clgi.S_RegisterSound( "misc/teleport01.wav" ), 1, ATTN_IDLE, 0 );
        //    CLG_TeleportParticles( &effectOrigin.x );
        //    break;

        /**
        *   Item Events:
        **/
        case EV_ITEM_RESPAWN:
            clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, clgi.S_RegisterSound( "items/respawn01.wav" ), 1, ATTN_IDLE, 0 );
            CLG_ItemRespawnParticles( effectOrigin );
            break;

        default:
            break;
    }
}
/**
*   @brief  Checks for entity generated events and processes them for execution.
**/
void CLG_CheckEntityEvents( centity_t *cent ) {
    // The event value we'll process.
    int32_t eventValue = EV_NONE;
    /**
    *   Check for Entity Event - only entity types:
    **/
    if ( cent->current.entityType > ET_TEMP_ENTITY_EVENT ) {
		// Already fired for this entity.
        if ( cent->previousEvent ) {
            return;
        }
        // if this is a player event set the entity number of the client entity number
        if ( cent->current.entityFlags & EF_OTHER_ENTITY_EVENT ) {
            cent->current.number = cent->current.otherEntityNumber;
        }
        // Set previous event to true.
        cent->previousEvent = 1;
        // The event is simply the entity type minus the ET_EVENTS offset.
        cent->current.event = cent->current.entityType - ET_TEMP_ENTITY_EVENT;
    /**
    *   Check for events riding with another entity:
    **/
    } else {
        // Already fired the event.
        if ( cent->current.event == cent->previousEvent ) {
            return;
        }
		// Save as previous event.
        cent->previousEvent = cent->current.event;

		// Acquire the actual event value by offing it with EV_EVENT_BITS.
        eventValue = SG_GetEntityEventValue( cent->current.event );
        // If no event, don't process anything. ( It hasn't changed again. )
        if ( eventValue == 0 ) {
            return;
        }
    }

    /**
    *   Event Value Decoding:
    **/
	//  Get the entity state.
    entity_state_t *currentEntityState = &cent->current;
	// Acquire the actual event value by offing it with EV_EVENT_BITS.
    eventValue = SG_GetEntityEventValue( currentEntityState->event );
    #if 0
        //if ( cg_debugEvents.integer ) {
            clgi.Print( PRINT_DEVELOPER, "%s: entity(#%d), eventValue(%d), eventName(%s)\n", __func__, currentEntityState->number, eventValue, sg_event_string_names[ eventValue ] );
        //}

        if ( !eventValue ) {
            //DEBUGNAME( "ZEROEVENT" );
			clgi.Print( PRINT_DEVELOPER, "%s: ZERO EVENT on entity(#%d)\n", __func__, currentEntityState->number );
            return;
        }
    #else
        if ( !eventValue ) {
            return;
	    }
    #endif

    /**
    *   Client Info Fetching by skinnum decoding:
    **/
	// The client info we'll acquire based on entity client number(decoded from skinnum if a player). 
    clientinfo_t *clientInfo = nullptr;
    // The client number of the entity, if any.
    int32_t clientNumber = -1;
	// Get client number of entity, if any.
    if ( currentEntityState->modelindex == MODELINDEX_PLAYER ) {
        // Decode the skinnum properties because we're dealing with a player model entity here.
        encoded_skinnum_t skinnum = static_cast<encoded_skinnum_t>( currentEntityState->skinnum );
        // It's a player model, so decode the client number from skinnum.
        clientNumber = skinnum.clientNumber;
        // Invalid client number by skin.
        if ( clientNumber >= 0 || clientNumber < MAX_CLIENTS ) {
            clientInfo = &clgi.client->clientinfo[ clientNumber ];
        } else {
            clientInfo = nullptr;
            clientNumber = -1;
        }
    }

    // calculate the position at exactly the frame time
    //BG_EvaluateTrajectory( &cent->currentState.pos, cg.snap->serverTime, cent->lerpOrigin );
    //CG_SetEntitySoundPosition( cent );
     
    /**
    *   Determine LerpOrigin and Process the Entity Events:
    **/
    // Calculate the position for lerp_origin at exactly the frame time.
    CLG_GetEntitySoundOrigin( cent->current.number, &cent->lerp_origin.x );
	// Process the event.
    // <Q2RTXP>: TODO: Use event value.
    ProcessEntityEvent( eventValue, cent->lerp_origin, cent, cent->current.number, clientNumber, clientInfo );
}



/**
*
*
*
*   PlayerState Event Handling:
*
*
*
**/
/**
*   @brief  Checks for player state generated events(usually by PMove) and processed them for execution.
**/
const bool CLG_CheckPlayerStateEvent( const player_state_t *ops, const player_state_t *ps, const int32_t playerStateEvent, const Vector3 &lerpOrigin ) {
	// Get the frame's client entity.
    //centity_t *cent = &clg_entities[ clgi.client->frame.clientNum + 1 ];

	// Entity has to be in the current frame to process though.
    //if ( cent->serverframe != clgi.client->frame.number ) {
    //    return false;
    //}

    // For dynamic muzzleflash lights.
	clg_dlight_t *dl = nullptr;

    // Is this the view bound entity?
    centity_t *viewBoundEntity = CLG_GetViewBoundEntity();
    // If the viewbound entity does not match, we won't process.
    if ( !viewBoundEntity ) {
        return false;
    }
    
    // Entity has to be in the current frame to process though.
    centity_t *cent = viewBoundEntity;
    if ( cent->serverframe != clgi.client->frame.number ) {
        return false;
    }

    // Calculate the angle vectors.
    Vector3 fv = {}, rv = {};
    QM_AngleVectors( cent->current.angles, &fv, &rv, NULL );

    // Get model resource.
    const model_t *model = clgi.R_GetModelDataForHandle( clgi.client->baseclientinfo.model /*cent->current.modelindex*/ );

    // Get the animation state mixer.
    sg_skm_animation_mixer_t *animationMixer = &cent->animationMixer;//&clgi.client->clientEntity->animationMixer;
    // Get body states.
    sg_skm_animation_state_t *currentBodyState = animationMixer->currentBodyStates;
    sg_skm_animation_state_t *lastBodyState = animationMixer->lastBodyStates;

    sg_skm_animation_state_t *eventBodyState = animationMixer->eventBodyState;

    int32_t entityNumber = cent->current.number;

    switch ( playerStateEvent ) {
        /**
        *   FootStep Events:
        **/
        case EV_PLAYER_FOOTSTEP:
                CLG_FootstepEvent( entityNumber );
                return true;
            break;
        // Applied externally outside of pmove.
        //case EV_FOOTSTEP_LADDER:
        //        CLG_FootstepLadderEvent( entityNumber );
        //        return true;
        //    break;

        /**
        *   Water Enter Events:
        **/
        case EV_WATER_ENTER_FEET:
                clgi.S_StartSound( NULL, entityNumber, CHAN_BODY, clgi.S_RegisterSound( "player/water_feet_in01.wav" ), 1, ATTN_NORM, 0 );
				return true;
            break;
        case EV_WATER_ENTER_WAIST:
				clgi.S_StartSound( NULL, entityNumber, CHAN_AUTO, clgi.S_RegisterSound( SG_RandomResourcePath( "player/water_splash_in", "wav", 0, 2 ).c_str() ), 1, ATTN_NORM, 0 );
                return true;
            break;
        case EV_WATER_ENTER_HEAD:
                clgi.S_StartSound( NULL, entityNumber, CHAN_BODY, clgi.S_RegisterSound( "player/water_head_under01.wav" ), 1, ATTN_NORM, 0 );
			    return true;
            break;

        /**
        *   Water Leave Events:
        **/
        case EV_WATER_LEAVE_FEET:
                clgi.S_StartSound( NULL, entityNumber, CHAN_AUTO, clgi.S_RegisterSound( "player/water_feet_out01.wav" ), 1, ATTN_NORM, 0 );
                return true;
			break;
        case EV_WATER_LEAVE_WAIST:
                clgi.S_StartSound( NULL, entityNumber, CHAN_AUTO, clgi.S_RegisterSound( "player/water_body_out01.wav" ), 1, ATTN_NORM, 0 );
                return true;
            break;
        case EV_WATER_LEAVE_HEAD:
                //clgi.S_StartSound( NULL, entityNumber, CHAN_VOICE, clgi.S_RegisterSound( "player/gasp01.wav" ), 1, ATTN_NORM, 0 );
				// <Q2RTXP>: TODO: Gasping sounds when surfacing from underwater.
				// Requires tracking air_finished_time in client player entity.
                //if ( ent->air_finished_time < level.time ) {
                //    // gasp for air
                //    gi.sound( ent, CHAN_VOICE, gi.soundindex( "player/gasp01.wav" ), 1, ATTN_NORM, 0 );
                //    SVG_Player_PlayerNoise( ent, ent->s.origin, PNOISE_SELF );
                //} else  if ( ent->air_finished_time < level.time + 11_sec ) {
                //    // just break surface
                //    gi.sound( ent, CHAN_VOICE, gi.soundindex( "player/gasp02.wav" ), 1, ATTN_NORM, 0 );
                //}
                return true;
            break;

        /**
		*   Jump Events:
        **/
        case EV_JUMP_UP: {
                const std::string jump_up_sfx_path = SG_RandomResourcePath( "player/jump", "wav", 0, 1 );
                clgi.S_StartSound( NULL, entityNumber, CHAN_VOICE, clgi.S_RegisterSound( jump_up_sfx_path.c_str() ), 1, ATTN_NORM, 0 );
                return true;
            break;
        }

        /**
        *   Fall and Landing Events:
        **/
        case EV_FALL_SHORT:
                clgi.S_StartSound( NULL, entityNumber, CHAN_AUTO, clgi.S_RegisterSound( "player/fall02.wav" ), 1, ATTN_NORM, 0 );
                return true;
            break;
        case EV_FALL_MEDIUM:
                clgi.S_StartSound( NULL, entityNumber, CHAN_AUTO, clgi.S_RegisterSound( "player/land01.wav" ), 1, ATTN_NORM, 0 );
                return true;
            break;
        case EV_FALL_FAR:
                clgi.S_StartSound( NULL, entityNumber, CHAN_AUTO, clgi.S_RegisterSound( "player/fall01.wav" ), 1, ATTN_NORM, 0 );
                return true;
            break;

        /**
        *   Teleport Events:
        **/
        case EV_PLAYER_LOGIN:
                dl = CLG_AddMuzzleflashDLight( cent, fv, rv );
                VectorSet( dl->color, 0, 1, 0 );
                clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, clgi.S_RegisterSound( "world/mz_login.wav" ), 1, ATTN_NORM, 0 );
                CLG_LogoutEffect( &lerpOrigin.x, 3/*MZ_LOGIN*/ );
            return true;
        break;
        case EV_PLAYER_LOGOUT:
                dl = CLG_AddMuzzleflashDLight( cent, fv, rv ); 
                VectorSet( dl->color, 0, 1, 0 );
                clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, clgi.S_RegisterSound( "world/mz_logout.wav" ), 1, ATTN_NORM, 0 );
                CLG_LogoutEffect( &lerpOrigin.x, 4/*MZ_LOGOUT*/ );
            return true;
        break;
        case EV_PLAYER_TELEPORT:
                clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, clgi.S_RegisterSound( "misc/teleport01.wav" ), 1, ATTN_IDLE, 0 );
                CLG_TeleportParticles( &clgi.client->predictedFrame.ps.pmove.origin.x );
            return true;
        break;
        default:
			break;
    }

    // Proceed to executing the event.
    // -- Jump Up:
    if ( playerStateEvent == EV_JUMP_UP ) {
        //clgi.Print( PRINT_NOTICE, "%s: PS_EV_JUMP_UP\n", __func__ );
        // Set event state animation.
        SG_SKM_SetStateAnimation( model, &eventBodyState[ SKM_BODY_LOWER ], "jump_up_pistol", QMTime::FromMilliseconds( clgi.client->servertime ), BASE_FRAMETIME, false, true );

        // Did handle the player state event.
        return true;
    // -- Jump Land:
    } else if ( playerStateEvent == EV_JUMP_LAND ) {
        //clgi.Print( PRINT_NOTICE, "%s: PS_EV_JUMP_LAND\n", __func__ );
        // We really do NOT want this when we are crouched when having hit the ground.
        if ( !( ps->pmove.pm_flags & PMF_DUCKED ) ) {
            // Set event state animation.
            SG_SKM_SetStateAnimation( model, &eventBodyState[ SKM_BODY_LOWER ], "jump_land_pistol", QMTime::FromMilliseconds( clgi.client->servertime ), BASE_FRAMETIME, false, true );

            // Did handle the player state event.
            //return true;
        }
        return true;
    // -- Weapon Fire Primary:
    } else if ( playerStateEvent == EV_WEAPON_PRIMARY_FIRE ) {
        const std::string animationName = PrimaryFireEvent_DetermineAnimation( ops, ps, playerStateEvent, lerpOrigin );
        //clgi.Print( PRINT_NOTICE, "%s: PS_EV_WEAPON_PRIMARY_FIRE\n", __func__ );
        // Set event state animation.
        SG_SKM_SetStateAnimation( model, &eventBodyState[ SKM_BODY_UPPER ], animationName.c_str(), QMTime::FromMilliseconds( clgi.client->servertime ), BASE_FRAMETIME, false, true );

        // Did handle the player state event.
        return true;
    // -- Weapon Reload:
    } else if ( playerStateEvent == EV_WEAPON_RELOAD ) {
        //clgi.Print( PRINT_NOTICE, "%s: PS_EV_WEAPON_RELOAD\n", __func__ );
        // Set event state animation.
        SG_SKM_SetStateAnimation( model, &eventBodyState[ SKM_BODY_UPPER ], "reload_stand_pistol", QMTime::FromMilliseconds( clgi.client->servertime ), BASE_FRAMETIME, false, true );

        // Did handle the player state event.
        return true;
    #if 1
    // -- Weapon Holster AND Draw:
    } else if ( playerStateEvent == EV_WEAPON_HOLSTER_AND_DRAW ) {
        //clgi.Print( PRINT_NOTICE, "%s: PS_EV_WEAPON_RELOAD\n", __func__ );
        // Set event state animation.
        SG_SKM_SetStateAnimation( model, &eventBodyState[ SKM_BODY_UPPER ], "holster_draw_pistol", QMTime::FromMilliseconds( clgi.client->servertime ), BASE_FRAMETIME, false, true );

        // Did handle the player state event.
        return true;
    }
    #else
    // -- Weapon Draw:
    } else if ( playerStateEvent == EV_WEAPON_HOLSTER ) {
        //clgi.Print( PRINT_NOTICE, "%s: PS_EV_WEAPON_RELOAD\n", __func__ );
        // Set event state animation.
        SG_SKM_SetStateAnimation( model, &eventBodyState[ SKM_BODY_UPPER ], "holster_pistol", QMTime::FromMilliseconds( clgi.client->servertime ), BASE_FRAMETIME, false, true );

        // Did handle the player state event.
        return true;
    // -- Weapon Holster:
    } else if ( playerStateEvent == EV_WEAPON_DRAW ) {
        //clgi.Print( PRINT_NOTICE, "%s: PS_EV_WEAPON_RELOAD\n", __func__ );
        // Set event state animation.
        SG_SKM_SetStateAnimation( model, &eventBodyState[ SKM_BODY_UPPER ], "draw_pistol", QMTime::FromMilliseconds( clgi.client->servertime ), BASE_FRAMETIME, false, true );

        // Did handle the player state event.
        return true;
    }
    #endif

    // Did NOT handle any player state event.
    return false;
}