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
void svg_info_player_deathmatch_t::info_player_deathmatch_spawn( svg_info_player_deathmatch_t *self ) {
    // Call upon base spawn.
    Base::info_player_start_base_spawn( self );

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