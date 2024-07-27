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
        
        // Temporary for setting the animation.
        sg_skm_animation_state_t newAnimationBodyState;

        // We want this to loop for most animations.
        if ( SG_SKM_SetStateAnimation( model, &newAnimationBodyState, baseAnimStr.c_str(), startTimer, frameTime, true, false ) ) {
            // However, if the last body state was of a different animation type, we want to continue using its
            // start time so we can ensure that switching directions keeps the feet neatly lerping.
            if ( lastBodyState[ SKM_BODY_LOWER ].animationID != newAnimationBodyState.animationID ) {
                // Store the what once was current, as last body state.
                lastBodyState[ SKM_BODY_LOWER ] = currentBodyState[ SKM_BODY_LOWER ];
                // Assign the newly set animation state.
                //newAnimationBodyState.timeSTart = lastBodyState[ SKM_BODY_LOWER ].timeStart;
                currentBodyState[ SKM_BODY_LOWER ] = newAnimationBodyState;
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
}

/**
*   @brief  Determine which animations to play based on the player state event channels.
**/
void CLG_ETPlayer_DetermineEventAnimations( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *newState ) {

}

#if 0
static const double CLG_ETPlayer_GetSwitchAnimationScaleFactor( const sg_time_t &lastTimeStamp, const sg_time_t &nextTimeStamp, const sg_time_t &animationTime ) {
    //double scaleFactor = 0.0;
    const double midWayLength = animationTime.milliseconds() - lastTimeStamp.milliseconds();
    const double framesDiff = nextTimeStamp.milliseconds() - lastTimeStamp.milliseconds();
    // WID: Prevent a possible division by zero?
    if ( framesDiff > 0 ) {
        return /*scaleFactor = */ midWayLength / framesDiff;
    } else {
        return 0;//0;
    }
}
#else
static const double CLG_ETPlayer_GetSwitchAnimationScaleFactor( const sg_time_t &lastTimeStamp, const sg_time_t &nextTimeStamp, const sg_time_t &animationTime ) {
    //double scaleFactor = 0.0;
    const double midWayLength = animationTime.milliseconds() - lastTimeStamp.milliseconds();
    const double framesDiff = nextTimeStamp.milliseconds() - lastTimeStamp.milliseconds();
    // WID: Prevent a possible division by zero?
    if ( framesDiff > 0 ) {
        return /*scaleFactor = */ midWayLength / framesDiff;
    } else {
        return 1;//0;
    }
}
#endif

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
    const sg_time_t serverTime = sg_time_t::from_ms( clgi.client->servertime );
    const sg_time_t extrapolatedTime = sg_time_t::from_ms( clgi.client->extrapolatedTime );

    // The model's hip bone.
    skm_bone_node_t *hipsBone = clgi.SKM_GetBoneByName( model, "mixamorig8:Hips" );
    // Spine bones, for upper body event animations.
    skm_bone_node_t *spineBone = clgi.SKM_GetBoneByName( model, "mixamorig8:Spine" );
    skm_bone_node_t *spine1Bone = clgi.SKM_GetBoneByName( model, "mixamorig8:Spine1" );
    skm_bone_node_t *spine2Bone = clgi.SKM_GetBoneByName( model, "mixamorig8:Spine2" );
    skm_bone_node_t *neckBone = clgi.SKM_GetBoneByName( model, "mixamorig8:Neck" );
    // Shoulder bones.
    skm_bone_node_t *leftShoulderBone = clgi.SKM_GetBoneByName( model, "mixamorig8:LeftShoulder" );
    skm_bone_node_t *rightShoulderBone = clgi.SKM_GetBoneByName( model, "mixamorig8:RightShoulder" );
    // Leg bones.
    skm_bone_node_t *leftUpLegBone = clgi.SKM_GetBoneByName( model, "mixamorig8:LeftUpLeg" );
    skm_bone_node_t *rightUpLegBone = clgi.SKM_GetBoneByName( model, "mixamorig8:RightUpLeg" );

    // Final state pose.
    static skm_transform_t finalStatePose[ SKM_MAX_BONES ];
    static skm_transform_t lastFinalStatePose[ SKM_MAX_BONES ];
    memcpy( lastFinalStatePose, finalStatePose, SKM_MAX_BONES * sizeof( skm_transform_t ) );
    
    // For the current active animation.
    static skm_transform_t currentStatePose[ SKM_MAX_BONES ];
    static skm_transform_t previousStatePose[ SKM_MAX_BONES ];
    // For the last animation that continues playing for transitioning purposes.
    static skm_transform_t lastCurrentStatePose[ SKM_MAX_BONES ];
    static skm_transform_t lastPreviousStatePose[ SKM_MAX_BONES ];

    // Backup the old previous frame state poses for smooth lerping.
    memcpy( lastPreviousStatePose, previousStatePose, SKM_MAX_BONES * sizeof( iqm_transform_t ) );
    memcpy( lastCurrentStatePose, currentStatePose, SKM_MAX_BONES * sizeof( iqm_transform_t ) );

    // Default rootmotion settings.
    int32_t rootMotionBoneID = 0, rootMotionAxisFlags = 0;

    //
    // Lower Body State(s):
    //
    int32_t currentLowerBodyCurrentFrame = 0, currentLowerBodyOldFrame = 0;
    int32_t lastLowerBodyCurrentFrame = 0, lastLowerBodyOldFrame = 0;
    double currentLowerBodyBackLerp = 0.0, lastLowerBodyBackLerp = 0.0;
    
    // Process the 'previous' lower body animation. (This is so we can smoothly transition from 'last' to 'current').
    sg_skm_animation_state_t *lastLowerBodyState = &animationMixer->lastBodyStates[ SKM_BODY_LOWER ];
    { 
        // Get frame.
        SG_SKM_ProcessAnimationStateForTime( model, lastLowerBodyState, extrapolatedTime, &lastLowerBodyOldFrame, &lastLowerBodyCurrentFrame, &lastLowerBodyBackLerp );
        // Lerp the relative frame poses.
        const skm_transform_t *framePose = clgi.SKM_GetBonePosesForFrame( model, lastLowerBodyCurrentFrame );
        const skm_transform_t *oldFramePose = clgi.SKM_GetBonePosesForFrame( model, lastLowerBodyOldFrame );
        
        // Copy in the frame pose.
        memcpy( previousStatePose, framePose, SKM_MAX_BONES * sizeof( iqm_transform_t ) );
    }
    // Process the 'current' lower body animation.
    sg_skm_animation_state_t *currentLowerBodyState = &animationMixer->currentBodyStates[ SKM_BODY_LOWER ];
    {
        // Get frame.
        SG_SKM_ProcessAnimationStateForTime( model, currentLowerBodyState, extrapolatedTime, &currentLowerBodyOldFrame, &currentLowerBodyCurrentFrame, &currentLowerBodyBackLerp );
        const skm_transform_t *framePose = clgi.SKM_GetBonePosesForFrame( model, currentLowerBodyCurrentFrame );
        const skm_transform_t *oldFramePose = clgi.SKM_GetBonePosesForFrame( model, currentLowerBodyOldFrame );
        
        // Copy in the frame pose.
        memcpy( currentStatePose, framePose, SKM_MAX_BONES * sizeof( iqm_transform_t ) );
    }

    //
    // Event State(s):
    //
    int32_t lowerEventOldFrame = 0, lowerEventCurrentFrame = 0;
    int32_t upperEventOldFrame = 0,upperEventCurrentFrame = 0;
    double upperEventBackLerp = 0.0, lowerEventBackLerp = 0.0;

    static skm_transform_t lowerEventStatePose[ SKM_MAX_BONES ];
    static skm_transform_t lastLowerEventStatePose[ SKM_MAX_BONES ];
    // Store last state pase.
    memcpy( lastLowerEventStatePose, lowerEventStatePose, SKM_MAX_BONES * sizeof( skm_transform_t ) );
    static skm_transform_t upperEventStatePose[ SKM_MAX_BONES ];
    static skm_transform_t lastUpperEventStatePose[ SKM_MAX_BONES ];
    // Store last state pase.
    memcpy( lastUpperEventStatePose, upperEventStatePose, SKM_MAX_BONES * sizeof( skm_transform_t ) );

    // "finished" by default:
    bool upperEventFinishedPlaying = true;
    // Process the upper event body state animation:
    sg_skm_animation_state_t *upperEventBodyState = &animationMixer->eventBodyState[ SKM_BODY_UPPER ];
    {
        // Process Animation:
        upperEventFinishedPlaying = SG_SKM_ProcessAnimationStateForTime( model, upperEventBodyState, extrapolatedTime, &upperEventOldFrame, &upperEventCurrentFrame, &upperEventBackLerp );
        if ( !upperEventFinishedPlaying && upperEventBodyState->timeEnd >= extrapolatedTime ) {
            // Acquire frame poses.
            const skm_transform_t *framePose = clgi.SKM_GetBonePosesForFrame( model, upperEventCurrentFrame );
            const skm_transform_t *oldFramePose = clgi.SKM_GetBonePosesForFrame( model, upperEventOldFrame );
            // Recurse blend it in the state poses.
            clgi.SKM_LerpBonePoses( model,
                framePose, oldFramePose,
                1.0 - upperEventBackLerp, upperEventBackLerp,
                upperEventStatePose,
                0, 0
            );
        }
    }

    // "finished" by default:
    bool lowerEventFinishedPlaying = true;
    // Process the upper event body state animation:
    sg_skm_animation_state_t *lowerEventBodyState = &animationMixer->eventBodyState[ SKM_BODY_LOWER ];
    {
        // Process Animation:
        lowerEventFinishedPlaying = SG_SKM_ProcessAnimationStateForTime( model, lowerEventBodyState, extrapolatedTime, &lowerEventOldFrame, &lowerEventCurrentFrame, &lowerEventBackLerp );
        if ( !lowerEventFinishedPlaying && lowerEventBodyState->timeEnd >= extrapolatedTime ) {
            // Acquire frame poses.
            const skm_transform_t *framePose = clgi.SKM_GetBonePosesForFrame( model, lowerEventCurrentFrame );
            const skm_transform_t *oldFramePose = clgi.SKM_GetBonePosesForFrame( model, lowerEventOldFrame );
            // Recurse blend it in the state poses.
            clgi.SKM_LerpBonePoses( model,
                framePose, oldFramePose,
                1.0 - lowerEventBackLerp, lowerEventBackLerp,
                lowerEventStatePose,
                0, 0
            );
        }
    }

    //
    // Base Animation Blend Transitioning:
    // 
    // Calculate the front and backlerp so that the last and current animation can smoothly
    // lerp into one another.
    double scaleFactor = 0;
    double frontLerp = clgi.client->xerpFraction;
    double backLerp = constclamp( ( 1.0 - frontLerp ), 0.0, 1.0 );// clgi.client->xerpFraction;
    if ( lastLowerBodyState->timeEnd >= extrapolatedTime && currentLowerBodyState->timeStart <= lastLowerBodyState->timeEnd ) {
        scaleFactor = CLG_ETPlayer_GetSwitchAnimationScaleFactor( lastLowerBodyState->timeStart, currentLowerBodyState->timeStart + lastLowerBodyState->timeDuration, extrapolatedTime );
        //scaleFactor = CLG_ETPlayer_GetSwitchAnimationScaleFactor( lastLowerBodyState->timeStart, currentLowerBodyState->timeStart + lastLowerBodyState->timeDuration, extrapolatedTime );
        if ( scaleFactor < 1 ) {
            frontLerp = scaleFactor;// 1.0 - constclamp( ( 1.0 / currentLowerBodyState->timeDuration.milliseconds() ) * ( ( 1.0 / lastLowerBodyState->timeDuration.milliseconds() ) * scaleFactor ), 0.0, 1.0 );
            backLerp = constclamp( ( 1.0 - frontLerp ), 0.0, 1.0 );
            //clgi.Print( PRINT_DEVELOPER, "%s: scaleFactor(%f), frontLerp(%f), backLerp(%f)\n", __func__, scaleFactor, frontLerp, backLerp );
        }
    }
    
    // Lerp the last state bone poses right into lastFinalStatePose.
    clgi.SKM_LerpBonePoses( model,
        previousStatePose, lastPreviousStatePose,
        frontLerp, backLerp,
        lastFinalStatePose,
        0, 0
    );
    // Lerp the current state bone poses right into finalStatePose.
    clgi.SKM_LerpBonePoses( model, 
        currentStatePose, lastCurrentStatePose,//currentStatePose, previousStatePose,
        frontLerp, backLerp, 
        finalStatePose, 
        0, 0
    );

    // Use last/final state and lerp between them if we still have a scale factor going.
    if ( scaleFactor < 1 ) {
        if ( lowerEventBodyState->timeEnd >= extrapolatedTime ) {
            clgi.SKM_RecursiveBlendFromBone( lastLowerEventStatePose, currentStatePose, hipsBone, nullptr, 0, 1, 1 );
            clgi.SKM_RecursiveBlendFromBone( lowerEventStatePose, finalStatePose, hipsBone, nullptr, 0, 1, 1 );
        }
        if ( upperEventBodyState->timeEnd >= extrapolatedTime ) {
            clgi.SKM_RecursiveBlendFromBone( lastUpperEventStatePose, currentStatePose, spineBone, nullptr, 0, 1, 1 );
            clgi.SKM_RecursiveBlendFromBone( upperEventStatePose, finalStatePose, spineBone, nullptr, 0, 1, 1 );
        }
        refreshEntity->bonePoses = finalStatePose;
        refreshEntity->lastBonePoses = currentStatePose;
        refreshEntity->rootMotionBoneID = 0;
        refreshEntity->rootMotionFlags = SKM_POSE_TRANSLATE_Z;
        refreshEntity->backlerp = scaleFactor; // clgi.client->xerpFraction;//1.0 - clgi.client->lerpfrac;
    } else {
        if ( lowerEventBodyState->timeEnd >= extrapolatedTime ) {
            clgi.SKM_RecursiveBlendFromBone( lastLowerEventStatePose, lastCurrentStatePose, hipsBone, nullptr, 0, 1, 1 );
            clgi.SKM_RecursiveBlendFromBone( lowerEventStatePose, currentStatePose, hipsBone, nullptr, 0, 1, 1 );
        }
        if ( upperEventBodyState->timeEnd >= extrapolatedTime ) {
            clgi.SKM_RecursiveBlendFromBone( lastUpperEventStatePose, lastCurrentStatePose, spineBone, nullptr, 0, 1, 1 );
            clgi.SKM_RecursiveBlendFromBone( upperEventStatePose, currentStatePose, spineBone, nullptr, 0, 1, 1 );
        }
        refreshEntity->bonePoses = currentStatePose;
        refreshEntity->lastBonePoses = lastCurrentStatePose;
        refreshEntity->rootMotionBoneID = 0;
        refreshEntity->rootMotionFlags = SKM_POSE_TRANSLATE_Z;
        refreshEntity->backlerp = clgi.client->xerpFraction; //1.0 - clgi.client->lerpfrac;
    }
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
            // For being Dead( Gibbed ):
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