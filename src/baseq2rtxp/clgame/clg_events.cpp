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
*   @brief  Determines the 'fire' animation to play for the given primary fire event.
**/
static const std::string CLG_PrimaryFireEvent_DetermineAnimation( const player_state_t *ops, const player_state_t *ps, const int32_t playerStateEvent, const Vector3 &lerpOrigin ) {
// We want this to work, but somehow moveDir is off.
#if 1
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
    // If walking:
    
    // When strafing just use fire_stand_pistol.
    //if ( isDucked ) {
    //    animationName = "fire_crouch_rifle";
    //} else {
    //    // Get move direction for animation.
    //    const int32_t animationMoveDirection = ps->animation.moveDirection;

    //    // Diagonal from left top to bottom right.
    //    if ( ( animationMoveDirection & PM_MOVEDIRECTION_FORWARD ) && ( animationMoveDirection & PM_MOVEDIRECTION_LEFT ) ) {
    //        animationName = "fire_run_diag_tl_rifle";
    //    } else if ( ( animationMoveDirection & PM_MOVEDIRECTION_BACKWARD ) && ( animationMoveDirection & PM_MOVEDIRECTION_RIGHT ) ) {
    //        animationName = "fire_run_diag_br_rifle";
    //    }
    //    //// Diagonal fire for: Backward Right, Forward Right
    //    //else if ( ( animationMoveDirection & PM_MOVEDIRECTION_FORWARD ) && ( animationMoveDirection & PM_MOVEDIRECTION_RIGHT ) ) {
    //    //    animationName = "fire_run_diagleft_rifle";
    //    //} else if ( ( animationMoveDirection & PM_MOVEDIRECTION_BACKWARD ) && ( animationMoveDirection & PM_MOVEDIRECTION_LEFT ) ) {
    //    //    animationName = "fire_run_diagleft_rifle";
    //    //}
    //}
    //clgi.Print( PRINT_NOTICE, "%s: %s\n", animationName.c_str() );
    return animationName;
#else
    // if it's moving to where is looking, it's moving forward
    // The desired yaw for the lower body.
    static constexpr float DIR_EPSILON = 0.3f;
    static constexpr float WALK_EPSILON = 5.0f;
    static constexpr float RUN_EPSILON = 100.f;

    // Angle Vectors.
    const Vector3 vForward = pml.forward;
    const Vector3 vRight = pml.right;

    // Get the move direction vector.
    Vector2 xyMoveDir = QM_Vector2FromVector3( ps->pmove.velocity );
    // Normalized move direction vector.
    Vector3 xyMoveDirNormalized = QM_Vector3FromVector2( QM_Vector2Normalize( xyMoveDir ) );

    // Dot products.
    const float xDotResult = QM_Vector3DotProduct( xyMoveDirNormalized, vRight );
    const float yDotResult = QM_Vector3DotProduct( xyMoveDirNormalized, vForward );

    // Are we even moving enough?
    if ( ps->xySpeed > WALK_EPSILON ) {
        // Forward:
        if ( ( yDotResult > DIR_EPSILON ) || ( pm->cmd.forwardmove > 0 ) ) {
            //ps->animation.moveDirection |= PM_MOVEDIRECTION_FORWARD;
            // Back:
        } else if ( ( -yDotResult > DIR_EPSILON ) || ( pm->cmd.forwardmove < 0 ) ) {
            //ps->animation.moveDirection |= PM_MOVEDIRECTION_BACKWARD;
        }
        // Right: (Only if the dotproduct is so, or specifically only side move is pressed.)
        if ( ( xDotResult > DIR_EPSILON ) || ( !pm->cmd.forwardmove && pm->cmd.sidemove > 0 ) ) {
            //ps->animation.moveDirection |= PM_MOVEDIRECTION_RIGHT;
            // Left: (Only if the dotproduct is so, or specifically only side move is pressed.)
        } else if ( ( -xDotResult > DIR_EPSILON ) || ( !pm->cmd.forwardmove && pm->cmd.sidemove < 0 ) ) {
            //ps->animation.moveDirection |= PM_MOVEDIRECTION_LEFT;
        }

        // Running:
        if ( pm->playerState->xySpeed > RUN_EPSILON ) {
            //ps->animation.moveDirection |= PM_MOVEDIRECTION_RUN;
            // Walking:
        } else if ( pm->playerState->xySpeed > WALK_EPSILON ) {
            //ps->animation.moveDirection |= PM_MOVEDIRECTION_WALK;
        }
    }
#endif
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
        SG_SKM_SetStateAnimation( model, &eventBodyState[ SKM_BODY_LOWER ], "jump_up_pistol", sg_time_t::from_ms( clgi.client->servertime ), BASE_FRAMETIME, false, true );
    // -- Jump Land:
    } else if ( playerStateEvent == PS_EV_JUMP_LAND ) {
        //clgi.Print( PRINT_NOTICE, "%s: PS_EV_JUMP_LAND\n", __func__ );
        // We really do NOT want this when we are crouched when having hit the ground.
        if ( !( ps->pmove.pm_flags & PMF_DUCKED ) ) {
            // Set event state animation.
            SG_SKM_SetStateAnimation( model, &eventBodyState[ SKM_BODY_LOWER ], "jump_land_pistol", sg_time_t::from_ms( clgi.client->servertime ), BASE_FRAMETIME, false, true );
        }
    // -- Weapon Fire Primary:
    } else if ( playerStateEvent == PS_EV_WEAPON_PRIMARY_FIRE ) {
        const std::string animationName = CLG_PrimaryFireEvent_DetermineAnimation( ops, ps, playerStateEvent, lerpOrigin );
        //clgi.Print( PRINT_NOTICE, "%s: PS_EV_WEAPON_PRIMARY_FIRE\n", __func__ );
        // Set event state animation.
        SG_SKM_SetStateAnimation( model, &eventBodyState[ SKM_BODY_UPPER ], animationName.c_str(), sg_time_t::from_ms( clgi.client->servertime ), BASE_FRAMETIME, false, true );
    // -- Weapon Reload:
    } else if ( playerStateEvent == PS_EV_WEAPON_RELOAD ) {
        //clgi.Print( PRINT_NOTICE, "%s: PS_EV_WEAPON_RELOAD\n", __func__ );
        // Set event state animation.
        SG_SKM_SetStateAnimation( model, &eventBodyState[ SKM_BODY_UPPER ], "reload_stand_pistol", sg_time_t::from_ms( clgi.client->servertime ), BASE_FRAMETIME, false, true );
    
    #if 1
    // -- Weapon Holster AND Draw:
    } else if ( playerStateEvent == PS_EV_WEAPON_HOLSTER_AND_DRAW ) {
        //clgi.Print( PRINT_NOTICE, "%s: PS_EV_WEAPON_RELOAD\n", __func__ );
        // Set event state animation.
        SG_SKM_SetStateAnimation( model, &eventBodyState[ SKM_BODY_UPPER ], "holster_draw_pistol", sg_time_t::from_ms( clgi.client->servertime ), BASE_FRAMETIME, false, true );
    }
    #else
    // -- Weapon Draw:
    } else if ( playerStateEvent == PS_EV_WEAPON_HOLSTER ) {
        //clgi.Print( PRINT_NOTICE, "%s: PS_EV_WEAPON_RELOAD\n", __func__ );
        // Set event state animation.
        SG_SKM_SetStateAnimation( model, &eventBodyState[ SKM_BODY_UPPER ], "holster_pistol", sg_time_t::from_ms( clgi.client->servertime ), BASE_FRAMETIME, false, true );
    // -- Weapon Holster:
    } else if ( playerStateEvent == PS_EV_WEAPON_DRAW ) {
        //clgi.Print( PRINT_NOTICE, "%s: PS_EV_WEAPON_RELOAD\n", __func__ );
        // Set event state animation.
        SG_SKM_SetStateAnimation( model, &eventBodyState[ SKM_BODY_UPPER ], "draw_pistol", sg_time_t::from_ms( clgi.client->servertime ), BASE_FRAMETIME, false, true );
    }
    #endif
}