/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2019, NVIDIA CORPORATION. All rights reserved.

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



/***
*
*
*
*   SelectSpawnPoint:
*
*
*
***/
/**
*   @brief  Determines the client that is most near to the entity, 
*           and returns its length for ( ent->origin - client->origin ).
**/
const float SVG_Player_DistanceToEntity( svg_base_edict_t *ent ) {
    // The best distance will always be flt_max.
    float bestDistance = CM_MAX_WORLD_SIZE + 1.f;

    for ( int32_t n = 1; n <= maxclients->value; n++ ) {
        // Get client.
        svg_base_edict_t *clientEntity = g_edict_pool.EdictForNumber( n );//&g_edicts[ n ];
        // Ensure is active and alive player.
        if ( !SVG_Entity_IsClient( clientEntity, true ) ) {
            continue;
        }

        // Calculate distance.
        Vector3 distance = Vector3( ent->s.origin ) - clientEntity->s.origin;
        const float distanceLength = QM_Vector3Length( distance );

        // Assign as best distance if nearer to ent.
        if ( distanceLength < bestDistance ) {
            bestDistance = distanceLength;
        }
    }

    // Return result.
    return bestDistance;
}