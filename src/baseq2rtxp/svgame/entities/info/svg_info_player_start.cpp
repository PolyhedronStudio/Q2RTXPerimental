/*******************************************************************
*
*
*	ServerGame: Info Entity 'info_player_start'.
*
*
********************************************************************/
#include "svgame/svg_local.h"

// Include player start class types header.
#include "svgame/entities/info/svg_info_player_start.h"

/**
*
*   info_player_base_start:
*
**/
/**
*   @brief  Spawn routine.
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_info_player_base_start_t, onSpawn )( svg_info_player_base_start_t *self ) -> void {
    // Call upon base spawn.
    Base::base_edict_spawn( self );
}

/**
*
*   info_player_start:
*
**/
/**
*   @brief  Spawn routine.
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_info_player_start_t, onSpawn )( svg_info_player_start_t *self ) -> void {
    // Call upon base spawn.
    Base::onSpawn( self );

    // If we are not in coop mode, then we don't want this entity to spawn.
    //if ( !coop->value ) {
    //    return;
    //}
}