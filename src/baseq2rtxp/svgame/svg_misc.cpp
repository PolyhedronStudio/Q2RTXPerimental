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
#include "svg_misc.h"



void ClipGibVelocity(edict_t *ent)
{
    ent->velocity = QM_Vector3Clamp( 
        ent->velocity, 
        { -300, -300, 200 }, 
        { 300, 300, 500 } // always some upwards
    );
    //clamp(ent->velocity[0], -300, 300);
    //clamp(ent->velocity[1], -300, 300);
    //clamp(ent->velocity[2],  200, 500); // always some upwards
}


/***
*
*
*
*   Gib Entity Logics:
*
*
*
***/
/**
*   @brief
**/
void gib_think(edict_t *self) {
    self->s.frame++;
    //self->nextthink = level.frameNumber + 1;
	self->nextthink = level.time + FRAME_TIME_S;
    if (self->s.frame == 10) {
        self->think = SVG_FreeEdict;
        self->nextthink = level.time + random_time(8_sec, 10_sec);//= level.frameNumber + (8 + random() * 10) * BASE_FRAMERATE;
    }
}
/**
*   @brief
**/
void gib_touch( edict_t *self, edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) {
    vec3_t  normal_angles, right;

    if ( !self->groundInfo.entity ) {
        return;
    }

    self->touch = NULL;

    if (plane) {
        gi.sound(self, CHAN_VOICE, gi.soundindex("world/gib_drop01.wav"), 1, ATTN_NORM, 0);

        QM_Vector3ToAngles(plane->normal, normal_angles);
        AngleVectors(normal_angles, NULL, right, NULL);
        QM_Vector3ToAngles(right, self->s.angles);

        if (self->s.modelindex == sm_meat_index) {
            self->s.frame++;
            self->think = gib_think;
            self->nextthink = level.time + FRAME_TIME_S;//level.frameNumber + 1;
        }
    }
}
/**
*   @brief
**/
void gib_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point ) {
    SVG_FreeEdict(self);
}



/***
*
*
*
*   Throw Gib Entity Utilities:
*
*
*
***/
/**
*   @brief
**/
void SVG_Misc_ThrowGib( edict_t *self, const char *gibname, const int32_t damage, const int32_t type ) {
    vec3_t  origin;
    vec3_t  size;
    float   vscale;

    edict_t *gib = SVG_AllocateEdict();

    VectorScale(self->size, 0.5f, size);
    VectorAdd(self->absmin, size, origin);
    VectorMA(origin, crandom(), size, gib->s.origin);

    gi.setmodel(gib, gibname);
    gib->solid = SOLID_NOT;
    gib->s.entityType = ET_GIB;
    gib->s.effects |= EF_GIB;
    gib->flags = static_cast<entity_flags_t>( gib->flags | FL_NO_KNOCKBACK );
    gib->takedamage = DAMAGE_YES;
    gib->die = gib_die;

    if (type == GIB_TYPE_ORGANIC) {
        gib->movetype = MOVETYPE_TOSS;
        gib->touch = gib_touch;
        vscale = 0.5f;
    } else {
        gib->movetype = MOVETYPE_BOUNCE;
        vscale = 1.0f;
    }

    Vector3 velocityDamage = SVG_Misc_VelocityForDamage(damage);
    VectorMA(self->velocity, vscale, velocityDamage, gib->velocity);
    ClipGibVelocity(gib);
    gib->avelocity[0] = random() * 600;
    gib->avelocity[1] = random() * 600;
    gib->avelocity[2] = random() * 600;

    gib->think = SVG_FreeEdict;
    gib->nextthink = level.time + random_time(10_sec, 20_sec);//= level.frameNumber + (10 + random() * 10) * BASE_FRAMERATE;

    gi.linkentity(gib);
}
/**
*   @brief
**/
void SVG_Misc_ThrowHead( edict_t *self, const char *gibname, const int32_t damage, const int32_t type ) {
    vec3_t  vd;
    float   vscale;

    self->s.skinnum = 0;
    self->s.frame = 0;
    VectorClear(self->mins);
    VectorClear(self->maxs);

    self->s.modelindex2 = 0;
    gi.setmodel(self, gibname);
    self->solid = SOLID_NOT;
    self->s.entityType = ET_GIB;
    self->s.effects |= EF_GIB;
    //self->s.effects &= ~EF_FLIES;
    self->s.sound = 0;
    self->flags = static_cast<entity_flags_t>( self->flags | FL_NO_KNOCKBACK );
    self->svflags &= ~SVF_MONSTER;
    self->takedamage = DAMAGE_YES;
    self->die = gib_die;

    if (type == GIB_TYPE_ORGANIC) {
        self->movetype = MOVETYPE_TOSS;
        self->touch = gib_touch;
        vscale = 0.5f;
    } else {
        self->movetype = MOVETYPE_BOUNCE;
        vscale = 1.0f;
    }

    Vector3 velocityDamage = SVG_Misc_VelocityForDamage(damage);
    VectorMA(self->velocity, vscale, vd, self->velocity);
    ClipGibVelocity(self);

    self->avelocity[YAW] = crandom() * 600;

    self->think = SVG_FreeEdict;
    self->nextthink = level.time + random_time( 10_sec, 20_sec ); //level.frameNumber + (10 + random() * 10) * BASE_FRAMERATE;

    gi.linkentity(self);
}
/**
*   @brief
**/
void SVG_Misc_ThrowClientHead( edict_t *self, const int32_t damage ) {
	// WID: C++20: Added const.
    const char    *gibname;

    if (Q_rand() & 1) {
        gibname = "models/objects/gibs/head2/tris.md2";
        self->s.skinnum = 1;        // second skin is player
    } else {
        gibname = "models/objects/gibs/skull/tris.md2";
        self->s.skinnum = 0;
    }

    self->s.origin[2] += 32;
    self->s.frame = 0;
    gi.setmodel(self, gibname);
    VectorSet(self->mins, -16, -16, 0);
    VectorSet(self->maxs, 16, 16, 16);

    self->takedamage = DAMAGE_NO;
    self->solid = SOLID_NOT;
    self->s.effects = EF_GIB;
    self->s.sound = 0;
    self->flags = static_cast<entity_flags_t>( self->flags | FL_NO_KNOCKBACK );

    self->movetype = MOVETYPE_BOUNCE;
    Vector3 velocityDamage = SVG_Misc_VelocityForDamage( damage );
    self->velocity += velocityDamage;

    if (self->client) { // bodies in the queue don't have a client anymore
        gi.dprintf( "%s: WID: TODO: Implement client death animation here!\n", __func__ );
        //self->client->anim_priority = ANIM_DEATH;
        //self->client->anim_end = self->s.frame;
    } else {
        self->think = NULL;
        self->nextthink = 0_ms;
    }

    gi.linkentity(self);
}



/***
*
* 
* 
*   Debris
* 
* 
* 
***/
void debris_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
    SVG_FreeEdict(self);
}

/**
*   @brief  
**/
void SVG_Misc_ThrowDebris(edict_t *self, const char *modelname, const float speed, vec3_t origin)
{

    edict_t *chunk = SVG_AllocateEdict();
    VectorCopy(origin, chunk->s.origin);
    gi.setmodel(chunk, modelname);
    Vector3 v = {
        100 * crandom(),
        100 * crandom(),
        100 + 100 * crandom()
    };
    VectorMA(self->velocity, speed, v, chunk->velocity);
    chunk->movetype = MOVETYPE_BOUNCE;
    chunk->solid = SOLID_NOT;
    chunk->avelocity[0] = random() * 600;
    chunk->avelocity[1] = random() * 600;
    chunk->avelocity[2] = random() * 600;
    chunk->think = SVG_FreeEdict;
    chunk->nextthink = level.time + random_time( 5_sec, 10_sec );//= level.frameNumber + (5 + random() * 5) * BASE_FRAMERATE;
    chunk->s.frame = 0;
    chunk->flags = FL_NONE;
    chunk->classname = "debris";
    chunk->takedamage = DAMAGE_YES;
    chunk->die = debris_die;
    gi.linkentity(chunk);
}



/***
*
*
*
*   Explosions:
*
*
*
***/
/**
*   @brief	Spawns a temp entity explosion effect at the entity's origin, and optionally frees the entity.
**/
void SVG_Misc_BecomeExplosion( edict_t *self, int type, const bool freeEntity ) {
    gi.WriteUint8(svc_temp_entity);
    //if ( type == 1 ) {
    //    gi.WriteUint8( TE_EXPLOSION1 );
    //} else if ( type == 2 ) {
    //    gi.WriteUint8( TE_EXPLOSION2 );
    //} else if ( type == 3 ) {
    //    gi.WriteUint8( TE_EXPLOSION1_BIG );
    //} else if ( type == 4 ) {
    //    gi.WriteUint8( TE_EXPLOSION1_NP );
    //} else if ( type == 5 ) {
    //    gi.WriteUint8( TE_ROCKET_EXPLOSION_WATER );
    //} else if ( type == 6 ) {
    //    gi.WriteUint8( TE_GRENADE_EXPLOSION_WATER );
    //} else {
    //      gi.WriteUint8( TE_PLAIN_EXPLOSION );
    //}
    // Regular explosion.
    gi.WriteUint8( TE_PLAIN_EXPLOSION );
    gi.WritePosition( self->s.origin, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
    gi.multicast( self->s.origin, MULTICAST_PVS, false );

    // Free the entity if requested.
	if ( freeEntity ) {
		SVG_FreeEdict( self );
	}
}

/**
*   @brief	Returns a random velocity matching the specified damage count.
**/
const Vector3 SVG_Misc_VelocityForDamage( const int32_t damage ) {
    // Generate random velocity vector.
    Vector3 v = {
        100.0f * crandom(),
        100.0f * crandom(),
        200.0f + 100.0f * random()
    };

    // Scale it less intense if damage < 50
    if ( damage < 50 ) {
        v = QM_Vector3Scale( v, 0.7f );
    // Otherwise, intensify.
    } else {
        v = QM_Vector3Scale( v, 1.2f );
    }

    // Return velocity.
    return v;
}


/**
*   Enable and/or modify in the future, disabled for now, just acts as a reference.
**/
#if 0
/*QUAKED misc_gib_arm (1 0 0) (-8 -8 -8) (8 8 8)
Intended for use with the target_spawner
*/
void SP_misc_gib_arm(edict_t *ent)
{
    gi.setmodel(ent, "models/objects/gibs/arm/tris.md2");
    ent->solid = SOLID_NOT;
    ent->s.effects |= EF_GIB;
    ent->s.entityType = ET_GIB;
    ent->takedamage = DAMAGE_YES;
    ent->die = gib_die;
    ent->movetype = MOVETYPE_TOSS;
    ent->svflags |= SVF_MONSTER;
    ent->lifeStatus = LIFESTATUS_DEAD;
    ent->avelocity[0] = random() * 200;
    ent->avelocity[1] = random() * 200;
    ent->avelocity[2] = random() * 200;
    ent->think = SVG_FreeEdict;
    ent->nextthink = level.time + 30_sec;
    gi.linkentity(ent);
}

/*QUAKED misc_gib_leg (1 0 0) (-8 -8 -8) (8 8 8)
Intended for use with the target_spawner
*/
void SP_misc_gib_leg(edict_t *ent)
{
    gi.setmodel(ent, "models/objects/gibs/leg/tris.md2");
    ent->solid = SOLID_NOT;
    ent->s.effects |= EF_GIB;
    ent->s.entityType = ET_GIB;
    ent->takedamage = DAMAGE_YES;
    ent->die = gib_die;
    ent->movetype = MOVETYPE_TOSS;
    ent->svflags |= SVF_MONSTER;
    ent->lifeStatus = LIFESTATUS_DEAD;
    ent->avelocity[0] = random() * 200;
    ent->avelocity[1] = random() * 200;
    ent->avelocity[2] = random() * 200;
    ent->think = SVG_FreeEdict;
    ent->nextthink = level.time + 30_sec;
    gi.linkentity(ent);
}

/*QUAKED misc_gib_head (1 0 0) (-8 -8 -8) (8 8 8)
Intended for use with the target_spawner
*/
void SP_misc_gib_head(edict_t *ent)
{
    gi.setmodel(ent, "models/objects/gibs/head/tris.md2");
    ent->solid = SOLID_NOT;
    ent->s.effects |= EF_GIB;
    ent->s.entityType = ET_GIB;
    ent->takedamage = DAMAGE_YES;
    ent->die = gib_die;
    ent->movetype = MOVETYPE_TOSS;
    ent->svflags |= SVF_MONSTER;
    ent->lifeStatus = LIFESTATUS_DEAD;
    ent->avelocity[0] = random() * 200;
    ent->avelocity[1] = random() * 200;
    ent->avelocity[2] = random() * 200;
    ent->think = SVG_FreeEdict;
    ent->nextthink = level.time + 30_sec;
    gi.linkentity(ent);
}
#endif

//=================================================================================





