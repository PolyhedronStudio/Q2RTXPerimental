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
// Game related types.
#include "svgame/svg_local.h"
// Save related types.
#include "svgame/svg_save.h"

#include "svgame/svg_clients.h"
#include "svgame/svg_edict_pool.h"

#include "svgame/entities/svg_player_edict.h"
#include "svgame/player/svg_player_client.h"

#if USE_ZLIB
    #include <zlib.h>
#else
#define gzopen(name, mode)          fopen(name, mode)
#define gzclose(file)               fclose(file)
#define gzwrite(file, buf, len)     fwrite(buf, 1, len, file)
#define gzread(file, buf, len)      fread(buf, 1, len, file)
#define gzbuffer(file, size)        (void)0
#define gzFile                      FILE *
#endif



/**
*   @description    WriteGame
*
*   This will be called whenever the game goes to a new level,
*   and when the user explicitly saves the game.
*
*   Game information include cross level data, like multi level
*   triggers, help computer info, and all client states.
*
*   A single player death will automatically restore from the
*   last save position.
**/
void SVG_WriteGame( const char *filename, const bool isAutoSave ) {
    // If not an autosave, make sure to save client data properly.
    if ( !isAutoSave ) {
        SVG_Player_SaveClientData();
    }

    gzFile f = gzopen( filename, "wb" );
    if ( !f ) {
        gi.error( "Couldn't open %s", filename );
    }

    // Create a savegame write context.
    game_write_context_t ctx = game_write_context_t::make_write_context( f );

    //f = gzopen(filename, "wb");
    //if (!f)
    //    gi.error("Couldn't open %s", filename);

    ctx.write_int32( SAVE_MAGIC1 );
    ctx.write_int32( SAVE_VERSION );

    game.autosaved = isAutoSave;
    ctx.write_fields( svg_game_locals_t::saveDescriptorFields, &game );//write_fields(f, gamefields, &game);
    game.autosaved = false;

    for ( int32_t i = 0; i < game.maxclients; i++) {
        //write_fields(f, clientfields, &game.clients[i] );
		ctx.write_fields( svg_client_t::saveDescriptorFields, &game.clients[ i ] );
    }

    // Possibly throw an error if the file couldn't be written.
    if ( gzclose( f ) ) {
        gi.error( "Couldn't write %s", filename );
    }
}

/**
*   @brief  Reads a game save file and initializes the game state.
*   @param  filename The name of the save file to read.
*   @description    This function reads a save file and initializes the game state.
*                   It allocates memory for the game state and clients, and reads the game data from the file.
*                   It also checks the version of the save file and ensures that the game state is valid.
**/
void SVG_ReadGame( const char *filename ) {
    gzFile	f;
    int     i;

    //if ( g_edict_pool.edicts != nullptr || g_edict_pool.max_edicts != game.maxentities ) {
    //    SVG_EdictPool_Release( &g_edict_pool );
    //}
    gi.FreeTags(TAG_SVGAME);
    game = {};

    // ReInitialize the game local's movewith array.
    game.moveWithEntities = static_cast<svg_game_locals_t::game_locals_movewith_t *>( gi.TagMalloc( sizeof( svg_game_locals_t::game_locals_movewith_t ) * MAX_EDICTS, TAG_SVGAME ) );
    memset( game.moveWithEntities, 0, sizeof( svg_game_locals_t::game_locals_movewith_t ) * MAX_EDICTS );

	// Open the file.
    f = gzopen(filename, "rb");
    if ( !f ) {
        gi.error( "Couldn't open %s", filename );
    }
	// Set the buffer size to 64k.
    gzbuffer(f, 65536);
    // Create a savegame read context.
    game_read_context_t ctx = game_read_context_t::make_read_context( f );

	// Read the magic number.
    i = ctx.read_int32();
    if (i != SAVE_MAGIC1) {
        gzclose(f);
        gi.error("Not a Q2RTXPerimental save game");
    }
	// Read the version number.
    i = ctx.read_int32();
    if ((i != SAVE_VERSION)  && (i != 2)) {
        // Version 2 was written by Q2RTX 1.5.0, and the savegame code was crafted such to allow reading it
        gzclose(f);
        gi.error("Savegame from different version (got %d, expected %d)", i, SAVE_VERSION);
    }

    // Read in svg_game_locals_t fields.
    ctx.read_fields( svg_game_locals_t::saveDescriptorFields, &game ); //read_fields(&ctx, gamefields, &game);

    // should agree with server's version
    if (game.maxclients != (int)maxclients->value) {
        gzclose(f);
        gi.error("Savegame has bad maxclients");
    }
    if (game.maxentities <= game.maxclients || game.maxentities > MAX_EDICTS) {
        gzclose(f);
        gi.error("Savegame has bad maxentities");
    }

    // Clamp maxentities within valid range.
    game.maxentities = QM_ClampUnsigned<uint32_t>( maxentities->integer, (int)maxclients->integer + 1, MAX_EDICTS );
    // Release all edicts.
    g_edicts = SVG_EdictPool_Release( &g_edict_pool );
    // Initialize the edicts array pointing to the memory allocated within the pool.
    g_edicts = SVG_EdictPool_Allocate( &g_edict_pool, game.maxentities );
	g_edict_pool.max_edicts = game.maxentities;

	// Initialize a fresh clients array.
    game.clients = SVG_Clients_Reallocate( game.maxclients );

    // Now read in the fields for each client.
    for (i = 0; i < game.maxclients; i++) {
        // Now read in the fields.
        //read_fields(&ctx, clientfields, &game.clients[i]);
        ctx.read_fields( svg_client_t::saveDescriptorFields, &game.clients[ i ] );
    }

    gzclose(f);
}



/***
*
*
*
*
*   Level Read/Write:
*
*
*
*
***/
/**
*   @brief  Writes the state of the current level to a file.
**/
void SVG_WriteLevel(const char *filename)
{
    gzFile f = gzopen(filename, "wb");
    if ( !f ) {
        gi.error( "Couldn't open %s", filename );
    }

    // Create a savegame write context.
    game_write_context_t ctx = game_write_context_t::make_write_context( f );

    ctx.write_int32( SAVE_MAGIC2 );
    ctx.write_int32( SAVE_VERSION );

    // Write out all the entities.
    for ( int32_t i = 0; i < globals.edictPool->num_edicts; i++) {
        svg_base_edict_t *ent = g_edict_pool.EdictForNumber( i );
        //ent = &g_edicts[i];
        if ( !ent || !ent->inuse ) {
            continue;
        }
        // Entity number.
        ctx.write_int32( i );
        // Entity classname.
        ctx.write_level_qstring( &ent->classname );
        // Rest of entity fields.
        ent->Save( &ctx );
        //write_fields(f, entityfields, ent);
    }

    // End of level data.
    ctx.write_int32( -1 );

    // Write out svg_level_locals_t. We do this after writing out the entities.
	// This is so that while loading the level, we initialize entities first
    // which in turn, the levelfields can optionally be pointing at.
	ctx.write_fields( svg_level_locals_t::saveDescriptorFields, &level );

	// Possibly throw an error if the file couldn't be written.
    if ( gzclose( f ) ) {
        gi.error( "Couldn't write %s", filename );
    }
} 

/**
*   @brief  SpawnEntities will allready have been called on the
*   level the same way it was when the level was saved.
*   
*   That is necessary to get the baselines set up identically.
*
*   The server will have cleared all of the world links before
*   calling ReadLevel.
*
*   No clients are connected yet.
**/
void SVG_MoveWith_FindParentTargetEntities( void );
void SVG_ReadLevel(const char *filename)
{
    int     entnum;
    svg_base_edict_t *ent;

	// For recovering the level cm_entity_t's.
    static const cm_entity_t *cm_entities[ MAX_EDICTS ] = {};
	for ( int32_t i = 0; i < game.maxentities; i++ ) {
		cm_entities[ i ] = g_edict_pool.edicts[ i ]->entityDictionary;
	}

    // free any dynamic memory allocated by loading the level
    // base state.
    //
	// WID: Keep in mind that freeing entities after this, would invalidate their sg_qtag_memory_t blocks.
    gi.FreeTags(TAG_SVGAME_LEVEL);
    
    gzFile f = gzopen(filename, "rb");
    if ( !f ) {
        gi.error( "Couldn't open %s", filename );
    }
    gzbuffer(f, 65536);

    // Create read context.
    game_read_context_t ctx = game_read_context_t::make_read_context( f );

    // Wipe all the entities back to 'baseline'.
    for ( int32_t i = 0; i < game.maxentities; i++ ) {
        #if 0
            // Store original number.
            const int32_t number = g_edicts[ i ]->s.number;
            // Zero out.
            *g_edicts[ i ] = {}; //memset( g_edicts, 0, game.maxentities * sizeof( g_edicts[ 0 ] ) );
            // Retain the entity's original number.
            g_edicts[ i ]->s.number = number;
        #else
            //// Reset the entity to base state.
            //if ( g_edicts[ i ] ) {
            //    // Reset entity.
            //    g_edicts[ i ]->Reset( g_edicts[ i ]->entityDictionary );
            //} else {
                const int32_t entityNumber = g_edicts[ i ]->s.number;
                ////*g_edicts[ i ] = { };
                if ( i >= 1 && i < game.maxclients + 1 ) {
                    g_edict_pool.edicts[ i ] = new svg_player_edict_t();
                } else {
                    g_edict_pool.edicts[ i ] = new svg_base_edict_t( cm_entities[ i ] );
                }
                //// Set the number to the current index.
                g_edict_pool.edicts[ i ]->s.number = entityNumber;
            //}
        #endif
    }

    // Default num_edicts.
    g_edict_pool.num_edicts = maxclients->value + 1;

    int32_t i = ctx.read_int32();
    if (i != SAVE_MAGIC2) {
        gzclose(f);
        gi.error("Not a Q2RTXPerimental save game");
    }

    i = ctx.read_int32();
    if ((i != SAVE_VERSION) && (i != 2)) {
        // Version 2 was written by Q2RTX 1.5.0, and the savegame code was crafted such to allow reading it
        gzclose(f);
        gi.error("Savegame from different version (got %d, expected %d)", i, SAVE_VERSION);
    }

    // load all the entities
    while (1) {
        // Read in entity number.
        entnum = ctx.read_int32( );
        if ( entnum == -1 )
            break;
        if ( entnum < 0 || entnum >= game.maxentities ) {
            gzclose( f );
            gi.error( "%s: bad entity number", __func__ );
        }
        if ( entnum >= g_edict_pool.num_edicts ) {
            g_edict_pool.num_edicts = entnum + 1;
        }
        // Inquire for the classname.
		svg_level_qstring_t classname;
        classname = ctx.read_level_qstring();

        // Acquire the typeinfo.
        // TypeInfo for this entity.
        EdictTypeInfo *typeInfo = EdictTypeInfo::GetInfoByWorldSpawnClassName( classname.ptr );
        if ( !typeInfo ) {
            classname = "svg_base_edict_t";
            typeInfo = EdictTypeInfo::GetInfoByWorldSpawnClassName( classname.ptr );
        }

        // For restoring the cm_entity_t.
        const cm_entity_t *cm_entity = cm_entities[ entnum ];//g_edict_pool.edicts[ entnum ]->entityDictionary;
        // Worldspawn:
        svg_base_edict_t *ent = g_edict_pool.edicts[ entnum ] = typeInfo->allocateEdictInstanceCallback( nullptr );
        ent->classname = classname;
        // It was the original entity instanced at the map load time.
		// Restore the cm_entity_t.
        if ( ent->spawn_count == 0 ) {
            ent->entityDictionary = cm_entity;
        }
        // Restore the entity.

        ent->Restore( &ctx );

        ent->inuse = true;
        ent->s.number = entnum;

        // Let the server relink the entity in.
        memset(&ent->area, 0, sizeof(ent->area));
        gi.linkentity(ent);
    }

	// We load these right after the entities, so that we can
	// optionally point to them from the svg_level_locals_t fields.
    //read_fields( &ctx, levelfields, &level );
	ctx.read_fields( svg_level_locals_t::saveDescriptorFields, &level );

    gzclose(f);

    // Mark all clients as unconnected
    for ( i = 0; i < maxclients->value; i++ ) {
        //ent = &g_edicts[ i + 1 ];
        ent = g_edict_pool.EdictForNumber( i + 1 );
        ent->client = &game.clients[ i ];
        ent->client->pers.connected = false;
        ent->client->pers.spawned = false;
    }

    // do any load time things at this point
    for (i = 0 ; i < g_edict_pool.num_edicts ; i++) {
        ent = g_edict_pool.EdictForNumber( i ); //&g_edicts[i];

        if ( !ent || !ent->inuse ) {
            continue;
        }

        // fire any cross-level triggers
        if (ent->classname)
            if (strcmp( (const char *)ent->classname, "target_crosslevel_target") == 0)
                ent->nextthink = level.time + QMTime::FromSeconds( ent->delay );
        #if 0
        if (ent->think == func_clock_think || ent->use == func_clock_use) {
            char *msg = ent->message;
			// WID: C++20: Added cast.
            ent->message = (char*)gi.TagMallocz(CLOCK_MESSAGE_SIZE, TAG_SVGAME_LEVEL);
            if (msg) {
                Q_strlcpy(ent->message, msg, CLOCK_MESSAGE_SIZE);
                gi.TagFree(msg);
            }
        }
        #endif
    }

    // Connect all movewith entities.
    //SVG_MoveWith_FindParentTargetEntities();
}

