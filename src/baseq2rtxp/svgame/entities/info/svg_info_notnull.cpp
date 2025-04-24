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
void svg_info_notnull_t::info_notnull_spawn( svg_info_notnull_t *self ) {
    // Does not free itself. Instead construct a 'point bbox'.
    VectorCopy( self->s.origin, self->absmin );
    VectorCopy( self->s.origin, self->absmax );
}
