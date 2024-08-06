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
*
*   TEMPORARY FOR TESTING:
*
*
*
**/
typedef float mat3x4[12];

static void QuatSlerp( const quat_t from, const quat_t _to, float fraction, quat_t out ) {
    // cos() of angle
    float cosAngle = from[ 0 ] * _to[ 0 ] + from[ 1 ] * _to[ 1 ] + from[ 2 ] * _to[ 2 ] + from[ 3 ] * _to[ 3 ];

    // negative handling is needed for taking shortest path (required for model joints)
    quat_t to;
    if ( cosAngle < 0.0f ) {
        cosAngle = -cosAngle;
        to[ 0 ] = -_to[ 0 ];
        to[ 1 ] = -_to[ 1 ];
        to[ 2 ] = -_to[ 2 ];
        to[ 3 ] = -_to[ 3 ];
    } else {
        QuatCopy( _to, to );
    }

    float backlerp, lerp;
    if ( cosAngle < 0.999999f ) {
        // spherical lerp (slerp)
        const float angle = acosf( cosAngle );
        const float sinAngle = sinf( angle );
        backlerp = sinf( ( 1.0f - fraction ) * angle ) / sinAngle;
        lerp = sinf( fraction * angle ) / sinAngle;
    } else {
        // linear lerp
        backlerp = 1.0f - fraction;
        lerp = fraction;
    }

    out[ 0 ] = from[ 0 ] * backlerp + to[ 0 ] * lerp;
    out[ 1 ] = from[ 1 ] * backlerp + to[ 1 ] * lerp;
    out[ 2 ] = from[ 2 ] * backlerp + to[ 2 ] * lerp;
    out[ 3 ] = from[ 3 ] * backlerp + to[ 3 ] * lerp;
}

// "multiply" 3x4 matrices, these are assumed to be the top 3 rows
// of a 4x4 matrix with the last row = (0 0 0 1)
static void Matrix34Multiply( const mat3x4 &a, const mat3x4 &b, mat3x4 &out ) {
    out[ 0 ] = a[ 0 ] * b[ 0 ] + a[ 1 ] * b[ 4 ] + a[ 2 ] * b[ 8 ];
    out[ 1 ] = a[ 0 ] * b[ 1 ] + a[ 1 ] * b[ 5 ] + a[ 2 ] * b[ 9 ];
    out[ 2 ] = a[ 0 ] * b[ 2 ] + a[ 1 ] * b[ 6 ] + a[ 2 ] * b[ 10 ];
    out[ 3 ] = a[ 0 ] * b[ 3 ] + a[ 1 ] * b[ 7 ] + a[ 2 ] * b[ 11 ] + a[ 3 ];
    out[ 4 ] = a[ 4 ] * b[ 0 ] + a[ 5 ] * b[ 4 ] + a[ 6 ] * b[ 8 ];
    out[ 5 ] = a[ 4 ] * b[ 1 ] + a[ 5 ] * b[ 5 ] + a[ 6 ] * b[ 9 ];
    out[ 6 ] = a[ 4 ] * b[ 2 ] + a[ 5 ] * b[ 6 ] + a[ 6 ] * b[ 10 ];
    out[ 7 ] = a[ 4 ] * b[ 3 ] + a[ 5 ] * b[ 7 ] + a[ 6 ] * b[ 11 ] + a[ 7 ];
    out[ 8 ] = a[ 8 ] * b[ 0 ] + a[ 9 ] * b[ 4 ] + a[ 10 ] * b[ 8 ];
    out[ 9 ] = a[ 8 ] * b[ 1 ] + a[ 9 ] * b[ 5 ] + a[ 10 ] * b[ 9 ];
    out[ 10 ] = a[ 8 ] * b[ 2 ] + a[ 9 ] * b[ 6 ] + a[ 10 ] * b[ 10 ];
    out[ 11 ] = a[ 8 ] * b[ 3 ] + a[ 9 ] * b[ 7 ] + a[ 10 ] * b[ 11 ] + a[ 11 ];
}

static void JointToMatrix( const Quaternion &rot, const Vector3 &scale, const Vector3 &trans, mat3x4 &mat ) {
    float xx = 2.0f * rot[ 0 ] * rot[ 0 ];
    float yy = 2.0f * rot[ 1 ] * rot[ 1 ];
    float zz = 2.0f * rot[ 2 ] * rot[ 2 ];
    float xy = 2.0f * rot[ 0 ] * rot[ 1 ];
    float xz = 2.0f * rot[ 0 ] * rot[ 2 ];
    float yz = 2.0f * rot[ 1 ] * rot[ 2 ];
    float wx = 2.0f * rot[ 3 ] * rot[ 0 ];
    float wy = 2.0f * rot[ 3 ] * rot[ 1 ];
    float wz = 2.0f * rot[ 3 ] * rot[ 2 ];

    mat[ 0 ] = scale[ 0 ] * ( 1.0f - ( yy + zz ) );
    mat[ 1 ] = scale[ 0 ] * ( xy - wz );
    mat[ 2 ] = scale[ 0 ] * ( xz + wy );
    mat[ 3 ] = trans[ 0 ];
    mat[ 4 ] = scale[ 1 ] * ( xy + wz );
    mat[ 5 ] = scale[ 1 ] * ( 1.0f - ( xx + zz ) );
    mat[ 6 ] = scale[ 1 ] * ( yz - wx );
    mat[ 7 ] = trans[ 1 ];
    mat[ 8 ] = scale[ 2 ] * ( xz - wy );
    mat[ 9 ] = scale[ 2 ] * ( yz + wx );
    mat[ 10 ] = scale[ 2 ] * ( 1.0f - ( xx + yy ) );
    mat[ 11 ] = trans[ 2 ];
}



/**
*
*
*
*   TEMPORARY: Skeletal Stuff!
*
*
*
**/
/**
*	@brief	Compute "Local/Model-Space" matrices for the given pose transformations.
**/
void SKM_TransformBonePosesLocalSpace( const skm_model_t *model, const skm_transform_t *relativeBonePose, const skm_bone_controller_t *boneControllers, float *pose_matrices ) {
    // multiply by inverse of bind pose and parent 'pose mat' (bind pose transform matrix)
    const skm_transform_t *relativeJoint = relativeBonePose;
    const int *jointParent = model->jointParents;
    const mat3x4 *invBindMat = (const mat3x4*)model->invBindJoints;
    const mat3x4 *bindJointMats = (const mat3x4 *)model->bindJoints;
    mat3x4 *poseMat = (mat3x4 *)pose_matrices;
    mat3x4 *poseMatrices = (mat3x4 *)pose_matrices;
    for ( uint32_t pose_idx = 0; pose_idx < model->num_poses; pose_idx++, relativeJoint++, jointParent++, invBindMat += 1/*12*/, poseMat += 1/*12*/ ) {
        // Matrices.
        mat3x4 mat1 = { };
        mat3x4 mat2 = { };

        // Default to the relative joint.
        skm_transform_t jointTransform = *relativeJoint;

        // Determine whether we have a bone controller for this pose joint.
        if ( boneControllers != nullptr ) {
            // Iterate controllers.
            for ( int32_t i = 0; i < SKM_MAX_BONE_CONTROLLERS; i++ ) {
                // Get controller ptr.
                const skm_bone_controller_t *boneController = &boneControllers[ i ];
                // Skip if inactive.
                if ( boneController->state != SKM_BONE_CONTROLLER_STATE_ACTIVE ) {
                    continue;
                }
                // If controller is active, and matches the bone number, override the desired bone transform properties:
                if ( pose_idx == boneController->boneNumber || ( boneController->boneNumber == -1 && *jointParent < 0 ) ) {
                    // Rotate:
                    if ( boneController->transformMask & SKM_BONE_CONTROLLER_TRANSFORM_ROTATION ) {
                        Vector4Copy( boneControllers->transform.rotate, jointTransform.rotate );
                    }
                    // Translate:
                    if ( boneController->transformMask & SKM_BONE_CONTROLLER_TRANSFORM_TRANSLATE ) {
                        VectorCopy( boneControllers->transform.translate, jointTransform.translate );
                    }
                    // Scale:
                    if ( boneController->transformMask & SKM_BONE_CONTROLLER_TRANSFORM_SCALE ) {
                        VectorCopy( boneControllers->transform.scale, jointTransform.scale );
                    }
                }
            }
        }

        // Now convert the joint to a 3x4 matrix.
        JointToMatrix( jointTransform.rotate, jointTransform.scale, jointTransform.translate, mat1 );

        // Concatenate if not the root joint.
        if ( *jointParent >= 0 ) {
            Matrix34Multiply( bindJointMats[ ( *jointParent ) /** 12*/ ], mat1, mat2 );
            Matrix34Multiply( mat2, *invBindMat, mat1 );
            Matrix34Multiply( poseMatrices[ ( *jointParent ) /** 12*/ ], mat1, *poseMat );
        } else {
            Matrix34Multiply( mat1, *invBindMat, *poseMat );
        }
    }
}

void SKM_TransformBonePosesWorldSpace( const skm_model_t *model, const skm_transform_t *relativeBonePose, const skm_bone_controller_t *boneControllers, float *pose_matrices ) {
    // First compute local space pose matrices from the relative joints.
    //SKM_TransformBonePosesLocalSpace( model, relativeBonePose, boneControllers, pose_matrices );

    // Calculate the world space pose matrices.
    const mat3x4 *poseMat = (const mat3x4 *)model->bindJoints;
    mat3x4 *outPose = (mat3x4 *)pose_matrices;

    for ( size_t i = 0; i < model->num_poses; i++, poseMat += 1, outPose += 1 ) {
        // It does this...
        mat3x4 inPose;
        memcpy( inPose, outPose, sizeof( mat3x4 ) );
        // To prevent this operation from failing.
        Matrix34Multiply( inPose, *poseMat, *outPose );
    }
}

//void SKM_RecursiveBlendFromBone( const skm_model_t *model, const mat3x4 *addBoneMatrices, mat3x4 *addToBoneMatrices, const skm_bone_node_t *boneNode ) {
//    // If the bone node is invalid, escape.
//    if ( !boneNode || !addBoneMatrices || !addToBoneMatrices ) {
//        // TODO: Warn.
//        return;
//    }
//
//    // Get bone number.
//    const int32_t boneNumber = ( boneNode->number > 0 ? boneNode->number : 0 );
//
//    // Proceed if the bone number is valid and not an excluded bone.
//    const mat3x4 *inBone = addBoneMatrices;
//    mat3x4 *outBone = addToBoneMatrices;
//    if ( boneNumber >= 0 ) {
//        inBone += boneNumber;
//        outBone += boneNumber;
//    }
//
//    mat3x4 inPose;
//    mat3x4 outPose;
//    //memcpy( poseMat, ((const mat3x4*)model->bindJoints)[ boneNumber ], sizeof( mat3x4 ) );
//    //memcpy( inPose, inBone, sizeof( mat3x4 ) );
//    memcpy( inPose, addBoneMatrices[ boneNode->number ], sizeof( mat3x4 ) );
//    memcpy( outPose, addToBoneMatrices[ boneNode->parentNumber ], sizeof( mat3x4 ) );
//    
//    Matrix34Multiply( outPose, inPose, *outBone);
//    //memcpy( addToBoneMatrices[ boneNumber ], addBoneMatrices[boneNumber], sizeof(mat3x4));
//
//
//    // Recursively blend all thise bone node's children.
//    for ( int32_t i = 0; i < boneNode->numberOfChildNodes; i++ ) {
//        const skm_bone_node_t *childBoneNode = boneNode->childBones[ i ];
//        if ( childBoneNode != nullptr && childBoneNode->numberOfChildNodes > 0 ) {
//            SKM_RecursiveBlendFromBone( model, addBoneMatrices, addToBoneMatrices, childBoneNode );
//        }
//    }
//}
/**
*	@brief	Combine 2 poses into one by performing a recursive blend starting from the given boneNode, using the given fraction as "intensity".
*	@param	fraction		When set to 1.0, it blends in the animation at 100% intensity. Take 0.5 for example,
*							and a tpose(frac 0.5)+walk would have its arms half bend.
*	@param	addBonePose		The actual animation that you want to blend in on top of inBonePoses.
*	@param	addToBonePose	A lerped bone pose which we want to blend addBonePoses animation on to.
**/
#if 0
typedef struct skm_exclude_blend_s {
    //! The bone node we're excluding.
    skm_bone_node_t *boneNode;
    //! Excludes the bone node itself from being blend.
    bool blendSelf;
    //! Don't recurse blend this bone's children.
    bool excludeChildren;
    
    float fraction;
} skm_exclude_blend_t;
void SKM_RecursiveBlendFromBone( const skm_transform_t *addBonePoses, skm_transform_t *addToBonePoses, const skm_bone_node_t *boneNode, const skm_exclude_blend_t *excludeNodes, const int32_t numExcludeNodes, const double backLerp, const double fraction ) {
    // If the bone node is invalid, escape.
    if ( !boneNode || !addBonePoses || !addToBonePoses ) {
        // TODO: Warn.
        return;
    }

    // Get bone number.
    const int32_t boneNumber = ( boneNode->number > 0 ? boneNode->number : -1 );

    // Exclude it?
    bool excludeNode = false;
    
    float _fraction = fraction;

    for ( int32_t j = 0; j < numExcludeNodes; j++ ) {
        const skm_exclude_blend_t *excludeNodeBlend = &excludeNodes[ j ];
        if ( boneNumber == excludeNodeBlend->boneNode->number ) {
            if ( !excludeNodeBlend->blendSelf ) {
                excludeNode = true;
            } else {
                _fraction = excludeNodeBlend->fraction;
            }
            break;
        }
    }

    if ( boneNumber != -1 || excludeNode != false ) {
        // Proceed if the bone number is valid and not an excluded bone.
        const skm_transform_t *inBone = addBonePoses;
        skm_transform_t *outBone = addToBonePoses;
        if ( boneNumber >= 0 ) {
            inBone += boneNumber;
            outBone += boneNumber;
        }

        if ( _fraction == 1 ) {
            #if 1
            // Copy bone translation and scale;
            *outBone->translate = *inBone->translate;
            *outBone->scale = *inBone->scale;
            //*outBone->rotate = *inBone->rotate;
            // Slerp the rotation at fraction.	
            QuatSlerp( outBone->rotate, inBone->rotate, _fraction, outBone->rotate );
            //Quaternion newQuat = QM_QuaternionMultiply( Quaternion( outBone->rotate ), Quaternion( inBone->rotate ) );
            //Vector4Copy( newQuat, outBone->rotate );
            #else
            const double frontLerp = 1.0 - _fraction;
            // Lerp the Translation.
            //*outBone->translate = *inBone->translate;
            outBone->translate[ 0 ] = ( outBone->translate[ 0 ] * _fraction + inBone->translate[ 0 ] * frontLerp );
            outBone->translate[ 1 ] = ( outBone->translate[ 1 ] * _fraction + inBone->translate[ 1 ] * frontLerp );
            outBone->translate[ 2 ] = ( outBone->translate[ 2 ] * _fraction + inBone->translate[ 2 ] * frontLerp );
            // Lerp the  Scale.
            //*outBone->scale = *inBone->scale;
            outBone->scale[ 0 ] = outBone->scale[ 0 ] * _fraction + inBone->scale[ 0 ] * frontLerp;
            outBone->scale[ 1 ] = outBone->scale[ 1 ] * _fraction + inBone->scale[ 1 ] * frontLerp;
            outBone->scale[ 2 ] = outBone->scale[ 2 ] * _fraction + inBone->scale[ 2 ] * frontLerp;		// Slerp the rotation at fraction.	

            // Slerp the rotation at fraction.	
            //*outBone->rotate = *inBone->rotate;
            //QuatSlerp( outBone->rotate, inBone->rotate, _fraction, outBone->rotate );
            Quaternion newQuat = QM_QuaternionMultiply( Quaternion( outBone->rotate ), Quaternion( inBone->rotate ) );
            Vector4Copy( newQuat, outBone->rotate );
            #endif
        } else {
            //	WID: Unsure if actually lerping these is favored.
            #if 1
            const double frontLerp = 1.0 - _fraction;
            // Lerp the Translation.
            outBone->translate[ 0 ] = ( outBone->translate[ 0 ] * _fraction + inBone->translate[ 0 ] * frontLerp );
            outBone->translate[ 1 ] = ( outBone->translate[ 1 ] * _fraction + inBone->translate[ 1 ] * frontLerp );
            outBone->translate[ 2 ] = ( outBone->translate[ 2 ] * _fraction + inBone->translate[ 2 ] * frontLerp );
            // Lerp the Scale.
            outBone->scale[ 0 ] = outBone->scale[ 0 ] * _fraction + inBone->scale[ 0 ] * frontLerp;
            outBone->scale[ 1 ] = outBone->scale[ 1 ] * _fraction + inBone->scale[ 1 ] * frontLerp;
            outBone->scale[ 2 ] = outBone->scale[ 2 ] * _fraction + inBone->scale[ 2 ] * frontLerp;

            // Slerp the rotation
            //*outBone->rotate = *inBone->rotate;
            QuatSlerp( outBone->rotate, inBone->rotate, _fraction, outBone->rotate );
            #else
            // Copy bone translation and scale;
            *outBone->translate = *inBone->translate;
            *outBone->scale = *inBone->scale;
            // Slerp rotation by fraction.
            QuatSlerp( outBone->rotate, inBone->rotate, _fraction, outBone->rotate );
            #endif
        }
    }

    // Recursively blend all thise bone node's children.
    for ( int32_t i = 0; i < boneNode->numberOfChildNodes; i++ ) {
        const skm_bone_node_t *childBoneNode = boneNode->childBones[ i ];

        if ( !childBoneNode ) {
            continue;
        }

        bool excludeChildren = false;
        for ( int32_t j = 0; j < numExcludeNodes; j++ ) {
            const skm_exclude_blend_t *excludeNodeBlend = &excludeNodes[ j ];
            if ( excludeNodeBlend->boneNode->number == childBoneNode->number ) {
                if ( excludeNodeBlend->excludeChildren ) {
                    excludeChildren = true;
                } else {
                    _fraction = excludeNodeBlend->fraction;
                }

                break;
            }
        }
        if ( !excludeChildren && childBoneNode != nullptr && childBoneNode->numberOfChildNodes > 0 ) {
            SKM_RecursiveBlendFromBone( addBonePoses, addToBonePoses, childBoneNode, excludeNodes, numExcludeNodes, backLerp, _fraction );
        }
    }
}
#else
void SKM_RecursiveBlendFromBone( const skm_transform_t *addBonePoses, skm_transform_t *addToBonePoses, const skm_bone_node_t *boneNode, const double backLerp, const double fraction ) {
    // If the bone node is invalid, escape.
    if ( !boneNode || !addBonePoses || !addToBonePoses ) {
        // TODO: Warn.
        return;
    }

    // Get bone number.
    const int32_t boneNumber = ( boneNode->number > 0 ? boneNode->number : 0 );

    // Proceed if the bone number is valid and not an excluded bone.
    const skm_transform_t *inBone = addBonePoses;
    skm_transform_t *outBone = addToBonePoses;
    if ( boneNumber >= 0 ) {
        inBone += boneNumber;
        outBone += boneNumber;
    }

    if ( fraction == 1 ) {
        #if 1
        // Copy bone translation and scale;
        *outBone->translate = *inBone->translate;
        *outBone->scale = *inBone->scale;
        //*outBone->rotate = *inBone->rotate;
        // Slerp the rotation at fraction.	
        QuatSlerp( outBone->rotate, inBone->rotate, 1.0, outBone->rotate );
        #else
        const double frontLerp = 1.0 - fraction;
        // Lerp the Translation.
        //*outBone->translate = *inBone->translate;
        outBone->translate[ 0 ] = ( outBone->translate[ 0 ] * backLerp + inBone->translate[ 0 ] * frontLerp );
        outBone->translate[ 1 ] = ( outBone->translate[ 1 ] * backLerp + inBone->translate[ 1 ] * frontLerp );
        outBone->translate[ 2 ] = ( outBone->translate[ 2 ] * backLerp + inBone->translate[ 2 ] * frontLerp );
        // Lerp the  Scale.
        //*outBone->scale = *inBone->scale;
        outBone->scale[ 0 ] = outBone->scale[ 0 ] * backLerp + inBone->scale[ 0 ] * frontLerp;
        outBone->scale[ 1 ] = outBone->scale[ 1 ] * backLerp + inBone->scale[ 1 ] * frontLerp;
        outBone->scale[ 2 ] = outBone->scale[ 2 ] * backLerp + inBone->scale[ 2 ] * frontLerp;		// Slerp the rotation at fraction.	

        // Slerp the rotation at fraction.	
        //*outBone->rotate = *inBone->rotate;
        QuatSlerp( outBone->rotate, inBone->rotate, fraction, outBone->rotate );
        #endif
    } else {
        //	WID: Unsure if actually lerping these is favored.
        #if 1
        const double frontLerp = 1.0 - backLerp;
        // Lerp the Translation.
        outBone->translate[ 0 ] = ( outBone->translate[ 0 ] * backLerp + inBone->translate[ 0 ] * frontLerp );
        outBone->translate[ 1 ] = ( outBone->translate[ 1 ] * backLerp + inBone->translate[ 1 ] * frontLerp );
        outBone->translate[ 2 ] = ( outBone->translate[ 2 ] * backLerp + inBone->translate[ 2 ] * frontLerp );
        // Lerp the Scale.
        outBone->scale[ 0 ] = outBone->scale[ 0 ] * backLerp + inBone->scale[ 0 ] * frontLerp;
        outBone->scale[ 1 ] = outBone->scale[ 1 ] * backLerp + inBone->scale[ 1 ] * frontLerp;
        outBone->scale[ 2 ] = outBone->scale[ 2 ] * backLerp + inBone->scale[ 2 ] * frontLerp;

        // Slerp the rotation
        //*outBone->rotate = *inBone->rotate;
        QuatSlerp( outBone->rotate, inBone->rotate, fraction, outBone->rotate );
        #else
        // Copy bone translation and scale;
        *outBone->translate = *inBone->translate;
        *outBone->scale = *inBone->scale;
        // Slerp rotation by fraction.
        QuatSlerp( outBone->rotate, inBone->rotate, fraction, outBone->rotate );
        #endif
    }

    // Recursively blend all thise bone node's children.
    for ( int32_t i = 0; i < boneNode->numberOfChildNodes; i++ ) {
        const skm_bone_node_t *childBoneNode = boneNode->childBones[ i ];
        if ( childBoneNode != nullptr && childBoneNode->numberOfChildNodes > 0 ) {
            SKM_RecursiveBlendFromBone( addBonePoses, addToBonePoses, childBoneNode, backLerp, fraction );
        }
    }
}
#endif


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
        const sg_time_t startTimer = sg_time_t::from_ms( clgi.client->servertime );
        
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
        const sg_time_t startTimer = sg_time_t::from_ms( clgi.client->servertime );
        // Calculate frameTime based on frameRate.
        const double frameTime = 1000.0 / animationFrameRate;
        // Temporary for setting the animation.
        sg_skm_animation_state_t newAnimationBodyState;
        // We want this to loop for most animations.
        if ( SG_SKM_SetStateAnimation( model, &newAnimationBodyState, lowerAnimationID, startTimer, frameTime, true, false ) ) {
            // However, if the last body state was of a different animation type, we want to continue using its
            // start time so we can ensure that switching directions keeps the feet neatly lerping.
            if ( lastBodyState[ SKM_BODY_LOWER ].animationID != lowerAnimationID ) {
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
        return 0;//0;
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

    //
    // TODO: These need to actually be precached memory blocks stored by packet entity.
    // 
    // Final all combined states pose.
    static skm_transform_t finalStatePose[ SKM_MAX_BONES ];
    static skm_transform_t lastFinalStatePose[ SKM_MAX_BONES ];
    // For the current active animation.
    static skm_transform_t currentStatePose[ SKM_MAX_BONES ];
    static skm_transform_t previousStatePose[ SKM_MAX_BONES ];
    // For the last animation that continues playing for transitioning purposes.
    static skm_transform_t lastCurrentStatePose[ SKM_MAX_BONES ];
    static skm_transform_t lastPreviousStatePose[ SKM_MAX_BONES ];

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
    double upperEventBackLerp = 0.0, lowerEventBackLerp = 0.0;

    //
    // Event Animations.
    // 
    // "finished" by default:
    bool lowerEventFinishedPlaying = true;
    static skm_transform_t lowerEventStatePose[ SKM_MAX_BONES ];
    int32_t lowerEventOldFrame = 0, lowerEventCurrentFrame = 0;
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
                0, SKM_POSE_TRANSLATE_Z
            );
        }
    }
    // "finished" by default:
    bool upperEventFinishedPlaying = true;
    static skm_transform_t upperEventStatePose[ SKM_MAX_BONES ];
    int32_t upperEventOldFrame = 0, upperEventCurrentFrame = 0;
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

            //
            // Spine Controller:
            // 
            // Now reorient torso
            skm_transform_t *hipsTransform = &currentStatePose[ hipsBone->number ];
            skm_transform_t *spineTransform = &upperEventStatePose[ spineBone->number ];

            // Get eulers.
            const Vector3 hipsEuler = QM_QuaternionToEuler( hipsTransform->rotate );
            const Vector3 spineEuler = QM_QuaternionToEuler( spineTransform->rotate );

            // Generate new spine quat.
            Quaternion yawSpineQuat = QM_QuaternionFromEuler( 0, -hipsEuler.y, 0 );

            Quaternion invertedHipQuat = QM_QuaternionMultiply( yawSpineQuat, Quaternion( spineTransform->rotate ) );//QM_QuaternionFromEuler( spineEuler.x, -hipsEuler.y, spineEuler.z );
            Vector4Copy( invertedHipQuat, spineTransform->rotate );
        }
    }


    //
    // Lerping:
    // 
    double frontLerp = clgi.client->xerpFraction;
    double backLerp = constclamp( ( 1.0 - frontLerp ), 0.0, 1.0 );// clgi.client->xerpFraction;
    // Lerp the last state bone poses right into lastFinalStatePose.
    clgi.SKM_LerpBonePoses( model,
        previousStatePose, lastPreviousStatePose,
        frontLerp, backLerp,
        lastFinalStatePose,
        0, SKM_POSE_TRANSLATE_Z
    );
    // Now store it as the last previous state pose.
    memcpy( lastPreviousStatePose, previousStatePose, SKM_MAX_BONES * sizeof( iqm_transform_t ) );
    // Lerp the current state bone poses right into finalStatePose.
    clgi.SKM_LerpBonePoses( model, 
        currentStatePose, lastCurrentStatePose,//currentStatePose, previousStatePose,
        frontLerp, backLerp, 
        finalStatePose, 
        0, SKM_POSE_TRANSLATE_Z
    );
    // Now store it as the last current state pose.
    memcpy( lastCurrentStatePose, currentStatePose, SKM_MAX_BONES * sizeof( iqm_transform_t ) );


    //
    // Old to New Anim Transitioning:
    //
    // Current animation lerped pose.
    skm_transform_t *currentLerpedPose = finalStatePose;
    // Last animation lerped pose.
    const skm_transform_t *lastLerpedPose = lastFinalStatePose;
    // Transition scale.
    double baseScaleFactor = CLG_ETPlayer_GetSwitchAnimationScaleFactor( currentLowerBodyState->timeStart, lastLowerBodyState->timeDuration + currentLowerBodyState->timeStart, extrapolatedTime );
    // Blend old animation into the new one.
    if ( baseScaleFactor < 1 ) {
        clgi.SKM_LerpBonePoses( model,
            currentLerpedPose, lastLerpedPose,
            1.0 - baseScaleFactor, baseScaleFactor,
            currentLerpedPose,
            0, SKM_POSE_TRANSLATE_Z
        );
    }


    //
    // Prepare Refresh Entity:
    // 
    refreshEntity->rootMotionBoneID = 0;
    refreshEntity->rootMotionFlags = SKM_POSE_TRANSLATE_Z;
    refreshEntity->backlerp = clgi.client->xerpFraction;

    // Local model space final representation matrices.
    static mat3x4 refreshBoneMats[ SKM_MAX_BONES ];
    // Local current lower body state local spaces.
    static mat3x4 localCurrentBoneMats[ SKM_MAX_BONES ];
    // Local upper body event state local spaces.
    static mat3x4 localUpperEventBoneMats[ SKM_MAX_BONES ];


    if ( !lowerEventFinishedPlaying && lowerEventBodyState->timeEnd >= extrapolatedTime ) {
        //clgi.SKM_RecursiveBlendFromBone( upperEventStatePose, currentLerpedPose, spineBone, nullptr, 0, clgi.client->xerpFraction, 1 );
        SKM_RecursiveBlendFromBone( lowerEventStatePose, currentLerpedPose, hipsBone, 0, 1 );
    }
    if ( !upperEventFinishedPlaying && upperEventBodyState->timeEnd >= extrapolatedTime ) {
        #if 0
        const skm_exclude_blend_t excludeBoneNodes[8] = {
            {
                hipsBone,
                false,
                false,
                1.0,
            },
            {
                spineBone,
                false,
                false,
                0.5
            },
            {
                spine1Bone,
                false,
                false,
                0.5
            },
            {
                spine2Bone,
                false,
                false,
                0.25
            },
            {
                leftShoulderBone,
                true,
                false,
                1
            },
            {
                rightShoulderBone,
                true,
                false,
                1
            },
            {
                leftUpLegBone,
                false,
                true,
                0.0
            },
            {
                rightUpLegBone,
                false,
                true,
                0.0
            }
        };
        SKM_RecursiveBlendFromBone( upperEventStatePose, currentLerpedPose, hipsBone, excludeBoneNodes, 8, clgi.client->xerpFraction, 1);
        #else
        SKM_RecursiveBlendFromBone( upperEventStatePose, currentLerpedPose, spineBone, 1, 1 );
        #endif
    }

    // Transform.
    SKM_TransformBonePosesLocalSpace( model->skmData, currentLerpedPose, nullptr, (float *)&localCurrentBoneMats[ 0 ] );

    // Copy into refresh bone mats.
    memcpy( refreshBoneMats, localCurrentBoneMats, sizeof( mat3x4 ) * SKM_MAX_BONES );

    // Assign final refresh mats.
    refreshEntity->localSpaceBonePose3x4Matrices = (float *)&refreshBoneMats[ 0 ];
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
            CLG_ETPlayer_ProcessAnimations( packetEntity, refreshEntity, newState );
        } else {
            // Determine the base animation to play.
            CLG_ETPlayer_DetermineBaseAnimations( packetEntity, refreshEntity, newState );
            // Determine the event animation(s) to play.
            CLG_ETPlayer_DetermineEventAnimations( packetEntity, refreshEntity, newState );
            // Don't tilt the model - looks weird.
            refreshEntity->angles[ 0 ] = 0.f;
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