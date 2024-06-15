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
#include "sg_skm_modelinfo.h"



//! List of loaded up skeletal model info scripts.
static sg_skminfo_t skmInfoList;

/**
*	@brief	
**/
static sg_skminfo_t *SG_ParseSkeletalModelInfo( const char *filename, const char *info ) {
	
}
/**
*	@brief	Loads and parses the skeletal model info configuration file.
*	@return	(nullptr) on failure. Otherwise, a pointer to an already existing, or newly created sg_skminfo_t.
**/
sg_skminfo_t *SG_LoadAndParseSkeletalModelInfo( const char *filename ) {

	return nullptr;
}



/**
*	@brief	Support method for calculating the exact translation of the root bone, in between frame poses.
**/
static constexpr int32_t SKM_POSE_TRANSLATE_ALL = 0;
static constexpr int32_t SKM_POSE_TRANSLATE_X = ( 1 << 0 );
static constexpr int32_t SKM_POSE_TRANSLATE_Y = ( 1 << 1 );
static constexpr int32_t SKM_POSE_TRANSLATE_Z = ( 1 << 2 );

const float SKM_CalculateFramePoseTranslation( const iqm_model_t *iqmModel, const int32_t frame, const int32_t oldFrame, const int32_t rootBoneID, const uint32_t axisFlags, Vector3 &translation ) {
	// frame == oldFrame Path:
	if ( oldFrame == frame ) {
		// No translation took place.
		translation = QM_Vector3Zero();
		// No distance was travered.
		return 0.f;
	// frame != oldFrame Path:
	} else {
		// Poses to determine the translation and distance travered from.
		const iqm_transform_t *pose = &iqmModel->poses[ frame * iqmModel->num_poses ];
		const iqm_transform_t *oldPose = &iqmModel->poses[ oldFrame * iqmModel->num_poses ];

		// Iterate over all 'poses' to find the specific root bone we want to know the translation about.
		for ( uint32_t pose_idx = 0; pose_idx < iqmModel->num_poses; pose_idx++, oldPose++, pose++ ) {
			// We found the root bone pose transform(s).
			if ( pose_idx == rootBoneID ) {
				// Get their translation vector.
				const Vector3 poseTranslate = pose->translate;
				const Vector3 oldPoseTranslate = oldPose->translate;
				// Determine the actual translation:
				translation = poseTranslate - oldPoseTranslate;
				// Zero out specific axis depending on the flags we got passed in.
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
		}
	}

	// No translation was found.
	SG_DPrintf( "%s: No translation found!\n", __func__ );
	// No distance traversed.
	return 0.f;
}


/**
*	@brief
**/
void SG_SKM_CalculateAnimationTranslations( const model_t *skm, const int32_t rootBoneID, const int32_t axisFlags ) {
	// Ensure the model is valid.
	if ( !skm || !skm->iqmData ) {
		SG_DPrintf( "%s: Invalid skm or skm->iqmData pointer.\n", __func__ );
		return;
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

		// Debug print:
		SG_DPrintf( "%s:---------------------------------------------------\n", __func__ );

		// Iterate over all its frames.
		for ( int32_t j = 0; j < iqmAnimation->num_frames; j++ ) {
			// First get the actual array frame offset.
			const int32_t unboundFrame = ( iqmAnimation->first_frame + j );

			// Ensure that the frame is safely in bounds for use with our poses data array.
			const int32_t boundFrame = iqmModel->num_frames ? unboundFrame % (int32_t)iqmModel->num_frames : 0;
			// If the unbound frame == 0, then that means the oldFrame == this animation's last frame.
			// However, it will take the entire sum of all translations together in order to have a value to
			// subtract with.
			// 
			// TODO: Figure this aspect out in a few.
			int32_t boundOldFrame = boundFrame;
			if ( boundFrame >= 1 ) {
				boundOldFrame = iqmModel->num_frames ? ( unboundFrame - 1 ) % (int32_t)iqmModel->num_frames : 0;
			}

			// Calculate the translation vector and its covering distance to traverse.
			Vector3 translation = {};
			const float distance = SKM_CalculateFramePoseTranslation( iqmModel, boundFrame, boundOldFrame, rootBoneID, axisFlags, translation );

			// Debug print:
			SG_DPrintf( "%s: animation(#%i, %s), frame(#%i), oldFrame(%i), rootBone[name:%s, translation(%f, %f, %f), distance(%f)]\n", 
				__func__,
				i, iqmAnimation->name,
				boundFrame, boundOldFrame,
				rootBoneName,
				translation.x, translation.y, translation.z,
				distance );
		}
	}
}