/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_water'.
* 
*   Technically, a func_door though.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"

#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_callfunction.hpp"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_door.h"
#include "svgame/entities/func/svg_func_water.h"



/**
*   Spawnflags:
**/
static constexpr int32_t FUNC_WATER_START_OPEN  = BIT( 0 );
static constexpr int32_t FUNC_WATER_TOGGLE      = BIT( 4 );



/*QUAKED func_water (0 .5 .8) ? START_OPEN
func_water is a moveable water brush.  It must be targeted to operate.  Use a non-water texture at your own risk.

START_OPEN causes the water to move to its destination when spawned and operate in reverse.

"angle"     determines the opening direction (up or down only)
"speed"     movement speed (25 default)
"wait"      wait before returning (-1 default, -1 = TOGGLE)
"lip"       lip remaining at end of move (0 default)
"sounds"    (yes, these need to be changed)
0)  no sound
1)  water
2)  lava
*/

void SP_func_water( svg_base_edict_t *self ) {
    vec3_t  abs_movedir;

    SVG_Util_SetMoveDir( self->s.angles, self->movedir );
    self->movetype = MOVETYPE_PUSH;
    self->solid = SOLID_BSP;
    self->s.entityType = ET_PUSHER;
    gi.setmodel( self, self->model );

    switch ( self->sounds ) {
    default:
        break;

    case 1: // water
        self->pushMoveInfo.sounds.start = gi.soundindex( "world/mov_watr.wav" );
        self->pushMoveInfo.sounds.end = gi.soundindex( "world/stp_watr.wav" );
        break;

    case 2: // lava
        self->pushMoveInfo.sounds.start = gi.soundindex( "world/mov_watr.wav" );
        self->pushMoveInfo.sounds.end = gi.soundindex( "world/stp_watr.wav" );
        break;
    }

    // calculate second position
    VectorCopy( self->s.origin, self->pos1 );
    abs_movedir[ 0 ] = fabsf( self->movedir[ 0 ] );
    abs_movedir[ 1 ] = fabsf( self->movedir[ 1 ] );
    abs_movedir[ 2 ] = fabsf( self->movedir[ 2 ] );
    self->pushMoveInfo.distance = abs_movedir[ 0 ] * self->size[ 0 ] + abs_movedir[ 1 ] * self->size[ 1 ] + abs_movedir[ 2 ] * self->size[ 2 ] - st.lip;
    VectorMA( self->pos1, self->pushMoveInfo.distance, self->movedir, self->pos2 );

    // if it starts open, switch the positions
    if ( self->spawnflags & FUNC_WATER_START_OPEN ) {
        VectorCopy( self->pos2, self->s.origin );
        VectorCopy( self->pos1, self->pos2 );
        VectorCopy( self->s.origin, self->pos1 );
    }

    VectorCopy( self->pos1, self->pushMoveInfo.startOrigin );
    VectorCopy( self->s.angles, self->pushMoveInfo.startAngles );
    VectorCopy( self->pos2, self->pushMoveInfo.endOrigin );
    VectorCopy( self->s.angles, self->pushMoveInfo.endAngles );

    self->pushMoveInfo.state = PUSHMOVE_STATE_BOTTOM;

    if ( !self->speed )
        self->speed = 25;
    self->pushMoveInfo.accel = self->pushMoveInfo.decel = self->pushMoveInfo.speed = self->speed;

    if ( !self->wait )
        self->wait = -1;
    self->pushMoveInfo.wait = self->wait;

    self->SetUseCallback( door_use );

    if ( self->wait == -1 )
        self->spawnflags |= DOOR_SPAWNFLAG_TOGGLE;

    self->classname = "func_door";

    gi.linkentity( self );
}