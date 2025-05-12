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
#include "svgame/svg_clients.h"
#include "svgame/svg_utils.h"

#include "svgame/player/svg_player_client.h"
#include "svgame/player/svg_player_hud.h"
#include "svgame/player/svg_player_trail.h"

#include "svg_save.h"

#include "svg_lua.h"



/**
*
*   Entity Spawn (Debug-) Configurations:
*
**/
//! Whether to exit early from ED_Spawn in case an entity has the reserved "noclass" or "freed" set as its classname.
#define ENTITY_SPAWN_EXIT_EARLY_ON_RESERVED_NAMES 1 // Uncomment to disable exiting early on reserved names.
//! Whether to properly debug output the collision model entity key/value iteration process:
//#define DEBUG_ENTITY_SPAWN_PROCESS 1 // Uncomment to enable.
//! Whether to show what data types the key/value pair's value got parsed for:
//#define DEBUG_ENTITY_SPAWN_PROCESS_SHOW_PARSED_FOR_FIELD_TYPES 1 // Uncomment to enable.

#if 0
/**
*   @brief  Stores the entity spawn functon pointer by hooking it up with a string name used as identifier.
**/
typedef struct {
	// WID: C++20: added const.
	const char    *name;
    void (*spawn)(svg_base_edict_t *ent);
} spawn_func_t;

/**
*   @brief  Stores the offset and expected parsed type of an entity's member variable by hooking it 
*           up with a string name used as identifier.
**/
typedef struct {
	// WID: C++20: added const.
    const char    *name;
    unsigned ofs;
    fieldtype_t type;
} spawn_field_t;


/**
*
* 
*   Function Declarations for all entity Spawn functions, required for the C/C++ linker.
* 
* 
**/
// <Q2RTXP>:
void SP_monster_testdummy_puppet( svg_base_edict_t *self );
// </Q2RTXP>

void SP_item_health(svg_base_edict_t *self);
void SP_item_health_small(svg_base_edict_t *self);
void SP_item_health_large(svg_base_edict_t *self);
void SP_item_health_mega(svg_base_edict_t *self);

//void SP_info_player_start(svg_base_edict_t *ent);
//void SP_info_player_deathmatch(svg_base_edict_t *ent);
//void SP_info_player_coop(svg_base_edict_t *ent);
//void SP_info_player_intermission(svg_base_edict_t *ent);

void SP_func_plat(svg_base_edict_t *ent);
void SP_func_rotating(svg_base_edict_t *ent);
void SP_func_button(svg_base_edict_t *ent);
void SP_func_door(svg_base_edict_t *ent);
#if 0
void SP_func_door_secret(svg_base_edict_t *ent);
#endif
void SP_func_door_rotating(svg_base_edict_t *ent);
void SP_func_water(svg_base_edict_t *ent);
void SP_func_train(svg_base_edict_t *ent);
void SP_func_conveyor(svg_base_edict_t *self);
void SP_func_wall(svg_base_edict_t *self);
void SP_func_object(svg_base_edict_t *self);
void SP_func_breakable(svg_base_edict_t *self);
void SP_func_timer(svg_base_edict_t *self);
void SP_func_areaportal(svg_base_edict_t *ent);
//void SP_func_clock(svg_base_edict_t *ent);
void SP_func_killbox(svg_base_edict_t *ent);

void SP_trigger_always(svg_base_edict_t *ent);
void SP_trigger_once(svg_base_edict_t *ent);
void SP_trigger_multiple(svg_base_edict_t *ent);
void SP_trigger_relay(svg_base_edict_t *ent);
void SP_trigger_push(svg_base_edict_t *ent);
void SP_trigger_hurt(svg_base_edict_t *ent);
void SP_trigger_counter(svg_base_edict_t *ent);
void SP_trigger_elevator(svg_base_edict_t *ent);
void SP_trigger_gravity(svg_base_edict_t *ent);

void SP_target_temp_entity(svg_base_edict_t *ent);
void SP_target_speaker(svg_base_edict_t *ent);
void SP_target_explosion(svg_base_edict_t *ent);
void SP_target_changelevel(svg_base_edict_t *ent);
void SP_target_secret(svg_base_edict_t *ent);
void SP_target_goal(svg_base_edict_t *ent);
void SP_target_splash(svg_base_edict_t *ent);
void SP_target_spawner(svg_base_edict_t *ent);
void SP_target_blaster(svg_base_edict_t *ent);
void SP_target_crosslevel_trigger(svg_base_edict_t *ent);
void SP_target_crosslevel_target(svg_base_edict_t *ent);
void SP_target_laser(svg_base_edict_t *self);
void SP_target_lightramp(svg_base_edict_t *self);
void SP_target_earthquake(svg_base_edict_t *ent);

//void SP_worldspawn(svg_base_edict_t *ent);

void SP_spotlight(svg_base_edict_t *self);
void SP_light(svg_base_edict_t *self);
//void SP_info_null(svg_base_edict_t *self);
//void SP_info_notnull(svg_base_edict_t *self);
void SP_path_corner(svg_base_edict_t *self);

//void SP_misc_explobox(svg_base_edict_t *self);
void SP_misc_gib_arm(svg_base_edict_t *self);
void SP_misc_gib_leg(svg_base_edict_t *self);
void SP_misc_gib_head(svg_base_edict_t *self);
void SP_misc_teleporter(svg_base_edict_t *self);
void SP_misc_teleporter_dest(svg_base_edict_t *self);


/**
*   @brief  Hooks up the entity classnames with their corresponding spawn functions.
**/
static const spawn_func_t spawn_funcs[] = {
    // <Q2RTXP>
    //{ "monster_testdummy_puppet", SP_monster_testdummy_puppet },
    // </Q2RTXP>
    {"item_health", SP_item_health},
    {"item_health_small", SP_item_health_small},
    {"item_health_large", SP_item_health_large},
    {"item_health_mega", SP_item_health_mega},

    // [X] Converted: {"info_player_start", SP_info_player_start},
    // [X] Converted: {"info_player_deathmatch", SP_info_player_deathmatch},
    // [X] Converted: {"info_player_coop", SP_info_player_coop},
    // [X] Converted: {"info_player_intermission", SP_info_player_intermission},

    {"func_plat", SP_func_plat},
    {"func_button", SP_func_button},
    {"func_door", SP_func_door},
    #if 0
    {"func_door_secret", SP_func_door_secret},
    #endif
    {"func_door_rotating", SP_func_door_rotating},
    {"func_rotating", SP_func_rotating},
    {"func_train", SP_func_train},
    {"func_water", SP_func_water},
    {"func_conveyor", SP_func_conveyor},
    {"func_areaportal", SP_func_areaportal},
    {"func_wall", SP_func_wall},
    {"func_object", SP_func_object},
    {"func_timer", SP_func_timer},
    {"func_breakable", SP_func_breakable},
    {"func_killbox", SP_func_killbox},

    {"trigger_always", SP_trigger_always},
    {"trigger_once", SP_trigger_once},
    {"trigger_multiple", SP_trigger_multiple},
    {"trigger_relay", SP_trigger_relay},
    {"trigger_push", SP_trigger_push},
    {"trigger_hurt", SP_trigger_hurt},
    {"trigger_counter", SP_trigger_counter},
    {"trigger_elevator", SP_trigger_elevator},
    {"trigger_gravity", SP_trigger_gravity},

    {"target_temp_entity", SP_target_temp_entity},
    {"target_speaker", SP_target_speaker},
    {"target_explosion", SP_target_explosion},
    {"target_changelevel", SP_target_changelevel},
    {"target_secret", SP_target_secret},
    {"target_goal", SP_target_goal},
    {"target_splash", SP_target_splash},
    {"target_spawner", SP_target_spawner},
    {"target_blaster", SP_target_blaster},
    {"target_crosslevel_trigger", SP_target_crosslevel_trigger},
    {"target_crosslevel_target", SP_target_crosslevel_target},
    {"target_laser", SP_target_laser},
    {"target_lightramp", SP_target_lightramp},
    {"target_earthquake", SP_target_earthquake},

    //{"worldspawn", SP_worldspawn},

	{"spotlight", SP_spotlight},
    {"light", SP_light},
    // [X] Converted: {"info_null", SP_info_null},
    // [X] Converted: {"func_group", SP_info_null},
    // [X] Converted: {"info_notnull", SP_info_notnull},
    {"path_corner", SP_path_corner},

    //{"misc_explobox", SP_misc_explobox},
    #if 0
    {"misc_gib_arm", SP_misc_gib_arm},
    {"misc_gib_leg", SP_misc_gib_leg},
    {"misc_gib_head", SP_misc_gib_head},
    #endif
    {"misc_teleporter", SP_misc_teleporter},
    {"misc_teleporter_dest", SP_misc_teleporter_dest},

    {NULL, NULL}
};
#endif // #if 0

/**
*   @brief  Chain together all entities with a matching team field.
*
*           All but the first will have the FL_TEAMSLAVE flag set.
*           All but the last will have the teamchain field set to the next one
**/
void SVG_FindTeams( void ) {
    svg_base_edict_t *e, *e2, *chain;
    int     i, j;
    int     c, c2;

    c = 0;
    c2 = 0;
    for ( i = 1, e = g_edict_pool.EdictForNumber( i ); i < globals.edictPool->num_edicts; i++, e = g_edict_pool.EdictForNumber( i ) ) {
        if ( !e ) {
            continue;
        }
        if ( !e->inuse )
            continue;
        if ( !e->targetNames.team )
            continue;
        if ( e->flags & FL_TEAMSLAVE )
            continue;
        chain = e;
        e->teammaster = e;
        c++;
        c2++;
        for ( j = i + 1, e2 = g_edict_pool.EdictForNumber( j ); j < globals.edictPool->num_edicts; j++, e2 = g_edict_pool.EdictForNumber( j ) ) {
            if ( !e2 ) {
                continue;
            }
            if ( !e2->inuse )
                continue;
            if ( !e2->targetNames.team )
                continue;
            if ( e2->flags & FL_TEAMSLAVE )
                continue;
            if ( !strcmp( (const char *)e->targetNames.team, (const char *)e2->targetNames.team ) ) {
                c2++;
                chain->teamchain = e2;
                e2->teammaster = e;
                chain = e2;
                e2->flags = static_cast<entity_flags_t>( e2->flags | FL_TEAMSLAVE );
            }
        }
    }

    gi.dprintf( "%i teams with %i entities\n", c, c2 );
}

/**
*   @brief  Find and set the 'movewith' parent entity for entities that have this key set.
**/
void SVG_MoveWith_FindParentTargetEntities( void ) {
    svg_base_edict_t *ent = g_edict_pool.EdictForNumber( 0 );
    int32_t moveParents  = 0;
    int32_t i = 1;
    for ( i = 1, ent = g_edict_pool.EdictForNumber( i ); i < globals.edictPool->num_edicts; i++, ent = g_edict_pool.EdictForNumber( i ) ) {
        // It has to be in-use.
        //if ( !ent->inuse ) {
        //    continue;
        //}

        // Not having any parent.
        if ( !ent->targetNames.movewith ) {
            continue;
        }
        // Already set, so skip it.
        if ( ent->targetEntities.movewith ) {
            continue;
        }

        // Fetch 'parent' target entity.
        svg_base_edict_t *parentMover = SVG_Entities_Find( NULL, q_offsetof( svg_base_edict_t, targetname ), (const char *)ent->targetNames.movewith.ptr );
        // Apply.
        if ( parentMover ) {
            // Set.
            SVG_MoveWith_SetTargetParentEntity( (const char *)ent->targetNames.movewith.ptr, parentMover, ent );
            // Increment.
            game.num_movewithEntityStates++;
        }
    }

    // Debug:
    gi.dprintf( "total entities moving with parent entities(total #%i).\n", moveParents );
}

/**
*   @brief  Allocates in Tag(TAG_SVGAME_LEVEL) a proper string for string entity fields.
*           The string itself is freed at disconnecting/map changes causing TAG_SVGAME_LEVEL memory to be cleared.
**/
char *ED_NewString(const char *string)
{
    char    *newb, *new_p;
    int     i, l;

    l = strlen(string) + 1;

	// WID: C++20: Addec cast.
    newb = (char*)gi.TagMallocz(l, TAG_SVGAME_LEVEL);

    new_p = newb;

    for (i = 0 ; i < l ; i++) {
        if (string[i] == '\\' && i < l - 1) {
            i++;
            if (string[i] == 'n')
                *new_p++ = '\n';
            else
                *new_p++ = '\\';
        } else
            *new_p++ = string[i];
    }

    return newb;
}

/*
Goal:
    - When the game loads a new map and SVG_SpawnEntities is called, we want to be able
	to allocate the appropriate type of entity based on the classname cm_entity_t key/value pair.
    The appropriate entity type to allocate is determined by looking into the TypeInfo system
	for a cm_entity_t's classname key matching the allocator function to use.
    - After that we want to reiterate the cm_entity_t key/value pairs and pass them to the edict's
	SpawnKey function. This allows us to set the edict's fields based on the cm_entity_t key/value pairs.
*/
/**
*   @brief  Loads in the map name and iterates over the collision model's parsed key/value
*           pairs in order to spawn entities appropriately matching the pair dictionary fields
*           of each entity.
**/
void SVG_SpawnEntities( const char *mapname, const char *spawnpoint, const cm_entity_t **entities, const int32_t numEntities ) {
    // Acquire the 'skill' level cvar value in order to exlude various entities for various
    // skill levels.
    float skill_level = floor( skill->value );
    skill_level = std::clamp( skill_level, 0.f, 3.f );

    // If it is an out of bounds cvar, force set skill level within the clamped bounds.
    if ( skill->value != skill_level ) {
        gi.cvar_forceset( "skill", va( "%f", skill_level ) );
    }

    // If we were running a previous session, make sure to save the session's client data before cleaning all level memory.
    SVG_Player_SaveClientData();

    // Free up all SVGAME_LEVEL tag memory.
    gi.FreeTags( TAG_SVGAME_LEVEL );

    // Zero out all level struct data.
    level = {};

    // Store cm_entity_t pointer.
    level.cm_entities = entities;

    if ( g_edicts[ 0 ] ) {
        // Reset all entities to 'baseline'.
        for ( int32_t i = 0; i < game.maxentities; i++ ) {
            if ( g_edicts[ i ] ) {
                // Store original number.
                const int32_t number = g_edicts[ i ]->s.number;
                // Zero out.
                //*g_edicts[ i ] = { }; //memset( g_edicts, 0, game.maxentities * sizeof( g_edicts[ 0 ] ) );
                // Reset the entity to base state.
                g_edicts[ i ]->Reset( g_edicts[ i ]->entityDictionary );
                // Retain the entity's original number.
                g_edicts[ i ]->s.number = number;
            }
        }
    }
    // (Re-)Initialize the edict pool, and store a pointer to its edicts array in g_edicts.
    //g_edicts = SVG_EdictPool_Allocate( &g_edict_pool, game.maxentities );
    // Set the number of edicts to the maxclients + 1 (Encounting for the world at slot #0).
    //g_edict_pool.num_edicts = game.maxclients + 1;

    // Initialize a fresh clients array.
    //game.clients = SVG_Clients_Reallocate( game.maxclients );

    // Copy over the mapname and the spawn point. (Optionally set by appending a map name with a $spawntarget)
    Q_strlcpy( level.mapname, mapname, sizeof( level.mapname ) );
    Q_strlcpy( game.spawnpoint, spawnpoint, sizeof( game.spawnpoint ) );

    // Set client fields on player entities.
    for ( int32_t i = 0; i < game.maxclients; i++ ) {
        // Assign this entity to the designated client.
        //g_edicts[ i + 1 ]->client = game.clients + i;
        svg_base_edict_t *ent = g_edict_pool.EdictForNumber( i + 1 );
        ent->client = &game.clients[ i ];

        // Assign client number.
        ent->client->clientNum = i;

        // Set their states as disconnected, unspawned, since the level is switching.
        game.clients[ i ].pers.connected = false;
        game.clients[ i ].pers.spawned = false;
    }

    //! Keep score of the total 'inhibited' entities. (Those that got removed for distinct reasons.)
    int32_t numInhibitedEntities = 0;

    // Keep track of the internal entityID.
    int32_t entityID = 0;

    // Pointer to the first key/value pair entry of a collision model's entities.
    const cm_entity_t *cm_entity = nullptr;

    // Pointer to the most recent allocated edict to spawn.
    svg_base_edict_t *spawnEdict = nullptr;

    // Iterate over the number of collision model entity entries.
    for ( size_t i = 0; i < numEntities; i++ ) {
        // Pointer to the worldspawn edict in first instance, after that the first free entity
        // we can acquire.
        //spawnEdict = ( !spawnEdict ? g_edict_pool.EdictForNumber( 0 ) /* worldspawn */ : g_edict_pool.AllocateNextFreeEdict<svg_base_edict_t>() );

        // TypeInfo for this entity.
        EdictTypeInfo *typeInfo = nullptr;

        // Get the incremental index entity.
        cm_entity = entities[ i ];

        // Call spawn on edicts that got a positive entityID. Worldspawn == entityID(#0).
        if ( entityID == 0 ) {
            // Get the type info for this entity.
            typeInfo = EdictTypeInfo::GetInfoByWorldSpawnClassName( "worldspawn" );
            // Worldspawn:
            g_edict_pool.edicts[ 0 ] = typeInfo->allocateEdictInstanceCallback( cm_entity );
            // Set the worldspawn entityID.
            g_edict_pool.edicts[ 0 ]->s.number = 0;
            // Don't forget to increment num_edicts. Do so before spawning, since the worldspawn
            // will actually pre-allocate entities for the dead client player's body queue.
            g_edict_pool.num_edicts++;
            // Spawn the worldspawn entity.
            g_edict_pool.edicts[ 0 ]->DispatchSpawnCallback();
            // Continue to next entity.
            entityID++;
            continue;
        }

        // First seek out its classname so we can get the TypeInfo needed to allocate the proper svg_base_edict_t derivative.
        const cm_entity_t *classnameKv = cm_entity;

        while ( classnameKv ) {
            // If we have a matching key, then we can spawn the entity.
            if ( strcmp( classnameKv->key, "classname" ) == 0 ) {
                // Get the type info for this entity.
                typeInfo = EdictTypeInfo::GetInfoByWorldSpawnClassName( classnameKv->string );
				// If we can't find its classname linked in the TypeInfo list, we'll resort to the default
				// svg_base_edict_t type instead.
                // If we don't have a type info, then we can't spawn it.
                if ( !typeInfo ) {
                    typeInfo = EdictTypeInfo::GetInfoByWorldSpawnClassName( "svg_base_edict_t" );
                    gi.dprintf( "No type info for entity(%s)!\n", classnameKv->string );
                }
                // Allocate the edict instance.
                if ( typeInfo ) {
                    spawnEdict = typeInfo->allocateEdictInstanceCallback( cm_entity );
                // This should never happen, but still.
                } else {
                    spawnEdict = nullptr;
                    gi.dprintf( "No failed to find TypeInfo for \"svg_base_edict_t\", entity(% s)!\n", classnameKv->string );
                }
                break;
            }
            // If the key is not 'classname', then we need to keep looking for it.
            // Seek out the next key/value pair for one that has the matching key 'classname'.
            classnameKv = classnameKv->next;
        };

        // If classnameKv == nullptr we never found any. Error out.
        if ( classnameKv == nullptr ) {
			gi.error( "%s: %s: No classname key/value pair found for entity(%d)!\n", __func__, mapname, i );
			return;
        }
		// If we don't have a spawn edict, then we can't spawn it.
        if ( !spawnEdict ) {
            numInhibitedEntities++;
			gi.dprintf( "%s: %s: No spawn edict found for entity(%d)!\n", __func__, mapname, i );
			continue;
        }

        // Iterate and process the remaining key/values now that we allocated the entity type class instance.
        const cm_entity_t *kv = cm_entity;
        while ( kv ) {
            // For possible Key/Value errors/warnings.
            std::string errorStr = "";
            // Give the edict a chance to process and assign spawn key/value.
            const bool processedKv = spawnEdict->KeyValue( kv, errorStr );
            // Print errorStr.
			if ( !errorStr.empty() ) {
                // Print the error string.
                gi.dprintf( "%s: KeyValue error. classname(%s) error: %s\n", __func__, spawnEdict->classname, errorStr.c_str() );
			}
            // Or generic unknown what went wrong error instead.
			else if ( !processedKv ) {
				// Print the error string.
				gi.dprintf( "%s: KeyValue error for entity(%d) Key(%s), Value(%s).\n",
                    __func__, spawnEdict->s.number,
					kv->key, kv->string );
            }
            // Get the next key/value pair of this entity.
            kv = kv->next;
        }

        // Emplace the spawned edict in the next avaible edict slot.
        g_edict_pool.EmplaceNextFreeEdict( spawnEdict );
		// Set the entityID.
        spawnEdict->DispatchSpawnCallback();

        // Increment entityID.
        entityID++;
    }

    // Give entities a chance to 'post spawn'.
    int32_t numPostSpawnedEntities = 0;
    for ( int32_t i = 0; i < globals.edictPool->num_edicts; i++ ) {
        svg_base_edict_t *ent = g_edict_pool.EdictForNumber( i );

        if ( ent && ent->HasPostSpawnCallback() ) {
            ent->DispatchPostSpawnCallback();
            numPostSpawnedEntities++;
        }
    }

    // Developer print the inhibited entities.
    gi.dprintf( "%i entities inhibited\n", numInhibitedEntities );
    // Developer print the post spawned entities.
    gi.dprintf( "%i entities post spawned\n", numPostSpawnedEntities );

    #ifdef DEBUG
    i = 1;
    ent = EDICT_FOR_NUMBER( i );
    while ( i < globals.edictPool.num_edicts ) {
        if ( ent->inuse != 0 || ent->inuse != 1 )
            Com_DPrintf( "Invalid entity %d\n", i );
        i++, ent++;
    }
    #endif

    // Find entity 'teams', NOTE: these are not actual player game teams.
    SVG_FindTeams();

    // Find all entities that are following a parent's movement.
    SVG_MoveWith_FindParentTargetEntities();

    // Initialize player trail.
    PlayerTrail_Init();

    // Load up and initialize the LUA map script. (If any.)
    SVG_Lua_LoadMapScript( mapname );

    // Give Lua a chance to precache media.
    SVG_Lua_CallBack_OnPrecacheMedia();
}