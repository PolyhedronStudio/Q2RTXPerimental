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
#include "svgame/svg_local.h"
#include "svgame/player/svg_m_player.h"
#include "svgame/svg_lua.h"


/**
*	@brief
**/
/**
*	@brief
**/
void SVG_Lua_DumpStack( lua_State *L );
void SVG_Command_Lua_DumpStack( void ) {
    // Dump stack.
    SVG_Lua_DumpStack( SVG_Lua_GetSolStateView() );
}


char *ClientTeam(edict_t *ent)
{
    char        *p;
    static char value[512];

    value[0] = 0;

    if (!ent->client)
        return value;

    strcpy(value, Info_ValueForKey(ent->client->pers.userinfo, "skin"));
    p = strchr(value, '/');
    if (!p)
        return value;

    if ((int)(dmflags->value) & DF_MODELTEAMS) {
        *p = 0;
        return value;
    }

    // if ((int)(dmflags->value) & DF_SKINTEAMS)
    return ++p;
}

const bool SVG_OnSameTeam(edict_t *ent1, edict_t *ent2)
{
    char    ent1Team [512];
    char    ent2Team [512];

    if (!((int)(dmflags->value) & (DF_MODELTEAMS | DF_SKINTEAMS)))
        return false;

    strcpy(ent1Team, ClientTeam(ent1));
    strcpy(ent2Team, ClientTeam(ent2));

    if (strcmp(ent1Team, ent2Team) == 0)
        return true;
    return false;
}


void SelectNextItem(edict_t *ent, int itflags)
{
    gclient_t   *cl;
    int         i, index;
    gitem_t     *it;

    cl = ent->client;

    if (cl->chase_target) {
        SVG_ChaseCam_Next(ent);
        return;
    }

    // scan  for the next valid one
    for (i = 1 ; i <= MAX_ITEMS ; i++) {
        index = (cl->pers.selected_item + i) % MAX_ITEMS;
        if (!cl->pers.inventory[index])
            continue;
        it = &itemlist[index];
        if (!it->use)
            continue;
        if (!(it->flags & itflags))
            continue;

        cl->pers.selected_item = index;
        return;
    }

    cl->pers.selected_item = -1;
}

void SelectPrevItem(edict_t *ent, int itflags)
{
    gclient_t   *cl;
    int         i, index;
    gitem_t     *it;

    cl = ent->client;

    if (cl->chase_target) {
        SVG_ChaseCam_Previous(ent);
        return;
    }

    // scan  for the next valid one
    for (i = 1 ; i <= MAX_ITEMS ; i++) {
        index = (cl->pers.selected_item + MAX_ITEMS - i) % MAX_ITEMS;
        if (!cl->pers.inventory[index])
            continue;
        it = &itemlist[index];
        if (!it->use)
            continue;
        if (!(it->flags & itflags))
            continue;

        cl->pers.selected_item = index;
        return;
    }

    cl->pers.selected_item = -1;
}

void SVG_HUD_ValidateSelectedItem(edict_t *ent)
{
    gclient_t   *cl;

    cl = ent->client;

    if (cl->pers.inventory[cl->pers.selected_item])
        return;     // valid

    SelectNextItem(ent, -1);
}


//=================================================================================

/*
==================
SVG_Command_Give_f

Give items to a client
==================
*/
void SVG_Command_Give_f(edict_t *ent)
{
    char        *name;
    const gitem_t     *it;
    int         index;
    int         i;
    bool        give_all;
    edict_t     *it_ent;

    if ((deathmatch->value || coop->value) && !sv_cheats->value) {
        gi.cprintf(ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
        return;
    }

    name = gi.args();

    if (Q_stricmp(name, "all") == 0)
        give_all = true;
    else
        give_all = false;

    if (give_all || Q_stricmp(gi.argv(1), "health") == 0) {
        if (gi.argc() == 3)
            ent->health = atoi(gi.argv(2));
        else
            ent->health = ent->max_health;
        if (!give_all)
            return;
    }

    if (give_all || Q_stricmp(name, "weapons") == 0) {
        for (i = 0 ; i < game.num_items ; i++) {
            it = itemlist + i;
            if (!it->pickup)
                continue;
            if (!(it->flags & ITEM_FLAG_WEAPON))
                continue;
            ent->client->pers.inventory[i] += 1;
        }
        if (!give_all)
            return;
    }

    if (give_all || Q_stricmp(name, "ammo") == 0) {
        for (i = 0 ; i < game.num_items ; i++) {
            it = itemlist + i;
            if (!it->pickup)
                continue;
            if (!(it->flags & ITEM_FLAG_AMMO))
                continue;
            Add_Ammo(ent, it, 1000);
        }
        if (!give_all)
            return;
    }

    //if (give_all || Q_stricmp(name, "armor") == 0) {
    //    gitem_armor_t   *info;

    //    it = FindItem("Jacket Armor");
    //    ent->client->pers.inventory[ITEM_INDEX(it)] = 0;

    //    it = FindItem("Combat Armor");
    //    ent->client->pers.inventory[ITEM_INDEX(it)] = 0;

    //    it = FindItem("Body Armor");
    //    info = (gitem_armor_t *)it->info;
    //    ent->client->pers.inventory[ITEM_INDEX(it)] = info->max_count;

    //    if (!give_all)
    //        return;
    //}

    //if (give_all || Q_stricmp(name, "Power Shield") == 0) {
    //    it = FindItem("Power Shield");
    //    it_ent = SVG_AllocateEdict();
    //    it_ent->classname = it->classname;
    //    SpawnItem(it_ent, it);
    //    Touch_Item(it_ent, ent, NULL, NULL);
    //    if (it_ent->inuse)
    //        SVG_FreeEdict(it_ent);

    //    if (!give_all)
    //        return;
    //}

    if (give_all) {
        for (i = 0 ; i < game.num_items ; i++) {
            it = itemlist + i;
            if (!it->pickup)
                continue;
            if (it->flags & (ITEM_FLAG_ARMOR | ITEM_FLAG_WEAPON | ITEM_FLAG_AMMO))
                continue;
            ent->client->pers.inventory[i] = 1;
        }
        return;
    }

    it = FindItem(name);
    if (!it) {
        name = gi.argv(1);
        it = FindItem(name);
        if (!it) {
            gi.cprintf(ent, PRINT_HIGH, "unknown item\n");
            return;
        }
    }

    if (!it->pickup) {
        gi.cprintf(ent, PRINT_HIGH, "non-pickup item\n");
        return;
    }

    index = ITEM_INDEX(it);

    if (it->flags & ITEM_FLAG_AMMO) {
        if (gi.argc() == 3)
            ent->client->pers.inventory[index] = atoi(gi.argv(2));
        else
            ent->client->pers.inventory[index] += it->quantity;
    } else {
        it_ent = SVG_AllocateEdict();
        it_ent->classname = it->classname;
        SpawnItem(it_ent, it);
        Touch_Item(it_ent, ent, NULL, NULL);
        if (it_ent->inuse)
            SVG_FreeEdict(it_ent);
    }
}


/*
==================
SVG_Command_God_f

Sets client to godmode

argv(0) god
==================
*/
void SVG_Command_God_f(edict_t *ent)
{
    if ((deathmatch->value || coop->value) && !sv_cheats->value) {
        gi.cprintf(ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
        return;
    }

    ent->flags = static_cast<entity_flags_t>( ent->flags ^ FL_GODMODE );
    if (!(ent->flags & FL_GODMODE))
        gi.cprintf(ent, PRINT_HIGH, "godmode OFF\n");
    else
        gi.cprintf(ent, PRINT_HIGH, "godmode ON\n");
}


/*
==================
SVG_Command_Notarget_f

Sets client to notarget

argv(0) notarget
==================
*/
void SVG_Command_Notarget_f(edict_t *ent)
{
    if ((deathmatch->value || coop->value) && !sv_cheats->value) {
        gi.cprintf(ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
        return;
    }

    ent->flags = static_cast<entity_flags_t>( ent->flags ^ FL_NOTARGET );
    if (!(ent->flags & FL_NOTARGET))
        gi.cprintf(ent, PRINT_HIGH, "notarget OFF\n");
    else
        gi.cprintf(ent, PRINT_HIGH, "notarget ON\n");
}


/*
==================
SVG_Command_Noclip_f

argv(0) noclip
==================
*/
void SVG_Command_Noclip_f(edict_t *ent)
{
    if ((deathmatch->value || coop->value) && !sv_cheats->value) {
        gi.cprintf(ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
        return;
    }

    if (ent->movetype == MOVETYPE_NOCLIP) {
        ent->movetype = MOVETYPE_WALK;
        gi.cprintf(ent, PRINT_HIGH, "noclip OFF\n");
    } else {
        ent->movetype = MOVETYPE_NOCLIP;
        gi.cprintf(ent, PRINT_HIGH, "noclip ON\n");
    }
}


/*
==================
SVG_Command_UseItem_f

Use an inventory item
==================
*/
void SVG_Command_UseItem_f(edict_t *ent) {
    const char *s = gi.args();
    const gitem_t *it = FindItem(s);
    if (!it) {
        gi.cprintf(ent, PRINT_HIGH, "unknown item: %s\n", s);
        return;
    }
    if (!it->use) {
        gi.cprintf(ent, PRINT_HIGH, "Item is not usable.\n");
        return;
    }
    const int32_t index = ITEM_INDEX(it);
    if (!ent->client->pers.inventory[index]) {
        gi.cprintf(ent, PRINT_HIGH, "Out of item: %s\n", s);
        return;
    }

    gi.dprintf( "%s: WE ARE USING %s\n", __func__, s );
    // Dispatch the item use callback.
    it->use(ent, it);
}


/*
==================
SVG_Command_Drop_f

Drop an inventory item
==================
*/
void SVG_Command_Drop_f(edict_t *ent) {
    const char *s = gi.args();
    const gitem_t *it = FindItem(s);
    if (!it) {
        gi.cprintf(ent, PRINT_HIGH, "unknown item: %s\n", s);
        return;
    }
    if (!it->drop) {
        gi.cprintf(ent, PRINT_HIGH, "Item is not dropable.\n");
        return;
    }
    const int32_t index = ITEM_INDEX(it);
    if (!ent->client->pers.inventory[index]) {
        gi.cprintf(ent, PRINT_HIGH, "Out of item: %s\n", s);
        return;
    }

    it->drop(ent, it);
}


/*
=================
SVG_Command_Inven_f
=================
*/
void SVG_Command_Inven_f(edict_t *ent)
{
    int         i;
    gclient_t   *cl;

    cl = ent->client;

    cl->showscores = false;
    cl->showhelp = false;

    if (cl->showinventory) {
        cl->showinventory = false;
        return;
    }

    cl->showinventory = true;

    gi.WriteUint8(svc_inventory);
    for (i = 0 ; i < MAX_ITEMS ; i++) {
        gi.WriteIntBase128(cl->pers.inventory[i]);
    }
    gi.unicast(ent, true);
}

/*
=================
SVG_Command_InvUseSelectedItem_f
=================
*/
void SVG_Command_InvUseSelectedItem_f(edict_t *ent)
{
    gitem_t     *it;

    SVG_HUD_ValidateSelectedItem(ent);

    if (ent->client->pers.selected_item == -1) {
        gi.cprintf(ent, PRINT_HIGH, "No item to use.\n");
        return;
    }

    it = &itemlist[ent->client->pers.selected_item];
    if (!it->use) {
        gi.cprintf(ent, PRINT_HIGH, "Item is not usable.\n");
        return;
    }
    it->use(ent, it);
}

/*
=================
SVG_Command_WeapPrev_f
=================
*/
void SVG_Command_WeapPrev_f(edict_t *ent)
{
    gclient_t   *cl;
    int         i, index;
    gitem_t     *it;
    int         selected_weapon;

    cl = ent->client;

    if (!cl->pers.weapon)
        return;

    selected_weapon = ITEM_INDEX(cl->pers.weapon);

    // scan  for the next valid one
    for (i = 1 ; i <= MAX_ITEMS ; i++) {
        index = (selected_weapon + i) % MAX_ITEMS;
        if (!cl->pers.inventory[index])
            continue;
        it = &itemlist[index];
        if (!it->use)
            continue;
        if (!(it->flags & ITEM_FLAG_WEAPON))
            continue;
        it->use(ent, it);
        if (cl->pers.weapon == it)
            return; // successful
    }
}

/*
=================
SVG_Command_WeapNext_f
=================
*/
void SVG_Command_WeapNext_f(edict_t *ent)
{
    gclient_t   *cl;
    int         i, index;
    gitem_t     *it;
    int         selected_weapon;

    cl = ent->client;

    if (!cl->pers.weapon)
        return;

    selected_weapon = ITEM_INDEX(cl->pers.weapon);

    // scan  for the next valid one
    for (i = 1 ; i <= MAX_ITEMS ; i++) {
        index = (selected_weapon + MAX_ITEMS - i) % MAX_ITEMS;
        if (!cl->pers.inventory[index])
            continue;
        it = &itemlist[index];
        if (!it->use)
            continue;
        if (!(it->flags & ITEM_FLAG_WEAPON))
            continue;
        it->use(ent, it);
        if (cl->pers.weapon == it)
            return; // successful
    }
}

/*
=================
SVG_Command_WeapLast_f
=================
*/
void SVG_Command_WeapLast_f(edict_t *ent)
{
    gclient_t   *cl;
    int         index;
    gitem_t     *it;

    cl = ent->client;

    if (!cl->pers.weapon || !cl->pers.lastweapon)
        return;

    index = ITEM_INDEX(cl->pers.lastweapon);
    if (!cl->pers.inventory[index])
        return;
    it = &itemlist[index];
    if (!it->use)
        return;
    if (!(it->flags & ITEM_FLAG_WEAPON))
        return;
    it->use(ent, it);
}

/*
=================
SVG_Command_WeapFlare_f
=================
*/
void SVG_Command_WeapFlare_f(edict_t* ent) {
    gclient_t *cl = ent->client;
    if (cl->pers.weapon && strcmp(cl->pers.weapon->pickup_name, "Flare Gun") == 0) {
        SVG_Command_WeapLast_f(ent);
    } else {
        const gitem_t *it = FindItem("Flare Gun");
        it->use(ent, it);
    }
}

/*
=================
SVG_Command_InvDrop_f
=================
*/
void SVG_Command_InvDrop_f(edict_t *ent) {
    gitem_t     *it;

    SVG_HUD_ValidateSelectedItem(ent);

    if (ent->client->pers.selected_item == -1) {
        gi.cprintf(ent, PRINT_HIGH, "No item to drop.\n");
        return;
    }

    it = &itemlist[ent->client->pers.selected_item];
    if (!it->drop) {
        gi.cprintf(ent, PRINT_HIGH, "Item is not dropable.\n");
        return;
    }
    it->drop(ent, it);
}

/*
=================
SVG_Command_Kill_f
=================
*/
void SVG_Command_Kill_f(edict_t *ent)
{
    if ((level.time - ent->client->respawn_time) < 5_sec)
        return;
    ent->flags = static_cast<entity_flags_t>( ent->flags & ~FL_GODMODE );
    ent->health = 0;
    ent->meansOfDeath = MEANS_OF_DEATH_SUICIDE;
    player_die(ent, ent, ent, 100000, ent->s.origin);
}

/*
=================
SVG_Command_PutAway_f
=================
*/
void SVG_Command_PutAway_f(edict_t *ent)
{
    ent->client->showscores = false;
    ent->client->showhelp = false;
    ent->client->showinventory = false;
}


int PlayerSort(void const *a, void const *b)
{
    int     anum, bnum;

    anum = *(int *)a;
    bnum = *(int *)b;

    anum = game.clients[anum].ps.stats[STAT_FRAGS];
    bnum = game.clients[bnum].ps.stats[STAT_FRAGS];

    if (anum < bnum)
        return -1;
    if (anum > bnum)
        return 1;
    return 0;
}

/*
=================
SVG_Command_Players_f
=================
*/
void SVG_Command_Players_f(edict_t *ent)
{
    int     i;
    int     count;
    char    small[64];
    char    large[1280];
    int     index[256];

    count = 0;
    for (i = 0 ; i < maxclients->value ; i++)
        if (game.clients[i].pers.connected) {
            index[count] = i;
            count++;
        }

    // sort by frags
    qsort(index, count, sizeof(index[0]), PlayerSort);

    // print information
    large[0] = 0;

    for (i = 0 ; i < count ; i++) {
        Q_snprintf(small, sizeof(small), "%3i %s\n",
                   game.clients[index[i]].ps.stats[STAT_FRAGS],
                   game.clients[index[i]].pers.netname);
        if (strlen(small) + strlen(large) > sizeof(large) - 100) {
            // can't print all of them in one packet
            strcat(large, "...\n");
            break;
        }
        strcat(large, small);
    }

    gi.cprintf(ent, PRINT_HIGH, "%s\n%i players\n", large, count);
}


static bool FloodProtect(edict_t *ent)
{
	int     i;
	//edict_t *other;
	//char    text[ 2048 ];
	gclient_t *cl;

	if ( flood_msgs->value ) {
		cl = ent->client;

		if ( level.time < cl->flood_locktill ) {
			gi.cprintf( ent, PRINT_HIGH, "You can't talk for %d more seconds\n",
					   (int)( cl->flood_locktill - level.time ).seconds<int32_t>( ) );
			return true;
		}
		i = cl->flood_whenhead - flood_msgs->value + 1;
        if ( i < 0 ) {
            i = ( sizeof( cl->flood_when ) / sizeof( cl->flood_when[ 0 ] ) ) + i;
        }
        if ( i >= q_countof( cl->flood_when ) ) {
            i = 0;
        }
		if ( cl->flood_when[ i ] &&
			level.time - cl->flood_when[ i ] < sg_time_t::from_sec( flood_persecond->value ) ) {
			cl->flood_locktill = level.time + sg_time_t::from_sec( flood_waitdelay->value );
			gi.cprintf(
				ent, PRINT_CHAT, "Flood protection:  You can't talk for %d seconds.\n",
					   (int)flood_waitdelay->value );
			return true;
		}
		cl->flood_whenhead = ( cl->flood_whenhead + 1 ) %
			( sizeof( cl->flood_when ) / sizeof( cl->flood_when[ 0 ] ) );
		cl->flood_when[ cl->flood_whenhead ] = level.time;
	}
    return false;
}

/*
==================
SVG_Command_Say_f
==================
*/
void SVG_Command_Say_f(edict_t *ent, bool team, bool arg0)
{
    int     j;
    edict_t *other;
    char    text[2048];

    if (gi.argc() < 2 && !arg0)
        return;

    if (FloodProtect(ent))
        return;

    if (!((int)(dmflags->value) & (DF_MODELTEAMS | DF_SKINTEAMS)))
        team = false;

    if (team)
        Q_snprintf(text, sizeof(text), "(%s): ", ent->client->pers.netname);
    else
        Q_snprintf(text, sizeof(text), "%s: ", ent->client->pers.netname);

    if (arg0) {
        strcat(text, gi.argv(0));
        strcat(text, " ");
        strcat(text, gi.args());
    } else {
        Q_strlcat(text, COM_StripQuotes(gi.args()), sizeof(text));
    }

    // don't let text be too long for malicious reasons
    if (strlen(text) > 150)
        text[150] = 0;

    strcat(text, "\n");

    if (dedicated->value)
        gi.cprintf(NULL, PRINT_CHAT, "%s", text);

    for (j = 1; j <= game.maxclients; j++) {
        other = &g_edicts[j];
        if (!other->inuse)
            continue;
        if (!other->client)
            continue;
        if (team) {
            if (!SVG_OnSameTeam(ent, other))
                continue;
        }
        gi.cprintf(other, PRINT_CHAT, "%s", text);
    }
}

void SVG_Command_PlayerList_f(edict_t *ent)
{
    int i;
    char st[80];
    char text[1400];
    edict_t *e2;

    // connect time, ping, score, name
    *text = 0;
    for (i = 0, e2 = g_edicts + 1; i < maxclients->value; i++, e2++) {
        if (!e2->inuse)
            continue;

        Q_snprintf(st, sizeof(st), "%02ld:%02ld %4d %3d %s%s\n",
                   (level.framenum - e2->client->resp.enterframe) / 600,
                   //((level.framenum - e2->client->resp.enterframe) % 600) / 10,
				   ( ( level.framenum - e2->client->resp.enterframe ) % 600 ) / BASE_FRAMERATE,
                   e2->client->ping,
                   e2->client->resp.score,
                   e2->client->pers.netname,
                   e2->client->resp.spectator ? " (spectator)" : "");
        if (strlen(text) + strlen(st) > sizeof(text) - 50) {
            if (strlen(text) < sizeof(text) - 12)
                strcat(text, "And more...\n");
            gi.cprintf(ent, PRINT_HIGH, "%s", text);
            return;
        }
        strcat(text, st);
    }
    gi.cprintf(ent, PRINT_HIGH, "%s", text);
}


/*
=================
ClientCommand
=================
*/
void ClientCommand( edict_t *ent ) {
    char *cmd;

    if ( !ent->client )
        return;     // not fully in game yet

    cmd = gi.argv( 0 );

    if ( Q_stricmp( cmd, "players" ) == 0 ) {
        SVG_Command_Players_f( ent );
        return;
    }
    if ( Q_stricmp( cmd, "say" ) == 0 ) {
        SVG_Command_Say_f( ent, false, false );
        return;
    }
    if ( Q_stricmp( cmd, "say_team" ) == 0 ) {
        SVG_Command_Say_f( ent, true, false );
        return;
    }
    if ( Q_stricmp( cmd, "score" ) == 0 ) {
        SVG_Command_Score_f( ent );
        return;
    }

    if ( level.intermission_framenum ) {
        return;
    }

    //
    // Inventory:
    //
    if ( Q_stricmp( cmd, "useitem" ) == 0 ) {
        SVG_Command_UseItem_f( ent );
    } else if ( Q_stricmp( cmd, "drop" ) == 0 ) {
        SVG_Command_Drop_f( ent );
    }
    if ( Q_stricmp( cmd, "inven" ) == 0 ) {
        SVG_Command_Inven_f( ent );
    } else if ( Q_stricmp( cmd, "invnext" ) == 0 ) {
        SelectNextItem( ent, -1 );
    } else if ( Q_stricmp( cmd, "invprev" ) == 0 ) {
        SelectPrevItem( ent, -1 );
    } else if ( Q_stricmp( cmd, "invnextw" ) == 0 ) {
        SelectNextItem( ent, ITEM_FLAG_WEAPON );
    } else if ( Q_stricmp( cmd, "invprevw" ) == 0 ) {
        SelectPrevItem( ent, ITEM_FLAG_WEAPON );
    } else if ( Q_stricmp( cmd, "invuse" ) == 0 ) {
        SVG_Command_InvUseSelectedItem_f( ent );
    } else if ( Q_stricmp( cmd, "invdrop" ) == 0 ) {
        SVG_Command_InvDrop_f( ent );
    } else if ( Q_stricmp( cmd, "weapprev" ) == 0 ) {
        SVG_Command_WeapPrev_f( ent );
    } else if ( Q_stricmp( cmd, "weapnext" ) == 0 ) {
        SVG_Command_WeapNext_f( ent );
    } else if ( Q_stricmp( cmd, "weaplast" ) == 0 ) {
        SVG_Command_WeapLast_f( ent );
    } else if ( Q_stricmp( cmd, "putaway" ) == 0 ) {
        SVG_Command_PutAway_f( ent );
    //
    // 'Cheats':
    //
    } else if ( Q_stricmp( cmd, "give" ) == 0 ) {
        SVG_Command_Give_f( ent );
    } else if ( Q_stricmp( cmd, "god" ) == 0 ) {
        SVG_Command_God_f( ent );
    } else if ( Q_stricmp( cmd, "notarget" ) == 0 ) {
        SVG_Command_Notarget_f( ent );
    } else if ( Q_stricmp( cmd, "noclip" ) == 0 ) {
        SVG_Command_Noclip_f( ent );
    //
    // Lua:
    // 
    } else if ( Q_stricmp( cmd, "lua_dumpstack" ) == 0 ) {
        SVG_Command_Lua_DumpStack();
    //
    // Other:
    //
    } else if ( Q_stricmp( cmd, "kill" ) == 0 ) {
        SVG_Command_Kill_f( ent );
    } else if ( Q_stricmp( cmd, "playerlist" ) == 0 ) {
        SVG_Command_PlayerList_f( ent );
    } else if ( Q_stricmp( cmd, "weapflare" ) == 0 ) {
        SVG_Command_WeapFlare_f( ent );
    //
    // Anything that doesn't match a command will be a chat.
    //
    } else {
        SVG_Command_Say_f( ent, false, true );
    }
}

