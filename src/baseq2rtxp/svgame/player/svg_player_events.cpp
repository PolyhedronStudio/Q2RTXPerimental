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

/**
*	@brief	Inspects the player state events for any events which may fire animation playbacks.
**/
static void SVG_FireClientPlayerStateEvent( const edict_t *ent, const player_state_t *ops, const player_state_t *ps, const int32_t playerStateEvent ) {
    // Sanity check.
    if ( !ent ) {
        return;
    }

    // Return if not viewing a player model entity.
    if ( ent->s.modelindex != MODELINDEX_PLAYER ) {
        return;     // not in the player model
    }

    // Acquire its client info.
    gclient_t *client = ent->client;
    if ( !client ) {
        return; // Need a client entity.
    }

    // Get the model resource.
    const model_t *model = gi.GetModelDataForName( ent->model );

    // Get animation mixer.
    sg_skm_animation_mixer_t *animationMixer = &client->animationMixer;

    // Get body states.
    sg_skm_animation_state_t *currentBodyState = animationMixer->currentBodyStates;
    sg_skm_animation_state_t *lastBodyState = animationMixer->lastBodyStates;

    sg_skm_animation_state_t *eventBodyState = animationMixer->eventBodyState;

    // Proceed to executing the event.
    if ( playerStateEvent == PS_EV_JUMP_UP ) {
        //clgi.Print( PRINT_NOTICE, "%s: PS_EV_JUMP_UP\n", __func__ );
        // Set event state animation.
        SG_SKM_SetStateAnimation( model, &eventBodyState[ SKM_BODY_LOWER ], "jump_up_pistol", level.time, BASE_FRAMETIME, false, true );
    } else if ( playerStateEvent == PS_EV_JUMP_LAND ) {
        //clgi.Print( PRINT_NOTICE, "%s: PS_EV_JUMP_LAND\n", __func__ );
        // We really do NOT want this when we are crouched when having hit the ground.
        if ( !( ps->pmove.pm_flags & PMF_DUCKED ) ) {
            // Set event state animation.
            SG_SKM_SetStateAnimation( model, &eventBodyState[ SKM_BODY_LOWER ], "jump_land_pistol", level.time, BASE_FRAMETIME, false, true );
        }
    } else if ( playerStateEvent == PS_EV_WEAPON_PRIMARY_FIRE ) {
        const std::string animationName = "fire_stand_pistol"; // CLG_PrimaryFireEvent_DetermineAnimation( ops, ps, playerStateEvent );
        //clgi.Print( PRINT_NOTICE, "%s: PS_EV_WEAPON_PRIMARY_FIRE\n", __func__ );
        // Set event state animation.
        SG_SKM_SetStateAnimation( model, &eventBodyState[ SKM_BODY_UPPER ], animationName.c_str(), level.time, BASE_FRAMETIME, false, true );
    } else if ( playerStateEvent == PS_EV_WEAPON_RELOAD ) {
        //clgi.Print( PRINT_NOTICE, "%s: PS_EV_WEAPON_RELOAD\n", __func__ );
        // Set event state animation.
        SG_SKM_SetStateAnimation( model, &eventBodyState[ SKM_BODY_UPPER ], "reload_stand_pistol", level.time, BASE_FRAMETIME, false, true );
    }
}

/**
*   @brief  Checks for player state generated events(usually by PMove) and processed them for execution.
**/
void SVG_CheckClientPlayerstateEvents( const edict_t *ent, player_state_t *ops, player_state_t *ps ) {
    // Sanity check.
    if ( !ent ) {
        return;
    }

    // Acquire its client info.
    gclient_t *client = ent->client;
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
    //centity_t *clientEntity= &clg_entities[ clgi.client->frame.clientNum + 1 ];//clgi.client->clientEntity; // cg_entities[ ps->clientNum + 1 ];
    // go through the predictable events buffer
    for ( int64_t i = ps->eventSequence - MAX_PS_EVENTS; i < ps->eventSequence; i++ ) {
        // If we have a new predictable event:
        if ( i >= ops->eventSequence
            // OR the server told us to play another event instead of a predicted event we already issued
            // OR something the server told us changed our prediction causing a different event
            || ( i > ops->eventSequence - MAX_PS_EVENTS && ps->events[ i & ( MAX_PS_EVENTS - 1 ) ] != ops->events[ i & ( MAX_PS_EVENTS - 1 ) ] ) ) {

            //clientEntity->current.event = event;
            //clientEntity->current.eventParm = ps->eventParms[ i & ( MAX_PS_EVENTS - 1 ) ];
            //CLG_EntityEvent( cent, cent->lerp_origin );

            // Get the event number.
            const int32_t playerStateEvent = ps->events[ i & ( MAX_PS_EVENTS - 1 ) ];
            // Proceed to firing the predicted/received event.
            SVG_FireClientPlayerStateEvent( ent, ops, ps, playerStateEvent );

            //// Add to the list of predictable events.
            //game.predictableEvents[ i & ( MAX_PREDICTED_EVENTS - 1 ) ] = event;
            //// Increment Event Sequence.
            //game.eventSequence++;
        }
    }
}
