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
*
*
*	Animation Encode/Decode:
*
*
**/
/**
*	@brief	Encodes the composed body animation part states into a single uint32_t, each one having a limit of 255. 
**/
static inline uint32_t SG_EncodeAnimationState( const uint8_t lowerBodyAnimation, const uint8_t upperBodyAnimation, const uint8_t headAnimation/*, const uint8_t framerate*/ ) {
	const uint32_t animationState = ( ( ( lowerBodyAnimation & 0xff ) << 0 ) | ( ( upperBodyAnimation & 0xff ) << 8 ) | ( ( headAnimation & 0xff ) << 16 ) /*| ( ( framerate & 0xff ) << 24 )*/ );
	return animationState;
}

/**
*	@brief	Decodes the body animations from a single uint32_t.
**/
static inline void SG_DecodeAnimationState( const uint32_t animationState, uint8_t *lowerBodyAnimation, uint8_t *upperBodyAnimation, uint8_t *headAnimation/*, uint8_t *frameRate*/ ) {
	*lowerBodyAnimation = ( animationState & 0xff );
	*upperBodyAnimation = ( ( animationState >> 8 ) & 0xff );
	*headAnimation = ( ( animationState >> 16 ) & 0xff );
	//*framerate = ( ( animationState >> 24 ) & 255 );
}



/**
*
*
*	Uncategorized Functions: TODO: CATEGORIZE!
*
*
**/
/**
* @brief	Calculates the current frame for the current time since the start time stamp.
*
* @return   The frame and interpolation fraction for current time in an animation started at a given time.
*           When the animation is finished it will return frame -1. Takes looping into account. Looping animations
*           are never finished.
**/
const double SG_FrameForTime( int32_t *frame, const sg_time_t &currentTime, const sg_time_t &startTimeStamp, const double frameTime, const int32_t firstFrame, const int32_t lastFrame, const int32_t loopingFrames, const bool forceLoop );