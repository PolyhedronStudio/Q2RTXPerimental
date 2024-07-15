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
#include "sg_skm.h"

#if 0
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
#endif



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
const skm_anim_t *SG_SKM_GetAnimationForName( const model_t *model, const char *name, qhandle_t *animationID ) {
	// Get skm data.
	const skm_model_t *skmData = model->skmData;
	// Soon to be pointer to the animation.
	const skm_anim_t *skmAnimation = nullptr;
	// If we got an animationID pointer, default it to -1 (no animation found.)
	if ( animationID ) {
		*animationID = -1;
	}

	// Find the animation with a matching name.
	if ( skmData && skmData->num_animations ) {
		for ( int32_t i = 0; i < skmData->num_animations; i++ ) {
			if ( !Q_strncasecmp( skmData->animations[ i ].name, name, MAX_QPATH ) ) {
				skmAnimation = &skmData->animations[ i ];
				if ( animationID ) {
					*animationID = i;
				}
				break;
			}
		}
	}
	// Return pointer.
	return skmAnimation;
}
/**
*	@brief	Will return a pointer to the model's SKM animation data matching the 'name'.
**/
const skm_anim_t *SG_SKM_GetAnimationForHandle( const model_t *model, const qhandle_t animationID ) {
	// Get skm data.
	const skm_model_t *skmData = model->skmData;
	// Soon to be pointer to the animation.
	const skm_anim_t *skmAnimation = nullptr;
	// Sanity check.
	if ( !skmData ) {
		// TODO: Warn?
		return skmAnimation;
	}
	// Sanity check.
	if ( animationID >= 0 && animationID < skmData->num_animations ) {
		skmAnimation = &skmData->animations[ animationID ];
	}
	// Return.
	return skmAnimation;
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
const double SG_AnimationFrameForTime( int32_t *frame, const sg_time_t &currentTime, const sg_time_t &startTimeStamp, const double frameTime, const int32_t firstFrame, const int32_t lastFrame, const int32_t loopingFrames, const bool forceLoop ) {
	// Working loopinframes variable.
	int32_t _loopingFrames = loopingFrames;

	// Always set frame to startFrame if the current time stamp is lower than the animation startTimeStamp.
	if ( currentTime <= startTimeStamp ) {
		*frame = firstFrame;
		return 0.0;
	}
	// If firstframe == last frame, it means we have no animation to begin with since it is already completed.
	// To accommodate this situation, assign firstFrame to frame and return a fraction of 1.0. The animation's completed.
	if ( firstFrame == lastFrame ) {
		*frame = firstFrame;
		return 1.0;
	}

	// Calculate how long the animation is currently running for.
	sg_time_t runningtime = currentTime - startTimeStamp;

	// Calculate the frame's fraction.
	double frameFraction = ( (double)runningtime.milliseconds() / (double)frameTime );
	const int64_t frameCount = (uint64_t)frameFraction;
	frameFraction -= frameCount;

	// Determine the current frame.
	int32_t currentFrame = firstFrame + frameCount;

	// If the current frame exceeds the last frame:
	if ( currentFrame > lastFrame ) {
		// If we're force looping, and loopingframes has not been set. Set it back to where it came from(lastframe - firstframe).
		if ( forceLoop && !loopingFrames ) {
			_loopingFrames = lastFrame - firstFrame;
		}

		// If we got looping frames:
		if ( _loopingFrames ) {
			// Calculate current loop start #.
			uint32_t startCount = ( lastFrame - firstFrame ) - _loopingFrames;
			// Calculate the number of loops left to do.
			uint32_t numberOfLoops = ( frameCount - startCount ) / _loopingFrames;
			// Determine current frame.
			currentFrame -= _loopingFrames * numberOfLoops;

			// Special frame fraction handling to play an animation just once.
			//if ( _loopingFrames == 1 ) {
			//	frameFraction = 1.0;
			//	*frame = lastFrame;
			//	return frameFraction;
			//}
		// Animation's finished:
		} else {
			// Set current frame to -1 and get over with it.
			currentFrame = -1;
			//if ( _loopingFrames == 1 ) {
			//	frameFraction = 1.0;
			//	*frame = lastFrame;
			//	return frameFraction;
			//}
		}
	}

	// Assign new frame value.
	*frame = currentFrame;

	// Return frame fraction.
	return frameFraction;
}

/**
*	@brief	Processes the animation state by time. Lerping from the last animation's frame to a newly set animation if needed.
*	@note	The resulting oldFrame, currentFrame, and lerpFraction are calculated for the moment in time by this function.
*
*	@return	True if the animation has finished playing. Also returns true in case of an error, since no valid animation is instantly finished playing.
**/
const bool SG_SKM_ProcessAnimationStateForTime( const model_t *model, sg_skm_animation_state_t *animationState, const sg_time_t &time, int32_t *outOldFrame, int32_t *outCurrentFrame, double *outBackLerp ) {
	// Sanity checks.
	if ( !animationState ) {
		SG_DPrintf( "%s: animationState == (nullptr)!\n", __func__ );
		return true;
	}
	if ( !model ) {
		SG_DPrintf( "%s: model == (nullptr)!\n", __func__ );
		return true;
	}
	if ( !model->skmData ) {
		SG_DPrintf( "%s: model->skmData == (nullptr)!\n", __func__ );
		return true;
	}
		
	// Make sure that the animationID is legit.
	const skm_anim_t *skmAnimation = SG_SKM_GetAnimationForHandle( model, animationState->animationID );
	if ( !skmAnimation ) {
		SG_DPrintf( "%s: skmAnimation for handle(#%i) == (nullptr)!\n", __func__, animationState->animationID );
		return true;
	}

	// The old frame of the animation state, possibly the one to lerp from into a new animation.
	const int32_t oldFrame = animationState->currentFrame;

	// Backup the previously 'current' frame as its last frame.
	animationState->previousFrame = animationState->currentFrame;

	// Animation start and end frames.
	const int32_t firstFrame = skmAnimation->first_frame;
	const int32_t lastFrame = skmAnimation->first_frame + skmAnimation->num_frames; // use these instead?

	// Calculate the actual current frame for the moment in time of the active animation.
	double lerpFraction = SG_AnimationFrameForTime( &animationState->currentFrame,
		time, animationState->animationStartTime /*- sg_time_t::from_ms( BASE_FRAMETIME )*/,
		// Use the one provided by the animation state instead of the skmData, since we may want to play at a different speed.
		animationState->frameTime,
		// Process animation from first to last frame.
		firstFrame, lastFrame,
		// If the animation is not looping, play the animation only once.
		( !animationState->isLooping ? 1 : 0), animationState->isLooping
	);

	// Whether we are finished playing this animation.
	bool isPlaybackDone = false;

	// Apply animation to gun model refresh entity.
	if ( animationState->currentFrame == -1 ) {
		*outCurrentFrame = animationState->srcEndFrame; // For this kinda crap?
		*outBackLerp = 1.0;
		isPlaybackDone = true;
	} else {
		*outCurrentFrame = animationState->currentFrame;
	}
	//	*outOldFrame = animationState->previousFrame;
	//} else {
		*outOldFrame = ( animationState->currentFrame > firstFrame && animationState->currentFrame <= lastFrame ? animationState->currentFrame - 1 : oldFrame /*animationState->currentFrame */);
	//}

	// Use lerpfraction between frames instead to rapidly switch.
	//if ( *outOldFrame == oldFrame ) {
	//	*outBackLerp = 1.0 - SG_GetFrameXerpFraction();
	//// Enforce lerp of 0.0 to ensure that the first frame does not 'bug out'.
	//} else
	if ( *outCurrentFrame == firstFrame ) {
		*outBackLerp = 0.0;
	// Enforce lerp of 1.0 if the calculated frame is equal or exceeds the last one.
	} else if ( *outCurrentFrame == lastFrame ) {
		// Full backlerp.
		*outBackLerp = 1.0;
	// Otherwise just subtract the resulting lerpFraction.
	} else {
		*outBackLerp = 1.0 - lerpFraction;
	}

	// Clamp just to be sure.
	*outBackLerp = ( *outBackLerp < 0.0 ? *outBackLerp = 0.0 : ( *outBackLerp > 1.0 ? *outBackLerp = 1.0 : *outBackLerp ) );
	if ( *outBackLerp >= 1.0 ) {
		// We are done playing this animation now.
		isPlaybackDone = true;
	}
	// Return whether finished playing or not.
	return isPlaybackDone;
	//// Reached the end of the animation:
	//} else {
	//	// Apply animation to gun model refresh entity.
	//	*outOldFrame = animationState->previousFrame;
	//	*outCurrentFrame = animationState->currentFrame;
	//	//*outBackLerp = 1.0 - clgi.client->xerpFraction;
	//}
}

/**
*	@brief	Will set the animation matching 'name' for the animation state.
*			Will force set the aniamtion, if desired.
**/
const bool SG_SKM_SetStateAnimation( const model_t *model, sg_skm_animation_state_t *animationState, const char *animationName, const sg_time_t &startTime, const double frameTime, const bool forceLoop, const bool forceSet ) {
	// Sanity checks.
	if ( !animationState ) {
		SG_DPrintf( "%s: animationState == (nullptr)!\n", __func__ );
		return false;
	}
	if ( !model ) {
		SG_DPrintf( "%s: model == (nullptr)!\n", __func__ );
		return false;
	}
	if ( !model->skmData ) {
		SG_DPrintf( "%s: model->skmData == (nullptr)!\n", __func__ );
		return false;
	}

	// Make sure that the animationID is legit.
	qhandle_t animationID = -1;
	const skm_anim_t *skmAnimation = SG_SKM_GetAnimationForName( model, animationName, &animationID );
	if ( !skmAnimation ) {
		SG_DPrintf( "%s: skmAnimation for name(#%s) == (nullptr)!\n", __func__, animationName );
		return false;
	}

	// Apply the animation either if it is not active yet, or is forcefully set.
	if ( animationState->animationID != animationID || forceSet ) {
		// Store source frame data for easy access later on.
		animationState->srcStartFrame = skmAnimation->first_frame;
		animationState->srcEndFrame = skmAnimation->first_frame + skmAnimation->num_frames;

		// Apply new animation data.
		animationState->previousAnimationID = animationState->animationID;
		animationState->animationID = animationID;
		animationState->frameTime = frameTime;
		#if 1
		animationState->animationStartTime = startTime;
		animationState->animationEndTime = ( startTime + sg_time_t::from_ms( ( animationState->srcEndFrame - animationState->srcStartFrame ) * animationState->frameTime ) );
		#else
		animationState->animationStartTime = sg_time_t::from_ms( clgi.client->servertime );
		animationState->animationEndTime = sg_time_t::from_ms( clgi.client->servertime + ( ( animationState->srcEndFrame - animationState->srcStartFrame ) * animationState->frameTime ) );
		#endif
		animationState->isLooping = ( skmAnimation->flags & IQM_LOOP ? true : ( forceLoop == true ? true : false ) );

		// Animation was set.
		return true;
	}

	// Already running, thus not set.
	return false;
}

/**
*	@brief	Will set the animation matching 'animationID' of the animation state.
*			Will force set the aniamtion, if desired.
**/
const bool SG_SKM_SetStateAnimation( const model_t *model, sg_skm_animation_state_t *animationState, const qhandle_t animationID, const sg_time_t &startTime, const double frameTime, const bool forceLoop, const bool forceSet ) {
	// Sanity checks.
	if ( !animationState ) {
		SG_DPrintf( "%s: animationState == (nullptr)!\n", __func__ );
		return false;
	}
	if ( !model ) {
		SG_DPrintf( "%s: model == (nullptr)!\n", __func__ );
		return false;
	}
	if ( !model->skmData ) {
		SG_DPrintf( "%s: model->skmData == (nullptr)!\n", __func__ );
		return false;
	}

	// Make sure that the animationID is legit.
	const skm_anim_t *skmAnimation = SG_SKM_GetAnimationForHandle( model, animationID );
	if ( !skmAnimation ) {
		SG_DPrintf( "%s: skmAnimation for handle(#%i) == (nullptr)!\n", __func__, animationID );
		return false;
	}

	// Apply the animation either if it is not active yet, or is forcefully set.
	if ( animationState->animationID != animationID || forceSet ) {
		// Store source frame data for easy access later on.
		animationState->srcStartFrame = skmAnimation->first_frame;
		animationState->srcEndFrame = skmAnimation->first_frame + skmAnimation->num_frames;

		// Apply new animation data.
		animationState->previousAnimationID = animationState->animationID;
		animationState->animationID = animationID;
		animationState->frameTime = frameTime;
		#if 1
		animationState->animationStartTime = startTime;
		animationState->animationEndTime = ( startTime + sg_time_t::from_ms( ( animationState->srcEndFrame - animationState->srcStartFrame ) * animationState->frameTime ) );
		#else
		animationState->animationStartTime = sg_time_t::from_ms( clgi.client->servertime );
		animationState->animationEndTime = sg_time_t::from_ms( clgi.client->servertime + ( ( animationState->srcEndFrame - animationState->srcStartFrame ) * animationState->frameTime ) );
		#endif
		animationState->isLooping = ( skmAnimation->flags & IQM_LOOP ? true : ( forceLoop == true ? true : false ) );

		// Animation was set.
		return true;
	}

	// Already running, thus not set.
	return false;
}