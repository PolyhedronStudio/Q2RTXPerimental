/********************************************************************
*
*
*	ClientGame: 'ET_PLAYER' Packet Entities.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_effects.h"
#include "clgame/clg_entities.h"
#include "clgame/clg_temp_entities.h"



/**
*
*
*   Skeletal Model Stuff:
*
*
**/
/**
*   @brief  Will only be called once whenever the add player entity method encounters an empty bone pose.
**/
void CLG_ETPlayer_AllocatePoseCache( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *newState ) {
    if ( packetEntity->bonePoseCache == nullptr ) {
        // Determine whether the entity now has a skeletal model, and if so, allocate a bone pose cache for it.
        if ( const model_t *entModel = clgi.R_GetModelDataForHandle( clgi.client->model_draw[ newState->modelindex ] ) ) {
            // Make sure it has proper SKM data.
            if ( ( entModel->skmData ) && ( entModel->skmConfig ) ) {
                // Allocate bone pose space. ( Use SKM_MAX_BONES instead of entModel->skmConfig->numberOfBones because client models could change. )
                packetEntity->bonePoseCache = clgi.SKM_PoseCache_AcquireCachedMemoryBlock( SKM_MAX_BONES /*entModel->skmConfig->numberOfBones*/ );
            }
        }
    }
}

/**
*   @brief  Determine the entity's 'Base' animations.
**/
void CLG_ETPlayer_DetermineBaseAnimations( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *newState ) {
    // Get model resource.
    const model_t *model = clgi.R_GetModelDataForHandle( refreshEntity->model );
    
    // Get the animation state mixer.
    sg_skm_animation_mixer_t *animationMixer = &packetEntity->animationMixer;
    // Get body states.
    sg_skm_animation_state_t *currentBodyState = animationMixer->currentBodyStates;
    sg_skm_animation_state_t *lastBodyState = animationMixer->lastBodyStates;

    // Third-person/mirrors model of our own client entity:
    if ( CLG_IsViewClientEntity( newState ) ) {
        // Determine 'Base' animation name.
        double frameTime = 1.f;
        const std::string baseAnimStr = SG_Player_GetClientBaseAnimation( &clgi.client->predictedState.lastPs, &clgi.client->predictedState.currentPs, &frameTime );

        // Start timer is always just servertime that we had.
        sg_time_t startTimer = sg_time_t::from_ms( clgi.client->servertime );
        // However, if the last body state was of a different animation type, we want to continue using its
        // start time so we can ensure that switching directions keeps the feet neatly lerping.
        if ( lastBodyState[ SKM_BODY_LOWER ].animationID != currentBodyState[ SKM_BODY_LOWER ].animationID ) {
            //startTimer = lastBodyState[ SKM_BODY_LOWER ].timeStart;
            lastBodyState[ SKM_BODY_LOWER ] = currentBodyState[ SKM_BODY_LOWER ];
        }
        // We want this to loop for most animations.
        SG_SKM_SetStateAnimation( model, &currentBodyState[ SKM_BODY_LOWER ], baseAnimStr.c_str(), startTimer, frameTime, true, false );
    } else {
        // Decode the entity's animationIDs from its newState.
        uint8_t lowerAnimationID = 0;
        uint8_t torsoAnimationID = 0;
        uint8_t headAnimationID = 0;
        uint8_t animationFrameRate = BASE_FRAMERATE; // NOTE: Also set by SG_DecodeAnimationState :-)
        // Decode it.
        SG_DecodeAnimationState( newState->frame, &lowerAnimationID, &torsoAnimationID, &headAnimationID, &animationFrameRate );
        // Start timer is always just servertime that we had.
        sg_time_t startTimer = sg_time_t::from_ms( clgi.client->servertime );
        // However, if the last body state was of a different animation type, we want to continue using its
        // start time so we can ensure that switching directions keeps the feet neatly lerping.
        if ( lastBodyState[ SKM_BODY_LOWER ].animationID != currentBodyState[ SKM_BODY_LOWER ].animationID ) {
            //startTimer = lastBodyState[ SKM_BODY_LOWER ].timeStart;
            lastBodyState[ SKM_BODY_LOWER ] = currentBodyState[ SKM_BODY_LOWER ];
        }
        // Calculate frameTime based on frameRate.
        double frameTime = 1000.0 / animationFrameRate;
        // Set animation.
        SG_SKM_SetStateAnimation( model, &currentBodyState[ SKM_BODY_LOWER ], lowerAnimationID, startTimer, frameTime, true, false );
    }
    

    #if 0
    // If we got an entity event...
    //if ( entityEvent == PS_EV_JUMP_UP ) {
    //    // Set event animation.
    //    sg_skm_animation_state_t *eventAnimationState = &animationMixer->eventBodyState[ SKM_BODY_LOWER ];// &animationMixer->eventBodyState[ 0 ];
    //    // Set lower body animation.
    //    SG_SKM_SetStateAnimation( model, eventAnimationState, "jump_up", sg_time_t::from_ms(clgi.client->servertime), BASE_FRAMETIME, false, true );
    //} else if ( entityEvent == PS_EV_JUMP_LAND ) {
    //    // Set event animation.
    //    sg_skm_animation_state_t *eventAnimationState = &animationMixer->eventBodyState[ SKM_BODY_LOWER ];// &animationMixer->currentBodyStates[ 0 ];
    //    // Set lower body animation.
    //    SG_SKM_SetStateAnimation( model, eventAnimationState, "jump_down", sg_time_t::from_ms( clgi.client->servertime ), BASE_FRAMETIME, false, true );
    //} else {
        player_state_t *predictedPlayerState = &clgi.client->predictedState.currentPs;

        if ( !predictedPlayerState->animation.isCrouched && predictedPlayerState->animation.inAir ) {
        //    // Set event animation.
            sg_skm_animation_state_t *eventAnimationState = &animationMixer->currentBodyStates[ SKM_BODY_LOWER ];//&animationMixer->currentBodyStates[ 0 ];
            // Wait for other event to finish.
            //if ( eventAnimationState->ler <= sg_time_t::from_ms( clgi.client->extrapolatedTime ) ) {
                // Set lower body animation.
                SG_SKM_SetStateAnimation( model, eventAnimationState, "jump_air", sg_time_t::from_ms( clgi.client->servertime ), BASE_FRAMETIME, true, true );
            //}
        }
#endif
    //}
}

/**
*   @brief  Determine which animations to play based on the player state event channels.
**/
void CLG_ETPlayer_DetermineEventAnimations( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *newState ) {
    //// Get model resource.
    //const model_t *model = clgi.R_GetModelDataForHandle( refreshEntity->model );

    //// Get the animation state mixer.
    //sg_skm_animation_mixer_t *animationMixer = &packetEntity->animationMixer;
    //// Get body states.
    //sg_skm_animation_state_t *currentBodyState = animationMixer->currentBodyStates;
    //sg_skm_animation_state_t *lastBodyState = animationMixer->lastBodyStates;

    //sg_skm_animation_state_t *lowerEventBodyState = &animationMixer->eventBodyState[ SKM_BODY_LOWER ];
    //sg_skm_animation_state_t *upperEventBodyState = &animationMixer->eventBodyState[ SKM_BODY_UPPER ];


    //// Third-person/mirrors model of our own client entity:
    //if ( CLG_IsViewClientEntity( newState ) ) {
    //    // WID: TODO: We need client-side weapon code if we want to use the predicted states,
    //    player_state_t *playerState = &clgi.client->predictedState.lastPs; // &clgi.client->frame.ps;//
    //    player_state_t *oldPlayerState = &clgi.client->predictedState.currentPs; // &clgi.client->oldframe.ps;//;
    //    if ( oldPlayerState->eventSequence != playerState->eventSequence ) {
    //        const int32_t currentSequenceIndex = playerState->eventSequence & ( MAX_PS_EVENTS - 1 );
    //        const int32_t oldSequenceIndex = oldPlayerState->eventSequence & ( MAX_PS_EVENTS - 1 );
    //        const int32_t entityEvent = playerState->events[ currentSequenceIndex ];
    //        const int32_t oldEntityEvent = oldPlayerState->events[ oldSequenceIndex ];

    //        // Set event animation.
    //        const sg_time_t extrapolatedTime = sg_time_t::from_ms( clgi.client->extrapolatedTime );
    //        //if ( extrapolatedTime > eventAnimationState->animationEndTime ) {
    //        //if ( entityEvent != oldEntityEvent ) {
    //        if ( entityEvent == PS_EV_JUMP_UP ) {
    //            //sg_skm_animation_state_t *currentLowerBodyAnimationState = &animationMixer->currentBodyStates[ SKM_BODY_LOWER ];
    //            //sg_skm_animation_state_t *lastLowerBodyAnimationState = &animationMixer->lastBodyStates[ SKM_BODY_LOWER ];
    //            //*lastLowerBodyAnimationState = *currentLowerBodyAnimationState;
    //            //SG_SKM_SetStateAnimation( model, currentLowerBodyAnimationState, "jump_up", sg_time_t::from_ms( clgi.client->servertime ), BASE_FRAMETIME, false, true );
    //        } else if ( entityEvent == PS_EV_JUMP_LAND ) {
    //            //sg_skm_animation_state_t *currentLowerBodyAnimationState = &animationMixer->currentBodyStates[ SKM_BODY_LOWER ];
    //            //sg_skm_animation_state_t *lastLowerBodyAnimationState = &animationMixer->lastBodyStates[ SKM_BODY_LOWER ];
    //            //*lastLowerBodyAnimationState = *currentLowerBodyAnimationState;
    //            //SG_SKM_SetStateAnimation( model, currentLowerBodyAnimationState, "jump_down", sg_time_t::from_ms( clgi.client->servertime ), BASE_FRAMETIME, false, true );
    //        } else if ( entityEvent == PS_EV_WEAPON_PRIMARY_FIRE ) {
    //            // Set event state animation.
    //            SG_SKM_SetStateAnimation( model, lowerEventBodyState, "fire_stand_rifle", sg_time_t::from_ms( clgi.client->servertime ), BASE_FRAMETIME, false, true );
    //        } else if ( entityEvent == PS_EV_WEAPON_RELOAD ) {
    //            // Set event state animation.
    //            SG_SKM_SetStateAnimation( model, lowerEventBodyState, "reload_stand_rifle", sg_time_t::from_ms( clgi.client->servertime ), BASE_FRAMETIME, false, true );
    //        }
    //        //}
    //        //}
    //        //
    //        //  if ( entityEvent == PS_EV_JUMP_UP ) {
    //        //      // &animationMixer->eventBodyState[ 0 ];
    //        //                      // Set lower body animation.
    //        //      SG_SKM_SetStateAnimation( model, eventAnimationState, "jump_up", sg_time_t::from_ms( clgi.client->servertime ), BASE_FRAMETIME, false, false );
    //        //  } else if ( entityEvent == PS_EV_JUMP_LAND ) {
    //        //      // Set event animation.
    //        //      sg_skm_animation_state_t *eventAnimationState = &animationMixer->eventBodyState[ SKM_BODY_LOWER ];// &animationMixer->currentBodyStates[ 0 ];
    //        //      // Set lower body animation.
    //        //      SG_SKM_SetStateAnimation( model, eventAnimationState, "jump_down", sg_time_t::from_ms( clgi.client->servertime ), BASE_FRAMETIME, false, false );
    //        //  }
    //        //}
    //        // 
    //            //predictedPlayerState->events[ sequenceIndex ] = EV_NONE;
    //    }
    //} else {
    //    //entityEvent = packetEntity->current.event;
    //}
}

static const double CLG_ETPlayer_GetSwitchAnimationScaleFactor( const sg_time_t &lastTimeStamp, const sg_time_t &nextTimeStamp, const sg_time_t &animationTime ) {
    //double scaleFactor = 0.0;
    const double midWayLength = animationTime.milliseconds() - lastTimeStamp.milliseconds();
    const double framesDiff = nextTimeStamp.milliseconds() - lastTimeStamp.milliseconds();
    // WID: Prevent a possible division by zero?
    if ( framesDiff > 0 ) {
        return /*scaleFactor = */ midWayLength / framesDiff;
    } else {
        return DBL_EPSILON;//0;
    }
}

/**
*   @brief  Process the entity's active animations.
**/
void CLG_ETPlayer_ProcessAnimations( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *newState ) {
    // Get model resource.
    const model_t *model = clgi.R_GetModelDataForHandle( refreshEntity->model );

    // Ensure it is valid.
    if ( !model ) {
        clgi.Print( PRINT_WARNING, "%s: Invalid model handle(#%i) for entity(#%i)\n", __func__, refreshEntity->model, packetEntity->current.number );
        return;
    }
    // Ensure it has SKM data.
    const skm_model_t *skmData = model->skmData;
    if ( !skmData ) {
        clgi.Print( PRINT_WARNING, "%s: No SKM data for model handle(#%i) for entity(#%i)\n", __func__, refreshEntity->model, packetEntity->current.number );
        return;
    }
    // Ensure it has SKM config.
    const skm_config_t *skmConfig = model->skmConfig;
    if ( !skmConfig ) {
        clgi.Print( PRINT_WARNING, "%s: No SKM config for model handle(#%i) for entity(#%i)\n", __func__, refreshEntity->model, packetEntity->current.number );
        return;
    }
    // Get the animation state mixer.
    sg_skm_animation_mixer_t *animationMixer = &packetEntity->animationMixer;
    if ( !animationMixer ) {
        clgi.Print( PRINT_WARNING, "%s: packetEntity(#%i)->animationMixer == (nullptr)\n", __func__, packetEntity->current.number );
        return;
    }

    /**
    *   Lower Body:
    **/
    // Time we're at.
    const sg_time_t extrapolatedTime = sg_time_t::from_ms( clgi.client->extrapolatedTime );

    // The model's hip bone.
    skm_bone_node_t *hipsBone = clgi.SKM_GetBoneByName( model, "mixamorig8:Hips" );
    // Spine bones, for upper body event animations.
    skm_bone_node_t *spineBone = clgi.SKM_GetBoneByName( model, "mixamorig8:Spine" );
    skm_bone_node_t *spine1Bone = clgi.SKM_GetBoneByName( model, "mixamorig8:Spine1" );
    skm_bone_node_t *spine2Bone = clgi.SKM_GetBoneByName( model, "mixamorig8:Spine2" );
    // Shoulder bones.
    skm_bone_node_t *leftShoulderBone = clgi.SKM_GetBoneByName( model, "mixamorig8:LeftShoulder" );
    skm_bone_node_t *rightShoulderBone = clgi.SKM_GetBoneByName( model, "mixamorig8:RightShoulder" );
    // Leg bones.
    skm_bone_node_t *leftUpLegBone = clgi.SKM_GetBoneByName( model, "mixamorig8:LeftUpLeg" );
    skm_bone_node_t *rightUpLegBone = clgi.SKM_GetBoneByName( model, "mixamorig8:RightUpLeg" );

    // Stores data for current and last frame poses.
    static skm_transform_t finalStatePose[ SKM_MAX_BONES ];
    static skm_transform_t lastFinalStatePose[ SKM_MAX_BONES ];
    memcpy( lastFinalStatePose, finalStatePose, SKM_MAX_BONES * sizeof( skm_transform_t ) );
    static skm_transform_t currentStatePose[ SKM_MAX_BONES ];
    static skm_transform_t lastCurrentStatePose[ SKM_MAX_BONES ];
    memcpy( lastCurrentStatePose, currentStatePose, SKM_MAX_BONES * sizeof( skm_transform_t ) );
    static skm_transform_t lastStatePose[ SKM_MAX_BONES ];
    static skm_transform_t lastLastStatePose[ SKM_MAX_BONES ];
    memcpy( lastLastStatePose, lastStatePose, SKM_MAX_BONES * sizeof( skm_transform_t ) );

    // Default rootmotion settings.
    int32_t rootMotionBoneID = 0, rootMotionAxisFlags = 0;

    //
    // Lower Body State
    //
    int32_t currentLowerBodyCurrentFrame = 0, currentLowerBodyOldFrame = 0;
    int32_t lastLowerBodyCurrentFrame = 0, lastLowerBodyOldFrame = 0;
    double currentLowerBodyBackLerp = 0.0, lastLowerBodyBackLerp;
    
    sg_skm_animation_state_t *currentLowerBodyState = &animationMixer->currentBodyStates[ SKM_BODY_LOWER ];
    sg_skm_animation_state_t *lastLowerBodyState = &animationMixer->lastBodyStates[ SKM_BODY_LOWER ];
    // Process the 'last' lower body animation. (This is so we can smoothly transition from 'last' to 'current').
    { 
        // Get frame.
        SG_SKM_ProcessAnimationStateForTime( model, lastLowerBodyState, extrapolatedTime, &lastLowerBodyOldFrame, &lastLowerBodyCurrentFrame, &lastLowerBodyBackLerp );
        // Lerp the relative frame poses.
        const skm_transform_t *framePose = clgi.SKM_GetBonePosesForFrame( model, lastLowerBodyCurrentFrame );
        const skm_transform_t *oldFramePose = clgi.SKM_GetBonePosesForFrame( model, lastLowerBodyOldFrame );
        clgi.SKM_LerpBonePoses(
            model,
            framePose, oldFramePose,
            1.0 - lastLowerBodyBackLerp, lastLowerBodyBackLerp,
            lastStatePose,
            rootMotionBoneID, rootMotionAxisFlags
        );
    }
    // Process the 'current' lower body animation.
    {
        // Get frame.
        SG_SKM_ProcessAnimationStateForTime( model, currentLowerBodyState, extrapolatedTime, &currentLowerBodyOldFrame, &currentLowerBodyCurrentFrame, &currentLowerBodyBackLerp );
        const skm_transform_t *framePose = clgi.SKM_GetBonePosesForFrame( model, currentLowerBodyCurrentFrame );
        const skm_transform_t *oldFramePose = clgi.SKM_GetBonePosesForFrame( model, currentLowerBodyOldFrame );
        // Lerp the relative frame poses.
        clgi.SKM_LerpBonePoses(
            model,
            framePose, oldFramePose,
            1.0 - currentLowerBodyBackLerp, currentLowerBodyBackLerp,
            currentStatePose,
            rootMotionBoneID, rootMotionAxisFlags
        );
    }

    //
    // Event: Lower Body State
    //
    int32_t lowerEventBodyOldFrame = 0;
    int32_t lowerEventBodyCurrentFrame = 0;
    double lowerEventBodyBackLerp = 0.0;
    // Process lower event animation.
    sg_skm_animation_state_t *lowerEventBodyState = &animationMixer->eventBodyState[ SKM_BODY_LOWER ];
    const bool lowerEventFinishedPlaying = SG_SKM_ProcessAnimationStateForTime( model, lowerEventBodyState, extrapolatedTime, &lowerEventBodyOldFrame, &lowerEventBodyCurrentFrame, &lowerEventBodyBackLerp );
    // Lerp lower event state poses and blend into currentStatePose.
    static skm_transform_t lowerEventStatePose[ SKM_MAX_BONES ];
    if ( lowerEventBodyState->timeEnd >= extrapolatedTime ) {
        // Acquire frame poses.
        const skm_transform_t *framePose = clgi.SKM_GetBonePosesForFrame( model, lowerEventBodyCurrentFrame );
        const skm_transform_t *oldFramePose = clgi.SKM_GetBonePosesForFrame( model, lowerEventBodyOldFrame );
        // Lerp bone poses.
        clgi.SKM_LerpBonePoses(
            model,
            framePose, oldFramePose,
            1.0 - lowerEventBodyBackLerp, lowerEventBodyBackLerp,
            lowerEventStatePose,
            rootMotionBoneID, rootMotionAxisFlags
        );
        // Perform the blend.
        clgi.SKM_RecursiveBlendFromBone( lowerEventStatePose, currentStatePose, hipsBone, nullptr, 0, lowerEventBodyBackLerp, 1 );
        clgi.SKM_RecursiveBlendFromBone( lowerEventStatePose, lastStatePose, hipsBone, nullptr, 0, lowerEventBodyBackLerp, 1 );
    }

    ////
    //// Event: Upper Body State
    ////
    int32_t upperEventBodyOldFrame = 0;
    int32_t upperEventBodyCurrentFrame = 0;
    double upperEventBodyBackLerp = 0.0;
    // Process upper event animation.
    sg_skm_animation_state_t *upperEventBodyState = &animationMixer->eventBodyState[ SKM_BODY_UPPER ];
    const bool upperEventFinishedPlaying = SG_SKM_ProcessAnimationStateForTime( model, upperEventBodyState, extrapolatedTime, &upperEventBodyOldFrame, &upperEventBodyCurrentFrame, &upperEventBodyBackLerp );
    // Lerp upper event state poses and blend into currentStatePose.
    static skm_transform_t upperEventStatePose[ SKM_MAX_BONES ];
    if ( upperEventBodyState->timeEnd >= extrapolatedTime ) {
        // Bones we want to exclude the upper animation overriding.
        const skm_bone_node_t *excludeNodes[] = {
            hipsBone,
            leftUpLegBone,
            rightUpLegBone,
        };
        const int32_t numExcludeNodes = 3;
               
        // Acquire frame poses.
        const skm_transform_t *framePose = clgi.SKM_GetBonePosesForFrame( model, upperEventBodyCurrentFrame );
        const skm_transform_t *oldFramePose = clgi.SKM_GetBonePosesForFrame( model, upperEventBodyOldFrame );
        // Lerp bone poses.
        clgi.SKM_LerpBonePoses(
            model,
            framePose, oldFramePose,
            1.0 - upperEventBodyBackLerp, upperEventBodyBackLerp,
            upperEventStatePose,
            0, 0
        );
        clgi.SKM_RecursiveBlendFromBone( upperEventStatePose, currentStatePose, spineBone, nullptr, 0, upperEventBodyBackLerp, 1 );
        clgi.SKM_RecursiveBlendFromBone( upperEventStatePose, lastStatePose, spineBone, nullptr, 0, upperEventBodyBackLerp, 1 );
    }

    #if 0
    Vector3 thirdpersonAngles = clgi.client->playerEntityAngles;
    Vector3 spineAngles = thirdpersonAngles;
    // Adjust pitch slightly.
    spineAngles[ YAW ] = -AngleMod( spineAngles[ YAW ] ) / 3;

    //hipZUpOrientQuat = QM_QuaternionMultiply( hipQuatZRotate, hipZUpOrientQuat );
    Quaternion quatZUpOrientation = QM_QuaternionIdentity();//QM_QuaternionZUpOrientation();
    // Get a quaternion of the players pitch angle.
    Quaternion spineQuatPitchRotate = QM_QuaternionFromAxisAngle( Vector3( 1, 0, 0 ), DEG2RAD( spineAngles[ PITCH ] ) );
    // Get a quaternion of the players yaw angle.
    Quaternion spineQuatYawRotate = QM_QuaternionFromAxisAngle( Vector3( 0, 1, 0 ), DEG2RAD( spineAngles[ YAW ] ) );
    // Apply the pitch rotation to the ZUp orientation to get the hipQuaternion.
    Quaternion spineQuat = QM_QuaternionMultiply( quatZUpOrientation, spineQuatYawRotate );
    // Apply the pitch rotation to the ZUp orientation to get the hipQuaternion.
    spineQuat = QM_QuaternionMultiply( spineQuat, spineQuatPitchRotate );

    // Assign.
    QuatCopy( spineQuat, currentStatePose[ spineBone->number ].rotate );
    QuatCopy( spineQuat, lastStatePose[ spineBone->number ].rotate );
    #else
    
        player_state_t *predictedState = &clgi.client->predictedState.currentPs;
        player_state_t *lastPredictedState = &clgi.client->predictedState.lastPs;

        // Final predicted player state entity angles.
        Vector3 thirdpersonAngles = clgi.client->playerEntityAngles;
        // Get the move direction vector.
        Vector2 xyMoveDir = QM_Vector2FromVector3( clgi.client->predictedState.currentPs.pmove.velocity );
        // Normalized move direction vector.
        Vector3 xyMoveDirNormalized = QM_Vector3FromVector2( QM_Vector2Normalize( xyMoveDir ) );      



        //
        // Calculate hip/Spine rotations.
        //
        Vector3 hipRotateEuler = QM_QuaternionToEuler( currentStatePose[ hipsBone->number ].rotate );

        // Calculate and 'turn to' desired yaw.
        static float initialBaseHipsYaw = hipRotateEuler.z;
        if ( predictedState->animation.moveDirection != lastPredictedState->animation.moveDirection ) {
            initialBaseHipsYaw = hipRotateEuler.z;
        }
        static float hipsDesiredYaw = initialBaseHipsYaw;
        static float hipsYaw = hipRotateEuler.z; // Z == Pitch for this.
        static float hipsAddStep = 1.8 * clgi.client->lerpfrac;

        Vector3 vRight;
        Vector3 vForward;
        QM_AngleVectors( predictedState->viewangles, &vForward, &vRight, nullptr );
        const float xDotResult = QM_Vector3DotProduct( xyMoveDirNormalized, vRight );
        const float yDotResult = QM_Vector3DotProduct( xyMoveDirNormalized, vForward );

        hipsAddStep = 12.5 * /*( 1.0 - xDotResult ) **/ ( clgi.client->xerpFraction );
        //hipsAddStep = ( xDotResult * (1.0 / 12.5 ) ) * ( 1.0 - clgi.client->xerpFraction );

        // Move direction animation flags.
        const int32_t moveDirectionFlags = predictedState->animation.moveDirection;
        if ( moveDirectionFlags & PM_MOVEDIRECTION_FORWARD ) {
            if ( moveDirectionFlags & PM_MOVEDIRECTION_LEFT ) {
                //hipsDesiredYaw -= 0.3 * BASE_FRAMETIME_1000;
                hipsDesiredYaw = initialBaseHipsYaw + 45;
                //hipsAdd = -0.3f;
            } else if ( moveDirectionFlags & PM_MOVEDIRECTION_RIGHT ) {
                //hipsDesiredYaw += 0.3 * BASE_FRAMETIME_1000;
                hipsDesiredYaw = initialBaseHipsYaw + -45;
                //hipsAdd = 0.3f;
            } else {
                hipsDesiredYaw = initialBaseHipsYaw;
            }
        } else if ( moveDirectionFlags & PM_MOVEDIRECTION_BACKWARD ) {
            if ( moveDirectionFlags & PM_MOVEDIRECTION_LEFT ) {
                //hipsDesiredYaw += 0.3 * BASE_FRAMETIME_1000;
                hipsDesiredYaw = initialBaseHipsYaw + -45;
                //hipsAdd = 0.3f;
            } else if ( moveDirectionFlags & PM_MOVEDIRECTION_RIGHT ) {
                //hipsDesiredYaw -= 0.3 * BASE_FRAMETIME_1000;
                hipsDesiredYaw = initialBaseHipsYaw + 45;
                //hipsAdd = -0.3f;
            } else {
                hipsDesiredYaw = initialBaseHipsYaw;
            }
        } else {
            if ( moveDirectionFlags & PM_MOVEDIRECTION_LEFT ) {
                //hipsDesiredYaw -= 0.3 * BASE_FRAMETIME_1000;
                hipsDesiredYaw = initialBaseHipsYaw;
                //hipsAdd = -0.3f;
            } else if ( moveDirectionFlags & PM_MOVEDIRECTION_RIGHT ) {
                //hipsDesiredYaw += 0.3 * BASE_FRAMETIME_1000;
                hipsDesiredYaw = initialBaseHipsYaw;
                //hipsAdd = 0.3f;
            } else {
                hipsDesiredYaw = initialBaseHipsYaw;
            }
        }

        // Subtract/Add depending on desire.
        if ( hipsYaw < hipsDesiredYaw ) {
            hipsYaw += hipsAddStep;
        } else if ( hipsYaw > hipsDesiredYaw ) {
            hipsYaw += -hipsAddStep;
        }

        //// Keep within bounds.
        //if ( hipsYaw > hipsDesiredYaw ) {
        //    hipsYaw = hipsDesiredYaw;
        //} else if ( hipsYaw > hipsDesiredYaw ) {
        //    hipsYaw = hipsDesiredYaw;
        //}

        // We need that annooooying 90 somehow.
        //hipsYaw = AngleMod( hipsYaw );


        #if 1
        //
        // Hips(Yaw) Rotation:
        // 
        //hipZUpOrientQuat = QM_QuaternionMultiply( hipQuatZRotate, hipZUpOrientQuat );
        // We use the Z Up Orientation when dealing with the hip bone.
        // ( It basically puts a rotation that defaults into quake's X/Z/Y system. )
        Quaternion quatZUpOrientation = QM_QuaternionZUpOrientation();
        // Get a quaternion of the players pitch angle.
        //Quaternion hipsQuatPitchRotate = QM_QuaternionFromAxisAngle( Vector3( 1, 0, 0 ), DEG2RAD( spineAngles[ PITCH ] ) );
        // Get a quaternion of the players yaw angle.
        Quaternion hipsQuatYawRotate = QM_QuaternionFromAxisAngle( Vector3( 0, 1, 0 ), DEG2RAD( hipsYaw ) );
        // Apply the pitch rotation to the ZUp orientation to get the hipQuaternion.
        //Quaternion hipsQuat = QM_QuaternionMultiply( quatZUpOrientation, hipsQuatYawRotate );
        // Apply the pitch rotation to the ZUp orientation to get the hipQuaternion.
        //hipsQuat = QM_QuaternionMultiply( hipsQuat, hipsQuatPitchRotate );
        Quaternion hipsQuat = QM_QuaternionMultiply( quatZUpOrientation, hipsQuatYawRotate );
        // Assign.
        QuatCopy( hipsQuat, currentStatePose[ hipsBone->number ].rotate );
        QuatCopy( hipsQuat, lastStatePose[ hipsBone->number ].rotate );
        #endif
        #if 0
        //
        // Legs(Yaw) Rotation:
        // 
        //hipZUpOrientQuat = QM_QuaternionMultiply( hipQuatZRotate, hipZUpOrientQuat );
        // We use the Z Up Orientation when dealing with the hip bone.
        // ( It basically puts a rotation that defaults into quake's X/Z/Y system. )
        Quaternion quatLeftLeg = currentStatePose[ leftUpLegBone->number ].rotate;// QM_QuaternionFromAxisAngle( Vector3( 1, 0, 0 ), DEG2RAD( -90 ) );
        Quaternion quatRightLeg = currentStatePose[ rightUpLegBone->number ].rotate;
        // Get a quaternion of the players pitch angle.
        //Quaternion hipsQuatPitchRotate = QM_QuaternionFromAxisAngle( Vector3( 1, 0, 0 ), DEG2RAD( spineAngles[ PITCH ] ) );
        // Get a quaternion of the players yaw angle.
        Quaternion legsQuatYawRotate = QM_QuaternionFromAxisAngle( Vector3( 0, 1, 0 ), DEG2RAD( hipsYaw ) );
        // Apply the pitch rotation to the ZUp orientation to get the hipQuaternion.
        //Quaternion hipsQuat = QM_QuaternionMultiply( quatZUpOrientation, hipsQuatYawRotate );
        // Apply the pitch rotation to the ZUp orientation to get the hipQuaternion.
        //hipsQuat = QM_QuaternionMultiply( hipsQuat, hipsQuatPitchRotate );
        float slerpFactor = ( clgi.client->xerpFraction ) * BASE_1_FRAMETIME;

        Quaternion legsLeftQuat = QM_QuaternionSlerp( quatLeftLeg, legsQuatYawRotate, slerpFactor );
        Quaternion legsRightQuat = QM_QuaternionSlerp( quatRightLeg, legsQuatYawRotate, slerpFactor );
        //Quaternion legsLeftQuat = QM_QuaternionMultiply( quatLeftLeg, legsQuatYawRotate );
        //Quaternion legsRightQuat = QM_QuaternionMultiply( quatRightLeg, legsQuatYawRotate );

        // Assign.
        //QuatCopy( hipsQuat, currentStatePose[ hipsBone->number ].rotate );
        //QuatCopy( hipsQuat, lastStatePose[ hipsBone->number ].rotate );
        QuatCopy( legsLeftQuat, currentStatePose[ leftUpLegBone->number ].rotate );
        QuatCopy( legsRightQuat, currentStatePose[ rightUpLegBone->number ].rotate );
        QuatCopy( legsLeftQuat, lastStatePose[ leftUpLegBone->number ].rotate );
        QuatCopy( legsRightQuat, lastStatePose[ rightUpLegBone->number ].rotate );
        #endif

        //
        // Spine(Pitch and Yaw)
        //
        Vector3 spineRotateEuler = QM_QuaternionToEuler( currentStatePose[ spineBone->number ].rotate );

        // Calculate and 'turn to' desired yaw.
        static float initialBaseSpineYaw = spineRotateEuler.z;
        static float spineDesiredYaw = initialBaseSpineYaw;
        static float spineYaw = initialBaseSpineYaw; // Z == Pitch for this.

        static float initialBaseSpinePitch = spineRotateEuler.y;
        static float spineDesiredPitch = initialBaseSpinePitch;
        static float spinePitch = initialBaseSpinePitch; // Z == Pitch for this.

        spineYaw = AngleMod( clgi.client->refdef.viewangles[ YAW ] );

        // Get a quaternion of the players pitch angle.
        //Quaternion spineQuatPitchRotate = QM_QuaternionFromAxisAngle( Vector3( 1, 0, 0 ), DEG2RAD( spineAngles[ PITCH ] ) );
        // Get a quaternion of the players yaw angle.
        Quaternion spineQuatYawRotate = QM_QuaternionFromAxisAngle( Vector3( 0, 1, 0 ), DEG2RAD( spineYaw ) );
        // Get a quaternion of the players yaw angle.
        Quaternion spineQuatPitchRotate = QM_QuaternionFromAxisAngle( Vector3( 1, 0, 0 ), DEG2RAD( spineDesiredPitch ) );
        // Apply the pitch rotation to the ZUp orientation to get the hipQuaternion.
        //Quaternion spineQuat = QM_QuaternionMultiply( quatZUpOrientation, spineQuatYawRotate );
        // Apply the pitch rotation to the ZUp orientation to get the hipQuaternion.
        //spineQuat = QM_QuaternionMultiply( spineQuat, spineQuatPitchRotate );
        Quaternion spineQuat = QM_QuaternionMultiply( QM_QuaternionIdentity(), spineQuatYawRotate );
        //spineQuat = QM_QuaternionMultiply( spineQuat, spineQuatPitchRotate);
        //spineQuat = QM_QuaternionSlerp( lastStatePose[ spineBone->number ].rotate, spineQuat, 1.0 - clgi.client->xerpFraction );

        // Assign.
        QuatCopy( spineQuat, currentStatePose[ spineBone->number ].rotate );
        QuatCopy( spineQuat, lastStatePose[ spineBone->number ].rotate );
    #endif

    // Calculate the front and backlerp so that the last and current animation can smoothly
    // lerp into one another.
    double scaleFactor = CLG_ETPlayer_GetSwitchAnimationScaleFactor( lastLowerBodyState->timeStart, currentLowerBodyState->timeStart, extrapolatedTime );
    const double frontLerp = constclamp( ( 1.0 / lastLowerBodyState->timeDuration.milliseconds() ) * ( 1.0 / scaleFactor ), 0.0, 1.0 );
    const double backLerp = constclamp( ( 1.0 - frontLerp ), 0.0, 1.0 );// clgi.client->xerpFraction;
    // Lerp.
    //clgi.Print( PRINT_DEVELOPER, "%s: scaleFactor(%f), frontLerp(%f), backLerp(%f)\n", __func__, scaleFactor, frontLerp, backLerp );

    clgi.SKM_LerpBonePoses( model, 
        currentStatePose, lastStatePose, 
        frontLerp, backLerp, 
        finalStatePose, 
        0, 0
    );

    // Defaulted, unless event animations are actively playing.
    refreshEntity->bonePoses = finalStatePose;
    refreshEntity->lastBonePoses = lastFinalStatePose;
    refreshEntity->rootMotionBoneID = 0;
    refreshEntity->rootMotionFlags = SKM_POSE_TRANSLATE_Z;
    refreshEntity->backlerp = clgi.client->xerpFraction;
    
#if 0
    //
    // Last Body State
    //
    int32_t lastLowerBodyOldFrame = 0;
    int32_t lastLowerBodyCurrentFrame = 0;
    double lastLowerBodyBackLerp = 0.0;
    // Process lower body animation.
    sg_skm_animation_state_t *lastLowerBodyState = &animationMixer->lastBodyStates[ SKM_BODY_LOWER ];
    SG_SKM_ProcessAnimationStateForTime( model, lastLowerBodyState, extrapolatedTime, &lastLowerBodyOldFrame, &lastLowerBodyCurrentFrame, &lastLowerBodyBackLerp );
    // Fetch and Lerp specified frame bone poses.
    static skm_transform_t lastStatePose[ SKM_MAX_BONES ];
    framePose = clgi.SKM_GetBonePosesForFrame( model, lastLowerBodyCurrentFrame );
    oldFramePose = clgi.SKM_GetBonePosesForFrame( model, lastLowerBodyOldFrame );
    clgi.SKM_LerpBonePoses(
        model,
        framePose, oldFramePose,
        1.0 - lastLowerBodyBackLerp, lastLowerBodyBackLerp,
        lastStatePose,
        rootMotionBoneID, rootMotionAxisFlags
    );

    //
    // Event Body State
    //
    int32_t eventBodyOldFrame = 0;
    int32_t eventBodyCurrentFrame = 0;
    double eventBodyBackLerp = 0.0;
    // Process lower body animation.
    sg_skm_animation_state_t *eventBodyState = &animationMixer->eventBodyState[ SKM_BODY_LOWER ];
    const bool eventFinishedPlaying = SG_SKM_ProcessAnimationStateForTime( model, eventBodyState, extrapolatedTime, &eventBodyOldFrame, &eventBodyCurrentFrame, &eventBodyBackLerp );
    // Fetch and Lerp specified frame bone poses.
    static skm_transform_t currentEventStatePose[ SKM_MAX_BONES ];
    framePose = clgi.SKM_GetBonePosesForFrame( model, eventBodyCurrentFrame );
    oldFramePose = clgi.SKM_GetBonePosesForFrame( model, eventBodyOldFrame );
    clgi.SKM_LerpBonePoses(
        model,
        framePose, oldFramePose,
        1.0 - eventBodyBackLerp, eventBodyBackLerp,
        currentEventStatePose,
        rootMotionBoneID, 0
    );

    //sg_time_t lowerBodyDuration = ( lowerBodyState->animationEndTime - lowerBodyState->timeStart );
    //sg_time_t lastLowerBodyDuration = ( lastLowerBodyState->animationEndTime - lastLowerBodyState->timeStart );
    //// Speed multipliers to correctly transition from one animation to another
    //float a = 1.0f;
    //float blendFactor = lowerBodyBackLerp;
    ////float b = pBaseAnimation->GetDuration() / pLayeredAnimation->GetDuration();
    //float b = lowerBodyDuration.seconds() / lastLowerBodyDuration.seconds();
    //const float animSpeedMultiplierUp = ( 1.0f - blendFactor ) * a + b * blendFactor; // Lerp
    //a = lastLowerBodyDuration.seconds() / lowerBodyDuration.seconds();
    //b = 1.0f;
    //const float animSpeedMultiplierDown = ( 1.0f - blendFactor ) * a + b * blendFactor; // Lerp

    //
    // Blend animations.
    //
    skm_bone_node_t *blendStartBone = clgi.SKM_GetBoneByName( model, "mixamorig8:Hips" );
    skm_bone_node_t *blendLeftUpBone = clgi.SKM_GetBoneByName( model, "mixamorig8:LeftUpLeg" );
    skm_bone_node_t *blendRightUpBone = clgi.SKM_GetBoneByName( model, "mixamorig8:RightUpLeg" );
    static skm_transform_t blendedStatePose[ SKM_MAX_BONES ];
    //clgi.SKM_RecursiveBlendFromBone( lastStatePose, blendedStatePose, blendStartBone, lastLowerBodyBackLerp, 1 );
    //clgi.SKM_RecursiveBlendFromBone( currentStatePose, blendedStatePose, blendStartBone, lowerBodyBackLerp, 1 );
    if ( eventBodyBackLerp < 1.0 ) {
        clgi.SKM_RecursiveBlendFromBone( currentEventStatePose, blendedStatePose, blendStartBone, eventBodyBackLerp, 1 );
        refreshEntity->bonePoses = currentEventStatePose;
        ////    //clgi.SKM_RecursiveBlendFromBone( currentEventStatePose, blendedStatePose, blendLeftUpBone, eventBodyBackLerp, 1 );
    ////    //clgi.SKM_RecursiveBlendFromBone( currentEventStatePose, blendedStatePose, blendRightUpBone, eventBodyBackLerp, 1 );
    } else {
        clgi.SKM_RecursiveBlendFromBone( currentStatePose, blendedStatePose, blendStartBone, lowerBodyBackLerp, 1 );
        refreshEntity->bonePoses = currentStatePose;
    }

    refreshEntity->rootMotionBoneID = rootMotionBoneID;
    refreshEntity->rootMotionFlags = SKM_POSE_TRANSLATE_Z;
    //refreshEntity->bonePoses = blendedStatePose;
#endif
}



/**
*
* 
*   Entity Type Player
* 
* 
**/
/**
*   @brief  Type specific routine for LERPing ET_PLAYER origins.
**/
void CLG_ETPlayer_LerpOrigin( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *newState ) {
    // If client entity, use predicted origin instead of Lerped:
    if ( CLG_IsViewClientEntity( newState ) ) {
        #if 0
            VectorCopy( clgi.client->playerEntityOrigin, refreshEntity->origin );
            VectorCopy( packetEntity->current.origin, refreshEntity->oldorigin );  // FIXME

            // We actually need to offset the Z axis origin by half the bbox height.
            Vector3 correctedOrigin = clgi.client->playerEntityOrigin;
            Vector3 correctedOldOrigin = packetEntity->current.origin;
            // For being Dead:
            if ( clgi.client->predictedState.currentPs.stats[ STAT_HEALTH ] <= -40 ) {
                correctedOrigin.z += PM_BBOX_GIBBED_MINS.z;
                correctedOldOrigin.z += PM_BBOX_GIBBED_MINS.z;
                // For being Ducked:
            } else if ( clgi.client->predictedState.currentPs.pmove.pm_flags & PMF_DUCKED ) {
                correctedOrigin.z += PM_BBOX_DUCKED_MINS.z;
                correctedOldOrigin.z += PM_BBOX_DUCKED_MINS.z;
            } else {
                correctedOrigin.z += PM_BBOX_STANDUP_MINS.z;
                correctedOldOrigin.z += PM_BBOX_STANDUP_MINS.z;
            }
            VectorCopy( correctedOrigin, refreshEntity->origin );
            VectorCopy( correctedOldOrigin, refreshEntity->oldorigin );
        #else
            // We actually need to offset the Z axis origin by half the bbox height.
            Vector3 correctedOrigin = clgi.client->playerEntityOrigin;
            // For being Dead:
            if ( clgi.client->predictedState.currentPs.stats[ STAT_HEALTH ] <= -40 ) {
                correctedOrigin.z += PM_BBOX_GIBBED_MINS.z;
                // For being Ducked:
            } else if ( clgi.client->predictedState.currentPs.pmove.pm_flags & PMF_DUCKED ) {
                correctedOrigin.z += PM_BBOX_DUCKED_MINS.z;
            } else {
                correctedOrigin.z += PM_BBOX_STANDUP_MINS.z;
            }

            // Now apply the corrected origin to our refresh entity.
            VectorCopy( correctedOrigin, refreshEntity->origin );
            VectorCopy( refreshEntity->origin, refreshEntity->oldorigin );
        #endif
    // Lerp Origin:
    } else {
        Vector3 cent_origin = QM_Vector3Lerp( packetEntity->prev.origin, packetEntity->current.origin, clgi.client->lerpfrac );
        VectorCopy( cent_origin, refreshEntity->origin );
        VectorCopy( refreshEntity->origin, refreshEntity->oldorigin );
    }
}
/**
*   @brief  Type specific routine for LERPing ET_PLAYER angles.
**/
void CLG_ETPlayer_LerpAngles( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *newState ) {
    if ( CLG_IsViewClientEntity( newState ) ) {
        VectorCopy( clgi.client->playerEntityAngles, refreshEntity->angles );      // use predicted angles
    } else {
        LerpAngles( packetEntity->prev.angles, packetEntity->current.angles, clgi.client->lerpfrac, refreshEntity->angles );
    }
}
/**
*   @brief Apply flag specified effects.
**/
void CLG_ETPlayer_AddEffects( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *newState ) {
    // This is specific to when the player entity turns into GIB without being an ET_GIB.
    // If no rotation flag is set, add specified trail flags. We don't need it spamming
    // a blood trail of entities when it basically stopped motion.
    if ( newState->effects & ~EF_ROTATE ) {
        if ( newState->effects & EF_GIB ) {
            CLG_DiminishingTrail( packetEntity->lerp_origin, refreshEntity->origin, packetEntity, newState->effects | EF_GIB );
        }
    }
}

/**
*   @brief Apply flag specified effects.
**/
void CLG_ETPlayer_LerpStairStep( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *newState ) {
    // Handle the possibility of a stair step occuring.
    static constexpr int64_t STEP_TIME = 150; // Smooths it out over 150ms, this used to be 100ms.
    uint64_t realTime = clgi.GetRealTime();
    if ( packetEntity->step_realtime >= realTime - STEP_TIME ) {
        uint64_t stair_step_delta = clgi.GetRealTime() - packetEntity->step_realtime;
        //uint64_t stair_step_delta = clgi.client->time - ( packetEntity->step_servertime - clgi.client->sv_frametime );

        // Smooth out stair step over 200ms.
        if ( stair_step_delta <= STEP_TIME ) {
            static constexpr float STEP_BASE_1_FRAMETIME = 1.0f / STEP_TIME; // 0.01f;

            // Smooth it out further for smaller steps.
            //static constexpr float STEP_MAX_SMALL_STEP_SIZE = 18.f;
            static constexpr float STEP_MAX_SMALL_STEP_SIZE = 15.f;
            if ( fabs( packetEntity->step_height ) <= STEP_MAX_SMALL_STEP_SIZE ) {
                stair_step_delta <<= 1; // small steps
            }

            // Calculate step time.
            int64_t stair_step_time = STEP_TIME - min( stair_step_delta, STEP_TIME );

            // Calculate lerped Z origin.
            //packetEntity->current.origin[ 2 ] = QM_Lerp( packetEntity->prev.origin[ 2 ], packetEntity->current.origin[ 2 ], stair_step_time * STEP_BASE_1_FRAMETIME );
            refreshEntity->origin[ 2 ] = QM_Lerp( packetEntity->prev.origin[ 2 ], packetEntity->current.origin[ 2 ], stair_step_time * STEP_BASE_1_FRAMETIME );
            VectorCopy( packetEntity->current.origin, refreshEntity->oldorigin );
        }
    }
}

/**
*	@brief	Will setup the refresh entity for the ET_PLAYER centity with the newState.
**/
void CLG_PacketEntity_AddPlayer( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *newState ) {
    //
    // Will only be called once for each ET_PLAYER.
    //
    if ( packetEntity->bonePoseCache == nullptr ) {
        CLG_ETPlayer_AllocatePoseCache( packetEntity, refreshEntity, newState );
    }
    
    //
    // Lerp Origin:
    //
    CLG_ETPlayer_LerpOrigin( packetEntity, refreshEntity, newState );
    //
    // Lerp Angles.
    //
    CLG_ETPlayer_LerpAngles( packetEntity, refreshEntity, newState );
    //
    // Apply Effects.
    //
    CLG_ETPlayer_AddEffects( packetEntity, refreshEntity, newState );
    //
    // Special RF_STAIR_STEP lerp for Z axis.
    // 
    CLG_ETPlayer_LerpStairStep( packetEntity, refreshEntity, newState );

    //
    // Add Refresh Entity Model:
    // 
    // Model Index #1:
    if ( newState->modelindex ) {
        // Get client information.
        clientinfo_t *ci = &clgi.client->clientinfo[ newState->skinnum & 0xff ];

        // A client player model index.
        if ( newState->modelindex == MODELINDEX_PLAYER ) {
            // Parse and use custom player skin.
            refreshEntity->skinnum = 0;
            
            refreshEntity->skin = ci->skin;
            refreshEntity->model = ci->model;

            // On failure to find any custom client skin and model, resort to defaults being baseclientinfo.
            if ( !refreshEntity->skin || !refreshEntity->model ) {
                refreshEntity->skin = clgi.client->baseclientinfo.skin;
                refreshEntity->model = clgi.client->baseclientinfo.model;
                ci = &clgi.client->baseclientinfo;
            }

            // In case of the DISGUISE renderflag set, use the disguise skin.
            if ( newState->renderfx & RF_USE_DISGUISE ) {
                char buffer[ MAX_QPATH ];

                Q_concat( buffer, sizeof( buffer ), "players/", ci->model_name, "/disguise.pcx" );
                refreshEntity->skin = clgi.R_RegisterSkin( buffer );
            }
        // A regular alias entity model instead:
        } else {
            refreshEntity->skinnum = newState->skinnum;
            refreshEntity->skin = 0;
            refreshEntity->model = clgi.client->model_draw[ newState->modelindex ];
        }

        // Allow skin override for remaster.
        if ( newState->renderfx & RF_CUSTOMSKIN && (unsigned)newState->skinnum < CS_IMAGES + MAX_IMAGES /* CS_MAX_IMAGES */ ) {
            if ( newState->skinnum >= 0 && newState->skinnum < 512 ) {
                refreshEntity->skin = clgi.client->image_precache[ newState->skinnum ];
            }
            refreshEntity->skinnum = 0;
        }

        // Render effects.
        refreshEntity->flags = newState->renderfx;

        // In case of the state belonging to the frame's viewed client number:
        if ( CLG_IsViewClientEntity( newState ) ) {
            // Determine the base animation to play.
            CLG_ETPlayer_DetermineBaseAnimations( packetEntity, refreshEntity, newState );
            // Determine the event animation(s) to play.
            CLG_ETPlayer_DetermineEventAnimations( packetEntity, refreshEntity, newState );

            // When not in third person mode:
            if ( !clgi.client->thirdPersonView ) {
                // If we're running RTX, we want the player entity to render for shadow/reflection reasons:
                //if ( clgi.GetRefreshType() == REF_TYPE_VKPT ) {
                    // Make it so it isn't seen by the FPS view, only from mirrors.
                refreshEntity->flags |= RF_VIEWERMODEL;    // only draw from mirrors
                // In OpenGL mode we're already done, so we skip.
                //} else {
                //    goto skip;
                //}
            }

            // Don't tilt the model - looks weird.
            refreshEntity->angles[ 0 ] = 0.f;

            // If not thirderson, offset the model back a bit to make the view point located in front of the head:
            if ( cl_player_model->integer != CL_PLAYER_MODEL_THIRD_PERSON ) {
                vec3_t angles = { 0.f, refreshEntity->angles[ 1 ], 0.f };
                vec3_t forward;
                AngleVectors( angles, forward, NULL, NULL );
                float offset = -15.f;
                VectorMA( refreshEntity->origin, offset, forward, refreshEntity->origin );
                VectorMA( refreshEntity->oldorigin, offset, forward, refreshEntity->oldorigin );
            // Offset it determined on what animation state it is in:
            } else {

            }

            // Process the animations.
            CLG_ETPlayer_ProcessAnimations( packetEntity, refreshEntity, newState );
        }

        // Add model.
        clgi.V_AddEntity( refreshEntity );
    }
    // Model Index #2:
    if ( newState->modelindex2 ) {
        // Client Entity Weapon Model:
        if ( newState->modelindex2 == MODELINDEX_PLAYER ) {
            // Fetch client info ID. (encoded in skinnum)
            clientinfo_t *ci = &clgi.client->clientinfo[ newState->skinnum & 0xff ];
            // Fetch weapon ID. (encoded in skinnum).
            int32_t weaponModelIndex = ( newState->skinnum >> 8 ); // 0 is default weapon model
            if ( weaponModelIndex < 0 || weaponModelIndex > precache.numViewModels - 1 ) {
                weaponModelIndex = 0;
            }
            // See if we got a precached weapon model index for the matching client info.
            refreshEntity->model = ci->weaponmodel[ weaponModelIndex ];
            // If not:
            if ( !refreshEntity->model ) {
                // Try using its default index 0 model.
                if ( weaponModelIndex != 0 ) {
                    refreshEntity->model = ci->weaponmodel[ 0 ];
                }
                // If not, use our own baseclient info index 0 weapon model:
                if ( !refreshEntity->model ) {
                    refreshEntity->model = clgi.client->baseclientinfo.weaponmodel[ 0 ];
                }
            }
        // Regular 2nd model index.
        } else {
            refreshEntity->model = clgi.client->model_draw[ newState->modelindex2 ];
        }
        // Add shell effect.
        //if ( newState->effects & EF_COLOR_SHELL ) {
        //    refreshEntity->flags = renderfx;
        //}
        clgi.V_AddEntity( refreshEntity );
    }
    // Model Index #3:
    if ( newState->modelindex3 ) {
        // Reset.
        refreshEntity->skinnum = 0;
        refreshEntity->skin = 0;
        refreshEntity->flags = 0;
        // Add model.
        refreshEntity->model = clgi.client->model_draw[ newState->modelindex3 ];
        clgi.V_AddEntity( refreshEntity );
    }
    // Model Index #4:
    if ( newState->modelindex4 ) {
        // Reset.
        refreshEntity->skinnum = 0;
        refreshEntity->skin = 0;
        refreshEntity->flags = 0;
        // Add model.
        refreshEntity->model = clgi.client->model_draw[ newState->modelindex4 ];
        clgi.V_AddEntity( refreshEntity );
    }

    // skip:
    VectorCopy( refreshEntity->origin, packetEntity->lerp_origin );
}