/********************************************************************
*
*
*	SharedGame: Skeletal Model Utilities.
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

//! Number of queued up animation events to select the most prioritized from.
static constexpr int32_t SKM_MAX_ANIMATION_QUEUE = 4;

/**
*	@brief	
**/
typedef struct sg_skm_animation_state_s {
	//! Animation Index. (For now, hard capped to 255.)
	int32_t animationID;

	//! Actual last calculated(making it current) frame.
	int32_t currentFrame;
	//! Previously last calculated frame.
	int32_t previousFrame;
	
	//! FrameTime for this animation.
	double frameTime;
	//! Whether it is looping or not.
	const bool isLooping;
} sg_skm_animation_state_t;

/**
*	@brief	Body Animation State.
**/
typedef struct sg_skm_animation_mixer_s {
	//! Stores the calculated animation state for each of the varying body parts.
	sg_skm_animation_state_t bodyStates[ SKM_BODY_MAX ];

	//! Animation Event Queue.
	int32_t animationEvents[ SKM_MAX_ANIMATION_QUEUE ];
	//! Animation Event Parameters.
	int32_t animationEventsParams[ SKM_MAX_ANIMATION_QUEUE ];
} sg_skm_animation_mixer_t;



/**
*
* 
*
*	Animation Encode/Decode:
*
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
* @brief	Calculates the current frame for the current time since the start time stamp.
*
* @return   The frame and interpolation fraction for current time in an animation started at a given time.
*           When the animation is finished it will return frame -1. Takes looping into account. Looping animations
*           are never finished.
**/
const double SG_FrameForTime( int32_t *frame, const sg_time_t &currentTime, const sg_time_t &startTimeStamp, const double frameTime, const int32_t firstFrame, const int32_t lastFrame, const int32_t loopingFrames, const bool forceLoop );