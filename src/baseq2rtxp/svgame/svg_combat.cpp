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

#define WARN_ON_TRIGGERDAMAGE_NO_PAIN_CALLBACK


/**
*   @brief
**/
static char *ClientTeam( edict_t *ent ) {
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
const bool SVG_OnSameTeam( edict_t *ent1, edict_t *ent2 ) {
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
SVG_CanDamage

Returns true if the inflictor can directly damage the target.  Used for
explosions and melee attacks.
============
*/
const bool SVG_CanDamage(edict_t *targ, edict_t *inflictor)
{
    vec3_t  dest;
    trace_t trace;

// bmodels need special checking because their origin is 0,0,0
    if (targ->movetype == MOVETYPE_PUSH) {
        VectorAdd(targ->absmin, targ->absmax, dest);
        VectorScale(dest, 0.5f, dest);
        trace = gi.trace(inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
        if (trace.fraction == 1.0f)
            return true;
        if (trace.ent == targ)
            return true;
        return false;
    }

    trace = gi.trace(inflictor->s.origin, vec3_origin, vec3_origin, targ->s.origin, inflictor, MASK_SOLID);
    if (trace.fraction == 1.0f)
        return true;

    VectorCopy(targ->s.origin, dest);
    dest[0] += 15.0f;
    dest[1] += 15.0f;
    trace = gi.trace(inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
    if (trace.fraction == 1.0f)
        return true;

    VectorCopy(targ->s.origin, dest);
    dest[0] += 15.0f;
    dest[1] -= 15.0f;
    trace = gi.trace(inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
    if (trace.fraction == 1.0f)
        return true;

    VectorCopy(targ->s.origin, dest);
    dest[0] -= 15.0f;
    dest[1] += 15.0f;
    trace = gi.trace(inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
    if (trace.fraction == 1.0f)
        return true;

    VectorCopy(targ->s.origin, dest);
    dest[0] -= 15.0f;
    dest[1] -= 15.0f;
    trace = gi.trace(inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
    if (trace.fraction == 1.0f)
        return true;


    return false;
}


/*
============
Killed
============
*/
void Killed(edict_t *targ, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
    if (targ->health < -999)
        targ->health = -999;

    targ->enemy = attacker;

    if ((targ->svflags & SVF_MONSTER) && (targ->lifeStatus != LIFESTATUS_DEAD)) {
            //targ->svflags |= SVF_DEADMONSTER;   // now treat as a different content type
            // WID: TODO: Monster Reimplement.        
            //if (!(targ->monsterinfo.aiflags & AI_GOOD_GUY)) {
            level.killed_monsters++;
            if (coop->value && attacker->client)
                attacker->client->resp.score++;
            // medics won't heal monsters that they kill themselves
            if (strcmp( (const char *)attacker->classname, "monster_medic") == 0)
                targ->owner = attacker;
        //}
    }

    if (targ->movetype == MOVETYPE_PUSH || targ->movetype == MOVETYPE_STOP || targ->movetype == MOVETYPE_NONE) {
        // doors, triggers, etc
        if ( targ->die ) {
            targ->die( targ, inflictor, attacker, damage, point );
        }
        return;
    }

    if ((targ->svflags & SVF_MONSTER) && (targ->lifeStatus != LIFESTATUS_DEAD)) {
        targ->touch = NULL;
        // WID: TODO: Actually trigger death.
        //monster_death_use(targ);
        //When a monster dies, it fires all of its targets with the current
        //enemy as activator.
        //void monster_death_use( edict_t * self ) {
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
    if ( targ->die ) {
        targ->die( targ, inflictor, attacker, damage, point );
    }
}


/*
================
SpawnDamage
================
*/
void SpawnDamage(int type, const vec3_t origin, const vec3_t normal, int damage)
{
    if (damage > 255)
        damage = 255;
    gi.WriteUint8(svc_temp_entity);
    gi.WriteUint8(type);
//  gi.WriteByte (damage);
    gi.WritePosition( origin, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
    gi.WriteDir8(normal);
    gi.multicast( origin, MULTICAST_PVS, false );
}


/*
============
SVG_TriggerDamage

targ        entity that is being damaged
inflictor   entity that is causing the damage
attacker    entity that caused the inflictor to damage targ
    example: targ=monster, inflictor=rocket, attacker=player

dir         direction of the attack
point       point at which the damage is being inflicted
normal      normal vector from that point
damage      amount of damage being inflicted
knockback   force to be applied against targ as a result of the damage

dflags      these flags are used to control how SVG_TriggerDamage works
    DAMAGE_RADIUS           damage was indirect (from a nearby explosion)
    DAMAGE_NO_ARMOR         armor does not protect from this damage
    DAMAGE_ENERGY           damage is from an energy based weapon
    DAMAGE_NO_KNOCKBACK     do not affect velocity, just view angles
    DAMAGE_BULLET           damage is from a bullet (used for ricochets)
    DAMAGE_NO_PROTECTION    kills godmode, armor, everything
============
*/
void M_ReactToDamage(edict_t *targ, edict_t *attacker)
{
    if (!(attacker->client) && !(attacker->svflags & SVF_MONSTER))
        return;

    if (attacker == targ || attacker == targ->enemy)
        return;

    // dead monsters, like misc_deadsoldier, don't have AI functions, but 
    // M_ReactToDamage might still be called on them
    if (targ->svflags & SVF_DEADMONSTER)
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

bool CheckTeamDamage(edict_t *targ, edict_t *attacker)
{
    //FIXME make the next line real and uncomment this block
    // if ((ability to damage a teammate == OFF) && (targ's team == attacker's team))
    return false;
}

void SVG_TriggerDamage(edict_t *targ, edict_t *inflictor, edict_t *attacker, const vec3_t dir, vec3_t point, const vec3_t normal, const int32_t damage, const int32_t knockBack, const entity_damageflags_t damageFlags, const sg_means_of_death_t meansOfDeath ) {
    // Final means of death.
    sg_means_of_death_t finalMeansOfDeath = meansOfDeath;
    
    // No use if we aren't accepting damage.
    if ( !targ->takedamage ) {
        return;
    }

    // easy mode takes half damage
    int32_t finalDamage = damage;
    if (skill->value == 0 && deathmatch->value == 0 && targ->client) {
        finalDamage *= 0.5f;
        if (!finalDamage )
            finalDamage = 1;
    }

    // Friendly fire avoidance.
    // If enabled you can't hurt teammates (but you can hurt yourself)
    // Knockback still occurs.
    if ((targ != attacker) && ((deathmatch->value && ((int)(dmflags->value) & (DF_MODELTEAMS | DF_SKINTEAMS))) || (coop->value && targ->client))) {
        if (SVG_OnSameTeam(targ, attacker)) {
            if ( (int)( dmflags->value ) & DF_NO_FRIENDLY_FIRE ) {
                finalDamage = 0;
            } else {
                finalMeansOfDeath = static_cast<sg_means_of_death_t>( finalMeansOfDeath | MEANS_OF_DEATH_FRIENDLY_FIRE );
            }
        }
    }

    targ->meansOfDeath = finalMeansOfDeath;

    gclient_t *client = targ->client;

    // Default TE that got us was sparks.
    int32_t te_sparks = TE_SPARKS;
    // Special sparks for a bullet.
    if ( damageFlags & DAMAGE_BULLET ) {
        te_sparks = TE_BULLET_SPARKS;
    }

    // Bonus damage for suprising a monster.
    if (!( damageFlags & DAMAGE_RADIUS) && (targ->svflags & SVF_MONSTER)
        && (attacker->client) && (!targ->enemy) && (targ->health > 0)) {
        finalDamage *= 2;
    }

    // Determine knockback value.
    const int32_t finalKnockBack = ( targ->flags & FL_NO_KNOCKBACK ? 0 : knockBack );

    // Figure momentum add.
    if ( !( damageFlags & DAMAGE_NO_KNOCKBACK ) ) {
        if ( ( finalKnockBack ) && (targ->movetype != MOVETYPE_NONE) && (targ->movetype != MOVETYPE_BOUNCE) 
            && (targ->movetype != MOVETYPE_PUSH) && (targ->movetype != MOVETYPE_STOP)) {
                vec3_t  kvel;
                float   mass;

                if ( targ->mass < 50 ) {
                    mass = 50;
                } else {
                    mass = targ->mass;
                }

                VectorNormalize2(dir, kvel);

                if ( targ->client && attacker == targ ) {
                    VectorScale( kvel, 1600.0f * (float)knockBack / mass, kvel );  // the rocket jump hack...
                } else {
                    VectorScale( kvel, 500.0f * (float)knockBack / mass, kvel );
                }
                VectorAdd(targ->velocity, kvel, targ->velocity);
        }
    }

    int32_t take = finalDamage;
    int32_t save = 0;

    // check for godmode
    if ((targ->flags & FL_GODMODE) && !( damageFlags & DAMAGE_NO_PROTECTION)) {
        take = 0;
        save = finalDamage;
        SpawnDamage(te_sparks, point, normal, save);
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
    if ( !( damageFlags & DAMAGE_NO_PROTECTION ) && CheckTeamDamage( targ, attacker ) ) {
        return;
    }

// do the damage
    if (take) {
        if ((targ->svflags & SVF_MONSTER) || (client))
        {
            // SpawnDamage(TE_BLOOD, point, normal, take);
            SpawnDamage(TE_BLOOD, point, dir, take);
        }
        else
            SpawnDamage(te_sparks, point, normal, take);


        targ->health = targ->health - take;

        if (targ->health <= 0) {
            if ( ( targ->svflags & SVF_MONSTER ) || ( client ) ) {
                targ->flags = static_cast<entity_flags_t>( targ->flags | FL_NO_KNOCKBACK );
            }
            Killed(targ, inflictor, attacker, take, point);
            return;
        }
    }

    if (targ->svflags & SVF_MONSTER) {
        M_ReactToDamage(targ, attacker);
        // WID: TODO: Monster Reimplement.
        //if (!(targ->monsterinfo.aiflags & AI_DUCKED) && (take)) {
        if ( take ) {
            if ( targ->pain ) {
                targ->pain( targ, attacker, finalKnockBack, take );
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
    } else if (client) {
        if ( !( targ->flags & FL_GODMODE ) && ( take ) ) {
            if ( targ->pain ) {
                targ->pain( targ, attacker, finalKnockBack, take );
            } else {
                #ifdef WARN_ON_TRIGGERDAMAGE_NO_PAIN_CALLBACK
                gi.bprintf( PRINT_WARNING, "%s: ( targ->pain == nullptr )!\n", __func__ );
                #endif
            }
        }
    } else if (take) {
        if ( targ->pain ) {
            targ->pain( targ, attacker, finalKnockBack, take );
        } else {
            #ifdef WARN_ON_TRIGGERDAMAGE_NO_PAIN_CALLBACK
            gi.bprintf( PRINT_WARNING, "%s: ( targ->pain == nullptr )!\n", __func__ );
            #endif
        }
    }

    // add to the damage inflicted on a player this frame
    // the total will be turned into screen blends and view angle kicks
    // at the end of the frame
    if (client) {
        client->frameDamage.armor += asave;
        client->frameDamage.blood += take;
        client->frameDamage.knockBack += finalKnockBack;
        VectorCopy(point, client->frameDamage.from);
                //client->last_damage_time = level.time + COOP_DAMAGE_RESPAWN_TIME;

        if ( !( damageFlags & DAMAGE_NO_INDICATOR )
            && inflictor != world && attacker != world 
            && ( take || asave ) ) 
        {
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
                indicator->from = ( damageFlags & DAMAGE_RADIUS ) ? inflictor->s.origin : attacker->s.origin;
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


/*
============
SVG_RadiusDamage
============
*/
void SVG_RadiusDamage(edict_t *inflictor, edict_t *attacker, float damage, edict_t *ignore, float radius, const sg_means_of_death_t meansOfDeath ) 
{
    float   points;
    edict_t *ent = NULL;
    vec3_t  v;
    vec3_t  dir;

    while ((ent = SVG_FindWithinRadius(ent, inflictor->s.origin, radius)) != NULL) {
        if (ent == ignore)
            continue;
        if (!ent->takedamage)
            continue;

        VectorAdd(ent->mins, ent->maxs, v);
        VectorMA(ent->s.origin, 0.5f, v, v);
        VectorSubtract(inflictor->s.origin, v, v);
        points = damage - 0.5f * VectorLength(v);
        if (ent == attacker)
            points = points * 0.5f;
        if (points > 0) {
            if (SVG_CanDamage(ent, inflictor)) {
                VectorSubtract(ent->s.origin, inflictor->s.origin, dir);
                SVG_TriggerDamage(ent, inflictor, attacker, dir, inflictor->s.origin, vec3_origin, (int)points, (int)points, DAMAGE_RADIUS, meansOfDeath );
            }
        }
    }
}



