/********************************************************************
*
*
*	ServerGame: Generic Client EntryPoints
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "sharedgame/sg_usetarget_hints.h"

#include "svgame/svg_commands_server.h"
#include "svgame/svg_misc.h"
#include "svgame/svg_utils.h"

#include "svgame/player/svg_player_client.h"
#include "svgame/player/svg_player_hud.h"
#include "svgame/player/svg_player_view.h"

#include "svgame/svg_lua.h"


/**
*   @brief
**/
void SVG_Client_UserinfoChanged( svg_entity_t *ent, char *userinfo );

/**
*   @brief  Will process(progress) the entity's active animations for each body state and event states.
**/
void SVG_P_ProcessAnimations( svg_entity_t *ent );



/**
*
*
*
*   Client Death:
*
*
*
**/
/**
*   @brief
**/
void Touch_Item( svg_entity_t *ent, svg_entity_t *other, cm_plane_t *plane, cm_surface_t *surf );
/**
*   @brief
**/
static void TossClientWeapon( svg_entity_t *self ) {
    if ( !deathmatch->value )
        return;

    // Need to have actual ammo to toss.
    const gitem_t *item = self->client->pers.weapon;
    if ( !self->client->pers.inventory[ self->client->ammo_index ] )
        item = NULL;
    // Can't toss away your fists.
    if ( item && ( strcmp( item->pickup_name, "Fists" ) == 0 ) )
        item = NULL;

    //if (item && quad)
    //    spread = 22.5f;
    //else
    float spread = 0.0f;

    if ( item ) {
        self->client->viewMove.viewAngles[ YAW ] -= spread;
        svg_entity_t *drop = Drop_Item( self, item );
        self->client->viewMove.viewAngles[ YAW ] += spread;
        drop->spawnflags = DROPPED_PLAYER_ITEM;
    }
}

/**
*   @brief  
**/
static void LookAtKiller( svg_entity_t *self, svg_entity_t *inflictor, svg_entity_t *attacker ) {
    vec3_t dir = {};
    float killerYaw = 0.f;

    if ( attacker && attacker != world && attacker != self ) {
        VectorSubtract( attacker->s.origin, self->s.origin, dir );
    } else if ( inflictor && inflictor != world && inflictor != self ) {
        VectorSubtract( inflictor->s.origin, self->s.origin, dir );
    } else {
        self->client->killer_yaw = /*self->client->ps.stats[ STAT_KILLER_YAW ] */ self->s.angles[ YAW ];
        return;
    }

    self->client->killer_yaw = /*self->client->ps.stats[ STAT_KILLER_YAW ] */ QM_Vector3ToYaw( dir );

    //if ( dir[ 0 ] ) {
    //    self->client->killer_yaw = RAD2DEG( atan2( dir[ 1 ], dir[ 0 ] ) );
    //} else {
    //    self->client->killer_yaw = 0;
    //    if ( dir[ 1 ] > 0 ) {
    //        self->client->killer_yaw = 90;
    //    } else if ( dir[ 1 ] < 0 ) {
    //        self->client->killer_yaw = 270; // WID: pitch-fix.
    //    }
    //}
    //if ( self->client->killer_yaw < 0 ) {
    //    self->client->killer_yaw += 360;
    //}
}



//=======================================================================


/*
==================
player_die
==================
*/
void player_die( svg_entity_t *self, svg_entity_t *inflictor, svg_entity_t *attacker, int damage, vec3_t point ) {
    int     n;

    VectorClear( self->avelocity );

    self->takedamage = DAMAGE_YES;
    self->movetype = MOVETYPE_TOSS;

    // Unset weapon model.
    self->s.modelindex2 = 0;    // remove linked weapon model

    // Clear X and Z angles.
    self->s.angles[ 0 ] = 0;
    self->s.angles[ 2 ] = 0;

    // Stop playing any sounds.
    self->s.sound = 0;
    self->client->weaponState.activeSound = 0;

    // Set bbox maxs to PM_BBOX_DUCKED_MAXS.
    self->maxs[ 2 ] = PM_BBOX_DUCKED_MAXS.z;

    //  self->solid = SOLID_NOT;
        // Flag as to be treated as 'deadmonster' collision.
    self->svflags |= SVF_DEADMONSTER;

    if ( !self->lifeStatus ) {
        // Determine respawn time.
        self->client->respawn_time = ( level.time + 1_sec );
        // Make sure the playerstate its pmove knows we're dead.
        self->client->ps.pmove.pm_type = PM_DEAD;
        // Set the look at killer yaw.
        LookAtKiller( self, inflictor, attacker );
        // Notify the obituary.
        SVG_Player_Obituary( self, inflictor, attacker );
        // Toss away weaponry.
        TossClientWeapon( self );

        // clear inventory
        // this is kind of ugly, but it's how we want to handle keys in coop
        //for (n = 0; n < game.num_items; n++) {
        //    if (coop->value && itemlist[n].flags & IT_KEY)
        //        self->client->resp.pers_respawn.inventory[n] = self->client->pers.inventory[n];
        //    self->client->pers.inventory[n] = 0;
        //}
    }

    // Gib Death:
    if ( self->health < -40 ) {
        // Play gib sound.
        gi.sound( self, CHAN_BODY, gi.soundindex( "world/gib01.wav" ), 1, ATTN_NORM, 0 );
        //! Throw 4 small meat gibs around.
        for ( n = 0; n < 4; n++ ) {
            SVG_Misc_ThrowGib( self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_TYPE_ORGANIC );
        }
        // Turn ourself into the thrown head entity.
        SVG_Misc_ThrowClientHead( self, damage );

        // Gibs don't take damage, but fade away as time passes.
        self->takedamage = DAMAGE_NO;
        // Normal death:
    } else {
        if ( !self->lifeStatus ) {
            static int i;

            gi.dprintf( "%s: WID: TODO: Implement a player death player animation here\n", __func__ );

            //i = (i + 1) % 3;
            //// start a death animation
            //self->client->anim_priority = ANIM_DEATH;
            //if (self->client->ps.pmove.pm_flags & PMF_DUCKED) {
            //    self->s.frame = FRAME_crdeath1 - 1;
            //    self->client->anim_end = FRAME_crdeath5;
            //} else switch (i) {
            //    case 0:
            //        self->s.frame = FRAME_death101 - 1;
            //        self->client->anim_end = FRAME_death106;
            //        break;
            //    case 1:
            //        self->s.frame = FRAME_death201 - 1;
            //        self->client->anim_end = FRAME_death206;
            //        break;
            //    case 2:
            //        self->s.frame = FRAME_death301 - 1;
            //        self->client->anim_end = FRAME_death308;
            //        break;
            //    }
            //gi.sound(self, CHAN_VOICE, gi.soundindex(va("*death%i.wav", (Q_rand() % 4) + 1)), 1, ATTN_NORM, 0);
            gi.sound( self, CHAN_VOICE, gi.soundindex( va( "player/death0%i.wav", ( irandom( 0, 4 ) ) + 1 ) ), 1, ATTN_NORM, 0 );
        }
    }

    self->lifeStatus = LIFESTATUS_DEAD;

    gi.linkentity( self );
}
/**
*   @brief  Player pain is handled at the end of the frame in P_DamageFeedback.
**/
void player_pain( svg_entity_t *self, svg_entity_t *other, float kick, int damage ) {
    // 
}



/**
*
*
*
*   Connect/Disconnect:
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
qboolean SVG_Client_Connect( svg_entity_t *ent, char *userinfo ) {
    // check to see if they are on the banned IP list
    char *value = Info_ValueForKey( userinfo, "ip" );
    if ( SVG_FilterPacket( value ) ) {
        Info_SetValueForKey( userinfo, "rejmsg", "Banned." );
        return false;
    }

    // check for a spectator
    value = Info_ValueForKey( userinfo, "spectator" );
    if ( deathmatch->value && *value && strcmp( value, "0" ) ) {
        int i, numspec;

        if ( *spectator_password->string &&
            strcmp( spectator_password->string, "none" ) &&
            strcmp( spectator_password->string, value ) ) {
            Info_SetValueForKey( userinfo, "rejmsg", "Spectator password required or incorrect." );
            return false;
        }

        // count spectators
        for ( i = numspec = 0; i < maxclients->value; i++ )
            if ( g_edicts[ i + 1 ].inuse && g_edicts[ i + 1 ].client->pers.spectator )
                numspec++;

        if ( numspec >= maxspectators->value ) {
            Info_SetValueForKey( userinfo, "rejmsg", "Server spectator limit is full." );
            return false;
        }
    } else {
        // check for a password
        value = Info_ValueForKey( userinfo, "password" );
        if ( *password->string && strcmp( password->string, "none" ) &&
            strcmp( password->string, value ) ) {
            Info_SetValueForKey( userinfo, "rejmsg", "Password required or incorrect." );
            return false;
        }
    }

    // they can connect
    ent->client = game.clients + ( ent - g_edicts - 1 );

    // if there is already a body waiting for us (a loadgame), just
    // take it, otherwise spawn one from scratch
    if ( ent->inuse == false ) {
        // clear the respawning variables
        SVG_Player_InitRespawnData( ent->client );
        if ( !game.autosaved || !ent->client->pers.weapon ) {
            SVG_Player_InitPersistantData( ent, ent->client );
        }
    }

    // make sure we start with known default(s)
    //ent->svflags = SVF_PLAYER;
    SVG_Client_UserinfoChanged( ent, userinfo );

    // Developer connection print.
    if ( game.maxclients > 1 ) {
        gi.dprintf( "%s connected\n", ent->client->pers.netname );
    }

    // Make sure we start with known default(s):
    // We're a player.
    ent->svflags = SVF_PLAYER;
    ent->s.entityType = ET_PLAYER;

    // We're connected.
    ent->client->pers.connected = true;

    return true;
}

/**
*   @brief  Called when a player drops from the server.
*           Will NOT be called between levels.
**/
void SVG_Client_Disconnect( svg_entity_t *ent ) {
    //int     playernum;

    if ( !ent->client )
        return;

    // WID: LUA:
    SVG_Lua_CallBack_ClientExitLevel( ent );

    gi.bprintf( PRINT_HIGH, "%s disconnected\n", ent->client->pers.netname );

    // send effect
    if ( ent->inuse ) {
        gi.WriteUint8( svc_muzzleflash );
        gi.WriteInt16( ent - g_edicts );
        gi.WriteUint8( MZ_LOGOUT );
        gi.multicast( ent->s.origin, MULTICAST_PVS, false );
    }

    gi.unlinkentity( ent );
    ent->s.modelindex = 0;
    ent->s.modelindex2 = 0;
    ent->s.sound = 0;
    ent->s.event = 0;
    ent->s.effects = 0;
    ent->s.renderfx = 0;
    ent->s.solid = SOLID_NOT; // 0
    ent->s.entityType = ET_GENERIC;
    ent->solid = SOLID_NOT;
    ent->inuse = false;
    ent->classname = "disconnected";
    ent->client->pers.spawned = false;
    ent->client->pers.connected = false;
    ent->timestamp = level.time + 1_sec;

    // WID: This is now residing in RunFrame
    // FIXME: don't break skins on corpses, etc
    //playernum = ent-g_edicts-1;
    //gi.configstring (CS_PLAYERSKINS+playernum, "");
}



/***
*
*
*
*   Client Data, Save, Restore, and Persistent.
*
*
*
***/
/**
*   @brief  For SinglePlayer: Called only once, at game first initialization.
*           For Multiplayer Modes: Called after each death, and level change.
**/
void SVG_Player_InitPersistantData( svg_entity_t *ent, svg_client_t *client ) {
    // Clear out persistent data.
    client->pers = {};

    // Find the fists item, add it to our inventory.
    const gitem_t *item_fists = SVG_FindItem( "Fists" );
    client->pers.selected_item = ITEM_INDEX( item_fists );
    client->pers.inventory[ client->pers.selected_item ] = 1;

    // Find the Pistol item, add it to our inventory and appoint it as the selected weapon.
    const gitem_t *item_pistol = SVG_FindItem( "Pistol" );
    client->pers.selected_item = ITEM_INDEX( item_pistol );
    client->pers.inventory[ client->pers.selected_item ] = 1;
    // Assign it as our selected weapon.
    client->pers.weapon = item_pistol;
    // Give it a single full clip of ammo.
    client->pers.weapon_clip_ammo[ client->pers.weapon->weapon_index ] = item_pistol->clip_capacity;
    // And some extra bullets to reload with.
    ent->client->ammo_index = ITEM_INDEX( SVG_FindItem( ent->client->pers.weapon->ammo ) );
    client->pers.inventory[ ent->client->ammo_index ] = 78;

    // Obviously we need to allow this.
    client->weaponState.canChangeMode = false;

    // Health.
    client->pers.health = 100;
    client->pers.max_health = 100;

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

    // In case of a coop game mode, we make sure to store the 
    // 'persistent across level changes' data into the client's
    // persistent respawn field, so we can restore it each respawn.
    client->resp.pers_respawn = client->pers;
}

/**
*    @brief  Some information that should be persistant, like health, is still stored in the edict structure.
*            So it needs to be mirrored out to the client structure before all the edicts are wiped.
**/
void SVG_Player_SaveClientData( void ) {
    for ( int32_t i = 0; i < game.maxclients; i++ ) {
        svg_entity_t *ent = &g_edicts[ 1 + i ];
        if ( !ent->inuse ) {
            continue;
        }
        game.clients[ i ].pers.health = ent->health;
        game.clients[ i ].pers.max_health = ent->max_health;
        game.clients[ i ].pers.savedFlags = static_cast<entity_flags_t>( ent->flags & ( FL_GODMODE | FL_NOTARGET /*| FL_POWER_ARMOR*/ ) );
        if ( coop->value ) {
            game.clients[ i ].pers.score = ent->client->resp.score;
        }
    }
}
/**
*   @brief  Restore the client stored persistent data to reinitialize several client entity fields.
**/
void SVG_Player_RestoreClientData( svg_entity_t *ent ) {
    ent->health = ent->client->pers.health;
    ent->max_health = ent->client->pers.max_health;
    ent->flags |= ent->client->pers.savedFlags;
    if ( coop->value )
        ent->client->resp.score = ent->client->pers.score;
}

/**
*   @brief  Will reset the entity client's 'Field of View' back to its defaults.
**/
void SVG_Player_ResetPlayerStateFOV( svg_client_t *client ) {
    // For DM Mode, possibly fixed FOV is set.
    if ( deathmatch->value && ( (int)dmflags->value & DF_FIXED_FOV ) ) {
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
void SVG_Client_UserinfoChanged( svg_entity_t *ent, char *userinfo ) {
    char *s;
    int     playernum;

    // check for malformed or illegal info strings
    if ( !Info_Validate( userinfo ) ) {
        strcpy( userinfo, "\\name\\badinfo\\skin\\testdummy/skin" );
    }

    // set name
    s = Info_ValueForKey( userinfo, "name" );
    Q_strlcpy( ent->client->pers.netname, s, sizeof( ent->client->pers.netname ) );

    // set spectator
    s = Info_ValueForKey( userinfo, "spectator" );
    // spectators are only supported in deathmatch
    if ( deathmatch->value && *s && strcmp( s, "0" ) ) {
        ent->client->pers.spectator = true;
    } else {
        ent->client->pers.spectator = false;
    }

    // set skin
    s = Info_ValueForKey( userinfo, "skin" );

    playernum = ent - g_edicts - 1;

    // combine name and skin into a configstring
    gi.configstring( CS_PLAYERSKINS + playernum, va( "%s\\%s", ent->client->pers.netname, s ) );

    // fov
    #if 1
    SVG_Player_ResetPlayerStateFOV( ent->client );
    #else
    if ( deathmatch->value && ( (int)dmflags->value & DF_FIXED_FOV ) ) {
        ent->client->ps.fov = 90;
    } else {
        ent->client->ps.fov = atoi( Info_ValueForKey( userinfo, "fov" ) );
        if ( ent->client->ps.fov < 1 )
            ent->client->ps.fov = 90;
        else if ( ent->client->ps.fov > 160 )
            ent->client->ps.fov = 160;
    }
    #endif

    // handedness
    s = Info_ValueForKey( userinfo, "hand" );
    if ( strlen( s ) ) {
        ent->client->pers.hand = atoi( s );
    }

    // save off the userinfo in case we want to check something later
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
void SVG_Client_RespawnPlayer( svg_entity_t *self ) {
    if ( deathmatch->value || coop->value ) {
        // spectator's don't leave bodies
        if ( self->movetype != MOVETYPE_NOCLIP ) {
            SVG_CopyToBodyQue( self );
        }
        self->svflags &= ~SVF_NOCLIENT;
        SVG_Player_PutInServer( self );

        // add a teleportation effect
        self->s.event = EV_PLAYER_TELEPORT;

        // hold in place briefly
        self->client->ps.pmove.pm_flags = PMF_TIME_TELEPORT;
        self->client->ps.pmove.pm_time = 14; // WID: 40hz: Q2RE has 112 here.

        self->client->respawn_time = level.time;

        return;
    }

    // restart the entire server
    gi.AddCommandString( "pushmenu loadgame\n" );
}

/**
*   @brief  Only called when pers.spectator changes.
*   @note   That resp.spectator should be the opposite of pers.spectator here
**/
void SVG_Client_RespawnSpectator( svg_entity_t *ent ) {
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
            if ( g_edicts[ i ].inuse && g_edicts[ i ].client->pers.spectator ) {
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

    ent->svflags &= ~SVF_NOCLIENT;
    SVG_Player_PutInServer( ent );

    // add a teleportation effect
    if ( !ent->client->pers.spectator ) {
        // send effect
        gi.WriteUint8( svc_muzzleflash );
        gi.WriteInt16( ent - g_edicts );
        gi.WriteUint8( MZ_LOGIN );
        gi.multicast( ent->s.origin, MULTICAST_PVS, false );

        // hold in place briefly
        ent->client->ps.pmove.pm_flags = PMF_TIME_TELEPORT;
        ent->client->ps.pmove.pm_time = 14; // WID: 40hz: Also here, 112.
    }

    ent->client->respawn_time = level.time;

    if ( ent->client->pers.spectator ) {
        gi.bprintf( PRINT_HIGH, "%s has moved to the sidelines\n", ent->client->pers.netname );
    } else {
        gi.bprintf( PRINT_HIGH, "%s joined the game\n", ent->client->pers.netname );
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
*   @brief  Called either when a player connects to a server, OR respawns in a multiplayer game.
* 
*           Will look up a spawn point, spawn(placing) the player 'body' into the server and (re-)initializing
*           saved entity and persistant data. (This includes actually raising the weapon up.)
**/
void SVG_Player_PutInServer( svg_entity_t *ent ) {
    Vector3 mins = PM_BBOX_STANDUP_MINS;
    Vector3 maxs = PM_BBOX_STANDUP_MAXS;
    int     index;
    Vector3  spawn_origin, spawn_angles;
    svg_client_t *client;
    client_respawn_t    savedRespawnData = {};
    Vector3 temp, temp2;
    cm_trace_t tr;

    // Always clear out any possibly previous left over of the useTargetHint.
    Client_ClearUseTargetHint( ent, ent->client, nullptr );

    // find a spawn point
    // do it before setting health back up, so farthest
    // ranging doesn't count this client
    SVG_Player_SelectSpawnPoint( ent, spawn_origin, spawn_angles );

    index = ent - g_edicts - 1;
    client = ent->client;

    if ( SG_IsMultiplayerGameMode( game.gamemode ) ) {
        // Store userinfo.
        char        userinfo[ MAX_INFO_STRING ];
        memcpy( userinfo, client->pers.userinfo, sizeof( userinfo ) );

        // Store respawn data.
        savedRespawnData = client->resp;

        // DeathMatch: Reinitialize a fresh persistent data.
        if ( game.gamemode == GAMEMODE_TYPE_DEATHMATCH ) {
            SVG_Player_InitPersistantData( ent, client );
        // Cooperative: 
        } else if ( game.gamemode == GAMEMODE_TYPE_COOPERATIVE ) {
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
        }
        // Administer the user info change.
        SVG_Client_UserinfoChanged( ent, userinfo );
    } else {
        char    userinfo[ MAX_INFO_STRING ];
        memcpy( userinfo, client->pers.userinfo, sizeof( userinfo ) );
        // Restore userinfo.
        SVG_Client_UserinfoChanged( ent, userinfo );
        // Acquire respawn data.
        savedRespawnData = client->resp;
        // Restore client persistent data.
        client->pers = savedRespawnData.pers_respawn;
        // Fresh SP mode has 0 score.
        client->pers.score = 0;
    }


    // Backup persistent data and clean everything.
    client_persistant_t savedPersistantData = client->pers;
    memset( client, 0, sizeof( *client ) );
    client->pers = savedPersistantData;
    // If dead at the time of the previous map switching to the current, reinitialize persistent data.
    if ( client->pers.health <= 0 ) {
        SVG_Player_InitPersistantData( ent, client );
    }
    client->resp = savedRespawnData;

    // copy some data from the client to the entity
    SVG_Player_RestoreClientData( ent );

    // fix level switch issue
    ent->client->pers.connected = true;

    // clear entity values
    ent->groundInfo = {};
    ent->client = &game.clients[ index ];
    ent->takedamage = DAMAGE_AIM;
    ent->movetype = MOVETYPE_WALK;
    ent->viewheight = PM_VIEWHEIGHT_STANDUP;
    ent->inuse = true;
    ent->classname = "player";
    ent->mass = 200;
    ent->solid = SOLID_BOUNDS_BOX;
    ent->lifeStatus = LIFESTATUS_ALIVE;
    ent->air_finished_time = level.time + 12_sec;
    ent->clipmask = ( MASK_PLAYERSOLID );
    ent->model = "players/playerdummy/tris.iqm";
    ent->pain = player_pain;
    ent->die = player_die;
    ent->liquidInfo.level = liquid_level_t::LIQUID_NONE;
    ent->liquidInfo.type = CONTENTS_NONE;
    ent->flags = static_cast<entity_flags_t>( ent->flags & ~FL_NO_KNOCKBACK );

    ent->svflags &= ~SVF_DEADMONSTER;
    ent->svflags &= ~FL_NO_KNOCKBACK;
    ent->svflags |= SVF_PLAYER;
    // Player Entity Type:
    ent->s.entityType = ET_PLAYER;

    VectorCopy( mins, ent->mins );
    VectorCopy( maxs, ent->maxs );
    VectorClear( ent->velocity );

    // Clear PlayerState values.
    client->ps = {}; // memset( &ent->client->ps, 0, sizeof( client->ps ) );
    // Reset the Field of View for the player state.
    SVG_Player_ResetPlayerStateFOV( ent->client );

    // Set viewheight for player state pmove.
    ent->client->ps.pmove.viewheight = ent->viewheight;

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
    ent->s.modelindex = 255;        // Will use the skin specified model.
    ent->s.modelindex2 = 255;       // Custom gun model.
    // sknum is player num and weapon number
    // weapon number will be added in changeweapon
    ent->s.skinnum = ent - globals.edicts - 1;
    ent->s.frame = 0;
    ent->s.old_frame = 0;

    // try to properly clip to the floor / spawn
    VectorCopy( spawn_origin, temp );
    VectorCopy( spawn_origin, temp2 );
    temp[ 2 ] -= 64;
    temp2[ 2 ] += 16;
    tr = gi.trace( &temp2.x, ent->mins, ent->maxs, &temp.x, ent, ( MASK_PLAYERSOLID ) );
    if ( !tr.allsolid && !tr.startsolid && Q_stricmp( level.mapname, "tech5" ) ) {
        VectorCopy( tr.endpos, ent->s.origin );
        ent->groundInfo.entity = tr.ent;
    } else {
        VectorCopy( spawn_origin, ent->s.origin );
        ent->s.origin[ 2 ] += 10; // make sure off ground
    }

    VectorCopy( ent->s.origin, ent->s.old_origin );

    client->ps.pmove.origin = ent->s.origin; // COORD2SHORT(ent->s.origin[i]); // WID: float-movement

    spawn_angles[ PITCH ] = 0;
    spawn_angles[ ROLL ] = 0;

    // set the delta angle
    client->ps.pmove.delta_angles = /*ANGLE2SHORT*/QM_Vector3AngleMod( spawn_angles - client->resp.cmd_angles );

    VectorCopy( spawn_angles, ent->s.angles );
    client->ps.viewangles = spawn_angles;
    client->viewMove.viewAngles = spawn_angles;
    AngleVectors( &client->viewMove.viewAngles.x, &client->viewMove.viewForward.x, nullptr, nullptr );

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

    if ( !SVG_Util_KillBox( ent, true ) ) {
        // could't spawn in?
    }

    gi.linkentity( ent );

    // force the current weapon up
    client->newweapon = client->pers.weapon;
    SVG_Player_Weapon_Change( ent );
}

/**
*   @brief  Deathmatch mode implementation of ClientBegin.
*
*           A client has just connected to the server in deathmatch mode, so clear everything 
*           out that might've been there and (re-)initialize a full new player 'body'.
**/
void SVG_Client_BeginDeathmatch( svg_entity_t *ent ) {
    // Init Edict.
    SVG_InitEdict( ent );

    // Make sure classname is player.
    ent->classname = "player";
    // Make sure entity type is player.
    ent->s.entityType = ET_PLAYER;
    // Ensure proper player flag is set.
    ent->svflags |= SVF_PLAYER;
    // Initialize respawn data.
    SVG_Player_InitRespawnData( ent->client );
    // Actually finds a spawnpoint and places the 'body' into the game.
    SVG_Player_PutInServer( ent );

    // If in intermission, move our client over into intermission state. (We connected at the end of a match).
    if ( level.intermissionFrameNumber ) {
        SVG_HUD_MoveClientToIntermission( ent );
    // Otherwise, send 'Login' effect even if NOT in a multiplayer game 
    } else {
        // send effect
        gi.WriteUint8( svc_muzzleflash );
        gi.WriteInt16( ent - g_edicts );
        gi.WriteUint8( MZ_LOGIN );
        gi.multicast( ent->s.origin, MULTICAST_PVS, false );
    }

    // Notify player joined.
    gi.bprintf( PRINT_HIGH, "%s entered the game\n", ent->client->pers.netname );

    // Call upon EndServerFrame to make sure all view stuff is valid.
    SVG_Client_EndServerFrame( ent );
}

/**
*   @brief  A client connected by loadgame(assuming singleplayer), there is already 
*           a body waiting for us to use. We just need to adjust its angles.
**/
void SVG_Client_BeginLoadGame( svg_entity_t *ent ) {
    // the client has cleared the client side viewangles upon
    // connecting to the server, which is different than the
    // state when the game is saved, so we need to compensate
    // with deltaangles
    ent->client->ps.pmove.delta_angles = /*ANGLE2SHORT*/QM_Vector3AngleMod( ent->client->ps.viewangles );
    // Make sure classname is player.
    ent->classname = "player";
    // Make sure entity type is player.
    ent->s.entityType = ET_PLAYER;
    // Ensure proper player flag is set.
    ent->svflags |= SVF_PLAYER;
}

/**
*   @brief  There is no body waiting for us yet, so (re-)initialize the entity we have with a full new 'body'.
**/
void SVG_Client_BeginNewBody( svg_entity_t *ent ) {
    // A spawn point will completely reinitialize the entity
    // except for the persistant data that was initialized at
    // connect time
    SVG_InitEdict( ent );
    // Make sure classname is player.
    ent->classname = "player";
    // Make sure entity type is player.
    ent->s.entityType = ET_PLAYER;
    // Ensure proper player flag is set.
    ent->svflags |= SVF_PLAYER;
    // Initialize respawn data.
    SVG_Player_InitRespawnData( ent->client );
    // Actually finds a spawnpoint and places the 'body' into the game.
    SVG_Player_PutInServer( ent );
}

/**
*   @brief  Called when a client has finished connecting, and is ready
*           to be placed into the game. This will happen every level load.
**/
void SVG_Client_Begin( svg_entity_t *ent ) {
    // Assign matching client for this entity.
    ent->client = game.clients + ( ent - g_edicts - 1 );

    // [Paril-KEX] we're always connected by this point...
    ent->client->pers.connected = true;

    // Always clear out any previous left over useTargetHint stuff.
    Client_ClearUseTargetHint( ent, ent->client, nullptr );

    // <Q2RTXP>: WID: TODO:
    // Determine the game mode, and handle specifically based on that.
    if ( deathmatch->value ) {
        SVG_Client_BeginDeathmatch( ent );
        return;
    }

    // We're spawned now of course.
    ent->client->pers.spawned = true;

    // If there is already a body waiting for us (a loadgame), just take it:
    if ( ent->inuse == (qboolean)true ) {
        SVG_Client_BeginLoadGame( ent );
    // Otherwise spawn one from scratch:
    } else {
        SVG_Client_BeginNewBody( ent );
    }

    // If in intermission, move our client over into intermission state. (We connected at the end of a match).
    if ( level.intermissionFrameNumber ) {
        SVG_HUD_MoveClientToIntermission( ent );
    // Otherwise, send 'Login' effect even if NOT in a multiplayer game 
    } else {
        // 
        if ( game.maxclients >= 1 ) {
            gi.WriteUint8( svc_muzzleflash );
            gi.WriteInt16( ent - g_edicts );
            gi.WriteUint8( MZ_LOGIN );
            gi.multicast( ent->s.origin, MULTICAST_PVS, false );

            // Only print this though, if NOT in a singleplayer game.
            //if ( game.gamemode != GAMEMODE_TYPE_SINGLEPLAYER ) {
            gi.bprintf( PRINT_HIGH, "%s entered the game\n", ent->client->pers.netname );
            //}
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
*   Client UseTargetHint Functionality.:
*
*
*
**/
/**
*   @brief  Unsets the current client stats usetarget info.
**/
void Client_ClearUseTargetHint( svg_entity_t *ent, svg_client_t *client, svg_entity_t *useTargetEntity ) {
    // Nothing for display.
    client->ps.stats[ STAT_USETARGET_HINT_ID ] = 0;
    client->ps.stats[ STAT_USETARGET_HINT_FLAGS ] = 0;
}
/**
*   @brief  Determines the necessary UseTarget Hint information for the hovered entity(if any).
*   @return True if the entity has legitimate UseTarget Hint information. False if unset, or not found at all.
**/
const bool SVG_Client_UpdateUseTargetHint( svg_entity_t *ent, svg_client_t *client, svg_entity_t *useTargetEntity ) {
    // We got an entity.
    if ( useTargetEntity && useTargetEntity->useTarget.hintInfo ) {
        // Determine UseTarget Hint Info:
        const sg_usetarget_hint_t *useTargetHint = SG_UseTargetHint_GetByID( useTargetEntity->useTarget.hintInfo->index );
        // > 0 means it is a const char in our data array.
        if ( useTargetEntity->useTarget.hintInfo->index > USETARGET_HINT_ID_NONE ) {
            // Apply stats if valid data.
            if ( useTargetHint &&
                ( useTargetHint->index > USETARGET_HINT_ID_NONE && useTargetHint->index < USETARGET_HINT_ID_MAX ) ) {
                client->ps.stats[ STAT_USETARGET_HINT_ID ] = useTargetHint->index;
                client->ps.stats[ STAT_USETARGET_HINT_FLAGS ] |= ( SG_USETARGET_HINT_FLAGS_DISPLAY | useTargetHint->flags );
            // Clear out otherwise.
            } else {
                Client_ClearUseTargetHint( ent, client, useTargetEntity );
            }
        // < 0 means that it is passed to us by a usetarget hint info command.
        } else if ( useTargetEntity->useTarget.hintInfo->index < 0 ) {
            // TODO:
            Client_ClearUseTargetHint( ent, client, useTargetEntity );
        // Clear whichever it previously contained. we got nothing to display.
        } else {
            // Clear whichever it previously contained. we got nothing to display.
            Client_ClearUseTargetHint( ent, client, useTargetEntity );
        }
    } else {
        // Clear whichever it previously contained. we got nothing to display.
        Client_ClearUseTargetHint( ent, client, useTargetEntity );
    }

    // Succeeded at updating the UseTarget Hint if it is NOT zero.
    return ( client->ps.stats[ STAT_USETARGET_HINT_ID ] == 0 ? false : true );
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
*	@brief	Calculates the to be determined movement induced recoil factor.
**/
void SVG_Client_CalculateMovementRecoil( svg_entity_t *ent ) {
    // Get playerstate.
    player_state_t *playerState = &ent->client->ps;
    
    // Base movement recoil factor.
    double baseMovementRecoil = 0.;

    // Is on a ladder?
	bool isOnLadder = ( ( playerState->pmove.pm_flags & PMF_ON_LADDER ) ? true : false );
    // Determine if crouched(ducked).
    bool isDucked = ( ( playerState->pmove.pm_flags & PMF_DUCKED) ? true : false );
    // Determine if off-ground.
    bool isOnGround = ( ( playerState->pmove.pm_flags & PMF_ON_GROUND ) ? true : false );
    // Determine if in water.
    bool isInWater = ( ent->liquidInfo.level > liquid_level_t::LIQUID_NONE ? true : false );
    // Get liquid level.
	liquid_level_t liquidLevel = ent->liquidInfo.level;
    
    // Resulting move factor.
    double recoilMoveFactor = 0.;

    // First check if in water, so we can skip the other tests.
    if ( isInWater && ent->liquidInfo.level > liquid_level_t::LIQUID_FEET) {
        // Waist in water.
        recoilMoveFactor = 0.55;
        // Head under water.
        if ( ent->liquidInfo.level > liquid_level_t::LIQUID_WAIST ) {
            recoilMoveFactor = 0.75;
        }
    } else {
        // If ducked, -0.3.
        if ( isDucked && isOnGround && !isOnLadder ) {
            recoilMoveFactor -= 0.3;
        } else if ( isOnLadder ) {
            recoilMoveFactor = 0.5;
        }
    }

    // Divide 1 by max speed, multiply by velocity length, ONLY if, speed > pm_stop_speed
    if ( playerState->xyzSpeed >= default_pmoveParams_t::pm_stop_speed / 2. ) {
        // Get multiply factor.
        const double multiplyFactor = ( 1.0 / default_pmoveParams_t::pm_max_speed );
        // Calculate actual scale factor.
        const double scaleFactor = multiplyFactor * playerState->xyzSpeed;

        // Assign.
        recoilMoveFactor += 0.5 * scaleFactor;

        //gi.dprintf( "playerState->xyzSpeed(%f), scaleFactor(%f)\n", playerState->xyzSpeed, multiplyFactor, scaleFactor );
    } else {
        //gi.dprintf( "playerState->xyzSpeed\n", playerState->xyzSpeed );
    }

    // ( We default to idle. ) Default to 0.
    ent->client->weaponState.recoil.moveFactor = recoilMoveFactor;
}
/**
*   @brief  Calculates the final resulting recoil value, being clamped between -1, to +1.
**/
const double SVG_Client_GetFinalRecoilFactor( svg_entity_t *ent ) {
    // Get the movement induced recoil factor.
    const double movementRecoil = QM_Clampd( ent->client->weaponState.recoil.moveFactor, -1., 1. );
	// Get the fire induced recoil factor.
    const double fireRecoil = ent->client->weaponState.recoil.weaponFactor;
	// Determine the final recoil factor and clamp it between -1, to +1.
	const double recoilFactor = QM_Clampd( movementRecoil + fireRecoil, -1., 2. );
	// Clamp the recoil factor between -1, to +1.
	// Return the final recoil factor.
	return recoilFactor;
}