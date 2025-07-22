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

#include "svgame/player/svg_player_hud.h"

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
    Super::onSpawn( self );

    // If we are not in coop mode, then we don't want this entity to spawn.
    //if ( !coop->value ) {
    //    return;
    //}
}
/**
*   @brief  OnUse callback, will move the using client to the intermission point.
**/
DEFINE_MEMBER_CALLBACK_USE( svg_info_player_intermission_t, onUse )( svg_info_player_intermission_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) -> void {
    SVG_HUD_MoveClientToIntermission( other );
}