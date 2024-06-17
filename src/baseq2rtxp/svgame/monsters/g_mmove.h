/********************************************************************
*
*
*	ServerGame: Monster Movement
*
*
********************************************************************/
#pragma once



/**
*
*
*	Monster Move Constants:
*
*
**/
/**
*	Default BoundingBoxes:
**/
//! For when monster is standing straight up.
static constexpr Vector3 MM_BBOX_STANDUP_MINS = { -16.f, -16.f, -36.f };
static constexpr Vector3 MM_BBOX_STANDUP_MAXS = { 16.f, 16.f, 36.f };
static constexpr float   MM_VIEWHEIGHT_STANDUP = 30.f;
//! For when monster is crouching.
static constexpr Vector3 MM_BBOX_DUCKED_MINS = { -16.f, -16.f, -36.f };
static constexpr Vector3 MM_BBOX_DUCKED_MAXS = { 16.f, 16.f, 8.f };
static constexpr float   MM_VIEWHEIGHT_DUCKED = 4.f;
//! For when monster is gibbed out.
//static constexpr Vector3 PM_BBOX_GIBBED_MINS = { -16.f, -16.f, 0.f };
//static constexpr Vector3 PM_BBOX_GIBBED_MAXS = { 16.f, 16.f, 24.f };
//static constexpr float   PM_VIEWHEIGHT_GIBBED = 8.f;



/**
*	StepHeight:
**/
//! Minimal step height difference for the Z axis before marking our move as a 'stair step'.
static constexpr float MM_MIN_STEP_SIZE = 4.f;
//! Maximal step height difference for the Z axis before marking our move as a 'stair step'.
static constexpr float MM_MAX_STEP_SIZE = 18.f;



/**
*	Slide Move Results:
**/
//! Succesfully performed the move.
static constexpr int32_t MM_SLIDEMOVEFLAG_MOVED = BIT( 0 );
//! It was blocked at some point, doesn't mean it didn't slide along the blocking object.
static constexpr int32_t MM_SLIDEMOVEFLAG_BLOCKED = BIT( 1 );
//! It is trapped.
static constexpr int32_t MM_SLIDEMOVEFLAG_TRAPPED = BIT( 2 );
//! Blocked by a literal wall.
static constexpr int32_t MM_SLIDEMOVEFLAG_WALL_BLOCKED = BIT( 3 );
//! Touched at least a single plane along the way.
static constexpr int32_t MM_SLIDEMOVEFLAG_PLANE_TOUCHED = BIT( 4 );



/**
*
*
*   Monster Move Touch Traces:
*
*
**/
/**
*   @brief  Used for registering entity touching resulting traces.
**/
static constexpr int32_t MM_MAX_TOUCH_TRACES = 32;
typedef struct mm_touch_trace_list_s {
    uint32_t numberOfTraces;
    trace_t traces[ MM_MAX_TOUCH_TRACES ];
} mm_touch_trace_list_t;



/**
*
*
*	Monster Move Utility Functions:
*
*
**/
/**
*	@brief	Clips trace against world only.
**/
const trace_t SVG_MMove_Clip( const Vector3 &start, const Vector3 &mins, const Vector3 &maxs, const Vector3 &end, const contents_t contentMask );
/**
*	@brief	Determines the mask to use and returns a trace doing so. If spectating, it'll return clip instead.
**/
const trace_t SVG_MMove_Trace( const Vector3 &start, const Vector3 &mins, const Vector3 &maxs, const Vector3 &end, edict_t *passEntity, contents_t contentMask = CONTENTS_NONE );



/**
*
*
*	Monster Move Functions:
*
*
**/