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
#include "sharedgame/sg_shared.h"
#include "sg_skm.h"
#include "sg_skm_rootmotion.h"

#if 0
/**
*	@brief	Support method for calculating the exact translation of the root bone, in between frame poses.
**/
const float SKM_CalculateFramePoseTranslation( const iqm_model_t *iqmModel, const int32_t frame, const int32_t oldFrame, const int32_t rootBoneID, const uint32_t axisFlags, Vector3 &translation ) {
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
	SG_DPrintf( "%s: No translation found!\n", __func__ );
	// No distance traversed.
	return 0.f;
}


/**
*	@brief
**/
const sg_skm_rootmotion_set_t SG_SKM_GenerateRootMotionSet( const model_t *skm, const int32_t rootBoneID, const int32_t axisFlags ) {
	// The root motion set we'll be operating on.
	sg_skm_rootmotion_set_t rootMotionSet = {};

	// Ensure the model is valid.
	if ( !skm || !skm->iqmData ) {
		// Debug Log.
		SG_DPrintf( "%s: Invalid skm or skm->iqmData pointer.\n", __func__ );
		// Return empty set.
		return {};
	}

	// Access IQM Data.
	const iqm_model_t *iqmModel = skm->iqmData;

	//
	// This data needs to actually be coming from some configuration script or API of sorts.
	//
	const char *rootBoneName = ( ( rootBoneID >= 0 && rootBoneID < iqmModel->num_joints ) ? &iqmModel->jointNames[ rootBoneID % iqmModel->num_joints ] : "Invalid RootBoneID" );

	// Iterate over all animations.
	for ( int32_t i = 0; i < iqmModel->num_animations; i++ ) {
		// Access the animation.
		const iqm_anim_t *iqmAnimation = &iqmModel->animations[ i ];

		// Prepare the set for filling.
		sg_skm_rootmotion_t rootMotion = {
			.name = iqmAnimation->name,
			.firstFrameIndex = (int32_t)iqmAnimation->first_frame,
			.lastFrameIndex = (int32_t)( iqmAnimation->first_frame + iqmAnimation->num_frames ),
			.frameCount = (int32_t)iqmAnimation->num_frames,
		};

		// Debug print:
		SG_DPrintf( "%s:---------------------------------------------------\n", __func__ );
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
			// First get the actual array frame offset.
			const int32_t unboundFrame = ( iqmAnimation->first_frame + j );
			const int32_t unboundOldFrame = ( iqmAnimation->first_frame + j ) - 1;

			// Ensure that the frame is safely in bounds for use with our poses data array.
			const int32_t boundFrame = iqmModel->num_frames ? unboundFrame % (int32_t)iqmModel->num_frames : 0;
			const int32_t boundOldFrame = iqmModel->num_frames ? unboundOldFrame % (int32_t)iqmModel->num_frames : 0;

			// When we've reached the end of this animation, only push back the last results.
			if ( j == iqmAnimation->num_frames ) {
				// Push back the translation and traversed distance of the last frame..
				rootMotion.translations.push_back( translation );
				rootMotion.distances.push_back( distance );
				// Accumulate the totals.
				rootMotion.totalTranslation += translation;
				rootMotion.totalDistance += distance;
				// Otherwise:
			} else {
				// Calculate translation and distance between each frame pose:
				distance = SKM_CalculateFramePoseTranslation( iqmModel, boundFrame, boundOldFrame, rootBoneID, axisFlags, translation );
				// Push back the translation and traversed distance.
				rootMotion.translations.push_back( translation );
				rootMotion.distances.push_back( distance );
				// Accumulate the totals.
				rootMotion.totalTranslation += translation;
				rootMotion.totalDistance += distance;
			}
		}

		// Push the motion data into the list.
		rootMotionSet.rootMotions.push_back( rootMotion );

		// Iterate over all its frames.
		for ( int32_t j = 0; j < iqmAnimation->num_frames; j++ ) {
			// First get the actual array frame offset.
			const int32_t unboundFrame = ( j );
			const int32_t unboundOldFrame = (j)+( j > 0 ? -1 : 0 );

			// Ensure that the frame is safely in bounds for use with our poses data array.
			const int32_t boundFrame = iqmModel->num_frames ? unboundFrame % (int32_t)( iqmAnimation->num_frames ) : 0;
			const int32_t boundOldFrame = iqmModel->num_frames ? unboundOldFrame % (int32_t)( iqmAnimation->num_frames ) : 0;

			// Translation and Distance that was calculated for this frame.
			const Vector3 translation = rootMotion.translations[ boundFrame ];
			const float distance = rootMotion.distances[ boundFrame ];

			// Debug print:
			SG_DPrintf( "%s: animation(#%i, %s), boundOldFrame(%i), boundFrame(#%i), rootBone[ name(%s), translation(%f, %f, %f), distance(%f) ]\n",
				__func__,
				i, iqmAnimation->name,
				boundOldFrame, boundFrame,
				rootBoneName,
				translation.x, translation.y, translation.z,
				distance );
		}
	}

	return rootMotionSet;
}
#endif