/********************************************************************
*
*
*	ServerGame: func_ entity utilities.
*
*
********************************************************************/
#include "svgame/svg_local.h"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_door.h"



/**
*	@brief
**/
void Touch_DoorTrigger( svg_entity_t *self, svg_entity_t *other, const cm_plane_t *plane, cm_surface_t *surf ) {
    //gi.dprintf( "(%s:%i) debugging! :-)\n ", __func__, __LINE__ );

    if ( other->health <= 0 )
        return;
    //gi.dprintf( "(%s:%i) debugging! :-)\n ", __func__, __LINE__ );
    if ( !( other->svflags & SVF_MONSTER ) && ( !other->client ) )
        return;
    //gi.dprintf( "(%s:%i) debugging! :-)\n ", __func__, __LINE__ );
    if ( ( self->owner->spawnflags & DOOR_SPAWNFLAG_NOMONSTER ) && ( other->svflags & SVF_MONSTER ) )
        return;
    //gi.dprintf( "(%s:%i) debugging! :-)\n ", __func__, __LINE__ );
    if ( level.time < self->touch_debounce_time )
        return;
    self->touch_debounce_time = level.time + 1_sec;

    door_use( self->owner, other, other, entity_usetarget_type_t::ENTITY_USETARGET_TYPE_TOGGLE, 1 );
}

/**
*	@brief
**/
void Think_SpawnDoorTrigger( svg_entity_t *ent ) {
    svg_entity_t *other;
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

    SVG_PushMove_Think_CalculateMoveSpeed( ent );
}





















