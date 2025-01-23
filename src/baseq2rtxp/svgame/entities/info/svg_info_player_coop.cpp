/*******************************************************************
*
*
*	ServerGame: Info Entity 'info_player_coop'.
*
*
********************************************************************/
#include "svgame/svg_local.h"

/*QUAKED info_player_coop (1 0 1) (-16 -16 -24) (16 16 32)
potential spawning position for coop games
*/
void SP_info_player_coop( edict_t *self ) {
    if ( !coop->value ) {
        SVG_FreeEdict( self );
        return;
    }
}