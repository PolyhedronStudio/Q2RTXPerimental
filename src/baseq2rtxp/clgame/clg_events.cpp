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
        CLG_TeleporterParticles( &effectOrigin.x );
    }

    // Handle the event.
    switch ( eventValue ) {
        case EV_ITEM_RESPAWN:
            clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, clgi.S_RegisterSound( "items/respawn01.wav" ), 1, ATTN_IDLE, 0 );
            CLG_ItemRespawnParticles( &effectOrigin.x );
            break;
        case EV_PLAYER_TELEPORT:
            clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, clgi.S_RegisterSound( "misc/teleport01.wav" ), 1, ATTN_IDLE, 0 );
            CLG_TeleportParticles( &effectOrigin.x );
            break;
        case EV_FOOTSTEP:
            CLG_FootstepEvent( entityNumber );
            break;
        case EV_FOOTSTEP_LADDER:
            CLG_FootstepLadderEvent( entityNumber );
            break;
        case EV_FALLSHORT:
            clgi.S_StartSound( NULL, entityNumber, CHAN_AUTO, clgi.S_RegisterSound( "player/land01.wav" ), 1, ATTN_NORM, 0 );
            break;
        case EV_FALL:
            clgi.S_StartSound( NULL, entityNumber, CHAN_AUTO, clgi.S_RegisterSound( "player/fall02.wav" ), 1, ATTN_NORM, 0 );
            break;
        case EV_FALLFAR:
            clgi.S_StartSound( NULL, entityNumber, CHAN_AUTO, clgi.S_RegisterSound( "player/fall01.wav" ), 1, ATTN_NORM, 0 );
            break;
        default:
            break;
    }
}
/**
*   @brief  Checks for entity generated events and processes them for execution.
**/
void CLG_CheckForEntityEvents( centity_t *cent ) {
    #if 0
    // Check for event-only entities
    if ( cent->current.entityType > ET_EVENTS ) {
        if ( cent->previousEvent ) {
            return;	// already fired
        }
        // if this is a player event set the entity number of the client entity number
        if ( cent->current.eFlags & EF_PLAYER_EVENT ) {
            cent->current.number = cent->current.otherEntityNum;
        }

        cent->previousEvent = 1;

        cent->current.event = cent->current.entityType - ET_EVENTS;
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
		// If no event, don't process anything. ( It hasn't changed again. )
        if ( ( cent->current.event & ~EV_EVENT_BITS ) == 0 ) {
            return;
        }
    }

    /**
    *   Event Value Decoding:
    **/
	//  Get the entity state.
    entity_state_t *currentEntityState = &cent->current;
	// Acquire the actual event value by offing it with EV_EVENT_BITS.
    const int32_t eventValue = currentEntityState->event & ~EV_EVENT_BITS;

    #if 0
    if ( cg_debugEvents.integer ) {
        CG_Printf( "ent:%3i  event:%3i ", currentEntityState->number, eventValue );
    }

    if ( !eventValue ) {
        DEBUGNAME( "ZEROEVENT" );
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
		// It's a player model, so decode the client number from skinnum.
        clientNumber = currentEntityState->skinnum & 0xFF;
        // Invalid client number by skin.
        if ( clientNumber < 0 || clientNumber >= MAX_CLIENTS ) {
            clientNumber = -1;
        // Valid client number, get the client info.
        } else {
			clientInfo = &clgi.client->clientinfo[ clientNumber ];
        }
    }

    // calculate the position at exactly the frame time
    //BG_EvaluateTrajectory( &cent->currentState.pos, cg.snap->serverTime, cent->lerpOrigin );
    //CG_SetEntitySoundPosition( cent );
     
    /**
    *   Determine LerpOrigin and Process the Entity Events:
    **/
    // Calculate the position for lerp_origin at exactly the frame time.
	PF_GetEntitySoundOrigin( cent->current.number, cent->lerp_origin );
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
*   @brief  Checks for player state generated events(usually by PMove) and processed them for execution.
**/
void CLG_FirePlayerStateEvent( const player_state_t *ops, const player_state_t *ps, const int32_t playerStateEvent, const Vector3 &lerpOrigin ) {
    //if ( !clgi.client->clientEntity ) {
    //    return;
    //}
    //centity_t *cent = clgi.client->clientEntity;
    centity_t *cent = &clg_entities[ clgi.client->frame.clientNum + 1 ];

    if ( cent->serverframe != clgi.client->frame.number ) {
        return;
    }

    // Get model resource.
    const model_t *model = clgi.R_GetModelDataForHandle( clgi.client->baseclientinfo.model /*cent->current.modelindex*/ );

    // Get the animation state mixer.
    sg_skm_animation_mixer_t *animationMixer = &cent->animationMixer;//&clgi.client->clientEntity->animationMixer;
    // Get body states.
    sg_skm_animation_state_t *currentBodyState = animationMixer->currentBodyStates;
    sg_skm_animation_state_t *lastBodyState = animationMixer->lastBodyStates;

    sg_skm_animation_state_t *eventBodyState = animationMixer->eventBodyState;

    // Proceed to executing the event.
    // -- Jump Up:
    if ( playerStateEvent == PS_EV_JUMP_UP ) {
        //clgi.Print( PRINT_NOTICE, "%s: PS_EV_JUMP_UP\n", __func__ );
        // Set event state animation.
        SG_SKM_SetStateAnimation( model, &eventBodyState[ SKM_BODY_LOWER ], "jump_up_pistol", QMTime::FromMilliseconds( clgi.client->servertime ), BASE_FRAMETIME, false, true );
    // -- Jump Land:
    } else if ( playerStateEvent == PS_EV_JUMP_LAND ) {
        //clgi.Print( PRINT_NOTICE, "%s: PS_EV_JUMP_LAND\n", __func__ );
        // We really do NOT want this when we are crouched when having hit the ground.
        if ( !( ps->pmove.pm_flags & PMF_DUCKED ) ) {
            // Set event state animation.
            SG_SKM_SetStateAnimation( model, &eventBodyState[ SKM_BODY_LOWER ], "jump_land_pistol", QMTime::FromMilliseconds( clgi.client->servertime ), BASE_FRAMETIME, false, true );
        }
    // -- Weapon Fire Primary:
    } else if ( playerStateEvent == PS_EV_WEAPON_PRIMARY_FIRE ) {
        const std::string animationName = CLG_PrimaryFireEvent_DetermineAnimation( ops, ps, playerStateEvent, lerpOrigin );
        //clgi.Print( PRINT_NOTICE, "%s: PS_EV_WEAPON_PRIMARY_FIRE\n", __func__ );
        // Set event state animation.
        SG_SKM_SetStateAnimation( model, &eventBodyState[ SKM_BODY_UPPER ], animationName.c_str(), QMTime::FromMilliseconds( clgi.client->servertime ), BASE_FRAMETIME, false, true );
    // -- Weapon Reload:
    } else if ( playerStateEvent == PS_EV_WEAPON_RELOAD ) {
        //clgi.Print( PRINT_NOTICE, "%s: PS_EV_WEAPON_RELOAD\n", __func__ );
        // Set event state animation.
        SG_SKM_SetStateAnimation( model, &eventBodyState[ SKM_BODY_UPPER ], "reload_stand_pistol", QMTime::FromMilliseconds( clgi.client->servertime ), BASE_FRAMETIME, false, true );
    
    #if 1
    // -- Weapon Holster AND Draw:
    } else if ( playerStateEvent == PS_EV_WEAPON_HOLSTER_AND_DRAW ) {
        //clgi.Print( PRINT_NOTICE, "%s: PS_EV_WEAPON_RELOAD\n", __func__ );
        // Set event state animation.
        SG_SKM_SetStateAnimation( model, &eventBodyState[ SKM_BODY_UPPER ], "holster_draw_pistol", QMTime::FromMilliseconds( clgi.client->servertime ), BASE_FRAMETIME, false, true );
    }
    #else
    // -- Weapon Draw:
    } else if ( playerStateEvent == PS_EV_WEAPON_HOLSTER ) {
        //clgi.Print( PRINT_NOTICE, "%s: PS_EV_WEAPON_RELOAD\n", __func__ );
        // Set event state animation.
        SG_SKM_SetStateAnimation( model, &eventBodyState[ SKM_BODY_UPPER ], "holster_pistol", QMTime::FromMilliseconds( clgi.client->servertime ), BASE_FRAMETIME, false, true );
    // -- Weapon Holster:
    } else if ( playerStateEvent == PS_EV_WEAPON_DRAW ) {
        //clgi.Print( PRINT_NOTICE, "%s: PS_EV_WEAPON_RELOAD\n", __func__ );
        // Set event state animation.
        SG_SKM_SetStateAnimation( model, &eventBodyState[ SKM_BODY_UPPER ], "draw_pistol", QMTime::FromMilliseconds( clgi.client->servertime ), BASE_FRAMETIME, false, true );
    }
    #endif
}