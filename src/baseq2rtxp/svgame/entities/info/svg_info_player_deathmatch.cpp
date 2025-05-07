/*******************************************************************
*
*
*	ServerGame: Info Entity 'info_player_deathmatch'.
*
*
********************************************************************/
#include "svgame/svg_local.h"

// Include player start class types header.
#include "svgame/entities/info/svg_info_player_start.h"



/**
*
*   info_player_deathmatch:
*
**/
/**
*   @brief  Spawn routine.
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_info_player_deathmatch_t, onSpawn )( svg_info_player_deathmatch_t *self ) -> void {
    // Call upon base spawn.
    Base::onSpawn( self );

    // If we are not in coop mode, then we don't want this entity to spawn.
    if ( !deathmatch->value ) {
        SVG_FreeEdict( self );
        return;
    }

    // Make it a misc_teleporter_dest entity.
    // TODO: Move into a static create function of said entity?
    //SP_misc_teleporter_dest( self );
    gi.setmodel( self, "models/objects/dmspot/tris.md2" );
    self->s.skinnum = 0;
    self->solid = SOLID_BOUNDS_BOX;
    self->s.renderfx |= RF_NOSHADOW;
    VectorSet( self->mins, -32, -32, -24 );
    VectorSet( self->maxs, 32, 32, -16 );
    gi.linkentity( self );
}