/********************************************************************
*
*
*	SharedGame: Player SlideBox Implementation
*
*
********************************************************************/
#pragma once

//! Maximum amount of clipping planes to test for.
static constexpr int32_t PM_MAX_CLIP_PLANES = 16;

//! Can't step up onto very steep slopes
static constexpr float PM_MIN_STEP_NORMAL = 0.7f;
//! Minimal Z Normal for testing whether something is legitimately a "WALL".
static constexpr float PM_MIN_WALL_NORMAL_Z = 0.03125;


//! Velocity has not been clipped by a Floor, nor any Wall/Step.
static constexpr int32_t PM_VELOCITY_CLIPPED_NONE = BIT( 0 );
//! Velocity has been clipped by a Floor.
static constexpr int32_t PM_VELOCITY_CLIPPED_FLOOR = BIT( 1 );
//! Velocity has been clipped by a Wall/Step.
static constexpr int32_t PM_VELOCITY_CLIPPED_WALL_OR_STEP = BIT( 2 );

/**
*	@brief	Clips trace against world only.
**/
const trace_t PM_Clip( const Vector3 &start, const Vector3 &mins, const Vector3 &maxs, const Vector3 &end, const contents_t contentMask );

/**
*	@brief	Determines the mask to use and returns a trace doing so. If spectating, it'll return clip instead.
**/
const trace_t PM_Trace( const Vector3 &start, const Vector3 &mins, const Vector3 &maxs, const Vector3 &end, const contents_t contentMask = CONTENTS_NONE );

/**
*	@brief	Clips the velocity to surface normal.
**/
const int32_t PM_ClipVelocity( const Vector3 &in, const Vector3 &normal, Vector3 &out, const float overbounce );

/**
*	@brief	As long as numberOfTraces does not exceed MAX_TOUCH_TRACES, and there is not a duplicate trace registered,
*			this function adds the trace into the touchTraceList array and increases the numberOfTraces.
**/
void PM_RegisterTouchTrace( pm_touch_trace_list_t &touchTraceList, trace_t &trace );

/**
*	@brief	Attempts to trace clip into velocity direction for the current frametime.
**/
const int32_t PM_StepSlideMove_Generic( Vector3 &origin, Vector3 &velocity, const float frametime, const Vector3 &mins, const Vector3 &maxs, pm_touch_trace_list_t &touch_traces, const bool has_time );