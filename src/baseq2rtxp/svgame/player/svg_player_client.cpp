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
#include "svgame/svg_local.h"
#include "svg_m_player.h"

void ClientUserinfoChanged(edict_t *ent, char *userinfo);

void SP_misc_teleporter_dest(edict_t *ent);


/**
*   @brief  Will process(progress) the entity's active animations for each body state and event states.
**/
void SVG_P_ProcessAnimations( edict_t *ent );

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
        spot = G_Find(spot, FOFS(classname), "info_player_start");
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
        spot = G_AllocateEdict();
        spot->classname = "info_player_coop";
        spot->s.origin[0] = 188 - 64;
        spot->s.origin[1] = -164;
        spot->s.origin[2] = 80;
        spot->targetname = "jail3";
        spot->s.angles[1] = 90;

        spot = G_AllocateEdict();
        spot->classname = "info_player_coop";
        spot->s.origin[0] = 188 + 64;
        spot->s.origin[1] = -164;
        spot->s.origin[2] = 80;
        spot->targetname = "jail3";
        spot->s.angles[1] = 90;

        spot = G_AllocateEdict();
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
        G_FreeEdict(self);
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
        G_FreeEdict(self);
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


//bool IsFemale(edict_t *ent)
//{
//    char        *info;
//
//    if (!ent->client)
//        return false;
//
//    info = Info_ValueForKey(ent->client->pers.userinfo, "gender");
//    if (info[0] == 'f' || info[0] == 'F')
//        return true;
//    return false;
//}
//
//bool IsNeutral(edict_t *ent)
//{
//    char        *info;
//
//    if (!ent->client)
//        return false;
//
//    info = Info_ValueForKey(ent->client->pers.userinfo, "gender");
//    if (info[0] != 'f' && info[0] != 'F' && info[0] != 'm' && info[0] != 'M')
//        return true;
//    return false;
//}

void ClientObituary(edict_t *self, edict_t *inflictor, edict_t *attacker)
{
    sg_means_of_death_t meansOfDeath;
	
    // WID: TODO: In the future, use a gamemode callback for testing if it was friendly fire.
    if ( coop->value && attacker->client ) {
        self->meansOfDeath = static_cast<sg_means_of_death_t>( self->meansOfDeath | MEANS_OF_DEATH_FRIENDLY_FIRE );
    }

    // WID: TODO: Move to gamemode specific callback.
    // WID: TODO: Enable for SP? Might be fun. 
    //! Only perform Means of Death in DM or Coop mode.
    if (deathmatch->value || coop->value) {
        // Store whether it was a friendly fire or not.
        const bool isFriendlyFire = ( self->meansOfDeath & MEANS_OF_DEATH_FRIENDLY_FIRE );
        // Make sure to remove friendly fire flag so our switch statements actually work.
        meansOfDeath = static_cast<sg_means_of_death_t>( self->meansOfDeath & ~MEANS_OF_DEATH_FRIENDLY_FIRE );
        
        // Main death message.
        const char *message = nullptr;
        // Appened to main message when set, after first appending attacker->client->name.
        const char *message2 = "";

        // Determine message based on 'environmental/external' influencing events:
        switch ( meansOfDeath ) {
            case MEANS_OF_DEATH_SUICIDE:
                message = "suicides";
                break;
            case MEANS_OF_DEATH_FALLING:
                message = "fell to death";
                break;
            case MEANS_OF_DEATH_CRUSHED:
                message = "imploded by crush";
                break;
            case MEANS_OF_DEATH_WATER:
                message = "went to swim with the fishes";
                break;
            case MEANS_OF_DEATH_SLIME:
                message = "took an acid bath";
                break;
            case MEANS_OF_DEATH_LAVA:
                message = "burned to hell by lava";
                break;
            case MEANS_OF_DEATH_EXPLOSIVE:
                message = "exploded to gibs";
                break;
            case MEANS_OF_DEATH_EXPLODED_BARREL:
                message = "bumped into an angry barrel";
                break;
            case MEANS_OF_DEATH_EXIT:
                message = "'found' a way out";
                break;
            case MEANS_OF_DEATH_LASER:
                message = "ran into a laser";
                break;
            case MEANS_OF_DEATH_SPLASH:
            case MEANS_OF_DEATH_TRIGGER_HURT:
                message = "was in the wrong place";
                break;
        }

        // Messages for when the attacker is actually ourselves inflicting damage.
        if (attacker == self) {
            //switch (meansOfDeath) {
                // WID: Left as example.
                //case MOD_HELD_GRENADE:
                //    message = "tried to put the pin back in";
                //    break;
            //    default:
                    //if (IsNeutral(self))
                    //    message = "killed itself";
                    //else if (IsFemale(self))
                    //    message = "killed herself";
                    //else
                        message = "killed himself";
            //        break;
            // }
        }

        // Print the Obituary Message if we have one.
        if (message) {
            gi.bprintf(PRINT_MEDIUM, "%s %s.\n", self->client->pers.netname, message);
            // If in deathmatch, decrement our score.
            if ( self->client && deathmatch->value ) {
                self->client->resp.score--;
            }
            // Reset enemy pointer.
            self->enemy = nullptr;
            // Exit.
            return;
        }

        // Assign attacker as our enemy.
        self->enemy = attacker;
        // Messages for whne the attacker is a client:
        if (attacker && attacker->client) {
            switch (meansOfDeath) {
            case MEANS_OF_DEATH_HIT_FIGHTING:
                message = "was fisted by";
                break;
            case MEANS_OF_DEATH_HIT_PISTOL:
                message = "was shot down by";
                message2 = "'s pistol";
                break;
            case MEANS_OF_DEATH_HIT_SMG:
                message = "was shot down by";
                message2 = "'s smg";
            case MEANS_OF_DEATH_HIT_RIFLE:
                message = "was shot down by";
                message2 = "'s rifle";
            case MEANS_OF_DEATH_HIT_SNIPER:
                message = "was shot down by";
                message2 = "'s sniper";
            case MEANS_OF_DEATH_TELEFRAGGED:
                message = "got teleport fragged by";
                message2 = "'s personal space";
                break;
            }
            if (message) {
                const char *netNameSelf = self->client->pers.netname;
                const char *netNameAttacker = attacker->client->pers.netname;
                gi.bprintf(PRINT_MEDIUM, "%s %s %s%s.\n", netNameSelf, message, netNameAttacker, message2 );
                // Make sure to decrement score.
                if ( coop->value || deathmatch->value ) {
                    if ( isFriendlyFire ) {
                        attacker->client->resp.score--;
                    } else {
                        attacker->client->resp.score++;
                    }
                }
                // Exit.
                return;
            }
        }
    }

    // We're unlikely to reach this point, however..
    gi.bprintf( PRINT_MEDIUM, "%s died.\n", self->client->pers.netname );
    if ( coop->value || deathmatch->value ) {
        self->client->resp.score--;
    }
}


void Touch_Item(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf);

void TossClientWeapon(edict_t *self) {
    if (!deathmatch->value)
        return;

    // Need to have actual ammo to toss.
    const gitem_t *item = self->client->pers.weapon;
    if (! self->client->pers.inventory[self->client->ammo_index])
        item = NULL;
    // Can't toss away your fists.
    if (item && (strcmp(item->pickup_name, "Fists") == 0))
        item = NULL;

    //if (item && quad)
    //    spread = 22.5f;
    //else
    float spread = 0.0f;

    if (item) {
        self->client->viewMove.viewAngles[YAW] -= spread;
        edict_t *drop = Drop_Item(self, item);
        self->client->viewMove.viewAngles[YAW] += spread;
        drop->spawnflags = DROPPED_PLAYER_ITEM;
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
    self->client->weaponState.activeSound = 0;

    // Set bbox maxs to PM_BBOX_DUCKED_MAXS.
    self->maxs[2] = PM_BBOX_DUCKED_MAXS.z;

//  self->solid = SOLID_NOT;
    // Flag as to be treated as 'deadmonster' collision.
    self->svflags |= SVF_DEADMONSTER;

    if (!self->deadflag) {
        // Determine respawn time.
		self->client->respawn_time = ( level.time + 1_sec );
        // Make sure the playerstate its pmove knows we're dead.
        self->client->ps.pmove.pm_type = PM_DEAD;
        // Set the look at killer yaw.
        LookAtKiller( self, inflictor, attacker );
        // Notify the obituary.
        ClientObituary(self, inflictor, attacker);
        // Toss away weaponry.
        TossClientWeapon(self);

        // clear inventory
        // this is kind of ugly, but it's how we want to handle keys in coop
        //for (n = 0; n < game.num_items; n++) {
        //    if (coop->value && itemlist[n].flags & IT_KEY)
        //        self->client->resp.pers_respawn.inventory[n] = self->client->pers.inventory[n];
        //    self->client->pers.inventory[n] = 0;
        //}
    }

    // Gib Death:
    if (self->health < -40) {
        // Play gib sound.
        gi.sound(self, CHAN_BODY, gi.soundindex("world/gib01.wav"), 1, ATTN_NORM, 0);
        //! Throw 4 small meat gibs around.
        for ( n = 0; n < 4; n++ ) {
            ThrowGib( self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC );
        }
        // Turn ourself into the thrown head entity.
        ThrowClientHead(self, damage);

        // Gibs don't take damage, but fade away as time passes.
        self->takedamage = DAMAGE_NO;
    // Normal death:
    } else {
        if (!self->deadflag) {
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
            gi.sound( self, CHAN_VOICE, gi.soundindex( va( "player/death0%i.wav", ( irandom(0, 4) ) + 1 ) ), 1, ATTN_NORM, 0 );
        }
    }

    self->deadflag = DEAD_DEAD;

    gi.linkentity(self);
}

//=======================================================================

/*
==============
InitClientPersistantData

This is only called when the game first initializes in single player,
but is called after each death and level change in deathmatch
==============
*/
void InitClientPersistantData( edict_t *ent, gclient_t *client ) {
    // Clear out persistent data.
    client->pers = {};

    // Find the fists item, add it to our inventory.
    const gitem_t *item_fists = FindItem( "Fists" );
    client->pers.selected_item = ITEM_INDEX( item_fists );
    client->pers.inventory[ client->pers.selected_item ] = 1;

    // Find the Pistol item, add it to our inventory and appoint it as the selected weapon.
    const gitem_t *item_pistol = FindItem( "Pistol" );
    client->pers.selected_item = ITEM_INDEX( item_pistol );
    client->pers.inventory[ client->pers.selected_item ] = 1;
    // Assign it as our selected weapon.
    client->pers.weapon = item_pistol;
    // Give it a single full clip of ammo.
    client->pers.weapon_clip_ammo[ client->pers.weapon->weapon_index ] = item_pistol->clip_capacity;
    // And some extra bullets to reload with.
    ent->client->ammo_index = ITEM_INDEX( FindItem( ent->client->pers.weapon->ammo ) );
    client->pers.inventory[ ent->client->ammo_index ] = 78;

    // Obviously we need to allow this.
    client->weaponState.canChangeMode = false;

    client->pers.health = 100;
    client->pers.max_health = 100;

    client->pers.ammoCapacities.pistol = 120;
    client->pers.ammoCapacities.rifle = 180;
    client->pers.ammoCapacities.smg = 250;
    client->pers.ammoCapacities.sniper = 80;
    client->pers.ammoCapacities.shotgun = 100;

    client->pers.connected = true;
    client->pers.spawned = true;
}


void InitClientRespawnData( gclient_t *client ) {
    // Clear out respawn data.
    client->resp = {};

    // Save the moment in time.
    client->resp.enterframe = level.framenum;
    client->resp.entertime = level.time;

    // In case of a coop game mode, we make sure to store the 
    // 'persistent across level changes' data into the client's
    // respawn field, so we can restore it each respawn.
    client->resp.pers_respawn = client->pers;
}

/*
==================
SaveClientData

Some information that should be persistant, like health,
is still stored in the edict structure, so it needs to
be mirrored out to the client structure before all the
edicts are wiped.
==================
*/
void SaveClientData( void ) {
    int     i;
    edict_t *ent;

    for ( i = 0; i < game.maxclients; i++ ) {
        ent = &g_edicts[ 1 + i ];
        if ( !ent->inuse ) {
            continue;
        }
        game.clients[ i ].pers.health = ent->health;
        game.clients[ i ].pers.max_health = ent->max_health;
        game.clients[ i ].pers.savedFlags = static_cast<ent_flags_t>( ent->flags & ( FL_GODMODE | FL_NOTARGET /*| FL_POWER_ARMOR*/ ) );
        if ( coop->value ) {
            game.clients[ i ].pers.score = ent->client->resp.score;
        }
    }
}

void FetchClientEntData( edict_t *ent ) {
    ent->health = ent->client->pers.health;
    ent->max_health = ent->client->pers.max_health;
    ent->flags |= ent->client->pers.savedFlags;
    if ( coop->value )
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
float PlayersRangeFromSpot( edict_t *spot ) {
    edict_t *player;
    float   bestplayerdistance;
    vec3_t  v;
    int     n;
    float   playerdistance;


    bestplayerdistance = 9999999;

    for ( n = 1; n <= maxclients->value; n++ ) {
        player = &g_edicts[ n ];

        if ( !player->inuse )
            continue;

        if ( player->health <= 0 )
            continue;

        VectorSubtract( spot->s.origin, player->s.origin, v );
        playerdistance = VectorLength( v );

        if ( playerdistance < bestplayerdistance )
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
edict_t *SelectRandomDeathmatchSpawnPoint( void ) {
    edict_t *spot, *spot1, *spot2;
    int     count = 0;
    int     selection;
    float   range, range1, range2;

    spot = NULL;
    range1 = range2 = 99999;
    spot1 = spot2 = NULL;

    while ( ( spot = G_Find( spot, FOFS( classname ), "info_player_deathmatch" ) ) != NULL ) {
        count++;
        range = PlayersRangeFromSpot( spot );
        if ( range < range1 ) {
            range1 = range;
            spot1 = spot;
        } else if ( range < range2 ) {
            range2 = range;
            spot2 = spot;
        }
    }

    if ( !count )
        return NULL;

    if ( count <= 2 ) {
        spot1 = spot2 = NULL;
    } else
        count -= 2;

    selection = Q_rand_uniform( count );

    spot = NULL;
    do {
        spot = G_Find( spot, FOFS( classname ), "info_player_deathmatch" );
        if ( spot == spot1 || spot == spot2 )
            selection++;
    } while ( selection-- );

    return spot;
}

/*
================
SelectFarthestDeathmatchSpawnPoint

================
*/
edict_t *SelectFarthestDeathmatchSpawnPoint( void ) {
    edict_t *bestspot;
    float   bestdistance, bestplayerdistance;
    edict_t *spot;


    spot = NULL;
    bestspot = NULL;
    bestdistance = 0;
    while ( ( spot = G_Find( spot, FOFS( classname ), "info_player_deathmatch" ) ) != NULL ) {
        bestplayerdistance = PlayersRangeFromSpot( spot );

        if ( bestplayerdistance > bestdistance ) {
            bestspot = spot;
            bestdistance = bestplayerdistance;
        }
    }

    if ( bestspot ) {
        return bestspot;
    }

    // if there is a player just spawned on each and every start spot
    // we have no choice to turn one into a telefrag meltdown
    spot = G_Find( NULL, FOFS( classname ), "info_player_deathmatch" );

    return spot;
}

edict_t *SelectDeathmatchSpawnPoint( void ) {
    if ( (int)( dmflags->value ) & DF_SPAWN_FARTHEST )
        return SelectFarthestDeathmatchSpawnPoint();
    else
        return SelectRandomDeathmatchSpawnPoint();
}


edict_t *SelectCoopSpawnPoint( edict_t *ent ) {
    int     index;
    edict_t *spot = NULL;
    // WID: C++20: Added const.
    const char *target;

    index = ent->client - game.clients;

    // player 0 starts in normal player spawn point
    if ( !index )
        return NULL;

    spot = NULL;

    // assume there are four coop spots at each spawnpoint
    while ( 1 ) {
        spot = G_Find( spot, FOFS( classname ), "info_player_coop" );
        if ( !spot )
            return NULL;    // we didn't have enough...

        target = spot->targetname;
        if ( !target )
            target = "";
        if ( Q_stricmp( game.spawnpoint, target ) == 0 ) {
            // this is a coop spawn point for one of the clients here
            index--;
            if ( !index )
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
void    SelectSpawnPoint( edict_t *ent, vec3_t origin, vec3_t angles ) {
    edict_t *spot = NULL;

    if ( deathmatch->value )
        spot = SelectDeathmatchSpawnPoint();
    else if ( coop->value )
        spot = SelectCoopSpawnPoint( ent );

    // find a single player start spot
    if ( !spot ) {
        while ( ( spot = G_Find( spot, FOFS( classname ), "info_player_start" ) ) != NULL ) {
            if ( !game.spawnpoint[ 0 ] && !spot->targetname )
                break;

            if ( !game.spawnpoint[ 0 ] || !spot->targetname )
                continue;

            if ( Q_stricmp( game.spawnpoint, spot->targetname ) == 0 )
                break;
        }

        if ( !spot ) {
            if ( !game.spawnpoint[ 0 ] ) {
                // there wasn't a spawnpoint without a target, so use any
                spot = G_Find( spot, FOFS( classname ), "info_player_start" );
            }
            if ( !spot )
                gi.error( "Couldn't find spawn point %s", game.spawnpoint );
        }
    }

    VectorCopy( spot->s.origin, origin );
    VectorCopy( spot->s.angles, angles );
}



/**
* 
* 
*   Body Death & Queue
* 
* 
**/
void InitBodyQue( void ) {
    int     i;
    edict_t *ent;

    level.body_que = 0;
    for ( i = 0; i < BODY_QUEUE_SIZE; i++ ) {
        ent = G_AllocateEdict();
        ent->classname = "bodyque";
    }
}

void body_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point ) {
    int n;

    if ( self->health < -40 ) {
        gi.sound( self, CHAN_BODY, gi.soundindex( "world/gib01.wav" ), 1, ATTN_NORM, 0 );
        for ( n = 0; n < 4; n++ ) {
            ThrowGib( self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC );
        }
        self->s.origin[ 2 ] -= 48;
        ThrowClientHead( self, damage );
        self->takedamage = DAMAGE_NO;
    }
}

void CopyToBodyQue( edict_t *ent ) {
    edict_t *body;

    gi.unlinkentity( ent );

    // grab a body que and cycle to the next one
    body = &g_edicts[ game.maxclients + level.body_que + 1 ];
    level.body_que = ( level.body_que + 1 ) % BODY_QUEUE_SIZE;

    // send an effect on the removed body
    if ( body->s.modelindex ) {
        gi.WriteUint8( svc_temp_entity );
        gi.WriteUint8( TE_BLOOD );
        gi.WritePosition( body->s.origin, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
        gi.WriteDir8( vec3_origin );
        gi.multicast( body->s.origin, MULTICAST_PVS, false );
    }

    gi.unlinkentity( body );

    body->s.number = body - g_edicts;
    VectorCopy( ent->s.origin, body->s.origin );
    VectorCopy( ent->s.origin, body->s.old_origin );
    VectorCopy( ent->s.angles, body->s.angles );
    body->s.modelindex = ent->s.modelindex;
    body->s.frame = ent->s.frame;
    body->s.skinnum = ent->s.skinnum;
    body->s.event = EV_OTHER_TELEPORT;

    body->svflags = ent->svflags;
    VectorCopy( ent->mins, body->mins );
    VectorCopy( ent->maxs, body->maxs );
    VectorCopy( ent->absmin, body->absmin );
    VectorCopy( ent->absmax, body->absmax );
    VectorCopy( ent->size, body->size );
    VectorCopy( ent->velocity, body->velocity );
    VectorCopy( ent->avelocity, body->avelocity );
    body->solid = ent->solid;
    body->clipmask = ent->clipmask;
    body->owner = ent->owner;
    body->movetype = ent->movetype;
    body->groundInfo.entity = ent->groundInfo.entity;

    body->s.entityType = ET_PLAYER_CORPSE;

    body->die = body_die;
    body->takedamage = DAMAGE_YES;

    gi.linkentity( body );
}



/**
*
*
*   Respawns:
*
*
**/
void respawn( edict_t *self ) {
    if ( deathmatch->value || coop->value ) {
        // spectator's don't leave bodies
        if ( self->movetype != MOVETYPE_NOCLIP )
            CopyToBodyQue( self );
        self->svflags &= ~SVF_NOCLIENT;
        PutClientInServer( self );

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

/*
 * only called when pers.spectator changes
 * note that resp.spectator should be the opposite of pers.spectator here
 */
void spectator_respawn( edict_t *ent ) {
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
    PutClientInServer( ent );

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
*   Client Utilities:
*
*
**/
/**
*   @brief  Will reset the entity client's 'Field of View' back to its defaults.
**/
void P_ResetPlayerStateFOV( gclient_t *client ) {
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
*   @brief  Determine the impacting falling damage to take. Called directly by ClientThink after each PMove.
**/
void P_FallingDamage( edict_t *ent, const pmove_t &pm ) {
    int	   damage;
    vec3_t dir;

    // Dead stuff can't crater.
    if ( ent->health <= 0 || ent->deadflag ) {
        return;
    }

    if ( ent->s.modelindex != MODELINDEX_PLAYER ) {
        return; // not in the player model
    }

    if ( ent->movetype == MOVETYPE_NOCLIP ) {
        return;
    }

    // Never take falling damage if completely underwater.
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

    if ( delta < 1 ) {
        return;
    }

    // WID: restart footstep timer <- NO MORE!! Because doing so causes the weapon bob 
    // position to insta shift back to start.
    //ent->client->ps.bobCycle = 0;

    //if ( ent->client->landmark_free_fall ) {
    //    delta = min( 30.f, delta );
    //    ent->client->landmark_free_fall = false;
    //    ent->client->landmark_noise_time = level.time + 100_ms;
    //}

    if ( delta < 15 ) {
        if ( !( pm.playerState->pmove.pm_flags & PMF_ON_LADDER ) ) {
            ent->s.event = EV_FOOTSTEP;
            //gi.dprintf( "%s: delta < 15 footstep\n", __func__ );
        }
        return;
    }

    ent->client->viewMove.fallValue = delta * 0.5f;
    if ( ent->client->viewMove.fallValue > 40 ) {
        ent->client->viewMove.fallValue = 40;
    }
    ent->client->viewMove.fallTime = level.time + FALL_TIME();

    if ( delta > 30 ) {
        if ( delta >= 55 ) {
            ent->s.event = EV_FALLFAR;
        } else {
            ent->s.event = EV_FALL;
        }

        // WID: We DO want the VOICE channel to SHOUT in PAIN
        //ent->pain_debounce_time = level.time + FRAME_TIME_S; // No normal pain sound.

        damage = (int)( ( delta - 30 ) / 2 );
        if ( damage < 1 ) {
            damage = 1;
        }
        VectorSet( dir, 0.f, 0.f, 1.f );// dir = { 0, 0, 1 };

        if ( !deathmatch->integer ) {
            T_Damage( ent, world, world, dir, ent->s.origin, vec3_origin, damage, 0, DAMAGE_NONE, MEANS_OF_DEATH_FALLING );
        }
    } else {
        ent->s.event = EV_FALLSHORT;
    }

    // Paril: falling damage noises alert monsters
    if ( ent->health ) {
        P_PlayerNoise( ent, &pm.playerState->pmove.origin[ 0 ], PNOISE_SELF );
    }
}



/**
*
*
*   Client PMove Clip/Trace/PointContents:
*
*
**/
/**
*   @brief  Player Move specific 'Trace' wrapper implementation.
**/
static const trace_t q_gameabi SV_PM_Trace( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, const void *passEntity, const contents_t contentMask ) {
    //if (pm_passent->health > 0)
    //    return gi.trace(start, mins, maxs, end, pm_passent, MASK_PLAYERSOLID);
    //else
    //    return gi.trace(start, mins, maxs, end, pm_passent, MASK_DEADSOLID);
    return gi.trace( start, mins, maxs, end, (edict_t *)passEntity, contentMask );
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



/**
*
*
*   Client Callbacks:
*
*
**/
/**
*   @brief  Called either when a player connects to a server, OR respawns in a multiplayer game.
**/
void PutClientInServer( edict_t *ent ) {
    Vector3 mins = PM_BBOX_STANDUP_MINS;
    Vector3 maxs = PM_BBOX_STANDUP_MAXS;
    int     index;
    vec3_t  spawn_origin, spawn_angles;
    gclient_t *client;
    int     i;
    client_persistant_t saved;
    client_respawn_t    resp;
    vec3_t temp, temp2;
    trace_t tr;

    // find a spawn point
    // do it before setting health back up, so farthest
    // ranging doesn't count this client
    SelectSpawnPoint( ent, spawn_origin, spawn_angles );

    index = ent - g_edicts - 1;
    client = ent->client;

    if ( SG_IsMultiplayerGameMode( game.gamemode ) ) {
        // Store userinfo.
        char        userinfo[ MAX_INFO_STRING ];
        memcpy( userinfo, client->pers.userinfo, sizeof( userinfo ) );
        // Store respawn data.
        resp = client->resp;

        // DeathMatch: Reinitialize a fresh persistent data.
        if ( game.gamemode == GAMEMODE_TYPE_DEATHMATCH ) {
            InitClientPersistantData( ent, client );
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
            client->pers = resp.pers_respawn;
            // Restore score.
            if ( resp.score > client->pers.score ) {
                client->pers.score = resp.score;
            }
        }
        // Administer the user info change.
        ClientUserinfoChanged( ent, userinfo );
    } else {
        //      int         n;
        char        userinfo[ MAX_INFO_STRING ];
        memcpy( userinfo, client->pers.userinfo, sizeof( userinfo ) );
        // Restore userinfo.
        ClientUserinfoChanged( ent, userinfo );
        // Acquire respawn data.
        resp = client->resp;
        // Restore client persistent data.
        client->pers = resp.pers_respawn;
        // Fresh SP mode has 0 score.
        client->pers.score = 0;
    }


    // clear everything but the persistant data
    saved = client->pers;
    memset( client, 0, sizeof( *client ) );
    client->pers = saved;
    // If dead at the time of the previous map switching to the current, reinitialize persistent data.
    if ( client->pers.health <= 0 ) {
        InitClientPersistantData( ent, client );
    }
    client->resp = resp;

    // copy some data from the client to the entity
    FetchClientEntData( ent );

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
    ent->deadflag = DEAD_NO;
    ent->air_finished_time = level.time + 12_sec;
    ent->clipmask = static_cast<contents_t>( MASK_PLAYERSOLID );
    ent->model = "players/playerdummy/tris.iqm";
    ent->pain = player_pain;
    ent->die = player_die;
    ent->liquidInfo.level = liquid_level_t::LIQUID_NONE;
    ent->liquidInfo.type = CONTENTS_NONE;
    ent->flags = static_cast<ent_flags_t>( ent->flags & ~FL_NO_KNOCKBACK );

    ent->svflags &= ~SVF_DEADMONSTER;
    ent->svflags &= ~FL_NO_KNOCKBACK;
    ent->svflags |= SVF_PLAYER;
    // Player Entity Type:
    ent->s.entityType = ET_PLAYER;

    VectorCopy( mins, ent->mins );
    VectorCopy( maxs, ent->maxs );
    VectorClear( ent->velocity );

    // Clear playerstate values.
    memset( &ent->client->ps, 0, sizeof( client->ps ) );
    // Reset the Field of View for the player state.
    P_ResetPlayerStateFOV( ent->client );


    // Set viewheight for player state pmove.
    ent->client->ps.pmove.viewheight = ent->viewheight;

    // Proper gunindex.
    if ( client->pers.weapon ) {
        client->ps.gun.modelIndex = gi.modelindex( client->pers.weapon->view_model );
    } else {
        client->ps.gun.modelIndex = 0;
        client->ps.gun.animationID = 0;
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
    VectorCopy( spawn_origin, temp );
    VectorCopy( spawn_origin, temp2 );
    temp[ 2 ] -= 64;
    temp2[ 2 ] += 16;
    tr = gi.trace( temp2, ent->mins, ent->maxs, temp, ent, static_cast<contents_t>( MASK_PLAYERSOLID ) );
    if ( !tr.allsolid && !tr.startsolid && Q_stricmp( level.mapname, "tech5" ) ) {
        VectorCopy( tr.endpos, ent->s.origin );
        ent->groundInfo.entity = tr.ent;
    } else {
        VectorCopy( spawn_origin, ent->s.origin );
        ent->s.origin[ 2 ] += 10; // make sure off ground
    }

    VectorCopy( ent->s.origin, ent->s.old_origin );

    for ( i = 0; i < 3; i++ ) {
        client->ps.pmove.origin[ i ] = ent->s.origin[ i ]; // COORD2SHORT(ent->s.origin[i]); // WID: float-movement
    }

    spawn_angles[ PITCH ] = 0;
    spawn_angles[ ROLL ] = 0;

    // set the delta angle
    for ( i = 0; i < 3; i++ ) {
        client->ps.pmove.delta_angles[ i ] = /*ANGLE2SHORT*/AngleMod( ( spawn_angles[ i ] - client->resp.cmd_angles[ i ] ) );
    }

    VectorCopy( spawn_angles, ent->s.angles );
    VectorCopy( spawn_angles, client->ps.viewangles );
    VectorCopy( spawn_angles, client->viewMove.viewAngles );
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
    } else
        client->resp.spectator = false;

    if ( !KillBox( ent, true ) ) {
        // could't spawn in?
    }

    gi.linkentity( ent );

    // force the current weapon up
    client->newweapon = client->pers.weapon;
    P_Weapon_Change( ent );
}

/**
*   @brief Deathmatch mode implementation of ClientBegin.
*
*           A client has just connected to the server in deathmatch mode, so clear everything 
*           out before starting them.
**/
void ClientBeginDeathmatch( edict_t *ent ) {
    // Init Edict.
    G_InitEdict( ent );

    // Make sure it is recognized as a player.
    ent->svflags |= SVF_PLAYER;
    ent->s.entityType = ET_PLAYER;

    InitClientRespawnData( ent->client );

    // locate ent at a spawn point
    PutClientInServer( ent );

    if ( level.intermission_framenum ) {
        MoveClientToIntermission( ent );
    } else {
        // send effect
        gi.WriteUint8( svc_muzzleflash );
        gi.WriteInt16( ent - g_edicts );
        gi.WriteUint8( MZ_LOGIN );
        gi.multicast( ent->s.origin, MULTICAST_PVS, false );
    }

    gi.bprintf( PRINT_HIGH, "%s entered the game\n", ent->client->pers.netname );

    // make sure all view stuff is valid
    ClientEndServerFrame( ent );
}

/**
*   @brief  Called when a client has finished connecting, and is ready
*           to be placed into the game. This will happen every level load.
**/
void ClientBegin( edict_t *ent ) {
    int     i;

    ent->client = game.clients + ( ent - g_edicts - 1 );

    // [Paril-KEX] we're always connected by this point...
    ent->client->pers.connected = true;

    if ( deathmatch->value ) {
        ClientBeginDeathmatch( ent );
        return;
    }

    ent->client->pers.spawned = true;

    // if there is already a body waiting for us (a loadgame), just
    // take it, otherwise spawn one from scratch
    if ( ent->inuse == (qboolean)true ) {
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
        G_InitEdict( ent );
        ent->classname = "player";
        ent->s.entityType = ET_PLAYER;
        InitClientRespawnData( ent->client );
        PutClientInServer( ent );
    }

    ent->svflags |= SVF_PLAYER;

    if ( level.intermission_framenum ) {
        MoveClientToIntermission( ent );
    } else {
        // Send effect even if NOT in a multiplayer game
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

    // make sure all view stuff is valid
    ClientEndServerFrame( ent );
}

/**
*   @brief  called whenever the player updates a userinfo variable.
*
*           The game can override any of the settings in place
*           (forcing skins or names, etc) before copying it off.
**/
void ClientUserinfoChanged( edict_t *ent, char *userinfo ) {
    char    *s;
    int     playernum;

    // check for malformed or illegal info strings
    if (!Info_Validate(userinfo)) {
        strcpy(userinfo, "\\name\\badinfo\\skin\\testdummy/skin");
    }

    // set name
    s = Info_ValueForKey(userinfo, "name");
    Q_strlcpy(ent->client->pers.netname, s, sizeof(ent->client->pers.netname));

    // set spectator
    s = Info_ValueForKey(userinfo, "spectator");
    // spectators are only supported in deathmatch
    if ( deathmatch->value && *s && strcmp( s, "0" ) ) {
        ent->client->pers.spectator = true;
    } else {
        ent->client->pers.spectator = false;
    }

    // set skin
    s = Info_ValueForKey(userinfo, "skin");

    playernum = ent - g_edicts - 1;

    // combine name and skin into a configstring
    gi.configstring(CS_PLAYERSKINS + playernum, va("%s\\%s", ent->client->pers.netname, s));

    // fov
    #if 1
    P_ResetPlayerStateFOV( ent->client );
    #else
        if (deathmatch->value && ((int)dmflags->value & DF_FIXED_FOV)) {
            ent->client->ps.fov = 90;
        } else {
            ent->client->ps.fov = atoi(Info_ValueForKey(userinfo, "fov"));
            if (ent->client->ps.fov < 1)
                ent->client->ps.fov = 90;
            else if (ent->client->ps.fov > 160)
                ent->client->ps.fov = 160;
        }
    #endif

    // handedness
    s = Info_ValueForKey(userinfo, "hand");
    if (strlen(s)) {
        ent->client->pers.hand = atoi(s);
    }

    // save off the userinfo in case we want to check something later
    Q_strlcpy(ent->client->pers.userinfo, userinfo, sizeof(ent->client->pers.userinfo));
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
qboolean ClientConnect( edict_t *ent, char *userinfo ) {
    // check to see if they are on the banned IP list
    char *value = Info_ValueForKey( userinfo, "ip" );
    if ( SV_FilterPacket( value ) ) {
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
    ent->client = game.clients + (ent - g_edicts - 1);

    // if there is already a body waiting for us (a loadgame), just
    // take it, otherwise spawn one from scratch
    if ( ent->inuse == false ) {
        // clear the respawning variables
        InitClientRespawnData( ent->client );
        if ( !game.autosaved || !ent->client->pers.weapon ) {
            InitClientPersistantData( ent, ent->client );
        }
    }

    // make sure we start with known default(s)
    //ent->svflags = SVF_PLAYER;
    ClientUserinfoChanged(ent, userinfo);

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
void ClientDisconnect(edict_t *ent)
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

/**
*   @brief  This will be called once for each client frame, which will usually 
*           be a couple times for each server frame.
**/
void ClientThink(edict_t *ent, usercmd_t *ucmd) {
    // Set the entity that is being processed.
    level.current_entity = ent;

    // Warn in case if it is not a client.
    if ( !ent) {
        gi.bprintf( PRINT_WARNING, "%s: ent == nullptr\n", __func__ );
        return;
    }

    // Acquire its client pointer.
    gclient_t *client = ent->client;
    // Warn in case if it is not a client.
    if ( !client ) {
        gi.bprintf( PRINT_WARNING, "%s: ent->client == nullptr\n", __func__ );
        return;
    }

    // Backup the old player state.
    client->ops = client->ps;

	// Configure pmove.
    pmove_t pm = {};
    pmoveParams_t pmp = {};
	SG_ConfigurePlayerMoveParameters( &pmp );

    // [Paril-KEX] pass buttons through even if we are in intermission or
    // chasing.
    client->oldbuttons = client->buttons;
    client->buttons = ucmd->buttons;
    client->latched_buttons |= client->buttons & ~client->oldbuttons;

    /**
    *   Level Intermission Path:
    **/
    if ( level.intermission_framenum ) {
        client->ps.pmove.pm_type = PM_FREEZE;

        // can exit intermission after five seconds
        if ( ( level.framenum > level.intermission_framenum + 5.0f * BASE_FRAMERATE ) && ( ucmd->buttons & BUTTON_ANY ) ) {
            level.exitintermission = true;
        }

        // WID: Also seems set in p_hud.cpp -> MoveClientToIntermission
        client->ps.pmove.viewheight = ent->viewheight = PM_VIEWHEIGHT_STANDUP;

        return;
    }

    /**
    *   Chase Target:
    **/
    if ( ent->client->chase_target ) {
        VectorCopy( ucmd->angles, client->resp.cmd_angles );
    /**
    *   Player Move, and other 'Thinking' logic:
    **/
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

        // Ensure the entity has proper RF_STAIR_STEP applied to it when moving up/down those:
        if ( pm.ground.entity && ent->groundInfo.entity ) {
            const float stepsize = fabs( ent->s.origin[ 2 ] - pm.playerState->pmove.origin[ 2 ] );
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
        // [Paril-KEX] save old position for G_TouchProjectiles
        //vec3_t old_origin = ent->s.origin;
        
        // [Paril-KEX] if we stepped onto/off of a ladder, reset the last ladder pos
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
        
        // [Paril-KEX] save old position for G_TouchProjectiles
        const Vector3 old_origin = ent->s.origin;

		// Copy back into the entity, both the resulting origin and velocity.
		VectorCopy( pm.playerState->pmove.origin, ent->s.origin );
		VectorCopy( pm.playerState->pmove.velocity, ent->velocity );
        // Copy back in bounding box results. (Player might've crouched for example.)
        VectorCopy( pm.mins, ent->mins );
        VectorCopy( pm.maxs, ent->maxs );

        // Play 'Jump' sound if pmove inquired so.
        if ( pm.jump_sound && !( pm.playerState->pmove.pm_flags & PMF_ON_LADDER ) ) { //if (~client->ps.pmove.pm_flags & pm.s.pm_flags & PMF_JUMP_HELD && pm.liquid.level == 0) {
            // Jump sound to play.
            const int32_t sndIndex = irandom( 2 ) + 1;
            std::string pathJumpSnd = "player/jump0"; pathJumpSnd += std::to_string( sndIndex ); pathJumpSnd += ".wav";
            gi.sound( ent, CHAN_VOICE, gi.soundindex( pathJumpSnd.c_str() ), 1, ATTN_NORM, 0 );
            // Jump sound to play.
            //gi.sound( ent, CHAN_VOICE, gi.soundindex( "player/jump01.wav" ), 1, ATTN_NORM, 0 );
            
            // Paril: removed to make ambushes more effective and to
            // not have monsters around corners come to jumps
            // PlayerNoise(ent, ent->s.origin, PNOISE_SELF);
        }

		// Update the entity's remaining viewheight, liquid and ground information:
        ent->viewheight = (int32_t)pm.playerState->pmove.viewheight;
        ent->liquidInfo.level = pm.liquid.level;
        ent->liquidInfo.type = pm.liquid.type;
        ent->groundInfo.entity = pm.ground.entity;
        if ( pm.ground.entity ) {
            ent->groundInfo.entityLinkCount = pm.ground.entity->linkcount;
        }
        // Apply a specific view angle if dead:
        if ( ent->deadflag ) {
            client->ps.viewangles[ROLL] = 40;
            client->ps.viewangles[PITCH] = -15;
            client->ps.viewangles[YAW] = client->killer_yaw;
        // Otherwise, apply the player move state view angles:
        } else {
            VectorCopy( pm.playerState->viewangles, client->ps.viewangles );
            VectorCopy( client->ps.viewangles, client->viewMove.viewAngles );
            QM_AngleVectors( client->viewMove.viewAngles, &client->viewMove.viewForward, nullptr, nullptr );
        }

        // Finally link the entity back in.
        gi.linkentity( ent );

        // PGM trigger_gravity support
        ent->gravity = 1.0;
        // PGM

        // If we're not 'No-Clipping', or 'Spectating', touch triggers and projectfiles.
		if ( ent->movetype != MOVETYPE_NOCLIP ) {
			G_TouchTriggers( ent );
            G_TouchProjectiles( ent, old_origin );
		}

        // Dispatch touch callbacks on all the remaining 'Solid' traced objects during our PMove.
        for ( int32_t i = 0; i < pm.touchTraces.numberOfTraces; i++ ) {
            trace_t &tr = pm.touchTraces.traces[ i ];
            edict_t *other = tr.ent;

            if ( other != nullptr && other->touch ) {
                // TODO: Q2RE has these for last 2 args: const trace_t &tr, bool other_touching_self
                // What the??
                other->touch( other, ent, &tr.plane, tr.surface );
            }
        }

    }

    /**
    *   Spectator Path:
    **/
    if ( client->resp.spectator ) {
        if ( client->latched_buttons & BUTTON_PRIMARY_FIRE ) {
            // Zero out latched buttons.
            client->latched_buttons = BUTTON_NONE;

            // Switch from chase target to no chase target:
            if ( client->chase_target ) {
                client->chase_target = nullptr;
                client->ps.pmove.pm_flags &= ~( PMF_NO_POSITIONAL_PREDICTION | PMF_NO_ANGULAR_PREDICTION );
                // Otherwise, get an active chase target:
            } else {
                GetChaseTarget( ent );
            }
        }

    /**
    *   Regular (Weapon-)Path:
    **/
    } else {
        // Check whether to engage switching to a new weapon.
        if ( client->newweapon ) {
            P_Weapon_Change( ent );
        }

        // Process weapon thinking.
        //if ( ent->client->weapon_thunk == false ) {
            P_Weapon_Think( ent, true );
            // Store that we thought for this frame.
            ent->client->weapon_thunk = true;
        //}

    // Determine any possibly necessary player state events to go along.

    }

    // Finally process the actual player_state animations.
    SVG_P_ProcessAnimations( ent );

    /**
    *   Spectator/Chase-Cam specific behaviors:
    **/
    if ( client->resp.spectator ) {
        // Switch to next chase target (or the first in-line if not chasing any).
        if ( ucmd->upmove >= 10 ) {
            if ( !( client->ps.pmove.pm_flags & PMF_JUMP_HELD ) ) {
                client->ps.pmove.pm_flags |= PMF_JUMP_HELD;

				if ( client->chase_target ) {
					ChaseNext( ent );
				} else {
					GetChaseTarget( ent );
				}
            }
        // Untoggle button state.
		} else {
            client->ps.pmove.pm_flags &= ~PMF_JUMP_HELD;
		}
    }

    // Update chase cam if being followed.
    for ( int32_t i = 1; i <= maxclients->value; i++ ) {
        edict_t *other = g_edicts + i;
        if ( other->inuse && other->client->chase_target == ent ) {
			UpdateChaseCam( other );
		}
    }


}

/**
*   @brief  This will be called once for each server frame, before running any other entities in the world.
**/
void ClientBeginServerFrame(edict_t *ent)
{
    gclient_t   *client;
    int         buttonMask;

    /**
    *   Remove RF_STAIR_STEP if we're in a new frame, not stepping.
    **/
    if ( gi.GetServerFrameNumber() != ent->client->last_stair_step_frame ) {
        ent->s.renderfx &= ~RF_STAIR_STEP;
    }

    /**
    *   Opt out of function if we're in intermission mode.
    **/
    if ( level.intermission_framenum ) {
        return;
    }

    /**
    *   Handle respawning as a spectating client:
    **/
    client = ent->client;

    if ( deathmatch->value && client->pers.spectator != client->resp.spectator &&
        ( level.time - client->respawn_time ) >= 5_sec ) {
        spectator_respawn( ent );
        return;
    }

    /**
    *   Run weapon logic if it hasn't been done by a usercmd_t in ClientThink.
    **/
    if ( client->weapon_thunk == false && !client->resp.spectator ) {
        P_Weapon_Think( ent, false );
    } else {
        client->weapon_thunk = false;
    }

    /**
    *   If dead, check for any user input after the client's respawn_time has expired.
    **/
    if ( ent->deadflag ) {
        // wait for any button just going down
        if ( level.time > client->respawn_time ) {
            // in deathmatch, only wait for attack button
            if ( deathmatch->value ) {
                buttonMask = BUTTON_PRIMARY_FIRE;
            } else {
                buttonMask = -1;
            }

            if ( ( client->latched_buttons & buttonMask ) ||
                ( deathmatch->value && ( (int)dmflags->value & DF_FORCE_RESPAWN ) ) ) {
                respawn( ent );
                client->latched_buttons = BUTTON_NONE;
            }
        }
        return;
    }

    /**
    *   Add player trail so monsters can follow
    **/
    if ( !deathmatch->value ) {
        // WID: TODO: Monster Reimplement.
        //if ( !visible( ent, PlayerTrail_LastSpot() ) ) {
            PlayerTrail_Add( ent->s.old_origin );
        //}
    }

    /**
    *   UNLATCH ALL LATCHED BUTTONS:
    **/
    client->latched_buttons = BUTTON_NONE;
}
