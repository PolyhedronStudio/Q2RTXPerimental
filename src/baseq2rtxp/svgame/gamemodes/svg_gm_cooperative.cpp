/********************************************************************
*
*
*	ServerGame: GameMode Related Stuff.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_entity_events.h"
#include "svgame/svg_utils.h"

#include "svgame/player/svg_player_client.h"
#include "svgame/player/svg_player_events.h"
#include "svgame/player/svg_player_hud.h"
#include "svgame/player/svg_player_trail.h"
#include "svgame/player/svg_player_usetargets.h"
#include "svgame/player/svg_player_view.h"


#include "svgame/svg_lua.h"

#include "sharedgame/pmove/sg_pmove.h"
#include "sharedgame/sg_entities.h"
#include "sharedgame/sg_gamemode.h"
#include "sharedgame/sg_means_of_death.h"
#include "sharedgame/sg_muzzleflashes.h"
#include "sharedgame/sg_tempentity_events.h"

#include "svgame/svg_gamemode.h"
#include "svgame/gamemodes/svg_gm_basemode.h"
#include "svgame/gamemodes/svg_gm_cooperative.h"

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
    gi.cvar_forceset( "deathmatch", "0" );

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
			if ( ed != nullptr && ed->inUse && ed->client->pers.spectator ) {
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
	if ( ent->inUse == false ) {
		// clear the respawning variables
		SVG_Player_InitRespawnData( ent->client );
		if ( !game.autosaved || !ent->client->pers.weapon ) {
			SVG_Player_InitPersistantData( ent, ent->client );
		}
	}

    // Developer connection print.
    if ( game.maxclients >= 1 ) {
        gi.bprintf( PRINT_HIGH, "%s connected\n", ent->client->pers.netname );
    }

	// Connected.
	return true;
}
/**
*	@brief	Called when a a player purposely disconnects, or is dropped due to
*			connectivity problems.
**/
void svg_gamemode_cooperative_t::ClientDisconnect( svg_player_edict_t *ent ) {
    // Notify the game that the player has disconnected.
    gi.bprintf( PRINT_HIGH, "%s disconnected\n", ent->client->pers.netname );

    // Send 'Logout' -effect.
    if ( ent->inUse ) {
        gi.WriteUint8( svc_muzzleflash );
        gi.WriteInt16( g_edict_pool.NumberForEdict( ent ) );//gi.WriteInt16( ent - g_edicts );
        gi.WriteUint8( MZ_LOGOUT );
        gi.multicast( &ent->s.origin, MULTICAST_PVS, false );
    }
    // Unlink.
    gi.unlinkentity( ent );
    // Clear the entity.
    ent->s.modelindex = 0;
    ent->s.modelindex2 = 0;
    ent->s.sound = 0;
    ent->s.event = 0;
    ent->s.entityFlags = 0;
    ent->s.renderfx = 0;
    ent->s.solid = SOLID_NOT; // 0
    ent->s.entityType = ET_GENERIC;
    ent->solid = SOLID_NOT;
    ent->inUse = false;
    ent->classname = svg_level_qstring_t::from_char_str( "disconnected" );
    ent->client->pers.spawned = false;
    ent->client->pers.connected = false;
    ent->timestamp = level.time + 1_sec;

    // WID: This is now residing in RunFrame
    // FIXME: don't break skins on corpses, etc
    //playernum = ent-g_edicts-1;
    //gi.configstring (CS_PLAYERSKINS+playernum, "");
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
void svg_gamemode_cooperative_t::ClientSpawnInBody( svg_player_edict_t *ent ) {
    // Always clear out any possibly previous left over of the useTargetHint.
    SVG_Player_ClearUseTargetHint( ent, ent->client, nullptr );

    // find a spawn point
    // do it before setting health back up, so farthest
    // ranging doesn't count this client
    Vector3 spawn_origin = QM_Vector3Zero();
    Vector3 spawn_angles = QM_Vector3Zero();
    // Seek spawn 'spot' to position us on to.
    if ( !game.mode->SelectSpawnPoint( ent, spawn_origin, spawn_angles ) ) {
        // <Q2RTXP>: WID: TODO: Warn or error out, or just ignore it like it used to?
    }

    // Client Index.
    const int32_t index = g_edict_pool.NumberForEdict( ent ) - 1;//ent - g_edicts - 1;
    // GClient Ptr.
    svg_client_t *client = ent->client;

    // Assign the found spawnspot origin and angles.
    SVG_Util_SetEntityOrigin( ent, spawn_origin, true ); // VectorCopy( spawn_origin, ent->s.origin );
    VectorCopy( spawn_origin, client->ps.pmove.origin );


    // Get persistent user info and store it into a buffer.
    char    userinfo[ MAX_INFO_STRING ];
	std::memcpy( userinfo, client->pers.userinfo, sizeof( userinfo ) );
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
    // Acquire actual client number and eventSequence.
    const int32_t clientNum = client->clientNum;
    const int64_t eventSequence = client->ps.eventSequence;

    // Reset client value.
    *client = { }; //memset( client, 0, sizeof( *client ) );
	// Reassign the client number.
    client->clientNum = clientNum;
	// Reassign the event sequence.
    //client->ps.eventSequence = eventSequence;

    // Reinitialize persistent data.
    client->pers = savedPersistantData;
    // Restore the client's originally set respawn data.
    client->resp = savedRespawnData;
    // Make sure to restore event sequence.
    client->ps.eventSequence = eventSequence;

    // <Q2RTXP>: WID: TODO: Do we want this in singleplayer?
    // If dead at the time of the previous map switching to the current, reinitialize persistent data.
    if ( client->pers.health <= 0 ) {
        SVG_Player_InitPersistantData( ent, client );
    }
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
    ent->inUse = true;
    ent->classname = svg_level_qstring_t::from_char_str( "player" );
    ent->mass = 200;
    ent->gravity = 1.0f;
    ent->solid = SOLID_BOUNDS_BOX;
    ent->lifeStatus = LIFESTATUS_ALIVE;
    ent->air_finished_time = level.time + 12_sec;
    ent->clipMask = ( CM_CONTENTMASK_PLAYERSOLID );
    ent->model = svg_level_qstring_t::from_char_str( "players/playerdummy/tris.iqm" );
    ent->SetPainCallback( &svg_player_edict_t::onPain );//ent->SetPainCallback( player_pain );
    ent->SetDieCallback( &svg_player_edict_t::onDie );//ent->SetDieCallback( player_die );
    ent->liquidInfo.level = cm_liquid_level_t::LIQUID_NONE;
    ent->liquidInfo.type = CONTENTS_NONE;
    ent->flags &= ~FL_NO_KNOCKBACK;

    // Make sure it has no DEADMONSTER set anymore.
    ent->svFlags &= ~SVF_DEADENTITY;
    // Ensure it is a proper player entity.
    ent->svFlags |= SVF_PLAYER;
    ent->s.entityType = ET_PLAYER;

    // Copy in the bounds.
    VectorCopy( PM_BBOX_STANDUP_MINS, ent->mins );
    VectorCopy( PM_BBOX_STANDUP_MAXS, ent->maxs );
    // Set player state client number.
    //client->ps.clientNumber = index;

    // Ensure velocity is cleared.
    VectorClear( ent->velocity );

    // Reset PlayerState values.
    client->ps = {
		.clientNumber = (int16_t)clientNum,
        .eventSequence = eventSequence,
    };

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
    ent->s.entityFlags = 0;
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
		//VectorCopy( tr.endpos, ent->s.origin );
		SVG_Util_SetEntityOrigin( ent, tr.endpos, true );
		ent->groundInfo.entityNumber = tr.entityNumber;
	} else {
		SVG_Util_SetEntityOrigin( ent, spawn_origin + Vector3{ 0.f, 0.f, 10.f }, true ); //VectorCopy( spawn_origin, ent->s.origin );
		//ent->s.origin[ 2 ] += 10; // make sure off ground
	}

	// <Q2RTXP>: WID: Restore the origin.
	ent->s.old_origin = ent->currentOrigin;
	client->ps.pmove.origin = ent->currentOrigin;

	// <Q2RTXP>: WID: Link it to calculate absmins/absmaxs, this is to prevent actual
	// other entities from Spawn Touching.
	gi.linkentity( ent );

    // Ensure proper pitch and roll angles for calculating the delta angles.
    spawn_angles[ PITCH ] = 0;
    spawn_angles[ ROLL ] = 0;
    // Configure all spawn view angles.
	SVG_Util_SetEntityAngles( ent, spawn_angles, true );//ent->s.angles = spawn_angles;
    client->ps.viewangles = spawn_angles;
    client->viewMove.viewAngles = spawn_angles;
    // Set the delta angle
    client->ps.pmove.delta_angles = /*ANGLE2SHORT*/QM_Vector3AngleMod( spawn_angles - client->resp.cmd_angles );

    // Calculate anglevectors.
    AngleVectors( &client->viewMove.viewAngles.x, &client->viewMove.viewForward.x, nullptr, nullptr );

    #if 1
    // spawn a spectator
    if ( client->pers.spectator ) {
        client->chase_target = NULL;

        client->resp.spectator = true;

        ent->movetype = MOVETYPE_NOCLIP;
        ent->solid = SOLID_NOT;
        ent->svFlags |= SVF_NOCLIENT;
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

    // Presend anything else and clear state variables.
    SVG_Client_EndServerFrame( ent );
}
/**
*   @brief  Called when a client has finished connecting, and is ready
*           to be placed into the game. This will happen every level load.
**/
void svg_gamemode_cooperative_t::ClientBegin( svg_player_edict_t *ent ) {
    // Assign matching client for this entity.
    ent->client = &game.clients[ g_edict_pool.NumberForEdict( ent ) - 1 ]; //game.clients + ( ent - g_edicts - 1 );

    // [Paril-KEX] we're always connected by this point...
    ent->client->pers.connected = true;

    // Always clear out any previous left over useTargetHint stuff.
    SVG_Player_ClearUseTargetHint( ent, ent->client, nullptr );

    // We're spawned now of course.
    ent->client->resp.entertime = level.time;
    ent->client->pers.spawned = true;

    // If there is already a body waiting for us (a loadgame), just take it:
    if ( ent->inUse == true ) {
        // Spawn inside the body which was created during load time.
        SetupLoadGameBody( ent );
        // Otherwise spawn one from scratch:
    } else {
        // Create a new body for this player and spawn inside it.
        SetupNewGameBody( ent );
    }

    // If in intermission, move our client over into intermission state. (We connected at the end of a match).
    if ( level.intermissionFrameNumber ) {
        SVG_HUD_MoveClientToIntermission( ent );
        // Otherwise, send 'Login' effect even if NOT in a multiplayer game 
    } else {
        // 
        if ( game.maxclients >= 1 ) {
            SVG_Util_AddEvent( ent, EV_PLAYER_TELEPORT, 0 );
            gi.bprintf( PRINT_HIGH, "%s entered the game\n", ent->client->pers.netname );
        }
    }

    // Call upon EndServerFrame to make sure all view stuff is valid.
    SVG_Client_EndServerFrame( ent );

    // WID: LUA:
    SVG_Lua_CallBack_ClientEnterLevel( ent );
}


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
*	@brief	Overrides to update client scores when killing monsters in coop.
**/
void svg_gamemode_cooperative_t::EntityKilled( svg_base_edict_t *targ, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int32_t damage, Vector3 *point ) {
    if ( ( targ->svFlags & SVF_MONSTER ) && ( targ->lifeStatus != LIFESTATUS_DEAD ) ) {
        targ->svFlags |= SVF_DEADENTITY;   // now treat as a different content type
        // WID: TODO: Monster Reimplement.        
        //if (!(targ->monsterinfo.aiflags & AI_GOOD_GUY)) {
        //level.killed_monsters++;
        if ( attacker->client ) {
            attacker->client->resp.score++;
        }
        // medics won't heal monsters that they kill themselves
        //if ( strcmp( (const char *)attacker->classname, "monster_medic" ) == 0 )
        //    targ->owner = attacker;
        //}
    }

    // First call base method.
    svg_gamemode_t::EntityKilled( targ, inflictor, attacker, damage, point );
}


/**
*	@brief	Called somewhere at the beginning of the game frame. This allows
*			to determine if conditions are met to engage exitting intermission
*			mode and/or exit the level.
*	@return	False if conditions are not yet met to end the game, true otherwise.
**/
const bool svg_gamemode_cooperative_t::PreCheckGameRuleConditions() {
	// Exit intermission.
	if ( level.exitintermission ) {
		ExitLevel();
		return true;
	}
	// Never exit the level automatically in coop mode.
	return false;
}
/**
*	@brief	Called somewhere at the end of the game frame. This allows
*			to determine if conditions are met to engage into intermission
*			mode and/or exit the level.
**/
void svg_gamemode_cooperative_t::PostCheckGameRuleConditions() {

};

/**
*   @brief  This will be called once for all clients on each server frame, before running any other entities in the world.
**/
void svg_gamemode_cooperative_t::BeginServerFrame( svg_player_edict_t *ent ) {
    /**
    *   Opt out of function if we're in intermission mode.
    **/
    if ( level.intermissionFrameNumber ) {
        return;
    }

    /**
    *   Handle respawning as a spectating client:
    **/
    svg_client_t *client = ent->client;

    if ( client->pers.spectator != client->resp.spectator && ( level.time - client->respawn_time ) >= 5_sec ) {
        // Respawn as spectator.
        SVG_Client_RespawnSpectator( ent );
        return;
    }

    /**
    *   Run (+usetarget) logics.
    **/
    // Update the (+/-usetarget) key state actions if not done so already by ClientUserThink.
    if ( client->useTarget.tracedFrameNumber < level.frameNumber && !client->resp.spectator ) {
        SVG_Player_TraceForUseTarget( ent, client, false );
    } else {
        client->useTarget.tracedFrameNumber = level.frameNumber;
    }

    /**
    *   Run weapon logic if it hasn't been done by a usercmd_t in ClientThink.
    **/
    if ( client->weapon_thunk == false && !client->resp.spectator ) {
        SVG_Player_Weapon_Think( ent, false );
    } else {
        client->weapon_thunk = false;
    }

    /**
    *   If dead, check for any user input after the client's respawn_time has expired.
    **/
    if ( ent->lifeStatus != LIFESTATUS_ALIVE ) {
        // wait for any button just going down
        if ( level.time > client->respawn_time ) {
            // in coop, only wait for attack button
            const int32_t buttonMask = BUTTON_PRIMARY_FIRE;

            if ( ( client->latched_buttons & buttonMask ) ||( (int)dmflags->value & DF_FORCE_RESPAWN ) ) {
                // respawn as player.
                SVG_Client_RespawnPlayer( ent );
                // Unlatch.
                client->latched_buttons = BUTTON_NONE;
            }
        }
        return;
    }

    /**
    *   Add player trail so monsters can follow
    **/
    //// WID: TODO: Monster Reimplement.
    if ( !SVG_Entity_IsVisible( ent, PlayerTrail_LastSpot() ) ) {
    	PlayerTrail_Add( ent->s.old_origin );
    }

    /**
    *   UNLATCH ALL LATCHED BUTTONS:
    **/
    client->latched_buttons = BUTTON_NONE;
}
/**
*	@brief	Called for each player at the end of the server frame, also right after spawning as well to finalize the view.
**/
void svg_gamemode_cooperative_t::EndServerFrame( svg_player_edict_t *ent ) {
    //
    // If the end of unit layout is displayed, don't give
    // the player any normal movement attributes
    //
    if ( level.intermissionFrameNumber ) {
        // FIXME: add view drifting here?
        ent->client->ps.screen_blend[ 3 ] = 0;
        ent->client->ps.fov = 90;
        SVG_HUD_SetStats( ent );
        return;
    }

    //// Calculate angle vectors.
    //Vector3 vForward = QM_Vector3Zero();
    //Vector3 vRight = QM_Vector3Zero();
    //Vector3 vUp = QM_Vector3Zero();
    //AngleVectors( &ent->client->viewMove.viewAngles.x, forward, vRight, vUp );

    // burn from lava, etc
    P_CheckWorldEffects();

    //
    // set model angles from view angles so other things in
    // the world can tell which direction you are looking
    //
    if ( ent->client->viewMove.viewAngles[ PITCH ] > 180. ) {
        ent->s.angles[ PITCH ] = ( -360. + ent->client->viewMove.viewAngles[ PITCH ] ) / 3.;
    } else {
        ent->s.angles[ PITCH ] = ent->client->viewMove.viewAngles[ PITCH ] / 3.;
    }
    ent->s.angles[ YAW ] = ent->client->viewMove.viewAngles[ YAW ];
    ent->s.angles[ ROLL ] = 0;
    ent->s.angles[ ROLL ] = SV_CalcRoll( ent->s.angles, ent->velocity ) * 4.;

    // apply all the damage taken this frame
    P_DamageFeedback( ent );

    // determine the view offsets
    P_CalculateViewOffset( ent );

    // WID: Moved to CLGame.
    // determine the gun offsets
    //SV_CalculateGunOffset( ent );

    // Determine the full screen color blend which must happen after applying viewoffset, 
    // so eye contents can be accurately determined.
    //----
    // FIXME: with client prediction, the contents should be determined by the client.
	// <Q2RTXP>: WID: Fixed, we only apply external blends here, the pmove will apply the rest.
    //----
    P_CalculateBlend( ent );

    // Different layout when spectating.
    if ( ent->client->resp.spectator ) {
        SVG_HUD_SetSpectatorStats( ent );
        // Regular layout.
    } else {
        SVG_HUD_SetStats( ent );
    }
    // Set stats to that of the entity which we're tracing. (Overriding previously set stats.)
    SVG_HUD_CheckChaseStats( ent );
    // Events.
    SVG_SetClientEvent( ent );
    // Effects.
    SVG_SetClientEffects( ent );
    // Sound.
    SVG_SetClientSound( ent );
    // Check for client playerstate its pmove generated events.
    //SVG_PlayerState_CheckForEvents( ent, &ent->client->ops, &ent->client->ps );
    // Animation Frame.
    SVG_SetClientFrame( ent );

    // Backup velocity, viewangles, and ground entity.
    ent->client->oldvelocity = ent->velocity;
    ent->client->oldviewangles = ent->client->ps.viewangles;
    ent->client->oldgroundentity = g_edict_pool.EdictForNumber( ent->groundInfo.entityNumber );

    // Clear out the weapon kicks.
    ent->client->weaponKicks = {};

    // <Q2RTXP>: WID: Happens outside in SVG_Client_EndServerFrame.
    // Convert certain playerstate properties into entity state properties.
    //SG_PlayerStateToEntityState( ent->client->clientNum, &ent->client->ps, &ent->s, false );

    // <Q2RTXP?: WID: Dun have this in SP mode.
    #if 1
    // If the scoreboard is up, update it.
    if ( ent->client->showscores && !( level.frameNumber & 63 ) ) {
        SVG_HUD_DeathmatchScoreboardMessage( ent, ent->enemy, false );
    }
    #endif // #if 0
}


/**
*   @brief  This function is used to apply damage to an entity.
*           It handles the damage calculation, knockback, and any special
*           effects based on the type of damage and the entities involved.
**/
void svg_gamemode_cooperative_t::DamageEntity( svg_base_edict_t *targ, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, const Vector3 &dir, Vector3 &point, const Vector3 &normal, const int32_t damage, const int32_t knockBack, const entity_damageflags_t damageFlags, const sg_means_of_death_t meansOfDeath ) {
    // Final means of death.
    sg_means_of_death_t finalMeansOfDeath = meansOfDeath;

    // No use if we aren't accepting damage.
    if ( !targ->takedamage ) {
        return;
    }

    // easy mode takes half damage
    int32_t finalDamage = damage;
    if ( skill->value == 0 && targ->client ) {
        finalDamage *= 0.5f;
        if ( !finalDamage )
            finalDamage = 1;
    }

    // Friendly fire avoidance.
    // If enabled you can't hurt teammates (but you can hurt yourself)
    // Knockback still occurs.
    if ( ( targ != attacker ) && ( /*( deathmatch->value && ( (int)( dmflags->value ) & ( DF_MODELTEAMS | DF_SKINTEAMS ) ) )|| ( coop->value && */ ( targ->client ) ) ) {
        if ( SVG_OnSameTeam( targ, attacker ) ) {
            if ( (int)( dmflags->value ) & DF_NO_FRIENDLY_FIRE ) {
                finalDamage = 0;
            } else {
                finalMeansOfDeath |= MEANS_OF_DEATH_FRIENDLY_FIRE;
            }
        }
    }

    targ->meansOfDeath = finalMeansOfDeath;

    svg_client_t *client = targ->client;

	// Default TE that got us was sparks.
	sg_entity_events_t te_sparks = EV_FX_IMPACT_SPARKS;
	// Special sparks for a bullet.
	if ( damageFlags & DAMAGE_BULLET ) {
		te_sparks = EV_FX_IMPACT_BULLET_SPARKS;
	}

    // Bonus damage for suprising a monster.
    if ( !( damageFlags & DAMAGE_RADIUS ) && ( targ->svFlags & SVF_MONSTER )
        && ( attacker->client ) && ( !targ->enemy ) && ( targ->health > 0 ) ) {
        finalDamage *= 2;
    }

    #if 0
    // Determine knockback value.
    int32_t finalKnockBack = ( /*targ->flags & FL_NO_KNOCKBACK ? 0 : */ knockBack );
    if ( ( targ->flags & FL_NO_KNOCKBACK ) ) {//||
        //( /*( targ->flags & FL_ALIVE_KNOCKBACK_ONLY ) &&*/
        //    ( !targ->lifeStatus || targ->death_time != level.time ) ) ) {
        finalKnockBack = 0;
    }
    #else
    // Determine knockback value.
    int32_t finalKnockBack = ( /*targ->flags & FL_NO_KNOCKBACK ? 0 : */ knockBack );
    if ( targ->flags & FL_NO_KNOCKBACK ||
        ( /*( targ->flags & FL_ALIVE_KNOCKBACK_ONLY ) &&*/
            ( !( targ->lifeStatus == entity_lifestatus_t::LIFESTATUS_ALIVE ) )
            /*|| ( targ->death_time != 0_ms && targ->death_time < level.time )*/
            ) ) {
        finalKnockBack = 0;
    }
    #endif
    // Figure momentum add.
    if ( !( damageFlags & DAMAGE_NO_KNOCKBACK ) ) {
        if ( ( finalKnockBack ) && ( targ->movetype != MOVETYPE_NONE ) && ( targ->movetype != MOVETYPE_BOUNCE )
            && ( targ->movetype != MOVETYPE_PUSH ) && ( targ->movetype != MOVETYPE_STOP ) ) {
            Vector3 normalizedDir = QM_Vector3Normalize( dir );
            Vector3 kvel;
            float   mass;

            if ( targ->mass < 50 ) {
                mass = 50;
            } else {
                mass = targ->mass;
            }

            // <Q2RTXP>: WID: This will do the classic "rocket jump" engagement when a
            // player is the actual cause of the explosion itself.
            if ( targ->client && attacker == targ ) {
                kvel = normalizedDir * ( 1600.0f * (float)knockBack / mass );
            } else {
                kvel = normalizedDir * ( 500.0f * (float)knockBack / mass );
            }
            VectorAdd( targ->velocity, kvel, targ->velocity );
        }
    }

    int32_t take = finalDamage;
    int32_t save = 0;

    // check for godmode
    if ( ( targ->flags & FL_GODMODE ) && !( damageFlags & DAMAGE_NO_PROTECTION ) ) {
        take = 0;
        save = finalDamage;
		SVG_TempEventEntity_GunShot( point, normal, te_sparks );
    }

    // check for invincibility
    //if ((client && client->invincible_time > level.time) && !(dflags & DAMAGE_NO_PROTECTION)) {
    //    if (targ->pain_debounce_time < level.time) {
    //        gi.sound(targ, CHAN_ITEM, gi.soundindex("items/protect4.wav"), 1, ATTN_NORM, 0);
    //        targ->pain_debounce_time = level.time + 2_sec;
    //    }
    //    take = 0;
    //    save = damage;
    //}

    //treat cheat/powerup savings the same as armor
    int32_t asave = save;

    // team damage avoidance
    if ( !( damageFlags & DAMAGE_NO_PROTECTION ) && SVG_CheckTeamDamage( targ, attacker ) ) {
        return;
    }

    // do the damage
    if ( take ) {
        if ( ( targ->svFlags & SVF_MONSTER ) || ( client ) ) {
            // SVG_SpawnDamage(TE_BLOOD, point, normal, take);
            //SVG_SpawnDamage( EV_FX_BLOOD, point, dir, take );
			SVG_TempEventEntity_Blood( point, normal, take );
		} else {
			//SVG_SpawnDamage( te_sparks, point, normal, take );
			SVG_TempEventEntity_GunShot( point, normal, te_sparks );
		}
        targ->health = targ->health - take;

        if ( targ->health <= 0 ) {
            if ( ( targ->svFlags & SVF_MONSTER ) || ( client ) ) {
                //targ->flags |= FL_NO_KNOCKBACK;
                //targ->flags |= FL_ALIVE_KNOCKBACK_ONLY;
                //targ->death_time = level.time;
            }
            #if 0
            targ->monsterinfo.damage_blood += take;
            targ->monsterinfo.damage_attacker = attacker;
            targ->monsterinfo.damage_inflictor = inflictor;
            targ->monsterinfo.damage_from = point;
            targ->monsterinfo.damage_mod = meansOfDeath;
            targ->monsterinfo.damage_knockback += knockBack;
            #endif
            game.mode->EntityKilled( targ, inflictor, attacker, take, &point );
            return;
        }
    }

    if ( targ->svFlags & SVF_MONSTER ) {
        if ( damage > 0 ) {
            //M_ReactToDamage( targ, attacker );

            # if 0
            float kick = (float)abs( finalKnockBack );
            if ( kick && targ->health > 0 ) { // kick of 0 means no view adjust at all
                //kick = kick * 100 / targ->health;

                if ( kick < damage * 0.5f ) {
                    kick = damage * 0.5f;
                }
                if ( kick > 50 ) {
                    kick = 50;
                }

                Vector3 knockBackVelocity = targ->s.origin - Vector3( inflictor ? inflictor->s.origin : attacker->s.origin );
                knockBackVelocity = QM_Vector3Normalize( knockBackVelocity );
                //knockBackVelocity *= kick;
                targ->velocity = QM_Vector3MultiplyAdd( knockBackVelocity, kick, targ->velocity );
                //targ->velocity += knockBackVelocity;

            }
            #endif
        }
        // WID: TODO: Monster Reimplement.
        //if (!(targ->monsterinfo.aiflags & AI_DUCKED) && (take)) {
        if ( take ) {
            if ( targ->HasPainCallback() ) {
                targ->DispatchPainCallback( attacker, finalKnockBack, take, damageFlags );
                // nightmare mode monsters don't go into pain frames often
                if ( skill->value == 3 )
                    targ->pain_debounce_time = level.time + 5_sec;
            } else {
                #ifdef WARN_ON_TRIGGERDAMAGE_NO_PAIN_CALLBACK
                gi.bprintf( PRINT_WARNING, "%s: ( targ->pain == nullptr )!\n", __func__ );
                #endif
            }
        }
        //}
    } else if ( client ) {
        if ( !( targ->flags & FL_GODMODE ) && ( take ) ) {
            if ( targ->HasPainCallback() ) {
                targ->DispatchPainCallback( attacker, finalKnockBack, take, damageFlags );
            } else {
                #ifdef WARN_ON_TRIGGERDAMAGE_NO_PAIN_CALLBACK
                gi.bprintf( PRINT_WARNING, "%s: ( targ->pain == nullptr )!\n", __func__ );
                #endif
            }
        }
    } else if ( take ) {
        if ( targ->HasPainCallback() ) {
            targ->DispatchPainCallback( attacker, finalKnockBack, take, damageFlags );
        } else {
            #ifdef WARN_ON_TRIGGERDAMAGE_NO_PAIN_CALLBACK
            gi.bprintf( PRINT_WARNING, "%s: ( targ->pain == nullptr )!\n", __func__ );
            #endif
        }
    }


    // add to the damage inflicted on a player this frame
    // the total will be turned into screen blends and view angle kicks
    // at the end of the frame
    if ( client ) {
        client->frameDamage.armor += asave;
        client->frameDamage.blood += take;
        client->frameDamage.knockBack += finalKnockBack;
        VectorCopy( point, client->frameDamage.from );
        //client->last_damage_time = level.time + COOP_DAMAGE_RESPAWN_TIME;

        if ( !( damageFlags & DAMAGE_NO_INDICATOR )
            && inflictor != world && attacker != world
            && ( take || asave ) ) {
            damage_indicator_t *indicator = nullptr;
            size_t i;

            for ( i = 0; i < client->frameDamage.num_damage_indicators; i++ ) {
                const float length = QM_Vector3Length( point - client->frameDamage.damage_indicators[ i ].from );
                if ( length < 32.f ) {
                    indicator = &client->frameDamage.damage_indicators[ i ];
                    break;
                }
            }

            if ( !indicator && i != MAX_DAMAGE_INDICATORS ) {
                indicator = &client->frameDamage.damage_indicators[ i ];
                // for projectile direct hits, use the attacker; otherwise
                // use the inflictor (rocket splash should point to the rocket)
                indicator->from = ( damageFlags & DAMAGE_RADIUS && inflictor != nullptr ) ? inflictor->s.origin : attacker->s.origin;
                indicator->health = indicator->armor = 0;
                client->frameDamage.num_damage_indicators++;
            }

            if ( indicator ) {
                indicator->health += take;
                indicator->armor += asave;
            }
        }
    }
}


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


/**
*   @brief  A client connected by loadgame(assuming singleplayer), there is already
*           a body waiting for us to use. We just need to adjust its angles.
**/
void svg_gamemode_cooperative_t::SetupLoadGameBody( svg_player_edict_t *ent ) {
    // The client has cleared the client side viewangles upon
    // connecting to the server, which is different than the
    // state when the game is saved, so we need to compensate
    // with deltaangles
    ent->client->ps.pmove.delta_angles = /*ANGLE2SHORT*/QM_Vector3AngleMod( ent->client->ps.viewangles );
    // Make sure classname is player.
    ent->classname = svg_level_qstring_t::from_char_str( "player" );
    // Make sure entity type is player.
    ent->s.entityType = ET_PLAYER;
    // Ensure proper player flag is set.
    ent->svFlags |= SVF_PLAYER;
}
/**
*   @brief  There is no body waiting for us yet, so (re-)initialize the entity we have with a full new 'body'.
**/
void svg_gamemode_cooperative_t::SetupNewGameBody( svg_player_edict_t *ent ) {
    /**
	*	A spawn point will completely reinitialize the entity
    *	except for the persistant data that was initialized at
    *	connect time
	**/
	// Re-initialize the edict.
    g_edict_pool._InitEdict( ent, ent->s.number );
    // Make sure classname is player.
    ent->classname = svg_level_qstring_t::from_char_str( "player" );;
    // Make sure entity type is player.
    ent->s.entityType = ET_PLAYER;
    // Ensure proper player flag is set.
    ent->svFlags |= SVF_PLAYER;

    // Initialize respawn data.
    SVG_Player_InitRespawnData( ent->client );
    // Actually finds a spawnpoint and places the 'body' into the game.
    SVG_Player_SpawnInBody( ent );
}
