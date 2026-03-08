/********************************************************************
*
*
*	SharedGame: Velocity (Clipping-)Utilities:
*
*
********************************************************************/
#pragma once


//! The overclip factor for velocity clipping.
static constexpr double SG_OVERCLIP = 1.001;
//! Can't step up onto very steep slopes
static constexpr double SG_STEP_MIN_NORMAL = 0.7;
//! Minimal Z Normal for testing whether something is legitimately a "WALL".
static constexpr double SG_MIN_WALL_NORMAL_Z = 0.03125;



/**
*
*
*	Bounce("Bouncy Reflection") and Sliding("Clipping") velocity
*	to Surface Normal utilities.
*
*	"PM_BounceClipVelocity" usage examples:
*		- Ground movement with slight bounce.
*		- Collision responce that needs elasticity.
*		- "Step" movement where a slight bounce helps prevent sticking.
*
*	"PM_SlideClipVelocity" usage examples:
*		- Wall sliding, no bounce.
*		- Surface alignment, no bounce.
*		- Preventing penetration without bounce.
*
*
**/
/**
*	Slide Move Results:
**/
enum SGVelocityClipFlags {
	//! None.
	SG_VELOCITY_CLIPPED_NONE = 0,
	//! Velocity has been clipped by a Floor.
	SG_VELOCITY_CLIPPED_FLOOR = BIT( 0 ),
	//! Velocity has been clipped by a sloped surface that is shallow enough to be considered floor.
	SG_VELOCITY_CLIPPED_SLOPE_FLOOR = BIT( 1 ),
	//! Velocity has been clipped by a sloped surface that is too steep to be considered floor, or a vertical wall/step.
	SG_VELOCITY_CLIPPED_SLOPE_STEEP = BIT( 2 ),
	//! Velocity has been clipped by a Wall/Step.
	SG_VELOCITY_CLIPPED_WALL_OR_STEP = BIT( 3 ),
};
QENUM_BIT_FLAGS( SGVelocityClipFlags );
/**
*	@brief	Bounces("Bouncy reflection") the velocity off the surface normal.
*	@details	Overbounce > 1 = bouncier, < 1 = less bouncy.
*				Usage examples:
*					- Ground movement with slight bounce.
*					- Collision responce that needs elasticity.
*					- "Step" movement where a slight bounce helps prevent sticking.
*	@return	The blocked flags (1 = floor, 2 = step / wall)
**/
const SGVelocityClipFlags SG_BounceClipVelocity( const Vector3 &in, const Vector3 &normal, Vector3 &out, const double overbounce );
/**
*	@brief	Slides("Clips"), the velocity (strictly-)along the surface normal.
*	@details	Usage examples:
*					- Wall sliding, no bounce.
*					- Surface alignment, no bounce.
*					- Preventing penetration without bounce.
*	@return	The blocked flags (1 = floor, 2 = step / wall)
**/
const SGVelocityClipFlags SG_SlideClipVelocity( const Vector3 &in, const Vector3 &normal, Vector3 &out );