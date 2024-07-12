/********************************************************************
*
*
*	ServerGame: Client Entity Animating Implementation
*
*
********************************************************************/
#include "svgame/svg_local.h"

/**
*   @brief  Returns a string stating the determined 'Base' animation, and sets the FrameTime value for frameTime pointer.
**/
const std::string SVG_P_DetermineClientBaseAnimation( const player_state_t *oldPlayerState, const player_state_t *playerState, double *frameTime ) {
    // Base animation string we'll end up seeking in SKM data.
    std::string baseAnimStr = "";
    // Weapon to append to animation string.
    std::string baseAnimWeaponStr = "_rifle";
    // Default frameTime to apply to animation.
    *frameTime = BASE_FRAMETIME;

    if ( playerState->animation.isIdle ) {
        baseAnimStr = "idle";
        if ( playerState->animation.isCrouched ) {
            baseAnimStr += "_crouch";
        }
    } else {
        if ( playerState->animation.isCrouched ) {
            baseAnimStr = "crouch";
            *frameTime -= 5.f;
        } else if ( playerState->animation.isWalking ) {
            baseAnimStr = "walk";
            *frameTime *= 0.5;
        } else {
            baseAnimStr = "run";
            *frameTime -= 5.f;
        }
    }

    // Directions:
    //if ( ( moveFlags & ANIM_MOVE_CROUCH ) || ( moveFlags & ANIM_MOVE_RUN ) || ( moveFlags & ANIM_MOVE_WALK ) ) {
    if ( !playerState->animation.isIdle ) {
        // Get move direction for animation.
        const int32_t animationMoveDirection = playerState->animation.moveDirection;

        // Append to the baseAnimStr the name of the directional animation.
        // Forward:
        if ( animationMoveDirection == PM_MOVEDIRECTION_FORWARD ) {
            baseAnimStr += "_forward";
        // Forward Left:
        } else if ( animationMoveDirection == PM_MOVEDIRECTION_FORWARD_LEFT ) {
            baseAnimStr += "_forward_left";
        // Forward Right:
        } else if ( animationMoveDirection == PM_MOVEDIRECTION_FORWARD_RIGHT ) {
            baseAnimStr += "_forward_right";
        // Backward:
        } else if ( animationMoveDirection == PM_MOVEDIRECTION_BACKWARD ) {
            baseAnimStr += "_backward";
        // Backward Left:
        } else if ( animationMoveDirection == PM_MOVEDIRECTION_BACKWARD_LEFT ) {
            baseAnimStr += "_backward_left";
        // Backward Right:
        } else if ( animationMoveDirection == PM_MOVEDIRECTION_BACKWARD_RIGHT ) {
            baseAnimStr += "_backward_right";
        // Left:
        } else if ( animationMoveDirection == PM_MOVEDIRECTION_LEFT ) {
            baseAnimStr += "_left";
        // Right:
        } else if ( animationMoveDirection == PM_MOVEDIRECTION_RIGHT ) {
            baseAnimStr += "_right";
        }
        #if 0
        // Only apply Z translation to the animations if NOT 'In-Air'.
        if ( !playerState->animation.inAir ) {
            // Jump?
            if ( oldPlayerState->animation.inAir ) {
                //jumpAnimStr = "jump_down";
            // Only translate Z axis for rootmotion rendering.
            } else {
                //rootMotionFlags |= SKM_POSE_TRANSLATE_Z;
            }
        } else {
            if ( !oldPlayerState->animation.inAir ) {
                //jumpAnimStr = "jump_up";
            } else {
                if ( !playerState->animation.isCrouched ) {
                    .//jumpAnimStr = "jump_air";
                }
            }
        }
        #endif
    // We're idling:
    } else {
        baseAnimStr = "idle";
        // Possible Crouched:
        if ( playerState->animation.isCrouched ) {
            baseAnimStr += "_crouch";
        }
    }

    // Add the weapon type stringname to the end.
    baseAnimStr += baseAnimWeaponStr;

    // Return.
    return baseAnimStr;
}

/**
*   @brief  Will process(progress) the entity's active animations for each body state and event states.
**/
void SVG_P_ProcessAnimations( edict_t *ent ) {
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

    // Determine the 'Base' animation for this client.
    double frameTime = BASE_FRAMETIME;
    const std::string baseAnimStr = SVG_P_DetermineClientBaseAnimation( &client->ps, &client->ops, &frameTime );

    /**
    *   Special change detection handling:
    **/
    // Default to level time for animation start.
    sg_time_t startTimer = level.time;
    // However, if the last body state was of a different animation type, we want to continue using its
    // start time so we can ensure that switching directions keeps the feet neatly lerping.
    if ( animationMixer->lastBodyStates[ SKM_BODY_LOWER ].animationID != animationMixer->currentBodyStates[ SKM_BODY_LOWER ].animationID ) {
        // Backup into lastBodyStates.
        animationMixer->lastBodyStates[ SKM_BODY_LOWER ] = animationMixer->currentBodyStates[ SKM_BODY_LOWER ];
        // Set the startTimer for the new 'Base' body state animation to that of the old.
        startTimer = animationMixer->lastBodyStates[ SKM_BODY_LOWER ].animationStartTime;
    }
    // Set lower 'Base' body animation.
    sg_skm_animation_state_t *lowerBodyState = &animationMixer->currentBodyStates[ SKM_BODY_LOWER ];
    // We want this to loop for most animations.
    SG_SKM_SetStateAnimation( model, lowerBodyState, baseAnimStr.c_str(), startTimer, frameTime, true, false );


    /**
    *   Process Animations.
    **/
    int32_t lowerBodyOldFrame = 0;
    int32_t lowerBodyCurrentFrame = 0;
    double lowerBodyBackLerp = 0.0;
    int32_t rootMotionBoneID = 0;
    int32_t rootMotionAxisFlags = SKM_POSE_TRANSLATE_Z;
    // Process lower body animation.
    SG_SKM_ProcessAnimationStateForTime( model, lowerBodyState, level.time, &lowerBodyOldFrame, &lowerBodyCurrentFrame, &lowerBodyBackLerp );
}