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
DEFINE_MEMBER_CALLBACK_SPAWN( svg_info_player_coop_t, onSpawn )( svg_info_player_coop_t *self ) -> void {
    // Call upon base spawn.
    Super::onSpawn( self );

    // Note: do not free this entity at spawn time based on the coop cvar.
    // The gamemode and CVARs may not be initialized during map entity spawning,
    // which would incorrectly remove valid coop spawnpoints. Selection
    // logic will consult the active gamemode when choosing spawn locations.
}