/*******************************************************************
*
*
*	ServerGame: Info Entity 'info_notnull'.
*
*
********************************************************************/
#include "svgame/svg_local.h"

// Include player start class types header.
#include "svgame/entities/info/svg_info_notnull.h"


/**
*
*   info_notnull:
*
**/
/**
*   @brief  Spawn routine.
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_info_notnull_t, onSpawn ) ( svg_info_notnull_t *self ) -> void {
    // Does not free itself. Instead construct a 'point bbox'.
    VectorCopy( self->s.origin, self->absMin );
    VectorCopy( self->s.origin, self->absMax );
}


/**
*
*   func_group:
*
**/
/**
*   @brief  Spawn routine.
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_func_group_t, onSpawn ) ( svg_func_group_t *self ) -> void {
    Super::onSpawn( self );
}