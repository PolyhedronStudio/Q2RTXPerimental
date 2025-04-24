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
*   @brief  Spawn routine.
**/
void svg_info_null_t::info_null_spawn( svg_info_null_t *self ) {
    // Frees itself.
	g_edict_pool.FreeEdict( self );
}


/**
*
*   func_group:
*
**/
/**
*   @brief  Spawn routine.
**/
void svg_func_group_t::func_group_spawn( svg_func_group_t *self ) {
	Base::info_null_spawn( self );
}
