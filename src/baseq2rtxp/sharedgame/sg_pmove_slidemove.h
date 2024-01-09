#pragma once

static constexpr float MIN_STEP_NORMAL = 0.7f;    // can't step up onto very steep slopes
static constexpr int32_t MAX_CLIP_PLANES = 5;

/**
*	@brief	Clips trace against world only.
**/
trace_t PM_Clip( const vec3_t &start, const vec3_t &mins, const vec3_t &maxs, const vec3_t &end, int32_t contentMask );

/**
*	@brief	Determines the mask to use and returns a trace doing so. If spectating, it'll return clip instead.
**/
trace_t PM_Trace( const vec3_t &start, const vec3_t &mins, const vec3_t &maxs, const vec3_t &end, int32_t contentMask = CONTENTS_NONE );

/**
*	@brief	Clips the velocity to surface normal.
**/
void PM_ClipVelocity( const vec3_t in, const vec3_t normal, vec3_t out, float overbounce );

/**
*	@brief	As long as numberOfTraces does not exceed MAX_TOUCH_TRACES, and there is not a duplicate trace registered,
*			this function adds the trace into the touchTraceList array and increases the numberOfTraces.
**/
inline void PM_RegisterTouchTrace( pm_touch_trace_list_t &touchTraceList, trace_t &trace );

/**
*	@brief	Attempts to trace clip into velocity direction for the current frametime.
**/
void PM_StepSlideMove_Generic( vec3_t &origin, vec3_t &velocity, float frametime, const vec3_t &mins, const vec3_t &maxs, pm_touch_trace_list_t &touch_traces, bool has_time );