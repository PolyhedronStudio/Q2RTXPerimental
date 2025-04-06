/*******************************************************************
*
*
*	ServerGame: Info Entity 'info_null'.
*
*
********************************************************************/
#include "svgame/svg_local.h"

void SP_info_null( edict_t *self ) {
    SVG_FreeEdict( self );
}