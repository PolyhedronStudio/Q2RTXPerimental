/********************************************************************
*
*
*	Skeletal Model Functions
* 
*
********************************************************************/
#include "shared/shared.h"
#include "shared/util_list.h"

#include "refresh/shared_types.h"

#include "common/cmd.h"
#include "common/common.h"
#include "common/cvar.h"
#include "common/files.h"
#include "common/math.h"
#include "common/skeletalmodels/cm_skm.h"
#include "common/skeletalmodels/cm_skm_configuration.h"

#include "system/hunk.h"



//!
//! Shamelessly taken from refresh/models.h
//! 
#ifndef MOD_Malloc
#define MOD_Malloc(size)    Hunk_TryAlloc(&model->hunk, size)
#endif
#ifndef CHECK
#define CHECK(x)    if (!(x)) { ret = Q_ERR(ENOMEM); goto fail; }
#endif



//! Uncomment to enable debug output in case of invalid model data.
#define _DEBUG_PRINT_SKM_INVALID_MODEL_DATA

//! Uncomment to enable debug developer output of rootmotion data per frame, for each animation.
//#define _DEBUG_PRINT_SKM_ROOTMOTION_DATA


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
static void Matrix34Multiply( const float *a, const float *b, float *out ) {
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

static void JointToMatrix( const quat_t rot, const vec3_t scale, const vec3_t trans, float *mat ) {
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
*	Skeletal Model Common:
*
*
**/
/**
*   @brief  Returns a pointer to the first bone that has a matching name.
**/
skm_bone_node_t *SKM_GetBoneByName( model_t *model, const char *boneName ) {
    // Make sure model is valid.
    if ( !model || !model->skmData || !model->skmConfig ) {
        #ifdef _DEBUG_PRINT_SKM_INVALID_MODEL_DATA
        Com_LPrintf( PRINT_DEVELOPER, "%s: Invalid model data encountered( !model || !model->skmData || !model->skmConfig )!\n", __func__ );
        #endif
        return nullptr;
    }
    // Iterate, and if the name matches, return pointer to node.
    for ( int32_t i = 0; i < model->skmConfig->numberOfBones; i++ ) {
        skm_bone_node_t *boneNode = &model->skmConfig->boneArray[ i ];
        if ( boneNode && boneNode->name[ 0 ] != '\0' && !strcmp( boneName, boneNode->name ) ) {
            return boneNode;
        }
    }
    // No node found.
    return nullptr;
}
/**
*   @brief  Returns a pointer to the first bone that has a matching number.
**/
skm_bone_node_t *SKM_GetBoneByNumber( model_t *model, const int32_t boneNumber ) {
    // Make sure model is valid.
    if ( !model || !model->skmData || !model->skmConfig ) {
        #ifdef _DEBUG_PRINT_SKM_INVALID_MODEL_DATA
        Com_LPrintf( PRINT_DEVELOPER, "%s: Invalid model data encountered( !model || !model->skmData || !model->skmConfig )!\n", __func__ );
        #endif
        return nullptr;
    }
    // Root Bone.
    if ( boneNumber == 0 ) {
        return model->skmConfig->boneTree;
    }
    // Iterate, and if the number matches, return pointer to node.
    for ( int32_t i = 0; i < model->skmConfig->numberOfBones; i++ ) {
        skm_bone_node_t *boneNode = &model->skmConfig->boneArray[ i ];
        if ( boneNode && boneNode->number == boneNumber ) {
            return boneNode;
        }
    }
    // No node found.
    return nullptr;
}



/**
*
* 
*
*   Skeletal Model Root Motion Bone System:
*
* 
*
**/
/**
*	@brief	Support method for calculating the exact translation of the root bone, in between frame poses.
**/
const float SKM_CalculateFramePoseTranslation( skm_model_t *iqmModel, const int32_t frame, const int32_t oldFrame, const int32_t rootBoneID, const uint32_t axisFlags, Vector3 *translation ) {
	// frame == oldFrame Path:
	if ( oldFrame == frame || frame < 0 || oldFrame < 0 ) {
		// No translation took place.
		*translation = QM_Vector3Zero();
		// No distance was travered.
		return 0.f;
		// frame != oldFrame Path:
	} else {
		// Pose to determine the translation and distance travered from.
		const iqm_transform_t *pose = &iqmModel->poses[ frame * iqmModel->num_poses ] + rootBoneID;
		const iqm_transform_t *oldPose = &iqmModel->poses[ oldFrame * iqmModel->num_poses ] + rootBoneID;

		// Get their translation vector.
		const Vector3 poseTranslate = pose->translate;
		const Vector3 oldPoseTranslate = oldPose->translate;
		// Determine the actual translation:
		*translation = poseTranslate - oldPoseTranslate;
		// If we got flags passed in, zero out translation for either of the flags that is missing.
		if ( axisFlags ) {
			if ( !( axisFlags & SKM_POSE_TRANSLATE_X ) ) {
				(*translation).x = 0;
			}
			if ( !( axisFlags & SKM_POSE_TRANSLATE_Y ) ) {
				(*translation).y = 0;
			}
			if ( !( axisFlags & SKM_POSE_TRANSLATE_Z ) ) {
				(*translation ).z = 0;
			}
		}
		// Determine and return the distance between the two translation vectors.
		return QM_Vector3Length( *translation );
	}

	// No translation was found.
	Com_LPrintf( PRINT_DEVELOPER, "%s: No translation found!\n", __func__ );
	// No distance traversed.
	return 0.f;
}


/**
*	@brief
**/
const skm_rootmotion_set_t *SKM_GenerateRootMotionSet( model_t *model, const int32_t rootBoneID, const int32_t axisFlags ) {
	// For CHECK( )
	int ret = 0;

	// The actual current set we're working on.
	skm_rootmotion_set_t *rootMotionSet = nullptr;
	// Name of the configured root bone.
	const char *rootBoneName = nullptr;

	// Ensure the model is valid.
	if ( !model || !model->skmData || !model->skmConfig ) {
		// Debug Log.
		Com_LPrintf( PRINT_DEVELOPER, "%s: Invalid model data( !model || !model->skmData || !model->skmConfig )!\n", __func__ );
		// Return empty set.
		return {};
	}


	// IQM Data Pointer.
	skm_model_t *skmData = model->skmData;
	// SKM Config Pointer.
	skm_config_t *skmConfig = model->skmConfig;

	//
	// Allocate memory for all root motion sets.
	//
	// Allocate space for the child bones.
	CHECK( skmConfig->rootMotion.motions = (skm_rootmotion_t**)( MOD_Malloc( sizeof( skm_rootmotion_t * ) * skmData->num_animations ) ) );
	// Ensure it is zeroed out.
	memset( skmConfig->rootMotion.motions, 0, sizeof( skm_rootmotion_t * ) * skmData->num_animations );

	// This data needs to actually be coming from some configuration script or API of sorts.
	//
	rootBoneName = ( ( rootBoneID >= 0 && rootBoneID < skmData->num_joints ) ? &skmData->jointNames[ rootBoneID % skmData->num_joints ] : "Invalid RootBoneID" );
	
	// Iterate over all animations.
	for ( int32_t i = 0; i < skmData->num_animations; i++ ) {
		// Access the IQM animation.
		const skm_anim_t *skmAnimation = &skmData->animations[ i ];


		// Allocate space for the translations as well as the distances traversed by each animation frame.
		// Allocate space for the child bones.
		CHECK( skmConfig->rootMotion.motions[ i ] = (skm_rootmotion_t *)( MOD_Malloc( sizeof( skm_rootmotion_t ) ) ) );
		// Get the pointer to the rootmotion set.
		skm_rootmotion_t *rootMotion = skmConfig->rootMotion.motions[ i ];
		// Ensure it is zeroed out.
		memset( rootMotion, 0, sizeof( skm_rootmotion_t* ) );
		
		CHECK( rootMotion->translations = (Vector3**)( MOD_Malloc( sizeof( Vector3 * ) * skmAnimation->num_frames ) ) );
		// Ensure it is zeroed out.
		memset( rootMotion->translations, 0, sizeof( Vector3 *) * skmAnimation->num_frames );

		// Distances
		CHECK( rootMotion->distances = (float **)( MOD_Malloc( sizeof( float *) * skmAnimation->num_frames ) ) );
		// Ensure it is zeroed out.
		memset( rootMotion->distances, 0, sizeof( float *) * skmAnimation->num_frames );


		// Prepare the set for filling.
		rootMotion->firstFrameIndex = (int32_t)skmAnimation->first_frame;
		rootMotion->lastFrameIndex = (int32_t)( skmAnimation->first_frame + skmAnimation->num_frames );
		rootMotion->frameCount = (int32_t)skmAnimation->num_frames;
		
		// Let us not forget copying in its name.
		Q_strlcpy( rootMotion->name, skmAnimation->name, sizeof( skmAnimation->name ) );

		#ifdef _DEBUG_PRINT_SKM_ROOTMOTION_DATA
			// Debug print:
			Com_LPrintf( PRINT_DEVELOPER, "%s:---------------------------------------------------\n", __func__ );
		#endif
		// This vector is cleared upon each new animation. It serves as a purpose to accumulate the total distance
		// traversed up till the end animation. This in turn is used to calculate the distance traversed from the
		// last to the first frame.
		Vector3 accumulatedTranslation = QM_Vector3Zero();
		float accumulatedDistance = 0.f;

		// Last calculated translation and distance.
		Vector3 translation = {};
		float distance = 0;

		// Iterate over all its frames.
		for ( int32_t j = 1; j <= skmAnimation->num_frames; j++ ) {
			// Index.
			int32_t frameIndex = j - 1;
			// Distances
			CHECK( rootMotion->translations[ frameIndex ] = (Vector3 *)( MOD_Malloc( sizeof( Vector3 ) ) ) );
			// Ensure it is zeroed out.
			memset( rootMotion->translations[ frameIndex ], 0, sizeof( Vector3 ) );
			// Distances
			CHECK( rootMotion->distances[ frameIndex ] = ( float* )( MOD_Malloc( sizeof( float ) ) ) );
			// Ensure it is zeroed out.
			memset( rootMotion->distances[ frameIndex ], 0, sizeof( float ) );

			// First get the actual array frame offset.
			const int32_t unboundFrame = ( skmAnimation->first_frame + j );
			const int32_t unboundOldFrame = ( skmAnimation->first_frame + j ) - 1;

			// Ensure that the frame is safely in bounds for use with our poses data array.
			const int32_t boundFrame = skmData->num_frames ? unboundFrame % (int32_t)skmData->num_frames : 0;
			const int32_t boundOldFrame = skmData->num_frames ? unboundOldFrame % (int32_t)skmData->num_frames : 0;

			// When we've reached the end of this animation, only push back the last results.
			if ( j == skmAnimation->num_frames ) {
				// Push back the translation and traversed distance of the last frame..
				( *rootMotion->translations[ frameIndex ] ) = translation;
				( *rootMotion->distances[ frameIndex ] ) = distance;
				// Accumulate the totals.
				rootMotion->totalTranslation += translation;
				rootMotion->totalDistance += distance;
				// Otherwise:
			} else {
				// Calculate translation and distance between each frame pose:
				distance = SKM_CalculateFramePoseTranslation( skmData, boundFrame, boundOldFrame, rootBoneID, axisFlags, &translation );
				// Push back the translation and traversed distance.
				( *rootMotion->translations[ frameIndex ] ) = translation;
				( *rootMotion->distances[ frameIndex ] ) = distance;
				// Accumulate the totals.
				rootMotion->totalTranslation += translation;
				rootMotion->totalDistance += distance;
			}
		}

		#ifdef _DEBUG_PRINT_SKM_ROOTMOTION_DATA
			// Iterate over all its frames.
			for ( int32_t j = 0; j < skmAnimation->num_frames; j++ ) {
				// First get the actual array frame offset.
				const int32_t unboundFrame = ( j );
				const int32_t unboundOldFrame = (j)+( j > 0 ? -1 : 0 );

				// Ensure that the frame is safely in bounds for use with our poses data array.
				const int32_t boundFrame = skmData->num_frames ? unboundFrame % (int32_t)( skmAnimation->num_frames ) : 0;
				const int32_t boundOldFrame = skmData->num_frames ? unboundOldFrame % (int32_t)( skmAnimation->num_frames ) : 0;

				// Translation and Distance that was calculated for this frame.
				const Vector3 translation = *rootMotion->translations[ boundFrame ];
				const float distance = *rootMotion->distances[ boundFrame ];

				// Debug print:
				Com_LPrintf( PRINT_DEVELOPER, "%s: animation(#%i, %s), boundOldFrame(%i), boundFrame(#%i), rootBone[ name(%s), translation(%f, %f, %f), distance(%f) ]\n",
					__func__,
					i, iqmAnimation->name,
					boundOldFrame, boundFrame,
					rootBoneName,
					translation.x, translation.y, translation.z,
					distance );
			}
		#endif
	}

	return rootMotionSet;

// For CHECK( )
fail:
	return nullptr;
}



/**
*
*
*
*	Skeletal Model(IQM) Pose Transform and Lerping:
*
*
*
**/
/**
*	@brief	Compute pose transformations for the given model + data
*			'relativeJoints' must have enough room for model->num_poses
**/
void SKM_ComputeLerpBonePoses( const model_t *model, const int32_t frame, const int32_t oldFrame, const float frontLerp, const float backLerp, skm_transform_t *outBonePose, const int32_t rootMotionBoneID, const int32_t rootMotionAxisFlags ) {
	// Sanity check.
	if ( !model || !model->skmData ) {
		return;
	}

	// Get IQM Data.
	skm_model_t *skmData = model->skmData;

	// Keep frame within bounds.
	const int32_t boundFrame = skmData->num_frames ? frame % (int32_t)skmData->num_frames : 0;
	const int32_t boundOldFrame = skmData->num_frames ? oldFrame % (int32_t)skmData->num_frames : 0;

	// Keep bone ID sane.
	int32_t boundRootMotionBoneID = ( rootMotionBoneID >= 0 ? rootMotionBoneID % (int32_t)skmData->num_joints : -1 );

	// Fetch first joint.
	skm_transform_t *relativeJoint = outBonePose;

	// Copy the animation frame pos.
	if ( frame == oldFrame ) {
		const skm_transform_t *pose = &skmData->poses[ boundFrame * skmData->num_poses ];
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
		//const float lerp = 1.0f - backlerp;
		const skm_transform_t *pose = &skmData->poses[ boundFrame * skmData->num_poses ];
		const skm_transform_t *oldPose = &skmData->poses[ boundOldFrame * skmData->num_poses ];
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
				relativeJoint->scale[ 0 ] = oldPose->scale[ 0 ] * backLerp + oldPose->scale[ 0 ] * frontLerp;
				relativeJoint->scale[ 1 ] = oldPose->scale[ 1 ] * backLerp + oldPose->scale[ 1 ] * frontLerp;
				relativeJoint->scale[ 2 ] = oldPose->scale[ 2 ] * backLerp + oldPose->scale[ 2 ] * frontLerp;
				// Quat Slerp as usual.
				QuatSlerp( oldPose->rotate, pose->rotate, frontLerp, relativeJoint->rotate );
				continue;
			}
			#endif

			relativeJoint->translate[ 0 ] = oldPose->translate[ 0 ] * backLerp + pose->translate[ 0 ] * frontLerp;
			relativeJoint->translate[ 1 ] = oldPose->translate[ 1 ] * backLerp + pose->translate[ 1 ] * frontLerp;
			relativeJoint->translate[ 2 ] = oldPose->translate[ 2 ] * backLerp + pose->translate[ 2 ] * frontLerp;

			relativeJoint->scale[ 0 ] = oldPose->scale[ 0 ] * backLerp + oldPose->scale[ 0 ] * frontLerp;
			relativeJoint->scale[ 1 ] = oldPose->scale[ 1 ] * backLerp + oldPose->scale[ 1 ] * frontLerp;
			relativeJoint->scale[ 2 ] = oldPose->scale[ 2 ] * backLerp + oldPose->scale[ 2 ] * frontLerp;

			QuatSlerp( oldPose->rotate, pose->rotate, frontLerp, relativeJoint->rotate );
		}
	}
}

/**
*	@brief	Compute "Local/Model-Space" matrices for the given pose transformations.
**/
void SKM_TransformBonePosesLocalSpace( const skm_model_t *model, const skm_transform_t *relativeBonePose, float *pose_matrices ) {
	// multiply by inverse of bind pose and parent 'pose mat' (bind pose transform matrix)
	const skm_transform_t *relativeJoint = relativeBonePose;
	const int *jointParent = model->jointParents;
	const float *invBindMat = model->invBindJoints;
	float *poseMat = pose_matrices;
	for ( uint32_t pose_idx = 0; pose_idx < model->num_poses; pose_idx++, relativeJoint++, jointParent++, invBindMat += 12, poseMat += 12 ) {
		float mat1[ 12 ], mat2[ 12 ];

		JointToMatrix( relativeJoint->rotate, relativeJoint->scale, relativeJoint->translate, mat1 );

		if ( *jointParent >= 0 ) {
			Matrix34Multiply( &model->bindJoints[ ( *jointParent ) * 12 ], mat1, mat2 );
			Matrix34Multiply( mat2, invBindMat, mat1 );
			Matrix34Multiply( &pose_matrices[ ( *jointParent ) * 12 ], mat1, poseMat );
		} else {
			Matrix34Multiply( mat1, invBindMat, poseMat );
		}
	}
}

/**
*	@brief	Compute "World-Space" matrices for the given pose transformations.
*			This is generally a slower procedure, but can be used to get the
*			actual bone's position for in-game usage.
**/
void SKM_TransformBonePosesWorldSpace( const skm_model_t *model, const skm_transform_t *relativeBonePose, float *pose_matrices ) {
	// First compute local space pose matrices from the relative joints.
	SKM_TransformBonePosesLocalSpace( model, relativeBonePose, pose_matrices );

	// Calculate the world space pose matrices.
	float *poseMat = model->bindJoints;
	float *outPose = pose_matrices;

	for ( size_t i = 0; i < model->num_poses; i++, poseMat += 12, outPose += 12 ) {
		float inPose[ 12 ];
		memcpy( inPose, outPose, sizeof( inPose ) );
		Matrix34Multiply( inPose, poseMat, outPose );
	}
}