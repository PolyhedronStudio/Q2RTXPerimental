/********************************************************************
*
*
*	ServerGame: Client Heads Up Display:
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_game_items.h"
#include "svgame/player/svg_player_client.h"
#include "svgame/player/svg_player_hud.h"



/***
*
*
*
*   Intermission:
*
*
*
***/
/**
*   @brief
**/
void SVG_HUD_MoveClientToIntermission(edict_t *ent)
{
    ent->client->showhelp = false;

    VectorCopy( level.intermission_origin, ent->s.origin );
    VectorCopy( level.intermission_origin, ent->client->ps.pmove.origin );
    VectorCopy( level.intermission_angle, ent->client->ps.viewangles );
    ent->client->ps.viewangles = QM_Vector3AngleMod( level.intermission_angle );
    if ( !SG_IsMultiplayerGameMode( game.gamemode ) ) {
        ent->client->ps.pmove.pm_type = PM_SPINTERMISSION;
    } else {
        ent->client->ps.pmove.pm_type = PM_INTERMISSION;
    }
    ent->client->ps.gun.modelIndex = 0;
    ent->client->ps.gun.animationID = 0;
    /*ent->client->ps.damage_blend[3] = */ent->client->ps.screen_blend[ 3 ] = 0; // damageblend?
    ent->client->ps.rdflags = RDF_NONE;

    // clean up powerup info
    ent->client->grenade_blew_up = false;
    ent->client->grenade_time = 0_ms;

    ent->liquidInfo.type = CONTENTS_NONE;
    ent->liquidInfo.level = cm_liquid_level_t::LIQUID_NONE;;
    ent->viewheight = 0;
    ent->s.modelindex = 0;
    ent->s.modelindex2 = 0;
    ent->s.modelindex3 = 0;
    ent->s.modelindex4 = 0;

    ent->s.effects = EF_NONE;
    ent->s.renderfx = RF_NONE;
    ent->s.sound = 0;
    ent->s.event = EV_NONE;
    ent->s.solid = SOLID_NOT; // 0
    ent->solid = SOLID_NOT;
    ent->svflags = SVF_NOCLIENT;
    gi.unlinkentity(ent);

    // Scoreboard:
    if (deathmatch->value || coop->value ) {

        // Write out svc_scoreboard command.
        SVG_HUD_DeathmatchScoreboardMessage( ent, nullptr, true );
    }
}

/**
*   @brief
**/
void SVG_HUD_BeginIntermission(edict_t *targ)
{
    int     i = 0;
    edict_t *ent = nullptr;
    edict_t *client = nullptr;

    if (level.intermissionFrameNumber)
        return;     // already activated

    game.autosaved = false;

    // Respawn any dead clients
    for (i = 0 ; i < maxclients->value ; i++) {
        client = g_edicts + 1 + i;
        if (!client->inuse)
            continue;
        if (client->health <= 0)
            SVG_Client_RespawnPlayer(client);
    }

    level.intermissionFrameNumber = level.frameNumber;
    level.changemap = targ->map;

    if (strchr(level.changemap, '*')) {
        if (coop->value) {
            for (i = 0 ; i < maxclients->value ; i++) {
                client = g_edicts + 1 + i;
                if (!client->inuse)
                    continue;
                //// strip players of all keys between units
                //for (n = 0; n < game.num_items; n++) {
                //    if (itemlist[n].flags & ITEM_FLAG_KEY)
                //        client->client->pers.inventory[n] = 0;
                //}
                //client->client->pers.power_cubes = 0;
            }
        }
    } else {
        if (!deathmatch->value) {
            level.exitintermission = 1;     // go immediately to the next level
            return;
        }
    }

    level.exitintermission = 0;

    // find an intermission spot
    ent = SVG_Entities_Find(NULL, FOFS_GENTITY(classname), "info_player_intermission");
    if (!ent) {
        // the map creator forgot to put in an intermission point...
        ent = SVG_Entities_Find(NULL, FOFS_GENTITY(classname), "info_player_start");
        if (!ent)
            ent = SVG_Entities_Find(NULL, FOFS_GENTITY(classname), "info_player_deathmatch");
    } else {
        // chose one of four spots
        i = Q_rand() & 3;
        while (i--) {
            ent = SVG_Entities_Find(ent, FOFS_GENTITY(classname), "info_player_intermission");
            if (!ent)   // wrap around the list
                ent = SVG_Entities_Find(ent, FOFS_GENTITY(classname), "info_player_intermission");
        }
    }

    if (ent) {
        VectorCopy(ent->s.origin, level.intermission_origin);
        VectorCopy(ent->s.angles, level.intermission_angle);
    }

    // move all clients to the intermission point
    for (i = 0 ; i < maxclients->value ; i++) {
        client = g_edicts + 1 + i;
        if (!client->inuse)
            continue;
        SVG_HUD_MoveClientToIntermission(client);
    }
}



/***
*
*
*
*   Scoreboard:
*
*
*
***/

/**
*   @brief
**/
void SVG_HUD_DeathmatchScoreboardMessage(edict_t *ent, edict_t *killer = nullptr, const bool sendAsReliable ) {
    #if 0
    char    entry[ 1024 ];
    char    string[ 1400 ];
    int     stringlength;
    int     i, j, k;
    int     sorted[ MAX_CLIENTS ];
    int     sortedscores[ MAX_CLIENTS ];
    int     score, total;
    int     x, y;
    svg_client_t *cl;
    edict_t *cl_ent;
    // WID: C++20: Added const.
    const char *tag;
    // sort the clients by score
    total = 0;
    for (i = 0 ; i < game.maxclients ; i++) {
        cl_ent = g_edicts + 1 + i;
        if (!cl_ent->inuse || game.clients[i].resp.spectator)
            continue;
        score = game.clients[i].resp.score;
        for (j = 0 ; j < total ; j++) {
            if (score > sortedscores[j])
                break;
        }
        for (k = total ; k > j ; k--) {
            sorted[k] = sorted[k - 1];
            sortedscores[k] = sortedscores[k - 1];
        }
        sorted[j] = i;
        sortedscores[j] = score;
        total++;
    }

    // print level name and exit rules
    string[0] = 0;

    stringlength = strlen(string);

    // add the clients in sorted order
    if (total > 12)
        total = 12;

    for (i = 0 ; i < total ; i++) {
        cl = &game.clients[sorted[i]];
        cl_ent = g_edicts + 1 + sorted[i];

        x = (i >= 6) ? 160 : 0;
        y = 32 + 32 * (i % 6);

        // add a dogtag
        if (cl_ent == ent)
            tag = "tag1";
        else if (cl_ent == killer)
            tag = "tag2";
        else
            tag = NULL;
        if (tag) {
            Q_snprintf(entry, sizeof(entry),
                       "xv %i yv %i picn %s ", x + 32, y, tag);
            j = strlen(entry);
            if (stringlength + j > 1024)
                break;
            strcpy(string + stringlength, entry);
            stringlength += j;
        }

        // send the layout
        Q_snprintf(entry, sizeof(entry),
                   "client %i %i %i %i %i %li ",
                   x, y, sorted[i], cl->resp.score, cl->ping, (level.frameNumber - cl->resp.enterframe) / 600);
        j = strlen(entry);
        if (stringlength + j > 1024)
            break;
        strcpy(string + stringlength, entry);
        stringlength += j;
    }

    gi.WriteUint8(svc_layout);
    gi.WriteString(string);
    #else
    if ( ent->client->showscores ) {
        gi.dprintf( "scores\n" );
	} else {
		gi.dprintf( "no scores\n" );
	}
    gi.WriteUint8( svc_scoreboard );

    // First count the total of clients we got in-game.
    int32_t numberOfClients = 0;
    for ( int32_t i = 0; i < game.maxclients; i++ ) {
        edict_t *cl_ent = g_edicts + 1 + i;
        if ( !cl_ent->inuse || game.clients[ i ].resp.spectator 
            /*|| !cl_ent->client->pers.connected*/ ) {
            continue;
        }
        numberOfClients++;
    }
    // Now send the number of clients.
    gi.WriteUint8( numberOfClients );

    // Now, for each client, send index, time, score, and ping.
    for ( int32_t i = 0; i < game.maxclients; i++ ) {
        edict_t *cl_ent = g_edicts + 1 + i;
        if ( !cl_ent->inuse || game.clients[ i ].resp.spectator 
            /*|| !cl_ent->client->pers.connected*/ ) {
            continue;
        }

        int64_t score = game.clients[ i ].resp.score;
        QMTime time = level.time - game.clients[ i ].resp.entertime;
        int16_t ping = game.clients[ i ].ping;

        // Client name is already known by client infos, so just send the index instead.
        gi.WriteUint8( i );
        gi.WriteIntBase128( time.Seconds() );
        gi.WriteIntBase128( score );
        gi.WriteUint16( ping );
    }
    #endif

    // Send it over the wire, using supplied reliable false/true.
    gi.unicast( ent, sendAsReliable );
}

/**
*   @brief  Draw instead of help message.
*   @note that it isn't that hard to overflow the 1400 byte message limit!
**/
void SVG_HUD_DeathmatchScoreboard(edict_t *ent)
{
    SVG_HUD_DeathmatchScoreboardMessage( ent, ent->enemy, true );
}






//=======================================================================
/**
*   @brief  Specific 'Weaponry' substitute function for SVG_HUD_SetStats.
**/
void SVG_SetWeaponStats( edict_t *ent ) {
    //
    // Type of ammo to display the total carrying amount of.
    //
    if ( !ent->client->ammo_index /* || !ent->client->pers.inventory[ent->client->ammo_index] */ ) {
        ent->client->ps.stats[ STAT_AMMO_ICON ] = 0;
        ent->client->ps.stats[ STAT_AMMO ] = 0;
    } else {
        // Acquire the item for fetching the current ammo_index type icon from.
        const gitem_t *item = &itemlist[ ent->client->ammo_index ];
        // Assign icon.
        ent->client->ps.stats[ STAT_AMMO_ICON ] = gi.imageindex( item->icon );
        // Assign amount of total carrying ammo of said ammo_index.
        ent->client->ps.stats[ STAT_AMMO ] = ent->client->pers.inventory[ ent->client->ammo_index ];
    }
    //
    // Weapon Item ID.
    //
    if ( !ent->client->pers.weapon /* || !ent->client->pers.inventory[ent->client->ammo_index] */ ) {
        //ent->client->ps.stats[ STAT_WEAPON_CLIP_AMMO_ICON ] = 0;
        ent->client->ps.stats[ STAT_WEAPON_ITEM ] = 0;
    } else {
        const int32_t weaponItemID = ent->client->pers.weapon->weapon_index;
        ent->client->ps.stats[ STAT_WEAPON_ITEM ] = weaponItemID;
    }
    //
    // Clip Ammo:
    //
    if ( !ent->client->pers.weapon /* || !ent->client->pers.inventory[ent->client->ammo_index] */ ) {
        ent->client->ps.stats[ STAT_WEAPON_CLIP_AMMO ] = 0;
    } else {
        // Find the item matching the 
        //int32_t clip_ammo_item_index = ITEM_INDEX( SVG_FindItem( ent->client->pers.weapon->pickup_name ) );
        const int32_t clip_ammo_item_index = ent->client->pers.weapon->weapon_index;
        //item = &itemlist[ ent->client->ammo_index ];
        //ent->client->ps.stats[ STAT_WEAPON_CLIP_AMMO_ICON ] = gi.imageindex( item->icon );
        ent->client->ps.stats[ STAT_WEAPON_CLIP_AMMO ] = ent->client->pers.weapon_clip_ammo[ clip_ammo_item_index ];
    }
    //
    // WeaponFlags/IsAiming
    //
    int32_t stat_weapon_flags = 0;
    // Apply IS_AIMING flag if we're weaponStating as AIMING IN or isAiming == true.
    if ( ent->client->weaponState.mode >= WEAPON_MODE_AIM_IN && ent->client->weaponState.mode <= WEAPON_MODE_AIM_OUT ) {
        stat_weapon_flags |= STAT_WEAPON_FLAGS_IS_AIMING;
    }
    ent->client->ps.stats[ STAT_WEAPON_FLAGS ] = stat_weapon_flags;

    //
    // Recoil.
    //
    float totalRecoil = SVG_Client_GetFinalRecoilFactor( ent );
    //if ( totalRecoil > 0 ) {
		ent->client->ps.stats[ STAT_WEAPON_RECOIL ] = float_to_half( (float)totalRecoil );
        //gi.dprintf( "%s: STAT_WEAPON_RECOIL(%llu), moveRecoil(%f), firedRecoil(%f)\n", 
        //    __func__, 
        //    ent->client->ps.stats[ STAT_WEAPON_RECOIL ],
        //    ent->client->weaponState.recoil.moveFactor,
        //    ent->client->weaponState.recoil.weaponFactor
        //);
    //} else {
    //    ent->client->weaponState.recoil.weaponFactor = 0;
    //    ent->client->weaponState.recoil.moveFactor = 0;
    //}
}

/**
*   @brief  Will update the client's player_state_t stats array with the current client entity's values.
**/
void SVG_HUD_SetStats(edict_t *ent) {
    //
    // Health
    //
    ent->client->ps.stats[STAT_HEALTH_ICON] = level.pic_health;
    ent->client->ps.stats[STAT_HEALTH] = ent->health;
    
    //
    // Killer Yaw
    //
    ent->client->ps.stats[ STAT_KILLER_YAW ] = ent->client->killer_yaw;

    //
    //  (Clip-)Ammo and Weapon(-Stats).
    //
    SVG_SetWeaponStats( ent );
    
    //
    // Armor
    //
    //index = ArmorIndex(ent);
    //if (index) {
    //    item = SVG_GetItemByIndex(index);
    //    if ( item && item->flags == ITEM_FLAG_ARMOR ) {
    //        ent->client->ps.stats[ STAT_ARMOR_ICON ] = gi.imageindex( item->icon );
    //        ent->client->ps.stats[ STAT_ARMOR ] = ent->client->pers.inventory[ index ];
    //    }
    //} else {
        ent->client->ps.stats[STAT_ARMOR_ICON] = 0;
        ent->client->ps.stats[STAT_ARMOR] = 0;
    //}

    //
    // Pickup Message
    //
    if (level.time > ent->client->pickup_msg_time ) {
        ent->client->ps.stats[STAT_PICKUP_ICON] = 0;
        ent->client->ps.stats[STAT_PICKUP_STRING] = 0;
    }

    //
    // Timer 1 (quad, enviro, breather)
    //
    //if (ent->client->quad_time > level.time) {
    //    ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("p_quad");
    //    ent->client->ps.stats[STAT_TIMER] = (ent->client->quad_time - level.time).seconds( ) / BASE_FRAMERATE;
    //} else if (ent->client->invincible_time > level.time) {
    //    ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("p_invulnerability");
    //    ent->client->ps.stats[STAT_TIMER] = (ent->client->invincible_time - level.time).seconds( ) / BASE_FRAMERATE;
    //} else if (ent->client->enviro_time > level.time) {
    //    ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("p_envirosuit");
    //    ent->client->ps.stats[STAT_TIMER] = (ent->client->enviro_time - level.time).seconds( ) / BASE_FRAMERATE;
    //} else if (ent->client->breather_time > level.time) {
    //    ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("p_rebreather");
    //    ent->client->ps.stats[STAT_TIMER] = (ent->client->breather_time - level.time).seconds() / BASE_FRAMERATE;
    //	//ent->client->ps.stats[ STAT_TIMER ] = ( ent->client->breather_time - level.time ) / 10;
    //} else {
        ent->client->ps.stats[STAT_TIMER_ICON] = 0;
        ent->client->ps.stats[STAT_TIMER] = 0;
    //}

    //
    // Timer 2 (pent)
    //
    ent->client->ps.stats[STAT_TIMER2_ICON] = 0;
    ent->client->ps.stats[STAT_TIMER2] = 0;


    //
    // Selected Item
    //
    if ( ent->client->pers.selected_item == -1 ) {
        ent->client->ps.stats[ STAT_SELECTED_ICON ] = 0;
    } else {
        ent->client->ps.stats[ STAT_SELECTED_ICON ] = gi.imageindex( itemlist[ ent->client->pers.selected_item ].icon );
    }
    ent->client->ps.stats[STAT_SELECTED_ITEM] = ent->client->pers.selected_item;

    //
    // layouts
    //
    ent->client->ps.stats[STAT_LAYOUTS] = 0;

    if (deathmatch->value) {
        if (ent->client->pers.health <= 0 || level.intermissionFrameNumber
            || ent->client->showscores)
            ent->client->ps.stats[STAT_LAYOUTS] |= 1;
        if (ent->client->showinventory && ent->client->pers.health > 0)
            ent->client->ps.stats[STAT_LAYOUTS] |= 2;
    } else {
        if (ent->client->showscores || ent->client->showhelp)
            ent->client->ps.stats[STAT_LAYOUTS] |= 1;
        if (ent->client->showinventory && ent->client->pers.health > 0)
            ent->client->ps.stats[STAT_LAYOUTS] |= 2;
    }

    //
    // GUI
    //
    //ent->client->ps.stats[ STAT_SHOW_SCORES ] = 0;
    //if ( gamemode->integer != GAMEMODE_SINGLEPLAYER ) {
    //    if ( ent->client->showscores ) {
    //        ent->client->ps.stats[ STAT_SHOW_SCORES ] |= 1;
    //    } else {
    //        ent->client->ps.stats[ STAT_SHOW_SCORES ] &= ~1;
    //    }
    //}

    //
    // Frags
    //
    ent->client->ps.stats[STAT_FRAGS] = ent->client->resp.score;

    // If this function was called, disable spectator mode stats.
    ent->client->ps.stats[STAT_SPECTATOR] = 0;
}

/**
*   @brief
**/
void SVG_HUD_SetSpectatorStats( edict_t *ent ) {
    svg_client_t *cl = ent->client;

    if ( !cl->chase_target )
        SVG_HUD_SetStats( ent );

    // If this function was called, enable spectator mode stats.
    cl->ps.stats[ STAT_SPECTATOR ] = 1;

    // layouts are independant in spectator
    cl->ps.stats[ STAT_LAYOUTS ] = 0;
    if ( cl->pers.health <= 0 || level.intermissionFrameNumber || cl->showscores )
        cl->ps.stats[ STAT_LAYOUTS ] |= 1;
    if ( cl->showinventory && cl->pers.health > 0 )
        cl->ps.stats[ STAT_LAYOUTS ] |= 2;

    if ( cl->chase_target && cl->chase_target->inuse )
        cl->ps.stats[ STAT_CHASE ] = CS_PLAYERSKINS +
        ( cl->chase_target - g_edicts ) - 1;
    else
        cl->ps.stats[ STAT_CHASE ] = 0;
}

/**
*   @brief
**/
void SVG_HUD_CheckChaseStats(edict_t *ent)
{
    int i;
    svg_client_t *cl;

    for (i = 1; i <= maxclients->value; i++) {
        cl = g_edicts[i].client;
        if (!g_edicts[i].inuse || cl->chase_target != ent)
            continue;
        memcpy(cl->ps.stats, ent->client->ps.stats, sizeof(cl->ps.stats));
        SVG_HUD_SetSpectatorStats(g_edicts + i);
    }
}
