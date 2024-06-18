/********************************************************************
*
*
*	ServerGmae: Monster Movement
*
*
********************************************************************/
#include "../g_local.h"

#include "g_mmove.h"
#include "g_mmove_slidemove.h"

/**
*
*
*	Monster Move Clip/Trace:
*
*
**/
/**
*	@brief	Clips trace against world only.
**/
const trace_t SVG_MMove_Clip( const Vector3 &start, const Vector3 &mins, const Vector3 &maxs, const Vector3 &end, const contents_t contentMask ) {
	//return pm->clip( QM_Vector3ToQFloatV( start ).v, QM_Vector3ToQFloatV( mins ).v, QM_Vector3ToQFloatV( maxs ).v, QM_Vector3ToQFloatV( end ).v, contentMask );
	// Clip against world.
	return gi.clip( &g_edicts[ 0 ], &start.x, &mins.x, &maxs.x, &end.x, contentMask );
}

/**
*	@brief	Determines the mask to use and returns a trace doing so.
**/
const trace_t SVG_MMove_Trace( const Vector3 &start, const Vector3 &mins, const Vector3 &maxs, const Vector3 &end, edict_t *passEntity, contents_t contentMask ) {
	//// Spectators only clip against world, so use clip instead.
	//if ( pm->playerState->pmove.pm_type == PM_SPECTATOR ) {
	//	return PM_Clip( start, mins, maxs, end, MASK_SOLID );
	//}

	if ( contentMask == CONTENTS_NONE ) {
		//	if ( pm->playerState->pmove.pm_type == PM_DEAD || pm->playerState->pmove.pm_type == PM_GIB ) {
		//		contentMask = MASK_DEADSOLID;
		//	} else if ( pm->playerState->pmove.pm_type == PM_SPECTATOR ) {
		//		contentMask = MASK_SOLID;
		//	} else {
		contentMask = MASK_MONSTERSOLID;
		//	}

		//	//if ( pm->s.pm_flags & PMF_IGNORE_PLAYER_COLLISION )
		//	//	mask &= ~CONTENTS_PLAYER;
	}

	//return pm->trace( QM_Vector3ToQFloatV( start ).v, QM_Vector3ToQFloatV( mins ).v, QM_Vector3ToQFloatV( maxs ).v, QM_Vector3ToQFloatV( end ).v, pm->player, contentMask );
	return gi.trace( &start.x, &mins.x, &maxs.x, &end.x, passEntity, contentMask );
}



/**
*
*
*	Step Slide Move:
*
*
**/
/**
*	@return True if the trace yielded a step, false otherwise.
**/
static bool MM_CheckStep( const trace_t *trace ) {
	// If not solid:
	if ( !trace->allsolid ) {
		// If trace clipped to an entity and the plane we hit its normal is sane for stepping:
		if ( trace->ent && trace->plane.normal[ 2 ] >= MM_MIN_STEP_NORMAL ) {
			// We just traversed a step of sorts.
			return true;
		}
	}

	// It wasn't a step, return false.
	return false;
}
/**
*	@brief	Will step to the trace its end position, calculating the height difference and
*			setting it as our step_height if it is equal or above the minimal step size.
**/
static void MM_StepDown( mm_move_t *monsterMove, const trace_t *trace ) {
	// Apply the trace endpos as the new origin.
	monsterMove->state.origin = trace->endpos;

	// Determine the step height based on the new, and previous origin.
	const float step_height = monsterMove->state.origin.z - monsterMove->state.previousOrigin.z;

	// If its absolute(-/+) value >= PM_MIN_STEP_SIZE(4.0) then we got an official step.
	if ( fabsf( step_height ) >= MM_MIN_STEP_SIZE ) {
		// Store non absolute but exact step height.
		monsterMove->step.height = step_height;
	}
}

/**
*	@brief	Each intersection will try to step over the obstruction instead of
*			sliding along it.
*
*			Returns a new origin, velocity, and contact entity
*			Does not modify any world state?
**/
const int32_t SVG_MMove_StepSlideMove( mm_move_t *monsterMove ) {
	trace_t trace = {};
	Vector3 startOrigin = monsterMove->state.previousOrigin = monsterMove->state.origin;
	Vector3 startVelocity = monsterMove->state.previousVelocity = monsterMove->state.velocity;

	// Perform an actual 'Step Slide'.
	int32_t blockedMask = SVG_MMove_SlideMove( monsterMove->state.origin, monsterMove->state.velocity, monsterMove->frameTime, monsterMove->mins, monsterMove->maxs, monsterMove->monster, monsterMove->touchTraces, true /* monsterMove->hasTime */ );

	// Store for downward move XY.
	Vector3 downOrigin = monsterMove->state.origin;
	Vector3 downVelocity = monsterMove->state.velocity;

	// Perform 'up-trace' to see whether we can step up at all.
	Vector3 up = startOrigin + Vector3{ 0.f, 0.f, MM_MAX_STEP_SIZE };
	trace = SVG_MMove_Trace( startOrigin, monsterMove->mins, monsterMove->maxs, up, monsterMove->monster );
	if ( trace.allsolid ) {
		return blockedMask; // can't step up
	}

	// Determine step size to test with.
	const float stepSize = trace.endpos[ 2 ] - startOrigin.z;

	// We can step up. Try sliding above.
	monsterMove->state.origin = trace.endpos;
	monsterMove->state.velocity = startVelocity;

	// Perform an actual 'Step Slide'.
	blockedMask |= SVG_MMove_SlideMove( monsterMove->state.origin, monsterMove->state.velocity, monsterMove->frameTime, monsterMove->mins, monsterMove->maxs, monsterMove->monster, monsterMove->touchTraces, true /* monsterMove->hasTime */ );

	// Push down the final amount.
	Vector3 down = monsterMove->state.origin - Vector3{ 0.f, 0.f, stepSize };

	// [Paril-KEX] jitspoe suggestion for stair clip fix; store
	// the old down position, and pick a better spot for downwards
	// trace if the start origin's Z position is lower than the down end pt.
	Vector3 original_down = down;
	if ( startOrigin.z < down.z ) {
		down.z = startOrigin.z - 1.f;
	}

	trace = SVG_MMove_Trace( monsterMove->state.origin, monsterMove->mins, monsterMove->maxs, down, monsterMove->monster );
	if ( !trace.allsolid ) {
		// [Paril-KEX] from above, do the proper trace now
		trace_t real_trace = SVG_MMove_Trace( monsterMove->state.origin, monsterMove->mins, monsterMove->maxs, original_down, monsterMove->monster );
		//pml.origin = real_trace.endpos;

		// WID: Use proper stair step checking.
		if ( MM_CheckStep( &trace ) ) {
			// Only an upwards jump is a stair clip.
			if ( monsterMove->state.velocity.z > 0.f ) {
				monsterMove->step.clipped = true;
			}
			// Step down to the new found ground.
			MM_StepDown( monsterMove, &real_trace );
		}
	}

	up = monsterMove->state.origin;

	// Decide which one went farther, use 'Vector2Length', ignore the Z axis.
	const float down_dist = ( downOrigin.x - startOrigin.x ) * ( downOrigin.y - startOrigin.y ) + ( downOrigin.y - startOrigin.y ) * ( downOrigin.y - startOrigin.y );
	const float up_dist = ( up.x - startOrigin.x ) * ( up.x - startOrigin.x ) + ( up.y - startOrigin.y ) * ( up.y - startOrigin.y );

	if ( down_dist > up_dist || trace.plane.normal[ 2 ] < MM_MIN_STEP_NORMAL ) {
		monsterMove->state.origin = downOrigin;
		monsterMove->state.velocity = downVelocity;
	}
	// [Paril-KEX] NB: this line being commented is crucial for ramp-jumps to work.
	// thanks to Jitspoe for pointing this one out.
	else {// if (ps->pmove.pm_flags & PMF_ON_GROUND)
		//!! Special case
		// if we were walking along a plane, then we need to copy the Z over
		monsterMove->state.velocity.z = downVelocity.z;
	}

	// Paril: step down stairs/slopes
	if ( ( monsterMove->state.mm_flags & MMF_ON_GROUND ) && !( monsterMove->state.mm_flags & MMF_ON_LADDER ) &&
		( monsterMove->liquid.level < liquid_level_t::LIQUID_WAIST || ( /*!( pm->cmd.buttons & BUTTON_JUMP ) &&*/ monsterMove->state.velocity.z <= 0 ) ) ) {
		Vector3 down = monsterMove->state.origin - Vector3{ 0.f, 0.f, MM_MAX_STEP_SIZE };
		trace = SVG_MMove_Trace( monsterMove->state.origin, monsterMove->mins, monsterMove->maxs, down, monsterMove->monster );

		// WID: Use proper stair step checking.
		// Check for stairs:
		if ( MM_CheckStep( &trace ) ) {
			// Step down stairs:
			MM_StepDown( monsterMove, &trace );
		// We're expecting it to be a slope, step down the slope instead:
		} else if ( trace.fraction < 1.f ) {
			monsterMove->state.origin = trace.endpos;
		}
	}

	return blockedMask;
}