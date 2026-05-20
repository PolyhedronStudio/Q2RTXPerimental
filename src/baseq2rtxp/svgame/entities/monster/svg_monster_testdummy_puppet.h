/********************************************************************
*
*    ServerGame: TestDummy Puppet (Straight-Line)
*    File: svg_monster_testdummy_puppet.h
*    Description:
*        Minimal test dummy for hitbox debugging. Moves in a straight line
*        toward a target point, no navigation system required.
*
********************************************************************/
#pragma once

#include "svgame/svg_local.h"

struct svg_monster_testdummy_puppet_t : public svg_base_edict_t {
    svg_monster_testdummy_puppet_t() = default;
    svg_monster_testdummy_puppet_t(const cm_entity_t *ed) : Super(ed) {}
    virtual ~svg_monster_testdummy_puppet_t() = default;

    DefineWorldSpawnClass(
        "monster_testdummy_puppet", svg_monster_testdummy_puppet_t, svg_base_edict_t,
        EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn,
        svg_monster_testdummy_puppet_t::onSpawn
    );

    DECLARE_MEMBER_CALLBACK_SPAWN(svg_monster_testdummy_puppet_t, onSpawn);
    DECLARE_MEMBER_CALLBACK_POSTSPAWN(svg_monster_testdummy_puppet_t, onPostSpawn);
    DECLARE_MEMBER_CALLBACK_THINK(svg_monster_testdummy_puppet_t, onThink);
    DECLARE_MEMBER_CALLBACK_THINK(svg_monster_testdummy_puppet_t, onThink_Dead);
    DECLARE_MEMBER_CALLBACK_USE(svg_monster_testdummy_puppet_t, onUse);
    DECLARE_MEMBER_CALLBACK_PAIN(svg_monster_testdummy_puppet_t, onPain);
    DECLARE_MEMBER_CALLBACK_DIE(svg_monster_testdummy_puppet_t, onDie);

    Vector3 moveTarget = {0,0,0};
    bool hasMoveTarget = false;
    bool isActivated = false;
    skm_rootmotion_set_t *rootMotionSet = nullptr;
};
