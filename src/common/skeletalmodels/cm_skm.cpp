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
    if ( !model || !model->iqmData || !model->skmConfig ) {
        #ifdef _DEBUG_PRINT_SKM_INVALID_MODEL_DATA
        Com_LPrintf( PRINT_DEVELOPER, "%s: Invalid model data encountered( !model || !model->iqmData || !model->skmConfig )!\n", __func__ );
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
    if ( !model || !model->iqmData || !model->skmConfig ) {
        #ifdef _DEBUG_PRINT_SKM_INVALID_MODEL_DATA
        Com_LPrintf( PRINT_DEVELOPER, "%s: Invalid model data encountered( !model || !model->iqmData || !model->skmConfig )!\n", __func__ );
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
*   Skeletal Model Root Motion Bone System:
*
*
**/
/**
*	@brief	Support method for calculating the exact translation of the root bone, in between frame poses.
**/
const float SKM_CalculateFramePoseTranslation( iqm_model_t *iqmModel, const int32_t frame, const int32_t oldFrame, const int32_t rootBoneID, const uint32_t axisFlags, Vector3 &translation ) {
	// frame == oldFrame Path:
	if ( oldFrame == frame || frame < 0 || oldFrame < 0 ) {
		// No translation took place.
		translation = QM_Vector3Zero();
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
		translation = poseTranslate - oldPoseTranslate;
		// If we got flags passed in, zero out translation for either of the flags that is missing.
		if ( axisFlags ) {
			if ( !( axisFlags & SKM_POSE_TRANSLATE_X ) ) {
				translation.x = 0;
			}
			if ( !( axisFlags & SKM_POSE_TRANSLATE_Y ) ) {
				translation.y = 0;
			}
			if ( !( axisFlags & SKM_POSE_TRANSLATE_Z ) ) {
				translation.z = 0;
			}
		}
		// Determine and return the distance between the two translation vectors.
		return QM_Vector3Length( translation );
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
	if ( !model || !model->iqmData || !model->skmConfig ) {
		// Debug Log.
		Com_LPrintf( PRINT_DEVELOPER, "%s: Invalid model data( !model || !model->iqmData || !model->skmConfig )!\n", __func__ );
		// Return empty set.
		return {};
	}


	// IQM Data Pointer.
	iqm_model_t *iqmData = model->iqmData;
	// SKM Config Pointer.
	skm_config_t *skmConfig = model->skmConfig;

	//
	// Allocate memory for all root motion sets.
	//
	// Allocate space for the child bones.
	CHECK( skmConfig->rootMotion.motions = (skm_rootmotion_t**)( MOD_Malloc( sizeof( skm_rootmotion_t * ) * iqmData->num_animations ) ) );
	// Ensure it is zeroed out.
	memset( skmConfig->rootMotion.motions, 0, sizeof( skm_rootmotion_t * ) * iqmData->num_animations );

	// This data needs to actually be coming from some configuration script or API of sorts.
	//
	rootBoneName = ( ( rootBoneID >= 0 && rootBoneID < iqmData->num_joints ) ? &iqmData->jointNames[ rootBoneID % iqmData->num_joints ] : "Invalid RootBoneID" );
	
	// Iterate over all animations.
	for ( int32_t i = 0; i < iqmData->num_animations; i++ ) {
		// Access the IQM animation.
		const iqm_anim_t *iqmAnimation = &iqmData->animations[ i ];


		// Allocate space for the translations as well as the distances traversed by each animation frame.
		// Allocate space for the child bones.
		CHECK( skmConfig->rootMotion.motions[ i ] = (skm_rootmotion_t *)( MOD_Malloc( sizeof( skm_rootmotion_t ) ) ) );
		// Get the pointer to the rootmotion set.
		skm_rootmotion_t *rootMotion = skmConfig->rootMotion.motions[ i ];
		// Ensure it is zeroed out.
		memset( rootMotion, 0, sizeof( skm_rootmotion_t ) );

		CHECK( rootMotion->translations = (Vector3**)( MOD_Malloc( sizeof( Vector3 * ) * iqmAnimation->num_frames ) ) );
		// Ensure it is zeroed out.
		memset( rootMotion->translations, 0, sizeof( Vector3 ) * iqmAnimation->num_frames );

		// Distances
		CHECK( rootMotion->distances = (float **)( MOD_Malloc( sizeof( float *) * iqmAnimation->num_frames ) ) );
		// Ensure it is zeroed out.
		memset( rootMotion->distances, 0, sizeof( float ) * iqmAnimation->num_frames );


		// Prepare the set for filling.
		rootMotion->firstFrameIndex = (int32_t)iqmAnimation->first_frame;
		rootMotion->lastFrameIndex = (int32_t)( iqmAnimation->first_frame + iqmAnimation->num_frames );
		rootMotion->frameCount = (int32_t)iqmAnimation->num_frames;
		
		// Let us not forget copying in its name.
		Q_strlcpy( rootMotion->name, iqmAnimation->name, sizeof( iqmAnimation->name ) );

		// Debug print:
		Com_LPrintf( PRINT_DEVELOPER, "%s:---------------------------------------------------\n", __func__ );
		// This vector is cleared upon each new animation. It serves as a purpose to accumulate the total distance
		// traversed up till the end animation. This in turn is used to calculate the distance traversed from the
		// last to the first frame.
		Vector3 accumulatedTranslation = QM_Vector3Zero();
		float accumulatedDistance = 0.f;

		// Last calculated translation and distance.
		Vector3 translation = {};
		float distance = 0;

		// Iterate over all its frames.
		for ( int32_t j = 1; j <= iqmAnimation->num_frames; j++ ) {
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
			const int32_t unboundFrame = ( iqmAnimation->first_frame + j );
			const int32_t unboundOldFrame = ( iqmAnimation->first_frame + j ) - 1;

			// Ensure that the frame is safely in bounds for use with our poses data array.
			const int32_t boundFrame = iqmData->num_frames ? unboundFrame % (int32_t)iqmData->num_frames : 0;
			const int32_t boundOldFrame = iqmData->num_frames ? unboundOldFrame % (int32_t)iqmData->num_frames : 0;

			// When we've reached the end of this animation, only push back the last results.
			if ( j == iqmAnimation->num_frames ) {
				// Push back the translation and traversed distance of the last frame..
				( *rootMotion->translations[ frameIndex ] ) = translation;
				( *rootMotion->distances[ frameIndex ] ) = distance;
				// Accumulate the totals.
				rootMotion->totalTranslation += translation;
				rootMotion->totalDistance += distance;
				// Otherwise:
			} else {
				// Calculate translation and distance between each frame pose:
				distance = SKM_CalculateFramePoseTranslation( iqmData, boundFrame, boundOldFrame, rootBoneID, axisFlags, translation );
				// Push back the translation and traversed distance.
				( *rootMotion->translations[ frameIndex ] ) = translation;
				( *rootMotion->distances[ frameIndex ] ) = distance;
				// Accumulate the totals.
				rootMotion->totalTranslation += translation;
				rootMotion->totalDistance += distance;
			}
		}

		// Iterate over all its frames.
		for ( int32_t j = 0; j < iqmAnimation->num_frames; j++ ) {
			// First get the actual array frame offset.
			const int32_t unboundFrame = ( j );
			const int32_t unboundOldFrame = (j)+( j > 0 ? -1 : 0 );

			// Ensure that the frame is safely in bounds for use with our poses data array.
			const int32_t boundFrame = iqmData->num_frames ? unboundFrame % (int32_t)( iqmAnimation->num_frames ) : 0;
			const int32_t boundOldFrame = iqmData->num_frames ? unboundOldFrame % (int32_t)( iqmAnimation->num_frames ) : 0;

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
	}

	return rootMotionSet;

// For CHECK( )
fail:
	return nullptr;
}