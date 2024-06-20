/********************************************************************
*
*
*	ServerGame: Monster SlideBox Implementation.
*
*
********************************************************************/
#pragma once

//! Maximum amount of clipping planes to test for.
static constexpr int32_t MM_MAX_CLIP_PLANES = 16;

//! Can't step up onto very steep slopes
static constexpr float MM_MIN_STEP_NORMAL = 0.7f;
//! Minimal Z Normal for testing whether something is legitimately a "WALL".
static constexpr float MM_MIN_WALL_NORMAL_Z = 0.03125;

/**
*	@brief	Clips the velocity to surface normal.
**/
void SVG_MMove_ClipVelocity( const Vector3 &in, const Vector3 &normal, Vector3 &out, const float overbounce );

/**
*	@brief	As long as numberOfTraces does not exceed MAX_TOUCH_TRACES, and there is not a duplicate trace registered,
*			this function adds the trace into the touchTraceList array and increases the numberOfTraces.
**/
void SVG_MMove_RegisterTouchTrace( mm_touch_trace_list_t &touchTraceList, trace_t &trace );

/**
*	@brief	Attempts to trace clip into velocity direction for the current frametime.
**/
const int32_t SVG_MMove_SlideMove( Vector3 &origin, Vector3 &velocity, const float frametime, const Vector3 &mins, const Vector3 &maxs, edict_t *passEntity, mm_touch_trace_list_t &touch_traces, const bool has_time );