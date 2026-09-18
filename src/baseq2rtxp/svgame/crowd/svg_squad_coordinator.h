/********************************************************************
*
*
*	ServerGame: Tactical Squad Coordinator
*	File: svg_squad_coordinator.h
*	Description:
*		Decentralized squad coordination layer. Manages tactical squad
*		rosters, rank assignments, commanded formation styles, dynamic
*		aperture auto-adaptation (compressing into Column formation at
*		bottlenecks and expanding in open sectors), and slot compaction.
*
*
********************************************************************/
#pragma once

#include "svgame/crowd/svg_crowd_types.h"
#include "svgame/crowd/svg_crowd_formations.h"
#include "svgame/nav/nav_sector_graph.h"
#include "shared/math/qm_vector3_dp.h"

#include <vector>
#include <unordered_map>

// Forward declaration.
struct svg_base_edict_t;

/**
*	@brief	Tactical squad state record.
**/
struct svg_squad_t {
	//! Unique squad identifier.
	int32_t squad_id = -1;
	//! Commanded tactical formation style requested by leader / script.
	crowd_chase_target_type_t commanded_style = crowd_chase_target_type_t::CROWD_STYLE_ARROW;
	//! Effective active formation style after geometric bottleneck adaptation.
	crowd_chase_target_type_t effective_style = crowd_chase_target_type_t::CROWD_STYLE_ARROW;
	//! Formation spacing and arrival parameters.
	svg_crowd_params_t params = {};
	//! World-space destination anchor position in double precision.
	Vector3DP destination_origin = { 0.0, 0.0, 0.0 };
	//! Entity number of the designated squad leader (ENTITYNUM_NONE if unassigned).
	int32_t leader_entity_number = ENTITYNUM_NONE;
	//! Target entity number if following an edict (ENTITYNUM_NONE if moving to static destination).
	int32_t target_entity_number = ENTITYNUM_NONE;
	//! Last recorded origin of target entity (for follow updates).
	Vector3DP last_target_origin = { 0.0, 0.0, 0.0 };
	//! Timestamp of last follow rebuild.
	QMTime last_target_update_time = 0_ms;
	//! Current forward heading yaw in degrees.
	double current_heading_yaw = 0.0;
	//! Dynamic corridor squeeze factor [0.2..1.0] applied to lateral spacing.
	double dynamic_squeeze_factor = 1.0;
	//! Whether the squad is actively moving toward its destination.
	bool is_moving = false;
	//! Roster of active member entity numbers sorted strictly by tactical rank (Rank 0 = leader / point-man).
	std::vector<int32_t> member_entity_numbers;
	//! Calculated formation slots in world space.
	std::vector<svg_crowd_slot_t> slots;
	//! Index of bottleneck portal currently constraining formation width (-1 if unconstrained).
	int32_t active_bottleneck_portal_id = -1;

	/**
	*	@brief	Resolve the pointer to the designated squad leader entity.
	*	@return	Leader edict pointer, or nullptr if dead / departed.
	**/
	svg_base_edict_t *GetLeaderEntity( void ) const;

	/**
	*	@brief	Resolve the pointer to the target entity being followed.
	*	@return	Target edict pointer, or nullptr if dead / departed.
	**/
	svg_base_edict_t *GetTargetEntity( void ) const;

	/**
	*	@brief	Get the tactical rank (slot index) of a member in this squad.
	*	@param	entityNumber	Entity number to query.
	*	@return	Rank index in [0, member_count - 1], or -1 if not enrolled.
	**/
	int32_t GetMemberRank( const int32_t entityNumber ) const;
};

/**
*	@brief	Initialize the tactical squad coordinator subsystem.
**/
void SVG_Squad_Init( void );

/**
*	@brief	Shutdown the squad coordinator and clear all active squads.
**/
void SVG_Squad_Shutdown( void );

/**
*	@brief	Retrieve or allocate a tactical squad record by ID.
*	@param	squadId	Unique squad identifier (> 0).
*	@return	Pointer to squad record.
**/
svg_squad_t *SVG_Squad_GetOrCreate( const int32_t squadId );

/**
*	@brief	Retrieve an existing squad record by ID.
*	@param	squadId	Unique squad identifier.
*	@return	Pointer to squad record, or nullptr if not found.
**/
svg_squad_t *SVG_Squad_Get( const int32_t squadId );

/**
*	@brief	Destroy and release an active squad.
*	@param	squadId	Unique squad identifier.
**/
void SVG_Squad_Destroy( const int32_t squadId );

/**
*	@brief	Enroll an entity into a tactical squad.
*	@param	squadId			Target squad ID.
*	@param	entityNumber	Entity pool index to enroll.
**/
void SVG_Squad_RegisterMember( const int32_t squadId, const int32_t entityNumber );

/**
*	@brief	Remove an entity from a tactical squad.
*	@param	squadId			Target squad ID.
*	@param	entityNumber	Entity pool index to remove.
**/
void SVG_Squad_UnregisterMember( const int32_t squadId, const int32_t entityNumber );

/**
*	@brief	Designate a squad member as the authoritative squad leader.
*	@param	squadId				Target squad ID.
*	@param	leaderEntityNumber	Entity number of designated leader.
**/
void SVG_Squad_SetLeader( const int32_t squadId, const int32_t leaderEntityNumber );

/**
*	@brief	Issue a movement command to a tactical squad.
*	@param	squadId		Target squad ID.
*	@param	destination	World-space destination in double precision.
*	@param	style		Commanded formation style.
*	@param	params		Spacing and arrival configuration parameters.
**/
void SVG_Squad_SetCommand( const int32_t squadId, const Vector3DP &destination, const crowd_chase_target_type_t style, const svg_crowd_params_t &params );

/**
*	@brief	Halt a tactical squad in place.
*	@param	squadId	Target squad ID.
**/
void SVG_Squad_Stop( const int32_t squadId );

/**
*	@brief	Recompute world-space formation slot positions for a squad.
*	@param	squad	Squad whose slots will be rebuilt.
**/
void SVG_Squad_RebuildSlots( svg_squad_t &squad );

/**
*	@brief	Compact squad roster immediately when a member dies to eliminate holes.
*	@param	squad	Squad whose roster will be compacted.
**/
void SVG_Squad_CompactOnMemberDeath( svg_squad_t &squad );

/**
*	@brief	Adapt squad formation style to bottleneck aperture geometry (e.g. narrow portal).
*	@param	squad	Target squad.
*	@param	portal	Pointer to upcoming portal, or nullptr if unconstrained.
**/
void SVG_Squad_AdaptFormationToBottleneck( svg_squad_t &squad, const nav_portal_t *portal );

/**
*	@brief	Query right-of-way priority for a squad member at a bottleneck portal.
*	@param	squadId			Squad ID.
*	@param	entityNumber	Member entity number.
*	@param	portalId		Portal index being traversed.
*	@param	outHasRightOfWay	[out] True if member may advance, false if member should yield.
*	@return	True if query succeeded.
**/
bool SVG_Squad_QueryBottleneckRightOfWay( const int32_t squadId, const int32_t entityNumber, const int32_t portalId, bool *outHasRightOfWay );

/**
*	@brief	Per-frame squad coordinator update loop.
*	@note	Invoked from SVG_Crowd_Frame() to synchronize squad state, aperture adaptation, and slot tracking.
**/
void SVG_Squad_Update( void );

/**
*	@brief	Retrieve all active squads.
*	@return	Const reference to internal squad map.
**/
const std::unordered_map<int32_t, svg_squad_t> &SVG_Squad_GetAllSquads( void );
