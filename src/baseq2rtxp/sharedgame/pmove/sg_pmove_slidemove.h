/********************************************************************
*
*
*	SharedGame: Player SlideBox Implementation
*
*
********************************************************************/
#pragma once

//! Maximum amount of clipping planes to test for.
static constexpr int32_t PM_MAX_CLIP_PLANES = 8;
//! The overclip factor for velocity clipping.
static constexpr double PM_OVERCLIP = 1.001;


//! Can't step up onto very steep slopes
static constexpr double PM_STEP_MIN_NORMAL = 0.7;
//! Minimal Z Normal for testing whether something is legitimately a "WALL".
static constexpr double PM_MIN_WALL_NORMAL_Z = 0.03125;



/**
*
*
*	Internal Exposed SlideMove Utilities:
*
*
**/
/**
*	Slide Move Results:
**/
enum pm_velocityClipFlags_t {
	//! None.
	PM_VELOCITY_CLIPPED_NONE = 0,
	//! Velocity has been clipped by a Floor.
	PM_VELOCITY_CLIPPED_FLOOR = BIT( 0 ),
	//! Velocity has been clipped by a Wall/Step.
	PM_VELOCITY_CLIPPED_WALL_OR_STEP = BIT( 1 ),
};
QENUM_BIT_FLAGS( pm_velocityClipFlags_t );
/**
*	@brief	Bounce clips the velocity From surface normal.
*	@return	The blocked flags (1 = floor, 2 = step / wall)
**/
const pm_velocityClipFlags_t PM_BounceVelocity( const Vector3 &in, const Vector3 &normal, Vector3 &out, const double overbounce );
/**
*	@brief	Clips the velocity to surface normal.
*	@return	The blocked flags (1 = floor, 2 = step / wall)
**/
const pm_velocityClipFlags_t PM_ClipVelocity( const Vector3 &in, const Vector3 &normal, Vector3 &out );


/**
*
*
*	Player SlideBox Movement:
*
*
**/
/**
*	Slide Move Results:
**/
enum pm_slideMoveFlags_t {
	//! None.
	PM_SLIDEMOVEFLAG_NONE = 0,
	//! Successfully performed the move.
	PM_SLIDEMOVEFLAG_MOVED = BIT( 0 ),
	//! Successfully stepped with the move.
	PM_SLIDEMOVEFLAG_STEPPED = BIT( 1 ),
	//! Touched at least a single plane along the way.
	PM_SLIDEMOVEFLAG_PLANE_TOUCHED = BIT( 2 ),
	//! It was blocked at some point, doesn't mean it didn't slide along the blocking object.
	PM_SLIDEMOVEFLAG_BLOCKED = BIT( 3 ),
	//! It is trapped.
	PM_SLIDEMOVEFLAG_TRAPPED = BIT( 4 ),
	//! Blocked by a literal wall(Normal UP == 1).
	PM_SLIDEMOVEFLAG_WALL_BLOCKED = BIT( 5 ),
	//! Standing on a sloped ground surface.
	PM_SLIDEMOVEFLAG_SLOPE_GROUND = BIT( 6 ),
	//! Blocked by a sloped surface.
	PM_SLIDEMOVEFLAG_SLOPE_BLOCKED = BIT( 7 ),
};
QENUM_BIT_FLAGS( pm_slideMoveFlags_t );

/**
*	@brief	Attempts to trace clip into velocity direction for the current frametime.
**/
const pm_slideMoveFlags_t PM_SlideMove_Generic(
	//! Pointer to the player move instanced object we're dealing with.
	pmove_t *pm,
	//! Pointer to the actual player move local we're dealing with.
	pml_t *pml,
	//! Applies gravity if true.
	const bool applyGravity
);
/**
*	@brief	If intersecting a brush surface, will try to step over the obstruction
*			instead of sliding along it.
*
*			Returns a new origin, velocity, and contact entity
*			Does not modify any world state?
**/
const pm_slideMoveFlags_t PM_StepSlideMove_Generic(
	//! Pointer to the player move instanced object we're dealing with.
	pmove_t *pm,
	//! Pointer to the actual player move local we're dealing with.
	pml_t *pml,
	//! Applies gravity if true.
	const bool applyGravity,
	//! Whether to predict or not.
	const bool isPredictive = false
);
/**
*	@brief	Predicts whether the step move actually stepped or not.
**/
const pm_slideMoveFlags_t PM_PredictStepMove(
	//! Pointer to the player move instanced object we're dealing with.
	pmove_t *pm,
	//! Pointer to the actual player move local we're dealing with.
	pml_t *pml,
	// Apply gravity?
	const bool applyGravity
);