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
*
*
*
*	These are uncategorized until later on. TODO: CATEGORIZE THESE FUNCTIONS
*
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
const double SG_FrameForTime( int32_t *frame, const sg_time_t &currentTime, const sg_time_t &startTimeStamp, const double frameTime, const int32_t firstFrame, const int32_t lastFrame, const int32_t loopingFrames, const bool forceLoop ) {
	// Working loopinframes variable.
	int32_t _loopingFrames = loopingFrames;

	// Always set frame to startFrame if the current time stamp is lower than the animation startTimeStamp.
	if ( currentTime <= startTimeStamp ) {
		*frame = firstFrame;
		return 0.0f;
	}
	// If firstframe == last frame, it means we have no animation to begin with since it is already completed.
	// To accommodate this situation, assign firstFrame to frame and return a fraction of 1.0. The animation's completed.
	if ( firstFrame == lastFrame ) {
		*frame = firstFrame;
		return 1.0f;
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
			if ( _loopingFrames == 1 ) {
				frameFraction = 1.0f;
			}
		// Animation's finished:
		} else {
			// Set current frame to -1 and get over with it.
			currentFrame = -1;
		}
	}

	// Assign new frame value.
	*frame = currentFrame;

	// Return frame fraction.
	return frameFraction;
}