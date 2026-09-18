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
#include "svgame/nav/nav_generate.h"

// Monster move and slide move.
#include "svgame/monsters/svg_mmove.h"
#include "svgame/monsters/svg_mmove_slidemove.h"

// Crowd and squad coordination.
#include "svgame/crowd/svg_crowd_manager.h"
#include "svgame/crowd/svg_squad_coordinator.h"
#include "svgame/nav/nav_sector_graph.h"


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
	suppressEntityDeflectionThisFrame = false;
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

	/**
	*	Enforce the finite serialized doorway aperture after collision resolution.
	*	SlideMove may rotate velocity, so test the exact stop-plane intersection and
	*	block only crossings whose lateral coordinate lies inside the reserved opening.
	**/
	const svg_crowd_group_t *ingressGroup = ( crowd.crowdID >= 0 ) ? SVG_Crowd_GetGroup( crowd.crowdID ) : nullptr;
	if ( ingressGroup != nullptr && ingressGroup->hasSerializedIngress && !crowd.ingressReleased ) {
		const Vector3DP previousOrigin( currentOrigin );
		const double previousDepth = QM_Vector3DotProductDP( previousOrigin - ingressGroup->ingressPortalOrigin, ingressGroup->ingressPortalInward );
		const double stopDepth = -ingressGroup->ingressReleaseDepth;
		const double resolvedDepth = QM_Vector3DotProductDP( monsterMove.state.origin - ingressGroup->ingressPortalOrigin, ingressGroup->ingressPortalInward );
		const Vector3DP portalTangent{ ingressGroup->ingressPortalInward.y, -ingressGroup->ingressPortalInward.x, 0.0 };
		const double reservedHalfSpan = ingressGroup->ingressPortalHalfWidth + ingressGroup->ingressReleaseDepth;
		bool crossesReservedAperture = false;

		// Interpolate lateral position exactly where this frame crosses the exterior stop plane.
		if ( previousDepth <= stopDepth && resolvedDepth > stopDepth ) {
			const double crossingFraction = std::clamp(
				( stopDepth - previousDepth ) / ( resolvedDepth - previousDepth ), 0.0, 1.0 );
			const double previousLateral = QM_Vector3DotProductDP(
				previousOrigin - ingressGroup->ingressPortalOrigin, portalTangent );
			const double resolvedLateral = QM_Vector3DotProductDP(
				monsterMove.state.origin - ingressGroup->ingressPortalOrigin, portalTangent );
			const double crossingLateral = previousLateral +
				( ( resolvedLateral - previousLateral ) * crossingFraction );
			crossesReservedAperture = std::fabs( crossingLateral ) < reservedHalfSpan;
		}

		// Project only a true aperture crossing; side-wall marshalling beyond the plane remains legal.
		if ( crossesReservedAperture ) {
			monsterMove.state.origin = monsterMove.state.origin -
				( ingressGroup->ingressPortalInward * ( resolvedDepth - stopDepth ) );
			const double resolvedInwardSpeed = QM_Vector3DotProductDP( monsterMove.state.velocity, ingressGroup->ingressPortalInward );
			if ( resolvedInwardSpeed > 0.0 ) {
				monsterMove.state.velocity = monsterMove.state.velocity -
					( ingressGroup->ingressPortalInward * resolvedInwardSpeed );
			}
		}
	}

	velocity = QM_Vector3FromDP( monsterMove.state.velocity );
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

	/**
	*	Clone the cached nav policy so queueing can temporarily disable entity deflection
	*	without mutating the persistent A* policy used by future path queries.
	**/
	nav_path_policy_t slidePolicy = pathNavigationState.policy;
	slidePolicy.allow_entity_deflection = !suppressEntityDeflectionThisFrame;
	monsterMove.navPolicy = &slidePolicy;

	const int32_t blockedMask = SVG_MMove_StepSlideMove( &monsterMove, slidePolicy );
	monsterMove.navPolicy = &pathNavigationState.policy;
	suppressEntityDeflectionThisFrame = false;
	UpdateBlockedNavigationRecovery( blockedMask);
	return blockedMask;
}

/**
*	@brief	Refresh ground support using the same analytical hull as movement.
*	@note	An AABB support test can start inside a slope underneath a valid capsule,
*			leaving stale airborne state and disabling navigation steering indefinitely.
**/
const void svg_monster_base_t::RecategorizeGroundAndLiquidState() {
	/**
	*	Flying/swimming actors retain their existing ground policy; grounded movers
	*	probe a quarter unit downward with their native hull, without relocating them.
	**/
	if ( !( flags & ( FL_SWIM | FL_FLY ) ) ) {
		groundInfo.entityNumber = ENTITYNUM_NONE;
		// Fast upward motion represents a jump, not a supported walking contact.
		if ( velocity.z <= 100.0f ) {
			const Vector3DP start( currentOrigin );
			const svg_trace_t support = SVG_MMove_Trace( start, mins, maxs,
				start - Vector3DP{ 0.0, 0.0, 0.25 }, this, SVG_GetClipMask( this ), SVG_MMove_GetNativeShape( this ) );
			// Accept only a real walkable contact; a miss must clear stale ground state.
			if ( !support.startsolid && !support.allsolid && support.fraction < 1.0f &&
				 support.plane.normal[ 2 ] >= NAV_MIN_WALKABLE_Z ) {
				groundInfo.entityNumber = support.entityNumber;
				groundInfo.entityLinkCount = support.ent != nullptr ? support.ent->linkCount : 0;
				groundInfo.material = support.material;
				groundInfo.contents = support.contents;
				// Some analytical contacts have no surface descriptor.
				if ( support.surface != nullptr ) {
					groundInfo.surface = *support.surface;
				}
				// Slope clipping from the previous frame is not an intentional jump impulse.
				velocity.z = 0.0f;
			}
		}
	}
	/**
	*	Liquid classification remains independent of the collision primitive.
	**/
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
	const Vector3DP myFeetDP = SVG_GetEntityFeetOriginDP( this );

	Vector3DP targetFeetDP = Vector3DP( target );
	if ( this->goalentity != nullptr && QM_Vector3DistanceSqr( target, this->goalentity->currentOrigin ) < ( 8.0f * 8.0f ) ) {
		targetFeetDP = SVG_GetEntityFeetOriginDP( this->goalentity );
	}

	/**
	*	Dynamically derive physical agent dimensions from entity bounding box with canonical fallbacks:
	*	PHYS_DEFAULT_BBOX_STANDUP_MINS = { -16., -16., -36. }, PHYS_DEFAULT_BBOX_STANDUP_MAXS = { 16., 16., 36. },
	*	PHYS_DEFAULT_VIEWHEIGHT_STANDUP = 30.
	**/
	const double rawRadiusX = std::max( std::abs( static_cast<double>( this->mins.x ) ), std::abs( static_cast<double>( this->maxs.x ) ) );
	const double rawRadiusY = std::max( std::abs( static_cast<double>( this->mins.y ) ), std::abs( static_cast<double>( this->maxs.y ) ) );
	const double rawRadius = std::max( rawRadiusX, rawRadiusY );
	constexpr double defaultRadius = NAV_DEFAULT_AGENT_RADIUS; // max(|PHYS_DEFAULT_BBOX_STANDUP_MINS.x|, |PHYS_DEFAULT_BBOX_STANDUP_MAXS.x|)
	const double agentRadius = ( rawRadius > 0.0 ) ? rawRadius : defaultRadius;
	const double agentRadiusSqr = agentRadius * agentRadius;

	const double rawHeight = static_cast<double>( this->maxs.z - this->mins.z );
	constexpr double defaultHeight = 72.0; // PHYS_DEFAULT_BBOX_STANDUP_MAXS.z - PHYS_DEFAULT_BBOX_STANDUP_MINS.z
	const double agentHeight = ( rawHeight > 0.0 ) ? rawHeight : defaultHeight;

	const double effectiveViewHeight = ( this->viewheight > 0.0f ) ? static_cast<double>( this->viewheight ) : PHYS_DEFAULT_VIEWHEIGHT_STANDUP;

	const bool targetMoved = ( QM_Vector3DistanceSqr( target, pathNavigationState.lastGoal.origin ) > ( 48.0f * 48.0f ) );
	const bool pathEmpty = navPath.empty() || stringPulledPath.empty();

	// 0) Fast O(1) topological face lookup: check if entity remains on its active path face or direct neighbors
	int32_t startFace = -1;
	if ( !navPath.empty() && pathPos < navPath.size() ) {
		const int32_t candIdx = navPath[ pathPos ];
		if ( candIdx >= 0 && static_cast<size_t>( candIdx ) < g_nav_faces.size() ) {
			const nav_face_t &candFace = g_nav_faces[ candIdx ];
			if ( Nav_PointInsideFace2D( myFeetDP, candFace ) ) {
				startFace = candIdx;
			} else {
				for ( int32_t e = 0; e < candFace.num_edges; ++e ) {
					const nav_halfedge_t &he = g_nav_halfedges[ candFace.first_edge_idx + e ];
					if ( he.twin_idx != -1 ) {
						const int32_t nbrIdx = g_nav_halfedges[ he.twin_idx ].face_idx;
						if ( nbrIdx >= 0 && static_cast<size_t>( nbrIdx ) < g_nav_faces.size() ) {
							if ( Nav_PointInsideFace2D( myFeetDP, g_nav_faces[ nbrIdx ] ) ) {
								startFace = nbrIdx;
								break;
							}
						}
					}
				}
			}
		}
	}
	/**
	*	Resolve the destination face conservatively before any path search.
	*	Strict containment prevents a staging cell near a seam, doorway, or slope
	*	from snapping to a different room component and making the monster appear
	*	idle at spawn.
	**/
	int32_t goalFace = -1;
	if ( !targetMoved && !navPath.empty() ) {
		goalFace = navPath.back();
	} else {
		goalFace = Nav_FindFaceInLeafStrict( targetFeetDP );
		if ( goalFace < 0 ) {
			Vector3DP loweredTargetFeetDP = targetFeetDP;
			loweredTargetFeetDP.z -= NAV_STANDOFF_FEET_SNAP_OFFSET_Z;
			goalFace = Nav_FindFaceInLeafStrict( loweredTargetFeetDP );
		}
		if ( goalFace < 0 ) {
			goalFace = Nav_FindClosestFaceInLeaf( targetFeetDP );
		}
	}

	if ( startFace == -1 ) {
		startFace = Nav_FindReachableFaceInLeaf( myFeetDP, goalFace, agentRadius );
	} else if ( goalFace >= 0 && Nav_GetFaceComponent( startFace ) != Nav_GetFaceComponent( goalFace ) ) {
		startFace = Nav_FindReachableFaceInLeaf( myFeetDP, goalFace, agentRadius );
	}

	/**
	*	If the recovered start face and tentative goal still disagree by connected
	*	component, search the goal leaf for a face reachable from the start. This is
	*	the inverse of the start-face repair above and fixes near-wall staging points
	*	that initially resolve onto the wrong side of a doorway seam.
	**/
	if ( startFace != -1 &&
		 goalFace != -1 &&
		 Nav_GetFaceComponent( startFace ) != Nav_GetFaceComponent( goalFace ) ) {
		const int32_t reachableGoalFace = Nav_FindReachableFaceInLeaf( targetFeetDP, startFace, agentRadius );
		if ( reachableGoalFace != -1 ) {
			goalFace = reachableGoalFace;
		}
	}

	if ( startFace == -1 || goalFace == -1 ) {
		return PathComputeResult::Failed;
	}

	/**
	*	Check if entity remains on or physically intersecting the active navigation path corridor.
	*	Adheres strictly to O(1) time complexity by testing bounded local windows and active segment geometry.
	**/
	bool stillOnPath = false;

	// 1) Fast O(1) polyline segment proximity: test distance from agent feet to the active string-pulled segments.
	// Evaluates the active waypoint W_k, the incoming segment [prevIdx, k], and the outgoing segment [k, k + 1].
	if ( !stringPulledPath.empty() ) {
		const size_t k = std::min( stringPathPos, stringPulledPath.size() - 1 );
		const size_t prevIdx = ( k > 0 ) ? ( k - 1 ) : 0;
		const Vector3DP &wpCurr = stringPulledPath[ k ];

		const double corridorLateralDist = agentRadius * 2.0 + MONSTER_NAV_CORRIDOR_MARGIN;
		const double corridorLateralDistSqr = corridorLateralDist * corridorLateralDist;
		const nav_zone_type_t currentZone = Nav_GetZoneTypeForPoint( myFeetDP );
		const double maxVerticalTolerance = ( currentZone == ZONE_TYPE_CORRIDOR_STAIRS )
			? ( static_cast<double>( NAV_MAX_STEP_HEIGHT ) * 1.5 + MONSTER_NAV_VERTICAL_STEP_TOLERANCE )
			: ( static_cast<double>( NAV_MAX_STEP_HEIGHT ) + MONSTER_NAV_VERTICAL_STEP_TOLERANCE );

		// Distance to active waypoint W_k:
		const double dxWp = myFeetDP.x - wpCurr.x;
		const double dyWp = myFeetDP.y - wpCurr.y;
		const double distToWpSqr = ( dxWp * dxWp ) + ( dyWp * dyWp );
		const double zDeltaWp = std::fabs( myFeetDP.z - wpCurr.z );
		if ( distToWpSqr <= corridorLateralDistSqr && zDeltaWp <= maxVerticalTolerance ) {
			stillOnPath = true;
		}

		// Distance to incoming segment [prevIdx, k]:
		if ( !stillOnPath && k > 0 ) {
			const Vector3DP &wpPrev = stringPulledPath[ prevIdx ];
			const double distToSegSqr = Nav_DistancePointToSegment2DSqr( myFeetDP, wpPrev, wpCurr );
			const double zDeltaSeg = std::fabs( myFeetDP.z - wpCurr.z );
			if ( distToSegSqr <= corridorLateralDistSqr && zDeltaSeg <= maxVerticalTolerance ) {
				stillOnPath = true;
			}
		}

		// Distance to outgoing segment [k, k + 1]:
		if ( !stillOnPath && k + 1 < stringPulledPath.size() ) {
			const Vector3DP &wpNext = stringPulledPath[ k + 1 ];
			const double distToNextSegSqr = Nav_DistancePointToSegment2DSqr( myFeetDP, wpCurr, wpNext );
			const double zDeltaNextSeg = std::fabs( myFeetDP.z - wpNext.z );
			if ( distToNextSegSqr <= corridorLateralDistSqr && zDeltaNextSeg <= maxVerticalTolerance ) {
				stillOnPath = true;
			}
		}
	}

	// 2) Fast O(1) topological corridor face check (fallback if segment proximity misses during wide turns):
	if ( !stillOnPath && !navPath.empty() && pathPos < navPath.size() ) {
		const int32_t currentFace = startFace;
		const int32_t startCheck = std::max<int32_t>( 0, static_cast<int32_t>( pathPos ) - 2 );
		const int32_t endCheck = std::min<int32_t>( static_cast<int32_t>( navPath.size() ) - 1, static_cast<int32_t>( pathPos ) + 8 );

		// Direct polygon containment or 2D capsule disk intersection with corridor faces
		const Vector3DP &feetPosDP = myFeetDP;
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

		// Topological neighbor tolerance: check if currentFace shares a boundary half-edge with any corridor face
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

	/**
	*	Anti-Chattering Path Commitment Hysteresis & Failure Cooldown:
	*	If the destination has not moved, and the agent is still physically on its active path corridor,
	*	reuse the cached path corridor indefinitely! Only recalculate if off-path, target moved, or path is empty.
	**/
	if ( !force && !targetMoved && stillOnPath && !pathEmpty ) {
		return PathComputeResult::ReusedCached;
	}

	const QMTime requiredCooldown = pathEmpty ? MONSTER_NAV_PATH_FAIL_RECALC_INTERVAL : MONSTER_NAV_PATH_RECALC_MIN_INTERVAL;
	const bool cooldownElapsed = ( ( level.time - lastPathCalcTime ) >= requiredCooldown );

	// When a recalculation is actually required, enforce minimum cooldown interval to prevent high-frequency A* thrashing:
	if ( !force && !targetMoved && !cooldownElapsed ) {
		return pathEmpty ? PathComputeResult::Failed : PathComputeResult::ReusedCached;
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
	pathPolicy.edge_cost_callback = &svg_monster_base_t::NavEdgeCostCallbackBridge;
	pathPolicy.edge_cost_monster = this;

	bool pathFound = Nav_FindPath( startFace, goalFace, navPath, pathPolicy );
	if ( !pathFound ) {
		const int32_t altStartFace = Nav_FindReachableFaceInLeaf( myFeetDP, goalFace, agentRadius );
		if ( altStartFace != -1 && altStartFace != startFace ) {
			startFace = altStartFace;
			pathFound = Nav_FindPath( startFace, goalFace, navPath, pathPolicy );
		}
	}

	if ( pathFound ) {
		pathNavigationState.lastGoal.origin = target;
		pathNavigationState.lastGoal.isValid = true;
		lastPathCalcTime = level.time;

		Nav_StringPull( navPath, myFeetDP, targetFeetDP, agentRadius, stringPulledPath, &stringPulledWaypointForced, this->mins, this->maxs, static_cast<int32_t>( SVG_MMove_GetNativeShape( this ) ) );

		if ( stringPulledPath.size() >= 3 ) {
			const double dx01 = myFeetDP.x - stringPulledPath[ 1 ].x;
			const double dy01 = myFeetDP.y - stringPulledPath[ 1 ].y;
			const double dist01Sqr = ( dx01 * dx01 ) + ( dy01 * dy01 );
			// If W_1 is already within the entity's physical capsule footprint (agentRadius),
			// immediately advance to W_2 so the agent continues forward momentum without turning to chase its feet:
			if ( dist01Sqr <= agentRadiusSqr ) {
				stringPathPos = 2;
			} else {
				stringPathPos = 1;
			}
		} else if ( stringPulledPath.size() >= 2 ) {
			stringPathPos = 1;
		} else {
			stringPathPos = 0;
		}
		return PathComputeResult::NewPathGenerated;
	}

	// Record failed search state and timestamp to prevent high-frequency A* thrashing:
	pathNavigationState.lastGoal.origin = target;
	pathNavigationState.lastGoal.isValid = false;
	lastPathCalcTime = level.time;
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

	const Vector3DP myFeetDP = SVG_GetEntityFeetOriginDP( this );
	// Serialized ingress must physically follow mandatory corner points, not blend across the doorway jamb.
	const svg_crowd_group_t *pathCrowd = crowd.crowdID >= 0 ? SVG_Crowd_GetGroup( crowd.crowdID ) : nullptr;
	const bool serializedIngressPath = pathCrowd != nullptr && pathCrowd->hasSerializedIngress && crowd.ingressReleased;

	/**
	*	Direct goal fallback when no string-pulled path polyline is available.
	*	Validates unobstructed physical line-of-sight before moving directly towards destination.
	**/
	if ( stringPulledPath.empty() ) {
		Vector3DP toGoal = finalGoal - myFeetDP;
		const double verticalDist = std::fabs( toGoal.z );
		toGoal.z = 0.0;
		const double dist2D = QM_Vector3LengthDP( toGoal );
		const double fallbackReachRadius = ( this->maxs.x > 0.0f ) ? static_cast<double>( this->maxs.x ) : NAV_DEFAULT_AGENT_RADIUS;
		if ( dist2D <= fallbackReachRadius && verticalDist <= MONSTER_NAV_FINAL_GOAL_MAX_Z_DELTA ) {
			return false;
		}

		// Prevent blind wall walking: only move directly if there is unobstructed entity swept line-of-sight through world geometry
		const Vector3DP startTrace = Vector3DP( currentOrigin );
		Vector3DP endTrace = finalGoal;
		endTrace.z -= static_cast<double>( this->mins.z );
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
	const int32_t currentFace = Nav_FindClosestFaceInLeaf( myFeetDP );
	if ( !navPath.empty() && currentFace != -1 ) {
		const int32_t localStart = std::max<int32_t>( 0, static_cast<int32_t>( pathPos ) - 2 );
		const int32_t localEnd = std::min<int32_t>( static_cast<int32_t>( navPath.size() ) - 1, static_cast<int32_t>( pathPos ) + 8 );
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

	// Determine agent bounding box horizontal radius for clearance and arrival thresholds:
	const double rawRadiusX = std::max( std::abs( static_cast<double>( this->mins.x ) ), std::abs( static_cast<double>( this->maxs.x ) ) );
	const double rawRadiusY = std::max( std::abs( static_cast<double>( this->mins.y ) ), std::abs( static_cast<double>( this->maxs.y ) ) );
	const double rawRadius = std::max( rawRadiusX, rawRadiusY );
	constexpr double defaultRadius = NAV_DEFAULT_AGENT_RADIUS;
	const double agentRadius = ( rawRadius > 0.0 ) ? rawRadius : defaultRadius;

	/**
	*	Waypoint arrival & segment advancement:
	*	Advances stringPathPos as each intermediate waypoint is reached or passed along its incoming segment.
	*	Bounded to at most 2 waypoint advancements per frame (O(1)) to prevent 1-frame position lag on closely-spaced arc points.
	**/
	int32_t advancedWaypointsCount = 0;
	while ( stringPathPos < stringPulledPath.size() - 1 && advancedWaypointsCount < 2 ) {
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
		bool isSharpTurn = false;
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
		} else if ( isForcedWp ) {
			isSharpTurn = true;
		}

		// Compute longitudinal distance along incoming segment past W_k:
		double forwardPast = 0.0;
		if ( stringPathPos > 0 ) {
			const Vector3DP prevWp = stringPulledPath[ stringPathPos - 1 ];
			Vector3DP inSegDir = currentWp - prevWp;
			inSegDir.z = 0.0;
			const double inSegLen = QM_Vector3LengthDP( inSegDir );
			if ( inSegLen > 0.001 ) {
				inSegDir = inSegDir * ( 1.0 / inSegLen );
				Vector3DP fromTarget = myFeetDP - currentWp;
				fromTarget.z = 0.0;
				forwardPast = QM_Vector3DotProductDP( fromTarget, inSegDir );
			}
		}

		// 1) Arrival radius around intermediate waypoint:
		// On sharp turns and tight corner standoffs, ensure the arrival radius is at least large enough
		// to accommodate the entity's physical collision hull radius (plus clearance).
		const double minHullReach = ( agentRadius > 0.0 ) ? ( agentRadius + MONSTER_NAV_CORNER_HULL_CLEARANCE_MARGIN ) : ( NAV_DEFAULT_AGENT_RADIUS + MONSTER_NAV_CORNER_HULL_CLEARANCE_MARGIN );
		const double sharpReach = std::max( MONSTER_NAV_SHARP_CORNER_REACH_RADIUS, minHullReach );
		const double reachRadius = serializedIngressPath && isForcedWp ? CROWD_INGRESS_ARRIVAL_RADIUS :
			( isSharpTurn ? sharpReach : MONSTER_NAV_WAYPOINT_REACH_RADIUS );
		const double reachRadiusSqr = reachRadius * reachRadius;
		bool withinRadius = ( dist2DSqr <= reachRadiusSqr );

		// Corner switching plane gating:
		// At a sharp turn around an obstacle corner standoff (isSharpTurn), an agent must NOT advance prematurely
		// while still on the approach side of the corner (forwardPast < 0.0) unless it already has guaranteed
		// physical clearance (agentRadius) to the subsequent waypoint W_{k+1}.
		// Advancing prematurely on the approach side forces the agent to turn into the solid corner brush!
		// Note: Forced stair step and doorway waypoints must NOT be gated here, as stair flight boundary edges
		// naturally lie within agentRadius of the step center and would falsely block stair progression.
		if ( withinRadius && isSharpTurn && !isForcedWp && stringPathPos + 1 < stringPulledPath.size() ) {
			const Vector3DP &nextWp = stringPulledPath[ stringPathPos + 1 ];
			Vector3DP toNextWp = nextWp - myFeetDP;
			toNextWp.z = 0.0;
			const double segDistToNext = QM_Vector3LengthDP( toNextWp );
			const double baseClearance = ( agentRadius > 0.0 ) ? agentRadius : NAV_DEFAULT_AGENT_RADIUS;
			const double proportionalThreshold = baseClearance * MONSTER_NAV_ARC_CLEARANCE_DIST_MULT;
			const double losClearance = ( segDistToNext < proportionalThreshold )
				? std::min( baseClearance, segDistToNext * MONSTER_NAV_ARC_CLEARANCE_RATIO )
				: baseClearance;
			if ( forwardPast < 0.0 && !Nav_HasGeometricLineOfSight2D( myFeetDP, nextWp, losClearance ) ) {
				withinRadius = false;
			}
		}

		// 2) Passed waypoint plane along the incoming segment:
		// Allows smooth progression along straight corridors and across doorway portals without stalling.
		bool passedPlane = false;
		if ( stringPathPos > 0 && ( onRamp || std::fabs( zDiff ) <= MONSTER_NAV_STEP_MIN_DELTA ) ) {
			const Vector3DP prevWp = stringPulledPath[ stringPathPos - 1 ];
			Vector3DP inSegDir = currentWp - prevWp;
			inSegDir.z = 0.0;
			const double inSegLen = QM_Vector3LengthDP( inSegDir );

			if ( inSegLen > 0.001 ) {
				inSegDir = inSegDir * ( 1.0 / inSegLen );
				Vector3DP fromTarget = myFeetDP - currentWp;
				fromTarget.z = 0.0;
				const double lateralDistSqr = QM_Vector3DotProductDP( fromTarget, fromTarget ) - ( forwardPast * forwardPast );

				// For unforced gentle turns: standard corridor bounds along travel segment
				if ( !isSharpTurn && !isForcedWp ) {
					if ( forwardPast >= 0.0 && lateralDistSqr <= MONSTER_NAV_SHARP_CORNER_REACH_RADIUS_SQR ) {
						passedPlane = true;
					}
				}
				// For sharp corner turns: allow advancing if forward past the corner plane and within corner reach bounds
				else if ( isSharpTurn ) {
					if ( forwardPast >= 0.0 && dist2DSqr <= ( sharpReach * sharpReach * 2.25 ) ) {
						passedPlane = true;
					}
				}
				// For forced doorway/corridor portals on flat ground: allow advancing if forward past the threshold plane
				// and within traversable doorway clearance, preventing chokepoint deadlocks.
				else if ( isForcedWp ) {
					if ( forwardPast >= 0.0 && lateralDistSqr <= MONSTER_NAV_DOORWAY_PLANE_ADVANCE_MAX_LATERAL_SQR ) {
						passedPlane = true;
					}
				}
			}
		}

		// 3) Corner clearance advancement:
		// If the entity has reached or passed the corner switching plane (forwardPast >= 0.0 or within margin),
		// is within proximate clearance of W_k, not blocked against a wall, and the subsequent
		// waypoint W_{k+1} has guaranteed physical capsule clearance (agentRadius) around the corner,
		// allow advancing past W_k to round the corner cleanly without wedging on the physical corner brush.
		// Note: On sharp turns (isSharpTurn), an entity must NOT advance while still on the approach side
		// (forwardPast < -MONSTER_NAV_CORNER_HULL_CLEARANCE_MARGIN), as premature advancement forces the agent
		// to turn into the solid corner brush before reaching the standoff waypoint.
		if ( !withinRadius && !passedPlane && !hasRecentWallBlockNormal && stringPathPos + 1 < stringPulledPath.size() ) {
			const bool isNearCornerWp = ( dist2DSqr <= MONSTER_NAV_CORNER_BRUSH_CLEARANCE_DIST_SQR );
			const bool hasReachedPlane = ( !isSharpTurn || forwardPast >= -MONSTER_NAV_CORNER_HULL_CLEARANCE_MARGIN );
			const Vector3DP &nextWp = stringPulledPath[ stringPathPos + 1 ];
			Vector3DP toNextWp = nextWp - myFeetDP;
			toNextWp.z = 0.0;
			const double segDistToNext = QM_Vector3LengthDP( toNextWp );
			const double baseClearance = ( agentRadius > 0.0 ) ? agentRadius : NAV_DEFAULT_AGENT_RADIUS;
			const double proportionalThreshold = baseClearance * MONSTER_NAV_ARC_CLEARANCE_DIST_MULT;
			const double losClearance = ( segDistToNext < proportionalThreshold )
				? std::min( baseClearance, segDistToNext * MONSTER_NAV_ARC_CLEARANCE_RATIO )
				: baseClearance;
			if ( isNearCornerWp && hasReachedPlane && Nav_HasGeometricLineOfSight2D( myFeetDP, nextWp, losClearance ) ) {
				passedPlane = true;
			}
		}

		if ( withinRadius ) {
			const size_t reachedIdx = stringPathPos;
			const Vector3DP reachedPos = stringPulledPath[ reachedIdx ];
			++stringPathPos;
			++advancedWaypointsCount;
			this->OnWaypointReached( reachedIdx, reachedPos, false );
			continue; // Re-evaluate next waypoint in bounded loop to prevent 1-frame position lag
		}

		if ( passedPlane ) {
			// When passing by W_k along a straight flat section or rounding a corner, verify both:
			// 1. Exact 2D geometric line-of-sight through all obstacle edges with full agentRadius collision hull clearance
			// 2. Physical kinematic step probe over curbs, stairs, and slopes using SVG_MMove_Probe
			if ( stringPathPos + 1 < stringPulledPath.size() ) {
				const Vector3DP &nextWp = stringPulledPath[ stringPathPos + 1 ];
				Vector3DP toNextWp = nextWp - myFeetDP;
				toNextWp.z = 0.0;
				const double segDistToNext = QM_Vector3LengthDP( toNextWp );
				const double baseClearance = ( agentRadius > 0.0 ) ? agentRadius : NAV_DEFAULT_AGENT_RADIUS;
				const double proportionalThreshold = baseClearance * MONSTER_NAV_ARC_CLEARANCE_DIST_MULT;
				const double losClearance = ( segDistToNext < proportionalThreshold )
					? std::min( baseClearance, segDistToNext * MONSTER_NAV_ARC_CLEARANCE_RATIO )
					: baseClearance;
				if ( !Nav_HasGeometricLineOfSight2D( myFeetDP, nextWp, losClearance ) ) {
					// Direct line to W_{k+1} is occluded by an obstacle edge; continue steering towards W_k until withinRadius
					break;
				}

				Vector3DP probeGround = {};
				Vector3DP nextOrigin = nextWp;
				nextOrigin.z -= static_cast<double>( this->mins.z );
				const double maxStep = ( this->pathNavigationState.policy.max_step_height > 0.0 ) ? this->pathNavigationState.policy.max_step_height : NAV_PROBE_DEFAULT_MAX_STEP_HEIGHT;
				const double maxDrop = ( this->pathNavigationState.policy.max_drop_height > 0.0 ) ? this->pathNavigationState.policy.max_drop_height : NAV_PROBE_DEFAULT_MAX_DROP_HEIGHT;
				if ( !SVG_MMove_Probe( Vector3DP( currentOrigin ), mins, maxs, nextOrigin, this, &probeGround, maxStep, maxDrop ) ) {
					// Physical step/slope probe blocked; continue steering towards W_k until withinRadius
					break;
				}

				// Ensure probe actually completed traversable progress to nextWp (not blocked after only 15% into a ramp/wall)
				Vector3DP toProbeDest = nextOrigin - probeGround;
				toProbeDest.z = 0.0;
				if ( ( toProbeDest.x * toProbeDest.x + toProbeDest.y * toProbeDest.y ) > ( reachRadiusSqr * NAV_PROBE_ARRIVAL_TOLERANCE_RATIO_SQR ) ) {
					// Incomplete probe progress to next waypoint; continue steering to current waypoint
					break;
				}
			}

			const size_t reachedIdx = stringPathPos;
			const Vector3DP reachedPos = stringPulledPath[ reachedIdx ];
			++stringPathPos;
			++advancedWaypointsCount;
			this->OnWaypointReached( reachedIdx, reachedPos, false );
			continue; // Re-evaluate next waypoint in bounded loop to prevent 1-frame position lag
		}

		break;
	}

	// Final goal arrival check
	if ( stringPathPos >= stringPulledPath.size() - 1 ) {
		const Vector3DP finalGoalWp = stringPulledPath.back();
		Vector3DP toFinal = finalGoalWp - myFeetDP;
		const double finalZDiff = toFinal.z;
		toFinal.z = 0.0;
		// The low-level path follower must not stop outside the crowd's tighter
		// staging tolerance, or station keeping and doorway admission can never agree.
		const svg_crowd_group_t *arrivalGroup = crowd.crowdID >= 0 ? SVG_Crowd_GetGroup( crowd.crowdID ) : nullptr;
		const double finalArrivalRadius = arrivalGroup != nullptr && arrivalGroup->hasSerializedIngress ?
			CROWD_INGRESS_ARRIVAL_RADIUS : MONSTER_NAV_WAYPOINT_REACH_RADIUS;
		if ( QM_Vector3DotProductDP( toFinal, toFinal ) <= finalArrivalRadius * finalArrivalRadius && std::fabs( finalZDiff ) <= MONSTER_NAV_FINAL_GOAL_MAX_Z_DELTA ) {
			this->OnWaypointReached( stringPulledPath.size() - 1, finalGoalWp, true );
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

			// Blend forward across gentle turns and intermediate arc waypoints (~45 deg) on flat ground or ramps:
			if ( turnDot > NAV_CORNER_ARC_MIN_TURN_DOT && ( onRamp || std::fabs( nextWp.z - targetWp.z ) <= MONSTER_NAV_STEP_MIN_DELTA ) ) {
				const double advance = std::min( MONSTER_NAV_LOOKAHEAD_DISTANCE - distToTarget, nextSegLen * NAV_STANDOFF_SCALE_HALF );
				const Vector3DP blendedTarget = targetWp + nextSegNorm * advance;
				// Maintain clear line-of-sight to blended target before cutting forward early:
				if ( Nav_HasGeometricLineOfSight2D( myFeetDP, blendedTarget, agentRadius ) ) {
					steerTarget = blendedTarget;
				}
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

	// Check whether entity has progressed into the departure zone of W_k toward W_{k+1}:
	bool inDepartureZone = false;
	if ( !awaitingStepUp && !isTargetForced && k + 1 < stringPulledPath.size() ) {
		const Vector3DP nextWp = stringPulledPath[ k + 1 ];
		Vector3DP toNext = nextWp - targetWp;
		toNext.z = 0.0;
		const double toNextLen = QM_Vector3LengthDP( toNext );

		if ( k > 0 && toNextLen > 0.001 ) {
			const Vector3DP prevWp = stringPulledPath[ k - 1 ];
			Vector3DP inSegDir = targetWp - prevWp;
			inSegDir.z = 0.0;
			const double inSegLen = QM_Vector3LengthDP( inSegDir );

			if ( inSegLen > 0.001 ) {
				const Vector3DP uIn = inSegDir * ( 1.0 / inSegLen );
				const Vector3DP uOut = toNext * ( 1.0 / toNextLen );

				// Radial corner switching plane between incoming and outgoing segments:
				Vector3DP switchNorm = uIn + uOut;
				switchNorm.z = 0.0;
				const double switchLen = QM_Vector3LengthDP( switchNorm );
				if ( switchLen > 0.001 ) {
					switchNorm = switchNorm * ( 1.0 / switchLen );
					Vector3DP fromTarget = myFeetDP - targetWp;
					fromTarget.z = 0.0;
					// Entity has physically crossed past the switching plane into the departure zone:
					if ( QM_Vector3DotProductDP( fromTarget, switchNorm ) >= 0.0 ) {
						inDepartureZone = true;
					}
				}
			}
		}

		// Proximity fallback: if entity is closer to W_{k+1} than W_k and within arrival reach:
		if ( !inDepartureZone && toNextLen > 0.001 ) {
			Vector3DP toNextFromFeet = nextWp - myFeetDP;
			toNextFromFeet.z = 0.0;
			const double distToNext = QM_Vector3LengthDP( toNextFromFeet );
			if ( distToNext < distToTarget && distToTarget <= ( MONSTER_NAV_WAYPOINT_REACH_RADIUS * 2.0 ) ) {
				inDepartureZone = true;
			}
		}

		// Geometric line-of-sight validation: when candidate departure zone is active near an obstacle corner,
		// ensure entity has unobstructed 2D line-of-sight clearance to nextWp with full agent collision radius.
		// If line-of-sight is obstructed by a corner brush, do not cut early into the corner; continue steering
		// to current waypoint W_k (which is the standoff arc point) until the corner is physically cleared:
		if ( inDepartureZone ) {
			if ( !Nav_HasGeometricLineOfSight2D( myFeetDP, nextWp, agentRadius * MONSTER_NAV_DEPARTURE_CLEARANCE_SCALE ) ) {
				inDepartureZone = false;
			}
		}
	}

	// Case 1: If entity has progressed into the departure zone of W_k and there is a subsequent
	// waypoint (k + 1 < size), steer forward toward W_{k+1} rather than steering backward toward W_k
	// (which would trigger a 180-degree yaw flip).
	if ( inDepartureZone ) {
		Vector3DP toNext = stringPulledPath[ k + 1 ] - myFeetDP;
		toNext.z = 0.0;
		const double nextDist = QM_Vector3LengthDP( toNext );
		*outMoveDir = ( nextDist > 0.001 ) ? ( toNext * ( 1.0 / nextDist ) ) : forwardYawDir;
	}
	// Case 2: At a step-up riser, actively awaiting vertical elevation.
	// Condition check: Entity is at or past the riser boundary along the stair flight ascending direction.
	// Rather than steering backward or falling into yaw-lock singularities, drive strictly forward
	// across the riser along the flight ascent direction so StepSlideMove can step up.
	else if ( awaitingStepUp ) {
		// Determine the forward flight ascending direction:
		Vector3DP stepDir = { 0.0, 0.0, 0.0 };
		if ( k + 1 < stringPulledPath.size() ) {
			stepDir = stringPulledPath[ k + 1 ] - targetWp;
			stepDir.z = 0.0;
		}
		if ( QM_Vector3LengthSqrDP( stepDir ) < MONSTER_NAV_DIR_EPS_SQR && k > 0 ) {
			stepDir = targetWp - stringPulledPath[ k - 1 ];
			stepDir.z = 0.0;
		}
		if ( QM_Vector3LengthSqrDP( stepDir ) < MONSTER_NAV_DIR_EPS_SQR ) {
			stepDir = targetWp - myFeetDP;
			stepDir.z = 0.0;
		}
		const double stepDirLen = QM_Vector3LengthDP( stepDir );
		const Vector3DP uStep = ( stepDirLen > 0.001 ) ? ( stepDir * ( 1.0 / stepDirLen ) ) : forwardYawDir;

		// Check if entity has reached or crossed the riser plane along flight direction:
		Vector3DP fromTarget = myFeetDP - targetWp;
		fromTarget.z = 0.0;
		const double fwdPast = QM_Vector3DotProductDP( fromTarget, uStep );

		// If at or past the riser (within deadband or forward), drive strictly forward across the riser:
		if ( fwdPast >= -MONSTER_NAV_WAYPOINT_DEADBAND || steerDist < MONSTER_NAV_WAYPOINT_DEADBAND ) {
			*outMoveDir = uStep;
		} else {
			// Still approaching the riser: steer directly toward the step waypoint
			*outMoveDir = ( steerDist > 0.001 ) ? ( toSteer * ( 1.0 / steerDist ) ) : uStep;
		}
	}
	// Mandatory ingress corners require their exact approach direction. The old
	// bisector/monotonicity shortcuts could skip a distant corner and drive into a jamb.
	else if ( isTargetForced ) {
		*outMoveDir = steerDist > 0.001 ? toSteer * ( 1.0 / steerDist ) : forwardYawDir;
	}
	// Case 3: Outside proximity deadband — steer towards lookahead target with corner transit bisector protection.
	// Condition check: Entity is >= MONSTER_NAV_WAYPOINT_DEADBAND (4.0 units) from steerTarget,
	// safely away from the (0, 0) division singularity where micro-fluctuations flip yaw angles.
	else if ( steerDist >= MONSTER_NAV_WAYPOINT_DEADBAND ) {
		Vector3DP candDir = toSteer * ( 1.0 / steerDist );

		// Corner standoff transit protection:
		// When approaching or stepping adjacent to an intermediate corner waypoint W_k,
		// ensure candDir does not pull the entity backward against incoming flow or sideways into the corner obstacle.
		if ( k > 0 && k + 1 < stringPulledPath.size() ) {
			const Vector3DP prevWp = stringPulledPath[ k - 1 ];
			Vector3DP inSegDir = targetWp - prevWp;
			inSegDir.z = 0.0;
			const double inSegLen = QM_Vector3LengthDP( inSegDir );

			const Vector3DP nextWp = stringPulledPath[ k + 1 ];
			Vector3DP toNext = nextWp - targetWp;
			toNext.z = 0.0;
			const double toNextLen = QM_Vector3LengthDP( toNext );

			if ( inSegLen > 0.001 && toNextLen > 0.001 ) {
				const Vector3DP uIn = inSegDir * ( 1.0 / inSegLen );
				const Vector3DP uOut = toNext * ( 1.0 / toNextLen );

				// Corner transit progression blend: as the agent approaches or rounds intermediate corner waypoint W_k,
				// smoothly blend steering from direct waypoint heading towards the outgoing segment tangent uOut.
				// This guarantees smooth continuous yaw rotation around convex corner obstacles without threshold chatter:
				const double minHullReach = ( agentRadius > 0.0 ) ? ( agentRadius + MONSTER_NAV_CORNER_HULL_CLEARANCE_MARGIN ) : ( NAV_DEFAULT_AGENT_RADIUS + MONSTER_NAV_CORNER_HULL_CLEARANCE_MARGIN );
				const double sharpReach = std::max( MONSTER_NAV_SHARP_CORNER_REACH_RADIUS, minHullReach );

				if ( distToTarget <= sharpReach ) {
					const double blendFrac = std::clamp( 1.0 - ( distToTarget / sharpReach ), 0.0, 1.0 );
					Vector3DP blendedSteer = ( candDir * ( 1.0 - blendFrac ) ) + ( uOut * blendFrac );
					blendedSteer.z = 0.0;
					const double blendedSteerLen = QM_Vector3LengthDP( blendedSteer );
					if ( blendedSteerLen > 0.001 ) {
						candDir = blendedSteer * ( 1.0 / blendedSteerLen );
					}
				}
			}
		}

		// Forward monotonicity guard: if the direct vector to targetWp pulls backward against incoming travel flow
		// (because the entity has already crossed past W_k into the departure quadrant), clamp steering to outgoing flow:
		if ( k + 1 < stringPulledPath.size() ) {
			const Vector3DP nextWp = stringPulledPath[ k + 1 ];
			Vector3DP toNext = nextWp - targetWp;
			toNext.z = 0.0;
			const double toNextLen = QM_Vector3LengthDP( toNext );
			if ( toNextLen > 0.001 ) {
				const Vector3DP uOut = toNext * ( 1.0 / toNextLen );
				if ( k > 0 ) {
					const Vector3DP prevWp = stringPulledPath[ k - 1 ];
					Vector3DP inSegDir = targetWp - prevWp;
					inSegDir.z = 0.0;
					const double inSegLen = QM_Vector3LengthDP( inSegDir );
					if ( inSegLen > 0.001 ) {
						const Vector3DP uIn = inSegDir * ( 1.0 / inSegLen );
						if ( QM_Vector3DotProductDP( candDir, uIn ) < 0.0 ) {
							candDir = uOut;
						}
					}
				} else if ( QM_Vector3DotProductDP( candDir, uOut ) < 0.0 ) {
					candDir = uOut;
				}
			}
		}

		*outMoveDir = candDir;
	}
	// Case 4: Inside deadband of an intermediate waypoint (flat ground, gentle turns, ramps, or step-downs).
	// Condition check: Entity is within 4.0 units of W_k, not waiting for step-up elevation, and has a subsequent waypoint (k + 1 < size).
	// Steer forward toward the subsequent waypoint W_{k+1} so the agent smoothly rounds the point without orbiting W_k.
	else if ( !awaitingStepUp && k + 1 < stringPulledPath.size() ) {
		Vector3DP toNext = stringPulledPath[ k + 1 ] - myFeetDP;
		toNext.z = 0.0;
		const double nextDist = QM_Vector3LengthDP( toNext );
		*outMoveDir = ( nextDist > 0.001 ) ? ( toNext * ( 1.0 / nextDist ) ) : forwardYawDir;
	}
	// Case 5: Final destination reached.
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
	/**
	*	Hold station in place while waiting in the serialized room egress queue.
	*	Members exit one-by-one rapidly in proximity order; unreleased members stand idle in formation.
	**/
	if ( this->crowd.crowdID >= 0 ) {
		const svg_crowd_group_t *egressGroup = SVG_Crowd_GetGroup( this->crowd.crowdID );
		if ( egressGroup != nullptr && egressGroup->hasSerializedEgress && !this->crowd.egressReleased ) {
			velocity.x = velocity.y = 0.0f;
			monsterMove.state.velocity.x = 0.0;
			monsterMove.state.velocity.y = 0.0;
			UpdateAnim( 1 ); // IDLE
			return true;
		}
	}

	/**
	*	Query decentralized squad coordinator for formation target and pacing scale:
	**/
	float squadSpeedScale = 1.0f;
	Vector3DP squadSlotTarget = {};
	if ( this->QuerySquadFormationTarget( &squadSlotTarget, &squadSpeedScale ) ) {
		// If yielding right-of-way to preceding rank at a bottleneck portal:
		if ( squadSpeedScale <= 0.01f ) {
			velocity.x = velocity.y = 0.0f;
			monsterMove.state.velocity.x = 0.0;
			monsterMove.state.velocity.y = 0.0;
			UpdateAnim( 1 ); // IDLE
			return true;
		}
	}

	const Vector3DP goalOriginDP( goalOrigin );
	Vector3DP moveDirDP = {};
	double speedScale = static_cast<double>( squadSpeedScale );

	if ( !ComputePathSteering( goalOriginDP, &moveDirDP, &speedScale ) ) {
		velocity.x = velocity.y = 0.0f;
		monsterMove.state.velocity.x = velocity.x;
		monsterMove.state.velocity.y = velocity.y;
		UpdateAnim( 1 ); // IDLE
		return false;
	}

	/**
	*	Blend mutual crowd separation force and compute teammate following speed throttling:
	**/
	if ( this->crowd.crowdID >= 0 ) {
		const double rawRadius = static_cast<double>( ( maxs.x - mins.x ) * 0.5f );
		const double defaultRadius = ( pathNavigationState.policy.agent_radius > 0.0f ) ? static_cast<double>( pathNavigationState.policy.agent_radius ) : CROWD_DEFAULT_AGENT_RADIUS;
		const double agentRadius = ( rawRadius > 0.0 ) ? rawRadius : defaultRadius;

		// 1. Teammate queueing deceleration: if a leading squad member is directly ahead in our corridor lane,
		// yield speed to maintain safe following distance and prevent chokepoint / doorway jamming.
		suppressEntityDeflectionThisFrame = false;
		double followScale = 1.0;
		bool strictQueueing = false;
		const bool foundLeaderAhead = SVG_Crowd_ComputeTeammateFollowSpeedScale( this->s.number, moveDirDP, &followScale, &strictQueueing );
		if ( foundLeaderAhead ) {
			speedScale *= followScale;
			// Disable entity deflection only for strict doorway/chokepoint queues; allow room-entry flow otherwise.
			suppressEntityDeflectionThisFrame = strictQueueing;
		}

		// 2. Mutual soft separation repulsion between adjacent teammates.
		// Strict queueing agents must stay centered on the lane so they do not side-slip around the blocker.
		if ( !strictQueueing ) {
			Vector3DP sepForce{ 0.0, 0.0, 0.0 };
			if ( SVG_Crowd_ComputeMutualSeparation( this->s.number, &sepForce ) ) {
				const Vector3DP myFeetDP = SVG_GetEntityFeetOriginDP( this );
				// Extract purely lateral separation perpendicular to moveDirDP to avoid flipping moveDir backwards:
				const double forwardSep = QM_Vector3DotProductDP( sepForce, moveDirDP );
				const Vector3DP lateralSep = sepForce - ( moveDirDP * forwardSep );

				// Dampen lateral separation when close to destination or when speed is throttled to prevent yaw jitter:
				const double distToGoal = QM_Vector3Distance2DDP( myFeetDP, goalOriginDP );
				const double proximityDampening = std::clamp( distToGoal / 48.0, 0.0, 1.0 );
				const double sepDampening = ( speedScale < 0.5 ) ? ( speedScale * 2.0 * proximityDampening ) : proximityDampening;
				Vector3DP blendedDir = moveDirDP + ( lateralSep * ( 0.5 * sepDampening ) );
				const double blendedLen = QM_Vector3LengthDP( blendedDir );
				if ( blendedLen > 0.001 ) {
					const Vector3DP candDir = blendedDir * ( 1.0 / blendedLen );
					// Ensure candidate direction preserves forward progress (within 45 degrees of moveDirDP):
					if ( QM_Vector3DotProductDP( candDir, moveDirDP) >= 0.707 ) {
						const Vector3DP probeEnd = myFeetDP + candDir * ( agentRadius + CROWD_WALL_STANDOFF_MARGIN );
						// Use physical kinematic probe with the agent's hull to ensure separation doesn't push us into a solid wall.
						Vector3DP dummyEndpos = {};
						const double maxStep = ( this->pathNavigationState.policy.max_step_height > 0.0 ) ? this->pathNavigationState.policy.max_step_height : NAV_PROBE_DEFAULT_MAX_STEP_HEIGHT;
						const double maxDrop = ( this->pathNavigationState.policy.max_drop_height > 0.0 ) ? this->pathNavigationState.policy.max_drop_height : NAV_PROBE_DEFAULT_MAX_DROP_HEIGHT;
						if ( SVG_MMove_Probe( myFeetDP, this->mins, this->maxs, probeEnd, this, &dummyEndpos, maxStep, maxDrop ) ) {
							moveDirDP = candDir;
						}
					}
				}
			}
		}

		// 3. Squad formation speed regulation: rubber-band followers to maintain rank relative to the leader
		const svg_crowd_group_t *group = SVG_Crowd_GetGroup( this->crowd.crowdID );
		if ( group && group->isMoving && !group->hasSerializedIngress && !group->hasSerializedEgress && !this->crowd.reachedGoal ) {
			const svg_base_edict_t *leader = group->GetLeaderEntity();
			if ( !leader && this->crowd.slotIndex != 0 ) {
				for ( const int32_t memberNum : group->memberEntityNumbers ) {
					const svg_base_edict_t *ent = g_edict_pool.EdictForNumber( memberNum );
					if ( ent && ent->crowd.slotIndex == 0 ) {
						leader = ent;
						break;
					}
				}
			}

			if ( leader && leader != this && SVG_Entity_IsActive( leader ) && !leader->crowd.reachedGoal ) {
				const double leaderZDelta = std::fabs( leader->currentOrigin.z - this->currentOrigin.z );
				// Only apply 2D planar rubber-banding when on the same elevation level; on stairs or ramps,
				// corridor follow speed throttling in SVG_Crowd_ComputeTeammateFollowSpeedScale governs spacing:
				if ( leaderZDelta < static_cast<double>( NAV_STEP_MIN_VERTICAL_DELTA ) ) {
					const double leaderDistToDest = QM_Vector2DistanceDP( Vector2DP( leader->currentOrigin ), Vector2DP( group->destinationOrigin ) );
					const double myDistToDest = QM_Vector2DistanceDP( Vector2DP( this->currentOrigin ), Vector2DP( group->destinationOrigin ) );

					if ( myDistToDest > ( leaderDistToDest + CROWD_SLOT_CATCHUP_DISTANCE ) ) {
						// Trailing behind the leader: catch-up sprint
						speedScale *= CROWD_CATCHUP_SPEED_SCALE;
					} else if ( myDistToDest < ( leaderDistToDest - CROWD_SLOT_FOLLOW_TOLERANCE ) ) {
						// Surging ahead of the moving leader: throttle back to stay in rank
						speedScale *= CROWD_SLOWDOWN_SPEED_SCALE;
					}
				}
			}
		}
	}

	// If yielding to a leading teammate directly ahead, come to a complete standstill:
	if ( speedScale <= 0.01 ) {
		velocity.x = 0.0f;
		velocity.y = 0.0f;
		monsterMove.state.velocity.x = 0.0f;
		monsterMove.state.velocity.y = 0.0f;
		UpdateAnim( 1 ); // IDLE
		return true;
	}

	constexpr double baseFrameVelocity = 220.0;
	const double frameVelocity = baseFrameVelocity * speedScale;

	Vector3DP velocityDP = moveDirDP * frameVelocity;

	/**
	*	Enforce the finite serialized-ingress aperture after every steering contribution.
	*	Unreleased agents may approach the stop plane and move beyond it beside the room;
	*	only a predicted crossing through the doorway's lateral hull span is constrained.
	**/
	const svg_crowd_group_t *ingressGroup = ( this->crowd.crowdID >= 0 ) ? SVG_Crowd_GetGroup( this->crowd.crowdID ) : nullptr;
	if ( ingressGroup != nullptr && ingressGroup->hasSerializedIngress && !this->crowd.ingressReleased ) {
		const Vector3DP currentFeet = SVG_GetEntityFeetOriginDP( this );
		const double currentDepth = QM_Vector3DotProductDP( currentFeet - ingressGroup->ingressPortalOrigin, ingressGroup->ingressPortalInward );
		const double stopDepth = -ingressGroup->ingressReleaseDepth;
		const double frameSeconds = FRAME_TIME_S.Seconds();
		const double inwardSpeed = QM_Vector3DotProductDP( velocityDP, ingressGroup->ingressPortalInward );
		const double predictedDepth = currentDepth + ( inwardSpeed * frameSeconds );
		const Vector3DP portalTangent{ ingressGroup->ingressPortalInward.y, -ingressGroup->ingressPortalInward.x, 0.0 };
		const double currentLateral = QM_Vector3DotProductDP(
			currentFeet - ingressGroup->ingressPortalOrigin, portalTangent );
		const double lateralSpeed = QM_Vector3DotProductDP( velocityDP, portalTangent );
		const double reservedHalfSpan = ingressGroup->ingressPortalHalfWidth + ingressGroup->ingressReleaseDepth;
		bool crossesReservedAperture = false;

		// Evaluate the continuous velocity segment at its stop-plane intersection to prevent diagonal tunnelling.
		if ( frameSeconds > 0.0 && currentDepth <= stopDepth && predictedDepth > stopDepth ) {
			const double crossingFraction = std::clamp(
				( stopDepth - currentDepth ) / ( predictedDepth - currentDepth ), 0.0, 1.0 );
			const double crossingLateral = currentLateral +
				( lateralSpeed * frameSeconds * crossingFraction );
			crossesReservedAperture = std::fabs( crossingLateral ) < reservedHalfSpan;
		}

		// Limit only the inward speed that would cross the reserved finite aperture this frame.
		if ( crossesReservedAperture ) {
			const double maximumInwardSpeed = std::clamp(
				( stopDepth - currentDepth ) / frameSeconds, -frameVelocity, frameVelocity );
			if ( inwardSpeed > maximumInwardSpeed ) {
				velocityDP = velocityDP -
					( ingressGroup->ingressPortalInward * ( inwardSpeed - maximumInwardSpeed ) );
			}
		}

		// Preserve the configured speed ceiling after the half-space projection rotates the velocity vector.
		const double constrainedSpeed = std::sqrt( ( velocityDP.x * velocityDP.x ) + ( velocityDP.y * velocityDP.y ) );
		if ( constrainedSpeed > frameVelocity && constrainedSpeed > 0.001 ) {
			velocityDP = velocityDP * ( frameVelocity / constrainedSpeed );
		}
	}

	const double actualFrameVelocity = std::sqrt( ( velocityDP.x * velocityDP.x ) + ( velocityDP.y * velocityDP.y ) );
	if ( this->crowd.crowdID >= 0 && this->crowd.slotIndex >= 0 ) {
		const svg_crowd_group_t *grp = SVG_Crowd_GetGroup( this->crowd.crowdID );
		const double distToGoal = QM_Vector3Distance2DDP( SVG_GetEntityFeetOriginDP( this ), goalOriginDP );
		if ( distToGoal < 24.0 && grp && this->crowd.slotIndex < static_cast<int32_t>( grp->slots.size() ) ) {
			const svg_crowd_slot_t &mySlot = grp->slots[ this->crowd.slotIndex ];
			ideal_yaw = static_cast<float>( QM_AngleMod( grp->currentHeadingYaw + mySlot.relativeYawDeg ) );
		} else if ( actualFrameVelocity > 1.0 ) {
			const Vector3DP facingDir = velocityDP * ( 1.0 / actualFrameVelocity );
			ideal_yaw = static_cast<float>( QM_Vector3ToYawDP( facingDir ) );
		}
	} else if ( actualFrameVelocity > 1.0 ) {
		const Vector3DP facingDir = velocityDP * ( 1.0 / actualFrameVelocity );
		ideal_yaw = static_cast<float>( QM_Vector3ToYawDP( facingDir ) );
	}
	SVG_MMove_FaceIdealYaw( this, ideal_yaw, 45.0f );

	velocity.x = static_cast<float>( velocityDP.x );
	velocity.y = static_cast<float>( velocityDP.y );
	monsterMove.state.velocity.x = velocity.x;
	monsterMove.state.velocity.y = velocity.y;

	// Cohesive animation state: select smooth animation tier based on computed frame velocity:
	if ( actualFrameVelocity < 40.0 ) {
		UpdateAnim( 1 ); // IDLE
	} else if ( actualFrameVelocity < 140.0 ) {
		UpdateAnim( 2 ); // WALK
	} else {
		UpdateAnim( 4 ); // RUN
	}
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

	/**
	*	Hold station in place while waiting in the serialized room egress queue.
	*	Members exit one-by-one rapidly in proximity order; unreleased members stand idle in formation.
	**/
	if ( this->crowd.crowdID >= 0 ) {
		const svg_crowd_group_t *egressGroup = SVG_Crowd_GetGroup( this->crowd.crowdID );
		if ( egressGroup != nullptr && egressGroup->hasSerializedEgress && !this->crowd.egressReleased ) {
			velocity.x = velocity.y = 0.0f;
			monsterMove.state.velocity.x = 0.0;
			monsterMove.state.velocity.y = 0.0;
			UpdateAnim( 1 ); // IDLE
			return true;
		}
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

	// Capture wall contact normal for navigation diagnostics (only against static world geometry)
	hasRecentWallBlockNormal = false;
	bool blockedByWorldGeometry = false;
	for ( uint32_t i = 0; i < monsterMove.touchTraces.numberOfTraces; i++ ) {
		const svg_trace_t &touchTrace = monsterMove.touchTraces.traces[ i ];
		const bool isWorld = ( touchTrace.ent == nullptr || touchTrace.ent->s.number == 0 );
		if ( isWorld ) {
			blockedByWorldGeometry = true;
			if ( touchTrace.plane.normal[ 2 ] < MM_MIN_WALL_NORMAL_Z ) {
				recentWallBlockNormal = Vector3{ touchTrace.plane.normal[ 0 ], touchTrace.plane.normal[ 1 ], touchTrace.plane.normal[ 2 ] };
				hasRecentWallBlockNormal = true;
				lastWallBlockTime = level.time;
			}
		}
	}

	// If the entity is only in collision contact with fellow squad members / other dynamic entities rather than
	// solid world architecture, do not increment consecutiveBlockedFrames to wipe the navigation path.
	// Clearing and recalculating A* every 8 frames while queued behind a teammate thrashes CPU and causes severe FPS drops!
	if ( !blockedByWorldGeometry && !isHardBlockedThisFrame && this->crowd.crowdID >= 0 ) {
		consecutiveBlockedFrames = 0;
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

	if ( isHardBlockedThisFrame || consecutiveBlockedFrames >= MONSTER_NAV_STUCK_RECOVER_BLOCKED_FRAMES ) {
		// Nudge entity slightly outward along blocking wall normal to dislodge from doorframe creases if free space exists
		if ( hasRecentWallBlockNormal ) {
			const Vector3 unstickOrigin = this->currentOrigin + ( recentWallBlockNormal * MONSTER_NAV_STUCK_WALL_NUDGE_DIST );
			const svg_trace_t unstickTr = SVG_MMove_Trace( Vector3DP( this->currentOrigin ), this->mins, this->maxs, Vector3DP( unstickOrigin ), this, CM_CONTENTMASK_SOLID, MM_SHAPE_AUTO );
			if ( !unstickTr.startsolid && !unstickTr.allsolid && unstickTr.fraction >= 0.99f ) {
				SVG_Util_SetEntityOrigin( this, unstickOrigin, true );
				monsterMove.state.origin = unstickOrigin;
			}
		}
		ResetNavigationPath();
		lastPathCalcTime = 0_ms;
		consecutiveBlockedFrames = 0;
	}
}

/**
*	@brief	Static bridge dispatching nav_path_policy_t edge cost callbacks to monster instances.
**/
double svg_monster_base_t::NavEdgeCostCallbackBridge( int32_t fromFaceIdx, int32_t toFaceIdx, const nav_halfedge_t &he, double baseCost, svg_monster_base_t *monster ) {
	if ( monster != nullptr && monster->GetTypeInfo()->IsSubClassType<svg_monster_base_t>() ) {
		return monster->OnNavEvaluateEdgeCost( fromFaceIdx, toFaceIdx, he, baseCost );
	}
	return baseCost;
}

/**
*	@brief	Custom edge cost evaluator for A* navigation pathfinding.
*	@details Allows individual monster classes or states to bias path choices (e.g. preferring stairs/ramps,
*			applying path commitment hysteresis to prevent bifurcation jitter, avoiding hazards).
*	@param	fromFaceIdx	Source polygon index.
*	@param	toFaceIdx	Target polygon index.
*	@param	he			Half-edge connecting fromFace to toFace.
*	@param	baseCost	Standard geometric cost (distance * slope * clearance).
*	@return	Adjusted edge traversal cost.
**/
double svg_monster_base_t::OnNavEvaluateEdgeCost( const int32_t fromFaceIdx, const int32_t toFaceIdx, const nav_halfedge_t &he, const double baseCost ) {
	double cost = baseCost;

	/**
	*	Corridor Hysteresis:
	*	If toFaceIdx is part of our currently committed navPath corridor ahead of our position,
	*	apply a 15% discount (cost *= 0.85). This decisively breaks equal-cost ties across
	*	bifurcation manifolds (e.g. around obstacles or split stairways) and permanently prevents
	*	high-frequency route flip-flopping.
	**/
	if ( !navPath.empty() ) {
		for ( size_t i = pathPos; i < navPath.size(); ++i ) {
			if ( navPath[ i ] == toFaceIdx ) {
				cost *= 0.85;
				break;
			}
		}
	}

	return cost;
}

/**
*	@brief	Invoked when an intermediate path waypoint or final destination is reached and advanced during navigation.
*	@param	waypointIndex	Index of the reached waypoint in stringPulledPath.
*	@param	waypointPos		World-space coordinates of the reached waypoint in Vector3DP.
*	@param	isFinalGoal		True if the reached waypoint represents the final path destination.
**/
void svg_monster_base_t::OnWaypointReached( const size_t waypointIndex, const Vector3DP &waypointPos, const bool isFinalGoal ) {
	( void )waypointIndex;

	/**
	*	Dispatch waypoint arrival to the tactical evaluation hook.
	**/
	this->EvaluateWaypointArrival( waypointPos, static_cast<float>( MONSTER_NAV_WAYPOINT_REACH_RADIUS ), isFinalGoal );
}

/**
*	@brief	Query assigned squad formation target origin and recommended pacing speed scale.
*	@param	outTargetOrigin	[out] World-space formation slot origin in Vector3DP.
*	@param	outSpeedScale	[out] Recommended proportional speed multiplier [0.0..1.5].
*	@return	True if enrolled in an active squad, false otherwise.
**/
const bool svg_monster_base_t::QuerySquadFormationTarget( Vector3DP *outTargetOrigin, float *outSpeedScale ) {
	// Initialize default speed scale.
	if ( outSpeedScale != nullptr ) {
		*outSpeedScale = 1.0f;
	}

	// Early return if entity is not registered to a squad.
	if ( this->crowd.crowdID < 0 ) {
		return false;
	}

	/**
	*	Retrieve active tactical squad record from coordinator.
	**/
	const svg_squad_t *squad = SVG_Squad_Get( this->crowd.crowdID );
	if ( squad == nullptr || !squad->is_moving ) {
		return false;
	}

	const int32_t myRank = squad->GetMemberRank( this->s.number );
	if ( myRank < 0 || myRank >= static_cast<int32_t>( squad->slots.size() ) ) {
		return false;
	}

	const svg_crowd_slot_t &mySlot = squad->slots[ myRank ];
	if ( outTargetOrigin != nullptr ) {
		*outTargetOrigin = mySlot.worldPosition;
	}

	/**
	*	Compute proportional rubber-band pacing relative to squad leader and assigned slot.
	**/
	if ( outSpeedScale != nullptr ) {
		float scale = 1.0f;
		const Vector3DP myFeet = SVG_GetEntityFeetOriginDP( this );
		const double distToSlot = QM_Vector3Distance2DDP( myFeet, mySlot.worldPosition );

		//! Distance behind assigned slot to trigger catch-up sprint.
		static constexpr double SQUAD_SLOT_CATCHUP_DIST = 64.0;
		//! Distance ahead of slot to trigger deceleration.
		static constexpr double SQUAD_SLOT_SLOWDOWN_DIST = 16.0;

		const svg_base_edict_t *leader = squad->GetLeaderEntity();
		if ( leader != nullptr && leader != this && !leader->crowd.reachedGoal ) {
			const double leaderDist = QM_Vector3Distance2DDP( SVG_GetEntityFeetOriginDP( leader ), squad->destination_origin );
			const double myDist = QM_Vector3Distance2DDP( myFeet, squad->destination_origin );

			if ( myDist > leaderDist + SQUAD_SLOT_CATCHUP_DIST ) {
				scale = 1.25f; // Catch-up sprint
			} else if ( myDist < leaderDist - SQUAD_SLOT_SLOWDOWN_DIST ) {
				scale = 0.8f; // Slowdown to preserve rank
			}
		} else if ( distToSlot > SQUAD_SLOT_CATCHUP_DIST ) {
			scale = 1.25f;
		}

		/**
		*	Decentralized right-of-way check if squad is traversing a bottleneck aperture.
		**/
		if ( squad->active_bottleneck_portal_id >= 0 ) {
			bool hasRow = true;
			if ( SVG_Squad_QueryBottleneckRightOfWay( this->crowd.crowdID, this->s.number, squad->active_bottleneck_portal_id, &hasRow ) ) {
				if ( !hasRow ) {
					scale = 0.0f; // Yield speed to preceding rank
				}
			}
		}

		*outSpeedScale = scale;
	}

	return true;
}

/**
*	@brief	Query instantaneous topological sector awareness and upcoming portal geometry.
*	@param	goalOrigin		Destination world position.
*	@param	outSectorInfo	[out] Populated sector awareness snapshot.
*	@return	True if current sector was resolved, false otherwise.
**/
const bool svg_monster_base_t::QuerySectorAwareness( const Vector3DP &goalOrigin, nav_sector_info_t *outSectorInfo ) {
	const Vector3DP myFeet = SVG_GetEntityFeetOriginDP( this );
	return Nav_SectorGraph_QueryAwareness( myFeet, goalOrigin, outSectorInfo );
}

/**
*	@brief	Query right-of-way precedence when traversing a bottleneck aperture or portal.
*	@param	portalId			Portal index to evaluate.
*	@param	outHasRightOfWay	[out] True if entity is clear to traverse, false if yielding to preceding rank.
*	@return	True if query succeeded.
**/
const bool svg_monster_base_t::QueryBottleneckRightOfWay( const int32_t portalId, bool *outHasRightOfWay ) {
	if ( this->crowd.crowdID >= 0 ) {
		return SVG_Squad_QueryBottleneckRightOfWay( this->crowd.crowdID, this->s.number, portalId, outHasRightOfWay );
	}
	if ( outHasRightOfWay != nullptr ) {
		*outHasRightOfWay = true;
	}
	return true;
}

/**
*	@brief	Evaluate tactical actions and custom pursuit weighting upon reaching a waypoint.
*	@param	waypointPos		Position of reached waypoint in Vector3DP.
*	@param	waypointRadius	Acceptance radius.
*	@param	isFinalGoal		True if final destination slot reached.
**/
void svg_monster_base_t::EvaluateWaypointArrival( const Vector3DP &waypointPos, const float waypointRadius, const bool isFinalGoal ) {
	( void )waypointPos;
	( void )waypointRadius;

	/**
	*	Final destination arrived: mark crowd member arrival state.
	**/
	if ( isFinalGoal ) {
		this->crowd.reachedGoal = true;
		this->crowd.blockedStartTime = 0_ms;
	}
}

/**
*	@brief	Invoked when a designated tactical cover point is successfully reached.
*	@param	coverIndex	Index of the reached cover point in g_nav_cover_points.
*	@param	coverPos	World-space coordinates of the reached cover point in Vector3.
**/
void svg_monster_base_t::OnCoverPointReached( const int32_t coverIndex, const Vector3 &coverPos ) {
	// Base implementation is a no-op; derived monster classes override this to react to cover arrivals.
	( void )coverIndex;
	( void )coverPos;
}
