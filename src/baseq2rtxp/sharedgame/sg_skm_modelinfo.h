/********************************************************************
*
*
*	SharedGame: Skeletal Model Information
*
*	Skeletal Models operate by .. TODO: explain ..
*
*
********************************************************************/
#pragma once



/**
*	@brief	Specific 'Body Parts' which are specified a bone number for use with
*			animation blending and general bone tag management.
**/
enum sg_skm_body_part_e {
	//! Indicates the lower hip bone. (Hip Bone)
	SKM_BODY_LOWER = 0,
	//! Indicates the upper spine torso bone. (Spine Bone)
	SKM_BODY_UPPER,
	//! Indicates the head. (Head Bone, duh).
	SKM_BODY_HEAD,

	//! Actual limit.
	SKM_BODY_MAX
};


/**
*	@brief	Stores the basic information that we generated from the skeletal model data.
**/
typedef struct sg_skminfo_s {
	//! Name of the skm configuration file that was parsed in-place.
	char *name;
	//! Animation root bone indices for each skeletal model body part.
	int32_t rootBones[ SKM_BODY_MAX ];

	//! Pointer to the next loaded up skeletal model info data instance.
	sg_skminfo_s *next;
} sg_skminfo_t;


/**
*	@brief
**/
typedef struct sg_skm_s {
	//! Pointer to the 'static' read-only information data for this skeletal model instance.
	sg_skminfo_t *modelInfo;

} sg_skm_t;


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