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

#include "g_local.h"
#include "g_save.h"

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


/**
*   @brief  Stores the entity spawn functon pointer by hooking it up with a string name used as identifier.
**/
typedef struct {
	// WID: C++20: added const.
	const char    *name;
    void (*spawn)(edict_t *ent);
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
void SP_monster_testdummy_puppet( edict_t *self );
// </Q2RTXP>

void SP_item_health(edict_t *self);
void SP_item_health_small(edict_t *self);
void SP_item_health_large(edict_t *self);
void SP_item_health_mega(edict_t *self);

void SP_info_player_start(edict_t *ent);
void SP_info_player_deathmatch(edict_t *ent);
void SP_info_player_coop(edict_t *ent);
void SP_info_player_intermission(edict_t *ent);

void SP_func_plat(edict_t *ent);
void SP_func_rotating(edict_t *ent);
void SP_func_button(edict_t *ent);
void SP_func_door(edict_t *ent);
void SP_func_door_secret(edict_t *ent);
void SP_func_door_rotating(edict_t *ent);
void SP_func_water(edict_t *ent);
void SP_func_train(edict_t *ent);
void SP_func_conveyor(edict_t *self);
void SP_func_wall(edict_t *self);
void SP_func_object(edict_t *self);
void SP_func_explosive(edict_t *self);
void SP_func_timer(edict_t *self);
void SP_func_areaportal(edict_t *ent);
void SP_func_clock(edict_t *ent);
void SP_func_killbox(edict_t *ent);

void SP_trigger_always(edict_t *ent);
void SP_trigger_once(edict_t *ent);
void SP_trigger_multiple(edict_t *ent);
void SP_trigger_relay(edict_t *ent);
void SP_trigger_push(edict_t *ent);
void SP_trigger_hurt(edict_t *ent);
void SP_trigger_counter(edict_t *ent);
void SP_trigger_elevator(edict_t *ent);
void SP_trigger_gravity(edict_t *ent);
void SP_trigger_monsterjump(edict_t *ent);

void SP_target_temp_entity(edict_t *ent);
void SP_target_speaker(edict_t *ent);
void SP_target_explosion(edict_t *ent);
void SP_target_changelevel(edict_t *ent);
void SP_target_secret(edict_t *ent);
void SP_target_goal(edict_t *ent);
void SP_target_splash(edict_t *ent);
void SP_target_spawner(edict_t *ent);
void SP_target_blaster(edict_t *ent);
void SP_target_crosslevel_trigger(edict_t *ent);
void SP_target_crosslevel_target(edict_t *ent);
void SP_target_laser(edict_t *self);
void SP_target_actor(edict_t *ent);
void SP_target_lightramp(edict_t *self);
void SP_target_earthquake(edict_t *ent);
void SP_target_character(edict_t *ent);
void SP_target_string(edict_t *ent);

void SP_worldspawn(edict_t *ent);
void SP_viewthing(edict_t *ent);

void SP_spotlight(edict_t *self);
void SP_light(edict_t *self);
void SP_light_mine1(edict_t *ent);
void SP_light_mine2(edict_t *ent);
void SP_info_null(edict_t *self);
void SP_info_notnull(edict_t *self);
void SP_path_corner(edict_t *self);
void SP_point_combat(edict_t *self);

void SP_misc_explobox(edict_t *self);
void SP_misc_actor(edict_t *self);
void SP_misc_gib_arm(edict_t *self);
void SP_misc_gib_leg(edict_t *self);
void SP_misc_gib_head(edict_t *self);
void SP_misc_deadsoldier(edict_t *self);
void SP_misc_teleporter(edict_t *self);
void SP_misc_teleporter_dest(edict_t *self);

void SP_monster_infantry(edict_t *self);
void SP_monster_soldier_light(edict_t *self);
void SP_monster_soldier(edict_t *self);
void SP_monster_soldier_ss(edict_t *self);

void SP_turret_breach(edict_t *self);
void SP_turret_base(edict_t *self);
void SP_turret_driver(edict_t *self);

/**
*   @brief  Hooks up the entity classnames with their corresponding spawn functions.
**/
static const spawn_func_t spawn_funcs[] = {
    // <Q2RTXP>
    { "monster_testdummy_puppet", SP_monster_testdummy_puppet },
    // </Q2RTXP>
    {"item_health", SP_item_health},
    {"item_health_small", SP_item_health_small},
    {"item_health_large", SP_item_health_large},
    {"item_health_mega", SP_item_health_mega},

    {"info_player_start", SP_info_player_start},
    {"info_player_deathmatch", SP_info_player_deathmatch},
    {"info_player_coop", SP_info_player_coop},
    {"info_player_intermission", SP_info_player_intermission},

    {"func_plat", SP_func_plat},
    {"func_button", SP_func_button},
    {"func_door", SP_func_door},
    {"func_door_secret", SP_func_door_secret},
    {"func_door_rotating", SP_func_door_rotating},
    {"func_rotating", SP_func_rotating},
    {"func_train", SP_func_train},
    {"func_water", SP_func_water},
    {"func_conveyor", SP_func_conveyor},
    {"func_areaportal", SP_func_areaportal},
    {"func_clock", SP_func_clock},
    {"func_wall", SP_func_wall},
    {"func_object", SP_func_object},
    {"func_timer", SP_func_timer},
    {"func_explosive", SP_func_explosive},
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
    {"trigger_monsterjump", SP_trigger_monsterjump},

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
    {"target_actor", SP_target_actor},
    {"target_lightramp", SP_target_lightramp},
    {"target_earthquake", SP_target_earthquake},
    {"target_character", SP_target_character},
    {"target_string", SP_target_string},

    {"worldspawn", SP_worldspawn},
    {"viewthing", SP_viewthing},

	{"spotlight", SP_spotlight},
    {"light", SP_light},
    {"light_mine1", SP_light_mine1},
    {"light_mine2", SP_light_mine2},
    {"info_null", SP_info_null},
    {"func_group", SP_info_null},
    {"info_notnull", SP_info_notnull},
    {"path_corner", SP_path_corner},
    {"point_combat", SP_point_combat},

    {"misc_explobox", SP_misc_explobox},
    {"misc_actor", SP_misc_actor},
    {"misc_gib_arm", SP_misc_gib_arm},
    {"misc_gib_leg", SP_misc_gib_leg},
    {"misc_gib_head", SP_misc_gib_head},
    {"misc_deadsoldier", SP_misc_deadsoldier},
    {"misc_teleporter", SP_misc_teleporter},
    {"misc_teleporter_dest", SP_misc_teleporter_dest},

    {"monster_infantry", SP_monster_infantry},
    {"monster_soldier_light", SP_monster_soldier_light},
    {"monster_soldier", SP_monster_soldier},
    {"monster_soldier_ss", SP_monster_soldier_ss},

    {"turret_breach", SP_turret_breach},
    {"turret_base", SP_turret_base},
    {"turret_driver", SP_turret_driver},

    {NULL, NULL}
};

/**
*   @brief  Hooks up the offset of an entity's struct member to a string, and stores
*           what type of value it holds and what to parse the key/value pairs with.
**/
static const spawn_field_t spawn_fields[] = {
	// ET_GENERIC:
	{"classname", FOFS( classname ), F_LSTRING},
	{"model", FOFS( model ), F_LSTRING},
	{"spawnflags", FOFS( spawnflags ), F_INT},
	{"speed", FOFS( speed ), F_FLOAT},
	{"accel", FOFS( accel ), F_FLOAT},
	{"decel", FOFS( decel ), F_FLOAT},
	{"target", FOFS( target ), F_LSTRING},
	{"targetname", FOFS( targetname ), F_LSTRING},
	{"pathtarget", FOFS( pathtarget ), F_LSTRING},
	{"deathtarget", FOFS( deathtarget ), F_LSTRING},
	{"killtarget", FOFS( killtarget ), F_LSTRING},
	{"combattarget", FOFS( combattarget ), F_LSTRING},
	{"message", FOFS( message ), F_LSTRING},
	{"team", FOFS( team ), F_LSTRING},
	{"wait", FOFS( wait ), F_FLOAT},
	{"delay", FOFS( delay ), F_FLOAT},
	{"random", FOFS( random ), F_FLOAT},
	{"move_origin", FOFS( move_origin ), F_VECTOR},
	{"move_angles", FOFS( move_angles ), F_VECTOR},
	{"style", FOFS( style ), F_INT},
	{"customLightStyle", FOFS( customLightStyle ), F_LSTRING},
	{"count", FOFS( count ), F_INT},
	{"health", FOFS( health ), F_INT},
	{"sounds", FOFS( sounds ), F_INT},
	{"light", FOFS( light ), F_FLOAT},
	{"dmg", FOFS( dmg ), F_INT},
	{"mass", FOFS( mass ), F_INT},
	{"volume", FOFS( volume ), F_FLOAT},
	{"attenuation", FOFS( attenuation ), F_FLOAT},
	{"map", FOFS( map ), F_LSTRING},
	{"origin", FOFS( s.origin ), F_VECTOR},
	{"angles", FOFS( s.angles ), F_VECTOR},
	{"angle", FOFS( s.angles ), F_ANGLEHACK},

	{"rgb", FOFS( s.spotlight.rgb ), F_VECTOR},
	{"intensity", FOFS( s.spotlight.intensity ), F_FLOAT},
	{"angle_width", FOFS( s.spotlight.angle_width ), F_FLOAT},
	{"angle_falloff", FOFS( s.spotlight.angle_falloff ), F_FLOAT},

    {NULL}
};

/**
*   @brief  Stores the spawn_field_t types for temporary spawn fields. Cleared at the start of 
*           each iteration over the collision model entity key/value pairs.
*           
*           These are only valid at the time when ED_CallSpawn calls the entity's spawn function.
**/
// temp spawn vars -- only valid when the spawn function is called
static const spawn_field_t temp_fields[] = {
    {"lip", STOFS(lip), F_INT},
    {"distance", STOFS(distance), F_INT},
    {"height", STOFS(height), F_INT},
    {"noise", STOFS(noise), F_LSTRING},
    {"pausetime", STOFS(pausetime), F_FLOAT},
    {"item", STOFS(item), F_LSTRING},

    {"gravity", STOFS(gravity), F_LSTRING},
    {"sky", STOFS(sky), F_LSTRING},
    {"skyrotate", STOFS(skyrotate), F_FLOAT},
    {"skyautorotate", STOFS(skyautorotate), F_INT},
    {"skyaxis", STOFS(skyaxis), F_VECTOR},
    {"minyaw", STOFS(minyaw), F_FLOAT},
    {"maxyaw", STOFS(maxyaw), F_FLOAT},
    {"minpitch", STOFS(minpitch), F_FLOAT},
    {"maxpitch", STOFS(maxpitch), F_FLOAT},
    {"nextmap", STOFS(nextmap), F_LSTRING},
    {"musictrack", STOFS(musictrack), F_LSTRING},

    {NULL}
};


/**
*   @brief  Finds the spawn function for the entity and calls it
**/
void ED_CallSpawn(edict_t *ent)
{
    const spawn_func_t *s;
    gitem_t *item;
    int     i;

    if (!ent->classname) {
        gi.dprintf("%s: NULL classname\n", __func__ );
        return;
    }

    #ifdef ENTITY_SPAWN_EXIT_EARLY_ON_RESERVED_NAMES
    if ( strcmp( ent->classname, "noclass" ) == 0 ) {
        #ifdef DEBUG_ENTITY_SPAWN_PROCESS
        gi.dprintf( "%s: 'noclass' classname for entity(#%d).\n", __func__, ent->s.number );
        #endif
        return;
    }
    if ( strcmp( ent->classname, "freed" ) == 0 ) {
        #ifdef DEBUG_ENTITY_SPAWN_PROCESS
        gi.dprintf( "%s: 'freed' classname for entity(#%d).\n", __func__, ent->s.number );
        #endif
        return;
    }
    #endif

    // Check item spawn functions.
    for (i = 0, item = itemlist ; i < game.num_items ; i++, item++) {
        // Skip if the item's classname is empty.
        if (!item->classname)
            continue;
        // If the classnames are an equal match, defer to SpawnItem and exit.
        if (!strcmp(item->classname, ent->classname)) {
            // found it
            SpawnItem(ent, item);
            return;
        }
    }

    // Check normal entity spawn functions.
    for (s = spawn_funcs ; s->name ; s++) {
        // Matching classname.
        if (!strcmp(s->name, ent->classname)) {
            // Spawn entity.
            s->spawn(ent);
            // Exit.
            return;
        }
    }

    #ifdef DEBUG_ENTITY_SPAWN_PROCESS
    gi.dprintf("%s doesn't have a spawn function\n", ent->classname);
    #endif
}

/**
*   @brief  Chain together all entities with a matching team field.
*
*           All but the first will have the FL_TEAMSLAVE flag set.
*           All but the last will have the teamchain field set to the next one
**/
void G_FindTeams( void ) {
    edict_t *e, *e2, *chain;
    int     i, j;
    int     c, c2;

    c = 0;
    c2 = 0;
    for ( i = 1, e = g_edicts + i; i < globals.num_edicts; i++, e++ ) {
        if ( !e->inuse )
            continue;
        if ( !e->team )
            continue;
        if ( e->flags & FL_TEAMSLAVE )
            continue;
        chain = e;
        e->teammaster = e;
        c++;
        c2++;
        for ( j = i + 1, e2 = e + 1; j < globals.num_edicts; j++, e2++ ) {
            if ( !e2->inuse )
                continue;
            if ( !e2->team )
                continue;
            if ( e2->flags & FL_TEAMSLAVE )
                continue;
            if ( !strcmp( e->team, e2->team ) ) {
                c2++;
                chain->teamchain = e2;
                e2->teammaster = e;
                chain = e2;
                e2->flags = static_cast<ent_flags_t>( e2->flags | FL_TEAMSLAVE );
            }
        }
    }

    gi.dprintf( "%i teams with %i entities\n", c, c2 );
}

/**
*   @brief  Allocates in Tag(TAG_SVGAME_LEVEL) a proper string for string entity fields.
*           The string itself is freed at disconnecting/map changes causing TAG_SVGAME_LEVEL memory to be cleared.
**/
static char *ED_NewString(const char *string)
{
    char    *newb, *new_p;
    int     i, l;

    l = strlen(string) + 1;

	// WID: C++20: Addec cast.
    newb = (char*)gi.TagMalloc(l, TAG_SVGAME_LEVEL);

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

/**
*   @return A pointer to the spawn field 'spawn_field_t' that has a matching key. (nullptr) otherwise.
**/
static const spawn_field_t *GetSpawnFieldByKey( const char *key ) {
    const spawn_field_t *f = nullptr;

    for ( f = spawn_fields; f->name; f++ ) {
        if ( !Q_stricmp( f->name, key ) ) {
            return f;
        }
    }

    return nullptr;
}

/**
*   @return A pointer to the temporary spawn field 'spawn_field_t' that has a matching key. (nullptr) otherwise.
**/
static const spawn_field_t *GetTempFieldByKey( const char *key ) {
    const spawn_field_t *f = nullptr;

    for ( f = temp_fields; f->name; f++ ) {
        if ( !Q_stricmp( f->name, key ) ) {
            return f;
        }
    }

    return nullptr;
}

/**
*   @brief  Will look up the (writeAddress + fields offset address) in order to write write the 
*           key/value pair's value into the address specifically based on the type the field is 
*           made to hold.into that address
**/
static void WritePairToEdictKeyField( byte *writeAddress, const spawn_field_t *keyOffsetField, const cm_entity_t *kv ) {
    // Look up the kind of field type we're dealing with.
    switch ( keyOffsetField->type ) {
        case F_IGNORE:
            break;
        case F_INT:
            if ( kv->parsed_type & cm_entity_parsed_type_t::ENTITY_INTEGER ) {
                *(int *)( writeAddress + keyOffsetField->ofs ) = kv->integer;
            } else {
                gi.dprintf( "%s: couldn't parse field '%s' as integer type.\n", __func__, kv->key );
            }
            break;
        case F_FLOAT:
            if ( kv->parsed_type & cm_entity_parsed_type_t::ENTITY_FLOAT ) {
                *(float *)( writeAddress + keyOffsetField->ofs ) = kv->value;
            } else {
                gi.dprintf( "%s: couldn't parse field '%s' as float type.\n", __func__, kv->key );
            }
            break;
        case F_ANGLEHACK:
            if ( kv->parsed_type & cm_entity_parsed_type_t::ENTITY_FLOAT ) {
                ( (float *)( writeAddress + keyOffsetField->ofs ) )[ 0 ] = 0;
                ( (float *)( writeAddress + keyOffsetField->ofs ) )[ 1 ] = kv->value;
                ( (float *)( writeAddress + keyOffsetField->ofs ) )[ 2 ] = 0;
            } else {
                gi.dprintf( "%s: couldn't parse field '%s' as yaw angle(float) type.\n", __func__, kv->key );
            }
            break;
        case F_LSTRING:
            *(char **)( writeAddress + keyOffsetField->ofs ) = ED_NewString( kv->string );
            break;
        case F_VECTOR:
            if ( kv->parsed_type & cm_entity_parsed_type_t::ENTITY_VECTOR4 ) {
                ( (float *)( writeAddress + keyOffsetField->ofs ) )[ 0 ] = kv->vec4[ 0 ];
                ( (float *)( writeAddress + keyOffsetField->ofs ) )[ 1 ] = kv->vec4[ 1 ];
                ( (float *)( writeAddress + keyOffsetField->ofs ) )[ 2 ] = kv->vec4[ 2 ];
                // TODO: We don't support vector4 yet, so just behave as if it were a Vector3 for now.
                //( (float *)( b + f->ofs ) )[ 3 ] = pair->vec4[ 3 ];
            } else if ( kv->parsed_type & cm_entity_parsed_type_t::ENTITY_VECTOR3 ) {
                ( (float *)( writeAddress + keyOffsetField->ofs ) )[ 0 ] = kv->vec3[ 0 ];
                ( (float *)( writeAddress + keyOffsetField->ofs ) )[ 1 ] = kv->vec3[ 1 ];
                ( (float *)( writeAddress + keyOffsetField->ofs ) )[ 2 ] = kv->vec3[ 2 ];
            } else if ( kv->parsed_type & cm_entity_parsed_type_t::ENTITY_VECTOR2 ) {
                ( (float *)( writeAddress + keyOffsetField->ofs ) )[ 0 ] = kv->vec2[ 0 ];
                ( (float *)( writeAddress + keyOffsetField->ofs ) )[ 1 ] = kv->vec2[ 1 ];
                ( (float *)( writeAddress + keyOffsetField->ofs ) )[ 2 ] = 0;
                //( (float *)( b + f->ofs ) )[ 2 ] = vec[ 2 ];
            } else {
                ( (float *)( writeAddress + keyOffsetField->ofs ) )[ 0 ] = 0;
                ( (float *)( writeAddress + keyOffsetField->ofs ) )[ 1 ] = 0;
                ( (float *)( writeAddress + keyOffsetField->ofs ) )[ 2 ] = 0;
                gi.dprintf( "%s: couldn't parse field '%s' as vector type.\n", __func__, kv->key );
            }
            break;
        default:
        //if ( edict ) {
        //    gi.dprintf( "Unhandled key/value pair(%s, %s) for cm_entity(%d) and edict(%d)\n", kv->key, kv->string, i, edict->s.number );
        //}
        break;
    }
}

/**
*   @brief  Loads in the map name and iterates over the collision model's parsed key/value
*           pairs in order to spawn entities appropriately matching the pair dictionary fields
*           of each entity.
**/
void SpawnEntities( const char *mapname, const char *spawnpoint, const cm_entity_t **entities, const int32_t numEntities ) {
    // Acquire the 'skill' level cvar value in order to exlude various entities for various
    // skill levels.
    float skill_level = floor( skill->value );
    skill_level = constclamp( skill_level, 0, 3 );
    
    // If it is an out of bounds cvar, force set skill level within the clamped bounds.
    if ( skill->value != skill_level ) {
        gi.cvar_forceset( "skill", va( "%f", skill_level ) );
    }

    // If we were running a previous session, make sure to save the session's client data.
    SaveClientData();

    // Free up all SVGAME_LEVEL tag memory.
    gi.FreeTags(TAG_SVGAME_LEVEL);

    // Zero out all level struct data as well as all the entities(edicts).
    memset( &level, 0, sizeof( level ) );
    memset( g_edicts, 0, game.maxentities * sizeof( g_edicts[ 0 ] ) );

    // Copy over the mapname and the spawn point. (Optionally set by appending a map name with a $spawntarget)
    Q_strlcpy( level.mapname, mapname, sizeof( level.mapname ) );
    Q_strlcpy( game.spawnpoint, spawnpoint, sizeof( game.spawnpoint ) );

    // Set client fields on player entities.
    for ( int32_t i = 0; i < game.maxclients; i++ ) {
        // Assign this entity to the designated client.
        g_edicts[ i + 1 ].client = game.clients + i;

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
    edict_t *spawnEdict = nullptr;

    // Iterate over the number of collision model entity entries.
    for ( size_t i = 0; i < numEntities; i++ ) {
        // For each entity we clear the temporary spawn fields.
        memset( &st, 0, sizeof( spawn_temp_t ) );

        // Pointer to the worldspawn edict in first instance, after that the first free entity
        // we can acquire.
        spawnEdict = ( !spawnEdict ? g_edicts : G_AllocateEdict() );

        // Get the incremental index entity.
        cm_entity = entities[ i ];

        // DEBUG: Open brace for this entity we're parsing.
        #ifdef DEBUG_ENTITY_SPAWN_PROCESS
        gi.dprintf( "Entity(#%i) {\n", entityID );
        #endif

        // Get the first key value.
        const cm_entity_t *kv = cm_entity;
        while ( kv ) {
            // The eventual field address we'll be dealing with.
            const spawn_field_t *keyField = nullptr;

            // Try and find the spawn_field_t* that has a matching kv->key value.
            const spawn_field_t *spf = GetSpawnFieldByKey( kv->key );
            // Didn't find it in spawn fields, so check if the temporary entity fields got the matching key.
            const spawn_field_t *tpf = GetTempFieldByKey( kv->key );

            // The address to write the pair's value into.
            byte *writeAddress = nullptr;

            // Spawn Field matches dominate the temporary spawn fields structure.
            if ( spf ) {
                // Set address to the entity's struct.
                writeAddress = (byte*)spawnEdict;
                // Set the key field pointer.
                keyField = spf;
            // The spawn field list failed to find a match, but the temporary spawn fields do have a match.
            } else if ( tpf ) {
                // Set address to the temporary spawn fields struct.
                writeAddress = (byte *)&st;
                // Set the key field pointer.
                keyField = tpf;
            }

            // With a write address and the specific matching field for 'key', we can write the pairs entity 
            // dictionary value to the designated variable's memory address.
            if ( writeAddress ) {
                // Write pair value to edict's key field address.
                WritePairToEdictKeyField( writeAddress, keyField, kv );

                // Debugging:
                #ifdef DEBUG_ENTITY_SPAWN_PROCESS_SHOW_PARSED_FOR_FIELD_TYPES
                if ( kv->parsed_type & cm_entity_parsed_type_t::ENTITY_STRING ) {
                    gi.dprintf( "\"%s\":\"%s\" parsed for type(string) value=(%s) \n", kv->key, kv->string, kv->nullable_string );
                }
                if ( kv->parsed_type & cm_entity_parsed_type_t::ENTITY_INTEGER ) {
                    gi.dprintf( "\"%s\":\"%s\" parsed for type(integer) value=(%d) \n", kv->key, kv->string, kv->integer );
                }
                if ( kv->parsed_type & cm_entity_parsed_type_t::ENTITY_FLOAT ) {
                    gi.dprintf( "\"%s\":\"%s\" parsed for type(float) value=(%f) \n", kv->key, kv->string, kv->value );
                }
                if ( kv->parsed_type & cm_entity_parsed_type_t::ENTITY_VECTOR2 ) {
                    gi.dprintf( "\"%s\":\"%s\" parsed for type(Vector2) value=(%f, %f) \n", kv->key, kv->string, kv->vec2[ 0 ], kv->vec2[ 1 ] );
                }
                if ( kv->parsed_type & cm_entity_parsed_type_t::ENTITY_VECTOR3 ) {
                    gi.dprintf( "\"%s\":\"%s\" parsed for type(Vector3) value=(%f, %f, %f) \n", kv->key, kv->string, kv->vec3[ 0 ], kv->vec3[ 1 ], kv->vec3[ 2 ] );
                }
                if ( kv->parsed_type & cm_entity_parsed_type_t::ENTITY_VECTOR4 ) {
                    gi.dprintf( "\"%s\":\"%s\" parsed for type(Vector4) value=(%f, %f, %f, %f) \n", kv->key, kv->vec4[ 0 ], kv->vec4[ 1 ], kv->vec4[ 2 ], kv->vec4[ 3 ] );
                }
                #endif
            // If we failed to find a field however, make sure to do warn/debug print.
            } else {
                #ifdef DEBUG_ENTITY_SPAWN_PROCESS
                // Technically, in sane scenario's, only this condition would occure:
                if ( spawnEdict ) {
                    gi.dprintf( "Unhandled key/value pair(%s, %s) for cm_entity(%d) and edict(%d)\n", kv->key, kv->string, i, spawnEdict->s.number );
                // Technically, in sane scenario's this would never happen:
                } else {
                    gi.dprintf( "Unhandled key/value pair(%s, %s) for cm_entity(%d) and edict(nullptr)\n", kv->key, kv->string, i );
                }
                #endif
            }

            // Get the next key/value pair of this entity.
            kv = kv->next;
        }


        // TODO: Move to some game mode specific function: CanEntitySpawn or a name of such nature.
        // See if we need to remove things (except the world) from different skill levels or game mode.
        if ( spawnEdict != g_edicts ) {
            // When nomonsters is set, remove any entities that have the word monster in their classname.
            if ( nomonsters->value && ( strstr( spawnEdict->classname, "monster" )
                || strstr( spawnEdict->classname, "misc_deadsoldier" )
                || strstr( spawnEdict->classname, "misc_insane" ) ) ) {
                // Free entity.
                G_FreeEdict( spawnEdict );
                // Increase the amount of inhibited entities we're keeping track of.
                numInhibitedEntities++;
                // Iterate to the next entity key/value list entry.
                continue;
            }

            // Deathmatch specific behavior, remove all entities that got a SPAWNFLAG_NOT_DEATHMATCH set.
            if ( deathmatch->value ) {
                if ( spawnEdict->spawnflags & SPAWNFLAG_NOT_DEATHMATCH ) {
                    // Free entity.
                    G_FreeEdict( spawnEdict );
                    // Increase the amount of inhibited entities we're keeping track of.
                    numInhibitedEntities++;
                    // Iterate to the next entity key/value list entry.
                }
            // Otherwise, assuming singleplayer/coop, remove all entities that do not match the current skill level:
            } else {
                if ( /* ((coop->value) && (ent->spawnflags & SPAWNFLAG_NOT_COOP)) || */
                    ( ( skill->value == 0 ) && ( spawnEdict->spawnflags & SPAWNFLAG_NOT_EASY ) ) ||
                    ( ( skill->value == 1 ) && ( spawnEdict->spawnflags & SPAWNFLAG_NOT_MEDIUM ) ) ||
                    ( ( ( skill->value == 2 ) || ( skill->value == 3 ) ) && ( spawnEdict->spawnflags & SPAWNFLAG_NOT_HARD ) )
                    ) {
                    // Free entity.
                    G_FreeEdict( spawnEdict );
                    // Increase the amount of inhibited entities we're keeping track of.
                    numInhibitedEntities++;
                    // Iterate to the next entity key/value list entry.
                }
            }

            // Remove the spawnflags for save/load games. 
            if ( spawnEdict != nullptr ) {
                spawnEdict->spawnflags &= ~( SPAWNFLAG_NOT_EASY | SPAWNFLAG_NOT_MEDIUM | SPAWNFLAG_NOT_HARD | SPAWNFLAG_NOT_COOP | SPAWNFLAG_NOT_DEATHMATCH );
            }
        }
        // EOF: TODO: Move to some game mode specific function: CanEntitySpawn or a name of such nature.
            
        // Call spawn on edicts that got a positive entityID. Worldspawn == entityID(#0).
        if ( entityID >= 0 ) {
            ED_CallSpawn( spawnEdict );
        }

        #ifdef DEBUG_ENTITY_SPAWN_PROCESS
        // End of entity closing brace.
        gi.dprintf( "}\n" );
        #endif

        // Increment entityID.
        entityID++;
    }

    // Give entities a chance to 'post spawn'.
    int32_t numPostSpawnedEntities = 0;
    for ( int32_t i = 0; i < globals.num_edicts; i++ ) {
        edict_t *ent = &g_edicts[ i ];

        if ( ent && ent->postspawn ) {
            ent->postspawn( ent );
            numPostSpawnedEntities++;
        }
    }

    // Developer print the inhibited entities.
    gi.dprintf( "%i entities inhibited\n", numInhibitedEntities );
    // Developer print the post spawned entities.
    gi.dprintf( "%i entities post spanwed\n", numPostSpawnedEntities );

#ifdef DEBUG
    i = 1;
    ent = EDICT_FOR_NUMBER(i);
    while (i < globals.num_edicts) {
        if (ent->inuse != 0 || ent->inuse != 1)
            Com_DPrintf("Invalid entity %d\n", i);
        i++, ent++;
    }
#endif

    // Find entity 'teams', NOTE: these are not actual player game teams.
    G_FindTeams();

    // Initialize player trail.
    PlayerTrail_Init();
}


//===================================================================

#if 0
// cursor positioning
xl <value>
xr <value>
yb <value>
yt <value>
xv <value>
yv <value>

// drawing
statpic <name>
pic <stat>
num <fieldwidth> <stat>
string <stat>

// control
if <stat>
ifeq <stat> <value>
ifbit <stat> <value>
endif

#endif

static const char single_statusbar[] =
"yb -24 "

// health
"xv 0 "
"hnum "
"xv 50 "
"pic 3 "

// ammo
"if 4 " // ammo
  "yb -40 "
  "xr -70 "
  "anum "
  "xr -95 "
  "pic 4 " // ammo icon.
  // Clip ammo.
  "xr -135 "
  "cnum "
"endif "
"yb +40 "
// armor
"if 6 " // armor_icon
  "xv 200 "
  "rnum "
  "xv 250 "
  "pic 6 " // armor_icon
"endif "

// selected item
"if 8 " // selected icon
  "xv 296 "
  "pic 8 "  // selected icon
"endif "

"yb -50 "

// picked up item
"if 9 "
  "xv 0 "
  "pic 9 " // pickup icon.
  "xv 26 "
  "yb -42 "
  "stat_string 10 " // pickup string.
  "yb -50 "
"endif "

// timer 1 (quad, enviro, breather)
"if 11 " // timer icon
  "xv 262 "
  "num 2 12 " // timer
  "xv 296 "
  "pic 11 "
"endif "

// timer 2 (pent)
"if 18 "
  "yb -76 "
  "xv 262 "
  "num 2 19 "
  "xv 296 "
  "pic 18 "
  "yb -50 "
"endif "

// help / weapon icon
"if 13 "
  "xv 148 "
  "pic 13 "
"endif "
;

static const char dm_statusbar[] =
// frags
"xr -50 "
"yt 2 "
"num 3 2 " // frags

// spectator
"if 17 " // stat_spectator
  "xv 0 "
  "yb -58 "
  "string2 \"SPECTATOR MODE\" "
"endif "

// chase camera
"if 16 "    // chase camera
  "xv 0 "
  "yb -68 "
  "string \"Chasing\" "
  "xv 64 "
  "stat_string 16 "
"endif "
;


/*QUAKED worldspawn (0 0 0) ?

Only used for the world.
"sky"   environment map name
"skyaxis"   vector axis for rotating sky
"skyrotate" speed of rotation in degrees/second
"sounds"    music cd track number
"gravity"   800 is default gravity
"message"   text to print at user logon
*/
void SP_worldspawn(edict_t *ent)
{
    ent->movetype = MOVETYPE_PUSH;
    ent->solid = SOLID_BSP;
    ent->inuse = true;          // since the world doesn't use G_AllocateEdict()
    ent->s.modelindex = 1;      // world model is always index 1

    //---------------

    // reserve some spots for dead player bodies for coop / deathmatch
    InitBodyQue();

    // set configstrings for items
    SetItemNames();

    if (st.nextmap)
        Q_strlcpy(level.nextmap, st.nextmap, sizeof(level.nextmap));

    // make some data visible to the server

    if (ent->message && ent->message[0]) {
        gi.configstring(CS_NAME, ent->message);
        Q_strlcpy(level.level_name, ent->message, sizeof(level.level_name));
    } else
        Q_strlcpy(level.level_name, level.mapname, sizeof(level.level_name));

    if (st.sky && st.sky[0])
        gi.configstring(CS_SKY, st.sky);
    else
        gi.configstring(CS_SKY, "unit1_");

    gi.configstring(CS_SKYROTATE, va("%f %d", st.skyrotate, st.skyautorotate));

    gi.configstring(CS_SKYAXIS, va("%f %f %f",
                                   st.skyaxis[0], st.skyaxis[1], st.skyaxis[2]));

    if (st.musictrack && st.musictrack[0])
        gi.configstring(CS_CDTRACK, st.musictrack);
    else
        gi.configstring(CS_CDTRACK, va("%i", ent->sounds));

    gi.configstring(CS_MAXCLIENTS, va("%i", (int)(maxclients->value)));

    // Set air acceleration properly.
    if ( COM_IsUint( sv_airaccelerate->string ) || COM_IsFloat( sv_airaccelerate->string ) ) {
        gi.configstring( CS_AIRACCEL, sv_airaccelerate->string );
    } else {
        gi.configstring( CS_AIRACCEL, "0" );
    }
     
    // status bar program
    if ( deathmatch->value ) {
        gi.configstring( CS_STATUSBAR, dm_statusbar );
    } else {
        gi.configstring( CS_STATUSBAR, single_statusbar );
    }

    //---------------


    // help icon for statusbar
    gi.imageindex("i_help");
    level.pic_health = gi.imageindex("i_health");
    gi.imageindex("help");
    gi.imageindex("field_3");

    if (!st.gravity)
        gi.cvar_set("sv_gravity", "800");
    else
        gi.cvar_set("sv_gravity", st.gravity);

    PrecacheItem(FindItem( "Fists" ));
    PrecacheItem(FindItem("Pistol"));

    // HUD Chat.
    gi.soundindex("hud/chat01.wav");

    // Body Gib.
    gi.soundindex( "world/gib01.wav" );
    gi.soundindex( "world/gib_drop01.wav" );

    // Item Respawn.
    gi.soundindex("items/respawn01.wav");

    // Login/Logout/Teleport.
    gi.soundindex( "world/mz_login.wav" );
    gi.soundindex( "world/mz_logout.wav" );
    gi.soundindex( "world/mz_respawn.wav" );
    gi.soundindex( "world/teleport01.wav" );

    // sexed sounds
    //gi.soundindex("*death1.wav");
    //gi.soundindex("*death2.wav");
    //gi.soundindex("*death3.wav");
    //gi.soundindex("*death4.wav");
    //gi.soundindex("*pain25_1.wav");
    //gi.soundindex("*pain25_2.wav");
    //gi.soundindex("*pain50_1.wav");
    //gi.soundindex("*pain50_2.wav");
    //gi.soundindex("*pain75_1.wav");
    //gi.soundindex("*pain75_2.wav");
    //gi.soundindex("*pain100_1.wav");
    //gi.soundindex("*pain100_2.wav");

    // Deaths:
    gi.soundindex( "player/death01.wav" );
    gi.soundindex( "player/death02.wav" );
    gi.soundindex( "player/death03.wav" );
    gi.soundindex( "player/death04.wav" );
    // Pains:
    gi.soundindex( "player/pain25_01.wav" );
    gi.soundindex( "player/pain50_01.wav" );
    gi.soundindex( "player/pain75_01.wav" );
    gi.soundindex( "player/pain100_01.wav" );
    // WID: All of these are now just burn01 and burn02 since the original sounds contained silly screams and all that.
    snd_fry = gi.soundindex( "player/burn01.wav" );  // standing in lava / slime
    //gi.soundindex( "player/lava_in.wav" );
    //gi.soundindex( "player/burn1.wav" );
    //gi.soundindex( "player/burn2.wav" );
    //gi.soundindex( "player/lava1.wav" );
    //gi.soundindex( "player/lava2.wav" );
    gi.soundindex( "player/burn01.wav" );
    gi.soundindex( "player/burn02.wav" );
    // Kinematics:
    gi.soundindex( "player/jump01.wav" );   // Player jump.
    gi.soundindex( "player/jump02.wav" ); // Player jump.
    gi.soundindex( "player/fall01.wav" );   // Player fall.
    gi.soundindex( "player/fall02.wav" );   // Player heavy fall.
    gi.soundindex( "player/land01.wav" );   // Player jump landing sound.
    // Water:
    gi.soundindex( "player/drown01.wav" );  // Drowning last breaths.
    gi.soundindex( "player/gasp01.wav" );   // Gasping for air.
    gi.soundindex( "player/gasp02.wav" );   // Head breaking surface, not gasping.
    gi.soundindex( "player/gurp01.wav" );   // Drowning damage 01.
    gi.soundindex( "player/gurp02.wav" );   // Drowning damage 02.
    gi.soundindex( "player/water_body_out01.wav" );     // Feet hitting water.
    gi.soundindex( "player/water_feet_in01.wav" );      // Feet hitting water.
    gi.soundindex( "player/water_feet_out01.wav" );     // Feet leaving water.
    gi.soundindex( "player/water_head_under01.wav" );   // Head going underwater.
    gi.soundindex( "player/water_splash_in01.wav" );    // Head going underwater.
    gi.soundindex( "player/water_splash_in02.wav" );    // Head going underwater.
    gi.soundindex( "world/water_land_splash01.wav" );   // Landing splash 01.
    gi.soundindex( "world/water_land_splash02.wav" );   // Landing splash 02.
    // Misc/World(Kinematics):
    gi.soundindex( "world/land01.wav" );    // Ground landing thud.

    //gi.soundindex("*pain25_1.wav");
    //gi.soundindex("*pain25_2.wav");
    //gi.soundindex("*pain50_1.wav");
    //gi.soundindex("*pain50_2.wav");
    //gi.soundindex("*pain75_1.wav");
    //gi.soundindex("*pain75_2.wav");
    //gi.soundindex("*pain100_1.wav");
    //gi.soundindex("*pain100_2.wav");
     
    // sexed models
    // THIS ORDER MUST MATCH THE DEFINES IN g_local.h
    // you can add more, max 15
    gi.modelindex( "#w_fists.iqm" ); // #w_fists.iqm
    gi.modelindex( "#w_pistol.iqm" );
    //gi.modelindex( "#w_shotgun.md2" );
    //gi.modelindex( "#w_sshotgun.md2" );
    //gi.modelindex( "#w_machinegun.md2" );
    //gi.modelindex( "#w_chaingun.md2" );
    //gi.modelindex( "#a_grenades.md2" );
    //gi.modelindex( "#w_glauncher.md2" );
    //gi.modelindex( "#w_rlauncher.md2" );
    //gi.modelindex( "#w_hyperblaster.md2" );
    //gi.modelindex( "#w_railgun.md2" );
    //gi.modelindex( "#w_bfg.md2" );

    //-------------------

    sm_meat_index = gi.modelindex("models/objects/gibs/sm_meat/tris.md2");
    gi.modelindex("models/objects/gibs/arm/tris.md2");
    gi.modelindex("models/objects/gibs/bone/tris.md2");
    gi.modelindex("models/objects/gibs/bone2/tris.md2");
    gi.modelindex("models/objects/gibs/chest/tris.md2");
    gi.modelindex("models/objects/gibs/skull/tris.md2");
    gi.modelindex("models/objects/gibs/head2/tris.md2");

//
// Setup light animation tables. 'a' is total darkness, 'z' is doublebright.
//

    // 0 normal
    gi.configstring(CS_LIGHTS + 0, "m");

    // 1 FLICKER (first variety)
    gi.configstring(CS_LIGHTS + 1, "mmnmmommommnonmmonqnmmo");

    // 2 SLOW STRONG PULSE
    gi.configstring(CS_LIGHTS + 2, "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba");

    // 3 CANDLE (first variety)
    gi.configstring(CS_LIGHTS + 3, "mmmmmaaaaammmmmaaaaaabcdefgabcdefg");

    // 4 FAST STROBE
    gi.configstring(CS_LIGHTS + 4, "mamamamamama");

    // 5 GENTLE PULSE 1
    gi.configstring(CS_LIGHTS + 5, "jklmnopqrstuvwxyzyxwvutsrqponmlkj");

    // 6 FLICKER (second variety)
    gi.configstring(CS_LIGHTS + 6, "nmonqnmomnmomomno");

    // 7 CANDLE (second variety)
    gi.configstring(CS_LIGHTS + 7, "mmmaaaabcdefgmmmmaaaammmaamm");

    // 8 CANDLE (third variety)
    gi.configstring(CS_LIGHTS + 8, "mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa");

    // 9 SLOW STROBE (fourth variety)
    gi.configstring(CS_LIGHTS + 9, "aaaaaaaazzzzzzzz");

    // 10 FLUORESCENT FLICKER
    gi.configstring(CS_LIGHTS + 10, "mmamammmmammamamaaamammma");

    // 11 SLOW PULSE NOT FADE TO BLACK
    gi.configstring(CS_LIGHTS + 11, "abcdefghijklmnopqrrqponmlkjihgfedcba");

    // styles 32-62 are assigned by the light program for switchable lights

    // 63 testing
    gi.configstring(CS_LIGHTS + 63, "a");
}

