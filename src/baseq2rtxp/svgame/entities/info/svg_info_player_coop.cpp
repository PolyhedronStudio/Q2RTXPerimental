/*******************************************************************
*
*
*	ServerGame: Info Entity 'info_player_coop'.
*
*
********************************************************************/
#include "svgame/svg_local.h"

// Include player start class types header.
#include "svgame/entities/info/svg_info_player_start.h"


/**
*
*   info_player_coop:
*
**/
/**
*   @brief  Spawn routine.
**/
void svg_info_player_coop_t::info_player_coop_spawn( svg_info_player_coop_t *self ) {
    // Call upon base spawn.
    Base::info_player_start_base_spawn( self );

    // If we are not in coop mode, then we don't want this entity to spawn.
    if ( !coop->value ) {
        SVG_FreeEdict( self );
        return;
    }
}