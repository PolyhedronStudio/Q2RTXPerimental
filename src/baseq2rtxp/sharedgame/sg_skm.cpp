/********************************************************************
*
*
*	SharedGame: Skeletal Model Information
* 
*	Skeletal Models operate by .. TODO: explain ..
*
*
********************************************************************/
#include "shared/shared.h"
#include "sg_shared.h"
#include "sg_skm.h"



/**
*
*
*
*   Quaternion Utilities that are currently specific only to Skeletal Model Functions:
*
*
*
**/
/**
*   @brief  
**/
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
/**
*   @brief  Slerp from 4 control points.
**/
static void QuatSSlerp( const quat_t fromA, const quat_t fromB, const quat_t toB, const quat_t toA, const float fraction, quat_t out ) {
    quat_t t1;
    quat_t t2;

    QuatSlerp( fromA, toA, fraction, t1 );
    QuatSlerp( fromB, toB, fraction, t2 );
    QuatSlerp( t1, t2, 2 * fraction * ( 1 - fraction ), out );
}

/**
*   Sets a quaternion to represent the shortest rotation from one
*   vector to another.
**/
const Quaternion QuaternionRotationTo( const Vector3 &a, const Vector3 &b ) {
    Vector3 tmpVec3;
    Vector3 xUnitVec3 = { 1., 0., 0. };
    Vector3 yUnitVec3 = { 1., 0., 0. };

    const float dot = QM_Vector3DotProduct( a, b );
    if ( dot < -0.999999 ) {
        tmpVec3 = QM_Vector3CrossProduct( xUnitVec3, a );
        if ( QM_Vector3Length( tmpVec3 ) < 0.000001 ) {
            tmpVec3 = QM_Vector3CrossProduct( yUnitVec3, a );
        }
        tmpVec3 = QM_Vector3Normalize( tmpVec3 );
        return QM_QuaternionFromAxisAngle( tmpVec3, M_PI );
    } else if ( dot > 0.999999 ) {
        return { 0., 0., 0., 1. };
    } else {
        tmpVec3 = QM_Vector3CrossProduct( a, b );
        return QM_QuaternionNormalize( { tmpVec3.x, tmpVec3.y, tmpVec3.z, 1 + dot } );
    }
}

/**
*   @brief  Will make sure that the Quaternions are within 180 degrees of each other, if not, reverse one instead.
**/
void QM_AlignQuaternions( const Quaternion &in1, const Quaternion &in2, Quaternion &out ) {
    float a = 0.0;
    float b = 0.0;

    // dot product of in1 - in2
    a += ( in1.x - in2.x ) * ( in1.x - in2.x );
    a += ( in1.y - in2.y ) * ( in1.y - in2.y );
    a += ( in1.z - in2.z ) * ( in1.z - in2.z );
    a += ( in1.w - in2.w ) * ( in1.w - in2.w );

    // dot product of in1 + in2
    b += ( in1.x + in2.x ) * ( in1.x + in2.x );
    b += ( in1.y + in2.y ) * ( in1.y + in2.y );
    b += ( in1.z + in2.z ) * ( in1.z + in2.z );
    b += ( in1.w + in2.w ) * ( in1.w + in2.w );

    if ( a > b ) {
        out[ 0 ] = -in2.x;
        out[ 1 ] = -in2.y;
        out[ 2 ] = -in2.z;
        out[ 3 ] = -in2.w;
    } else {
        out[ 0 ] = in2.x;
        out[ 1 ] = in2.y;
        out[ 2 ] = in2.z;
        out[ 3 ] = in2.w;
    }
}

/**
*   @brief
**/
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
/**
*   @brief
**/
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
/**
*   @brief
**/
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
*   @brief  "multiply" 3x4 matrices, these are assumed to be the top 3 rows
*           of a 4x4 matrix with the last row = (0 0 0 1)
**/
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

/**
*   @brief  
**/
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
*	Animations:
*
*
*
**/
/**
*	@brief	Will return a pointer to the model's SKM animation data matching the 'name',
*			as well as set the value of 'animationID' if it is a valid pointer.
**/
const skm_anim_t *SG_SKM_GetAnimationForName( const model_t *model, const char *name, qhandle_t *animationID ) {
	// Get skm data.
	const skm_model_t *skmData = model->skmData;
	// Soon to be pointer to the animation.
	const skm_anim_t *skmAnimation = nullptr;
	// If we got an animationID pointer, default it to -1 (no animation found.)
	if ( animationID ) {
		*animationID = -1;
	}

	// Find the animation with a matching name.
	if ( skmData && skmData->num_animations ) {
		for ( int32_t i = 0; i < skmData->num_animations; i++ ) {
			if ( !Q_strncasecmp( skmData->animations[ i ].name, name, MAX_QPATH ) ) {
				skmAnimation = &skmData->animations[ i ];
				if ( animationID ) {
					*animationID = i;
				}
				break;
			}
		}
	}
	// Return pointer.
	return skmAnimation;
}
/**
*	@brief	Will return a pointer to the model's SKM animation data matching the 'name'.
**/
const skm_anim_t *SG_SKM_GetAnimationForHandle( const model_t *model, const qhandle_t animationID ) {
	// Get skm data.
	const skm_model_t *skmData = model->skmData;
	// Soon to be pointer to the animation.
	const skm_anim_t *skmAnimation = nullptr;
	// Sanity check.
	if ( !skmData ) {
		// TODO: Warn?
		return skmAnimation;
	}
	// Sanity check.
	if ( animationID >= 0 && animationID < skmData->num_animations ) {
		skmAnimation = &skmData->animations[ animationID ];
	}
	// Return.
	return skmAnimation;
}



/**
*
*
*
*	AnimationState Functionalities:
*
*
*
**/
/**
* @brief	Calculates the current frame since the 'start timestamp' for the 'current time'.
*
* @return   The frame and interpolation fraction for current time in an animation started at a given time.
*           When the animation is finished it will return frame -1. Takes looping into account. Looping animations
*           are never finished.
**/
const double SG_AnimationFrameForTime( int32_t *frame, const sg_time_t &currentTime, const sg_time_t &startTimeStamp, const double frameTime, const int32_t firstFrame, const int32_t lastFrame, const int32_t loopingFrames, const bool forceLoop ) {
	// Working loopinframes variable.
	int32_t _loopingFrames = loopingFrames;

	// Always set frame to startFrame if the current time stamp is lower than the animation startTimeStamp.
	if ( currentTime <= startTimeStamp ) {
		*frame = firstFrame;
		return 0.0;
	}
	// If firstframe == last frame, it means we have no animation to begin with since it is already completed.
	// To accommodate this situation, assign firstFrame to frame and return a fraction of 1.0. The animation's completed.
	if ( firstFrame == lastFrame ) {
		*frame = firstFrame;
		return 1.0;
	}

	// Calculate how long the animation is currently running for.
	sg_time_t runningtime = currentTime - startTimeStamp;

	// Calculate the frame's fraction.
	double frameFraction = ( (double)runningtime.milliseconds() / (double)frameTime );
	const int64_t frameCount = std::floor(frameFraction);
	frameFraction -= frameCount;

	// Determine the current frame.
	int32_t currentFrame = firstFrame + frameCount;

	// If the current frame exceeds the last frame:
	if ( currentFrame >= lastFrame ) {
		// If we're force looping, and loopingframes has not been set. Set it back to where it came from(lastframe - firstframe).
		if ( forceLoop && !loopingFrames ) {
			_loopingFrames = lastFrame - firstFrame;
		}

		// If we got looping frames:
		if ( _loopingFrames ) {
			// Calculate current loop start #.
			uint32_t startCount = ( lastFrame - firstFrame ) - _loopingFrames;
			// Calculate the number of loops left to do.
			uint32_t numberOfLoops = ( frameCount - startCount ) / _loopingFrames;
			// Determine current frame.
			currentFrame -= _loopingFrames * numberOfLoops;

			// Special frame fraction handling to play an animation just once.
			if ( _loopingFrames == 1 ) {
				frameFraction = 1.0;
				//currentFrame = lastFrame; // -1
			//	return frameFraction;
			}
		// Animation's finished:
		} else {
			// Set current frame to -1 and get over with it.
			currentFrame = -1;
		}
	}

	// Assign new frame value.
	*frame = currentFrame;

	// Return frame fraction.
	return frameFraction;
}

/**
*	@brief	Processes the animation state by time. Lerping from the last animation's frame to a newly set animation if needed.
*	@note	The resulting oldFrame, currentFrame, and lerpFraction are calculated for the moment in time by this function.
*
*	@return	True if the animation has finished playing. Also returns true in case of an error, since no valid animation is instantly finished playing.
**/
const bool SG_SKM_ProcessAnimationStateForTime( const model_t *model, sg_skm_animation_state_t *animationState, const sg_time_t &time, int32_t *outOldFrame, int32_t *outCurrentFrame, double *outBackLerp ) {
	// Sanity checks.
	if ( !animationState ) {
		SG_DPrintf( "%s: animationState == (nullptr)!\n", __func__ );
		return true;
	}
	if ( !model ) {
		SG_DPrintf( "%s: model == (nullptr)!\n", __func__ );
		return true;
	}
	if ( !model->skmData ) {
		SG_DPrintf( "%s: model->skmData == (nullptr)!\n", __func__ );
		return true;
	}
		
	// Make sure that the animationID is legit.
	const skm_anim_t *skmAnimation = SG_SKM_GetAnimationForHandle( model, animationState->animationID );
	if ( !skmAnimation ) {
		SG_DPrintf( "%s: skmAnimation for handle(#%i) == (nullptr)!\n", __func__, animationState->animationID );
		return true;
	}

	// Animation start and end frames.
	const int32_t skmFirstFrame = skmAnimation->first_frame;
	const int32_t skmLastFrame = skmAnimation->first_frame + skmAnimation->num_frames; // use these instead?

    // The old frame of the animation state, possibly the one to lerp from into a new animation.
    const int32_t oldFrame = QM_ClampInt32( animationState->currentFrame, skmFirstFrame, skmLastFrame );

    // Backup the previously 'current' frame as its last frame.
    animationState->previousFrame = oldFrame;

	// Calculate the actual current frame for the moment in time of the active animation.
	const double lerpFraction = SG_AnimationFrameForTime( &animationState->currentFrame,
		time, animationState->timeStart /*- sg_time_t::from_ms( BASE_FRAMETIME )*/,
		// Use the one provided by the animation state instead of the skmData, since we may want to play at a different speed.
		animationState->frameTime,
		// Process animation from first to last frame.
		skmFirstFrame, skmLastFrame,
		// If the animation is not looping, play the animation only once.
		( !animationState->isLooping ? 1 : 0 ), animationState->isLooping
	);

	// Whether we are finished playing this animation.
	bool isPlaybackDone = false;

	// Apply animation to gun model refresh entity.
	if ( animationState->currentFrame == -1 ) {
        *outCurrentFrame = animationState->previousFrame;
		isPlaybackDone = true;
	} else {
		*outCurrentFrame = animationState->currentFrame;
	}

    *outOldFrame = ( animationState->currentFrame > skmFirstFrame && animationState->currentFrame <= skmLastFrame ? animationState->currentFrame - 1 : oldFrame /*animationState->currentFrame */);

    // No backlerp.
	if ( *outCurrentFrame == skmFirstFrame ) {
		*outBackLerp = 0.0;
	// Enforce lerp of 1.0 if the calculated frame is equal or exceeds the last one.
	} else if ( *outCurrentFrame == skmLastFrame ) {
		// Full backlerp.
		*outBackLerp = 1.0;
        // Done playing.
		isPlaybackDone = true;
	// Otherwise just subtract the resulting lerpFraction.
	} else {
		*outBackLerp = 1.0 - lerpFraction;
	}

	// Return whether finished playing or not.
	return isPlaybackDone;
}

/**
*	@brief	Internal support routine: Sets the necessary data needed from skmAnimation for the animationState.
**/
static void SG_SKM_AnimationStateFromAnimation( const skm_anim_t *skmAnimation, sg_skm_animation_state_t *animationState, const int32_t animationID, const sg_time_t &startTime, const double frameTime, const bool forceLoop ) {
	// Store source frame data for easy access later on.
	animationState->srcStartFrame = skmAnimation->first_frame;
	animationState->srcEndFrame = skmAnimation->first_frame + skmAnimation->num_frames;

	// Apply new animation data.
	animationState->animationID = animationID;
	animationState->frameTime = frameTime;
	#if 1
	animationState->timeStart = startTime;
	animationState->timeDuration = sg_time_t::from_ms( ( animationState->srcEndFrame - animationState->srcStartFrame ) * animationState->frameTime );
	animationState->timeEnd = ( startTime + animationState->timeDuration );
	#else
	animationState->animationStartTime = sg_time_t::from_ms( clgi.client->servertime );
	animationState->animationEndTime = sg_time_t::from_ms( clgi.client->servertime + ( ( animationState->srcEndFrame - animationState->srcStartFrame ) * animationState->frameTime ) );
	#endif
	animationState->isLooping = ( skmAnimation->flags & IQM_LOOP ? true : ( forceLoop == true ? true : false ) );
}

/**
*	@brief	Will set the animation matching 'name' for the animation state.
*			Will force set the aniamtion, if desired.
**/
const bool SG_SKM_SetStateAnimation( const model_t *model, sg_skm_animation_state_t *animationState, const char *animationName, const sg_time_t &startTime, const double frameTime, const bool forceLoop, const bool forceSet ) {
	// Sanity checks.
	if ( !animationState ) {
		SG_DPrintf( "%s: animationState == (nullptr)!\n", __func__ );
		return false;
	}
	if ( !model ) {
		SG_DPrintf( "%s: model == (nullptr)!\n", __func__ );
		return false;
	}
	if ( !model->skmData ) {
		SG_DPrintf( "%s: model->skmData == (nullptr)!\n", __func__ );
		return false;
	}

	// Make sure that the animationID is legit.
	qhandle_t animationID = -1;
	const skm_anim_t *skmAnimation = SG_SKM_GetAnimationForName( model, animationName, &animationID );
	if ( !skmAnimation ) {
		SG_DPrintf( "%s: skmAnimation for name(#%s) == (nullptr)!\n", __func__, animationName );
		return false;
	}

	// Apply the animation either if it is not active yet, or is forcefully set.
	if ( animationState->animationID != animationID || forceSet ) {
		// Apply SKM Animation data to Animation State.
		SG_SKM_AnimationStateFromAnimation( skmAnimation,
			animationState,
			animationID,
			startTime,
			frameTime,
			( skmAnimation->flags & IQM_LOOP ? true : ( forceLoop == true ? true : false ) ) 
		);

		// Animation State was set.
		return true;
	}

	// Already running this animation, thus not set.
	return false;
}

/**
*	@brief	Will set the animation matching 'animationID' of the animation state.
*			Will force set the aniamtion, if desired.
**/
const bool SG_SKM_SetStateAnimation( const model_t *model, sg_skm_animation_state_t *animationState, const qhandle_t animationID, const sg_time_t &startTime, const double frameTime, const bool forceLoop, const bool forceSet ) {
	// Sanity checks.
	if ( !animationState ) {
		SG_DPrintf( "%s: animationState == (nullptr)!\n", __func__ );
		return false;
	}
	if ( !model ) {
		SG_DPrintf( "%s: model == (nullptr)!\n", __func__ );
		return false;
	}
	if ( !model->skmData ) {
		SG_DPrintf( "%s: model->skmData == (nullptr)!\n", __func__ );
		return false;
	}

	// Make sure that the animationID is legit.
	const skm_anim_t *skmAnimation = SG_SKM_GetAnimationForHandle( model, animationID );
	if ( !skmAnimation ) {
		SG_DPrintf( "%s: skmAnimation for handle(#%i) == (nullptr)!\n", __func__, animationID );
		return false;
	}

	// Apply the animation either if it is not active yet, or is forcefully set.
	if ( animationState->animationID != animationID || forceSet ) {
		// Apply SKM Animation data to Animation State.
		SG_SKM_AnimationStateFromAnimation( skmAnimation,
			animationState,
			animationID,
			startTime,
			frameTime,
			( skmAnimation->flags & IQM_LOOP ? true : ( forceLoop == true ? true : false ) )
		);

		// Animation State was set.
		return true;
	}

	// Already running this animation, thus not set.
	return false;
}



/**
*
*
*
*   BonePoses Space Transforming:
*
*
*
**/
/**
*	@brief	Compute "Local/Model-Space" matrices for the given pose transformations.
**/
void SKM_TransformBonePosesLocalSpace( const skm_model_t *model, const skm_transform_t *relativeBonePose, float *pose_matrices ) {
    // multiply by inverse of bind pose and parent 'pose mat' (bind pose transform matrix)
    const skm_transform_t *relativeJoint = relativeBonePose;
    const int *jointParent = model->jointParents;
    const mat3x4 *invBindMat = (const mat3x4 *)model->invBindJoints;
    const mat3x4 *bindJointMats = (const mat3x4 *)model->bindJoints;
    mat3x4 *poseMat = (mat3x4 *)pose_matrices;
    mat3x4 *poseMatrices = (mat3x4 *)pose_matrices;
    for ( uint32_t pose_idx = 0; pose_idx < model->num_poses; pose_idx++, relativeJoint++, jointParent++, invBindMat += 1/*12*/, poseMat += 1/*12*/ ) {
        // Matrices.
        mat3x4 mat1 = { };
        mat3x4 mat2 = { };

        // Now convert the joint to a 3x4 matrix.
        JointToMatrix( relativeJoint->rotate, relativeJoint->scale, relativeJoint->translate, mat1 );

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

/**
*   @brief  
**/
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



/**
*
*
*
*   Blending/Layering BonePoses:
*
*
*
**/
/**
*   @brief  Will recursively blend(lerp by lerpfracs/slerp by fraction) the addBonePoses to addToBonePoses starting from the boneNode.
**/
void SKM_RecursiveBlendFromBone( const skm_transform_t *addBonePoses, const skm_transform_t *addToBonePoses, skm_transform_t *outBonePoses, const skm_bone_node_t *boneNode, const double backLerp, const double fraction ) {
    // If the bone node is invalid, escape.
    if ( !boneNode || !addBonePoses || !addToBonePoses || !outBonePoses ) {
        // TODO: Warn.
        return;
    }

    // Get bone number.
    const int32_t boneNumber = ( boneNode->number > 0 ? boneNode->number : 0 );

    // Proceed if the bone number is valid and not an excluded bone.
    const skm_transform_t *addBone = addBonePoses;
    const skm_transform_t *addToBone = addToBonePoses;
    skm_transform_t *outBone = outBonePoses;

    if ( boneNumber >= 0 ) {
        addBone += boneNumber;
        addToBone += boneNumber;
        outBone += boneNumber;
    }

    if ( fraction == 1 ) {
        #if 1
        // Copy bone translation and scale;
        *outBone->translate = *addBone->translate;
        *outBone->scale = *addBone->scale;
        //*outBone->rotate = *inBone->rotate;
        // Slerp the rotation at fraction.	
        quat_t t1;
        quat_t t2;

        const double frontLerp = 1.0 - backLerp;

        QuatSlerp( addBone->rotate, addToBone->rotate, fraction, t1 );
        QuatSlerp( addToBone->rotate, addBone->rotate, fraction, t2 );
        QuatSlerp( t2, t1, 2 * fraction * ( 1 - fraction ), outBone->rotate );

        #else
        const double frontLerp = 1.0 - fraction;
        // Lerp the Translation.
        //*outBone->translate = *inBone->translate;
        outBone->translate[ 0 ] = ( addToBone->translate[ 0 ] * backLerp + addBone->translate[ 0 ] * frontLerp );
        outBone->translate[ 1 ] = ( addToBone->translate[ 1 ] * backLerp + addBone->translate[ 1 ] * frontLerp );
        outBone->translate[ 2 ] = ( addToBone->translate[ 2 ] * backLerp + addBone->translate[ 2 ] * frontLerp );
        // Lerp the  Scale.
        //*outBone->scale = *inBone->scale;
        outBone->scale[ 0 ] = addToBone->scale[ 0 ] * backLerp + addBone->scale[ 0 ] * frontLerp;
        outBone->scale[ 1 ] = addToBone->scale[ 1 ] * backLerp + addBone->scale[ 1 ] * frontLerp;
        outBone->scale[ 2 ] = addToBone->scale[ 2 ] * backLerp + addBone->scale[ 2 ] * frontLerp;		// Slerp the rotation at fraction.	

        // Slerp the rotation at fraction.	
        //*outBone->rotate = *inBone->rotate;
        QuatSSlerp( addBone->rotate, addToBone->rotate, addBone->rotate, addToBone->rotate, fraction, outBone->rotate );
        //QuatSlerp( inBone->rotate, outBone->rotate, fraction, outBone->rotate );
        #endif
    } else {
        //	WID: Unsure if actually lerping these is favored.
        #if 1
        const double frontLerp = 1.0 - backLerp;
        // Lerp the Translation.
        //*outBone->translate = *inBone->translate;
        outBone->translate[ 0 ] = ( addToBone->translate[ 0 ] * backLerp + addBone->translate[ 0 ] * frontLerp );
        outBone->translate[ 1 ] = ( addToBone->translate[ 1 ] * backLerp + addBone->translate[ 1 ] * frontLerp );
        outBone->translate[ 2 ] = ( addToBone->translate[ 2 ] * backLerp + addBone->translate[ 2 ] * frontLerp );
        // Lerp the  Scale.
        //*outBone->scale = *inBone->scale;
        outBone->scale[ 0 ] = addToBone->scale[ 0 ] * backLerp + addBone->scale[ 0 ] * frontLerp;
        outBone->scale[ 1 ] = addToBone->scale[ 1 ] * backLerp + addBone->scale[ 1 ] * frontLerp;
        outBone->scale[ 2 ] = addToBone->scale[ 2 ] * backLerp + addBone->scale[ 2 ] * frontLerp;		// Slerp the rotation at fraction.	

        // Slerp the rotation at fraction.	
        //QuatSSlerp( addBone->rotate, addBone->rotate, addToBone->rotate, addToBone->rotate, fraction, outBone->rotate );
        quat_t t1;
        quat_t t2;

        QuatSlerp( addBone->rotate, addToBone->rotate, fraction, t1 );
        QuatSlerp( addToBone->rotate, addBone->rotate, fraction, t2 );
        QuatSlerp( t2, t1, 2 * fraction * ( 1 - fraction ), outBone->rotate );

        #else
        // Copy bone translation and scale;
        *outBone->translate = *addBone->translate;
        *outBone->scale = *addBone->scale;
        //*outBone->rotate = *inBone->rotate;
        // Slerp the rotation at fraction.	
        QuatSlerp( addToBone->rotate, addBone->rotate, fraction, outBone->rotate );
        //QuatSlerp( inBone->rotate, outBone->rotate, fraction, outBone->rotate );
        #endif
    }

    // Recursively blend all thise bone node's children.
    for ( int32_t i = 0; i < boneNode->numberOfChildNodes; i++ ) {
        const skm_bone_node_t *childBoneNode = boneNode->childBones[ i ];
        if ( childBoneNode != nullptr && childBoneNode->numberOfChildNodes > 0 ) {
            SKM_RecursiveBlendFromBone( addBonePoses, addToBonePoses, outBonePoses, childBoneNode, backLerp, fraction );
        }
    }
}



/**
*
*
*
*   Lerping BonePoses:
*
*
*
**/
/**
*	@brief	Lerped pose transformations between frameBonePoses and oldFrameBonePoses.
*			'outBonePose' must have enough room for model->num_poses
**/
void SKM_LerpBonePoses( const model_t *model, const skm_transform_t *frameBonePoses, const skm_transform_t *oldFrameBonePoses, const float frontLerp, const float backLerp, skm_transform_t *outBonePose, const int32_t rootMotionBoneID, const int32_t rootMotionAxisFlags ) {
    // Sanity check.
    if ( !model || !model->skmData ) {
        return;
    }

    // Get IQM Data.
    skm_model_t *skmData = model->skmData;

    // Keep bone ID sane.
    int32_t boundRootMotionBoneID = ( rootMotionBoneID >= 0 ? rootMotionBoneID % (int32_t)skmData->num_joints : -1 );
    // Fetch first joint.
    skm_transform_t *relativeJoint = outBonePose;

    // Copy the animation frame pos.
    if ( frameBonePoses == oldFrameBonePoses ) {
        const skm_transform_t *pose = frameBonePoses;
        for ( uint32_t pose_idx = 0; pose_idx < skmData->num_poses; pose_idx++, pose++, relativeJoint++ ) {
            #if 1
            if ( rootMotionAxisFlags != SKM_POSE_TRANSLATE_ALL && pose_idx == boundRootMotionBoneID ) {
                // Translate the selected axises.
                if ( !( rootMotionAxisFlags & SKM_POSE_TRANSLATE_X ) ) {
                    relativeJoint->translate[ 0 ] = 0;
                } else {
                    relativeJoint->translate[ 0 ] = pose->translate[ 0 ];
                }
                if ( !( rootMotionAxisFlags & SKM_POSE_TRANSLATE_Y ) ) {
                    relativeJoint->translate[ 1 ] = 0;
                } else {
                    relativeJoint->translate[ 1 ] = pose->translate[ 1 ];
                }
                if ( !( rootMotionAxisFlags & SKM_POSE_TRANSLATE_Z ) ) {
                    relativeJoint->translate[ 2 ] = 0;
                } else {
                    relativeJoint->translate[ 2 ] = pose->translate[ 2 ];
                }
                // Copy in scale as per usual.
                VectorCopy( pose->scale, relativeJoint->scale );
                // Copy quat rotation as usual.
                QuatCopy( pose->rotate, relativeJoint->rotate );
                continue;
            }
            #endif

            VectorCopy( pose->translate, relativeJoint->translate );
            QuatCopy( pose->rotate, relativeJoint->rotate );
            VectorCopy( pose->scale, relativeJoint->scale );
        }
        // Lerp the animation frame pose.
    } else {
        const skm_transform_t *pose = frameBonePoses;
        const skm_transform_t *oldPose = oldFrameBonePoses;
        for ( uint32_t pose_idx = 0; pose_idx < skmData->num_poses; pose_idx++, oldPose++, pose++, relativeJoint++ ) {
            #if 1
            if ( rootMotionAxisFlags != SKM_POSE_TRANSLATE_ALL && pose_idx == boundRootMotionBoneID ) {
                // Translate the selected axises.
                if ( !( rootMotionAxisFlags & SKM_POSE_TRANSLATE_X ) ) {
                    relativeJoint->translate[ 0 ] = 0;
                } else {
                    relativeJoint->translate[ 0 ] = oldPose->translate[ 0 ] * backLerp + pose->translate[ 0 ] * frontLerp; //relativeJoint->translate[ 1 ] = 0; //relativeJoint->translate[ 0 ] = 0;
                }
                if ( !( rootMotionAxisFlags & SKM_POSE_TRANSLATE_Y ) ) {
                    relativeJoint->translate[ 1 ] = 0;
                } else {
                    relativeJoint->translate[ 1 ] = oldPose->translate[ 1 ] * backLerp + pose->translate[ 1 ] * frontLerp; //relativeJoint->translate[ 1 ] = 0;
                }
                if ( !( rootMotionAxisFlags & SKM_POSE_TRANSLATE_Z ) ) {
                    relativeJoint->translate[ 2 ] = 0;
                } else {
                    relativeJoint->translate[ 2 ] = oldPose->translate[ 2 ] * backLerp + pose->translate[ 2 ] * frontLerp;
                }

                // Scale Lerp as usual.
                relativeJoint->scale[ 0 ] = oldPose->scale[ 0 ] * backLerp + pose->scale[ 0 ] * frontLerp;
                relativeJoint->scale[ 1 ] = oldPose->scale[ 1 ] * backLerp + pose->scale[ 1 ] * frontLerp;
                relativeJoint->scale[ 2 ] = oldPose->scale[ 2 ] * backLerp + pose->scale[ 2 ] * frontLerp;
                // Quat Slerp as usual.
                QuatSlerp( oldPose->rotate, pose->rotate, frontLerp, relativeJoint->rotate );
                continue;
            }
            #endif

            relativeJoint->translate[ 0 ] = oldPose->translate[ 0 ] * backLerp + pose->translate[ 0 ] * frontLerp;
            relativeJoint->translate[ 1 ] = oldPose->translate[ 1 ] * backLerp + pose->translate[ 1 ] * frontLerp;
            relativeJoint->translate[ 2 ] = oldPose->translate[ 2 ] * backLerp + pose->translate[ 2 ] * frontLerp;

            relativeJoint->scale[ 0 ] = oldPose->scale[ 0 ] * backLerp + pose->scale[ 0 ] * frontLerp;
            relativeJoint->scale[ 1 ] = oldPose->scale[ 1 ] * backLerp + pose->scale[ 1 ] * frontLerp;
            relativeJoint->scale[ 2 ] = oldPose->scale[ 2 ] * backLerp + pose->scale[ 2 ] * frontLerp;

            QuatSlerp( oldPose->rotate, pose->rotate, frontLerp, relativeJoint->rotate );
        }
    }
}

/**
*	@brief	Lerped pose transformations between frameBonePoses and oldFrameBonePoses.
*			'outBonePose' must have enough room for model->num_poses
**/
void SKM_LerpRangeBonePoses( const model_t *model, const skm_transform_t *frameBonePoses, const skm_transform_t *oldFrameBonePoses, const int32_t rangeStart, const int32_t rangeSize, const float frontLerp, const float backLerp, skm_transform_t *outBonePose, const int32_t rootMotionBoneID, const int32_t rootMotionAxisFlags ) {
    // Sanity check.
    if ( !model || !model->skmData ) {
        return;
    }

    // Get IQM Data.
    skm_model_t *skmData = model->skmData;

    // Keep bone ID sane.
    int32_t boundRootMotionBoneID = ( rootMotionBoneID >= 0 ? rootMotionBoneID % (int32_t)skmData->num_joints : -1 );
    // Fetch first joint.
    skm_transform_t *relativeJoint = outBonePose;

    // Ensure the ranges are within bounds.
    const int32_t _rangeStart = QM_ClampInt32( rangeStart, 0, skmData->num_poses );
    const int32_t _rangeSize = QM_ClampInt32( rangeSize, 0, skmData->num_poses - _rangeStart );

    // Copy the animation frame pos.
    if ( frameBonePoses == oldFrameBonePoses ) {
        const skm_transform_t *pose = frameBonePoses + _rangeStart;
        for ( uint32_t pose_idx = 0; pose_idx < _rangeSize; pose_idx++, pose++, relativeJoint++ ) {
            #if 1
            if ( rootMotionAxisFlags != SKM_POSE_TRANSLATE_ALL && pose_idx == boundRootMotionBoneID ) {
                // Translate the selected axises.
                if ( !( rootMotionAxisFlags & SKM_POSE_TRANSLATE_X ) ) {
                    relativeJoint->translate[ 0 ] = 0;
                } else {
                    relativeJoint->translate[ 0 ] = pose->translate[ 0 ];
                }
                if ( !( rootMotionAxisFlags & SKM_POSE_TRANSLATE_Y ) ) {
                    relativeJoint->translate[ 1 ] = 0;
                } else {
                    relativeJoint->translate[ 1 ] = pose->translate[ 1 ];
                }
                if ( !( rootMotionAxisFlags & SKM_POSE_TRANSLATE_Z ) ) {
                    relativeJoint->translate[ 2 ] = 0;
                } else {
                    relativeJoint->translate[ 2 ] = pose->translate[ 2 ];
                }
                // Copy in scale as per usual.
                VectorCopy( pose->scale, relativeJoint->scale );
                // Copy quat rotation as usual.
                QuatCopy( pose->rotate, relativeJoint->rotate );
                continue;
            }
            #endif

            VectorCopy( pose->translate, relativeJoint->translate );
            QuatCopy( pose->rotate, relativeJoint->rotate );
            VectorCopy( pose->scale, relativeJoint->scale );
        }
        // Lerp the animation frame pose.
    } else {
        const skm_transform_t *pose = frameBonePoses + _rangeStart;
        const skm_transform_t *oldPose = oldFrameBonePoses + _rangeStart;
        for ( uint32_t pose_idx = 0; pose_idx < _rangeSize; pose_idx++, oldPose++, pose++, relativeJoint++ ) {
            #if 1
            if ( rootMotionAxisFlags != SKM_POSE_TRANSLATE_ALL && pose_idx == boundRootMotionBoneID ) {
                // Translate the selected axises.
                if ( !( rootMotionAxisFlags & SKM_POSE_TRANSLATE_X ) ) {
                    relativeJoint->translate[ 0 ] = 0;
                } else {
                    relativeJoint->translate[ 0 ] = oldPose->translate[ 0 ] * backLerp + pose->translate[ 0 ] * frontLerp; //relativeJoint->translate[ 1 ] = 0; //relativeJoint->translate[ 0 ] = 0;
                }
                if ( !( rootMotionAxisFlags & SKM_POSE_TRANSLATE_Y ) ) {
                    relativeJoint->translate[ 1 ] = 0;
                } else {
                    relativeJoint->translate[ 1 ] = oldPose->translate[ 1 ] * backLerp + pose->translate[ 1 ] * frontLerp; //relativeJoint->translate[ 1 ] = 0;
                }
                if ( !( rootMotionAxisFlags & SKM_POSE_TRANSLATE_Z ) ) {
                    relativeJoint->translate[ 2 ] = 0;
                } else {
                    relativeJoint->translate[ 2 ] = oldPose->translate[ 2 ] * backLerp + pose->translate[ 2 ] * frontLerp;
                }

                // Scale Lerp as usual.
                relativeJoint->scale[ 0 ] = oldPose->scale[ 0 ] * backLerp + pose->scale[ 0 ] * frontLerp;
                relativeJoint->scale[ 1 ] = oldPose->scale[ 1 ] * backLerp + pose->scale[ 1 ] * frontLerp;
                relativeJoint->scale[ 2 ] = oldPose->scale[ 2 ] * backLerp + pose->scale[ 2 ] * frontLerp;
                // Quat Slerp as usual.
                QuatSlerp( oldPose->rotate, pose->rotate, frontLerp, relativeJoint->rotate );
                continue;
            }
            #endif

            relativeJoint->translate[ 0 ] = oldPose->translate[ 0 ] * backLerp + pose->translate[ 0 ] * frontLerp;
            relativeJoint->translate[ 1 ] = oldPose->translate[ 1 ] * backLerp + pose->translate[ 1 ] * frontLerp;
            relativeJoint->translate[ 2 ] = oldPose->translate[ 2 ] * backLerp + pose->translate[ 2 ] * frontLerp;

            relativeJoint->scale[ 0 ] = oldPose->scale[ 0 ] * backLerp + pose->scale[ 0 ] * frontLerp;
            relativeJoint->scale[ 1 ] = oldPose->scale[ 1 ] * backLerp + pose->scale[ 1 ] * frontLerp;
            relativeJoint->scale[ 2 ] = oldPose->scale[ 2 ] * backLerp + pose->scale[ 2 ] * frontLerp;

            QuatSlerp( oldPose->rotate, pose->rotate, frontLerp, relativeJoint->rotate );
        }
    }
}



/**
*
*
*
*   Skeletal Model Bone Controllers:
*
*
*
**/
/**
*   @brief  Activates the bone controller and (re-)initializes its initial state as well as its imer.
**/
void SKM_BoneController_Activate( skm_bone_controller_t *boneController, const skm_bone_node_t *boneNode, const skm_bone_controller_target_t &target, const skm_transform_t *initialTransform, const skm_transform_t *currentTransform, const int32_t transformMask, const sg_time_t &timeDuration, const sg_time_t &timeActivated ) {
    // Sanity check.
    if ( !boneController || !boneNode ) {
        // TODO: WARN
        return;
    }
    // TODO: Ensure boneNumber is within model range?
    // if ( boneNode->number >= 0 && ... ) {
    //  return;
    // }

    // Activate.
    boneController->boneNumber = boneNode->number;

    boneController->state = SKM_BONE_CONTROLLER_STATE_ACTIVE;
    boneController->transformMask = transformMask;

    boneController->timeActivated = timeActivated.milliseconds();
    boneController->slerpDuration = timeDuration.milliseconds();
    boneController->timeEnd = boneController->timeActivated + boneController->slerpDuration;

    // Apply 'initial' base transform.
    if ( initialTransform ) {
        boneController->baseTransform = *initialTransform;
    }
    //if ( currentTransform ) {
    //    boneController->currentTransform = *currentTransform;
    //}
    boneController->target = target;
}

/**
*   @brief  Processes and applies the bone controllers to the pose, for the current time.
*   @note   The pose is expected to be untempered with in any way. 
*           This means that it should contain purely relative blended joints sourced by lerping and recursive blending.
**/
void SKM_BoneController_ApplyToPoseForTime( skm_bone_controller_t *boneControllers, const int32_t numBoneControllers, const sg_time_t &currentTime, skm_transform_t *inOutBonePoses ) {
    // Sanity check.
    if ( !boneControllers || !numBoneControllers || !inOutBonePoses ) {
        // TODO: WARN
        return;
    }

    // Iterate controllers.
    for ( int32_t i = 0; i < numBoneControllers; i++ ) {
        // Get bone controller.
        skm_bone_controller_t *boneController = &boneControllers[ i ];

        // If inactive, continue.
        if ( boneController[ i ].state == SKM_BONE_CONTROLLER_STATE_INACTIVE ) {
            continue;
        }

        // TODO: Clamp bone number to model num_poses?
        const int32_t boneNumber = boneController->boneNumber;

        // Determine bone control (S)Lerp controlFactor based on the time changed.
        const sg_time_t timeEnd = sg_time_t::from_ms( boneController->timeEnd );

        // However if the current time is lesser than the time it is meant to end.. Process it.
        double boneControlFactor = 1.0;
        if ( currentTime <= timeEnd ) {
            // Get the scale fraction derived from duration.
            const double scaleFraction = 1.0 / boneController->slerpDuration;
            // Calculate the (S)Lerp controlFactor.
            boneControlFactor = 1.0 - ( ( timeEnd - currentTime ).milliseconds() * scaleFraction );
        }

        /**
        *   Rotation:
        **/
        if ( boneController->transformMask & SKM_BONE_CONTROLLER_TRANSFORM_ROTATION ) {
            // The desired target Yaw.
            const double targetYaw = boneController->target.rotation.targetYaw;

            // Calculate the final target destination quaternion relative to the source bone pose Quaternion.
            Quaternion targetQuat = QuatRotateY( inOutBonePoses[ boneController->boneNumber ].rotate, DEG2RAD( targetYaw ) );
            // Make sure to -180/+180 align normalize the quaternions to prevent any odd rotations from happening. (Edge cases.)
            QM_AlignQuaternions( inOutBonePoses[ boneController->boneNumber ].rotate, targetQuat, targetQuat );

            // 'Control' the BonePose's rotation Quaternion by Slerping for 'boneControlFactor' to the target Quaternion.
            Quaternion outQuat = QM_QuaternionSlerp( boneController->currentTransform.rotate, targetQuat, ( boneControlFactor < 1 ? boneControlFactor : 1 ) );
            // Store a copy of the current actual 'Controller' Quaternion for later re-use as a source to start Slerping from.
            Vector4Copy( outQuat, boneController->currentTransform.rotate );
            // Copy the 'controller' Quaternion into the BonePose's Rotation Quaternion.
            Vector4Copy( outQuat, inOutBonePoses[ boneController->boneNumber ].rotate );
        }
    }
}