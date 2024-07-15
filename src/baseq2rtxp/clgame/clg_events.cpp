/********************************************************************
*
*
*	ClientGame: (Entity/Player State) -Events:
*
*
********************************************************************/
#include "clg_local.h"
#include "clg_events.h"



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
    if ( playerStateEvent == PS_EV_JUMP_UP ) {
        //clgi.Print( PRINT_NOTICE, "%s: PS_EV_JUMP_UP\n", __func__ );
        // Set event state animation.
        SG_SKM_SetStateAnimation( model, &eventBodyState[ SKM_BODY_LOWER ], "jump_up_rifle", sg_time_t::from_ms(clgi.client->servertime), BASE_FRAMETIME, false, true);
    } else if ( playerStateEvent == PS_EV_JUMP_LAND ) {
        //clgi.Print( PRINT_NOTICE, "%s: PS_EV_JUMP_LAND\n", __func__ );
        // We really do NOT want this when we are crouched when having hit the ground.
        if ( !( ps->pmove.pm_flags & PMF_DUCKED ) ) {
            // Set event state animation.
            SG_SKM_SetStateAnimation( model, &eventBodyState[ SKM_BODY_LOWER ], "jump_land_rifle", sg_time_t::from_ms( clgi.client->servertime ), BASE_FRAMETIME, false, true );
        }
    } else if ( playerStateEvent == PS_EV_WEAPON_PRIMARY_FIRE ) {
        //clgi.Print( PRINT_NOTICE, "%s: PS_EV_WEAPON_PRIMARY_FIRE\n", __func__ );
        // Set event state animation.
        SG_SKM_SetStateAnimation( model, &eventBodyState[ SKM_BODY_UPPER ], "fire_stand_rifle", sg_time_t::from_ms( clgi.client->servertime ), BASE_FRAMETIME, false, true );
    } else if ( playerStateEvent == PS_EV_WEAPON_RELOAD ) {
        //clgi.Print( PRINT_NOTICE, "%s: PS_EV_WEAPON_RELOAD\n", __func__ );
        // Set event state animation.
        SG_SKM_SetStateAnimation( model, &eventBodyState[ SKM_BODY_UPPER ], "reload_stand_rifle", sg_time_t::from_ms( clgi.client->servertime ), BASE_FRAMETIME, false, true );
    }
}