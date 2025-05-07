/*******************************************************************
*
*
*	ServerGame: Info Entity 'SP_info_player_intermission'.
*
*
********************************************************************/
#include "svgame/svg_local.h"

// Include player start class types header.
#include "svgame/entities/info/svg_info_player_start.h"


/**
*
*   info_player_intermission:
*
**/
/**
*   @brief  Spawn routine.
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_info_player_intermission_t, onSpawn )( svg_info_player_intermission_t *self ) -> void {
    // Call upon base spawn.
    Base::onSpawn( self );

    // If we are not in coop mode, then we don't want this entity to spawn.
    //if ( !coop->value ) {
    //    return;
    //}
}