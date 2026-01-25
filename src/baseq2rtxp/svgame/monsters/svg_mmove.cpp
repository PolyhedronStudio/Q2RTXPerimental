/********************************************************************
*
*
*	ServerGmae: Monster Movement
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"

#include "svg_mmove.h"
#include "svg_mmove_slidemove.h"

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
const svg_trace_t SVG_MMove_Clip( const Vector3 &start, const Vector3 &mins, const Vector3 &maxs, const Vector3 &end, const cm_contents_t contentMask ) {
	//return pm->clip( QM_Vector3ToQFloatV( start ).v, QM_Vector3ToQFloatV( mins ).v, QM_Vector3ToQFloatV( maxs ).v, QM_Vector3ToQFloatV( end ).v, contentMask );
	// Clip against world.
	return SVG_Clip( g_edict_pool.EdictForNumber( 0 ) /* worldspawn */, start, mins, maxs, end, contentMask);
}

/**
*	@brief	Determines the mask to use and returns a trace doing so.
**/
const svg_trace_t SVG_MMove_Trace( const Vector3 &start, const Vector3 &mins, const Vector3 &maxs, const Vector3 &end, svg_base_edict_t *passEntity, cm_contents_t contentMask ) {
	//// Spectators only clip against world, so use clip instead.
	//if ( pm->state->pmove.pm_type == PM_SPECTATOR ) {
	//	return PM_Clip( start, mins, maxs, end, CM_CONTENTMASK_SOLID );
	//}

	if ( contentMask == CONTENTS_NONE ) {
		//	if ( pm->state->pmove.pm_type == PM_DEAD || pm->state->pmove.pm_type == PM_GIB ) {
		//		contentMask = CM_CONTENTMASK_DEADSOLID;
		//	} else if ( pm->state->pmove.pm_type == PM_SPECTATOR ) {
		//		contentMask = CM_CONTENTMASK_SOLID;
		//	} else {
		contentMask = CM_CONTENTMASK_MONSTERSOLID;
		//	}

		//	//if ( pm->s.pm_flags & PMF_IGNORE_PLAYER_COLLISION )
		//	//	mask &= ~CONTENTS_PLAYER;
	}

	//return pm->trace( QM_Vector3ToQFloatV( start ).v, QM_Vector3ToQFloatV( mins ).v, QM_Vector3ToQFloatV( maxs ).v, QM_Vector3ToQFloatV( end ).v, pm->player, contentMask );
	return SVG_Trace( start, mins, maxs, end, passEntity, contentMask );
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
static bool MMove_CheckStep( const svg_trace_t *trace ) {
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
static void MMove_StepDown( mm_move_t *monsterMove, const svg_trace_t *trace ) {
	// Apply the trace endpos as the new origin.
	monsterMove->state.origin = trace->endpos;

	// Determine the step height based on the new, and previous origin.
	const float step_height = monsterMove->state.origin.z - monsterMove->state.previousOrigin.z;

	// If its absolute(-/+) value >= PM_STEP_MIN_SIZE(4.0) then we got an official step.
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
	svg_trace_t trace = {};
	Vector3 startOrigin = monsterMove->state.previousOrigin = monsterMove->state.origin;
	Vector3 startVelocity = monsterMove->state.previousVelocity = monsterMove->state.velocity;

	// Perform an actual 'Step Slide'.
	int32_t blockedMask = SVG_MMove_SlideMove( monsterMove->state.origin, monsterMove->state.velocity, monsterMove->frameTime, monsterMove->mins, monsterMove->maxs, monsterMove->monster, monsterMove->touchTraces, false /* monsterMove->hasTime */ );

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
	blockedMask |= SVG_MMove_SlideMove( monsterMove->state.origin, monsterMove->state.velocity, monsterMove->frameTime, monsterMove->mins, monsterMove->maxs, monsterMove->monster, monsterMove->touchTraces, false /* monsterMove->hasTime */ );

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
		svg_trace_t real_trace = SVG_MMove_Trace( monsterMove->state.origin, monsterMove->mins, monsterMove->maxs, original_down, monsterMove->monster );
		//monsterMove.origin = real_trace.endpos;
		//monsterMove->state.origin = real_trace.endpos;

		// WID: Use proper stair step checking.
		if ( MMove_CheckStep( &trace ) ) {
			// Only an upwards jump is a stair clip.
			if ( monsterMove->state.velocity.z > 0.f ) {
				monsterMove->step.clipped = true;
			}
			// Step down to the new found ground.
			MMove_StepDown( monsterMove, &real_trace );
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
		( monsterMove->liquid.level < cm_liquid_level_t::LIQUID_WAIST || ( /*!( pm->cmd.buttons & BUTTON_JUMP ) &&*/ monsterMove->state.velocity.z <= 0 ) ) ) {
		Vector3 down = monsterMove->state.origin - Vector3{ 0.f, 0.f, MM_MAX_STEP_SIZE };
		trace = SVG_MMove_Trace( monsterMove->state.origin, monsterMove->mins, monsterMove->maxs, down, monsterMove->monster );

		// WID: Use proper stair step checking.
		// Check for stairs:
		if ( MMove_CheckStep( &trace ) ) {
			// Step down stairs:
			MMove_StepDown( monsterMove, &trace );
		// We're expecting it to be a slope, step down the slope instead:
		} else if ( trace.fraction < 1.f ) {
			monsterMove->state.origin = trace.endpos;
		}
	}

	//if ( monsterMove->state.gravity > 0 ) {
	//	monsterMove->state.velocity.z = 0;
	//} else {
	//	monsterMove->state.velocity.z -= monsterMove->state.gravity * monsterMove->frameTime;
	//}

    // Apply gravity after having stored original startVelocity.
    if ( !( monsterMove->state.mm_flags & MMF_ON_GROUND ) ) {
        const float oldZ = monsterMove->state.velocity.z;
        const float delta = ( float )monsterMove->state.gravity * ( float )monsterMove->frameTime;
        monsterMove->state.velocity.z -= delta;
        // Limit logging to once per server frame to reduce spam.
        static int32_t s_last_mmove_log_frame = -1;
        if ( monsterMove->monster && level.frameNumber != s_last_mmove_log_frame ) {
            s_last_mmove_log_frame = level.frameNumber;
            const double timeSec = level.time.Seconds<double>();
            gi.dprintf( "[DEBUG][%f frame=%d] SVG_MMove_StepSlideMove: ent=%d gravity=%d frameTime=%.6f delta=%.6f oldZ=%.6f newZ=%.6f\n",
                timeSec,
                level.frameNumber,
                monsterMove->monster->s.number,
                ( int )monsterMove->state.gravity,
                monsterMove->frameTime,
                delta,
                oldZ,
                monsterMove->state.velocity.z );
        }
    }

	return blockedMask;
}

[[nodiscard]] inline Vector3 QM_Vector3Slerp( const Vector3 &from, const Vector3 &to, float t ) {
	float dot = QM_Vector3DotProduct( from, to );
	float aFactor;
	float bFactor;
	if ( fabsf( dot ) > 0.9995f ) {
		aFactor = 1.0f - t;
		bFactor = t;
	} else {
		float ang = acos( dot );
		float sinOmega = sin( ang );
		float sinAOmega = sin( ( 1.0f - t ) * ang );
		float sinBOmega = sin( t * ang );
		aFactor = sinAOmega / sinOmega;
		bFactor = sinBOmega / sinOmega;
	}
	return from * aFactor + to * bFactor;
}

/**
*	@brief	Will move the yaw to its ideal position based on the yaw speed(per frame) value.
**/
void SVG_MMove_FaceIdealYaw( svg_base_edict_t *ent, const float idealYaw, const float yawSpeed ) {
	// Get angle modded angles.
	const float currentYawAngle = QM_AngleMod( ent->currentAngles[ YAW ] );

	// If we're already facing ideal yaw, escape.
	if ( currentYawAngle == idealYaw ) {
		return;
	}

	double yawAngleMove = idealYaw - currentYawAngle;

	// Prevent the monster from rotating a full circle around the yaw.
	// Do so by keeping angles between -180/+180, depending on whether ideal yaw is higher or lower than current.
	yawAngleMove = QM_Wrap( yawAngleMove, -180., 180. );
	#if 0
	if (ideal > current) { if ( yawAngleMove >= 180 ) { yawAngleMove = yawAngleMove - 360; } } else { if ( yawAngleMove <= -180 ) { yawAngleMove = yawAngleMove + 360; } }
	#endif
	// Clamp the yaw move speed.
	yawAngleMove = QM_Clamp( yawAngleMove, (double) - yawSpeed, (double)yawSpeed );
	#if 0
	if (move > 0) { if ( yawAngleMove > yawSpeed ) { yawAngleMove = yawSpeed; } } else { if ( yawAngleMove < -yawSpeed ) { yawAngleMove = -yawSpeed; }
	#endif
	// AngleMod the final resulting angles.
	ent->currentAngles[ YAW ] = QM_AngleMod( currentYawAngle + yawAngleMove );

	//SVG_Util_SetEntityAngles( ent, ent->currentAngles, true );
}