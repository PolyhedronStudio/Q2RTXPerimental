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
DECLARE_GLOBAL_CALLBACK_TOUCH( DoorTrigger_Touch );
DEFINE_GLOBAL_CALLBACK_TOUCH( DoorTrigger_Touch )( svg_base_edict_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) -> void {
    //gi.dprintf( "(%s:%i) debugging! :-)\n ", __func__, __LINE__ );

    if ( other->health <= 0 ) {
        return;
    }
    //gi.dprintf( "(%s:%i) debugging! :-)\n ", __func__, __LINE__ );
    if ( !( other->svflags & SVF_MONSTER ) && ( !other->client ) ) {
        return;
    }
    //gi.dprintf( "(%s:%i) debugging! :-)\n ", __func__, __LINE__ );
    if ( ( self->owner->spawnflags & svg_func_door_t::SPAWNFLAG_NOMONSTER ) && ( other->svflags & SVF_MONSTER ) ) {
        return;
    }
    //gi.dprintf( "(%s:%i) debugging! :-)\n ", __func__, __LINE__ );
    if ( level.time < self->touch_debounce_time ) {
        return;
    }
    // New debounce time.
    self->touch_debounce_time = level.time + 1_sec;

    // Can't trigger anything if we ain't got an owner.
    if ( !self->owner ) {
        gi.dprintf( "(%s:%i) No owner!\n", __func__, __LINE__ );
        return;
    }

	// <Q2RTXP>: WID: This dun work right now.
    #if 1
    if ( !self->owner->GetTypeInfo()->IsSubClassType<svg_pushmove_edict_t>(true) ) {
        gi.dprintf( "(%s:%i) Owner is not a pushmove entity!\n", __func__, __LINE__ );
        return;
	}
    #endif

    // <Q2RTXP>: WID: This was (other, other, ...)
    self->owner->DispatchUseCallback( other, other, entity_usetarget_type_t::ENTITY_USETARGET_TYPE_TOGGLE, 1 );
}

/**
*	@brief
**/
DECLARE_GLOBAL_CLASSNAME_CALLBACK_THINK( svg_func_door_t, DoorTrigger_SpawnThink );
DEFINE_GLOBAL_CALLBACK_THINK( DoorTrigger_SpawnThink )( svg_func_door_t *ent ) -> void {
    if ( !ent || !ent->GetTypeInfo()->IsSubClassType<svg_func_door_t>( false ) ) {
		gi.dprintf( "(%s:%i) Invalid entity type!\n", __func__, __LINE__ );
		return;
    }

    svg_base_edict_t *other;
    vec3_t      mins, maxs;
    
    svg_pushmove_edict_t *pushMoveEnt = static_cast<svg_pushmove_edict_t *>( ent );
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
		if ( pushMoveEnt->GetTypeInfo()->IsSubClassType<svg_func_door_t>( false ) ) {
			static_cast<svg_func_door_t *>( pushMoveEnt )->SetAreaPortal( true );
		}
    }

    // Apply next think time and method.
    //pushMoveEnt->nextthink = level.time + FRAME_TIME_S;
    //pushMoveEnt->SetThinkCallback( &svg_func_door_t::SVG_PushMove_Think_CalculateMoveSpeed );

    pushMoveEnt->SVG_PushMove_Think_CalculateMoveSpeed( pushMoveEnt );
}





















