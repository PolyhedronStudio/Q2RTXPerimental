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
const float SVG_Client_DistanceToEntity( svg_base_edict_t *ent ) {
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

/**
*   @brief  Go to a random point, but NOT the two points closest to other players.
**/
svg_base_edict_t *SelectRandomDeathmatchSpawnPoint( void ) {
    svg_base_edict_t *spot, *spot1, *spot2;
    int     count = 0;
    int     selection;
    float   range, range1, range2;

    spot = NULL;
    range1 = range2 = 99999;
    spot1 = spot2 = NULL;

    while ( ( spot = SVG_Entities_Find( spot, q_offsetof( svg_base_edict_t, classname ), "info_player_deathmatch" ) ) != NULL ) {
        count++;
        range = SVG_Client_DistanceToEntity( spot );
        if ( range < range1 ) {
            range1 = range;
            spot1 = spot;
        } else if ( range < range2 ) {
            range2 = range;
            spot2 = spot;
        }
    }

    if ( !count )
        return NULL;

    if ( count <= 2 ) {
        spot1 = spot2 = NULL;
    } else
        count -= 2;

    selection = Q_rand_uniform( count );

    spot = NULL;
    do {
        spot = SVG_Entities_Find( spot, q_offsetof( svg_base_edict_t, classname ), "info_player_deathmatch" );
        if ( spot == spot1 || spot == spot2 )
            selection++;
    } while ( selection-- );

    return spot;
}

/**
*   @brief
**/
static svg_base_edict_t *SelectFarthestDeathmatchSpawnPoint( void ) {
    svg_base_edict_t *bestspot;
    float   bestdistance, bestplayerdistance;
    svg_base_edict_t *spot;


    spot = NULL;
    bestspot = NULL;
    bestdistance = 0;
    while ( ( spot = SVG_Entities_Find( spot, q_offsetof( svg_base_edict_t, classname ), "info_player_deathmatch" ) ) != NULL ) {
        bestplayerdistance = SVG_Client_DistanceToEntity( spot );

        if ( bestplayerdistance > bestdistance ) {
            bestspot = spot;
            bestdistance = bestplayerdistance;
        }
    }

    if ( bestspot ) {
        return bestspot;
    }

    // if there is a player just spawned on each and every start spot
    // we have no choice to turn one into a telefrag meltdown
    spot = SVG_Entities_Find( NULL, q_offsetof( svg_base_edict_t, classname ), "info_player_deathmatch" );

    return spot;
}

/**
*   @brief
**/
static svg_base_edict_t *SelectDeathmatchSpawnPoint( void ) {
    if ( (int)( dmflags->value ) & DF_SPAWN_FARTHEST )
        return SelectFarthestDeathmatchSpawnPoint();
    else
        return SelectRandomDeathmatchSpawnPoint();
}

/**
*   @brief  
**/
static svg_base_edict_t *SelectCoopSpawnPoint( svg_base_edict_t *ent ) {
    int     index;
    svg_base_edict_t *spot = NULL;
    // WID: C++20: Added const.
    const char *target;

    index = ent->client - game.clients;

    // player 0 starts in normal player spawn point
    if ( !index )
        return NULL;

    spot = NULL;

    // assume there are four coop spots at each spawnpoint
    while ( 1 ) {
        spot = SVG_Entities_Find( spot, q_offsetof( svg_base_edict_t, classname ), "info_player_coop" );
        if ( !spot )
            return NULL;    // we didn't have enough...

        target = (const char *)spot->targetname;
        if ( !target )
            target = "";
        if ( Q_stricmp( game.spawnpoint, target ) == 0 ) {
            // this is a coop spawn point for one of the clients here
            index--;
            if ( !index )
                return spot;        // this is it
        }
    }


    return spot;
}


/**
*   @brief  Will select the player's spawn point entity. First it seeks out
*           a spawn with a matching targetname and game.spawnpoint.
* 
*           If it can't find that, it'll take 'info_player_start' classname entity.
* 
*           If that fails, errors out.
**/
void SVG_Player_SelectSpawnPoint( svg_base_edict_t *ent, Vector3 &origin, Vector3 &angles ) {
    svg_base_edict_t *spot = NULL;

    if ( deathmatch->value ) {
        spot = SelectDeathmatchSpawnPoint();
    } else if ( coop->value ) {
        spot = SelectCoopSpawnPoint( ent );
    }

    // Find a single player start spot since the game modes found none.
    if ( !spot ) {
        // Iterate for info_player_start that matches the game.spawnpoint targetname to spawn at..
        while ( ( spot = SVG_Entities_Find( spot, q_offsetof( svg_base_edict_t, classname ), "info_player_start" ) ) != NULL ) {
            // Break out if the string data is invalid.
            if ( !game.spawnpoint[ 0 ] && !(const char *)spot->targetname )
                break;
            if ( !game.spawnpoint[ 0 ] || !(const char *)spot->targetname )
                continue;

            // Break out in case we found the game.spawnpoint matching info_player_start 'targetname'.
            if ( Q_stricmp( game.spawnpoint, (const char *)spot->targetname ) == 0 )
                break;
        }

        // Couldn't find any designated spawn points, pick one with just a matching classname instead.
        if ( !spot ) {
            if ( !game.spawnpoint[ 0 ] ) {
                // there wasn't a spawnpoint without a target, so use any
                spot = SVG_Entities_Find( spot, q_offsetof( svg_base_edict_t, classname ), "info_player_start" );
            }
            if ( !spot )
                gi.error( "Couldn't find spawn point %s", game.spawnpoint );
        }
    }

    origin = spot->s.origin;
    angles = spot->s.angles;
}