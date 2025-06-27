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

//#define ENABLE_PLAYER_TRAIL_ENTITIES

#ifdef ENABLE_PLAYER_TRAIL_ENTITIES
#define TRAIL_LENGTH    128

svg_base_edict_t     *trail[TRAIL_LENGTH];
int         trail_head;
bool        trail_active = false;

#define NEXT(n)     (((n) + 1) & (TRAIL_LENGTH - 1))
#define PREV(n)     (((n) - 1) & (TRAIL_LENGTH - 1))

void PlayerTrail_Init(void)
{
    int     n;

    if (deathmatch->value /* FIXME || coop */)
        return;

    for (n = 0; n < TRAIL_LENGTH; n++) {
        trail[n] = SVG_AllocateEdict();
        trail[n]->classname = svg_level_qstring_t::from_char_str( "player_trail" );
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


svg_base_edict_t *PlayerTrail_PickFirst(svg_base_edict_t *self)
{
    int32_t     marker = 0;
    int32_t     n = 0;

    if (!trail_active)
        return NULL;

    // WID: TODO: Monster Reimplement.
    for (marker = trail_head, n = TRAIL_LENGTH; n; n--) {
        if (trail[marker]->timestamp <= self->trail_time )
            marker = NEXT(marker);
        else
            break;
    }

    if ( SVG_Entity_IsVisible(self, trail[marker])) {
        return trail[marker];
    }

    if ( SVG_Entity_IsVisible(self, trail[PREV(marker)])) {
        return trail[PREV(marker)];
    }

    return trail[marker];
}

svg_base_edict_t *PlayerTrail_PickNext(svg_base_edict_t *self)
{
    int32_t     marker = 0;
    int32_t     n = 0;

    if (!trail_active)
        return NULL;

    // WID: TODO: Monster Reimplement.
    for (marker = trail_head, n = TRAIL_LENGTH; n; n--) {
        if (trail[marker]->timestamp <= self->trail_time)
            marker = NEXT(marker);
        else
            break;
    }

    return trail[marker];
}

svg_base_edict_t *PlayerTrail_LastSpot(void)
{
    return trail[PREV(trail_head)];
}
#else // !ENABLE_PLAYER_TRAIL_ENTITIES
#define TRAIL_LENGTH    128

svg_base_edict_t *trail[ TRAIL_LENGTH ];
int         trail_head;
bool        trail_active = false;

#define NEXT(n)     (((n) + 1) & (TRAIL_LENGTH - 1))
#define PREV(n)     (((n) - 1) & (TRAIL_LENGTH - 1))
void PlayerTrail_Init( void ) {
	// Nothing to do.
}
void PlayerTrail_Add( vec3_t spot ) {
	// Nothing to do.
}
void PlayerTrail_New( vec3_t spot ) {
	// Nothing to do.
}
svg_base_edict_t *PlayerTrail_PickFirst( svg_base_edict_t *self ) {
	// Nothing to do.
	return nullptr;
}

#endif // ENABLE_PLAYER_TRAIL_ENTITIES