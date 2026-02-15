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

	DECLARE_MEMBER_CALLBACK_SPAWN( svg_monster_testdummy_debug_t, onSpawn );
	DECLARE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_debug_t, onThink );
};
