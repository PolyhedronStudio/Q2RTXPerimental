/********************************************************************
*
*
*	ServerGame: GameMode Related Stuff.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"

#include "svgame/player/svg_player_client.h"
#include "svgame/player/svg_player_hud.h"
#include "svgame/player/svg_player_view.h"

#include "svgame/svg_lua.h"

#include "sharedgame/sg_pmove.h"
#include "sharedgame/sg_gamemode.h"
#include "sharedgame/sg_means_of_death.h"
#include "svgame/svg_gamemode.h"
#include "svgame/gamemodes/svg_gm_basemode.h"
#include "svgame/gamemodes/svg_gm_cooperative.h"

#include "svgame/entities/target/svg_target_changelevel.h"
#include "svgame/entities/svg_player_edict.h"



/**
*
*
*
*	Cooperative GameMode:
*
*
*
**/
/**
*	@brief	Returns true if this gamemode allows saving the game.
**/
const bool svg_gamemode_cooperative_t::AllowSaveGames() const {
	// We only allow saving in coop mode if we're a dedicated server.
	return ( dedicated != nullptr && dedicated->value != 0 ? true : false );
}

/**
*	@brief	Called once by client during connection to the server. Called once by
*			the server when a new game is started.
**/
void svg_gamemode_cooperative_t::PrepareCVars() {
	// Set the CVar for keeping old code in-tact. TODO: Remove in the future.
	gi.cvar_forceset( "coop", "1" );

	// Setup maxclients correctly.
	if ( maxclients->integer <= 1 || maxclients->integer > 4 ) {
		gi.cvar_forceset( "maxclients", "4" ); // Cvar_Set( "maxclients", "4" );
	}
}


/**
*   @details    Called when a player begins connecting to the server.
*
*               The game can refuse entrance to a client by returning false.
*
*               If the client is allowed, the connection process will continue
*               and eventually get to ClientBegin()
*
*               Changing levels will NOT cause this to be called again, but
*               loadgames WILL.
**/
const bool svg_gamemode_cooperative_t::ClientConnect( svg_player_edict_t *ent, char *userinfo ) {
	/**
	*	@brief	Password and spectator checks.
	**/
	// Check for a spectator
	const char *value = Info_ValueForKey( userinfo, "spectator" );
	if ( /*deathmatch->value && */*value && strcmp( value, "0" ) ) {
		//
		// Check for a spectator password.
		if ( *spectator_password->string &&
			strcmp( spectator_password->string, "none" ) &&
			strcmp( spectator_password->string, value ) ) {
			Info_SetValueForKey( userinfo, "rejmsg", "Spectator password required or incorrect." );
			// Refused.
			return false;
		}

		// Count total spectators.
		int32_t numspec = 0;
		for ( int32_t i = 0; i < maxclients->value; i++ ) {
			// Get edict.
			svg_base_edict_t *ed = g_edict_pool.EdictForNumber( i + 1 );
			// Entity is a spectator.
			if ( ed != nullptr && ed->inuse && ed->client->pers.spectator ) {
				numspec++;
			}
		}

		if ( numspec >= maxspectators->value ) {
			Info_SetValueForKey( userinfo, "rejmsg", "Server spectator limit is full." );
			// Refused.
			return false;
		}
	} else {
		// Check for a regular password if any is set.
		value = Info_ValueForKey( userinfo, "password" );
		if ( *password->string && strcmp( password->string, "none" ) &&
			strcmp( password->string, value ) ) {
			Info_SetValueForKey( userinfo, "rejmsg", "Password required or incorrect." );
			// Refused.
			return false;
		}
	}

	/**
	*	(Re-)Initialize client and entity body.
	**/
	// They can connect.
	ent->client = &game.clients[ g_edict_pool.NumberForEdict( ent ) - 1 ];

	// If there is already a body waiting for us (a loadgame), just
	// take it, otherwise spawn a new one from scratch.
	if ( ent->inuse == false ) {
		// clear the respawning variables
		SVG_Player_InitRespawnData( ent->client );
		if ( !game.autosaved || !ent->client->pers.weapon ) {
			SVG_Player_InitPersistantData( ent, ent->client );
		}
	}

	// Connected.
	return true;
}
/**
*   @brief  called whenever the player updates a userinfo variable.
*
*           The game can override any of the settings in place
*           (forcing skins or names, etc) before copying it off.
**/
void svg_gamemode_cooperative_t::ClientUserinfoChanged( svg_player_edict_t *ent, char *userinfo ) {
	// Set spectator
	char *strSpectator = Info_ValueForKey( userinfo, "spectator" );
	if ( /*deathmatch->value && */*strSpectator && strcmp( strSpectator, "0" ) ) {
		ent->client->pers.spectator = true;
	} else {
		ent->client->pers.spectator = false;
	}

	// Set character skin.
	const int32_t playernum = g_edict_pool.NumberForEdict( ent ) - 1;
	// Combine name and skin into a configstring.
	char *strSkin = Info_ValueForKey( userinfo, "skin" );
	gi.configstring( CS_PLAYERSKINS + playernum, va( "%s\\%s", ent->client->pers.netname, strSkin ) );

	// Reset the FOV in case it had been changed by any in-use weapon modes.
	SVG_Player_ResetPlayerStateFOV( ent->client );

	// Set weapon handedness
	char *strHand = Info_ValueForKey( userinfo, "hand" );
	if ( strlen( strHand ) ) {
		ent->client->pers.hand = atoi( strHand );
	}
}
/**
*   @brief  Called either when a player connects to a server, OR (to respawn thus) respawns in a multiplayer game.
*
*           Will look up a spawn point, spawn(placing) the player 'body' into the server and (re-)initializing
*           saved entity and persistant data. (This includes actually raising the weapon up.)
**/
void svg_gamemode_cooperative_t::ClientSpawnBody( svg_player_edict_t *ent ) {
    // Always clear out any possibly previous left over of the useTargetHint.
    Client_ClearUseTargetHint( ent, ent->client, nullptr );

    // find a spawn point
    // do it before setting health back up, so farthest
    // ranging doesn't count this client
    Vector3 spawn_origin = QM_Vector3Zero();
    Vector3 spawn_angles = QM_Vector3Zero();
    // Seek spawn 'spot' to position us on to.
    if ( !game.mode->SelectSpawnPoint( static_cast<svg_player_edict_t *>( ent ), spawn_origin, spawn_angles ) ) {
        // <Q2RTXP>: WID: TODO: Warn or error out, or just ignore it like it used to?
    }

    // Client Index.
    const int32_t index = g_edict_pool.NumberForEdict( ent ) - 1;//ent - g_edicts - 1;
    // GClient Ptr.
    svg_client_t *client = ent->client;

    // Assign the found spawnspot origin and angles.
    VectorCopy( spawn_origin, ent->s.origin );
    VectorCopy( spawn_origin, client->ps.pmove.origin );


    // Get persistent user info and store it into a buffer.
    char    userinfo[ MAX_INFO_STRING ];
    memcpy( userinfo, client->pers.userinfo, sizeof( userinfo ) );
    // Acquire other respawn data.
    const client_respawn_t savedRespawnData = client->resp;

    // this is kind of ugly, but it's how we want to handle keys in coop
    //for (n = 0; n < game.num_items; n++)
    //{
    //    if (itemlist[n].flags & IT_KEY)
    //    resp.coop_respawn.inventory[n] = client->pers.inventory[n];
    //}
    //resp.coop_respawn.game_helpchanged = client->pers.game_helpchanged;
    //resp.coop_respawn.helpchanged = client->pers.helpchanged;
    client->pers = savedRespawnData.pers_respawn;
    // Restore score.
    if ( savedRespawnData.score > client->pers.score ) {
        client->pers.score = savedRespawnData.score;
    }

    // Use the buffer to restore the client user info which had been stored as persistent.
    SVG_Client_UserinfoChanged( ent, userinfo );

    // Backup persistent client data and wipe the client structure for reuse.
    const client_persistant_t savedPersistantData = client->pers;
    // Acquire actual client number.
    const int32_t clientNum = client->clientNum;
    // Reset client value.
    *client = { };
    //memset( client, 0, sizeof( *client ) );
    // Restore client number.
    client->clientNum = clientNum;
    // Reinitialize persistent data.
    client->pers = savedPersistantData;
    // <Q2RTXP>: WID: TODO: Do we want this in singleplayer?
    // If dead at the time of the previous map switching to the current, reinitialize persistent data.
    if ( client->pers.health <= 0 ) {
        SVG_Player_InitPersistantData( ent, client );
    }
    // Restore the client's originally set respawn data.
    client->resp = savedRespawnData;

    // copy some data from the client to the entity
    SVG_Player_RestoreClientData( ent );

    // <Q2RTXP>: WID: TODO: ???
    // Fix level switch issue.
    ent->client->pers.connected = true;

    // Clear key entity values by reconfiguring this as a clean slate player entity.
    ent->groundInfo = {};
    ent->client = &game.clients[ index ];
    ent->takedamage = DAMAGE_AIM;
    ent->movetype = MOVETYPE_WALK;
    ent->viewheight = PM_VIEWHEIGHT_STANDUP;
    ent->inuse = true;
    ent->classname = svg_level_qstring_t::from_char_str( "player" );
    ent->mass = 200;
    ent->gravity = 1.0f;
    ent->solid = SOLID_BOUNDS_BOX;
    ent->lifeStatus = LIFESTATUS_ALIVE;
    ent->air_finished_time = level.time + 12_sec;
    ent->clipmask = ( CM_CONTENTMASK_PLAYERSOLID );
    ent->model = svg_level_qstring_t::from_char_str( "players/playerdummy/tris.iqm" );
    ent->SetPainCallback( &svg_player_edict_t::onPain );//ent->SetPainCallback( player_pain );
    ent->SetDieCallback( &svg_player_edict_t::onDie );//ent->SetDieCallback( player_die );
    ent->liquidInfo.level = cm_liquid_level_t::LIQUID_NONE;
    ent->liquidInfo.type = CONTENTS_NONE;
    ent->flags &= ~FL_NO_KNOCKBACK;

    // Make sure it has no DEADMONSTER set anymore.
    ent->svflags &= ~SVF_DEADMONSTER;
    // Ensure it is a proper player entity.
    ent->svflags |= SVF_PLAYER;
    ent->s.entityType = ET_PLAYER;

    // Copy in the bounds.
    VectorCopy( PM_BBOX_STANDUP_MINS, ent->mins );
    VectorCopy( PM_BBOX_STANDUP_MAXS, ent->maxs );
    // Ensure velocity is cleared.
    VectorClear( ent->velocity );

    // Reset PlayerState values.
    client->ps = {};
    //memset( &ent->client->ps, 0, sizeof( client->ps ) );
    // Reset the Field of View for the player state.
    SVG_Player_ResetPlayerStateFOV( ent->client );

    // Initialize basic player state and player move data.
    ent->client->ps.pmove.viewheight = ent->viewheight;
    ent->client->ps.pmove.origin = ent->s.origin;
    // Proper gunindex.
    if ( client->pers.weapon ) {
        client->ps.gun.modelIndex = gi.modelindex( client->pers.weapon->view_model );
    } else {
        client->ps.gun.modelIndex = 0;
        client->ps.gun.animationID = 0;
    }

    // Clear EntityState values.
    ent->s.sound = 0;
    ent->s.effects = 0;
    ent->s.renderfx = 0;
    ent->s.modelindex = MODELINDEX_PLAYER;        // Will use the skin specified model.
    ent->s.modelindex2 = MODELINDEX_PLAYER;       // Custom gun model.
    // sknum is player num and weapon number
    // weapon number will be added in changeweapon
    ent->s.skinnum = g_edict_pool.NumberForEdict( ent ) - 1;//ent - globals.edictPool->edicts - 1;
    ent->s.frame = 0;
    ent->s.old_frame = 0;

    // Try to properly clip to the floor / spawn
    Vector3 temp = spawn_origin;
    Vector3 temp2 = spawn_origin;
    temp[ 2 ] -= 64;
    temp2[ 2 ] += 16;
    svg_trace_t tr = SVG_Trace( &temp2.x, ent->mins, ent->maxs, &temp.x, ent, ( CM_CONTENTMASK_PLAYERSOLID ) );
    if ( !tr.allsolid && !tr.startsolid && Q_stricmp( level.mapname, "tech5" ) ) {
        VectorCopy( tr.endpos, ent->s.origin );
        ent->groundInfo.entity = tr.ent;
    } else {
        VectorCopy( spawn_origin, ent->s.origin );
        ent->s.origin[ 2 ] += 10; // make sure off ground
    }

    // <Q2RTXP>: WID: Restore the origin.
    VectorCopy( ent->s.origin, ent->s.old_origin );
    client->ps.pmove.origin = ent->s.origin;

    // <Q2RTXP>: WID: 
    // Link it to calculate absmins/absmaxs, this is to prevent actual
    // other entities from Spawn Touching.
    gi.linkentity( ent );

    // Ensure proper pitch and roll angles for calculating the delta angles.
    spawn_angles[ PITCH ] = 0;
    spawn_angles[ ROLL ] = 0;
    // Configure all spawn view angles.
    VectorCopy( spawn_angles, ent->s.angles );
    client->ps.viewangles = spawn_angles;
    client->viewMove.viewAngles = spawn_angles;
    // Set the delta angle
    client->ps.pmove.delta_angles = /*ANGLE2SHORT*/QM_Vector3AngleMod( spawn_angles - client->resp.cmd_angles );

    // Calculate anglevectors.
    AngleVectors( &client->viewMove.viewAngles.x, &client->viewMove.viewForward.x, nullptr, nullptr );

    #if 0
    // spawn a spectator
    if ( client->pers.spectator ) {
        client->chase_target = NULL;

        client->resp.spectator = true;

        ent->movetype = MOVETYPE_NOCLIP;
        ent->solid = SOLID_NOT;
        ent->svflags |= SVF_NOCLIENT;
        ent->client->ps.gun.modelIndex = 0;
        ent->client->ps.gun.animationID = 0;

        gi.linkentity( ent );
        return;
    } else {
        client->resp.spectator = false;
    }
    #else
    // No spectator in singleplayer.
    client->resp.spectator = false;
    #endif

    // Unlink it again, for SVG_UTIL_KillBox.
    gi.unlinkentity( ent );

    // Kill anything in its path.
    if ( !SVG_Util_KillBox( ent, true, MEANS_OF_DEATH_TELEFRAGGED ) ) {
        // could't spawn in?
        //int x = 10; // debug breakpoint
    }
    // And link it back in.
    gi.linkentity( ent );

    // Force a current active weapon up
    client->newweapon = client->pers.weapon;
    SVG_Player_Weapon_Change( ent );
}
/**
*	@brief	Called somewhere at the beginning of the game frame. This allows
*			to determine if conditions are met to engage exitting intermission
*			mode and/or exit the level.
**/
void svg_gamemode_cooperative_t::PreCheckGameRuleConditions() {
	// Exit intermission.
	if ( level.exitintermission ) {
		ExitLevel();
		return;
	}
}
/**
*	@brief	Called somewhere at the end of the game frame. This allows
*			to determine if conditions are met to engage into intermission
*			mode and/or exit the level.
**/
void svg_gamemode_cooperative_t::PostCheckGameRuleConditions() {

};

/**
*	@brief	Sets the spawn origin and angles to that matching the found spawn point.
**/
svg_base_edict_t *svg_gamemode_cooperative_t::SelectSpawnPoint( svg_player_edict_t *ent, Vector3 &origin, Vector3 &angles ) {
	svg_base_edict_t *spot = nullptr;

	// <Q2RTXP>: WID: Implement dmflags as gamemodeflags?
	//if ( (int)( dmflags->value ) & DF_SPAWN_FARTHEST ) {
	//	spot = SelectFarthestDeathmatchSpawnPoint();
	//} else {
		spot = SelectCoopSpawnPoint( ent );
	//}

	// Resort to seeking 'info_player_start' spot since the game modes found none.
	if ( !spot ) {
		spot = svg_gamemode_t::SelectSpawnPoint( ent, origin, angles );
	}

	// If we found a spot, set the origin and angles.
	if ( spot ) {
		origin = spot->s.origin;
		angles = spot->s.angles;
	} else {
		// Debug print if no spawn point was found.
		gi.dprintf( "%s: No spawn point found for player %s, using default origin and angles.\n", __func__, ent->client->pers.netname );
		origin = Vector3( 0, 0, 10 ); // Default origin.
		angles = Vector3( 0, 0, 0 ); // Default angles.
	}

	return spot;
}

/**
*	@brief  Will select a coop spawn point for the player.
**/
svg_base_edict_t *svg_gamemode_cooperative_t::SelectCoopSpawnPoint( svg_player_edict_t *ent ) {
	int     index;
	svg_base_edict_t *spot = nullptr;
	// WID: C++20: Added const.
	const char *target;

	index = g_edict_pool.NumberForEdict( ent ) - 1; // ent->client - game.clients;

	// player 0 starts in normal player spawn point
	if ( !index ) {
		return NULL;
	}
	spot = NULL;

	// assume there are four coop spots at each spawnpoint
	while ( 1 ) {
		spot = SVG_Entities_Find( spot, q_offsetof( svg_base_edict_t, classname ), "info_player_coop" );
		if ( !spot ) {
			return NULL;    // we didn't have enough...
		}
		target = (const char *)spot->targetname;
		if ( !target ) {
			target = "";
		}
		if ( Q_stricmp( game.spawnpoint, target ) == 0 ) {
			// this is a coop spawn point for one of the clients here
			index--;
			if ( !index ) {
				return spot;        // this is it
			}
		}
	}

	return spot;
}