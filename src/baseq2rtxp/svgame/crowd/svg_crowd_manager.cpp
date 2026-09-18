/********************************************************************
*
*
*	ServerGame: Crowd & Crew Navigation Manager
*	File: svg_crowd_manager.cpp
*	Description:
*		Central manager for tracking crowd groups, allocating formation
*		slots, assigning tactical cover, dispatching A* navigation routes,
*		and managing crowd lifecycle using Vector3DP.
*
*
********************************************************************/
#include "svgame/crowd/svg_crowd_manager.h"
#include "svgame/crowd/svg_crowd_formations.h"
#include "svgame/crowd/svg_squad_coordinator.h"
#include "svgame/nav/nav_sector_graph.h"
#include "svgame/entities/svg_base_edict.h"
#include "svgame/entities/monster/svg_monster_base.h"
#include "svgame/entities/monster/svg_monster_testdummy_debug.h"
#include "svgame/svg_edict_pool.h"
#include "svgame/nav/nav_debug_draw.h"
#include "svgame/nav/nav_cover_query.h"
#include "svgame/nav/nav_path.h"
#include "svgame/nav/nav_types.h"
#include "svgame/svg_utils.h"

#include "shared/math/qm_math_cpp.h"
#include "shared/math/qm_vector3_dp.h"
#include "shared/util/util_endian.h"
#include "svgame/nav/nav_generate.h"
#include "svgame/nav/nav_cover_types.h"
#include "svgame/monsters/svg_mmove.h"


//! Minimum mutual separation distance between two agents abreast before the lower-priority agent yields.
//! Corresponds to three agent radii (48.0 units: 2 * 16.0 hull + 16.0 margin).
static constexpr double CROWD_ABREAST_DECONFLICT_DIST = CROWD_DEFAULT_AGENT_RADIUS * 3.0;
static constexpr double CROWD_ABREAST_DECONFLICT_DIST_SQR = CROWD_ABREAST_DECONFLICT_DIST * CROWD_ABREAST_DECONFLICT_DIST;

//! Minimum incoming movement speed required before applying head-on opposing course right-of-way arbitration.
static constexpr double CROWD_HEADON_MIN_SPEED = 4.0;
static constexpr double CROWD_HEADON_MIN_SPEED_SQR = CROWD_HEADON_MIN_SPEED * CROWD_HEADON_MIN_SPEED;

//! Opposing directional course dot threshold (cos 102 degrees) for head-on collision arbitration.
static constexpr double CROWD_HEADON_COURSE_DOT_THRESHOLD = -0.2;

//! Buffer air gaps added to physical agent hull diameter (2 * R = 32u) for following distance.
//! Flat ground requires only a minimal buffer (4u) to prevent inter-capsule penetration.
static constexpr double CROWD_GROUND_STOP_BUFFER = 4.0;
static constexpr double CROWD_GROUND_CRAWL_BUFFER = 8.0;
//! Stairs and vertical step risers require a wider buffer (24u clear air) so the climbing leader's
//! elevated sweep trace (SVG_MMove_StepSlideMove) is never clipped or blocked by the trailing follower.
static constexpr double CROWD_STAIR_STOP_BUFFER = 24.0;
static constexpr double CROWD_STAIR_CRAWL_BUFFER = 32.0;

//! Slope projection factor for vertical clearance on stairs and ramps.
static constexpr double CROWD_STAIR_SLOPE_Z_FACTOR = 0.85;

//! Maximum distance squared between active waypoints to consider agents converging on the same bottleneck passage.
static constexpr double CROWD_BOTTLENECK_WAYPOINT_PROXIMITY = 96.0;
static constexpr double CROWD_BOTTLENECK_WAYPOINT_PROXIMITY_SQR = CROWD_BOTTLENECK_WAYPOINT_PROXIMITY * CROWD_BOTTLENECK_WAYPOINT_PROXIMITY;

//! Distance delta threshold in world units to determine clear right-of-way priority at bottleneck passages.
static constexpr double CROWD_BOTTLENECK_DISTANCE_EPSILON = 0.1;

//! Influence zone radius from a doorway bottleneck within which single-file zipper queueing is active.
static constexpr double CROWD_BOTTLENECK_ZONE_INFLUENCE_DIST = 140.0;

//! Headway distance behind a leading agent traversing a bottleneck under which trailing agents must halt.
static constexpr double CROWD_BOTTLENECK_STOP_HEADWAY_DIST = 44.0;

//! Headway distance behind a leading agent traversing a bottleneck under which trailing agents throttle to crawl speed.
static constexpr double CROWD_BOTTLENECK_CRAWL_HEADWAY_DIST = 72.0;

//! Throttled frame velocity scale applied to trailing agents pacing in single file behind a bottleneck leader.
static constexpr double CROWD_BOTTLENECK_CRAWL_SPEED_SCALE = 0.35;

//! Clearance distance in world units past a doorway bottleneck waypoint beyond which a crossing leader has cleared the aperture.
static constexpr double CROWD_BOTTLENECK_APERTURE_CLEAR_DIST = 48.0;

//! Threshold hold distance in world units from a doorway bottleneck waypoint where waiting agents halt outside the aperture.
//! Set to 72.0 units so an agent holding at the threshold line leaves the 48-unit aperture zone completely unobstructed.
static constexpr double CROWD_BOTTLENECK_THRESHOLD_HOLD_DIST = 72.0;

//! Funnel distance in world units from a doorway bottleneck waypoint within which zipper deconfliction engages.
//! Matches CROWD_BOTTLENECK_ZONE_INFLUENCE_DIST (140.0) so converging followers decelerate early and zipper into single file.
static constexpr double CROWD_BOTTLENECK_FUNNEL_DIST = 140.0;



extern std::vector<nav_cover_point_t> g_nav_cover_points;

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

/**
*	Console cvars controlling crowd debug visuals and global tuning.
**/
//! CVAR toggling crowd formation and assignment debug visuals.
cvar_t *s_crowd_debug_draw = nullptr;
//! Default lateral spacing between formation members in world units.
cvar_t *s_crowd_lateral_spacing = nullptr;
//! Default longitudinal spacing between formation ranks in world units.
cvar_t *s_crowd_longitudinal_spacing = nullptr;
//! Default max distance search radius for tactical cover queries.
cvar_t *s_crowd_cover_max_dist = nullptr;

/**
*	Crowd group registry mapping crowdID to group runtime state.
**/
//! Global map of all active crowd groups indexed by crowd ID.
static std::unordered_map<int32_t, svg_crowd_group_t> g_crowd_groups;

/**
*	@brief	Safely resolve the target entity being followed.
*	@return	Pointer to target edict if active and alive, nullptr otherwise.
**/
svg_base_edict_t *svg_crowd_group_t::GetTargetEntity( void ) const {
	if ( targetEntityNumber == ENTITYNUM_NONE || targetEntityNumber < 0 || targetEntityNumber >= globals.edictPool->num_edicts ) {
		return nullptr;
	}
	svg_base_edict_t *ent = g_edict_pool.EdictForNumber( targetEntityNumber );
	return ( ent && SVG_Entity_IsActive( ent ) && ent->health > 0 ) ? ent : nullptr;
}

/**
*	@brief	Safely resolve the designated squad leader entity.
*	@return	Pointer to leader edict if active and alive, nullptr otherwise.
**/
svg_base_edict_t *svg_crowd_group_t::GetLeaderEntity( void ) const {
	if ( leaderEntityNumber == ENTITYNUM_NONE || leaderEntityNumber < 0 || leaderEntityNumber >= globals.edictPool->num_edicts ) {
		return nullptr;
	}
	svg_base_edict_t *ent = g_edict_pool.EdictForNumber( leaderEntityNumber );
	return ( ent && SVG_Entity_IsActive( ent ) && ent->health > 0 ) ? ent : nullptr;
}

/**
*	Private Internal Helpers:
**/

/**
*	@brief	Compute heading yaw angle in degrees toward destination or from target entity.
*	@param	centroid	Collective center position of the crowd in Vector3DP.
*	@param	destOrigin	Destination target origin in Vector3DP.
*	@param	targetEnt	Target entity pointer (if following entity).
*	@param	params		Crowd parameters.
*	@return	Heading yaw in degrees [0..360).
**/
static double SVG_Crowd_CalculateHeadingYaw( const Vector3DP &centroid, const Vector3DP &destOrigin, const svg_base_edict_t *targetEnt, const svg_crowd_params_t &params ) {
	if ( params.orientationMode == crowd_orientation_mode_t::ORIENTATION_FIXED_YAW ) {
		return params.fixedYaw;
	}

	if ( params.orientationMode == crowd_orientation_mode_t::ORIENTATION_TARGET_ENTITY_YAW && targetEnt ) {
		return static_cast<double>( targetEnt->s.angles.y );
	}

	// Default: compute horizontal vector from squad centroid to destination.
	const Vector3DP toDest = destOrigin - centroid;
	const Vector3DP toDest2D{ toDest.x, toDest.y, 0.0 };

	if ( QM_Vector3LengthSqrDP( toDest2D ) > 0.001 ) {
		return QM_Vector3ToYawDP( toDest2D );
	}

	if ( targetEnt ) {
		return static_cast<double>( targetEnt->s.angles.y );
	}

	return 0.0;
}

/**
*	@brief	Compute collective centroid position of all active living squad members.
*	@param	members	List of member entities.
*	@return	Centroid point in Vector3DP.
**/
static Vector3DP SVG_Crowd_ComputeCentroid( const std::vector<svg_base_edict_t*> &members ) {
	if ( members.empty() ) {
		return Vector3DP{ 0.0, 0.0, 0.0 };
	}

	Vector3DP sum{ 0.0, 0.0, 0.0 };
	for ( const svg_base_edict_t *ent : members ) {
		sum = sum + Vector3DP( ent->currentOrigin );
	}

	const double invCount = 1.0 / static_cast<double>( members.size() );
	return sum * invCount;
}

/**
*	@brief	Clear transient serialized-doorway state for a group and its current members.
*	@param	group		Crowd group whose reservation state is reset.
*	@param	members	Active members whose O(1) queue metadata is reset.
*	@note	Final formation slot assignments are intentionally preserved.
**/
static void SVG_Crowd_ResetSerializedIngress( svg_crowd_group_t &group, const std::vector<svg_base_edict_t*> &members ) {
	/**
	*	Reset group-owned portal reservation state.
	**/
	group.ingressPortalOrigin = Vector3DP{ 0.0, 0.0, 0.0 };
	group.ingressPortalInward = Vector3DP{ 0.0, 0.0, 0.0 };
	group.ingressPortalHalfWidth = 0.0;
	group.ingressQueueEntityNumbers.clear();
	group.ingressStagingPositions.clear();
	group.ingressStagingLine.clear();
	group.ingressQueueHead = 0;
	group.ingressMarshalCursor = 0;
	group.ingressQueueHeadStartTime = 0_ms;
	group.ingressReleaseDepth = 0.0;
	group.ingressPhase = crowd_ingress_phase_t::INACTIVE;
	group.hasSerializedIngress = false;

	/**
	*	Reset per-member direct-access queue metadata.
	**/
	for ( svg_base_edict_t *member : members ) {
		// Ignore stale entity pointers while rebuilding an order.
		if ( member == nullptr ) {
			continue;
		}
		member->crowd.ingressQueueRank = -1;
		member->crowd.ingressReleased = true;
	}
}

/**
*	@brief	Reset group-owned serialized room egress state and restore member egress permissions.
*	@param	group	Active crowd coordination group.
*	@param	members	List of active squad member entities.
**/
static void SVG_Crowd_ResetSerializedEgress( svg_crowd_group_t &group, const std::vector<svg_base_edict_t*> &members ) {
	/**
	*	Reset group-owned egress portal state.
	**/
	group.egressPortalOrigin = Vector3DP{ 0.0, 0.0, 0.0 };
	group.egressPortalOutward = Vector3DP{ 0.0, 0.0, 0.0 };
	group.egressPortalHalfWidth = 0.0;
	group.egressQueueEntityNumbers.clear();
	group.egressQueueHead = 0;
	group.egressQueueHeadStartTime = 0_ms;
	group.egressNextReleaseTime = 0_ms;
	group.hasSerializedEgress = false;

	/**
	*	Reset per-member direct-access egress queue metadata.
	**/
	for ( svg_base_edict_t *member : members ) {
		// Ignore stale entity pointers while rebuilding an order.
		if ( member == nullptr ) {
			continue;
		}
		member->crowd.egressQueueRank = -1;
		member->crowd.egressReleased = true;
	}
}

/**
*	@brief	Invalidate one member navigation corridor after changing its transient ingress goal.
*	@param	member	Member whose next think must rebuild its path.
**/
static void SVG_Crowd_ResetMemberNavigation( svg_base_edict_t *member ) {
	/**
	*	Sanity check: only live entity records can own navigation state.
	**/
	if ( member == nullptr ) {
		return;
	}

	/**
	*	Clear crowd and monster path timing so the new goal is consumed immediately.
	**/
	member->crowd.lastPathCalcTime = 0_ms;
	member->crowd.blockedStartTime = 0_ms;
	member->nextthink = level.time + FRAME_TIME_MS;

	svg_monster_base_t *monster = dynamic_cast<svg_monster_base_t*>( member );
	if ( monster != nullptr ) {
		monster->ResetNavigationPath();
		monster->lastPathCalcTime = 0_ms;
		monster->consecutiveBlockedFrames = 0;
	}
}

/**
*	@brief	Configure an immutable-final-slot doorway queue for a static defensive formation.
*	@param	group				Active crowd group receiving portal reservation state.
*	@param	members			Active members with final slot indices already assigned.
*	@param	guidePath			Shared navigation path used to place ordered exterior hold points.
*	@param	portalOrigin		Center of the destination doorway.
*	@param	destinationInterior	Interior anchor used to orient the signed portal plane.
*	@param	destinationRoom		Destination room whose finite exterior wall envelope defines staging.
*	@param	portalWidth			Finite width of the selected doorway aperture.
*	@param	maxAgentRadius		Largest horizontal member radius in the group.
*	@return	True when two or more members require serialized ingress.
*	@note	Final slots remain in group.slots; only crowd.assignedGoalOrigin is temporarily staged.
**/
static bool SVG_Crowd_ConfigureSerializedIngress( svg_crowd_group_t &group, const std::vector<svg_base_edict_t*> &members, const std::vector<Vector3DP> &guidePath, const Vector3DP &portalOrigin, const Vector3DP &destinationInterior, const nav_room_t *destinationRoom, const double portalWidth, const double maxAgentRadius ) {
	/**
	*	Derive the signed doorway plane and reject degenerate geometry.
	**/
	Vector3DP inward = destinationInterior - portalOrigin;
	inward.z = 0.0;
	if ( QM_Vector3LengthSqrDP( inward ) <= 0.0001 ) {
		// Recover orientation from the shared route when the portal and destination anchors coincide.
		if ( guidePath.size() >= 2 ) {
			inward = guidePath.back() - guidePath.front();
			inward.z = 0.0;
		}
		// A stable axis keeps admission defined even for completely degenerate navigation metadata.
		if ( QM_Vector3LengthSqrDP( inward ) <= 0.0001 ) {
			inward = Vector3DP{ 1.0, 0.0, 0.0 };
		}
	}
	inward = QM_Vector3NormalizeDP( inward );

	//! Full center clearance required before the next member may own the doorway.
	const double releaseDepth = maxAgentRadius + CROWD_PORTAL_KEEPOUT_AGENT_MARGIN;
	//! Center-to-center pitch for non-overlapping exterior hold points.
	const double queueSpacing = std::max( CROWD_ROOM_OVERFLOW_QUEUE_STEP,
		( maxAgentRadius * 2.0 ) + ( CROWD_INGRESS_ARRIVAL_RADIUS * 2.0 ) + CROWD_ROOM_FILL_SPACING_BUFFER );

	/**
	*	Build a deterministic approach spine ending exactly at the doorway.
	*	Queue cells sampled on this spine inherit the A* route's bends and slopes,
	*	so the pre-formation stays reachable and ordered instead of scattering
	*	across unrelated room-side cover candidates.
	**/
	const Vector3DP portalTangent{ inward.y, -inward.x, 0.0 };
	std::vector<Vector3DP> exteriorGuidePath;
	exteriorGuidePath.reserve( guidePath.size() + 2 );
	bool hasPathStagingSpine = false;
	if ( guidePath.size() >= 2 ) {
		double bestPortalDistanceSq = std::numeric_limits<double>::max();
		size_t bestPortalSegment = 0;
		double bestPortalFraction = 0.0;

		// Project the physical doorway center to the shared path once at order time.
		for ( size_t pathIndex = 0; pathIndex + 1 < guidePath.size(); pathIndex++ ) {
			Vector3DP segment = guidePath[ pathIndex + 1 ] - guidePath[ pathIndex ];
			segment.z = 0.0;
			const double segmentLengthSq = QM_Vector3LengthSqrDP( segment );
			if ( segmentLengthSq <= 0.0001 ) {
				continue;
			}

			const Vector3DP portalDelta = portalOrigin - guidePath[ pathIndex ];
			const double segmentFraction = std::clamp( QM_Vector3DotProductDP( portalDelta, segment ) / segmentLengthSq, 0.0, 1.0 );
			const Vector3DP projectedPortal = guidePath[ pathIndex ] + ( ( guidePath[ pathIndex + 1 ] - guidePath[ pathIndex ] ) * segmentFraction );
			const double portalDistanceSq = QM_Vector3Distance2DSqrDP( portalOrigin, projectedPortal );
			if ( portalDistanceSq < bestPortalDistanceSq ) {
				bestPortalDistanceSq = portalDistanceSq;
				bestPortalSegment = pathIndex;
				bestPortalFraction = segmentFraction;
			}
		}

		// Keep only the exterior prefix that leads into the selected doorway.
		if ( bestPortalDistanceSq < std::numeric_limits<double>::max() ) {
			for ( size_t pathIndex = 0; pathIndex <= bestPortalSegment; pathIndex++ ) {
				exteriorGuidePath.push_back( guidePath[ pathIndex ] );
			}

			const Vector3DP portalSegmentPoint = guidePath[ bestPortalSegment ] +
				( ( guidePath[ bestPortalSegment + 1 ] - guidePath[ bestPortalSegment ] ) * bestPortalFraction );
			Vector3DP exactPortalPoint = portalOrigin;
			exactPortalPoint.z = portalSegmentPoint.z;
			if ( exteriorGuidePath.empty() ||
				 QM_Vector3Distance2DSqrDP( exteriorGuidePath.back(), exactPortalPoint ) > 1.0 ) {
				exteriorGuidePath.push_back( exactPortalPoint );
			} else {
				exteriorGuidePath.back() = exactPortalPoint;
			}
			hasPathStagingSpine = ( exteriorGuidePath.size() >= 2 );
		}
	}

	// Degenerate metadata still receives a finite synthetic approach lane.
	if ( !hasPathStagingSpine ) {
		const double fallbackDepth = releaseDepth + CROWD_PORTAL_KEEPOUT_AGENT_MARGIN +
			( queueSpacing * static_cast<double>( std::max<size_t>( 1, members.size() ) ) );
		exteriorGuidePath.push_back( portalOrigin - ( inward * fallbackDepth ) );
		exteriorGuidePath.push_back( portalOrigin );
		hasPathStagingSpine = ( exteriorGuidePath.size() >= 2 );
	}

	std::vector<double> distanceToPortalBySpineIndex( exteriorGuidePath.size(), 0.0 );
	if ( exteriorGuidePath.size() >= 2 ) {
		for ( size_t pathIndex = exteriorGuidePath.size() - 1; pathIndex > 0; pathIndex-- ) {
			const double segmentLength = QM_Vector3DistanceDP( exteriorGuidePath[ pathIndex - 1 ], exteriorGuidePath[ pathIndex ] );
			distanceToPortalBySpineIndex[ pathIndex - 1 ] = distanceToPortalBySpineIndex[ pathIndex ] + segmentLength;
		}
	}

	auto projectDistanceToPortalOnSpine = [&]( const Vector3DP &point, double *outLateralDistanceSq ) -> double {
		/**
		*	Project an arbitrary member position onto the finite staging spine and
		*	return its remaining arclength to the doorway. This makes queue order
		*	follow the actual route instead of a single signed portal half-plane.
		**/
		double bestLateralDistanceSq = std::numeric_limits<double>::max();
		double bestDistanceToPortal = QM_Vector3Distance2DDP( point, portalOrigin );
		if ( exteriorGuidePath.size() >= 2 ) {
			for ( size_t pathIndex = 0; pathIndex + 1 < exteriorGuidePath.size(); pathIndex++ ) {
				Vector3DP segment = exteriorGuidePath[ pathIndex + 1 ] - exteriorGuidePath[ pathIndex ];
				segment.z = 0.0;
				const double segmentLengthSq = QM_Vector3LengthSqrDP( segment );
				if ( segmentLengthSq <= 0.0001 ) {
					continue;
				}

				const Vector3DP pointDelta = point - exteriorGuidePath[ pathIndex ];
				const double segmentFraction = std::clamp( QM_Vector3DotProductDP( pointDelta, segment ) / segmentLengthSq, 0.0, 1.0 );
				const Vector3DP projection = exteriorGuidePath[ pathIndex ] + ( segment * segmentFraction );
				const double lateralDistanceSq = QM_Vector3Distance2DSqrDP( point, projection );
				if ( lateralDistanceSq < bestLateralDistanceSq ) {
					const double segmentLength = std::sqrt( segmentLengthSq );
					bestLateralDistanceSq = lateralDistanceSq;
					bestDistanceToPortal = distanceToPortalBySpineIndex[ pathIndex + 1 ] +
						( ( 1.0 - segmentFraction ) * segmentLength );
				}
			}
		}
		if ( outLateralDistanceSq != nullptr ) {
			*outLateralDistanceSq = bestLateralDistanceSq;
		}
		return bestDistanceToPortal;
	};

	/**
	*	Freeze queue order by signed progress through the portal plane.
	*	Higher progress is always closer to, or already through, the destination doorway.
	**/
	struct svg_crowd_ingress_candidate_t {
		double finalSlotDepth = 0.0;
		double currentProgress = 0.0;
		double approachDistance = 0.0;
		double approachLateralDistanceSq = 0.0;
		svg_base_edict_t *member = nullptr;
	};
	std::vector<svg_crowd_ingress_candidate_t> rankedMembers;
	rankedMembers.reserve( members.size() );
	for ( svg_base_edict_t *member : members ) {
		// Skip only invalid entities; incomplete slot metadata must not disable the doorway queue.
		if ( member == nullptr || !SVG_Entity_IsActive( member ) || member->health <= 0 ) {
			continue;
		}

		const Vector3DP memberFeet = SVG_GetEntityFeetOriginDP( member );
		const bool hasFinalSlot = member->crowd.slotIndex >= 0 &&
			member->crowd.slotIndex < static_cast<int32_t>( group.slots.size() );
		const Vector3DP finalSlot = hasFinalSlot
			? group.slots[ member->crowd.slotIndex ].worldPosition
			: destinationInterior;
		const double finalSlotDepth = QM_Vector3DotProductDP( finalSlot - portalOrigin, inward );

		const double signedProgress = QM_Vector3DotProductDP( memberFeet - portalOrigin, inward );
		double approachLateralDistanceSq = 0.0;
		const double approachDistance = projectDistanceToPortalOnSpine( memberFeet, &approachLateralDistanceSq );

		rankedMembers.push_back( svg_crowd_ingress_candidate_t{
			finalSlotDepth,
			signedProgress,
			approachDistance,
			approachLateralDistanceSq,
			member
		} );
	}

	// Imperfect portal-side metadata must not disable serialization for a multi-agent room order.
	if ( rankedMembers.size() <= 1 ) {
		return false;
	}

	std::sort( rankedMembers.begin(), rankedMembers.end(), []( const auto &a, const auto &b ) {
		/**
		*	Order by arclength along the actual approach spine: lower distance is
		*	closer to the doorway and therefore owns the earlier queue cell.
		**/
		// Exact lexicographic comparisons preserve the strict weak ordering required by std::sort.
		if ( a.approachDistance != b.approachDistance ) {
			return a.approachDistance < b.approachDistance;
		}
		if ( a.approachLateralDistanceSq != b.approachLateralDistanceSq ) {
			return a.approachLateralDistanceSq < b.approachLateralDistanceSq;
		}
		if ( a.currentProgress != b.currentProgress ) {
			return a.currentProgress > b.currentProgress;
		}
		return a.member->s.number < b.member->s.number;
	} );

	/**
	*	Sort immutable circle slots deepest-first independently from member order.
	*	Frontmost entrants fill the room back-to-front, so later agents never cross
	*	through shallower occupied slots.
	**/
	std::vector<int32_t> finalSlotOrder;
	finalSlotOrder.reserve( group.slots.size() );
	for ( size_t slotIndex = 0; slotIndex < group.slots.size(); slotIndex++ ) {
		const Vector3DP slotDelta = group.slots[ slotIndex ].worldPosition - portalOrigin;
		const double slotDepth = QM_Vector3DotProductDP( slotDelta, inward );

		// Only true interior room seats are doorway-admission goals; exterior overflow cells stay staged outside.
		if ( !group.slots[ slotIndex ].isNavmeshValid || slotDepth <= 0.0 ) {
			continue;
		}
		finalSlotOrder.push_back( static_cast<int32_t>( slotIndex ) );
	}
	if ( finalSlotOrder.empty() ) {
		group.hasSerializedIngress = false;
		return false;
	}
	std::sort( finalSlotOrder.begin(), finalSlotOrder.end(), [&]( const int32_t a, const int32_t b ) {
		const double aDepth = QM_Vector3DotProductDP( group.slots[ a ].worldPosition - portalOrigin, inward );
		const double bDepth = QM_Vector3DotProductDP( group.slots[ b ].worldPosition - portalOrigin, inward );
		if ( aDepth != bDepth ) {
			return aDepth > bDepth;
		}
		return a < b;
	} );

	/**
	*	Commit queue geometry and direct-index reservation state.
	**/
	group.ingressPortalOrigin = portalOrigin;
	group.ingressPortalInward = inward;
	const double boundedPortalWidth = std::clamp( portalWidth, maxAgentRadius * 2.0,
		CROWD_PORTAL_BOTTLENECK_MAX_WIDTH - 1.0 );
	group.ingressPortalHalfWidth = boundedPortalWidth * 0.5;
	group.ingressQueueEntityNumbers.clear();
	group.ingressQueueEntityNumbers.reserve( finalSlotOrder.size() );
	group.ingressStagingPositions.clear();
	group.ingressStagingPositions.reserve( finalSlotOrder.size() );
	group.ingressQueueHead = 0;
	group.ingressMarshalCursor = 0;
	group.ingressQueueHeadStartTime = level.time;
	group.ingressReleaseDepth = releaseDepth;
	group.ingressPhase = crowd_ingress_phase_t::MARSHALLING;
	group.hasSerializedIngress = true;

	/**
	*	Construct an exterior single-file row on the actual approach spine.
	*	The spine comes from the A* corridor prefix ending at this doorway, so its
	*	cells naturally bend around corners instead of selecting an unrelated room
	*	wall from the destination envelope.
	**/
	const double lateralClearance = group.ingressPortalHalfWidth + releaseDepth;
	const size_t queueCount = rankedMembers.size();
	double memberLateralSum = 0.0;
	for ( const svg_crowd_ingress_candidate_t &rankedMember : rankedMembers ) {
		// Select the approach-side offset already containing most of the crowd to minimize crossing trajectories.
		memberLateralSum += QM_Vector3DotProductDP( SVG_GetEntityFeetOriginDP( rankedMember.member ) - portalOrigin, portalTangent );
	}
	const double preferredSide = ( memberLateralSum < 0.0 ) ? -1.0 : 1.0;
	const double exteriorDepth = releaseDepth + CROWD_PORTAL_KEEPOUT_AGENT_MARGIN;
	const double sideOffset = std::clamp( lateralClearance, maxAgentRadius + CROWD_PORTAL_KEEPOUT_AGENT_MARGIN,
		std::max( lateralClearance, queueSpacing * 0.75 ) );
	const double totalSpineLength = !distanceToPortalBySpineIndex.empty() ? distanceToPortalBySpineIndex.front() : 0.0;
	const double rowCorridorTolerance = std::max( releaseDepth, queueSpacing * 0.65 );
	const double rowCorridorToleranceSq = rowCorridorTolerance * rowCorridorTolerance;
	const double stageProjectionToleranceSq = rowCorridorToleranceSq;

	/**
	*	Build the waiting row around the destination's finite footprint. Projecting
	*	the floor vertices into the doorway frame keeps the first arm on the front
	*	wall and turns the second arm toward the back wall at the actual room corner.
	*	The approach route remains the fallback for destinations without a footprint.
	**/
	std::vector<Vector3DP> wallGuidePath;
	double wallRowLength = 0.0;
	if ( destinationRoom != nullptr ) {
		double minLateral = std::numeric_limits<double>::max();
		double maxLateral = -std::numeric_limits<double>::max();
		double minDepth = std::numeric_limits<double>::max();
		double maxDepth = -std::numeric_limits<double>::max();
		// Traverse destination faces only once per order, never per monster frame.
		for ( const int32_t faceIndex : destinationRoom->face_indices ) {
			// Invalid topology cannot author a waiting position.
			if ( faceIndex < 0 || faceIndex >= static_cast<int32_t>( g_nav_faces.size() ) ) {
				continue;
			}
			const nav_face_t &face = g_nav_faces[ faceIndex ];
			int32_t edgeIndex = face.first_edge_idx;
			for ( int32_t offset = 0; offset < face.num_edges; offset++ ) {
				// Follow explicit half-edge links and stop malformed loops safely.
				if ( edgeIndex < 0 || edgeIndex >= static_cast<int32_t>( g_nav_halfedges.size() ) ) {
					break;
				}
				const nav_halfedge_t &edge = g_nav_halfedges[ edgeIndex ];
				if ( edge.vertex_idx < 0 || edge.vertex_idx >= static_cast<int32_t>( g_nav_vertices.size() ) ) {
					break;
				}
				const Vector3DP delta = g_nav_vertices[ edge.vertex_idx ] - portalOrigin;
				const double lateral = QM_Vector3DotProductDP( delta, portalTangent );
				const double depth = QM_Vector3DotProductDP( delta, inward );
				minLateral = std::min( minLateral, lateral );
				maxLateral = std::max( maxLateral, lateral );
				minDepth = std::min( minDepth, depth );
				maxDepth = std::max( maxDepth, depth );
				edgeIndex = edge.next_idx;
			}
		}
		// Require a finite room-sized footprint before constructing a wall row.
		if ( maxLateral > minLateral && maxDepth > minDepth ) {
			const double wallOffset = maxAgentRadius * 2.0 + CROWD_PORTAL_KEEPOUT_AGENT_MARGIN;
			const double sideBound = preferredSide > 0.0 ? maxLateral + wallOffset : minLateral - wallOffset;
			const double otherBound = preferredSide > 0.0 ? minLateral - wallOffset : maxLateral + wallOffset;
			const double frontDepth = std::min( 0.0, minDepth ) - wallOffset;
			const double rearDepth = maxDepth + wallOffset;
			wallGuidePath.push_back( portalOrigin + inward * rearDepth + portalTangent * otherBound );
			wallGuidePath.push_back( portalOrigin + inward * rearDepth + portalTangent * sideBound );
			wallGuidePath.push_back( portalOrigin + inward * frontDepth + portalTangent * sideBound );
			wallGuidePath.push_back( portalOrigin + inward * frontDepth + portalTangent * ( preferredSide * lateralClearance ) );
			// Cache total row length once; runtime admission uses direct waypoint indices.
			for ( size_t index = 1; index < wallGuidePath.size(); index++ ) {
				wallRowLength += QM_Vector3Distance2DDP( wallGuidePath[ index - 1 ], wallGuidePath[ index ] );
			}
		}
	}

	std::vector<Vector3DP> stagingLine;
	stagingLine.reserve( rankedMembers.size() );
	const double stageMinSeparation = queueSpacing;
	const double stageMinSeparationSq = stageMinSeparation * stageMinSeparation;

	/**
	*	Project one staging candidate onto a real local nav face before accepting it.
	*	The path-spine row is authoritative: face projection may correct floor
	*	height or a small polygon-edge miss, but it may not move the cell into the
	*	destination room or to a different nearby cover island.
	**/
	auto appendProjectedStagePoint = [&]( const Vector3DP &candidate ) -> bool {
		Vector3DP projected = candidate;
		int32_t stageFace = Nav_FindFaceInLeafStrict( projected );
		if ( stageFace < 0 || stageFace >= static_cast<int32_t>( g_nav_faces.size() ) ) {
			Vector3DP feet = projected;
			feet.z -= CROWD_SLOT_FEET_SNAP_OFFSET_Z;
			stageFace = Nav_FindFaceInLeafStrict( feet );
		}
		if ( stageFace < 0 || stageFace >= static_cast<int32_t>( g_nav_faces.size() ) ) {
			const int32_t closestFace = Nav_FindClosestFaceInLeaf( projected );
			if ( closestFace >= 0 && closestFace < static_cast<int32_t>( g_nav_faces.size() ) ) {
				const nav_face_t &closestNavFace = g_nav_faces[ closestFace ];
				Vector3DP closestPoint = projected;
				double closestDistanceSq = 0.0;
				bool hasClosestPoint = Nav_PointInsideFace2D( projected, closestNavFace );

				/**
				*	Clamp small numerical/off-polygon misses to the nearest point of the
				*	local convex face. This is a geometric projection, not a face-center
				*	teleport, and remains bounded by the row corridor tolerance.
				**/
				if ( !hasClosestPoint ) {
					closestDistanceSq = std::numeric_limits<double>::max();
					int32_t halfedgeIndex = closestNavFace.first_edge_idx;
					// Inspect the bounded polygon edge loop once at order time.
					for ( int32_t edgeOffset = 0; edgeOffset < closestNavFace.num_edges; edgeOffset++ ) {
						// Stop malformed half-edge loops before dereferencing global topology.
						if ( halfedgeIndex < 0 || halfedgeIndex >= static_cast<int32_t>( g_nav_halfedges.size() ) ) {
							break;
						}
						const nav_halfedge_t &halfedge = g_nav_halfedges[ halfedgeIndex ];
						if ( halfedge.vertex_idx < 0 ||
							 halfedge.vertex_idx >= static_cast<int32_t>( g_nav_vertices.size() ) ||
							 halfedge.next_idx < 0 ||
							 halfedge.next_idx >= static_cast<int32_t>( g_nav_halfedges.size() ) ) {
							break;
						}
						const int32_t endVertexIndex = g_nav_halfedges[ halfedge.next_idx ].vertex_idx;
						if ( endVertexIndex < 0 || endVertexIndex >= static_cast<int32_t>( g_nav_vertices.size() ) ) {
							break;
						}

						const Vector3DP &edgeStart = g_nav_vertices[ halfedge.vertex_idx ];
						const Vector3DP &edgeEnd = g_nav_vertices[ endVertexIndex ];
						Vector3DP edge = edgeEnd - edgeStart;
						edge.z = 0.0;
						const double edgeLengthSq = QM_Vector3LengthSqrDP( edge );
						if ( edgeLengthSq > 0.0001 ) {
							const double edgeFraction = std::clamp(
								QM_Vector3DotProductDP( projected - edgeStart, edge ) / edgeLengthSq, 0.0, 1.0 );
							const Vector3DP edgePoint = edgeStart + ( edge * edgeFraction );
							const double edgeDistanceSq = QM_Vector3Distance2DSqrDP( projected, edgePoint );
							if ( edgeDistanceSq < closestDistanceSq ) {
								closestDistanceSq = edgeDistanceSq;
								closestPoint = edgePoint;
								hasClosestPoint = true;
							}
						}
						halfedgeIndex = halfedge.next_idx;
					}
				}

				// Accept only a genuinely local projection that preserves the intended row cell.
				if ( hasClosestPoint && closestDistanceSq <= stageProjectionToleranceSq ) {
					Vector3DP faceInset = closestNavFace.center - closestPoint;
					faceInset.z = 0.0;
					if ( QM_Vector3LengthSqrDP( faceInset ) > 0.0001 ) {
						//! Small inset keeps the result numerically inside the selected face.
						static constexpr double CROWD_STAGE_FACE_BOUNDARY_INSET = 1.0;
						closestPoint += QM_Vector3NormalizeDP( faceInset ) * CROWD_STAGE_FACE_BOUNDARY_INSET;
					}
					projected.x = closestPoint.x;
					projected.y = closestPoint.y;
					stageFace = closestFace;
				}
			}
		}
		if ( stageFace < 0 || stageFace >= static_cast<int32_t>( g_nav_faces.size() ) ) {
			return false;
		}

		const nav_face_t &face = g_nav_faces[ stageFace ];
		if ( ( face.surface_flags & ( CM_SURFACE_NO_NAVMESH | CONTENTS_NO_NAVMESH ) ) != 0 ||
			 face.normal.z < NAV_MIN_WALKABLE_Z ) {
			return false;
		}
		if ( std::fabs( face.normal.z ) > 0.001 ) {
			const Vector3DP v0 = g_nav_vertices[ g_nav_halfedges[ face.first_edge_idx ].vertex_idx ];
			const double d = QM_Vector3DotProductDP( v0, face.normal );
			projected.z = ( d - ( projected.x * face.normal.x + projected.y * face.normal.y ) ) / face.normal.z;
		}
		// A closest-face fallback may find a wall top on another storey. Reject that
		// discontinuous rise instead of assigning an unreachable roof-height goal.
		if ( projected.z - candidate.z > static_cast<double>( NAV_MAX_STEP_HEIGHT ) ||
			 candidate.z - projected.z > CROWD_ARRIVAL_MAX_Z_DIFF ) {
			return false;
		}

		/**
		*	Keep staging outside the target room and close to its geometric sample.
		*	This prevents nearest-face repair from snapping a queue cell through the
		*	doorway, which would recreate the congestion we are trying to prevent.
		**/
		if ( QM_Vector3Distance2DSqrDP( projected, candidate ) > stageProjectionToleranceSq ) {
			return false;
		}
		const nav_room_t *projectedRoom = Nav_GetRoomForPoint( projected );
		const double projectedDepth = QM_Vector3DotProductDP( projected - portalOrigin, inward );
		if ( destinationRoom != nullptr &&
			 projectedRoom != nullptr &&
			 projectedRoom->room_id == destinationRoom->room_id &&
			 projectedDepth > group.ingressReleaseDepth * 0.25 ) {
			return false;
		}

		/**
		*	Validate actual standing-hull clearance and support. On a slope the top
		*	of the footprint is above the plane at its center; sweep down from above
		*	that support height instead of rejecting a safe slope as start-solid.
		**/
		const double slopeLift = maxAgentRadius * std::sqrt( face.normal.x * face.normal.x +
			face.normal.y * face.normal.y ) / face.normal.z;
		const Vector3 probeMins{ static_cast<float>( -maxAgentRadius ), static_cast<float>( -maxAgentRadius ), 0.0f };
		const Vector3 probeMaxs{ static_cast<float>( maxAgentRadius ), static_cast<float>( maxAgentRadius ), 72.0f };
		const Vector3DP probeStart = projected + Vector3DP{ 0.0, 0.0, slopeLift + 2.0 };
		const Vector3DP probeEnd = projected - Vector3DP{ 0.0, 0.0, static_cast<double>( NAV_MAX_STEP_HEIGHT ) };
		const svg_trace_t support = SVG_MMove_Trace( probeStart, probeMins, probeMaxs, probeEnd,
			nullptr, CM_CONTENTMASK_SOLID, MM_SHAPE_CYLINDER );
		// Require a walkable support contact, not merely a nearby navigation polygon.
		if ( support.startsolid || support.allsolid || support.fraction >= 1.0 ||
			 support.plane.normal[ 2 ] < NAV_MIN_WALKABLE_Z ) {
			return false;
		}
		projected = probeStart + ( probeEnd - probeStart ) * support.fraction;

		// Bends can bring nonadjacent arclength cells together; test all accepted cells once at order time.
		for ( const Vector3DP &acceptedStage : stagingLine ) {
			if ( QM_Vector3Distance2DSqrDP( projected, acceptedStage ) < stageMinSeparationSq ) {
				return false;
			}
		}

		stagingLine.push_back( projected );
		return true;
	};

	/**
	*	Sample one immutable cell per queue rank at exact arclength along the exterior spine.
	*	Cover points may adjust only wall-normal standoff and floor height; they may
	*	never replace the ideal arclength coordinate with an arbitrary nearby position.
	*	This preserves a contiguous row independently of monster arrival locations.
	**/
	//! Bounded cover neighborhood radius; one 256-unit spatial-hash cell neighborhood is sufficient.
	const double coverSnapRadius = std::max( 8.0, std::min( rowCorridorTolerance, queueSpacing * 0.5 ) );
	//! Maximum number of distance-ranked local cover records inspected for one row cell.
	static constexpr size_t CROWD_STAGE_MAX_LOCAL_COVER = 24;
	//! Minimum dot product between a cover normal and the wall arm's exterior normal.
	static constexpr double CROWD_STAGE_COVER_NORMAL_ALIGNMENT = 0.6;
	//! Fixed perpendicular repair distance used when an ideal point lies on a nav polygon seam.
	static constexpr double CROWD_STAGE_REPAIR_STEP = 8.0;
	//! Maximum bounded seam-repair probes on each side of an ideal row cell.
	static constexpr int32_t CROWD_STAGE_REPAIR_ATTEMPTS = 3;

	std::vector<int32_t> localCoverIndices;
	localCoverIndices.reserve( CROWD_STAGE_MAX_LOCAL_COVER );
	size_t coverCandidatesExamined = 0;
	size_t coverSnappedStages = 0;
	size_t proceduralStages = 0;
	size_t rejectedStages = 0;

	// Walk a bounded number of contour samples, accepting only separated, supported cells.
	for ( size_t sampleIndex = 0; stagingLine.size() < queueCount && sampleIndex < queueCount * 16; sampleIndex++ ) {
		const double rowDistance = queueSpacing * 0.25 * static_cast<double>( sampleIndex );
		const double targetDistBack = exteriorDepth + rowDistance;
		Vector3DP spinePoint = portalOrigin - ( inward * targetDistBack );
		Vector3DP spineForward = inward;

		/**
		*	Sample the existing route by arclength. Overflow beyond the known route
		*	continues backward from the first route tangent, preserving a single row
		*	instead of collapsing multiple ranks onto path.front().
		**/
		if ( !SVG_Crowd_SampleGuidePathInReverse( exteriorGuidePath, targetDistBack, &spinePoint, &spineForward ) ||
			 ( totalSpineLength > 0.0 && targetDistBack > totalSpineLength ) ) {
			Vector3DP frontForward = inward;
			if ( exteriorGuidePath.size() >= 2 ) {
				frontForward = exteriorGuidePath[ 1 ] - exteriorGuidePath[ 0 ];
				frontForward.z = 0.0;
				if ( QM_Vector3LengthSqrDP( frontForward ) > 0.0001 ) {
					frontForward = QM_Vector3NormalizeDP( frontForward );
				} else {
					frontForward = inward;
				}
				const double overflowDistance = std::max( 0.0, targetDistBack - totalSpineLength );
				spinePoint = exteriorGuidePath.front() - ( frontForward * overflowDistance );
			}
			spineForward = frontForward;
		}

		spineForward.z = 0.0;
		if ( QM_Vector3LengthSqrDP( spineForward ) <= 0.0001 ) {
			spineForward = inward;
		} else {
			spineForward = QM_Vector3NormalizeDP( spineForward );
		}
		Vector3DP sideDirection{ spineForward.y, -spineForward.x, 0.0 };
		if ( QM_Vector3LengthSqrDP( sideDirection ) <= 0.0001 ) {
			sideDirection = portalTangent;
		} else {
			sideDirection = QM_Vector3NormalizeDP( sideDirection );
		}
		sideDirection = sideDirection * preferredSide;

		Vector3DP idealStage = spinePoint + ( sideDirection * sideOffset );
		Vector3DP rowDirection = spineForward;
		Vector3DP wallExteriorNormal = sideDirection;
		// A finite room footprint supplies the requested front-wall / side-wall L turn.
		if ( !wallGuidePath.empty() ) {
			// Never extend a room row off the known footprint into unrelated cover islands.
			if ( rowDistance > wallRowLength ) {
				break;
			}
			SVG_Crowd_SampleGuidePathInReverse( wallGuidePath, rowDistance, &idealStage, &rowDirection );
			wallExteriorNormal = Vector3DP{ -rowDirection.y * preferredSide, rowDirection.x * preferredSide, 0.0 };
		}

		bool stagePlaced = false;
		localCoverIndices.clear();
		Nav_QueryCoverPointsRadius( idealStage, coverSnapRadius, &localCoverIndices, CROWD_STAGE_MAX_LOCAL_COVER );
		coverCandidatesExamined += localCoverIndices.size();

		/**
		*	Use the nearest compatible cover point as local wall/floor evidence.
		*	Only its perpendicular standoff and Z are transferred: the ideal row's
		*	arclength remains unchanged, so cover density cannot scatter the formation.
		**/
		for ( const int32_t coverIndex : localCoverIndices ) {
			// Reject stale or non-walkable cover records.
			if ( coverIndex < 0 || coverIndex >= static_cast<int32_t>( g_nav_cover_points.size() ) ) {
				continue;
			}
			const nav_cover_point_t &coverPoint = g_nav_cover_points[ coverIndex ];
			if ( coverPoint.face_idx < 0 || coverPoint.face_idx >= static_cast<int32_t>( g_nav_faces.size() ) ) {
				continue;
			}
			const nav_face_t &coverFace = g_nav_faces[ coverPoint.face_idx ];
			if ( ( coverFace.surface_flags & ( CM_SURFACE_NO_NAVMESH | CONTENTS_NO_NAVMESH ) ) != 0 ||
				 coverFace.normal.z < NAV_MIN_WALKABLE_Z ) {
				continue;
			}

			Vector3DP coverPosition = {};
			Vector3DP coverNormal = {};
			if ( !Nav_GetCoverPointWorldDP( coverPoint, &coverPosition, &coverNormal ) ||
				 std::fabs( coverPosition.z - idealStage.z ) > CROWD_ARRIVAL_MAX_Z_DIFF ) {
				continue;
			}
			coverNormal.z = 0.0;
			if ( QM_Vector3LengthSqrDP( coverNormal ) <= 0.0001 ) {
				continue;
			}
			coverNormal = QM_Vector3NormalizeDP( coverNormal );
			if ( QM_Vector3DotProductDP( coverNormal, wallExteriorNormal ) < CROWD_STAGE_COVER_NORMAL_ALIGNMENT ) {
				continue;
			}

			const Vector3DP coverDelta = coverPosition - idealStage;
			const double alongRowError = QM_Vector3DotProductDP( coverDelta, rowDirection );
			const double wallNormalOffset = QM_Vector3DotProductDP( coverDelta, wallExteriorNormal );
			const double tangentialSlack = std::max( 4.0, ( queueSpacing - stageMinSeparation ) * 0.5 );
			// Reject cover belonging to a neighboring arclength cell or another nearby wall.
			if ( std::fabs( alongRowError ) > tangentialSlack ||
				 std::fabs( wallNormalOffset ) > coverSnapRadius ) {
				continue;
			}

			Vector3DP coverSnappedStage = idealStage + ( wallExteriorNormal * wallNormalOffset );
			coverSnappedStage.z = coverPosition.z;
			if ( appendProjectedStagePoint( coverSnappedStage ) ) {
				stagePlaced = true;
				coverSnappedStages++;
				break;
			}
		}

		// The exact geometric sample is authoritative when no compatible cover point is nearby.
		if ( !stagePlaced && appendProjectedStagePoint( idealStage ) ) {
			stagePlaced = true;
			proceduralStages++;
		}

		/**
		*	Repair only perpendicular nav-polygon seam misses. These probes preserve the
		*	row's exact arclength and therefore cannot reorder or cross neighboring cells.
		**/
		for ( int32_t repairAttempt = 1; !stagePlaced && repairAttempt <= CROWD_STAGE_REPAIR_ATTEMPTS; repairAttempt++ ) {
			const double repairDistance = CROWD_STAGE_REPAIR_STEP * static_cast<double>( repairAttempt );
			const Vector3DP outwardRepair = idealStage + ( wallExteriorNormal * repairDistance );
			if ( appendProjectedStagePoint( outwardRepair ) ) {
				stagePlaced = true;
				proceduralStages++;
				break;
			}
			const Vector3DP inwardRepair = idealStage - ( wallExteriorNormal * repairDistance );
			if ( appendProjectedStagePoint( inwardRepair ) ) {
				stagePlaced = true;
				proceduralStages++;
				break;
			}
		}

		// Failed geometry is skipped; it is never converted into an unchecked movement goal.
		if ( !stagePlaced ) {
			rejectedStages++;
		}
	}

	gi.dprintf( "[crowd ingress staging] crowd=%" PRId32 " room=%" PRId32 " wall_row=%d path_len=%.1f cover_examined=%zu cover_snapped=%zu procedural=%zu rejected=%zu stages=%zu queue_members=%zu phase=admission\n",
		group.crowdID, destinationRoom != nullptr ? destinationRoom->room_id : -1,
		!wallGuidePath.empty() ? 1 : 0, totalSpineLength, coverCandidatesExamined, coverSnappedStages, proceduralStages, rejectedStages,
		stagingLine.size(), rankedMembers.size() );

	// An incomplete row must fail explicitly rather than stack multiple members on one valid cell.
	if ( stagingLine.size() != queueCount ) {
		gi.dprintf( "[crowd ingress] crowd=%" PRId32 " cannot construct a supported exterior row (%zu/%zu)\n",
			group.crowdID, stagingLine.size(), queueCount );
		SVG_Crowd_StopCrowd( group.crowdID );
		return false;
	}
	group.ingressStagingLine = stagingLine;

	/**
	*	Assign queue rank directly to staging rank.
	*	A doorway is a one-dimensional capacity constraint, so a nearest-cell
	*	bipartite match is mathematically wrong here: it can reduce individual
	*	travel while reordering the queue and making paths cross.
	**/
	for ( size_t rank = 0; rank < rankedMembers.size(); rank++ ) {
		svg_base_edict_t *member = rankedMembers[ rank ].member;
		const bool hasInteriorFinalSlot = ( rank < finalSlotOrder.size() );
		// Pair admission rank with a back-to-front immutable circle slot.
		if ( hasInteriorFinalSlot ) {
			member->crowd.slotIndex = finalSlotOrder[ rank ];
			member->crowd.role = group.slots[ member->crowd.slotIndex ].role;
		} else {
			member->crowd.slotIndex = -1;
			member->crowd.role = crowd_member_role_t::ROLE_CENTER;
		}
		const size_t assignedStageIndex = std::min( rank, stagingLine.size() - 1 );
		const Vector3DP assignedStage = stagingLine[ assignedStageIndex ];
		if ( hasInteriorFinalSlot ) {
			const size_t queueRank = group.ingressQueueEntityNumbers.size();
			group.ingressQueueEntityNumbers.push_back( member->s.number );
			group.ingressStagingPositions.push_back( assignedStage );
			member->crowd.ingressQueueRank = static_cast<int32_t>( queueRank );
			// Every entrant must first claim its own exterior staging cell; the queue head is
			// released to its immutable room slot only after it physically reaches that cell.
			member->crowd.ingressReleased = false;
			member->crowd.assignedGoalOrigin = QM_Vector3FromDP( assignedStage );
		} else {
			// Members without a valid interior seat hold outside the doorway and never participate in the admission queue.
			member->crowd.ingressQueueRank = -1;
			member->crowd.ingressReleased = false;
			member->crowd.assignedGoalOrigin = QM_Vector3FromDP( assignedStage );
		}
		member->crowd.reachedGoal = false;
		SVG_Crowd_ResetMemberNavigation( member );
	}

	return !group.ingressQueueEntityNumbers.empty();
}

/**
*	@brief	Advance one serialized doorway reservation when its owner clears the portal plane.
*	@param	group	Active group with a frozen ingress queue.
*	@return	True while unreleased queue members remain.
*	@note	Queue-head entity lookup and final-slot access are both O(1).
**/
static bool SVG_Crowd_UpdateSerializedIngress( svg_crowd_group_t &group ) {
	/**
	*	Sanity check: validate queue state before direct indexed access.
	**/
	if ( !group.hasSerializedIngress || group.ingressQueueHead < 0 ||
		 group.ingressQueueHead >= static_cast<int32_t>( group.ingressQueueEntityNumbers.size() ) ) {
		group.ingressPhase = crowd_ingress_phase_t::INACTIVE;
		group.hasSerializedIngress = false;
		return false;
	}

	/**
	*	Begin rolling admission immediately after plotting the pre-formation.
	*	The previous full-row barrier let one unreachable spawn member freeze every
	*	other entrant. The queue head is still required to occupy its staging cell
	*	before release, but only that O(1) owner can hold the doorway reservation.
	**/
	if ( group.ingressPhase == crowd_ingress_phase_t::MARSHALLING ) {
		group.ingressPhase = crowd_ingress_phase_t::ADMISSION;
		group.ingressMarshalCursor = 0;
		group.ingressQueueHeadStartTime = level.time;
	}

	const int32_t activeEntityNumber = group.ingressQueueEntityNumbers[ group.ingressQueueHead ];
	svg_base_edict_t *activeMember = ( activeEntityNumber >= 1 && activeEntityNumber < globals.edictPool->num_edicts )
		? g_edict_pool.EdictForNumber( activeEntityNumber )
		: nullptr;

	/**
	*	Close one adjacent vacancy per frame. A ready tail member must not receive
	*	the doorway token from the rear of the row: its route would cross every
	*	stationary member ahead. Instead transfer the vacant predecessor cell.
	**/
	auto promoteReadyMember = [&]() -> bool {
		const int32_t queueCount = static_cast<int32_t>( group.ingressQueueEntityNumbers.size() );
		// There is nobody left to probe when the current owner is the last reservation.
		if ( group.ingressQueueHead + 1 >= queueCount || activeMember == nullptr ) {
			return false;
		}
		// Wrap the persistent cursor across the unserved suffix without rescanning it in one frame.
		if ( group.ingressMarshalCursor <= group.ingressQueueHead || group.ingressMarshalCursor >= queueCount ) {
			group.ingressMarshalCursor = group.ingressQueueHead + 1;
		}
		const int32_t candidateIndex = group.ingressMarshalCursor++;
		const int32_t predecessorIndex = candidateIndex - 1;
		const int32_t predecessorNumber = group.ingressQueueEntityNumbers[ predecessorIndex ];
		svg_base_edict_t *predecessor = predecessorNumber >= 1 && predecessorNumber < globals.edictPool->num_edicts ?
			g_edict_pool.EdictForNumber( predecessorNumber ) : nullptr;
		const int32_t candidateNumber = group.ingressQueueEntityNumbers[ candidateIndex ];
		svg_base_edict_t *candidate = candidateNumber >= 1 && candidateNumber < globals.edictPool->num_edicts ?
			g_edict_pool.EdictForNumber( candidateNumber ) : nullptr;
		// Dead or already admitted members cannot own a new reservation.
		if ( candidate == nullptr || !SVG_Entity_IsActive( candidate ) || candidate->health <= 0 ||
			 candidate->crowd.ingressReleased || predecessor == nullptr ||
			 !SVG_Entity_IsActive( predecessor ) || predecessor->health <= 0 || predecessor->crowd.ingressReleased ) {
			return false;
		}
		const Vector3DP candidateFeet = SVG_GetEntityFeetOriginDP( candidate );
		const Vector3DP &candidateStage = group.ingressStagingPositions[ candidateIndex ];
		const double readyRadius = CROWD_INGRESS_ARRIVAL_RADIUS * 1.35;
		// Check real occupancy, not a possibly stale reachedGoal flag.
		if ( QM_Vector3Distance2DSqrDP( candidateFeet, candidateStage ) > readyRadius * readyRadius ||
			 std::fabs( candidateFeet.z - candidateStage.z ) > CROWD_ARRIVAL_MAX_Z_DIFF ) {
			return false;
		}
		const Vector3DP predecessorFeet = SVG_GetEntityFeetOriginDP( predecessor );
		const Vector3DP &predecessorStage = group.ingressStagingPositions[ predecessorIndex ];
		// Never displace a correctly queued predecessor. Apply the no-overtaking
		// projection only locally: a distant spawn can be on either side of a corner's plane.
		const double localRowSpan = QM_Vector3Distance2DDP( predecessorStage, candidateStage ) * 2.0;
		const bool predecessorIsNearby = QM_Vector3Distance2DSqrDP( predecessorFeet, candidateFeet ) <= localRowSpan * localRowSpan;
		if ( QM_Vector3Distance2DSqrDP( predecessorFeet, predecessorStage ) <= readyRadius * readyRadius ||
			 ( predecessorIsNearby && QM_Vector3DotProductDP( predecessorFeet - candidateFeet, predecessorStage - candidateStage ) > 0.0 ) ) {
			return false;
		}
		/**
		*	Swap ownership, not geometry: the ready follower advances one vacant cell
		*	and the late predecessor approaches the vacated follower cell. Neither is
		*	released until physically occupying the current front cell.
		**/
		predecessor->crowd.assignedGoalOrigin = QM_Vector3FromDP( candidateStage );
		candidate->crowd.assignedGoalOrigin = QM_Vector3FromDP( predecessorStage );
		predecessor->crowd.reachedGoal = false;
		candidate->crowd.reachedGoal = false;
		SVG_Crowd_ResetMemberNavigation( predecessor );
		SVG_Crowd_ResetMemberNavigation( candidate );
		std::swap( predecessor->crowd.slotIndex, candidate->crowd.slotIndex );
		std::swap( predecessor->crowd.role, candidate->crowd.role );
		std::swap( group.ingressQueueEntityNumbers[ predecessorIndex ], group.ingressQueueEntityNumbers[ candidateIndex ] );
		predecessor->crowd.ingressQueueRank = candidateIndex;
		candidate->crowd.ingressQueueRank = predecessorIndex;
		// Give the new front owner time to walk to the vacant cell before another promotion.
		if ( predecessorIndex == group.ingressQueueHead ) {
			group.ingressQueueHeadStartTime = level.time;
		}
		return true;
	};

	/**
	* Admit the queue head only after it reaches its own local staging cell.
	* Other agents may still approach their cells, so a remote member cannot freeze the doorway pipeline.
	**/
	if ( activeMember != nullptr && SVG_Entity_IsActive( activeMember ) && activeMember->health > 0 &&
		 !activeMember->crowd.ingressReleased ) {
		const bool hasStagingGoal = group.ingressQueueHead >= 0 &&
			group.ingressQueueHead < static_cast<int32_t>( group.ingressStagingPositions.size() );
		if ( !hasStagingGoal ) {
			return true;
		}

		const Vector3DP activeFeet = SVG_GetEntityFeetOriginDP( activeMember );
		const Vector3DP &stagingGoal = group.ingressStagingPositions[ group.ingressQueueHead ];
		const double stagingArrivalRadius = CROWD_INGRESS_ARRIVAL_RADIUS * 1.35;
		const bool stagingPositionMatches =
			QM_Vector3Distance2DSqrDP( activeFeet, stagingGoal ) <= ( stagingArrivalRadius * stagingArrivalRadius ) &&
			std::fabs( activeFeet.z - stagingGoal.z ) <= CROWD_ARRIVAL_MAX_Z_DIFF;
		// Preserve the reservation until the head is physically occupying its plotted exterior cell.
		if ( !stagingPositionMatches ) {
			// Compute proximity of the queue head to the destination staging zone.
			const double distHeadToStagingSq = QM_Vector3Distance2DSqrDP( activeFeet, stagingGoal );
			const double stagingInfluenceRadius = CROWD_BOTTLENECK_ZONE_INFLUENCE_DIST;
			const bool headNearStaging = ( distHeadToStagingSq <= stagingInfluenceRadius * stagingInfluenceRadius );

			// While the queue head is traversing across the map towards the staging area,
			// it is making normal travel progress and must not time out on the 2.5s wait timer.
			// Keep refreshing ingressQueueHeadStartTime so the wait timeout only elapses once proximate.
			if ( !headNearStaging ) {
				group.ingressQueueHeadStartTime = level.time;
			}

			//! Maximum stationary interval before an unreachable staging owner yields its queue rank.
			static constexpr QMTime CROWD_INGRESS_STAGING_STALL_TIMEOUT = 1500_ms;
			//! Maximum queue-head wait even when path failure leaves blockedStartTime unset.
			static constexpr QMTime CROWD_INGRESS_STAGING_WAIT_TIMEOUT = 2500_ms;
			const bool stagingOwnerStalled = activeMember->crowd.blockedStartTime.Milliseconds() > 0 &&
				( level.time - activeMember->crowd.blockedStartTime ) >= CROWD_INGRESS_STAGING_STALL_TIMEOUT;
			const bool stagingOwnerTimedOut = headNearStaging && group.ingressQueueHeadStartTime.Milliseconds() > 0 &&
				( level.time - group.ingressQueueHeadStartTime ) >= CROWD_INGRESS_STAGING_WAIT_TIMEOUT;
			if ( !stagingOwnerStalled && !stagingOwnerTimedOut ) {
				return true;
			}

			if ( group.ingressQueueHead + 1 < static_cast<int32_t>( group.ingressQueueEntityNumbers.size() ) ) {
				promoteReadyMember();
				return true;
			}

			// A last remaining owner cannot block anyone else; let its normal A* goal recovery handle the final slot.
			const int32_t stalledSlotIndex = activeMember->crowd.slotIndex;
			if ( stalledSlotIndex >= 0 && stalledSlotIndex < static_cast<int32_t>( group.slots.size() ) ) {
				activeMember->crowd.ingressReleased = true;
				activeMember->crowd.assignedGoalOrigin = QM_Vector3FromDP( group.slots[ stalledSlotIndex ].worldPosition );
				activeMember->crowd.reachedGoal = false;
				activeMember->crowd.blockedStartTime = 0_ms;
				SVG_Crowd_ResetMemberNavigation( activeMember );
			}
			return true;
		}

		const int32_t slotIndex = activeMember->crowd.slotIndex;
		if ( slotIndex < 0 || slotIndex >= static_cast<int32_t>( group.slots.size() ) ) {
			return true;
		}
		activeMember->crowd.ingressReleased = true;
		activeMember->crowd.assignedGoalOrigin = QM_Vector3FromDP( group.slots[ slotIndex ].worldPosition );
		activeMember->crowd.reachedGoal = false;
		SVG_Crowd_ResetMemberNavigation( activeMember );
		return true;
	}

	bool releaseNext = ( activeMember == nullptr || !SVG_Entity_IsActive( activeMember ) || activeMember->health <= 0 );
	if ( !releaseNext ) {
		// Retain the token until the entrant reaches its seat; doorway clearance alone
		// does not prevent two routes crossing immediately inside a small room.
		releaseNext = activeMember->crowd.reachedGoal;
	}

	// Preserve exclusive ownership until the active hull is fully clear of the doorway.
	if ( !releaseNext ) {
		//! Maximum stall interval before passing doorway ownership to the next staged member.
		static constexpr QMTime CROWD_INGRESS_ACTIVE_STALL_TIMEOUT = 1500_ms;
		const Vector3DP activeFeet = SVG_GetEntityFeetOriginDP( activeMember );
		const double activeDepth = QM_Vector3DotProductDP( activeFeet - group.ingressPortalOrigin,
			group.ingressPortalInward );
			
		const bool activeStalled = activeMember->crowd.blockedStartTime.Milliseconds() > 0 &&
			( level.time - activeMember->crowd.blockedStartTime ) >= CROWD_INGRESS_ACTIVE_STALL_TIMEOUT;
			
		const int32_t nextQueueIndex = group.ingressQueueHead + 1;
		if ( activeStalled &&
			 nextQueueIndex < static_cast<int32_t>( group.ingressQueueEntityNumbers.size() ) ) {
			promoteReadyMember();
		}
		return true;
	}

	/**
	*	Advance exactly one queue position and activate its immutable final formation slot.
	**/
	group.ingressQueueHead++;
	group.ingressQueueHeadStartTime = level.time;
	if ( group.ingressQueueHead >= static_cast<int32_t>( group.ingressQueueEntityNumbers.size() ) ) {
		group.ingressPhase = crowd_ingress_phase_t::INACTIVE;
		group.hasSerializedIngress = false;
		return false;
	}

	const int32_t nextEntityNumber = group.ingressQueueEntityNumbers[ group.ingressQueueHead ];
	svg_base_edict_t *nextMember = ( nextEntityNumber >= 1 && nextEntityNumber < globals.edictPool->num_edicts )
		? g_edict_pool.EdictForNumber( nextEntityNumber )
		: nullptr;

	// Invalid members are skipped on the next frame without scanning the queue.
	if ( nextMember == nullptr || !SVG_Entity_IsActive( nextMember ) || nextMember->health <= 0 ) {
		return true;
	}

	const int32_t slotIndex = nextMember->crowd.slotIndex;
	if ( slotIndex < 0 || slotIndex >= static_cast<int32_t>( group.slots.size() ) ) {
		return true;
	}

	const bool hasStagingGoal = group.ingressQueueHead >= 0 &&
		group.ingressQueueHead < static_cast<int32_t>( group.ingressStagingPositions.size() );
	if ( !hasStagingGoal ) {
		return true;
	}

	// Keep the next doorway owner staged until its own exterior pre-formation cell is occupied.
	nextMember->crowd.ingressReleased = false;
	nextMember->crowd.assignedGoalOrigin = QM_Vector3FromDP( group.ingressStagingPositions[ group.ingressQueueHead ] );
	nextMember->crowd.reachedGoal = false;
	SVG_Crowd_ResetMemberNavigation( nextMember );
	return true;
}

/**
*	@brief		Configure serialized in-order room egress when a squad is departing an enclosed room/zone through a narrow doorway.
*	@param	group			Active crowd coordination group record.
*	@param	members			Living squad members executing this movement command.
*	@param	guidePath		Piecewise-linear navigation corridor from squad centroid to destination.
*	@param	navPathFaces	Sequence of navigation mesh faces connecting start to destination.
*	@param	startRoom		Origin room/zone containing squad centroid (nullptr if unclassified).
*	@param	destRoom		Destination room/zone containing final goal (nullptr if unclassified).
*	@param	centroid		Collective centroid of the squad at command dispatch time.
*	@return	True if serialized room egress was successfully configured; false otherwise.
**/
static bool SVG_Crowd_ConfigureSerializedEgress( svg_crowd_group_t &group, const std::vector<svg_base_edict_t*> &members, const std::vector<Vector3DP> &guidePath, const std::vector<int32_t> &navPathFaces, const nav_room_t *startRoom, const nav_room_t *destRoom, const Vector3DP &centroid ) {
	/**
	*	Sanity check: single-member squads never congest doorways; require at least two members.
	**/
	if ( members.size() <= 1 ) {
		return false;
	}

	/**
	*	If destination is inside the exact same enclosed room as start, no room egress occurs.
	**/
	if ( startRoom != nullptr && destRoom != nullptr && startRoom->room_id == destRoom->room_id ) {
		return false;
	}

	Vector3DP exitPortalPoint = {};
	Vector3DP exitPortalOutward = {};
	double exitPortalWidth = 0.0;
	bool hasExitPortal = false;

	/**
	*	Method 1: Identify the exact room-exit transition edge along the A* nav face sequence.
	*	The first face transition where faceA belongs to startRoom and faceB does not
	*	is authoritative for the physical doorway traversed by this order.
	**/
	if ( startRoom != nullptr && navPathFaces.size() >= 2 ) {
		// Traverse faces from route start looking for the boundary leaving startRoom.
		for ( size_t i = 0; i + 1 < navPathFaces.size(); i++ ) {
			const int32_t faceAIndex = navPathFaces[ i ];
			const int32_t faceBIndex = navPathFaces[ i + 1 ];
			if ( faceAIndex < 0 || faceBIndex < 0 ||
				 faceAIndex >= static_cast<int32_t>( g_nav_faces.size() ) ||
				 faceBIndex >= static_cast<int32_t>( g_nav_faces.size() ) ) {
				continue;
			}
			const nav_face_t &faceA = g_nav_faces[ faceAIndex ];
			const nav_face_t &faceB = g_nav_faces[ faceBIndex ];
			if ( faceA.room_id == startRoom->room_id && faceB.room_id != startRoom->room_id ) {
				// Search the half-edge boundary loop of faceA for the transition into faceB.
				int32_t edgeIndex = faceA.first_edge_idx;
				for ( int32_t edgeOffset = 0; edgeOffset < faceA.num_edges; edgeOffset++ ) {
					if ( edgeIndex < 0 || edgeIndex >= static_cast<int32_t>( g_nav_halfedges.size() ) ) {
						break;
					}
					const nav_halfedge_t &he = g_nav_halfedges[ edgeIndex ];
					if ( he.twin_idx >= 0 && he.twin_idx < static_cast<int32_t>( g_nav_halfedges.size() ) &&
						 g_nav_halfedges[ he.twin_idx ].face_idx == faceBIndex ) {
						if ( he.vertex_idx >= 0 && he.vertex_idx < static_cast<int32_t>( g_nav_vertices.size() ) &&
							 he.next_idx >= 0 && he.next_idx < static_cast<int32_t>( g_nav_halfedges.size() ) ) {
							const int32_t endVertexIndex = g_nav_halfedges[ he.next_idx ].vertex_idx;
							if ( endVertexIndex >= 0 && endVertexIndex < static_cast<int32_t>( g_nav_vertices.size() ) ) {
								const Vector3DP &vStart = g_nav_vertices[ he.vertex_idx ];
								const Vector3DP &vEnd = g_nav_vertices[ endVertexIndex ];
								exitPortalPoint = ( vStart + vEnd ) * 0.5;
								exitPortalWidth = QM_Vector3Distance2DDP( vStart, vEnd );
								Vector3DP outward = faceB.center - faceA.center;
								outward.z = 0.0;
								exitPortalOutward = ( QM_Vector3LengthSqrDP( outward ) > 0.0001 )
									? QM_Vector3NormalizeDP( outward )
									: Vector3DP{ 1.0, 0.0, 0.0 };
								hasExitPortal = ( exitPortalWidth > 0.001 );
								break;
							}
						}
					}
					edgeIndex = he.next_idx;
				}
				if ( hasExitPortal ) {
					break;
				}

				// Fallback: check precomputed boundary portals belonging to startRoom.
				for ( const int32_t portalIndex : startRoom->portal_indices ) {
					if ( portalIndex < 0 || portalIndex >= static_cast<int32_t>( g_nav_portals.size() ) ) {
						continue;
					}
					const nav_portal_t &routePortal = g_nav_portals[ portalIndex ];
					if ( routePortal.halfedge_idx < 0 ||
						 routePortal.halfedge_idx >= static_cast<int32_t>( g_nav_halfedges.size() ) ) {
						continue;
					}
					const nav_halfedge_t &portalHalfedge = g_nav_halfedges[ routePortal.halfedge_idx ];
					if ( portalHalfedge.twin_idx < 0 ||
						 portalHalfedge.twin_idx >= static_cast<int32_t>( g_nav_halfedges.size() ) ) {
						continue;
					}
					const int32_t portalFaceA = portalHalfedge.face_idx;
					const int32_t portalFaceB = g_nav_halfedges[ portalHalfedge.twin_idx ].face_idx;
					if ( ( portalFaceA == faceAIndex && portalFaceB == faceBIndex ) ||
						 ( portalFaceA == faceBIndex && portalFaceB == faceAIndex ) ) {
						exitPortalPoint = routePortal.center;
						exitPortalWidth = routePortal.width;
						Vector3DP outward = faceB.center - faceA.center;
						outward.z = 0.0;
						exitPortalOutward = ( QM_Vector3LengthSqrDP( outward ) > 0.0001 )
							? QM_Vector3NormalizeDP( outward )
							: routePortal.normal;
						hasExitPortal = true;
						break;
					}
				}
				if ( hasExitPortal ) {
					break;
				}
			}
		}
	}

	/**
	*	Method 2: If topological face transitions did not yield a portal, check startRoom's portals
	*	for the one nearest the initial guide corridor segment.
	**/
	if ( !hasExitPortal && startRoom != nullptr && !startRoom->portal_indices.empty() ) {
		double bestDistanceSq = std::numeric_limits<double>::max();
		for ( const int32_t portalIndex : startRoom->portal_indices ) {
			if ( portalIndex < 0 || portalIndex >= static_cast<int32_t>( g_nav_portals.size() ) ) {
				continue;
			}
			const nav_portal_t &portal = g_nav_portals[ portalIndex ];
			double dSq = QM_Vector3Distance2DSqrDP( centroid, portal.center );
			if ( guidePath.size() >= 2 ) {
				const Vector3DP &segmentStart = guidePath[ 0 ];
				const Vector3DP &segmentEnd = guidePath[ 1 ];
				const Vector3DP segment = segmentEnd - segmentStart;
				const double segmentLengthSq = QM_Vector3LengthSqrDP( segment );
				if ( segmentLengthSq > 0.0001 ) {
					const double t = std::clamp(
						QM_Vector3DotProductDP( portal.center - segmentStart, segment ) / segmentLengthSq, 0.0, 1.0 );
					const Vector3DP projection = segmentStart + ( segment * t );
					dSq = QM_Vector3Distance2DSqrDP( portal.center, projection );
				}
			}
			if ( dSq < bestDistanceSq ) {
				bestDistanceSq = dSq;
				exitPortalPoint = portal.center;
				exitPortalWidth = portal.width;
				exitPortalOutward = portal.normal;
				hasExitPortal = true;
			}
		}
	}

	/**
	*	Method 3: Geometric fallback for initial corridor chokepoint within 350 units of centroid.
	*	Detects doorway bottleneck when spatial room metadata is missing or unassigned.
	**/
	if ( !hasExitPortal && guidePath.size() >= 2 ) {
		for ( size_t pointIndex = 0; pointIndex + 1 < guidePath.size() && pointIndex < 3; pointIndex++ ) {
			const double distFromCentroid = QM_Vector3Distance2DDP( centroid, guidePath[ pointIndex + 1 ] );
			if ( distFromCentroid > 350.0 ) {
				break;
			}
			const int32_t faceIndex = Nav_FindFaceInLeafStrict( guidePath[ pointIndex + 1 ] );
			if ( faceIndex >= 0 && faceIndex < static_cast<int32_t>( g_nav_faces.size() ) ) {
				const double clearance = g_nav_faces[ faceIndex ].clearance * 2.0;
				if ( clearance > 0.0 && clearance < CROWD_PORTAL_BOTTLENECK_MAX_WIDTH ) {
					exitPortalPoint = guidePath[ pointIndex + 1 ];
					exitPortalWidth = clearance;
					Vector3DP outward = guidePath[ pointIndex + 1 ] - guidePath[ pointIndex ];
					outward.z = 0.0;
					exitPortalOutward = ( QM_Vector3LengthSqrDP( outward ) > 0.0001 )
						? QM_Vector3NormalizeDP( outward )
						: Vector3DP{ 1.0, 0.0, 0.0 };
					hasExitPortal = true;
					break;
				}
			}
		}
	}

	/**
	*	Reject if no doorway portal was identified or if the aperture is sufficiently wide (>= 128u)
	*	that members can exit abreast without wedging.
	**/
	if ( !hasExitPortal || exitPortalWidth >= CROWD_PORTAL_BOTTLENECK_MAX_WIDTH ) {
		return false;
	}

	/**
	*	Ensure the outward normal points away from the squad centroid into the corridor.
	**/
	exitPortalOutward.z = 0.0;
	if ( QM_Vector3LengthSqrDP( exitPortalOutward ) > 0.0001 ) {
		exitPortalOutward = QM_Vector3NormalizeDP( exitPortalOutward );
	} else {
		exitPortalOutward = Vector3DP{ 1.0, 0.0, 0.0 };
	}
	const Vector3DP centroidToPortal = exitPortalPoint - centroid;
	if ( QM_Vector3DotProductDP( centroidToPortal, exitPortalOutward ) < 0.0 ) {
		exitPortalOutward = exitPortalOutward * -1.0;
	}

	/**
	*	Gather living squad members and compute their distance to the exit portal.
	**/
	struct svg_crowd_egress_candidate_t {
		svg_base_edict_t *member = nullptr;
		double distanceToDoorway = 0.0;
	};
	std::vector<svg_crowd_egress_candidate_t> rankedCandidates;
	rankedCandidates.reserve( members.size() );

	// Inspect each squad member to build the candidate egress list.
	for ( svg_base_edict_t *member : members ) {
		if ( member == nullptr || !SVG_Entity_IsActive( member ) || member->health <= 0 ) {
			continue;
		}
		const Vector3DP feet = SVG_GetEntityFeetOriginDP( member );
		const double distance2D = QM_Vector3Distance2DDP( feet, exitPortalPoint );
		rankedCandidates.push_back( { member, distance2D } );
	}

	if ( rankedCandidates.size() <= 1 ) {
		return false;
	}

	/**
	*	Sort members in ascending order of proximity to the exit doorway:
	*	Rank 0 is closest to the exit and steps out first with zero obstruction;
	*	higher ranks are progressively further back and hold station until released.
	**/
	std::sort( rankedCandidates.begin(), rankedCandidates.end(), []( const svg_crowd_egress_candidate_t &a, const svg_crowd_egress_candidate_t &b ) {
		if ( std::fabs( a.distanceToDoorway - b.distanceToDoorway ) > 0.001 ) {
			return a.distanceToDoorway < b.distanceToDoorway;
		}
		return a.member->s.number < b.member->s.number;
	} );

	/**
	*	Commit group egress state.
	**/
	group.hasSerializedEgress = true;
	group.egressPortalOrigin = exitPortalPoint;
	group.egressPortalOutward = exitPortalOutward;
	group.egressPortalHalfWidth = std::max( CROWD_DEFAULT_AGENT_RADIUS, exitPortalWidth * 0.5 );
	group.egressQueueHead = 0;
	group.egressQueueHeadStartTime = level.time;
	group.egressNextReleaseTime = 0_ms;
	group.egressQueueEntityNumbers.clear();
	group.egressQueueEntityNumbers.reserve( rankedCandidates.size() );

	/**
	*	Assign egress queue rank: release the lead member immediately; followers hold in place.
	**/
	for ( size_t rank = 0; rank < rankedCandidates.size(); rank++ ) {
		svg_base_edict_t *member = rankedCandidates[ rank ].member;
		group.egressQueueEntityNumbers.push_back( member->s.number );
		member->crowd.egressQueueRank = static_cast<int32_t>( rank );
		if ( rank == 0 ) {
			member->crowd.egressReleased = true;
			SVG_Crowd_ResetMemberNavigation( member );
		} else {
			member->crowd.egressReleased = false;
		}
	}

	return true;
}

/**
*	@brief	Advance serialized room egress queue when the current head clears the exit doorway, dies, or stalls.
*	@param	group	Active crowd coordination group.
*	@return	True while unreleased egress queue members remain or head is traversing the exit doorway.
*	@note	Strictly O(1) amortized runtime per frame: inspects only the single active queue head entity.
**/
static bool SVG_Crowd_UpdateSerializedEgress( svg_crowd_group_t &group ) {
	/**
	*	Sanity check: validate active egress state before indexed access.
	**/
	if ( !group.hasSerializedEgress ) {
		return false;
	}

	const int32_t queueCount = static_cast<int32_t>( group.egressQueueEntityNumbers.size() );
	if ( group.egressQueueHead < 0 || group.egressQueueHead >= queueCount ) {
		group.hasSerializedEgress = false;
		return false;
	}

	/**
	*	Instant dead member queue shift: if the active queue head is dead or invalid,
	*	immediately advance past it on this exact tick without any delay or timeout.
	**/
	while ( group.egressQueueHead < queueCount ) {
		const int32_t candidateEntityNumber = group.egressQueueEntityNumbers[ group.egressQueueHead ];
		svg_base_edict_t *candidateMember = ( candidateEntityNumber >= 1 && candidateEntityNumber < globals.edictPool->num_edicts )
			? g_edict_pool.EdictForNumber( candidateEntityNumber )
			: nullptr;

		// If this member is alive and active, it is the valid active queue head.
		if ( candidateMember != nullptr && SVG_Entity_IsActive( candidateMember ) && candidateMember->health > 0 ) {
			break;
		}

		// Active member is dead or removed: shift queue immediately on this tick.
		group.egressQueueHead++;
		group.egressQueueHeadStartTime = level.time;

		// Release the next member in line immediately if one exists.
		if ( group.egressQueueHead < queueCount ) {
			const int32_t nextEntityNumber = group.egressQueueEntityNumbers[ group.egressQueueHead ];
			svg_base_edict_t *nextMember = ( nextEntityNumber >= 1 && nextEntityNumber < globals.edictPool->num_edicts )
				? g_edict_pool.EdictForNumber( nextEntityNumber )
				: nullptr;
			if ( nextMember != nullptr && SVG_Entity_IsActive( nextMember ) && nextMember->health > 0 ) {
				nextMember->crowd.egressReleased = true;
				SVG_Crowd_ResetMemberNavigation( nextMember );
			}
		}
	}

	// If all members were traversed or dead, room egress is complete.
	if ( group.egressQueueHead >= queueCount ) {
		group.hasSerializedEgress = false;
		return false;
	}

	const int32_t activeEntityNumber = group.egressQueueEntityNumbers[ group.egressQueueHead ];
	svg_base_edict_t *activeMember = g_edict_pool.EdictForNumber( activeEntityNumber );
	if ( activeMember == nullptr ) {
		group.hasSerializedEgress = false;
		return false;
	}

	/**
	*	Calculate analytical outward depth of active head beyond the exit doorway plane.
	**/
	const Vector3DP activeFeet = SVG_GetEntityFeetOriginDP( activeMember );
	const double outwardDepth = QM_Vector3DotProductDP( activeFeet - group.egressPortalOrigin, group.egressPortalOutward );
	constexpr double minClearanceDepth = 16.0;
	const double requiredClearance = std::max( minClearanceDepth, CROWD_DEFAULT_AGENT_RADIUS * CROWD_EGRESS_CLEARANCE_RADIUS_SCALE );

	bool releaseNext = false;
	if ( outwardDepth >= requiredClearance ) {
		// Active head has physically cleared the doorway aperture into the corridor.
		releaseNext = true;
	} else {
		// Watchdog fail-safes for alive-but-wedged members:
		const bool activeStalled = activeMember->crowd.blockedStartTime.Milliseconds() > 0 &&
			( level.time - activeMember->crowd.blockedStartTime ) >= CROWD_EGRESS_ACTIVE_STALL_TIMEOUT;
		const bool transitTimedOut = group.egressQueueHeadStartTime.Milliseconds() > 0 &&
			( level.time - group.egressQueueHeadStartTime ) >= CROWD_EGRESS_TRANSIT_TIMEOUT;

		if ( activeStalled || transitTimedOut ) {
			releaseNext = true;
		}
	}

	/**
	*	Advance to next queue rank if doorway clearance or watchdog timeout is satisfied.
	**/
	if ( releaseNext ) {
		group.egressQueueHead++;
		group.egressQueueHeadStartTime = level.time;

		if ( group.egressQueueHead < queueCount ) {
			const int32_t nextEntityNumber = group.egressQueueEntityNumbers[ group.egressQueueHead ];
			svg_base_edict_t *nextMember = ( nextEntityNumber >= 1 && nextEntityNumber < globals.edictPool->num_edicts )
				? g_edict_pool.EdictForNumber( nextEntityNumber )
				: nullptr;

			if ( nextMember != nullptr && SVG_Entity_IsActive( nextMember ) && nextMember->health > 0 ) {
				nextMember->crowd.egressReleased = true;
				SVG_Crowd_ResetMemberNavigation( nextMember );
			}
			gi.dprintf( "[crowd egress release] crowd=%" PRId32 " rank=%" PRId32 " ent=%" PRId32 " time=%" PRId64 "ms\n",
				group.crowdID, group.egressQueueHead, nextEntityNumber, level.time.Milliseconds() );
			return true;
		} else {
			// All squad members have been released AND the final member has cleared the exit doorway!
			group.hasSerializedEgress = false;
			gi.dprintf( "[crowd egress complete] crowd=%" PRId32 " all members cleared exit room time=%" PRId64 "ms\n",
				group.crowdID, level.time.Milliseconds() );
			return false;
		}
	}

	return true;
}

/**
*	Public Lifecycle Functions:
**/

/**
*	@brief	Initialize crowd management subsystems and cvars.
**/
void SVG_Crowd_Init( void ) {
	s_crowd_debug_draw = gi.cvar( "s_crowd_debug_draw", "0", 0 );
	s_crowd_lateral_spacing = gi.cvar( "s_crowd_lateral_spacing", "64", 0 );
	s_crowd_longitudinal_spacing = gi.cvar( "s_crowd_longitudinal_spacing", "64", 0 );
	s_crowd_cover_max_dist = gi.cvar( "s_crowd_cover_max_dist", "768", 0 );

	g_crowd_groups.clear();
	SVG_Squad_Init();
}

/**
*	@brief	Shutdown crowd management subsystems and release all reservations.
**/
void SVG_Crowd_Shutdown( void ) {
	// Release all tactical cover claims held by any crowd entities.
	for ( int32_t i = 1; i < globals.edictPool->num_edicts; i++ ) {
		svg_base_edict_t *ent = g_edict_pool.EdictForNumber( i );
		if ( SVG_Entity_IsActive( ent ) && ent->crowd.activeCoverIdx >= 0 ) {
			Nav_ReleaseCoverPoint( ent->crowd.activeCoverIdx, ent->s.number );
			ent->crowd.activeCoverIdx = -1;
		}
	}

	g_crowd_groups.clear();
	SVG_Squad_Shutdown();
}

/**
*	@brief	Synchronize crowd group registries with active entities in the edict pool.
*	@note	Invoked after map spawn and savegame load to rebuild crowd group records
*			from entity states without wiping individual member assignments.
**/
void SVG_Crowd_SyncFromEntities( void ) {
	/**
	*	Iterate through all active entities in the pool and register any
	*	entities configured with a non-negative crowd identifier.
	**/
	// Iterate through all potential edicts allocated in the pool.
	for ( int32_t i = 1; i < globals.edictPool->num_edicts; i++ ) {
		svg_base_edict_t *ent = g_edict_pool.EdictForNumber( i );

		// Skip invalid, inactive, unassigned, or deceased entities.
		if ( !ent || !SVG_Entity_IsActive( ent ) || ent->crowd.crowdID < 0 || ent->health <= 0 ) {
			continue;
		}

		const int32_t cid = ent->crowd.crowdID;

		/**
		*	Ensure crowd group record exists in global registry.
		**/
		// Check if group record is missing from registry and instantiate if necessary.
		if ( g_crowd_groups.find( cid ) == g_crowd_groups.end() ) {
			svg_crowd_group_t group;
			group.crowdID = cid;
			group.style = ( cid == 0 ) ? crowd_chase_target_type_t::CROWD_STYLE_SURROUND_PERIMETER : crowd_chase_target_type_t::CROWD_STYLE_ARROW;
			group.leaderEntityNumber = ( ent->crowd.role == crowd_member_role_t::ROLE_LEADER ) ? ent->s.number : ent->crowd.leaderEntityNumber;
			g_crowd_groups[ cid ] = group;
		} else if ( ent->crowd.role == crowd_member_role_t::ROLE_LEADER ) {
			// Explicitly assign designated squad leader to group registry.
			g_crowd_groups[ cid ].leaderEntityNumber = ent->s.number;
			ent->crowd.leaderEntityNumber = ent->s.number;
		} else if ( ent->crowd.leaderEntityNumber != ENTITYNUM_NONE && g_crowd_groups[ cid ].leaderEntityNumber == ENTITYNUM_NONE ) {
			// Restore group leader designation from entity state if not already set.
			g_crowd_groups[ cid ].leaderEntityNumber = ent->crowd.leaderEntityNumber;
		}

		/**
		*	Track entity number in group memberEntityNumbers list.
		**/
		// Register entity number in cached group member list if not already tracked.
		auto &membersList = g_crowd_groups[ cid ].memberEntityNumbers;
		if ( std::find( membersList.begin(), membersList.end(), ent->s.number ) == membersList.end() ) {
			membersList.push_back( ent->s.number );
		}

		/**
		*	Update monster entity custom skin visuals to match crowd group.
		**/
		// Apply custom skin if this entity derives from svg_monster_base_t.
		if ( ent->GetTypeInfo()->IsSubClassType<svg_monster_base_t>() ) {
			ent->s.renderfx |= RF_CUSTOMSKIN;
			ent->s.skinnum = svg_monster_testdummy_debug_t::GetCrowdSkinImageIndex( cid );
		}
	}
}

/**
*	@brief	Register an entity as a member of a specific crowd group by entity number.
*	@param	entityNumber	Entity number to register.
*	@param	crowdID			Crowd identifier.
**/
void SVG_Crowd_RegisterMember( const int32_t entityNumber, const int32_t crowdID ) {
	if ( entityNumber < 1 || entityNumber >= globals.edictPool->num_edicts ) {
		return;
	}
	svg_base_edict_t *ent = g_edict_pool.EdictForNumber( entityNumber );
	SVG_Crowd_RegisterMember( ent, crowdID );
}

/**
*	@brief	Register an entity as a member of a specific crowd group.
*	@param	ent		Entity to register.
*	@param	crowdID	Crowd identifier.
**/
void SVG_Crowd_RegisterMember( svg_base_edict_t *ent, const int32_t crowdID ) {
	if ( !ent ) {
		return;
	}

	// If already registered to another crowd, clean up previous membership first.
	if ( ent->crowd.crowdID >= 0 && ent->crowd.crowdID != crowdID ) {
		SVG_Crowd_UnregisterMember( ent );
	}

	ent->crowd.crowdID = crowdID;
	ent->crowd.slotIndex = -1;
	ent->crowd.role = ( crowdID == 0 ) ? crowd_member_role_t::ROLE_CIVILIAN_WANDERER : crowd_member_role_t::ROLE_UNASSIGNED;
	ent->crowd.leaderEntityNumber = ENTITYNUM_NONE;
	ent->crowd.reachedGoal = false;
	ent->crowd.ingressQueueRank = -1;
	ent->crowd.ingressReleased = true;
	ent->crowd.egressQueueRank = -1;
	ent->crowd.egressReleased = true;
	ent->crowd.activeCoverIdx = -1;
	ent->crowd.startTimeForSeeking = level.time;
	ent->crowd.lastPathCalcTime = 0_ms;

	/**
	*	Update custom skin for monster entities based on crowd membership.
	**/
	if ( ent && ent->GetTypeInfo()->IsSubClassType<svg_monster_base_t>() ) {
		ent->s.renderfx |= RF_CUSTOMSKIN;
		ent->s.skinnum = svg_monster_testdummy_debug_t::GetCrowdSkinImageIndex( crowdID );
	}

	// Ensure group record exists in registry.
	if ( crowdID >= 0 ) {
		if ( g_crowd_groups.find( crowdID ) == g_crowd_groups.end() ) {
			svg_crowd_group_t group;
			group.crowdID = crowdID;
			group.style = ( crowdID == 0 ) ? crowd_chase_target_type_t::CROWD_STYLE_SURROUND_PERIMETER : crowd_chase_target_type_t::CROWD_STYLE_ARROW;
			g_crowd_groups[ crowdID ] = group;
		}

		// Register entity number in cached group member list if not already tracked.
		auto &membersList = g_crowd_groups[ crowdID ].memberEntityNumbers;
		if ( std::find( membersList.begin(), membersList.end(), ent->s.number ) == membersList.end() ) {
			membersList.push_back( ent->s.number );
		}

		// Sync with decentralized tactical squad coordinator.
		SVG_Squad_RegisterMember( crowdID, ent->s.number );
	}
}

/**
*	@brief	Unregister an entity from its active crowd group by entity number.
*	@param	entityNumber	Entity number to unregister.
**/
void SVG_Crowd_UnregisterMember( const int32_t entityNumber ) {
	if ( entityNumber < 1 || entityNumber >= globals.edictPool->num_edicts ) {
		return;
	}
	svg_base_edict_t *ent = g_edict_pool.EdictForNumber( entityNumber );
	SVG_Crowd_UnregisterMember( ent );
}

/**
*	@brief	Unregister an entity from its active crowd group and release any cover leases.
*	@param	ent	Entity to unregister.
**/
void SVG_Crowd_UnregisterMember( svg_base_edict_t *ent ) {
	if ( !ent ) {
		return;
	}

	if ( ent->crowd.activeCoverIdx >= 0 ) {
		Nav_ReleaseCoverPoint( ent->crowd.activeCoverIdx, ent->s.number );
		ent->crowd.activeCoverIdx = -1;
	}

	// Remove from group memberEntityNumbers list.
	const int32_t oldCrowdID = ent->crowd.crowdID;
	if ( oldCrowdID >= 0 ) {
		SVG_Squad_UnregisterMember( oldCrowdID, ent->s.number );
		auto it = g_crowd_groups.find( oldCrowdID );
		if ( it != g_crowd_groups.end() ) {
			auto &membersList = it->second.memberEntityNumbers;
			membersList.erase( std::remove( membersList.begin(), membersList.end(), ent->s.number ), membersList.end() );
		}
	}

	ent->crowd.crowdID = -1;
	ent->crowd.slotIndex = -1;
	ent->crowd.role = crowd_member_role_t::ROLE_UNASSIGNED;
	ent->crowd.leaderEntityNumber = ENTITYNUM_NONE;
	ent->crowd.reachedGoal = false;
	ent->crowd.ingressQueueRank = -1;
	ent->crowd.ingressReleased = true;
	ent->crowd.egressQueueRank = -1;
	ent->crowd.egressReleased = true;

	/**
	*	Reset custom skin for monster entities to neutral grey upon unregistering.
	**/
	if ( ent && ent->GetTypeInfo()->IsSubClassType<svg_monster_base_t>() ) {
		ent->s.renderfx |= RF_CUSTOMSKIN;
		ent->s.skinnum = svg_monster_testdummy_debug_t::GetCrowdSkinImageIndex( -1 );
	}
}

/**
*	@brief	Retrieve entity numbers of all living members in a specific crowd.
*	@param	crowdID			Crowd identifier.
*	@param	outEntityNums	[out] Vector to receive member entity numbers.
**/
void SVG_Crowd_GetCrowdMembers( const int32_t crowdID, std::vector<int32_t> &outEntityNums ) {
	outEntityNums.clear();
	if ( crowdID < 0 ) {
		return;
	}

	const svg_crowd_group_t *group = SVG_Crowd_GetGroup( crowdID );
	if ( !group ) {
		return;
	}

	outEntityNums.reserve( group->memberEntityNumbers.size() );
	for ( const int32_t num : group->memberEntityNumbers ) {
		if ( num < 1 || num >= globals.edictPool->num_edicts ) {
			continue;
		}
		const svg_base_edict_t *ent = g_edict_pool.EdictForNumber( num );
		if ( ent && SVG_Entity_IsActive( ent ) && ent->crowd.crowdID == crowdID && ent->health > 0 ) {
			outEntityNums.push_back( num );
		}
	}
}

/**
*	@brief	Retrieve living members of a specific crowd.
*	@param	crowdID		Crowd identifier.
*	@param	outMembers	[out] Vector to receive member entity pointers.
**/
void SVG_Crowd_GetCrowdMembers( const int32_t crowdID, std::vector<svg_base_edict_t*> &outMembers ) {
	outMembers.clear();
	if ( crowdID < 0 ) {
		return;
	}

	const svg_crowd_group_t *group = SVG_Crowd_GetGroup( crowdID );
	if ( !group ) {
		return;
	}

	outMembers.reserve( group->memberEntityNumbers.size() );
	for ( const int32_t num : group->memberEntityNumbers ) {
		if ( num < 1 || num >= globals.edictPool->num_edicts ) {
			continue;
		}
		svg_base_edict_t *ent = g_edict_pool.EdictForNumber( num );
		if ( ent && SVG_Entity_IsActive( ent ) && ent->crowd.crowdID == crowdID && ent->health > 0 ) {
			outMembers.push_back( ent );
		}
	}
}

/**
*	@brief	Designate a squad member as the squad leader by entity number.
*	@param	crowdID				Crowd identifier.
*	@param	leaderEntityNumber	Entity number to set as leader (or ENTITYNUM_NONE to clear).
**/
void SVG_Crowd_SetLeader( const int32_t crowdID, const int32_t leaderEntityNumber ) {
	svg_crowd_group_t *group = SVG_Crowd_GetGroup( crowdID );
	if ( !group ) {
		return;
	}
	group->leaderEntityNumber = leaderEntityNumber;
	SVG_Squad_SetLeader( crowdID, leaderEntityNumber );

	std::vector<svg_base_edict_t*> members;
	SVG_Crowd_GetCrowdMembers( crowdID, members );
	for ( svg_base_edict_t *member : members ) {
		member->crowd.leaderEntityNumber = leaderEntityNumber;
		if ( leaderEntityNumber != ENTITYNUM_NONE && member->s.number == leaderEntityNumber ) {
			member->crowd.role = crowd_member_role_t::ROLE_LEADER;
		} else if ( member->crowd.role == crowd_member_role_t::ROLE_LEADER ) {
			member->crowd.role = crowd_member_role_t::ROLE_CENTER;
		}
	}
}

/**
*	@brief	Designate a squad member as the squad leader by edict pointer.
*	@param	crowdID		Crowd identifier.
*	@param	leaderEnt	Entity to set as leader.
**/
void SVG_Crowd_SetLeader( const int32_t crowdID, svg_base_edict_t *leaderEnt ) {
	SVG_Crowd_SetLeader( crowdID, leaderEnt ? leaderEnt->s.number : ENTITYNUM_NONE );
}

/**
*	@brief	Compute mutual separation steering push force from fellow crowd members by entity number.
*	@param	entityNumber		Query entity number.
*	@param	outSeparationForce	[out] Vector to receive 2D repulsive displacement vector in Vector3DP.
*	@return	True if a non-zero separation force was calculated.
**/
bool SVG_Crowd_ComputeMutualSeparation( const int32_t entityNumber, Vector3DP *outSeparationForce ) {
	if ( !outSeparationForce || entityNumber < 1 || entityNumber >= globals.edictPool->num_edicts ) {
		return false;
	}
	*outSeparationForce = Vector3DP{ 0.0, 0.0, 0.0 };

	const svg_base_edict_t *selfEnt = g_edict_pool.EdictForNumber( entityNumber );
	if ( !selfEnt || !SVG_Entity_IsActive( selfEnt ) || selfEnt->crowd.crowdID < 0 ) {
		return false;
	}

	const int32_t crowdID = selfEnt->crowd.crowdID;
	const svg_crowd_group_t *group = SVG_Crowd_GetGroup( crowdID );
	if ( !group || group->memberEntityNumbers.empty() ) {
		return false;
	}

	const double rawRadiusSelf = static_cast<double>( ( selfEnt->maxs.x - selfEnt->mins.x ) * 0.5f );
	const double selfRadius = ( rawRadiusSelf > 0.0 ) ? rawRadiusSelf : CROWD_DEFAULT_AGENT_RADIUS;

	const double sepStrength = ( group->params.separationStrength > 0.0 ) ? group->params.separationStrength : CROWD_DEFAULT_SEPARATION_STRENGTH;

	Vector3DP separationAcc{ 0.0, 0.0, 0.0 };
	int32_t neighborCount = 0;
	const Vector3DP myOrigin( selfEnt->currentOrigin );

	for ( const int32_t otherNum : group->memberEntityNumbers ) {
		if ( otherNum == entityNumber ) {
			continue;
		}
		if ( otherNum < 1 || otherNum >= globals.edictPool->num_edicts ) {
			continue;
		}
		const svg_base_edict_t *other = g_edict_pool.EdictForNumber( otherNum );
		if ( !other || !SVG_Entity_IsActive( other ) || other->health <= 0 ) {
			continue;
		}

		// The doorway reservation owner must never yield to unreleased exterior waiters.
		if ( group->hasSerializedIngress && selfEnt->crowd.ingressReleased && !other->crowd.ingressReleased ) {
			continue;
		}
		// Arrived station-keeping members hold their plotted slot and zero their velocity
		// every think; if they also act as a separation source they perpetually repel
		// their ring neighbors off-slot (root cause of the arrived-crowd jitter). Skip
		// them here: en-route agents still route around them via pathing/deflection.
		if ( other->crowd.reachedGoal ) {
			continue;
		}

		const double rawRadiusOther = static_cast<double>( ( other->maxs.x - other->mins.x ) * 0.5f );
		const double otherRadius = ( rawRadiusOther > 0.0 ) ? rawRadiusOther : CROWD_DEFAULT_AGENT_RADIUS;
		const double combinedHullRadius = selfRadius + otherRadius;
		const double sepRadius = ( group->params.separationRadius > 0.0 ) ? group->params.separationRadius : ( combinedHullRadius * 2.0 );
		const double sepRadiusSq = sepRadius * sepRadius;

		const Vector3DP otherOrigin( other->currentOrigin );
		Vector3DP diff = myOrigin - otherOrigin;
		diff.z = 0.0;
		const double distSq = QM_Vector3LengthSqrDP( diff );

		// A released egress member must not be repelled by an unreleased teammate holding station in the room,
		// unless their physical collision hulls are actively intersecting/overlapping (distSq < combinedHullRadiusSq).
		// When overlapping, soft separation force must push the released agent clear to break the allsolid trap.
		if ( group->hasSerializedEgress && selfEnt->crowd.egressReleased && !other->crowd.egressReleased ) {
			if ( distSq >= ( combinedHullRadius * combinedHullRadius ) ) {
				continue;
			}
		}

		if ( distSq < sepRadiusSq && distSq > 0.0001 ) {
			const double dist = std::sqrt( distSq );
			const double pushWeight = ( sepRadius - dist ) / sepRadius;
			const Vector3DP pushDir = diff * ( 1.0 / dist );
			separationAcc = separationAcc + ( pushDir * pushWeight );
			neighborCount++;
		}
	}

	if ( neighborCount > 0 && QM_Vector3LengthSqrDP( separationAcc ) > 0.0001 ) {
		*outSeparationForce = QM_Vector3NormalizeDP( separationAcc ) * sepStrength;
		return true;
	}

	return false;
}

/**
*	@brief	Compute mutual separation steering push force from fellow crowd members by edict pointer.
*	@param	ent					Query entity.
*	@param	outSeparationForce	[out] Vector to receive 2D repulsive displacement vector in Vector3DP.
*	@return	True if a non-zero separation force was calculated.
**/
bool SVG_Crowd_ComputeMutualSeparation( const svg_base_edict_t *ent, Vector3DP *outSeparationForce ) {
	if ( !ent ) {
		return false;
	}
	return SVG_Crowd_ComputeMutualSeparation( ent->s.number, outSeparationForce );
}

/**
*	@brief		Compute speed throttling scale for trailing squad members to yield to leading teammates in narrow corridors.
*	@details	Projects displacements to teammates onto our forward movement direction. If a teammate is directly ahead
*				in our travel corridor (within half-width lateral threshold), scales speed down smoothly to 0.0 as distance
*				approaches CROWD_FOLLOW_MIN_SEPARATION to prevent chokepoint queuing jams.
*	@param	entityNumber			Query entity number.
*	@param	moveDir				Normalized 2D horizontal movement direction towards active waypoint.
*	@param	outSpeedScale			[out] Multiplier applied to frame velocity [0.0..1.0] to maintain following distance.
*	@param	outStrictQueueing		[out] Optional flag set when throttling was caused by strict bottleneck/doorway single-file queueing.
*	@return	True if a leading teammate was found directly ahead in the travel corridor.
**/
bool SVG_Crowd_ComputeTeammateFollowSpeedScale( const int32_t entityNumber, const Vector3DP &moveDir, double *outSpeedScale, bool *outStrictQueueing ) {
	if ( outStrictQueueing != nullptr ) {
		*outStrictQueueing = false;
	}
	if ( !outSpeedScale || entityNumber < 1 || entityNumber >= globals.edictPool->num_edicts ) {
		return false;
	}
	*outSpeedScale = 1.0;

	const svg_base_edict_t *selfEnt = g_edict_pool.EdictForNumber( entityNumber );
	if ( !selfEnt || !SVG_Entity_IsActive( selfEnt ) || selfEnt->crowd.crowdID < 0 ) {
		return false;
	}

	const int32_t crowdID = selfEnt->crowd.crowdID;
	const svg_crowd_group_t *group = SVG_Crowd_GetGroup( crowdID );
	if ( !group || group->memberEntityNumbers.empty() ) {
		return false;
	}

	const Vector3DP myOrigin( selfEnt->currentOrigin );
	double minScale = 1.0;
	bool foundLeaderAhead = false;
	bool strictQueueing = false;

	static constexpr double corridorLateralSqr = CROWD_CORRIDOR_LATERAL_THRESHOLD * CROWD_CORRIDOR_LATERAL_THRESHOLD;
	static constexpr double maxAheadDist = CROWD_FOLLOW_MIN_SEPARATION + CROWD_FOLLOW_SLOWDOWN_RANGE;

	const double rawRadiusSelf = static_cast<double>( ( selfEnt->maxs.x - selfEnt->mins.x ) * 0.5f );
	const double selfRadius = ( rawRadiusSelf > 0.0 ) ? rawRadiusSelf : CROWD_DEFAULT_AGENT_RADIUS;
	const int32_t mySlot = ( selfEnt->crowd.slotIndex >= 0 ) ? selfEnt->crowd.slotIndex : entityNumber;
	const svg_monster_base_t *selfMonster = dynamic_cast<const svg_monster_base_t*>( selfEnt );

	const size_t selfWpIdx = ( selfMonster != nullptr && !selfMonster->stringPulledPath.empty() ) ?
		std::min( selfMonster->stringPathPos, selfMonster->stringPulledPath.size() - 1 ) : 0;
	const bool hasSelfWp = ( selfMonster != nullptr && !selfMonster->stringPulledPath.empty() );
	const Vector3DP &selfWp = hasSelfWp ? selfMonster->stringPulledPath[ selfWpIdx ] : myOrigin;
	const double distSelfToWp = hasSelfWp ? QM_Vector3Distance2DDP( myOrigin, selfWp ) : 999999.0;

	// Precompute whether self origin and active waypoint reside in a narrow corridor (< 96 units).
	// Computing this once per agent outside the teammate loop guarantees strict O(1) pairwise arbitration.
	const int32_t myFaceIdx = Nav_FindFaceInLeafStrict( myOrigin );
	const bool isMyFaceNarrow = ( myFaceIdx >= 0 && myFaceIdx < static_cast<int32_t>( g_nav_faces.size() ) ) &&
		( g_nav_faces[ myFaceIdx ].clearance < CROWD_MIN_TWO_AGENT_ABREAST_CLEARANCE );

	const int32_t wpFaceIdx = hasSelfWp ? Nav_FindFaceInLeafStrict( selfWp ) : -1;
	const bool isWpFaceNarrow = ( wpFaceIdx >= 0 && wpFaceIdx < static_cast<int32_t>( g_nav_faces.size() ) ) &&
		( g_nav_faces[ wpFaceIdx ].clearance < CROWD_MIN_TWO_AGENT_ABREAST_CLEARANCE );
	// Self is only traversing a bottleneck if physically standing on a narrow face (< 96u)
	// or approaching within the immediate bottleneck funnel (<= 80u).
	// Distant agents in an open room (> 80u) are not constrained by the bottleneck.
	const bool isNearNarrowWp = isWpFaceNarrow && ( distSelfToWp <= CROWD_BOTTLENECK_FUNNEL_DIST );
	const bool isBottleneckTraverse = isMyFaceNarrow || isNearNarrowWp;

	for ( const int32_t otherNum : group->memberEntityNumbers ) {
		if ( otherNum == entityNumber ) {
			continue;
		}
		if ( otherNum < 1 || otherNum >= globals.edictPool->num_edicts ) {
			continue;
		}
		const svg_base_edict_t *other = g_edict_pool.EdictForNumber( otherNum );
		if ( !other || !SVG_Entity_IsActive( other ) || other->health <= 0 ) {
			continue;
		}

		// The active reservation owner proceeds through the portal without yielding to staged waiters.
		if ( group->hasSerializedIngress && selfEnt->crowd.ingressReleased && !other->crowd.ingressReleased ) {
			continue;
		}
		// An unreleased egress member is holding station in formation; do not yield to an unreleased teammate behind us in the egress queue.
		if ( group->hasSerializedEgress && selfEnt->crowd.egressReleased && !other->crowd.egressReleased ) {
			continue;
		}
		const double rawRadiusOther = static_cast<double>( ( other->maxs.x - other->mins.x ) * 0.5f );
		const double otherRadius = ( rawRadiusOther > 0.0 ) ? rawRadiusOther : CROWD_DEFAULT_AGENT_RADIUS;
		const double combinedRadius = selfRadius + otherRadius;

		const Vector3DP otherOrigin( other->currentOrigin );
		Vector3DP diff = otherOrigin - myOrigin;
		diff.z = 0.0;
		const double distSq = QM_Vector3LengthSqrDP( diff );

		// If teammate has already reached their assigned formation slot and is holding station:
		if ( other->crowd.reachedGoal ) {
			const double aheadDist = diff.x * moveDir.x + diff.y * moveDir.y;
			if ( aheadDist > 0.0 && aheadDist < combinedRadius ) {
				const double lateralDistSq = distSq - ( aheadDist * aheadDist );
				// Yield to rolling crawl speed so moving teammates can smoothly slide around arrived station-keeping teammates:
				if ( lateralDistSq < ( combinedRadius * combinedRadius * 0.75 ) ) {
					minScale = std::min( minScale, CROWD_FOLLOW_ROLLING_MIN_SCALE );
					foundLeaderAhead = true;
				}
			}
			continue;
		}

		// 1. Dynamic Bottleneck & Passage Waypoint Convergence Arbitration:
		// When entering or exiting a room through a narrow doorway/passage (< 96 units), multiple agents
		// converging from different angles must zipper in single file without jamming against the door jambs.
		const svg_monster_base_t *otherMonster = dynamic_cast<const svg_monster_base_t*>( other );

		if ( hasSelfWp && otherMonster != nullptr && !otherMonster->stringPulledPath.empty() && ( isWpFaceNarrow || isMyFaceNarrow ) ) {
			const size_t otherWpIdx = std::min( otherMonster->stringPathPos, otherMonster->stringPulledPath.size() - 1 );
			const Vector3DP &otherWp = otherMonster->stringPulledPath[ otherWpIdx ];

			// If active waypoints converge on the same passage bottleneck (< 96 units apart):
			if ( QM_Vector3Distance2DSqrDP( selfWp, otherWp ) < CROWD_BOTTLENECK_WAYPOINT_PROXIMITY_SQR ) {
				const double distOtherToWp = QM_Vector3Distance2DDP( otherOrigin, otherWp );

				const bool hasEgressRank = ( group->hasSerializedEgress && selfEnt->crowd.egressQueueRank >= 0 && other->crowd.egressQueueRank >= 0 );
				const bool hasIngressRank = ( group->hasSerializedIngress && selfEnt->crowd.ingressQueueRank >= 0 && other->crowd.ingressQueueRank >= 0 );
				const bool otherHasQueuePriority = hasEgressRank ? ( other->crowd.egressQueueRank < selfEnt->crowd.egressQueueRank ) :
					( hasIngressRank ? ( other->crowd.ingressQueueRank < selfEnt->crowd.ingressQueueRank ) : ( otherNum < entityNumber ) );

				/**
				*	Side-aware right-of-way arbitration (priority-inversion free).
				*	Classify each agent by which side of the aperture plane it currently occupies
				*	using the signed projection onto the inward flow direction. An agent that has
				*	already crossed past the doorway waypoint proceeds unconditionally and is NEVER
				*	yielded to once it has cleared the physical aperture (< 48u). Among agents on the
				*	same side, the one physically nearer the shared waypoint proceeds first, queue
				*	rank and entity number breaking ties.
				**/
				const Vector3DP selfToWp = selfWp - myOrigin;
				const Vector3DP otherToWp = otherWp - otherOrigin;
				const double selfAlong = QM_Vector3DotProductDP( selfToWp, moveDir );
				const double otherAlong = QM_Vector3DotProductDP( otherToWp, moveDir );
				// Positive = waypoint still ahead (still approaching/crossing); negative = past it (inside).
				const bool selfInside = ( selfAlong < 0.0 );
				const bool otherInside = ( otherAlong < 0.0 );

				bool otherHasRightOfWay = false;
				if ( selfInside != otherInside ) {
					// Only yield to an insider that is STILL physically inside the aperture.
					// Once the leader clears the aperture (>= 48u), the doorway is open.
					otherHasRightOfWay = otherInside && ( distOtherToWp < CROWD_BOTTLENECK_APERTURE_CLEAR_DIST );
				} else {
					// Same side: strictly nearer to the shared waypoint wins (priority-inversion free)!
					if ( distOtherToWp < ( distSelfToWp - CROWD_BOTTLENECK_DISTANCE_EPSILON ) ) {
						otherHasRightOfWay = true;
					} else if ( distSelfToWp < ( distOtherToWp - CROWD_BOTTLENECK_DISTANCE_EPSILON ) ) {
						otherHasRightOfWay = false;
					} else {
						otherHasRightOfWay = otherHasQueuePriority;
					}
				}

				// If other teammate is blocked by world geometry, do not yield right-of-way to a stuck agent:
				const bool otherIsBlocked = other->crowd.blockedStartTime.Milliseconds() > 0 &&
					( level.time - other->crowd.blockedStartTime ) >= 500_ms;
				if ( otherIsBlocked ) {
					otherHasRightOfWay = false;
				}

				if ( otherHasRightOfWay ) {
					/**
					*	Queue-slot cork exemption:
					*	Members assigned an overflow-queue slot OUTSIDE the room (goal deeper than the
					*	shared bottleneck waypoint, measured along the leader's path direction) must
					*	never hard-stop at the aperture. Halting them parks their hull inside the
					*	ingress lane and corks the doorway for every interior-assigned member,
					*	producing a permanent mutual-yield deadlock. They instead keep crawling to
					*	their flank queue position off the corridor.
					**/
					bool selfIsExteriorQueue = false;
					if ( selfEnt->crowd.slotIndex >= 0 && selfEnt->crowd.slotIndex < static_cast<int32_t>( group->slots.size() ) ) {
						const svg_crowd_slot_t &mySlotDef = group->slots[ selfEnt->crowd.slotIndex ];
						const Vector3DP wpToSlot = mySlotDef.worldPosition - selfWp;
						const double along = QM_Vector3DotProductDP( wpToSlot, moveDir );
						// Exterior queue slot: goal lies clearly BEHIND the bottleneck waypoint
						// relative to the ingress flow direction.
						selfIsExteriorQueue = ( along < -CROWD_BOTTLENECK_STOP_HEADWAY_DIST );
					}

					// Leading teammate is currently inside the doorway aperture:
					const bool leaderInAperture = ( distOtherToWp < CROWD_BOTTLENECK_APERTURE_CLEAR_DIST );

					if ( !selfIsExteriorQueue && leaderInAperture && distSelfToWp <= CROWD_BOTTLENECK_THRESHOLD_HOLD_DIST ) {
						// Self is at the doorway threshold: hold outside the aperture until leader clears!
						foundLeaderAhead = true;
						strictQueueing = true;
						minScale = 0.0;
						continue;
					} else if ( distSelfToWp <= CROWD_BOTTLENECK_FUNNEL_DIST ) {
						// Self is approaching in the funnel: decelerate to single-file crawl speed
						// so the leader pulls ahead and agents zipper into single file before reaching the doorway!
						foundLeaderAhead = true;
						minScale = std::min( minScale, CROWD_BOTTLENECK_CRAWL_SPEED_SCALE );
						continue;
					}
				}
			}
		}

		// Project displacement onto our forward movement direction to determine longitudinal distance ahead.
		const double aheadDist = diff.x * moveDir.x + diff.y * moveDir.y;
		const double zDelta = std::fabs( otherOrigin.z - myOrigin.z );

		// Check if we or the teammate are currently traversing a narrow corridor or tight staircase:
		const bool isNarrowOrStairs = isBottleneckTraverse || ( zDelta >= static_cast<double>( NAV_MAX_STEP_HEIGHT ) );

		// Abreast deconfliction: when entering a narrow corridor or tight bottleneck (< 96 units),
		// or when converging on a shared doorway/passage, two agents abreast cannot both fit.
		// The lower-priority member yields to let the nearer/lead rank enter the chokepoint first in single file.
		const bool isHorizontalAbreast = ( zDelta < NAV_STEP_MIN_VERTICAL_DELTA );
		const bool isBottleneckConvergence = ( hasSelfWp && otherMonster != nullptr &&
			!otherMonster->stringPulledPath.empty() && ( isWpFaceNarrow || isMyFaceNarrow ) &&
			( QM_Vector3Distance2DSqrDP( selfWp, otherMonster->stringPulledPath[ std::min( otherMonster->stringPathPos, otherMonster->stringPulledPath.size() - 1 ) ] ) < CROWD_BOTTLENECK_WAYPOINT_PROXIMITY_SQR ) );
		// Bottleneck convergence only funnels agents when within the funnel distance (<= 140 units) to the chokepoint:
		const bool isInBottleneckFunnel = isBottleneckConvergence && ( distSelfToWp <= CROWD_BOTTLENECK_FUNNEL_DIST );
		const bool isNarrowFunnel = isNarrowOrStairs || isInBottleneckFunnel;

		if ( isNarrowFunnel && isHorizontalAbreast && distSq < CROWD_ABREAST_DECONFLICT_DIST_SQR && aheadDist >= -CROWD_ABREAST_AHEAD_TOLERANCE_LONGITUDINAL && aheadDist < combinedRadius ) {
			const bool hasEgressRank = ( group->hasSerializedEgress && selfEnt->crowd.egressQueueRank >= 0 && other->crowd.egressQueueRank >= 0 );
			const bool hasIngressRank = ( group->hasSerializedIngress && selfEnt->crowd.ingressQueueRank >= 0 && other->crowd.ingressQueueRank >= 0 );
			const bool otherHasPriority = hasEgressRank ? ( other->crowd.egressQueueRank < selfEnt->crowd.egressQueueRank ) :
				( hasIngressRank ? ( other->crowd.ingressQueueRank < selfEnt->crowd.ingressQueueRank ) : ( otherNum < entityNumber ) );

			bool shouldYieldAbreast = false;
			if ( isBottleneckConvergence ) {
				// In a doorway funnel, the agent physically further from the shared doorway yields:
				const Vector3DP &oWp = otherMonster->stringPulledPath[ std::min( otherMonster->stringPathPos, otherMonster->stringPulledPath.size() - 1 ) ];
				const double dOther = QM_Vector3Distance2DDP( otherOrigin, oWp );
				if ( distSelfToWp > ( dOther + CROWD_BOTTLENECK_DISTANCE_EPSILON ) ) {
					shouldYieldAbreast = true;
				} else if ( std::fabs( distSelfToWp - dOther) <= CROWD_BOTTLENECK_DISTANCE_EPSILON ) {
					shouldYieldAbreast = otherHasPriority;
				}
			} else {
				shouldYieldAbreast = otherHasPriority;
			}

			if ( shouldYieldAbreast ) {
				foundLeaderAhead = true;
				// At the doorway threshold (<= 72u), full halt to let lead rank enter first.
				// In the approach funnel (72u - 140u), throttle to crawl speed (0.35) so agents zipper seamlessly into single file:
				if ( distSelfToWp <= CROWD_BOTTLENECK_THRESHOLD_HOLD_DIST ) {
					strictQueueing = true;
					minScale = 0.0;
				} else {
					minScale = std::min( minScale, CROWD_BOTTLENECK_CRAWL_SPEED_SCALE );
				}
				continue;
			}
		}

		if ( aheadDist <= 0.0 || aheadDist > maxAheadDist ) {
			continue;
		}

		// Vertical proximity check: allow up to max step height on flat ground, or proportionally
		// higher along stairs/slopes ahead in our travel corridor lane:
		const double maxAllowedZDiff = static_cast<double>( NAV_MAX_STEP_HEIGHT ) + ( aheadDist * CROWD_STAIR_SLOPE_Z_FACTOR );
		if ( zDelta > maxAllowedZDiff ) {
			continue;
		}

		// Compute perpendicular lateral distance squared from our travel centerline.
		const double lateralDistSq = distSq - ( aheadDist * aheadDist );
		if ( lateralDistSq > corridorLateralSqr ) {
			continue; // Teammate is off to the side outside our travel corridor lane
		}

		// Head-on opposing course check: if teammate is moving towards us, arbitrate right-of-way:
		const Vector3 otherVel = other->velocity;
		const double otherVelSq = static_cast<double>( otherVel.x * otherVel.x + otherVel.y * otherVel.y );
		if ( otherVelSq > CROWD_HEADON_MIN_SPEED_SQR ) {
			const double invSpeed = 1.0 / std::sqrt( otherVelSq );
			const Vector3DP otherDir = { static_cast<double>( otherVel.x ) * invSpeed, static_cast<double>( otherVel.y ) * invSpeed, 0.0 };
			const double dotCourse = ( moveDir.x * otherDir.x ) + ( moveDir.y * otherDir.y );

			if ( dotCourse < CROWD_HEADON_COURSE_DOT_THRESHOLD ) {
				// Opposing collision course: determine right-of-way priority.
				// Priority order: lower queue rank / entity number as strict tie-breaker.
				const bool hasEgressRank = ( group->hasSerializedEgress && selfEnt->crowd.egressQueueRank >= 0 && other->crowd.egressQueueRank >= 0 );
				const bool hasIngressRank = ( group->hasSerializedIngress && selfEnt->crowd.ingressQueueRank >= 0 && other->crowd.ingressQueueRank >= 0 );
				const bool selfHasPriority = hasEgressRank ? ( selfEnt->crowd.egressQueueRank < other->crowd.egressQueueRank ) :
					( hasIngressRank ? ( selfEnt->crowd.ingressQueueRank < other->crowd.ingressQueueRank ) : ( entityNumber < otherNum ) );
				if ( !selfHasPriority ) {
					// Self has lower priority: yield right-of-way and stop to let the oncoming teammate clear the corridor.
					foundLeaderAhead = true;
					if ( isNarrowOrStairs ) {
						strictQueueing = true;
					}
					minScale = 0.0;
					break;
				} else {
					// Self has higher priority: maintain forward movement right-of-way while oncoming teammate yields.
					continue;
				}
			}
		}

		// Teammate is directly ahead in our corridor lane: calculate smooth deceleration scale.
		foundLeaderAhead = true;
		// In narrow corridors and stairs we enforce strict single-file behavior to avoid side-slip overtakes.
		if ( isNarrowOrStairs ) {
			strictQueueing = true;
		}
		const bool isStairStepAhead = ( zDelta >= NAV_STEP_MIN_VERTICAL_DELTA );
		// On stairs or approaching a vertical step-up, expand safe following separation so that the climbing leader's elevated step sweep is never blocked:
		const double stopSeparation = isStairStepAhead ? ( combinedRadius + CROWD_STAIR_STOP_BUFFER ) : ( combinedRadius + CROWD_GROUND_STOP_BUFFER );
		const double dynamicMinSeparation = combinedRadius + ( ( group->params.longitudinalSpacing > 0.0 ) ? ( group->params.longitudinalSpacing * 0.15 ) : CROWD_SLOT_MIN_SEPARATION_MARGIN );
		const double crawlSeparation = isStairStepAhead ? ( dynamicMinSeparation + CROWD_STAIR_CRAWL_BUFFER ) : ( dynamicMinSeparation + CROWD_GROUND_CRAWL_BUFFER );

		double scale = 1.0;
		if ( other->crowd.reachedGoal ) {
			// Teammate ahead has reached its final goal slot and is holding station:
			// Maintain rolling crawl scale so trailing agents can gently slide past arrived station-keeping teammates to reach empty slots:
			if ( aheadDist <= stopSeparation ) {
				scale = CROWD_FOLLOW_ROLLING_MIN_SCALE;
			} else if ( aheadDist <= crawlSeparation ) {
				const double frac = ( aheadDist - stopSeparation ) / ( crawlSeparation - stopSeparation );
				scale = CROWD_FOLLOW_ROLLING_MIN_SCALE + ( frac * ( CROWD_FOLLOW_CRAWL_SPEED_SCALE - CROWD_FOLLOW_ROLLING_MIN_SCALE ) );
			} else {
				const double frac = std::clamp( ( aheadDist - crawlSeparation ) / CROWD_FOLLOW_SLOWDOWN_RANGE, 0.0, 1.0 );
				scale = CROWD_FOLLOW_CRAWL_SPEED_SCALE + frac * ( 1.0 - CROWD_FOLLOW_CRAWL_SPEED_SCALE );
			}
		} else {
			// Teammate ahead is actively moving along the corridor:
			// In narrow passages or strict doorway queues, trailing agents must halt at stopSeparation
			// so they do not push into the back of the leader and wedge them against the aperture:
			if ( isNarrowFunnel || strictQueueing ) {
				if ( aheadDist <= stopSeparation ) {
					scale = 0.0;
				} else if ( aheadDist <= crawlSeparation ) {
					const double frac = ( aheadDist - stopSeparation ) / ( crawlSeparation - stopSeparation );
					scale = frac * CROWD_FOLLOW_CRAWL_SPEED_SCALE;
				} else {
					scale = 1.0;
				}
			} else {
				// Open terrain: maintain rolling minimum crawl scale so followers never deadlock in tight clusters and stream smoothly in single file:
				if ( aheadDist <= combinedRadius ) {
					scale = CROWD_FOLLOW_ROLLING_MIN_SCALE;
				} else if ( aheadDist <= stopSeparation ) {
					const double frac = std::clamp( ( aheadDist - combinedRadius ) / ( stopSeparation - combinedRadius ), 0.0, 1.0 );
					scale = CROWD_FOLLOW_ROLLING_MIN_SCALE + ( frac * ( CROWD_FOLLOW_CRAWL_SPEED_SCALE - CROWD_FOLLOW_ROLLING_MIN_SCALE ) );
				} else if ( aheadDist <= crawlSeparation ) {
					const double frac = ( aheadDist - stopSeparation ) / ( crawlSeparation - stopSeparation );
					scale = CROWD_FOLLOW_CRAWL_SPEED_SCALE + frac * ( 1.0 - CROWD_FOLLOW_CRAWL_SPEED_SCALE );
				} else {
					scale = 1.0;
				}
			}
		}
		minScale = std::min( minScale, scale );
	}

	*outSpeedScale = minScale;
	if ( outStrictQueueing != nullptr ) {
		*outStrictQueueing = strictQueueing;
	}
	return foundLeaderAhead;
}

/**
*	Tactical Cover Allocation:
**/

/**
*	@brief	Allocate distinct occluded tactical cover points for squad members.
*	@param	group		Crowd group record.
*	@param	members		Living squad members.
*	@param	destOrigin	Destination or threat source origin in Vector3DP.
*	@param	outSlots	[out] Computed tactical cover slots.
**/
static void SVG_Crowd_AllocateTacticalCover( svg_crowd_group_t &group, const std::vector<svg_base_edict_t*> &members, const Vector3DP &destOrigin, std::vector<svg_crowd_slot_t> &outSlots ) {
	outSlots.clear();
	const size_t count = members.size();
	if ( count == 0 ) {
		return;
	}

	const double maxDist = ( group.params.maxCoverDistance > 0.0 ) ? group.params.maxCoverDistance : 768.0;
	const double minDist = ( group.params.minCoverDistance > 0.0 ) ? group.params.minCoverDistance : 192.0;
	const double exclusionRadius = ( group.params.coverExclusionRadius > 0.0 ) ? group.params.coverExclusionRadius : 160.0;

	// Calculate collective squad centroid in Vector3DP.
	const Vector3DP centroid = SVG_Crowd_ComputeCentroid( members );
	const Vector3DP searchOrigin = ( centroid );
	const Vector3DP threatOrigin = ( destOrigin );

	/**
	*	Determine tactical temperament from squad members:
	*	- MOOD_TYPE_AGGRESSIVE / NORMAL: Strictly requires offensive peek/engagement sightlines to the threat.
	*	- MOOD_TYPE_SCARED: Seeks pure defensive protection (hiding spots, enclosed rooms, bunkers).
	**/
	bool requireEngagementLOS = true;
	for ( const svg_base_edict_t *member : members ) {
		if ( member && member->GetTypeInfo()->IsSubClassType<svg_monster_base_t>() ) {
			const svg_monster_testdummy_debug_t *dummy = static_cast<const svg_monster_testdummy_debug_t*>( member );
			if ( dummy->mood == svg_monster_mood_type_t::MOOD_TYPE_SCARED ) {
				requireEngagementLOS = false;
				break;
			}
		}
	}

	// Query available tactical cover candidates.
	std::vector<int32_t> candidateIndices;
	Nav_FindCoverPoints( searchOrigin, threatOrigin, static_cast<float>( maxDist ), -1, &candidateIndices, NAV_COVER_NONE, Vector3DP{ 0.0, 0.0, 0.0 }, 12, requireEngagementLOS );

	// Separate candidates based on distance and spatial anti-clustering.
	std::vector<int32_t> selectedCoverIndices;
	std::vector<Vector3DP> claimedCoverPositions;

	for ( const int32_t coverIdx : candidateIndices ) {
		if ( selectedCoverIndices.size() >= count ) {
			break;
		}

		if ( coverIdx < 0 || coverIdx >= static_cast<int32_t>( g_nav_cover_points.size() ) ) {
			continue;
		}

		const nav_cover_point_t &cp = g_nav_cover_points[ coverIdx ];
		if ( cp.face_idx < 0 || cp.face_idx >= static_cast<int32_t>( g_nav_faces.size() ) ) {
			continue;
		}

		const Vector3DP worldPos = Vector3DP( cp.local_position );
		const double distToThreat = QM_Vector3DistanceDP( worldPos, destOrigin );

		// Check stand-off distance bounds.
		if ( distToThreat < minDist || distToThreat > maxDist ) {
			continue;
		}

		// Enforce spatial separation between squad mates to prevent bunching behind same cover.
		bool tooCloseToAnother = false;
		for ( const Vector3DP &claimedPos : claimedCoverPositions ) {
			if ( QM_Vector3DistanceDP( worldPos, claimedPos ) < exclusionRadius ) {
				tooCloseToAnother = true;
				break;
			}
		}

		if ( tooCloseToAnother ) {
			continue;
		}

		selectedCoverIndices.push_back( coverIdx );
		claimedCoverPositions.push_back( worldPos );
	}

	// Create slots for found cover points.
	for ( size_t i = 0; i < selectedCoverIndices.size(); i++ ) {
		const int32_t cIdx = selectedCoverIndices[ i ];
		svg_crowd_slot_t slot;
		slot.slotIndex = static_cast<int32_t>( i );
		slot.worldPosition = Vector3DP( g_nav_cover_points[ cIdx ].local_position );
		slot.localOffset = slot.worldPosition - destOrigin;
		slot.role = crowd_member_role_t::ROLE_COVER_OPERATIVE;
		slot.coverIndex = cIdx;
		slot.isNavmeshValid = true;
		outSlots.push_back( slot );
	}

	// If fewer cover points than members were found, pad remaining slots with defensive reserve slots behind the cover line.
	if ( outSlots.size() < count ) {
		const size_t needed = count - outSlots.size();
		std::vector<svg_crowd_slot_t> fallbackSlots;
		SVG_Crowd_GenerateArrowSlots( needed, group.params, fallbackSlots );

		// Compute defensive reserve anchor facing threatOrigin:
		Vector3DP toThreat = destOrigin - centroid;
		toThreat.z = 0.0;
		const double toThreatLen = QM_Vector3LengthDP( toThreat );
		const double reserveHeadingYaw = ( toThreatLen > 0.001 ) ? QM_Vector3ToYawDP( toThreat ) : group.currentHeadingYaw;

		// Offset reserve anchor behind squad centroid away from threat:
		Vector3DP reserveAnchor = centroid;
		if ( toThreatLen > 0.001 ) {
			reserveAnchor = centroid - ( toThreat * ( CROWD_TACTICAL_COVER_RESERVE_OFFSET / toThreatLen ) );
		}

		const double minSlotSeparation = std::max( ( CROWD_DEFAULT_AGENT_RADIUS * 2.0 ) + CROWD_SLOT_MIN_SEPARATION_MARGIN, group.params.lateralSpacing );
		SVG_Crowd_TransformLocalSlotsToWorld( reserveAnchor, reserveHeadingYaw, fallbackSlots );
		SVG_Crowd_SnapSlotsToNavMesh( fallbackSlots, reserveAnchor );
		SVG_Crowd_ResolveSlotCollisionsAndInvalidSlots( fallbackSlots, reserveAnchor, reserveHeadingYaw, minSlotSeparation, CROWD_DEFAULT_AGENT_RADIUS );

		for ( size_t k = 0; k < fallbackSlots.size(); k++ ) {
			fallbackSlots[ k ].slotIndex = static_cast<int32_t>( outSlots.size() );
			outSlots.push_back( fallbackSlots[ k ] );
		}
	}

	// Final pass: ensure all allocated slots (cover and fallback) are mutually separated and on valid navmesh
	const double minSlotSeparation = std::max( ( CROWD_DEFAULT_AGENT_RADIUS * 2.0 ) + CROWD_SLOT_MIN_SEPARATION_MARGIN, group.params.lateralSpacing );
	SVG_Crowd_ResolveSlotCollisionsAndInvalidSlots( outSlots, centroid, group.currentHeadingYaw, minSlotSeparation, CROWD_DEFAULT_AGENT_RADIUS );
}

/**
*	Movement Command Implementations:
**/

/**
*	@brief	Command a crowd to navigate to a world origin in Vector3DP with the specified formation style.
*	@param	crowdID	Crowd group identifier.
*	@param	origin	World-space destination origin in Vector3DP.
*	@param	style	Formation / chase style.
*	@param	params	Formation parameters (spacing, cover distance, etc.).
*	@return	True when orders were successfully dispatched to one or more crowd members.
**/
bool MoveAStarCrowdOrigin( const int32_t crowdID, const Vector3DP &origin, const crowd_chase_target_type_t style, const svg_crowd_params_t &params ) {
	if ( crowdID < 0 ) {
		return false;
	}

	std::vector<svg_base_edict_t*> members;
	SVG_Crowd_GetCrowdMembers( crowdID, members );
	if ( members.empty() ) {
		return false;
	}

	svg_crowd_group_t &group = g_crowd_groups[ crowdID ];
	group.crowdID = crowdID;
	group.style = style;
	group.params = params;
	group.destinationOrigin = origin;
	group.targetEntityNumber = ENTITYNUM_NONE;
	group.orderStartTime = level.time;
	group.isMoving = true;
	SVG_Squad_SetCommand( crowdID, origin, style, params );
	SVG_Crowd_ResetSerializedIngress( group, members );
	SVG_Crowd_ResetSerializedEgress( group, members );

	// Calculate collective squad centroid in Vector3DP.
	const Vector3DP centroid = SVG_Crowd_ComputeCentroid( members );
	group.currentHeadingYaw = SVG_Crowd_CalculateHeadingYaw( centroid, origin, nullptr, params );

	// Compute navigation guide path from squad centroid to destination to follow curved corridors, ramps, and staircases.
	std::vector<Vector3DP> guidePath;
	std::vector<int32_t> navPathFaces;

	// Resolve destination face with feet-offset and closest face fallbacks for stairs and elevated brushes:
	int32_t goalFace = Nav_FindFaceInLeafStrict( origin );
	if ( goalFace < 0 ) {
		Vector3DP feetOrigin = origin;
		feetOrigin.z -= CROWD_SLOT_FEET_SNAP_OFFSET_Z;
		goalFace = Nav_FindFaceInLeafStrict( feetOrigin );
	}
	if ( goalFace < 0 ) {
		goalFace = Nav_FindClosestFaceInLeaf( origin );
	}

	// Resolve squad start face with member fallbacks if centroid falls in a boundary or empty leaf:
	int32_t startFace = Nav_FindFaceInLeafStrict( centroid );
	if ( startFace < 0 ) {
		Vector3DP feetCentroid = centroid;
		feetCentroid.z -= CROWD_SLOT_FEET_SNAP_OFFSET_Z;
		startFace = Nav_FindFaceInLeafStrict( feetCentroid );
	}
	if ( startFace < 0 ) {
		startFace = Nav_FindClosestFaceInLeaf( centroid );
	}

	const svg_base_edict_t *anchorMember = nullptr;
	if ( startFace < 0 || ( goalFace >= 0 && Nav_GetFaceComponent( startFace ) != Nav_GetFaceComponent( goalFace ) ) ) {
		double bestDistSq = std::numeric_limits<double>::max();

		// 1. First priority: designated squad leader if active and connected to goalFace component:
		const svg_base_edict_t *leader = group.GetLeaderEntity();
		if ( leader && SVG_Entity_IsActive( leader ) ) {
			const Vector3DP leaderFeet = SVG_GetEntityFeetOriginDP( leader );
			const int32_t leaderFace = Nav_FindFaceInLeafStrict( leaderFeet );
			if ( leaderFace >= 0 && ( goalFace < 0 || Nav_GetFaceComponent( leaderFace ) == Nav_GetFaceComponent( goalFace ) ) ) {
				startFace = leaderFace;
				anchorMember = leader;
			}
		}

		// 2. Second priority: select the member closest to squad centroid that connects to the goal component:
		if ( !anchorMember ) {
			for ( const svg_base_edict_t *ent : members ) {
				if ( !ent || !SVG_Entity_IsActive( ent ) ) {
					continue;
				}
				const Vector3DP entFeet = SVG_GetEntityFeetOriginDP( ent );
				const int32_t entFace = Nav_FindFaceInLeafStrict( entFeet );
				if ( entFace >= 0 && ( goalFace < 0 || Nav_GetFaceComponent( entFace ) == Nav_GetFaceComponent( goalFace ) ) ) {
					const double dSq = QM_Vector3DistanceSqrDP( entFeet, centroid );
					if ( dSq < bestDistSq ) {
						bestDistSq = dSq;
						startFace = entFace;
						anchorMember = ent;
					}
				}
			}
		}
	}

	if ( startFace >= 0 && goalFace >= 0 && !g_nav_faces.empty() ) {
		nav_path_policy_t guidePolicy;
		guidePolicy.agent_radius = static_cast<float>( CROWD_DEFAULT_AGENT_RADIUS );
		if ( Nav_FindPath( startFace, goalFace, navPathFaces, guidePolicy ) ) {
			std::vector<bool> forcedWps;
			const Vector3DP guideStart = anchorMember ? SVG_GetEntityFeetOriginDP( anchorMember ) : centroid;
			Nav_StringPull( navPathFaces, guideStart, origin, CROWD_DEFAULT_AGENT_RADIUS, guidePath, &forcedWps );
			// If guide path has at least 2 points, derive heading yaw from the final corridor segment entering origin
			if ( guidePath.size() >= 2 ) {
				Vector3DP inSeg = guidePath.back() - guidePath[ guidePath.size() - 2 ];
				inSeg.z = 0.0;
				if ( QM_Vector3LengthDP( inSeg ) > 0.001 ) {
					group.currentHeadingYaw = QM_Vector3ToYawDP( inSeg );
				}
			}
		}
	}

	// Calculate dynamic corridor clearance and squeeze factor at destination and squad centroid if enabled.
	crowd_chase_target_type_t effectiveStyle = style;
	svg_crowd_params_t effectiveParams = params;
	double squadClearance = 1000.0;
	double destClearance = 1000.0;
	double routeBottleneckClearance = 1000.0;
	bool hasRouteStairs = false;
	bool hasRouteLedge = false;

	if ( params.enableCorridorSqueeze ) {
		const double desiredWidth = ( members.size() > 1 ) ? ( static_cast<double>( members.size() - 1 ) * params.lateralSpacing ) : params.lateralSpacing;
		destClearance = SVG_Crowd_ComputeCorridorClearance( origin, desiredWidth );
		squadClearance = SVG_Crowd_ComputeCorridorClearance( centroid, desiredWidth );

		// Compute bottleneck clearance and detect vertical stair/ledge transitions along the entire route.
		// If any intermediate segment along the path passes over a staircase, step ledge, or narrow bottleneck
		// (< 96 units), the squad must collapse into single-file column march so members do not march
		// abreast off stair edges, catwalks, or into chokepoint jambs.
		routeBottleneckClearance = destClearance;

		if ( !navPathFaces.empty() ) {
			for ( size_t i = 0; i < navPathFaces.size(); ++i ) {
				const int32_t fIdx = navPathFaces[ i ];
				if ( fIdx >= 0 && fIdx < static_cast<int32_t>( g_nav_faces.size() ) ) {
					const nav_face_t &faceCurr = g_nav_faces[ fIdx ];
					if ( faceCurr.clearance > 0.0 ) {
						routeBottleneckClearance = std::min( routeBottleneckClearance, faceCurr.clearance * 2.0 );
					}

					// Test adjacent face transitions for vertical stair steps or ledges.
					if ( i + 1 < navPathFaces.size() ) {
						const int32_t fNextIdx = navPathFaces[ i + 1 ];
						if ( fNextIdx >= 0 && fNextIdx < static_cast<int32_t>( g_nav_faces.size() ) ) {
							const nav_face_t &faceNext = g_nav_faces[ fNextIdx ];
							const bool isRampTransition = ( faceCurr.normal.z < NAV_RAMP_MAX_NORMAL_Z || faceNext.normal.z < NAV_RAMP_MAX_NORMAL_Z );
							const double verticalDelta = isRampTransition ? 0.0 : std::fabs( faceNext.center.z - faceCurr.center.z );
							if ( !isRampTransition ) {
								if ( verticalDelta >= NAV_STEP_MIN_VERTICAL_DELTA && verticalDelta <= static_cast<double>( NAV_MAX_STEP_HEIGHT ) ) {
									hasRouteStairs = true;
								} else if ( verticalDelta > static_cast<double>( NAV_MAX_STEP_HEIGHT ) ) {
									hasRouteLedge = true;
								}
							}
						}
					}
				}
			}
		}

		// When the destination area itself is a narrow corridor, plank, or bottleneck (< 96 units),
		// collapse lateral spacing to column march along the guide path for traveling/marching formations.
		// Defensive and area formations (CIRCLE_FILLED, SURROUND_PERIMETER, TACTICAL_COVER) must NEVER
		// Squeeze formation geometry based on destination clearance:
		const double squeeze = ( desiredWidth > 0.0 ) ? std::clamp( destClearance / desiredWidth, 0.2, 1.0 ) : 1.0;
		group.dynamicSqueezeFactor = squeeze;
		effectiveParams.lateralSpacing = std::max( params.minCorridorSpacing, params.lateralSpacing * squeeze );
		effectiveParams.longitudinalSpacing = std::max( params.minCorridorSpacing, params.longitudinalSpacing * squeeze );
	} else {
		group.dynamicSqueezeFactor = 1.0;
	}

	// Extract doorway/passage bottleneck portal along the navigation route entering destination:
	Vector3DP portalPoint = origin;
	double portalWidth = 0.0;
	bool hasPortal = false;

	// 1. First, check if destination origin belongs to an enclosed spatial room/zone with precomputed portals:
	Vector3DP effectiveDestOrigin = origin;
	const nav_room_t *destRoom = Nav_GetRoomForPoint( origin );

	/**
	*	Resolve threshold-height and boundary clicks through the exact destination
	*	nav face. Point-in-room can legitimately miss on a portal seam or Z offset,
	*	while goalFace already identifies the connected floor polygon and room ID.
	**/
	if ( destRoom == nullptr && goalFace >= 0 && goalFace < static_cast<int32_t>( g_nav_faces.size() ) ) {
		const int32_t goalRoomID = g_nav_faces[ goalFace ].room_id;
		if ( goalRoomID >= 0 && goalRoomID < static_cast<int32_t>( g_nav_rooms.size() ) ) {
			destRoom = &g_nav_rooms[ goalRoomID ];
		}
	}

	// If origin landed on a porch or threshold outside, search for nearby room portals entering a
	// portal-bounded destination room (enclosed room, alcove, or corridor passage). Corridors must
	// participate here as well: otherwise circle-style ingress into a corridor-class destination
	// never receives the room centroid anchor / portal hint and the ring collapses at the doorway.
	if ( destRoom == nullptr || destRoom->portal_indices.empty() ||
		 ( destRoom->zone_type != ZONE_TYPE_ROOM_ENCLOSED && destRoom->zone_type != ZONE_TYPE_ALCOVE &&
		   destRoom->zone_type != ZONE_TYPE_CORRIDOR_FLAT && destRoom->zone_type != ZONE_TYPE_CORRIDOR_RAMP ) ) {
		double bestPortalDistSq = 160.0 * 160.0;
		for ( const nav_portal_t &portal : g_nav_portals ) {
			const double dSq = QM_Vector3Distance2DSqrDP( origin, portal.center );
			if ( dSq < bestPortalDistSq ) {
				const int32_t rTo = portal.to_room_id;
				const int32_t rFrom = portal.from_room_id;
				if ( rTo >= 0 && rTo < static_cast<int32_t>( g_nav_rooms.size() ) &&
					 !g_nav_rooms[ rTo ].portal_indices.empty() &&
					 ( g_nav_rooms[ rTo ].zone_type == ZONE_TYPE_ROOM_ENCLOSED || g_nav_rooms[ rTo ].zone_type == ZONE_TYPE_ALCOVE ||
					   g_nav_rooms[ rTo ].zone_type == ZONE_TYPE_CORRIDOR_FLAT || g_nav_rooms[ rTo ].zone_type == ZONE_TYPE_CORRIDOR_RAMP ) ) {
					destRoom = &g_nav_rooms[ rTo ];
					portalPoint = portal.center;
					portalWidth = portal.width;
					hasPortal = true;
					bestPortalDistSq = dSq;
				} else if ( rFrom >= 0 && rFrom < static_cast<int32_t>( g_nav_rooms.size() ) &&
							!g_nav_rooms[ rFrom ].portal_indices.empty() &&
							( g_nav_rooms[ rFrom ].zone_type == ZONE_TYPE_ROOM_ENCLOSED || g_nav_rooms[ rFrom ].zone_type == ZONE_TYPE_ALCOVE ||
							  g_nav_rooms[ rFrom ].zone_type == ZONE_TYPE_CORRIDOR_FLAT || g_nav_rooms[ rFrom ].zone_type == ZONE_TYPE_CORRIDOR_RAMP ) ) {
					destRoom = &g_nav_rooms[ rFrom ];
					portalPoint = portal.center;
					portalWidth = portal.width;
					hasPortal = true;
					bestPortalDistSq = dSq;
				}
			}
		}
	}

	if ( destRoom != nullptr &&
		 ( destRoom->zone_type == ZONE_TYPE_ROOM_ENCLOSED || destRoom->zone_type == ZONE_TYPE_ALCOVE ||
		   destRoom->zone_type == ZONE_TYPE_CORRIDOR_FLAT || destRoom->zone_type == ZONE_TYPE_CORRIDOR_RAMP ) ) {
		effectiveDestOrigin = destRoom->centroid;

		/**
		* Resolve the exact portal traversed by the A* face path.
		* This topological identity takes precedence over proximity heuristics because a room may have
		* several doors near the same string-pulled corridor.
		**/
		bool foundRoutePortal = false;
		for ( size_t pathIndex = navPathFaces.size(); pathIndex > 1; pathIndex-- ) {
			const int32_t outsideFaceIndex = navPathFaces[ pathIndex - 2 ];
			const int32_t insideFaceIndex = navPathFaces[ pathIndex - 1 ];
			if ( outsideFaceIndex < 0 || insideFaceIndex < 0 ||
				 outsideFaceIndex >= static_cast<int32_t>( g_nav_faces.size() ) ||
				 insideFaceIndex >= static_cast<int32_t>( g_nav_faces.size() ) ||
				 g_nav_faces[ insideFaceIndex ].room_id != destRoom->room_id ||
				 g_nav_faces[ outsideFaceIndex ].room_id == destRoom->room_id ) {
				continue;
			}

			/**
			*	Resolve the exact A* transition edge first. This remains authoritative
			*	when spatial portal metadata omitted or merged the physical doorway.
			**/
			const nav_face_t &outsideFace = g_nav_faces[ outsideFaceIndex ];
			int32_t transitionHalfedgeIndex = outsideFace.first_edge_idx;
			// Search only the current face's bounded edge loop at order time.
			for ( int32_t edgeOffset = 0; edgeOffset < outsideFace.num_edges; edgeOffset++ ) {
				// Reject malformed half-edge indices before dereferencing packed topology.
				if ( transitionHalfedgeIndex < 0 ||
					 transitionHalfedgeIndex >= static_cast<int32_t>( g_nav_halfedges.size() ) ) {
					break;
				}

				const nav_halfedge_t &transitionHalfedge = g_nav_halfedges[ transitionHalfedgeIndex ];
				const int32_t twinIndex = transitionHalfedge.twin_idx;
				const bool reachesDestinationFace = twinIndex >= 0 &&
					twinIndex < static_cast<int32_t>( g_nav_halfedges.size() ) &&
					g_nav_halfedges[ twinIndex ].face_idx == insideFaceIndex;
				if ( reachesDestinationFace &&
					 transitionHalfedge.vertex_idx >= 0 &&
					 transitionHalfedge.vertex_idx < static_cast<int32_t>( g_nav_vertices.size() ) &&
					 transitionHalfedge.next_idx >= 0 &&
					 transitionHalfedge.next_idx < static_cast<int32_t>( g_nav_halfedges.size() ) ) {
					const int32_t endVertexIndex = g_nav_halfedges[ transitionHalfedge.next_idx ].vertex_idx;
					if ( endVertexIndex >= 0 && endVertexIndex < static_cast<int32_t>( g_nav_vertices.size() ) ) {
						const Vector3DP &edgeStart = g_nav_vertices[ transitionHalfedge.vertex_idx ];
						const Vector3DP &edgeEnd = g_nav_vertices[ endVertexIndex ];
						portalPoint = ( edgeStart + edgeEnd ) * 0.5;
						portalWidth = QM_Vector3Distance2DDP( edgeStart, edgeEnd );
						hasPortal = portalWidth > 0.001;
						foundRoutePortal = hasPortal;
					}
					break;
				}

				transitionHalfedgeIndex = transitionHalfedge.next_idx;
			}
			if ( foundRoutePortal ) {
				break;
			}

			for ( const int32_t portalIndex : destRoom->portal_indices ) {
				// Validate the portal and its twin half-edge before comparing the exact face pair.
				if ( portalIndex < 0 || portalIndex >= static_cast<int32_t>( g_nav_portals.size() ) ) {
					continue;
				}
				const nav_portal_t &routePortal = g_nav_portals[ portalIndex ];
				if ( routePortal.halfedge_idx < 0 ||
					 routePortal.halfedge_idx >= static_cast<int32_t>( g_nav_halfedges.size() ) ) {
					continue;
				}
				const nav_halfedge_t &portalHalfedge = g_nav_halfedges[ routePortal.halfedge_idx ];
				if ( portalHalfedge.twin_idx < 0 ||
					 portalHalfedge.twin_idx >= static_cast<int32_t>( g_nav_halfedges.size() ) ) {
					continue;
				}
				const int32_t portalFaceA = portalHalfedge.face_idx;
				const int32_t portalFaceB = g_nav_halfedges[ portalHalfedge.twin_idx ].face_idx;
				const bool matchesRouteTransition =
					( portalFaceA == outsideFaceIndex && portalFaceB == insideFaceIndex ) ||
					( portalFaceA == insideFaceIndex && portalFaceB == outsideFaceIndex );
				if ( !matchesRouteTransition ) {
					continue;
				}

				portalPoint = routePortal.center;
				portalWidth = routePortal.width;
				hasPortal = true;
				foundRoutePortal = true;
				break;
			}
			if ( foundRoutePortal ) {
				break;
			}
		}
		if ( !hasPortal && !destRoom->portal_indices.empty() ) {
			/**
			*	Select the room portal geometrically closest to the actual guide corridor.
			*	Using portal_indices[0] is topology-order dependent and can reserve the wrong door.
			**/
			double bestPortalScoreSq = std::numeric_limits<double>::max();
			for ( const int32_t portalIndex : destRoom->portal_indices ) {
				// Ignore stale room-to-portal indices.
				if ( portalIndex < 0 || portalIndex >= static_cast<int32_t>( g_nav_portals.size() ) ) {
					continue;
				}

				const Vector3DP candidatePortal = g_nav_portals[ portalIndex ].center;
				double portalScoreSq = QM_Vector3Distance2DSqrDP( centroid, candidatePortal );

				// Measure exact squared distance to the piecewise-linear guide corridor when available.
				if ( guidePath.size() >= 2 ) {
					portalScoreSq = std::numeric_limits<double>::max();
					for ( size_t pathIndex = 1; pathIndex < guidePath.size(); pathIndex++ ) {
						const Vector3DP &segmentStart = guidePath[ pathIndex - 1 ];
						const Vector3DP &segmentEnd = guidePath[ pathIndex ];
						const Vector3DP segment = segmentEnd - segmentStart;
						const double segmentLengthSq = QM_Vector3LengthSqrDP( segment );
						double t = 0.0;
						// Clamp projection to the finite path segment.
						if ( segmentLengthSq > 0.0001 ) {
							t = std::clamp( QM_Vector3DotProductDP( candidatePortal - segmentStart, segment ) / segmentLengthSq, 0.0, 1.0 );
						}
						const Vector3DP projection = segmentStart + ( segment * t );
						portalScoreSq = std::min( portalScoreSq, QM_Vector3Distance2DSqrDP( candidatePortal, projection ) );
					}
				}

				if ( portalScoreSq < bestPortalScoreSq ) {
					bestPortalScoreSq = portalScoreSq;
					portalPoint = candidatePortal;
					portalWidth = g_nav_portals[ portalIndex ].width;
					hasPortal = true;
				}
			}
		}
	}

	/**
	*	Recover a missing portal record from the actual guide-path crossing of the
	*	destination room bounds. Some generated rooms have valid face membership but
	*	no portal_indices entry; without this fallback, defensive formations bypass
	*	the entire pre-formation protocol and every member targets the room directly.
	**/
	const bool needsDefensiveEntrance = ( effectiveStyle == crowd_chase_target_type_t::CROWD_STYLE_CIRCLE_FILLED ||
		effectiveStyle == crowd_chase_target_type_t::CROWD_STYLE_SURROUND_PERIMETER ||
		effectiveStyle == crowd_chase_target_type_t::CROWD_STYLE_TACTICAL_COVER );
	if ( !hasPortal && needsDefensiveEntrance && destRoom != nullptr && guidePath.size() >= 2 ) {
		for ( size_t pointIndex = 1; pointIndex < guidePath.size(); pointIndex++ ) {
			const nav_room_t *segmentStartRoom = Nav_GetRoomForPoint( guidePath[ pointIndex - 1 ] );
			const nav_room_t *segmentEndRoom = Nav_GetRoomForPoint( guidePath[ pointIndex ] );
			const bool startsOutsideDestination = ( segmentStartRoom == nullptr || segmentStartRoom->room_id != destRoom->room_id );
			const bool endsInsideDestination = ( segmentEndRoom != nullptr && segmentEndRoom->room_id == destRoom->room_id );

			// The first outside-to-inside route transition is the physical entrance used by this order.
			if ( !startsOutsideDestination || !endsInsideDestination ) {
				continue;
			}

			const Vector3DP segmentStart = guidePath[ pointIndex - 1 ];
			const Vector3DP segmentDelta = guidePath[ pointIndex ] - segmentStart;
			double entryFraction = 0.0;
			double exitFraction = 1.0;
			bool intersectsBounds = true;

			// Clip one horizontal axis against the destination AABB using the slab method.
			auto clipAxis = [&]( const double start, const double delta, const double minimum, const double maximum ) {
				if ( std::fabs( delta ) <= 0.0001 ) {
					if ( start < minimum || start > maximum ) {
						intersectsBounds = false;
					}
					return;
				}
				double nearFraction = ( minimum - start ) / delta;
				double farFraction = ( maximum - start ) / delta;
				if ( nearFraction > farFraction ) {
					std::swap( nearFraction, farFraction );
				}
				entryFraction = std::max( entryFraction, nearFraction );
				exitFraction = std::min( exitFraction, farFraction );
				if ( entryFraction > exitFraction ) {
					intersectsBounds = false;
				}
			};
			clipAxis( segmentStart.x, segmentDelta.x, destRoom->bounds.mins.x, destRoom->bounds.maxs.x );
			clipAxis( segmentStart.y, segmentDelta.y, destRoom->bounds.mins.y, destRoom->bounds.maxs.y );

			if ( intersectsBounds ) {
				portalPoint = segmentStart + ( segmentDelta * std::clamp( entryFraction, 0.0, 1.0 ) );
			} else {
				portalPoint = guidePath[ pointIndex ];
			}
			const int32_t entranceFace = Nav_FindClosestFaceInLeaf( portalPoint );
			const double entranceClearance = ( entranceFace >= 0 && entranceFace < static_cast<int32_t>( g_nav_faces.size() ) )
				? g_nav_faces[ entranceFace ].clearance * 2.0
				: CROWD_MIN_TWO_AGENT_ABREAST_WIDTH;
			portalWidth = std::clamp( entranceClearance, CROWD_DEFAULT_AGENT_RADIUS * 2.0,
				CROWD_PORTAL_BOTTLENECK_MAX_WIDTH - 1.0 );
			hasPortal = true;
			break;
		}
	}

	// 2. If no spatial room portal was found, inspect route faces for route chokepoints:
	if ( !hasPortal && navPathFaces.size() >= 2 ) {
		for ( int32_t p = static_cast<int32_t>( navPathFaces.size() ) - 2; p >= 0; --p ) {
			const int32_t fIdx = navPathFaces[ p ];
			if ( fIdx >= 0 && fIdx < static_cast<int32_t>( g_nav_faces.size() ) ) {
				const nav_face_t &face = g_nav_faces[ fIdx ];
				if ( face.clearance > 0.0 && ( face.clearance * 2.0 ) < CROWD_PORTAL_BOTTLENECK_MAX_WIDTH ) {
					portalPoint = face.center;
					portalWidth = face.clearance * 2.0;
					hasPortal = true;
					break;
				}
			}
		}
	}

	/**
	*	Final topology-independent entrance guarantee for defensive area orders.
	*	A string-pulled route containing an intermediate waypoint has proven that
	*	direct travel to the destination is obstructed. Its last intermediate point
	*	is therefore the approach gate nearest the destination and must instantiate
	*	the marshalling protocol even when room and portal metadata are incomplete.
	**/
	if ( !hasPortal && needsDefensiveEntrance && guidePath.size() >= 3 ) {
		portalPoint = guidePath[ guidePath.size() - 2 ];
		portalWidth = std::max( CROWD_DEFAULT_AGENT_RADIUS * 2.0,
			std::min( routeBottleneckClearance, CROWD_PORTAL_BOTTLENECK_MAX_WIDTH - 1.0 ) );
		hasPortal = true;
	}

	/**
	*	Absolute pre-formation guarantee for static defensive area fills.
	*	A direct two-point path contains no intermediate topology from which to recover
	*	a doorway. Define a finite approach plane one formation depth before the room
	*	anchor so this command can never bypass marshalling merely because generated
	*	room metadata or string-pull corner points are absent.
	**/
	if ( !hasPortal && needsDefensiveEntrance && members.size() > 1 ) {
		Vector3DP approachInward = effectiveDestOrigin - centroid;
		if ( guidePath.size() >= 2 ) {
			approachInward = guidePath.back() - guidePath.front();
		}
		approachInward.z = 0.0;
		if ( QM_Vector3LengthSqrDP( approachInward ) > 0.0001 ) {
			approachInward = QM_Vector3NormalizeDP( approachInward );
			const double syntheticGateDepth = std::max( 96.0,
				( CROWD_DEFAULT_AGENT_RADIUS * 2.0 ) + CROWD_ROOM_FILL_DOOR_LANE_MARGIN );
			portalPoint = effectiveDestOrigin - ( approachInward * syntheticGateDepth );
			portalWidth = std::max( CROWD_DEFAULT_AGENT_RADIUS * 2.0,
				std::min( routeBottleneckClearance, CROWD_PORTAL_BOTTLENECK_MAX_WIDTH - 1.0 ) );
			hasPortal = true;
		}
	}

	group.style = effectiveStyle;

	// Build formation / cover slots at destination origin.
	if ( effectiveStyle == crowd_chase_target_type_t::CROWD_STYLE_COLUMN_MARCH && guidePath.size() >= 2 ) {
		// Single-file column along the navigation guide path: sample slots along the approach vector at origin.
		// Slots are bounded within CROWD_DESTINATION_COLUMN_MAX_DEPTH and CROWD_COLUMN_PATH_MAX_FRACTION
		// so that the assembled column stays localized at the destination area / platform rather than stretching hundreds of units across the map.
		group.slots.clear();
		group.slots.reserve( members.size() );
		// Ensure minimum physical spacing respects agent collision hull diameter and follow stop separation:
		const double minPhysicalSeparation = ( CROWD_DEFAULT_AGENT_RADIUS * 2.0 ) + CROWD_GROUND_STOP_BUFFER;
		const double minSpacing = std::max( minPhysicalSeparation, ( effectiveParams.minCorridorSpacing > 0.0 ) ? effectiveParams.minCorridorSpacing : CROWD_DEFAULT_MIN_CORRIDOR_SPACING );
		const double rawSpacing = ( effectiveParams.longitudinalSpacing > 0.0 ) ? effectiveParams.longitudinalSpacing : CROWD_DEFAULT_LONGITUDINAL_SPACING;

		// Calculate total guide path length to bound reverse sampling:
		double totalGuidePathLen = 0.0;
		for ( size_t p = 0; p + 1 < guidePath.size(); p++ ) {
			totalGuidePathLen += QM_Vector3DistanceDP( guidePath[ p ], guidePath[ p + 1 ] );
		}

		// Maximum spacing allowable per slot. Must never be less than minSpacing to guarantee valid bounds for std::clamp
		// and prevent large squads (size >= 10) from stretching beyond the destination arrival zone:
		const double minSquadDepth = ( members.size() > 1 ) ? ( static_cast<double>( members.size() - 1 ) * minSpacing ) : minSpacing;
		const double pathBoundDepth = totalGuidePathLen * CROWD_COLUMN_PATH_MAX_FRACTION;
		const double maxDepth = std::max( minSquadDepth, std::min( CROWD_DESTINATION_COLUMN_MAX_DEPTH, pathBoundDepth ) );
		const double maxSpacing = ( members.size() > 1 ) ? ( maxDepth / static_cast<double>( members.size() - 1 ) ) : minSpacing;
		const double colSpacing = ( maxSpacing >= minSpacing ) ? std::clamp( rawSpacing, minSpacing, maxSpacing ) : minSpacing;

		for ( size_t i = 0; i < members.size(); i++ ) {
			svg_crowd_slot_t slot;
			slot.slotIndex = static_cast<int32_t>( i );
			slot.role = ( i == 0 ) ? crowd_member_role_t::ROLE_POINT : crowd_member_role_t::ROLE_CENTER;

			Vector3DP pathPos = origin;
			Vector3DP pathTangent = Vector3DP{ 1.0, 0.0, 0.0 };
			const double targetDistBack = static_cast<double>( i ) * colSpacing;

			if ( SVG_Crowd_SampleGuidePathInReverse( guidePath, targetDistBack, &pathPos, &pathTangent ) ) {
				slot.worldPosition = pathPos;
			} else {
				// Fallback if path ran out: extend along backwards tangent:
				slot.worldPosition = pathPos - ( pathTangent * targetDistBack );
			}
			slot.relativeYawDeg = 0.0;
			slot.isNavmeshValid = true;
			group.slots.push_back( slot );
		}
		// Determine maximum dynamic agent radius across all squad members:
		double maxAgentRadius = CROWD_DEFAULT_AGENT_RADIUS;
		for ( const svg_base_edict_t *ent : members ) {
			if ( ent ) {
				const double rawRadius = static_cast<double>( ( ent->maxs.x - ent->mins.x ) * 0.5f );
				if ( rawRadius > maxAgentRadius ) {
					maxAgentRadius = rawRadius;
				}
			}
		}
		SVG_Crowd_SnapSlotsToNavMesh( group.slots, origin, maxAgentRadius );
	} else if ( effectiveStyle == crowd_chase_target_type_t::CROWD_STYLE_TACTICAL_COVER ) {
		SVG_Crowd_AllocateTacticalCover( group, members, origin, group.slots );
	} else {
		// Determine maximum dynamic agent radius across all squad members:
		double maxAgentRadius = CROWD_DEFAULT_AGENT_RADIUS;
		for ( const svg_base_edict_t *ent : members ) {
			if ( ent ) {
				const double rawRadius = static_cast<double>( ( ent->maxs.x - ent->mins.x ) * 0.5f );
				if ( rawRadius > maxAgentRadius ) {
					maxAgentRadius = rawRadius;
				}
			}
		}
		SVG_Crowd_GenerateFormationSlots( effectiveStyle, members.size(), effectiveParams, group.slots );
		SVG_Crowd_TransformLocalSlotsToWorld( effectiveDestOrigin, group.currentHeadingYaw, group.slots );
		SVG_Crowd_SnapSlotsToNavMesh( group.slots, effectiveDestOrigin, maxAgentRadius );
		const double minPhysicalSeparation = ( maxAgentRadius * 2.0 ) + CROWD_GROUND_STOP_BUFFER;
		const double minSlotSeparation = std::max( minPhysicalSeparation, std::min( effectiveParams.lateralSpacing, effectiveParams.longitudinalSpacing ) );
		if ( effectiveStyle == crowd_chase_target_type_t::CROWD_STYLE_CIRCLE_FILLED ||
			 effectiveStyle == crowd_chase_target_type_t::CROWD_STYLE_SURROUND_PERIMETER ) {
			Vector3DP ingressDir = effectiveDestOrigin - centroid;
			if ( guidePath.size() >= 2 ) {
				ingressDir = guidePath.back() - guidePath[ guidePath.size() - 2 ];
			}
			ingressDir.z = 0.0;
			SVG_Crowd_FitCircularFormationToRoom( group.slots, effectiveDestOrigin, maxAgentRadius, hasPortal ? &portalPoint : nullptr, QM_Vector3LengthDP( ingressDir ) > 0.001 ? &ingressDir : nullptr );
		}
		SVG_Crowd_ResolveSlotCollisionsAndInvalidSlots( group.slots, effectiveDestOrigin, group.currentHeadingYaw, minSlotSeparation, maxAgentRadius, guidePath.empty() ? nullptr : &guidePath, effectiveParams.tacticalFlags, hasPortal ? &portalPoint : nullptr );
	}
	// Extract member feet origins in Vector3DP using distinct solid shape geometry.
	std::vector<Vector3DP> memberOrigins;
	memberOrigins.reserve( members.size() );
	for ( const svg_base_edict_t *ent : members ) {
		memberOrigins.emplace_back( SVG_GetEntityFeetOriginDP( ent ) );
	}

	// Preserve the previous member->slot mapping as assignment hysteresis so re-orders
	// (and any runtime reassignment pass) do not flip two members' slots back and forth
	// — slot thrash manifests visibly as yaw jitter and members sliding into each other
	// while swapping spots. Members whose old slot index is now out of range fall back
	// to -1 (fresh assignment).
	std::vector<int32_t> previousSlotMap( members.size(), -1 );
	for ( size_t i = 0; i < members.size(); i++ ) {
		if ( members[ i ] && members[ i ]->crowd.slotIndex >= 0 && members[ i ]->crowd.slotIndex < static_cast<int32_t>( group.slots.size() ) ) {
			previousSlotMap[ i ] = members[ i ]->crowd.slotIndex;
		}
	}
	std::vector<int32_t> slotMapping;

	const bool isCircleOrDefensive = ( effectiveStyle == crowd_chase_target_type_t::CROWD_STYLE_CIRCLE_FILLED ||
	                                   effectiveStyle == crowd_chase_target_type_t::CROWD_STYLE_SURROUND_PERIMETER ||
	                                   effectiveStyle == crowd_chase_target_type_t::CROWD_STYLE_TACTICAL_COVER );
	const bool isSquadConstrained = ( squadClearance < CROWD_CONSTRAINED_AREA_CLEARANCE_LIMIT );
	const bool isDestConstrainedArea = ( destClearance < CROWD_CONSTRAINED_AREA_CLEARANCE_LIMIT );
	const bool isRouteChokepoint = ( routeBottleneckClearance < CROWD_MIN_TWO_AGENT_ABREAST_WIDTH );
	const bool isConstrainedIngress = !isCircleOrDefensive && params.enableCorridorSqueeze &&
	                                  ( group.dynamicSqueezeFactor < CROWD_CONSTRAINED_INGRESS_SQUEEZE_THRESHOLD ||
	                                    isSquadConstrained || isDestConstrainedArea || isRouteChokepoint || hasRouteStairs );
	const bool isInflowConstrained = ( isConstrainedIngress || isDestConstrainedArea || hasRouteStairs || isRouteChokepoint || hasPortal );

	// Single-file column along guide path: slots were sampled along the arrival corridor at destination.
	// Rank members monotonically by projected progress along guidePath so leading corridor agents take front slots:
	if ( effectiveStyle == crowd_chase_target_type_t::CROWD_STYLE_COLUMN_MARCH && guidePath.size() >= 2 ) {
		group.ingressDirection = Vector3DP{ 0.0, 0.0, 0.0 };

		// Precompute cumulative arc-lengths along guidePath from destination back to start:
		const size_t numPoints = guidePath.size();
		std::vector<double> distToGoalAtPoint( numPoints, 0.0 );
		for ( size_t p = numPoints - 1; p > 0; p-- ) {
			distToGoalAtPoint[ p - 1 ] = distToGoalAtPoint[ p ] + QM_Vector3DistanceDP( guidePath[ p - 1 ], guidePath[ p ] );
		}

		// Rank members by projected path distance along guidePath to origin:
		std::vector<std::pair<double, int32_t>> memberDistRanks;
		memberDistRanks.reserve( members.size() );

		for ( size_t m = 0; m < members.size(); m++ ) {
			const Vector3DP &mPos = memberOrigins[ m ];
			double bestDistAlongPath = std::numeric_limits<double>::max();
			double bestOrthogDistSq = std::numeric_limits<double>::max();

			// Project member position onto closest guidePath segment:
			for ( size_t p = 0; p + 1 < numPoints; p++ ) {
				const Vector3DP &segA = guidePath[ p ];
				const Vector3DP &segB = guidePath[ p + 1 ];
				const Vector3DP segVec = segB - segA;
				const double segLenSq = QM_Vector3LengthSqrDP( segVec );
				double t = 0.0;
				if ( segLenSq > 0.0001 ) {
					t = std::clamp( QM_Vector3DotProductDP( mPos - segA, segVec ) / segLenSq, 0.0, 1.0 );
				}
				const Vector3DP projPoint = segA + ( segVec * t );
				const double orthogDistSq = QM_Vector3DistanceSqrDP( mPos, projPoint );

				if ( orthogDistSq < bestOrthogDistSq ) {
					bestOrthogDistSq = orthogDistSq;
					const double segLen = std::sqrt( segLenSq );
					const double remDistOnSeg = ( 1.0 - t ) * segLen;
					bestDistAlongPath = remDistOnSeg + distToGoalAtPoint[ p + 1 ];
				}
			}

			// Fallback to direct Euclidean distance if projection was unavailable:
			if ( bestDistAlongPath == std::numeric_limits<double>::max() ) {
				bestDistAlongPath = QM_Vector3DistanceDP( mPos, origin );
			}
			memberDistRanks.emplace_back( bestDistAlongPath, static_cast<int32_t>( m ) );
		}

		std::sort( memberDistRanks.begin(), memberDistRanks.end(), []( const auto &a, const auto &b ) {
			return a.first < b.first;
		} );

		slotMapping.assign( members.size(), 0 );
		for ( size_t rank = 0; rank < memberDistRanks.size(); rank++ ) {
			const int32_t memberIdx = memberDistRanks[ rank ].second;
			slotMapping[ memberIdx] = static_cast<int32_t>( rank );
		}
	} else {
		// Sort slots deepest-first along squad ingress approach vector when entering or exiting
		// an enclosed corridor/room where squeeze or bottleneck is active (either at destination, at squad origin,
		// or anywhere along traversed route faces).
		// In unconstrained open space or for radial defense rings (CIRCLE_FILLED, SURROUND_PERIMETER),
		// slots remain anchored to their radial rings with leader at slot 0 and anti-crossover hysteresis assignment.
		Vector3DP ingressDir = origin - centroid;
		if ( guidePath.size() >= 2 ) {
			ingressDir = guidePath.back() - guidePath[ guidePath.size() - 2 ];
		}
		ingressDir.z = 0.0;
		if ( QM_Vector3LengthDP( ingressDir ) > 0.001 ) {
			group.ingressDirection = QM_Vector3NormalizeDP( ingressDir );
			// Sort slots by ingress depth for column/marching formations in open terrain; for circles/defensive rings
			// or room-packed layouts, slot order is already established and must not be overwritten:
			if ( !isCircleOrDefensive && !hasPortal ) {
				SVG_Crowd_SortSlotsByIngressDepth( group.slots, origin, group.ingressDirection );
			}
		} else {
			group.ingressDirection = Vector3DP{ 0.0, 0.0, 0.0 };
		}

		// Match members to formation slots using ingress-depth and approach progress ordering.
		// When entering an enclosed room (hasPortal) or constricted chokepoint, ALWAYS use SVG_Crowd_AssignMembersToSlotsIngress
		// so leading incoming squad members take the deepest interior slots (back wall/corners) and never block the entrance:
		if ( hasPortal || ( QM_Vector3LengthDP( group.ingressDirection ) > 0.001 && isInflowConstrained ) ) {
			SVG_Crowd_AssignMembersToSlotsIngress( memberOrigins, group.slots, previousSlotMap, slotMapping, effectiveDestOrigin, group.ingressDirection, CROWD_DEFAULT_MIN_CORRIDOR_SPACING, hasPortal ? &portalPoint : nullptr, guidePath.empty() ? nullptr : &guidePath );
		} else {
			SVG_Crowd_AssignMembersToSlotsHysteresis( memberOrigins, group.slots, previousSlotMap, slotMapping );
		}
	}

	// Ensure squad leader receives slot 0 (Point / Lead / Anchor) in open unconstrained space.
	// In constrained ingress through doorway portals, monotonic queue ordering is strictly preserved to prevent crossover deadlocks:
	if ( !hasPortal && !isInflowConstrained && group.leaderEntityNumber != ENTITYNUM_NONE && !group.slots.empty() ) {
		int32_t leaderIdx = -1;
		for ( size_t i = 0; i < members.size(); i++ ) {
			if ( members[ i ]->s.number == group.leaderEntityNumber ) {
				leaderIdx = static_cast<int32_t>( i );
				break;
			}
		}

		if ( leaderIdx >= 0 && leaderIdx < static_cast<int32_t>( slotMapping.size() ) && slotMapping[ leaderIdx ] > 0 ) {
			for ( size_t i = 0; i < slotMapping.size(); i++ ) {
				if ( slotMapping[ i ] == 0 ) {
					slotMapping[ i ] = slotMapping[ leaderIdx ];
					slotMapping[ leaderIdx ] = 0;
					break;
				}
			}
		}
	}

	// Dispatch individual goal destinations and lease cover points.
	for ( size_t i = 0; i < members.size(); i++ ) {
		svg_base_edict_t *member = members[ i ];
		const int32_t slotIdx = slotMapping[ i ];

		if ( slotIdx >= 0 && slotIdx < static_cast<int32_t>( group.slots.size() ) ) {
			const svg_crowd_slot_t &slot = group.slots[ slotIdx ];

			// Release previous cover if changing.
			if ( member->crowd.activeCoverIdx >= 0 && member->crowd.activeCoverIdx != slot.coverIndex ) {
				Nav_ReleaseCoverPoint( member->crowd.activeCoverIdx, member->s.number );
				member->crowd.activeCoverIdx = -1;
			}

			// Claim new cover if applicable.
			if ( slot.coverIndex >= 0 ) {
				Nav_ClaimCoverPoint( slot.coverIndex, member->s.number );
				member->crowd.activeCoverIdx = slot.coverIndex;
			}

			member->crowd.slotIndex = slotIdx;
			// The protected leader is strictly immutable: only the designated leader holds ROLE_LEADER
			if ( group.leaderEntityNumber != ENTITYNUM_NONE && member->s.number == group.leaderEntityNumber ) {
				member->crowd.role = crowd_member_role_t::ROLE_LEADER;
			} else {
				member->crowd.role = ( slot.role == crowd_member_role_t::ROLE_LEADER ) ? crowd_member_role_t::ROLE_CENTER : slot.role;
			}
			member->crowd.assignedGoalOrigin = QM_Vector3FromDP( slot.worldPosition );
			member->crowd.reachedGoal = false;
			member->crowd.blockedStartTime = 0_ms;
			member->crowd.startTimeForSeeking = level.time;
			member->crowd.maxTimeToSeek = params.maxTimeToSeek;
			member->crowd.leaderEntityNumber = group.leaderEntityNumber;
			// Stagger initial path calculation time across squad members.
			member->crowd.lastPathCalcTime = level.time + QMTime::FromMilliseconds( static_cast<int64_t>( i * params.pathStaggerMs ) );
			member->nextthink = level.time + FRAME_TIME_MS;

			// Clear obsolete paths and force fresh pathfinding toward the new destination:
			svg_monster_base_t *monster = dynamic_cast<svg_monster_base_t*>( member );
			if ( monster != nullptr ) {
				monster->ResetNavigationPath();
				monster->lastPathCalcTime = 0_ms;
				monster->consecutiveBlockedFrames = 0;
			}
		} else {
			/**
			*	Mapping fallback: if no concrete slot is available, still force movement toward the
			*	new crowd destination center so members never remain parked in the previous room.
			**/
			if ( member->crowd.activeCoverIdx >= 0 ) {
				Nav_ReleaseCoverPoint( member->crowd.activeCoverIdx, member->s.number );
				member->crowd.activeCoverIdx = -1;
			}

			member->crowd.slotIndex = -1;
			if ( group.leaderEntityNumber != ENTITYNUM_NONE && member->s.number == group.leaderEntityNumber ) {
				member->crowd.role = crowd_member_role_t::ROLE_LEADER;
			} else {
				member->crowd.role = crowd_member_role_t::ROLE_CENTER;
			}
			member->crowd.assignedGoalOrigin = QM_Vector3FromDP( origin );
			member->crowd.reachedGoal = false;
			member->crowd.blockedStartTime = 0_ms;
			member->crowd.startTimeForSeeking = level.time;
			member->crowd.maxTimeToSeek = params.maxTimeToSeek;
			member->crowd.leaderEntityNumber = group.leaderEntityNumber;
			member->crowd.lastPathCalcTime = 0_ms;
			member->nextthink = level.time + FRAME_TIME_MS;

			svg_monster_base_t *monster = dynamic_cast<svg_monster_base_t*>( member );
			if ( monster != nullptr ) {
				monster->ResetNavigationPath();
				monster->lastPathCalcTime = 0_ms;
				monster->consecutiveBlockedFrames = 0;
			}
		}
	}

	/**
	*	Serialize constrained room ingress without mutating the immutable final formation slots.
	**/
	if ( isCircleOrDefensive && group.targetEntityNumber == ENTITYNUM_NONE && members.size() > 1 ) {
		/**
		* Guarantee a finite admission plane for every static defensive room order.
		* Missing generated portal metadata must never fall back to simultaneous direct movement.
		**/
		if ( !hasPortal ) {
			Vector3DP fallbackInward = effectiveDestOrigin - centroid;
			fallbackInward.z = 0.0;
			if ( QM_Vector3LengthSqrDP( fallbackInward ) <= 0.0001 && guidePath.size() >= 2 ) {
				fallbackInward = guidePath.back() - guidePath.front();
				fallbackInward.z = 0.0;
			}
			if ( QM_Vector3LengthSqrDP( fallbackInward ) <= 0.0001 ) {
				const double headingRadians = static_cast<double>( group.currentHeadingYaw ) * ( M_PI / 180.0 );
				fallbackInward = Vector3DP{ std::cos( headingRadians ), std::sin( headingRadians ), 0.0 };
			}
			fallbackInward = QM_Vector3NormalizeDP( fallbackInward );
			const double fallbackDepth = std::max( 96.0,
				( CROWD_DEFAULT_AGENT_RADIUS * 2.0 ) + CROWD_ROOM_FILL_DOOR_LANE_MARGIN );
			portalPoint = effectiveDestOrigin - ( fallbackInward * fallbackDepth );
			portalWidth = CROWD_DEFAULT_AGENT_RADIUS * 2.0;
			hasPortal = true;
		}

		double maxAgentRadius = CROWD_DEFAULT_AGENT_RADIUS;
		for ( const svg_base_edict_t *member : members ) {
			// Expand the portal release plane to clear the largest member hull.
			if ( member == nullptr ) {
				continue;
			}
			const double memberRadius = static_cast<double>( member->maxs.x - member->mins.x ) * 0.5;
			maxAgentRadius = std::max( maxAgentRadius, memberRadius );
		}
		const bool ingressConfigured = SVG_Crowd_ConfigureSerializedIngress( group, members, guidePath,
			portalPoint, effectiveDestOrigin, destRoom, portalWidth, maxAgentRadius );
		gi.dprintf( "[crowd ingress] crowd=%" PRId32 " style=%" PRId32 " members=%zu portal=%d configured=%d queue=%zu\n",
			group.crowdID, static_cast<int32_t>( effectiveStyle ), members.size(), hasPortal ? 1 : 0,
			ingressConfigured ? 1 : 0, group.ingressQueueEntityNumbers.size() );
	}

	/**
	*	Configure serialized in-order room egress when departing an enclosed room/zone through a narrow doorway.
	**/
	if ( members.size() > 1 ) {
		const nav_room_t *startRoom = Nav_GetRoomForPoint( centroid );
		if ( startRoom == nullptr && startFace >= 0 && startFace < static_cast<int32_t>( g_nav_faces.size() ) ) {
			const int32_t startRoomID = g_nav_faces[ startFace ].room_id;
			if ( startRoomID >= 0 && startRoomID < static_cast<int32_t>( g_nav_rooms.size() ) ) {
				startRoom = &g_nav_rooms[ startRoomID ];
			}
		}
		if ( startRoom == nullptr ) {
			for ( const svg_base_edict_t *m : members ) {
				if ( m != nullptr && SVG_Entity_IsActive( m ) ) {
					startRoom = Nav_GetRoomForPoint( SVG_GetEntityFeetOriginDP( m ) );
					if ( startRoom != nullptr ) {
						break;
					}
				}
			}
		}
		const bool egressConfigured = SVG_Crowd_ConfigureSerializedEgress( group, members, guidePath, navPathFaces, startRoom, destRoom, centroid );
		gi.dprintf( "[crowd egress] crowd=%" PRId32 " start_room=%" PRId32 " members=%zu configured=%d queue=%zu\n",
			group.crowdID, startRoom != nullptr ? startRoom->room_id : -1, members.size(),
			egressConfigured ? 1 : 0, group.egressQueueEntityNumbers.size() );
	}

	return true;
}

/**
*	@brief	Command a crowd to navigate to a world origin in Vector3 with the specified formation style (Vector3 overload).
*	@param	crowdID	Crowd group identifier.
*	@param	origin	World-space destination origin in Vector3.
*	@param	style	Formation / chase style.
*	@param	params	Formation parameters (spacing, cover distance, etc.).
*	@return	True when orders were successfully dispatched to one or more crowd members.
**/
bool MoveAStarCrowdOrigin( const int32_t crowdID, const Vector3 &origin, const crowd_chase_target_type_t style, const svg_crowd_params_t &params ) {
	return MoveAStarCrowdOrigin( crowdID, Vector3DP( origin ), style, params );
}

/**
*	@brief	Command a crowd to follow an entity by entity number in the specified formation style.
*	@param	crowdID				Crowd group identifier.
*	@param	targetEntityNumber	Target entity number to follow.
*	@param	style				Formation / chase style.
*	@param	params				Formation parameters (spacing, cover distance, etc.).
*	@return	True when orders were successfully dispatched to one or more crowd members.
**/
bool MoveAStarFollowEntity( const int32_t crowdID, const int32_t targetEntityNumber, const crowd_chase_target_type_t style, const svg_crowd_params_t &params ) {
	if ( crowdID < 0 || targetEntityNumber == ENTITYNUM_NONE || targetEntityNumber < 0 || targetEntityNumber >= globals.edictPool->num_edicts ) {
		return false;
	}

	svg_base_edict_t *targetEnt = g_edict_pool.EdictForNumber( targetEntityNumber );
	if ( !targetEnt || !SVG_Entity_IsActive( targetEnt ) || targetEnt->health <= 0 ) {
		return false;
	}

	std::vector<svg_base_edict_t*> members;
	SVG_Crowd_GetCrowdMembers( crowdID, members );
	if ( members.empty() ) {
		return false;
	}

	svg_crowd_group_t &group = g_crowd_groups[ crowdID ];
	group.crowdID = crowdID;
	group.style = style;
	group.params = params;
	group.destinationOrigin = Vector3DP( targetEnt->currentOrigin );
	group.targetEntityNumber = targetEntityNumber;
	group.lastTargetEntityOrigin = Vector3DP( targetEnt->currentOrigin );
	group.orderStartTime = level.time;
	group.isMoving = true;
	SVG_Crowd_ResetSerializedIngress( group, members );
	SVG_Crowd_ResetSerializedEgress( group, members );

	const Vector3DP centroid = SVG_Crowd_ComputeCentroid( members );
	group.currentHeadingYaw = SVG_Crowd_CalculateHeadingYaw( centroid, Vector3DP( targetEnt->currentOrigin ), targetEnt, params );

	// Calculate dynamic corridor clearance and squeeze factor if enabled.
	svg_crowd_params_t effectiveParams = params;
	if ( params.enableCorridorSqueeze ) {
		const double desiredWidth = ( members.size() > 1 ) ? ( static_cast<double>( members.size() - 1 ) * params.lateralSpacing ) : params.lateralSpacing;
		const double clearance = SVG_Crowd_ComputeCorridorClearance( Vector3DP( targetEnt->currentOrigin ), desiredWidth );
		const double squeeze = ( desiredWidth > 0.0 ) ? std::clamp( clearance / desiredWidth, 0.2, 1.0 ) : 1.0;
		group.dynamicSqueezeFactor = squeeze;
		effectiveParams.lateralSpacing = std::max( params.minCorridorSpacing, params.lateralSpacing * squeeze );
		effectiveParams.longitudinalSpacing = std::max( params.minCorridorSpacing, params.longitudinalSpacing * squeeze );
	} else {
		group.dynamicSqueezeFactor = 1.0;
	}

	if ( style == crowd_chase_target_type_t::CROWD_STYLE_TACTICAL_COVER ) {
		SVG_Crowd_AllocateTacticalCover( group, members, Vector3DP( targetEnt->currentOrigin ), group.slots );
	} else {
		SVG_Crowd_GenerateFormationSlots( style, members.size(), effectiveParams, group.slots );
		SVG_Crowd_TransformLocalSlotsToWorld( Vector3DP( targetEnt->currentOrigin ), group.currentHeadingYaw, group.slots );
		SVG_Crowd_SnapSlotsToNavMesh( group.slots, Vector3DP( targetEnt->currentOrigin ) );
		const double distToDest = QM_Vector3DistanceDP( Vector3DP( targetEnt->currentOrigin ), centroid );
		const double minPhysicalSeparation = ( CROWD_DEFAULT_AGENT_RADIUS * 2.0 ) + 4.0;
		const double minSlotSeparation = std::max( minPhysicalSeparation, std::min( effectiveParams.lateralSpacing, effectiveParams.longitudinalSpacing ) );

		// If squad is approaching destination across a distance or through corridors, generate guide path:
		std::vector<Vector3DP> chaseGuidePath;
		Vector3DP chasePortalPoint = Vector3DP( targetEnt->currentOrigin );
		bool hasChasePortal = false;
		if ( distToDest > 128.0 && !g_nav_faces.empty() ) {
			const int32_t startPoly = Nav_FindPolyInLeaf( centroid );
			const int32_t goalPoly = Nav_FindPolyInLeaf( Vector3DP( targetEnt->currentOrigin ) );
			if ( startPoly >= 0 && goalPoly >= 0 ) {
				std::vector<int32_t> facePath;
				nav_path_policy_t navPol{};
				navPol.agent_radius = CROWD_DEFAULT_AGENT_RADIUS;
				if ( Nav_FindPath( startPoly, goalPoly, facePath, navPol ) ) {
					std::vector<bool> chaseForcedWps;
					Nav_StringPull( facePath, centroid, Vector3DP( targetEnt->currentOrigin ), CROWD_DEFAULT_AGENT_RADIUS, chaseGuidePath, &chaseForcedWps );

					if ( facePath.size() >= 2 ) {
						for ( int32_t p = static_cast<int32_t>( facePath.size() ) - 2; p >= 0; --p ) {
							const int32_t fIdx = facePath[ p ];
							if ( fIdx >= 0 && fIdx < static_cast<int32_t>( g_nav_faces.size() ) ) {
								const nav_face_t &face = g_nav_faces[ fIdx ];
								if ( face.clearance > 0.0 && ( face.clearance * 2.0 ) < CROWD_PORTAL_BOTTLENECK_MAX_WIDTH ) {
									chasePortalPoint = face.center;
									hasChasePortal = true;
									break;
								}
							}
						}
						if ( !hasChasePortal && facePath.size() >= 2 ) {
							const int32_t fIdx = facePath[ facePath.size() - 2 ];
							if ( fIdx >= 0 && fIdx < static_cast<int32_t>( g_nav_faces.size() ) ) {
								chasePortalPoint = g_nav_faces[ fIdx ].center;
								hasChasePortal = true;
							}
						}
					}
				}
			}
		}

		SVG_Crowd_ResolveSlotCollisionsAndInvalidSlots( group.slots, Vector3DP( targetEnt->currentOrigin ), group.currentHeadingYaw, minSlotSeparation, CROWD_DEFAULT_AGENT_RADIUS, chaseGuidePath.empty() ? nullptr : &chaseGuidePath, effectiveParams.tacticalFlags, hasChasePortal ? &chasePortalPoint : nullptr );

		const bool isCircleOrDefensive = ( style == crowd_chase_target_type_t::CROWD_STYLE_CIRCLE_FILLED ||
		                                   style == crowd_chase_target_type_t::CROWD_STYLE_SURROUND_PERIMETER ||
		                                   style == crowd_chase_target_type_t::CROWD_STYLE_TACTICAL_COVER );
		const bool isConstrainedIngress = ( !isCircleOrDefensive && params.enableCorridorSqueeze && group.dynamicSqueezeFactor < 0.95 && distToDest > 128.0 );

		if ( isConstrainedIngress ) {
			Vector3DP ingressDir = Vector3DP( targetEnt->currentOrigin ) - centroid;
			ingressDir.z = 0.0;
			if ( QM_Vector3LengthDP( ingressDir ) > 0.001 ) {
				group.ingressDirection = QM_Vector3NormalizeDP( ingressDir );
				SVG_Crowd_SortSlotsByIngressDepth( group.slots, Vector3DP( targetEnt->currentOrigin ), group.ingressDirection );
			} else {
				group.ingressDirection = Vector3DP{ 0.0, 0.0, 0.0 };
			}
		} else {
			group.ingressDirection = Vector3DP{ 0.0, 0.0, 0.0 };
		}
	}

	std::vector<Vector3DP> memberOrigins;
	memberOrigins.reserve( members.size() );
	for ( const svg_base_edict_t *ent : members ) {
		memberOrigins.emplace_back( SVG_GetEntityFeetOriginDP( ent ) );
	}

	std::vector<int32_t> previousSlotMap( members.size(), -1 );
	for ( size_t i = 0; i < members.size(); i++ ) {
		previousSlotMap[ i ] = members[ i ]->crowd.slotIndex;
	}

	// Match members to formation slots using ingress-depth and approach progress ordering.
	std::vector<int32_t> slotMapping;
	if ( QM_Vector3LengthDP( group.ingressDirection ) > 0.001 ) {
		SVG_Crowd_AssignMembersToSlotsIngress( memberOrigins, group.slots, previousSlotMap, slotMapping, SVG_GetEntityFeetOriginDP( targetEnt ), group.ingressDirection );
	} else {
		SVG_Crowd_AssignMembersToSlotsHysteresis( memberOrigins, group.slots, previousSlotMap, slotMapping );
	}

	// Ensure squad leader receives slot 0 (Point / Lead) if designated.
	if ( group.leaderEntityNumber != ENTITYNUM_NONE && !group.slots.empty() ) {
		int32_t leaderIdx = -1;
		for ( size_t i = 0; i < members.size(); i++ ) {
			if ( members[ i ]->s.number == group.leaderEntityNumber ) {
				leaderIdx = static_cast<int32_t>( i );
				break;
			}
		}

		if ( leaderIdx >= 0 && leaderIdx < static_cast<int32_t>( slotMapping.size() ) && slotMapping[ leaderIdx ] > 0 ) {
			for ( size_t i = 0; i < slotMapping.size(); i++ ) {
				if ( slotMapping[ i ] == 0 ) {
					slotMapping[ i ] = slotMapping[ leaderIdx ];
					slotMapping[ leaderIdx ] = 0;
					break;
				}
			}
		}
	}

	for ( size_t i = 0; i < members.size(); i++ ) {
		svg_base_edict_t *member = members[ i ];
		const int32_t slotIdx = slotMapping[ i ];

		if ( slotIdx >= 0 && slotIdx < static_cast<int32_t>( group.slots.size() ) ) {
			const svg_crowd_slot_t &slot = group.slots[ slotIdx ];

			if ( member->crowd.activeCoverIdx >= 0 && member->crowd.activeCoverIdx != slot.coverIndex ) {
				Nav_ReleaseCoverPoint( member->crowd.activeCoverIdx, member->s.number );
				member->crowd.activeCoverIdx = -1;
			}

			if ( slot.coverIndex >= 0 ) {
				Nav_ClaimCoverPoint( slot.coverIndex, member->s.number );
				member->crowd.activeCoverIdx = slot.coverIndex;
			}

			const Vector3 newGoal = QM_Vector3FromDP( slot.worldPosition );
			const double goalDistDelta = QM_Vector3Distance( newGoal, member->crowd.assignedGoalOrigin );

			/**
			*	Goal-update hysteresis:
			*	Do NOT force a path reset when the newly assigned slot resolves to essentially the
			*	same world goal the member is already navigating toward. Resetting the navigation
			*	path for a sub-arrival-radius goal delta re-plans a route through the current crowd
			*	configuration every re-order, re-wedging members against each other at the doorway
			*	each time. Keep the existing path and arrival state for near-identical goals.
			**/
			const bool sameWorldGoal = ( goalDistDelta <= CROWD_DEFAULT_ARRIVAL_RADIUS );

			member->crowd.slotIndex = slotIdx;
			// The protected leader is strictly immutable: only the designated leader holds ROLE_LEADER
			if ( group.leaderEntityNumber != ENTITYNUM_NONE && member->s.number == group.leaderEntityNumber ) {
				member->crowd.role = crowd_member_role_t::ROLE_LEADER;
			} else {
				member->crowd.role = ( slot.role == crowd_member_role_t::ROLE_LEADER ) ? crowd_member_role_t::ROLE_CENTER : slot.role;
			}
			member->crowd.assignedGoalOrigin = newGoal;
			// Only reset arrival state if the assigned slot moved significantly:
			if ( goalDistDelta > CROWD_DEFAULT_ARRIVAL_RADIUS ) {
				member->crowd.reachedGoal = false;
				member->crowd.blockedStartTime = 0_ms;
				svg_monster_base_t *monster = dynamic_cast<svg_monster_base_t*>( member );
				if ( monster != nullptr ) {
					monster->ResetNavigationPath();
					monster->lastPathCalcTime = 0_ms;
					monster->consecutiveBlockedFrames = 0;
				}
			}
			member->crowd.leaderEntityNumber = group.leaderEntityNumber;
			member->nextthink = level.time + FRAME_TIME_MS;
		} else {
			/**
			*	Mapping fallback: never leave stale goals from a previous room when slot mapping is incomplete.
			*	Force this member to seek the new destination anchor and clear any previous cover ownership.
			**/
			if ( member->crowd.activeCoverIdx >= 0 ) {
				Nav_ReleaseCoverPoint( member->crowd.activeCoverIdx, member->s.number );
				member->crowd.activeCoverIdx = -1;
			}

			member->crowd.slotIndex = -1;
			if ( group.leaderEntityNumber != ENTITYNUM_NONE && member->s.number == group.leaderEntityNumber ) {
				member->crowd.role = crowd_member_role_t::ROLE_LEADER;
			} else {
				member->crowd.role = crowd_member_role_t::ROLE_CENTER;
			}
			member->crowd.assignedGoalOrigin = targetEnt->currentOrigin;
			member->crowd.reachedGoal = false;
			member->crowd.blockedStartTime = 0_ms;
			member->crowd.startTimeForSeeking = level.time;
			member->crowd.maxTimeToSeek = params.maxTimeToSeek;
			member->crowd.leaderEntityNumber = group.leaderEntityNumber;
			member->crowd.lastPathCalcTime = 0_ms;
			member->nextthink = level.time + FRAME_TIME_MS;

			svg_monster_base_t *monster = dynamic_cast<svg_monster_base_t*>( member );
			if ( monster != nullptr ) {
				monster->ResetNavigationPath();
				monster->lastPathCalcTime = 0_ms;
				monster->consecutiveBlockedFrames = 0;
			}
		}
	}

	return true;
}

/**
*	@brief	Command a crowd to follow an entity in the specified formation style (convenience pointer overload).
*	@param	crowdID	Crowd identifier.
*	@param	entity	Target entity to follow.
*	@param	style	Formation / chase style.
*	@param	params	Formation parameters (spacing, cover distance, etc.).
*	@return	True when orders were successfully dispatched to one or more crowd members.
**/
bool MoveAStarFollowEntity( const int32_t crowdID, svg_base_edict_t *entity, const crowd_chase_target_type_t style, const svg_crowd_params_t &params ) {
	return MoveAStarFollowEntity( crowdID, entity ? entity->s.number : ENTITYNUM_NONE, style, params );
}

/**
*	@brief	Update the formation style of an active crowd group.
*	@param	crowdID	Crowd identifier.
*	@param	style	New formation style.
**/
void SVG_Crowd_SetCrowdStyle( const int32_t crowdID, const crowd_chase_target_type_t style ) {
	auto it = g_crowd_groups.find( crowdID );
	if ( it == g_crowd_groups.end() ) {
		return;
	}

	it->second.style = style;
	if ( it->second.isMoving ) {
		if ( it->second.targetEntityNumber != ENTITYNUM_NONE ) {
			MoveAStarFollowEntity( crowdID, it->second.targetEntityNumber, style, it->second.params );
		} else {
			MoveAStarCrowdOrigin( crowdID, it->second.destinationOrigin, style, it->second.params );
		}
	}
}

/**
*	@brief	Update the configuration parameters of an active crowd group.
*	@param	crowdID	Crowd identifier.
*	@param	params	New configuration parameters.
**/
void SVG_Crowd_SetCrowdParams( const int32_t crowdID, const svg_crowd_params_t &params ) {
	auto it = g_crowd_groups.find( crowdID );
	if ( it == g_crowd_groups.end() ) {
		return;
	}

	it->second.params = params;
	if ( it->second.isMoving ) {
		if ( it->second.targetEntityNumber != ENTITYNUM_NONE ) {
			MoveAStarFollowEntity( crowdID, it->second.targetEntityNumber, it->second.style, params );
		} else {
			MoveAStarCrowdOrigin( crowdID, it->second.destinationOrigin, it->second.style, params );
		}
	}
}

/**
*	@brief	Halt and clear movement orders for an active crowd group.
*	@param	crowdID	Crowd identifier.
**/
void SVG_Crowd_StopCrowd( const int32_t crowdID ) {
	SVG_Squad_Stop( crowdID );
	auto it = g_crowd_groups.find( crowdID );
	if ( it != g_crowd_groups.end() ) {
		it->second.isMoving = false;
		it->second.hasSerializedIngress = false;
		it->second.ingressPhase = crowd_ingress_phase_t::INACTIVE;
		it->second.ingressPortalHalfWidth = 0.0;
		it->second.ingressQueueEntityNumbers.clear();
		it->second.ingressStagingPositions.clear();
		it->second.ingressStagingLine.clear();
		it->second.ingressQueueHead = 0;
		it->second.ingressMarshalCursor = 0;
		it->second.ingressQueueHeadStartTime = 0_ms;
		it->second.hasSerializedEgress = false;
		it->second.egressPortalHalfWidth = 0.0;
		it->second.egressQueueEntityNumbers.clear();
		it->second.egressQueueHead = 0;
		it->second.egressQueueHeadStartTime = 0_ms;
		it->second.egressNextReleaseTime = 0_ms;
	}

	std::vector<svg_base_edict_t*> members;
	SVG_Crowd_GetCrowdMembers( crowdID, members );

	for ( svg_base_edict_t *member : members ) {
		if ( member->crowd.activeCoverIdx >= 0 ) {
			Nav_ReleaseCoverPoint( member->crowd.activeCoverIdx, member->s.number );
			member->crowd.activeCoverIdx = -1;
		}
		member->crowd.ingressQueueRank = -1;
		member->crowd.ingressReleased = true;
		member->crowd.egressQueueRank = -1;
		member->crowd.egressReleased = true;
		member->crowd.reachedGoal = true;
	}
}

/**
*	@brief	Retrieve a pointer to an active crowd group record.
*	@param	crowdID	Crowd identifier.
*	@return	Pointer to crowd group record, or nullptr if none exists.
**/
svg_crowd_group_t *SVG_Crowd_GetGroup( const int32_t crowdID ) {
	auto it = g_crowd_groups.find( crowdID );
	return ( it != g_crowd_groups.end() ) ? &it->second : nullptr;
}

/**
*	Per-Frame Update & Debug Drawing:
**/

/**
*	@brief	Resolve single-member slot deadlocks when a stationary lagging member is blocked by an arrived teammate.
*	@details	Detects the most stalled non-arrived member and attempts one deterministic slot swap with an
*				arrived member occupying the stalled member's goal neighborhood, only when the swap
*				strictly reduces total travel distance and preserves ingress ordering constraints.
*	@param	group		Active crowd coordination group.
*	@param	members		List of active squad member entities.
*	@param	arrivalRadius	Current slot arrival radius for this group.
*	@return	True if a deadlock-resolving slot swap was applied; false otherwise.
**/
static bool SVG_Crowd_ResolveStalledMemberOcclusion( svg_crowd_group_t &group, const std::vector<svg_base_edict_t*> &members, const double arrivalRadius ) {
	/**
	*	Sanity checks: require multiple members and valid slots.
	**/
	if ( members.size() <= 1 || group.slots.empty() ) {
		return false;
	}

	//! Minimum distance factor from slot before classifying as meaningfully stalled.
	static constexpr double CROWD_STALLED_MIN_DIST_FACTOR = 1.10;
	//! Radius factor around stalled goal used to detect an arrived blocker occupying that region.
	static constexpr double CROWD_STALLED_BLOCKER_GOAL_RADIUS_FACTOR = 1.35;

	const double stalledDistThreshold = arrivalRadius * CROWD_STALLED_MIN_DIST_FACTOR;
	const double blockerGoalRadius = std::max( arrivalRadius * CROWD_STALLED_BLOCKER_GOAL_RADIUS_FACTOR, CROWD_DEFAULT_AGENT_RADIUS * 2.0 );
	const double blockerGoalRadiusSq = blockerGoalRadius * blockerGoalRadius;
	const double swapHysteresisSq = CROWD_SWAP_HYSTERESIS_ARRIVED * CROWD_SWAP_HYSTERESIS_ARRIVED;

	const bool hasIngressDir = ( QM_Vector3LengthSqrDP( group.ingressDirection ) > 0.001 );
	const Vector3DP ingressDir = hasIngressDir ? QM_Vector3NormalizeDP( Vector3DP{ group.ingressDirection.x, group.ingressDirection.y, 0.0 } ) : Vector3DP{ 0.0, 0.0, 0.0 };

	svg_base_edict_t *stalledMember = nullptr;
	int32_t stalledSlotIdx = -1;
	double stalledDistSq = 0.0;

	/**
	*	Find the most stalled non-arrived member: far from slot, stationary, and stalled long enough.
	**/
	for ( svg_base_edict_t *member : members ) {
		if ( !member || !SVG_Entity_IsActive( member ) || member->health <= 0 ) {
			continue;
		}
		if ( member->crowd.reachedGoal ) {
			continue;
		}
		if ( ( group.leaderEntityNumber != ENTITYNUM_NONE && member->s.number == group.leaderEntityNumber ) || member->crowd.role == crowd_member_role_t::ROLE_LEADER ) {
			continue;
		}

		const int32_t slotIdx = member->crowd.slotIndex;
		if ( slotIdx < 0 || slotIdx >= static_cast<int32_t>( group.slots.size() ) ) {
			continue;
		}

		const Vector3DP memberPos( member->currentOrigin );
		const Vector3DP goalPos = group.slots[ slotIdx ].worldPosition;
		const double distToGoal = QM_Vector3DistanceDP( memberPos, goalPos );
		if ( distToGoal <= stalledDistThreshold ) {
			continue;
		}

		const Vector3 vel = member->velocity;
		const double speedSq2D = static_cast<double>( vel.x * vel.x + vel.y * vel.y );
		svg_monster_base_t *memberMonster = dynamic_cast<svg_monster_base_t*>( member );
		const bool hasPersistentBlockedFrames = ( memberMonster != nullptr && memberMonster->consecutiveBlockedFrames >= ( MONSTER_NAV_STUCK_RECOVER_BLOCKED_FRAMES / 2 ) );
		if ( speedSq2D > CROWD_BLOCKED_STATIONARY_SPEED_SQ && !hasPersistentBlockedFrames ) {
			continue;
		}

		if ( !hasPersistentBlockedFrames ) {
			if ( member->crowd.blockedStartTime.Milliseconds() <= 0 ) {
				continue;
			}
			const QMTime stalledDuration = level.time - member->crowd.blockedStartTime;
			if ( stalledDuration < CROWD_BLOCKED_ARRIVAL_STALL_TIME ) {
				continue;
			}
		}

		const double distSq = distToGoal * distToGoal;
		if ( stalledMember == nullptr || distSq > stalledDistSq ) {
			stalledMember = member;
			stalledSlotIdx = slotIdx;
			stalledDistSq = distSq;
		}
	}

	if ( stalledMember == nullptr || stalledSlotIdx < 0 || stalledSlotIdx >= static_cast<int32_t>( group.slots.size() ) ) {
		return false;
	}

	const Vector3DP stalledPos( stalledMember->currentOrigin );
	const Vector3DP stalledGoal = group.slots[ stalledSlotIdx ].worldPosition;
	const double stalledRawRadius = static_cast<double>( ( stalledMember->maxs.x - stalledMember->mins.x ) * 0.5f );
	const double stalledRadius = ( stalledRawRadius > 0.0 ) ? stalledRawRadius : CROWD_DEFAULT_AGENT_RADIUS;

	svg_base_edict_t *bestSwapMember = nullptr;
	int32_t bestSwapSlotIdx = -1;
	double bestSwapCost = std::numeric_limits<double>::max();

	/**
	*	Find an arrived blocker near the stalled goal where swapping slots strictly improves total travel.
	**/
	for ( svg_base_edict_t *candidate : members ) {
		if ( !candidate || candidate == stalledMember || !SVG_Entity_IsActive( candidate ) || candidate->health <= 0 ) {
			continue;
		}
		if ( !candidate->crowd.reachedGoal ) {
			continue;
		}
		if ( ( group.leaderEntityNumber != ENTITYNUM_NONE && candidate->s.number == group.leaderEntityNumber ) || candidate->crowd.role == crowd_member_role_t::ROLE_LEADER ) {
			continue;
		}

		const int32_t candidateSlotIdx = candidate->crowd.slotIndex;
		if ( candidateSlotIdx < 0 || candidateSlotIdx >= static_cast<int32_t>( group.slots.size() ) || candidateSlotIdx == stalledSlotIdx ) {
			continue;
		}

		const Vector3DP candidatePos( candidate->currentOrigin );
		const Vector3DP candidateGoal = group.slots[ candidateSlotIdx ].worldPosition;

		// Focus on arrived members occupying the stalled goal neighborhood.
		if ( QM_Vector3DistanceSqrDP( candidatePos, stalledGoal ) > blockerGoalRadiusSq ) {
			continue;
		}

		const double candidateRawRadius = static_cast<double>( ( candidate->maxs.x - candidate->mins.x ) * 0.5f );
		const double candidateRadius = ( candidateRawRadius > 0.0 ) ? candidateRawRadius : CROWD_DEFAULT_AGENT_RADIUS;
		if ( !Nav_HasGeometricLineOfSight2D( stalledPos, candidateGoal, stalledRadius * 0.5 ) ) {
			continue;
		}
		if ( !Nav_HasGeometricLineOfSight2D( candidatePos, stalledGoal, candidateRadius * 0.5 ) ) {
			continue;
		}

		if ( hasIngressDir ) {
			const double stalledProgress = QM_Vector3DotProductDP( stalledPos - group.destinationOrigin, ingressDir );
			const double candidateProgress = QM_Vector3DotProductDP( candidatePos - group.destinationOrigin, ingressDir );
			const double stalledDepth = QM_Vector3DotProductDP( stalledGoal - group.destinationOrigin, ingressDir );
			const double candidateDepth = QM_Vector3DotProductDP( candidateGoal - group.destinationOrigin, ingressDir );

			// If stalled member is clearly behind, do not assign it a clearly deeper slot than the blocker.
			if ( stalledProgress < ( candidateProgress - CROWD_INGRESS_ORDER_TOLERANCE ) &&
				 candidateDepth > ( stalledDepth + CROWD_INGRESS_ORDER_TOLERANCE ) ) {
				continue;
			}
		}

		const double currCost = QM_Vector3DistanceSqrDP( stalledPos, stalledGoal ) + QM_Vector3DistanceSqrDP( candidatePos, candidateGoal );
		const double swapCost = QM_Vector3DistanceSqrDP( stalledPos, candidateGoal ) + QM_Vector3DistanceSqrDP( candidatePos, stalledGoal );
		if ( swapCost + swapHysteresisSq >= currCost ) {
			continue;
		}

		if ( swapCost < bestSwapCost ) {
			bestSwapCost = swapCost;
			bestSwapMember = candidate;
			bestSwapSlotIdx = candidateSlotIdx;
		}
	}

	if ( bestSwapMember == nullptr || bestSwapSlotIdx < 0 || bestSwapSlotIdx >= static_cast<int32_t>( group.slots.size() ) ) {
		return false;
	}

	/**
	*	Commit deadlock-resolving swap and force immediate path refresh to new goals.
	**/
	const int32_t stalledOldSlotIdx = stalledMember->crowd.slotIndex;
	const int32_t blockerOldSlotIdx = bestSwapMember->crowd.slotIndex;
	const Vector3DP stalledOldGoal = group.slots[ stalledOldSlotIdx ].worldPosition;
	const Vector3DP blockerOldGoal = group.slots[ blockerOldSlotIdx ].worldPosition;

	std::swap( stalledMember->crowd.slotIndex, bestSwapMember->crowd.slotIndex );
	std::swap( stalledMember->crowd.role, bestSwapMember->crowd.role );
	if ( stalledMember->crowd.activeCoverIdx >= 0 || bestSwapMember->crowd.activeCoverIdx >= 0 ) {
		std::swap( stalledMember->crowd.activeCoverIdx, bestSwapMember->crowd.activeCoverIdx );
	}

	stalledMember->crowd.assignedGoalOrigin = QM_Vector3FromDP( blockerOldGoal );
	bestSwapMember->crowd.assignedGoalOrigin = QM_Vector3FromDP( stalledOldGoal );
	stalledMember->crowd.lastPathCalcTime = 0_ms;
	bestSwapMember->crowd.lastPathCalcTime = 0_ms;
	stalledMember->crowd.blockedStartTime = 0_ms;
	bestSwapMember->crowd.blockedStartTime = 0_ms;

	const Vector3DP blockerPos( bestSwapMember->currentOrigin );
	stalledMember->crowd.reachedGoal = ( QM_Vector3DistanceDP( stalledPos, blockerOldGoal ) <= arrivalRadius );
	bestSwapMember->crowd.reachedGoal = ( QM_Vector3DistanceDP( blockerPos, stalledOldGoal ) <= arrivalRadius );

	svg_monster_base_t *stalledMonster = dynamic_cast<svg_monster_base_t*>( stalledMember );
	if ( stalledMonster != nullptr ) {
		stalledMonster->ResetNavigationPath();
		stalledMonster->lastPathCalcTime = 0_ms;
		stalledMonster->consecutiveBlockedFrames = 0;
	}
	svg_monster_base_t *blockerMonster = dynamic_cast<svg_monster_base_t*>( bestSwapMember );
	if ( blockerMonster != nullptr ) {
		blockerMonster->ResetNavigationPath();
		blockerMonster->lastPathCalcTime = 0_ms;
		blockerMonster->consecutiveBlockedFrames = 0;
	}

	return true;
}

/**
*	@brief		Compact formation slot assignments upon crowd member death to eliminate vacant interior holes.
*	@details	Detects deceased squad members and reassigns living members occupying peripheral/outermost
*				slots into vacant core/interior slots, collapsing the formation compactly with zero gaps.
*	@param	group			Active crowd coordination group.
*	@param	livingMembers	List of active living squad member entities.
*	@return	True if one or more formation slots were compacted; false otherwise.
*	@note		Strictly O(M) where M is squad size; executes only when member count decreases below slot count.
**/
static bool SVG_Crowd_CompactFormationSlotsOnMemberDeath( svg_crowd_group_t &group, const std::vector<svg_base_edict_t*> &livingMembers ) {
	/**
	*	Sanity checks: requires an active moving group with multiple slots and living members.
	**/
	if ( !group.isMoving || group.slots.empty() || livingMembers.empty() ) {
		return false;
	}

	// If living member count matches or exceeds slot count, all slots are claimed; no compaction required.
	if ( livingMembers.size() >= group.slots.size() ) {
		return false;
	}

	/**
	*	Catalog all slot indices currently claimed by living members.
	**/
	std::vector<bool> slotClaimed( group.slots.size(), false );
	for ( const svg_base_edict_t *member : livingMembers ) {
		if ( member != nullptr && member->crowd.slotIndex >= 0 && member->crowd.slotIndex < static_cast<int32_t>( group.slots.size() ) ) {
			slotClaimed[ member->crowd.slotIndex ] = true;
		}
	}

	bool anyCompacted = false;

	/**
	*	Iteratively shift living members from highest/outermost slot indices into lowest vacant core slots.
	**/
	for ( size_t coreSlotIdx = 0; coreSlotIdx < livingMembers.size(); coreSlotIdx++ ) {
		// If this core slot is already claimed, proceed to next slot.
		if ( slotClaimed[ coreSlotIdx ] ) {
			continue;
		}

		// Find the living member occupying the highest claimed slot index above coreSlotIdx.
		int32_t highestClaimedIdx = -1;
		svg_base_edict_t *outermostMember = nullptr;

		for ( svg_base_edict_t *candidate : livingMembers ) {
			if ( candidate != nullptr && candidate->crowd.slotIndex > static_cast<int32_t>( coreSlotIdx ) ) {
				if ( candidate->crowd.slotIndex > highestClaimedIdx ) {
					highestClaimedIdx = candidate->crowd.slotIndex;
					outermostMember = candidate;
				}
			}
		}

		// If no living member occupies a higher slot, compaction is complete.
		if ( outermostMember == nullptr || highestClaimedIdx < 0 ) {
			break;
		}

		/**
		*	Shift the outermost living member into the vacant core slot.
		**/
		const int32_t oldSlotIdx = outermostMember->crowd.slotIndex;
		outermostMember->crowd.slotIndex = static_cast<int32_t>( coreSlotIdx );
		outermostMember->crowd.role = group.slots[ coreSlotIdx ].role;

		// If member is moving towards final formation slots (or ingress is released), update goal origin.
		if ( !group.hasSerializedIngress || outermostMember->crowd.ingressReleased ) {
			outermostMember->crowd.assignedGoalOrigin = QM_Vector3FromDP( group.slots[ coreSlotIdx ].worldPosition );
			outermostMember->crowd.reachedGoal = false;
			SVG_Crowd_ResetMemberNavigation( outermostMember );
		}

		// Update claimed state for the shifted slots.
		slotClaimed[ coreSlotIdx ] = true;
		slotClaimed[ oldSlotIdx ] = false;
		anyCompacted = true;

		gi.dprintf( "[crowd compaction] crowd=%" PRId32 " ent=%" PRId32 " shifted slot %" PRId32 " -> %" PRId32 "\n",
			group.crowdID, outermostMember->s.number, oldSlotIdx, static_cast<int32_t>( coreSlotIdx ) );
	}

	/**
	*	Truncate orphan peripheral slots beyond the living member count to preserve compactness.
	**/
	if ( anyCompacted && group.slots.size() > livingMembers.size() ) {
		group.slots.resize( livingMembers.size() );
	}

	return anyCompacted;
}

/**
*	@brief	Dynamically optimize slot assignments among crowd members to eliminate crossing trajectories.
*	@details	Executes continuous 2-Opt pairwise distance optimization based on the triangle inequality theorem:
*				whenever two trajectory segments intersect, swapping their goals strictly reduces the sum of
*				distances and strictly eliminates the intersection.
*	@param	group	Active crowd coordination group.
*	@param	members	List of active squad member entities.
**/
void SVG_Crowd_OptimizeSlotAssignments( svg_crowd_group_t &group, const std::vector<svg_base_edict_t*> &members ) {
	/**
	*	Sanity checks: ensure group has at least two members and valid formation slots.
	**/
	const size_t count = members.size();
	if ( count <= 1 || group.slots.empty() ) {
		return;
	}

	// For discrete destination move orders (static origin) or circular formations, slot assignments are established monotonically
	// by SVG_Crowd_AssignMembersToSlotsIngress. Runtime 2-Opt and head-on distance swapping causes cross-floor
	// slot thrashing and inversions in doorways and chokepoints and must be bypassed:
	const bool isCircleOrDefensive = ( group.style == crowd_chase_target_type_t::CROWD_STYLE_CIRCLE_FILLED ||
	                                   group.style == crowd_chase_target_type_t::CROWD_STYLE_SURROUND_PERIMETER ||
	                                   group.style == crowd_chase_target_type_t::CROWD_STYLE_TACTICAL_COVER );
	if ( group.targetEntityNumber == ENTITYNUM_NONE || group.style == crowd_chase_target_type_t::CROWD_STYLE_COLUMN_MARCH || isCircleOrDefensive ) {
		return;
	}

	const double arrivalRadius = ( group.params.arrivalRadius > 0.0 ) ? group.params.arrivalRadius : CROWD_DEFAULT_ARRIVAL_RADIUS;

	bool swapped = true;
	int32_t iter = 0;

	while ( swapped && iter < CROWD_MAX_OPTIMIZE_ITERS ) {
		swapped = false;
		iter++;

		for ( size_t i = 0; i < count; i++ ) {
			svg_base_edict_t *memberA = members[ i ];
			if ( !memberA || !SVG_Entity_IsActive( memberA ) || memberA->health <= 0 ) {
				continue;
			}
			// The protected leader is strictly immutable: its slot and role are never swapped or transferred to any other member
			if ( ( group.leaderEntityNumber != ENTITYNUM_NONE && memberA->s.number == group.leaderEntityNumber ) || memberA->crowd.role == crowd_member_role_t::ROLE_LEADER ) {
				continue;
			}

			const int32_t slotIdxA = memberA->crowd.slotIndex;
			if ( slotIdxA < 0 || slotIdxA >= static_cast<int32_t>( group.slots.size() ) ) {
				continue;
			}

			const Vector3DP posA( memberA->currentOrigin );
			const Vector3DP goalA = group.slots[ slotIdxA ].worldPosition;

			for ( size_t j = i + 1; j < count; j++ ) {
				svg_base_edict_t *memberB = members[ j ];
				if ( !memberB || !SVG_Entity_IsActive( memberB ) || memberB->health <= 0 ) {
					continue;
				}
				// The protected leader is strictly immutable: its slot and role are never swapped or transferred to any other member
				if ( ( group.leaderEntityNumber != ENTITYNUM_NONE && memberB->s.number == group.leaderEntityNumber ) || memberB->crowd.role == crowd_member_role_t::ROLE_LEADER ) {
					continue;
				}

				const int32_t slotIdxB = memberB->crowd.slotIndex;
				if ( slotIdxB < 0 || slotIdxB >= static_cast<int32_t>( group.slots.size() ) ) {
					continue;
				}

				const double rawRadiusA = static_cast<double>( ( memberA->maxs.x - memberA->mins.x ) * 0.5f );
				const double radiusA = ( rawRadiusA > 0.0 ) ? rawRadiusA : CROWD_DEFAULT_AGENT_RADIUS;

				const double rawRadiusB = static_cast<double>( ( memberB->maxs.x - memberB->mins.x ) * 0.5f );
				const double radiusB = ( rawRadiusB > 0.0 ) ? rawRadiusB : CROWD_DEFAULT_AGENT_RADIUS;
				const double combinedRadius = radiusA + radiusB;

				// If either member has already reached their assigned slot, do not swap them.
				// Arrived members are settled in their positions; dynamic re-swapping causes thrashing/jitter:
				if ( memberA->crowd.reachedGoal || memberB->crowd.reachedGoal ) {
					continue;
				}

				const Vector3DP posB( memberB->currentOrigin );
				const Vector3DP goalB = group.slots[ slotIdxB ].worldPosition;

				// Check for direct head-on encounter between moving teammates in close proximity:
				// If memberA and memberB are traveling towards each other in a narrow passage,
				// swap their slots so both continue forward along their current headings without deadlock:
				const Vector3DP velA{ static_cast<double>( memberA->velocity.x ), static_cast<double>( memberA->velocity.y ), 0.0 };
				const Vector3DP velB{ static_cast<double>( memberB->velocity.x ), static_cast<double>( memberB->velocity.y ), 0.0 };
				const double speedSqA = QM_Vector3LengthSqrDP( velA );
				const double speedSqB = QM_Vector3LengthSqrDP( velB );
				if ( speedSqA > CROWD_SWAP_MIN_SPEED_SQR && speedSqB > CROWD_SWAP_MIN_SPEED_SQR ) {
					const double dotVel = ( ( velA.x * velB.x ) + ( velA.y * velB.y ) ) / std::sqrt( speedSqA * speedSqB );
					const double distBetween = QM_Vector3DistanceDP( posA, posB );
					if ( dotVel < CROWD_HEADON_COURSE_DOT_THRESHOLD && distBetween < ( combinedRadius * 1.5 ) ) {
						// Swap slot indices and roles immediately
						std::swap( memberA->crowd.slotIndex, memberB->crowd.slotIndex );
						std::swap( memberA->crowd.role, memberB->crowd.role );
						if ( memberA->crowd.activeCoverIdx >= 0 || memberB->crowd.activeCoverIdx >= 0 ) {
							std::swap( memberA->crowd.activeCoverIdx, memberB->crowd.activeCoverIdx );
						}
						memberA->crowd.assignedGoalOrigin = QM_Vector3FromDP( goalB );
						memberB->crowd.assignedGoalOrigin = QM_Vector3FromDP( goalA );
						memberA->crowd.lastPathCalcTime = 0_ms;
						memberB->crowd.lastPathCalcTime = 0_ms;
						swapped = true;
						continue;
					}
				}

				// Reject swap if candidate goal has excessive vertical delta (different building floor / ledge)
				if ( std::fabs( posA.z - goalB.z ) > CROWD_ARRIVAL_MAX_Z_DIFF || std::fabs( posB.z - goalA.z ) > CROWD_ARRIVAL_MAX_Z_DIFF ) {
					continue;
				}

				// If group has an active ingress direction, enforce ingress monotonicity:
				// After swap, memberA receives goalB (depthB) and memberB receives goalA (depthA).
				// A swap is invalid if it would give a trailing member a deeper slot than a leading member.
				if ( QM_Vector3LengthSqrDP( group.ingressDirection ) > 0.001 ) {
					const Vector3DP &fwdNorm = group.ingressDirection;
					const double progA = QM_Vector3DotProductDP( posA - group.destinationOrigin, fwdNorm );
					const double progB = QM_Vector3DotProductDP( posB - group.destinationOrigin, fwdNorm );
					const double depthA = QM_Vector3DotProductDP( goalA - group.destinationOrigin, fwdNorm );
					const double depthB = QM_Vector3DotProductDP( goalB - group.destinationOrigin, fwdNorm );

					// Reject swap if it would give the trailing member a deeper slot than the leading member:
					if ( progA > ( progB + CROWD_INGRESS_ORDER_TOLERANCE ) && depthB < ( depthA - CROWD_INGRESS_ORDER_TOLERANCE ) ) {
						continue;
					}
					if ( progB > ( progA + CROWD_INGRESS_ORDER_TOLERANCE ) && depthA < ( depthB - CROWD_INGRESS_ORDER_TOLERANCE ) ) {
						continue;
					}
				}

				const double currDistSq = QM_Vector3DistanceSqrDP( posA, goalA ) + QM_Vector3DistanceSqrDP( posB, goalB );
				const double swapDistSq = QM_Vector3DistanceSqrDP( posA, goalB ) + QM_Vector3DistanceSqrDP( posB, goalA );

				// Reject swap if candidate goals are occluded by solid brush walls during transit, or if
				// swapping would force an actively moving member to turn around 180 degrees against its movement vector:
				const double distToDestA = QM_Vector3DistanceDP( posA, group.destinationOrigin );
				const double distToDestB = QM_Vector3DistanceDP( posB, group.destinationOrigin );
				const double destSectorRadius = arrivalRadius * 4.0;
				const bool isNearDestA = ( distToDestA <= destSectorRadius );
				const bool isNearDestB = ( distToDestB <= destSectorRadius );

				if ( !isNearDestA && !Nav_HasGeometricLineOfSight2D( posA, goalB, radiusA ) ) {
					continue;
				}
				if ( !isNearDestB && !Nav_HasGeometricLineOfSight2D( posB, goalA, radiusB ) ) {
					continue;
				}

				// Reject swap if it would force an actively moving member to reverse travel direction:
				if ( QM_Vector3LengthSqrDP( velA ) > CROWD_SWAP_MIN_SPEED_SQR ) {
					const Vector3DP toNewGoalA = goalB - posA;
					if ( ( ( toNewGoalA.x * velA.x ) + ( toNewGoalA.y * velA.y ) ) <= 0.0 ) {
						continue;
					}
				}
				if ( QM_Vector3LengthSqrDP( velB ) > CROWD_SWAP_MIN_SPEED_SQR ) {
					const Vector3DP toNewGoalB = goalA - posB;
					if ( ( ( toNewGoalB.x * velB.x ) + ( toNewGoalB.y * velB.y ) ) <= 0.0 ) {
						continue;
					}
				}

				constexpr double hysteresisSq = CROWD_SWAP_HYSTERESIS_MOVING * CROWD_SWAP_HYSTERESIS_MOVING;
				if ( swapDistSq + hysteresisSq < currDistSq ) {
					// Swap slot indices and roles
					std::swap( memberA->crowd.slotIndex, memberB->crowd.slotIndex );
					std::swap( memberA->crowd.role, memberB->crowd.role );

					// If tactical cover, swap active cover leases
					if ( memberA->crowd.activeCoverIdx >= 0 || memberB->crowd.activeCoverIdx >= 0 ) {
						std::swap( memberA->crowd.activeCoverIdx, memberB->crowd.activeCoverIdx );
					}

					memberA->crowd.assignedGoalOrigin = QM_Vector3FromDP( goalB );
					memberB->crowd.assignedGoalOrigin = QM_Vector3FromDP( goalA );

					// Recompute arrival status symmetrically
					const double arrivedThresh = arrivalRadius * CROWD_BLOCKED_ARRIVAL_RADIUS_FACTOR;
					const double distA = QM_Vector3DistanceDP( posA, goalB );
					memberA->crowd.reachedGoal = ( distA <= arrivedThresh );
					if ( memberA->crowd.reachedGoal ) {
						memberA->velocity = { 0.0f, 0.0f, 0.0f };
					}

					const double distB = QM_Vector3DistanceDP( posB, goalA );
					memberB->crowd.reachedGoal = ( distB <= arrivedThresh );
					if ( memberB->crowd.reachedGoal ) {
						memberB->velocity = { 0.0f, 0.0f, 0.0f };
					}

					// Trigger immediate path re-steer to new slot
					memberA->crowd.lastPathCalcTime = 0_ms;
					memberB->crowd.lastPathCalcTime = 0_ms;

					swapped = true;
				}
			}
		}
	}
}

/**
*	@brief	Execute per-frame crowd coordination, slot updates, and staggered pathing.
**/
void SVG_Crowd_Frame( void ) {
	/**
	*	Update decentralized tactical squads, bottleneck adaptation, and compaction.
	**/
	SVG_Squad_Update();

	// Process each active moving crowd group.
	for ( auto &pair : g_crowd_groups ) {
		svg_crowd_group_t &group = pair.second;
		if ( !group.isMoving ) {
			continue;
		}

		std::vector<svg_base_edict_t*> members;
		SVG_Crowd_GetCrowdMembers( group.crowdID, members );
		if ( members.empty() ) {
			group.isMoving = false;
			continue;
		}

		const double configuredArrivalRadius = ( group.params.arrivalRadius > 0.0 ) ? group.params.arrivalRadius : CROWD_DEFAULT_ARRIVAL_RADIUS;
		const double lateralSpacing = ( group.params.lateralSpacing > 0.0 ) ? group.params.lateralSpacing : CROWD_DEFAULT_LATERAL_SPACING;
		const double longitudinalSpacing = ( group.params.longitudinalSpacing > 0.0 ) ? group.params.longitudinalSpacing : CROWD_DEFAULT_LONGITUDINAL_SPACING;
		const double minCorridorSpacing = ( group.params.minCorridorSpacing > 0.0 ) ? group.params.minCorridorSpacing : CROWD_DEFAULT_MIN_CORRIDOR_SPACING;
		const double slotSpacing = std::max( minCorridorSpacing, std::min( lateralSpacing, longitudinalSpacing ) );
		const double baseArrivalFloor = std::min( configuredArrivalRadius, CROWD_DEFAULT_AGENT_RADIUS * 0.75 );
		const double baseEnterArrivalRadius = std::max( baseArrivalFloor, std::min( configuredArrivalRadius, std::min( slotSpacing * 0.35, CROWD_DEFAULT_AGENT_RADIUS * 1.25 ) ) );
		const double baseExitArrivalRadius = baseEnterArrivalRadius * 1.35;

		/**
		*	Advance at most one O(1) doorway reservation before evaluating arrival state.
		**/
		const bool serializedIngressPending = SVG_Crowd_UpdateSerializedIngress( group );
		const bool serializedEgressPending = SVG_Crowd_UpdateSerializedEgress( group );

		// Compact formation slots if any squad member died during transit to eliminate vacant interior holes.
		SVG_Crowd_CompactFormationSlotsOnMemberDeath( group, members );

		// Dynamically untangle crossing trajectories and optimize slot assignments among members
		SVG_Crowd_OptimizeSlotAssignments( group, members );

		// Check member arrival states symmetrically and track stationary stall windows.
		bool allArrived = true;
		for ( svg_base_edict_t *member : members ) {
			const Vector3DP memberPos = SVG_GetEntityFeetOriginDP( member );
			const Vector3DP slotPos( member->crowd.assignedGoalOrigin );
			const Vector3DP toSlot = slotPos - memberPos;
			const double distToSlot2D = std::sqrt( ( toSlot.x * toSlot.x ) + ( toSlot.y * toSlot.y ) );
			const double zDistToSlot = std::fabs( toSlot.z );
			const double rawMemberRadius = static_cast<double>( member->maxs.x - member->mins.x ) * 0.5;
			const double memberRadius = ( rawMemberRadius > 0.0 ) ? rawMemberRadius : CROWD_DEFAULT_AGENT_RADIUS;
			const double memberArrivalFloor = std::min( configuredArrivalRadius, memberRadius * 0.75 );
			const double enterArrivalRadius = group.hasSerializedIngress ? CROWD_INGRESS_ARRIVAL_RADIUS :
				std::max( memberArrivalFloor, std::min( configuredArrivalRadius, std::min( slotSpacing * 0.35, memberRadius * 1.25 ) ) );
			const double exitArrivalRadius = group.hasSerializedIngress ? enterArrivalRadius * 1.35 :
				std::max( enterArrivalRadius, baseExitArrivalRadius );

			/**
			*	Maintain precise slot occupancy state with hysteresis so members do not
			*	park far off-slot and permanently block the final ingress member.
			**/
			if ( member->crowd.reachedGoal ) {
				if ( zDistToSlot > CROWD_ARRIVAL_MAX_Z_DIFF || distToSlot2D > exitArrivalRadius ) {
					member->crowd.reachedGoal = false;
					member->crowd.blockedStartTime = 0_ms;
				}
			} else if ( zDistToSlot <= CROWD_ARRIVAL_MAX_Z_DIFF && distToSlot2D <= enterArrivalRadius ) {
				member->crowd.reachedGoal = true;
				member->crowd.blockedStartTime = 0_ms;
			} else {
				const Vector3 vel = member->velocity;
				const double speedSq2D = static_cast<double>( vel.x * vel.x + vel.y * vel.y );
				if ( speedSq2D <= CROWD_BLOCKED_STATIONARY_SPEED_SQ ) {
					if ( member->crowd.blockedStartTime.Milliseconds() <= 0 ) {
						member->crowd.blockedStartTime = level.time;
					}
				} else {
					member->crowd.blockedStartTime = 0_ms;
				}
			}

			if ( !member->crowd.reachedGoal ) {
				allArrived = false;
			}
		}

		// Exterior hold-point occupancy or pending room egress is not final formation completion.
		if ( serializedIngressPending || serializedEgressPending ) {
			allArrived = false;
		}

		// If following a target entity by entity number, check if formation needs rebuilding.
		if ( group.targetEntityNumber != ENTITYNUM_NONE ) {
			svg_base_edict_t *targetEnt = group.GetTargetEntity();
			if ( !targetEnt ) {
				// Target died or departed the game; halt the crowd.
				SVG_Crowd_StopCrowd( group.crowdID );
				continue;
			}

			// Debounce follow updates: only update formation when target moves significantly AND interval elapsed.
			const double moveDist = QM_Vector3DistanceDP( Vector3DP( targetEnt->currentOrigin ), group.lastTargetEntityOrigin );
			const bool intervalElapsed = ( level.time - group.lastTargetEntityUpdateTime >= CROWD_FOLLOW_REBUILD_MIN_INTERVAL );
			if ( moveDist > CROWD_FOLLOW_REBUILD_MIN_DIST && intervalElapsed ) {
				group.lastTargetEntityOrigin = Vector3DP( targetEnt->currentOrigin );
				group.lastTargetEntityUpdateTime = level.time;
				MoveAStarFollowEntity( group.crowdID, group.targetEntityNumber, group.style, group.params );
			}
		} else {
			// Static destination move order:
			// Resolve deadlock-only slot occlusion when one stalled member cannot reach its assigned slot.
			// Circle/defensive formations are excluded: their assignments are owned by the portal-aware
			// Hungarian matching, and a periodic goal-flipper fighting it re-wedges members at the doorway.
			const bool isCircleOrDefensive = ( group.style == crowd_chase_target_type_t::CROWD_STYLE_CIRCLE_FILLED ||
											   group.style == crowd_chase_target_type_t::CROWD_STYLE_SURROUND_PERIMETER ||
											   group.style == crowd_chase_target_type_t::CROWD_STYLE_TACTICAL_COVER );
			if ( !allArrived && !isCircleOrDefensive ) {
				//! Debounce interval for stalled static-order deadlock resolution passes.
				static constexpr QMTime CROWD_STATIC_STALLED_RESOLVE_INTERVAL = 200_ms;
				if ( level.time >= group.nextUpdateTick ) {
					if ( SVG_Crowd_ResolveStalledMemberOcclusion( group, members, baseEnterArrivalRadius ) ) {
						group.nextUpdateTick = level.time + CROWD_STATIC_STALLED_RESOLVE_INTERVAL;
					} else {
						group.nextUpdateTick = level.time + CROWD_FOLLOW_REBUILD_MIN_INTERVAL;
					}
				}
			}

			// If all members have arrived at their destination slots, mark group as no longer moving.
			if ( allArrived ) {
				group.isMoving = false;
				continue;
			}
		}
	}

	// Render debug visualization if enabled.
	if ( s_crowd_debug_draw && s_crowd_debug_draw->integer > 0 ) {
		SVG_Crowd_DebugDraw();
	}
}

/**
*	@brief	Render debug visualization for all active crowd groups and formation slots.
**/
void SVG_Crowd_DebugDraw( void ) {
	for ( const auto &pair : g_crowd_groups ) {
		const svg_crowd_group_t &group = pair.second;
		if ( !group.isMoving && group.slots.empty() ) {
			continue;
		}

		const Vector3 groupDest = QM_Vector3FromDP( group.destinationOrigin );

		// Draw heading arrow at destination.
		const Vector3 headingAngles{ 0.0f, static_cast<float>( group.currentHeadingYaw ), 0.0f };
		Vector3 fwd, rgt, up;
		QM_AngleVectors( headingAngles, &fwd, &rgt, &up );
		const Vector3 arrowEnd = groupDest + ( fwd * 48.0f );
		SVG_Nav_DebugDraw_AddArrow( groupDest, arrowEnd, 12.0f, MakeColor( 255, 255, 0, 255 ) );

		// Draw distinct golden crown/beacon above the squad leader if designated.
		const svg_base_edict_t *leader = group.GetLeaderEntity();
		if ( leader ) {
			const Vector3 leaderHeadPos = leader->currentOrigin + Vector3{ 0.0f, 0.0f, leader->maxs.z + 16.0f };
			SVG_Nav_DebugDraw_AddSphere( leaderHeadPos, 8.0f, MakeColor( 255, 215, 0, 255 ), 12 );
			SVG_Nav_DebugDraw_AddLine( leader->currentOrigin, leaderHeadPos, MakeColor( 255, 215, 0, 200 ) );
		}

		// Draw each formation slot.
		for ( const svg_crowd_slot_t &slot : group.slots ) {
			const Vector3 slotWorld = QM_Vector3FromDP( slot.worldPosition );
			const uint32_t slotColor = slot.isNavmeshValid ? MakeColor( 0, 255, 76, 230 ) : MakeColor( 255, 51, 51, 230 );
			SVG_Nav_DebugDraw_AddSphere( slotWorld, 12.0f, slotColor, SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE, 2.0f );

			// If slot is a tactical cover point, draw an AABB around it.
			if ( slot.coverIndex >= 0 ) {
				const Vector3 mins = slotWorld - Vector3{ 16.0f, 16.0f, 0.0f };
				const Vector3 maxs = slotWorld + Vector3{ 16.0f, 16.0f, 64.0f };
				SVG_Nav_DebugDraw_AddAabb( mins, maxs, MakeColor( 255, 153, 0, 204 ) );
			}
		}

		// For circular / perimeter formations, render the protective spokes, perimeter circular ring, and outward defense vectors:
		const bool isCircular = ( group.style == crowd_chase_target_type_t::CROWD_STYLE_CIRCLE_FILLED ||
		                          group.style == crowd_chase_target_type_t::CROWD_STYLE_SURROUND_PERIMETER );

		if ( isCircular && group.slots.size() > 1 ) {
			const Vector3 centerPos = QM_Vector3FromDP( group.slots[ 0 ].worldPosition );
			// Distinct golden sphere highlighting the protected center VIP:
			SVG_Nav_DebugDraw_AddSphere( centerPos, 14.0f, MakeColor( 255, 215, 0, 255 ), SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE, 3.0f );

			for ( size_t i = 1; i < group.slots.size(); i++ ) {
				const Vector3 curPos = QM_Vector3FromDP( group.slots[ i ].worldPosition );
				// Radial protective spoke from center VIP to perimeter defender:
				SVG_Nav_DebugDraw_AddLine( centerPos, curPos, MakeColor( 255, 215, 0, 160 ), SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE, 1.5f );

				// Perimeter ring segment connecting adjacent defenders:
				const size_t nextIdx = ( i < group.slots.size() - 1 ) ? ( i + 1 ) : 1;
				const Vector3 nextPos = QM_Vector3FromDP( group.slots[ nextIdx ].worldPosition );
				SVG_Nav_DebugDraw_AddLine( curPos, nextPos, MakeColor( 0, 255, 200, 200 ), SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE, 2.0f );

				// 360-degree outward defensive cover facing arrow:
				const Vector3 facingAngles{ 0.0f, static_cast<float>( group.slots[ i ].relativeYawDeg ), 0.0f };
				Vector3 facingFwd, facingRgt, facingUp;
				QM_AngleVectors( facingAngles, &facingFwd, &facingRgt, &facingUp );
				SVG_Nav_DebugDraw_AddArrow( curPos, curPos + ( facingFwd * 28.0f ), 8.0f, MakeColor( 0, 255, 76, 255 ), SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE, 2.0f );
			}
		}

		// Draw link lines from members to their assigned slots.
		std::vector<svg_base_edict_t*> members;
		SVG_Crowd_GetCrowdMembers( group.crowdID, members );

		/**
		*	Render the complete two-phase route graph so staging and final room-fill
		*	destinations remain independently inspectable during live debugging.
		**/
		if ( group.hasSerializedIngress ) {
			const Vector3 portalWorld = QM_Vector3FromDP( group.ingressPortalOrigin );
			SVG_Nav_DebugDraw_AddSphere( portalWorld, 10.0f, MakeColor( 255, 128, 0, 255 ),
				SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE, 3.0f );
			for ( size_t stageIndex = 0; stageIndex < group.ingressStagingLine.size(); stageIndex++ ) {
				// Magenta cells are mandatory exterior pre-formation positions.
				const Vector3DP &stagingPosition = group.ingressStagingLine[ stageIndex ];
				const Vector3 stagingWorld = QM_Vector3FromDP( stagingPosition );
				SVG_Nav_DebugDraw_AddSphere( stagingWorld, 9.0f, MakeColor( 255, 0, 220, 255 ),
					SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE, 2.5f );
				SVG_Nav_DebugDraw_AddLine( stagingWorld, portalWorld, MakeColor( 255, 160, 0, 150 ),
					SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE, 1.5f );
				if ( stageIndex > 0 ) {
					// Draw the ordered pre-formation spine explicitly so queue scatter is visible immediately.
					const Vector3 previousStageWorld = QM_Vector3FromDP( group.ingressStagingLine[ stageIndex - 1 ] );
					SVG_Nav_DebugDraw_AddLine( previousStageWorld, stagingWorld, MakeColor( 255, 0, 220, 220 ),
						SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE, 2.0f );
				}
			}
		}

		for ( const svg_base_edict_t *member : members ) {
			if ( member->crowd.slotIndex >= 0 && member->crowd.slotIndex < static_cast<int32_t>( group.slots.size() ) ) {
				const Vector3 assignedGoal = member->crowd.assignedGoalOrigin;
				const Vector3 slotPos = QM_Vector3FromDP( group.slots[ member->crowd.slotIndex ].worldPosition );
				const uint32_t activeGoalColor = ( !member->crowd.egressReleased )
					? MakeColor( 255, 140, 0, 220 )
					: ( member->crowd.ingressReleased
						? MakeColor( 51, 204, 255, 220 )
						: MakeColor( 255, 0, 220, 220 ) );
				// Current-to-assigned shows the path target actually consumed by this member.
				SVG_Nav_DebugDraw_AddLine( member->currentOrigin, assignedGoal, activeGoalColor,
					SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE, 2.0f );

				// Portal-to-final shows the second leg and exact circular-fill destination.
				if ( group.hasSerializedIngress && member->crowd.ingressQueueRank >= 0 ) {
					const Vector3 portalWorld = QM_Vector3FromDP( group.ingressPortalOrigin );
					SVG_Nav_DebugDraw_AddLine( portalWorld, slotPos, MakeColor( 0, 220, 255, 150 ),
						SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE, 1.5f );
				}
			}
		}

		// Draw egress exit portal aperture in orange.
		if ( group.hasSerializedEgress ) {
			const Vector3 egressPortalWorld = QM_Vector3FromDP( group.egressPortalOrigin );
			SVG_Nav_DebugDraw_AddSphere( egressPortalWorld, 12.0f, MakeColor( 255, 140, 0, 255 ),
				SG_SVC_DEBUG_DRAW_STYLE_FLAG_NONE, 2.5f );
		}
	}
}
