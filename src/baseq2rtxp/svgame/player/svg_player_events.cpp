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
#include "svgame/svg_local.h"
#include "svgame/svg_entity_events.h"
#include "svgame/player/svg_player_events.h"
#include "sharedgame/sg_shared.h"
#include "sharedgame/sg_means_of_death.h"
#include "sharedgame/sg_gamemode.h"
#include "sharedgame/sg_usetarget_hints.h"
#include "sharedgame/pmove/sg_pmove.h"
#include "svgame/entities/svg_player_edict.h"



/**
*
*
*
*   Event Implementations::
*
*
*
**/
/**
*   @brief  Water entry event processing.
**/
static void PlayerStateEvent_WaterEnter( svg_player_edict_t *ent, const int32_t event, const int32_t eventParm ) {
	// No-clip mode doesn't make a splash.
    if ( ent->movetype == MOVETYPE_NOCLIP ) {
        return;
    }
    
	// Determine splash type.
    if ( event == EV_WATER_ENTER_FEET ) {
        SVG_Player_PlayerNoise( ent, ent->s.origin, PNOISE_SELF );
        if ( ent->liquidInfo.type & CONTENTS_LAVA ) {
            //gi.sound( current_player, CHAN_BODY, gi.soundindex( "player/lava_in.wav" ), 1, ATTN_NORM, 0 );
            gi.sound( ent, CHAN_BODY, gi.soundindex( "player/burn01.wav" ), 1, ATTN_NORM, 0 );
            // clear damage_debounce, so the pain sound will play immediately
            ent->damage_debounce_time = level.time - 1_sec;
        }
        #if 0
            else if ( ent->liquidInfo.type & CONTENTS_SLIME ) {
                gi.sound( ent, CHAN_BODY, gi.soundindex( "player/water_feet_in01.wav" ), 1, ATTN_NORM, 0 );
            } else if ( ent->liquidInfo.type & CONTENTS_WATER ) {
                gi.sound( ent, CHAN_BODY, gi.soundindex( "player/water_feet_in01.wav" ), 1, ATTN_NORM, 0 );
            }
        #endif
        ent->flags |= FL_INWATER;
    } else if ( event >= EV_WATER_ENTER_WAIST ) {
        SVG_Player_PlayerNoise( ent, ent->s.origin, PNOISE_SELF );
        if ( ent->liquidInfo.type & CONTENTS_LAVA ) {
            gi.sound( ent, CHAN_BODY, gi.soundindex( "player/burn02.wav" ), 1, ATTN_NORM, 0 );
            // clear damage_debounce, so the pain sound will play immediately
            ent->damage_debounce_time = level.time - 1_sec;
        }
        #if 0
            else if ( ent->liquidInfo.type & CONTENTS_SLIME ) {
                const std::string splash_sfx_path = SG_RandomResourcePath( "player/water_splash_in", "wav", 0, 2 );
                gi.sound( ent, CHAN_AUTO, gi.soundindex( splash_sfx_path.c_str() ), 1, ATTN_NORM, 0 );
            } else if ( ent->liquidInfo.type & CONTENTS_WATER ) {
                const std::string splash_sfx_path = SG_RandomResourcePath( "player/water_splash_in", "wav", 0, 2 );
                gi.sound( ent, CHAN_AUTO, gi.soundindex( splash_sfx_path.c_str() ), 1, ATTN_NORM, 0 );
            }
        #endif
        ent->flags |= FL_INWATER;
    }

    // Check for head just going under water.
    if ( event >= EV_WATER_ENTER_HEAD ) {
        //gi.sound( ent, CHAN_BODY, gi.soundindex( "player/water_head_under01.wav" ), 1, ATTN_NORM, 0 );
        ent->flags |= FL_INWATER;
    }
}
/**
*   @brief  Water leave event processing.
**/
static void PlayerStateEvent_WaterLeave( svg_player_edict_t *ent, const int32_t event, const int32_t eventParm ) {
    // No-clip mode doesn't make a splash.
    if ( ent->movetype == MOVETYPE_NOCLIP ) {
        return;
    }

    // Determine splash type.
    if ( event == EV_WATER_LEAVE_FEET ) {
		// Generate the sound as a temporary event entity.
		svg_base_edict_t *tEeSound = SVG_TempEventEntity_GeneralSound( ent, CHAN_AUTO, gi.soundindex( "player/water_feet_out01.wav" ) );
        // It is locally predicted, so exclude the client itself from the event.
        tEeSound->svFlags |= SVF_SENDCLIENT_EXCLUDE_ID;
        tEeSound->sendClientID = ent->client->clientNum;
        // Alarm the environment to the player's noise.
        SVG_Player_PlayerNoise( ent, ent->s.origin, PNOISE_SELF );
        //gi.sound( ent, CHAN_AUTO, gi.soundindex( "player/water_feet_out01.wav" ), 1, ATTN_NORM, 0 );
        ent->flags &= ~FL_INWATER;
    }
    if ( event == EV_WATER_LEAVE_WAIST ) {
        // Generate the sound as a temporary event entity.
        svg_base_edict_t *tempEntityEvent = SVG_TempEventEntity_GeneralSound( ent, CHAN_AUTO, gi.soundindex( "player/water_body_out01.wav" ) );
        // It is locally predicted, so exclude the client itself from the event.
        tempEntityEvent->svFlags |= SVF_SENDCLIENT_EXCLUDE_ID;
        tempEntityEvent->sendClientID = ent->client->clientNum;
        // Alarm the environment to the player's noise.
        SVG_Player_PlayerNoise( ent, ent->s.origin, PNOISE_SELF );
        //gi.sound( ent, CHAN_AUTO, gi.soundindex( "player/water_body_out01.wav" ), 1, ATTN_NORM, 0 );
        ent->flags &= ~FL_INWATER;
    }
    // <Q2RTXP>: TODO: Can be implemented client-side, so exclude the client ID from this entity event.
	if ( event == EV_WATER_LEAVE_HEAD ) {
        if ( ent->air_finished_time < level.time ) {
            // Generate the sound as a temporary event entity.: Gasp for air.
            svg_base_edict_t *tempEntityEvent = SVG_TempEventEntity_GeneralSoundEx( ent, CHAN_VOICE, gi.soundindex( "player/gasp01.wav" ), ATTN_NORM );
            //gi.sound( ent, CHAN_VOICE, gi.soundindex( "player/gasp01.wav" ), 1, ATTN_NORM, 0 );
            
            // Alarm the environment to the player's noise.
            SVG_Player_PlayerNoise( ent, ent->s.origin, PNOISE_SELF );
        } else  if ( ent->air_finished_time < level.time + 11_sec ) {
            // Generate the sound as a temporary event entity: Just break the surface.
            svg_base_edict_t *tempEntityEvent = SVG_TempEventEntity_GeneralSoundEx( ent, CHAN_VOICE, gi.soundindex( "player/gasp02.wav" ), ATTN_NORM );
            //gi.sound( ent, CHAN_VOICE, gi.soundindex( "player/gasp02.wav" ), 1, ATTN_NORM, 0 );
        }
    }
}

/**
*   @brief  Fall to the ground event processing.
**/
static void PlayerStateEvent_Fall( svg_player_edict_t *ent, const int32_t event, const int32_t eventParm ) {
    // Dead stuff can't crater.
    if ( ent->health <= 0 || ent->lifeStatus > entity_lifestatus_t::LIFESTATUS_ALIVE ) {
        return;
    }

    if ( ent->s.modelindex != MODELINDEX_PLAYER ) {
        return; // not in the player model
    }

    if ( ent->movetype == MOVETYPE_NOCLIP ) {
        return;
    }

    // Never take falling damage if completely underwater.
    if ( ent->liquidInfo.level == LIQUID_UNDER ) {
        return;
    }

    // Get impact delta.
    const double delta = eventParm;

    // Damage is too small to be taken into consideration.
    if ( delta < 7. ) {
        return;
    }
    // WID: restart footstep timer <- NO MORE!! Because doing so causes the weapon bob 
    // position to instantly shift back to start.
    //ent->client->ps.bobCycle = 0;

    //if ( ent->client->landmark_free_fall ) {
    //    delta = min( 30.f, delta );
    //    ent->client->landmark_free_fall = false;
    //    ent->client->landmark_noise_time = level.time + 100_ms;
    //}

    //if ( delta < 15  ) {
    //    if ( !( pm.state->pmove.pm_flags & PMF_ON_LADDER ) ) {
    //        ent->s.event = EV_FOOTSTEP;
    //        gi.dprintf( "%s: delta < 15 footstep\n", __func__ );
    //    }
    //    return;
    //}

    // Calculate the fall value for view adjustments.
    ent->client->viewMove.fallValue = delta * 0.5f;
    if ( ent->client->viewMove.fallValue > 40 ) {
        ent->client->viewMove.fallValue = 40;
    }
    ent->client->viewMove.fallTime = level.time + FALL_TIME();

    // Apply fall event based on delta.
    if ( delta > 30. ) {
        // WID: We DO want the VOICE channel to SHOUT in PAIN
        //ent->pain_debounce_time = level.time + FRAME_TIME_S; // No normal pain sound.

        int32_t damage = (int32_t)( ( delta - 30. ) / 2. );
        if ( damage < 1 ) {
            damage = 1;
        }
        Vector3 dir = { 0., 0., 1. };
        
        SVG_DamageEntity( ent, world, world, &dir.x, ent->s.origin, vec3_origin, damage, 0, DAMAGE_NONE, MEANS_OF_DEATH_FALLING );
    }

    // Falling damage noises alert monsters
    if ( ent->health >= 0 ) { // Was: if ( ent->health )
        SVG_Player_PlayerNoise( ent, &ent->client->ps.pmove.origin[ 0 ], PNOISE_SELF );
    }
}



/**
*
*
*
*   Event Firing:
*
*
*
**/
/**
*	@brief	Inspects the player state events for any events which may fire animation playbacks.
**/
static void SVG_PlayerState_FireEvent( svg_player_edict_t *ent, const int32_t playerStateEvent, const int32_t playerStateEventParm ) {
    // Sanity check.
    if ( !ent ) {
        return;
    }

    // Return if not viewing a player model entity.
    if ( ent->s.modelindex != MODELINDEX_PLAYER ) {
        return;     // not in the player model
    }

    // Acquire its client info.
    svg_client_t *client = ent->client;
    if ( !client ) {
        return; // Need a client entity.
    }

    /**
    *   Regular Events:
    **/
    #ifdef _DEBUG_PRINT_PLAYER_EVENTS
    gi.dprintf( "PLAYER EVENT: name(\"%s\")eventID(#%d), maskedeventID(#%d), parm(%d)\n", 
        sg_event_string_names[ playerStateEvent ], 
        playerStateEvent & ~EV_EVENT_BITS, 
        playerStateEvent, 
        playerStateEventParm 
    );
    #endif

	// All fall events are handled the same way.
    if ( playerStateEvent == EV_FALL_SHORT
        || playerStateEvent == EV_FALL_MEDIUM 
        || playerStateEvent == EV_FALL_FAR ) {
            PlayerStateEvent_Fall( ent, playerStateEvent, playerStateEventParm );
        return;
    }
	// All water entry and exit events are handled the same way.
    if ( playerStateEvent == EV_WATER_ENTER_FEET
        || playerStateEvent == EV_WATER_ENTER_WAIST
        || playerStateEvent == EV_WATER_ENTER_HEAD ) {
            PlayerStateEvent_WaterEnter( ent, playerStateEvent, playerStateEventParm );
        return;
    }
    // All water entry and exit events are handled the same way.
    if ( playerStateEvent == EV_WATER_LEAVE_FEET
        || playerStateEvent == EV_WATER_LEAVE_WAIST
        || playerStateEvent == EV_WATER_LEAVE_HEAD ) {
            PlayerStateEvent_WaterLeave( ent, playerStateEvent, playerStateEventParm );
        return;
    }

    // Get the model resource.
    const model_t *model = gi.GetModelDataForName( ent->model );

    /**
    *   Animation Events:
    **/
    // Get animation mixer.
    sg_skm_animation_mixer_t *animationMixer = &client->animationMixer;

    // Get body states.
    sg_skm_animation_state_t *currentBodyState = animationMixer->currentBodyStates;
    sg_skm_animation_state_t *lastBodyState = animationMixer->lastBodyStates;

    sg_skm_animation_state_t *eventBodyState = animationMixer->eventBodyState;

    // Proceed to executing the event.
    if ( playerStateEvent == EV_JUMP_UP ) {
        //clgi.Print( PRINT_NOTICE, "%s: PS_EV_JUMP_UP\n", __func__ );
        // Set event state animation.
        SG_SKM_SetStateAnimation( model, &eventBodyState[ SKM_BODY_LOWER ], "jump_up_pistol", level.time, BASE_FRAMETIME, false, true );
    } else if ( playerStateEvent == EV_JUMP_LAND ) {
        //clgi.Print( PRINT_NOTICE, "%s: PS_EV_JUMP_LAND\n", __func__ );
        // We really do NOT want this when we are crouched when having hit the ground.
        if ( !( client->ps.pmove.pm_flags & PMF_DUCKED ) ) {
            // Set event state animation.
            SG_SKM_SetStateAnimation( model, &eventBodyState[ SKM_BODY_LOWER ], "jump_land_pistol", level.time, BASE_FRAMETIME, false, true );
        }
    } else if ( playerStateEvent == EV_WEAPON_PRIMARY_FIRE ) {
        const std::string animationName = "fire_stand_pistol"; // CLG_PrimaryFireEvent_DetermineAnimation( ops, ps, playerStateEvent );
        //clgi.Print( PRINT_NOTICE, "%s: PS_EV_WEAPON_PRIMARY_FIRE\n", __func__ );
        // Set event state animation.
        SG_SKM_SetStateAnimation( model, &eventBodyState[ SKM_BODY_UPPER ], animationName.c_str(), level.time, BASE_FRAMETIME, false, true );
    } else if ( playerStateEvent == EV_WEAPON_RELOAD ) {
        //clgi.Print( PRINT_NOTICE, "%s: PS_EV_WEAPON_RELOAD\n", __func__ );
        // Set event state animation.
        SG_SKM_SetStateAnimation( model, &eventBodyState[ SKM_BODY_UPPER ], "reload_stand_pistol", level.time, BASE_FRAMETIME, false, true );
    }
}

/**
*   @brief  Checks for player state generated events(usually by PMove) and processed them for execution.
**/
void SVG_PlayerState_CheckForEvents( svg_player_edict_t *ent, player_state_t *ops, player_state_t *ps, const int64_t oldEventSequence ) {
    // Sanity check.
    if ( !ent ) {
        return;
    }

    // Acquire its client info.
    svg_client_t *client = ent->client;
    if ( !client ) {
        return; // Need a client entity.
    }

    // WID: We don't have support for external events yet. In fact, they would in Q3 style rely on
    // 'temp entities', which are alike a normal entity. Point being is this requires too much refactoring
    // right now.
    #if 0
    if ( ps->externalEvent && ps->externalEvent != ops->externalEvent ) {
        centity_t *clientEntity = clgi.client->clientEntity;//cent = &cg_entities[ ps->clientNum + 1 ];
        clientEntity->currentState.event = ps->externalEvent;
        clientEntity->currentState.eventParm = ps->externalEventParm;
        CG_EntityEvent( clientEntity, clientEntity->lerpOrigin );
    }
    #endif

    // 
    int64_t _oldEventSequence = oldEventSequence;
    if ( _oldEventSequence < client->ps.eventSequence - MAX_PS_EVENTS ) {
        _oldEventSequence = client->ps.eventSequence - MAX_PS_EVENTS;
    }
    //centity_t *clientEntity= &clg_entities[ clgi.client->frame.clientNum + 1 ];//clgi.client->clientEntity; // cg_entities[ ps->clientNum + 1 ];
    // go through the predictable events buffer
    for ( int64_t i = _oldEventSequence; i < ps->eventSequence; i++ ) {
        // Get the event number.
        const int32_t playerStateEvent = ps->events[ i & ( MAX_PS_EVENTS - 1 ) ];
        const int32_t playerStateEventParm = ps->eventParms[ i & ( MAX_PS_EVENTS - 1 ) ];

        // Proceed to firing the predicted/received event.
        SVG_PlayerState_FireEvent( ent, /*ops, ps, */playerStateEvent, playerStateEventParm );
    }
}
