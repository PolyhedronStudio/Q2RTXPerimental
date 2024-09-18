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


/*QUAKED func_group (0 0 0) ?
Used to group brushes together just for editor convenience.
*/

//=====================================================

void Use_Areaportal(edict_t *ent, edict_t *other, edict_t *activator)
{
    //ent->count ^= 1;        // toggle state
    int32_t areaPortalState = gi.GetAreaPortalState( ent->style );
    areaPortalState ^= 1;
//  gi.dprintf ("portalstate: %i = %i\n", ent->style, ent->count);
    gi.SetAreaPortalState(ent->style, areaPortalState );
}

/*QUAKED func_areaportal (0 0 0) ?

This is a non-visible object that divides the world into
areas that are seperated when this portal is not activated.
Usually enclosed in the middle of a door.
*/
void SP_func_areaportal(edict_t *ent)
{
    ent->s.entityType = ET_AREA_PORTAL;
    ent->use = Use_Areaportal;
    // always start closed;
    ent->count = 0; // gi.GetAreaPortalState( ent->style );     
}

//=====================================================


/*
=================
Misc functions
=================
*/
void VelocityForDamage(int damage, vec3_t v)
{
    v[0] = 100.0f * crandom();
    v[1] = 100.0f * crandom();
    v[2] = 200.0f + 100.0f * random();

    if (damage < 50)
        VectorScale(v, 0.7f, v);
    else
        VectorScale(v, 1.2f, v);
}

void ClipGibVelocity(edict_t *ent)
{
    clamp(ent->velocity[0], -300, 300);
    clamp(ent->velocity[1], -300, 300);
    clamp(ent->velocity[2],  200, 500); // always some upwards
}


/*
=================
gibs
=================
*/
void gib_think(edict_t *self)
{
    self->s.frame++;
    //self->nextthink = level.framenum + 1;
	self->nextthink = level.time + FRAME_TIME_S;
    if (self->s.frame == 10) {
        self->think = SVG_FreeEdict;
        self->nextthink = level.time + random_time(8_sec, 10_sec);//= level.framenum + (8 + random() * 10) * BASE_FRAMERATE;
    }
}

void gib_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
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
            self->nextthink = level.time + FRAME_TIME_S;//level.framenum + 1;
        }
    }
}

void gib_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
    SVG_FreeEdict(self);
}

// WID: C++20: Added const.
void ThrowGib(edict_t *self, const char *gibname, int damage, int type)
{
    edict_t *gib;
    vec3_t  vd;
    vec3_t  origin;
    vec3_t  size;
    float   vscale;

    gib = SVG_AllocateEdict();

    VectorScale(self->size, 0.5f, size);
    VectorAdd(self->absmin, size, origin);
    VectorMA(origin, crandom(), size, gib->s.origin);

    gi.setmodel(gib, gibname);
    gib->solid = SOLID_NOT;
    gib->s.entityType = ET_GIB;
    gib->s.effects |= EF_GIB;
    gib->flags = static_cast<ent_flags_t>( gib->flags | FL_NO_KNOCKBACK );
    gib->takedamage = DAMAGE_YES;
    gib->die = gib_die;

    if (type == GIB_ORGANIC) {
        gib->movetype = MOVETYPE_TOSS;
        gib->touch = gib_touch;
        vscale = 0.5f;
    } else {
        gib->movetype = MOVETYPE_BOUNCE;
        vscale = 1.0f;
    }

    VelocityForDamage(damage, vd);
    VectorMA(self->velocity, vscale, vd, gib->velocity);
    ClipGibVelocity(gib);
    gib->avelocity[0] = random() * 600;
    gib->avelocity[1] = random() * 600;
    gib->avelocity[2] = random() * 600;

    gib->think = SVG_FreeEdict;
    gib->nextthink = level.time + random_time(10_sec, 20_sec);//= level.framenum + (10 + random() * 10) * BASE_FRAMERATE;

    gi.linkentity(gib);
}

// WID: C++20: Added const.
void ThrowHead(edict_t *self, const char *gibname, int damage, int type)
{
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
    self->flags = static_cast<ent_flags_t>( self->flags | FL_NO_KNOCKBACK );
    self->svflags &= ~SVF_MONSTER;
    self->takedamage = DAMAGE_YES;
    self->die = gib_die;

    if (type == GIB_ORGANIC) {
        self->movetype = MOVETYPE_TOSS;
        self->touch = gib_touch;
        vscale = 0.5f;
    } else {
        self->movetype = MOVETYPE_BOUNCE;
        vscale = 1.0f;
    }

    VelocityForDamage(damage, vd);
    VectorMA(self->velocity, vscale, vd, self->velocity);
    ClipGibVelocity(self);

    self->avelocity[YAW] = crandom() * 600;

    self->think = SVG_FreeEdict;
    self->nextthink = level.time + random_time( 10_sec, 20_sec ); //level.framenum + (10 + random() * 10) * BASE_FRAMERATE;

    gi.linkentity(self);
}


void ThrowClientHead(edict_t *self, int damage)
{
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
    self->flags = static_cast<ent_flags_t>( self->flags | FL_NO_KNOCKBACK );

    self->movetype = MOVETYPE_BOUNCE;
    VelocityForDamage(damage, vd);
    VectorAdd(self->velocity, vd, self->velocity);

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


/*
=================
debris
=================
*/
void debris_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
    SVG_FreeEdict(self);
}

// WID: C++20: Added const.
void ThrowDebris(edict_t *self, const char *modelname, float speed, vec3_t origin)
{
    edict_t *chunk;
    vec3_t  v;

    chunk = SVG_AllocateEdict();
    VectorCopy(origin, chunk->s.origin);
    gi.setmodel(chunk, modelname);
    v[0] = 100 * crandom();
    v[1] = 100 * crandom();
    v[2] = 100 + 100 * crandom();
    VectorMA(self->velocity, speed, v, chunk->velocity);
    chunk->movetype = MOVETYPE_BOUNCE;
    chunk->solid = SOLID_NOT;
    chunk->avelocity[0] = random() * 600;
    chunk->avelocity[1] = random() * 600;
    chunk->avelocity[2] = random() * 600;
    chunk->think = SVG_FreeEdict;
    chunk->nextthink = level.time + random_time( 5_sec, 10_sec );//= level.framenum + (5 + random() * 5) * BASE_FRAMERATE;
    chunk->s.frame = 0;
    chunk->flags = FL_NONE;
    chunk->classname = "debris";
    chunk->takedamage = DAMAGE_YES;
    chunk->die = debris_die;
    gi.linkentity(chunk);
}


void BecomeExplosion1(edict_t *self)
{
    gi.WriteUint8(svc_temp_entity);
    gi.WriteUint8(TE_EXPLOSION1);
    gi.WritePosition( self->s.origin, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
    gi.multicast( self->s.origin, MULTICAST_PVS, false );

    SVG_FreeEdict(self);
}


void BecomeExplosion2(edict_t *self)
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


/*QUAKED point_combat (0.5 0.3 0) (-8 -8 -8) (8 8 8) Hold
Makes this the target of a monster and it will head here
when first activated before going after the activator.  If
hold is selected, it will stay here.
*/
void point_combat_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    edict_t *activator;

    if (other->movetarget != self)
        return;

    if (self->targetNames.target) {
        other->targetNames.target = self->targetNames.target;
        other->goalentity = other->movetarget = SVG_PickTarget(other->targetNames.target);
        if (!other->goalentity) {
            gi.dprintf("%s at %s target %s does not exist\n", self->classname, vtos(self->s.origin), self->targetNames.target);
            other->movetarget = self;
        }
        self->targetNames.target = NULL;
    // WID: TODO: Monster Reimplement.
    }// else if ((self->spawnflags & 1) && !(other->flags & (FL_SWIM | FL_FLY))) {
    //    other->monsterinfo.pause_time = HOLD_FOREVER;
    //    other->monsterinfo.aiflags |= AI_STAND_GROUND;
    //    other->monsterinfo.stand(other);
    //}

    if (other->movetarget == self) {
        other->targetNames.target = NULL;
        other->movetarget = NULL;
        other->goalentity = other->enemy;
        // WID: TODO: Monster Reimplement.
        //other->monsterinfo.aiflags &= ~AI_COMBAT_POINT;
    }

    if (self->targetNames.path) {
        char *savetarget;

        savetarget = self->targetNames.target;
        self->targetNames.target = self->targetNames.path;
        if (other->enemy && other->enemy->client)
            activator = other->enemy;
        else if (other->oldenemy && other->oldenemy->client)
            activator = other->oldenemy;
        else if (other->activator && other->activator->client)
            activator = other->activator;
        else
            activator = other;
        SVG_UseTargets(self, activator);
        self->targetNames.target = savetarget;
    }
}

void SP_point_combat(edict_t *self)
{
    if (deathmatch->value) {
        SVG_FreeEdict(self);
        return;
    }
    self->solid = SOLID_TRIGGER;
    self->touch = point_combat_touch;
    VectorSet(self->mins, -8, -8, -16);
    VectorSet(self->maxs, 8, 8, 16);
    self->svflags = SVF_NOCLIENT;
    gi.linkentity(self);
}


/*QUAKED viewthing (0 .5 .8) (-8 -8 -8) (8 8 8)
Just for the debugging level.  Don't use
*/
void TH_viewthing(edict_t *ent)
{
    ent->s.frame = (ent->s.frame + 1) % 7;
    ent->nextthink = level.time + FRAME_TIME_S;//level.framenum + 1;
}

void SP_viewthing(edict_t *ent)
{
    gi.dprintf("viewthing spawned\n");

    ent->movetype = MOVETYPE_NONE;
    ent->solid = SOLID_BOUNDS_BOX;
    ent->s.renderfx = RF_FRAMELERP;
    VectorSet(ent->mins, -16, -16, -24);
    VectorSet(ent->maxs, 16, 16, 32);
    ent->s.modelindex = gi.modelindex("models/objects/banner/tris.md2");
    gi.linkentity(ent);
    ent->nextthink = level.time + 0.5_sec; //level.framenum + 0.5f * BASE_FRAMERATE;
    ent->think = TH_viewthing;
    return;
}


/*QUAKED info_null (0 0.5 0) (-4 -4 -4) (4 4 4)
Used as a positional target for spotlights, etc.
*/
void SP_info_null(edict_t *self)
{
    SVG_FreeEdict(self);
}


/*QUAKED info_notnull (0 0.5 0) (-4 -4 -4) (4 4 4)
Used as a positional target for lightning.
*/
void SP_info_notnull(edict_t *self)
{
    VectorCopy(self->s.origin, self->absmin);
    VectorCopy(self->s.origin, self->absmax);
}


/*QUAKED light (0 1 0) (-8 -8 -8) (8 8 8) START_OFF
Non-displayed light.
Default light value is 300.
Default style is 0.
If targeted, will toggle between on and off. Firing other set target also.
Default _cone value is 10 (used to set size of light for spotlights)
*/

#define START_OFF   1

void light_use( edict_t *self, edict_t *other, edict_t *activator ) {
    if ( self->spawnflags & START_OFF ) {
        if ( self->customLightStyle ) {
            gi.configstring( CS_LIGHTS + self->style, self->customLightStyle );
        } else {
            gi.configstring( CS_LIGHTS + self->style, "m" );
        }
        self->spawnflags &= ~START_OFF;
    } else {
        gi.configstring( CS_LIGHTS + self->style, "a" );
        self->spawnflags |= START_OFF;
    }

    // Apply activator.
    self->activator = activator;

    // Fire set target.
    SVG_UseTargets( self, activator );
}

void SP_light( edict_t *self ) {
    #if 0
    // no targeted lights in deathmatch, because they cause global messages
    if ( !self->targetname || deathmatch->value ) {
        SVG_FreeEdict( self );
        return;
    }
    #endif

    if ( self->style >= 32 ) {
        self->use = light_use;
        if ( self->spawnflags & START_OFF ) {
            gi.configstring( CS_LIGHTS + self->style, "a" );
        } else {
            if ( self->customLightStyle ) {
                gi.configstring( CS_LIGHTS + self->style, self->customLightStyle );
            } else {
                gi.configstring( CS_LIGHTS + self->style, "m" );
            }
        }
    }
}


/*QUAKED func_wall (0 .5 .8) ? TRIGGER_SPAWN TOGGLE START_ON ANIMATED ANIMATED_FAST
This is just a solid wall if not inhibited

TRIGGER_SPAWN   the wall will not be present until triggered
                it will then blink in to existance; it will
                kill anything that was in it's way

TOGGLE          only valid for TRIGGER_SPAWN walls
                this allows the wall to be turned on and off

START_ON        only valid for TRIGGER_SPAWN walls
                the wall will initially be present
*/

void func_wall_use(edict_t *self, edict_t *other, edict_t *activator)
{
    if (self->solid == SOLID_NOT) {
        self->solid = SOLID_BSP;
        self->svflags &= ~SVF_NOCLIENT;
        KillBox(self, false);
    } else {
        self->solid = SOLID_NOT;
        self->svflags |= SVF_NOCLIENT;
    }
    gi.linkentity(self);

    if (!(self->spawnflags & 2))
        self->use = NULL;
}

void SP_func_wall(edict_t *self)
{
    self->movetype = MOVETYPE_PUSH;
    self->s.entityType = ET_PUSHER;
    gi.setmodel(self, self->model);

    if (self->spawnflags & 8)
        self->s.effects |= EF_ANIM_ALL;
    if (self->spawnflags & 16)
        self->s.effects |= EF_ANIM_ALLFAST;

    // just a wall
    if ((self->spawnflags & 7) == 0) {
        self->solid = SOLID_BSP;
        gi.linkentity(self);
        return;
    }

    // it must be TRIGGER_SPAWN
    if (!(self->spawnflags & 1)) {
//      gi.dprintf("func_wall missing TRIGGER_SPAWN\n");
        self->spawnflags |= 1;
    }

    // yell if the spawnflags are odd
    if (self->spawnflags & 4) {
        if (!(self->spawnflags & 2)) {
            gi.dprintf("func_wall START_ON without TOGGLE\n");
            self->spawnflags |= 2;
        }
    }

    self->use = func_wall_use;
    if (self->spawnflags & 4) {
        self->solid = SOLID_BSP;
    } else {
        self->solid = SOLID_NOT;
        self->svflags |= SVF_NOCLIENT;
    }
    gi.linkentity(self);
}


/*QUAKED func_object (0 .5 .8) ? TRIGGER_SPAWN ANIMATED ANIMATED_FAST
This is solid bmodel that will fall if it's support it removed.
*/

void func_object_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    // only squash thing we fall on top of
    if ( !plane ) {
        return;
    }
    if ( plane->normal[ 2 ] < 1.0f ) {
        return;
    }
    if ( other && other->takedamage == DAMAGE_NO ) {
        return;
    }
    T_Damage(other, self, self, vec3_origin, self->s.origin, vec3_origin, self->dmg, 1, 0, MEANS_OF_DEATH_CRUSHED );
}

void func_object_release(edict_t *self)
{
    self->movetype = MOVETYPE_TOSS;
    self->touch = func_object_touch;
}

void func_object_use(edict_t *self, edict_t *other, edict_t *activator)
{
    self->solid = SOLID_BSP;
    self->svflags &= ~SVF_NOCLIENT;
    self->use = NULL;
    KillBox(self, false);
    func_object_release(self);
}

void SP_func_object(edict_t *self)
{
    gi.setmodel(self, self->model);

    self->mins[0] += 1;
    self->mins[1] += 1;
    self->mins[2] += 1;
    self->maxs[0] -= 1;
    self->maxs[1] -= 1;
    self->maxs[2] -= 1;

    if (!self->dmg)
        self->dmg = 100;

    if (self->spawnflags == 0) {
        self->solid = SOLID_BSP;
        self->movetype = MOVETYPE_PUSH;
        self->s.entityType = ET_PUSHER;
        self->think = func_object_release;
		self->nextthink = level.time + 20_hz;
    } else {
        self->solid = SOLID_NOT;
        self->movetype = MOVETYPE_PUSH;
        self->s.entityType = ET_PUSHER;
        self->use = func_object_use;
        self->svflags |= SVF_NOCLIENT;
    }

    if (self->spawnflags & 2)
        self->s.effects |= EF_ANIM_ALL;
    if (self->spawnflags & 4)
        self->s.effects |= EF_ANIM_ALLFAST;

    self->clipmask = MASK_MONSTERSOLID;

    gi.linkentity(self);
}


/*QUAKED func_explosive (0 .5 .8) ? Trigger_Spawn ANIMATED ANIMATED_FAST
Any brush that you want to explode or break apart.  If you want an
ex0plosion, set dmg and it will do a radius explosion of that amount
at the center of the bursh.

If targeted it will not be shootable.

health defaults to 100.

mass defaults to 75.  This determines how much debris is emitted when
it explodes.  You get one large chunk per 100 of mass (up to 8) and
one small chunk per 25 of mass (up to 16).  So 800 gives the most.
*/
void func_explosive_explode(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
    vec3_t  origin;
    vec3_t  chunkorigin;
    vec3_t  size;
    int     count;
    int     mass;

    // bmodel origins are (0 0 0), we need to adjust that here
    VectorScale(self->size, 0.5f, size);
    VectorAdd(self->absmin, size, origin);
    VectorCopy(origin, self->s.origin);

    self->takedamage = DAMAGE_NO;

    if (self->dmg)
        T_RadiusDamage(self, attacker, self->dmg, NULL, self->dmg + 40, MEANS_OF_DEATH_EXPLOSIVE);

    VectorSubtract(self->s.origin, inflictor->s.origin, self->velocity);
    self->velocity = QM_Vector3Normalize(self->velocity);
    VectorScale(self->velocity, 150, self->velocity);

    // start chunks towards the center
    VectorScale(size, 0.5f, size);

    mass = self->mass;
    if (!mass)
        mass = 75;

    // big chunks
    if (mass >= 100) {
        count = mass / 100;
        if (count > 8)
            count = 8;
        while (count--) {
            VectorMA(origin, crandom(), size, chunkorigin);
            ThrowDebris(self, "models/objects/debris1/tris.md2", 1, chunkorigin);
        }
    }

    // small chunks
    count = mass / 25;
    if (count > 16)
        count = 16;
    while (count--) {
        VectorMA(origin, crandom(), size, chunkorigin);
        ThrowDebris(self, "models/objects/debris2/tris.md2", 2, chunkorigin);
    }

    SVG_UseTargets(self, attacker);

    if (self->dmg)
        BecomeExplosion1(self);
    else
        SVG_FreeEdict(self);
}

void func_explosive_use(edict_t *self, edict_t *other, edict_t *activator)
{
    func_explosive_explode(self, self, activator, self->health, self->s.origin);
}

void func_explosive_spawn(edict_t *self, edict_t *other, edict_t *activator)
{
    self->solid = SOLID_BSP;
    self->svflags &= ~SVF_NOCLIENT;
    self->use = NULL;
    KillBox(self, false);
    gi.linkentity(self);
}

void SP_func_explosive(edict_t *self)
{
    if (deathmatch->value) {
        // auto-remove for deathmatch
        SVG_FreeEdict(self);
        return;
    }

    self->movetype = MOVETYPE_PUSH;
    self->s.entityType = ET_PUSHER;

    gi.modelindex("models/objects/debris1/tris.md2");
    gi.modelindex("models/objects/debris2/tris.md2");

    gi.setmodel(self, self->model);

    if (self->spawnflags & 1) {
        self->svflags |= SVF_NOCLIENT;
        self->solid = SOLID_NOT;
        self->use = func_explosive_spawn;
    } else {
        self->solid = SOLID_BSP;
        if (self->targetname)
            self->use = func_explosive_use;
    }

    if (self->spawnflags & 2)
        self->s.effects |= EF_ANIM_ALL;
    if (self->spawnflags & 4)
        self->s.effects |= EF_ANIM_ALLFAST;

    if (self->use != func_explosive_use) {
        if (!self->health)
            self->health = 100;
        self->die = func_explosive_explode;
        self->takedamage = DAMAGE_YES;
    }

    gi.linkentity(self);
}


/*QUAKED misc_explobox (0 .5 .8) (-16 -16 0) (16 16 40)
Large exploding box.  You can override its mass (100),
health (80), and dmg (150).
*/

void barrel_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf ) {
    
    if ( ( !other->groundInfo.entity ) || ( other->groundInfo.entity == self ) ) {
        return;
    }

    // Calculate direction.
    vec3_t v = { };
    VectorSubtract(self->s.origin, other->s.origin, v);

    // Move ratio(based on their masses).
    const float ratio = (float)other->mass / (float)self->mass;

    // Yaw direction angle.
    const float yawAngle = QM_Vector3ToYaw( v );
    const float direction = yawAngle;
    // Distance to travel.
    float distance = 20 * ratio * FRAMETIME;

    // Debug output:
    if ( plane ) {
        gi.dprintf( "self->s.origin( %s ), other->s.origin( %s )\n", vtos( self->s.origin ), vtos( other->s.origin ) );
        gi.dprintf( "v( %s ), plane->normal( %s ), direction(%f), distance(%f)\n", vtos( v ), vtos( plane->normal ), direction, distance );
    } else {
        gi.dprintf( "self->s.origin( %s ), other->s.origin( %s )\n", vtos( self->s.origin ), vtos( other->s.origin ) );
        gi.dprintf( "v( %s ), direction(%f), distance(%f)\n", vtos( v ), direction, distance );
    }

    // WID: TODO: Use new monster walkmove/slidebox implementation.
    // Perform move.
    //M_walkmove( self, direction, distance );
}

void barrel_explode(edict_t *self)
{
    vec3_t  org;
    float   spd;
    vec3_t  save;
    int     i;

    T_RadiusDamage(self, self->activator, self->dmg, NULL, self->dmg + 40, MEANS_OF_DEATH_EXPLODED_BARREL);

    VectorCopy(self->s.origin, save);
    VectorMA(self->absmin, 0.5f, self->size, self->s.origin);

    // a few big chunks
    spd = 1.5f * (float)self->dmg / 200.0f;
    VectorMA(self->s.origin, crandom(), self->size, org);
    ThrowDebris(self, "models/objects/debris1/tris.md2", spd, org);
    VectorMA(self->s.origin, crandom(), self->size, org);
    ThrowDebris(self, "models/objects/debris1/tris.md2", spd, org);

    // bottom corners
    spd = 1.75f * (float)self->dmg / 200.0f;
    VectorCopy(self->absmin, org);
    ThrowDebris(self, "models/objects/debris3/tris.md2", spd, org);
    VectorCopy(self->absmin, org);
    org[0] += self->size[0];
    ThrowDebris(self, "models/objects/debris3/tris.md2", spd, org);
    VectorCopy(self->absmin, org);
    org[1] += self->size[1];
    ThrowDebris(self, "models/objects/debris3/tris.md2", spd, org);
    VectorCopy(self->absmin, org);
    org[0] += self->size[0];
    org[1] += self->size[1];
    ThrowDebris(self, "models/objects/debris3/tris.md2", spd, org);

    // a bunch of little chunks
    spd = 2 * self->dmg / 200;
    for (i = 0; i < 8; i++) {
        VectorMA(self->s.origin, crandom(), self->size, org);
        ThrowDebris(self, "models/objects/debris2/tris.md2", spd, org);
    }

    VectorCopy(save, self->s.origin);
    if ( self->groundInfo.entity ) {
        BecomeExplosion2( self );
    } else {
        BecomeExplosion1( self );
    }
}

void barrel_delay(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
    self->takedamage = DAMAGE_NO;
    self->nextthink = level.time + random_time( 150_ms );
    self->think = barrel_explode;
    self->activator = attacker;
}

void SP_misc_explobox(edict_t *self)
{
    if (deathmatch->value) {
        // auto-remove for deathmatch
        SVG_FreeEdict(self);
        return;
    }

    gi.modelindex("models/objects/debris1/tris.md2");
    gi.modelindex("models/objects/debris2/tris.md2");
    gi.modelindex("models/objects/debris3/tris.md2");

    self->solid = SOLID_BOUNDS_OCTAGON;
    self->movetype = MOVETYPE_STEP;

    self->model = "models/objects/barrels/tris.md2";
    self->s.modelindex = gi.modelindex(self->model);
    VectorSet(self->mins, -16, -16, 0);
    VectorSet(self->maxs, 16, 16, 40);

    if (!self->mass)
        self->mass = 400;
    if (!self->health)
        self->health = 10;
    if (!self->dmg)
        self->dmg = 150;

    self->die = barrel_delay;
    self->takedamage = DAMAGE_YES;
    //self->monsterinfo.aiflags = AI_NOSTEP;

    self->touch = barrel_touch;

    self->think = M_droptofloor;
	self->nextthink = level.time + 20_hz;

    gi.linkentity(self);
}


//
// miscellaneous specialty items
//

/*QUAKED light_mine1 (0 1 0) (-2 -2 -12) (2 2 12)
*/
void SP_light_mine1(edict_t *ent)
{
    ent->movetype = MOVETYPE_NONE;
    ent->solid = SOLID_BOUNDS_BOX;
    ent->s.modelindex = gi.modelindex("models/objects/minelite/light1/tris.md2");
    gi.linkentity(ent);
}


/*QUAKED light_mine2 (0 1 0) (-2 -2 -12) (2 2 12)
*/
void SP_light_mine2(edict_t *ent)
{
    ent->movetype = MOVETYPE_NONE;
    ent->solid = SOLID_BOUNDS_BOX;
    ent->s.modelindex = gi.modelindex("models/objects/minelite/light2/tris.md2");
    gi.linkentity(ent);
}


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
    ent->deadflag = DEAD_DEAD;
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
    ent->deadflag = DEAD_DEAD;
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
    ent->deadflag = DEAD_DEAD;
    ent->avelocity[0] = random() * 200;
    ent->avelocity[1] = random() * 200;
    ent->avelocity[2] = random() * 200;
    ent->think = SVG_FreeEdict;
    ent->nextthink = level.time + 30_sec;
    gi.linkentity(ent);
}

//=====================================================

/*QUAKED target_character (0 0 1) ?
used with target_string (must be on same "team")
"count" is position in the string (starts at 1)
*/

void SP_target_character(edict_t *self)
{
    self->movetype = MOVETYPE_PUSH;
    self->s.entityType = ET_PUSHER;
    gi.setmodel(self, self->model);
    self->solid = SOLID_BSP;
    self->s.frame = 12;
    gi.linkentity(self);
    return;
}


/*QUAKED target_string (0 0 1) (-8 -8 -8) (8 8 8)
*/

void target_string_use(edict_t *self, edict_t *other, edict_t *activator)
{
    edict_t *e;
    int     n, l;
    char    c;

    l = strlen(self->message);
    for (e = self->teammaster; e; e = e->teamchain) {
        if (!e->count)
            continue;
        n = e->count - 1;
        if (n > l) {
            e->s.frame = 12;
            continue;
        }

        c = self->message[n];
        if (c >= '0' && c <= '9')
            e->s.frame = c - '0';
        else if (c == '-')
            e->s.frame = 10;
        else if (c == ':')
            e->s.frame = 11;
        else
            e->s.frame = 12;
    }
}

void SP_target_string(edict_t *self)
{
    if (!self->message)
        self->message = const_cast<char*>(""); // WID: C++20: Added cast.
    self->use = target_string_use;
}


/*QUAKED func_clock (0 0 1) (-8 -8 -8) (8 8 8) TIMER_UP TIMER_DOWN START_OFF MULTI_USE
target a target_string with this

The default is to be a time of day clock

TIMER_UP and TIMER_DOWN run for "count" seconds and the fire "targetNames.path"
If START_OFF, this entity must be used before it starts

"style"     0 "xx"
            1 "xx:xx"
            2 "xx:xx:xx"
*/

static void func_clock_reset(edict_t *self)
{
    self->activator = NULL;
    if (self->spawnflags & 1) {
        self->health = 0;
        self->wait = self->count;
    } else if (self->spawnflags & 2) {
        self->health = self->count;
        self->wait = 0;
    }
}

static void func_clock_format_countdown(edict_t *self)
{
    if (self->style == 0) {
        Q_snprintf(self->message, CLOCK_MESSAGE_SIZE, "%2i", self->health);
        return;
    }

    if (self->style == 1) {
        Q_snprintf(self->message, CLOCK_MESSAGE_SIZE, "%2i:%2i", self->health / 60, self->health % 60);
        if (self->message[3] == ' ')
            self->message[3] = '0';
        return;
    }

    if (self->style == 2) {
        Q_snprintf(self->message, CLOCK_MESSAGE_SIZE, "%2i:%2i:%2i", self->health / 3600, (self->health - (self->health / 3600) * 3600) / 60, self->health % 60);
        if (self->message[3] == ' ')
            self->message[3] = '0';
        if (self->message[6] == ' ')
            self->message[6] = '0';
        return;
    }
}

void func_clock_think(edict_t *self)
{
    if (!self->enemy) {
        self->enemy = SVG_Find(NULL, FOFS(targetname), self->targetNames.target);
        if (!self->enemy)
            return;
    }

    if (self->spawnflags & 1) {
        func_clock_format_countdown(self);
        self->health++;
    } else if (self->spawnflags & 2) {
        func_clock_format_countdown(self);
        self->health--;
    } else {
        struct tm   *ltime;
        time_t      gmtime;

        gmtime = time(NULL);
        ltime = localtime(&gmtime);
        if (ltime)
            Q_snprintf(self->message, CLOCK_MESSAGE_SIZE, "%2i:%2i:%2i", ltime->tm_hour, ltime->tm_min, ltime->tm_sec);
        else
            strcpy(self->message, "00:00:00");
        if (self->message[3] == ' ')
            self->message[3] = '0';
        if (self->message[6] == ' ')
            self->message[6] = '0';
    }

    self->enemy->message = self->message;
    self->enemy->use(self->enemy, self, self);

    if (((self->spawnflags & 1) && (self->health > self->wait)) ||
        ((self->spawnflags & 2) && (self->health < self->wait))) {
        if (self->targetNames.path) {
            char *savetarget;
            char *savemessage;

            savetarget = self->targetNames.target;
            savemessage = self->message;
            self->targetNames.target = self->targetNames.path;
            self->message = NULL;
            SVG_UseTargets(self, self->activator);
            self->targetNames.target = savetarget;
            self->message = savemessage;
        }

        if (!(self->spawnflags & 8))
            return;

        func_clock_reset(self);

        if (self->spawnflags & 4)
            return;
    }

	self->nextthink = level.time + 1_sec;
}

void func_clock_use(edict_t *self, edict_t *other, edict_t *activator)
{
    if (!(self->spawnflags & 8))
        self->use = NULL;
    if (self->activator)
        return;
    self->activator = activator;
    self->think(self);
}

void SP_func_clock(edict_t *self)
{
    if (!self->targetNames.target) {
        gi.dprintf("%s with no target at %s\n", self->classname, vtos(self->s.origin));
        SVG_FreeEdict(self);
        return;
    }

    if ((self->spawnflags & 2) && (!self->count)) {
        gi.dprintf("%s with no count at %s\n", self->classname, vtos(self->s.origin));
        SVG_FreeEdict(self);
        return;
    }

    if ((self->spawnflags & 1) && (!self->count))
        self->count = 60 * 60;;

    func_clock_reset(self);

	// WID: C++20: Addec cast.
    self->message = (char*)gi.TagMalloc(CLOCK_MESSAGE_SIZE, TAG_SVGAME_LEVEL);

    self->think = func_clock_think;

    if (self->spawnflags & 4)
        self->use = func_clock_use;
    else
		self->nextthink = level.time + 1_sec;
}

//=================================================================================

void teleporter_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    edict_t     *dest;
    int         i;

    if (!other->client)
        return;
    dest = SVG_Find(NULL, FOFS(targetname), self->targetNames.target);
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

