/********************************************************************
*
*
*   SharedGame Entity Types:
*
*
********************************************************************/
#pragma once


/**
*	@brief	Starts right off where ET_GAME_TYPES left off.
**/
typedef enum sg_entity_type_e {
	//
	// Player Entity Types:
	// 
	//! Client Player Entity.
	ET_PLAYER = ET_GAME_TYPES,
	//! Client Player Corps Entity.
	ET_PLAYER_CORPSE,
	

	//
	// Monster Entity Types:
	// 
	//! (Monster-) NPC Entity.
	ET_MONSTER,
	//! (Monster-) NPC Corpse Entity.
	ET_MONSTER_CORPSE,


	//
	// Other Entity Types:
	// 
	//! Pusher(Mover) Entity.
	ET_PUSHER,
	//! Item Entity.
	ET_ITEM,
	//! Missile Entity.
	//ET_MISSILE,


	//
	// Effects Entities:
	// 
	//! Explosion Entity.
	//ET_EXPLOSION,
	//! Gib Entity.
	ET_GIB,


	//
	// Targets:
	// 
	//! Speaker Entity.
	ET_TARGET_SPEAKER,


	//
	// Triggers:
	// 
	//! The trigger entity type that is created by ET_PUSHER entities.
	ET_PUSH_TRIGGER,
	//! Teleport Trigger.
	ET_TELEPORT_TRIGGER,




	//! Invisible Entity.
	//ET_INVISIBLE,

	/**
	*	Event Entities :
	*	Any of the EV_* events can be added freestanding
	*	by setting s.entityType to ET_TEMP_EVENT_ENTITY + eventNum
	*	this avoids having to set eFlags and eventNum
	**/
	//! A Temporary Entity Event. To get the value of the event, subtract ET_TEMP_EVENT_ENTITY from the entityType value.
	ET_TEMP_EVENT_ENTITY,


	//!
	//! Maximum types supported.
	//!
	ET_MAX_SHAREDGAME_TYPES,
} sg_entity_type_t;
