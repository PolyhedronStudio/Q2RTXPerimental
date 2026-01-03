/*********************************************************************
*
*
*	SVGame: (Entity/MoveType-Specific Mechanic -) Physics:
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_physics.h"
#include "svgame/svg_utils.h"



/**
*	@brief	The pushed structure is used to keep track of entities being moved by a pushmove type entity.
**/
struct pushed_t {
	//! The entity being pushed.
	svg_base_edict_t *ent;
	//! The original origin before the push.
	Vector3 origin;
	//! The original angles before the push.
	Vector3 angles;
	//#if USE_SMOOTH_DELTA_ANGLES
	double yawDelta;
	//#endif
};

/**
*	@brief	State during a pushmovefor pushed entities.
**/
struct pushed_state_t {
	//! Array of pushed entities.
	static pushed_t pushed[ MAX_EDICTS ];
	//! ``State and pointer into the pushed array`` for the ``current pushed entity``.
	pushed_t *pushedPtr = nullptr;
	//! The ``obstacle`` entity ``blocking`` movement.
	svg_base_edict_t *obstacle = nullptr;
} pushedState = {};
//! Static instance of pushed entities.
pushed_t pushed_state_t::pushed[ MAX_EDICTS ] = {};


/*

pushmove objects do not obey gravity, and do not interact with each other or trigger fields, but block normal movement and push normal objects when they move.

onground is set for toss objects when they come to a complete rest.  it is set for steping or walking objects

doors, plats, etc are SOLID_BSP, and MOVETYPE_PUSH
bonus items are SOLID_TRIGGER touch, and MOVETYPE_TOSS
corpses are SOLID_NOT and MOVETYPE_TOSS
crates are SOLID_BOUNDS_BOX and MOVETYPE_TOSS
walking monsters are SOLID_SLIDEBOX and MOVETYPE_STEP
flying/floating monsters are SOLID_SLIDEBOX and MOVETYPE_FLY

solid_edge items only clip against bsp models.

*/
//! Epsilon value for stopping clipped velocity.
static constexpr double CLIPVELOCITY_STOP_EPSILON = 0.1;

//! Return bit set in case of having clipped to a floor plane.
static constexpr int32_t CLIPVELOCITY_CLIPPED_FLOOR = BIT( 0 );
//! Return bit set in case of having clipped to a step plane. (Straight up wall.)
static constexpr int32_t CLIPVELOCITY_CLIPPED_STEP = BIT( 1 );
//! Return bit set in case of having resulting in a dead stop. ( Corner or such. )
static constexpr int32_t CLIPVELOCITY_CLIPPED_DEAD_STOP = BIT( 2 );
//! Return bit set in case of being trapped inside a solid.
static constexpr int32_t CLIPVELOCITY_CLIPPED_STUCK_SOLID = BIT( 3 );
//! Return bit set in case of num of clipped planes overflowing. ( Should never happen. )
static constexpr int32_t CLIPVELOCITY_CLIPPED_OVERFLOW = BIT( 4 );
//! Return bit set in case of stopping because if the crease not matching 2 planes.
static constexpr int32_t CLIPVELOCITY_CLIPPED_CREASE_STOP = BIT( 5 );


/**
*
*
*
*	Generic Physics Functions:
*
*
*
**/
/**
*	@brief	Applies a frame's worth of the gravity into the direction of the gravity vector for this entity.
* 	@param	ent The entity to apply gravity to.
**/
void SVG_AddGravity( svg_base_edict_t *ent ) {
	ent->velocity += ent->gravityVector * ( ent->gravity * level.gravity * gi.frame_time_s );
}

/**
*	@brief	Ensure that an entity's velocity does not exceed sv_maxvelocity.
*	@param	ent	The entity to check.
**/
void SVG_ClampEntityMaxVelocity( svg_base_edict_t *ent ) {
	// Clamp current velocity to sv_maxvelocity mins and maxs.
	ent->velocity = QM_Vector3Clamp( 
		// Current:
		ent->velocity,
		// Min:
		{ -sv_maxvelocity->value, -sv_maxvelocity->value, -sv_maxvelocity->value },
		// Max:
		{ sv_maxvelocity->value, sv_maxvelocity->value, sv_maxvelocity->value }
	);
}
/**
*	@brief Redirects the input velocity vector 'in' along the plane defined by the 'normal' vector,
*	@param in The input velocity vector to be clipped.
*	@param normal The normal vector defining the clipping plane.
*	@param out The output velocity vector after clipping.
*	@param overbounce The overbounce factor to apply during clipping.
*	@return The blocked flags indicating the type of clipping that occurred.
**/
const int32_t SVG_Physics_ClipVelocity( Vector3 &in, vec3_t normal, Vector3 &out, const double overbounce ) {
	// Determine if clip got 'blocked'.
	int32_t blocked = 0;
	// Floor:
	if ( normal[ 2 ] > 0 ) {
		blocked |= CLIPVELOCITY_CLIPPED_FLOOR;
	}
	// Step/Wall:
	if ( !normal[ 2 ] ) {
		blocked |= CLIPVELOCITY_CLIPPED_STEP;
	}

	// Backoff factor.
	const double backOff = QM_Vector3DotProduct( in, normal ) * overbounce;
	// Calculate and apply change.
	for ( int32_t i = 0; i < 3; i++ ) {
		// Calculate change for axis.
		const float change = normal[ i ] * backOff;
		// Apply velocity change.
		out[ i ] = in[ i ] - change;
		// Halt if we're past epsilon.
		if ( out[ i ] > -CLIPVELOCITY_STOP_EPSILON && out[ i ] < CLIPVELOCITY_STOP_EPSILON ) {
			out[ i ] = 0;
		}
	}
	return blocked;
}



/**
*
*
*
*	Entity (Clip -)Test/Tracing:
*
*
*
**/
/**
*	@brief	Fetch the clipMask for this entity; certain modifiers affect the clipping behavior of objects.
* 	@param	ent The entity to get the clip mask for.
* 	@return The contents mask to use for clipping traces against this entity.
**/
const cm_contents_t SVG_GetClipMask( const svg_base_edict_t *ent ) {
	// Get current clip mask.
	const cm_contents_t mask = ent->clipMask;

	// If none, setup a default mask based on the svFlags.
	if ( !mask ) {
		// Player clipMask:
		if ( ent->svFlags & SVF_PLAYER ) {
			return CM_CONTENTMASK_PLAYERSOLID;
		// Monster clipmasks:
		} else if ( ent->svFlags & SVF_MONSTER ) {
			return ( CM_CONTENTMASK_MONSTERSOLID );
		// Projectile clipMask:
		} else if ( ent->svFlags & SVF_PROJECTILE ) {
			return ( CM_CONTENTMASK_PROJECTILE );
		// Resort to default mask.
		} else {
			return ( CM_CONTENTMASK_SHOT & ~CONTENTS_DEADMONSTER );
		}
	}

	// Non-Solid objects (items, etc) shouldn't try to clip against players/monsters.
	if ( ent->solid == SOLID_NOT || ent->solid == SOLID_TRIGGER ) {
		return ( mask & ~( CONTENTS_MONSTER | CONTENTS_PLAYER ) );
	}
	// Monsters/Players that are also dead shouldn't clip against players/monsters.
	if ( ( ent->svFlags & ( SVF_MONSTER | SVF_PLAYER ) ) && ( ent->svFlags & SVF_DEADENTITY ) ) {
		return ( mask & ~( CONTENTS_MONSTER | CONTENTS_PLAYER ) );
	}

	return mask;
}
/**
*	@brief	Will test the entity's current position to see if it is	obstructed by anything. 
*	@note	In case of the trace yielding ENTITYNUM_NONE, the 'world' entity is returned instead.
*	@param	ent The entity to test.
*	@return	nullptr if not obstructed, otherwise the entity that is obstructing it.
**/
svg_base_edict_t *SVG_TestEntityPosition( const svg_base_edict_t *ent ) {
	// Get the clip mask for this entity.
	const cm_contents_t clipMask = SVG_GetClipMask( ent );
	// Perform a trace to test for obstructions.
	const svg_trace_t trace = SVG_Trace( ent->currentOrigin, ent->mins, ent->maxs, ent->currentOrigin, ent, clipMask );

	// Return the 'world' entity in case of being stuck inside of anything..
	// <Q2RTXP>: Note: This is a change from the original Quake 2 behavior,
	// which just returned 'world', yet we return the actual touched entity
	// based on the trace's entitynumber instead.
    if ( trace.startsolid ) {
		// If for whichever reason the entitynumber is ENTITYNUM_NONE we return world,
		// just like Quake 2 does.
		if ( trace.entityNumber == ENTITYNUM_NONE ) {
			return g_edict_pool.EdictForNumber( ENTITYNUM_WORLD );
		} else {
			return g_edict_pool.EdictForNumber( trace.entityNumber );
		}
    }

	// Otherwise, return nullptr, we aren't being obstructed..
    return nullptr;
}
/**
*	@brief	Two entities have collided; run their touch functions.
*	@param	e1 The first entity.
*	@param	trace The trace result containing information about the collision.
*	@note	The entity in the trace is the second entity.
**/
void SVG_Impact( svg_base_edict_t *e1, svg_trace_t *trace ) {
	// Get second entity.
	svg_base_edict_t *e2 = trace->ent;

	// Dispatch touch functions.
	if ( e1 != nullptr && e1->HasTouchCallback() && e1->solid != SOLID_NOT ) {
		e1->DispatchTouchCallback( e2, &trace->plane, trace->surface );
	}
	// Dispatch touch for second entity.
	if ( e2 != nullptr && e2->HasTouchCallback() && e2->solid != SOLID_NOT ) {
		e2->DispatchTouchCallback( e1, NULL, NULL );
	}
}



/**
*
*
*
*	Entity Thinking:
*
*
*
**/
/**
*	@brief	Runs an entity's thinking code for this frame if necessary.
**/
static const bool SVG_RunEntityThink( svg_base_edict_t *ent ) {
	// Get next think time.
	const QMTime thinktime = ent->nextthink;
	// Check if we need to think this frame.
    if ( thinktime <= 0_ms ) {
        return false;
    }
	// Not time yet.
    if ( thinktime > level.time ) {
        return true;
    }
	// Clear next think.
    ent->nextthink = 0_ms;
	// Sanity check.
    if ( !ent->HasThinkCallback() ) {
        // WID: Useful to output exact information about what entity we are dealing with here, that'll help us fix the problem :-).
        gi.error( "[ entityNumber(%d), inUse(%s), classname(%s), targetname(%s), luaName(%s), (nullptr) ent->think ]\n",
            ent->s.number, ( ent->inUse != false ? "true" : "false" ), (const char*)ent->classname, (const char *)ent->targetname, (const char *)ent->luaProperties.luaName);
        // Failed.
        return false;
    }
	// Call the think function.
    ent->DispatchThinkCallback();
	// We're succesfully done.
    return true;
}



/**
*
*
*
*	Entity Physics "SlideBox" Move Mechanics:
*
*
*
**/
// Maximum number of clipping planes to consider.
static constexpr int32_t MAX_CLIP_PLANES = 5;

/**
*	@brief	The basic solid body movement clip that slides along multiple planes
*	@return	Returns the clipflags if the velocity was modified (hit something solid)
*			1 = floor
*			2 = wall / step
*			4 = dead stop
**/
static const int32_t SVG_SlideBox( svg_base_edict_t *ent, const double time, const cm_contents_t mask ) {
    svg_base_edict_t *hit = nullptr;
	Vector3 new_velocity = {};
    int32_t i = 0, j = 0;

	svg_trace_t trace = svg_trace_t();
    Vector3		end = {};
	double      d = 0.;

	Vector3 planes[ MAX_CLIP_PLANES ] = {};
	int32_t numplanes = 0;
	int32_t bumpcount = 0;
	int32_t numbumps = 4;
	int32_t blocked = 0;

	Vector3 dir = {};
	Vector3 original_velocity = ent->velocity; // VectorCopy( ent->velocity, original_velocity );
	Vector3 primal_velocity = ent->velocity; // VectorCopy( ent->velocity, primal_velocity );

	double time_left = time;

	ent->groundInfo.entityNumber = ENTITYNUM_NONE;
	for ( bumpcount = 0; bumpcount < numbumps; bumpcount++ ) {
		for ( i = 0; i < 3; i++ ) {
			end[ i ] = ent->currentOrigin[ i ] + time_left * ent->velocity[ i ];
		}

		trace = SVG_Trace( ent->currentOrigin, ent->mins, ent->maxs, end, ent, mask );

		if ( trace.allsolid ) {
			// entity is trapped in another solid
			VectorClear( ent->velocity );
			return CLIPVELOCITY_CLIPPED_STUCK_SOLID;
		}

		if ( trace.fraction > 0 ) {
			// actually covered some distance
			//VectorCopy(trace.endpos, ent->s.origin);
			SVG_Util_SetEntityOrigin( ent, trace.endpos, true );
			original_velocity = ent->velocity;// VectorCopy(ent->velocity, original_velocity);
			numplanes = 0;
		}

		if ( trace.fraction == 1 ) {
			break;     // moved the entire distance
		}

		hit = trace.ent;

		if ( trace.plane.normal[ 2 ] > 0.7f ) {
			blocked |= CLIPVELOCITY_CLIPPED_FLOOR;       // floor
			if ( hit->solid == SOLID_BSP ) {
				ent->groundInfo.entityNumber = trace.entityNumber;
				ent->groundInfo.entityLinkCount = hit->linkCount;
			}
		}
		if ( !trace.plane.normal[ 2 ] ) {
			blocked |= CLIPVELOCITY_CLIPPED_STEP;       // step
		}

//
// run the impact function
//
		SVG_Impact( ent, &trace );
		if ( !ent->inUse ) {
			break;      // removed by the impact function
		}

		time_left -= time_left * trace.fraction;

		// cliped to another plane
		if ( numplanes >= MAX_CLIP_PLANES ) {
			// this shouldn't really happen
			VectorClear( ent->velocity );
			return CLIPVELOCITY_CLIPPED_OVERFLOW;
		}

		VectorCopy( trace.plane.normal, planes[ numplanes ] );
		numplanes++;

//
// modify original_velocity so it parallels all of the clip planes
//
		for ( i = 0; i < numplanes; i++ ) {
			blocked |= SVG_Physics_ClipVelocity( original_velocity, &planes[ i ].x, new_velocity, 1 );

			for ( j = 0; j < numplanes; j++ )
				if ( ( j != i ) && !VectorCompare( planes[ i ], planes[ j ] ) ) {
					if ( DotProduct( new_velocity, planes[ j ] ) < 0 )
						break;  // not ok
				}
			if ( j == numplanes )
				break;
		}

		if ( i != numplanes ) {
			// go along this plane
			VectorCopy( new_velocity, ent->velocity );
		} else {
			// go along the crease
			if ( numplanes != 2 ) {
//              gi.dprintf ("clip velocity, numplanes == %i\n",numplanes);
				VectorClear( ent->velocity );
				return CLIPVELOCITY_CLIPPED_CREASE_STOP;
			}
			CrossProduct( planes[ 0 ], planes[ 1 ], dir );
			d = DotProduct( dir, ent->velocity );
			VectorScale( dir, d, ent->velocity );
		}

//
// if original velocity is against the original velocity, stop dead
// to avoid tiny occilations in sloping corners
//
		if ( DotProduct( ent->velocity, primal_velocity ) <= 0 ) {
			VectorClear( ent->velocity );
			return blocked | CLIPVELOCITY_CLIPPED_DEAD_STOP;
		}
	}

	return blocked;
}



/**
*
*
*
*	PushMove:
*
*
*
**/
/**
*	@brief	Will attempt to push an entity by the specified vector, handling collisions and impacts.
*	@param	ent The entity to push.
* 	@param	push The vector to push the entity by.
* 	@return The trace result of the push operation.
* 	@note	If the entity is blocked during the push, its position will be set to the end position of the trace.
* 			If the entity is removed during the impact handling, it will not be re-linked.
**/
svg_trace_t SVG_PushEntity( svg_base_edict_t *ent, const Vector3& pushOffset ) {
	// The resulting trace.
    svg_trace_t trace;

	// Setup start and end positions.
	Vector3 start = ent->currentOrigin; // VectorCopy(ent->s.origin, start);
	Vector3 end = start + pushOffset; // VectorAdd(start, push, end);

retry:
    //if (ent->clipMask)
    //    mask = ent->clipMask;
    //else
    //    mask = CM_CONTENTMASK_SOLID;

	// Perform the trace.
	trace = SVG_Trace( start, ent->mins, ent->maxs, end, ent, SVG_GetClipMask( ent ) );

	SVG_Util_SetEntityOrigin( ent, trace.endpos, true ); // VectorCopy(trace.endpos, ent->s.origin);
	gi.linkentity( ent );

	if ( trace.fraction != 1.0f ) {
		// We hit something, so call the impact function.
		SVG_Impact( ent, &trace );

		// If the pushed entity went away and the pusher is still there:
		if ( !trace.ent->inUse && ent->inUse ) {
			// Move the pusher back and try again.
			SVG_Util_SetEntityOrigin( ent, start, true ); // VectorCopy( start, ent->s.origin );
			gi.linkentity( ent );
			goto retry;
		}
	}

    // PGM
    // FIXME - is this needed?
    ent->gravity = 1.0;
    // PGM

	// Touch triggers.
	if ( ent->inUse ) {
		SVG_Util_TouchTriggers( ent );
	}
	// Return the trace.
    return trace;
}


//const float SnapToEights( const float x ) {
//    // WID: Float-movement.
//    //x *= 8.0f;
//    //if (x > 0.0f)
//    //    x += 0.5f;
//    //else
//    //    x -= 0.5f;
//    //return 0.125f * (int)x;
//    return x;
//}

/**
*	@brief	Objects need to be moved back on a failed push, otherwise riders would continue to slide.
*	@param	pusher The entity that is pushing.
* 	@param	move The movement vector.
* 	@param	amove The angular movement vector.
* 	@return	true if the push was successful, false if blocked.
**/
const bool SVG_PushMover( svg_base_edict_t *pusher, const Vector3 &move, const Vector3 &amove ) {
	svg_base_edict_t *blockPtr = nullptr;
	Vector3 org2 = {};
	Vector3 move2 = {};
	Vector3 vForward = {}, vRight = {}, vUp = {};

	// Sanity check.
    if ( !pusher ) {
        return false;
    }

    // clamp the move to 1/8 units, so the position will
    // be accurate for client side prediction
    //for ( int32_t i = 0; i < 3; i++ )
    //    move[i] = SnapToEights(move[i]);

    // find the bounding box
	Vector3 mins = pusher->absMin + move;
	Vector3 maxs = pusher->absMax + move;
    //for (int32_t i = 0; i < 3; i++) {
    //    mins[i] = pusher->absMin[i] + move[i];
    //    maxs[i] = pusher->absMax[i] + move[i];
    //}

	/**
	*	We need this for pushing things later.
	**/
	Vector3 org = QM_Vector3Negate( amove );// VectorNegate( amove, org );
	QM_AngleVectors( org, &vForward, &vRight, &vUp );

// save the pusher's original position
    pushedState.pushedPtr->ent = pusher;
	SVG_Util_SetEntityOrigin( pushedState.pushedPtr->ent, pusher->currentOrigin, true ); // VectorCopy(pusher->s.origin, pushed_p->origin);
	SVG_Util_SetEntityAngles( pushedState.pushedPtr->ent, pusher->currentAngles, true ); // VectorCopy(pusher->s.angles, pushed_p->angles);
    //#if USE_SMOOTH_DELTA_ANGLES
	if ( pusher->client ) {
		pushedState.pushedPtr->yawDelta = pusher->client->ps.pmove.delta_angles[ YAW ];
	}
    //#endif
	pushedState.pushedPtr++;

// move the pusher to it's final position
	SVG_Util_SetEntityOrigin( pusher, pusher->currentOrigin + move, true ); //
	SVG_Util_SetEntityAngles( pusher, pusher->currentAngles + amove, true ); //
	//pusher->currentOrigin += move; // VectorAdd(pusher->s.origin, move, pusher->s.origin);
	//pusher->currentAngles += amove; // VectorAdd(pusher->s.angles, amove, pusher->s.angles);
    gi.linkentity(pusher);

// see if any solid entities are inside the final position
    svg_base_edict_t *checkPtr = g_edict_pool.EdictForNumber( 1 );
    for ( int32_t e = 1; e < globals.edictPool->num_edicts; e++, checkPtr = g_edict_pool.EdictForNumber( e ) ) {
		// Ensure it is in use.
		if ( !checkPtr->inUse ) {
			continue;
		}
		if ( checkPtr->movetype == MOVETYPE_PUSH
			|| checkPtr->movetype == MOVETYPE_STOP
			|| checkPtr->movetype == MOVETYPE_NONE
			|| checkPtr->movetype == MOVETYPE_NOCLIP ) {
			continue;
		}

		if ( !checkPtr->area.prev /*!checkPtr->isLinked*/ ) {
			continue;       // not linked in anywhere
		}

        // If the entity is standing on the pusher, it will definitely be moved.
        if ( checkPtr->groundInfo.entityNumber != pusher->s.number ) {
            // see if the ent needs to be tested
			if ( checkPtr->absMin[ 0 ] >= maxs[ 0 ]
				|| checkPtr->absMin[ 1 ] >= maxs[ 1 ]
				|| checkPtr->absMin[ 2 ] >= maxs[ 2 ]
				|| checkPtr->absMax[ 0 ] <= mins[ 0 ]
				|| checkPtr->absMax[ 1 ] <= mins[ 1 ]
				|| checkPtr->absMax[ 2 ] <= mins[ 2 ] ) {
				continue;
			}

            // see if the ent's bbox is inside the pusher's final position
			if ( !SVG_TestEntityPosition( checkPtr ) ) {
				continue;
			}
        }

        if ( ( pusher->movetype == MOVETYPE_PUSH ) || ( checkPtr->groundInfo.entityNumber == pusher->s.number ) ) {
            // move this entity
            pushedState.pushedPtr->ent = checkPtr;
			pushedState.pushedPtr->origin = checkPtr->currentOrigin; // VectorCopy( check->s.origin, pushed_p->origin );
			pushedState.pushedPtr->angles = checkPtr->currentAngles; // VectorCopy( check->s.angles, pushed_p->angles );
            //#if USE_SMOOTH_DELTA_ANGLES
			if ( checkPtr->client ) {
				pushedState.pushedPtr->yawDelta = checkPtr->client->ps.pmove.delta_angles[ YAW ];
			}
            //#endif
			pushedState.pushedPtr++;

            // Try moving the contacted entity.
			SVG_Util_SetEntityOrigin( checkPtr, checkPtr->currentOrigin + move, true ); ////checkPtr->currentOrigin += move; // VectorAdd( check->s.origin, move, check->s.origin );
            //#if USE_SMOOTH_DELTA_ANGLES
            if ( checkPtr->client ) {
                // FIXME: doesn't rotate monsters?
                // FIXME: skuller: needs client side interpolation
                //check->client->ps.pmove.delta_angles[YAW] += /*ANGLE2SHORT*/(amove[YAW]);
                checkPtr->client->ps.pmove.delta_angles[ YAW ] = QM_AngleMod( checkPtr->client->ps.pmove.delta_angles[ YAW ] + amove[ YAW ] );
            }
            //#endif

            // Figure movement due to the pusher's amove.
			org = checkPtr->currentOrigin - pusher->currentOrigin; // VectorSubtract( checkPtr->s.origin, pusher->s.origin, org );
			org2[ 0 ] = DotProduct( org, vForward );
			org2[ 1 ] = -DotProduct( org, vRight );
			org2[ 2 ] = DotProduct( org, vUp );
			move2 = org2 - org; // VectorSubtract( org2, org, move2 );
			SVG_Util_SetEntityOrigin( checkPtr, checkPtr->currentOrigin + move2, true ); //checkPtr->currentOrigin += move2;// VectorAdd( checkPtr->s.origin, move2, checkPtr->s.origin );

            // may have pushed them off an edge
            if ( checkPtr->groundInfo.entityNumber != pusher->s.number ) {
                checkPtr->groundInfo.entityNumber = ENTITYNUM_NONE;
            }

            blockPtr = SVG_TestEntityPosition(checkPtr);
            if (!blockPtr) {
				//SVG_Util_SetEntityOrigin( checkPtr, checkPtr->currentOrigin, true );
                // pushed ok
                gi.linkentity(checkPtr);
                // impact?
                continue;
            }

            // if it is ok to leave in the old position, do it
            // this is only relevent for riding entities, not pushed
            // FIXME: this doesn't acount for rotation
			SVG_Util_SetEntityOrigin( checkPtr, checkPtr->currentOrigin - move, true ); //			checkPtr->currentOrigin -= move; //VectorSubtract(checkPtr->s.origin, move, checkPtr->s.origin);
            blockPtr = SVG_TestEntityPosition( checkPtr );
            if ( !blockPtr ) {
				pushedState.pushedPtr--;
                continue;
            }
        }

        // save off the obstacle so we can call the block function
		pushedState.obstacle = checkPtr;

        // move back any entities we already moved
        // go backwards, so if the same entity was pushed
        // twice, it goes back to the original position
        for ( int32_t i = ( pushedState.pushedPtr - pushedState.pushed ) - 1; i >= 0; i-- ) {
			pushed_t *p = &pushedState.pushed[ i ];
			SVG_Util_SetEntityOrigin( p->ent, p->origin, true ); // VectorCopy(p->origin, p->ent->s.origin);
			SVG_Util_SetEntityAngles( p->ent, p->angles, true ); // VectorCopy(p->angles, p->ent->s.angles);

			//#if USE_SMOOTH_DELTA_ANGLES
			if ( p->ent->client ) {
				p->ent->client->ps.pmove.delta_angles[ YAW ] = p->yawDelta;
			}
			//#endif
			gi.linkentity( p->ent );
		}
        return false;
    }

//FIXME: is there a better way to handle this?
    // see if anything we moved has touched a trigger
	for ( int32_t i = ( pushedState.pushedPtr - pushedState.pushed ) - 1; i >= 0; i-- ) {
		SVG_Util_TouchTriggers( pushedState.pushed[ i ].ent );
	}

    return true;
}



/**
*
*
*
*	Entity Pusher Core Mechanics:
*
*	Brush-model objects don't interact with each other, but push all box objects.
*
*
**/
static void SV_Physics_Pusher( svg_base_edict_t *ent ) {
    svg_base_edict_t     *part, *mv;

    // If not a team captain, so movement will be handled elsewhere.
	if ( ent->flags & FL_TEAMSLAVE ) {
		return;
	}

    /**
	*	Make sure all team slaves can move before commiting
    *	any moves or calling any think functions
    *	if the move is blocked, all moved objects will be backed out
	**/
	// Reinitialize the pushedState properly.
	pushedState = {};
	// Ensure to clear the static pushed array.
	std::memset( pushedState.pushed, 0, sizeof( pushedState.pushed ) );

	// Setup the initial pushPtr.
	pushedState.pushedPtr = pushedState.pushed;
	//pushedState.obstacle = nullptr;
#if 1
retry:
#endif
	// Try moving all parts:
	pushedState.pushedPtr = pushedState.pushed;
	for ( part = ent; part; part = part->teamchain ) {
		// See if it needs to move:
		if ( !VectorEmpty( part->velocity ) || !VectorEmpty( part->avelocity ) ) {
			// object is moving
			const Vector3 move = part->velocity * gi.frame_time_s; //VectorScale(part->velocity, gi.frame_time_s, move);
			const Vector3 amove = part->avelocity * gi.frame_time_s; //VectorScale(part->avelocity, gi.frame_time_s, amove);

			// Try to move it.
			if ( !SVG_PushMover( part, move, amove ) ) {
				// Oh noes, seems that the move was blocked, faack.
				break;
			}
		}
	}
	// Sanity check for pushed_p overflow.
	if ( pushedState.pushedPtr > &pushedState.pushed[ MAX_EDICTS ] ) {
		gi.error( "pushed_p > &pushed[MAX_EDICTS], memory corrupted" );
	}

	// Did we get blocked?
	if ( part ) {
		// The move failed, bump all nextthink times and back out moves.
		for ( mv = ent; mv; mv = mv->teamchain ) {
			if ( mv->nextthink > 0_ms ) {
				mv->nextthink += FRAME_TIME_S;
			}
		}

		// if the pusher has a "blocked" function, call it
		// otherwise, just stay in place until the obstacle is gone
		if ( part->HasBlockedCallback() ) {
			part->DispatchBlockedCallback( pushedState.obstacle );
		}
		#if 1
			// if the pushed entity went away and the pusher is still there
			if ( pushedState.obstacle && !pushedState.obstacle->inUse && part->inUse ) {
				goto retry;
			}
		#endif
	} else {
		// the move succeeded, so call all think functions
		for ( part = ent; part; part = part->teamchain ) {
			SVG_RunEntityThink( part );
		}
	}
}



/**
*
*
*
*	Entity's without Physics still 'Think':
*
*
**/
/**
*	@brief	Non moving objects can only think.
**/
void SVG_Physics_None( svg_base_edict_t *ent ) {
// regular thinking
    SVG_RunEntityThink( ent );
}



/**
*
*
*
*	Entity Physics NoClip Mechanics:
*
*	A moving object that doesn't obey physics.
*
*
**/
void SVG_Physics_Noclip( svg_base_edict_t *ent ) {
// regular thinking
	if ( !SVG_RunEntityThink( ent ) ) {
		return;
	}
	if ( !ent->inUse ) {
		return;
	}

	// Calculate new origin.
	const Vector3 newOrigin = QM_Vector3MultiplyAdd( ent->currentOrigin, gi.frame_time_s, ent->velocity ); // VectorMA(ent->s.origin, FRAMETIME,  ent->velocity, ent->s.origin);
	// Apply new origin.
	SVG_Util_SetEntityOrigin( ent, newOrigin, true );
	// Calculate new angles.
	const Vector3 newAngles = QM_Vector3MultiplyAdd( ent->currentAngles, gi.frame_time_s, ent->avelocity ); // VectorMA(ent->s.angles, FRAMETIME, ent->avelocity, ent->s.angles);
	// Apply new angles.
	SVG_Util_SetEntityAngles( ent, newAngles, true );

	// Link entity.
	gi.linkentity( ent );
}



/**
*
*
*
*	Entity Physics 'Toss', 'Bounce', and 'Fly/Swim/Float' Mechanics:
*
*	When onground, do nothing. (Exception for 'Fly/Swim/Float' movetypes.)
*
*
**/
void SVG_Physics_Toss( svg_base_edict_t *ent ) {
    svg_trace_t     trace;
    Vector3     move;
    float       backoff;
    svg_base_edict_t     *slave;
    int         wasinwater;
    int         isinwater;
    Vector3     old_origin;

// regular thinking
    SVG_RunEntityThink(ent);
    if ( !ent->inUse ) {
        return;
    }

    // if not a team captain, so movement will be handled elsewhere
    if ( ent->flags & FL_TEAMSLAVE ) {
        return;
    }

    if ( ent->velocity[ 2 ] > 0 ) {
        ent->groundInfo.entityNumber = ENTITYNUM_NONE;
    }

// check for the groundentity going away
    if ( ent->groundInfo.entityNumber != ENTITYNUM_NONE ) {
		svg_base_edict_t *groundEntity = g_edict_pool.EdictForNumber( ent->groundInfo.entityNumber );
        if ( ( groundEntity && !groundEntity->inUse ) ) {
            ent->groundInfo.entityNumber = ENTITYNUM_NONE;
        } else if ( !groundEntity ) {
            ent->groundInfo.entityNumber = ENTITYNUM_NONE;
		}
    }

// if onground, return without moving
    if ( ent->groundInfo.entityNumber != ENTITYNUM_NONE && ent->gravity > 0.0f ) {  // PGM - gravity hack
        if ( ent->svFlags & SVF_MONSTER ) {
            M_CatagorizePosition( ent, ent->currentOrigin, ent->liquidInfo.level, ent->liquidInfo.type );
            M_WorldEffects( ent );
        }

        return;
    }

    VectorCopy(ent->currentOrigin, old_origin);

	SVG_ClampEntityMaxVelocity(ent);

// add gravity.
	if ( ent->movetype != MOVETYPE_FLY && ent->movetype != MOVETYPE_FLYMISSILE ) {
		SVG_AddGravity( ent );
	}

// Move angles
    // Calculate new angles.
	const Vector3 newAngles = QM_Vector3MultiplyAdd( ent->currentAngles, FRAMETIME, ent->avelocity ); // VectorMA(ent->s.angles, FRAMETIME, ent->avelocity, ent->s.angles);
	// Apply new angles.
	SVG_Util_SetEntityAngles( ent, newAngles, true );
// Move origin.
	move = QM_Vector3Scale( ent->velocity, FRAMETIME ); // VectorScale( ent->velocity, FRAMETIME, move );
    trace = SVG_PushEntity(ent, move);
	if ( !ent->inUse ) {
		return;
	}

    if (trace.fraction < 1) {
		if ( ent->movetype == MOVETYPE_BOUNCE ) {
			backoff = 1.5f;
		} else {
			backoff = 1;
		}
		SVG_Physics_ClipVelocity( ent->velocity, trace.plane.normal, ent->velocity, backoff );

        // Stop if on ground.
		if ( trace.plane.normal[ 2 ] > 0.7f ) {
			if ( ent->velocity[ 2 ] < 60 || ent->movetype != MOVETYPE_BOUNCE ) {
				ent->groundInfo.entityNumber = trace.entityNumber;
				ent->groundInfo.entityLinkCount = trace.ent->linkCount;
				VectorClear( ent->velocity );
				VectorClear( ent->avelocity );
			}
		}

//      if (ent->touch)
//          ent->touch (ent, trace.ent, &trace.plane, trace.surface);
    }

// check for water transition
    wasinwater = (ent->liquidInfo.type & CM_CONTENTMASK_LIQUID);
    ent->liquidInfo.type = gi.pointcontents( &ent->s.origin );
    isinwater = ent->liquidInfo.type & CM_CONTENTMASK_LIQUID;

	if ( isinwater ) {
		ent->liquidInfo.level = cm_liquid_level_t::LIQUID_FEET;
	} else {
		ent->liquidInfo.level = cm_liquid_level_t::LIQUID_NONE;
	}

	// Play water splash sounds.
    const qhandle_t water_sfx_index = gi.soundindex( SG_RandomResourcePath( "world/water_land_splash", ".wav", 0, 8 ).c_str() );
    if ( !wasinwater && isinwater ) {
        gi.positioned_sound( &old_origin, g_edict_pool.EdictForNumber( 0 ), CHAN_AUTO, water_sfx_index, 1, 1, 0);
    } else if ( wasinwater && !isinwater ) {
        gi.positioned_sound( &ent->s.origin, g_edict_pool.EdictForNumber( 0 ), CHAN_AUTO, water_sfx_index, 1, 1, 0);
    }

// move teamslaves
    for (slave = ent->teamchain; slave; slave = slave->teamchain) {
		SVG_Util_SetEntityOrigin( slave, ent->currentOrigin, true );
        //VectorCopy(ent->s.origin, slave->s.origin);
        gi.linkentity(slave);
    }
}

/**
*
*
*
*	Entity Physics 'Step' Mechanics:
*
*	When onground, do nothing. (Exception for 'Fly/Swim/Float' movetypes.)
*
*
**/

/*
=============
SV_Physics_Step

Monsters freefall when they don't have a ground entity, otherwise
all movement is done with discrete steps.

This is also used for objects that have become still on the ground, but
will fall if the floor is pulled out from under them.
FIXME: is this true?
=============
*/

//FIXME: hacked in for E3 demo
static constexpr double sv_stopspeed		= 100.;
static constexpr double sv_friction			= 6.;
static constexpr double sv_waterfriction	= 1.;

/**
*	@brief	Applies rotational friction to an entity's avelocity.
**/
void SVG_AddRotationalFriction( svg_base_edict_t *ent ) {
	// Calculate new angles.
	const Vector3 newAngles = QM_Vector3MultiplyAdd( ent->currentAngles, FRAMETIME, ent->avelocity ); // VectorMA( ent->currentAngles, FRAMETIME, ent->avelocity, ent->s.angles );
	// Apply new angles.
	SVG_Util_SetEntityAngles( ent, newAngles, true );

	const double adjustment = FRAMETIME * sv_stopspeed * sv_friction;
	for ( int32_t n = 0; n < 3; n++ ) {
		if ( ent->avelocity[ n ] > 0 ) {
			ent->avelocity[ n ] -= adjustment;
			if ( ent->avelocity[ n ] < 0 ) {
				ent->avelocity[ n ] = 0;
			}
		} else {
			ent->avelocity[ n ] += adjustment;
			if ( ent->avelocity[ n ] > 0 ) {
				ent->avelocity[ n ] = 0;
			}
		}
	}
}

/**
*	@brief	Handles physics for entities that move by stepping.
**/
void SV_Physics_Step( svg_base_edict_t *ent ) {
    bool	wasonground = false;
    bool	hitsound = false;
    float *vel = nullptr;
    double speed = 0., newspeed = 0., control = 0.;
    double friction = 0.;
    svg_base_edict_t *groundentity;
    cm_contents_t mask = SVG_GetClipMask( ent );

    // airborne monsters should always check for ground
    if ( ent->groundInfo.entityNumber == ENTITYNUM_NONE ) {
        M_CheckGround( ent, mask );
    }

    groundentity = g_edict_pool.EdictForNumber( ent->groundInfo.entityNumber );

	SVG_ClampEntityMaxVelocity( ent );

    if ( groundentity ) {
        wasonground = true;
    } else {
        wasonground = false;
    }

    if ( ent->avelocity[ 0 ] || ent->avelocity[ 1 ] || ent->avelocity[ 2 ] ) {
        SVG_AddRotationalFriction( ent );
    }

    // FIXME: figure out how or why this is happening
    //if ( std::isnan( ent->velocity[ 0 ] ) || std::isnan( ent->velocity[ 1 ] ) || std::isnan( ent->velocity[ 2 ] ) )
    //if ( std::isnan( ent->velocity[ 0 ] ) || std::isnan( ent->velocity[ 1 ] ) || std::isnan( ent->velocity[ 2 ] ) )
    //    ent->velocity = {};

    // add gravity except:
    //   flying monsters
    //   swimming monsters who are in the water
    if ( !wasonground )
        if ( !( ent->flags & FL_FLY ) )
            if ( !( ( ent->flags & FL_SWIM ) && ( ent->liquidInfo.level > LIQUID_WAIST ) ) ) {
                //if ( ent->velocity[ 2 ] < level.gravity * -0.1f )
                if ( ent->velocity[ 2 ] < sv_gravity->value * -0.1f ) {
                    hitsound = true;
                }
                if ( ent->liquidInfo.level != LIQUID_UNDER ) {
                    SVG_AddGravity( ent );
                }
            }

    // friction for flying monsters that have been given vertical velocity
    if ( ( ent->flags & FL_FLY ) && ( ent->velocity[ 2 ] != 0 ) /*&& !( ent->monsterinfo.aiflags & AI_ALTERNATE_FLY )*/ ) {
        speed = fabsf( ent->velocity[ 2 ] );
        //control = speed < sv_stopspeed->value ? sv_stopspeed->value : speed;
        control = speed < sv_stopspeed ? sv_stopspeed : speed;
        friction = sv_friction / 3;
        newspeed = speed - ( gi.frame_time_s * control * friction );
        if ( newspeed < 0 ) {
            newspeed = 0;
        }
        newspeed /= speed;
        ent->velocity[ 2 ] *= newspeed;
    }

    // friction for swimming monsters that have been given vertical velocity
    if ( ( ent->flags & FL_SWIM ) && ( ent->velocity[ 2 ] != 0 ) /*&& !( ent->monsterinfo.aiflags & AI_ALTERNATE_FLY )*/ ) {
        speed = fabsf( ent->velocity[ 2 ] );
        //control = speed < sv_stopspeed->value ? sv_stopspeed->value : speed;
        control = speed < sv_stopspeed ? sv_stopspeed : speed;
        newspeed = speed - ( gi.frame_time_s * control * sv_waterfriction * (float)ent->liquidInfo.level );
        if ( newspeed < 0 )
            newspeed = 0;
        newspeed /= speed;
        ent->velocity[ 2 ] *= newspeed;
    }

    if ( ent->velocity[ 2 ] || ent->velocity[ 1 ] || ent->velocity[ 0 ] ) {
        // apply friction
        if ( ( wasonground || ( ent->flags & ( FL_SWIM | FL_FLY ) ) ) /*&& !( ent->monsterinfo.aiflags & AI_ALTERNATE_FLY )*/ ) {
            vel = &ent->velocity[0];
            speed = sqrtf( vel[ 0 ] * vel[ 0 ] + vel[ 1 ] * vel[ 1 ] );
            if ( speed ) {
                friction = sv_friction;

                // Paril: lower friction for dead monsters
                if ( ent->lifeStatus )
                    friction *= 0.5f;

                //control = speed < sv_stopspeed->value ? sv_stopspeed->value : speed;
                control = speed < sv_stopspeed ? sv_stopspeed : speed;
                newspeed = speed - gi.frame_time_s * control * friction;

                if ( newspeed < 0 )
                    newspeed = 0;
                newspeed /= speed;

                vel[ 0 ] *= newspeed;
                vel[ 1 ] *= newspeed;
            }
        }

        //Vector3 old_origin = ent->s.origin;
		Vector3 old_origin = ent->currentOrigin;

        SVG_SlideBox( ent, gi.frame_time_s, mask );

        SVG_Util_TouchProjectiles( ent, old_origin );

        M_CheckGround( ent, mask );

        gi.linkentity( ent );

        // ========
        // PGM - reset this every time they move.
        //       SVG_touchtriggers will set it back if appropriate
        ent->gravity = 1.0;
        // ========

        // [Paril-KEX] this is something N64 does to avoid doors opening
        // at the start of a level, which triggers some monsters to spawn.
        if ( /*!level.is_n64 || */level.time > FRAME_TIME_S ) {
            SVG_Util_TouchTriggers( ent );
        }

        if ( !ent->inUse ) {
            return;
        }

        if ( ent->groundInfo.entityNumber != ENTITYNUM_NONE ) {
            if ( !wasonground ) {
                if ( hitsound ) {
                    SVG_Util_AddEvent( ent, EV_OTHER_FOOTSTEP, 0 );
                }
            }
        }
    }

    if ( !ent->inUse ) { // PGM g_touchtrigger free problem
        return;
    }

    if ( ent->svFlags & SVF_MONSTER ) {
        M_CatagorizePosition( ent, ent->currentOrigin, ent->liquidInfo.level, ent->liquidInfo.type );
        M_WorldEffects( ent );
    }

    // regular thinking
    SVG_RunEntityThink( ent );

//    bool        wasonground = false;
//    bool        hitsound = false;
//    float       *vel = nullptr;
//    float       speed = 0.f, newspeed = 0.f, control = 0.f;
//    float       friction = 0.f;
//    svg_base_edict_t     *groundentity = nullptr;
//    cm_contents_t  mask = SVG_GetClipMask( ent );
//
//    // airborn monsters should always check for ground
//    if ( !ent->groundentity ) {
//        M_CheckGround( ent, mask );
//    }
//
//    groundentity = ent->groundentity;
//
//    SV_ClampVelocityLimit(ent);
//
//    if (groundentity)
//        wasonground = true;
//    else
//        wasonground = false;
//
//    if (!VectorEmpty(ent->avelocity))
//        SVG_AddRotationalFriction(ent);
//
//    // add gravity except:
//    //   flying monsters
//    //   swimming monsters who are in the water
//    if (! wasonground)
//        if (!(ent->flags & FL_FLY))
//            if (!((ent->flags & FL_SWIM) && (ent->liquidInfo.level > 2))) {
//                if (ent->velocity[2] < sv_gravity->value * -0.1f)
//                    hitsound = true;
//                if (ent->liquidInfo.level == 0)
//                    SVG_AddGravity(ent);
//            }
//
//    // friction for flying monsters that have been given vertical velocity
//    if ((ent->flags & FL_FLY) && (ent->velocity[2] != 0)) {
//        speed = fabsf(ent->velocity[2]);
//        control = speed < sv_stopspeed ? sv_stopspeed : speed;
//        friction = sv_friction / 3;
//        newspeed = speed - (FRAMETIME * control * friction);
//        if (newspeed < 0)
//            newspeed = 0;
//        newspeed /= speed;
//        ent->velocity[2] *= newspeed;
//    }
//
//    // friction for flying monsters that have been given vertical velocity
//    if ((ent->flags & FL_SWIM) && (ent->velocity[2] != 0)) {
//        speed = fabsf(ent->velocity[2]);
//        control = speed < sv_stopspeed ? sv_stopspeed : speed;
//        newspeed = speed - (FRAMETIME * control * sv_waterfriction * (float)ent->liquidInfo.level);
//        if (newspeed < 0)
//            newspeed = 0;
//        newspeed /= speed;
//        ent->velocity[2] *= newspeed;
//    }
//
//    if (ent->velocity[2] || ent->velocity[1] || ent->velocity[0]) {
//        // apply friction
//        // let dead monsters who aren't completely onground slide
//        if ((wasonground) || (ent->flags & (FL_SWIM | FL_FLY)))
//            if (!(ent->health <= 0.0f && !M_CheckBottom(ent))) {
//                vel = ent->velocity;
//                speed = sqrtf(vel[0] * vel[0] + vel[1] * vel[1]);
//                if (speed) {
//                    friction = sv_friction;
//
//                    control = speed < sv_stopspeed ? sv_stopspeed : speed;
//                    newspeed = speed - FRAMETIME * control * friction;
//
//                    if (newspeed < 0)
//                        newspeed = 0;
//                    newspeed /= speed;
//
//                    vel[0] *= newspeed;
//                    vel[1] *= newspeed;
//                }
//            }
//
//        if (ent->svFlags & SVF_MONSTER)
//            mask = CM_CONTENTMASK_MONSTERSOLID;
//        else
//            mask = CM_CONTENTMASK_SOLID;
//
//        const Vector3 oldOrigin = ent->s.origin;
//        SVG_SlideBox(ent, FRAMETIME, mask);
//        SVG_Util_TouchProjectiles( ent, oldOrigin );
//
//        gi.linkentity(ent);
//        SVG_Util_TouchTriggers(ent);
//        if (!ent->inUse)
//            return;
//
//        if (ent->groundentity)
//            if (!wasonground)
//                if (hitsound)
//                    gi.sound(ent, 0, gi.soundindex("world/land01.wav"), 1, 1, 0);
//    }
//
//// regular thinking
//    SVG_RunEntityThink(ent);
}



/**
*
*
*
*	Entity Physics 'RootMotion' Mechanics:
*
*
*
**/
/**
*   @brief  For RootMotion entities.
**/
void SVG_Physics_RootMotion( svg_base_edict_t *ent ) {
    cm_contents_t mask = SVG_GetClipMask( ent );

    // airborne monsters should always check for ground
    if ( ent->groundInfo.entityNumber == ENTITYNUM_NONE ) {
        M_CheckGround( ent, mask );
    }

	SVG_ClampEntityMaxVelocity( ent );
    // regular thinking
    SVG_RunEntityThink( ent );
	return;
    bool	   wasonground;
    bool	   hitsound = false;
    float *vel;
    float	   speed, newspeed, control;
    float	   friction;
    svg_base_edict_t *groundentity;
    //..cm_contents_t mask = SVG_GetClipMask( ent );

    // airborne monsters should always check for ground
    if ( ent->groundInfo.entityNumber == ENTITYNUM_NONE ) {
        M_CheckGround( ent, mask );
    }

    groundentity = g_edict_pool.EdictForNumber( ent->groundInfo.entityNumber );

	SVG_ClampEntityMaxVelocity( ent );

    if ( groundentity ) {
        wasonground = true;
    } else {
        wasonground = false;
    }

    if ( ent->avelocity[ 0 ] || ent->avelocity[ 1 ] || ent->avelocity[ 2 ] ) {
        SVG_AddRotationalFriction( ent );
    }

    // FIXME: figure out how or why this is happening
    //if ( std::isnan( ent->velocity[ 0 ] ) || std::isnan( ent->velocity[ 1 ] ) || std::isnan( ent->velocity[ 2 ] ) )
    //if ( std::isnan( ent->velocity[ 0 ] ) || std::isnan( ent->velocity[ 1 ] ) || std::isnan( ent->velocity[ 2 ] ) )
    //    ent->velocity = {};

    // add gravity except:
    //   flying monsters
    //   swimming monsters who are in the water
    if ( !wasonground ) {
        if ( !( ent->flags & FL_FLY ) ) {
            if ( !( ( ent->flags & FL_SWIM ) && ( ent->liquidInfo.level > LIQUID_WAIST ) ) ) {
                //if ( ent->velocity[ 2 ] < level.gravity * -0.1f )
				if ( ent->velocity[ 2 ] < sv_gravity->value * -0.1f ) {
					hitsound = true;
				}
				if ( ent->liquidInfo.level != LIQUID_UNDER ) {
					SVG_AddGravity( ent );
				}
            }
        }
    }

    // friction for flying monsters that have been given vertical velocity
    if ( ( ent->flags & FL_FLY ) && ( ent->velocity[ 2 ] != 0 ) /*&& !( ent->monsterinfo.aiflags & AI_ALTERNATE_FLY )*/ ) {
        speed = fabsf( ent->velocity[ 2 ] );
        //control = speed < sv_stopspeed->value ? sv_stopspeed->value : speed;
        control = speed < sv_stopspeed ? sv_stopspeed : speed;
        friction = sv_friction / 3;
        newspeed = speed - ( gi.frame_time_s * control * friction );
        if ( newspeed < 0 )
            newspeed = 0;
        newspeed /= speed;
        ent->velocity[ 2 ] *= newspeed;
    }

    // friction for swimming monsters that have been given vertical velocity
    if ( ( ent->flags & FL_SWIM ) && ( ent->velocity[ 2 ] != 0 ) /*&& !( ent->monsterinfo.aiflags & AI_ALTERNATE_FLY )*/ ) {
        speed = fabsf( ent->velocity[ 2 ] );
        //control = speed < sv_stopspeed->value ? sv_stopspeed->value : speed;
        control = speed < sv_stopspeed ? sv_stopspeed : speed;
        newspeed = speed - ( gi.frame_time_s * control * sv_waterfriction * (float)ent->liquidInfo.level );
        if ( newspeed < 0 )
            newspeed = 0;
        newspeed /= speed;
        ent->velocity[ 2 ] *= newspeed;
    }

    if ( ent->velocity[ 2 ] || ent->velocity[ 1 ] || ent->velocity[ 0 ] ) {
        // apply friction
        if ( ( wasonground || ( ent->flags & ( FL_SWIM | FL_FLY ) ) ) /*&& !( ent->monsterinfo.aiflags & AI_ALTERNATE_FLY )*/ ) {
            vel = &ent->velocity[ 0 ];
            speed = sqrtf( vel[ 0 ] * vel[ 0 ] + vel[ 1 ] * vel[ 1 ] );
            if ( speed ) {
                friction = sv_friction;

                // Paril: lower friction for dead monsters
                if ( ent->lifeStatus )
                    friction *= 0.5f;

                //control = speed < sv_stopspeed->value ? sv_stopspeed->value : speed;
                control = speed < sv_stopspeed ? sv_stopspeed : speed;
                newspeed = speed - gi.frame_time_s * control * friction;

                if ( newspeed < 0 )
                    newspeed = 0;
                newspeed /= speed;

                vel[ 0 ] *= newspeed;
                vel[ 1 ] *= newspeed;
            }
        }

		Vector3 old_origin = ent->currentOrigin;//Vector3 old_origin = ent->s.origin;

        SVG_SlideBox( ent, gi.frame_time_s, mask );

        SVG_Util_TouchProjectiles( ent, old_origin );

        M_CheckGround( ent, mask );

        gi.linkentity( ent );

        // ========
        // PGM - reset this every time they move.
        //       SVG_touchtriggers will set it back if appropriate
        ent->gravity = 1.0;
        // ========

        // [Paril-KEX] this is something N64 does to avoid doors opening
        // at the start of a level, which triggers some monsters to spawn.
        if ( /*!level.is_n64 || */level.time > FRAME_TIME_S ) {
            SVG_Util_TouchTriggers( ent );
        }

        if ( !ent->inUse ) {
            return;
        }

        if ( ent->groundInfo.entityNumber != ENTITYNUM_NONE ) {
            if ( !wasonground ) {
                if ( hitsound ) {
                    //ent->s.event = EV_OTHER_FOOTSTEP;
                    SVG_Util_AddEvent( ent, EV_OTHER_FOOTSTEP, 0 );
                }
            }
        }
    }

    if ( !ent->inUse ) { // PGM g_touchtrigger free problem
        return;
    }

    if ( ent->svFlags & SVF_MONSTER ) {
        M_CatagorizePosition( ent, Vector3( ent->currentOrigin ), ent->liquidInfo.level, ent->liquidInfo.type );
        M_WorldEffects( ent );
    }

    // regular thinking
    SVG_RunEntityThink( ent );
}



/**
*
*
*
*	Entity Physics Core Mechanics:
*
*
*
**/
/**
*	@brief	Runs physics for a single entity based on its movetype.
**/
void SVG_RunEntity(svg_base_edict_t *ent) {
    Vector3	previousOrigin;
    bool	isMoveStepper = false;

	// For the MOVETYPE_STEP entities, we need to check if they actually moved.
	// So we save off their previous origin here, in case they get stuck in a wall.
    if ( ent->movetype == MOVETYPE_STEP ) {
        previousOrigin = ent->currentOrigin;
        isMoveStepper = true;
    }

    if ( ent->HasPreThinkCallback() ) {
        ent->DispatchPreThinkCallback( );
    }

    switch ( ent->movetype ) {
    case MOVETYPE_PUSH:
    case MOVETYPE_STOP:
        SV_Physics_Pusher( ent );
        break;
    case MOVETYPE_NONE:
    case MOVETYPE_WALK:
        SVG_Physics_None( ent );
        break;
    case MOVETYPE_NOCLIP:
        SVG_Physics_Noclip( ent );
        break;
    case MOVETYPE_STEP:
        SV_Physics_Step( ent );
        break;
    case MOVETYPE_ROOTMOTION:
        SVG_Physics_RootMotion( ent );
        break;
    case MOVETYPE_TOSS:
    case MOVETYPE_BOUNCE:
    case MOVETYPE_FLY:
    case MOVETYPE_FLYMISSILE:
        SVG_Physics_Toss( ent );
        break;
    default:
        gi.error( "SV_Physics: bad movetype %i", ent->movetype );
    }

    if ( isMoveStepper && ent->movetype == MOVETYPE_STEP ) {
        // if we moved, check and fix origin if needed
        if ( !VectorCompare( ent->currentOrigin, previousOrigin ) ) {
            svg_trace_t trace = SVG_Trace( ent->currentOrigin, ent->mins, ent->maxs, &previousOrigin.x, ent, SVG_GetClipMask( ent ) );
			if ( trace.allsolid || trace.startsolid ) {
				SVG_Util_SetEntityOrigin( ent, previousOrigin, true ); // VectorCopy( previousOrigin, ent->s.origin ); // = previous_origin;		
			}
        }
    }

    if ( ent->HasPostThinkCallback() ) {
        ent->DispatchPostThinkCallback();
    }
}
