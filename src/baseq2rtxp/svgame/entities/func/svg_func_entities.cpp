/********************************************************************
*
*
*	ServerGame: func_ entity utilities.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_trigger.h"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_door.h"



/**
*	@brief
**/
DEFINE_GLOBAL_CALLBACK_TOUCH( DoorTrigger_Touch )( svg_base_edict_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) -> void {
    //gi.dprintf( "(%s:%i) debugging! :-)\n ", __func__, __LINE__ );

    if ( other->health <= 0 )
        return;
    //gi.dprintf( "(%s:%i) debugging! :-)\n ", __func__, __LINE__ );
    if ( !( other->svflags & SVF_MONSTER ) && ( !other->client ) )
        return;
    //gi.dprintf( "(%s:%i) debugging! :-)\n ", __func__, __LINE__ );
    if ( ( self->owner->spawnflags & svg_func_door_t::SPAWNFLAG_NOMONSTER ) && ( other->svflags & SVF_MONSTER ) )
        return;
    //gi.dprintf( "(%s:%i) debugging! :-)\n ", __func__, __LINE__ );
    if ( level.time < self->touch_debounce_time )
        return;
    self->touch_debounce_time = level.time + 1_sec;

    self->owner->DispatchUseCallback( /*self->owner, */other, other, entity_usetarget_type_t::ENTITY_USETARGET_TYPE_TOGGLE, 1 );
}

/**
*	@brief
**/
DEFINE_GLOBAL_CALLBACK_THINK( DoorTrigger_SpawnThink )( svg_func_door_t *ent ) -> void {
    if ( !ent || !ent->GetTypeInfo()->IsSubClassType<svg_pushmove_edict_t>() ) {
		gi.dprintf( "(%s:%i) Invalid entity type!\n", __func__, __LINE__ );
		return;
    }

    svg_base_edict_t *other;
    vec3_t      mins, maxs;
    
    svg_func_door_t *pushMoveEnt = static_cast<svg_func_door_t *>( ent );
    if ( pushMoveEnt->flags & FL_TEAMSLAVE )
        return;     // only the team leader spawns a trigger

    VectorCopy( pushMoveEnt->absmin, mins );
    VectorCopy( pushMoveEnt->absmax, maxs );

    for ( other = pushMoveEnt->teamchain; other; other = other->teamchain ) {
        AddPointToBounds( other->absmin, mins, maxs );
        AddPointToBounds( other->absmax, mins, maxs );
    }

    // expand
    mins[ 0 ] -= 60;
    mins[ 1 ] -= 60;
    maxs[ 0 ] += 60;
    maxs[ 1 ] += 60;

    other = g_edict_pool.AllocateNextFreeEdict<svg_base_edict_t>();
    VectorCopy( mins, other->mins );
    VectorCopy( maxs, other->maxs );
    other->owner = pushMoveEnt;
    other->solid = SOLID_TRIGGER;
    other->movetype = MOVETYPE_NONE;
    other->s.entityType = ET_PUSH_TRIGGER;
    other->SetTouchCallback( &DoorTrigger_Touch );
    gi.linkentity( other );

    if ( pushMoveEnt->spawnflags & svg_func_door_t::SPAWNFLAG_START_OPEN ) {
		if ( pushMoveEnt->GetTypeInfo()->IsSubClassType<svg_func_door_t>() ) {
			static_cast<svg_func_door_t *>( pushMoveEnt )->SetAreaPortal( true );
		}
    }

    pushMoveEnt->SVG_PushMove_Think_CalculateMoveSpeed( pushMoveEnt );
}





















