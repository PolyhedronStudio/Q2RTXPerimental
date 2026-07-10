/********************************************************************
*
*
*	ServerGmae: Monster Movement
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"

#include "svgame/nav/nav_path.h"

#include "svg_mmove.h"
#include "svg_mmove_slidemove.h"

#ifndef SVG_DEBUG_STAIR_TRACES
#define SVG_DEBUG_STAIR_TRACES 0
#endif

#if defined( SVG_DEBUG_STAIR_TRACES )
static inline void SVG_MMove_DebugTrace( const char *shape, const Vector3 &start, const Vector3 &end, const svg_trace_t &trace, const float radius, const float halfHeight ) {
	if ( trace.fraction >= 1.0 && !trace.startsolid && !trace.allsolid ) {
		return;
	}

	gi.dprintf( "[STAIR TRACE][Monster][%s] start=(%.2f %.2f %.2f) end=(%.2f %.2f %.2f) frac=%.5f startsolid=%d allsolid=%d ent=%d brush=%d radius=%.2f halfheight=%.2f endpos=(%.2f %.2f %.2f) normal=(%.4f %.4f %.4f)\n",
		shape,
		start.x, start.y, start.z,
		end.x, end.y, end.z,
		trace.fraction,
		trace.startsolid ? 1 : 0,
		trace.allsolid ? 1 : 0,
		trace.entityNumber,
		trace.brushID,
		radius,
		halfHeight,
		trace.endpos.x, trace.endpos.y, trace.endpos.z,
		trace.plane.normal[ 0 ], trace.plane.normal[ 1 ], trace.plane.normal[ 2 ] );
}
#endif

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
static constexpr double DIST_EPSILON = 0.03125;

/**
*	@brief	Trace the monster bbox as a capsule using center-space parameters.
**/
const svg_trace_t SVG_MMove_Trace( const Vector3 &start, const Vector3 &mins, const Vector3 &maxs, const Vector3 &end, svg_base_edict_t *passEntity, cm_contents_t contentMask, mm_trace_shape_t shape ) {
	if ( contentMask == CONTENTS_NONE ) {
		contentMask = CM_CONTENTMASK_MONSTERSOLID;
	}

	// Calculate dimensions for shape.
	const float radius = ( maxs.x - mins.x ) * 0.5f;
	const float fullHalfHeight = ( maxs.z - mins.z ) * 0.5f;
	const float capsuleHalfHeight = std::max( 0.0f, fullHalfHeight - radius );
	const float centerOffsetZ = ( mins.z + maxs.z ) * 0.5f;

	Vector3 capStart = start;
	capStart.z += centerOffsetZ;
	Vector3 capEnd = end;
	capEnd.z += centerOffsetZ;

	// Trace against the world and all entities (including BSPs).
	svg_trace_t tr;
	if ( shape == MM_SHAPE_AUTO ) {
		// Use the AABB trace to preserve the existing entity/world shape behavior.
		const Vector3 boxMins = mins;
		const Vector3 boxMaxs = maxs;
		const svg_trace_t traceBox = SVG_Trace( start, boxMins, boxMaxs, end, passEntity, contentMask );
	#if defined( SVG_DEBUG_STAIR_TRACES )
		SVG_MMove_DebugTrace( "AABB probe", start, end, traceBox, radius, fullHalfHeight );
	#endif
		if ( traceBox.ent && traceBox.ent != g_edict_pool.EdictForNumber( 0 ) ) {
			shape = MM_SHAPE_CAPSULE;
		} else {
			shape = MM_SHAPE_CYLINDER;
		}
	}

	if ( shape == MM_SHAPE_CYLINDER ) {
		tr = SVG_TraceCylinder( capStart, radius, fullHalfHeight, capEnd, passEntity, contentMask );
	} else {
		tr = SVG_TraceCapsule( capStart, radius, capsuleHalfHeight, capEnd, passEntity, contentMask );
	}
	#if defined( SVG_DEBUG_STAIR_TRACES )
	SVG_MMove_DebugTrace( shape == MM_SHAPE_CYLINDER ? "Cylinder" : "Capsule", capStart, capEnd, tr, radius, shape == MM_SHAPE_CYLINDER ? fullHalfHeight : capsuleHalfHeight );
	#endif
	tr.endpos.z -= centerOffsetZ;
	return tr;
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
static bool MMove_CheckStep( const mm_move_t *monsterMove, const svg_trace_t *trace ) {
	// If not solid:
	if ( !trace->allsolid ) {
		// Get min step normal.
		double minStepNormal = MM_MIN_STEP_NORMAL;
		//if ( monsterMove->navPolicy ) {
		//	minStepNormal = monsterMove->navPolicy->min_step_normal;
		//}
		// If the plane we hit has a sufficient upward normal, it's a step (entity or world).
		if ( trace->plane.normal[ 2 ] >= minStepNormal ) {
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

	// If its absolute(-/+) value >= PM_STEP_MIN_SIZE(14.0) then we got an official step.
	
	// Get the step height.
	double minStepSize = MM_MIN_STEP_HEIGHT;
	if ( monsterMove->navPolicy ) {
		minStepSize = monsterMove->navPolicy->min_step_height;
	}
	if ( fabsf( step_height ) >= minStepSize ) {
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
/**
*   @brief   Performs step/slide movement for a monster, using nav path policy for drop/jump limits.
*   @param   monsterMove   Movement state struct.
*   @param   policy        Navigation/path policy (drop height, jump, etc).
*   @return  Slide/step move result flags.
*   @note    All drop/jump/step logic uses the policy struct for limits.
**/
const mm_slide_move_flags_t SVG_MMove_StepSlideMove( mm_move_t *monsterMove, const nav_path_policy_t &policy ) {
	svg_trace_t trace = {};
	Vector3 startOrigin = monsterMove->state.previousOrigin = monsterMove->state.origin;
	Vector3 startVelocity = monsterMove->state.previousVelocity = monsterMove->state.velocity;

	// Perform an actual 'Step Slide'.
	mm_slide_move_flags_t blockedMask = SVG_MMove_SlideMove( monsterMove->state.origin, monsterMove->state.velocity, monsterMove->frameTime, monsterMove->mins, monsterMove->maxs, monsterMove->monster, monsterMove->touchTraces, false /* monsterMove->hasTime */ );

	// Store for downward move XY.
	Vector3 downOrigin = monsterMove->state.origin;
	Vector3 downVelocity = monsterMove->state.velocity;
	bool acceptedStepUp = false;

	// Get max step size.
	double maxStepSize = MM_MAX_STEP_HEIGHT;
	if ( monsterMove->navPolicy ) {
		maxStepSize = monsterMove->navPolicy->max_step_height;
	}

	// Perform 'up-trace' to see whether we can step up at all
	Vector3 up = startOrigin + Vector3{ 0., 0., maxStepSize };
	// Use the entity's native shape (MM_SHAPE_AUTO). Using a cylinder here causes instant 'startsolid' failures
	// if the entity is a capsule that has wedged its rounded bottom closer to a step edge than the cylinder radius allows.
	trace = SVG_MMove_Trace( startOrigin, monsterMove->mins, monsterMove->maxs, up, monsterMove->monster, CONTENTS_NONE, MM_SHAPE_AUTO );
	if ( trace.allsolid ) {
		return blockedMask; // can't step up
	}

	// Determine step size to test with.
	const float stepSize = trace.endpos[ 2 ] - startOrigin.z;

	// We can step up. Try sliding above.
	monsterMove->state.origin = trace.endpos;
	monsterMove->state.velocity = startVelocity;

	// Perform an actual 'Step Slide'. Use the native shape so we don't startsolid if wedged in a corner.
	blockedMask |= SVG_MMove_SlideMove( monsterMove->state.origin, monsterMove->state.velocity, monsterMove->frameTime, monsterMove->mins, monsterMove->maxs, monsterMove->monster, monsterMove->touchTraces, false /* monsterMove->hasTime */, MM_SHAPE_AUTO );

	// Push down the final amount.
	Vector3 down = monsterMove->state.origin;
	down.z -= stepSize + (float)MM_STEP_GROUND_DIST;

	// Trace down to the step floor.
	trace = SVG_MMove_Trace( monsterMove->state.origin, monsterMove->mins, monsterMove->maxs, down, monsterMove->monster, CONTENTS_NONE, MM_SHAPE_AUTO );
	if ( !trace.allsolid ) {
		// WID: Use proper stair step checking.
		if ( MMove_CheckStep( monsterMove, &trace ) ) {
			// Only an upwards jump is a stair clip.
			if ( monsterMove->state.velocity.z > 0.f ) {
				monsterMove->step.clipped = true;
			}
			// Step down to the new found ground.
			MMove_StepDown( monsterMove, &trace );
		}
	}

	up = monsterMove->state.origin;

	// Decide which one went farther, use 'Vector2Length', ignore the Z axis.
	const float down_dist = ( downOrigin.x - startOrigin.x ) * ( downOrigin.x - startOrigin.x ) + ( downOrigin.y - startOrigin.y ) * ( downOrigin.y - startOrigin.y );
	const float up_dist = ( up.x - startOrigin.x ) * ( up.x - startOrigin.x ) + ( up.y - startOrigin.y ) * ( up.y - startOrigin.y );

	// The elevated candidate is valid only when the downward probe found a
	// walkable landing surface. A missed/all-solid probe must restore the
	// original slide result; otherwise the temporary step-up becomes a jump.
	const bool hasWalkableLanding = !trace.allsolid && trace.fraction < 1.f && trace.plane.normal[ 2 ] >= MM_MIN_STEP_NORMAL;
	if ( down_dist > up_dist || !hasWalkableLanding ) {
		monsterMove->state.origin = downOrigin;
		monsterMove->state.velocity = downVelocity;
	}
	// [Paril-KEX] NB: this line being commented is crucial for ramp-jumps to work.
	// thanks to Jitspoe for pointing this one out.
	else {// if (ps->pmove.pm_flags & PMF_ON_GROUND)
		acceptedStepUp = true;
		//!! Special case
		// if we were walking along a plane, then we need to copy the Z over
		monsterMove->state.velocity.z = downVelocity.z;
	}

	// Paril: step down stairs/slopes
	if ( !acceptedStepUp && ( monsterMove->state.mm_flags & MMF_ON_GROUND ) && !( monsterMove->state.mm_flags & MMF_ON_LADDER ) &&
        ( monsterMove->liquid.level < cm_liquid_level_t::LIQUID_WAIST || ( /*!( pm->cmd.buttons & BUTTON_JUMP ) &&*/ monsterMove->state.velocity.z <= 0 ) ) ) {
        // Use policy for step height.
		Vector3 downOffset = { 0.f, 0.f, (float)policy.max_obstruction_jump_height };
        Vector3 down = QM_Vector3Subtract(monsterMove->state.origin, downOffset);
		// Keep slope-follow/down-step probe on native shape for ramp continuity and jump behavior.
		trace = SVG_MMove_Trace( monsterMove->state.origin, monsterMove->mins, monsterMove->maxs, down, monsterMove->monster, CONTENTS_NONE, MM_SHAPE_AUTO );

		// WID: Use proper stair step checking.
		// Check for stairs:
		if ( MMove_CheckStep( monsterMove, &trace ) ) {
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
		#if 0
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
		#endif
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

	// Keep render/network state synchronized with the authoritative currentAngles update.
	SVG_Util_SetEntityAngles( ent, ent->currentAngles, true );
}