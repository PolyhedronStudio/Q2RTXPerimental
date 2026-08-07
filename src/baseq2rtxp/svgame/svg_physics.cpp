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

// Rotating-door spawnflag used to select angular rider carry behavior.
#include "svgame/entities/func/svg_func_door_rotating.h"

// PMove constants and step defines.
#include "sharedgame/pmove/sg_pmove.h"
#include "sharedgame/pmove/sg_pmove_slidemove.h"



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
	//! The original ground relationship before the push.
	ground_info_t groundInfo;
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

//! Distance used to determine whether a start-solid push path is leaving the blocking surface immediately.
static constexpr float PUSH_PATH_SOLID_PROBE_DISTANCE = 0.25f;

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
*		@param	ent The entity to apply gravity to.
**/
void SVG_AddGravity( svg_base_edict_t *ent ) {
	ent->velocity += ent->gravityVector *	( ent->gravity *	level.gravity *	gi.frame_time_s );
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
	const double backOff = QM_Vector3DotProduct( in, normal );

	if ( backOff < 0. ) {
		const double clipBackOff = backOff *	overbounce;
		// Calculate and apply change.
		for ( int32_t i = 0; i < 3; i++ ) {
			// Calculate change for axis.
			const float change = normal[ i ] *	clipBackOff;
			// Apply velocity change.
			out[ i ] = in[ i ] - change;
			// Halt if we're past epsilon.
			if ( out[ i ] > -CLIPVELOCITY_STOP_EPSILON && out[ i ] < CLIPVELOCITY_STOP_EPSILON ) {
				out[ i ] = 0;
			}
		}
	} else {
		// If velocity is moving away from the plane, do not clip it.
		out = in;
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
*		@param	ent The entity to get the clip mask for.
*		@return The contents mask to use for clipping traces against this entity.
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
	const svg_trace_t trace = SVG_TraceEntityShape( ent->currentOrigin, ent->currentOrigin, ent, ent, clipMask );

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
*	@brief	Check whether an entity still overlaps a specific clip entity at its current position.
*	@param	ent			Entity that was tentatively moved by the pusher.
*	@param	clipEdict	Specific clip entity that may be blocking the move.
*	@return	True when the entity still overlaps the clip entity, false when the apparent block
*			comes only from world/frame geometry.
*	@note	Uses the entity's native solid shape so capsule, cylinder, sphere, and box hulls are
*			tested with their matching clip wrapper.
**/
static svg_trace_t SVG_TraceEntityShapeAgainstClipEdict( const svg_base_edict_t *ent, svg_base_edict_t *clipEdict, const Vector3 &start, const Vector3 &end ) {
	/**
	*	Validate the two collision participants before selecting a shape-specific clip path.
	**/
	if ( !ent || !clipEdict ) {
		return {};
	}

	/**
	*	Use the moving entity's native primitive so the probe has the same extents as gameplay movement.
	**/
	const cm_contents_t clipMask = SVG_GetClipMask( ent );
	switch ( ent->solid ) {
		case SOLID_CAPSULE: {
			// Derive the capsule radius from the horizontal bounds and remove the spherical cap from its half-height.
			const float radius = std::max( std::fabs( ent->maxs.x ), std::fabs( ent->maxs.y ) );
			const float halfHeight = std::max( 0.0f, std::fabs( ent->maxs.z ) - radius );
			return SVG_ClipCapsule( clipEdict, start, radius, halfHeight, end, clipMask );
		}
		case SOLID_CYLINDER: {
			// Derive the cylinder radius and axial half-height from the entity bounds.
			const float radius = std::max( std::fabs( ent->maxs.x ), std::fabs( ent->maxs.y ) );
			const float halfHeight = std::fabs( ent->maxs.z );
			return SVG_ClipCylinder( clipEdict, start, radius, halfHeight, end, clipMask );
		}
		case SOLID_SPHERE: {
			// Use the largest bound component so the spherical probe contains the complete entity shape.
			const float radius = std::max( std::fabs( ent->maxs.x ), std::max( std::fabs( ent->maxs.y ), std::fabs( ent->maxs.z ) ) );
			return SVG_ClipSphere( clipEdict, start, radius, end, clipMask );
		}
		default: {
			// Box-like solids use their native local mins/maxs against the selected clip edict.
			return SVG_Clip( clipEdict, start, ent->mins, ent->maxs, end, clipMask );
		}
	}
}

/**
*	@brief  Check whether an entity still overlaps a specific clip entity at its current position.
*	@param  ent         Entity that was tentatively moved by the pusher.
*	@param  clipEdict   Specific clip entity that may be blocking the move.
*	@return True only when the entity penetrates the clip entity at its current position.
*	@note   A zero-length probe reports only startsolid/allsolid as final penetration. A mere
*	        contact at a valid separating surface must not be promoted to a blocked mover result.
**/
static bool SVG_TestEntityPositionAgainstClipEdictAt( const svg_base_edict_t *ent, svg_base_edict_t *clipEdict, const Vector3 &origin ) {
	/**
	*	Reject invalid or non-blocking candidates before performing a collision-model query.
	**/
	if ( !ent || !clipEdict ) {
		return false;
	}
	if ( ent->solid == SOLID_NOT || ent->solid == SOLID_TRIGGER ) {
		return false;
	}

	/**
	*	A zero-length shape probe distinguishes actual penetration from a surface that is merely touching.
	**/
	const svg_trace_t trace = SVG_TraceEntityShapeAgainstClipEdict( ent, clipEdict, origin, origin );
	return trace.startsolid || trace.allsolid;
}

/**
*	@brief  Check whether an entity penetrates a specific clip entity at its current origin.
*	@param  ent         Entity that was tentatively moved by the pusher.
*	@param  clipEdict   Specific clip entity that may be blocking the move.
*	@return True only when the entity penetrates the clip entity at its current origin.
*	@note   This convenience wrapper keeps current-pose validation separate from virtual path probes.
**/
static bool SVG_TestEntityPositionAgainstClipEdict( const svg_base_edict_t *ent, svg_base_edict_t *clipEdict ) {
	return SVG_TestEntityPositionAgainstClipEdictAt( ent, clipEdict, ent ? ent->currentOrigin : Vector3{} );
}

/**
*	@brief  Check whether an edict is already recorded in the current recursive push chain.
*	@param  ent         Entity to query.
*	@param  visited     Bitset containing entities participating in the current chain.
*	@return True when the entity is present in the chain bitset.
**/
static bool SVG_IsEntityInPushChain( const svg_base_edict_t *ent, const uint8_t *visited ) {
	/**
	*	Invalid entities or an uninitialized chain have no recorded membership.
	**/
	if ( !ent || !visited ) {
		return false;
	}

	/**
	*	Convert the edict number into the byte and bit used by the recursion guard.
	**/
	const int32_t entNum = ent->s.number;
	if ( entNum <= 0 || entNum >= MAX_EDICTS ) {
		return false;
	}
	const int32_t byteIdx = entNum / 8;
	const uint8_t bitMask = static_cast<uint8_t>( 1u << ( entNum % 8 ) );
	return ( visited[ byteIdx ] & bitMask ) != 0;
}

/**
*	@brief  Check whether an entity already has a rollback record in the active mover transaction.
*	@param  ent         Entity to query.
*	@param  transaction First rollback record belonging to the current mover attempt.
*	@return True when the entity was already displaced by this transaction.
*	@note   Recursive blocker resolution records entities before mutation, so the outer contact scan
*	        must not apply their mover displacement a second time.
**/
static bool SVG_HasPushedEntity( const svg_base_edict_t *ent, const pushed_t *transaction ) {
	/**
	*	Invalid inputs cannot identify a previous mutation.
	**/
	if ( !ent || !transaction || transaction > pushedState.pushedPtr ) {
		return false;
	}

	/**
	*	Search the bounded transaction records for the entity's first saved state.
	**/
	for ( const pushed_t *record = transaction; record < pushedState.pushedPtr; record++ ) {
		if ( record->ent == ent ) {
			return true;
		}
	}
	return false;
}

/**
*	@brief  Determine whether a pushed entity path contains a newly encountered obstruction.
*	@param  trace       Shape trace returned for a tentative pusher displacement.
*	@param  ent         Entity whose shape was swept.
*	@param  clipEdict   Entity or world BSP that produced `trace`.
*	@param  start       World-space path start.
*	@param  end         World-space path end.
*	@param  allowInitialSeparation Allow a caller that has already resolved the initial overlap through a
*	                               bounded slide search to commit its proven escape displacement.
*	@return True when the path enters a solid, remains embedded after starting solid, or is all solid.
*	@note   Ordinary mover paths require the first movement increment to leave an initial overlap. The
*	        specialized no-carry slide transaction may bypass that conservative probe only after its
*	        virtual path has explicitly recovered a separating normal and validated the remaining path.
**/
static bool SVG_IsPushedEntityPathBlocked( const svg_trace_t &trace, const svg_base_edict_t *ent, svg_base_edict_t *clipEdict, const Vector3 &start, const Vector3 &end, const bool allowInitialSeparation = false ) {
	/**
	*	Preserve the collision-model contract that permits a path to leave an initial solid overlap.
	**/
	if ( trace.allsolid || ( !trace.startsolid && trace.fraction < 1.0f ) ) {
		return true;
	}

	/**
	*	A start-solid trace is safe only when the first small step leaves the same solid volume. If that
	*	probe remains embedded, the candidate is moving through or along the obstruction and must stop.
	**/
	if ( !trace.startsolid || !ent || !clipEdict ) {
		return false;
	}
	const Vector3 path = end - start;
	const float pathLength = QM_Vector3Length( path );
	if ( pathLength <= 0.0001f ) {
		return allowInitialSeparation;
	}
	if ( allowInitialSeparation ) {
		/**
		*	Locate the first free sample along the complete candidate path. This adaptive search is reserved
		*	for the no-carry slide transaction because its aggregate displacement can begin deeper inside a
		*	brush than the conservative ordinary-push probe distance.
		**/
		if ( SVG_TestEntityPositionAgainstClipEdictAt( ent, clipEdict, end ) ) {
			return true;
		}

		constexpr int32_t escapeSampleCount = 16;
		Vector3 lastSolidOrigin = start;
		Vector3 firstFreeOrigin = end;
		bool foundFreeSample = false;
		for ( int32_t sampleIndex = 1; sampleIndex <= escapeSampleCount; sampleIndex++ ) {
			const float sampleFraction = static_cast<float>( sampleIndex ) / static_cast<float>( escapeSampleCount );
			const Vector3 sampleOrigin = start + ( path *	sampleFraction );
			if ( SVG_TestEntityPositionAgainstClipEdictAt( ent, clipEdict, sampleOrigin ) ) {
				lastSolidOrigin = sampleOrigin;
				continue;
			}
			firstFreeOrigin = sampleOrigin;
			foundFreeSample = true;
			break;
		}
		if ( !foundFreeSample ) {
			return true;
		}

		/**
		*	Refine the solid-to-free boundary before checking the remainder, so a deep initial overlap is
		*	separated without allowing the trace to skip a second intersection with the same clip volume.
		**/
		constexpr int32_t escapeSearchIterations = 8;
		for ( int32_t searchIndex = 0; searchIndex < escapeSearchIterations; searchIndex++ ) {
			const Vector3 midpoint = lastSolidOrigin + ( firstFreeOrigin - lastSolidOrigin ) *	0.5f;
			if ( SVG_TestEntityPositionAgainstClipEdictAt( ent, clipEdict, midpoint ) ) {
				lastSolidOrigin = midpoint;
			} else {
				firstFreeOrigin = midpoint;
			}
		}

		/**
		*	Start the remaining sweep just inside the proven free region. A short free margin avoids treating
		*	the collision-model boundary epsilon as a second start-solid overlap.
		**/
		const Vector3 pathDirection = path *	( 1.0f / pathLength );
		const float remainingLength = QM_Vector3Length( end - firstFreeOrigin );
		constexpr float freeMargin = 0.125f;
		const Vector3 remainingStart = remainingLength > freeMargin
			? firstFreeOrigin + ( pathDirection *	freeMargin )
			: end;
		if ( SVG_TestEntityPositionAgainstClipEdictAt( ent, clipEdict, remainingStart ) ) {
			return true;
		}
		const svg_trace_t remainingTrace = SVG_TraceEntityShapeAgainstClipEdict( ent, clipEdict, remainingStart, end );
		return remainingTrace.allsolid || ( !remainingTrace.startsolid && remainingTrace.fraction < 1.0f );
	}

	/**
	*	Ordinary pushes retain the short conservative probe so a side-contact shove cannot tunnel through
	*	adjacent BSP while still allowing tiny numerical overlaps to leave the original surface.
	**/
	const float probeDistance = std::min( PUSH_PATH_SOLID_PROBE_DISTANCE, pathLength );
	const Vector3 probeOrigin = start + ( path *	( probeDistance / pathLength ) );
	if ( SVG_TestEntityPositionAgainstClipEdictAt( ent, clipEdict, probeOrigin ) ) {
		return true;
	}

	/**
	*	Once the path has escaped its initial overlap, sweep the remaining segment from the first known
	*	free sample. This catches a second BSP brush entered later in the same displacement instead of
	*	allowing a start-solid trace to tunnel through it.
	**/
	const svg_trace_t remainingTrace = SVG_TraceEntityShapeAgainstClipEdict( ent, clipEdict, probeOrigin, end );
	return remainingTrace.allsolid || ( !remainingTrace.startsolid && remainingTrace.fraction < 1.0f );
}

/**
*	@brief  Find the first solid entity hit while moving one pushed entity along a candidate path.
*	@param  ent                    Moving entity whose native shape is swept.
*	@param  pusher                 Mover that is carrying or pushing the entity.
*	@param  start                  World-space path start.
*	@param  end                    World-space path end.
*	@param  visited                Recursive chain members retained for call-site context; final-pose
*	                               validation must still inspect those entities for residual penetration.
*	@param  outBlocker             [out] Entity producing the earliest blocking hit, or nullptr when clear.
*	@param  includePusherAsBlocker Include the moved pusher for riders not carried around a pivot.
*	@param  allowInitialSeparation Expose an initial start-solid contact to the caller so a specialized
*	                               slide resolver can recover a separating normal; final-pose validation
*	                               remains strict.
*	@return Trace for the earliest blocking geometry.
*	@note   World BSP and each dynamic edict are queried independently so a world startsolid cannot be
*	        masked by an unrelated entity result in the merged server trace.
**/
static svg_trace_t SVG_TracePushedEntityPath( const svg_base_edict_t *ent, svg_base_edict_t *pusher, const Vector3 &start, const Vector3 &end, const uint8_t *visited, svg_base_edict_t **outBlocker, const bool includePusherAsBlocker, const bool allowInitialSeparation = false ) {
	/**
	*	Initialize the output blocker before evaluating world and entity candidates.
	**/
	if ( outBlocker ) {
		*outBlocker = nullptr;
	}
	if ( !ent ) {
		return {};
	}

	/**
	*	Test world BSP first. A complete world-solid path is authoritative and cannot be displaced by recursion.
	**/
	svg_base_edict_t *worldEntity = g_edict_pool.EdictForNumber( ENTITYNUM_WORLD );
	svg_trace_t bestTrace = SVG_TraceEntityShapeAgainstClipEdict( ent, worldEntity, start, end );
	bool haveBlocker = false;
	if ( bestTrace.allsolid ) {
		if ( outBlocker ) {
			*outBlocker = worldEntity;
		}
		return bestTrace;
	}
	if ( SVG_IsPushedEntityPathBlocked( bestTrace, ent, worldEntity, start, end, allowInitialSeparation ) ) {
		if ( outBlocker ) {
			*outBlocker = worldEntity;
		}
		haveBlocker = true;
	} else if ( allowInitialSeparation && bestTrace.startsolid ) {
		if ( outBlocker ) {
			*outBlocker = worldEntity;
		}
		haveBlocker = true;
	}

	/**
	*	Compare the candidate path against active solid edicts, excluding self and chain members. The mover
	*	is additionally excluded for ordinary pushes but tested for non-carried rotating riders.
	**/
	for ( int32_t e = 1; e < globals.edictPool->num_edicts; e++ ) {
		svg_base_edict_t *candidate = g_edict_pool.EdictForNumber( e );
		if ( !candidate || !candidate->inUse || !candidate->isLinked ) {
			continue;
		}
		if ( candidate == ent || SVG_IsEntityInPushChain( candidate, visited ) ) {
			continue;
		}
		if ( candidate == pusher && !includePusherAsBlocker ) {
			continue;
		}
		if ( candidate->solid == SOLID_NOT || candidate->solid == SOLID_TRIGGER ) {
			continue;
		}

		/**
		*	A direct clip preserves the candidate's entity identity without allowing the merged trace to report self.
		**/
		const svg_trace_t candidateTrace = SVG_TraceEntityShapeAgainstClipEdict( ent, candidate, start, end );
		const bool candidateHit = SVG_IsPushedEntityPathBlocked( candidateTrace, ent, candidate, start, end, allowInitialSeparation ) || ( allowInitialSeparation && candidateTrace.startsolid );
		if ( !candidateHit ) {
			continue;
		}
		if ( !haveBlocker || candidateTrace.fraction < bestTrace.fraction ) {
			bestTrace = candidateTrace;
			if ( outBlocker ) {
				*outBlocker = candidate;
			}
			haveBlocker = true;
		}
	}

	return bestTrace;
}

/**
*	@brief  Find a solid entity penetrating a pushed entity's current candidate pose.
  *	@param  ent                    Entity at the tentative destination.
 *	@param  pusher                 Mover responsible for the displacement.
 *	@param  includePusherAsBlocker Include the mover when validating a non-carried rotating rider.
*	@return The penetrating world or dynamic entity; nullptr when the pose is valid.
*	@note   Final-pose validation checks penetration only. Valid contact at a separating boundary is allowed.
**/
static svg_base_edict_t *SVG_FindPushedEntityPositionBlocker( const svg_base_edict_t *ent, svg_base_edict_t *pusher, const bool includePusherAsBlocker ) {
	/**
	*	Validate the candidate entity before probing final geometry.
	**/
	if ( !ent || ent->solid == SOLID_NOT || ent->solid == SOLID_TRIGGER ) {
		return nullptr;
	}

	/**
	*	Check static world geometry first so a world penetration is never hidden by a mover contact.
	**/
	svg_base_edict_t *worldEntity = g_edict_pool.EdictForNumber( ENTITYNUM_WORLD );
	if ( SVG_TestEntityPositionAgainstClipEdict( ent, worldEntity ) ) {
		return worldEntity;
	}

	/**
	*	A rider that opts out of angular carry must not be committed inside the pusher's rotated end pose.
	*	Carried riders intentionally retain this contact, so the probe is conditional.
	**/
	if ( includePusherAsBlocker && pusher && SVG_TestEntityPositionAgainstClipEdict( ent, pusher ) ) {
		return pusher;
	}

	/**
	*	The pusher is an intentional contact surface for carried and directly pushed entities unless the
	*	caller explicitly requested its validation. Check every other active solid edict while excluding only the entity itself. A
	*	previously visited chain member is deliberately not skipped: recursive displacement must prove
	*	that the complete committed chain is non-overlapping.
	**/
	for ( int32_t e = 1; e < globals.edictPool->num_edicts; e++ ) {
		svg_base_edict_t *candidate = g_edict_pool.EdictForNumber( e );
		if ( !candidate || !candidate->inUse || !candidate->isLinked ) {
			continue;
		}
		if ( candidate == ent || ( candidate == pusher && !includePusherAsBlocker ) ) {
			continue;
		}
		if ( candidate->solid == SOLID_NOT || candidate->solid == SOLID_TRIGGER ) {
			continue;
		}
		if ( SVG_TestEntityPositionAgainstClipEdict( ent, candidate ) ) {
			return candidate;
		}
	}

	return nullptr;
}

/**
*	@brief	Compute the exact 3D displacement of a point carried around a pusher pivot.
*	@param	relOrg	World-space position relative to the pusher pivot before rotation.
*	@param	startAngles	Pusher orientation at the beginning of the movement step.
*	@param	endAngles	Pusher orientation at the end of the movement step.
*	@return	World-space displacement produced by the start-to-end rotation.
*	@note	The point is converted into the pusher's start frame and then reconstructed through its end
*			frame, preserving the engine's forward/-right/up basis convention.
**/
static Vector3 SVG_Compute3DRotationDisplacement( const Vector3 &relOrg, const Vector3 &startAngles, const Vector3 &endAngles ) {
	/**
	*	Return zero immediately when the pusher orientation did not change during this frame.
	**/
	const Vector3 angularMove = endAngles - startAngles;
	if ( VectorEmpty( angularMove ) ) {
		return {};
	}

	/**
	*	Build the pusher's start and end bases using the same Euler convention as transformed collision
	*	traces. The negative right vector represents the world direction of local positive Y.
	**/
	Vector3 startForward = {};
	Vector3 startRight = {};
	Vector3 startUp = {};
	Vector3 endForward = {};
	Vector3 endRight = {};
	Vector3 endUp = {};
	QM_AngleVectors( startAngles, &startForward, &startRight, &startUp );
	QM_AngleVectors( endAngles, &endForward, &endRight, &endUp );

	/**
	*	Convert the world-relative rider origin into coordinates local to the pusher's start orientation.
	*	Dot products invert the orthonormal basis while negating right preserves local positive Y.
	**/
	const Vector3 localOrigin = {
		QM_Vector3DotProduct( relOrg, startForward ),
		-QM_Vector3DotProduct( relOrg, startRight ),
		QM_Vector3DotProduct( relOrg, startUp )
	};

	/**
	*	Convert the unchanged local offset through the pusher's end orientation and return only the
	*	angular displacement. The caller adds linear move separately.
	**/
	const Vector3 rotatedOrigin = ( endForward *	localOrigin.x ) - ( endRight *	localOrigin.y ) + ( endUp *	localOrigin.z );
	return rotatedOrigin - relOrg;
}
	
/**
*	@brief	Determine whether a mover carries riders around its rotating pivot.
*	@param	pusher	Mover whose rider policy is being queried.
*	@return	True when the existing angular rider displacement should be retained.
*	@note	Only func_door_rotating consumes the opt-out flag; all other movers preserve their
*			current behavior and therefore return true.
**/
static bool SVG_ShouldCarryRidersAroundPivot( const svg_base_edict_t *pusher ) {
	/**
	*	Preserve the existing behavior for invalid, non-rotating, and non-door movers.
	**/
	if ( !pusher || !pusher->GetTypeInfo()->IsSubClassType<svg_func_door_rotating_t>() ) {
		return true;
	}

	/**
	*	The explicit rotating-door opt-out leaves riders in ordinary collision handling so they can slide
	*	away from the brush or block the mover rather than being teleported around its pivot.
	**/
	return ( pusher->spawnflags & svg_func_door_rotating_t::SPAWNFLAG_NO_RIDER_CARRY ) == 0;
}

/**
*	@brief	Set origin of an entity during mover physics and synchronize client pmove prediction origin if applicable.
*	@param	ent			Entity to translate.
*	@param	origin		New world-space origin position.
*	@param	setPrevious	If true, stores old origin in s.old_origin for interpolation.
**/
static void SVG_Mover_SetEntityOrigin( svg_base_edict_t *ent, const Vector3 &origin, bool setPrevious = true );

/**
*	@brief  Save an entity's authoritative state before a tentative mover mutation.
*	@param  ent  Entity that is about to be moved or have its ground state changed.
*	@return True when a rollback record was created, false when the transaction has no capacity.
*	@note   Records are intentionally append-only during a transaction. A checkpoint can therefore
*	        restore nested attempts in reverse mutation order without losing the original state.
**/
static bool SVG_SavePushedEntityState( svg_base_edict_t *ent ) {
	/**
	*	Validate the entity and ensure the bounded rollback array has a free slot.
	**/
	if ( !ent || !pushedState.pushedPtr || pushedState.pushedPtr >= &pushedState.pushed[ MAX_EDICTS ] ) {
		return false;
	}

	/**
	*	Capture all state changed by mover displacement, including client prediction and grounding.
	**/
	pushedState.pushedPtr->ent = ent;
	pushedState.pushedPtr->origin = ent->currentOrigin;
	pushedState.pushedPtr->angles = ent->currentAngles;
	pushedState.pushedPtr->groundInfo = ent->groundInfo;
	if ( ent->client ) {
		pushedState.pushedPtr->yawDelta = ent->client->ps.pmove.delta_angles[ YAW ];
	}
	pushedState.pushedPtr++;
	return true;
}

/**
*	@brief  Restore all mover mutations made after a transaction checkpoint.
*	@param  checkpoint  First rollback record belonging to the nested attempt.
*	@note   Restoration runs in reverse order so entities pushed recursively are returned before
*	        their parents, preserving the same dependency order used during displacement.
**/
static void SVG_RestorePushedStateTo( pushed_t *checkpoint ) {
	/**
	*	Ignore invalid checkpoints instead of walking outside the bounded rollback storage.
	**/
	if ( !checkpoint || checkpoint < pushedState.pushed || checkpoint > pushedState.pushedPtr ) {
		return;
	}

	/**
	*	Restore every entity changed after the checkpoint, including links and prediction state.
	**/
	while ( pushedState.pushedPtr > checkpoint ) {
		--pushedState.pushedPtr;
		pushed_t *saved = pushedState.pushedPtr;
		if ( !saved->ent || !saved->ent->inUse ) {
			continue;
		}
		SVG_Mover_SetEntityOrigin( saved->ent, saved->origin, true );
		SVG_Util_SetEntityAngles( saved->ent, saved->angles, true );
		saved->ent->groundInfo = saved->groundInfo;
		if ( saved->ent->client ) {
			saved->ent->client->ps.pmove.delta_angles[ YAW ] = saved->yawDelta;
		}
		gi.linkentity( saved->ent );
	}
}

/**
*	@brief  Trace an entity downward against a mover to determine its end-pose support surface.
*	@param  ent      Entity whose native collision shape is being tested.
*	@param  pusher   Mover that may still support the entity.
*	@param  origin   Candidate entity origin to test.
*	@return Shape trace from a small lift above origin down through the walkable support range.
*	@note   The trace is directed at the mover only, so nearby world geometry cannot make a rider
*	        appear supported by the rotating brush.
**/
static svg_trace_t SVG_TraceEntitySupportAgainstMover( const svg_base_edict_t *ent, svg_base_edict_t *pusher, const Vector3 &origin ) {
	/**
	*	Reject invalid participants before constructing the support probe.
	**/
	if ( !ent || !pusher ) {
		return {};
	}

	/**
	*	Lift the shape slightly to avoid classifying an embedded or boundary-starting pose as a valid floor
	*	contact, then trace down by the same walkable-ground distance used by rider acquisition.
	**/
	const Vector3 start = origin + Vector3{ 0.0f, 0.0f, 0.1f };
	const Vector3 end = origin - Vector3{ 0.0f, 0.0f, static_cast<float>( PM_STEP_GROUND_DIST ) };
	return SVG_TraceEntityShapeAgainstClipEdict( ent, pusher, start, end );
}

/**
*	@brief  Test whether an entity is supported by a mover at a candidate origin.
*	@param  ent             Entity whose support state is being evaluated.
*	@param  pusher          Mover providing the possible support surface.
*	@param  origin          Candidate entity origin in the mover's current pose.
*	@param  outGroundInfo   Optional support data populated when the mover is walkable.
*	@return True when the mover is hit below the entity on a walkable, non-startsolid trace.
**/
static bool SVG_IsEntitySupportedByMoverAt( const svg_base_edict_t *ent, svg_base_edict_t *pusher, const Vector3 &origin, ground_info_t *outGroundInfo = nullptr ) {
	/**
	*	Validate the support participants before issuing the downward trace.
	**/
	if ( !ent || !pusher ) {
		return false;
	}

	/**
	*	Require a clean downward hit on the mover. A startsolid result represents penetration rather than
	*	a rider standing freely on the rotating brush.
	**/
	const svg_trace_t trace = SVG_TraceEntitySupportAgainstMover( ent, pusher, origin );
	if ( trace.startsolid || trace.allsolid || trace.fraction >= 1.0f || trace.entityNumber != pusher->s.number || trace.plane.normal[ 2 ] < static_cast<float>( PM_STEP_MIN_NORMAL ) ) {
		return false;
	}

	/**
	*	Preserve the authoritative support details when the caller needs to keep the rider grounded after a
	*	successful no-carry displacement.
	**/
	if ( outGroundInfo ) {
		outGroundInfo->entityNumber = pusher->s.number;
		outGroundInfo->entityLinkCount = pusher->linkCount;
		outGroundInfo->plane = trace.plane;
		outGroundInfo->contents = trace.contents;
		outGroundInfo->material = trace.material;
		if ( trace.surface ) {
			outGroundInfo->surface = *trace.surface;
		}
	}

	return true;
}

/**
*	@brief	Probe whether an entity is genuinely standing on top of a mover.
*	@param	ent		Entity edict to evaluate.
*	@param	pusher	Mover entity undergoing movement.
*	@return	True when the entity is standing on top of the mover's horizontal surface, false otherwise.
*	@note	First checks existing groundInfo data, then performs a non-startsolid downward shape trace
*			lifted by 0.1f and extending by PM_STEP_GROUND_DIST to test for walkable floor surface contact.
**/
static bool SVG_IsEntityRidingMover( const svg_base_edict_t *ent, svg_base_edict_t *pusher ) {
	// Sanity check: both entities must be valid and solid.
	if ( !ent || !pusher ) {
		return false;
	}

	// Perform a fresh direct mover-only support trace so stale link information cannot retain a dropped rider.
	return SVG_IsEntitySupportedByMoverAt( ent, pusher, ent->currentOrigin );
}

/**
*	@brief	Probe whether an entity shape intersects a mover along the mover's relative motion vector.
*	@param	ent			Candidate entity being checked for mover contact.
*	@param	pusher		Mover entity undergoing linear or angular displacement.
*	@param	startPoint	Start point of the shape sweep in world space.
*	@param	endPoint	End point of the shape sweep in world space.
*	@return	True when the mover sweeps into or overlaps the entity shape, false otherwise.
*	@note	Uses the entity's native shape primitive (SOLID_CAPSULE, SOLID_CYLINDER, SOLID_SPHERE, SOLID_BBOX)
*			to perform a relative motion sweep from startPoint to endPoint against pusher.
**/
static bool SVG_TestEntityContactWithMover( const svg_base_edict_t *ent, svg_base_edict_t *pusher, const Vector3 &startPoint, const Vector3 &endPoint ) {
	/**
	*	Validate both collision participants before testing the relative mover sweep.
	**/
	if ( !ent || !pusher ) {
		return false;
	}
	if ( ent->solid == SOLID_NOT || ent->solid == SOLID_TRIGGER ) {
		return false;
	}

	/**
	*	Reject a sweep that starts inside the mover. That state represents contact at T_start, not
	*	a pusher entering a stationary entity; final overlap is handled by the explicit end probe.
	**/
	const svg_trace_t trace = SVG_TraceEntityShapeAgainstClipEdict( ent, pusher, startPoint, endPoint );
	return !trace.startsolid && !trace.allsolid && trace.fraction < 1.0f;
}

/**
*	@brief	Set origin of an entity during mover physics and synchronize client pmove prediction origin if applicable.
*	@param	ent			Entity to translate.
*	@param	origin		New world-space origin position.
*	@param	setPrevious	If true, stores old origin in s.old_origin for interpolation.
**/
static void SVG_Mover_SetEntityOrigin( svg_base_edict_t *ent, const Vector3 &origin, bool setPrevious ) {
	SVG_Util_SetEntityOrigin( ent, origin, setPrevious );
	if ( ent != nullptr && ent->client != nullptr ) {
		ent->client->ps.pmove.origin = ent->currentOrigin;
	}
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
			end[ i ] = ent->currentOrigin[ i ] + time_left *	ent->velocity[ i ];
		}

		trace = SVG_TraceEntityShape( ent->currentOrigin, end, ent, ent, mask );

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

		time_left -= time_left *	trace.fraction;

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
*		@param	push The vector to push the entity by.
*		@return The trace result of the push operation.
*		@note	If the entity is blocked during the push, its position will be set to the end position of the trace.
*				If the entity is removed during the impact handling, it will not be re-linked.
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
	trace = SVG_TraceEntityShape( start, end, ent, ent, SVG_GetClipMask( ent ) );

	SVG_Mover_SetEntityOrigin( ent, trace.endpos, true ); // VectorCopy(trace.endpos, ent->s.origin);
	gi.linkentity( ent );

	if ( trace.fraction != 1.0f ) {
		// We hit something, so call the impact function.
		SVG_Impact( ent, &trace );

		// If the pushed entity went away and the pusher is still there:
		if ( !trace.ent->inUse && ent->inUse ) {
			// Move the pusher back and try again.
			SVG_Mover_SetEntityOrigin( ent, start, true ); // VectorCopy( start, ent->s.origin );
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
//    //return 0.125f *	(int)x;
//    return x;
//}

/**
*	@brief	Recursively attempts to displace a chain of dynamic entities pushed by a mover.
*	@param	pusher		The root mover entity.
*	@param	ent			The entity currently being pushed in the chain.
*	@param	delta		The displacement vector to apply.
*	@param	depth		Recursion depth guard to prevent infinite loops.
*	@param	visited		Array of edict numbers already pushed in this chain.
 *	@param	includePusherAsBlocker	Whether the root candidate must validate against the mover's rotated end pose.
 *	@param	allowInitialSeparation	Whether this transaction is the prevalidated no-carry slide escape.
 *	@return	true if ent and all downstream entities were successfully displaced into free space;
 *			false if any link in the chain is blocked.
**/
static bool SVG_TryPushEntityChain( svg_base_edict_t *pusher, svg_base_edict_t *ent, const Vector3 &delta, int32_t depth = 0, uint8_t *visited = nullptr, const bool includePusherAsBlocker = false, const bool allowInitialSeparation = false ) {

	/**
	*	Initialize the recursion tracker at the root and reject invalid or non-blocking entities.
	**/
	uint8_t localVisited[ MAX_EDICTS / 8 ] = {};
	if ( !visited ) {
		visited = localVisited;
	}
	if ( !ent || ent == pusher || depth >= 16 || ent->solid == SOLID_NOT || ent->solid == SOLID_TRIGGER ) {
		return false;
	}

	/**
	 *	Mark the current entity before probing blockers so a circular contact graph cannot recurse forever.
	 **/
	const int32_t entNum = ent->s.number;
	if ( entNum <= 0 || entNum >= MAX_EDICTS ) {
		return false;
	}
	const int32_t byteIdx = entNum / 8;
	const uint8_t bitMask = static_cast<uint8_t>( 1u << ( entNum % 8 ) );
	if ( ( visited[ byteIdx ] & bitMask ) != 0 ) {
		pushedState.obstacle = ent;
		return false;
	}
	visited[ byteIdx ] |= bitMask;

	/**
	*	Record the entity before any tentative mutation. The checkpoint owns this entity and all
	*	downstream records created while resolving its blockers.
	**/
	pushed_t *checkpoint = pushedState.pushedPtr;
	if ( !SVG_SavePushedEntityState( ent ) ) {
		pushedState.obstacle = ent;
		return false;
	}
	const Vector3 startOrigin = ent->currentOrigin;
	const Vector3 targetOrigin = startOrigin + delta;

	/**
	 *	Resolve up to the bounded number of possible contact links. Every iteration either moves a
	 *	dynamic blocker or reaches the exact target pose; static geometry always aborts the transaction.
	 **/
	for ( int32_t attempt = 0; attempt < 16; attempt++ ) {
		svg_base_edict_t *pathBlocker = nullptr;
		const svg_trace_t pathTrace = SVG_TracePushedEntityPath( ent, pusher, startOrigin, targetOrigin, visited, &pathBlocker, includePusherAsBlocker, allowInitialSeparation );
		if ( pathBlocker && SVG_IsPushedEntityPathBlocked( pathTrace, ent, pathBlocker, startOrigin, targetOrigin, allowInitialSeparation ) ) {
			/**
			*	Only ordinary movable entities may be displaced recursively. World BSP, brush movers,
			*	stopped movers, and static solids are authoritative blockers for this transaction.
			**/
			const bool blockerIsMovable = pathBlocker != g_edict_pool.EdictForNumber( ENTITYNUM_WORLD )
				&& pathBlocker->solid != SOLID_BSP
				&& pathBlocker->movetype != MOVETYPE_PUSH
				&& pathBlocker->movetype != MOVETYPE_STOP
				&& pathBlocker->movetype != MOVETYPE_NONE
				&& !SVG_IsEntityInPushChain( pathBlocker, visited );
			if ( !blockerIsMovable || !SVG_TryPushEntityChain( pusher, pathBlocker, delta, depth + 1, visited, includePusherAsBlocker, allowInitialSeparation ) ) {
				pushedState.obstacle = pathBlocker;
				SVG_RestorePushedStateTo( checkpoint );
				return false;
			}
			continue;
		}

		/**
		*	Apply the complete requested displacement only after the path is clear. Partial trace endpoints
		*	are not valid commits because they desynchronize the moved entity from the pusher.
		**/
		SVG_Mover_SetEntityOrigin( ent, targetOrigin, true );
		svg_base_edict_t *finalBlocker = SVG_FindPushedEntityPositionBlocker( ent, pusher, includePusherAsBlocker );
		if ( !finalBlocker ) {
			// A successfully displaced entity is no longer grounded on its previous support surface.
			ent->groundInfo.entityNumber = ENTITYNUM_NONE;
			gi.linkentity( ent );
			return true;
		}

		/**
		 *	A dynamic overlap discovered at the exact target still requires downstream displacement. The
		 *	current entity remains at targetOrigin while the blocker is moved so the final pose is re-tested.
		 **/
		const bool blockerIsMovable = finalBlocker != g_edict_pool.EdictForNumber( ENTITYNUM_WORLD )
			&& finalBlocker->solid != SOLID_BSP
			&& finalBlocker->movetype != MOVETYPE_PUSH
			&& finalBlocker->movetype != MOVETYPE_STOP
			&& finalBlocker->movetype != MOVETYPE_NONE
			&& !SVG_IsEntityInPushChain( finalBlocker, visited );
		if ( !blockerIsMovable || !SVG_TryPushEntityChain( pusher, finalBlocker, delta, depth + 1, visited, includePusherAsBlocker, allowInitialSeparation ) ) {
			pushedState.obstacle = finalBlocker;
			SVG_RestorePushedStateTo( checkpoint );
			return false;
		}
	}

	/**
	*	The bounded chain limit was reached without proving a valid final pose.
	**/
	pushedState.obstacle = g_edict_pool.EdictForNumber( ENTITYNUM_WORLD );
	SVG_RestorePushedStateTo( checkpoint );
	return false;
}

/**
*	@brief  Recover an outward contact normal for a pushed entity that began inside a blocker.
*	@param  ent             Entity whose native collision shape is being sampled.
*	@param  clipEdict       World or entity blocker whose contact normal is required.
*	@param  origin          Virtual origin at which the entity is currently being resolved.
*	@param  preferredDelta  Displacement direction that caused the contact.
*	@param  outNormal       [out] Normal pointing away from the blocking surface.
*	@return True when a usable outward normal was recovered.
*	@note   Position traces intentionally report startsolid without a plane. Reverse probes from a
*	        small set of likely free-space directions recover the missing plane without changing the
*	        collision-model contract or mutating the active mover transaction.
**/
static bool SVG_FindPushedEntityContactNormal( const svg_base_edict_t *ent, svg_base_edict_t *clipEdict, const Vector3 &origin, const Vector3 &preferredDelta, Vector3 *outNormal, float *outSeparationDistance ) {
	/**
	*	Validate the collision participants and output storage before issuing any probe traces.
	**/
	if ( !ent || !clipEdict || !outNormal || !outSeparationDistance ) {
		return false;
	}
	*outNormal = {};
	*outSeparationDistance = 0.0f;

	/**
	*	Build probe directions beginning with the direction opposite the requested displacement. This is
	*	the most likely free-space direction when the entity is pressed into a surface by the mover.
	**/
	Vector3 probeDirections[ 12 ] = {};
	int32_t numProbeDirections = 0;
	const float preferredLength = QM_Vector3Length( preferredDelta );
	if ( preferredLength > 0.001f ) {
		const Vector3 preferredDirection = preferredDelta *	( 1.0f / preferredLength );
		probeDirections[ numProbeDirections++ ] = QM_Vector3Negate( preferredDirection );
		probeDirections[ numProbeDirections++ ] = preferredDirection;
	}

	/**
	*	Add radial probes as a useful fallback for rotating brush sides whose contact normal is not aligned
	*	with the entity's instantaneous tangential displacement.
	**/
	const Vector3 radial = origin - clipEdict->currentOrigin;
	const float radialLength = QM_Vector3Length( radial );
	if ( radialLength > 0.001f && numProbeDirections + 2 <= static_cast<int32_t>( q_countof( probeDirections ) ) ) {
		const Vector3 radialDirection = radial *	( 1.0f / radialLength );
		probeDirections[ numProbeDirections++ ] = radialDirection;
		probeDirections[ numProbeDirections++ ] = QM_Vector3Negate( radialDirection );
	}

	/**
	*	Use axis probes last so a corner contact can still expose either of its independent separating planes.
	**/
	const Vector3 axisDirections[ 6 ] = {
		{ 1.0f, 0.0f, 0.0f },
		{ -1.0f, 0.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f },
		{ 0.0f, -1.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f },
		{ 0.0f, 0.0f, -1.0f }
	};
	for ( int32_t axisIndex = 0; axisIndex < static_cast<int32_t>( q_countof( axisDirections ) ) && numProbeDirections < static_cast<int32_t>( q_countof( probeDirections ) ); axisIndex++ ) {
		probeDirections[ numProbeDirections++ ] = axisDirections[ axisIndex ];
	}

	/**
	*	Start sufficiently outside the pushed shape to turn a zero-length start-solid contact into an
	*	ordinary sweep with a collision plane. The probe remains local and never commits an entity move.
	**/
	const float probeDistance = std::max( 16.0f, SVG_GetEntityBoundingRadius( ent ) + 1.0f );
	for ( int32_t directionIndex = 0; directionIndex < numProbeDirections; directionIndex++ ) {
		const Vector3 probeStart = origin + ( probeDirections[ directionIndex ] *	probeDistance );
		const svg_trace_t probeTrace = SVG_TraceEntityShapeAgainstClipEdict( ent, clipEdict, probeStart, origin );
		if ( probeTrace.startsolid || probeTrace.allsolid || probeTrace.fraction >= 1.0f ) {
			continue;
		}

		/**
		*	Reject degenerate planes before normalizing so malformed or synthetic traces cannot create NaNs.
		**/
		const Vector3 normal = { probeTrace.plane.normal[ 0 ], probeTrace.plane.normal[ 1 ], probeTrace.plane.normal[ 2 ] };
		const float normalLength = QM_Vector3Length( normal );
		if ( normalLength <= 0.001f ) {
			continue;
		}
		*outNormal = normal *	( 1.0f / normalLength );
		if ( QM_Vector3DotProduct( *outNormal, probeDirections[ directionIndex ] ) < 0.0f ) {
			*outNormal = QM_Vector3Negate( *outNormal );
		}
		*outSeparationDistance = std::max( 0.0f, QM_Vector3Length( probeTrace.endpos - origin ) );
		return true;
	}

	return false;
}

/**
*	@brief  Attempt to resolve a pushed entity by sliding its requested contact displacement.
*	@param  pusher              Mover currently held at its tentative end pose.
*	@param  ent                 Entity that must separate from the mover and surrounding geometry.
*	@param  requestedDelta      Contact displacement induced by the mover's linear and angular motion.
*	@param  includePusherAsBlocker Include the tentative mover pose in final collision validation.
*	@return True when a collision-free slid pose was committed to the active transaction.
*	@note   Every failed candidate is rolled back by SVG_TryPushEntityChain. The final commit therefore
*	        remains atomic and a valid slide can never leave a partial rider displacement behind.
**/
static bool SVG_TrySlidePushedEntityChain( svg_base_edict_t *pusher, svg_base_edict_t *ent, const Vector3 &requestedDelta, const bool includePusherAsBlocker ) {
	/**
	*	Reject invalid participants and empty contact motion. An empty displacement cannot separate an
	*	entity from a rotating pusher and must remain a genuine block when the final pose overlaps.
	**/
	if ( !pusher || !ent || QM_Vector3Length( requestedDelta ) <= 0.001f ) {
		return false;
	}

	/**
	*	Prefer the complete contact displacement first. This preserves ordinary push behavior whenever the
	*	requested tangent already has a free path and avoids introducing an unnecessary partial move.
	**/
	if ( SVG_TryPushEntityChain( pusher, ent, requestedDelta, 0, nullptr, includePusherAsBlocker ) ) {
		return true;
	}

	/**
	*	Resolve the requested motion through a small number of collision planes. The virtual origin is used
	*	only for trace queries; the entity itself remains at its original position until the final chain
	*	commit succeeds.
	**/
	const Vector3 startOrigin = ent->currentOrigin;
	Vector3 virtualOrigin = startOrigin;
	Vector3 accumulatedDelta = {};
	Vector3 remainingDelta = requestedDelta;
	constexpr int32_t maxSlideBumps = 4;
	constexpr float minSlideDistance = 0.001f;
	constexpr float startSolidSeparation = 0.125f;

	for ( int32_t bumpIndex = 0; bumpIndex < maxSlideBumps; bumpIndex++ ) {
		/**
		*	Stop the bounded slide search once the remaining contact displacement is numerically exhausted.
		**/
		if ( QM_Vector3Length( remainingDelta ) <= minSlideDistance ) {
			break;
		}

		/**
		*	Trace the virtual remaining displacement against world geometry, dynamic entities, and the
		*	tentative rotating pusher. A blocker that is only being left is not treated as a new hit.
		**/
		svg_base_edict_t *pathBlocker = nullptr;
		const Vector3 virtualEnd = virtualOrigin + remainingDelta;
		const svg_trace_t pathTrace = SVG_TracePushedEntityPath( ent, pusher, virtualOrigin, virtualEnd, nullptr, &pathBlocker, includePusherAsBlocker, true );
		const bool pathEnteredNewSolid = ( !pathTrace.startsolid && pathTrace.fraction < 1.0f );
		const bool pathBlocked = pathBlocker && ( pathTrace.allsolid || pathEnteredNewSolid || pathTrace.startsolid );
		if ( !pathBlocked ) {
			accumulatedDelta += remainingDelta;
			break;
		}

		/**
		*	Prefer the authoritative sweep plane, then recover a normal for start-solid contacts whose trace
		*	intentionally contains no plane data.
		**/
		Vector3 contactNormal = { pathTrace.plane.normal[ 0 ], pathTrace.plane.normal[ 1 ], pathTrace.plane.normal[ 2 ] };
		bool haveContactNormal = QM_Vector3Length( contactNormal ) > 0.001f;
		float separationDistance = 0.0f;
		if ( haveContactNormal ) {
			contactNormal = QM_Vector3Normalize( contactNormal );
		} else {
			haveContactNormal = SVG_FindPushedEntityContactNormal( ent, pathBlocker, virtualOrigin, remainingDelta, &contactNormal, &separationDistance );
		}
		if ( !haveContactNormal ) {
			return false;
		}

		/**
		*	Advance to the safe side of the first impact, then remove only the component of the remaining
		*	contact motion that drives the capsule into the surface.
		**/
		const float traceFraction = static_cast<float>( std::clamp( pathTrace.fraction, 0.0, 1.0 ) );
		accumulatedDelta += remainingDelta *	traceFraction;
		remainingDelta = remainingDelta *	( 1.0f - traceFraction );
		if ( pathTrace.startsolid ) {
			// Start-solid probes need an outward correction because collision-model position tests are conservative.
			accumulatedDelta += contactNormal *	( separationDistance + startSolidSeparation );
		}
		virtualOrigin = startOrigin + accumulatedDelta;

		const float intoSurface = QM_Vector3DotProduct( remainingDelta, contactNormal );
		if ( intoSurface < 0.0f ) {
			remainingDelta -= contactNormal *	intoSurface;
		}
	}

	/**
	*	Commit the aggregate slide as one normal transactional displacement. Final-pose validation still
	*	rejects a candidate that remains inside the pusher, world, or another solid entity.
	**/
	if ( QM_Vector3Length( accumulatedDelta ) <= minSlideDistance ) {
		return false;
	}
	return SVG_TryPushEntityChain( pusher, ent, accumulatedDelta, 0, nullptr, includePusherAsBlocker, true );
}

/**
*	@brief	Displaces entities pushed by or riding on a mover edict (MOVETYPE_PUSH / MOVETYPE_STOP).
*	@param	pusher	The mover entity performing movement.
*	@param	move	The linear movement offset for this frame step.
*	@param	amove	The angular movement offset for this frame step.
*	@return	true if the push operation succeeded for all riders and pushed entities; false if blocked.
*	@note	Refactored 3-Phase Mover Architecture:
*			Phase 1: Pre-displacement rider acquisition at T_start.
*			Phase 2: Forward swept-volume brush contact acquisition (T_start -> T_end).
*			Phase 3: Recursive chain displacement (SVG_TryPushEntityChain), atomic prediction sync
*					 (SVG_Mover_SetEntityOrigin), and block validation against static world walls
*					 and MOVETYPE_PUSH / MOVETYPE_STOP entities.
**/
const bool SVG_PushMover( svg_base_edict_t *pusher, const Vector3 &move, const Vector3 &amove ) {
	/**
	*	Validate the mover before beginning the atomic transaction.
	**/
	if ( !pusher ) {
		return false;
	}

	/**
	*	Preserve broadphase swept bounds and the pusher transform at T_start before mutating it.
	**/
	const Vector3 pusherStartOrigin = pusher->currentOrigin;
	const Vector3 pusherStartAngles = pusher->currentAngles;
	Vector3 sweepMins = pusher->absMin;
	Vector3 sweepMaxs = pusher->absMax;
	if ( !VectorEmpty( amove ) ) {
		const Vector3 bboxCenter = ( pusher->absMin + pusher->absMax ) *	0.5f;
		const Vector3 halfSize = ( pusher->absMax - pusher->absMin ) *	0.5f;
		const float bboxRadius = QM_Vector3Length( halfSize );
		const float pivotOffset = QM_Vector3Length( pusher->currentOrigin - bboxCenter );
		const float sweepRadius = bboxRadius + pivotOffset;
		sweepMins = pusher->currentOrigin - Vector3{ sweepRadius, sweepRadius, sweepRadius };
		sweepMaxs = pusher->currentOrigin + Vector3{ sweepRadius, sweepRadius, sweepRadius };
	}
	for ( int32_t i = 0; i < 3; i++ ) {
		if ( move[ i ] < 0.0f ) {
			sweepMins[ i ] += move[ i ];
		} else {
			sweepMaxs[ i ] += move[ i ];
		}
	}

	/**
	*	Start an atomic transaction by saving the pusher state first.
	**/
	pushed_t *transactionStart = pushedState.pushedPtr;
	if ( !SVG_SavePushedEntityState( pusher ) ) {
		pushedState.obstacle = pusher;
		return false;
	}

	/**
	*	Acquire riders at T_start before mutating the pusher transform.
	**/
	uint8_t riderFlags[ MAX_EDICTS / 8 ] = {};
	for ( int32_t e = 1; e < globals.edictPool->num_edicts; e++ ) {
		svg_base_edict_t *candidate = g_edict_pool.EdictForNumber( e );
		if ( !candidate || !candidate->inUse || !candidate->isLinked ) {
			continue;
		}
		if ( candidate == pusher
			|| candidate->movetype == MOVETYPE_PUSH
			|| candidate->movetype == MOVETYPE_STOP
			|| candidate->movetype == MOVETYPE_NONE
			|| candidate->movetype == MOVETYPE_NOCLIP ) {
			continue;
		}
		if ( SVG_IsEntityRidingMover( candidate, pusher ) ) {
			// Record rider membership without mutating state until the displacement transaction succeeds.
			const int32_t byteIdx = e / 8;
			const uint8_t bitMask = static_cast<uint8_t>( 1u << ( e % 8 ) );
			riderFlags[ byteIdx ] |= bitMask;
		}
	}

	/**
	*	Move the pusher to T_end as part of the same transaction.
	**/
	const Vector3 pusherEndOrigin = pusherStartOrigin + move;
	const Vector3 pusherEndAngles = pusherStartAngles + amove;
	SVG_Util_SetEntityOrigin( pusher, pusherEndOrigin, true );
	SVG_Util_SetEntityAngles( pusher, pusherEndAngles, true );
	gi.linkentity( pusher );
	const bool carryRidersAroundPivot = SVG_ShouldCarryRidersAroundPivot( pusher );

	/**
	*	Resolve every affected entity with exact linear + angular displacement and recursive blocker
	*	handling. Any failure restores every mutation performed since transactionStart.
	**/
	for ( int32_t e = 1; e < globals.edictPool->num_edicts; e++ ) {
		svg_base_edict_t *candidate = g_edict_pool.EdictForNumber( e );
		if ( !candidate || !candidate->inUse || !candidate->isLinked ) {
			continue;
		}
		if ( candidate == pusher
			|| candidate->movetype == MOVETYPE_PUSH
			|| candidate->movetype == MOVETYPE_STOP
			|| candidate->movetype == MOVETYPE_NONE
			|| candidate->movetype == MOVETYPE_NOCLIP ) {
			continue;
		}
		if ( SVG_HasPushedEntity( candidate, transactionStart ) ) {
			continue;
		}

		const int32_t byteIdx = e / 8;
		const uint8_t bitMask = static_cast<uint8_t>( 1u << ( e % 8 ) );
		const bool isRider = ( riderFlags[ byteIdx ] & bitMask ) != 0;
		if ( pusher->movetype != MOVETYPE_PUSH && !isRider ) {
			continue;
		}

		bool isPusherContact = isRider;
		if ( !isRider ) {
			const float entRadius = SVG_GetEntityBoundingRadius( candidate ) + 1.0f;
			if ( candidate->currentOrigin.x + entRadius <= sweepMins.x
				|| candidate->currentOrigin.x - entRadius >= sweepMaxs.x
				|| candidate->currentOrigin.y + entRadius <= sweepMins.y
				|| candidate->currentOrigin.y - entRadius >= sweepMaxs.y
				|| candidate->currentOrigin.z + entRadius <= sweepMins.z
				|| candidate->currentOrigin.z - entRadius >= sweepMaxs.z ) {
				continue;
			}

			/**
			*	Test the candidate in the pusher's end frame using inverse relative motion. A candidate that
			*	merely touches a side plane at both frame endpoints is not a rider and must not be dragged
			*	tangentially through neighboring BSP; only a mover-entering sweep qualifies as a shove.
			**/
			const Vector3 localOrigin = candidate->currentOrigin - pusherStartOrigin;
			const Vector3 angularDelta = SVG_Compute3DRotationDisplacement( localOrigin, pusherStartAngles, pusherEndAngles );
			const Vector3 totalDelta = move + angularDelta;
			const Vector3 sweepStart = candidate->currentOrigin + totalDelta;
			isPusherContact = SVG_TestEntityContactWithMover( candidate, pusher, sweepStart, candidate->currentOrigin );
		}

		if ( !isPusherContact ) {
			continue;
		}

		/**
		*	Use exact pivot displacement for ordinary riders by default. The rotating-door opt-out instead
		*	gives the rider only the mover's linear displacement and lets collision resolution decide whether
		*	it can remain clear, block the mover, or slide away from the brush.
		**/
		const Vector3 relativeOrigin = candidate->currentOrigin - pusherStartOrigin;
		const Vector3 angularDelta = SVG_Compute3DRotationDisplacement( relativeOrigin, pusherStartAngles, pusherEndAngles );
		const bool carryThisRider = !isRider || carryRidersAroundPivot;
		const Vector3 totalDelta = move + ( carryThisRider ? angularDelta : Vector3{} );
		const bool includePusherAsBlocker = isRider && !carryThisRider;
		/**
		*	First apply the configured displacement. In no-carry mode this is only the mover's linear motion,
		*	allowing a rider to remain in place when the rotating brush has already moved away from it.
		*	When pivot carry is enabled, the full transformed delta remains the primary candidate; if nearby
		*	BSP or another surface clips that pose, the bounded slide resolver can preserve recoverable motion.
		**/
		bool displacementSucceeded = SVG_TryPushEntityChain( pusher, candidate, totalDelta, 0, nullptr, includePusherAsBlocker );
		if ( !displacementSucceeded && isRider && !VectorEmpty( amove ) ) {
			/**
			*	The rotating brush may have entered the rider while the rider was corner-hugging or standing on
			*	its top. Treat the configured rider displacement as a slide candidate before reporting a crush.
			**/
			displacementSucceeded = SVG_TrySlidePushedEntityChain( pusher, candidate, totalDelta, includePusherAsBlocker );
		}

		/**
		*	Preserve transactional rollback behavior when both the ordinary displacement and any valid slide
		*	candidate fail against world geometry or an unsolved solid obstruction.
		**/
		if ( !displacementSucceeded ) {
			/**
			*	A world/BSP failure belongs to the entity the mover was trying to push. Preserve that entity
			*	for the blocked policy so a door can apply its configured crush damage and reverse, while the
			*	world remains an internal collision result and can never enter an edict-free path.
			**/
			if ( !pushedState.obstacle || pushedState.obstacle->s.number <= ENTITYNUM_WORLD || !pushedState.obstacle->inUse ) {
				pushedState.obstacle = candidate;
			}
			SVG_RestorePushedStateTo( transactionStart );
			return false;
		}

		/**
		*	Preserve support for carried riders after displacement. The chain resolver clears stale ground
		*	state for ordinary pushed entities, while a carried rider remains supported by the pusher's new link.
		**/
		if ( isRider && carryThisRider ) {
			candidate->groundInfo.entityNumber = pusher->s.number;
			candidate->groundInfo.entityLinkCount = pusher->linkCount;
			if ( candidate->groundInfo.plane.normal[ 2 ] < 0.7f ) {
				VectorSet( candidate->groundInfo.plane.normal, 0.0f, 0.0f, 1.0f );
			}
		}
		if ( isRider && !carryThisRider ) {
			/**
			*	No-carry riders are not attached to the pivot. Retain support only when a fresh end-pose trace
			*	still finds the rotated pusher beneath the rider; otherwise clear stale support and let ordinary
			*	physics slide or drop the entity away on the next simulation step.
			**/
			ground_info_t endSupport = {};
			if ( SVG_IsEntitySupportedByMoverAt( candidate, pusher, candidate->currentOrigin, &endSupport ) ) {
				candidate->groundInfo = endSupport;
			} else {
				candidate->groundInfo.entityNumber = ENTITYNUM_NONE;
			}
		}

		if ( isRider && carryThisRider && candidate->client ) {
			candidate->client->ps.pmove.delta_angles[ YAW ] = QM_AngleMod( candidate->client->ps.pmove.delta_angles[ YAW ] + amove[ YAW ] );
		}
	}

	/**
	*	Dispatch trigger touches only after all transaction mutations have committed.
	**/
	for ( pushed_t *record = transactionStart; record < pushedState.pushedPtr; record++ ) {
		if ( record->ent && record->ent != pusher && record->ent->inUse ) {
			SVG_Util_TouchTriggers( record->ent );
		}
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
	// Clear the previous attempt's blocker before resolving a new team-mover transaction.
	pushedState.obstacle = nullptr;
	// Try moving all parts:
	pushedState.pushedPtr = pushedState.pushed;
	for ( part = ent; part; part = part->teamchain ) {
		// See if it needs to move:
		if ( !VectorEmpty( part->velocity ) || !VectorEmpty( part->avelocity ) ) {
			// object is moving
			const Vector3 move = part->velocity *	gi.frame_time_s; //VectorScale(part->velocity, gi.frame_time_s, move);
			const Vector3 amove = part->avelocity *	gi.frame_time_s; //VectorScale(part->avelocity, gi.frame_time_s, amove);

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
		/**
		*	Roll back every team part, not only the part that reported the obstruction. Team movers are one
		*	atomic transform and must never leave earlier members committed when a later member is blocked.
		**/
		SVG_RestorePushedStateTo( pushedState.pushed );

		// The move failed, bump all nextthink times and back out moves.
		for ( mv = ent; mv; mv = mv->teamchain ) {
			if ( mv->nextthink > 0_ms ) {
				mv->nextthink += FRAME_TIME_S;
			}
		}

		// if the pusher has a "blocked" function, call it
		// otherwise, just stay in place until the obstacle is gone
		if ( part->HasBlockedCallback() ) {
			/**
			*	Dispatch only a real live edict. World BSP is an internal collision result and must never be
			*	passed into a callback that may damage, explode, or schedule an edict free operation.
			**/
			svg_base_edict_t *blockedEntity = pushedState.obstacle;
			if ( !blockedEntity || blockedEntity->s.number <= ENTITYNUM_WORLD || !blockedEntity->inUse ) {
				blockedEntity = nullptr;
			}
			if ( blockedEntity || ( part->svFlags & SVF_DOOR ) ) {
				part->DispatchBlockedCallback( blockedEntity );
			}
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
    ent->liquidInfo.type = gi.pointcontents( &ent->currentOrigin );
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
        gi.positioned_sound( &ent->currentOrigin, g_edict_pool.EdictForNumber( 0 ), CHAN_AUTO, water_sfx_index, 1, 1, 0);
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

	const double adjustment = FRAMETIME *	sv_stopspeed *	sv_friction;
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
                //if ( ent->velocity[ 2 ] < level.gravity *	-0.1f )
                if ( ent->velocity[ 2 ] < sv_gravity->value *	-0.1f ) {
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
        newspeed = speed - ( gi.frame_time_s *	control *	friction );
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
        newspeed = speed - ( gi.frame_time_s *	control *	sv_waterfriction *	(float)ent->liquidInfo.level );
        if ( newspeed < 0 )
            newspeed = 0;
        newspeed /= speed;
        ent->velocity[ 2 ] *= newspeed;
    }

    if ( ent->velocity[ 2 ] || ent->velocity[ 1 ] || ent->velocity[ 0 ] ) {
        // apply friction
        if ( ( wasonground || ( ent->flags & ( FL_SWIM | FL_FLY ) ) ) /*&& !( ent->monsterinfo.aiflags & AI_ALTERNATE_FLY )*/ ) {
            vel = &ent->velocity[0];
            speed = sqrtf( vel[ 0 ] *	vel[ 0 ] + vel[ 1 ] *	vel[ 1 ] );
            if ( speed ) {
                friction = sv_friction;

                // Paril: lower friction for dead monsters
                if ( ent->lifeStatus )
                    friction *= 0.5f;

                //control = speed < sv_stopspeed->value ? sv_stopspeed->value : speed;
                control = speed < sv_stopspeed ? sv_stopspeed : speed;
                newspeed = speed - gi.frame_time_s *	control *	friction;

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
//                if (ent->velocity[2] < sv_gravity->value *	-0.1f)
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
//        newspeed = speed - (FRAMETIME *	control *	friction);
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
//        newspeed = speed - (FRAMETIME *	control *	sv_waterfriction *	(float)ent->liquidInfo.level);
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
//                speed = sqrtf(vel[0] *	vel[0] + vel[1] *	vel[1]);
//                if (speed) {
//                    friction = sv_friction;
//
//                    control = speed < sv_stopspeed ? sv_stopspeed : speed;
//                    newspeed = speed - FRAMETIME *	control *	friction;
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
*	@brief  For RootMotion entities.
**/
void SVG_Physics_RootMotion( svg_base_edict_t *ent ) {
    cm_contents_t mask = SVG_GetClipMask( ent );

    // airborne monsters should always check for ground
 //   if ( ent->groundInfo.entityNumber == ENTITYNUM_NONE ) {
 //       M_CheckGround( ent, mask );
 //   }

	//SVG_ClampEntityMaxVelocity( ent );
 //   // regular thinking
 //   SVG_RunEntityThink( ent );
	//return;

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
                //if ( ent->velocity[ 2 ] < level.gravity *	-0.1f )
				if ( ent->velocity[ 2 ] < sv_gravity->value *	-0.1f ) {
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
        newspeed = speed - ( gi.frame_time_s *	control *	friction );
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
        newspeed = speed - ( gi.frame_time_s *	control *	sv_waterfriction *	(float)ent->liquidInfo.level );
        if ( newspeed < 0 )
            newspeed = 0;
        newspeed /= speed;
        ent->velocity[ 2 ] *= newspeed;
    }

    if ( ent->velocity[ 2 ] || ent->velocity[ 1 ] || ent->velocity[ 0 ] ) {
        // apply friction
        if ( ( wasonground || ( ent->flags & ( FL_SWIM | FL_FLY ) ) ) /*&& !( ent->monsterinfo.aiflags & AI_ALTERNATE_FLY )*/ ) {
            vel = &ent->velocity[ 0 ];
            speed = sqrtf( vel[ 0 ] *	vel[ 0 ] + vel[ 1 ] *	vel[ 1 ] );
            if ( speed ) {
                friction = sv_friction;

                // Paril: lower friction for dead monsters
                if ( ent->lifeStatus )
                    friction *= 0.5f;

                //control = speed < sv_stopspeed->value ? sv_stopspeed->value : speed;
                control = speed < sv_stopspeed ? sv_stopspeed : speed;
                newspeed = speed - gi.frame_time_s *	control *	friction;

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
    if ( ent->movetype == MOVETYPE_STEP || ent->movetype == MOVETYPE_ROOTMOTION ) {
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

    if ( isMoveStepper ) {
        // if we moved, check and fix origin if needed
        if ( !VectorCompare( ent->currentOrigin, previousOrigin ) ) {
            svg_trace_t trace = SVG_TraceEntityShape( ent->currentOrigin, previousOrigin, ent, ent, SVG_GetClipMask( ent ) );
			if ( trace.allsolid || trace.startsolid ) {
				SVG_Util_SetEntityOrigin( ent, previousOrigin, true ); // VectorCopy( previousOrigin, ent->s.origin ); // = previous_origin;		
			}
        }
    }

    if ( ent->HasPostThinkCallback() ) {
        ent->DispatchPostThinkCallback();
    }
}
