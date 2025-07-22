/********************************************************************
*
*
*	SharedGame: GameMode Related Stuff.
*
*
********************************************************************/
#include "svgame/svg_local.h"

#include "svgame/player/svg_player_client.h"
#include "svgame/player/svg_player_hud.h"

#include "svgame/svg_lua.h"

#include "sharedgame/sg_gamemode.h"
#include "svgame/svg_gamemode.h"
#include "svgame/gamemodes/svg_gm_basemode.h"

#include "svgame/entities/target/svg_target_changelevel.h"
#include "svgame/entities/svg_player_edict.h"



/**
*
*
*
*	General GameMode Functionality:
*
*
*
**/
/**
*	@brief	Defines the behavior for the game mode when the level has to exit.
**/
void svg_gamemode_t::ExitLevel() {
	// Notify Lua.
	SVG_Lua_CallBack_ExitMap();

	// Generate the command string to change the map.
	char    command[ 256 ];
	Q_snprintf( command, sizeof( command ), "gamemap \"%s\"\n", (const char *)level.changemap );
	// Add it to the command string queue.
	gi.AddCommandString( command );
	// Reset the level changemap, and intermission state.
	level.changemap = NULL;
	level.exitintermission = 0;
	level.intermissionFrameNumber = 0;

	// Clear some things before going to next level.
	for ( int32_t i = 0; i < maxclients->value; i++ ) {
		// Get client entity and ensure it is valid.
		svg_base_edict_t *ent = g_edict_pool.EdictForNumber( i + 1 );
		if ( !ent || !ent->inuse ) {
			continue;
		}
		// Reset the health of the player to their max health.
		if ( ent->health > ent->client->pers.max_health ) {
			ent->health = ent->client->pers.max_health;
		}
	}
}

/**
*   @brief  Returns the created target changelevel
**/
svg_base_edict_t *svg_gamemode_t::CreateTargetChangeLevel( const char *map ) {
	svg_base_edict_t *ent = g_edict_pool.AllocateNextFreeEdict<svg_target_changelevel_t>( "target_changelevel" );
	//ent->classname = svg_level_qstring_t::from_char_str( "target_changelevel" );
	if ( strcmp( map, level.nextmap ) != 0 ) {
		Q_strlcpy( level.nextmap, map, sizeof( level.nextmap ) );
	}
	ent->map = svg_level_qstring_t::from_char_str( level.nextmap );
	return ent;
}