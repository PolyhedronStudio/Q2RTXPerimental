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
    // Determine whether the entity now has a skeletal model, and if so, allocate a bone pose cache for it.
    if ( const model_t *entModel = clgi.R_GetModelDataForHandle( clgi.client->model_draw[ newState->modelindex ] ) ) {
        // Make sure it has proper SKM data.
        if ( ( entModel->skmData ) && ( entModel->skmConfig ) ){
            // Allocate bone pose space. ( Use SKM_MAX_BONES instead of entModel->skmConfig->numberOfBones because client models could change. )
            packetEntity->bonePoseCache = clgi.SKM_PoseCache_AcquireCachedMemoryBlock( SKM_MAX_BONES /*entModel->skmConfig->numberOfBones*/ );
        }
    }
}

/**
*   @brief  Determine the entity's active animations.
**/
void CLG_ETPlayer_DetermineBaseAnimations( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *newState ) {
    // Get pmove state.
    player_state_t *playerState = &clgi.client->frame.ps;

    // xySpeed
    const float xySpeed = clgi.client->predictedState.currentPs.xySpeed;
    // xyzSpeed
    const float xyzSpeed = clgi.client->predictedState.currentPs.xyzSpeed;
    // xyZVelocity.
    const Vector3 xyzVelocity = clgi.client->predictedState.currentPs.pmove.velocity;
    const Vector3 xyVelocity = { xyzVelocity.x, xyzVelocity.y, 0.f };
    // xyMove Direction vector.
    const Vector3 xyMoveDirection = QM_Vector3Normalize( xyVelocity );

    // Get angle vectors.
    Vector3 yawAngles = { 0.f, newState->angles[ YAW ], 0.f };
    Vector3 vForward = {}, vRight = {}, vUp = {};
    QM_AngleVectors( yawAngles, &vForward, &vRight, &vUp );
    
    //
    // Movement limit indicators.
    //
    static constexpr double MOVEDIREPSILON = 0.3;
    static constexpr double WALKEPSILON = 0.25;
    static constexpr double RUNEPSILON = 280.0;
    //
    // Move Anim Flags.
    //
    static constexpr int32_t ANIM_MOVE_RIGHT = BIT( 0 );
    static constexpr int32_t ANIM_MOVE_LEFT = BIT( 1 );
    static constexpr int32_t ANIM_MOVE_FORWARD = BIT( 2 );
    static constexpr int32_t ANIM_MOVE_BACKWARD = BIT( 3 );
    static constexpr int32_t ANIM_MOVE_RUN = BIT( 4 );
    static constexpr int32_t ANIM_MOVE_WALK = BIT( 5 );
    static constexpr int32_t ANIM_MOVE_CROUCH = BIT( 6 );
    static constexpr int32_t ANIM_MOVE_IDLE = BIT( 7 );
    
    // Precalculated, set a bit further down the function.
    int32_t moveFlags = 0;
    int32_t rootMotionFlags = 0;
    double frameTime = BASE_FRAMETIME;

    // Determine "Animation Type" based on Jump/Crouch and Speed.
    if ( clgi.client->predictedState.currentPs.pmove.pm_flags & PMF_DUCKED ) {
        moveFlags |= ANIM_MOVE_CROUCH;
    }
    if ( clgi.client->predictedState.currentPs.pmove.pm_flags & PMF_ON_GROUND ) {
        if ( xySpeed > RUNEPSILON ) {
            moveFlags |= ANIM_MOVE_RUN;
        } else if ( xySpeed > WALKEPSILON ) {
            moveFlags |= ANIM_MOVE_WALK;
        } else {
            moveFlags |= ANIM_MOVE_IDLE;
        }
    } 

    // Move directions.
    if ( QM_Vector3DotProduct( xyMoveDirection, vRight ) > MOVEDIREPSILON ) {
        moveFlags |= ANIM_MOVE_RIGHT;
        rootMotionFlags |= SKM_POSE_TRANSLATE_Z;// | SKM_POSE_TRANSLATE_Z;
    } else if ( -QM_Vector3DotProduct( xyMoveDirection, vRight ) > MOVEDIREPSILON ) {
        moveFlags |= ANIM_MOVE_LEFT;
        rootMotionFlags |= SKM_POSE_TRANSLATE_Z;// | SKM_POSE_TRANSLATE_Z;
    }
    if ( QM_Vector3DotProduct( xyMoveDirection, vForward ) > MOVEDIREPSILON ) {
        moveFlags |= ANIM_MOVE_FORWARD;
        rootMotionFlags |= SKM_POSE_TRANSLATE_Z; //SKM_POSE_TRANSLATE_X | SKM_POSE_TRANSLATE_Y;
    } else if ( -QM_Vector3DotProduct( xyMoveDirection, vForward ) > MOVEDIREPSILON ) {
        moveFlags |= ANIM_MOVE_BACKWARD;
        rootMotionFlags |= SKM_POSE_TRANSLATE_Z; //SKM_POSE_TRANSLATE_X | SKM_POSE_TRANSLATE_Y;
    }
    // Generate debug string.
    std::string dbgStrWeapon = "_rifle";
    std::string dbgStr = "idle";
    if ( moveFlags & ANIM_MOVE_IDLE ) {
        dbgStr = "idle";
        if ( moveFlags & ANIM_MOVE_CROUCH ) {
            dbgStr += "_crouch";
        }
    } else {
        if ( moveFlags & ANIM_MOVE_CROUCH ) {
            dbgStr = "crouch";
        } else if ( moveFlags & ANIM_MOVE_RUN ) {
            dbgStr = "run";
        } else if ( moveFlags & ANIM_MOVE_WALK ) {
            // We don't have any walk animations yet, so stick to running, but change frametime.
            //dbgStr += "walk_";
            dbgStr = "walk";
            // = BASE_FRAMETIME * 1.5;
        }
    }

    // Directions:
    if ( ( moveFlags & ANIM_MOVE_CROUCH ) || ( moveFlags & ANIM_MOVE_RUN ) || ( moveFlags & ANIM_MOVE_WALK ) ) {
        if ( moveFlags & ANIM_MOVE_FORWARD ) {
            dbgStr += "_forward";
        } else if ( moveFlags & ANIM_MOVE_BACKWARD ) {
            dbgStr += "_backward";
        }
        if ( moveFlags & ANIM_MOVE_LEFT ) {
            dbgStr += "_left";
        } else if ( moveFlags & ANIM_MOVE_RIGHT ) {
            dbgStr += "_right";
        }
    } else {
        dbgStr = "idle";
        if ( clgi.client->predictedState.currentPs.pmove.pm_flags & PMF_DUCKED ) {
            dbgStr += "_crouch";
        }
    }
    dbgStr += dbgStrWeapon;




    //
    //  See if we can find the matching animation in our SKM data.
    //
    // Get model resource.
    const model_t *model = clgi.R_GetModelDataForHandle( refreshEntity->model );
    // Get skm data.
    const skm_model_t *skmData = model->skmData;
    // Soon to be pointer to the animation.
    const skm_anim_t *skmAnimation = nullptr;
    int32_t skmAnimationID = -1;
    // Find the animation with a matching name.
    if ( skmData && skmData->num_animations ) {
        for ( int32_t i = 0; i < skmData->num_animations; i++ ) {
            // Resort to idle.
            skmAnimationID = 0;
            std::string animName = skmData->animations[ i ].name;
            if ( animName == dbgStr ) {
                skmAnimation = &skmData->animations[ i ];
                skmAnimationID = i;
                break;
            }

        }
    }

    //
    // Apply the animation if it isn't active already.
    //
    if ( skmAnimation && skmAnimationID >= 0 ) {
        // Get the animation state mixer.
        sg_skm_animation_mixer_t *animationMixer = &packetEntity->animationMixer;
        // Set lower body animation.
        sg_skm_animation_state_t *lowerBodyAnimation = &animationMixer->bodyStates[ SKM_BODY_LOWER ];

        //
        // Different animation than we're currently playing, so prepare the switch.
        //
        if ( lowerBodyAnimation->animationID != skmAnimationID ) {

            lowerBodyAnimation->srcStartFrame = skmAnimation->first_frame;
            lowerBodyAnimation->srcEndFrame = skmAnimation->first_frame + skmAnimation->num_frames;

            // Apply new animation data.
            lowerBodyAnimation->previousAnimationID = lowerBodyAnimation->animationID;
            lowerBodyAnimation->animationID = skmAnimationID;
            lowerBodyAnimation->animationStartTime = sg_time_t::from_ms( clgi.client->servertime );
            lowerBodyAnimation->animationEndTime = sg_time_t::from_ms( clgi.client->servertime + ( ( lowerBodyAnimation->srcEndFrame - lowerBodyAnimation->srcStartFrame ) * frameTime ) );
            lowerBodyAnimation->frameTime = frameTime;
            lowerBodyAnimation->isLooping = true;

            lowerBodyAnimation->rootMotionFlags = rootMotionFlags;
            // Apply the rootmotion flags
            //refreshEntity->rootMotionFlags = rootMotionFlags;

            clgi.Print( PRINT_DEVELOPER, "%s\n", dbgStr.c_str() );

        //
        // Same animation, keep on playing.
        //
        } else {

        }
    } else {
        clgi.Print( PRINT_DEVELOPER, "%s\n", dbgStr.c_str() );

        //refreshEntity->rootMotionFlags = 0;
    }
}
/**
*   @brief  Process the entity's active animations.
**/
void CLG_ETPlayer_ProcessAnimations( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *newState ) {
    // Get the animation state mixer.
    sg_skm_animation_mixer_t *animationMixer = &packetEntity->animationMixer;
    // Set lower body animation.
    sg_skm_animation_state_t *lowerBodyAnimation = &animationMixer->bodyStates[ SKM_BODY_LOWER ];

    //
    // Different animation than we're currently playing, so prepare the switch.
    //
    if ( lowerBodyAnimation && lowerBodyAnimation->animationID >= 0 ) {
        // Backup the previously 'current' frame as its last frame.
        lowerBodyAnimation->previousFrame = refreshEntity->frame;

        // Apply rootmotion flags and bone.
        refreshEntity->rootMotionBoneID = 0;
        refreshEntity->rootMotionFlags = lowerBodyAnimation->rootMotionFlags;

        // Old frame.
        const int32_t oldFrame = lowerBodyAnimation->previousFrame;

        // Animation is running:
        //if ( lowerBodyAnimation->previousAnimationID == lowerBodyAnimation->animationID ) {
            // Animation start and end frames.
            const int32_t firstFrame = lowerBodyAnimation->srcStartFrame;
            const int32_t lastFrame = lowerBodyAnimation->srcEndFrame;
            // Calculate the actual current frame for the moment in time of the active animation.
            double lerpFraction = SG_AnimationFrameForTime( &lowerBodyAnimation->currentFrame,
                //sg_time_t::from_ms( clgi.GetRealTime() ), sg_time_t::from_ms( game.viewWeapon.real_time ),
                sg_time_t::from_ms( clgi.client->extrapolatedTime ), lowerBodyAnimation->animationStartTime,
                lowerBodyAnimation->frameTime,
                firstFrame, lastFrame,
                0, lowerBodyAnimation->isLooping
            );
            if ( lerpFraction < 1.0 ) {
                // Apply animation to gun model refresh entity.
                refreshEntity->frame = lowerBodyAnimation->currentFrame;
                refreshEntity->oldframe = ( refreshEntity->frame > firstFrame && refreshEntity->frame <= lastFrame ? refreshEntity->frame - 1 : oldFrame );
                // Enforce lerp of 0.0 to ensure that the first frame does not 'bug out'.
                if ( refreshEntity->frame == firstFrame ) {
                    refreshEntity->backlerp = 0.0;
                    // Enforce lerp of 1.0 if the calculated frame is equal or exceeds the last one.
                } else if ( refreshEntity->frame == lastFrame ) {
                    refreshEntity->backlerp = 1.0;
                    // Otherwise just subtract the resulting lerpFraction.
                } else {
                    refreshEntity->backlerp = 1.0 - lerpFraction;
                }
                // Clamp just to be sure.
                clamp( refreshEntity->backlerp, 0.0, 1.0 );
                // Reached the end of the animation:
            } else {
                // Apply animation to gun model refresh entity.
                refreshEntity->frame = lowerBodyAnimation->currentFrame;
                        refreshEntity->backlerp = clgi.client->xerpFraction;
                        refreshEntity->oldframe = oldFrame;
            }
            //if ( lerpFraction <= 1.0 ) {
            //    // Apply animation to gun model refresh entity.
            //    refreshEntity->frame = lowerBodyAnimation->currentFrame;
            //    refreshEntity->oldframe = ( refreshEntity->frame > firstFrame && refreshEntity->frame <= lastFrame ? refreshEntity->frame - 1 : oldFrame );
            //    // Enforce lerp of 0.0 to ensure that the first frame does not 'bug out'.
            //    if ( refreshEntity->frame == firstFrame ) {
            //        refreshEntity->backlerp = 0.0;
            //        // Enforce lerp of 1.0 if the calculated frame is equal or exceeds the last one.
            //    } else if ( refreshEntity->frame == lastFrame ) {
            //        refreshEntity->backlerp = 1.0;
            //        // Otherwise just subtract the resulting lerpFraction.
            //    } else if ( refreshEntity->frame > firstFrame && refreshEntity->frame <= lastFrame ) {
            //        refreshEntity->backlerp = 1.0 - lerpFraction;
            //    } else {
            //        refreshEntity->backlerp = 1.0 - clgi.client->xerpFraction;
            //    }
            //    // Clamp just to be sure.
            //    clamp( refreshEntity->backlerp, 0.0, 1.0 );
            //// Reached the end of the animation:
            //} else {
            //    if ( refreshEntity->frame > firstFrame && refreshEntity->frame <= lastFrame ) {
            //        // No more lerping.
            //        refreshEntity->backlerp = 1.0;
            //        // Otherwise, oldframe now equals the current(end) frame.
            //        refreshEntity->oldframe = refreshEntity->frame = lastFrame;
            //    } else {
            //        refreshEntity->backlerp = 1.0 - clgi.client->xerpFraction;
            //        refreshEntity->oldframe = oldFrame;
            //    }
            //}

        // Animation is being switched.
        //} else {

        //}
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

        ////
        //// Animation Frame Lerping:
        ////
        //if ( !( refreshEntity->model & 0x80000000 ) && packetEntity->last_frame != packetEntity->current_frame ) {
        //    // Calculate back lerpfraction. (10hz.)
        //    //refreshEntity->backlerp = 1.0f - ( ( clgi.client->time - ( (float)packetEntity->frame_servertime - clgi.client->sv_frametime ) ) / 100.f );
        //    //refreshEntity->backlerp = QM_Clampf( refreshEntity->backlerp, 0.0f, 1.f );
        //    //refreshEntity->frame = packetEntity->current_frame;
        //    //refreshEntity->oldframe = packetEntity->last_frame;

        //    // Calculate back lerpfraction using RealTime. (40hz.)
        //    //refreshEntity->backlerp = 1.0f - ( ( clgi.GetRealTime()  - ( (float)packetEntity->frame_realtime - clgi.client->sv_frametime ) ) / 25.f );
        //    //refreshEntity->backlerp = QM_Clampf( refreshEntity->backlerp, 0.0f, 1.f );
        //    //refreshEntity->frame = packetEntity->current_frame;
        //    //refreshEntity->oldframe = packetEntity->last_frame;

        //    // Calculate back lerpfraction using clgi.client->time. (40hz.)
        //    constexpr int32_t animationHz = BASE_FRAMERATE;
        //    constexpr float animationMs = 1.f / ( animationHz ) * 1000.f;
        //    refreshEntity->backlerp = 1.f - ( ( clgi.client->time - ( (float)packetEntity->frame_servertime - clgi.client->sv_frametime ) ) / animationMs );
        //    refreshEntity->backlerp = QM_Clampf( refreshEntity->backlerp, 0.0f, 1.f );
        //    refreshEntity->frame = packetEntity->current_frame;
        //    refreshEntity->oldframe = packetEntity->last_frame;
        //}

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