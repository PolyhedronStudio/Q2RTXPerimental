/********************************************************************
*
*
*	ServerGame: func_ entity utilities.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_callfunction.hpp"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_door.h"



/**
*	@brief
**/
void Touch_DoorTrigger( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf ) {
    if ( other->health <= 0 )
        return;

    if ( !( other->svflags & SVF_MONSTER ) && ( !other->client ) )
        return;

    if ( ( self->owner->spawnflags & DOOR_SPAWNFLAG_NOMONSTER ) && ( other->svflags & SVF_MONSTER ) )
        return;

    if ( level.time < self->touch_debounce_time )
        return;
    self->touch_debounce_time = level.time + 1_sec;

    door_use( self->owner, other, other, entity_usetarget_type_t::ENTITY_USETARGET_TYPE_TOGGLE, 0 );
}

/**
*	@brief
**/
void Think_CalcMoveSpeed( edict_t *self ) {
    edict_t *ent;
    float   minDist;
    float   time;
    float   newspeed;
    float   ratio;
    float   dist;

    if ( self->flags & FL_TEAMSLAVE )
        return;     // only the team master does this

    // find the smallest distance any member of the team will be moving
    minDist = fabsf( self->pushMoveInfo.distance );
    for ( ent = self->teamchain; ent; ent = ent->teamchain ) {
        dist = fabsf( ent->pushMoveInfo.distance );
        if ( dist < minDist )
            minDist = dist;
    }

    time = minDist / self->pushMoveInfo.speed;

    // adjust speeds so they will all complete at the same time
    for ( ent = self; ent; ent = ent->teamchain ) {
        newspeed = fabsf( ent->pushMoveInfo.distance ) / time;
        ratio = newspeed / ent->pushMoveInfo.speed;
        if ( ent->pushMoveInfo.accel == ent->pushMoveInfo.speed )
            ent->pushMoveInfo.accel = newspeed;
        else
            ent->pushMoveInfo.accel *= ratio;
        if ( ent->pushMoveInfo.decel == ent->pushMoveInfo.speed )
            ent->pushMoveInfo.decel = newspeed;
        else
            ent->pushMoveInfo.decel *= ratio;
        ent->pushMoveInfo.speed = newspeed;
    }
}

/**
*	@brief
**/
void Think_SpawnDoorTrigger( edict_t *ent ) {
    edict_t *other;
    vec3_t      mins, maxs;

    if ( ent->flags & FL_TEAMSLAVE )
        return;     // only the team leader spawns a trigger

    VectorCopy( ent->absmin, mins );
    VectorCopy( ent->absmax, maxs );

    for ( other = ent->teamchain; other; other = other->teamchain ) {
        AddPointToBounds( other->absmin, mins, maxs );
        AddPointToBounds( other->absmax, mins, maxs );
    }

    // expand
    mins[ 0 ] -= 60;
    mins[ 1 ] -= 60;
    maxs[ 0 ] += 60;
    maxs[ 1 ] += 60;

    other = SVG_AllocateEdict();
    VectorCopy( mins, other->mins );
    VectorCopy( maxs, other->maxs );
    other->owner = ent;
    other->solid = SOLID_TRIGGER;
    other->movetype = MOVETYPE_NONE;
    other->s.entityType = ET_PUSH_TRIGGER;
    other->touch = Touch_DoorTrigger;
    gi.linkentity( other );

    if ( ent->spawnflags & DOOR_SPAWNFLAG_START_OPEN )
        door_use_areaportals( ent, true );

    Think_CalcMoveSpeed( ent );
}





















