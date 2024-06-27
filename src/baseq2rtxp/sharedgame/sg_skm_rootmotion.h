/********************************************************************
*
*
*	SharedGame: Skeletal Model Root Motion System
*
*	TODO: Explain.
*
*
********************************************************************/
#pragma once


#if 0
////! Calculate it for all axis.
//static constexpr int32_t SKM_POSE_TRANSLATE_ALL = 0;
////! Calculate translation of the bone's X Axis.
//static constexpr int32_t SKM_POSE_TRANSLATE_X = ( 1 << 0 );
////! Calculate translation of the bone's Y Axis.
//static constexpr int32_t SKM_POSE_TRANSLATE_Y = ( 1 << 1 );
////! Calculate translation of the bone's Z Axis.
//static constexpr int32_t SKM_POSE_TRANSLATE_Z = ( 1 << 2 );

/**
*	@brief
**/
typedef struct sg_skm_rootmotion_s {
	//! Name of this root motion.
	const char *name;

	//! The first frame index into the animation frames array.
	int32_t firstFrameIndex;
	//! The last frame index into the animation frames array.
	int32_t lastFrameIndex;

	//! The total number of 'Root Bone' frames this motion contains.
	int32_t frameCount;

	//! The 'Root Bone' translation offset relative to each frame. From 0 to frameCount.
	std::vector<Vector3> translations;
	//! The total 'Root Bone' of all translations by this motion.
	Vector3 totalTranslation;

	//! The 'Root Bone' translation distance relative to each frame. From 0 to frameCount.
	std::vector<float> distances;
	//! The total 'Root Bone' distance traversed by this motion.
	float totalDistance;
} sg_skm_rootmotion_t;

/**
*	@brief
**/
typedef struct sg_skm_rootmotion_set_s {
	//! The total number of Root Motions contained in this set.
	int32_t totalRootMotions;

	//! Stores the root motions sorted by animation index number.
	std::vector<sg_skm_rootmotion_t> rootMotions;
} sg_skm_rootmotion_set_t;

#endif