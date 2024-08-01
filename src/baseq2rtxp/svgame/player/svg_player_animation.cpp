/********************************************************************
*
*
*	ServerGame: Client Entity Animating Implementation
*
*
********************************************************************/
#include "svgame/svg_local.h"



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
    const std::string baseAnimStr = SG_Player_GetClientBaseAnimation( &client->ps, &client->ops, &frameTime );

    /**
    *   Special change detection handling:
    **/
    // Default to level time for animation start.
    // Temporary for setting the animation.
    sg_skm_animation_state_t newAnimationBodyState = animationMixer->currentBodyStates[ SKM_BODY_LOWER ];
    // We want this to loop for most animations.
    if ( SG_SKM_SetStateAnimation( model, &newAnimationBodyState, baseAnimStr.c_str(), level.time, frameTime, true, false ) ) {
        // However, if the last body state was of a different animation type, we want to continue using its
        // start time so we can ensure that switching directions keeps the feet neatly lerping.
        if ( animationMixer->currentBodyStates[ SKM_BODY_LOWER ].animationID != newAnimationBodyState.animationID ) {
            // Store the what once was current, as last body state.
            animationMixer->lastBodyStates[ SKM_BODY_LOWER ] = animationMixer->currentBodyStates[ SKM_BODY_LOWER ];
            // Assign the newly set animation state.
            //newAnimationBodyState.timeSTart = lastBodyState[ SKM_BODY_LOWER ].timeStart;
            animationMixer->currentBodyStates[ SKM_BODY_LOWER ] = newAnimationBodyState;
        }
    }
    //sg_time_t startTimer = level.time;
    //// However, if the last body state was of a different animation type, we want to continue using its
    //// start time so we can ensure that switching directions keeps the feet neatly lerping.
    //if ( animationMixer->lastBodyStates[ SKM_BODY_LOWER ].animationID != animationMixer->currentBodyStates[ SKM_BODY_LOWER ].animationID ) {
    //    // Set the startTimer for the new 'Base' body state animation to that of the old.
    //    startTimer = animationMixer->lastBodyStates[ SKM_BODY_LOWER ].timeStart;
    //    // Backup into lastBodyStates.
    //    animationMixer->lastBodyStates[ SKM_BODY_LOWER ] = animationMixer->currentBodyStates[ SKM_BODY_LOWER ];
    //}
    //// Set lower 'Base' body animation.
    sg_skm_animation_state_t *lowerBodyState = &animationMixer->currentBodyStates[ SKM_BODY_LOWER ];
    //// We want this to loop for most animations.
    //SG_SKM_SetStateAnimation( model, lowerBodyState, baseAnimStr.c_str(), startTimer, frameTime, true, false );


    /**
    *   Process Animations.
    **/
    //
    // Current Lower Body:
    //
    int32_t lowerBodyOldFrame = 0;
    int32_t lowerBodyCurrentFrame = 0;
    double lowerBodyBackLerp = 0.0;
    int32_t rootMotionBoneID = 0;
    int32_t rootMotionAxisFlags = SKM_POSE_TRANSLATE_Z;
    // Process lower body animation.
    SG_SKM_ProcessAnimationStateForTime( model, lowerBodyState, level.time, &lowerBodyOldFrame, &lowerBodyCurrentFrame, &lowerBodyBackLerp );
    //
    // Event Lower Body:
    //
    int32_t eventLowerBodyOldFrame = 0;
    int32_t eventLowerBodyCurrentFrame = 0;
    double eventLowerBodyBackLerp = 0.0;
    rootMotionAxisFlags = 0;
    // Process lower body animation.
    sg_skm_animation_state_t *eventLowerBodyState = &animationMixer->eventBodyState[ SKM_BODY_LOWER ];
    SG_SKM_ProcessAnimationStateForTime( model, eventLowerBodyState, level.time, &eventLowerBodyOldFrame, &eventLowerBodyCurrentFrame, &eventLowerBodyBackLerp );

    //
    // Event Upper Body:
    //
    int32_t eventUpperBodyOldFrame = 0;
    int32_t eventUpperBodyCurrentFrame = 0;
    double eventUpperBodyBackLerp = 0.0;
    // Process lower body animation.
    sg_skm_animation_state_t *eventUpperBodyState = &animationMixer->eventBodyState[ SKM_BODY_UPPER ];
    SG_SKM_ProcessAnimationStateForTime( model, eventUpperBodyState, level.time, &eventUpperBodyOldFrame, &eventUpperBodyCurrentFrame, &eventUpperBodyBackLerp );
}