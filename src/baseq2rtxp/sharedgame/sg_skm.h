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
typedef enum sg_skm_body_part_e {
	//! Indicates the lower hip bone. (Hip Bone)
	SKM_BODY_LOWER = 0,
	//! Indicates the upper spine torso bone. (Spine Bone)
	SKM_BODY_UPPER,
	//! Indicates the head. (Head Bone, duh).
	SKM_BODY_HEAD,

	//! Actual limit.
	SKM_BODY_MAX
} sg_skm_body_part_t;

//! Number of queued up animation events to select the most prioritized from.
static constexpr int32_t SKM_MAX_ANIMATION_QUEUE = 4;

//! Maximum number of blend layers that can be composited on top of the base pose.
static constexpr int32_t SKM_MAX_BLEND_LAYERS = 4;

/**
*	@brief	
**/
typedef struct sg_skm_animation_state_s {
	/**
	*	ID and settings:
	**/
	//! Animation Index. (For now, hard capped to 255.)
	int32_t animationID;
	//! Whether it is looping or not.
	bool isLooping;
	//! Root Motion Flags for this animation.
	int32_t rootMotionFlags;

	/**
	*	State Timers: Looping animations will always surpass timeDuration and timeEnd.
	**/
	//! Start time.
	QMTime timeStart;
	//! Duration of animation( ( srcEndFrame - srcStartFrame ) * frameTime ).
	QMTime timeDuration;
	//! Expected time of ending the animation. ( timeStart + timeDuration ).
	QMTime timeEnd;
	//! FrameTime for this animation.
	double frameTime;

	/**
	*	Frames:
	**/
	//! Actual last calculated(making it current) frame.
	int32_t currentFrame;
	//! Previously last calculated frame.
	int32_t previousFrame;

	//! Total offset for the animation's model source start frame index.
	int32_t srcStartFrame;
	//! Total offset for the animation's model source end frame index.
	int32_t srcEndFrame;

} sg_skm_animation_state_t;

/**
*	@brief	Body Animation State.
**/
typedef struct sg_skm_animation_mixer_s {
	//! Stores the calculated animation state for each of the varying body parts.
	sg_skm_animation_state_t currentBodyStates[ SKM_BODY_MAX ];
	//! Stores the last animation state for each of the varying body parts.
	sg_skm_animation_state_t lastBodyStates[ SKM_BODY_MAX ];

	//! Stores the calculated animation state for each of the varying body parts.
	sg_skm_animation_state_t eventBodyState[ SKM_MAX_ANIMATION_QUEUE ];

	//! Animation Event Queue.
	int32_t animationEvents[ SKM_MAX_ANIMATION_QUEUE ];
	//! Animation Event Parameters.
	int32_t animationEventsParams[ SKM_MAX_ANIMATION_QUEUE ];
} sg_skm_animation_mixer_t;

/**
*	@brief	Blend layer application mode.
**/
typedef enum sg_skm_blend_mode_e {
	//! Weighted slerp/lerp of the base pose towards the layer pose. (Default.)
	SKM_BLEND_MODE_OVERRIDE = 0,
	//! Additive: apply the layer's delta relative to its reference pose on top of the base pose.
	SKM_BLEND_MODE_ADDITIVE,

	SKM_BLEND_MODE_MAX
} sg_skm_blend_mode_t;

/**
*	@brief	A single blend layer: an animation state composited over the base pose,
*			starting at a given bone, with per-bone weights and a fade envelope.
**/
typedef struct sg_skm_blend_layer_s {
	//! The layer's animation state. (Reuses the regular state struct.)
	sg_skm_animation_state_t state;
	//! Bone number at which this layer starts blending. (-1 == inactive layer.)
	int32_t startBone;
	//! How the layer is applied to the base pose. (OVERRIDE or ADDITIVE.)
	sg_skm_blend_mode_t mode;
	//! Per-bone blend weights, indexed by bone number. (0.0 == keep base, 1.0 == full layer.)
	float boneWeights[ SKM_MAX_BONES ];
	//! Weight applied to bones under startBone that have no explicit mask entry.
	float defaultWeight;
	//! Fade-in duration in milliseconds. (0 == instant.)
	int64_t fadeInTime;
	//! Fade-out duration in milliseconds, applied after timeEnd. (0 == instant.)
	int64_t fadeOutTime;
} sg_skm_blend_layer_t;

/**
*	@brief	A named 'faux animation': a composite recipe of blend layers that is
*			registered alongside real IQM animations and resolvable by name.
**/
typedef struct sg_skm_faux_animation_s {
	//! Recipe name, resolvable through SG_SKM_GetFauxAnimationForName.
	char name[ MAX_QPATH ];
	//! Number of active layers in this recipe.
	int32_t numberOfLayers;
	//! The recipe's layers, applied in order on top of the base pose.
	sg_skm_blend_layer_t layers[ SKM_MAX_BLEND_LAYERS ];
} sg_skm_faux_animation_t;



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
static inline uint32_t SG_EncodeAnimationState( const uint8_t lowerBodyAnimation, const uint8_t upperBodyAnimation, const uint8_t headAnimation, const uint8_t frameRate ) {
	const uint32_t animationState = ( ( ( lowerBodyAnimation & 0xff ) << 0 ) | ( ( upperBodyAnimation & 0xff ) << 8 ) | ( ( headAnimation & 0xff ) << 16 ) | ( ( frameRate & 0xff ) << 24 ) );
	return animationState;
}
/**
*	@brief	Decodes the body animations from a single uint32_t.
**/
static inline void SG_DecodeAnimationState( const uint32_t animationState, uint8_t *lowerBodyAnimation, uint8_t *upperBodyAnimation, uint8_t *headAnimation, uint8_t *frameRate ) {
	if ( lowerBodyAnimation )	{ *lowerBodyAnimation = ( animationState & 0xff ); }
	if ( upperBodyAnimation )	{ *upperBodyAnimation = ( ( animationState >> 8 ) & 0xff );	}
	if ( headAnimation )		{ *headAnimation = ( ( animationState >> 16 ) & 0xff );	}
	if ( frameRate )			{ *frameRate = ( ( animationState >> 24 ) & 0xff );	}// 1000 / frameRate == FrameTime(ms)
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
const double SG_AnimationFrameForTime( int32_t *frame, const QMTime &currentTime, const QMTime &startTimeStamp, const double frameTime, const int32_t firstFrame, const int32_t lastFrame, const int32_t loopingFrames, const bool forceLoop );
/**
*	@brief	Processes the animation state by time. Lerping from the last animation's frame to a newly set animation if needed.
*	@note	The resulting oldFrame, currentFrame, and lerpFraction are calculated for the moment in time by this function.
* 
*	@return	True if the animation has finished playing. Also returns true in case of an error, since no valid animation is instantly finished playing.
**/
const bool SG_SKM_ProcessAnimationStateForTime( const model_t *model, sg_skm_animation_state_t *animationState, const QMTime &time, int32_t *outOldFrame, int32_t *outCurrentFrame, double *outBackLerp );
/**
*	@brief	Will set the animation matching 'name' for the animation state. Forced, if desired.
**/
const bool SG_SKM_SetStateAnimation( const model_t *model, sg_skm_animation_state_t *animationState, const char *animationName, const QMTime &startTime, const double frameTime, const bool forceLoop, const bool forceSet );
/**
*	@brief	Will set the animation matching 'animationID' of the animation state.
*			Will force set the aniamtion, if desired.
**/
const bool SG_SKM_SetStateAnimation( const model_t *model, sg_skm_animation_state_t *animationState, const qhandle_t animationID, const QMTime &startTime, const double frameTime, const bool forceLoop, const bool forceSet );



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
void SG_SKM_TransformBonePosesLocalSpace( const skm_model_t *model, const skm_transform_t *relativeBonePose, float *pose_matrices );
/**
*   @brief
**/
void SG_SKM_TransformBonePosesWorldSpace( const skm_model_t *model, const skm_transform_t *relativeBonePose, const skm_bone_controller_t *boneControllers, float *pose_matrices );



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
void SG_SKM_LerpBonePoses( const model_t *model, const skm_transform_t *frameBonePoses, const skm_transform_t *oldFrameBonePoses, const float frontLerp, const float backLerp, skm_transform_t *outBonePose, const int32_t rootMotionBoneID, const int32_t rootMotionAxisFlags );
/**
*	@brief	Lerped pose transformations between frameBonePoses and oldFrameBonePoses.
*			'outBonePose' must have enough room for model->num_poses
**/
void SG_SKM_LerpRangeBonePoses( const model_t * model, const skm_transform_t * frameBonePoses, const skm_transform_t * oldFrameBonePoses, const int32_t rangeStart, const int32_t rangeSize, const float frontLerp, const float backLerp, skm_transform_t * outBonePose, const int32_t rootMotionBoneID, const int32_t rootMotionAxisFlags );



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
void SG_SKM_RecursiveBlendFromBone( const skm_transform_t *addBonePoses, const skm_transform_t *addToBonePoses, skm_transform_t *outBonePoses, const skm_bone_node_t *boneNode, const double backLerp, const double fraction );
/**
*	@brief	Applies a blend layer on top of the base pose, starting from startBoneNumber,
*			using the layer's per-bone weights, mode, and fade envelope for the given time.
*			'baseBonePoses' and 'outBonePoses' may alias.
*	@note	layerPose is expected to be a full num_poses sized pose buffer processed from
*			the layer's animation state for the same moment in time.
**/
void SG_SKM_ApplyBlendLayer( const model_t *model, const sg_skm_blend_layer_t *layer, const skm_transform_t *layerPose, const skm_transform_t *baseBonePoses, skm_transform_t *outBonePoses, const QMTime &currentTime );



/**
*
*
*
*   Faux Animations(Composite Recipes):
*
*
*
**/
/**
*	@brief	Returns a pointer to the model's faux animation recipe matching 'name', or nullptr.
*			Resolves recipes parsed from the model's .skc config, falling back to the
*			built-in code-side defaults.
**/
const sg_skm_faux_animation_t *SG_SKM_GetFauxAnimationForName( const model_t *model, const char *name );
/**
*	@brief	Applies all layers of the given faux animation recipe on top of the base pose,
*			processing each layer's animation state for the current time.
*			'baseBonePoses' and 'outBonePoses' may alias.
*	@return	True if any layer was actually applied(still playing).
**/
const bool SG_SKM_ApplyFauxAnimation( const model_t *model, const sg_skm_faux_animation_t *fauxAnimation, sg_skm_blend_layer_t *workingLayers, const int32_t maxWorkingLayers, const QMTime &currentTime, const skm_transform_t *baseBonePoses, skm_transform_t *outBonePoses );



/**
*
*
*
*   Module Bindings(Engine Access Wrappers):
*
*
*
**/
/**
*	@brief	Returns a pointer to the first bone that has a matching name, or nullptr.
*			(Per-module binding wrapper around the engine's SKM_GetBoneByName.)
**/
skm_bone_node_t *SG_SKM_GetBoneByName( const model_t *model, const char *boneName );
/**
*	@brief	Returns a pointer to the model's bone poses for 'frame'.
*			Frame will wrap around in case of exceeding the frame pose limit.
*			(Per-module binding wrapper around the engine's SKM_GetBonePosesForFrame.)
**/
const skm_transform_t *SG_SKM_GetBonePosesForFrame( const model_t *model, const int32_t frame );



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
void SG_SKM_LerpBonePoses( const model_t *model, const skm_transform_t *frameBonePoses, const skm_transform_t *oldFrameBonePoses, const float frontLerp, const float backLerp, skm_transform_t *outBonePose, const int32_t rootMotionBoneID, const int32_t rootMotionAxisFlags );
/**
*	@brief	:erped pose transformations between frameBonePoses and oldFrameBonePoses.
*			'outBonePose' must have enough room for model->num_poses
**/
void SG_SKM_LerpRangeBonePoses( const model_t *model, const skm_transform_t *frameBonePoses, const skm_transform_t *oldFrameBonePoses, const int32_t rangeStart, const int32_t rangeSize, const float frontLerp, const float backLerp, skm_transform_t *outBonePose, const int32_t rootMotionBoneID, const int32_t rootMotionAxisFlags );
/**
*	@brief	Compute lerped pose transformations for the given model's frame/oldFrame.
*			'outBonePose' must have enough room for model->num_poses
**/
void SG_SKM_ComputeLerpBonePoses( const model_t *model, const int32_t frame, const int32_t oldFrame, const float frontLerp, const float backLerp, skm_transform_t *outBonePose, const int32_t rootMotionBoneID, const int32_t rootMotionAxisFlags );



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
void SG_SKM_BoneController_Activate( skm_bone_controller_t *boneController, const skm_bone_node_t *boneNode, const skm_bone_controller_target_t &target, const skm_transform_t *initialTransform, const skm_transform_t *currentTransform, const int32_t transformMask, const QMTime &timeDuration, const QMTime &timeActivated );
/**
*   @brief  Processes and applies the bone controllers to the pose, for the current time.
*   @note   The pose is expected to be untempered with in any way.
*           This means that it should contain purely relative blended joints sourced by lerping and recursive blending.
**/
void SG_SKM_BoneController_ApplyToPoseForTime( skm_bone_controller_t *boneControllers, const int32_t numBoneControllers, const QMTime &currentTime, skm_transform_t *inOutBonePoses );
