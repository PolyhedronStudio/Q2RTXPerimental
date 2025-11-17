/*******************************************************************
*
*
*	ServerGame: Info Entity 'info_null'.
*
*
********************************************************************/
#include "svgame/svg_local.h"

// Include player start class types header.
#include "svgame/entities/info/svg_info_null.h"


/**
*
*   info_null:
*
**/
/**
*
*   info_notnull:
*
**/
/**
*   @brief  Spawn routine.
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_info_null_t, onSpawn ) ( svg_info_null_t *self ) -> void {
	// Always spawn Super class.
	Super::onSpawn( self );

	// Frees itself.
	g_edict_pool.FreeEdict( self );
}
