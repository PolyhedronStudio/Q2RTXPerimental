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

static inline Quaternion QuatRotateX( const Quaternion &a, const double rad ) {
    const double halfRad = rad * 0.5;

    const float ax = a[ 0 ],
        ay = a[ 1 ],
        az = a[ 2 ],
        aw = a[ 3 ];
    const float bx = sinf( halfRad ),
        bw = cosf( halfRad );

    return {
        ax * bw + aw * bx,
        ay * bw + az * bx,
        az * bw - ay * bx,
        aw * bw - ax * bx,
    };

}

static inline const Quaternion QuatRotateY( const Quaternion &a, const double rad ) {
    const double halfRad = rad * 0.5;

    const float ax = a[ 0 ],
        ay = a[ 1 ],
        az = a[ 2 ],
        aw = a[ 3 ];
    const float by = sinf( halfRad ),
        bw = cosf( halfRad );

    return {
        ax * bw - az * by,
        ay * bw + aw * by,
        az * bw + ax * by,
        aw * bw - ay * by
    };
}

static inline const Quaternion QuatRotateZ( const Quaternion &a, const double rad ) {
    const double halfRad = rad * 0.5;

    const float ax = a[ 0 ],
        ay = a[ 1 ],
        az = a[ 2 ],
        aw = a[ 3 ];
    const float bz = sinf( halfRad ),
        bw = cosf( halfRad );

    return {
        ax * bw + ay * bz,
        ay * bw - ax * bz,
        az * bw + aw * bz,
        aw * bw - az * bz
    };
}

/**
 * Sets a quat from the given angle and rotation axis,
 * then returns it.
 *
 * @param {quat} out the receiving quaternion
 * @param {vec3} axis the axis around which to rotate
 * @param {Number} rad the angle in radians
 * @returns {quat} out
 **/
const Quaternion QuatSetAxisAngle( const Vector3 &axis, float rad ) {
    
    rad = rad * 0.5;
    float s = sinf( rad );
    return {
        s * axis[ 0 ],
        s * axis[ 1 ],
        s * axis[ 2 ],
        cosf( rad )
    };
}

/*
* AngleNormalize180
*
* returns angle normalized to the range [-180 < angle <= 180]
*/
float QM_Angle_Normalize180( const float angle ) {
    float _angle = fmod( ( angle ), 360.0f );
    if ( _angle > 180.0 ) {
        _angle -= 360.0;
    }
    return _angle;
}

/*
* AngleDelta
*
* returns the normalized delta from angle1 to angle2
*/
float QM_AngleDelta( const float angle1, const float angle2 ) {
    return QM_Angle_Normalize180( angle1 - angle2 );
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
void SKM_TransformBonePosesLocalSpace( const skm_model_t *model, const skm_transform_t *relativeBonePose, const skm_bone_controller_t *boneControllers, const int32_t numBoneControllers, float *pose_matrices ) {
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
            for ( int32_t i = 0; i < numBoneControllers/*SKM_MAX_BONE_CONTROLLERS*/; i++ ) {
                // Get controller ptr.
                const skm_bone_controller_t *boneController = &boneControllers[ i ];
                // Skip if inactive.
                if ( boneController->state != SKM_BONE_CONTROLLER_STATE_ACTIVE ) {
                    continue;
                }
                // If controller is active, and matches the bone number, override the desired bone transform properties:
                if ( pose_idx == boneController->boneNumber /*|| ( boneController->boneNumber == -1 && *jointParent < 0 )*/ ) {
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
        //QuatSlerp( inBone->rotate, outBone->rotate, 1.0, outBone->rotate );
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
        //QuatSlerp( inBone->rotate, outBone->rotate, fraction, outBone->rotate );
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
        //QuatSlerp( inBone->rotate, outBone->rotate, fraction, outBone->rotate );
        #else
        // Copy bone translation and scale;
        *outBone->translate = *inBone->translate;
        *outBone->scale = *inBone->scale;
        // Slerp rotation by fraction.
        QuatSlerp( outBone->rotate, inBone->rotate, fraction, outBone->rotate );
        //QuatSlerp( inBone->rotate, outBone->rotate, fraction, outBone->rotate );
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

        //const sg_time_t startTimer = lastBodyState[ SKM_BODY_LOWER ].timeStart - sg_time_t::from_ms( clgi.client->extrapolatedTime );
        
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

/**
*   @brief  Calculates the 'scaleFactor' of an animation switch, for use with the fraction and lerp values between two animation poses. 
**/
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

/**
*   @brief  
*   @return A boolean of whether the animation state is still activeplay playing, or not.
*           ( !!! Does not per se mean, backLerp > 0 ).
*   @note   poseBuffer is expected to be SKM_MAX_BONES in size.
*   @note   The model and its skmData member are expected to be valid. Make sure to check these before calling this function.
**/
static const bool CLG_ETPlayer_GenerateAnimationStatePoseForTime( const model_t *model, const sg_time_t &time, const int32_t rootMotionAxisFlags, const skm_bone_node_t *boneNode, sg_skm_animation_state_t *animationState, skm_transform_t *poseBuffer ) {
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
        return false;
    }
    // If for whatever reason animation state is out of bounds frame wise...
    if ( ( animationStateOldFrame < 0 || animationStateOldFrame > model->skmData->num_frames ) ) {
        return false;
    }

    // Get pose frames for time..
    const skm_transform_t *framePose = clgi.SKM_GetBonePosesForFrame( model, animationStateCurrentFrame );
    const skm_transform_t *oldFramePose = clgi.SKM_GetBonePosesForFrame( model, animationStateOldFrame );

    // If the lerp is > 0, it means we have a legitimate framePose and oldFramePose to work with.
    if ( animationStateBackLerp > 0 && framePose && oldFramePose ) {
        const double frontLerp = 1.0 - animationStateBackLerp;
        const double backLerp = animationStateBackLerp;
        // Lerp the final pose.
        clgi.SKM_LerpBonePoses( model,
            framePose, oldFramePose,
            frontLerp, backLerp,
            poseBuffer,
            ( boneNode != nullptr ? boneNode->number : 0 ), rootMotionAxisFlags
        );
    // If we don't, just copy in the retreived first frame pose data instead.
    } else if ( framePose ) {
        memcpy( poseBuffer, framePose, SKM_MAX_BONES * sizeof( iqm_transform_t ) );
    }

    // !!! Does not per se mean, backLerp > 0
    return isPlaying;
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
    *   Time:
    **/
    // The frame's Server Time.
    const sg_time_t serverTime = sg_time_t::from_ms( clgi.client->servertime );
    // The Time we're at.
    const sg_time_t extrapolatedTime = sg_time_t::from_ms( clgi.client->extrapolatedTime );

    // FrontLerp Fraction (extrapolated ahead of Server Time).
    const double frontLerp = clgi.client->xerpFraction;
    // BackLerp Fraction.
    double backLerp = constclamp( ( 1.0 - frontLerp ), 0.0, 1.0 );// clgi.client->xerpFraction;

    /**
    *   Determine Yaw movement:
    **/
    player_state_t *playerState = &clgi.client->predictedState.currentPs;
    player_state_t *oldPlayerState = &clgi.client->predictedState.lastPs;
    // Determine new wish yaw.
    static float wishYaw = 0;
    static sg_time_t timeDirChanged = level.time;
    float adjustFactor = 1;
    if ( playerState->animation.moveDirection != oldPlayerState->animation.moveDirection ) {
        const int32_t moveDirection = playerState->animation.moveDirection;
        if ( ( moveDirection & PM_MOVEDIRECTION_FORWARD ) && ( moveDirection & PM_MOVEDIRECTION_LEFT ) ) {
            wishYaw = 45;
            timeDirChanged = extrapolatedTime;
        } else if ( ( moveDirection & PM_MOVEDIRECTION_FORWARD ) && ( moveDirection & PM_MOVEDIRECTION_RIGHT ) ) {
            wishYaw = -45;
            timeDirChanged = extrapolatedTime;
        } else if ( ( moveDirection & PM_MOVEDIRECTION_BACKWARD ) && ( moveDirection & PM_MOVEDIRECTION_LEFT ) ) {
            wishYaw = -45;
            timeDirChanged = extrapolatedTime;
        } else if ( ( moveDirection & PM_MOVEDIRECTION_BACKWARD ) && ( moveDirection & PM_MOVEDIRECTION_RIGHT ) ) {
            wishYaw = 45;
            timeDirChanged = extrapolatedTime;
        } else {
            wishYaw = 0;
            timeDirChanged = extrapolatedTime;
        }
    }
    
    // Setup the time of change for lerping.
    const sg_time_t timeDirChangeEnd = timeDirChanged + 150_ms;
    if ( timeDirChangeEnd >= extrapolatedTime ) {
        double factor = 1.0 / 150.0;
        adjustFactor = 1.0 - ( ( timeDirChangeEnd - extrapolatedTime ).milliseconds() * factor );
    } else {
        adjustFactor = 1;
    }


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
    // WID: TODO: !!! These need to actually be precached memory blocks stored by packet entities ??
    // Last Animation, and Current Animation State Pose Buffers.
    static skm_transform_t finalStatePose[ SKM_MAX_BONES ] = {};
    static skm_transform_t lastFinalStatePose[ SKM_MAX_BONES ] = {};
    // Transition scale.
    const double baseScaleFactor = CLG_ETPlayer_GetSwitchAnimationScaleFactor( currentLowerBodyState->timeStart, lastLowerBodyState->timeDuration + currentLowerBodyState->timeStart, extrapolatedTime );
    //if ( baseScaleFactor < 1 ) {
        // Process the 'LAST' lower body animation. (This is so we can smoothly transition from 'last' to 'current').
        CLG_ETPlayer_GenerateAnimationStatePoseForTime( model, extrapolatedTime, SKM_POSE_TRANSLATE_ALL, nullptr, lastLowerBodyState, lastFinalStatePose );
    //}
    // Process the 'CURRENT' lower body animation.
    CLG_ETPlayer_GenerateAnimationStatePoseForTime( model, extrapolatedTime, SKM_POSE_TRANSLATE_ALL, nullptr, currentLowerBodyState, finalStatePose );
    /**
    *   'Events' Animation States Pose Lerping:
    **/
    // Lerped frame for time poses of each active event.
    static skm_transform_t lowerEventStatePose[ SKM_MAX_BONES ] = {};
    static skm_transform_t upperEventStatePose[ SKM_MAX_BONES ] = {};
    // These events are, "finished" by default:
    bool lowerEventFinishedPlaying = true;
    bool upperEventFinishedPlaying = true;
    // Root Motion Axis Flags:
    const int32_t lowerEventRootMotionAxisFlags = SKM_POSE_TRANSLATE_Z; // Z only by default.
    const int32_t upperEventRootMotionAxisFlags = SKM_POSE_TRANSLATE_ALL; // Z only by default.
    // Process the lower event body state animation IF still within valid time range:
    if ( lowerEventBodyState->timeEnd >= extrapolatedTime ) {
        lowerEventFinishedPlaying = CLG_ETPlayer_GenerateAnimationStatePoseForTime( model, extrapolatedTime, lowerEventRootMotionAxisFlags, nullptr, lowerEventBodyState, lowerEventStatePose );
    }
    // Process the upper event body state animation IF still within valid time range:
    if ( upperEventBodyState->timeEnd >= extrapolatedTime ) {
        upperEventFinishedPlaying = CLG_ETPlayer_GenerateAnimationStatePoseForTime( model, extrapolatedTime, upperEventRootMotionAxisFlags, nullptr, upperEventBodyState, upperEventStatePose );
    }


    /**
    *   'Base' Blend Animation Transition over time, from the 'LAST' to 'CURRENT' lerped poses.
    **/
    // Current animation lerped pose.
    skm_transform_t *currentLerpedPose = finalStatePose;
    // Last animation lerped pose.
    skm_transform_t *lastLerpedPose = lastFinalStatePose;
    // Transition scale.
    //double baseScaleFactor = CLG_ETPlayer_GetSwitchAnimationScaleFactor( currentLowerBodyState->timeStart, lastLowerBodyState->timeDuration + currentLowerBodyState->timeStart, extrapolatedTime );
    // Blend old animation into the new one.
    if ( baseScaleFactor < 1 ) {
        SKM_RecursiveBlendFromBone( lastLerpedPose, currentLerpedPose, hipsBone, baseScaleFactor, baseScaleFactor );
    } else {
        #if 0
        // Just lerp them.
        clgi.SKM_LerpBonePoses( model,
            currentLerpedPose, lastLerpedPose,
            frontLerp, backLerp,
            currentLerpedPose,
            0, SKM_POSE_TRANSLATE_ALL
        );
        #endif
    }
    /**
    *   'Event' Animation Override Layering:
    **/
    // If playing, within a valid time range: Override the whole skeleton with the the lower event state pose.
    if ( !lowerEventFinishedPlaying && lowerEventBodyState->timeEnd >= extrapolatedTime ) {
        //memcpy( currentLerpedPose, lowerEventStatePose, SKM_MAX_BONES ); // This is faster if we override from the hips.
        SKM_RecursiveBlendFromBone( lowerEventStatePose, currentLerpedPose, hipsBone, 1, 1 ); // Slower path..
    }
    // If playing, the upper event state overrides only the spine bone and all its child bones.
    if ( !upperEventFinishedPlaying && upperEventBodyState->timeEnd >= extrapolatedTime ) {
        SKM_RecursiveBlendFromBone( upperEventStatePose, currentLerpedPose, spineBone, 1, 1 );
    }


    /**
    *   Bone Controlling:
    **/
    // Now reorient torso
    skm_transform_t *hipsTransform = &finalStatePose[ hipsBone->number ];
    skm_transform_t *spineTransform = &finalStatePose[ spineBone->number ];
    skm_transform_t *spine1Transform = &finalStatePose[ spine1Bone->number ];
    skm_transform_t *spine2Transform = &finalStatePose[ spine2Bone->number ];

    // Rotate(Slerp) the 'Hips Bone' to the desired wish yaw.
    const Vector3 hipsEuler = QM_QuaternionToEuler( finalStatePose[ hipsBone->number ].rotate );
    Quaternion newHipsQuat = QuatRotateY( hipsTransform->rotate, DEG2RAD( wishYaw ) );
    newHipsQuat = QM_QuaternionSlerp( currentLerpedPose[ hipsBone->number ].rotate, newHipsQuat, ( adjustFactor < 1 ? adjustFactor : 1 ) );
    // Copy in the rotations to the bone controller.
    Vector4Copy( newHipsQuat, currentLerpedPose[ hipsBone->number ].rotate );

    const Vector3 spineEuler = QM_QuaternionToEuler( spineTransform->rotate );
    Quaternion newSpineQuat = QuatRotateY( spineTransform->rotate, -( DEG2RAD( wishYaw ) ) );
    newSpineQuat = QM_QuaternionSlerp( spineTransform->rotate, newSpineQuat, ( adjustFactor < 1 ? adjustFactor : 1 ) );
    // Copy in the rotations to the bone controller.
    Vector4Copy( newSpineQuat, currentLerpedPose[ spineBone->number ].rotate );

    const Vector3 spine1Euler = QM_QuaternionToEuler( spine1Transform->rotate );
    Quaternion newSpine1Quat = QuatRotateY( spine1Transform->rotate, ( DEG2RAD( wishYaw / 3 ) ) );
    newSpine1Quat = QM_QuaternionSlerp( spine1Transform->rotate, newSpine1Quat, ( adjustFactor < 1 ? adjustFactor : 1 ) );
    // Copy in the rotations to the bone controller.
    Vector4Copy( newSpine1Quat, currentLerpedPose[ spine1Bone->number ].rotate );

    const Vector3 spine2Euler = QM_QuaternionToEuler( spine2Transform->rotate );
    Quaternion newSpine2Quat = QuatRotateY( spine2Transform->rotate, ( DEG2RAD( wishYaw / 3 ) ) );
    newSpine2Quat = QM_QuaternionSlerp( spine2Transform->rotate, newSpine2Quat, ( adjustFactor < 1 ? adjustFactor : 1 ) );
    // Copy in the rotations to the bone controller.
    Vector4Copy( newSpine2Quat, currentLerpedPose[ spine2Bone->number ].rotate );

    /**
    *   Prepare the RefreshEntity by generating the local model space matrices for rendering.
    **/
    // TODO: THIS NEEDS TO BE UNIQUE FOR EACH PACKETENTITY! OTHERWISE IT'LL OVERWRITE SAME MEMORY BEFORE IT IS RENDERED...
    // Local model space final representation matrices.
    static mat3x4 refreshBoneMats[ SKM_MAX_BONES ];
    // Transform.
    SKM_TransformBonePosesLocalSpace( model->skmData, currentLerpedPose, nullptr, 0, (float *)&refreshBoneMats[ 0 ] );
    // Assign final refresh mats.
    refreshEntity->localSpaceBonePose3x4Matrices = (float *)&refreshBoneMats[ 0 ];
    refreshEntity->rootMotionBoneID = 0;
    refreshEntity->rootMotionFlags = SKM_POSE_TRANSLATE_ALL;
    refreshEntity->backlerp = clgi.client->xerpFraction;
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