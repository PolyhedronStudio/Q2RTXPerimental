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
	//! Previous animationID.
	int32_t previousAnimationID;

	//! Source animation data.
	int32_t srcStartFrame;
	int32_t srcEndFrame;

	//! Start time.
	sg_time_t animationStartTime;
	//! Expected time of ending the animation.
	sg_time_t animationEndTime;

	//! FrameTime for this animation.
	double frameTime;

	//! Whether it is looping or not.
	bool isLooping;

	//! Root Motion Flags for this animation.
	int32_t rootMotionFlags;

	//! Actual last calculated(making it current) frame.
	int32_t currentFrame;
	//! Previously last calculated frame.
	int32_t previousFrame;
} sg_skm_animation_state_t;

/**
*	@brief	Body Animation State.
**/
typedef struct sg_skm_animation_mixer_s {
	//! Stores the calculated animation state for each of the varying body parts.
	sg_skm_animation_state_t currentBodyStates[ SKM_BODY_MAX ];
	//! Stores the calculated animation state for each of the varying body parts.
	sg_skm_animation_state_t eventBodyState[ SKM_MAX_ANIMATION_QUEUE ];

	//! Animation Event Queue.
	int32_t animationEvents[ SKM_MAX_ANIMATION_QUEUE ];
	//! Animation Event Parameters.
	int32_t animationEventsParams[ SKM_MAX_ANIMATION_QUEUE ];
} sg_skm_animation_mixer_t;



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
const skm_anim_t *SG_SKM_GetAnimationForName( const model_t *model, const char *name, qhandle_t *animationID );
/**
*	@brief	Will return a pointer to the model's SKM animation data matching the 'name'.
**/
const skm_anim_t *SG_SKM_GetAnimationForHandle( const model_t *model, const qhandle_t animationID );



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
*
*
*
*	Animation Processing:
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
const double SG_AnimationFrameForTime( int32_t *frame, const sg_time_t &currentTime, const sg_time_t &startTimeStamp, const double frameTime, const int32_t firstFrame, const int32_t lastFrame, const int32_t loopingFrames, const bool forceLoop );
/**
*	@brief	Processes the animation state by time. Lerping from the last animation's frame to a newly set animation if needed.
*	@note	The resulting oldFrame, currentFrame, and lerpFraction are calculated for the moment in time by this function.
* 
*	@return	True if the animation has finished playing. Also returns true in case of an error, since no valid animation is instantly finished playing.
**/
const bool SG_SKM_ProcessAnimationStateForTime( const model_t *model, sg_skm_animation_state_t *animationState, const sg_time_t &time, int32_t *outOldFrame, int32_t *outCurrentFrame, double *outBackLerp );
/**
*	@brief	Will set the animation matching 'name' for the animation state. Forced, if desired.
**/
const bool SG_SKM_SetStateAnimation( const model_t *model, sg_skm_animation_state_t *animationState, const char *animationName, const sg_time_t &startTime, const double frameTime, const bool forceSet );