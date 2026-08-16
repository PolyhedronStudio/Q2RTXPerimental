/********************************************************************
*
*
*	ServerGame: Monster Base Entity Class
*	File: svg_monster_base.cpp
*	Description:
*		Unified foundation implementation for all monster entities in the engine.
*		Provides navigation mesh pathfinding, waypoint progression,
*		steering, grounding, and slide-move physics execution.
*
*
********************************************************************/
// Includes needed.
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"
#include "svgame/svg_entity_events.h"
#include "svgame/entities/monster/svg_monster_base.h"
#include "svgame/nav/nav_debug.h"

// Monster move and slide move.
#include "svgame/monsters/svg_mmove.h"
#include "svgame/monsters/svg_mmove_slidemove.h"

/**
*	@brief	Find closest nav face in the current BSP leaf with fallback to global KD-tree.
*	@param	point	Query position in feet-origin space.
*	@return	Index of closest nav face or -1.
**/
static int32_t Nav_FindClosestFaceInLeaf( const Vector3 &point ) {
	/**
	*	Prefer local KD-tree face lookup for stable corner progression.
	*	Verify 2D containment and vertical proximity to ensure a valid surface.
	**/
	const int32_t leafFace = Nav_FindPolyInLeaf( point );
	if ( leafFace >= 0 && static_cast<size_t>( leafFace ) < g_nav_faces.size() ) {
		const nav_face_t &face = g_nav_faces[ leafFace ];
		if ( Nav_PointInsideFace2D( point, face ) ) {
			const Vector3DP v0 = g_nav_vertices[ g_nav_halfedges[ face.first_edge_idx ].vertex_idx ];
			const float planeDist = static_cast<float>( QM_Vector3DotProductDP( v0, face.normal ) );
			const float verticalDist = std::fabs( static_cast<float>( QM_Vector3DotProductDP( Vector3DP( point ), face.normal ) ) - planeDist );
			if ( verticalDist <= 64.0f ) {
				return leafFace;
			}
		}
	}

	/**
	*	Fallback to global closest poly lookup.
	**/
	return Nav_FindClosestPolyGlobal( point );
}

/**
*	@brief	Reconstructs the object, optionally retaining the entityDictionary.
*	@param	retainDictionary	When true, preserves the entityDictionary pointer.
**/
void svg_monster_base_t::Reset( const bool retainDictionary ) {
	IMPLEMENT_EDICT_RESET_BY_COPY_ASSIGNMENT( Super, SelfType, retainDictionary );

	monsterMove = {};
	pathNavigationState = {};
	navPath.clear();
	stringPulledPath.clear();
	stringPulledWaypointForced.clear();
	pathPos = 0;
	stringPathPos = 0;
	lastPathCalcTime = 0_ms;
	consecutiveBlockedFrames = 0;
	lastBlockedFrameTime = 0_ms;
	recentWallBlockNormal = { 0.0f, 0.0f, 0.0f };
	hasRecentWallBlockNormal = false;
	lastWallBlockTime = 0_ms;
	cachedLeaf = -1;
	cachedPoly = -1;
}

/**
*	@brief	Save the entity into a file using game_write_context.
*	@param	ctx	Context to write into.
**/
void svg_monster_base_t::Save( struct game_write_context_t *ctx ) {
	svg_base_edict_t::Save( ctx );
}

/**
*	@brief	Restore the entity from a loadgame read context.
*	@param	ctx	Context to read from.
**/
void svg_monster_base_t::Restore( struct game_read_context_t *ctx ) {
	svg_base_edict_t::Restore( ctx );
}

/**
*	@brief	Generic support routine taking care of the base logic that each onThink implementation relies on.
*	@return	True if caller should proceed with specific think logic, false if dead or should skip.
**/
const bool svg_monster_base_t::GenericThinkBegin() {
	s.renderfx &= ~( RF_STAIR_STEP | RF_OLD_FRAME_LERP );

	RecategorizeGroundAndLiquidState();

	// Sync physics state with entity state
	monsterMove.state.origin = currentOrigin;
	monsterMove.state.velocity = velocity;
	monsterMove.ground = groundInfo;
	monsterMove.liquid = liquidInfo;

	if ( groundInfo.entityNumber != ENTITYNUM_NONE ) {
		monsterMove.state.mm_flags |= MMF_ON_GROUND;
	} else {
		monsterMove.state.mm_flags &= ~MMF_ON_GROUND;
	}

	if ( health <= 0 || ( lifeStatus & LIFESTATUS_ALIVE ) != LIFESTATUS_ALIVE ) {
		return false;
	}

	return true;
}

/**
*	@brief	Generic support routine taking care of the finishing logic that each onThink implementation relies on.
*	@param	processSlideMove	When true, performs slide move physics.
*	@param	blockedMask			[out] The blockedMask result from the slide move.
*	@return	False if trapped or failed to move, true otherwise.
**/
const bool svg_monster_base_t::GenericThinkFinish( const bool processSlideMove, int32_t &blockedMask ) {
	blockedMask = ( processSlideMove ? ProcessSlideMove() : MM_SLIDEMOVEFLAG_NONE );

	velocity = monsterMove.state.velocity;
	groundInfo = monsterMove.ground;
	liquidInfo = monsterMove.liquid;
	SVG_Util_SetEntityOrigin( this, monsterMove.state.origin, true );
	gi.linkentity( this );

	if ( blockedMask & MM_SLIDEMOVEFLAG_TRAPPED ) {
		return false;
	}

	return true;
}

/**
*	@brief	Performs SlideMove processing and updates the final origin if successful.
*	@return	The blockedMask result from the slide move.
**/
const int32_t svg_monster_base_t::ProcessSlideMove() {
	monsterMove.monster = this;
	monsterMove.frameTime = FRAME_TIME_S.Seconds();
	monsterMove.state.origin = currentOrigin;
	monsterMove.state.velocity = velocity;
	monsterMove.mins = mins;
	monsterMove.maxs = maxs;
	monsterMove.ground = groundInfo;
	monsterMove.liquid = liquidInfo;
	monsterMove.navPolicy = &pathNavigationState.policy;

	const int32_t blockedMask = SVG_MMove_StepSlideMove( &monsterMove, pathNavigationState.policy );
	UpdateBlockedNavigationRecovery( blockedMask );
	return blockedMask;
}

/**
*	@brief	Recategorizes the entity's ground/liquid and ground states.
**/
const void svg_monster_base_t::RecategorizeGroundAndLiquidState() {
	const cm_contents_t mask = SVG_GetClipMask( this );
	M_CheckGround( this, mask );
	M_CatagorizePosition( this, currentOrigin, liquidInfo.level, liquidInfo.type );
}

/**
*	@brief	Retrieves the feet-origin agent bounds for navigation queries.
*	@param	out_mins	[out] Minimum bounding extent in feet-origin space.
*	@param	out_maxs	[out] Maximum bounding extent in feet-origin space.
**/
void svg_monster_base_t::GetNavigationAgentBounds( Vector3 *out_mins, Vector3 *out_maxs ) {
	if ( out_mins != nullptr ) {
		*out_mins = this->mins;
	}
	if ( out_maxs != nullptr ) {
		*out_maxs = this->maxs;
	}
}

/**
*	@brief	Clear stale async nav request state when no navmesh is loaded.
*	@return	True when navmesh is unavailable and caller should early-return.
**/
const bool svg_monster_base_t::GuardForNullNavMesh() {
	if ( g_nav_faces.empty() || g_nav_halfedges.empty() ) {
		ResetNavigationPath();
		return true;
	}
	return false;
}

/**
*	@brief	Reset cached navigation path state.
**/
void svg_monster_base_t::ResetNavigationPath() {
	navPath.clear();
	stringPulledPath.clear();
	stringPulledWaypointForced.clear();
	pathPos = 0;
	stringPathPos = 0;
}

/**
*	@brief	Check if the path should be recalculated based on distance and time.
*	@param	pos	Target destination position.
*	@return	True if path needs recalculation.
**/
const bool svg_monster_base_t::ShouldRecalcPath( const Vector3 &pos ) {
	constexpr QMTime PATH_RECALC_INTERVAL_MS = 250_ms;
	const uint64_t now = level.time.Milliseconds();
	if ( now - lastPathCalcTime.Milliseconds() > PATH_RECALC_INTERVAL_MS.Milliseconds() ) {
		return true;
	}
	return false;
}

/**
*	@brief	Find the current KD-Tree polygon the entity is standing on.
*	@return	Face index or -1.
**/
const int32_t svg_monster_base_t::FindCurrentPoly() {
	Vector3 myFeet = currentOrigin;
	myFeet.z += this->mins.z;
	return Nav_FindClosestFaceInLeaf( myFeet );
}

/**
*	@brief	Compute an A* path to the target origin.
*	@param	target	Target destination world-space position.
*	@param	force	When true, bypasses debouncing and recalculates immediately.
*	@return	Path evaluation result state.
**/
svg_monster_base_t::PathComputeResult svg_monster_base_t::ComputePathTo( const Vector3 &target, const bool force ) {
	Vector3 myFeet = currentOrigin;
	myFeet.z += this->mins.z;

	Vector3 targetFeet = target;
	if ( this->goalentity != nullptr ) {
		targetFeet.z += this->goalentity->mins.z;
	} else {
		targetFeet.z += this->mins.z;
	}

	const int32_t startFace = Nav_FindClosestFaceInLeaf( myFeet );
	const int32_t goalFace = Nav_FindClosestFaceInLeaf( targetFeet );

	if ( startFace == -1 || goalFace == -1 ) {
		return PathComputeResult::Failed;
	}

	constexpr QMTime PATH_DEBOUNCE_INTERVAL = 250_ms;
	const bool timeElapsed = ( level.time - lastPathCalcTime >= PATH_DEBOUNCE_INTERVAL );
	const bool targetMoved = ( QM_Vector3DistanceSqr( target, pathNavigationState.lastGoal.origin ) > ( 64.0f * 64.0f ) );
	const bool pathEmpty = navPath.empty() || stringPulledPath.empty();

	// Check if entity is still on the current path corridor
	bool stillOnPath = false;
	if ( !navPath.empty() && pathPos < navPath.size() ) {
		const int32_t currentFace = startFace;
		const int32_t startCheck = std::max<int32_t>( 0, static_cast<int32_t>( pathPos ) - 1 );
		const int32_t endCheck = std::min<int32_t>( static_cast<int32_t>( navPath.size() ) - 1, static_cast<int32_t>( pathPos ) + 4 );
		for ( int32_t i = startCheck; i <= endCheck; ++i ) {
			if ( navPath[ i ] == currentFace ) {
				stillOnPath = true;
				break;
			}
		}
	}

	if ( !force && !pathEmpty && stillOnPath && !targetMoved && !timeElapsed ) {
		return PathComputeResult::ReusedCached;
	}

	navPath.clear();
	stringPulledPath.clear();
	stringPulledWaypointForced.clear();
	pathPos = 0;
	stringPathPos = 0;

	const float agentRadius = static_cast<float>( std::max( std::abs( this->mins.x ), std::abs( this->maxs.x ) ) );

	if ( Nav_FindPath( startFace, goalFace, navPath, pathNavigationState.policy ) ) {
		pathNavigationState.lastGoal.origin = target;
		pathNavigationState.lastGoal.isValid = true;
		lastPathCalcTime = level.time;

		Nav_StringPull( navPath, Vector3DP( myFeet ), Vector3DP( targetFeet ), static_cast<double>( agentRadius ), stringPulledPath, &stringPulledWaypointForced );

		if ( stringPulledPath.size() >= 2 ) {
			stringPathPos = 1;
		} else {
			stringPathPos = 0;
		}
		return PathComputeResult::NewPathGenerated;
	}

	return PathComputeResult::Failed;
}

/**
*	@brief	Progress along navigation path, advance active segments, and compute lookahead steering direction.
*	@param	finalGoal		Destination goal in world space.
*	@param	outMoveDir		[out] Normalized 2D horizontal movement direction.
*	@param	outSpeedScale	[out] Speed scaling factor (0.4 to 1.0) for smooth corner deceleration.
*	@return	True if movement towards goal should continue, false if arrived at goal.
**/
const bool svg_monster_base_t::ComputePathSteering( const Vector3DP &finalGoal, Vector3DP *outMoveDir, double *outSpeedScale ) {
	/**
	*	Sanity checks: ensure valid output pointers.
	**/
	if ( outMoveDir == nullptr || outSpeedScale == nullptr ) {
		return false;
	}

	Vector3DP myFeetDP = Vector3DP( currentOrigin );
	myFeetDP.z += static_cast<double>( this->mins.z );

	/**
	*	Direct goal fallback when no string-pulled path polyline is available.
	**/
	if ( stringPulledPath.empty() ) {
		Vector3DP toGoal = finalGoal - myFeetDP;
		const double verticalDist = std::fabs( toGoal.z );
		toGoal.z = 0.0;
		const double dist2D = QM_Vector3LengthDP( toGoal );
		if ( dist2D <= 16.0 && verticalDist <= 32.0 ) {
			return false;
		}
		*outMoveDir = ( dist2D > 0.001 ) ? ( toGoal * ( 1.0 / dist2D ) ) : Vector3DP{ 1.0, 0.0, 0.0 };
		*outSpeedScale = 1.0;
		return true;
	}

	/**
	*	Synchronize navPath polygon index with current physical standing surface.
	**/
	const int32_t currentFace = Nav_FindClosestFaceInLeaf( static_cast<Vector3>( myFeetDP ) );
	if ( !navPath.empty() && currentFace != -1 ) {
		const int32_t localStart = std::max<int32_t>( 0, static_cast<int32_t>( pathPos ) - 2 );
		const int32_t localEnd = std::min<int32_t>( static_cast<int32_t>( navPath.size() ) - 1, static_cast<int32_t>( pathPos ) + 6 );
		for ( int32_t i = localStart; i <= localEnd; i++ ) {
			if ( navPath[ i ] == currentFace ) {
				if ( i > static_cast<int32_t>( pathPos ) ) {
					pathPos = static_cast<size_t>( i );
				}
				break;
			}
		}
	}

	/**
	*	Detect ramp/slope surface.
	**/
	bool onRamp = false;
	if ( currentFace >= 0 && static_cast<size_t>( currentFace ) < g_nav_faces.size() ) {
		if ( g_nav_faces[ currentFace ].normal.z < 0.99 ) {
			onRamp = true;
		}
	}

	/**
	*	Clamp stringPathPos to valid range [1, N-1] or 0.
	**/
	if ( stringPathPos == 0 && stringPulledPath.size() >= 2 ) {
		stringPathPos = 1;
	}

	/**
	*	Waypoint arrival & segment advancement loop:
	*	Advances stringPathPos as each intermediate waypoint is reached or passed.
	**/
	while ( stringPathPos < stringPulledPath.size() - 1 ) {
		const Vector3DP currentWp = stringPulledPath[ stringPathPos ];
		Vector3DP toWp = currentWp - myFeetDP;
		const double zDiff = toWp.z;
		toWp.z = 0.0;
		const double dist2DSqr = QM_Vector3DotProductDP( toWp, toWp );

		// Vertical step-up constraint: if target is elevated, wait until feet reach step height
		const bool needsStepUp = ( zDiff > 8.0 ) && !onRamp;
		const bool hasSteppedUp = !needsStepUp || ( myFeetDP.z >= currentWp.z - 4.0 );

		// 1) Arrival radius around intermediate waypoint (16.0 units)
		constexpr double REACH_RADIUS_SQR = 16.0 * 16.0;
		const bool withinRadius = ( dist2DSqr <= REACH_RADIUS_SQR );

		// 2) Passed waypoint projection into next segment
		bool passedIntoNext = false;
		if ( stringPathPos + 1 < stringPulledPath.size() ) {
			const Vector3DP nextWp = stringPulledPath[ stringPathPos + 1 ];
			Vector3DP nextSegDir = nextWp - currentWp;
			nextSegDir.z = 0.0;
			const double nextSegLen = QM_Vector3LengthDP( nextSegDir );
			if ( nextSegLen > 0.001 ) {
				nextSegDir = nextSegDir * ( 1.0 / nextSegLen );
				Vector3DP fromCurrent = myFeetDP - currentWp;
				fromCurrent.z = 0.0;
				const double forwardAlongNext = QM_Vector3DotProductDP( fromCurrent, nextSegDir );
				if ( forwardAlongNext > 0.0 && dist2DSqr <= ( 36.0 * 36.0 ) ) {
					passedIntoNext = true;
				}
			}
		}

		if ( ( withinRadius || passedIntoNext ) && hasSteppedUp ) {
			++stringPathPos;
			continue;
		}

		break;
	}

	// Final goal arrival check
	if ( stringPathPos >= stringPulledPath.size() - 1 ) {
		const Vector3DP finalGoalWp = stringPulledPath.back();
		Vector3DP toFinal = finalGoalWp - myFeetDP;
		const double finalZDiff = toFinal.z;
		toFinal.z = 0.0;
		if ( QM_Vector3DotProductDP( toFinal, toFinal ) <= ( 16.0 * 16.0 ) && std::fabs( finalZDiff ) <= 32.0 ) {
			return false; // Arrived at destination
		}
	}

	/**
	*	Compute Continuous Lookahead Steering Target along the Path Polyline:
	*	Project feet position onto active segment (W_{k-1} -> W_k) and sample forward
	*	by lookahead distance (24 units), seamlessly peering into W_k -> W_{k+1} across turns.
	**/
	const size_t k = std::min( stringPathPos, stringPulledPath.size() - 1 );
	const size_t prevIdx = ( k > 0 ) ? ( k - 1 ) : 0;
	const Vector3DP w0 = stringPulledPath[ prevIdx ];
	const Vector3DP w1 = stringPulledPath[ k ];

	Vector3DP segDir = w1 - w0;
	segDir.z = 0.0;
	const double segLen = QM_Vector3LengthDP( segDir );

	Vector3DP steerTarget = w1;
	Vector3DP forwardDir = ( segLen > 0.001 ) ? ( segDir * ( 1.0 / segLen ) ) : Vector3DP{ 1.0, 0.0, 0.0 };
	constexpr double LOOKAHEAD_DIST = 24.0;

	if ( segLen > 0.001 ) {
		const Vector3DP segDirNorm = forwardDir;
		Vector3DP toFeet = myFeetDP - w0;
		toFeet.z = 0.0;
		const double t = std::clamp( QM_Vector3DotProductDP( toFeet, segDirNorm ) / segLen, 0.0, 1.0 );
		const Vector3DP projPoint = w0 + segDirNorm * ( t * segLen );
		const double distRemOnSeg = ( 1.0 - t ) * segLen;

		if ( distRemOnSeg >= LOOKAHEAD_DIST || k >= stringPulledPath.size() - 1 ) {
			// Lookahead point lies entirely on current segment
			steerTarget = projPoint + segDirNorm * std::min( distRemOnSeg, LOOKAHEAD_DIST );
		} else {
			// Lookahead reaches end of current segment (W_0 -> W_1)
			const size_t nextIdx = k + 1;
			const Vector3DP w2 = stringPulledPath[ nextIdx ];
			Vector3DP nextSegDir = w2 - w1;
			nextSegDir.z = 0.0;
			const double nextSegLen = QM_Vector3LengthDP( nextSegDir );

			double turnDot = 1.0;
			if ( nextSegLen > 0.001 ) {
				const Vector3DP nextSegDirNorm = nextSegDir * ( 1.0 / nextSegLen );
				turnDot = QM_Vector3DotProductDP( segDirNorm, nextSegDirNorm );
				forwardDir = nextSegDirNorm;
			}

			if ( turnDot > 0.50 && nextSegLen > 0.001 ) {
				// Gentle turn (< 60 degrees): safe to smoothly blend into next segment
				const Vector3DP nextSegDirNorm = nextSegDir * ( 1.0 / nextSegLen );
				const double nextAdvance = std::min( LOOKAHEAD_DIST - distRemOnSeg, nextSegLen );
				steerTarget = w1 + nextSegDirNorm * nextAdvance;
			} else {
				// Sharp turn / U-turn: steer directly to corner waypoint W_1
				steerTarget = w1;
			}
		}
	} else {
		steerTarget = w1;
	}

	/**
	*	Compute 2D horizontal steering vector and normalize.
	**/
	Vector3DP toSteer = steerTarget - myFeetDP;
	toSteer.z = 0.0;
	const double steerDist = QM_Vector3LengthDP( toSteer );

	// If standing on target or steering vector reverses path direction, use forward path direction
	if ( steerDist <= 0.001 || QM_Vector3DotProductDP( toSteer, forwardDir ) <= 0.0 ) {
		*outMoveDir = forwardDir;
	} else {
		*outMoveDir = toSteer * ( 1.0 / steerDist );
	}

	/**
	*	Smooth corner deceleration when turning sharply towards the lookahead target.
	**/
	const double steerYaw = QM_AngleMod( QM_Vector3ToYawDP( *outMoveDir ) );
	const double currentYaw = QM_AngleMod( static_cast<double>( currentAngles[ YAW ] ) );
	const double yawDeltaAbs = std::fabs( QM_AngleDelta( steerYaw, currentYaw ) );

	double speedScale = 1.0;
	if ( yawDeltaAbs > 35.0 ) {
		speedScale = std::max( 0.40, ( 180.0 - yawDeltaAbs ) / 145.0 );
	}
	*outSpeedScale = speedScale;

	return true;
}

/**
*	@brief	Steer and move entity towards goal origin using navigation path steering.
*	@param	goalOrigin	Target world-space position.
*	@return	True if entity moved, false if arrived or stopped.
**/
const bool svg_monster_base_t::StepMoveToGoal( const Vector3 &goalOrigin ) {
	const Vector3DP goalOriginDP = Vector3DP( goalOrigin );

	Vector3DP moveDirDP = {};
	double speedScale = 1.0;

	if ( !ComputePathSteering( goalOriginDP, &moveDirDP, &speedScale ) ) {
		velocity.x = velocity.y = 0.0f;
		monsterMove.state.velocity.x = velocity.x;
		monsterMove.state.velocity.y = velocity.y;
		UpdateAnim( 1 ); // IDLE
		return false;
	}

	/**
	*	Wall contact glancing deflection:
	*	If the entity is currently in contact with a vertical wall or corner edge
	*	and the path steering direction pushes into the wall normal (dot < 0),
	*	deflect the move direction along the wall surface with a gentle outward bias
	*	so the entity smoothly slides off corners without getting pinned at the vertex.
	**/
	if ( hasRecentWallBlockNormal && ( level.time - lastWallBlockTime ) <= 200_ms ) {
		Vector3DP wallNormDP = Vector3DP( recentWallBlockNormal );
		wallNormDP.z = 0.0;
		const double wallNormLen = QM_Vector3LengthDP( wallNormDP );
		if ( wallNormLen > 0.001 ) {
			wallNormDP = wallNormDP * ( 1.0 / wallNormLen );
			const double dot = QM_Vector3DotProductDP( moveDirDP, wallNormDP );
			if ( dot < 0.0 ) {
				Vector3DP deflected = moveDirDP - wallNormDP * dot + wallNormDP * 0.25;
				deflected.z = 0.0;
				const double defLen = QM_Vector3LengthDP( deflected );
				if ( defLen > 0.001 ) {
					moveDirDP = deflected * ( 1.0 / defLen );
				}
			}
		}
	}

	ideal_yaw = static_cast<float>( QM_Vector3ToYawDP( moveDirDP ) );
	SVG_MMove_FaceIdealYaw( this, ideal_yaw, 50.0f );

	constexpr double baseFrameVelocity = 220.0;
	const double frameVelocity = baseFrameVelocity * speedScale;

	const Vector3DP velocityDP = moveDirDP * frameVelocity;
	velocity.x = static_cast<float>( velocityDP.x );
	velocity.y = static_cast<float>( velocityDP.y );
	monsterMove.state.velocity.x = velocity.x;
	monsterMove.state.velocity.y = velocity.y;
	UpdateAnim( 4 ); // RUN
	return true;
}

/**
*	@brief	High-level navigation driver that recalculates paths when necessary and drives movement.
*	@param	goalOrigin	Target world-space position.
*	@param	force		When true, forces path recalculation.
*	@return	True if movement was updated, false otherwise.
**/
const bool svg_monster_base_t::MoveAStarToOrigin( const Vector3 &goalOrigin, bool force ) {
	if ( GuardForNullNavMesh() ) {
		return false;
	}

	if ( !( monsterMove.state.mm_flags & MMF_ON_GROUND ) ) {
		UpdateAnim( 4 ); // RUN
		Vector3DP dummyDir = {};
		double dummyScale = 1.0;
		ComputePathSteering( Vector3DP( goalOrigin ), &dummyDir, &dummyScale );
		return true;
	}

	ComputePathTo( goalOrigin, force );
	return StepMoveToGoal( goalOrigin );
}

/**
*	@brief	Get the next active waypoint from the navigation path in full double precision.
*	@param	finalGoal	Destination goal in world space.
*	@return	Next waypoint position.
**/
const Vector3DP svg_monster_base_t::NextWaypoint( const Vector3DP &finalGoal ) {
	if ( stringPulledPath.empty() || stringPathPos >= stringPulledPath.size() ) {
		return finalGoal;
	}
	return stringPulledPath[ stringPathPos ];
}

/**
*	@brief	Get the next active waypoint from the navigation path (single precision convenience wrapper).
*	@param	finalGoal	Destination goal in world space.
*	@return	Next waypoint position.
**/
const Vector3 svg_monster_base_t::NextWaypoint( const Vector3 &finalGoal ) {
	return static_cast<Vector3>( NextWaypoint( Vector3DP( finalGoal ) ) );
}

/**
*	@brief	Update blocked/trapped recovery bookkeeping and force path refresh after sustained stalls.
*	@param	blockedMask	Slide move blocked flags for the current think frame.
**/
void svg_monster_base_t::UpdateBlockedNavigationRecovery( const int32_t blockedMask ) {
	const bool isBlockedThisFrame = ( ( blockedMask & ( MM_SLIDEMOVEFLAG_BLOCKED | MM_SLIDEMOVEFLAG_TRAPPED ) ) != 0 );
	const bool isHardBlockedThisFrame = ( ( blockedMask & MM_SLIDEMOVEFLAG_TRAPPED ) != 0 );

	if ( !isBlockedThisFrame ) {
		consecutiveBlockedFrames = 0;
		hasRecentWallBlockNormal = false;
		return;
	}

	// Capture wall contact normal for navigation diagnostics
	hasRecentWallBlockNormal = false;
	for ( uint32_t i = 0; i < monsterMove.touchTraces.numberOfTraces; i++ ) {
		const svg_trace_t &touchTrace = monsterMove.touchTraces.traces[ i ];
		if ( touchTrace.plane.normal[ 2 ] < MM_MIN_WALL_NORMAL_Z ) {
			recentWallBlockNormal = Vector3{ touchTrace.plane.normal[ 0 ], touchTrace.plane.normal[ 1 ], touchTrace.plane.normal[ 2 ] };
			hasRecentWallBlockNormal = true;
			lastWallBlockTime = level.time;
			break;
		}
	}

	if ( isHardBlockedThisFrame ) {
		ResetNavigationPath();
		lastPathCalcTime = 0_ms;
		consecutiveBlockedFrames = 0;
		lastBlockedFrameTime = level.time;
		return;
	}

	const uint64_t nowMs = level.time.Milliseconds();
	const uint64_t lastMs = lastBlockedFrameTime.Milliseconds();
	const uint64_t maxGapMs = FRAME_TIME_MS.Milliseconds() * 2;
	const bool isContiguousSample = ( lastMs > 0 && ( nowMs - lastMs ) <= maxGapMs );
	if ( isContiguousSample ) {
		consecutiveBlockedFrames++;
	} else {
		consecutiveBlockedFrames = 1;
	}
	lastBlockedFrameTime = level.time;

	static constexpr int32_t STUCK_RECOVER_BLOCKED_FRAMES = 8;
	if ( consecutiveBlockedFrames >= STUCK_RECOVER_BLOCKED_FRAMES ) {
		ResetNavigationPath();
		lastPathCalcTime = 0_ms;
		consecutiveBlockedFrames = 0;
	}
}
