/*******************************************************************
*
*
*	ServerGame: Info Entity 'info_player_start'.
*
*
********************************************************************/
#include "svgame/svg_local.h"

/*QUAKED info_player_start (1 0 0) (-16 -16 -24) (16 16 32)
The normal starting point for a level (Singleplayer, and as last resort if missing gamemode specific spawn points.).
*/
void SP_info_player_start( svg_entity_t *self ) {
    if ( !coop->value )
        return;
}