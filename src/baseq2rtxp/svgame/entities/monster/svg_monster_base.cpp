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
	if ( this->goalentity != nullptr && QM_Vector3DistanceSqr( target, this->goalentity->currentOrigin ) < ( 8.0f * 8.0f ) ) {
		targetFeet.z += this->goalentity->mins.z;
	}

	/**
	*	Dynamically derive physical agent dimensions from entity bounding box with canonical fallbacks:
	*	PHYS_DEFAULT_BBOX_STANDUP_MINS = { -16., -16., -36. }, PHYS_DEFAULT_BBOX_STANDUP_MAXS = { 16., 16., 36. },
	*	PHYS_DEFAULT_VIEWHEIGHT_STANDUP = 30.
	**/
	const double rawRadiusX = std::max( std::abs( static_cast<double>( this->mins.x ) ), std::abs( static_cast<double>( this->maxs.x ) ) );
	const double rawRadiusY = std::max( std::abs( static_cast<double>( this->mins.y ) ), std::abs( static_cast<double>( this->maxs.y ) ) );
	const double rawRadius = std::max( rawRadiusX, rawRadiusY );
	constexpr double defaultRadius = 16.0; // max(|PHYS_DEFAULT_BBOX_STANDUP_MINS.x|, |PHYS_DEFAULT_BBOX_STANDUP_MAXS.x|)
	const double agentRadius = ( rawRadius > 0.0 ) ? rawRadius : defaultRadius;
	const double agentRadiusSqr = agentRadius * agentRadius;

	const double rawHeight = static_cast<double>( this->maxs.z - this->mins.z );
	constexpr double defaultHeight = 72.0; // PHYS_DEFAULT_BBOX_STANDUP_MAXS.z - PHYS_DEFAULT_BBOX_STANDUP_MINS.z
	const double agentHeight = ( rawHeight > 0.0 ) ? rawHeight : defaultHeight;

	const double effectiveViewHeight = ( this->viewheight > 0.0f ) ? static_cast<double>( this->viewheight ) : PHYS_DEFAULT_VIEWHEIGHT_STANDUP;

	const int32_t startFace = Nav_FindClosestFaceInLeaf( myFeet );
	const int32_t goalFace = Nav_FindClosestFaceInLeaf( targetFeet );

	if ( startFace == -1 || goalFace == -1 ) {
		return PathComputeResult::Failed;
	}

	const bool targetMoved = ( QM_Vector3DistanceSqr( target, pathNavigationState.lastGoal.origin ) > ( 48.0f * 48.0f ) );
	const bool pathEmpty = navPath.empty() || stringPulledPath.empty();

	/**
	*	Check if entity remains on or physically intersecting the active navigation path corridor.
	*	We avoid strict 1-polygon point checks that break when a 32-unit-wide capsule crosses polygon boundaries.
	**/
	bool stillOnPath = false;
	if ( !navPath.empty() && pathPos < navPath.size() ) {
		const int32_t currentFace = startFace;
		const int32_t startCheck = std::max<int32_t>( 0, static_cast<int32_t>( pathPos ) - 1 );
		const int32_t endCheck = std::min<int32_t>( static_cast<int32_t>( navPath.size() ) - 1, static_cast<int32_t>( pathPos ) + 4 );

		// 1) Direct polygon containment or 2D capsule disk intersection with corridor faces
		const Vector3DP feetPosDP = Vector3DP( myFeet );
		for ( int32_t i = startCheck; i <= endCheck; ++i ) {
			const int32_t faceIdx = navPath[ i ];
			if ( faceIdx < 0 || static_cast<size_t>( faceIdx ) >= g_nav_faces.size() ) {
				continue;
			}

			if ( faceIdx == currentFace ) {
				stillOnPath = true;
				break;
			}

			const nav_face_t &face = g_nav_faces[ faceIdx ];
			// Check if the agent's circular footprint overlaps the corridor face
			if ( Nav_PointInsideFace2D( feetPosDP, face ) ) {
				stillOnPath = true;
				break;
			}

			// Boundary edge proximity: if closest point on any face edge is within agent radius, capsule intersects
			for ( int32_t e = 0; e < face.num_edges; ++e ) {
				const nav_halfedge_t &he = g_nav_halfedges[ face.first_edge_idx + e ];
				const Vector3DP &v0 = g_nav_vertices[ he.vertex_idx ];
				const Vector3DP &v1 = g_nav_vertices[ g_nav_halfedges[ he.next_idx ].vertex_idx ];
				if ( Nav_DistancePointToSegment2DSqr( feetPosDP, v0, v1 ) <= agentRadiusSqr ) {
					stillOnPath = true;
					break;
				}
			}
			if ( stillOnPath ) {
				break;
			}
		}

		// 2) Topological neighbor tolerance: check if currentFace shares a boundary half-edge with any corridor face
		if ( !stillOnPath && currentFace >= 0 && static_cast<size_t>( currentFace ) < g_nav_faces.size() ) {
			for ( int32_t i = startCheck; i <= endCheck && !stillOnPath; ++i ) {
				const int32_t faceIdx = navPath[ i ];
				if ( faceIdx < 0 || static_cast<size_t>( faceIdx ) >= g_nav_faces.size() ) {
					continue;
				}

				const nav_face_t &corridorFace = g_nav_faces[ faceIdx ];
				for ( int32_t e = 0; e < corridorFace.num_edges; ++e ) {
					const nav_halfedge_t &he = g_nav_halfedges[ corridorFace.first_edge_idx + e ];
					if ( he.twin_idx != -1 && g_nav_halfedges[ he.twin_idx ].face_idx == currentFace ) {
						stillOnPath = true;
						break;
					}
				}
			}
		}
	}

	// 3) Physical polyline segment proximity: test distance from agent feet to the active string-pulled segment
	if ( !stillOnPath && !stringPulledPath.empty() ) {
		const size_t k = std::min( stringPathPos, stringPulledPath.size() - 1 );
		const size_t prevIdx = ( k > 0 ) ? ( k - 1 ) : 0;
		const Vector3DP &wpPrev = stringPulledPath[ prevIdx ];
		const Vector3DP &wpCurr = stringPulledPath[ k ];

		const double corridorLateralDist = agentRadius * 2.0 + MONSTER_NAV_CORRIDOR_MARGIN;
		const double corridorLateralDistSqr = corridorLateralDist * corridorLateralDist;
		constexpr double maxVerticalTolerance = static_cast<double>( NAV_MAX_STEP_HEIGHT ) + MONSTER_NAV_VERTICAL_STEP_TOLERANCE;

		const double distToSegSqr = Nav_DistancePointToSegment2DSqr( Vector3DP( myFeet ), wpPrev, wpCurr );
		const double zDelta = std::fabs( static_cast<double>( myFeet.z ) - wpCurr.z );
		if ( distToSegSqr <= corridorLateralDistSqr && zDelta <= maxVerticalTolerance ) {
			stillOnPath = true;
		}
	}

	/**
	*	Anti-Chattering Path Commitment Hysteresis:
	*	When the destination has not moved, enforce a minimum recalculation interval (400 ms).
	*	This allows the agent sufficient time (traveling ~88 units at 220 u/s) to clear the
	*	2*R_agent (32 unit) bifurcation decision manifold, decisively making the chosen route shorter
	*	and permanently preventing high-frequency 10-Hz flip-flopping across polygon boundaries.
	**/
	const bool cooldownElapsed = ( ( level.time - lastPathCalcTime ) >= MONSTER_NAV_PATH_RECALC_MIN_INTERVAL );

	// Reuse active path while entity remains on or near corridor and target has not moved significantly
	if ( !force && !pathEmpty && !targetMoved ) {
		if ( stillOnPath || !cooldownElapsed ) {
			return PathComputeResult::ReusedCached;
		}
	}

	navPath.clear();
	stringPulledPath.clear();
	stringPulledWaypointForced.clear();
	pathPos = 0;
	stringPathPos = 0;

	nav_path_policy_t pathPolicy = pathNavigationState.policy;
	if ( pathPolicy.agent_radius <= 0.0 ) {
		pathPolicy.agent_radius = static_cast<float>( agentRadius );
	}

	if ( Nav_FindPath( startFace, goalFace, navPath, pathPolicy ) ) {
		pathNavigationState.lastGoal.origin = target;
		pathNavigationState.lastGoal.isValid = true;
		lastPathCalcTime = level.time;

		Nav_StringPull( navPath, Vector3DP( myFeet ), Vector3DP( targetFeet ), agentRadius, stringPulledPath, &stringPulledWaypointForced, this->mins, this->maxs, static_cast<int32_t>( SVG_MMove_GetNativeShape( this ) ) );

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
	*	Validates unobstructed physical line-of-sight before moving directly towards destination.
	**/
	if ( stringPulledPath.empty() ) {
		Vector3DP toGoal = finalGoal - myFeetDP;
		const double verticalDist = std::fabs( toGoal.z );
		toGoal.z = 0.0;
		const double dist2D = QM_Vector3LengthDP( toGoal );
		if ( dist2D <= 16.0 && verticalDist <= 32.0 ) {
			return false;
		}

		// Prevent blind wall walking: only move directly if there is unobstructed entity swept line-of-sight through world geometry
		const Vector3 startTrace = currentOrigin;
		Vector3 endTrace = static_cast<Vector3>( finalGoal );
		endTrace.z -= this->mins.z;
		const svg_trace_t losTr = SVG_MMove_Trace( startTrace, this->mins, this->maxs, endTrace, this, CM_CONTENTMASK_SOLID, MM_SHAPE_AUTO );
		if ( losTr.fraction < 1.0f || losTr.startsolid || losTr.allsolid ) {
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
	*	Detect ramp/slope surface:
	*	Check whether either the agent's current nav face or the active waypoint face is inclined.
	**/
	bool onRamp = false;
	if ( currentFace >= 0 && static_cast<size_t>( currentFace ) < g_nav_faces.size() ) {
		if ( g_nav_faces[ currentFace ].normal.z < NAV_RAMP_MAX_NORMAL_Z ) {
			onRamp = true;
		}
	}
	if ( !onRamp && stringPathPos < stringPulledPath.size() ) {
		const int32_t wpFace = Nav_FindClosestFaceInLeaf( stringPulledPath[ stringPathPos ] );
		if ( wpFace >= 0 && static_cast<size_t>( wpFace ) < g_nav_faces.size() ) {
			if ( g_nav_faces[ wpFace ].normal.z < NAV_RAMP_MAX_NORMAL_Z ) {
				onRamp = true;
			}
		}
	}

	/**
	*	Clamp stringPathPos to valid range [1, N-1] or 0.
	**/
	if ( stringPathPos == 0 && stringPulledPath.size() >= 2 ) {
		stringPathPos = 1;
	}

	/**
	*	Waypoint arrival & segment advancement:
	*	Advances stringPathPos as each intermediate waypoint is reached or passed along its incoming segment.
	**/
	while ( stringPathPos < stringPulledPath.size() - 1 ) {
		const Vector3DP currentWp = stringPulledPath[ stringPathPos ];
		Vector3DP toWp = currentWp - myFeetDP;
		const double zDiff = toWp.z;
		toWp.z = 0.0;
		const double dist2DSqr = QM_Vector3DotProductDP( toWp, toWp );

		// Check if current waypoint is a forced constraint (corner standoff, stair crossing)
		const bool isForcedWp = ( stringPathPos < stringPulledWaypointForced.size() && stringPulledWaypointForced[ stringPathPos ] );

		// Vertical step transition arrival verification:
		// Only forced stair step-ups require waiting for physical vertical elevation changes,
		// because turning before elevating slides the agent into the step riser.
		// Step-downs, flat ground, and ramps allow advancing past the edge to continue traversal onto the lower surface.
		if ( isForcedWp && !onRamp ) {
			if ( zDiff > NAV_STEP_MIN_VERTICAL_DELTA ) {
				// Step-up: hold waypoint until entity has physically stepped up onto the tread
				if ( myFeetDP.z < currentWp.z - MONSTER_NAV_VERTICAL_STEP_TOLERANCE ) {
					break;
				}
			}
		}

		// Check turn angle at W_k:
		bool isSharpTurn = isForcedWp;
		if ( stringPathPos > 0 && stringPathPos + 1 < stringPulledPath.size() ) {
			const Vector3DP prevWp = stringPulledPath[ stringPathPos - 1 ];
			const Vector3DP nextWp = stringPulledPath[ stringPathPos + 1 ];
			Vector3DP inDir = currentWp - prevWp;
			Vector3DP outDir = nextWp - currentWp;
			inDir.z = 0.0;
			outDir.z = 0.0;
			const double inLen = QM_Vector3LengthDP( inDir );
			const double outLen = QM_Vector3LengthDP( outDir );
			if ( inLen > 0.001 && outLen > 0.001 ) {
				const double turnDot = QM_Vector3DotProductDP( inDir * ( 1.0 / inLen ), outDir * ( 1.0 / outLen ) );
				// If turn angle is sharper than 30 degrees, treat as a sharp turn
				if ( turnDot < MONSTER_NAV_GENTLE_TURN_MIN_DOT ) {
					isSharpTurn = true;
				}
			}
		}

		// 1) Arrival radius around intermediate waypoint (strict arrival on sharp corners or forced constraints)
		const double reachRadiusSqr = isSharpTurn ? MONSTER_NAV_SHARP_CORNER_REACH_RADIUS_SQR : MONSTER_NAV_WAYPOINT_REACH_RADIUS_SQR;
		const bool withinRadius = ( dist2DSqr <= reachRadiusSqr );

		// 2) Passed waypoint plane along the incoming segment (only allowed on straight, unforced, flat/ramp sections)
		bool passedPlane = false;
		if ( !isSharpTurn && !isForcedWp && stringPathPos > 0 && ( onRamp || std::fabs( zDiff ) <= MONSTER_NAV_STEP_MIN_DELTA ) ) {
			const Vector3DP prevWp = stringPulledPath[ stringPathPos - 1 ];
			Vector3DP inSegDir = currentWp - prevWp;
			inSegDir.z = 0.0;
			const double inSegLen = QM_Vector3LengthDP( inSegDir );

			if ( inSegLen > 0.001 ) {
				inSegDir = inSegDir * ( 1.0 / inSegLen );
				Vector3DP fromTarget = myFeetDP - currentWp;
				fromTarget.z = 0.0;
				const double forwardPast = QM_Vector3DotProductDP( fromTarget, inSegDir );
				const double lateralDistSqr = QM_Vector3DotProductDP( fromTarget, fromTarget ) - ( forwardPast * forwardPast );
				// Crossed the plane of W_k while remaining within the corridor lateral bounds
				if ( forwardPast >= 0.0 && lateralDistSqr <= MONSTER_NAV_SHARP_CORNER_REACH_RADIUS_SQR && dist2DSqr <= MONSTER_NAV_WAYPOINT_REACH_RADIUS_SQR ) {
					passedPlane = true;
				}
			}
		}

		if ( withinRadius ) {
			++stringPathPos;
			continue;
		}

		if ( passedPlane ) {
			// When passing by W_k along a straight flat section, verify that moving directly toward
			// W_{k+1} is physically unobstructed before cutting over to the next segment early.
			if ( stringPathPos + 1 < stringPulledPath.size() ) {
				const Vector3DP &nextWp = stringPulledPath[ stringPathPos + 1 ];
				const Vector3 traceStart = currentOrigin;
				Vector3 traceEnd = static_cast<Vector3>( nextWp );
				traceEnd.z -= this->mins.z;
				const svg_trace_t tr = SVG_MMove_Trace( traceStart, mins, maxs, traceEnd, this, CM_CONTENTMASK_SOLID, SVG_MMove_GetNativeShape( this ) );
				if ( tr.fraction < 1.0f || tr.startsolid || tr.allsolid ) {
					// Direct corridor to W_{k+1} is occluded; continue steering towards W_k until withinRadius
					break;
				}
			}

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
		if ( QM_Vector3DotProductDP( toFinal, toFinal ) <= MONSTER_NAV_WAYPOINT_REACH_RADIUS_SQR && std::fabs( finalZDiff ) <= MONSTER_NAV_FINAL_GOAL_MAX_Z_DELTA ) {
			return false; // Arrived at destination
		}
	}

	/**
	*	Compute Target Steering Vector:
	*	Directly track the active path waypoint W_k (stringPathPos).
	*	If approaching W_k within lookahead distance and rounding a gentle turn (< 30 deg),
	*	smoothly look ahead into W_{k+1}. Sharp corners and forced standoff waypoints
	*	strictly steer directly to W_k to guarantee full corner clearance before commencing the turn.
	**/
	const size_t k = std::min( stringPathPos, stringPulledPath.size() - 1 );
	const Vector3DP targetWp = stringPulledPath[ k ];

	Vector3DP toTarget = targetWp - myFeetDP;
	toTarget.z = 0.0;
	const double distToTarget = QM_Vector3LengthDP( toTarget );

	Vector3DP steerTarget = targetWp;

	const bool isTargetForced = ( k < stringPulledWaypointForced.size() && stringPulledWaypointForced[ k ] );
	if ( !isTargetForced && distToTarget < MONSTER_NAV_LOOKAHEAD_DISTANCE && k + 1 < stringPulledPath.size() ) {
		const size_t prevIdx = ( k > 0 ) ? ( k - 1 ) : 0;
		const Vector3DP prevWp = stringPulledPath[ prevIdx ];
		Vector3DP currSeg = targetWp - prevWp;
		currSeg.z = 0.0;
		const double currSegLen = QM_Vector3LengthDP( currSeg );

		const Vector3DP nextWp = stringPulledPath[ k + 1 ];
		Vector3DP nextSeg = nextWp - targetWp;
		nextSeg.z = 0.0;
		const double nextSegLen = QM_Vector3LengthDP( nextSeg );

		if ( currSegLen > 0.001 && nextSegLen > 0.001 ) {
			const Vector3DP currSegNorm = currSeg * ( 1.0 / currSegLen );
			const Vector3DP nextSegNorm = nextSeg * ( 1.0 / nextSegLen );
			const double turnDot = QM_Vector3DotProductDP( currSegNorm, nextSegNorm );

			// Only blend forward across gentle turns (< 30 degrees) on flat ground or ramps; sharp turns and stair steps must round W_k directly
			if ( turnDot > MONSTER_NAV_GENTLE_TURN_MIN_DOT && ( onRamp || std::fabs( nextWp.z - targetWp.z ) <= MONSTER_NAV_STEP_MIN_DELTA ) ) {
				const double advance = std::min( MONSTER_NAV_LOOKAHEAD_DISTANCE - distToTarget, nextSegLen * 0.5 );
				steerTarget = targetWp + nextSegNorm * advance;
			}
		}
	}

	/**
	*	Compute 2D horizontal steering vector and normalize.
	**/
	Vector3DP toSteer = steerTarget - myFeetDP;
	toSteer.z = 0.0;
	const double steerDist = QM_Vector3LengthDP( toSteer );

	// Precompute normalized direction vector corresponding to current ideal_yaw as fallback:
	const double yawRad = static_cast<double>( ideal_yaw ) * ( QM_PI / 180.0 );
	const Vector3DP forwardYawDir( std::cos( yawRad ), std::sin( yawRad ), 0.0 );

	// Identify whether the entity is at a forced step-up riser actively waiting for feet elevation:
	const bool awaitingStepUp = ( isTargetForced && !onRamp && ( targetWp.z - myFeetDP.z ) > MONSTER_NAV_STEP_MIN_DELTA );

	// Case 1: Outside proximity deadband — steer directly towards the lookahead target.
	// Condition check: Entity is >= MONSTER_NAV_WAYPOINT_DEADBAND (4.0 units) from steerTarget,
	// safely away from the (0, 0) division singularity where micro-fluctuations flip yaw angles.
	if ( steerDist >= MONSTER_NAV_WAYPOINT_DEADBAND ) {
		*outMoveDir = toSteer * ( 1.0 / steerDist );
	}
	// Case 2: Inside deadband of an intermediate waypoint (flat ground, gentle turns, ramps, or step-downs).
	// Condition check: Entity is within 4.0 units of W_k, not waiting for step-up elevation, and has a subsequent waypoint (k + 1 < size).
	// Steer forward toward the subsequent waypoint W_{k+1} so the agent smoothly rounds the point without orbiting W_k.
	else if ( !awaitingStepUp && k + 1 < stringPulledPath.size() ) {
		Vector3DP toNext = stringPulledPath[ k + 1 ] - myFeetDP;
		toNext.z = 0.0;
		const double nextDist = QM_Vector3LengthDP( toNext );
		*outMoveDir = ( nextDist > 0.001 ) ? ( toNext * ( 1.0 / nextDist ) ) : forwardYawDir;
	}
	// Case 3: Inside deadband at a step-up riser, actively awaiting vertical elevation.
	// Condition check: Entity is at the riser boundary (steerDist < 4.0) but feet have not yet stepped up onto the tread.
	// Rather than turning toward W_{k+1} (which would glance off the riser or turn sideways into walls),
	// drive forward across the riser along the incoming segment direction (k > 0) so StepSlideMove can step up.
	else if ( awaitingStepUp && k > 0 ) {
		Vector3DP segFwd = targetWp - stringPulledPath[ k - 1 ];
		segFwd.z = 0.0;
		const double segFwdLen = QM_Vector3LengthDP( segFwd );
		*outMoveDir = ( segFwdLen > 0.001 ) ? ( segFwd * ( 1.0 / segFwdLen ) ) : forwardYawDir;
	}
	// Case 4: Final destination reached, or first waypoint awaiting step-up without prior history.
	// Condition check: No subsequent waypoint exists (k + 1 >= size); the entity is within deadband of its final arrival point.
	// Preserve the current facing direction (forwardYawDir) to prevent 360-degree jitter across the final destination.
	else {
		*outMoveDir = forwardYawDir;
	}



	/**
	*	Smooth corner deceleration when turning sharply towards the lookahead target.
	**/
	const double steerYaw = QM_AngleMod( QM_Vector3ToYawDP( *outMoveDir ) );
	const double currentYaw = QM_AngleMod( static_cast<double>( currentAngles[ YAW ] ) );
	const double yawDeltaAbs = std::fabs( QM_AngleDelta( steerYaw, currentYaw ) );

	double speedScale = 1.0;
	if ( yawDeltaAbs > MONSTER_NAV_CORNER_DECEL_THRESHOLD_DEG ) {
		speedScale = std::max( MONSTER_NAV_CORNER_DECEL_MIN_SPEED_SCALE, ( 180.0 - yawDeltaAbs ) / ( 180.0 - MONSTER_NAV_CORNER_DECEL_THRESHOLD_DEG ) );
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

	ideal_yaw = static_cast<float>( QM_Vector3ToYawDP( moveDirDP ) );
	SVG_MMove_FaceIdealYaw( this, ideal_yaw, 45.0f );

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
