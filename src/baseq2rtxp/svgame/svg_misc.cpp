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
// g_misc.c
#include "svg_local.h"




void ClipGibVelocity(edict_t *ent)
{
    clamp(ent->velocity[0], -300, 300);
    clamp(ent->velocity[1], -300, 300);
    clamp(ent->velocity[2],  200, 500); // always some upwards
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
void gib_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf ) {
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

    Vector3 velocityDamage = VelocityForDamage(damage);
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

    Vector3 velocityDamage = VelocityForDamage(damage);
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
    vec3_t  vd;
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
    Vector3 velocityDamage = VelocityForDamage( damage );
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
*   @brief	Spawns a temp entity explosion effect at the entity's origin, and frees the entity.
**/
void SVG_Misc_BecomeExplosion1(edict_t *self)
{
    gi.WriteUint8(svc_temp_entity);
    gi.WriteUint8(TE_EXPLOSION1);
    gi.WritePosition( self->s.origin, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
    gi.multicast( self->s.origin, MULTICAST_PVS, false );

    SVG_FreeEdict(self);
}
/**
*   @brief	Spawns a temp entity explosion effect at the entity's origin, and frees the entity.
**/
void SVG_Misc_BecomeExplosion2(edict_t *self)
{
    gi.WriteUint8(svc_temp_entity);
    gi.WriteUint8(TE_EXPLOSION2);
    gi.WritePosition( self->s.origin, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
    gi.multicast( self->s.origin, MULTICAST_PVS, false );

    SVG_FreeEdict(self);
}


/*QUAKED path_corner (.5 .3 0) (-8 -8 -8) (8 8 8) TELEPORT
Target: next path corner
Pathtarget: gets used when an entity that has
    this path_corner targeted touches it
*/

void path_corner_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    vec3_t      v;
    edict_t     *next;

    if (other->movetarget != self)
        return;

    if (other->enemy)
        return;

    if (self->targetNames.path) {
        char *savetarget;

        savetarget = self->targetNames.target;
        self->targetNames.target = self->targetNames.path;
        SVG_UseTargets(self, other);
        self->targetNames.target = savetarget;
    }

    if (self->targetNames.target)
        next = SVG_PickTarget(self->targetNames.target);
    else
        next = NULL;

    if ((next) && (next->spawnflags & 1)) {
        VectorCopy(next->s.origin, v);
        v[2] += next->mins[2];
        v[2] -= other->mins[2];
        VectorCopy(v, other->s.origin);
        next = SVG_PickTarget(next->targetNames.target);
        other->s.event = EV_OTHER_TELEPORT;
    }

    other->goalentity = other->movetarget = next;

    if (self->wait) {
        // WID: TODO: Monster Reimplement.
        //other->monsterinfo.pause_time = level.time + sg_time_t::from_sec( self->wait );
        //other->monsterinfo.stand(other);
        return;
    }

    if (!other->movetarget) {
        // WID: TODO: Monster Reimplement.
        //other->monsterinfo.pause_time = HOLD_FOREVER;
        //other->monsterinfo.stand(other);
    } else {
        VectorSubtract(other->goalentity->s.origin, other->s.origin, v);
        other->ideal_yaw = QM_Vector3ToYaw(v);
    }
}

void SP_path_corner(edict_t *self)
{
    if (!self->targetname) {
        gi.dprintf("path_corner with no targetname at %s\n", vtos(self->s.origin));
        SVG_FreeEdict(self);
        return;
    }

    self->solid = SOLID_TRIGGER;
    self->touch = path_corner_touch;
    VectorSet(self->mins, -8, -8, -8);
    VectorSet(self->maxs, 8, 8, 8);
    self->svflags |= SVF_NOCLIENT;
    gi.linkentity(self);
}

/**
*   @brief	Returns a random velocity matching the specified damage count.
**/
const Vector3 &VelocityForDamage( const int32_t damage ) {
    // Generate random velocity vector.
    Vector3 v = {
        100.0f * crandom(),
        100.0f * crandom(),
        200.0f + 100.0f * random()
    };

    if ( damage < 50 ) {
        v = QM_Vector3Scale( v, 0.7f );
    } else {
        v = QM_Vector3Scale( v, 1.2f );
    }
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

void teleporter_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    edict_t     *dest;
    int         i;

    if (!other->client)
        return;
    dest = SVG_Find(NULL, FOFS_GENTITY(targetname), self->targetNames.target);
    if (!dest) {
        gi.dprintf("Couldn't find destination\n");
        return;
    }

    // unlink to make sure it can't possibly interfere with KillBox
    gi.unlinkentity(other);

    VectorCopy(dest->s.origin, other->s.origin);
    VectorCopy(dest->s.origin, other->s.old_origin);
    other->s.origin[2] += 10;

    // clear the velocity and hold them in place briefly
    VectorClear(other->velocity);
    other->client->ps.pmove.pm_time = 160 >> 3;     // hold time
    other->client->ps.pmove.pm_flags |= PMF_TIME_TELEPORT;

    // draw the teleport splash at source and on the player
    self->owner->s.event = EV_PLAYER_TELEPORT;
    other->s.event = EV_PLAYER_TELEPORT;

    // set angles
    for (i = 0 ; i < 3 ; i++) {
        other->client->ps.pmove.delta_angles[i] = /*ANGLE2SHORT*/(dest->s.angles[i] - other->client->resp.cmd_angles[i]);
    }

    VectorClear(other->s.angles);
    VectorClear(other->client->ps.viewangles);
    VectorClear(other->client->viewMove.viewAngles);
    QM_AngleVectors( other->client->viewMove.viewAngles, &other->client->viewMove.viewForward, nullptr, nullptr );

    // kill anything at the destination
    KillBox(other, !!other->client );

    gi.linkentity(other);
}

/*QUAKED misc_teleporter (1 0 0) (-32 -32 -24) (32 32 -16)
Stepping onto this disc will teleport players to the targeted misc_teleporter_dest object.
*/
void SP_misc_teleporter(edict_t *ent)
{
    edict_t     *trig;

    if (!ent->targetNames.target) {
        gi.dprintf("teleporter without a target.\n");
        SVG_FreeEdict(ent);
        return;
    }

    gi.setmodel(ent, "models/objects/dmspot/tris.md2");
    ent->s.skinnum = 1;
    ent->s.effects = EF_TELEPORTER;
    ent->s.renderfx = RF_NOSHADOW;
    //ent->s.sound = gi.soundindex("world/amb10.wav");
    ent->solid = SOLID_BOUNDS_BOX;

    VectorSet(ent->mins, -32, -32, -24);
    VectorSet(ent->maxs, 32, 32, -16);
    gi.linkentity(ent);

    trig = SVG_AllocateEdict();
    trig->touch = teleporter_touch;
    trig->solid = SOLID_TRIGGER;
    trig->s.entityType = ET_TELEPORT_TRIGGER;
    trig->targetNames.target = ent->targetNames.target;
    trig->owner = ent;
    VectorCopy(ent->s.origin, trig->s.origin);
    VectorSet(trig->mins, -8, -8, 8);
    VectorSet(trig->maxs, 8, 8, 24);
    gi.linkentity(trig);

}

/*QUAKED misc_teleporter_dest (1 0 0) (-32 -32 -24) (32 32 -16)
Point teleporters at these.
*/
void SP_misc_teleporter_dest(edict_t *ent)
{
    gi.setmodel(ent, "models/objects/dmspot/tris.md2");
    ent->s.skinnum = 0;
    ent->solid = SOLID_BOUNDS_BOX;
//  ent->s.effects |= EF_FLIES;
    ent->s.renderfx |= RF_NOSHADOW;
    VectorSet(ent->mins, -32, -32, -24);
    VectorSet(ent->maxs, 32, 32, -16);
    gi.linkentity(ent);
}

