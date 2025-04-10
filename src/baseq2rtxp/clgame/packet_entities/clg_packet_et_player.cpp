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
    // We'll assume that, in this case, all caches are allocated.
    if ( packetEntity->bonePoseCache[ 0 ] != nullptr ) {
        return;
    }

    // Determine whether the entity now has a skeletal model, and if so, allocate a bone pose cache for it.
    if ( const model_t *entModel = clgi.R_GetModelDataForHandle( refreshEntity->model ) ) {
        // Make sure it has proper SKM data.
        if ( ( entModel->skmData ) && ( entModel->skmConfig ) ) {
            // Acquire cached memory blocks.
            for ( int i = 0; i < SKM_BODY_MAX; i++ ) {
                if ( packetEntity->bonePoseCache[i] == nullptr ) {
                    // Allocate bone pose space. ( Use SKM_MAX_BONES instead of entModel->skmConfig->numberOfBones because client models could change. )
                    packetEntity->bonePoseCache[ i ] = clgi.SKM_PoseCache_AcquireCachedMemoryBlock( SKM_MAX_BONES /*entModel->skmConfig->numberOfBones*/ );
                }
                if ( packetEntity->lastBonePoseCache[ i ] == nullptr ) {
                    // Allocate bone pose space. ( Use SKM_MAX_BONES instead of entModel->skmConfig->numberOfBones because client models could change. )
                    packetEntity->lastBonePoseCache[ i ] = clgi.SKM_PoseCache_AcquireCachedMemoryBlock( SKM_MAX_BONES /*entModel->skmConfig->numberOfBones*/ );
                }
                if ( packetEntity->eventBonePoseCache[ i ] == nullptr ) {
                    // Allocate bone pose space. ( Use SKM_MAX_BONES instead of entModel->skmConfig->numberOfBones because client models could change. )
                    packetEntity->eventBonePoseCache[ i ] = clgi.SKM_PoseCache_AcquireCachedMemoryBlock( SKM_MAX_BONES /*entModel->skmConfig->numberOfBones*/ );
                }
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
    if ( CLG_IsClientEntity( newState ) ) {
        // Determine 'Base' animation name.
        double frameTime = 1.f;
        const std::string baseAnimStr = SG_Player_GetClientBaseAnimation( &game.predictedState.lastPs, &game.predictedState.currentPs, &frameTime );

        // Start timer is always just servertime that we had.
        const QMTime startTimer = QMTime::FromMilliseconds( clgi.client->servertime );

        //const QMTime startTimer = lastBodyState[ SKM_BODY_LOWER ].timeStart - QMTime::FromMilliseconds( clgi.client->extrapolatedTime );
        
        // Temporary for setting the animation.
        sg_skm_animation_state_t newAnimationBodyState;
        // We want this to loop for most animations.
        if ( SG_SKM_SetStateAnimation( model, &newAnimationBodyState, baseAnimStr.c_str(), startTimer, frameTime, true, false ) ) {
            // However, if the last body state was of a different animation type, we want to continue using its
            // start time so we can ensure that switching directions keeps the feet neatly lerping.
            if ( currentBodyState[ SKM_BODY_LOWER ].animationID != newAnimationBodyState.animationID ) {
                // Store the what once was current, as last body state.
                lastBodyState[ SKM_BODY_LOWER ] = currentBodyState[ SKM_BODY_LOWER ];
                // Assign the newly set animation state.
                currentBodyState[ SKM_BODY_LOWER ] = newAnimationBodyState;
                //clgi.Print( PRINT_DEVELOPER, "%s: newAnimID=%i\n", __func__, newAnimationBodyState.animationID );
            }
        }
    } else {
        // Decode the entity's animationIDs from its newState.
        uint8_t lowerAnimationID = 0;
        uint8_t torsoAnimationID = 0;
        uint8_t headAnimationID = 0;
        uint8_t animationFrameRate = BASE_FRAMERATE; // NOTE: Also set by SG_DecodeAnimationState :-)
        // Decode it.
        SG_DecodeAnimationState( newState->frame, &lowerAnimationID, &torsoAnimationID, &headAnimationID, &animationFrameRate );
        //clgi.Print( PRINT_NOTICE, "%s: lowerAnimationID(%i), torsoAnimationID(%i), headAnimationID(%i), framerate(%i)\n", __func__, lowerAnimationID, torsoAnimationID, headAnimationID, animationFrameRate );
        
        // Start timer is always just servertime that we had.
        const QMTime startTimer = QMTime::FromMilliseconds( clgi.client->servertime );
        // Calculate frameTime based on frameRate.
        const double frameTime = 1000.0 / animationFrameRate;
        // Temporary for setting the animation.
        sg_skm_animation_state_t newAnimationBodyState;
        // We want this to loop for most animations.
        if ( SG_SKM_SetStateAnimation( model, &newAnimationBodyState, lowerAnimationID, startTimer, frameTime, true, false ) ) {
            // However, if the last body state was of a different animation type, we want to continue using its
            // start time so we can ensure that switching directions keeps the feet neatly lerping.
            if ( currentBodyState[ SKM_BODY_LOWER ].animationID != newAnimationBodyState.animationID ) {
                // Store the what once was current, as last body state.
                lastBodyState[ SKM_BODY_LOWER ] = currentBodyState[ SKM_BODY_LOWER ];
                // Assign the newly set animation state.
                //newAnimationBodyState.timeSTart = lastBodyState[ SKM_BODY_LOWER ].timeStart;
                currentBodyState[ SKM_BODY_LOWER ] = newAnimationBodyState;
            }
        }
    }
}

/***
*
*
*
*   Animation Utilities: TODO: Clean Up a bit, relocate, etc.
* 
*
*
**/
/**
*   @brief  Determine which animations to play based on the player state event channels.
**/
void CLG_ETPlayer_DetermineEventAnimations( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *newState ) {

}

/**
*   @brief  Calculates the 'scaleFactor' of an animation switch, for use with the fraction and lerp values between two animation poses. 
**/
static const double CLG_ETPlayer_GetSwitchAnimationScaleFactor( const QMTime &lastTimeStamp, const QMTime &nextTimeStamp, const QMTime &animationTime ) {
    const double midWayLength = animationTime.Milliseconds() - lastTimeStamp.Milliseconds();
    const double framesDiff = nextTimeStamp.Milliseconds() - lastTimeStamp.Milliseconds();
    // WID: Prevent a possible division by zero?
    if ( framesDiff > 0 ) {
        return midWayLength / framesDiff;
    } else {
        return 0;
    }
}

/**
*   @brief  
*   @return A boolean of whether the animation state is still activeplay playing, or not.
*           ( !!! Does not per se mean, backLerp > 0 ).
*   @note   poseBuffer is expected to be SKM_MAX_BONES in size.
*   @note   The model and its skmData member are expected to be valid. Make sure to check these before calling this function.
**/
static const bool CLG_ETPlayer_GetLerpedAnimationStatePoseForTime( const model_t *model, const QMTime &time, const int32_t rootMotionAxisFlags, const skm_bone_node_t *boneNode, sg_skm_animation_state_t *animationState, skm_transform_t *outPoseBuffer ) {
    // Get current and old frame poses and determine backlerp.
    double animationStateBackLerp = 0.0;
    int32_t animationStateCurrentFrame = 0, animationStateOldFrame = 0;
    const bool isPlaying = SG_SKM_ProcessAnimationStateForTime( 
        model, 
        animationState, 
        time, 
        &animationStateOldFrame, 
        &animationStateCurrentFrame,
        &animationStateBackLerp 
    );

    // If for whatever reason animation state is out of bounds frame wise...
    if ( ( animationStateCurrentFrame < 0 || animationStateCurrentFrame > model->skmData->num_frames ) ) {
        //animationStateCurrentFrame = animationState->srcStartFrame;
        return false;
    }
    // If for whatever reason animation state is out of bounds frame wise...
    if ( ( animationStateOldFrame < 0 || animationStateOldFrame > model->skmData->num_frames ) ) {
        animationStateOldFrame = animationState->currentFrame;
        return false;
    }

    // Get pose frames for time..
    const skm_transform_t *framePose = clgi.SKM_GetBonePosesForFrame( model, animationStateCurrentFrame );
    const skm_transform_t *oldFramePose = clgi.SKM_GetBonePosesForFrame( model, animationStateOldFrame );

    // If the lerp is > 0, it means we have a legitimate framePose and oldFramePose to work with.
    if ( animationStateCurrentFrame != -1 && animationStateBackLerp <= 1 ) {/*animationStateBackLerp > 0 && framePose && oldFramePose*/
        const double frontLerp = 1.0 - animationStateBackLerp;
        const double backLerp = animationStateBackLerp;
        // Lerp the final pose.
        SKM_LerpBonePoses( model,
            framePose, oldFramePose,
            frontLerp, backLerp,
            outPoseBuffer,
            ( boneNode != nullptr ? boneNode->number : 0 ), rootMotionAxisFlags
        );
    }
    return isPlaying;
}

/**
*   @brief  Calculate desired yaw derived player state move direction, and recored time of change.
**/
const bool CLG_ETPlayer_CalculateDesiredYaw( centity_t *packetEntity, const player_state_t *playerState, const player_state_t *oldPlayerState, const QMTime &currentTime ) {
    if ( playerState->animation.moveDirection != oldPlayerState->animation.moveDirection ) {
        const int32_t moveDirection = playerState->animation.moveDirection;
        if ( ( moveDirection & PM_MOVEDIRECTION_FORWARD ) && ( moveDirection & PM_MOVEDIRECTION_LEFT ) ) {
            packetEntity->moveInfo.current.transitions.yaw.timeChanged = currentTime;
            packetEntity->moveInfo.current.transitions.yaw.desired = 45.;
            return true;
        } else if ( ( moveDirection & PM_MOVEDIRECTION_FORWARD ) && ( moveDirection & PM_MOVEDIRECTION_RIGHT ) ) {
            packetEntity->moveInfo.current.transitions.yaw.timeChanged = currentTime;
            packetEntity->moveInfo.current.transitions.yaw.desired = -45.;
            return true;
        } else if ( ( moveDirection & PM_MOVEDIRECTION_BACKWARD ) && ( moveDirection & PM_MOVEDIRECTION_LEFT ) ) {
            packetEntity->moveInfo.current.transitions.yaw.timeChanged = currentTime;
            packetEntity->moveInfo.current.transitions.yaw.desired = -45.;
            return true;
        } else if ( ( moveDirection & PM_MOVEDIRECTION_BACKWARD ) && ( moveDirection & PM_MOVEDIRECTION_RIGHT ) ) {
            packetEntity->moveInfo.current.transitions.yaw.timeChanged = currentTime;
            packetEntity->moveInfo.current.transitions.yaw.desired = 45.;
            return true;
        } else {
            packetEntity->moveInfo.current.transitions.yaw.timeChanged = currentTime;
            packetEntity->moveInfo.current.transitions.yaw.desired = 0.;
            return true;
        }
    }

    return false;
}
/**
*   @brief  Calculate desired yaw derived from moveInfo, and recored time of change.
**/
const bool CLG_ETPlayer_CalculateDesiredYaw( centity_t *packetEntity, const QMTime &currentTime ) {
    if ( packetEntity->moveInfo.current.flags != packetEntity->moveInfo.previous.flags ) {
        const int32_t moveInfoFlags = packetEntity->moveInfo.current.flags;
        if ( ( moveInfoFlags & CLG_MOVEINFOFLAG_DIRECTION_FORWARD ) && ( moveInfoFlags & CLG_MOVEINFOFLAG_DIRECTION_LEFT ) ) {
            packetEntity->moveInfo.current.transitions.yaw.timeChanged = currentTime;
            packetEntity->moveInfo.current.transitions.yaw.desired = 45.;
            return true;
        } else if ( ( moveInfoFlags & CLG_MOVEINFOFLAG_DIRECTION_FORWARD ) && ( moveInfoFlags & CLG_MOVEINFOFLAG_DIRECTION_RIGHT ) ) {
            packetEntity->moveInfo.current.transitions.yaw.timeChanged = currentTime;
            packetEntity->moveInfo.current.transitions.yaw.desired = -45.;
            return true;
        } else if ( ( moveInfoFlags & CLG_MOVEINFOFLAG_DIRECTION_BACKWARD ) && ( moveInfoFlags & CLG_MOVEINFOFLAG_DIRECTION_LEFT ) ) {
            packetEntity->moveInfo.current.transitions.yaw.timeChanged = currentTime;
            packetEntity->moveInfo.current.transitions.yaw.desired = -45.;
            return true;
        } else if ( ( moveInfoFlags & CLG_MOVEINFOFLAG_DIRECTION_BACKWARD ) && ( moveInfoFlags & CLG_MOVEINFOFLAG_DIRECTION_RIGHT ) ) {
            packetEntity->moveInfo.current.transitions.yaw.timeChanged = currentTime;
            packetEntity->moveInfo.current.transitions.yaw.desired = 45.;
            return true;
        } else {
            packetEntity->moveInfo.current.transitions.yaw.timeChanged = currentTime;
            packetEntity->moveInfo.current.transitions.yaw.desired = 0.;
            return true;
        }
    }

    return false;
}

/**
*   @brief  Performs bone controller work.
**/
void CLG_ETPlayer_ApplyBoneControllers( centity_t *packetEntity, const entity_state_t *newState, const model_t *model, const skm_transform_t *initialStatePose, skm_transform_t *lerpedBonePose, const QMTime &currentTime ) {
    /**
    *   Determine which direction we're heading into. And use it to possibly adjust an
    *   animation with, as well as to 'Control' various bones after our "final" pose
    *   has been calculated.
    **/
    // If the yaw has changed, we want to (re-)activate the bone controllers so they are updated to the new desired yaw.
    bool updateYawControllers = false;

    // Get the desired 'Yaw' angle to rotate into based on the (predicted-) player states.
    if ( CLG_IsClientEntity( newState ) ) {
        // States to determine it by.
        player_state_t *playerState = &game.predictedState.currentPs;
        player_state_t *oldPlayerState = &game.predictedState.lastPs;
        // Go at it.
        updateYawControllers = CLG_ETPlayer_CalculateDesiredYaw( packetEntity, playerState, oldPlayerState, currentTime );
        // Get the desired 'Yaw' angle based on the packetEntity's moveInfo.
    } else {
        updateYawControllers = CLG_ETPlayer_CalculateDesiredYaw( packetEntity, currentTime );
    }

    /**
    *   Update the controllers if a change has been detected,
    **/
    if ( updateYawControllers ) {
        /**
        *   Define time speeds for each individual bone (S)Lerp and acquire time of 'Yaw' change.
        **/
        // Speeds:
        static constexpr QMTime hipsYawSlerpTime = 350_ms;
        static constexpr QMTime spineYawSlerpTime = 250_ms;
        static constexpr QMTime spine1YawSlerpTime = 150_ms;
        // Time of Change:
        const QMTime yawChangedTime = packetEntity->moveInfo.current.transitions.yaw.timeChanged;

        // The desired yaw for the hips to turn to.
        //const float fabsXDot = ( packetEntity->moveInfo.current.xDot );
        //const double wishYaw = packetEntity->moveInfo.current.transitions.yaw.desired * fabsXDot;
        const double wishYaw = packetEntity->moveInfo.current.transitions.yaw.desired;

        //clgi.Print( PRINT_DEVELOPER, "xDot=%f\n", packetEntity->moveInfo.current.xDot );

        /**
        *   Acquire access to desired Bone Nodes:
        **/
        // The model's hip bone.
        skm_bone_node_t *hipsBoneNode = clgi.SKM_GetBoneByName( model, "mixamorig8:Hips" );
        // Spine bones, for upper body event animations.
        skm_bone_node_t *spineBoneNode = clgi.SKM_GetBoneByName( model, "mixamorig8:Spine" );
        skm_bone_node_t *spine1BoneNode = clgi.SKM_GetBoneByName( model, "mixamorig8:Spine1" );
        skm_bone_node_t *spine2BoneNode = clgi.SKM_GetBoneByName( model, "mixamorig8:Spine2" );
        //skm_bone_node_t *neckBone = clgi.SKM_GetBoneByName( model, "mixamorig8:Neck" );
        // Shoulder bones.
        //skm_bone_node_t *leftShoulderBone = clgi.SKM_GetBoneByName( model, "mixamorig8:LeftShoulder" );
        //skm_bone_node_t *rightShoulderBone = clgi.SKM_GetBoneByName( model, "mixamorig8:RightShoulder" );
        // Leg bones.
        //skm_bone_node_t *leftUpLegBone = clgi.SKM_GetBoneByName( model, "mixamorig8:LeftUpLeg" );
        //skm_bone_node_t *rightUpLegBone = clgi.SKM_GetBoneByName( model, "mixamorig8:RightUpLeg" );

        /**
        *   Acquire access to needed bone poses.
        **/
        skm_transform_t *hipsTransform = &lerpedBonePose[ hipsBoneNode->number ];
        skm_transform_t *spineTransform = &lerpedBonePose[ spineBoneNode->number ];
        skm_transform_t *spine1Transform = &lerpedBonePose[ spine1BoneNode->number ];
        skm_transform_t *spine2Transform = &lerpedBonePose[ spine2BoneNode->number ];

        /**
        *   Configure the bone controllers and their target 'Yaw's.
        **/
        skm_bone_controller_target_t hipsBoneTarget;
        skm_bone_controller_target_t spineBoneTarget;
        skm_bone_controller_target_t spine1BoneTarget;
        hipsBoneTarget.rotation.targetYaw = wishYaw;
        spineBoneTarget.rotation.targetYaw = -wishYaw;
        spine1BoneTarget.rotation.targetYaw = ( wishYaw / 4 );

        /**
        *   Activate them.
        **/
        // Activate 'Hips', bone controller.
        SKM_BoneController_Activate(
            &packetEntity->boneControllers[ 0 ],
            hipsBoneNode, hipsBoneTarget,
            hipsTransform, hipsTransform, SKM_BONE_CONTROLLER_TRANSFORM_ROTATION,
            hipsYawSlerpTime, yawChangedTime
        );
        // Activate 'Spine', bone controller.
        SKM_BoneController_Activate(
            &packetEntity->boneControllers[ 1 ],
            spineBoneNode, spineBoneTarget,
            spineTransform, spineTransform, SKM_BONE_CONTROLLER_TRANSFORM_ROTATION,
            spineYawSlerpTime, yawChangedTime
        );
        // Activate 'Spine1', bone controller.
        SKM_BoneController_Activate(
            &packetEntity->boneControllers[ 2 ],
            spine1BoneNode, spine1BoneTarget,
            spine1Transform, spine1Transform, SKM_BONE_CONTROLLER_TRANSFORM_ROTATION,
            spine1YawSlerpTime, yawChangedTime
        );
    }

    /**
    *   Apply the bone controllers to the lerpedBonePoses.
    **/
    SKM_BoneController_ApplyToPoseForTime( packetEntity->boneControllers, 3, currentTime, lerpedBonePose );
}

/**
*   @brief  Process the entity's active animations.
**/
void CLG_ETPlayer_ProcessAnimations( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *newState, const bool isLocalClientEntity ) {
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

    // Wait till we got the cache.
    if ( !packetEntity->bonePoseCache[0] || !packetEntity->lastBonePoseCache[0] ) {
        return;
    }

    /**
    *   Time:
    **/
    // The frame's Server Time.
    const QMTime serverTime = QMTime::FromMilliseconds( clgi.client->servertime );
    // The Time we're at.
    const QMTime extrapolatedTime = QMTime::FromMilliseconds( ( isLocalClientEntity ? clgi.client->extrapolatedTime : clgi.client->servertime ) );

    // FrontLerp Fraction (extrapolated ahead of Server Time).
    const double frontLerp = ( isLocalClientEntity ? clgi.client->xerpFraction : clgi.client->lerpfrac );
    // BackLerp Fraction.
    double backLerp = std::clamp( ( 1.0 - frontLerp ), 0.0, 1.0 );// clgi.client->xerpFraction;


    /**
    *   Acquire access to desired Bone Nodes:
    **/
    // The model's hip bone.
    skm_bone_node_t *hipsBone = clgi.SKM_GetBoneByName( model, "mixamorig8:Hips" );
    // Spine bones, for upper body event animations.
    skm_bone_node_t *spineBone = clgi.SKM_GetBoneByName( model, "mixamorig8:Spine" );
    skm_bone_node_t *spine1Bone = clgi.SKM_GetBoneByName( model, "mixamorig8:Spine1" );
    skm_bone_node_t *spine2Bone = clgi.SKM_GetBoneByName( model, "mixamorig8:Spine2" );
    //skm_bone_node_t *neckBone = clgi.SKM_GetBoneByName( model, "mixamorig8:Neck" );
    // Shoulder bones.
    //skm_bone_node_t *leftShoulderBone = clgi.SKM_GetBoneByName( model, "mixamorig8:LeftShoulder" );
    //skm_bone_node_t *rightShoulderBone = clgi.SKM_GetBoneByName( model, "mixamorig8:RightShoulder" );
    // Leg bones.
    //skm_bone_node_t *leftUpLegBone = clgi.SKM_GetBoneByName( model, "mixamorig8:LeftUpLeg" );
    //skm_bone_node_t *rightUpLegBone = clgi.SKM_GetBoneByName( model, "mixamorig8:RightUpLeg" );

    /**
    *   Acquire access to 'Base' Animation States:
    **/
    // The 'LAST' actively playing lower body animation state.
    sg_skm_animation_state_t *lastLowerBodyState = &animationMixer->lastBodyStates[ SKM_BODY_LOWER ];
    // The 'CURRENT' actively playing lower body animation state.
    sg_skm_animation_state_t *currentLowerBodyState = &animationMixer->currentBodyStates[ SKM_BODY_LOWER ];
    /**
    *   Acquire access to 'Event' Animation States:
    **/
    // The last received(possibly actively playing) 'LOWER BODY' event animation state.
    sg_skm_animation_state_t *lowerEventBodyState = &animationMixer->eventBodyState[ SKM_BODY_LOWER ];
    // The last received(possibly actively playing) 'UPPER BODY' event animation state.
    sg_skm_animation_state_t *upperEventBodyState = &animationMixer->eventBodyState[ SKM_BODY_UPPER ];


    /**
    *   'Base' Animation States Pose Lerping:
    **/
    skm_transform_t *finalStatePose = packetEntity->bonePoseCache[ SKM_BODY_LOWER ];
    skm_transform_t *lastFinalStatePose = packetEntity->lastBonePoseCache[ SKM_BODY_LOWER ];

    // Transition scale.
    bool lastStateIsPlaying = false;
    const double switchAnimationScaleFactor = CLG_ETPlayer_GetSwitchAnimationScaleFactor( currentLowerBodyState->timeStart, lastLowerBodyState->timeDuration + currentLowerBodyState->timeStart, extrapolatedTime );
    if ( switchAnimationScaleFactor >= 0 && switchAnimationScaleFactor <= 1 ) {
        // Process the 'LAST' lower body animation. (This is so we can smoothly transition from 'last' to 'current').
        lastStateIsPlaying = CLG_ETPlayer_GetLerpedAnimationStatePoseForTime( model, extrapolatedTime, SKM_POSE_TRANSLATE_ALL, nullptr, lastLowerBodyState, lastFinalStatePose );
    }
    // Process the 'CURRENT' lower body animation.
    CLG_ETPlayer_GetLerpedAnimationStatePoseForTime( model, extrapolatedTime, SKM_POSE_TRANSLATE_ALL, nullptr, currentLowerBodyState, finalStatePose );
    /**
    *   'Events' Animation States Pose Lerping:
    **/
    // These events are, "unfinished" by default:
    bool lowerEventIsPlaying = false;
    bool upperEventIsPlaying = false;
    // Root Motion Axis Flags:
    const int32_t lowerEventRootMotionAxisFlags = SKM_POSE_TRANSLATE_Z; // Z only by default.
    const int32_t upperEventRootMotionAxisFlags = SKM_POSE_TRANSLATE_ALL; // Z only by default.
    // Process the lower event body state animation IF still within valid time range:
    skm_transform_t *lowerEventStatePose = packetEntity->eventBonePoseCache[ SKM_BODY_LOWER ];
    if ( lowerEventBodyState->timeEnd >= extrapolatedTime ) {
        lowerEventIsPlaying = !CLG_ETPlayer_GetLerpedAnimationStatePoseForTime( model, extrapolatedTime, lowerEventRootMotionAxisFlags, nullptr, lowerEventBodyState, lowerEventStatePose );
    }
    // Process the upper event body state animation IF still within valid time range:
    skm_transform_t *upperEventStatePose = packetEntity->eventBonePoseCache[ SKM_BODY_UPPER ];
    if ( upperEventBodyState->timeEnd >= extrapolatedTime ) {
        upperEventIsPlaying = !CLG_ETPlayer_GetLerpedAnimationStatePoseForTime( model, extrapolatedTime, upperEventRootMotionAxisFlags, nullptr, upperEventBodyState, upperEventStatePose );
    }


    /**
    *   'Base' Blend Animation Transition over time, from the 'LAST' to 'CURRENT' lerped poses.
    **/
    // Blend old animation into the new one.
    if ( lastStateIsPlaying ) {
        /**
        *   Bone Controlling:
        **/
        //CLG_ETPlayer_ApplyBoneControllers( packetEntity, newState, model, lastFinalStatePose, lastLerpedPose, extrapolatedTime );
        //CLG_ETPlayer_ApplyBoneControllers( packetEntity, newState, model, finalStatePose, currentLerpedPose, extrapolatedTime );

        //SKM_RecursiveBlendFromBone( lastFinalStatePose, finalStatePose, finalStatePose, hipsBone, switchAnimationScaleFactor, switchAnimationScaleFactor );
        SKM_RecursiveBlendFromBone( finalStatePose, lastFinalStatePose, finalStatePose, hipsBone, switchAnimationScaleFactor, switchAnimationScaleFactor );
    } else {
        // They are already lerped so..
        #if 0
        // Just lerp them.
        clgi.SKM_LerpBonePoses( model,
            currentLerpedPose, lastLerpedPose,
            frontLerp, backLerp/*switchAnimationScaleFactor*/,
            currentLerpedPose,
            0, SKM_POSE_TRANSLATE_ALL
        );
        #endif
    }
    /**
    *   'Event' Animation Override Layering:
    **/
    // If playing, within a valid time range: Override the whole skeleton with the the lower event state pose.
    if ( lowerEventIsPlaying /*&& lowerEventBodyState->timeEnd >= extrapolatedTime */) {
        //memcpy( currentLerpedPose, lowerEventStatePose, SKM_MAX_BONES ); // This is faster if we override from the hips.
        SKM_RecursiveBlendFromBone( lowerEventStatePose, finalStatePose, finalStatePose, hipsBone, 1, 1 ); // Slower path..
    }
    // If playing, the upper event state overrides only the spine bone and all its child bones.
    if ( upperEventIsPlaying /*&& upperEventBodyState->timeEnd >= extrapolatedTime*/ ) {
        SKM_RecursiveBlendFromBone( upperEventStatePose, finalStatePose, finalStatePose, spineBone, 1, 1 );
    }


    /**
    *   Bone Controlling:
    **/
    //if ( lastStateIsPlaying ) {
    //    CLG_ETPlayer_ApplyBoneControllers( packetEntity, newState, model, lastFinalStatePose, finalStatePose, extrapolatedTime );
    //} else {
        CLG_ETPlayer_ApplyBoneControllers( packetEntity, newState, model, finalStatePose, finalStatePose, extrapolatedTime );
    //}


    /**
    *   Prepare the RefreshEntity by generating the local model space matrices for rendering.
    **/
    // This will suffice.
    refreshEntity->bonePoses = finalStatePose;
    #if 0
    // TODO: THIS NEEDS TO BE UNIQUE FOR EACH ET_PLAYER PACKETENTITY! OTHERWISE IT'LL OVERWRITE SAME MEMORY BEFORE IT IS RENDERED...
    // Local model space final representation matrices.
    static mat3x4 refreshBoneMats[ SKM_MAX_BONES ];
    // Transform.
    SKM_TransformBonePosesLocalSpace( model->skmData, currentLerpedPose, (float *)&refreshBoneMats[ 0 ] );
    // Assign final refresh mats.
    refreshEntity->localSpaceBonePose3x4Matrices = (float *)&refreshBoneMats[ 0 ];
    refreshEntity->rootMotionBoneID = 0;
    refreshEntity->rootMotionFlags = SKM_POSE_TRANSLATE_ALL;
    refreshEntity->backlerp = clgi.client->xerpFraction;
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
    if ( CLG_IsClientEntity( newState ) ) {
        #if 0
            VectorCopy( clgi.client->playerEntityOrigin, refreshEntity->origin );
            VectorCopy( packetEntity->current.origin, refreshEntity->oldorigin );  // FIXME

            // We actually need to offset the Z axis origin by half the bbox height.
            Vector3 correctedOrigin = clgi.client->playerEntityOrigin;
            Vector3 correctedOldOrigin = packetEntity->current.origin;
            // For being Dead:
            if ( game.predictedState.currentPs.stats[ STAT_HEALTH ] <= -40 ) {
                correctedOrigin.z += PM_BBOX_GIBBED_MINS.z;
                correctedOldOrigin.z += PM_BBOX_GIBBED_MINS.z;
                // For being Ducked:
            } else if ( game.predictedState.currentPs.pmove.pm_flags & PMF_DUCKED ) {
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
            // For being Dead( Gibbed ):
            if ( game.predictedState.currentPs.stats[ STAT_HEALTH ] <= -40 ) {
                correctedOrigin.z += PM_BBOX_GIBBED_MINS.z;
            // For being Ducked:
            } else if ( game.predictedState.currentPs.pmove.pm_flags & PMF_DUCKED ) {
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
        Vector3 lerpedOrigin = QM_Vector3Lerp( packetEntity->prev.origin, packetEntity->current.origin, clgi.client->lerpfrac );
        //Vector3 lerpedOrigin = QM_Vector3Lerp( packetEntity->current.origin, newState->origin, clgi.client->lerpfrac );
        Vector3 correctionVector = { 0.f, 0.f, packetEntity->mins[ 2 ] };
        lerpedOrigin += correctionVector;
        VectorCopy( lerpedOrigin, refreshEntity->origin );
        VectorCopy( refreshEntity->origin, refreshEntity->oldorigin );
    }
}
/**
*   @brief  Type specific routine for LERPing ET_PLAYER angles.
**/
void CLG_ETPlayer_LerpAngles( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *newState ) {
    if ( CLG_IsClientEntity( newState ) ) {
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
        uint64_t stair_step_delta = realTime - packetEntity->step_realtime;
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
            int64_t stair_step_time = STEP_TIME - std::min<int64_t>( stair_step_delta, STEP_TIME );

            // Calculate lerped Z origin.
            //packetEntity->current.origin[ 2 ] = QM_Lerp( packetEntity->prev.origin[ 2 ], packetEntity->current.origin[ 2 ], stair_step_time * STEP_BASE_1_FRAMETIME );
            refreshEntity->origin[ 2 ] = QM_Lerp<double>( packetEntity->prev.origin[ 2 ], packetEntity->current.origin[ 2 ], stair_step_time * STEP_BASE_1_FRAMETIME );
            VectorCopy( packetEntity->current.origin, refreshEntity->oldorigin );
        }
    }
}

/**
*	@brief	Will setup the refresh entity for the ET_PLAYER centity with the newState.
**/
void CLG_PacketEntity_AddPlayer( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *newState ) {
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

        //
        // Will only be called once for each ET_PLAYER.
        //
        if ( packetEntity->bonePoseCache[0] == nullptr ) {
            CLG_ETPlayer_AllocatePoseCache( packetEntity, refreshEntity, newState );
        }

        // In case of the state belonging to the frame's viewed client number:
        if ( CLG_IsClientEntity( newState ) ) {
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
            // Determine the base animation to play.
            CLG_ETPlayer_DetermineBaseAnimations( packetEntity, refreshEntity, newState );
            // Determine the event animation(s) to play.
            CLG_ETPlayer_DetermineEventAnimations( packetEntity, refreshEntity, newState );
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
            CLG_ETPlayer_ProcessAnimations( packetEntity, refreshEntity, newState, true );
        } else {
            // Determine the base animation to play.
            CLG_ETPlayer_DetermineBaseAnimations( packetEntity, refreshEntity, newState );
            // Determine the event animation(s) to play.
            CLG_ETPlayer_DetermineEventAnimations( packetEntity, refreshEntity, newState );
            // Don't tilt the model - looks weird.
            refreshEntity->angles[ 0 ] = 0.f;
            // Process the animations.
            CLG_ETPlayer_ProcessAnimations( packetEntity, refreshEntity, newState, false );
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