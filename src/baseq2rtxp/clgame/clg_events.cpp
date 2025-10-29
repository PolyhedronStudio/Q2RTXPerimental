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
#include "sharedgame/sg_entity_effects.h"
#include "sharedgame/sg_entity_types.h"



/**
*   @brief  Determines the 'fire' animation to play for the given primary fire event.
**/
static const std::string CLG_PrimaryFireEvent_DetermineAnimation( const player_state_t *ops, const player_state_t *ps, const int32_t playerStateEvent, const Vector3 &lerpOrigin ) {
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
*   Entity Event Handling:
*
*
*
**/
/**
*   @brief  Processes the given entity event.
**/
void CLG_ProcessEntityEvents( const int32_t eventValue, const Vector3 &lerpOrigin, centity_t *cent, const int32_t entityNumber, const int32_t clientNumber, clientinfo_t *clientInfo ) {

    // <Q2RTXP>: TODO: Fix so it doesn't do the teleporter at incorrect spawn origin.
    const Vector3 effectOrigin = lerpOrigin; // cent->current.origin 

    // EF_TELEPORTER acts like an event, but is not cleared each frame
    if ( ( cent->current.effects & EF_TELEPORTER ) ) {
        // Use player state pmove origin if we're the view bound entity.
        if ( CLG_GetViewBoundEntity() == cent && clientNumber != -1 ) {
            CLG_TeleporterParticles( &clgi.client->frame.ps.pmove.origin.x );
            clgi.Print( PRINT_DEVELOPER, "%s: PLAYER TELEPORTERRRRR (%f,%f,%f)\n",
                clgi.client->frame.ps.pmove.origin.x,
                clgi.client->frame.ps.pmove.origin.y,
                clgi.client->frame.ps.pmove.origin.z
            );
        // Otherwise, use lerp origin.
        } else {
            CLG_TeleporterParticles( &effectOrigin.x );

            clgi.Print( PRINT_DEVELOPER, "%s: ENTITY TELEPORTERRRRR (%f,%f,%f)\n",
                effectOrigin.x,
                effectOrigin.y,
                effectOrigin.z
            );
        }
    }

    // Handle the event.
    switch ( eventValue ) {
        ///**
        //*   FootStep Events:
        //**/
        //case EV_PLAYER_FOOTSTEP:
        //    CLG_FootstepEvent( entityNumber );
        //    break;
        case EV_FOOTSTEP_LADDER:
            CLG_FootstepLadderEvent( entityNumber );
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
            CLG_ItemRespawnParticles( &effectOrigin.x );
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

    #if 1
    /**
    *   Check for (temporary-) event-only entities
    **/
    if ( cent->current.entityType > ET_TEMP_ENTITY_EVENT ) {
        // Already fired.
        if ( cent->previousEvent ) {
            return;	
        }
        // If this is a player event set the entity number as that of the client entity number.
        if ( cent->current.effects & EF_PLAYER_EVENT ) {
            cent->current.number = cent->current.otherEntityNumber;
        }
		// Set previous event to true.
        cent->previousEvent = 1;
		// The event is simply the entity type minus the ET_EVENTS offset.
        cent->current.event = cent->current.entityType - ET_TEMP_ENTITY_EVENT;
    } else
    #endif
    /**
    *   Check for events riding with another entity:
    **/
    {
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
    *   Client Info Fetching by skinnum decoding:
    **/
    // The client info we'll acquire based on entity client number(decoded from skinnum if a player). 
    clientinfo_t *clientInfo = nullptr;
    // The client number of the entity, if any.
    int32_t clientNumber = -1;
	// Get the entity state.
    entity_state_t *currentEntityState = &cent->current;
    // Get client number of entity, if any.
    if ( cent->current.modelindex == MODELINDEX_PLAYER ) {
        // It's a player model, so decode the client number from skinnum.
        clientNumber = cent->current.skinnum & 0xFF;
        // Invalid client number by skin.
        if ( clientNumber < 0 || clientNumber > MAX_CLIENTS ) {
            clientNumber = -1;
        // Valid client number, get the client info.
        } else {
            clientInfo = &clgi.client->clientinfo[ clientNumber ];
            if ( clgi.client->clientEntity ) {
                currentEntityState = &clgi.client->clientEntity->current;
            }
        }
    }

    /**
    *   Event Value Decoding:
    **/
	// Acquire the actual event value by offing it with EV_EVENT_BITS.
    eventValue = SG_GetEntityEventValue( currentEntityState->event );

	// Debugging:
    if ( clg_debug_events->value ) {
        clgi.Print( PRINT_DEVELOPER, "ent:%3i  event:%3i \n", currentEntityState->number, eventValue );
        if ( !eventValue ) {
            clgi.Print( PRINT_DEVELOPER, "ZEROEVENT\n" );
            return;
        }
    }

	// If no event, don't process anything.
    if ( !eventValue ) {
        return;
    }


    // calculate the position at exactly the frame time
    //BG_EvaluateTrajectory( &cent->currentState.pos, cg.snap->serverTime, cent->lerpOrigin );
    //CG_SetEntitySoundPosition( cent );
     
    /**
    *   Determine LerpOrigin and Process the Entity Events:
    **/
    // Calculate the position for lerp_origin at exactly the frame time.
	PF_GetEntitySoundOrigin( cent->current.number, &cent->lerp_origin.x );
	// Process the event.
    // <Q2RTXP>: TODO: Use event value.
    CLG_ProcessEntityEvents( eventValue, cent->lerp_origin, cent, cent->current.number, clientNumber, clientInfo );
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
                CLG_LogoutEffect( &clgi.client->predictedFrame.ps.pmove.origin.x, 3/*MZ_LOGIN*/ );
            return true;
        break;
        case EV_PLAYER_LOGOUT:
                dl = CLG_AddMuzzleflashDLight( cent, fv, rv ); 
                VectorSet( dl->color, 0, 1, 0 );
                clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, clgi.S_RegisterSound( "world/mz_logout.wav" ), 1, ATTN_NORM, 0 );
                CLG_LogoutEffect( &clgi.client->predictedFrame.ps.pmove.origin.x, 4/*MZ_LOGOUT*/ );
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
        const std::string animationName = CLG_PrimaryFireEvent_DetermineAnimation( ops, ps, playerStateEvent, lerpOrigin );
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