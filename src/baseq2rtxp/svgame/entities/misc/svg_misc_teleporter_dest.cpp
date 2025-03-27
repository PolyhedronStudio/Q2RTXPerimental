/********************************************************************
*
*
*	ServerGame: Misc Entity 'misc_teleporter_dest'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_misc.h"
#include "svgame/entities/misc/svg_misc_teleporter_dest.h"



/*QUAKED misc_teleporter_dest (1 0 0) (-32 -32 -24) (32 32 -16)
Point teleporters at these.
*/
/**
*   @brief  
**/
void SP_misc_teleporter_dest( svg_entity_t *ent ) {
    gi.setmodel( ent, "models/objects/dmspot/tris.md2" );
    ent->s.skinnum = 0;
    ent->solid = SOLID_BOUNDS_BOX;
    ent->s.renderfx |= RF_NOSHADOW;
    VectorSet( ent->mins, -32, -32, -24 );
    VectorSet( ent->maxs, 32, 32, -16 );
    gi.linkentity( ent );
}