/********************************************************************
*
*
*    ServerGame: TestDummy Debug Monster Edict (A* only)
*    File: svg_monster_testdummy_debug.h
*    Description:
*        Lightweight debug variant of the TestDummy that always attempts
*        asynchronous A* to its activator. Useful for isolating pathfinding
*        failures without other AI behavior interfering.
*
*
********************************************************************/
#pragma once

#include "svgame/entities/monster/svg_monster_testdummy.h"

/**
 *    Debug TestDummy Entity: always A* to activator
 **/
struct svg_monster_testdummy_debug_t : public svg_monster_testdummy_t {
	//! Default constructor.
	svg_monster_testdummy_debug_t() = default;
		//! Constructor for use with constructing for an cm_entity_t *entityDictionary.
	svg_monster_testdummy_debug_t( const cm_entity_t *ed ) : Super( ed ) {};
	//! Destructor.
	virtual ~svg_monster_testdummy_debug_t() = default;

	/**
	*    Register this spawn class as a world-spawnable entity.
	**/
	DefineWorldSpawnClass(
		"monster_testdummy_debug", svg_monster_testdummy_debug_t, svg_monster_testdummy_t,
		EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn,
		svg_monster_testdummy_debug_t::onSpawn
	);

	/**
	*	@brief	For this debug variant, we override the spawn and think callbacks to always attempt async A* to the activator.
	*			Spawn for debug testdummy: call base onSpawn then set think to our simple loop.
	**/
	DECLARE_MEMBER_CALLBACK_SPAWN( svg_monster_testdummy_debug_t, onSpawn );

	//=============================================================================================

	/**
	*	@brief	A* specific thinker: always attempt async A* to activator if present(and if it goes LOS, sets think to onThink_AStarPursuitTrail.), otherwise go idle.
	*
	*	@details	Will always check for player presence first, and if not present will check for trail presence.
	*				If player is present, will attempt async A* to player.
	*				If trail is present, sets nextThink to onThink_AStarPursuitTrail and attempt async A* to the trail's last known position.
	*				If neither are present, will set nextThink to onThink_Idle.
	**/
	DECLARE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_debug_t, onThink_AStarToPlayer );
	/**
	*	@brief	A* specific thinker: always attempt async A* to activator trail if present, and otherwise go idle.
	* 
	*	@details	Will always check for player presence first, and if not present will check for trail presence. 
	*				If trail is present, will attempt async A* to the trail's last known position. 
	*				If player is present, sets nextThink to onThink_AStarToPlayer and attempt async A* to player. 
	*				If neither are present, will set nextThink to onThink_Idle.
	**/
	DECLARE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_debug_t, onThink_AStarPursuitTrail );

	/**
	*	@brief	Always looks for activator presence, or its trail, and otherwise does nothing.
	* 
	*	@details	If we are in idle state, it means we failed to find a valid target to pursue in the previous think.
	*				If activator is present, sets nextThink to onThink_AStarToPlayer and attempt async A* to player.
	*				If trail is present, sets nextThink to onThink_AStarPursuitTrail and attempt async A* to the trail's last known position.
	*				If neither are present, will keep nextThink as onThink_Idle and do nothing.
	**/
	DECLARE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_debug_t, onThink_Idle );
	/**
	*	@brief	Set when dead. Does nothing.
	**/
	DECLARE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_debug_t, onThink_Dead );

	/**
	*	@brief	Handle use toggles so the debug pursuit only runs when explicitly activated.
	**/
	DECLARE_MEMBER_CALLBACK_USE( svg_monster_testdummy_debug_t, onUse );


	//=============================================================================================
	//=============================================================================================
	

	/**
	*	@brief	Generic support routine taking care of the base logic that each onThink implementation relies on.
	*	@return	True if the caller should proceed with its specific think logic, or false if it should return early and skip the specific think logic.
	**/
	const bool OnThinkGeneric();

	/**
	*	@brief	Performs the actual SlideMove processing and updates the final origin if successful.
	*	@return	The blockedMask result from the slide move, which is important for the caller to determine if we should do any special handling for being trapped (e.g., try to jump, pick a new path, etc).
	**/
	const int32_t PerformSlideMove();


	//=============================================================================================
	//=============================================================================================


	/**
	*    @brief    Attempt A* navigation to a target origin. (Local to this TU to avoid parent dependency).
	*    @param    goalOrigin      World-space destination used for the rebuild request.
	*    @param    force           When true, cancel any pending rebuild and enqueue immediately.
	*    @return   True if movement/animation was updated.
	**/
	const bool MoveAStarToOrigin( const Vector3 &goalOrigin, bool force = false );

	/**
	*    @brief    Last known valid navigation fallback point (entity feet-origin space).
	*    @note     Used as a conservative fallback when async A* hasn't produced a path yet.
	**/
	Vector3 last_valid_nav_point = { 0.0f, 0.0f, 0.0f };
	//! Whether last_valid_nav_point contains a meaningful value.
	bool has_last_valid_nav_point = false;
	//! Current breadcrumb we are attempting to chase when following the trail.
	svg_base_edict_t *trail_target = nullptr;
	//! Tracks whether the debug monster has been enabled by the player.
	bool isActivated = false;

	/**
	*    @brief    Slerp direction helper. (Local to this TU to avoid parent dependency).
	**/
	const Vector3 SlerpLocalVector3( const Vector3 &from, const Vector3 &to, float t );

	/**
	*	@brief	Recategorize the entity's ground/liquid and ground states.
	**/
	const void RecategorizeGroundAndLiquidState();
};
