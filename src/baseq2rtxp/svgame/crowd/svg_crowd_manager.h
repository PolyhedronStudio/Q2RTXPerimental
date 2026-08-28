/********************************************************************
*
*
*	ServerGame: Crowd & Crew Navigation Manager
*	File: svg_crowd_manager.h
*	Description:
*		Central manager for tracking crowd groups, allocating formation
*		slots, assigning tactical cover, dispatching A* navigation routes,
*		and managing crowd lifecycle using Vector3DP.
*
*
********************************************************************/
#pragma once

#include "svgame/crowd/svg_crowd_types.h"
#include "svgame/crowd/svg_crowd_formations.h"
#include "svgame/nav/nav_path.h"
#include "svgame/nav/nav_cover_query.h"
#include "shared/math/qm_vector3_dp.h"

#include <vector>
#include <unordered_map>
#include <memory>

// Forward declarations.
struct svg_base_edict_t;

/**
*	@brief	Active crowd/crew group coordination record.
**/
/**
*	@brief	Active crowd/crew group coordination record.
**/
struct svg_crowd_group_t {
	//! Crowd identifier (0 = neutral NPC crowd, > 0 = tactical squad).
	int32_t crowdID = -1;
	//! Active chase/formation style.
	crowd_chase_target_type_t style = crowd_chase_target_type_t::CROWD_STYLE_ARROW;
	//! Configuration parameters for spacing, tactical cover, etc.
	svg_crowd_params_t params = {};
	//! Static world-space destination origin in double precision (valid when targetEntityNumber == ENTITYNUM_NONE).
	Vector3DP destinationOrigin = { 0.0, 0.0, 0.0 };
	//! Entity number of the target being followed (ENTITYNUM_NONE if navigating to static destinationOrigin).
	int32_t targetEntityNumber = ENTITYNUM_NONE;
	//! Entity number of the designated squad leader establishing the formation path (ENTITYNUM_NONE if unassigned).
	int32_t leaderEntityNumber = ENTITYNUM_NONE;
	//! Last recorded origin of targetEntity (used to detect when to rebuild formation).
	Vector3DP lastTargetEntityOrigin = { 0.0, 0.0, 0.0 };
	//! Forward heading in degrees used for the active formation orientation.
	double currentHeadingYaw = 0.0;
	//! Dynamic corridor squeeze scale factor [0.2..1.0] applied to lateral spacing.
	double dynamicSqueezeFactor = 1.0;
	//! Cached polyline waypoints from the leader's primary path for followers to share.
	std::vector<Vector3DP> sharedLeaderPath = {};
	//! Calculated formation slots (in world space).
	std::vector<svg_crowd_slot_t> slots = {};
	//! Entity numbers of members actively registered to this group.
	std::vector<int32_t> memberEntityNumbers = {};
	//! Server timestamp when current order began.
	QMTime orderStartTime = 0_ms;
	//! Server timestamp of next scheduled formation/path update tick.
	QMTime nextUpdateTick = 0_ms;
	//! Server timestamp of last target entity follow update.
	QMTime lastTargetEntityUpdateTime = 0_ms;
	//! Whether the crowd is actively moving towards orders.
	bool isMoving = false;

	/**
	*	@brief	Safely resolve the target entity being followed.
	*	@return	Pointer to target edict if active and alive, nullptr otherwise.
	**/
	svg_base_edict_t *GetTargetEntity( void ) const;

	/**
	*	@brief	Safely resolve the designated squad leader entity.
	*	@return	Pointer to leader edict if active and alive, nullptr otherwise.
	**/
	svg_base_edict_t *GetLeaderEntity( void ) const;
};

/**
*	@brief	Initialize crowd management subsystems and cvars.
**/
void SVG_Crowd_Init( void );

/**
*	@brief	Shutdown crowd management subsystems and release all reservations.
**/
void SVG_Crowd_Shutdown( void );

/**
*	@brief	Execute per-frame crowd coordination, slot updates, and staggered pathing.
**/
void SVG_Crowd_Frame( void );

/**
*	@brief	Command a crowd to navigate to a world origin in Vector3DP with the specified formation style.
*	@param	crowdID	Crowd group identifier.
*	@param	origin	World-space destination origin in Vector3DP.
*	@param	style	Formation / chase style.
*	@param	params	Formation parameters (spacing, cover distance, etc.).
*	@return	True when orders were successfully dispatched to one or more crowd members.
**/
bool MoveAStarCrowdOrigin( const int32_t crowdID, const Vector3DP &origin, const crowd_chase_target_type_t style, const svg_crowd_params_t &params = {} );

/**
*	@brief	Command a crowd to navigate to a world origin in Vector3 with the specified formation style.
*	@param	crowdID	Crowd group identifier.
*	@param	origin	World-space destination origin in Vector3.
*	@param	style	Formation / chase style.
*	@param	params	Formation parameters (spacing, cover distance, etc.).
*	@return	True when orders were successfully dispatched to one or more crowd members.
**/
bool MoveAStarCrowdOrigin( const int32_t crowdID, const Vector3 &origin, const crowd_chase_target_type_t style, const svg_crowd_params_t &params = {} );

/**
*	@brief	Command a crowd to follow an entity by entity number in the specified formation style.
*	@param	crowdID				Crowd group identifier.
*	@param	targetEntityNumber	Target entity number to follow.
*	@param	style				Formation / chase style.
*	@param	params				Formation parameters (spacing, cover distance, etc.).
*	@return	True when orders were successfully dispatched to one or more crowd members.
**/
bool MoveAStarFollowEntity( const int32_t crowdID, const int32_t targetEntityNumber, const crowd_chase_target_type_t style, const svg_crowd_params_t &params = {} );

/**
*	@brief	Command a crowd to follow an entity pointer in the specified formation style (convenience overload).
*	@param	crowdID	Crowd group identifier.
*	@param	entity	Target entity to follow.
*	@param	style	Formation / chase style.
*	@param	params	Formation parameters (spacing, cover distance, etc.).
*	@return	True when orders were successfully dispatched to one or more crowd members.
**/
bool MoveAStarFollowEntity( const int32_t crowdID, svg_base_edict_t *entity, const crowd_chase_target_type_t style, const svg_crowd_params_t &params = {} );

/**
*	@brief	Designate a squad member as the squad leader by entity number.
*	@param	crowdID				Crowd identifier.
*	@param	leaderEntityNumber	Entity number to set as leader (or ENTITYNUM_NONE to clear).
**/
void SVG_Crowd_SetLeader( const int32_t crowdID, const int32_t leaderEntityNumber );

/**
*	@brief	Designate a squad member as the squad leader by edict pointer.
*	@param	crowdID		Crowd identifier.
*	@param	leaderEnt	Entity to set as leader.
**/
void SVG_Crowd_SetLeader( const int32_t crowdID, svg_base_edict_t *leaderEnt );

/**
*	@brief	Register an entity as a member of a specific crowd group by entity number.
*	@param	entityNumber	Entity number to register.
*	@param	crowdID			Crowd identifier.
**/
void SVG_Crowd_RegisterMember( const int32_t entityNumber, const int32_t crowdID );

/**
*	@brief	Register an entity as a member of a specific crowd group by edict pointer.
*	@param	ent		Entity to register.
*	@param	crowdID	Crowd identifier.
**/
void SVG_Crowd_RegisterMember( svg_base_edict_t *ent, const int32_t crowdID );

/**
*	@brief	Unregister an entity from its active crowd group by entity number.
*	@param	entityNumber	Entity number to unregister.
**/
void SVG_Crowd_UnregisterMember( const int32_t entityNumber );

/**
*	@brief	Unregister an entity from its active crowd group by edict pointer.
*	@param	ent	Entity to unregister.
**/
void SVG_Crowd_UnregisterMember( svg_base_edict_t *ent );

/**
*	@brief	Retrieve entity numbers of all living members in a specific crowd.
*	@param	crowdID			Crowd identifier.
*	@param	outEntityNums	[out] Vector to receive member entity numbers.
**/
void SVG_Crowd_GetCrowdMembers( const int32_t crowdID, std::vector<int32_t> &outEntityNums );

/**
*	@brief	Retrieve living member edicts of a specific crowd.
*	@param	crowdID		Crowd identifier.
*	@param	outMembers	[out] Vector to receive member entity pointers.
**/
void SVG_Crowd_GetCrowdMembers( const int32_t crowdID, std::vector<svg_base_edict_t*> &outMembers );

/**
*	@brief	Update the formation style of an active crowd group.
*	@param	crowdID	Crowd identifier.
*	@param	style	New formation style.
**/
void SVG_Crowd_SetCrowdStyle( const int32_t crowdID, const crowd_chase_target_type_t style );

/**
*	@brief	Update the configuration parameters of an active crowd group.
*	@param	crowdID	Crowd identifier.
*	@param	params	New configuration parameters.
**/
void SVG_Crowd_SetCrowdParams( const int32_t crowdID, const svg_crowd_params_t &params );

/**
*	@brief	Halt and clear movement orders for an active crowd group.
*	@param	crowdID	Crowd identifier.
**/
void SVG_Crowd_StopCrowd( const int32_t crowdID );

/**
*	@brief	Retrieve a pointer to an active crowd group record.
*	@param	crowdID	Crowd identifier.
*	@return	Pointer to crowd group record, or nullptr if none exists.
**/
svg_crowd_group_t *SVG_Crowd_GetGroup( const int32_t crowdID );

/**
*	@brief	Compute mutual separation steering push force from fellow crowd members by entity number.
*	@param	entityNumber		Query entity number.
*	@param	outSeparationForce	[out] Vector to receive 2D repulsive displacement vector in Vector3DP.
*	@return	True if a non-zero separation force was calculated.
**/
bool SVG_Crowd_ComputeMutualSeparation( const int32_t entityNumber, Vector3DP *outSeparationForce );

/**
*	@brief	Compute mutual separation steering push force from fellow crowd members by edict pointer.
*	@param	ent					Query entity.
*	@param	outSeparationForce	[out] Vector to receive 2D repulsive displacement vector in Vector3DP.
*	@return	True if a non-zero separation force was calculated.
**/
bool SVG_Crowd_ComputeMutualSeparation( const svg_base_edict_t *ent, Vector3DP *outSeparationForce );

/**
*	@brief	Render debug visualization for all active crowd groups and formation slots.
**/
void SVG_Crowd_DebugDraw( void );
