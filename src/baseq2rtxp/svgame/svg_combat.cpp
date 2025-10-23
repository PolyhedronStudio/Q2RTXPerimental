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
// g_combat.c

#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"
#include "svgame/entities/monster/svg_monster_testdummy.h"

#include "sharedgame/sg_means_of_death.h"
#include "sharedgame/sg_tempentity_events.h"


#include "sharedgame/sg_gamemode.h"
#include "svgame/svg_gamemode.h"
#include "svgame/gamemodes/svg_gm_basemode.h"

#define WARN_ON_TRIGGERDAMAGE_NO_PAIN_CALLBACK


/**
*   @brief
**/
static char *ClientTeam( svg_base_edict_t *ent ) {
    char *p;
    static char value[ 512 ];

    value[ 0 ] = 0;

    if ( !ent->client )
        return value;

    strcpy( value, Info_ValueForKey( ent->client->pers.userinfo, "skin" ) );
    p = strchr( value, '/' );
    if ( !p )
        return value;

    if ( (int)( dmflags->value ) & DF_MODELTEAMS ) {
        *p = 0;
        return value;
    }

    // if ((int)(dmflags->value) & DF_SKINTEAMS)
    return ++p;
}

/**
*   @brief
**/
const bool SVG_OnSameTeam( svg_base_edict_t *ent1, svg_base_edict_t *ent2 ) {
    char    ent1Team[ 512 ];
    char    ent2Team[ 512 ];

    if ( !( (int)( dmflags->value ) & ( DF_MODELTEAMS | DF_SKINTEAMS ) ) ) {
        return false;
    }

    strcpy( ent1Team, ClientTeam( ent1 ) );
    strcpy( ent2Team, ClientTeam( ent2 ) );

    if ( strcmp( ent1Team, ent2Team ) == 0 )
        return true;
    return false;
}


/*
============
Killed
============
*/
static void Killed(svg_base_edict_t *targ, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int32_t damage, Vector3 *point)
{
    if (targ->health < -999)
        targ->health = -999;

    targ->enemy = attacker;

    if ((targ->svflags & SVF_MONSTER) && (targ->lifeStatus != LIFESTATUS_DEAD)) {
            //targ->svflags |= SVF_DEADENTITY;   // now treat as a different content type
            // WID: TODO: Monster Reimplement.        
            //if (!(targ->monsterinfo.aiflags & AI_GOOD_GUY)) {
            //level.killed_monsters++;
            if (coop->value && attacker->client)
                attacker->client->resp.score++;
            // medics won't heal monsters that they kill themselves
            if (strcmp( (const char *)attacker->classname, "monster_medic") == 0)
                targ->owner = attacker;
        //}
    }

    if (targ->movetype == MOVETYPE_PUSH || targ->movetype == MOVETYPE_STOP || targ->movetype == MOVETYPE_NONE) {
        // doors, triggers, etc
        if ( targ->HasDieCallback() ) {
            targ->DispatchDieCallback( inflictor, attacker, damage, point );
        }
        return;
    }

    if ((targ->svflags & SVF_MONSTER) && (targ->lifeStatus != LIFESTATUS_DEAD)) {
        targ->SetTouchCallback( nullptr );//targ->touch = NULL;
        // WID: TODO: Actually trigger death.
        //monster_death_use(targ);
        //When a monster dies, it fires all of its targets with the current
        //enemy as activator.
        //void monster_death_use( svg_base_edict_t * self ) {
        //    self->flags &= ~( FL_FLY | FL_SWIM );
        //    self->monsterinfo.aiflags &= ( AI_HIGH_TICK_RATE | AI_GOOD_GUY );

        //    if ( self->item ) {
        //        Drop_Item( self, self->item );
        //        self->item = NULL;
        //    }

        //    if ( self->targetNames.death )
        //        self->targetNames.target = self->targetNames.death;

        //    if ( !self->targetNames.target )
        //        return;

        //    SVG_UseTargets( self, self->enemy );
        //}
    }
    if ( targ->HasDieCallback() ) {
        targ->DispatchDieCallback( inflictor, attacker, damage, point );
    }
}


/*
================
SVG_SpawnDamage
================
*/
void SVG_SpawnDamage(const int32_t type, const Vector3 &origin, const Vector3 &normal, const int32_t damage) {
    const int32_t cappedDamage = damage > 255 ? 255 : damage;

    gi.WriteUint8( svc_temp_entity );
    gi.WriteUint8( type );
//  gi.WriteByte ( cappedDamage );
    gi.WritePosition( &origin, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
    gi.WriteDir8( &normal );
    gi.multicast( &origin, MULTICAST_PVS, false );
}


/*
============
SVG_DamageEntity

targ        entity that is being damaged
inflictor   entity that is causing the damage
attacker    entity that caused the inflictor to damage targ
    example: targ=monster, inflictor=rocket, attacker=player

dir         direction of the attack
point       point at which the damage is being inflicted
normal      normal vector from that point
damage      amount of damage being inflicted
knockback   force to be applied against targ as a result of the damage

dflags      these flags are used to control how SVG_DamageEntity works
    DAMAGE_RADIUS           damage was indirect (from a nearby explosion)
    DAMAGE_NO_ARMOR         armor does not protect from this damage
    DAMAGE_ENERGY           damage is from an energy based weapon
    DAMAGE_NO_KNOCKBACK     do not affect velocity, just view angles
    DAMAGE_BULLET           damage is from a bullet (used for ricochets)
    DAMAGE_NO_PROTECTION    kills godmode, armor, everything
============
*/
void M_ReactToDamage(svg_base_edict_t *targ, svg_base_edict_t *attacker)
{
    if (!(attacker->client) && !(attacker->svflags & SVF_MONSTER))
        return;

    if (attacker == targ || attacker == targ->enemy)
        return;

    // dead monsters, like misc_deadsoldier, don't have AI functions, but 
    // M_ReactToDamage might still be called on them
    if (targ->svflags & SVF_DEADENTITY)
        return;

    // if we are a good guy monster and our attacker is a player
    // or another good guy, do not get mad at them
    // WID: TODO: Monster Reimplement.
    //if (targ->monsterinfo.aiflags & AI_GOOD_GUY) {
    //    if (attacker->client || (attacker->monsterinfo.aiflags & AI_GOOD_GUY))
    //        return;
    //}

    // we now know that we are not both good guys

    // if attacker is a client, get mad at them because he's good and we're not
    if (attacker->client) {
        // WID: TODO: Monster Reimplement.
        //targ->monsterinfo.aiflags &= ~AI_SOUND_TARGET;

        // this can only happen in coop (both new and old enemies are clients)
        // only switch if can't see the current enemy
        // WID: TODO: Monster Reimplement.
        if (targ->enemy && targ->enemy->client) {
            //if (visible(targ, targ->enemy)) {
            //    targ->oldenemy = attacker;
            //    return;
            //}
            targ->oldenemy = targ->enemy;
        }
        targ->enemy = attacker;
        // WID: TODO: Monster Reimplement
        //if (!(targ->monsterinfo.aiflags & AI_DUCKED))
        //    FoundTarget(targ);
        return;
    }

    // it's the same base (walk/swim/fly) type and a different classname and it's not a tank
    // (they spray too much), get mad at them
    if ( ( ( targ->flags & (FL_FLY | FL_SWIM) ) == ( attacker->flags & (FL_FLY | FL_SWIM) ) ) &&
        ( strcmp( (const char *)targ->classname, (const char*)attacker->classname ) != 0) ) {
        if ( targ->enemy && targ->enemy->client ) {
            targ->oldenemy = targ->enemy;
        }
        targ->enemy = attacker;
        // WID: TODO: Monster Reimplement
        //if (!(targ->monsterinfo.aiflags & AI_DUCKED))
        //    FoundTarget(targ);
    }
    // if they *meant* to shoot us, then shoot back
    else if ( attacker->enemy == targ ) {
        if ( targ->enemy && targ->enemy->client ) {
            targ->oldenemy = targ->enemy;
        }
        targ->enemy = attacker;
        // WID: TODO: Monster Reimplement
        //if (!(targ->monsterinfo.aiflags & AI_DUCKED))
        //    FoundTarget(targ);
    }
    // otherwise get mad at whoever they are mad at (help our buddy) unless it is us!
    else if ( attacker->enemy && attacker->enemy != targ ) {
        if ( targ->enemy && targ->enemy->client ) {
            targ->oldenemy = targ->enemy;
        }
        targ->enemy = attacker->enemy;
        // WID: TODO: Monster Reimplement
        //if (!(targ->monsterinfo.aiflags & AI_DUCKED))
        //    FoundTarget(targ);
    }
}

/**
*   @brief  Check whether we're allowed to damage a teammate.
**/
const bool SVG_CheckTeamDamage(svg_base_edict_t *targ, svg_base_edict_t *attacker)
{
    //FIXME make the next line real and uncomment this block
    // if ((ability to damage a teammate == OFF) && (targ's team == attacker's team))
    return false;
}

/**
*   @brief  This function is used to apply damage to an entity.
*           It handles the damage calculation, knockback, and any special
*           effects based on the type of damage and the entities involved.
**/
void SVG_DamageEntity( svg_base_edict_t *targ, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, const Vector3 &dir, Vector3 &point, const Vector3 &normal, const int32_t damage, const int32_t knockBack, const entity_damageflags_t damageFlags, const sg_means_of_death_t meansOfDeath ) {
    // Let game mode decide.
    game.mode->DamageEntity( targ, inflictor, attacker, dir, point, normal, damage, knockBack, damageFlags, meansOfDeath );
}


/*
============
SVG_RadiusDamage
============
*/
void SVG_RadiusDamage( svg_base_edict_t *inflictor, svg_base_edict_t *attacker, float damage, svg_base_edict_t *ignore, float radius, const sg_means_of_death_t meansOfDeath ) {
    float   points;
    svg_base_edict_t *ent = NULL;
    vec3_t  v;
    vec3_t  dir;

    while ( ( ent = SVG_Entities_FindWithinRadius( ent, inflictor->s.origin, radius ) ) != nullptr ) {
		// Ensure that the entity is valid for damage processing.
        if ( ent == ignore ) {
            continue;
        }
        if ( ent->takedamage == DAMAGE_NO ) {
            continue;
        }
		// Get the distance from the inflictor to the entity's center.
        VectorAdd( ent->mins, ent->maxs, v );
        VectorMA( ent->s.origin, 0.5f, v, v );
        VectorSubtract( inflictor->s.origin, v, v );
		// Calculate the amount of damage points to apply based on the distance.
        points = damage - 0.5f * VectorLength( v );
		// Half damage if the attacker is the same as the entity.
        if ( ent == attacker ) {
            points = points * 0.5f;
        }
		// Only apply damage if the point count is greater than 0.
        if ( points > 0 ) {
			// Make sure that the entity can be damaged.
            if ( game.mode->CanDamageEntityDirectly( ent, inflictor ) ) {
				// Determe the direction vector of the damage.
                VectorSubtract( ent->s.origin, inflictor->s.origin, dir );
				// Apply the damage to the entity.
                SVG_DamageEntity( ent, inflictor, attacker, dir, inflictor->s.origin, vec3_origin, (int)points, (int)points, DAMAGE_RADIUS, meansOfDeath );
            }
        }
    }
}


