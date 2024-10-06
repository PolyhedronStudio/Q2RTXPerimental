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

#include "g_local.h"


/*
======================================================================

INTERMISSION

======================================================================
*/

void SVG_HUD_MoveClientToIntermission(edict_t *ent)
{
    if ( deathmatch->value || coop->value ) {
        ent->client->showscores = true;
    } else {
        ent->client->showscores = false;
    }
    ent->client->showhelp = false;

    VectorCopy( level.intermission_origin, ent->s.origin );
    VectorCopy( level.intermission_origin, ent->client->ps.pmove.origin );
    VectorCopy( level.intermission_angle, ent->client->ps.viewangles );
    ent->client->ps.viewangles[ 0 ] = AngleMod( level.intermission_angle[ 0 ] );
    ent->client->ps.viewangles[ 1 ] = AngleMod( level.intermission_angle[ 1 ] );
    ent->client->ps.viewangles[ 2 ] = AngleMod( level.intermission_angle[ 2 ] );
    if ( !G_IsMultiplayerGameMode( game.gamemode ) ) {
        ent->client->ps.pmove.pm_type = PM_SPINTERMISSION;
    } else {
        ent->client->ps.pmove.pm_type = PM_INTERMISSION;
    }
    ent->client->ps.gunindex = 0;
    /*ent->client->ps.damage_blend[3] = */ent->client->ps.screen_blend[ 3 ] = 0; // damageblend?
    ent->client->ps.rdflags = RDF_NONE;

    // clean up powerup info
    ent->client->quad_time = 0_ms;
    ent->client->invincible_time = 0_ms;
    ent->client->breather_time = 0_ms;
    ent->client->enviro_time = 0_ms;
    ent->client->grenade_blew_up = false;
    ent->client->grenade_time = 0_ms;

    ent->liquidtype = CONTENTS_NONE;
    ent->liquidlevel = liquid_level_t::LIQUID_NONE;;
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

    // add the layout
    if (deathmatch->value || coop->value) {
        SVG_HUD_DeathmatchScoreboardMessage(ent, NULL);
        gi.unicast(ent, true);
    }

}

void SVG_HUD_BeginIntermission(edict_t *targ)
{
    int     i, n;
    edict_t *ent, *client;

    if (level.intermission_framenum)
        return;     // already activated

    game.autosaved = false;

    // SVG_Client_Respawn any dead clients
    for (i = 0 ; i < maxclients->value ; i++) {
        client = g_edicts + 1 + i;
        if (!client->inuse)
            continue;
        if (client->health <= 0)
            SVG_Client_Respawn(client);
    }

    level.intermission_framenum = level.framenum;
    level.changemap = targ->map;

    if (strchr(level.changemap, '*')) {
        if (coop->value) {
            for (i = 0 ; i < maxclients->value ; i++) {
                client = g_edicts + 1 + i;
                if (!client->inuse)
                    continue;
                // strip players of all keys between units
                for (n = 0; n < game.num_items; n++) {
                    if (itemlist[n].flags & IT_KEY)
                        client->client->pers.inventory[n] = 0;
                }
                client->client->pers.power_cubes = 0;
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
    ent = SVG_Find(NULL, FOFS(classname), "info_player_intermission");
    if (!ent) {
        // the map creator forgot to put in an intermission point...
        ent = SVG_Find(NULL, FOFS(classname), "info_player_start");
        if (!ent)
            ent = SVG_Find(NULL, FOFS(classname), "info_player_deathmatch");
    } else {
        // chose one of four spots
        i = Q_rand() & 3;
        while (i--) {
            ent = SVG_Find(ent, FOFS(classname), "info_player_intermission");
            if (!ent)   // wrap around the list
                ent = SVG_Find(ent, FOFS(classname), "info_player_intermission");
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


/*
==================
SVG_HUD_DeathmatchScoreboardMessage

==================
*/
void SVG_HUD_DeathmatchScoreboardMessage(edict_t *ent, edict_t *killer)
{
    char    entry[1024];
    char    string[1400];
    int     stringlength;
    int     i, j, k;
    int     sorted[MAX_CLIENTS];
    int     sortedscores[MAX_CLIENTS];
    int     score, total;
    int     x, y;
    gclient_t   *cl;
    edict_t     *cl_ent;
	// WID: C++20: Added const.
    const char    *tag;

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
                   x, y, sorted[i], cl->resp.score, cl->ping, (level.framenum - cl->resp.enterframe) / 600);
        j = strlen(entry);
        if (stringlength + j > 1024)
            break;
        strcpy(string + stringlength, entry);
        stringlength += j;
    }

    gi.WriteUint8(svc_layout);
    gi.WriteString(string);
#if 0
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
        sg_time_t time = level.time - game.clients[ i ].resp.entertime;
        int16_t ping = game.clients[ i ].ping;

        // Client name is already known by client infos, so just send the index instead.
        gi.WriteUint8( i );
        gi.WriteIntBase128( time.seconds() );
        gi.WriteIntBase128( score );
        gi.WriteUint16( ping );
    }
#endif
}


/*
==================
DeathmatchScoreboard

Draw instead of help message.
Note that it isn't that hard to overflow the 1400 byte message limit!
==================
*/
void DeathmatchScoreboard(edict_t *ent)
{
    SVG_HUD_DeathmatchScoreboardMessage(ent, ent->enemy);
    gi.unicast(ent, true);
}


/*
==================
SVG_Cmd_Score_f

Display the scoreboard
==================
*/
void SVG_Cmd_Score_f(edict_t *ent)
{
    ent->client->showinventory = false;
    ent->client->showhelp = false;

    if (!deathmatch->value && !coop->value)
        return;

    if (ent->client->showscores) {
        ent->client->showscores = false;
        return;
    }

    ent->client->showscores = true;
    DeathmatchScoreboard(ent);
}


/*
==================
HelpComputer

Draw help computer.
==================
*/
void HelpComputer(edict_t *ent)
{
    char    string[1024];
	// WID: C++20: Added const.
    const char    *sk;

    if (skill->value == 0)
        sk = "easy";
    else if (skill->value == 1)
        sk = "medium";
    else if (skill->value == 2)
        sk = "hard";
    else
        sk = "hard+";

    // send the layout
    Q_snprintf(string, sizeof(string),
               "xv 32 yv 8 picn help "         // background
               "xv 202 yv 12 string2 \"%s\" "      // skill
               "xv 0 yv 24 cstring2 \"%s\" "       // level name
               "xv 0 yv 54 cstring2 \"%s\" "       // help 1
               "xv 0 yv 110 cstring2 \"%s\" "      // help 2
               "xv 50 yv 164 string2 \" kills     goals    secrets\" "
               "xv 50 yv 172 string2 \"%3i/%3i     %i/%i       %i/%i\" ",
               sk,
               level.level_name,
               game.helpmessage1,
               game.helpmessage2,
               level.killed_monsters, level.total_monsters,
               level.found_goals, level.total_goals,
               level.found_secrets, level.total_secrets);

    gi.WriteUint8(svc_layout);
    gi.WriteString(string);
    gi.unicast(ent, true);
}


/*
==================
SVG_Cmd_Help_f

Display the current help message
==================
*/
void SVG_Cmd_Help_f(edict_t *ent)
{
    // this is for backwards compatability
    if (deathmatch->value) {
        SVG_Cmd_Score_f(ent);
        return;
    }

    ent->client->showinventory = false;
    ent->client->showscores = false;

    if (ent->client->showhelp && (ent->client->pers.game_helpchanged == game.helpchanged)) {
        ent->client->showhelp = false;
        return;
    }

    ent->client->showhelp = true;
    ent->client->pers.helpchanged = 0;
    HelpComputer(ent);
}


//=======================================================================

/*
===============
SVG_HUD_SetStats
===============
*/
void SVG_HUD_SetStats(edict_t *ent)
{
    gitem_t     *item;
    int         index, cells;
    int         power_armor_type;

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
    // Ammo
    //
    if (!ent->client->ammo_index /* || !ent->client->pers.inventory[ent->client->ammo_index] */) {
        ent->client->ps.stats[STAT_AMMO_ICON] = 0;
        ent->client->ps.stats[STAT_AMMO] = 0;
    } else {
        item = &itemlist[ent->client->ammo_index];
        ent->client->ps.stats[STAT_AMMO_ICON] = gi.imageindex(item->icon);
        ent->client->ps.stats[STAT_AMMO] = ent->client->pers.inventory[ent->client->ammo_index];
    }

    //
    // Armor
    //
    power_armor_type = PowerArmorType(ent);
    if (power_armor_type) {
        cells = ent->client->pers.inventory[ITEM_INDEX(FindItem("cells"))];
        if (cells == 0) {
            // ran out of cells for power armor
            ent->flags = static_cast<entity_flags_t>( ent->flags & ~FL_POWER_ARMOR );
            gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/power2.wav"), 1, ATTN_NORM, 0);
            power_armor_type = 0;
        }
    }

    index = ArmorIndex(ent);
    if (power_armor_type && (!index || (level.framenum & 8))) {
        // flash between power armor and other armor icon
        if (power_armor_type == POWER_ARMOR_SHIELD)
            ent->client->ps.stats[STAT_ARMOR_ICON] = gi.imageindex("i_powershield");
        else
            ent->client->ps.stats[STAT_ARMOR_ICON] = gi.imageindex("i_powerscreen");
        ent->client->ps.stats[STAT_ARMOR] = cells;
    } else if (index) {
        item = GetItemByIndex(index);
        ent->client->ps.stats[STAT_ARMOR_ICON] = gi.imageindex(item->icon);
        ent->client->ps.stats[STAT_ARMOR] = ent->client->pers.inventory[index];
    } else {
        ent->client->ps.stats[STAT_ARMOR_ICON] = 0;
        ent->client->ps.stats[STAT_ARMOR] = 0;
    }

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
    if (ent->client->quad_time > level.time) {
        ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("p_quad");
        ent->client->ps.stats[STAT_TIMER] = (ent->client->quad_time - level.time).seconds( ) / BASE_FRAMERATE;
    } else if (ent->client->invincible_time > level.time) {
        ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("p_invulnerability");
        ent->client->ps.stats[STAT_TIMER] = (ent->client->invincible_time - level.time).seconds( ) / BASE_FRAMERATE;
    } else if (ent->client->enviro_time > level.time) {
        ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("p_envirosuit");
        ent->client->ps.stats[STAT_TIMER] = (ent->client->enviro_time - level.time).seconds( ) / BASE_FRAMERATE;
    } else if (ent->client->breather_time > level.time) {
        ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("p_rebreather");
        ent->client->ps.stats[STAT_TIMER] = (ent->client->breather_time - level.time).seconds() / BASE_FRAMERATE;
		//ent->client->ps.stats[ STAT_TIMER ] = ( ent->client->breather_time - level.time ) / 10;
    } else {
        ent->client->ps.stats[STAT_TIMER_ICON] = 0;
        ent->client->ps.stats[STAT_TIMER] = 0;
    }

    //
    // Timer 2 (pent)
    //
    ent->client->ps.stats[STAT_TIMER2_ICON] = 0;
    ent->client->ps.stats[STAT_TIMER2] = 0;
	if ( ent->client->invincible_time > level.time ) {
        if (ent->client->ps.stats[STAT_TIMER_ICON]) {
            ent->client->ps.stats[STAT_TIMER2_ICON] = gi.imageindex("p_invulnerability");
			ent->client->ps.stats[ STAT_TIMER2 ] = ( ent->client->invincible_time - level.time ).seconds( ) / BASE_FRAMERATE;
        } else {
            ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("p_invulnerability");
			ent->client->ps.stats[ STAT_TIMER ] = ( ent->client->invincible_time - level.time ).seconds( ) / BASE_FRAMERATE;
        }
    }

    //
    // Selected Item
    //
    if (ent->client->pers.selected_item == -1)
        ent->client->ps.stats[STAT_SELECTED_ICON] = 0;
    else
        ent->client->ps.stats[STAT_SELECTED_ICON] = gi.imageindex(itemlist[ent->client->pers.selected_item].icon);

    ent->client->ps.stats[STAT_SELECTED_ITEM] = ent->client->pers.selected_item;

    //
    // layouts
    //
    ent->client->ps.stats[STAT_LAYOUTS] = 0;

    if (deathmatch->value) {
        if (ent->client->pers.health <= 0 || level.intermission_framenum
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

    //
    // Help icon / Current weapon if not shown
    //
    if (ent->client->pers.helpchanged && (level.framenum & 8))
        ent->client->ps.stats[STAT_HELPICON] = gi.imageindex("i_help");
    else if ((ent->client->pers.hand == CENTER_HANDED || ent->client->ps.fov > 91)
             && ent->client->pers.weapon)
        ent->client->ps.stats[STAT_HELPICON] = gi.imageindex(ent->client->pers.weapon->icon);
    else
        ent->client->ps.stats[STAT_HELPICON] = 0;

    // If this function was called, disable spectator mode stats.
    ent->client->ps.stats[STAT_SPECTATOR] = 0;
}

/*
===============
SVG_HUD_CheckChaseStats
===============
*/
void SVG_HUD_CheckChaseStats(edict_t *ent)
{
    int i;
    gclient_t *cl;

    for (i = 1; i <= maxclients->value; i++) {
        cl = g_edicts[i].client;
        if (!g_edicts[i].inuse || cl->chase_target != ent)
            continue;
        memcpy(cl->ps.stats, ent->client->ps.stats, sizeof(cl->ps.stats));
        SVG_HUD_SetSpectatorStats(g_edicts + i);
    }
}

/*
===============
SVG_HUD_SetSpectatorStats
===============
*/
void SVG_HUD_SetSpectatorStats(edict_t *ent)
{
    gclient_t *cl = ent->client;

    if (!cl->chase_target)
        SVG_HUD_SetStats(ent);

    // If this function was called, enable spectator mode stats.
    cl->ps.stats[STAT_SPECTATOR] = 1;

    // layouts are independant in spectator
    cl->ps.stats[STAT_LAYOUTS] = 0;
    if (cl->pers.health <= 0 || level.intermission_framenum || cl->showscores)
        cl->ps.stats[STAT_LAYOUTS] |= 1;
    if (cl->showinventory && cl->pers.health > 0)
        cl->ps.stats[STAT_LAYOUTS] |= 2;

    if (cl->chase_target && cl->chase_target->inuse)
        cl->ps.stats[STAT_CHASE] = CS_PLAYERSKINS +
                                   (cl->chase_target - g_edicts) - 1;
    else
        cl->ps.stats[STAT_CHASE] = 0;
}

