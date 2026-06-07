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
	// Make it non-solid, and ensure it doesn't have any model or anything else that would cause it to be visible or interactable.
	self->solid = SOLID_NOT;
	// No model.
	self->s.modelindex = 0;
	// Set bounds regardless.
	self->mins = PHYS_DEFAULT_BBOX_STANDUP_MINS;
	self->maxs = PHYS_DEFAULT_BBOX_STANDUP_MAXS;
	// Call upon base spawn.
    Super::onSpawn( self );


	// Link it. Bounding Box has been set by Super::onSpawn.
	//gi.linkentity( self );
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
    Super::onSpawn( self );

    // If we are not in coop mode, then we don't want this entity to spawn.
    //if ( !coop->value ) {
    //    return;
    //}
}