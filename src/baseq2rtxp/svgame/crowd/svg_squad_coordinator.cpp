/********************************************************************
*
*
*	ServerGame: Tactical Squad Coordinator Implementation
*	File: svg_squad_coordinator.cpp
*	Description:
*		Decentralized squad coordinator managing rosters, rank orders,
*		dynamic aperture adaptation, hole-filling compaction, and
*		slot allocation.
*
*
********************************************************************/
#include "svgame/crowd/svg_squad_coordinator.h"
#include "svgame/crowd/svg_crowd_manager.h"
#include "svgame/entities/svg_base_edict.h"
#include "svgame/svg_edict_pool.h"
#include "svgame/svg_utils.h"
#include "svgame/nav/nav_sector_graph.h"
#include "svgame/nav/nav_core.h"

#include <algorithm>

//! Global registry of all active tactical squads.
static std::unordered_map<int32_t, svg_squad_t> s_activeSquads;

/**
*
*
*
*	svg_squad_t Member Functions:
*
*
*
**/
/**
*	@brief	Resolve the pointer to the designated squad leader entity.
*	@return	Leader edict pointer, or nullptr if dead / departed.
**/
svg_base_edict_t *svg_squad_t::GetLeaderEntity( void ) const {
	// If unassigned, return nullptr.
	if ( leader_entity_number == ENTITYNUM_NONE ) {
		return nullptr;
	}

	// Lookup edict from pool.
	svg_base_edict_t *ent = g_edict_pool.EdictForNumber( leader_entity_number );
	// Validate active usage and health.
	if ( ent != nullptr && ent->inUse && ent->health > 0 ) {
		return ent;
	}

	return nullptr;
}

/**
*	@brief	Resolve the pointer to the target entity being followed.
*	@return	Target edict pointer, or nullptr if dead / departed.
**/
svg_base_edict_t *svg_squad_t::GetTargetEntity( void ) const {
	// If unassigned, return nullptr.
	if ( target_entity_number == ENTITYNUM_NONE ) {
		return nullptr;
	}

	// Lookup edict from pool.
	svg_base_edict_t *ent = g_edict_pool.EdictForNumber( target_entity_number );
	// Validate active usage and health.
	if ( ent != nullptr && ent->inUse && ent->health > 0 ) {
		return ent;
	}

	return nullptr;
}

/**
*	@brief	Get the tactical rank (slot index) of a member in this squad.
*	@param	entityNumber	Entity number to query.
*	@return	Rank index in [0, member_count - 1], or -1 if not enrolled.
**/
int32_t svg_squad_t::GetMemberRank( const int32_t entityNumber ) const {
	// Linear scan across squad roster to find matching entity number.
	for ( size_t i = 0; i < member_entity_numbers.size(); i++ ) {
		if ( member_entity_numbers[ i ] == entityNumber ) {
			return static_cast<int32_t>( i );
		}
	}
	// Not enrolled in this squad.
	return -1;
}

/**
*
*
*
*	Coordinator Lifecycle & Management:
*
*
*
**/
/**
*	@brief	Initialize the tactical squad coordinator subsystem.
**/
void SVG_Squad_Init( void ) {
	// Clear all squads.
	s_activeSquads.clear();
	gi.dprintf( "Tactical Squad Coordinator initialized.\n" );
}

/**
*	@brief	Shutdown the squad coordinator and clear all active squads.
**/
void SVG_Squad_Shutdown( void ) {
	// Release all squad records.
	s_activeSquads.clear();
}

/**
*	@brief	Retrieve or allocate a tactical squad record by ID.
*	@param	squadId	Unique squad identifier (> 0).
*	@return	Pointer to squad record.
**/
svg_squad_t *SVG_Squad_GetOrCreate( const int32_t squadId ) {
	// Find existing squad or instantiate new entry.
	auto it = s_activeSquads.find( squadId );
	if ( it != s_activeSquads.end() ) {
		return &it->second;
	}

	svg_squad_t newSquad = {};
	newSquad.squad_id = squadId;
	auto insertResult = s_activeSquads.emplace( squadId, newSquad );
	return &insertResult.first->second;
}

/**
*	@brief	Retrieve an existing squad record by ID.
*	@param	squadId	Unique squad identifier.
*	@return	Pointer to squad record, or nullptr if not found.
**/
svg_squad_t *SVG_Squad_Get( const int32_t squadId ) {
	auto it = s_activeSquads.find( squadId );
	if ( it != s_activeSquads.end() ) {
		return &it->second;
	}
	return nullptr;
}

/**
*	@brief	Destroy and release an active squad.
*	@param	squadId	Unique squad identifier.
**/
void SVG_Squad_Destroy( const int32_t squadId ) {
	s_activeSquads.erase( squadId );
}

/**
*	@brief	Enroll an entity into a tactical squad.
*	@param	squadId			Target squad ID.
*	@param	entityNumber	Entity pool index to enroll.
**/
void SVG_Squad_RegisterMember( const int32_t squadId, const int32_t entityNumber ) {
	// Get or instantiate target squad.
	svg_squad_t *squad = SVG_Squad_GetOrCreate( squadId );
	if ( squad == nullptr ) {
		return;
	}

	// Check if already registered.
	if ( squad->GetMemberRank( entityNumber ) >= 0 ) {
		return;
	}

	// Append entity to roster.
	squad->member_entity_numbers.push_back( entityNumber );

	// Tag edict crowd properties.
	svg_base_edict_t *ent = g_edict_pool.EdictForNumber( entityNumber );
	if ( ent != nullptr ) {
		ent->crowd.crowdID = squadId;
		ent->crowd.slotIndex = static_cast<int32_t>( squad->member_entity_numbers.size() - 1 );
	}

	// Rebuild slots if squad is currently moving.
	if ( squad->is_moving ) {
		SVG_Squad_RebuildSlots( *squad );
	}
}

/**
*	@brief	Remove an entity from a tactical squad.
*	@param	squadId			Target squad ID.
*	@param	entityNumber	Entity pool index to remove.
**/
void SVG_Squad_UnregisterMember( const int32_t squadId, const int32_t entityNumber ) {
	svg_squad_t *squad = SVG_Squad_Get( squadId );
	if ( squad == nullptr ) {
		return;
	}

	// Locate member index.
	auto it = std::find( squad->member_entity_numbers.begin(), squad->member_entity_numbers.end(), entityNumber );
	if ( it == squad->member_entity_numbers.end() ) {
		return;
	}

	// Remove from roster.
	squad->member_entity_numbers.erase( it );

	// Reset edict crowd linkage.
	svg_base_edict_t *ent = g_edict_pool.EdictForNumber( entityNumber );
	if ( ent != nullptr ) {
		ent->crowd.crowdID = -1;
		ent->crowd.slotIndex = -1;
	}

	// Compact roster to fill vacant rank.
	SVG_Squad_CompactOnMemberDeath( *squad );
}

/**
*	@brief	Designate a squad member as the authoritative squad leader.
*	@param	squadId				Target squad ID.
*	@param	leaderEntityNumber	Entity number of designated leader.
**/
void SVG_Squad_SetLeader( const int32_t squadId, const int32_t leaderEntityNumber ) {
	svg_squad_t *squad = SVG_Squad_GetOrCreate( squadId );
	if ( squad == nullptr ) {
		return;
	}

	squad->leader_entity_number = leaderEntityNumber;

	// Promote leader to Rank 0 in the roster if present.
	auto it = std::find( squad->member_entity_numbers.begin(), squad->member_entity_numbers.end(), leaderEntityNumber );
	if ( it != squad->member_entity_numbers.end() && it != squad->member_entity_numbers.begin() ) {
		squad->member_entity_numbers.erase( it );
		squad->member_entity_numbers.insert( squad->member_entity_numbers.begin(), leaderEntityNumber );

		// Update slot indices across members.
		for ( size_t i = 0; i < squad->member_entity_numbers.size(); i++ ) {
			svg_base_edict_t *ent = g_edict_pool.EdictForNumber( squad->member_entity_numbers[ i ] );
			if ( ent != nullptr ) {
				ent->crowd.slotIndex = static_cast<int32_t>( i );
			}
		}
	}
}

/**
*	@brief	Issue a movement command to a tactical squad.
*	@param	squadId		Target squad ID.
*	@param	destination	World-space destination in double precision.
*	@param	style		Commanded formation style.
*	@param	params		Spacing and arrival configuration parameters.
**/
void SVG_Squad_SetCommand( const int32_t squadId, const Vector3DP &destination, const crowd_chase_target_type_t style, const svg_crowd_params_t &params ) {
	svg_squad_t *squad = SVG_Squad_GetOrCreate( squadId );
	if ( squad == nullptr ) {
		return;
	}

	squad->destination_origin = destination;
	squad->commanded_style = style;
	squad->effective_style = style;
	squad->params = params;
	squad->is_moving = true;
	squad->active_bottleneck_portal_id = -1;

	// Calculate forward heading yaw from squad centroid toward destination.
	Vector3DP squadCenter = { 0.0, 0.0, 0.0 };
	int32_t livingCount = 0;
	for ( const int32_t entNum : squad->member_entity_numbers ) {
		const svg_base_edict_t *ent = g_edict_pool.EdictForNumber( entNum );
		if ( ent != nullptr && ent->inUse && ent->health > 0 ) {
			squadCenter = squadCenter + Vector3DP( ent->currentOrigin );
			livingCount++;
		}
	}

	if ( livingCount > 0 ) {
		squadCenter = squadCenter * ( 1.0 / static_cast<double>( livingCount ) );
		Vector3DP toDest = destination - squadCenter;
		toDest.z = 0.0;
		if ( QM_Vector3LengthDP( toDest ) > 0.001 ) {
			squad->current_heading_yaw = QM_Vector3ToYawDP( toDest );
		}
	}

	// Rebuild formation slots.
	SVG_Squad_RebuildSlots( *squad );
}

/**
*	@brief	Halt a tactical squad in place.
*	@param	squadId	Target squad ID.
**/
void SVG_Squad_Stop( const int32_t squadId ) {
	svg_squad_t *squad = SVG_Squad_Get( squadId );
	if ( squad != nullptr ) {
		squad->is_moving = false;
	}
}

/**
*	@brief	Recompute world-space formation slot positions for a squad.
*	@param	squad	Squad whose slots will be rebuilt.
**/
void SVG_Squad_RebuildSlots( svg_squad_t &squad ) {
	const size_t memberCount = squad.member_entity_numbers.size();
	if ( memberCount == 0 ) {
		squad.slots.clear();
		return;
	}

	// 1. Generate local slot offsets using mathematical template generators:
	squad.slots.clear();
	SVG_Crowd_GenerateFormationSlots( squad.effective_style, memberCount, squad.params, squad.slots );

	// 2. Transform local slot offsets to world coordinates:
	SVG_Crowd_TransformLocalSlotsToWorld( squad.destination_origin, squad.current_heading_yaw, squad.slots );

	// 3. Project and snap slots onto walkable navmesh faces:
	//! Default agent collision radius for slot boundary clearance.
	static constexpr double SQUAD_AGENT_RADIUS = 24.0;
	SVG_Crowd_SnapSlotsToNavMesh( squad.slots, squad.destination_origin, SQUAD_AGENT_RADIUS );

	// 4. For circular or defensive perimeter formations, fit rings to room walls:
	if ( squad.effective_style == crowd_chase_target_type_t::CROWD_STYLE_CIRCLE_FILLED ||
	     squad.effective_style == crowd_chase_target_type_t::CROWD_STYLE_SURROUND_PERIMETER ) {
		SVG_Crowd_FitCircularFormationToRoom( squad.slots, squad.destination_origin, SQUAD_AGENT_RADIUS );
	}

	// 5. Update assigned goal origins on each member edict:
	for ( size_t i = 0; i < memberCount && i < squad.slots.size(); i++ ) {
		const int32_t entNum = squad.member_entity_numbers[ i ];
		svg_base_edict_t *ent = g_edict_pool.EdictForNumber( entNum );
		if ( ent != nullptr && ent->inUse ) {
			ent->crowd.slotIndex = static_cast<int32_t>( i );
			ent->crowd.assignedGoalOrigin = QM_Vector3FromDP( squad.slots[ i ].worldPosition );
			ent->crowd.reachedGoal = false;
		}
	}
}

/**
*	@brief	Compact squad roster immediately when a member dies to eliminate holes.
*	@param	squad	Squad whose roster will be compacted.
**/
void SVG_Squad_CompactOnMemberDeath( svg_squad_t &squad ) {
	bool rosterChanged = false;

	// Filter out inactive or dead members:
	auto it = squad.member_entity_numbers.begin();
	while ( it != squad.member_entity_numbers.end() ) {
		const svg_base_edict_t *ent = g_edict_pool.EdictForNumber( *it );
		if ( ent == nullptr || !ent->inUse || ent->health <= 0 ) {
			it = squad.member_entity_numbers.erase( it );
			rosterChanged = true;
		} else {
			++it;
		}
	}

	// If members departed, re-index surviving members and rebuild slots:
	if ( rosterChanged ) {
		for ( size_t i = 0; i < squad.member_entity_numbers.size(); i++ ) {
			svg_base_edict_t *ent = g_edict_pool.EdictForNumber( squad.member_entity_numbers[ i ] );
			if ( ent != nullptr ) {
				ent->crowd.slotIndex = static_cast<int32_t>( i );
			}
		}

		if ( squad.is_moving ) {
			SVG_Squad_RebuildSlots( squad );
		}
	}
}

/**
*	@brief	Adapt squad formation style to bottleneck aperture geometry (e.g. narrow portal).
*	@param	squad	Target squad.
*	@param	portal	Pointer to upcoming portal, or nullptr if unconstrained.
**/
void SVG_Squad_AdaptFormationToBottleneck( svg_squad_t &squad, const nav_portal_t *portal ) {
	//! Maximum aperture width that forces single-file column formation.
	static constexpr double BOTTLENECK_APERTURE_MAX_WIDTH = 128.0;

	if ( portal != nullptr && portal->width <= BOTTLENECK_APERTURE_MAX_WIDTH ) {
		// Constrained by narrow portal: adapt to single-file column march.
		if ( squad.effective_style != crowd_chase_target_type_t::CROWD_STYLE_COLUMN_MARCH ) {
			squad.effective_style = crowd_chase_target_type_t::CROWD_STYLE_COLUMN_MARCH;
			squad.active_bottleneck_portal_id = portal->portal_id;
			SVG_Squad_RebuildSlots( squad );
		}
	} else {
		// Open space: restore commanded formation style.
		if ( squad.effective_style != squad.commanded_style ) {
			squad.effective_style = squad.commanded_style;
			squad.active_bottleneck_portal_id = -1;
			SVG_Squad_RebuildSlots( squad );
		}
	}
}

/**
*	@brief	Query right-of-way priority for a squad member at a bottleneck portal.
*	@param	squadId			Squad ID.
*	@param	entityNumber	Member entity number.
*	@param	portalId		Portal index being traversed.
*	@param	outHasRightOfWay	[out] True if member may advance, false if member should yield.
*	@return	True if query succeeded.
**/
bool SVG_Squad_QueryBottleneckRightOfWay( const int32_t squadId, const int32_t entityNumber, const int32_t portalId, bool *outHasRightOfWay ) {
	// Validate output pointer.
	if ( outHasRightOfWay == nullptr ) {
		return false;
	}

	*outHasRightOfWay = true;

	const svg_squad_t *squad = SVG_Squad_Get( squadId );
	if ( squad == nullptr ) {
		return false;
	}

	const int32_t myRank = squad->GetMemberRank( entityNumber );
	if ( myRank < 0 ) {
		return false;
	}

	// Rank 0 (leader / point-man) always retains absolute right-of-way.
	if ( myRank == 0 ) {
		*outHasRightOfWay = true;
		return true;
	}

	// Following rank: inspect immediate predecessor rank (myRank - 1).
	const int32_t prevEntNum = squad->member_entity_numbers[ myRank - 1 ];
	const svg_base_edict_t *prevEnt = g_edict_pool.EdictForNumber( prevEntNum );
	if ( prevEnt == nullptr || !prevEnt->inUse || prevEnt->health <= 0 ) {
		// Predecessor is dead or missing: right-of-way granted.
		*outHasRightOfWay = true;
		return true;
	}

	const nav_portal_t *portal = Nav_SectorGraph_GetPortal( portalId );
	if ( portal == nullptr ) {
		*outHasRightOfWay = true;
		return true;
	}

	// Check predecessor's progress past the portal plane:
	const Vector3DP prevFeet = SVG_GetEntityFeetOriginDP( prevEnt );
	const double prevDepth = QM_Vector3DotProductDP( prevFeet - portal->center, portal->normal );

	//! Standoff clearance distance beyond portal plane before follower is permitted to cross.
	static constexpr double PORTAL_CLEARANCE_STANDOFF = 36.0;

	// If predecessor has cleared the portal threshold, follower has right of way; otherwise follower yields.
	*outHasRightOfWay = ( prevDepth >= PORTAL_CLEARANCE_STANDOFF );
	return true;
}

/**
*	@brief	Per-frame squad coordinator update loop.
**/
void SVG_Squad_Update( void ) {
	// Process each active squad.
	for ( auto &pair : s_activeSquads ) {
		svg_squad_t &squad = pair.second;
		if ( !squad.is_moving ) {
			continue;
		}

		// 1. Compact dead members immediately:
		SVG_Squad_CompactOnMemberDeath( squad );

		if ( squad.member_entity_numbers.empty() ) {
			squad.is_moving = false;
			continue;
		}

		// 2. Check upcoming portal along squad path to adapt formation style:
		const svg_base_edict_t *leader = squad.GetLeaderEntity();
		if ( leader != nullptr ) {
			nav_sector_info_t sectorInfo = {};
			if ( Nav_SectorGraph_QueryAwareness( SVG_GetEntityFeetOriginDP( leader ), squad.destination_origin, &sectorInfo ) ) {
				if ( sectorInfo.next_portal_id >= 0 ) {
					const nav_portal_t *nextPortal = Nav_SectorGraph_GetPortal( sectorInfo.next_portal_id );
					if ( nextPortal != nullptr ) {
						const double distToPortal = QM_Vector3DistanceDP( SVG_GetEntityFeetOriginDP( leader ), nextPortal->center );
						//! Distance threshold to begin formation compression when approaching bottleneck.
						static constexpr double BOTTLENECK_APPROACH_DISTANCE = 250.0;
						if ( distToPortal <= BOTTLENECK_APPROACH_DISTANCE ) {
							SVG_Squad_AdaptFormationToBottleneck( squad, nextPortal );
						} else {
							SVG_Squad_AdaptFormationToBottleneck( squad, nullptr );
						}
					}
				} else {
					SVG_Squad_AdaptFormationToBottleneck( squad, nullptr );
				}
			}
		}
	}
}

/**
*	@brief	Retrieve all active squads.
*	@return	Const reference to internal squad map.
**/
const std::unordered_map<int32_t, svg_squad_t> &SVG_Squad_GetAllSquads( void ) {
	return s_activeSquads;
}
