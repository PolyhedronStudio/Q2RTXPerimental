/*******************************************************************
*
*
*	ServerGame: Info Entity 'info_player_deathmatch'.
*
*
********************************************************************/
#include "svgame/svg_local.h"



void SP_misc_teleporter_dest( svg_entity_t *ent );

/*QUAKED info_player_deathmatch (1 0 1) (-16 -16 -24) (16 16 32)
potential spawning position for deathmatch games
*/
void SP_info_player_deathmatch( svg_entity_t *self ) {
    if ( !deathmatch->value ) {
        SVG_FreeEdict( self );
        return;
    }
    SP_misc_teleporter_dest( self );
}