/*******************************************************************
*
*
*	ServerGame: Info Entity 'info_notnull'.
*
*
********************************************************************/
#include "svgame/svg_local.h"

void SP_info_notnull( svg_edict_t *self ) {
    VectorCopy( self->s.origin, self->absmin );
    VectorCopy( self->s.origin, self->absmax );
}