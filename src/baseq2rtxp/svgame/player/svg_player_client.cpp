/********************************************************************
*
*
*	ServerGame: Generic Client EntryPoints
*
*
********************************************************************/
#include "svgame/svg_local.h"

#include "sharedgame/sg_entity_flags.h"
#include "sharedgame/sg_entities.h"
#include "sharedgame/sg_gamemode.h"
#include "sharedgame/sg_means_of_death.h"
#include "sharedgame/sg_muzzleflashes.h"
#include "sharedgame/pmove/sg_pmove.h"
#include "sharedgame/sg_usetarget_hints.h"

#include "svgame/svg_commands_server.h"
#include "svgame/svg_misc.h"
#include "svgame/svg_utils.h"

#include "svgame/svg_gamemode.h"
#include "svgame/gamemodes/svg_gm_basemode.h"

#include "svgame/player/svg_player_client.h"
#include "svgame/player/svg_player_hud.h"
#include "svgame/player/svg_player_view.h"

#include "svgame/entities/svg_player_edict.h"

#include "svgame/svg_lua.h"


/**
*   @brief
**/
void SVG_Client_UserinfoChanged( svg_base_edict_t *ent, char *userinfo );

/**
*   @brief  Will process(progress) the entity's active animations for each body state and event states.
**/
void SVG_P_ProcessAnimations( svg_base_edict_t *ent );



/***
*
*
*
*   Player(client) -Data: Save and Restore, and Persistent Data(Session Data).
*
*
*
***/
/**
*   @brief  For SinglePlayer: Called only once, at game first initialization.
*           For Multiplayer Modes: Called after each death, and level change.
**/
void SVG_Player_InitPersistantData( svg_base_edict_t *ent, svg_client_t *client ) {
    // Clear out persistent data.
    client->pers = {};

    // Find the fists item, add it to our inventory.
    const gitem_t *item_fists = SVG_Item_FindByPickupName( "Fists" );
    client->pers.selected_item = ITEM_INDEX( item_fists );
    client->pers.inventory[ client->pers.selected_item ] = 1;

    // Find the Pistol item, add it to our inventory and appoint it as the selected weapon.
    const gitem_t *item_pistol = SVG_Item_FindByPickupName( "Pistol" );
    client->pers.selected_item = ITEM_INDEX( item_pistol );
    client->pers.inventory[ client->pers.selected_item ] = 1;
    // Assign it as our selected weapon.
    client->pers.weapon = item_pistol;
    // Give it a single full clip of ammo.
    client->pers.weapon_clip_ammo[ client->pers.weapon->weapon_index ] = item_pistol->clip_capacity;
    // And some extra bullets to reload with.
    ent->client->ammo_index = ITEM_INDEX( SVG_Item_FindByPickupName( ent->client->pers.weapon->ammo ) );
    client->pers.inventory[ ent->client->ammo_index ] = 78;

    // Obviously we need to allow this.
    client->weaponState.canChangeMode = false;

    // Health.
    client->pers.health = 100;
    client->pers.max_health = 100;
    // Armor.
    client->pers.armor = 100;
    client->pers.max_armor = 100;

    // Ammo capacities.
    client->pers.ammoCapacities.pistol = 120;
    client->pers.ammoCapacities.rifle = 180;
    client->pers.ammoCapacities.smg = 250;
    client->pers.ammoCapacities.sniper = 80;
    client->pers.ammoCapacities.shotgun = 100;

    // Connected, and spawned!
    client->pers.connected = true;
    client->pers.spawned = true;
}
/**
*   @brief  Clears respawnable client data, which stores the timing of respawn as well as a copy of
*           the client persistent data, to reapply after respawn.
**/
void SVG_Player_InitRespawnData( svg_client_t *client ) {
    // Clear out respawn data.
    client->resp = {};

    // Save the moment in time.
    client->resp.enterframe = level.frameNumber;
    client->resp.entertime = level.time;

    // We make sure to store the  'persistent across level changes' data into the client's
    // persistent respawn field, so we can restore it each respawn(cooperative/sp).
    client->resp.pers_respawn = client->pers;
}

/**
*    @brief  Some information that should be persistant, like health, is still stored in the edict structure.
*            So it needs to be mirrored out to the client structure before all the edicts are wiped.
**/
void SVG_Player_SaveClientData( void ) {
    // Iterate the clients.
    for ( int32_t i = 0; i < game.maxclients; i++ ) {
        // Get entity for this client.
        svg_base_edict_t *ent = g_edict_pool.EdictForNumber( i + 1 );//g_edicts[ 1 + i ];
        // If the entity is not valid, or not in use, skip it.
        if ( !ent || !ent->inUse ) {
            continue;
        }
        // Store the persistent data into the client structure.
        game.clients[ i ].pers.health = ent->health;
        game.clients[ i ].pers.max_health = ent->max_health;

        game.clients[ i ].pers.armor = ent->armor;
        game.clients[ i ].pers.max_armor = ent->max_armor;

        game.clients[ i ].pers.savedFlags = static_cast<entity_flags_t>( ent->flags & ( FL_GODMODE | FL_NOTARGET /*| FL_POWER_ARMOR*/ ) );
        // Store the respawn score.
        if ( coop->value ) {
            game.clients[ i ].pers.score = ent->client->resp.score;
        }
    }
}
/**
*   @brief  Restore the client stored persistent data to reinitialize several client entity fields.
**/
void SVG_Player_RestoreClientData( svg_base_edict_t *ent ) {
    // Restore the persistent data from the client structure to the entity.
    ent->health = ent->client->pers.health;
    ent->max_health = ent->client->pers.max_health;

    ent->armor = ent->client->pers.armor;
    ent->max_armor = ent->client->pers.max_armor;

    ent->flags |= ent->client->pers.savedFlags;
    // If we are in cooperative mode, restore the score.
    if ( coop->value ) {
        ent->client->resp.score = ent->client->pers.score;
    }
}

/**
*   @brief  Will reset the entity client's 'Field of View' back to its defaults.
**/
void SVG_Player_ResetPlayerStateFOV( svg_client_t *client ) {
    // For DM Mode, possibly fixed FOV is set.
    if ( /*deathmatch->value && */( (int)dmflags->value & DF_FIXED_FOV ) ) {
        client->ps.fov = 90;
        // Otherwise:
    } else {
        // Get the actual integral FOV value.
        client->ps.fov = atoi( Info_ValueForKey( client->pers.userinfo, "fov" ) );
        // Make sure it is sane and within 'bounds'.
        if ( client->ps.fov < 1 ) {
            client->ps.fov = 90;
        } else if ( client->ps.fov > 160 ) {
            client->ps.fov = 160;
        }
    }
}



/**
*
*
*
*   Client Recoil Utilities:
*
*
*
**/
/**
*   @brief  Calculates the final resulting recoil value, being clamped between -1, to +1.
**/
const double SVG_Client_GetFinalRecoilFactor( svg_base_edict_t *ent ) {
    // Get the movement induced recoil factor.
    const double movementRecoil = QM_Clamp( ent->client->weaponState.recoil.moveFactor, -1., 1. );
    // Get the fire induced recoil factor.
    const double fireRecoil = ent->client->weaponState.recoil.weaponFactor;
    // Determine the final recoil factor and clamp it between -1, to +1.
    const double recoilFactor = QM_Clamp( movementRecoil + fireRecoil, -1., 2. );
    // Clamp the recoil factor between -1, to +1.
    // Return the final recoil factor.
    return recoilFactor;
}



/**
*
*
*
*   Client pending Predictable playerState Events:
*
*
*
**/
/**
*   @brief  Sends any pending of the remaining predictable events to all clients, except "ourselves".
**/
void SVG_Client_SendPendingPredictableEvents( svg_player_edict_t *ent, svg_client_t *client ) {
	player_state_t *ps = &client->ps;

	// Check for pending events.
    if ( ps->entityEventSequence < ps->eventSequence ) {
        // Create a temporary entity for this event which is send to all clients
        // except "ourselves", who generated the event.
        const int64_t seq = ps->entityEventSequence & ( MAX_PS_EVENTS - 1 );
		const sg_entity_events_t event = ( sg_entity_events_t )(ps->events[ seq ] | ( ( ps->entityEventSequence & 3 ) << 8ULL ));
		const int32_t eventParm0 = ps->eventParms[ seq ];

		// Backup the external event before converting playerstate to entitystate.
		const sg_entity_events_t externalEvent = ( sg_entity_events_t )ps->externalEvent;
		// Reset external event.
        ps->externalEvent = EV_NONE;
		// Create a temporary entity event for all other clients.
		svg_base_edict_t *tempEventEntity = SVG_Util_CreateTempEventEntity( 
            ps->pmove.origin, 
            event, eventParm0,
            0/*client->clientNum + 1*/,
            true 
        );

        // This seems to be ok.
        #if 1
		    // Store number for restoration later.
            const int32_t tempEventEntityNumber = tempEventEntity->s.number;
		    // Convert certain playerstate properties into entity state properties.
            SG_PlayerStateToMinimalEntityState( client->clientNum, ps, &tempEventEntity->s, true );
            // Restore the number.
            tempEventEntity->s.number = tempEventEntityNumber;
		    // No model for temp event entities.
		    tempEventEntity->s.modelindex = 0; // Undo the model just in case. (Set by SG_PlayerStateToEntityState).
        // This will send it with the model and no frame, tposing it hard.
        #else
            // Store number for restoration later.
            const int32_t tempEventEntityNumber = tempEventEntity->s.number;
            // Convert certain playerstate properties into entity state properties.
            SG_PlayerStateToEntityState( client->clientNum, ps, &tempEventEntity->s, true );
            // Restore the number.
            tempEventEntity->s.number = tempEventEntityNumber;
        #endif
        // Adjust type, assign EF_ENTITY_EVENT_TARGET_OTHER event flag.
        tempEventEntity->s.entityType = ET_TEMP_ENTITY_EVENT + event;
		tempEventEntity->s.entityFlags |= EF_ENTITY_EVENT_TARGET_OTHER;
		// Set other entity number to that of our client.
        tempEventEntity->s.otherEntityNumber = ent->s.number;
		// Set eventParm from playerstate.
		tempEventEntity->s.eventParm0 = eventParm0;
        tempEventEntity->s.eventParm1 = 0; // eventParm1.
		// Mark as exclusion to be sent our own client self.
		tempEventEntity->svFlags |= SVF_SENDCLIENT_EXCLUDE_ID;
		tempEventEntity->sendClientID = client->clientNum;

        // Set back the external event.
		ps->externalEvent = externalEvent;
    }
}



/**
*
*
*
*   Client Generic Functions:
*
*
*
**/
/**
*   @brief  Called either when a player connects to a server, OR to respawn in a multiplayer game.
*
*           Will look up a spawn point, spawn(and place) the player 'body' into the server, where necessary
*           (re-)initializing saved entity and persistant data. 
*           (This includes actually raising the weapon up.)
**/
void SVG_Player_SpawnInBody( svg_base_edict_t *ent ) {
    // Ensure we are dealing with a player entity here.
    if ( !ent->GetTypeInfo()->IsSubClassType<svg_player_edict_t>() ) {
        gi.dprintf( "%s: Not a player entity.\n", __func__ );
        return;
    }

    // Pass to game mode.
    game.mode->ClientSpawnInBody( static_cast<svg_player_edict_t *>( ent ) );

    // Convert certain playerstate properties into entity state properties.
    SG_PlayerStateToEntityState( ent->client->clientNum, &ent->client->ps, &ent->s, true, false );
}



/**
*
*
*
*   Begin/Connect/Disconnect:
*
*
*
**/
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
const bool SVG_Client_Connect( svg_base_edict_t *ent, char *userinfo ) {
	// Ensure it is a player entity.
    if ( !ent->GetTypeInfo()->IsSubClassType<svg_player_edict_t>() ) {
        gi.dprintf( "SVG_Client_Connect: Not a player entity.\n" );
        return false;
	}

    // Check to see if they are on the banned IP list.
    char *value = Info_ValueForKey( userinfo, "ip" );
    if ( SVG_FilterPacket( value ) ) {
        Info_SetValueForKey( userinfo, "rejmsg", "Banned." );
        // Refused.
        return false;
    }

	// Pass over control to the gamemode.
    if ( !game.mode->ClientConnect( static_cast<svg_player_edict_t*>( ent ), userinfo ) ) {
        // Refused.
        return false;
	}

	// Connected, so set the userinfo.
    // make sure we start with known default(s)
    SVG_Client_UserinfoChanged( ent, userinfo );

    // Make sure we start with known default(s):
    // We're a player.
    ent->svFlags = SVF_PLAYER;
    ent->s.entityType = ET_PLAYER;

    // We're connected.
    ent->client->pers.connected = true;

    // Connected.
    return true;
}

/**
*   @brief  Called when a client has finished connecting, and is ready
*           to be placed into the game. This will happen every level load,
* 		    including loadgames.
**/
void SVG_Client_Begin( svg_base_edict_t *ent ) {
    // Sanity checks.
    if ( !ent->client ) {
		gi.dprintf( "%s: No client data.\n", __func__ );
        return;
    }
    // Ensure we are dealing with a player entity here.
    if ( !ent->GetTypeInfo()->IsSubClassType<svg_player_edict_t>() ) {
        gi.dprintf( "%s: Not a player entity.\n", __func__ );
        return;
    }

    game.mode->ClientBegin( static_cast<svg_player_edict_t *>( ent ) );
}

/**
*   @brief  Called when a player drops from the server.
*           Will NOT be called between levels.
**/
void SVG_Client_Disconnect( svg_base_edict_t *ent ) {
    if ( !ent->client ) {
        return;
    }
    // Ensure we are dealing with a player entity here.
    if ( !ent->GetTypeInfo()->IsSubClassType<svg_player_edict_t>() ) {
        gi.dprintf( "%s: Not a player entity.\n", __func__ );
        return;
    }

    // WID: LUA:
    SVG_Lua_CallBack_ClientExitLevel( ent );

    // Pass on to the game mode.
    game.mode->ClientDisconnect( static_cast<svg_player_edict_t*>( ent ) );
}



/**
*
*
* 
*   Client Userinfo:
*
* 
*
**/
/**
*   @brief  called whenever the player updates a userinfo variable.
*
*           The game can override any of the settings in place
*           (forcing skins or names, etc) before copying it off.
**/
void SVG_Client_UserinfoChanged( svg_base_edict_t *ent, char *userinfo ) {
    // Check for malformed or illegal info strings
    if ( !Info_Validate( userinfo ) ) {
		// Overwrite the userinfo with a default value that is not malformed.
        strcpy( userinfo, "\\name\\badinfo\\skin\\testdummy/skin" );
    }

    // Make sure we got game mode.
    if ( !game.mode ) {
        gi.dprintf( "SVG_Client_UserinfoChanged: No game mode set.\n" );
        return;
	}
	// Make sure entity is a player entity.
    if ( !ent->GetTypeInfo()->IsSubClassType<svg_player_edict_t>() ) {
        gi.dprintf( "SVG_Client_UserinfoChanged: Not a player entity.\n" );
        return;
    }

    // Set name
    char *s = Info_ValueForKey( userinfo, "name" );
    Q_strlcpy( ent->client->pers.netname, s, sizeof( ent->client->pers.netname ) );

    // Give the game mode a chance to deal with the user info.
    game.mode->ClientUserinfoChanged( static_cast<svg_player_edict_t*>( ent ), userinfo );

    // Save off the userinfo in case we want to check something later.
    Q_strlcpy( ent->client->pers.userinfo, userinfo, sizeof( ent->client->pers.userinfo ) );
}



/**
*
*
* 
*   Respawns:
*
* 
*
**/
/**
*   @brief  
**/
void SVG_Client_RespawnPlayer( svg_base_edict_t *self ) {
    if ( deathmatch->value || coop->value ) {
        // Spectator's don't leave bodies
        if ( self->movetype != MOVETYPE_NOCLIP ) {
            SVG_Entities_BodyQueueAddForPlayer( self );
        }

		// Re-ensure we're sending to clients again.
        self->svFlags &= ~SVF_NOCLIENT;
		// Spawn at a spawn point
        SVG_Player_SpawnInBody( self );

        // Add a teleportation effect
        //self->s.event = EV_PLAYER_TELEPORT;
		SVG_Util_AddEvent( self, EV_PLAYER_TELEPORT, 0 );

        // Set the time of the current respawn.
        self->client->respawn_time = level.time;

        // Hold in place briefly
        self->client->ps.pmove.pm_flags = PMF_TIME_TELEPORT;
        self->client->ps.pmove.pm_time = 14; // WID: 40hz: Q2RE has 112 here.

        return;
    }

    // restart the entire server
    gi.AddCommandString( "pushmenu loadgame\n" );
}

/**
*   @brief  Only called when pers.spectator changes.
*   @note   That resp.spectator should be the opposite of pers.spectator here
**/
void SVG_Client_RespawnSpectator( svg_base_edict_t *ent ) {
    int i, numspec;

    // if the user wants to become a spectator, make sure he doesn't
    // exceed max_spectators

    if ( ent->client->pers.spectator ) {
        char *value = Info_ValueForKey( ent->client->pers.userinfo, "spectator" );
        if ( *spectator_password->string &&
            strcmp( spectator_password->string, "none" ) &&
            strcmp( spectator_password->string, value ) ) {
            gi.cprintf( ent, PRINT_HIGH, "Spectator password incorrect.\n" );
            ent->client->pers.spectator = false;
            gi.WriteUint8( svc_stufftext );
            gi.WriteString( "spectator 0\n" );
            gi.unicast( ent, true );
            return;
        }

        // count spectators
        for ( i = 1, numspec = 0; i <= maxclients->value; i++ ) {
            svg_base_edict_t *ed = g_edict_pool.EdictForNumber( i );
            if ( ed != nullptr && ed->inUse && ed->client->pers.spectator ) {
                numspec++;
            }
        }

        if ( numspec >= maxspectators->value ) {
            gi.cprintf( ent, PRINT_HIGH, "Server spectator limit is full." );
            ent->client->pers.spectator = false;
            // reset his spectator var
            gi.WriteUint8( svc_stufftext );
            gi.WriteString( "spectator 0\n" );
            gi.unicast( ent, true );
            return;
        }
    } else {
        // he was a spectator and wants to join the game
        // he must have the right password
        char *value = Info_ValueForKey( ent->client->pers.userinfo, "password" );
        if ( *password->string && strcmp( password->string, "none" ) &&
            strcmp( password->string, value ) ) {
            gi.cprintf( ent, PRINT_HIGH, "Password incorrect.\n" );
            ent->client->pers.spectator = true;
            gi.WriteUint8( svc_stufftext );
            gi.WriteString( "spectator 1\n" );
            gi.unicast( ent, true );
            return;
        }
    }

    // clear client on respawn
    ent->client->resp.score = ent->client->pers.score = 0;

	// Ensure we're sending to clients again.
    ent->svFlags &= ~SVF_NOCLIENT;
	// Spawn at a spawn point
    SVG_Player_SpawnInBody( ent );

    // Add a teleportation effect
    if ( !ent->client->pers.spectator ) {
        // send effect
        //gi.WriteUint8( svc_muzzleflash );
        //gi.WriteInt16( g_edict_pool.NumberForEdict( ent ) );//ent - g_edicts );
        //gi.WriteUint8( MZ_LOGIN );
        //gi.multicast( &ent->s.origin, MULTICAST_PVS, false );
        SVG_Util_AddEvent( ent, EV_PLAYER_LOGIN, 0 );

        // hold in place briefly
        ent->client->ps.pmove.pm_flags = PMF_TIME_TELEPORT;
        ent->client->ps.pmove.pm_time = 14; // WID: 40hz: Also here, 112.
    }

	// Set the time of the current respawn.
    ent->client->respawn_time = level.time;

    if ( ent->client->pers.spectator ) {
        gi.bprintf( PRINT_HIGH, "%s has moved to the sidelines\n", ent->client->pers.netname );
    } else {
        gi.bprintf( PRINT_HIGH, "%s joined the game\n", ent->client->pers.netname );
    }
}


/**
*   @brief  This will be called once for all clients on each server frame, before running any other entities in the world.
**/
void SVG_Client_BeginServerFrame( svg_base_edict_t *ent ) {
    // <Q2RTXP>: WID: TODO: WARN?
    if ( !ent->client ) {
        gi.dprintf( "%s: !ent->client && !ent->client->pers.spawned\n", __func__ );
        return;
    }
    // Ensure we are dealing with a player entity here.
    if ( !ent->GetTypeInfo()->IsSubClassType<svg_player_edict_t>() ) {
        gi.dprintf( "%s: Not a player entity.\n", __func__ );
        return;
    }

    /**
    *   Remove RF_STAIR_STEP if we're in a new frame, not stepping.
    **/
    if ( level.frameNumber != ent->client->last_stair_step_frame ) {
        ent->s.renderfx &= ~RF_STAIR_STEP;
    }
    /**
    *	Give game mode control.
    **/
    game.mode->BeginServerFrame( static_cast<svg_player_edict_t *>( ent ) );

    /**
    *   UNLATCH ALL LATCHED BUTTONS:
    **/
    ent->client->latched_buttons = BUTTON_NONE;
}

/**
*	@brief	Called for each player at the end of the server frame,
*			and right after spawning.
**/
void SVG_Client_EndServerFrame( svg_base_edict_t *ent ) {
    // <Q2RTXP>: WID: TODO: WARN?
    if ( !ent->client ) {
        return;
    }
    // no player exists yet (load game)
    if ( !ent->client->pers.spawned ) {
        gi.dprintf( "%s: !ent->client->pers.spawned\n", __func__ );
        return;
    }

    // Ensure we are dealing with a player entity here.
    if ( !ent->GetTypeInfo()->IsSubClassType<svg_player_edict_t>() ) {
        gi.dprintf( "%s: Not a player entity.\n", __func__ );
        return;
    }

    game.currentViewPlayer = static_cast<svg_player_edict_t*>( ent );
    game.currentViewClient = ent->client;


    /**
    *   If the origin or velocity have changed since ClientThink(),
    *   update the pmove values.  This will happen when the client
    *   is pushed by a bmodel or kicked by an explosion.
    *   
    *   If it wasn't updated here, the view position would lag a frame
    *   behind the body position when pushed -- "sinking into plats"
    **/
    game.currentViewClient->ps.pmove.origin = game.currentViewPlayer->s.origin;
    game.currentViewClient->ps.pmove.velocity = game.currentViewPlayer->velocity;

    /**
    *	Let game mode handle it from here.
    **/
    game.mode->EndServerFrame( game.currentViewPlayer );

    /**
    *	Now Finalize player state.
    **/
    // Convert certain playerstate properties into entity state properties.
    SG_PlayerStateToEntityState( game.currentViewClient->clientNum, &game.currentViewClient->ps, &game.currentViewPlayer->s, true, false );
    // Send any remaining pending predictable events to all clients but "ourselves".
    SVG_Client_SendPendingPredictableEvents( game.currentViewPlayer, game.currentViewClient );
}