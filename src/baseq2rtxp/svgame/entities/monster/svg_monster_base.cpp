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
#include "svgame/nav/nav_query.h"
#include "svgame/nav/nav_worker.h"
#include "svgame/nav/nav_perf.h"
#include "svgame/nav/nav_actor_fixture.h"
#include "svgame/nav/nav_corner_capture.h"
#include "svgame/nav/nav_movement_geometry.h"
#include <chrono>

// Monster move and slide move.
#include "svgame/monsters/svg_mmove.h"
#include "svgame/monsters/svg_mmove_slidemove.h"

// Crowd and squad coordination.
#include "svgame/crowd/svg_crowd_manager.h"
#include "svgame/crowd/svg_squad_coordinator.h"
#include "svgame/nav/nav_sector_graph.h"


/**
*	@brief	Sample the monotonic clock shared by worker transport and owner latency accounting.
*	@return	Nanoseconds since the standard steady clock epoch.
**/
static uint64_t SVG_Nav_SteadyTimeNs() {
	// Use the same clock and unit as the pure service's enqueue/solve lifecycle timestamps.
	return static_cast<uint64_t>( std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::steady_clock::now().time_since_epoch() ).count() );
}

/**
*	@brief	Reconstructs the object, optionally retaining the entityDictionary.
*	@param	retainDictionary	When true, preserves the entityDictionary pointer.
**/
void svg_monster_base_t::Reset( const bool retainDictionary ) {
	// Record the old request before resetting its timing identity and cancelling transport ownership.
	SVG_Nav_SetQueryTerminalState( NavigationQueryState::Cancelled );
	// Rekey before pool reset/reuse so an old slot reply cannot match the replacement actor.
	Nav_CancelQueryWorkerEntityMainThread( s.number );
	navigationIncarnation = Nav_IssueIdentityRevisionMainThread();
	navigationStateRevision = Nav_IssueIdentityRevisionMainThread();
	IMPLEMENT_EDICT_RESET_BY_COPY_ASSIGNMENT( Super, SelfType, retainDictionary );

	monsterMove = {};
	pathNavigationState = {};
	// Pool reuse cannot retain the old actor's outstanding state or latency origin.
	navigationRequestSequence = 0;
	navigationSubmittedNs = navigationGraphStartNs = navigationGraphFinishNs = 0;
	navigationSubmittedTick = navigationEarliestDeliveryTick = 0;
	navigationTimingSequence = navigationTimingEpoch = navigationTerminalCountedSequence = 0;
	navPath.clear();
	stringPulledPath.clear();
	navigationWaypoints.clear();
	navigationRouteRevision = Nav_IssueIdentityRevisionMainThread();
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
	/**
	*	Start fresh tick-local evidence even when AI chooses a reservation or arrival hold.
	*	A held frame must not inherit a previously evaluated steering candidate.
	**/
	navigationSteering = {};
	navigationSteering.old_index = static_cast<uint32_t>( stringPathPos );
	navigationSteering.yaw_before = currentAngles[ YAW ];
	lastWaypointRelease = {};
	lastMovementValidation = {};
	lastGeometricValidation = {};
	navigationValidationTraceCount = 0;
	// Remove transient render interpolation flags before refreshing native support.
	s.renderfx &= ~( RF_STAIR_STEP | RF_OLD_FRAME_LERP );

	RecategorizeGroundAndLiquidState();

	/**
	*	Synchronize native movement inputs after recategorizing ground and liquid support.
	**/
	// Sync physics state with entity state
	monsterMove.state.origin = currentOrigin;
	monsterMove.state.velocity = velocity;
	monsterMove.ground = groundInfo;
	monsterMove.liquid = liquidInfo;

	// Grounded movement must agree with the newly resolved native support state.
	if ( groundInfo.entityNumber != ENTITYNUM_NONE ) {
		monsterMove.state.mm_flags |= MMF_ON_GROUND;
	} else {
		monsterMove.state.mm_flags &= ~MMF_ON_GROUND;
	}

	// Dead actors cannot proceed into live navigation or animation behavior.
	if ( !Nav_ActorMayNavigate( inUse, health, static_cast<int32_t>( lifeStatus ), static_cast<int32_t>( LIFESTATUS_ALIVE ) ) ) {
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
	// Capture the original command independently of authoritative clipped velocity.
	const Vector3DP captureStart( currentOrigin );
	const Vector3DP captureCommand( velocity );
	blockedMask = ( processSlideMove ? ProcessSlideMove() : MM_SLIDEMOVEFLAG_NONE );
	if ( !processSlideMove ) {
		// A reservation hold has no native trial; stale contacts must not be counted as accepted motion.
		monsterMove.acceptedDiagnostics = {};
		monsterMove.groundDiagnostics = {};
		monsterMove.elevatedDiagnostics = {};
	}

	/**
	*	Enforce the finite serialized doorway aperture after collision resolution.
	*	SlideMove may rotate velocity, so test the exact stop-plane intersection and
	*	block only crossings whose lateral coordinate lies inside the reserved opening.
	**/
	const svg_crowd_group_t *ingressGroup = ( crowd.crowdID >= 0 ) ? SVG_Crowd_GetGroup( crowd.crowdID ) : nullptr;
	// Both reservation senses retain the exact existing float endpoint enforcement after authoritative collision.
	const auto enforceAperture = [&]( const Vector3DP &portalOrigin, const Vector3DP &portalInward,
		const double stopDepth, const double reservedHalfSpan ) {
		const Vector3DP previousOrigin( currentOrigin );
		const double previousDepth = QM_Vector3DotProductDP( previousOrigin - portalOrigin, portalInward );
		// The entity stores float coordinates; enforce reservation against that exact post-conversion endpoint.
		const Vector3DP storedOrigin( QM_Vector3FromDP( monsterMove.state.origin ) );
		const double resolvedDepth = QM_Vector3DotProductDP( storedOrigin - portalOrigin, portalInward );
		const Vector3DP portalTangent{ portalInward.y, -portalInward.x, 0.0 };
		bool crossesReservedAperture = false;

		// Interpolate lateral position exactly where this frame crosses the exterior stop plane.
		if ( previousDepth <= stopDepth && resolvedDepth > stopDepth ) {
			const double crossingFraction = std::clamp(
				( stopDepth - previousDepth ) / ( resolvedDepth - previousDepth ), 0.0, 1.0 );
			const double previousLateral = QM_Vector3DotProductDP(
				previousOrigin - portalOrigin, portalTangent );
			const double resolvedLateral = QM_Vector3DotProductDP(
				storedOrigin - portalOrigin, portalTangent );
			const double crossingLateral = previousLateral +
				( ( resolvedLateral - previousLateral ) * crossingFraction );
			crossesReservedAperture = std::fabs( crossingLateral ) <= reservedHalfSpan;
		}

		// Project only a true aperture crossing; side-wall marshalling beyond the plane remains legal.
		if ( crossesReservedAperture ) {
			// Keep the stored float endpoint strictly outside; projecting to the literal double plane can round inward.
			const Vector3DP projectedOrigin = storedOrigin - portalInward * ( resolvedDepth - stopDepth );
			const double storageMargin = Nav_ReservationStorageMargin( projectedOrigin, portalInward );
			monsterMove.state.origin = projectedOrigin - portalInward * storageMargin;
			const double resolvedInwardSpeed = QM_Vector3DotProductDP( monsterMove.state.velocity, portalInward );
			if ( resolvedInwardSpeed > 0.0 ) {
				monsterMove.state.velocity = monsterMove.state.velocity -
					( portalInward * resolvedInwardSpeed );
			}
		}
	};
	// Preserve admission reservation after physical sliding can rotate the validated command.
	if ( ingressGroup != nullptr && ingressGroup->hasSerializedIngress && !crowd.ingressReleased ) {
		enforceAperture( ingressGroup->ingressPortalOrigin, ingressGroup->ingressPortalInward,
			-ingressGroup->ingressReleaseDepth, ingressGroup->ingressPortalHalfWidth + ingressGroup->ingressReleaseDepth );
	}
	// Retain the cleared origin lifetime's outward half-space while its remaining departure queue is active.
	if ( ingressGroup != nullptr && SVG_Crowd_HasClearedEgress( this ) ) {
		const double radius = std::max( { std::fabs( mins.x ), std::fabs( maxs.x ), std::fabs( mins.y ), std::fabs( maxs.y ) } );
		enforceAperture( ingressGroup->egressPortalOrigin, ingressGroup->egressPortalOutward * -1.0,
			-SVG_Crowd_GetEgressClearanceDepth( this ), ingressGroup->egressPortalHalfWidth + radius );
	}

	velocity = QM_Vector3FromDP( monsterMove.state.velocity );
	groundInfo = monsterMove.ground;
	liquidInfo = monsterMove.liquid;
	SVG_Util_SetEntityOrigin( this, monsterMove.state.origin, true );
	gi.linkentity( this );
	// Realized arrival commits its final event before the crowd manager can stop the thinker next frame.
	SVG_Nav_CheckFinalArrival();
	// Sampling is disabled unless an explicit bounded capture selects this slot/incarnation.
	Nav_Corner_AuditMovement( *this, captureStart, captureCommand );
	Nav_Corner_RecordMovement( *this, captureStart, captureCommand, blockedMask );

	if ( blockedMask & MM_SLIDEMOVEFLAG_TRAPPED ) {
		return false;
	}

	return true;
}

/**
*	@brief	Performs SlideMove processing and updates the final origin if successful.
*	@return	The blockedMask result from the slide move.
*	@note	Measures actual supported/ordinary collision execution separately from steering previews.
**/
const int32_t svg_monster_base_t::ProcessSlideMove() {
	/**
	*	Measure authoritative execution separately from graph work and command selection.
	**/
	nav_perf_scope_t movementScope( NAV_PERF_PHASE_ACTOR_MOVEMENT );
	/**
	*	Synchronize the exact native inputs before either supported navigation or general physics.
	**/
	monsterMove.monster = this;
	monsterMove.frameTime = FRAME_TIME_S.Seconds();
	monsterMove.state.origin = currentOrigin;
	monsterMove.state.velocity = velocity;
	monsterMove.mins = mins;
	monsterMove.maxs = maxs;
	monsterMove.ground = groundInfo;
	monsterMove.liquid = liquidInfo;

	/**
	*	Keep temporary entity-deflection policy local to this update.
	**/
	nav_path_policy_t slidePolicy = pathNavigationState.policy;
	slidePolicy.allow_entity_deflection = !suppressEntityDeflectionThisFrame;
	monsterMove.navPolicy = &slidePolicy;
	int32_t blockedMask = MM_SLIDEMOVEFLAG_NONE;

	/**
	*	A validated grounded navigation command must use the same complete native step contract
	*	during execution. General slide selection could choose a clipped or nudged floor trial
	*	with different XY progress despite a successful preview. Airborne/swimming/ladder and
	*	unvalidated commands retain their existing general physical integration.
	**/
	const bool executeSupportedNavigation = groundInfo.entityNumber == 0 && velocity.z == 0.0f &&
		!( monsterMove.state.mm_flags & MMF_ON_LADDER ) && liquidInfo.level < cm_liquid_level_t::LIQUID_WAIST &&
		navigationSteering.candidate_count > 0 && lastMovementValidation.reason == svg_nav_movement_reason_t::clear &&
		QM_Vector3Distance2DSqrDP( Vector3DP( velocity ), navigationSteering.final_velocity ) < 1e-8;
	if ( executeSupportedNavigation ) {
		// Reset actual-trial evidence before the authoritative sweeps; previews are never counted as clips.
		monsterMove.groundDiagnostics = {};
		monsterMove.elevatedDiagnostics = {};
		monsterMove.acceptedDiagnostics = {};
		monsterMove.state.previousOrigin = monsterMove.state.origin;
		monsterMove.state.previousVelocity = monsterMove.state.velocity;
		const Vector3DP feet = SVG_GetEntityFeetOriginDP( this );
		const Vector3DP commandedFeet = feet + Vector3DP( velocity ) * monsterMove.frameTime;
		ground_info_t landing = groundInfo;
		// Execute fresh actor-inclusive native sweeps, preserving contact effects and actual landing metadata.
		lastMovementValidation = SVG_Nav_TestMovementHull( feet, commandedFeet, mins, maxs,
			SVG_MMove_GetNativeShape( this ), slidePolicy, this, SVG_GetClipMask( this ), false, false,
			&monsterMove.touchTraces, &landing );
		navigationValidationTraceCount += lastMovementValidation.trace_count;
		if ( lastMovementValidation.reason == svg_nav_movement_reason_t::clear ) {
			/**
			*	On an ascending step transition or staircase flight, downward floor traces can probe
			*	past the upper step edge onto lower treads when the hull partially overhangs the riser.
			*	Clamping to the current feet elevation prevents the mover from being pulled back down.
			**/
			const bool isAscending = ( !stringPulledPath.empty() && stringPathPos < stringPulledPath.size() &&
				stringPulledPath[ stringPathPos ].z > feet.z - NAV_MOVEMENT_ENDPOINT_TOLERANCE );
			if ( isAscending && lastMovementValidation.bbox_endpoint.z < ( feet.z - mins.z ) - NAV_MOVEMENT_ENDPOINT_TOLERANCE ) {
				lastMovementValidation.bbox_endpoint.z = feet.z - mins.z;
			}

			// Commit only the fully swept supported endpoint; no slide, recovery displacement or radius reduction.
			monsterMove.state.origin = lastMovementValidation.bbox_endpoint;
			monsterMove.state.velocity.z = 0.0;
			monsterMove.ground = landing;
			const double rise = monsterMove.state.origin.z - monsterMove.state.previousOrigin.z;
			monsterMove.step.height = std::fabs( rise ) >= slidePolicy.min_step_height ? rise : 0.0;
			blockedMask = MM_SLIDEMOVEFLAG_MOVED;
		} else {
			// An execution-time obstruction holds at the original position and requests normal debounced replanning.
			monsterMove.state.velocity = Vector3DP{};
			navigationCommandRejected = true;
			blockedMask = MM_SLIDEMOVEFLAG_BLOCKED;
		}
	} else {
		// Preserve ordinary airborne, liquid, ladder and non-navigation slide movement behavior.
		blockedMask = SVG_MMove_StepSlideMove( &monsterMove, slidePolicy );
	}

	/**
	*	Restore persistent ownership and update blocked-route recovery from the actual execution outcome.
	**/
	monsterMove.navPolicy = &pathNavigationState.policy;
	suppressEntityDeflectionThisFrame = false;
	UpdateBlockedNavigationRecovery( blockedMask );
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
	// Explicit cancellation invalidates outstanding behavior requests before clearing geometry.
	Nav_CancelQueryWorkerEntityMainThread( s.number );
	SVG_Nav_SetQueryTerminalState( NavigationQueryState::Cancelled );
	navigationRequestSequence = 0;
	navigationStateRevision = Nav_IssueIdentityRevisionMainThread();
	navPath.clear();
	stringPulledPath.clear();
	navigationWaypoints.clear();
	navigationRouteRevision = Nav_IssueIdentityRevisionMainThread();
	waypointFacesMeshGeneration = 0;
	waypointTopologyRevision = 0;
	finalWaypointEventFired = false;
	navigationCommandRejected = false;
	lastMovementValidation = {};
	lastGeometricValidation = {};
	navigationValidationTraceCount = 0;
	currentStandingFace = -1;
	lastStandingFaceResolveTime = -1_ms;
	lastStandingFaceFeetOrigin = Vector3DP{ 0.0, 0.0, 0.0 };
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
*	@brief	Resolve current standing polygon face with local polygon and topological cache before KD-leaf lookup.
*	@param	myFeetDP	Agent feet-origin position in Vector3DP.
*	@return	Resolved face index or -1 if no face found.
**/
int32_t svg_monster_base_t::ResolveCurrentFace( const Vector3DP &myFeetDP ) {
	/**
	*	Sanity check: ensure nav faces exist in world.
	**/
	if ( g_nav_faces.empty() ) {
		currentStandingFace = -1;
		return -1;
	}

	/**
	*	Fast frame deduplication: if already resolved this server frame at the same feet position,
	*	return immediately in O(1) without re-running polygon/KD-tree scans.
	**/
	if ( lastStandingFaceResolveTime == level.time && currentStandingFace >= 0 && static_cast<size_t>( currentStandingFace ) < g_nav_faces.size() ) {
		// If queried at the exact same physical feet position within this server frame, reuse the cached face directly.
		if ( QM_Vector3DistanceSqrDP( myFeetDP, lastStandingFaceFeetOrigin ) < 0.001 ) {
			return currentStandingFace;
		}

		// Otherwise, if the position shifted slightly within the same frame, verify containment and return.
		const nav_face_t &face = g_nav_faces[ currentStandingFace ];
		if ( Nav_PointInsideFace2D( myFeetDP, face ) ) {
			const double zDelta = std::fabs( myFeetDP.z - face.center.z );
			if ( zDelta <= static_cast<double>( NAV_MAX_STEP_HEIGHT ) * 2.0 ) {
				lastStandingFaceFeetOrigin = myFeetDP;
				return currentStandingFace;
			}
		}
	}

	/**
	*	Check previously known standing face and its immediate topological neighbors.
	**/
	if ( currentStandingFace >= 0 && static_cast<size_t>( currentStandingFace ) < g_nav_faces.size() ) {
		const nav_face_t &face = g_nav_faces[ currentStandingFace ];
		if ( Nav_PointInsideFace2D( myFeetDP, face ) ) {
			const double zDelta = std::fabs( myFeetDP.z - face.center.z );
			if ( zDelta <= static_cast<double>( NAV_MAX_STEP_HEIGHT ) * 2.0 ) {
				lastStandingFaceResolveTime = level.time;
				lastStandingFaceFeetOrigin = myFeetDP;
				return currentStandingFace;
			}
		}

		for ( int32_t e = 0; e < face.num_edges; ++e ) {
			const nav_halfedge_t &he = g_nav_halfedges[ face.first_edge_idx + e ];
			if ( he.twin_idx != -1 ) {
				const int32_t nbrIdx = g_nav_halfedges[ he.twin_idx ].face_idx;
				if ( nbrIdx >= 0 && static_cast<size_t>( nbrIdx ) < g_nav_faces.size() ) {
					const nav_face_t &nbrFace = g_nav_faces[ nbrIdx ];
					if ( Nav_PointInsideFace2D( myFeetDP, nbrFace ) ) {
						const double zDelta = std::fabs( myFeetDP.z - nbrFace.center.z );
						if ( zDelta <= static_cast<double>( NAV_MAX_STEP_HEIGHT ) * 2.0 ) {
							currentStandingFace = nbrIdx;
							lastStandingFaceResolveTime = level.time;
							lastStandingFaceFeetOrigin = myFeetDP;
							return currentStandingFace;
						}
					}
				}
			}
		}
	}

	/**
	*	Check active path face and immediate topological neighbors.
	**/
	if ( !navPath.empty() && pathPos < navPath.size() ) {
		const int32_t pathFaceIdx = navPath[ pathPos ];
		if ( pathFaceIdx >= 0 && static_cast<size_t>( pathFaceIdx ) < g_nav_faces.size() ) {
			const nav_face_t &pathFace = g_nav_faces[ pathFaceIdx ];
			if ( Nav_PointInsideFace2D( myFeetDP, pathFace ) ) {
				const double zDelta = std::fabs( myFeetDP.z - pathFace.center.z );
				if ( zDelta <= static_cast<double>( NAV_MAX_STEP_HEIGHT ) * 2.0 ) {
					currentStandingFace = pathFaceIdx;
					lastStandingFaceResolveTime = level.time;
					lastStandingFaceFeetOrigin = myFeetDP;
					return currentStandingFace;
				}
			}

			for ( int32_t e = 0; e < pathFace.num_edges; ++e ) {
				const nav_halfedge_t &he = g_nav_halfedges[ pathFace.first_edge_idx + e ];
				if ( he.twin_idx != -1 ) {
					const int32_t nbrIdx = g_nav_halfedges[ he.twin_idx ].face_idx;
					if ( nbrIdx >= 0 && static_cast<size_t>( nbrIdx ) < g_nav_faces.size() ) {
						const nav_face_t &nbrFace = g_nav_faces[ nbrIdx ];
						if ( Nav_PointInsideFace2D( myFeetDP, nbrFace ) ) {
							const double zDelta = std::fabs( myFeetDP.z - nbrFace.center.z );
							if ( zDelta <= static_cast<double>( NAV_MAX_STEP_HEIGHT ) * 2.0 ) {
								currentStandingFace = nbrIdx;
								lastStandingFaceResolveTime = level.time;
								lastStandingFaceFeetOrigin = myFeetDP;
								return currentStandingFace;
							}
						}
					}
				}
			}
		}
	}

	/**
	*	Fallback to KD-leaf spatial lookup.
	**/
	currentStandingFace = Nav_FindClosestFaceInLeaf( myFeetDP );
	lastStandingFaceResolveTime = level.time;
	lastStandingFaceFeetOrigin = myFeetDP;
	return currentStandingFace;
}

/**
*	@brief	Find the current KD-Tree polygon the entity is standing on.
*	@return	Face index or -1.
**/
const int32_t svg_monster_base_t::FindCurrentPoly() {
	Vector3 myFeet = currentOrigin;
	myFeet.z += this->mins.z;
	return ResolveCurrentFace( Vector3DP( myFeet ) );
}

/**
*	@brief	Copy every owner revision needed to reject superseded actor route results.
*	@return	Scalar identity with no pointer to the live actor or crowd assignment.
**/
nav_query_identity_t svg_monster_base_t::SVG_Nav_CaptureIdentity() {
	// Issuing first permits controlled epoch rollover before the remaining scalar capture.
	nav_query_identity_t identity;
	identity.sequence = Nav_IssueIdentityRevisionMainThread();
	navigationRequestSequence = identity.sequence;
	identity.entity = s.number;
	identity.incarnation = navigationIncarnation;
	identity.goal = navigationGoalRevision;
	identity.state = navigationStateRevision;
	identity.crowd = crowd.navigationRevision;
	identity.epoch = Nav_IdentityEpochMainThread();
	identity.generation = g_nav_mesh_generation;
	identity.topology = g_nav_topology_revision;
	return identity;
}

/**
*	@brief	Check copied reply ownership against the actor and current navigation publication.
*	@return	True only for a live actor with every submission revision still current.
**/
bool svg_monster_base_t::SVG_Nav_IsIdentityCurrent( const nav_query_identity_t &identity ) const {
	// Native commit never accepts a dead actor, reused slot or superseded order.
	nav_query_identity_t current;
	current.entity = s.number;
	current.incarnation = navigationIncarnation;
	current.goal = navigationGoalRevision;
	current.state = navigationStateRevision;
	current.crowd = crowd.navigationRevision;
	current.epoch = Nav_IdentityEpochMainThread();
	current.generation = g_nav_mesh_generation;
	current.topology = g_nav_topology_revision;
	return inUse && lifeStatus == entity_lifestatus_t::LIFESTATUS_ALIVE && Nav_ActorIdentityMatches( current, identity, false );
}

/**
*	@brief	Retain safe installed geometry during a goal-only pending replacement.
*	@return	True only for unchanged actor/order/publication and physical traversal limits.
*	@note	Final arrival and gameplay callbacks still require the exact current goal revision.
**/
bool svg_monster_base_t::SVG_Nav_IsCommittedRouteUsable() const {
	/**
	*	Relax only the goal identity while pending; every other ownership field remains mandatory.
	**/
	auto identity = navigationAcceptedIdentity;
	// A goal-only replan/backoff may retain safe geometry; order/state changes still fail closed.
	if ( !stringPulledPath.empty() ) {
		identity.goal = navigationGoalRevision;
	}
	auto current = pathNavigationState.policy;
	current.agent_radius = current.agent_radius > 0.0f ? current.agent_radius : maxs.x;
	current.agent_mins = mins;
	current.agent_maxs = maxs;
	current.trace_shape = static_cast<int32_t>( SVG_MMove_GetNativeShape( this ) );
	return SVG_Nav_IsIdentityCurrent( identity ) && Nav_ActorNativePolicyMatches( navigationCommittedPolicy, current );
}

/**
*	@brief	Set a terminal actor request state and count the latest worker disposition once.
*	@param	state	NoRoute, Stale, Cancelled or ResourceLimited terminal state.
*	@param	cause	Owner-observed rejection site for accounting; retries and native validation remain unchanged.
*	@note	Accepted commits also close their accounting identity before gameplay callbacks run.
**/
void svg_monster_base_t::SVG_Nav_SetQueryTerminalState( const NavigationQueryState state,
	const nav_actor_terminal_cause_t cause ) {
	/**
	*	Reject nonterminal states before changing either lifecycle or performance accounting.
	**/
	const bool terminal = state == NavigationQueryState::NoRoute || state == NavigationQueryState::Stale ||
		state == NavigationQueryState::Cancelled || state == NavigationQueryState::ResourceLimited;
	Q_assert( terminal );
	// An invalid internal caller must not label a live request with an unsupported terminal disposition.
	if ( !terminal ) {
		return;
	}
	navigationQueryState = state;
	/**
	*	Only an actual latest worker submission that has not completed contributes a terminal sample.
	**/
	if ( navigationTimingSequence == 0 || navigationTimingSequence != navigationRequestSequence ||
		 navigationTerminalCountedSequence == navigationTimingSequence ) {
		return;
	}
	nav_perf_worker_terminal_t disposition = nav_perf_worker_terminal_t::NoRoute;
	// Map the actor-facing lifecycle explicitly rather than relying on unrelated enum ordinal values.
	switch ( state ) {
	case NavigationQueryState::NoRoute:
		disposition = nav_perf_worker_terminal_t::NoRoute;
		break;
	case NavigationQueryState::Stale: {
		/**
		*	Separate legitimate goal supersession without permitting any ownership or policy mismatch.
		**/
		nav_query_identity_t current;
		current.entity = s.number;
		current.incarnation = navigationIncarnation;
		current.sequence = navigationRequestSequence;
		current.goal = navigationGoalRevision;
		current.state = navigationStateRevision;
		current.crowd = crowd.navigationRevision;
		current.epoch = Nav_IdentityEpochMainThread();
		current.generation = g_nav_mesh_generation;
		current.topology = g_nav_topology_revision;
		auto policy = pathNavigationState.policy;
		policy.agent_radius = policy.agent_radius > 0.0f ? policy.agent_radius : maxs.x;
		policy.agent_mins = mins;
		policy.agent_maxs = maxs;
		policy.trace_shape = static_cast<int32_t>( SVG_MMove_GetNativeShape( this ) );
		nav_query_cost_policy_t cost;
		const auto &request = navigationGraphWorkspace.request;
		const bool liveContract = Nav_ActorMayNavigate( inUse, health, static_cast<int32_t>( lifeStatus ),
			static_cast<int32_t>( entity_lifestatus_t::LIFESTATUS_ALIVE ) ) && navigationTimingEpoch == current.epoch &&
			BuildNavigationCostSnapshot( cost );
		const bool goalOnly = liveContract &&
			Nav_ActorGoalOnlySuperseded( current, request.identity, policy, navigationPendingPolicy, cost, request.cost );
		// Only the explicit failed attachment site with a completely current contract gets its own recovery counter.
		const bool detached = liveContract && cause == nav_actor_terminal_cause_t::CorridorDetached &&
			Nav_ActorPhysicalRequestMatches( current, request.identity, policy, navigationPendingPolicy, cost, request.cost );
		disposition = goalOnly ? nav_perf_worker_terminal_t::GoalSuperseded :
			detached ? nav_perf_worker_terminal_t::CorridorDetached : nav_perf_worker_terminal_t::Stale;
		break;
	}
	case NavigationQueryState::Cancelled:
		disposition = nav_perf_worker_terminal_t::Cancelled;
		break;
	case NavigationQueryState::ResourceLimited:
		disposition = nav_perf_worker_terminal_t::ResourceLimited;
		break;
	default:
		Q_assert( false );
		return;
	}
	// Mark first so any later reset or obsolete reply cannot count this request a second time.
	navigationTerminalCountedSequence = navigationTimingSequence;
	Nav_Perf_CountWorkerTerminal( disposition );
}

/**
*	@brief	Finish the latest copied graph reply after transport storage has been released.
*	@note	All face attachment, collision, callbacks and installation remain on the game owner.
**/
void svg_monster_base_t::SVG_Nav_AcceptWorkerResult() {
	nav_perf_scope_t commitScope( NAV_PERF_PHASE_NAV_COMMIT );
	auto &graph = navigationGraphWorkspace.result;
	const auto &request = navigationGraphWorkspace.request;
	/**
	*	Check identity and copied physical limits again before native traces; stale replies cannot move actors.
	**/
	if ( navigationQueryState != NavigationQueryState::Pending || !SVG_Nav_IsIdentityCurrent( graph.identity ) ||
		 graph.identity.sequence != navigationRequestSequence || request.identity.sequence != graph.identity.sequence ) {
		return;
	}
	auto policy = pathNavigationState.policy;
	policy.agent_radius = policy.agent_radius > 0.0f ? policy.agent_radius : maxs.x;
	policy.agent_mins = mins;
	policy.agent_maxs = maxs;
	policy.trace_shape = static_cast<int32_t>( SVG_MMove_GetNativeShape( this ) );
	// Hull/policy mutation without a behavioral reset still invalidates its pending physical contract.
	if ( !Nav_ActorNativePolicyMatches( navigationPendingPolicy, policy ) ) {
		// Attribute this native rejection without relaxing its fail-closed disposition.
		Nav_ActorBenchmarkRecordRejectionMainThread( 0u );
		// Attribute this native rejection without relaxing its fail-closed disposition.
		Nav_ActorBenchmarkRecordRejectionMainThread( 0u );
		SVG_Nav_SetQueryTerminalState( NavigationQueryState::Stale );
		return;
	}
	// Tactical state changes must also match the copied cost contract before native route construction.
	nav_query_cost_policy_t currentCost;
	if ( !BuildNavigationCostSnapshot( currentCost ) || !Nav_ActorCostPolicyMatches( request.cost, currentCost ) ) {
		// Attribute this native rejection without relaxing its fail-closed disposition.
		Nav_ActorBenchmarkRecordRejectionMainThread( 1u );
		SVG_Nav_SetQueryTerminalState( NavigationQueryState::Stale );
		return;
	}
	/**
	*	Terminal statuses back off instead of being mistaken for arrival or invoking an unlimited owner solve.
	**/
	lastPathCalcTime = level.time;
	if ( graph.status != nav_query_status_t::found ) {
		SVG_Nav_SetQueryTerminalState( graph.status == nav_query_status_t::resource_limited ? NavigationQueryState::ResourceLimited :
			graph.status == nav_query_status_t::cancelled ? NavigationQueryState::Cancelled :
			graph.status == nav_query_status_t::no_route ? NavigationQueryState::NoRoute : NavigationQueryState::Stale );
		pathNavigationState.lastGoal.isValid = false;
		return;
	}
	/**
	*	Delayed movement must remain in the solved corridor; native funnel attachment starts at current feet.
	**/
	const Vector3DP feet = SVG_GetEntityFeetOriginDP( this );
	const int32_t face = ResolveCurrentFace( feet );
	const auto attached = std::find( graph.corridor.begin(), graph.corridor.end(), face );
	if ( attached == graph.corridor.end() ) {
		// Attribute this native rejection without relaxing its fail-closed disposition.
		Nav_ActorBenchmarkRecordRejectionMainThread( 2u );
		SVG_Nav_SetQueryTerminalState( NavigationQueryState::Stale, nav_actor_terminal_cause_t::CorridorDetached );
		return;
	}
	svg_nav_result_t candidate;
	candidate.identity = graph.identity;
	candidate.corridor.assign( attached, graph.corridor.end() );
	const auto counter = []( const uint64_t value ) { return static_cast<int32_t>( std::min<uint64_t>( value, INT32_MAX ) ); };
	const auto &counters = graph.diagnostics;
	auto &diagnostics = candidate.diagnostics;
	diagnostics.start_face = face;
	diagnostics.goal_face = request.goal_face;
	diagnostics.query_valid = true;
	diagnostics.route_found = true;
	diagnostics.graph_cost = graph.cost;
	diagnostics.expanded_faces = counter( counters.expansions );
	diagnostics.accepted_transitions = counter( counters.accepted );
	diagnostics.rejected_disabled = counter( counters.disabled );
	diagnostics.rejected_narrow_portal = counter( counters.narrow );
	diagnostics.rejected_step_height = counter( counters.rises );
	diagnostics.rejected_drop_height = counter( counters.drops );
	const Vector3DP goal{ graph.goal.x, graph.goal.y, graph.goal.z };
	// Shared completion validates the exact requested goal and current native continuation before commit.
	const uint64_t submittedSequence = graph.identity.sequence, submittedIncarnation = graph.identity.incarnation;
	if ( SVG_Nav_InstallCorridor( candidate, feet, goal, navigationPendingTarget, navigationPendingPolicy, navigationPendingReason ) != PathComputeResult::NewPathGenerated &&
		 navigationQueryState == NavigationQueryState::Pending && navigationRequestSequence == submittedSequence && navigationIncarnation == submittedIncarnation ) {
		SVG_Nav_SetQueryTerminalState( NavigationQueryState::NoRoute );
	}
}

/**
*	@brief	Compute an A* path to the target origin.
*	@param	target	Target destination world-space position.
*	@param	force	When true, bypasses debouncing and recalculates immediately.
*	@return	Path evaluation result state.
**/
svg_monster_base_t::PathComputeResult svg_monster_base_t::ComputePathTo( const Vector3 &target, const bool force ) {
	nav_perf_scope_t orch_scope( NAV_PERF_PHASE_NAV_ORCHESTRATION );
	/**
	*	Direct diagnostic or gameplay calls cannot submit new work after death or pool release.
	**/
	if ( !Nav_ActorMayNavigate( inUse, health, static_cast<int32_t>( lifeStatus ), static_cast<int32_t>( LIFESTATUS_ALIVE ) ) ) {
		Nav_CancelQueryWorkerEntityMainThread( s.number );
		SVG_Nav_SetQueryTerminalState( NavigationQueryState::Cancelled );
		return PathComputeResult::Failed;
	}
	/**
	*	Reject unsupported XY hulls explicitly; path and physical trace must use the same radius.
	**/
	if ( !Nav_IsSupportedNativeHull( mins, maxs ) ) {
		Nav_Perf_CountRequest( NAV_PERF_REQ_REASON_NEW_GOAL, NAV_PERF_OUTCOME_FAILED_NO_ROUTE );
		navigationCommandRejected = true;
		return PathComputeResult::Failed;
	}
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

	// Any actual target change invalidates the old goal identity, even inside the historical debounce radius.
	const bool targetMoved = !navigationRequestedGoalValid || QM_Vector3DistanceSqrDP( targetFeetDP, navigationRequestedGoal ) > 0.000001;
	if ( targetMoved ) {
		navigationRequestedGoal = targetFeetDP;
		navigationRequestedGoalValid = true;
		navigationGoalRevision = Nav_IssueIdentityRevisionMainThread();
	}
	const bool pathEmpty = navPath.empty() || stringPulledPath.empty();
	/**
	*	A matching outstanding request survives force callers without a per-tick resubmission burst.
	*	Goal-only replacement preserves queued age; other ownership changes cancel immediately.
	**/
	if ( navigationQueryState == NavigationQueryState::Pending ) {
		const bool workerEnabled = Nav_IsActorQueryWorkerEnabledMainThread();
		const auto &pendingIdentity = navigationGraphWorkspace.request.identity;
		const bool latestRequest = pendingIdentity.sequence == navigationRequestSequence;
		// Keep an unchanged request outstanding until delivery rather than repeatedly submitting it.
		if ( workerEnabled && latestRequest && SVG_Nav_IsIdentityCurrent( pendingIdentity ) ) {
			return PathComputeResult::Pending;
		}
		// Relax only goal for this classification; state, order, actor and publication remain strict.
		auto replacementIdentity = pendingIdentity;
		replacementIdentity.goal = navigationGoalRevision;
		const bool goalOnlyReplacement = workerEnabled && latestRequest && SVG_Nav_IsIdentityCurrent( replacementIdentity );
		// Submit preserves age when coalescing queued goals and separately cancels running replacements.
		// Immediate cancellation is reserved for ownership that can no longer authorize a replacement.
		if ( !goalOnlyReplacement ) {
			Nav_CancelQueryWorkerEntityMainThread( s.number );
		}
		SVG_Nav_SetQueryTerminalState( NavigationQueryState::Stale );
	}


	nav_perf_request_reason_t reqReason = NAV_PERF_REQ_REASON_NEW_GOAL;
	if ( force ) {
		reqReason = NAV_PERF_REQ_REASON_FORCE;
	} else if ( targetMoved ) {
		reqReason = pathNavigationState.lastGoal.isValid ? NAV_PERF_REQ_REASON_MOVED_GOAL : NAV_PERF_REQ_REASON_NEW_GOAL;
	} else if ( pathEmpty ) {
		reqReason = NAV_PERF_REQ_REASON_BLOCKED;
	}

	const QMTime requiredCooldown = pathEmpty ? MONSTER_NAV_PATH_FAIL_RECALC_INTERVAL : MONSTER_NAV_PATH_RECALC_MIN_INTERVAL;
	const bool cooldownElapsed = ( ( level.time - lastPathCalcTime ) >= requiredCooldown );
	const bool queryBackoff = Nav_ActorQueryShouldBackoff( navigationQueryState, cooldownElapsed, force,
		Nav_IsActorQueryWorkerEnabledMainThread() && navigationRequestSequence != 0 );
	/**
	*	Missing/obsolete routes already fail the cooldown gate; avoid spatial work before that decision.
	*	Current routes retain corridor reuse checks and all forced/worker retry semantics.
	**/
	if ( queryBackoff && ( pathEmpty || !SVG_Nav_IsCommittedRouteUsable() ) ) {
		Nav_Perf_CountRequest( reqReason, NAV_PERF_OUTCOME_COOLDOWN_THROTTLED );
		return PathComputeResult::Failed;
	}

	// 0) Fast O(1) topological face lookup: resolve agent's current standing face once with local checks before KD-leaf.
	int32_t startFace = ResolveCurrentFace( myFeetDP );
	/**
	*	Resolve the destination face conservatively before any path search.
	*	Strict containment prevents a staging cell near a seam, doorway, or slope
	*	from snapping to a different room component and making the monster appear
	*	idle at spawn.
	**/
	int32_t goalFace = -1;
	// Retained geometry may belong to an earlier goal while replacement is debounced or retried.
	// Reuse its endpoint face only when that accepted goal and publication still own this target.
	if ( !targetMoved && !navPath.empty() && navigationAcceptedGoalRevision == navigationGoalRevision &&
		 waypointFacesMeshGeneration == g_nav_mesh_generation && waypointTopologyRevision == g_nav_topology_revision ) {
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
		Nav_Perf_CountRequest( reqReason, NAV_PERF_OUTCOME_FAILED_NO_ROUTE );
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
		/**
		*	Resolve stair classification only when its larger tolerance could affect proximity.
		*	Incoming/waypoint checks share wpCurr; outgoing checks also use the next waypoint.
		**/
		double maxVerticalTolerance = static_cast<double>( NAV_MAX_STEP_HEIGHT ) + MONSTER_NAV_VERTICAL_STEP_TOLERANCE;
		const bool waypointNeedsStairTolerance = std::fabs( myFeetDP.z - wpCurr.z ) > maxVerticalTolerance;
		const bool nextNeedsStairTolerance = k + 1 < stringPulledPath.size() &&
			std::fabs( myFeetDP.z - stringPulledPath[ k + 1 ].z ) > maxVerticalTolerance;
		// Near-height routes pass the same vertical check in every zone without another face search.
		if ( ( waypointNeedsStairTolerance || nextNeedsStairTolerance ) &&
			Nav_GetZoneTypeForPoint( myFeetDP ) == ZONE_TYPE_CORRIDOR_STAIRS ) {
			maxVerticalTolerance = static_cast<double>( NAV_MAX_STEP_HEIGHT ) * 1.5 + MONSTER_NAV_VERTICAL_STEP_TOLERANCE;
		}

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
	// Reuse only a route whose geometry and final command remain valid at the current revisions.
	if ( !force && !targetMoved && stillOnPath && !pathEmpty && !navigationCommandRejected && SVG_Nav_IsIdentityCurrent( navigationAcceptedIdentity ) &&
		 waypointFacesMeshGeneration == g_nav_mesh_generation && waypointTopologyRevision == g_nav_topology_revision ) {
		Nav_Perf_CountRequest( reqReason, NAV_PERF_OUTCOME_REUSED_CACHED );
		return PathComputeResult::ReusedCached;
	}


	// When a recalculation is actually required, enforce minimum cooldown interval to prevent high-frequency A* thrashing:
	if ( queryBackoff ) {
		Nav_Perf_CountRequest( reqReason, NAV_PERF_OUTCOME_COOLDOWN_THROTTLED );
		return ( pathEmpty || !SVG_Nav_IsCommittedRouteUsable() || !stillOnPath || navigationCommandRejected )
			? PathComputeResult::Failed
			: PathComputeResult::ReusedCached;
	}

	/**
	*	Snapshot corridor membership into indexed marks for O(1) hysteresis discount in OnNavEvaluateEdgeCost.
	*	We keep the existing navPath intact so the discount can reference the committed corridor.
	**/
	if ( !navPath.empty() ) {
		if ( corridorMembershipMarks.size() < g_nav_faces.size() ) {
			corridorMembershipMarks.resize( g_nav_faces.size(), 0 );
		}
		corridorMembershipStamp++;
		if ( corridorMembershipStamp == 0 ) {
			std::fill( corridorMembershipMarks.begin(), corridorMembershipMarks.end(), 0 );
			corridorMembershipStamp = 1;
		}
		for ( size_t i = pathPos; i < navPath.size(); ++i ) {
			const int32_t faceIdx = navPath[ i ];
			if ( faceIdx >= 0 && static_cast<size_t>( faceIdx ) < corridorMembershipMarks.size() ) {
				corridorMembershipMarks[ faceIdx ] = corridorMembershipStamp;
			}
		}
	} else {
		corridorMembershipStamp++;
		if ( corridorMembershipStamp == 0 ) {
			std::fill( corridorMembershipMarks.begin(), corridorMembershipMarks.end(), 0 );
			corridorMembershipStamp = 1;
		}
	}

	nav_path_policy_t pathPolicy = pathNavigationState.policy;
	if ( pathPolicy.agent_radius <= 0.0 ) {
		pathPolicy.agent_radius = static_cast<float>( agentRadius );
	}
	// Graph and native continuation use the actor's actual hull and analytical primitive.
	pathPolicy.agent_mins = mins;
	pathPolicy.agent_maxs = maxs;
	pathPolicy.trace_shape = static_cast<int32_t>( SVG_MMove_GetNativeShape( this ) );
	pathPolicy.edge_cost_callback = &svg_monster_base_t::NavEdgeCostCallbackBridge;
	pathPolicy.edge_cost_monster = this;

	// Explicit causal modes preserve identical accepted geometry instead of changing the route during observation.
	if ( navigationFixtureMode >= 2 && navigationFixtureMode <= 4 && !stringPulledPath.empty() ) {
		return navigationAcceptedGoalRevision == navigationGoalRevision &&
			waypointFacesMeshGeneration == g_nav_mesh_generation && waypointTopologyRevision == g_nav_topology_revision ?
			PathComputeResult::ReusedCached : PathComputeResult::Failed;
	}
	// Build replacement route into candidate buffers without clearing committed route.
	svg_nav_result_t candidate;
	// Capture every actor/order revision before graph preparation, with no live pointer in the reply.
	candidate.identity = SVG_Nav_CaptureIdentity();
	auto &graphWorkspace = navigationGraphWorkspace;
	auto &candidateNavPath = candidate.corridor;
	// Snapshot supported virtual policies; unknown subclasses retain the explicit synchronous callback path.
	nav_query_cost_policy_t snapshotCost;
	const bool hasSnapshotCost = BuildNavigationCostSnapshot( snapshotCost );
	/**
	*	Supported policies enqueue copied graph work; saturation never falls back to a synchronous solve.
	**/
	if ( hasSnapshotCost && Nav_IsActorQueryWorkerEnabledMainThread() ) {
		navigationPendingPolicy = pathPolicy;
		navigationPendingTarget = target;
		navigationPendingReason = static_cast<int32_t>( reqReason );
		lastPathCalcTime = level.time;
		// Admission priority belongs to the actor's actual serialized order, not default release flags.
		const auto *queryGroup = crowd.crowdID >= 0 ? SVG_Crowd_GetGroup( crowd.crowdID ) : nullptr;
		const uint32_t priority = Nav_ActorQueryPriority( queryGroup != nullptr,
			queryGroup != nullptr && queryGroup->hasSerializedIngress, crowd.ingressReleased,
			queryGroup != nullptr && queryGroup->hasSerializedEgress, crowd.egressReleased );
		// Sample the latest actual submission attempt; queued coalescing may preserve an older fairness age.
		navigationTimingSequence = candidate.identity.sequence;
		navigationTimingEpoch = candidate.identity.epoch;
		// A new actual attempt owns fresh accounting even when epoch rollover repeats a prior sequence.
		navigationTerminalCountedSequence = 0;
		navigationGraphStartNs = navigationGraphFinishNs = 0;
		navigationSubmittedNs = SVG_Nav_SteadyTimeNs();
		navigationSubmittedTick = static_cast<uint64_t>( level.frameNumber );
		if ( Nav_SubmitActorQueryMainThread( graphWorkspace.request, startFace, goalFace, pathPolicy, snapshotCost,
			corridorMembershipMarks, corridorMembershipStamp, myFeetDP, targetFeetDP, candidate.identity, priority, crowd.crowdID, &navigationEarliestDeliveryTick ) ) {
			navigationQueryState = NavigationQueryState::Pending;
			return PathComputeResult::Pending;
		}
		SVG_Nav_SetQueryTerminalState( NavigationQueryState::ResourceLimited );
		return PathComputeResult::Failed;
	}

	const auto findCorridor = [&]( const int32_t fromFace ) {
		if ( hasSnapshotCost ) {
			return Nav_FindPathSnapshotCostsMainThread( fromFace, goalFace, candidateNavPath, pathPolicy,
				snapshotCost, corridorMembershipMarks, corridorMembershipStamp, &myFeetDP, &targetFeetDP, &graphWorkspace, &candidate );
		}
		// Explicit live-policy fallback copies its terminal graph diagnostics before any unrelated query.
		const bool found = Nav_FindPath( fromFace, goalFace, candidateNavPath, pathPolicy );
		candidate.diagnostics = Nav_CopyLastPathDiagnosticsMainThread();
		return found;
	};
	bool pathFound = findCorridor( startFace );
	if ( !pathFound ) {
		const int32_t altStartFace = Nav_FindReachableFaceInLeaf( myFeetDP, goalFace, agentRadius );
		if ( altStartFace != -1 && altStartFace != startFace ) {
			startFace = altStartFace;
			pathFound = findCorridor( startFace );
		}
	}

	// Both execution modes finish through the identical native validation and installation phase.
	return SVG_Nav_InstallCorridor( candidate, myFeetDP, targetFeetDP, target, pathPolicy, static_cast<int32_t>( reqReason ) );
}

/**
*	@brief	Validate and install an owned graph corridor through the shared native owner phase.
*	@param	candidate	Pointer-free graph reply; transport storage must already be released.
*	@return	NewPathGenerated only after current identity and complete native route proof.
*	@note	Current feet are used for attachment; the exact requested goal is preserved.
**/
svg_monster_base_t::PathComputeResult svg_monster_base_t::SVG_Nav_InstallCorridor( svg_nav_result_t &candidate,
	const Vector3DP &myFeetDP, const Vector3DP &targetFeetDP, const Vector3 &target,
	const nav_path_policy_t &pathPolicy, const int32_t reason ) {
	const double agentRadius = pathPolicy.agent_radius;
	const double agentRadiusSqr = agentRadius * agentRadius;
	auto &candidateNavPath = candidate.corridor;
	/**
	*	Reject obsolete ownership before native collision work or callbacks.
	**/
	if ( !SVG_Nav_IsIdentityCurrent( candidate.identity ) || candidate.identity.sequence != navigationRequestSequence ) {
		return PathComputeResult::Failed;
	}
	if ( !candidateNavPath.empty() ) {
		auto &candidateWaypoints = candidate.waypoints;
		auto &candidateWaypointForced = candidate.forced;
		auto &candidateWaypointRecords = candidate.records;
		const bool funnelOk = Nav_StringPull( candidateNavPath, myFeetDP, targetFeetDP, agentRadius, candidateWaypoints, &candidateWaypointForced, this->mins, this->maxs, static_cast<int32_t>( SVG_MMove_GetNativeShape( this ) ), &candidateWaypointRecords, &pathPolicy, &candidate.validation, this, &candidate.diagnostics, &navigationRouteContext );

		// Issue before the final identity gate so namespace rollover cannot invalidate an already installed reply.
		const uint64_t candidateRouteRevision = funnelOk ? Nav_IssueIdentityRevisionMainThread() : 0;
		if ( funnelOk && !candidateWaypoints.empty() && SVG_Nav_IsIdentityCurrent( candidate.identity ) &&
			 candidate.identity.sequence == navigationRequestSequence ) {
			// Commit diagnostics and geometry together only after complete native validation.
			lastRouteValidation = candidate.validation;
			navigationDiagnostics = candidate.diagnostics;
			navigationAcceptedIdentity = candidate.identity;
			navigationCommittedPolicy = pathPolicy;
			navigationQueryState = NavigationQueryState::Ready;
			navPath = std::move( candidateNavPath );
			stringPulledPath = std::move( candidateWaypoints );
			navigationWaypoints = std::move( candidateWaypointRecords );
			navigationRouteRevision = candidateRouteRevision;
			navigationAcceptedGoalRevision = navigationGoalRevision;
			waypointFacesMeshGeneration = g_nav_mesh_generation;
			waypointTopologyRevision = g_nav_topology_revision;
			finalWaypointEventFired = false;
			navigationCommandRejected = false;
			pathPos = 0;

			pathNavigationState.lastGoal.origin = target;
			pathNavigationState.lastGoal.isValid = true;
			lastPathCalcTime = level.time;
			// Count real native installations in either mode before reentrant waypoint callbacks.
			Nav_ActorBenchmarkRecordCommitMainThread( this );

			/**
			*	Record successful owner geometry installation before any waypoint callback can reset timing state.
			**/
			if ( navigationTimingSequence == candidate.identity.sequence && navigationTimingEpoch == candidate.identity.epoch &&
				 navigationTerminalCountedSequence != navigationTimingSequence ) {
				const uint64_t completedNs = SVG_Nav_SteadyTimeNs();
				// Close this request before callbacks or later route cancellation can add a terminal failure.
				navigationTerminalCountedSequence = navigationTimingSequence;
				Nav_Perf_RecordWorkerCommit( navigationSubmittedNs, navigationGraphStartNs, navigationGraphFinishNs, completedNs );
			}

			/**
			*	Choose W1 first; proximity proposes the same full-hull release used at runtime.
			**/
			stringPathPos = stringPulledPath.size() >= 2 ? 1 : 0;
			// A nearby W1 may be released only if its outgoing continuation is safe.
			if ( stringPulledPath.size() >= 3 &&
				 SVG_Nav_CanAdvanceWaypoint( 1, QM_Vector3Distance2DSqrDP( myFeetDP, stringPulledPath[ 1 ] ) <= agentRadiusSqr ) ) {
				stringPathPos = 2;
				const uint64_t committedRevision = navigationRouteRevision;
				SVG_Nav_FireWaypoint( 1 );
				// A callback may cancel or replace the just-installed route; do not authorize stale steering afterward.
				if ( navigationRouteRevision != committedRevision || stringPulledPath.empty() ) {
					return PathComputeResult::Failed;
				}
			}
			Nav_Perf_CountRequest( static_cast<nav_perf_request_reason_t>( reason ), NAV_PERF_OUTCOME_NEW_PATH );
			return PathComputeResult::NewPathGenerated;
		}
	}

	// Preserve failed-candidate evidence without altering the committed route buffers.
	lastRouteValidation = candidate.validation;
	// Replan failed or funnel rejected: retain committed route if valid; record failure debounce.
	pathNavigationState.lastGoal.origin = target;
	pathNavigationState.lastGoal.isValid = false;
	lastPathCalcTime = level.time;
	Nav_Perf_CountRequest( static_cast<nav_perf_request_reason_t>( reason ), NAV_PERF_OUTCOME_FAILED_NO_ROUTE );
	return PathComputeResult::Failed;
}

/**
*	@brief	Read mandatory ownership from the accepted semantic route rather than parallel flags.
*	@param	index	Waypoint index.
*	@return	True only for a retained mandatory obligation.
**/
bool svg_monster_base_t::SVG_Nav_IsWaypointMandatory( const size_t index ) const {
	// Missing metadata fails closed in release validation; drawing may still show a legacy position.
	return index < navigationWaypoints.size() && Nav_WaypointIsMandatory( navigationWaypoints[ index ] );
}

/**
*	@brief	Dispatch an accepted waypoint event exactly once per route revision.
*	@param	index	Committed reached point, never a tentative release candidate.
**/
void svg_monster_base_t::SVG_Nav_FireWaypoint( const size_t index ) {
	/**
	*	Reject missing or already dispatched obligations before a gameplay callback can mutate state.
	**/
	// Old geometry may guide pending movement, but obsolete goals cannot dispatch gameplay events.
	if ( navigationAcceptedGoalRevision != navigationGoalRevision || !SVG_Nav_IsIdentityCurrent( navigationAcceptedIdentity ) ||
		 index >= navigationWaypoints.size() || navigationWaypoints[ index ].callback_fired ) {
		return;
	}
	// Mark ownership first so callback re-entry cannot duplicate the event.
	navigationWaypoints[ index ].callback_fired = true;
	const Vector3DP position = navigationWaypoints[ index ].feet;
	const bool final = ( navigationWaypoints[ index ].flags & NAV_WAYPOINT_FINAL_GOAL ) != 0;
	// Realized current-goal arrival terminates a pending same-goal refresh before it can reinstall a completed route.
	if ( final ) {
		// A pending same-goal refresh ends without a second commit when realized arrival wins first.
		if ( navigationQueryState == NavigationQueryState::Pending ) {
			SVG_Nav_SetQueryTerminalState( NavigationQueryState::Cancelled );
		}
		Nav_CancelQueryWorkerEntityMainThread( s.number );
		navigationQueryState = NavigationQueryState::Ready;
	}

	// Record ownership before callbacks can reset or replace the route.
	if ( navigationEventFrame != static_cast<uint64_t>( level.frameNumber ) ) {
		navigationEventFrame = static_cast<uint64_t>( level.frameNumber );
		navigationEventCount = 0;
	}
	Q_assert( navigationEventCount < navigationEventRoutes.size() );
	if ( navigationEventCount < navigationEventRoutes.size() ) {
		navigationEventRoutes[ navigationEventCount ] = navigationRouteRevision;
		navigationEventIndices[ navigationEventCount ] = static_cast<uint32_t>( index );
		navigationEventFinal[ navigationEventCount++ ] = final;
	}
	OnWaypointReached( index, position, final );
}

/**
*	@brief	Use the same safe continuation contract for initial and runtime waypoint release.
*	@param	index	Intermediate waypoint being proposed for release.
*	@param	progressEligible	Radius/plane progress proposal.
*	@return	Owned rejection reason and witnesses, or clear when full native continuation permits release.
*	@note	Semantic obligations remain owned by the current route until a successful release is committed.
**/
svg_nav_release_t svg_monster_base_t::SVG_Nav_QueryWaypointRelease( const size_t index, const bool progressEligible ) {
	/**
	*	Reject stale geometry, unsupported profiles and exhausted budget before collision work.
	**/
	svg_nav_release_t decision;
	decision.index = static_cast<uint32_t>( index );
	decision.route = navigationRouteRevision;
	decision.goal = navigationGoalRevision;
	if ( !progressEligible ) {
		return decision;
	}
	if ( index == 0 || index + 1 >= stringPulledPath.size() || navigationWaypoints.size() != stringPulledPath.size() ) {
		decision.reason = svg_nav_release_reason_t::invalid_index;
		return decision;
	}
	if ( !SVG_Nav_IsCommittedRouteUsable() || waypointFacesMeshGeneration != g_nav_mesh_generation ||
		 waypointTopologyRevision != g_nav_topology_revision ) {
		decision.reason = svg_nav_release_reason_t::stale_route;
		return decision;
	}
	if ( !Nav_IsSupportedNativeHull( mins, maxs ) ) {
		decision.reason = svg_nav_release_reason_t::unsupported_hull;
		return decision;
	}
	if ( navigationReleaseFrame == static_cast<uint64_t>( level.frameNumber ) && navigationReleasesThisFrame >= 2 ) {
		decision.reason = svg_nav_release_reason_t::budget_exhausted;
		return decision;
	}
	const Vector3DP feet = SVG_GetEntityFeetOriginDP( this );
	const auto &record = navigationWaypoints[ index ];
	const Vector3DP &waypoint = record.feet;
	const Vector3DP &next = navigationWaypoints[ index + 1 ].feet;
	decision.turn_dot = QM_Vector3DotProductDP( record.incoming, record.outgoing );
	decision.plane_distance = QM_Vector3DotProductDP( feet - waypoint, record.incoming );
	/**
	*	A flat release needs closed full-radius clearance; elevations use actual local native continuation.
	**/
	const int32_t face = ResolveCurrentFace( feet );
	const bool flat = std::fabs( next.z - feet.z ) <= NAV_MOVEMENT_ENDPOINT_TOLERANCE &&
		face >= 0 && static_cast<size_t>( face ) < g_nav_faces.size() && g_nav_faces[ face ].normal.z >= 0.999 &&
		( g_nav_faces[ face ].room_id < 0 || static_cast<size_t>( g_nav_faces[ face ].room_id ) >= g_nav_rooms.size() ||
		  g_nav_rooms[ g_nav_faces[ face ].room_id ].zone_type != ZONE_TYPE_CORRIDOR_STAIRS );
	/**
	*	A ramp corner cannot use a short outgoing probe to certify a distant wall-free chord.
	*	Reach its incoming plane before changing direction; the installed full route already proves
	*	continuation from this constraint. Door and riser obligations keep their own release rules.
	**/
	if ( !flat && ( record.flags & NAV_WAYPOINT_CORNER_APEX ) != 0 &&
		 ( record.flags & ( NAV_WAYPOINT_STEP_RISER | NAV_WAYPOINT_DOOR_PORTAL ) ) == 0 &&
		 decision.plane_distance < -NAV_MOVEMENT_ENDPOINT_TOLERANCE ) {
		decision.reason = svg_nav_release_reason_t::elevation_pending;
		return decision;
	}
	if ( flat ) {
		decision.geometric = Nav_TestSegmentClearance( feet, next, static_cast<double>( maxs.x ) );
		lastGeometricValidation = decision.geometric;
		if ( decision.geometric.reason != nav_clearance_reason_t::clear ) {
			decision.reason = svg_nav_release_reason_t::geometric_rejection;
			// An exactly reached but impossible link must request replacement instead of shuffling forever.
			if ( decision.geometric.reason == nav_clearance_reason_t::boundary_contact &&
				 QM_Vector3Distance2DSqrDP( feet, waypoint ) <= MONSTER_NAV_WAYPOINT_DEADBAND * MONSTER_NAV_WAYPOINT_DEADBAND ) {
				navigationCommandRejected = true;
			}
			return decision;
		}
	}
	/**
	*	A one-tick preview cannot release a corner whose later outgoing chord enters a native brush envelope.
	*	Static flat continuation excludes actors; live actor precedence is tested by the local preview below.
	**/
	if ( flat ) {
		decision.native = SVG_Nav_TestMovementHull( feet, next, mins, maxs, SVG_MMove_GetNativeShape( this ),
			pathNavigationState.policy, this, static_cast<cm_contents_t>( CONTENTS_SOLID | CONTENTS_MONSTERCLIP ), false );
		navigationValidationTraceCount += decision.native.trace_count;
		// Retain the current obligation until its complete native outgoing chord is physically legal.
		if ( decision.native.reason != svg_nav_movement_reason_t::clear ) {
			lastMovementValidation = decision.native;
			decision.reason = svg_nav_release_reason_t::native_rejection;
			return decision;
		}
	}
	if ( Nav_WaypointIsMandatory( record ) && waypoint.z - feet.z > MONSTER_NAV_VERTICAL_STEP_TOLERANCE && !flat ) {
		decision.reason = svg_nav_release_reason_t::elevation_pending;
		return decision;
	}
	/**
	*	Use a bounded horizontal command like live movement; native landing owns ramp/step support height.
	**/
	Vector3DP continuation = next - feet;
	continuation.z = 0.0;
	const double length = QM_Vector3LengthDP( continuation );
	const double previewLength = std::min( length, 220.0 * FRAME_TIME_S.Seconds() );
	const Vector3DP preview = length > 0.001 ? feet + continuation * ( previewLength / length ) : feet;
	decision.native = SVG_Nav_TestMovementCandidate( this, feet, preview, pathNavigationState.policy, false );
	lastMovementValidation = decision.native;
	navigationValidationTraceCount += decision.native.trace_count;
	decision.reason = decision.native.reason == svg_nav_movement_reason_t::clear ?
		svg_nav_release_reason_t::clear : svg_nav_release_reason_t::native_rejection;
	return decision;
}

/**
*	@brief	Consume one successful shared release inside the total initial/runtime frame budget.
*	@param	index	Intermediate constraint proposed for commit.
*	@param	progressEligible	Radius/plane progress proposal.
*	@return	True only for a complete authoritative release decision.
**/
bool svg_monster_base_t::SVG_Nav_CanAdvanceWaypoint( const size_t index, const bool progressEligible ) {
	/**
	*	Reset the two-release budget once per server frame, including route-installation advancement.
	**/
	const uint64_t frame = static_cast<uint64_t>( level.frameNumber );
	if ( navigationReleaseFrame != frame ) {
		navigationReleaseFrame = frame;
		navigationReleasesThisFrame = 0;
	}
	lastWaypointRelease = SVG_Nav_QueryWaypointRelease( index, progressEligible );
	// Rejected proposals neither consume progress nor invoke callbacks.
	if ( lastWaypointRelease.reason != svg_nav_release_reason_t::clear ) {
		return false;
	}
	++navigationReleasesThisFrame;
	return true;
}

/**
*	@brief	Fire the route's final callback from realized supported arrival before crowd completion removes its thinker.
*	@note	The event is owned once per accepted route; pending intermediate points and obsolete goals cannot satisfy it.
**/
void svg_monster_base_t::SVG_Nav_CheckFinalArrival() {
	/**
	*	Arrival never substitutes for missing geometry, stale identities or an unresolved intermediate constraint.
	**/
	if ( finalWaypointEventFired || stringPulledPath.empty() || stringPathPos + 1 < stringPulledPath.size() ||
		 !SVG_Nav_IsIdentityCurrent( navigationAcceptedIdentity ) || navigationAcceptedGoalRevision != navigationGoalRevision || waypointFacesMeshGeneration != g_nav_mesh_generation ||
		 waypointTopologyRevision != g_nav_topology_revision || groundInfo.entityNumber != 0 ) {
		return;
	}
	const Vector3DP feet = SVG_GetEntityFeetOriginDP( this );
	const Vector3DP goal = stringPulledPath.back();
	const double radius = GetNavigationArrivalRadius();
	// Only realized feet on the goal floor and inside the shared radius authorize the callback.
	if ( QM_Vector3Distance2DSqrDP( feet, goal ) <= radius * radius && std::fabs( feet.z - goal.z ) <= MONSTER_NAV_FINAL_GOAL_MAX_Z_DELTA ) {
		finalWaypointEventFired = true;
		SVG_Nav_FireWaypoint( stringPulledPath.size() - 1 );
	}
}

/**
*	@brief	Compute one shared physical arrival threshold for path following and crowd station keeping.
*	@return	The current order's unchanged queue/slot tolerance, capped by solo arrival distance.
*	@note	A small actor cannot stop at a larger path radius and remain permanently unarrived in its crowd.
**/
double svg_monster_base_t::GetNavigationArrivalRadius() const {
	/**
	*	Ordinary solo navigation and serialized staging each have an explicit stable arrival contract.
	**/
	const auto *group = crowd.crowdID >= 0 ? SVG_Crowd_GetGroup( crowd.crowdID ) : nullptr;
	if ( group == nullptr ) {
		return MONSTER_NAV_WAYPOINT_REACH_RADIUS;
	}
	if ( group->hasSerializedIngress ) {
		return CROWD_INGRESS_ARRIVAL_RADIUS;
	}
	/**
	*	Match slot spacing and native hull radius without allowing path following to stop outside station keeping.
	**/
	const auto &params = group->params;
	const double configured = params.arrivalRadius > 0.0 ? params.arrivalRadius : CROWD_DEFAULT_ARRIVAL_RADIUS;
	const double lateral = params.lateralSpacing > 0.0 ? params.lateralSpacing : CROWD_DEFAULT_LATERAL_SPACING;
	const double longitudinal = params.longitudinalSpacing > 0.0 ? params.longitudinalSpacing : CROWD_DEFAULT_LONGITUDINAL_SPACING;
	const double minimumSpacing = params.minCorridorSpacing > 0.0 ? params.minCorridorSpacing : CROWD_DEFAULT_MIN_CORRIDOR_SPACING;
	const double spacing = std::max( minimumSpacing, std::min( lateral, longitudinal ) );
	const double radius = ( maxs.x - mins.x ) * 0.5;
	const double floor = std::min( configured, radius * 0.75 );
	return std::min( MONSTER_NAV_WAYPOINT_REACH_RADIUS, std::max( floor, std::min( configured, std::min( spacing * 0.35, radius * 1.25 ) ) ) );
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

	/**
	*	Do not steer a stale route or unsupported hull while a debounced replacement is pending.
	**/
	if ( !Nav_IsSupportedNativeHull( mins, maxs ) || !SVG_Nav_IsCommittedRouteUsable() || ( !stringPulledPath.empty() &&
		 ( waypointFacesMeshGeneration != g_nav_mesh_generation || waypointTopologyRevision != g_nav_topology_revision ) ) ) {
		navigationCommandRejected = true;
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
	const int32_t currentFace = ResolveCurrentFace( myFeetDP );
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
		int32_t wpFace = -1;
		if ( waypointFacesMeshGeneration == g_nav_mesh_generation && stringPathPos < navigationWaypoints.size() ) {
			wpFace = navigationWaypoints[ stringPathPos ].face;
		}
		if ( wpFace < 0 ) {
			wpFace = Nav_FindClosestFaceInLeaf( stringPulledPath[ stringPathPos ] );
			if ( waypointFacesMeshGeneration == g_nav_mesh_generation && stringPathPos < navigationWaypoints.size() ) {
				navigationWaypoints[ stringPathPos ].face = wpFace;
			}
		}
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
	*	Bounded to at most 2 waypoint advancements per frame; each strict boundary scan remains O(E).
	**/
	int32_t advancedWaypointsCount = 0;
	while ( stringPathPos < stringPulledPath.size() - 1 && advancedWaypointsCount < 2 ) {
		const Vector3DP currentWp = stringPulledPath[ stringPathPos ];
		Vector3DP toWp = currentWp - myFeetDP;
		const double zDiff = toWp.z;
		toWp.z = 0.0;
		const double dist2DSqr = QM_Vector3DotProductDP( toWp, toWp );

		// Check if current waypoint is a forced constraint (corner standoff, stair crossing)
		const bool isForcedWp = ( SVG_Nav_IsWaypointMandatory( stringPathPos ) );

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

		/**
		*	Radius and approach-plane tests are proposals; one contract commits all releases.
		**/
		if ( !SVG_Nav_CanAdvanceWaypoint( stringPathPos, withinRadius || passedPlane ) ) {
			// Keep the active constraint when either progress or safe continuation is missing.
			break;
		}
		const size_t reachedIdx = stringPathPos;
		++stringPathPos;
		++advancedWaypointsCount;
		const uint64_t releasedRoute = navigationRouteRevision;
		SVG_Nav_FireWaypoint( reachedIdx );
		// Gameplay callbacks may reset or replace navigation; stop this computation before dereferencing old geometry.
		if ( navigationRouteRevision != releasedRoute || stringPulledPath.empty() ) {
			return false;
		}
	}

	// Final goal arrival check
	if ( stringPathPos >= stringPulledPath.size() - 1 ) {
		const Vector3DP finalGoalWp = stringPulledPath.back();
		Vector3DP toFinal = finalGoalWp - myFeetDP;
		const double finalZDiff = toFinal.z;
		toFinal.z = 0.0;
		// The low-level path follower must not stop outside the crowd's tighter
		// staging tolerance, or station keeping and doorway admission can never agree.
		const double finalArrivalRadius = GetNavigationArrivalRadius();
		if ( QM_Vector3DotProductDP( toFinal, toFinal ) <= finalArrivalRadius * finalArrivalRadius && std::fabs( finalZDiff ) <= MONSTER_NAV_FINAL_GOAL_MAX_Z_DELTA ) {
			// A route that remains installed after arrival must not refire its final event every tick.
			if ( !finalWaypointEventFired && navigationAcceptedGoalRevision == navigationGoalRevision ) {
				finalWaypointEventFired = true;
				SVG_Nav_FireWaypoint( stringPulledPath.size() - 1 );
			}
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

	navigationSteering.raw = distToTarget > 0.001 ? toTarget * ( 1.0 / distToTarget ) : Vector3DP{};
	Vector3DP steerTarget = targetWp;

	const bool isTargetForced = ( SVG_Nav_IsWaypointMandatory( k ) );
	if ( navigationFixtureMode != 2 && !isTargetForced && distToTarget < MONSTER_NAV_LOOKAHEAD_DISTANCE && k + 1 < stringPulledPath.size() ) {
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
				if ( Nav_TestSegmentClearance( myFeetDP, blendedTarget, agentRadius ).reason == nav_clearance_reason_t::clear ) {
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
	navigationSteering.lookahead = steerDist > 0.001 ? toSteer * ( 1.0 / steerDist ) : Vector3DP{};

	// Precompute normalized direction vector corresponding to current ideal_yaw as fallback:
	const double yawRad = static_cast<double>( ideal_yaw ) * ( QM_PI / 180.0 );
	const Vector3DP forwardYawDir( std::cos( yawRad ), std::sin( yawRad ), 0.0 );

	// Identify whether the entity is at a forced step-up riser actively waiting for feet elevation:
	const bool awaitingStepUp = ( isTargetForced && !onRamp && ( targetWp.z - myFeetDP.z ) > MONSTER_NAV_STEP_MIN_DELTA );

	/**
	*	Steering remains attached to the active obligation until the shared release loop commits it.
	*	An unbounded departure-plane shortcut could steer hundreds of units toward W(k+1)
	*	without advancing k, then oscillate around the old point on a slope. Optional lookahead
	*	remains local; its eventual release uses the same bounded progress/native contract.
	**/
	// Case 2: At a step-up riser, actively awaiting vertical elevation.
	// Condition check: Entity is at or past the riser boundary along the stair flight ascending direction.
	// Rather than steering backward or falling into yaw-lock singularities, drive strictly forward
	// across the riser along the flight ascent direction so StepSlideMove can step up.
	if ( awaitingStepUp ) {
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
	else if ( isTargetForced || navigationFixtureMode == 2 ) {
		*outMoveDir = steerDist > 0.001 ? toSteer * ( 1.0 / steerDist ) : forwardYawDir;
	}
	// Case 3: Outside proximity deadband â€” steer towards lookahead target with corner transit bisector protection.
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

		navigationSteering.transit = candDir;
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

		navigationSteering.monotonic = candDir;
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
	navigationSteering.deadband = *outMoveDir;
	*outSpeedScale = speedScale;

	return true;
}

/**
*	@brief	Choose collision-validated navigation velocity and animation for the requested goal.
*	@param	goalOrigin	Target world-space feet position.
*	@return	True for a handled movement/hold command; false when arrival or unavailable steering stops the behavior.
*	@note	Authoritative physics executes later; this phase records selection and native preview costs.
**/
const bool svg_monster_base_t::StepMoveToGoal( const Vector3 &goalOrigin ) {
	/**
	*	Measure the complete steering/animation decision, including final command validation.
	**/
	nav_perf_scope_t movementScope( NAV_PERF_PHASE_ACTOR_STEERING );
	// Copy causal identity before any release or yaw change; zeroed stages distinguish unevaluated early returns.
	navigationSteering = {};
	navigationSteering.old_index = static_cast<uint32_t>( stringPathPos );
	navigationSteering.yaw_before = currentAngles[ YAW ];
	navigationSteering.fixture_scale = navigationFixtureSpeedScale;
	// Clear request-local diagnostic state before steering/release/final-candidate evaluation.
	navigationValidationTraceCount = 0;
	lastMovementValidation = {};
	lastGeometricValidation = {};
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
	*	Keep the approach owner stationary until its current follower physically vacates the reserved transit.
	**/
	if ( SVG_Crowd_IsWaitingForApproachYield( this ) ) {
		// Preserve route progress and admission ownership; ordinary physics still maintains native support.
		velocity.x = velocity.y = 0.0f;
		monsterMove.state.velocity.x = monsterMove.state.velocity.y = 0.0;
		UpdateAnim( 1 );
		return true;
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
	double speedScale = 1.0;

	// Explicit geometry-only observation holds the actor without mutating its accepted route.
	if ( navigationFixtureMode == 1 ) {
		velocity.x = velocity.y = 0.0f;
		monsterMove.state.velocity.x = monsterMove.state.velocity.y = 0.0;
		return true;
	}
	// Unavailable safe steering holds the actor and reports a stopped command without integrating physics.
	if ( !ComputePathSteering( goalOriginDP, &moveDirDP, &speedScale ) ) {
		velocity.x = velocity.y = 0.0f;
		monsterMove.state.velocity.x = velocity.x;
		monsterMove.state.velocity.y = velocity.y;
		UpdateAnim( 1 ); // IDLE
		return false;
	}

	navigationSteering.turn_scale = speedScale;
	navigationSteering.squad_scale = squadSpeedScale;
	// Combine squad pacing with the steering turn factor; steering never overwrites squad pacing.
	speedScale *= static_cast<double>( squadSpeedScale ) * navigationFixtureSpeedScale;

	/**
	*	Blend mutual crowd separation force and compute teammate following speed throttling:
	**/
	if ( this->crowd.crowdID >= 0 && navigationFixtureMode != 2 && navigationFixtureMode != 3 ) {
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
			navigationSteering.follow_scale = followScale;
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
						// Keep the separation contribution as a candidate; validate after all speed/reservation changes.
						moveDirDP = candDir;
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
						navigationSteering.catchup_scale = CROWD_CATCHUP_SPEED_SCALE;
						speedScale *= CROWD_CATCHUP_SPEED_SCALE;
					} else if ( myDistToDest < ( leaderDistToDest - CROWD_SLOT_FOLLOW_TOLERANCE ) ) {
						// Surging ahead of the moving leader: throttle back to stay in rank
						navigationSteering.catchup_scale = CROWD_SLOWDOWN_SPEED_SCALE;
						speedScale *= CROWD_SLOWDOWN_SPEED_SCALE;
					}
				}
			}
		}
	}

	navigationSteering.separation = moveDirDP;
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
	// The current queue head owns the march through vacated staging cells before admission.
	const bool isStagingHead = SVG_Crowd_IsCurrentIngressHead( this );
	// A claimed holding mover must vacate the approach even if a later actor enters its previously clear displacement.
	const bool isHoldingMover = SVG_Crowd_IsCurrentApproachYieldMember( this );
	/**
	*	Project each finite aperture with the same strict storage margin and native speed ceiling.
	*	Negating the outward exit normal expresses cleared departure as the same exterior half-space contract.
	**/
	bool reservationAdjusted = false;
	const auto projectAperture = [&]( Vector3DP candidateVelocity, const Vector3DP &portalOrigin,
		const Vector3DP &portalInward, const double stopDepth, const double reservedHalfSpan ) {
		const Vector3DP currentFeet = SVG_GetEntityFeetOriginDP( this );
		const double currentDepth = QM_Vector3DotProductDP( currentFeet - portalOrigin, portalInward );
		const double frameSeconds = FRAME_TIME_S.Seconds();
		const double inwardSpeed = QM_Vector3DotProductDP( candidateVelocity, portalInward );
		const double predictedDepth = currentDepth + ( inwardSpeed * frameSeconds );
		// Reserve rounding before validation even when the ideal double endpoint only reaches the literal stop plane.
		const Vector3DP predictedFeet = currentFeet + candidateVelocity * frameSeconds;
		const double storageMargin = Nav_ReservationStorageMargin( predictedFeet, portalInward );
		const Vector3DP portalTangent{ portalInward.y, -portalInward.x, 0.0 };
		const double currentLateral = QM_Vector3DotProductDP(
			currentFeet - portalOrigin, portalTangent );
		const double lateralSpeed = QM_Vector3DotProductDP( candidateVelocity, portalTangent );
		bool crossesReservedAperture = false;

		// Evaluate the continuous velocity segment at its stop-plane intersection to prevent diagonal tunnelling.
		if ( frameSeconds > 0.0 && inwardSpeed > 0.0 && currentDepth <= stopDepth && predictedDepth > stopDepth - storageMargin ) {
			const double crossingFraction = std::clamp(
				( stopDepth - currentDepth ) / ( predictedDepth - currentDepth ), 0.0, 1.0 );
			const double crossingLateral = currentLateral +
				( lateralSpeed * frameSeconds * crossingFraction );
			crossesReservedAperture = std::fabs( crossingLateral ) <= reservedHalfSpan;
		}

		// Limit only the inward speed that would cross the reserved finite aperture this frame.
		if ( crossesReservedAperture ) {
			// Reserve the actual entity storage error before native validation, keeping the final float endpoint outside.

			const double maximumInwardSpeed = std::clamp(
				( stopDepth - storageMargin - currentDepth ) / frameSeconds, -frameVelocity, frameVelocity );
			if ( inwardSpeed > maximumInwardSpeed ) {
				// Record actual half-space clipping independently of harmless speed-ceiling rounding.
				reservationAdjusted = true;
				candidateVelocity = candidateVelocity -
					( portalInward * ( inwardSpeed - maximumInwardSpeed ) );
			}
		}

		// Preserve the configured speed ceiling after the half-space projection rotates the velocity vector.
		const double constrainedSpeed = std::sqrt( ( candidateVelocity.x * candidateVelocity.x ) + ( candidateVelocity.y * candidateVelocity.y ) );
		if ( constrainedSpeed > frameVelocity && constrainedSpeed > 0.001 ) {
			candidateVelocity = candidateVelocity * ( frameVelocity / constrainedSpeed );
		}
		return candidateVelocity;
	};
	const auto projectReservation = [&]( Vector3DP candidateVelocity ) {
		// Unadmitted actors retain the destination doorway's original finite reservation.
		if ( ingressGroup != nullptr && ingressGroup->hasSerializedIngress && !crowd.ingressReleased ) {
			candidateVelocity = projectAperture( candidateVelocity, ingressGroup->ingressPortalOrigin,
				ingressGroup->ingressPortalInward, -ingressGroup->ingressReleaseDepth,
				ingressGroup->ingressPortalHalfWidth + ingressGroup->ingressReleaseDepth );
		}
		// A cleared pool/navigation lifetime cannot undo departure while other origin owners still need its opening.
		if ( ingressGroup != nullptr && SVG_Crowd_HasClearedEgress( this ) ) {
			const double radius = std::max( { std::fabs( mins.x ), std::fabs( maxs.x ), std::fabs( mins.y ), std::fabs( maxs.y ) } );
			candidateVelocity = projectAperture( candidateVelocity, ingressGroup->egressPortalOrigin,
				ingressGroup->egressPortalOutward * -1.0, -SVG_Crowd_GetEgressClearanceDepth( this ),
				ingressGroup->egressPortalHalfWidth + radius );
		}
		return candidateVelocity;
	};

	/**
	*	Validate actual tick displacement after every steering, speed and reservation contribution.
	*	Try at most four physical candidates: preferred, direct, short/preferred tangent, then opposite tangent.
	**/
	const Vector3DP validationFeet = SVG_GetEntityFeetOriginDP( this );
	const double tickSeconds = FRAME_TIME_S.Seconds();
	Vector3DP directDirection = moveDirDP;
	double constraintDistance = frameVelocity * tickSeconds;
	// Cap travel at the next unresolved waypoint so a fast tick cannot overshoot its obligation.
	if ( !stringPulledPath.empty() && stringPathPos < stringPulledPath.size() ) {
		directDirection = stringPulledPath[ stringPathPos ] - validationFeet;
		directDirection.z = 0.0;
		constraintDistance = QM_Vector3LengthDP( directDirection );
		// At a vertical riser, keep the existing horizontal approach direction.
		if ( constraintDistance > 0.001 ) {
			directDirection = directDirection * ( 1.0 / constraintDistance );
		} else {
			directDirection = moveDirDP;
			constraintDistance = frameVelocity * tickSeconds;
		}
	}
	navigationSteering.constraint_distance = constraintDistance;

	/**
	*	Step transitions require forward momentum to execute Quake 2 physics step-up/step-down
	*	over the vertical riser without decelerating into a standstill deadlock before the edge.
	**/
	const bool isTargetForced = ( !stringPulledPath.empty() && stringPathPos < stringPulledPath.size() && SVG_Nav_IsWaypointMandatory( stringPathPos ) );
	const bool awaitingStepTransition = ( isTargetForced && std::fabs( stringPulledPath[ stringPathPos ].z - validationFeet.z ) > MONSTER_NAV_STEP_MIN_DELTA );
	const double limitedSpeed = tickSeconds > 0.0 ?
		( awaitingStepTransition ? frameVelocity : std::min( frameVelocity, constraintDistance / tickSeconds ) ) : 0.0;
	const Vector3DP candidates[ 3 ] = { moveDirDP * limitedSpeed, directDirection * limitedSpeed, directDirection * ( limitedSpeed * 0.5 ) };
	bool candidateAccepted = false;
	// Retain the opposite of the same first blocker tangent for this owner-thread trial only.
	Vector3DP oppositeTangentCandidate{};
	bool hasOppositeTangent = false;
	int32_t blockingActor = ENTITYNUM_NONE;
	// Current floor classification separates flat-corner clearance from native step/ramp traversal.
	const int32_t validationFace = ResolveCurrentFace( validationFeet );
	const bool flatSurface = !awaitingStepTransition && validationFace >= 0 && static_cast<size_t>( validationFace ) < g_nav_faces.size() &&
		g_nav_faces[ validationFace ].normal.z >= 0.999 && ( g_nav_faces[ validationFace ].room_id < 0 || static_cast<size_t>( g_nav_faces[ validationFace ].room_id ) >= g_nav_rooms.size() ||
		  g_nav_rooms[ g_nav_faces[ validationFace ].room_id ].zone_type != ZONE_TYPE_CORRIDOR_STAIRS );
	// A rejected command remains held through the normal replan cooldown instead of endless short retries.
	if ( !navigationCommandRejected && Nav_IsSupportedNativeHull( mins, maxs ) ) {
		// Every fallback must pass the same unchanged hull, support, wall and reservation checks.
		for ( uint32_t candidateIndex = 0; candidateIndex < navigationSteering.candidates.size(); ++candidateIndex ) {
			// The fourth trial exists only when the original blocker defined an authorized tangent.
			if ( candidateIndex == 3 && !hasOppositeTangent ) {
				break;
			}
			auto &evidence = navigationSteering.candidates[ candidateIndex ];
			navigationSteering.candidate_count = candidateIndex + 1;
			Vector3DP candidate = candidateIndex == 3 ? oppositeTangentCandidate : candidates[ candidateIndex ];
			/**
			*	Preserve the existing native-validated single-actor tangent on flat floors, steps and ramps.
			**/
			if ( candidateIndex == 2 && blockingActor > 0 ) {
				const auto *obstacle = g_edict_pool.EdictForNumber( blockingActor );
				const bool outsideCrowd = obstacle != nullptr && ( crowd.crowdID < 0 || obstacle->crowd.crowdID != crowd.crowdID );
				// A head holding its cell yields approach space without surrendering queue or doorway ownership.
				const bool holdingStagingHead = obstacle != nullptr && SVG_Crowd_IsCurrentIngressHead( obstacle ) &&
					!obstacle->crowd.ingressReleased && obstacle->crowd.reachedGoal;
				// An arrived exact holding owner must not trap later staging traffic while its admission head waits for that traffic.
				const bool settledHoldingObstacle = obstacle != nullptr && obstacle->crowd.reachedGoal &&
					SVG_Crowd_IsCurrentApproachYieldMember( obstacle );
				const bool admittedContention = obstacle != nullptr && !outsideCrowd &&
					( crowd.ingressReleased || obstacle->crowd.ingressReleased || isStagingHead || holdingStagingHead ||
					  isHoldingMover || settledHoldingObstacle );
				// Approach around the stationary head remains subject to the same hull, wall and reservation gates.
				if ( obstacle != nullptr && SVG_Entity_IsActive( obstacle ) && ( outsideCrowd || admittedContention ) &&
					 obstacle->GetTypeInfo()->IsSubClassType<svg_monster_base_t>() ) {
					Vector3DP radial = validationFeet - SVG_GetEntityFeetOriginDP( obstacle );
					radial.z = 0.0;
					const double length = QM_Vector3LengthDP( radial );
					// Coincident hulls cannot define a safe tangent and remain subject to native rejection.
					if ( length > 0.001 ) {
						radial = radial * ( 1.0 / length );
						Vector3DP tangent{ -radial.y, radial.x, 0.0 };
						const double alignment = QM_Vector3DotProductDP( tangent, directDirection );
						// Prefer goal progress; entity parity resolves an exactly head-on symmetric encounter.
						if ( alignment < -0.000001 || ( std::fabs( alignment ) <= 0.000001 && ( s.number & 1 ) != 0 ) ) {
							tangent = tangent * -1.0;
						}
						const Vector3DP avoidance = tangent + radial * 0.25;
						candidate = avoidance * ( limitedSpeed / QM_Vector3LengthDP( avoidance ) );
						// A second peer may block the preferred side; independently validate the other side of this same obstacle.
						const Vector3DP oppositeAvoidance = tangent * -1.0 + radial * 0.25;
						oppositeTangentCandidate = oppositeAvoidance * ( limitedSpeed / QM_Vector3LengthDP( oppositeAvoidance ) );
						hasOppositeTangent = true;
					}
				}
			}
			evidence.proposed = candidate;
			reservationAdjusted = false;
			const Vector3DP projected = projectReservation( candidate );
			evidence.projected = projected;
			const Vector3DP predictedFeet = validationFeet + projected * tickSeconds;
			evidence.endpoint = predictedFeet;
			/**
			*	A reservation-annulled command is waiting for a usable route, not successful cached forward movement.
			**/
			const Vector3 storedPredictedFeet = QM_Vector3FromDP( predictedFeet );
			const Vector3 storedRequestedFeet = QM_Vector3FromDP( validationFeet + candidate * tickSeconds );
			// Compare actual entity storage: meaningful requested XY motion must not become an unchanged stationary endpoint.
			if ( reservationAdjusted &&
				storedPredictedFeet.x == currentOrigin.x && storedPredictedFeet.y == currentOrigin.y &&
				( storedRequestedFeet.x != currentOrigin.x || storedRequestedFeet.y != currentOrigin.y ) ) {
				continue;
			}
			// Projection can reverse an otherwise forward fallback; reject it so the debounced replan can repair the route.
			if ( reservationAdjusted && QM_Vector3DotProductDP( candidate, directDirection ) > 0.0 &&
				QM_Vector3DotProductDP( projected, directDirection ) <= 0.0 ) {
				continue;
			}
			// Flat traversal uses the complete native radius even for a very short displacement.
			// Keep each geometric witness; physical validation remains a distinct, possibly unexecuted phase.
			if ( flatSurface ) {
				lastGeometricValidation = Nav_TestSegmentClearance( validationFeet, predictedFeet, static_cast<double>( maxs.x ) );
				evidence.geometric = lastGeometricValidation;
			}
			if ( flatSurface && lastGeometricValidation.reason != nav_clearance_reason_t::clear ) {
				continue;
			}
			lastMovementValidation = SVG_Nav_TestMovementCandidate( this, validationFeet, predictedFeet, pathNavigationState.policy, false, false );
			evidence.native = lastMovementValidation;
			navigationValidationTraceCount += lastMovementValidation.trace_count;
			// Retain the physical actor witness for the remaining bounded fallback candidate.
			if ( lastMovementValidation.reason != svg_nav_movement_reason_t::clear && lastMovementValidation.blocking_actor > 0 ) {
				blockingActor = lastMovementValidation.blocking_actor;
			}
			// Commit only the exact projected candidate that passed both static and physical validation.
			if ( lastMovementValidation.reason == svg_nav_movement_reason_t::clear ) {
				/**
				*	On an ascending step transition or staircase flight, downward floor traces can probe
				*	past the upper step edge onto lower treads when the hull partially overhangs the riser.
				*	Clamping to the current feet elevation prevents candidate acceptance from pulling the mover down.
				**/
				const bool isAscending = ( !stringPulledPath.empty() && stringPathPos < stringPulledPath.size() &&
					stringPulledPath[ stringPathPos ].z > validationFeet.z - NAV_MOVEMENT_ENDPOINT_TOLERANCE );
				if ( isAscending && lastMovementValidation.bbox_endpoint.z < ( validationFeet.z - mins.z ) - NAV_MOVEMENT_ENDPOINT_TOLERANCE ) {
					lastMovementValidation.bbox_endpoint.z = validationFeet.z - mins.z;
				}

				velocityDP = projected;
				evidence.accepted = true;
				candidateAccepted = true;
				break;
			}
		}
	}
	// Failed candidates hold and request a debounced replan; no unchecked fallback is issued.
	if ( !candidateAccepted ) {
		navigationCommandRejected = true;
		velocityDP = Vector3DP{};
	}

	navigationSteering.final_velocity = velocityDP;
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

	const PathComputeResult result = ComputePathTo( goalOrigin, force );
	/**
	*	Pending without safe installed geometry holds position and remains an active behavior.
	**/
	if ( result == PathComputeResult::Pending && ( stringPulledPath.empty() || !SVG_Nav_IsCommittedRouteUsable() ) ) {
		velocity.x = velocity.y = 0.0f;
		monsterMove.state.velocity.x = monsterMove.state.velocity.y = 0.0;
		UpdateAnim( 1 );
		return true;
	}
	const bool moving = StepMoveToGoal( goalOrigin );
	// Reaching an old endpoint while waiting is a safe hold, never arrival at the new requested goal.
	if ( !moving && result == PathComputeResult::Pending ) {
		velocity.x = velocity.y = 0.0f;
		monsterMove.state.velocity.x = monsterMove.state.velocity.y = 0.0;
		UpdateAnim( 1 );
		return true;
	}
	return moving;
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
*	@brief	Keep unknown derived cost overrides in the owner-only synchronous phase by default.
*	@param	cost	Receives the base corridor preference for an explicitly supporting subclass.
*	@return	False; dynamic classes opt in only after all their behavior is represented.
*	@note	An inherited opt-in could silently erase an unrepresented virtual override, so the base refuses it.
**/
bool svg_monster_base_t::BuildNavigationCostSnapshot( nav_query_cost_policy_t &cost ) const {
	// Define the base scalar preference without authorizing unknown derived implementations.
	cost = {};
	cost.corridor_multiplier = 0.85;
	return false;
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
	( void )fromFaceIdx;
	( void )he;
	double cost = baseCost;

	/**
	*	Corridor Hysteresis:
	*	If toFaceIdx is part of our currently committed navPath corridor ahead of our position,
	*	apply a 15% discount (cost *= 0.85). This decisively breaks equal-cost ties across
	*	bifurcation manifolds (e.g. around obstacles or split stairways) and permanently prevents
	*	high-frequency route flip-flopping.
	**/
	if ( toFaceIdx >= 0 && static_cast<size_t>( toFaceIdx ) < corridorMembershipMarks.size() ) {
		if ( corridorMembershipMarks[ toFaceIdx ] == corridorMembershipStamp ) {
			cost *= 0.85;
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
	const svg_crowd_group_t *crowdGroup = SVG_Crowd_GetGroup( this->crowd.crowdID );
	// The active crowd order already supplies following/admission pacing; a second squad token can deadlock it.
	if ( crowdGroup != nullptr && crowdGroup->isMoving ) {
		return false;
	}
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
