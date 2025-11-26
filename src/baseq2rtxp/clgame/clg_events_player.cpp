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
#include "clgame/clg_events_player.h"
#include "clgame/clg_precache.h"

// Needed:
#include "sharedgame/sg_entity_events.h"
#include "sharedgame/sg_entity_flags.h"
#include "sharedgame/sg_entity_types.h"
#include "sharedgame/sg_muzzleflashes.h"



/**
*
*
*
*   Helper Functions:
*
*
*
**/
/**
*   @brief  Determines the 'fire' animation to play for the given primary fire event.
**/
static const std::string PrimaryFireEvent_DetermineAnimation( const player_state_t *ps, const int32_t playerStateEvent, const Vector3 &lerpOrigin ) {
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
*   Player State FootStep Events:
*
*
*
**/
/**
*   @brief  The player footstep event handler.
**/
static void CLG_PlayerEvent_FootStep( const int32_t entityNumber, const Vector3 &lerpOrigin ) {
	// Play the footstep sound.
    CLG_FX_FootStepSound( entityNumber, lerpOrigin, false, true );
}



/**
*
*
*
*   Player State Water Events:
*
*
*
**/
/**
*   @brief  
**/
static void CLG_PlayerEvent_WaterEnterFeet( const int32_t entityNumber ) {
    clgi.S_StartSound( NULL, entityNumber, CHAN_BODY, precache.sfx.players.water_feet_in01, 1, ATTN_NORM, 0 );
}
/**
*   @brief  
**/
static void CLG_PlayerEvent_WaterEnterWaist( const int32_t entityNumber ) {
        // Generate the random file index.
    const int32_t index = irandom( 0, 2 ) + 1;
    if ( index == 1 ) {
        clgi.S_StartSound( NULL, entityNumber, CHAN_BODY, precache.sfx.players.water_splash_in01, 1, ATTN_NORM, 0 );
	} else {
        clgi.S_StartSound( NULL, entityNumber, CHAN_BODY, precache.sfx.players.water_splash_in02, 1, ATTN_NORM, 0 );
    }
}
/**
*   @brief  
**/
static void CLG_PlayerEvent_WaterEnterHead( const int32_t entityNumber ) {
    clgi.S_StartSound( NULL, entityNumber, CHAN_BODY, precache.sfx.players.water_head_under01, 1, ATTN_NORM, 0 );
}

/**
*   @brief
**/
static void CLG_PlayerEvent_WaterLeaveFeet( const int32_t entityNumber ) {
    clgi.S_StartSound( NULL, entityNumber, CHAN_AUTO, precache.sfx.players.water_feet_out01, 1, ATTN_NORM, 0 );
}
/**
*   @brief
**/
static void CLG_PlayerEvent_WaterLeaveWaist( const int32_t entityNumber ) {
    clgi.S_StartSound( NULL, entityNumber, CHAN_AUTO, precache.sfx.players.water_body_out01, 1, ATTN_NORM, 0 );
}
/**
*   @brief
**/
static void CLG_PlayerEvent_WaterLeaveHead( const int32_t entityNumber ) {
    //clgi.S_StartSound( NULL, entityNumber, CHAN_BODY, clgi.S_RegisterSound( "player/water_head_under01.wav" ), 1, ATTN_NORM, 0 );
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
}



/**
*
*
*
*   Player State - Jump/Fall/Landing Events:
*
*
*
**/
/**
*   @brief
**/
static void CLG_PlayerEvent_JumpUp( centity_t *playerEntity, const int32_t entityNumber ) {
    /**
	*   Player the audio for jumping up.
    **/
	// Get a random jump sound.
    const std::string jump_up_sfx_path = SG_RandomResourcePath( "player/jump", "wav", 0, 1 );
	// Play the sound.
    clgi.S_StartSound( NULL, entityNumber, CHAN_VOICE, clgi.S_RegisterSound( jump_up_sfx_path.c_str() ), 1, ATTN_NORM, 0 );

    /**
	*   Engage into the jump animation.
    **/
            // Get model resource.
    const model_t *model = clgi.R_GetModelDataForHandle( clgi.client->baseclientinfo.model /*cent->current.modelindex*/ );

    // Get the animation state mixer.
    sg_skm_animation_mixer_t *animationMixer = &playerEntity->animationMixer;//&clgi.client->clientEntity->animationMixer;
    // Get the state for body events.
    sg_skm_animation_state_t *eventBodyState = animationMixer->eventBodyState;
	// Apply and set the jump animation.
    SG_SKM_SetStateAnimation(
        model,
        &eventBodyState[ SKM_BODY_LOWER ],
        "jump_up_pistol",
        QMTime::FromMilliseconds( clgi.client->servertime ),
        BASE_FRAMETIME,
        false   /* forceLoop */,
        true    /* forceSet */
    );
}

/**
*   @brief
**/
static void CLG_PlayerEvent_JumpLand( centity_t *playerEntity, const int32_t entityNumber, const player_state_t *ps ) {
    /**
    *   Engage into the landing animation.
    **/
            // Get model resource.
    const model_t *model = clgi.R_GetModelDataForHandle( clgi.client->baseclientinfo.model /*cent->current.modelindex*/ );

    // Get the animation state mixer.
    sg_skm_animation_mixer_t *animationMixer = &playerEntity->animationMixer;//&clgi.client->clientEntity->animationMixer;
    // Get the state for body events.
    sg_skm_animation_state_t *eventBodyState = animationMixer->eventBodyState;
    // We really do NOT want this when we are crouched when having hit the ground.
    if ( !( ps->pmove.pm_flags & PMF_DUCKED ) ) {
        // Apply and set the jump animation.
        SG_SKM_SetStateAnimation(
            model, 
            &eventBodyState[ SKM_BODY_LOWER ], 
            "jump_land_pistol", 
            QMTime::FromMilliseconds( clgi.client->servertime ), 
            BASE_FRAMETIME, 
            false   /* forceLoop */,
            true    /* forceSet */
        );
    }
}

/**
*   @brief
**/
static void CLG_PlayerEvent_ShortFall( const int32_t entityNumber ) {
    clgi.S_StartSound( NULL, entityNumber, CHAN_AUTO, precache.sfx.players.fall02, 1, ATTN_NORM, 0 );
}
/**
*   @brief
**/
static void CLG_PlayerEvent_MediumFall( const int32_t entityNumber ) {
    clgi.S_StartSound( NULL, entityNumber, CHAN_AUTO, precache.sfx.players.land01, 1, ATTN_NORM, 0 );
}
/**
*   @brief
**/
static void CLG_PlayerEvent_FarFall( const int32_t entityNumber ) {
    clgi.S_StartSound( NULL, entityNumber, CHAN_AUTO, precache.sfx.players.fall01, 1, ATTN_NORM, 0 );
}



/**
*
*
*
*   Player State - Teleport Events:
*
*
*
**/
/**
*   @brief
**/
static void CLG_PlayerEvent_Login( const centity_t *playerEntity, const int32_t entityNumber ) {
    // Add dynamic light.
	clg_dlight_t *dynamicLight = CLG_AddMuzzleflashDLight( playerEntity, playerEntity->vAngles.forward, playerEntity->vAngles.right );
    // Color it green.
	dynamicLight->color = Vector3{ 0.0f, 1.0f, 0.0f };
    // Play login sound.
    clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, precache.sfx.world.mz_login, 1, ATTN_NORM, 0 );
	// Spawn login effect.
	CLG_LogoutEffect( &playerEntity->lerp_origin.x, 3/*MZ_LOGIN*/ );
}
/**
*   @brief
**/
static void CLG_PlayerEvent_Logout( const centity_t *playerEntity, const int32_t entityNumber ) {
    // Add dynamic light.
    clg_dlight_t *dynamicLight = CLG_AddMuzzleflashDLight( playerEntity, playerEntity->vAngles.forward, playerEntity->vAngles.right );
    // Color it green.
    dynamicLight->color = Vector3{ 1.0f, 0.0f, 0.0f };
    // Play logout sound.
    clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, precache.sfx.world.mz_logout, 1, ATTN_NORM, 0 );
    // Spawn logout effect.
    CLG_LogoutEffect( &playerEntity->lerp_origin.x, 4/*MZ_LOGOUT*/ );
}
/**
*   @brief
**/
static void CLG_PlayerEvent_Teleport( const centity_t *playerEntity, const int32_t entityNumber ) {
    // Add dynamic light.
    clg_dlight_t *dynamicLight = CLG_AddMuzzleflashDLight( playerEntity, playerEntity->vAngles.forward, playerEntity->vAngles.right );
    // Color it green.
    dynamicLight->color = Vector3{ 0.75f, 0.75f, 0.0f };
    // Play login sound.
    clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, precache.sfx.world.mz_logout, 1, ATTN_NORM, 0 );
    // Spawn login effect.
    CLG_LogoutEffect( &playerEntity->lerp_origin.x, 4/*MZ_LOGOUT*/ );
}



/**
*
*
*
*   Player State - Weapon Events:
*
*
*
**/
/**
*   @brief
**/
static void CLG_PlayerEvent_WeaponPrimaryFire( centity_t *playerEntity, const int32_t entityNumber, const player_state_t *ps, const int32_t eventValue, const Vector3 &lerpOrigin ) {
    /**
	*   Get necessary data and play the animation for primary fire.
    **/
    // Get model resource.
    const model_t *model = clgi.R_GetModelDataForHandle( clgi.client->baseclientinfo.model /*cent->current.modelindex*/ );
    // Get the animation state mixer.
    sg_skm_animation_mixer_t *animationMixer = &playerEntity->animationMixer;//&clgi.client->clientEntity->animationMixer;
    // Get the state for body events.
    sg_skm_animation_state_t *eventBodyState = animationMixer->eventBodyState;
	// Determine the animation to play.
    const std::string animationName = PrimaryFireEvent_DetermineAnimation( ps, eventValue, lerpOrigin );

    // Set event state animation.
    SG_SKM_SetStateAnimation( 
        model, 
        &eventBodyState[ SKM_BODY_UPPER ], 
        animationName.c_str(), 
        QMTime::FromMilliseconds( clgi.client->servertime ), 
        BASE_FRAMETIME, 
        false   /* forceLoop */,
        true    /* forceSet */
    );
}
/**
*   @brief
**/
static void CLG_PlayerEvent_WeaponReload( centity_t *playerEntity, const int32_t entityNumber, const player_state_t *ps, const int32_t eventValue, const Vector3 &lerpOrigin ) {
    /**
    *   Get necessary data and play the animation for primary fire.
    **/
    // Get model resource.
    const model_t *model = clgi.R_GetModelDataForHandle( clgi.client->baseclientinfo.model /*cent->current.modelindex*/ );
    // Get the animation state mixer.
    sg_skm_animation_mixer_t *animationMixer = &playerEntity->animationMixer;//&clgi.client->clientEntity->animationMixer;
    // Get the state for body events.
    sg_skm_animation_state_t *eventBodyState = animationMixer->eventBodyState;

    // Set event state animation.
    SG_SKM_SetStateAnimation( 
        model, 
        &eventBodyState[ SKM_BODY_UPPER ], 
        "reload_stand_pistol", 
        QMTime::FromMilliseconds( clgi.client->servertime ), 
        BASE_FRAMETIME, 
        false   /* forceLoop */,
        true    /* forceSet */
    );
}
/**
*   @brief
**/
static void CLG_PlayerEvent_WeaponHolsterAndDraw( centity_t *playerEntity, const int32_t entityNumber, const player_state_t *ps, const int32_t eventValue, const Vector3 &lerpOrigin ) {
    /**
    *   Get necessary data and play the animation for primary fire.
    **/
    // Get model resource.
    const model_t *model = clgi.R_GetModelDataForHandle( clgi.client->baseclientinfo.model /*cent->current.modelindex*/ );
    // Get the animation state mixer.
    sg_skm_animation_mixer_t *animationMixer = &playerEntity->animationMixer;//&clgi.client->clientEntity->animationMixer;
    // Get the state for body events.
    sg_skm_animation_state_t *eventBodyState = animationMixer->eventBodyState;

    // Set event state animation.
    SG_SKM_SetStateAnimation(
        model,
        &eventBodyState[ SKM_BODY_UPPER ],
        "holster_draw_pistol",
        QMTime::FromMilliseconds( clgi.client->servertime ),
        BASE_FRAMETIME,
        false   /* forceLoop */,
        true    /* forceSet */
    );
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
*   @brief  Processes the given player state event.
**/
const bool CLG_Events_FirePlayerStateEvent( centity_t *playerEntity, const player_state_t *ops, const player_state_t *ps, const int32_t playerStateEvent, const Vector3 &lerpOrigin ) {
    // For dynamic lights.
	clg_dlight_t *dl = nullptr;

	// Get entity number.
    const int32_t entityNumber = playerEntity->current.number;

    #if 0
        // Calculate the angle vectors.
        Vector3 fv = {}, rv = {};
        QM_AngleVectors( /*cent->current.angles*/ ps->viewangles, &fv, &rv, NULL );

        // Get model resource.
        const model_t *model = clgi.R_GetModelDataForHandle( clgi.client->baseclientinfo.model /*cent->current.modelindex*/ );

        // Get the animation state mixer.
        sg_skm_animation_mixer_t *animationMixer = &playerEntity->animationMixer;//&clgi.client->clientEntity->animationMixer;
        // Get body states.
        sg_skm_animation_state_t *currentBodyState = animationMixer->currentBodyStates;
        sg_skm_animation_state_t *lastBodyState = animationMixer->lastBodyStates;

        sg_skm_animation_state_t *eventBodyState = animationMixer->eventBodyState;
    #endif

    switch ( playerStateEvent ) {
        /**
        *   FootStep Events:
        **/
        case EV_PLAYER_FOOTSTEP:
			DEBUG_PRINT_EVENT_NAME( "EV_PLAYER_FOOTSTEP" );
            CLG_PlayerEvent_FootStep( entityNumber, lerpOrigin );
            return true;
        break;
        // Applied externally outside of pmove.
        //case EV_FOOTSTEP_LADDER:
        //        CLG_FootStepLadderEvent( entityNumber );
        //        return true;
        //    break;

        /**
        *   Water Enter Events:
        **/
        case EV_WATER_ENTER_FEET:
    			DEBUG_PRINT_EVENT_NAME( "EV_WATER_ENTER_FEET" );
    			CLG_PlayerEvent_WaterEnterFeet( entityNumber );
				return true;
            break;
        case EV_WATER_ENTER_WAIST:
			    DEBUG_PRINT_EVENT_NAME( "EV_WATER_ENTER_WAIST" );
				CLG_PlayerEvent_WaterEnterWaist( entityNumber );
                return true;
            break;
        case EV_WATER_ENTER_HEAD:
			    DEBUG_PRINT_EVENT_NAME( "EV_WATER_ENTER_HEAD" );
                CLG_PlayerEvent_WaterEnterHead( entityNumber );
			    return true;
            break;

        /**
        *   Water Leave Events:
        **/
        case EV_WATER_LEAVE_FEET:
			    DEBUG_PRINT_EVENT_NAME( "EV_WATER_LEAVE_FEET" );
                CLG_PlayerEvent_WaterLeaveFeet( entityNumber );
                return true;
			break;
        case EV_WATER_LEAVE_WAIST:
			    DEBUG_PRINT_EVENT_NAME( "EV_WATER_LEAVE_WAIST" );
                CLG_PlayerEvent_WaterLeaveWaist( entityNumber );
                return true;
            break;
        case EV_WATER_LEAVE_HEAD:
			    DEBUG_PRINT_EVENT_NAME( "EV_WATER_LEAVE_HEAD" );
    			CLG_PlayerEvent_WaterLeaveHead( entityNumber );
                return true;
            break;

        /**
		*   Jump Events:
        **/
        case EV_JUMP_UP:
			    DEBUG_PRINT_EVENT_NAME( "EV_JUMP_UP" );
		        CLG_PlayerEvent_JumpUp( playerEntity, entityNumber );
                return true;
            break;
        case EV_JUMP_LAND:
			    DEBUG_PRINT_EVENT_NAME( "EV_JUMP_LAND" );
			    CLG_PlayerEvent_JumpLand( playerEntity, entityNumber, ps );
                return true;
			break;

        /**
        *   Fall and Landing Events:
        **/
        case EV_FALL_SHORT:
			    DEBUG_PRINT_EVENT_NAME( "EV_FALL_SHORT" );
			    CLG_PlayerEvent_ShortFall( entityNumber );
                return true;
            break;
        case EV_FALL_MEDIUM:
			    DEBUG_PRINT_EVENT_NAME( "EV_FALL_MEDIUM" );
                CLG_PlayerEvent_MediumFall( entityNumber );
                return true;
            break;
        case EV_FALL_FAR:
			    DEBUG_PRINT_EVENT_NAME( "EV_FALL_FAR" );
                CLG_PlayerEvent_FarFall( entityNumber );
                return true;
            break;

        /**
        *   Teleport Events:
        **/
        case EV_PLAYER_LOGIN:
			DEBUG_PRINT_EVENT_NAME( "EV_PLAYER_LOGIN" );
			    CLG_PlayerEvent_Login( playerEntity, entityNumber );
                return true;
            break;
        case EV_PLAYER_LOGOUT:
			    DEBUG_PRINT_EVENT_NAME( "EV_PLAYER_LOGOUT" );
                CLG_PlayerEvent_Logout( playerEntity, entityNumber );
                return true;
            break;
        case EV_PLAYER_TELEPORT:
			    DEBUG_PRINT_EVENT_NAME( "EV_PLAYER_TELEPORT" );
                CLG_PlayerEvent_Teleport( playerEntity, entityNumber );
                return true;
            break;

        /**
		*   Weapon Events:
        **/
		case EV_WEAPON_PRIMARY_FIRE:
			    DEBUG_PRINT_EVENT_NAME( "EV_WEAPON_PRIMARY_FIRE" );
			    CLG_PlayerEvent_WeaponPrimaryFire( playerEntity, entityNumber, ps, playerStateEvent, lerpOrigin );
                return true;
            break;
		case EV_WEAPON_RELOAD:
			    DEBUG_PRINT_EVENT_NAME( "EV_WEAPON_RELOAD" );
                CLG_PlayerEvent_WeaponReload( playerEntity, entityNumber, ps, playerStateEvent, lerpOrigin );
                return true;
            break;
        case EV_WEAPON_HOLSTER_AND_DRAW:
			    DEBUG_PRINT_EVENT_NAME( "EV_WEAPON_HOLSTER_AND_DRAW" );
			    CLG_PlayerEvent_WeaponHolsterAndDraw( playerEntity, entityNumber, ps, playerStateEvent, lerpOrigin );
                return true;
            break;
        default:
			break;
    }

    #if 0
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