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
enum {
	//! Client Player Entity.
	ET_PLAYER = ET_GAME_TYPES,
	//! Client Player Corps Entity.
	ET_PLAYER_CORPSE,
	
	//! (Monster-) NPC Entity.
	ET_MONSTER,
	//! (Monster-) NPC Corpse Entity.
	ET_MONSTER_CORPSE,

	//! Pusher(Mover) Entity.
	ET_PUSHER,
	//! Item Entity.
	ET_ITEM,
	//! Missile Entity.
	//ET_MISSILE,

	//! Explosion Entity.
	//ET_EXPLOSION,
	//! Gib Entity.
	ET_GIB,

	//! Speaker Entity.
	ET_TARGET_SPEAKER,

	//! Push(Mover) Trigger.
	ET_PUSH_TRIGGER,
	//! Teleport Trigger.
	ET_TELEPORT_TRIGGER,

	//! Maximum types supported.
	ET_MAX_SHAREDGAME_TYPES

	//! Invisible Entity.
	//ET_INVISIBLE,
	//! Event Entities:
	//ET_EVENTS				// any of the EV_* events can be added freestanding
							// by setting eType to ET_EVENTS + eventNum
							// this avoids having to set eFlags and eventNum
};
