/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "svgame/svg_local.h"


/*
==============================================================================

PLAYER TRAIL

==============================================================================

This is a circular list containing the a list of points of where
the player has been recently.  It is used by monsters for pursuit.

.origin     the spot
.owner      forward link
.aiment     backward link
*/


#define TRAIL_LENGTH    128

edict_t     *trail[TRAIL_LENGTH];
int         trail_head;
bool        trail_active = false;

#define NEXT(n)     (((n) + 1) & (TRAIL_LENGTH - 1))
#define PREV(n)     (((n) - 1) & (TRAIL_LENGTH - 1))

/*
=============
visible

returns 1 if the entity is visible to self, even if not infront ()
=============
*/
static const bool visible( edict_t *self, edict_t *other ) {
    vec3_t  spot1;
    vec3_t  spot2;
    trace_t trace;

    VectorCopy( self->s.origin, spot1 );
    spot1[ 2 ] += self->viewheight;
    VectorCopy( other->s.origin, spot2 );
    spot2[ 2 ] += other->viewheight;
    trace = gi.trace( spot1, vec3_origin, vec3_origin, spot2, self, MASK_OPAQUE );

    if ( trace.fraction == 1.0f )
        return true;
    return false;
}

void PlayerTrail_Init(void)
{
    int     n;

    if (deathmatch->value /* FIXME || coop */)
        return;

    for (n = 0; n < TRAIL_LENGTH; n++) {
        trail[n] = G_AllocateEdict();
        trail[n]->classname = "player_trail";
    }

    trail_head = 0;
    trail_active = true;
}


void PlayerTrail_Add(vec3_t spot)
{
    vec3_t  temp;

    if (!trail_active)
        return;

    VectorCopy(spot, trail[trail_head]->s.origin);

    trail[trail_head]->timestamp = level.time;

    VectorSubtract(spot, trail[PREV(trail_head)]->s.origin, temp);
    trail[trail_head]->s.angles[1] = QM_Vector3ToYaw(temp);

    trail_head = NEXT(trail_head);
}


void PlayerTrail_New(vec3_t spot)
{
    if (!trail_active)
        return;

    PlayerTrail_Init();
    PlayerTrail_Add(spot);
}


edict_t *PlayerTrail_PickFirst(edict_t *self)
{
    int     marker;
    int     n;

    if (!trail_active)
        return NULL;

    // WID: TODO: Monster Reimplement.
    //for (marker = trail_head, n = TRAIL_LENGTH; n; n--) {
    //    if (trail[marker]->timestamp <= self->monsterinfo.trail_time )
    //        marker = NEXT(marker);
    //    else
    //        break;
    //}

    if (visible(self, trail[marker])) {
        return trail[marker];
    }

    if (visible(self, trail[PREV(marker)])) {
        return trail[PREV(marker)];
    }

    return trail[marker];
}

edict_t *PlayerTrail_PickNext(edict_t *self)
{
    int     marker;
    int     n;

    if (!trail_active)
        return NULL;

    // WID: TODO: Monster Reimplement.
    //for (marker = trail_head, n = TRAIL_LENGTH; n; n--) {
    //    if (trail[marker]->timestamp <= self->monsterinfo.trail_time)
    //        marker = NEXT(marker);
    //    else
    //        break;
    //}

    return trail[marker];
}

edict_t *PlayerTrail_LastSpot(void)
{
    return trail[PREV(trail_head)];
}
