/********************************************************************
*
*
*	SharedGame: Player SlideBox Implementation
*
*
********************************************************************/
#include "shared/shared.h"

#include "sharedgame/sg_shared.h"
#include "sharedgame/pmove/sg_pmove.h"
#include "sharedgame/pmove/sg_pmove_slidemove.h"

//! Uncomment for enabling second best hit plane tracing results.
//#define SECOND_PLANE_TRACE

//! Enables proper speed clamping instead of going full stop if the PM_STOP_EPSILON is reached.
//#define PM_SLIDEMOVE_CLIPVELOCITY_CLAMPING
//! Enables proper speed clamping instead of going full stop if the PM_STOP_EPSILON is reached.
//#define PM_SLIDEMOVE_BOUNCEVELOCITY_CLAMPING



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
*	@brief	Bounces("Bouncy reflection") the velocity off the surface normal.
*	@details	Overbounce > 1 = bouncier, < 1 = less bouncy.
*				Usage examples:
*					- Ground movement with slight bounce.
*					- Collision responce that needs elasticity.
*					- "Step" movement where a slight bounce helps prevent sticking.
*	@return	The blocked flags (1 = floor, 2 = step / wall)
**/
const pm_clipflags_t PM_BounceClipVelocity( const Vector3 &in, const Vector3 &normal, Vector3 &out, const double overbounce ) {
	// Whether we're actually blocked or not.
	pm_clipflags_t blocked = PM_VELOCITY_CLIPPED_NONE;

	// If the plane that is blocking us has a positive z component, then assume it's a floor.
	if ( normal.z > 0 /*PM_MIN_WALL_NORMAL_Z*/ ) {
		blocked |= PM_VELOCITY_CLIPPED_FLOOR;
	}
	// If the plane has no Z, it is vertical Wall/Step:
	if ( normal.z == 0 /*PM_MIN_WALL_NORMAL_Z*/ ) {
		blocked |= PM_VELOCITY_CLIPPED_WALL_OR_STEP;
	}

	// Determine how far to slide based on the incoming direction.
	// Finish it by scaling by the overBounce factor.
	double backOff = QM_Vector3DotProductDP( in, normal );

	// Amplifies reverse direction for bounce:
	if ( backOff < 0. ) {
		backOff *= overbounce;
	// Reduces along-surface movement:
	} else {
		backOff /= overbounce;
	}
	// Reflect the velocity along the normal.
	out = in - ( normal * backOff );

	#ifdef PM_SLIDEMOVE_BOUNCEVELOCITY_CLAMPING
	{
		const double oldSpeed = QM_Vector3Length( in );
		const double newSpeed = QM_Vector3Length( out );
		if ( newSpeed > oldSpeed ) {
			out = QM_Vector3Normalize( out );
			out = QM_Vector3Scale( oldspeed, out );
		}
	}
	#endif

	// Return blocked by flag(s).
	return blocked;
}

/**
*	@brief	Slides("Clips"), the velocity (strictly-)along the surface normal.
*	@details	Usage examples:
*					- Wall sliding, no bounce.
*					- Surface alignment, no bounce.
*					- Preventing penetration without bounce.
*	@return	The blocked flags (1 = floor, 2 = step / wall)
**/
const pm_clipflags_t PM_SlideClipVelocity( const Vector3 &in, const Vector3 &normal, Vector3 &out ) {
	// Whether we're actually blocked or not.
	pm_clipflags_t blocked = PM_VELOCITY_CLIPPED_NONE;

	// If the plane that is blocking us has a positive z component, then assume it's a floor.
	if ( normal.z > 0 /*PM_MIN_WALL_NORMAL_Z*/ ) {
		blocked |= PM_VELOCITY_CLIPPED_FLOOR;
	}
	// If the plane has no Z, it is vertical Wall/Step:
	if ( normal.z == 0 /*PM_MIN_WALL_NORMAL_Z*/ ) {
		blocked |= PM_VELOCITY_CLIPPED_WALL_OR_STEP;
	}

	// Determine how far to slide based on the incoming direction.
	// Finish it by scaling with overBounce factor.
	double backOff = QM_Vector3DotProductDP( in, normal );
	out = in - ( normal * backOff );

	#ifdef PM_SLIDEMOVE_CLIPVELOCITY_CLAMPING
	{
		const double oldSpeed = QM_Vector3Length( in );
		const double newSpeed = QM_Vector3Length( out );
		if ( newSpeed > oldSpeed ) {
			out = QM_Vector3Normalize( out );
			out = QM_Vector3Scale( oldspeed, out );
		}
	}
	#endif

	// Return blocked by flag(s).
	return blocked;
}



/**
*
*
*	PMove Step SlideMove:
*
*
**/
/**
*	@brief	Attempts to trace clip into velocity direction for the current frametime.
**/
const pm_slidemove_flags_t PM_SlideMove_Generic(
	//! Pointer to the player move instanced object we're dealing with.
	pmove_t *pm,
	//! Pointer to the actual player move local we're dealing with.
	pml_t *pml,
	//! Applies gravity if true.
	const bool applyGravity
) {
	pm_slidemove_flags_t slideMoveFlags = PM_SLIDEMOVEFLAG_NONE;

	Vector3 planes[ PM_MAX_CLIP_PLANES ] = {};
	Vector3 dir = {};
	Vector3	end = {};

	cm_trace_t trace = {};
	// Number of planes we have touched.
	int32_t numPlanes = 0;

	// Used for clipping velocity.
	double d = 0.;
	double timeLeft = 0.;
	double into = 0.;

	// For loops.
	int32_t i = 0;
	int32_t j = 0;

	// Times we bumped into a plane of a brush/hull.
	int32_t bumpCount = 0;
	// Maximum number of plane's we'll bump and clip into.
	int32_t numBumps = PM_MAX_CLIP_PLANES - 1;

	// Primal velocity is used to determine the original direction of the player.
	Vector3 primalVelocity = pm->state->pmove.velocity;

	// Clip velocity and end velocity are used to determine the new velocity after clipping.
	Vector3 clipVelocity = {};
	Vector3 endVelocity = {};
	Vector3 endClipVelocity = {};

	/**
	*	Handle Gravity
	**/
	if ( applyGravity ) {
		endVelocity = pm->state->pmove.velocity;
		endVelocity.z -= pm->state->pmove.gravity * pml->frameTime;
		pm->state->pmove.velocity.z = ( pm->state->pmove.velocity.z + endVelocity.z ) * 0.5f;
		primalVelocity.z = endVelocity.z;
		if ( pml->hasGroundPlane ) {
			// Slide along the ground plane.
			PM_BounceClipVelocity( pm->state->pmove.velocity, pml->groundTrace.plane.normal, pm->state->pmove.velocity, PM_OVERCLIP );
		}
	}

	// Setup the frametime.
	timeLeft = pml->frameTime;

	// Now prevent us from turning and clipping against the ground plane, by simply adding the trace 
	// beforehand.
	if ( pml->hasGroundPlane ) {
		numPlanes = 1;
		planes[ 0 ] = pml->groundTrace.plane.normal;
	} else {
		numPlanes = 0;
	}

	// Never turn against original velocity. 
	// (We assign it to a variable, to stop compiler from warnings about unused return value.)
	const double velLength = QM_Vector3NormalizeLength2( pm->state->pmove.velocity, planes[ numPlanes ] );
	numPlanes++;

	for ( bumpCount = 0; bumpCount < numBumps; bumpCount++ ) {
		// Calculate the end position in which we're trying to move to.
		VectorMA( pm->state->pmove.origin, timeLeft, pm->state->pmove.velocity, end );
		// See if we can trace up to it in one go.
		trace = PM_Trace( pm->state->pmove.origin, pm->mins, pm->maxs, end );
		// Entity is trapped in another solid, DON'T build up falling damage.
		if ( trace.allsolid ) {
			// Don't build up falling damage, but allow sliding.
			pm->state->pmove.velocity[ 2 ] = 0;
			// Save entity for contact.
			//PM_RegisterTouchTrace( pm->touchTraces, trace );
			// Return trapped mask.
			return slideMoveFlags |= PM_SLIDEMOVEFLAG_TRAPPED;
		}

		// We did cover some distance, so update the origin.
		if ( trace.fraction > 0 ) {
			pm->state->pmove.origin = trace.endpos;
		}

		// We moved the entire distance, so no need to loop on, break out instead.
		if ( trace.fraction == 1 ) {
			slideMoveFlags |= PM_SLIDEMOVEFLAG_MOVED;
			break;     // moved the entire distance
		} else {
			// Touched a plane.
			slideMoveFlags |= PM_SLIDEMOVEFLAG_PLANE_TOUCHED;
		}

		// Save entity for contact.
		PM_RegisterTouchTrace( pm->touchTraces, trace );

		// Calculate movement time.
		timeLeft -= timeLeft * trace.fraction;

		// This should technically never happen, but if it does, we are blocked.
		if ( numPlanes >= PM_MAX_CLIP_PLANES ) {
			// This shouldn't really happen.
			pm->state->pmove.velocity = {}; // Clear out velocity.
			return slideMoveFlags |= PM_SLIDEMOVEFLAG_TRAPPED;
		}

		// At this point we are blocked but not trapped.
		//slideMoveFlags |= PM_SLIDEMOVEFLAG_BLOCKED;
		// Is it a vertical wall?
		if ( trace.plane.normal[ 2 ] >= 1.0 ) {
			slideMoveFlags |= PM_SLIDEMOVEFLAG_WALL_BLOCKED;
		} else if ( trace.plane.normal[ 2 ] >= PM_STEP_MIN_NORMAL ) {
			// We hit a wall/step.
			slideMoveFlags |= PM_SLIDEMOVEFLAG_SLOPE_BLOCKED;
		} else if ( trace.plane.normal[ 2 ] > 0.0 && trace.plane.normal[ 2 ] < PM_STEP_MIN_NORMAL ) {
			// We hit a slope.
			slideMoveFlags |= PM_SLIDEMOVEFLAG_SLOPE_GROUND;
		}

		// If this is a plane we have touched before, try clipping
		// the velocity along it's normal and repeat.
		for ( i = 0; i < numPlanes; i++ ) {
			if ( QM_Vector3DotProductDP( trace.plane.normal, planes[ i ] ) > 0.99 /*1.0 - DBL_EPSILON */) {
				pm->state->pmove.velocity += trace.plane.normal;
				break;
			}
		}
		// Found a repeated plane, so don't add it, just repeat the trace.
		if ( i < numPlanes ) {
			continue;
		}
		// Add unique plane to the list of planes.
		planes[ numPlanes ] = trace.plane.normal;
		numPlanes++;

		/**
		*	Modify original_velocity so it parallels all of the clip planes
		**/
		// Find a plane it enters.
		for ( i = 0; i < numPlanes; i++ ) {
			// Determine if we bumped into it.
			into = QM_Vector3DotProductDP( pm->state->pmove.velocity, planes[ i ] );
			// Not entering this plane.
			if ( into >= 0.1 ) {
				continue;
			}

			// See how hard we hit into things.
			if ( -into > pml->impactSpeed ) {
				// Set the impact speed.
				pml->impactSpeed = -into;
			}

			// Slide velocity along the plane.
			PM_BounceClipVelocity( pm->state->pmove.velocity, planes[ i ], clipVelocity, PM_OVERCLIP );
			// Slide endVelocity along the plane.
			PM_BounceClipVelocity( endVelocity, planes[ i ], endClipVelocity, PM_OVERCLIP );

			// See if there is a second plane that the new move enters.
			for ( j = 0; j < numPlanes; j++ ) {
				// Skip the same plane.
				if ( j == i ) {
					continue;
				}
				// Move doesn't interact with the plane.
				if ( QM_Vector3DotProductDP( clipVelocity, planes[ j ] ) >= 0.1 ) {
					continue;
				}
				// Try clipping the move to the plane.
				PM_BounceClipVelocity( clipVelocity, planes[ j ], clipVelocity, PM_OVERCLIP );
				PM_BounceClipVelocity( endClipVelocity, planes[ j ], endClipVelocity, PM_OVERCLIP );

				// See if it goes back into the first clip plane.
				if ( QM_Vector3DotProductDP( clipVelocity, planes[ i ] ) >= 0. ) {
					continue;
				}

				// Now we can slide the original velocity along the crease.
				dir = QM_Vector3CrossProduct( planes[ i ], planes[ j ] );
				dir = QM_Vector3Normalize( dir );
				d = QM_Vector3DotProductDP( dir, pm->state->pmove.velocity );
				clipVelocity = QM_Vector3Scale( dir, d );

				dir = QM_Vector3CrossProduct( planes[ i ], planes[ j ] );
				dir = QM_Vector3Normalize( dir );
				d = QM_Vector3DotProductDP( dir, endVelocity );
				endClipVelocity = QM_Vector3Scale( dir, d );

				// See if there is a third plane that the new move enters.
				for ( int32_t k = 0; k < numPlanes; k++ ) {
					// Skip the planes we already checked.
					if ( k == i || k == j ) {
						continue;
					}
					// Move doesn't interact with the plane.
					if ( QM_Vector3DotProductDP( clipVelocity, planes[ k ] ) >= 0.1 ) {
						continue;
					}
					// Stop dead at a tripple plane interaction.
					pm->state->pmove.velocity = {};
					return slideMoveFlags |= PM_SLIDEMOVEFLAG_TRAPPED;
				}
			}

			// If we have fixed all interactions, try another move.
			pm->state->pmove.velocity = clipVelocity;
			endVelocity = endClipVelocity;
			break;
		}
	}

	// Gravity is enabled, so we need to set the velocity to the end velocity.
	if ( applyGravity ) {
		pm->state->pmove.velocity = endVelocity;
	}
	// Don't set the velocity if we are in a timer.
	// <Q2RTXP>: WID: TODO: This necessary?
	if ( pm->state->pmove.pm_time > 0. ) {
		pm->state->pmove.velocity = primalVelocity;
	}

	return slideMoveFlags;
}

/**
*	@brief	If intersecting a brush surface, will try to step over the obstruction
*			instead of sliding along it.
*
*			Returns a new origin, velocity, and contact entity
*			Does not modify any world state?
**/
const pm_slidemove_flags_t PM_StepSlideMove_Generic(
	//! Pointer to the player move instanced object we're dealing with.
	pmove_t *pm,
	//! Pointer to the actual player move local we're dealing with.
	pml_t *pml,
	//! Applies gravity if true.
	const bool applyGravity,
	//! Whether to predict or not.
	const bool isPredictive
) {
	// Q3 Stepmove
	Vector3 start_o = {}, start_v = {};
	Vector3 down_o = {}, down_v = {};
	// For traces.
	cm_trace_t trace = {};
	// For testing step distance.
	double down_dist = 0., up_dist = 0.;
	//	vec3_t		delta, delta2;
	// For up and down traces.
	Vector3	up = {}, down = {};
	// Size of step.
	double stepSize = 0.;
	// Did we step?
	bool stepped = false;

	// Move.
	pm_slidemove_flags_t slideMoveFlags = PM_SLIDEMOVEFLAG_NONE;
	start_o = pm->state->pmove.origin;
	start_v = pm->state->pmove.velocity;
	slideMoveFlags |= PM_SlideMove_Generic( pm, pml, applyGravity );

	down = pm->state->pmove.origin;
	down.z -= stepSize + PM_STEP_GROUND_DIST;
	trace = PM_Trace( start_o, pm->mins, pm->maxs, down );
	// never step up when you still have up velocity
	if ( QM_Vector3DotProductDP( trace.plane.normal, pm->state->pmove.velocity ) > 0.0 &&
		( trace.fraction == 1.0f || QM_Vector3DotProductDP( trace.plane.normal, QM_Vector3Up() ) < PM_STEP_MIN_NORMAL ) ) {
		pm->step_clip = true;
		return slideMoveFlags;
	}

	down_o = pm->state->pmove.origin;
	down_v = pm->state->pmove.velocity;

	// Try and step up.
	up = start_o;
	up.z += PM_STEP_MAX_SIZE /*+ PM_STEP_GROUND_DIST*/;
	trace = PM_Trace( start_o, pm->mins, pm->maxs, up );
	if ( trace.allsolid ) {
		// Can't step up.
		return slideMoveFlags;
	}


	// Get step size.
	stepSize = trace.endpos[ 2 ] - start_o.z;

	// Try sliding above.
	pm->state->pmove.origin = trace.endpos;
	pm->state->pmove.velocity = start_v;
	slideMoveFlags |= PM_SlideMove_Generic( pm, pml, applyGravity );
	// Push down the final amount.
	down = pm->state->pmove.origin;
	down.z -= stepSize + PM_STEP_GROUND_DIST;
	trace = PM_Trace( pm->state->pmove.origin, pm->mins, pm->maxs, down );

	// Used below for down step.
	if ( !trace.allsolid ) {
		// [Paril-KEX] from above, do the proper trace now
		if ( !trace.allsolid ) {
			#if 0
			// Get the step size.
			stepSize = pm->state->pmove.origin[ 2 ] - trace.endpos[ 2 ];
			// And assign trace endpos as new origin.
			pm->state->pmove.origin = trace.endpos;
			#else
			pm->state->pmove.origin = trace.endpos;
			//stepSize = pm->state->pmove.origin[ 2 ] - start_o.z;
			#endif
		}
	}

	up = pm->state->pmove.origin;

	// decide which one went farther
	down_dist = QM_Vector2LengthSqr( down_o - start_o );// ( down_o[ 0 ] - start_o[ 0 ] ) * ( down_o[ 0 ] - start_o[ 0 ] ) + ( down_o[ 1 ] - start_o[ 1 ] ) * ( down_o[ 1 ] - start_o[ 1 ] );
	up_dist = QM_Vector2LengthSqr( up - start_o ); //( up[ 0 ] - start_o[ 0 ] ) *( up[ 0 ] - start_o[ 0 ] ) + ( up[ 1 ] - start_o[ 1 ] ) * ( up[ 1 ] - start_o[ 1 ] );

	if ( down_dist > up_dist || trace.plane.normal[ 2 ] < PM_STEP_MIN_NORMAL ) {
		pm->state->pmove.origin = down_o;
		pm->state->pmove.velocity = down_v;

		//stepSize = pm->state->pmove.origin[ 2 ] - start_o.z;
	}
	// [Paril-KEX] NB: this line being commented is crucial for ramp-jumps to work.
	// thanks to Jitspoe for pointing this one out.
	else {// if (pm->s.pm_flags & PMF_ON_GROUND)
		//!! Special case
		// if we were walking along a plane, then we need to copy the Z over
		pm->state->pmove.velocity[ 2 ] = down_v.z;
	}

	// Paril: Step down stairs/slopes
	if ( 
		// Has to be either walking, or have a ground plane.
		( pml->isWalking || pml->hasGroundPlane )
		// Well, can't step on a ladder.
		&& !( pm->state->pmove.pm_flags & PMF_ON_LADDER )
		// Do not step for waterjumps, if we still got up velocity.
		&& !( ( pm->state->pmove.pm_flags & PMF_TIME_WATERJUMP ) && pm->state->pmove.velocity.z > 0. )
		// Don't step if we're falling down.
		&& ( pm->liquid.level < LIQUID_FEET
			|| ( !( pm->cmd.buttons & BUTTON_JUMP ) && pm->state->pmove.velocity.z <= 0. ) 
	) ) {
		down = pm->state->pmove.origin;
		down.z -= stepSize + PM_STEP_GROUND_DIST; // <Q2RTXP>: WID: -= stepSize + PM_STEP_GROUND_DIST.
		trace = PM_Trace( pm->state->pmove.origin, pm->mins, pm->maxs, down );

		if ( !trace.allsolid && trace.fraction < 1.0 ) {
			// Assign new origin.
			pm->state->pmove.origin = trace.endpos;
			// Determine step size.
			stepSize = pm->state->pmove.origin[ 2 ] - start_o.z;
			// We stepped.
			#if 0
			slideMoveFlags |= PM_SLIDEMOVEFLAG_STEPPED;
			//stepSize = pm->state->pmove.origin[ 2 ] - start_o.z;
			#endif
			// Only do this if we're not predictive(forward in time testing) the move.
			if ( !isPredictive ) {
				// >= 2.0 is an actual step however,
				if ( fabs( stepSize ) >= PM_STEP_MIN_SIZE ) {
					SG_DPrintf( "%i:stepped %f\n", pm->simulationTime, stepSize );
					// Set step size.
					pm->step_height = stepSize;
					// Stepped flag.
					slideMoveFlags |= PM_SLIDEMOVEFLAG_STEPPED;
				}
				// Keeps things smooth.
				//pm->step_height = stepSize;
			} else {
				// >= 2.0 is an actual step however,
				if ( fabs( stepSize >= PM_STEP_MIN_SIZE ) ) {
					// Stepped flag.
					slideMoveFlags |= PM_SLIDEMOVEFLAG_STEPPED;
				}
			}
			// StepMove Events.
			//double delta = pm->state->pmove.origin[ 2 ] - start_o[ 2 ];
			//if ( delta < 7 ) {
			//	PM_AddEvent( EV_STEP_4 );
			//} else if ( delta < 11 ) {
			//	PM_AddEvent( EV_STEP_8 );
			//} else if ( delta < 15 ) {
			//	PM_AddEvent( EV_STEP_12 );
			//} else {
			//	PM_AddEvent( EV_STEP_16 );
			//}
		}
		if ( trace.fraction < 1.0 ) {
			PM_BounceClipVelocity( pm->state->pmove.velocity, trace.plane.normal, pm->state->pmove.velocity, PM_OVERCLIP );
		}
	}

	#if 0
	// if the down trace can trace back to the original position directly, don't step
	trace = PM_Trace( pm->state->pmove.origin, pm->mins, pm->maxs, start_o );
	if ( trace.fraction == 1.0 ) {
		// use the original move
		//pm->state->pmove.origin = down_o;
		//pm->state->pmove.velocity = down_v;
		//pm->step_height = 0;
		////if ( pm->debugLevel ) {
		//if ( delta > 0 ) {
		//	SG_DPrintf( "%i:bend %f\n", pm->simulationTime, stepSize );
		//}
		//}
	} else {
		// We stepped.
		#if 0
		slideMoveFlags |= PM_SLIDEMOVEFLAG_STEPPED;
		//stepSize = pm->state->pmove.origin[ 2 ] - start_o.z;
		#endif
		// Only do this if we're not predictive(forward in time testing) the move.
		if ( !isPredictive ) {
			// >= 2.0 is an actual step however,
			if ( fabs( stepSize ) >= PM_STEP_MIN_SIZE ) {
				SG_DPrintf( "%i:stepped %f\n", pm->simulationTime, stepSize );
				// Keeps things smooth.
				pm->step_height = stepSize;
				slideMoveFlags |= PM_SLIDEMOVEFLAG_STEPPED;
			}
			// Keeps things smooth.
			//pm->step_height = stepSize;
		} else {
			// >= 2.0 is an actual step however,
			if ( fabs( stepSize >= PM_STEP_MIN_SIZE ) ) {
				slideMoveFlags |= PM_SLIDEMOVEFLAG_STEPPED;
			}
		}
		// StepMove Events.
		//double delta = pm->state->pmove.origin[ 2 ] - start_o[ 2 ];
		//if ( delta < 7 ) {
		//	PM_AddEvent( EV_STEP_4 );
		//} else if ( delta < 11 ) {
		//	PM_AddEvent( EV_STEP_8 );
		//} else if ( delta < 15 ) {
		//	PM_AddEvent( EV_STEP_12 );
		//} else {
		//	PM_AddEvent( EV_STEP_16 );
		//}
	}
	#endif
	// Return move result flags.
	return slideMoveFlags;
}
/**
*	@brief	Predicts whether the step move actually stepped or not.
**/
const pm_slidemove_flags_t PM_PredictStepMove(	
	//! Pointer to the player move instanced object we're dealing with.
	pmove_t *pm,
	//! Pointer to the actual player move local we're dealing with.
	pml_t *pml,
	// Apply gravity?
	const bool applyGravity 
) {
	// Backup original origin, velocity and impact speed.
	const Vector3 origin = pm->state->pmove.origin;
	const Vector3 velocity = pm->state->pmove.velocity;
	const double impactSpeed = pml->impactSpeed;

	//const double stepSize = pm->step_height;
	//const bool stepClip = pm->step_clip;
	// Test for whether we'll step.
	pm_slidemove_flags_t moveResultFlags = PM_StepSlideMove_Generic( pm, pml, applyGravity, true );
	// Restore the original, previous origin and velocity from before testing.
	pm->state->pmove.origin = origin;
	pm->state->pmove.velocity = velocity;
	// Restore impact speed.
	pml->impactSpeed = impactSpeed;
	// Restore step height.
	//pm->step_height = stepSize;
	// Restore step clip bool.
	//pm->step_clip = stepClip;
	// Return the result.
	return moveResultFlags;
}