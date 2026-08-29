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
#include "svgame/entities/svg_base_edict.h"
#include "svgame/entities/monster/svg_monster_testdummy_debug.h"
#include "svgame/svg_edict_pool.h"
#include "svgame/nav/nav_debug_draw.h"
#include "svgame/nav/nav_cover_query.h"
#include "svgame/nav/nav_path.h"
#include "svgame/nav/nav_types.h"

#include "shared/math/qm_math_cpp.h"
#include "shared/math/qm_vector3_dp.h"
#include "shared/util/util_endian.h"
#include "svgame/nav/nav_generate.h"
#include "svgame/nav/nav_cover_types.h"

extern std::vector<nav_cover_point_t> g_nav_cover_points;

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
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
			group.leaderEntityNumber = ent->crowd.leaderEntityNumber;
			g_crowd_groups[ cid ] = group;
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

	std::vector<svg_base_edict_t*> members;
	SVG_Crowd_GetCrowdMembers( crowdID, members );
	for ( svg_base_edict_t *member : members ) {
		member->crowd.leaderEntityNumber = leaderEntityNumber;
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

	const double sepRadius = ( group->params.separationRadius > 0.0 ) ? group->params.separationRadius : CROWD_DEFAULT_SEPARATION_RADIUS;
	const double sepStrength = ( group->params.separationStrength > 0.0 ) ? group->params.separationStrength : CROWD_DEFAULT_SEPARATION_STRENGTH;
	const double sepRadiusSq = sepRadius * sepRadius;

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

		const Vector3DP otherOrigin( other->currentOrigin );
		Vector3DP diff = myOrigin - otherOrigin;
		diff.z = 0.0;
		const double distSq = QM_Vector3LengthSqrDP( diff );

		if ( distSq < sepRadiusSq && distSq > 0.0001 ) {
			const double dist = std::sqrt( distSq );
			const double pushWeight = ( sepRadius - dist ) / sepRadius;
			const Vector3DP pushDir = diff * ( 1.0 / dist );
			separationAcc = separationAcc + ( pushDir * pushWeight );
			neighborCount++;

			// If self is moving and other teammate is directly ahead on a collision course,
			// compute lateral (perpendicular) avoidance force to smoothly steer around teammate's capsule.
			const Vector3DP selfVel( selfEnt->velocity );
			const double velSq = selfVel.x * selfVel.x + selfVel.y * selfVel.y;
			if ( velSq > 16.0 ) {
				const double invVel = 1.0 / std::sqrt( velSq );
				const Vector3DP fwdDir = { selfVel.x * invVel, selfVel.y * invVel, 0.0 };
				const Vector3DP rightDir = { fwdDir.y, -fwdDir.x, 0.0 };

				const Vector3DP toOther = otherOrigin - myOrigin;
				const double dLong = toOther.x * fwdDir.x + toOther.y * fwdDir.y;
				const double dLat = toOther.x * rightDir.x + toOther.y * rightDir.y;

				constexpr double lateralHullLimit = ( CROWD_DEFAULT_AGENT_RADIUS * 2.0 ) + 8.0;
				if ( dLong > 0.0 && dLong < sepRadius && std::fabs( dLat ) < lateralHullLimit ) {
					// Steer laterally away from the teammate's relative side (left if teammate on right, right if on left)
					const double steerSign = ( dLat >= 0.0 ) ? -1.0 : 1.0;
					const double latWeight = ( lateralHullLimit - std::fabs( dLat ) ) / lateralHullLimit;
					const double longWeight = ( sepRadius - dLong ) / sepRadius;
					const Vector3DP latPush = rightDir * ( steerSign * latWeight * longWeight * 1.5 );
					separationAcc = separationAcc + latPush;
				}
			}
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
*	@param	entityNumber	Query entity number.
*	@param	moveDir			Normalized 2D horizontal movement direction towards active waypoint.
*	@param	outSpeedScale	[out] Multiplier applied to frame velocity [0.0..1.0] to maintain following distance.
*	@return	True if a leading teammate was found directly ahead in the travel corridor.
**/
bool SVG_Crowd_ComputeTeammateFollowSpeedScale( const int32_t entityNumber, const Vector3DP &moveDir, double *outSpeedScale ) {
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

	static constexpr double corridorLateralSqr = CROWD_CORRIDOR_LATERAL_THRESHOLD * CROWD_CORRIDOR_LATERAL_THRESHOLD;
	static constexpr double maxAheadDist = CROWD_FOLLOW_MIN_SEPARATION + CROWD_FOLLOW_SLOWDOWN_RANGE;

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
		// If the teammate ahead has already reached its assigned slot and parked,
		// it is stationary station-keeping; do not throttle moving members attempting to reach their slots.
		if ( other->crowd.reachedGoal ) {
			continue;
		}

		const Vector3DP otherOrigin( other->currentOrigin );
		Vector3DP diff = otherOrigin - myOrigin;
		diff.z = 0.0;

		// Project displacement onto our forward movement direction to determine longitudinal distance ahead.
		const double aheadDist = diff.x * moveDir.x + diff.y * moveDir.y;
		if ( aheadDist <= 0.0 || aheadDist > maxAheadDist ) {
			continue;
		}

		// Vertical proximity check within maximum step height to ensure teammates are on the same floor level.
		if ( std::fabs( otherOrigin.z - myOrigin.z ) > static_cast<double>( NAV_MAX_STEP_HEIGHT ) ) {
			continue;
		}

		// Compute perpendicular lateral distance squared from our travel centerline.
		const double distSq = QM_Vector3LengthSqrDP( diff );
		const double lateralDistSq = distSq - ( aheadDist * aheadDist );
		if ( lateralDistSq > corridorLateralSqr ) {
			continue; // Teammate is off to the side outside our travel corridor lane
		}

		// Teammate is directly ahead in our corridor lane: calculate smooth deceleration scale.
		foundLeaderAhead = true;
		if ( aheadDist <= CROWD_FOLLOW_MIN_SEPARATION ) {
			minScale = std::min( minScale, CROWD_FOLLOW_CRAWL_SPEED_SCALE );
		} else {
			const double scale = std::clamp( ( aheadDist - CROWD_FOLLOW_MIN_SEPARATION ) / CROWD_FOLLOW_SLOWDOWN_RANGE, CROWD_FOLLOW_CRAWL_SPEED_SCALE, 1.0 );
			minScale = std::min( minScale, scale );
		}
	}

	*outSpeedScale = minScale;
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

	// Calculate collective squad centroid in Vector3DP.
	const Vector3DP centroid = SVG_Crowd_ComputeCentroid( members );
	group.currentHeadingYaw = SVG_Crowd_CalculateHeadingYaw( centroid, origin, nullptr, params );

	// Compute navigation guide path from squad centroid to destination to follow curved corridors, ramps, and staircases.
	std::vector<Vector3DP> guidePath;
	const int32_t startFace = Nav_FindFaceInLeafStrict( centroid );
	const int32_t goalFace = Nav_FindFaceInLeafStrict( origin );
	if ( startFace >= 0 && goalFace >= 0 && !g_nav_faces.empty() ) {
		std::vector<int32_t> navPathFaces;
		nav_path_policy_t guidePolicy;
		guidePolicy.agent_radius = static_cast<float>( CROWD_DEFAULT_AGENT_RADIUS );
		if ( Nav_FindPath( startFace, goalFace, navPathFaces, guidePolicy ) ) {
			std::vector<bool> forcedWps;
			Nav_StringPull( navPathFaces, centroid, origin, CROWD_DEFAULT_AGENT_RADIUS, guidePath, &forcedWps );
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

	// Calculate dynamic corridor clearance and squeeze factor if enabled.
	svg_crowd_params_t effectiveParams = params;
	if ( params.enableCorridorSqueeze ) {
		const double desiredWidth = ( members.size() > 1 ) ? ( static_cast<double>( members.size() - 1 ) * params.lateralSpacing ) : params.lateralSpacing;
		const double clearance = SVG_Crowd_ComputeCorridorClearance( origin, desiredWidth );
		const double squeeze = ( desiredWidth > 0.0 ) ? std::clamp( clearance / desiredWidth, 0.2, 1.0 ) : 1.0;
		group.dynamicSqueezeFactor = squeeze;
		effectiveParams.lateralSpacing = std::max( params.minCorridorSpacing, params.lateralSpacing * squeeze );
		effectiveParams.longitudinalSpacing = std::max( params.minCorridorSpacing, params.longitudinalSpacing * squeeze );
	} else {
		group.dynamicSqueezeFactor = 1.0;
	}

	// Build formation / cover slots.
	if ( style == crowd_chase_target_type_t::CROWD_STYLE_TACTICAL_COVER ) {
		SVG_Crowd_AllocateTacticalCover( group, members, origin, group.slots );
	} else {
		SVG_Crowd_GenerateFormationSlots( style, members.size(), effectiveParams, group.slots );
		SVG_Crowd_TransformLocalSlotsToWorld( origin, group.currentHeadingYaw, group.slots );
		SVG_Crowd_SnapSlotsToNavMesh( group.slots, origin );
		const double minPhysicalSeparation = ( CROWD_DEFAULT_AGENT_RADIUS * 2.0 ) + 4.0;
		const double minSlotSeparation = std::max( minPhysicalSeparation, std::min( effectiveParams.lateralSpacing, effectiveParams.longitudinalSpacing ) );
		SVG_Crowd_ResolveSlotCollisionsAndInvalidSlots( group.slots, origin, group.currentHeadingYaw, minSlotSeparation, CROWD_DEFAULT_AGENT_RADIUS, guidePath.empty() ? nullptr : &guidePath );

		// Sort slots deepest-first along squad ingress approach vector ONLY when entering
		// an enclosed corridor/room where squeeze is active and approach distance is significant (> 128 units).
		// In open space or local maneuvers, forcing ingress depth ordering creates an artificial 1D ranking
		// that causes agents' paths to criss-cross and deadlock.
		const double distToDest = QM_Vector3DistanceDP( origin, centroid );
		const bool isConstrainedIngress = ( params.enableCorridorSqueeze && group.dynamicSqueezeFactor < 0.95 && distToDest > 128.0 );

		if ( isConstrainedIngress ) {
			Vector3DP ingressDir = origin - centroid;
			if ( guidePath.size() >= 2 ) {
				ingressDir = guidePath.back() - guidePath[ guidePath.size() - 2 ];
			}
			ingressDir.z = 0.0;
			if ( QM_Vector3LengthDP( ingressDir ) > 0.001 ) {
				group.ingressDirection = QM_Vector3NormalizeDP( ingressDir );
				SVG_Crowd_SortSlotsByIngressDepth( group.slots, origin, group.ingressDirection );
			} else {
				group.ingressDirection = Vector3DP{ 0.0, 0.0, 0.0 };
			}
		} else {
			group.ingressDirection = Vector3DP{ 0.0, 0.0, 0.0 };
		}
	}

	// Extract member feet origins in Vector3DP.
	std::vector<Vector3DP> memberOrigins;
	memberOrigins.reserve( members.size() );
	for ( const svg_base_edict_t *ent : members ) {
		memberOrigins.emplace_back( ent->currentOrigin );
	}

	// For discrete destination move orders, start with a fresh geometric slate (no hysteresis)
	// so agents are matched purely by current geometry without carrying over obsolete historical slot ranks.
	std::vector<int32_t> previousSlotMap( members.size(), -1 );

	// Match members to formation slots using ingress-depth and approach progress ordering.
	std::vector<int32_t> slotMapping;
	if ( QM_Vector3LengthDP( group.ingressDirection ) > 0.001 ) {
		SVG_Crowd_AssignMembersToSlotsIngress( memberOrigins, group.slots, previousSlotMap, slotMapping, origin, group.ingressDirection );
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

		if ( leaderIdx >= 0 && slotMapping[ leaderIdx ] != 0 ) {
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
			member->crowd.role = slot.role;
			member->crowd.assignedGoalOrigin = QM_Vector3FromDP( slot.worldPosition );
			member->crowd.reachedGoal = false;
			member->crowd.startTimeForSeeking = level.time;
			member->crowd.maxTimeToSeek = params.maxTimeToSeek;
			member->crowd.leaderEntityNumber = group.leaderEntityNumber;
			// Stagger initial path calculation time across squad members.
			member->crowd.lastPathCalcTime = level.time + QMTime::FromMilliseconds( static_cast<int64_t>( i * params.pathStaggerMs ) );
		}
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
		const double minPhysicalSeparation = ( CROWD_DEFAULT_AGENT_RADIUS * 2.0 ) + 4.0;
		const double minSlotSeparation = std::max( minPhysicalSeparation, std::min( effectiveParams.lateralSpacing, effectiveParams.longitudinalSpacing ) );
		SVG_Crowd_ResolveSlotCollisionsAndInvalidSlots( group.slots, Vector3DP( targetEnt->currentOrigin ), group.currentHeadingYaw, minSlotSeparation, CROWD_DEFAULT_AGENT_RADIUS );

		const double distToDest = QM_Vector3DistanceDP( Vector3DP( targetEnt->currentOrigin ), centroid );
		const bool isConstrainedIngress = ( params.enableCorridorSqueeze && group.dynamicSqueezeFactor < 0.95 && distToDest > 128.0 );

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
		memberOrigins.emplace_back( ent->currentOrigin );
	}

	std::vector<int32_t> previousSlotMap( members.size(), -1 );
	for ( size_t i = 0; i < members.size(); i++ ) {
		previousSlotMap[ i ] = members[ i ]->crowd.slotIndex;
	}

	// Match members to formation slots using ingress-depth and approach progress ordering.
	std::vector<int32_t> slotMapping;
	if ( QM_Vector3LengthDP( group.ingressDirection ) > 0.001 ) {
		SVG_Crowd_AssignMembersToSlotsIngress( memberOrigins, group.slots, previousSlotMap, slotMapping, Vector3DP( targetEnt->currentOrigin ), group.ingressDirection );
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

		if ( leaderIdx >= 0 && slotMapping[ leaderIdx ] != 0 ) {
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

			member->crowd.slotIndex = slotIdx;
			member->crowd.role = slot.role;
			member->crowd.assignedGoalOrigin = newGoal;
			// Only reset arrival state if the assigned slot moved significantly.
			if ( goalDistDelta > CROWD_DEFAULT_ARRIVAL_RADIUS ) {
				member->crowd.reachedGoal = false;
			}
			member->crowd.leaderEntityNumber = group.leaderEntityNumber;
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
	auto it = g_crowd_groups.find( crowdID );
	if ( it != g_crowd_groups.end() ) {
		it->second.isMoving = false;
	}

	std::vector<svg_base_edict_t*> members;
	SVG_Crowd_GetCrowdMembers( crowdID, members );

	for ( svg_base_edict_t *member : members ) {
		if ( member->crowd.activeCoverIdx >= 0 ) {
			Nav_ReleaseCoverPoint( member->crowd.activeCoverIdx, member->s.number );
			member->crowd.activeCoverIdx = -1;
		}
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
			// Protect designated squad leader if point slot 0 is assigned
			if ( group.leaderEntityNumber != ENTITYNUM_NONE && memberA->s.number == group.leaderEntityNumber ) {
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
				if ( group.leaderEntityNumber != ENTITYNUM_NONE && memberB->s.number == group.leaderEntityNumber ) {
					continue;
				}

				const int32_t slotIdxB = memberB->crowd.slotIndex;
				if ( slotIdxB < 0 || slotIdxB >= static_cast<int32_t>( group.slots.size() ) ) {
					continue;
				}

				// If both members have already reached their slots, do not swap.
				if ( memberA->crowd.reachedGoal && memberB->crowd.reachedGoal ) {
					continue;
				}

				// If one member has arrived, allow swapping ONLY if the arrived member is directly blocking
				// the moving member from reaching a deeper slot, allowing the arrived member to step forward.
				if ( memberA->crowd.reachedGoal || memberB->crowd.reachedGoal ) {
					svg_base_edict_t *arrivedMember = memberA->crowd.reachedGoal ? memberA : memberB;
					svg_base_edict_t *movingMember = memberA->crowd.reachedGoal ? memberB : memberA;
					const Vector3DP arrivedPos( arrivedMember->currentOrigin );
					const Vector3DP movingPos( movingMember->currentOrigin );
					const double distBetween = QM_Vector3DistanceDP( arrivedPos, movingPos );

					// Only allow yield if moving member is pressing against the arrived member
					constexpr double maxYieldTouchDist = ( CROWD_DEFAULT_AGENT_RADIUS * 2.0 ) + 12.0;
					if ( distBetween > maxYieldTouchDist ) {
						continue;
					}

					// And arrived member must have clean line of sight to advance to moving member's goal
					const int32_t movingSlot = movingMember->crowd.slotIndex;
					if ( movingSlot < 0 || movingSlot >= static_cast<int32_t>( group.slots.size() ) ) {
						continue;
					}
					if ( !Nav_HasGeometricLineOfSight2D( arrivedPos, group.slots[ movingSlot ].worldPosition, CROWD_DEFAULT_AGENT_RADIUS ) ) {
						continue;
					}
				}

				const Vector3DP posB( memberB->currentOrigin );
				const Vector3DP goalB = group.slots[ slotIdxB ].worldPosition;

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

				// Reject swap if candidate goal is occluded by a solid brush wall from the agent's current position,
				// while the other member already has unobstructed line-of-sight inside the room/structure
				if ( !Nav_HasGeometricLineOfSight2D( posA, goalB, CROWD_DEFAULT_AGENT_RADIUS ) && Nav_HasGeometricLineOfSight2D( posB, goalB, CROWD_DEFAULT_AGENT_RADIUS ) ) {
					continue;
				}
				if ( !Nav_HasGeometricLineOfSight2D( posB, goalA, CROWD_DEFAULT_AGENT_RADIUS ) && Nav_HasGeometricLineOfSight2D( posA, goalA, CROWD_DEFAULT_AGENT_RADIUS ) ) {
					continue;
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

		const double arrivalRadius = ( group.params.arrivalRadius > 0.0 ) ? group.params.arrivalRadius : CROWD_DEFAULT_ARRIVAL_RADIUS;

		// Dynamically untangle crossing trajectories and optimize slot assignments among members
		SVG_Crowd_OptimizeSlotAssignments( group, members );

		// Check member arrival states symmetrically.
		bool allArrived = true;
		for ( svg_base_edict_t *member : members ) {
			const double distToSlot = QM_Vector3DistanceDP( Vector3DP( member->currentOrigin ), Vector3DP( member->crowd.assignedGoalOrigin ) );
			if ( distToSlot <= arrivalRadius ) {
				member->crowd.reachedGoal = true;
			}
			if ( !member->crowd.reachedGoal ) {
				allArrived = false;
			}
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
			SVG_Nav_DebugDraw_AddSphere( slotWorld, 12.0f, slotColor, 12 );

			// If slot is a tactical cover point, draw an AABB around it.
			if ( slot.coverIndex >= 0 ) {
				const Vector3 mins = slotWorld - Vector3{ 16.0f, 16.0f, 0.0f };
				const Vector3 maxs = slotWorld + Vector3{ 16.0f, 16.0f, 64.0f };
				SVG_Nav_DebugDraw_AddAabb( mins, maxs, MakeColor( 255, 153, 0, 204 ) );
			}
		}

		// Draw link lines from members to their assigned slots.
		std::vector<svg_base_edict_t*> members;
		SVG_Crowd_GetCrowdMembers( group.crowdID, members );

		for ( const svg_base_edict_t *member : members ) {
			if ( member->crowd.slotIndex >= 0 && member->crowd.slotIndex < static_cast<int32_t>( group.slots.size() ) ) {
				const Vector3 slotPos = QM_Vector3FromDP( group.slots[ member->crowd.slotIndex ].worldPosition );
				SVG_Nav_DebugDraw_AddLine( member->currentOrigin, slotPos, MakeColor( 51, 204, 255, 178 ) );
			}
		}
	}
}
