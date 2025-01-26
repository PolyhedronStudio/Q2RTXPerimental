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
#include "m_player.h"

void SVG_Client_UserinfoChanged(edict_t *ent, char *userinfo);

void SP_misc_teleporter_dest(edict_t *ent);

//
// Gross, ugly, disgustuing hack section
//

// this function is an ugly as hell hack to fix some map flaws
//
// the coop spawn spots on some maps are SNAFU.  There are coop spots
// with the wrong targetname as well as spots with no name at all
//
// we use carnal knowledge of the maps to fix the coop spot targetnames to match
// that of the nearest named single player spot

void SP_FixCoopSpots(edict_t *self)
{
    edict_t *spot;
    vec3_t  d;

    spot = NULL;

    while (1) {
        spot = SVG_Find(spot, FOFS(classname), "info_player_start");
        if (!spot)
            return;
        if (!spot->targetname)
            continue;
        VectorSubtract(self->s.origin, spot->s.origin, d);
        if (VectorLength(d) < 384) {
            if ((!self->targetname) || Q_stricmp(self->targetname, spot->targetname) != 0) {
//              gi.dprintf("FixCoopSpots changed %s at %s targetname from %s to %s\n", self->classname, vtos(self->s.origin), self->targetname, spot->targetname);
                self->targetname = spot->targetname;
            }
            return;
        }
    }
}

// now if that one wasn't ugly enough for you then try this one on for size
// some maps don't have any coop spots at all, so we need to create them
// where they should have been

void SP_CreateCoopSpots(edict_t *self)
{
    edict_t *spot;

    if (Q_stricmp(level.mapname, "security") == 0) {
        spot = SVG_AllocateEdict();
        spot->classname = "info_player_coop";
        spot->s.origin[0] = 188 - 64;
        spot->s.origin[1] = -164;
        spot->s.origin[2] = 80;
        spot->targetname = "jail3";
        spot->s.angles[1] = 90;

        spot = SVG_AllocateEdict();
        spot->classname = "info_player_coop";
        spot->s.origin[0] = 188 + 64;
        spot->s.origin[1] = -164;
        spot->s.origin[2] = 80;
        spot->targetname = "jail3";
        spot->s.angles[1] = 90;

        spot = SVG_AllocateEdict();
        spot->classname = "info_player_coop";
        spot->s.origin[0] = 188 + 128;
        spot->s.origin[1] = -164;
        spot->s.origin[2] = 80;
        spot->targetname = "jail3";
        spot->s.angles[1] = 90;

        return;
    }
}


/*QUAKED info_player_start (1 0 0) (-16 -16 -24) (16 16 32)
The normal starting point for a level.
*/
void SP_info_player_start(edict_t *self)
{
    if (!coop->value)
        return;
    if (Q_stricmp(level.mapname, "security") == 0) {
        // invoke one of our gross, ugly, disgusting hacks
        self->think = SP_CreateCoopSpots;
		self->nextthink = level.time + FRAME_TIME_S;
    }
}

/*QUAKED info_player_deathmatch (1 0 1) (-16 -16 -24) (16 16 32)
potential spawning position for deathmatch games
*/
void SP_info_player_deathmatch(edict_t *self)
{
    if (!deathmatch->value) {
        SVG_FreeEdict(self);
        return;
    }
    SP_misc_teleporter_dest(self);
}

/*QUAKED info_player_coop (1 0 1) (-16 -16 -24) (16 16 32)
potential spawning position for coop games
*/

void SP_info_player_coop(edict_t *self)
{
    if (!coop->value) {
        SVG_FreeEdict(self);
        return;
    }

    if ((Q_stricmp(level.mapname, "jail2") == 0)   ||
        (Q_stricmp(level.mapname, "jail4") == 0)   ||
        (Q_stricmp(level.mapname, "mine1") == 0)   ||
        (Q_stricmp(level.mapname, "mine2") == 0)   ||
        (Q_stricmp(level.mapname, "mine3") == 0)   ||
        (Q_stricmp(level.mapname, "mine4") == 0)   ||
        (Q_stricmp(level.mapname, "lab") == 0)     ||
        (Q_stricmp(level.mapname, "boss1") == 0)   ||
        (Q_stricmp(level.mapname, "fact3") == 0)   ||
        (Q_stricmp(level.mapname, "biggun") == 0)  ||
        (Q_stricmp(level.mapname, "space") == 0)   ||
        (Q_stricmp(level.mapname, "command") == 0) ||
        (Q_stricmp(level.mapname, "power2") == 0) ||
        (Q_stricmp(level.mapname, "strike") == 0)) {
        // invoke one of our gross, ugly, disgusting hacks
        self->think = SP_FixCoopSpots;
		self->nextthink = level.time + FRAME_TIME_S;
    }
}


/*QUAKED info_player_intermission (1 0 1) (-16 -16 -24) (16 16 32)
The deathmatch intermission point will be at one of these
Use 'angles' instead of 'angle', so you can set pitch or roll as well as yaw.  'pitch yaw roll'
*/
void SP_info_player_intermission(edict_t *ent)
{
}


//=======================================================================


void player_pain(edict_t *self, edict_t *other, float kick, int damage)
{
    // player pain is handled at the end of the frame in P_DamageFeedback
}


bool IsFemale(edict_t *ent)
{
    char        *info;

    if (!ent->client)
        return false;

    info = Info_ValueForKey(ent->client->pers.userinfo, "gender");
    if (info[0] == 'f' || info[0] == 'F')
        return true;
    return false;
}

bool IsNeutral(edict_t *ent)
{
    char        *info;

    if (!ent->client)
        return false;

    info = Info_ValueForKey(ent->client->pers.userinfo, "gender");
    if (info[0] != 'f' && info[0] != 'F' && info[0] != 'm' && info[0] != 'M')
        return true;
    return false;
}

void SVG_Player_Obituary(edict_t *self, edict_t *inflictor, edict_t *attacker)
{
    int         mod;
	// WID: C++20: Added const.
    const char        *message;
    const char        *message2;
    int         ff;

    if (coop->value && attacker->client)
        meansOfDeath |= MOD_FRIENDLY_FIRE;

    if (deathmatch->value || coop->value) {
        ff = meansOfDeath & MOD_FRIENDLY_FIRE;
        mod = meansOfDeath & ~MOD_FRIENDLY_FIRE;
        message = NULL;
        message2 = "";

        switch (mod) {
        case MOD_SUICIDE:
            message = "suicides";
            break;
        case MOD_FALLING:
            message = "cratered";
            break;
        case MOD_CRUSH:
            message = "was squished";
            break;
        case MOD_WATER:
            message = "sank like a rock";
            break;
        case MOD_SLIME:
            message = "melted";
            break;
        case MOD_LAVA:
            message = "does a back flip into the lava";
            break;
        case MOD_EXPLOSIVE:
        case MOD_BARREL:
            message = "blew up";
            break;
        case MOD_EXIT:
            message = "found a way out";
            break;
        case MOD_TARGET_LASER:
            message = "saw the light";
            break;
        case MOD_TARGET_BLASTER:
            message = "got blasted";
            break;
        case MOD_BOMB:
        case MOD_SPLASH:
        case MOD_TRIGGER_HURT:
            message = "was in the wrong place";
            break;
        }
        if (attacker == self) {
            switch (mod) {
            case MOD_HELD_GRENADE:
                message = "tried to put the pin back in";
                break;
            case MOD_HG_SPLASH:
            case MOD_G_SPLASH:
                if (IsNeutral(self))
                    message = "tripped on its own grenade";
                else if (IsFemale(self))
                    message = "tripped on her own grenade";
                else
                    message = "tripped on his own grenade";
                break;
            case MOD_R_SPLASH:
                if (IsNeutral(self))
                    message = "blew itself up";
                else if (IsFemale(self))
                    message = "blew herself up";
                else
                    message = "blew himself up";
                break;
            case MOD_BFG_BLAST:
                message = "should have used a smaller gun";
                break;
            default:
                if (IsNeutral(self))
                    message = "killed itself";
                else if (IsFemale(self))
                    message = "killed herself";
                else
                    message = "killed himself";
                break;
            }
        }
        if (message) {
            gi.bprintf(PRINT_MEDIUM, "%s %s.\n", self->client->pers.netname, message);
            if (deathmatch->value)
                self->client->resp.score--;
            self->enemy = NULL;
            return;
        }

        self->enemy = attacker;
        if (attacker && attacker->client) {
            switch (mod) {
            case MOD_BLASTER:
                message = "was blasted by";
                break;
            case MOD_SHOTGUN:
                message = "was gunned down by";
                break;
            case MOD_SSHOTGUN:
                message = "was blown away by";
                message2 = "'s super shotgun";
                break;
            case MOD_MACHINEGUN:
                message = "was machinegunned by";
                break;
            case MOD_CHAINGUN:
                message = "was cut in half by";
                message2 = "'s chaingun";
                break;
            case MOD_GRENADE:
                message = "was popped by";
                message2 = "'s grenade";
                break;
            case MOD_G_SPLASH:
                message = "was shredded by";
                message2 = "'s shrapnel";
                break;
            case MOD_ROCKET:
                message = "ate";
                message2 = "'s rocket";
                break;
            case MOD_R_SPLASH:
                message = "almost dodged";
                message2 = "'s rocket";
                break;
            case MOD_HYPERBLASTER:
                message = "was melted by";
                message2 = "'s hyperblaster";
                break;
            case MOD_RAILGUN:
                message = "was railed by";
                break;
            case MOD_BFG_LASER:
                message = "saw the pretty lights from";
                message2 = "'s BFG";
                break;
            case MOD_BFG_BLAST:
                message = "was disintegrated by";
                message2 = "'s BFG blast";
                break;
            case MOD_BFG_EFFECT:
                message = "couldn't hide from";
                message2 = "'s BFG";
                break;
            case MOD_HANDGRENADE:
                message = "caught";
                message2 = "'s handgrenade";
                break;
            case MOD_HG_SPLASH:
                message = "didn't see";
                message2 = "'s handgrenade";
                break;
            case MOD_HELD_GRENADE:
                message = "feels";
                message2 = "'s pain";
                break;
            case MOD_TELEFRAG:
                message = "tried to invade";
                message2 = "'s personal space";
                break;
            }
            if (message) {
                gi.bprintf(PRINT_MEDIUM, "%s %s %s%s\n", self->client->pers.netname, message, attacker->client->pers.netname, message2);
                if (deathmatch->value) {
                    if (ff)
                        attacker->client->resp.score--;
                    else
                        attacker->client->resp.score++;
                }
                return;
            }
        }
    }

    gi.bprintf(PRINT_MEDIUM, "%s died.\n", self->client->pers.netname);
    if (deathmatch->value)
        self->client->resp.score--;
}


void Touch_Item(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf);

void TossClientWeapon(edict_t *self)
{
    gitem_t     *item;
    edict_t     *drop;
    bool        quad;
    float       spread;

    if (!deathmatch->value)
        return;

    item = self->client->pers.weapon;
    if (! self->client->pers.inventory[self->client->ammo_index])
        item = NULL;
    if (item && (strcmp(item->pickup_name, "Blaster") == 0))
        item = NULL;

    if (!((int)(dmflags->value) & DF_QUAD_DROP))
        quad = false;
    else
		quad = ( self->client->quad_time > ( level.time + 1_sec ) );

    if (item && quad)
        spread = 22.5f;
    else
        spread = 0.0f;

    if (item) {
        self->client->v_angle[YAW] -= spread;
        drop = Drop_Item(self, item);
        self->client->v_angle[YAW] += spread;
        drop->spawnflags = DROPPED_PLAYER_ITEM;
    }

    if (quad) {
        self->client->v_angle[YAW] += spread;
        drop = Drop_Item(self, FindItemByClassname("item_quad"));
        self->client->v_angle[YAW] -= spread;
        drop->spawnflags |= DROPPED_PLAYER_ITEM;

        drop->touch = Touch_Item;
        drop->nextthink = self->client->quad_time;
        drop->think = SVG_FreeEdict;
    }
}


/*
==================
LookAtKiller
==================
*/
void LookAtKiller(edict_t *self, edict_t *inflictor, edict_t *attacker)
{
    vec3_t dir = {};
    float killerYaw = 0.f;

    if (attacker && attacker != world && attacker != self) {
        VectorSubtract(attacker->s.origin, self->s.origin, dir);
    } else if (inflictor && inflictor != world && inflictor != self) {
        VectorSubtract(inflictor->s.origin, self->s.origin, dir);
    } else {
        self->client->killer_yaw = /*self->client->ps.stats[ STAT_KILLER_YAW ] */ self->s.angles[YAW];
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

/*
==================
player_die
==================
*/
void player_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
    int     n;

    VectorClear(self->avelocity);

    self->takedamage = DAMAGE_YES;
    self->movetype = MOVETYPE_TOSS;

    // Unset weapon model.
    self->s.modelindex2 = 0;    // remove linked weapon model

    // Clear X and Z angles.
    self->s.angles[0] = 0;
    self->s.angles[2] = 0;

    // Stop playing any sounds.
    self->s.sound = 0;
    self->client->weapon_sound = 0;

    // Set bbox maxs to -8.
    self->maxs[2] = -8;

//  self->solid = SOLID_NOT;
    // Flag as to be treated as 'deadmonster' collision.
    self->svflags |= SVF_DEADMONSTER;

    if (!self->deadflag) {
		self->client->respawn_time = ( level.time + 1_sec );
        LookAtKiller(self, inflictor, attacker);
        self->client->ps.pmove.pm_type = PM_DEAD;
        SVG_Player_Obituary(self, inflictor, attacker);
        TossClientWeapon(self);
        if (deathmatch->value)
            SVG_Cmd_Help_f(self);       // show scores

        // clear inventory
        // this is kind of ugly, but it's how we want to handle keys in coop
        for (n = 0; n < game.num_items; n++) {
            if (coop->value && itemlist[n].flags & IT_KEY)
                self->client->resp.coop_respawn.inventory[n] = self->client->pers.inventory[n];
            self->client->pers.inventory[n] = 0;
        }
    }

    // Remove powerups.
    self->client->quad_time = 0_ms;
    self->client->invincible_time = 0_ms;
    self->client->breather_time = 0_ms;
    self->client->enviro_time = 0_ms;
    self->flags = static_cast<entity_flags_t>( self->flags & ~FL_POWER_ARMOR );

    // Gib Death:
    if (self->health < -40) {
        // Play gib sound.
        gi.sound(self, CHAN_BODY, gi.soundindex("misc/udeath.wav"), 1, ATTN_NORM, 0);
        //! Throw 4 small meat gibs around.
        for ( n = 0; n < 4; n++ ) {
            SVG_Misc_ThrowGib( self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_TYPE_ORGANIC );
        }
        // Turn ourself into the thrown head entity.
        SVG_Misc_ThrowClientHead(self, damage);

        // Gibs don't take damage, but fade away as time passes.
        self->takedamage = DAMAGE_NO;
    // Normal death:
    } else {
        if (!self->deadflag) {
            static int i;

            i = (i + 1) % 3;
            // start a death animation
            self->client->anim_priority = ANIM_DEATH;
            if (self->client->ps.pmove.pm_flags & PMF_DUCKED) {
                self->s.frame = FRAME_crdeath1 - 1;
                self->client->anim_end = FRAME_crdeath5;
            } else switch (i) {
                case 0:
                    self->s.frame = FRAME_death101 - 1;
                    self->client->anim_end = FRAME_death106;
                    break;
                case 1:
                    self->s.frame = FRAME_death201 - 1;
                    self->client->anim_end = FRAME_death206;
                    break;
                case 2:
                    self->s.frame = FRAME_death301 - 1;
                    self->client->anim_end = FRAME_death308;
                    break;
                }
            gi.sound(self, CHAN_VOICE, gi.soundindex(va("*death%i.wav", (Q_rand() % 4) + 1)), 1, ATTN_NORM, 0);

			self->client->anim_time = 0_ms; // WID: 40hz:
        }
    }

    self->deadflag = DEADFLAG_DEAD;

    gi.linkentity(self);
}

//=======================================================================

/*
==============
SVG_Player_InitPersistantData

This is only called when the game first initializes in single player,
but is called after each death and level change in deathmatch
==============
*/
void SVG_Player_InitPersistantData(edict_t *ent, gclient_t *client)
{
	gitem_t     *item;

    // Clear out persistent data.
    client->pers = {};

    // Find the blaster item, add it to our inventory and appoint it as the selected weapon.
	item = FindItem("Blaster");
	client->pers.selected_item = ITEM_INDEX(item);
	client->pers.inventory[client->pers.selected_item] = 1;

	client->pers.weapon = item;

	if (sv_flaregun->value > 0)
	{
		// Q2RTX: Spawn with a flare gun and some grenades to use with it.
		// Flare gun is new and not found anywhere in the game as a pickup item.
		gitem_t* item_flareg = FindItem("Flare Gun");
		if (item_flareg)
		{
			client->pers.inventory[ITEM_INDEX(item_flareg)] = 1;

			if (sv_flaregun->value == 2)
			{
				gitem_t* item_grenades = FindItem("Grenades");
				client->pers.inventory[ITEM_INDEX(item_grenades)] = 5;
			}
		}
	}

    client->pers.health         = 100;
    client->pers.max_health     = 100;

    client->pers.max_bullets    = 200;
    client->pers.max_shells     = 100;
    client->pers.max_rockets    = 50;
    client->pers.max_grenades   = 50;
    client->pers.max_cells      = 200;
    client->pers.max_slugs      = 50;

    client->pers.connected = true;
    client->pers.spawned = true;
}


void SVG_Player_InitRespawnData(gclient_t *client)
{
    // Clear out SVG_Client_RespawnPlayer data.
    client->resp = {};

    // Save the moment in time.
    client->resp.enterframe = level.frameNumber;
    client->resp.entertime = level.time;

    // In case of a coop game mode, we make sure to store the 
    // 'persistent across level changes' data into the client's
    // SVG_Client_RespawnPlayer field, so we can restore it.
    client->resp.coop_respawn = client->pers;
}

/*
==================
SVG_Player_SaveClientData

Some information that should be persistant, like health,
is still stored in the edict structure, so it needs to
be mirrored out to the client structure before all the
edicts are wiped.
==================
*/
void SVG_Player_SaveClientData(void)
{
    int     i;
    edict_t *ent;

    for (i = 0 ; i < game.maxclients ; i++) {
        ent = &g_edicts[1 + i];
        if (!ent->inuse)
            continue;
        game.clients[i].pers.health = ent->health;
        game.clients[i].pers.max_health = ent->max_health;
        game.clients[i].pers.savedFlags = static_cast<entity_flags_t>(ent->flags & (FL_GODMODE | FL_NOTARGET | FL_POWER_ARMOR));
        if (coop->value)
            game.clients[i].pers.score = ent->client->resp.score;
    }
}

void SVG_Player_RestoreClientData(edict_t *ent)
{
    ent->health = ent->client->pers.health;
    ent->max_health = ent->client->pers.max_health;
    ent->flags |= ent->client->pers.savedFlags;
    if (coop->value)
        ent->client->resp.score = ent->client->pers.score;
}



/*
=======================================================================

  SelectSpawnPoint

=======================================================================
*/

/*
================
PlayersRangeFromSpot

Returns the distance to the nearest player from the given spot
================
*/
float   PlayersRangeFromSpot(edict_t *spot)
{
    edict_t *player;
    float   bestplayerdistance;
    vec3_t  v;
    int     n;
    float   playerdistance;


    bestplayerdistance = 9999999;

    for (n = 1; n <= maxclients->value; n++) {
        player = &g_edicts[n];

        if (!player->inuse)
            continue;

        if (player->health <= 0)
            continue;

        VectorSubtract(spot->s.origin, player->s.origin, v);
        playerdistance = VectorLength(v);

        if (playerdistance < bestplayerdistance)
            bestplayerdistance = playerdistance;
    }

    return bestplayerdistance;
}

/*
================
SelectRandomDeathmatchSpawnPoint

go to a random point, but NOT the two points closest
to other players
================
*/
edict_t *SelectRandomDeathmatchSpawnPoint(void)
{
    edict_t *spot, *spot1, *spot2;
    int     count = 0;
    int     selection;
    float   range, range1, range2;

    spot = NULL;
    range1 = range2 = 99999;
    spot1 = spot2 = NULL;

    while ((spot = SVG_Find(spot, FOFS(classname), "info_player_deathmatch")) != NULL) {
        count++;
        range = PlayersRangeFromSpot(spot);
        if (range < range1) {
            range1 = range;
            spot1 = spot;
        } else if (range < range2) {
            range2 = range;
            spot2 = spot;
        }
    }

    if (!count)
        return NULL;

    if (count <= 2) {
        spot1 = spot2 = NULL;
    } else
        count -= 2;

    selection = Q_rand_uniform(count);

    spot = NULL;
    do {
        spot = SVG_Find(spot, FOFS(classname), "info_player_deathmatch");
        if (spot == spot1 || spot == spot2)
            selection++;
    } while (selection--);

    return spot;
}

/*
================
SelectFarthestDeathmatchSpawnPoint

================
*/
edict_t *SelectFarthestDeathmatchSpawnPoint(void)
{
    edict_t *bestspot;
    float   bestdistance, bestplayerdistance;
    edict_t *spot;


    spot = NULL;
    bestspot = NULL;
    bestdistance = 0;
    while ((spot = SVG_Find(spot, FOFS(classname), "info_player_deathmatch")) != NULL) {
        bestplayerdistance = PlayersRangeFromSpot(spot);

        if (bestplayerdistance > bestdistance) {
            bestspot = spot;
            bestdistance = bestplayerdistance;
        }
    }

    if (bestspot) {
        return bestspot;
    }

    // if there is a player just spawned on each and every start spot
    // we have no choice to turn one into a telefrag meltdown
    spot = SVG_Find(NULL, FOFS(classname), "info_player_deathmatch");

    return spot;
}

edict_t *SelectDeathmatchSpawnPoint(void)
{
    if ((int)(dmflags->value) & DF_SPAWN_FARTHEST)
        return SelectFarthestDeathmatchSpawnPoint();
    else
        return SelectRandomDeathmatchSpawnPoint();
}


edict_t *SelectCoopSpawnPoint(edict_t *ent)
{
    int     index;
    edict_t *spot = NULL;
	// WID: C++20: Added const.
	const char    *target;

    index = ent->client - game.clients;

    // player 0 starts in normal player spawn point
    if (!index)
        return NULL;

    spot = NULL;

    // assume there are four coop spots at each spawnpoint
    while (1) {
        spot = SVG_Find(spot, FOFS(classname), "info_player_coop");
        if (!spot)
            return NULL;    // we didn't have enough...

        target = spot->targetname;
        if (!target)
            target = "";
        if (Q_stricmp(game.spawnpoint, target) == 0) {
            // this is a coop spawn point for one of the clients here
            index--;
            if (!index)
                return spot;        // this is it
        }
    }


    return spot;
}


/*
===========
SelectSpawnPoint

Chooses a player start, deathmatch start, coop start, etc
============
*/
void    SelectSpawnPoint(edict_t *ent, vec3_t origin, vec3_t angles)
{
    edict_t *spot = NULL;

    if (deathmatch->value)
        spot = SelectDeathmatchSpawnPoint();
    else if (coop->value)
        spot = SelectCoopSpawnPoint(ent);

    // find a single player start spot
    if (!spot) {
        while ((spot = SVG_Find(spot, FOFS(classname), "info_player_start")) != NULL) {
            if (!game.spawnpoint[0] && !spot->targetname)
                break;

            if (!game.spawnpoint[0] || !spot->targetname)
                continue;

            if (Q_stricmp(game.spawnpoint, spot->targetname) == 0)
                break;
        }

        if (!spot) {
            if (!game.spawnpoint[0]) {
                // there wasn't a spawnpoint without a target, so use any
                spot = SVG_Find(spot, FOFS(classname), "info_player_start");
            }
            if (!spot)
                gi.error("Couldn't find spawn point %s", game.spawnpoint);
        }
    }

    VectorCopy(spot->s.origin, origin);
    VectorCopy(spot->s.angles, angles);
}

//======================================================================


void SVG_InitBodyQue(void)
{
    int     i;
    edict_t *ent;

    level.body_que = 0;
    for (i = 0; i < BODY_QUEUE_SIZE ; i++) {
        ent = SVG_AllocateEdict();
        ent->classname = "bodyque";
    }
}

void body_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
    int n;

    if (self->health < -40) {
        gi.sound(self, CHAN_BODY, gi.soundindex("misc/udeath.wav"), 1, ATTN_NORM, 0);
        for (n = 0; n < 4; n++)
            SVG_Misc_ThrowGib(self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_TYPE_ORGANIC);
        self->s.origin[2] -= 48;
        SVG_Misc_ThrowClientHead(self, damage);
        self->takedamage = DAMAGE_NO;
    }
}

void CopyToBodyQue(edict_t *ent)
{
    edict_t     *body;

    gi.unlinkentity(ent);

    // grab a body que and cycle to the next one
    body = &g_edicts[game.maxclients + level.body_que + 1];
    level.body_que = (level.body_que + 1) % BODY_QUEUE_SIZE;

    // send an effect on the removed body
    if (body->s.modelindex) {
        gi.WriteUint8(svc_temp_entity);
        gi.WriteUint8(TE_BLOOD);
        gi.WritePosition( body->s.origin, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
        gi.WriteDir8(vec3_origin);
        gi.multicast( body->s.origin, MULTICAST_PVS, false );
    }

    gi.unlinkentity(body);

    body->s.number = body - g_edicts;
    VectorCopy(ent->s.origin, body->s.origin);
    VectorCopy(ent->s.origin, body->s.old_origin);
    VectorCopy(ent->s.angles, body->s.angles);
    body->s.modelindex = ent->s.modelindex;
    body->s.frame = ent->s.frame;
    body->s.skinnum = ent->s.skinnum;
    body->s.event = EV_OTHER_TELEPORT;

    body->svflags = ent->svflags;
    VectorCopy(ent->mins, body->mins);
    VectorCopy(ent->maxs, body->maxs);
    VectorCopy(ent->absmin, body->absmin);
    VectorCopy(ent->absmax, body->absmax);
    VectorCopy(ent->size, body->size);
    VectorCopy(ent->velocity, body->velocity);
    VectorCopy(ent->avelocity, body->avelocity);
    body->solid = ent->solid;
    body->clipmask = ent->clipmask;
    body->owner = ent->owner;
    body->movetype = ent->movetype;
    body->groundentity = ent->groundentity;

    body->die = body_die;
    body->takedamage = DAMAGE_YES;

    gi.linkentity(body);
}

void SVG_Client_RespawnPlayer(edict_t *self)
{
    if (deathmatch->value || coop->value) {
        // spectator's don't leave bodies
        if (self->movetype != MOVETYPE_NOCLIP)
            CopyToBodyQue(self);
        self->svflags &= ~SVF_NOCLIENT;
        SVG_Player_PutInServer(self);

        // add a teleportation effect
        self->s.event = EV_PLAYER_TELEPORT;

        // hold in place briefly
        self->client->ps.pmove.pm_flags = PMF_TIME_TELEPORT;
        self->client->ps.pmove.pm_time = 14; // WID: 40hz: Q2RE has 112 here.

        self->client->respawn_time = level.time;

        return;
    }

    // restart the entire server
    gi.AddCommandString("pushmenu loadgame\n");
}

/*
 * only called when pers.spectator changes
 * note that resp.spectator should be the opposite of pers.spectator here
 */
void spectator_respawn(edict_t *ent)
{
    int i, numspec;

    // if the user wants to become a spectator, make sure he doesn't
    // exceed max_spectators

    if (ent->client->pers.spectator) {
        char *value = Info_ValueForKey(ent->client->pers.userinfo, "spectator");
        if (*spectator_password->string &&
            strcmp(spectator_password->string, "none") &&
            strcmp(spectator_password->string, value)) {
            gi.cprintf(ent, PRINT_HIGH, "Spectator password incorrect.\n");
            ent->client->pers.spectator = false;
            gi.WriteUint8(svc_stufftext);
            gi.WriteString("spectator 0\n");
            gi.unicast(ent, true);
            return;
        }

        // count spectators
        for (i = 1, numspec = 0; i <= maxclients->value; i++)
            if (g_edicts[i].inuse && g_edicts[i].client->pers.spectator)
                numspec++;

        if (numspec >= maxspectators->value) {
            gi.cprintf(ent, PRINT_HIGH, "Server spectator limit is full.");
            ent->client->pers.spectator = false;
            // reset his spectator var
            gi.WriteUint8(svc_stufftext);
            gi.WriteString("spectator 0\n");
            gi.unicast(ent, true);
            return;
        }
    } else {
        // he was a spectator and wants to join the game
        // he must have the right password
        char *value = Info_ValueForKey(ent->client->pers.userinfo, "password");
        if (*password->string && strcmp(password->string, "none") &&
            strcmp(password->string, value)) {
            gi.cprintf(ent, PRINT_HIGH, "Password incorrect.\n");
            ent->client->pers.spectator = true;
            gi.WriteUint8(svc_stufftext);
            gi.WriteString("spectator 1\n");
            gi.unicast(ent, true);
            return;
        }
    }

    // clear client on SVG_Client_RespawnPlayer
    ent->client->resp.score = ent->client->pers.score = 0;

    ent->svflags &= ~SVF_NOCLIENT;
    SVG_Player_PutInServer(ent);

    // add a teleportation effect
    if (!ent->client->pers.spectator)  {
        // send effect
        gi.WriteUint8(svc_muzzleflash);
        gi.WriteInt16(ent - g_edicts);
        gi.WriteUint8(MZ_LOGIN);
        gi.multicast( ent->s.origin, MULTICAST_PVS, false );

        // hold in place briefly
        ent->client->ps.pmove.pm_flags = PMF_TIME_TELEPORT;
        ent->client->ps.pmove.pm_time = 14; // WID: 40hz: Also here, 112.
    }

    ent->client->respawn_time = level.time;

    if (ent->client->pers.spectator)
        gi.bprintf(PRINT_HIGH, "%s has moved to the sidelines\n", ent->client->pers.netname);
    else
        gi.bprintf(PRINT_HIGH, "%s joined the game\n", ent->client->pers.netname);
}

//==============================================================


/*
===========
SVG_Player_PutInServer

Called when a player connects to a server or respawns in
a deathmatch.
============
*/
void SVG_Player_PutInServer(edict_t *ent)
{
    vec3_t  mins = { -16, -16, -24};
    vec3_t  maxs = {16, 16, 32};
    int     index;
    vec3_t  spawn_origin, spawn_angles;
    gclient_t   *client;
    int     i;
    client_persistant_t saved;
    client_respawn_t    resp;
    vec3_t temp, temp2;
    trace_t tr;

    // find a spawn point
    // do it before setting health back up, so farthest
    // ranging doesn't count this client
    SelectSpawnPoint(ent, spawn_origin, spawn_angles);

    index = ent - g_edicts - 1;
    client = ent->client;

    // deathmatch wipes most client data every spawn
    if (deathmatch->value) {
        char        userinfo[ MAX_INFO_STRING ];
        resp = client->resp;
        memcpy( userinfo, client->pers.userinfo, sizeof( userinfo ) );
        SVG_Player_InitPersistantData( ent, client );
        SVG_Client_UserinfoChanged( ent, userinfo );
    } else {
//      int         n;
        char        userinfo[ MAX_INFO_STRING ];
        resp = client->resp;
        memcpy( userinfo, client->pers.userinfo, sizeof( userinfo ) );
        // this is kind of ugly, but it's how we want to handle keys in coop
//      for (n = 0; n < game.num_items; n++)
//      {
//          if (itemlist[n].flags & IT_KEY)
//              resp.coop_respawn.inventory[n] = client->pers.inventory[n];
//      }
        resp.coop_respawn.game_helpchanged = client->pers.game_helpchanged;
        resp.coop_respawn.helpchanged = client->pers.helpchanged;
        client->pers = resp.coop_respawn;
        SVG_Client_UserinfoChanged( ent, userinfo );
        if (resp.score > client->pers.score)
            client->pers.score = resp.score;
    } 


    // clear everything but the persistant data
    saved = client->pers;
    memset(client, 0, sizeof(*client));
    client->pers = saved;
    if (client->pers.health <= 0)
        SVG_Player_InitPersistantData( ent, client );
    client->resp = resp;

    // copy some data from the client to the entity
    SVG_Player_RestoreClientData(ent);

    // fix level switch issue
    ent->client->pers.connected = true;

    // clear entity values
    ent->groundentity = NULL;
    ent->client = &game.clients[index];
    ent->takedamage = DAMAGE_AIM;
    ent->movetype = MOVETYPE_WALK;
    ent->viewheight = 22;
    ent->inuse = true;
    ent->classname = "player";
    ent->mass = 200;
    ent->solid = SOLID_BOUNDS_BOX;
    ent->deadflag = DEADFLAG_NO;
    ent->air_finished_time = level.time + 12_sec;
    ent->clipmask = ( MASK_PLAYERSOLID );
    ent->model = "players/male/tris.md2";
    ent->pain = player_pain;
    ent->die = player_die;
    ent->liquidlevel = liquid_level_t::LIQUID_NONE;;
    ent->liquidtype = CONTENTS_NONE;
    ent->flags = static_cast<entity_flags_t>( ent->flags & ~FL_NO_KNOCKBACK );

    ent->svflags &= ~SVF_DEADMONSTER;
    ent->svflags &= ~FL_NO_KNOCKBACK;
    ent->svflags |= SVF_PLAYER;

    VectorCopy(mins, ent->mins);
    VectorCopy(maxs, ent->maxs);
    VectorClear(ent->velocity);

    // clear playerstate values
    memset(&ent->client->ps, 0, sizeof(client->ps));

    if (deathmatch->value && ((int)dmflags->value & DF_FIXED_FOV)) {
        client->ps.fov = 90;
    } else {
        client->ps.fov = atoi(Info_ValueForKey(client->pers.userinfo, "fov"));
        if (client->ps.fov < 1)
            client->ps.fov = 90;
        else if (client->ps.fov > 160)
            client->ps.fov = 160;
    }

    // Set viewheight for player state pmove.
    ent->client->ps.pmove.viewheight = ent->viewheight;

    // Proper gunindex.
    if ( client->pers.weapon ) {
        client->ps.gunindex = gi.modelindex( client->pers.weapon->view_model );
    } else {
        client->ps.gunindex = 0;
    }    

    // clear entity state values
    ent->s.sound = 0;
    ent->s.effects = 0;
    ent->s.renderfx = 0;
    ent->s.modelindex = 255;        // will use the skin specified model
    ent->s.modelindex2 = 255;       // custom gun model
    // sknum is player num and weapon number
    // weapon number will be added in changeweapon
    ent->s.skinnum = ent - globals.edicts - 1;
    ent->s.frame = 0;
    ent->s.old_frame = 0;

    // try to properly clip to the floor / spawn
    VectorCopy(spawn_origin, temp);
    VectorCopy(spawn_origin, temp2);
    temp[2] -= 64;
    temp2[2] += 16;
    tr = gi.trace(temp2, ent->mins, ent->maxs, temp, ent, ( MASK_PLAYERSOLID ));
    if (!tr.allsolid && !tr.startsolid && Q_stricmp(level.mapname, "tech5")) {
        VectorCopy(tr.endpos, ent->s.origin);
        ent->groundentity = tr.ent;
    } else {
        VectorCopy(spawn_origin, ent->s.origin);
        ent->s.origin[2] += 10; // make sure off ground
    }

    VectorCopy(ent->s.origin, ent->s.old_origin);

    for (i = 0; i < 3; i++) {
        client->ps.pmove.origin[i] = ent->s.origin[i]; // COORD2SHORT(ent->s.origin[i]); // WID: float-movement
    }

    spawn_angles[PITCH] = 0;
    spawn_angles[ROLL] = 0;

    // set the delta angle
    for (i = 0 ; i < 3 ; i++) {
        client->ps.pmove.delta_angles[i] = /*ANGLE2SHORT*/AngleMod( (spawn_angles[i] - client->resp.cmd_angles[i]) );
    }

    VectorCopy(spawn_angles, ent->s.angles);
    VectorCopy(spawn_angles, client->ps.viewangles);
    VectorCopy(spawn_angles, client->v_angle);
    AngleVectors( client->v_angle, client->v_forward, nullptr, nullptr );

    // spawn a spectator
    if (client->pers.spectator) {
        client->chase_target = NULL;

        client->resp.spectator = true;

        ent->movetype = MOVETYPE_NOCLIP;
        ent->solid = SOLID_NOT;
        ent->svflags |= SVF_NOCLIENT;
        ent->client->ps.gunindex = 0;
        gi.linkentity(ent);
        return;
    } else
        client->resp.spectator = false;

    if (!KillBox(ent, true)) {
        // could't spawn in?
    }

    gi.linkentity(ent);

    // force the current weapon up
    client->newweapon = client->pers.weapon;
    ChangeWeapon(ent);
}

/*
=====================
ClientBeginDeathmatch

A client has just connected to the server in
deathmatch mode, so clear everything out before starting them.
=====================
*/
void ClientBeginDeathmatch(edict_t *ent)
{
    // Init Edict.
    SVG_InitEdict(ent);
    
    // Make sure it is recognized as a player.
    ent->svflags |= SVF_PLAYER;

    SVG_Player_InitRespawnData(ent->client);

    // locate ent at a spawn point
    SVG_Player_PutInServer(ent);

    if (level.intermissionFrameNumber) {
        SVG_HUD_MoveClientToIntermission(ent);
    } else {
        // send effect
        gi.WriteUint8(svc_muzzleflash);
        gi.WriteInt16(ent - g_edicts);
        gi.WriteUint8(MZ_LOGIN);
        gi.multicast( ent->s.origin, MULTICAST_PVS, false );
    }

    gi.bprintf(PRINT_HIGH, "%s entered the game\n", ent->client->pers.netname);

    // make sure all view stuff is valid
    SVG_Client_EndServerFrame(ent);
}


/*
===========
ClientBegin

called when a client has finished connecting, and is ready
to be placed into the game.  This will happen every level load.
============
*/
void ClientBegin(edict_t *ent)
{
    int     i;

    ent->client = game.clients + (ent - g_edicts - 1);

    // [Paril-KEX] we're always connected by this point...
    ent->client->pers.connected = true;

    if (deathmatch->value) {
        ClientBeginDeathmatch(ent);
        return;
    }

    ent->client->pers.spawned = true;

    // if there is already a body waiting for us (a loadgame), just
    // take it, otherwise spawn one from scratch
    if (ent->inuse == (qboolean)true) {
        // the client has cleared the client side viewangles upon
        // connecting to the server, which is different than the
        // state when the game is saved, so we need to compensate
        // with deltaangles
        for ( i = 0; i < 3; i++ ) {
            ent->client->ps.pmove.delta_angles[ i ] = /*ANGLE2SHORT*/AngleMod( ent->client->ps.viewangles[ i ] );
        }
    } else {
        // a spawn point will completely reinitialize the entity
        // except for the persistant data that was initialized at
        // ClientConnect() time
        SVG_InitEdict(ent);
        ent->classname = "player";
        SVG_Player_InitRespawnData(ent->client);
        SVG_Player_PutInServer(ent);
    }

    ent->svflags |= SVF_PLAYER;

    if (level.intermissionFrameNumber) {
        SVG_HUD_MoveClientToIntermission(ent);
    } else {
        // send effect if in a multiplayer game
        if (game.maxclients > 1) {
            gi.WriteUint8(svc_muzzleflash);
            gi.WriteInt16(ent - g_edicts);
            gi.WriteUint8(MZ_LOGIN);
            gi.multicast( ent->s.origin, MULTICAST_PVS, false );

            gi.bprintf(PRINT_HIGH, "%s entered the game\n", ent->client->pers.netname);
        }
    }

    // make sure all view stuff is valid
    SVG_Client_EndServerFrame(ent);
}

/*
===========
ClientUserInfoChanged

called whenever the player updates a userinfo variable.

The game can override any of the settings in place
(forcing skins or names, etc) before copying it off.
============
*/
void SVG_Client_UserinfoChanged(edict_t *ent, char *userinfo)
{
    char    *s;
    int     playernum;

    // check for malformed or illegal info strings
    if (!Info_Validate(userinfo)) {
        strcpy(userinfo, "\\name\\badinfo\\skin\\male/grunt");
    }

    // set name
    s = Info_ValueForKey(userinfo, "name");
    Q_strlcpy(ent->client->pers.netname, s, sizeof(ent->client->pers.netname));

    // set spectator
    s = Info_ValueForKey(userinfo, "spectator");
    // spectators are only supported in deathmatch
    if (deathmatch->value && *s && strcmp(s, "0"))
        ent->client->pers.spectator = true;
    else
        ent->client->pers.spectator = false;

    // set skin
    s = Info_ValueForKey(userinfo, "skin");

    playernum = ent - g_edicts - 1;

    // combine name and skin into a configstring
    gi.configstring(CS_PLAYERSKINS + playernum, va("%s\\%s", ent->client->pers.netname, s));

    // fov
    if (deathmatch->value && ((int)dmflags->value & DF_FIXED_FOV)) {
        ent->client->ps.fov = 90;
    } else {
        ent->client->ps.fov = atoi(Info_ValueForKey(userinfo, "fov"));
        if (ent->client->ps.fov < 1)
            ent->client->ps.fov = 90;
        else if (ent->client->ps.fov > 160)
            ent->client->ps.fov = 160;
    }

    // handedness
    s = Info_ValueForKey(userinfo, "hand");
    if (strlen(s)) {
        ent->client->pers.hand = atoi(s);
    }

    // save off the userinfo in case we want to check something later
    Q_strlcpy(ent->client->pers.userinfo, userinfo, sizeof(ent->client->pers.userinfo));
}


/*
===========
ClientConnect

Called when a player begins connecting to the server.
The game can refuse entrance to a client by returning false.
If the client is allowed, the connection process will continue
and eventually get to ClientBegin()
Changing levels will NOT cause this to be called again, but
loadgames will.
============
*/
qboolean ClientConnect(edict_t *ent, char *userinfo)
{
    char    *value;

    // check to see if they are on the banned IP list
    value = Info_ValueForKey(userinfo, "ip");
    if (SVG_FilterPacket(value)) {
        Info_SetValueForKey(userinfo, "rejmsg", "Banned.");
        return false;
    }

    // check for a spectator
    value = Info_ValueForKey(userinfo, "spectator");
    if (deathmatch->value && *value && strcmp(value, "0")) {
        int i, numspec;

        if (*spectator_password->string &&
            strcmp(spectator_password->string, "none") &&
            strcmp(spectator_password->string, value)) {
            Info_SetValueForKey(userinfo, "rejmsg", "Spectator password required or incorrect.");
            return false;
        }

        // count spectators
        for (i = numspec = 0; i < maxclients->value; i++)
            if (g_edicts[i + 1].inuse && g_edicts[i + 1].client->pers.spectator)
                numspec++;

        if (numspec >= maxspectators->value) {
            Info_SetValueForKey(userinfo, "rejmsg", "Server spectator limit is full.");
            return false;
        }
    } else {
        // check for a password
        value = Info_ValueForKey(userinfo, "password");
        if (*password->string && strcmp(password->string, "none") &&
            strcmp(password->string, value)) {
            Info_SetValueForKey(userinfo, "rejmsg", "Password required or incorrect.");
            return false;
        }
    }


    // they can connect
    ent->client = game.clients + (ent - g_edicts - 1);

    // if there is already a body waiting for us (a loadgame), just
    // take it, otherwise spawn one from scratch
    if (ent->inuse == false) {
        // clear the respawning variables
        SVG_Player_InitRespawnData(ent->client);
        if (!game.autosaved || !ent->client->pers.weapon)
            SVG_Player_InitPersistantData( ent, ent->client );
    }

    // make sure we start with known default(s)
    //ent->svflags = SVF_PLAYER;

    SVG_Client_UserinfoChanged(ent, userinfo);

    if ( game.maxclients > 1 ) {
        gi.dprintf( "%s connected\n", ent->client->pers.netname );
    }

    // make sure we start with known default(s)
    ent->svflags = SVF_PLAYER;
    ent->client->pers.connected = true;
    return true;
}

/*
===========
ClientDisconnect

Called when a player drops from the server.
Will not be called between levels.
============
*/
void SVG_Client_Disconnect(edict_t *ent)
{
    //int     playernum;

    if (!ent->client)
        return;

    gi.bprintf(PRINT_HIGH, "%s disconnected\n", ent->client->pers.netname);

    // send effect
    if (ent->inuse) {
        gi.WriteUint8(svc_muzzleflash);
        gi.WriteInt16(ent - g_edicts);
        gi.WriteUint8(MZ_LOGOUT);
        gi.multicast( ent->s.origin, MULTICAST_PVS, false );
    }

    gi.unlinkentity(ent);
    ent->s.modelindex = 0;
    ent->s.modelindex2 = 0;
    ent->s.sound = 0;
    ent->s.event = 0;
    ent->s.effects = 0;
    ent->s.renderfx = 0;
    ent->s.solid = SOLID_NOT; // 0
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


//==============================================================

/**
*   @brief  Player Move specific 'Trace' wrapper implementation.
**/
static const trace_t q_gameabi SV_PM_Trace(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const void *passEntity, const contents_t contentMask ) {
    //if (pm_passent->health > 0)
    //    return gi.trace(start, mins, maxs, end, pm_passent, MASK_PLAYERSOLID);
    //else
    //    return gi.trace(start, mins, maxs, end, pm_passent, MASK_DEADSOLID);
    return gi.trace( start, mins, maxs, end, (edict_t*)passEntity, contentMask );
}
/**
*   @brief  Player Move specific 'Clip' wrapper implementation. Clips to world only.
**/
static const trace_t q_gameabi SV_PM_Clip( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const contents_t contentMask ) {
    return gi.clip( &g_edicts[ 0 ], start, mins, maxs, end, contentMask );
}
/**
*   @brief  Player Move specific 'PointContents' wrapper implementation.
**/
static const contents_t q_gameabi SV_PM_PointContents( const vec3_t point ) {
    return gi.pointcontents( point );
}

/*
=================
P_FallingDamage

Paril-KEX: this is moved here and now reacts directly
to ClientThink rather than being delayed.
=================
*/
void P_FallingDamage( edict_t *ent, const pmove_t &pm ) {
    int	   damage;
    vec3_t dir;

    // dead stuff can't crater
    if ( ent->health <= 0 || ent->deadflag ) {
        return;
    }

    if ( ent->s.modelindex != MODELINDEX_PLAYER ) {
        return; // not in the player model
    }

    if ( ent->movetype == MOVETYPE_NOCLIP ) {
        return;
    }

    // never take falling damage if completely underwater
    if ( pm.liquid.level == LIQUID_UNDER ) {
        return;
    }

    // ZOID
    //  never take damage if just release grapple or on grapple
    //if ( ent->client->ctf_grapplereleasetime >= level.time ||
    //    ( ent->client->ctf_grapple &&
    //        ent->client->ctf_grapplestate > CTF_GRAPPLE_STATE_FLY ) )
    //    return;
    // ZOID

    float delta = pm.impact_delta;

    delta = delta * delta * 0.0001f;

    if ( pm.liquid.level == LIQUID_WAIST ) {
        delta *= 0.25f;
    }
    if ( pm.liquid.level == LIQUID_FEET ) {
        delta *= 0.5f;
    }

    if ( delta < 1 )
        return;

    // restart footstep timer
    ent->client->bobtime = 0;

    //if ( ent->client->landmark_free_fall ) {
    //    delta = min( 30.f, delta );
    //    ent->client->landmark_free_fall = false;
    //    ent->client->landmark_noise_time = level.time + 100_ms;
    //}

    if ( delta < 15 ) {
        if ( !( pm.playerState->pmove.pm_flags & PMF_ON_LADDER ) ) {
            ent->s.event = EV_FOOTSTEP;
            gi.dprintf( "%s: delta < 15 footstep\n", __func__ );
        }
        return;
    }

    ent->client->fall_value = delta * 0.5f;
    if ( ent->client->fall_value > 40 ) {
        ent->client->fall_value = 40;
    }
    ent->client->fall_time = level.time + FALL_TIME();

    if ( delta > 30 ) {
        if ( delta >= 55 ) {
            ent->s.event = EV_FALLFAR;
        } else {
            ent->s.event = EV_FALL;
        }

        ent->pain_debounce_time = level.time + FRAME_TIME_S; // no normal pain sound
        damage = (int)( ( delta - 30 ) / 2 );
        if ( damage < 1 ) {
            damage = 1;
        }
        VectorSet( dir, 0.f, 0.f, 1.f );// dir = { 0, 0, 1 };

        if ( !deathmatch->integer /*|| !g_dm_no_fall_damage->integer*/ ) {
            SVG_TriggerDamage( ent, world, world, dir, ent->s.origin, vec3_origin, damage, 0, DAMAGE_NONE, MOD_FALLING );
        }
    } else {
        ent->s.event = EV_FALLSHORT;
    }

    // Paril: falling damage noises alert monsters
    if ( ent->health )
        SVG_PlayerNoise( ent, &pm.playerState->pmove.origin[ 0 ], PNOISE_SELF);
}

/*
==============
ClientThink

This will be called once for each client frame, which will
usually be a couple times for each server frame.
==============
*/
void ClientThink(edict_t *ent, usercmd_t *ucmd)
{
    gclient_t   *client = nullptr;
    edict_t     *other = nullptr;
    
	// Configure pmove.
    pmove_t pm = {};
    pmoveParams_t pmp = {};
	SG_ConfigurePlayerMoveParameters( &pmp );

    level.current_entity = ent;
    client = ent->client;

    // [Paril-KEX] pass buttons through even if we are in intermission or
    // chasing.
    client->oldbuttons = client->buttons;
    client->buttons = ucmd->buttons;
    client->latched_buttons |= client->buttons & ~client->oldbuttons;
    //client->oldbuttons
    //client->cmd = *ucmd;

    if (level.intermissionFrameNumber) {
        client->ps.pmove.pm_type = PM_FREEZE;

        // can exit intermission after five seconds
        if ( ( level.frameNumber > level.intermissionFrameNumber + 5.0f * BASE_FRAMERATE ) && ( ucmd->buttons & BUTTON_ANY ) ) {
            level.exitintermission = true;
        }

        client->ps.pmove.viewheight = ent->viewheight = 22;

        return;
    }

    if (ent->client->chase_target) {
		VectorCopy( ucmd->angles, client->resp.cmd_angles );
    } else {

        // set up for pmove
		//pm = {};

        // NoClip/Spectator:
		if ( ent->movetype == MOVETYPE_NOCLIP ) {
			if ( ent->client->resp.spectator ) {
				client->ps.pmove.pm_type = PM_SPECTATOR;
			} else {
				client->ps.pmove.pm_type = PM_NOCLIP;
			}
        // If our model index differs, we're gibbing out:
		} else if ( ent->s.modelindex != MODELINDEX_PLAYER ) {
			client->ps.pmove.pm_type = PM_GIB;
        // Dead:
		} else if ( ent->deadflag ) {
			client->ps.pmove.pm_type = PM_DEAD;
        // Otherwise, default, normal movement behavior:
		} else {
			client->ps.pmove.pm_type = PM_NORMAL;
		}

        // PGM	trigger_gravity support
        client->ps.pmove.gravity = (short)( sv_gravity->value * ent->gravity );
        
        //pm.playerState = client->ps;
        pm.playerState = &client->ps;

		// Copy the current entity origin and velocity into our 'pmove movestate'.
        pm.playerState->pmove.origin = ent->s.origin;
        pm.playerState->pmove.velocity = ent->velocity;

		// Determine if it has changed and we should 'resnap' to position.
        if ( memcmp( &client->old_pmove, &pm.playerState->pmove, sizeof( pm.playerState->pmove ) ) ) {
            pm.snapinitial = true; // gi.dprintf ("pmove changed!\n");
        }
		// Setup 'User Command', 'Player Skip Entity' and Function Pointers.
        pm.cmd = *ucmd;
        pm.player = ent;
        pm.trace = SV_PM_Trace;
        pm.pointcontents = SV_PM_PointContents;
        pm.clip = SV_PM_Clip;
        //pm.viewoffset = ent->client->ps.viewoffset;

        // Perform a PMove.
        SG_PlayerMove( &pm, &pmp );

        // Save into the client pointer, the resulting move states pmove
        client->ps = *pm.playerState;
        //client->ps = pm.playerState;
        client->old_pmove = pm.playerState->pmove;
        // Backup the command angles given from ast command.
        VectorCopy( ucmd->angles, client->resp.cmd_angles );

        // Ensure the entity has proper RF_STAIR_STEP applied to it when moving up/down those.
        if ( pm.ground.entity && ent->groundentity ) {
            float stepsize = fabs( ent->s.origin[ 2 ] - pm.playerState->pmove.origin[ 2 ] );

            if ( stepsize > PM_MIN_STEP_SIZE && stepsize < PM_MAX_STEP_SIZE ) {
                ent->s.renderfx |= RF_STAIR_STEP;
                ent->client->last_stair_step_frame = gi.GetServerFrameNumber() + 1;
            }
        }

        // Apply falling damage directly.
        P_FallingDamage( ent, pm );

        //if ( ent->client->landmark_free_fall && pm.groundentity ) {
        //    ent->client->landmark_free_fall = false;
        //    ent->client->landmark_noise_time = level.time + 100_ms;
        //}

        //// [Paril-KEX] save old position for SVG_TouchProjectiles
        //vec3_t old_origin = ent->s.origin;
        
        // [Paril-KEX] if we stepped onto/off of a ladder, reset the
        // last ladder pos
        if ( ( pm.playerState->pmove.pm_flags & PMF_ON_LADDER ) != ( client->ps.pmove.pm_flags & PMF_ON_LADDER ) ) {
            VectorCopy( ent->s.origin, client->last_ladder_pos );

            if ( pm.playerState->pmove.pm_flags & PMF_ON_LADDER ) {
                if ( !deathmatch->integer &&
                    client->last_ladder_sound < level.time ) {
                    ent->s.event = EV_FOOTSTEP_LADDER;
                    client->last_ladder_sound = level.time + LADDER_SOUND_TIME;
                }
            }
        }
        
        // [Paril-KEX] save old position for SVG_TouchProjectiles
        const Vector3 old_origin = ent->s.origin;

		// Copy back into the entity, both the resulting origin and velocity.
		VectorCopy( pm.playerState->pmove.origin, ent->s.origin );
		VectorCopy( pm.playerState->pmove.velocity, ent->velocity );
        // Copy back in bounding box results. (Player might've crouched for example.)
        VectorCopy( pm.mins, ent->mins );
        VectorCopy( pm.maxs, ent->maxs );



        if ( pm.jump_sound && !( pm.playerState->pmove.pm_flags & PMF_ON_LADDER ) ) { //if (~client->ps.pmove.pm_flags & pm.s.pm_flags & PMF_JUMP_HELD && pm.liquid.level == 0) {
            gi.sound( ent, CHAN_VOICE, gi.soundindex( "*jump1.wav" ), 1, ATTN_NORM, 0 );
            // Paril: removed to make ambushes more effective and to
            // not have monsters around corners come to jumps
            // SVG_PlayerNoise(ent, ent->s.origin, PNOISE_SELF);
        }

		// Update the entity's other properties.
        ent->viewheight = (int32_t)pm.playerState->pmove.viewheight;
        ent->liquidlevel = pm.liquid.level;
        ent->liquidtype = pm.liquid.type;
        ent->groundentity = pm.ground.entity;
        if ( pm.ground.entity ) {
            ent->groundentity_linkcount = pm.ground.entity->linkcount;
        }

        if ( ent->deadflag ) {
            client->ps.viewangles[ROLL] = 40;
            client->ps.viewangles[PITCH] = -15;
            client->ps.viewangles[YAW] = client->killer_yaw;
        } else {
            VectorCopy( pm.playerState->viewangles, client->ps.viewangles );
            VectorCopy( client->ps.viewangles, client->v_angle );
            AngleVectors( client->v_angle, client->v_forward, nullptr, nullptr );
        }

        gi.linkentity( ent );

        // PGM trigger_gravity support
        ent->gravity = 1.0;
        // PGM

		if ( ent->movetype != MOVETYPE_NOCLIP ) {
			SVG_TouchTriggers( ent );
            SVG_TouchProjectiles( ent, old_origin );
		}

        // Touch other objects
        for ( int32_t i = 0; i < pm.touchTraces.numberOfTraces; i++ ) {
            trace_t &tr = pm.touchTraces.traces[ i ];
            edict_t *other = tr.ent;

            if ( other->touch ) {
                // TODO: Q2RE has these for last 2 args: const trace_t &tr, bool other_touching_self
                // What the??
                other->touch( other, ent, &tr.plane, tr.surface );
            }
        }

    }

    client->oldbuttons = client->buttons;
    client->buttons = ucmd->buttons;
    client->latched_buttons |= client->buttons & ~client->oldbuttons;

	// save light level the player is standing on for
	// monster sighting AI
	ent->light_level = 255;//ucmd->lightlevel;

    // fire weapon from final position if needed
    if ( client->latched_buttons & BUTTON_ATTACK ) {
        if ( client->resp.spectator ) {

            client->latched_buttons = BUTTON_NONE;

            if ( client->chase_target ) {
                client->chase_target = nullptr;
                client->ps.pmove.pm_flags &= ~( PMF_NO_POSITIONAL_PREDICTION | PMF_NO_ANGULAR_PREDICTION );
			} else {
				SVG_ChaseCam_GetTarget( ent );
			}

        } else if ( !client->weapon_thunk ) {
			// we can only do this during a ready state and
			// if enough time has passed from last fire
			if ( ent->client->weaponstate == WEAPON_READY ) {
				ent->client->weapon_fire_buffered = true;

				if ( ent->client->weapon_fire_finished <= level.time ) {
					ent->client->weapon_thunk = true;
					Think_Weapon( ent );
				}
			}
        }
    }

    if ( client->resp.spectator ) {
        if ( ucmd->upmove >= 10 ) {
            if ( !( client->ps.pmove.pm_flags & PMF_JUMP_HELD ) ) {
                client->ps.pmove.pm_flags |= PMF_JUMP_HELD;

				if ( client->chase_target ) {
					SVG_ChaseCam_Next( ent );
				} else {
					SVG_ChaseCam_GetTarget( ent );
				}
            }
		} else {
            client->ps.pmove.pm_flags &= ~PMF_JUMP_HELD;
		}
    }

    // update chase cam if being followed
    for ( int32_t i = 1; i <= maxclients->value; i++ ) {
        other = g_edicts + i;
        if ( other->inuse && other->client->chase_target == ent ) {
			SVG_ChaseCam_Update( other );
		}
    }
}


/*
==============
SVG_Client_BeginServerFrame

This will be called once for each server frame, before running
any other entities in the world.
==============
*/
void SVG_Client_BeginServerFrame(edict_t *ent)
{
    gclient_t   *client;
    int         buttonMask;

    // Remove RF_STAIR_STEP if we're in a new frame, not stepping.
    if ( gi.GetServerFrameNumber() != ent->client->last_stair_step_frame ) {
        ent->s.renderfx &= ~RF_STAIR_STEP;
    }

    if ( level.intermissionFrameNumber )
        return;

    client = ent->client;

    if ( deathmatch->value &&
        client->pers.spectator != client->resp.spectator &&
        ( level.time - client->respawn_time ) >= 5_sec ) {
        spectator_respawn( ent );
        return;
    }

    // run weapon animations if it hasn't been done by a ucmd_t
    if ( !client->weapon_thunk && !client->resp.spectator ) {
        Think_Weapon(ent);
    } else {
        client->weapon_thunk = false;
    }

    if ( ent->deadflag ) {
        // wait for any button just going down
        if ( level.time > client->respawn_time ) {
            // in deathmatch, only wait for attack button
            if ( deathmatch->value ) {
                buttonMask = BUTTON_ATTACK;
            } else {
                buttonMask = -1;
            }

            if ( ( client->latched_buttons & buttonMask ) ||
                ( deathmatch->value && ( (int)dmflags->value & DF_FORCE_RESPAWN ) ) ) {
                SVG_Client_RespawnPlayer( ent );
                client->latched_buttons = BUTTON_NONE;
            }
        }
        return;
    }

    // add player trail so monsters can follow
    if ( !deathmatch->value ) {
        if ( !visible( ent, PlayerTrail_LastSpot() ) ) {
            PlayerTrail_Add( ent->s.old_origin );
        }
    }
    client->latched_buttons = BUTTON_NONE;
}
