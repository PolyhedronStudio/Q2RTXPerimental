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
// g_turret.c

#include "g_local.h"


void AnglesNormalize(vec3_t vec)
{
    while (vec[0] > 360)
        vec[0] -= 360;
    while (vec[0] < 0)
        vec[0] += 360;
    while (vec[1] > 360)
        vec[1] -= 360;
    while (vec[1] < 0)
        vec[1] += 360;
}

const float SnapToEights(const float x)
{
	// WID: Float-movement.
    //x *= 8.0f;
    //if (x > 0.0f)
    //    x += 0.5f;
    //else
    //    x -= 0.5f;
    //return 0.125f * (int)x;
	return x;
}


void turret_blocked(edict_t *self, edict_t *other)
{
    edict_t *attacker;

    if (other->takedamage) {
        if (self->teammaster->owner)
            attacker = self->teammaster->owner;
        else
            attacker = self->teammaster;
        SVG_TriggerDamage(other, self, attacker, vec3_origin, other->s.origin, vec3_origin, self->teammaster->dmg, 10, 0, MOD_CRUSH);
    }
}

/*QUAKED turret_breach (0 0 0) ?
This portion of the turret can change both pitch and yaw.
The model  should be made with a flat pitch.
It (and the associated base) need to be oriented towards 0.
Use "angle" to set the starting angle.

"speed"     default 50
"dmg"       default 10
"angle"     point this forward
"target"    point this at an info_notnull at the muzzle tip
"minpitch"  min acceptable pitch angle : default -30
"maxpitch"  max acceptable pitch angle : default 30
"minyaw"    min acceptable yaw angle   : default 0
"maxyaw"    max acceptable yaw angle   : default 360
*/

void turret_breach_fire(edict_t *self)
{
    vec3_t  f, r, u;
    vec3_t  start;
    int     damage;
    int     speed;

    AngleVectors(self->s.angles, f, r, u);
    VectorMA(self->s.origin, self->move_origin[0], f, start);
    VectorMA(start, self->move_origin[1], r, start);
    VectorMA(start, self->move_origin[2], u, start);

    damage = 100 + random() * 50;
    speed = 550 + 50 * skill->value;
    fire_rocket(self->teammaster->owner, start, f, damage, speed, 150, damage);
    gi.positioned_sound(start, self, CHAN_WEAPON, gi.soundindex("weapons/rocklf1a.wav"), 1, ATTN_NORM, 0);
}

void turret_breach_think(edict_t *self)
{
    edict_t *ent;
    vec3_t  current_angles;
    vec3_t  delta;

    VectorCopy(self->s.angles, current_angles);
    AnglesNormalize(current_angles);

    AnglesNormalize(self->move_angles);
    if (self->move_angles[PITCH] > 180)
        self->move_angles[PITCH] -= 360;

    // clamp angles to mins & maxs
    if (self->move_angles[PITCH] > self->pos1[PITCH])
        self->move_angles[PITCH] = self->pos1[PITCH];
    else if (self->move_angles[PITCH] < self->pos2[PITCH])
        self->move_angles[PITCH] = self->pos2[PITCH];

    if ((self->move_angles[YAW] < self->pos1[YAW]) || (self->move_angles[YAW] > self->pos2[YAW])) {
        float   dmin, dmax;

        dmin = fabsf(self->pos1[YAW] - self->move_angles[YAW]);
        if (dmin < -180)
            dmin += 360;
        else if (dmin > 180)
            dmin -= 360;
        dmax = fabsf(self->pos2[YAW] - self->move_angles[YAW]);
        if (dmax < -180)
            dmax += 360;
        else if (dmax > 180)
            dmax -= 360;
        if (fabsf(dmin) < fabsf(dmax))
            self->move_angles[YAW] = self->pos1[YAW];
        else
            self->move_angles[YAW] = self->pos2[YAW];
    }

    VectorSubtract(self->move_angles, current_angles, delta);
    if (delta[0] < -180)
        delta[0] += 360;
    else if (delta[0] > 180)
        delta[0] -= 360;
    if (delta[1] < -180)
        delta[1] += 360;
    else if (delta[1] > 180)
        delta[1] -= 360;
    delta[2] = 0;

	if ( delta[ 0 ] > self->speed * gi.frame_time_s )
		delta[ 0 ] = self->speed * gi.frame_time_s;
	if ( delta[ 0 ] < -1 * self->speed * gi.frame_time_s )
		delta[ 0 ] = -1 * self->speed * gi.frame_time_s;
	if ( delta[ 1 ] > self->speed * gi.frame_time_s )
		delta[ 1 ] = self->speed * gi.frame_time_s;
	if ( delta[ 1 ] < -1 * self->speed * gi.frame_time_s )
		delta[ 1 ] = -1 * self->speed * gi.frame_time_s;

    VectorScale(delta, ( 1.0f / gi.frame_time_s ), self->avelocity);

	self->nextthink = level.time + FRAME_TIME_S;

    for (ent = self->teammaster; ent; ent = ent->teamchain)
        ent->avelocity[1] = self->avelocity[1];

    // if we have adriver, adjust his velocities
    if (self->owner) {
        float   angle;
        float   target_z;
        float   diff;
        vec3_t  target;
        vec3_t  dir;

        // angular is easy, just copy ours
        self->owner->avelocity[0] = self->avelocity[0];
        self->owner->avelocity[1] = self->avelocity[1];

        // x & y
        angle = self->s.angles[1] + self->owner->move_origin[1];
        angle = DEG2RAD(angle);
        target[0] = SnapToEights(self->s.origin[0] + cos(angle) * self->owner->move_origin[0]);
        target[1] = SnapToEights(self->s.origin[1] + sin(angle) * self->owner->move_origin[0]);
        target[2] = self->owner->s.origin[2];

        VectorSubtract(target, self->owner->s.origin, dir);
        self->owner->velocity[0] = dir[0] * 1.0f / FRAMETIME;
        self->owner->velocity[1] = dir[1] * 1.0f / FRAMETIME;

        // z
        angle = DEG2RAD(self->s.angles[PITCH]);
        target_z = SnapToEights(self->s.origin[2] + self->owner->move_origin[0] * tan(angle) + self->owner->move_origin[2]);

        diff = target_z - self->owner->s.origin[2];
        self->owner->velocity[2] = diff * 1.0f / FRAMETIME;

        if (self->spawnflags & 65536) {
            turret_breach_fire(self);
            self->spawnflags &= ~65536;
        }
    }
}

void turret_breach_finish_init(edict_t *self)
{
    // get and save info for muzzle location
    if (!self->targetNames.target) {
        gi.dprintf("%s at %s needs a target\n", self->classname, vtos(self->s.origin));
    } else {
        self->targetEntities.target = SVG_PickTarget(self->targetNames.target);
        VectorSubtract(self->targetEntities.target->s.origin, self->s.origin, self->move_origin);
        SVG_FreeEdict(self->targetEntities.target);
    }

    self->teammaster->dmg = self->dmg;
    self->think = turret_breach_think;
    self->think(self);
}

void SP_turret_breach(edict_t *self)
{
    self->solid = SOLID_BSP;
    self->movetype = MOVETYPE_PUSH;
    gi.setmodel(self, self->model);

    if (!self->speed)
        self->speed = 50;
    if (!self->dmg)
        self->dmg = 10;

    if (!st.minpitch)
        st.minpitch = -30;
    if (!st.maxpitch)
        st.maxpitch = 30;
    if (!st.maxyaw)
        st.maxyaw = 360;

    self->pos1[PITCH] = -1 * st.minpitch;
    self->pos1[YAW]   = st.minyaw;
    self->pos2[PITCH] = -1 * st.maxpitch;
    self->pos2[YAW]   = st.maxyaw;

    self->ideal_yaw = self->s.angles[YAW];
    self->move_angles[YAW] = self->ideal_yaw;

    self->blocked = turret_blocked;

    self->think = turret_breach_finish_init;
	self->nextthink = level.time + FRAME_TIME_S;
    gi.linkentity(self);
}


/*QUAKED turret_base (0 0 0) ?
This portion of the turret changes yaw only.
MUST be teamed with a turret_breach.
*/

void SP_turret_base(edict_t *self)
{
    self->solid = SOLID_BSP;
    self->movetype = MOVETYPE_PUSH;
    gi.setmodel(self, self->model);
    self->blocked = turret_blocked;
    gi.linkentity(self);
}


/*QUAKED turret_driver (1 .5 0) (-16 -16 -24) (16 16 32)
Must NOT be on the team with the rest of the turret parts.
Instead it must target the turret_breach.
*/

void infantry_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point);
void infantry_stand(edict_t *self);
void monster_use(edict_t *self, edict_t *other, edict_t *activator);

void turret_driver_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
    edict_t *ent;

    // level the gun
    self->targetEntities.target->move_angles[0] = 0;

    // remove the driver from the end of them team chain
    for (ent = self->targetEntities.target->teammaster; ent->teamchain != self; ent = ent->teamchain)
        ;
    ent->teamchain = NULL;
    self->teammaster = NULL;
    self->flags = static_cast<entity_flags_t>( self->flags & ~FL_TEAMSLAVE );

    self->targetEntities.target->owner = NULL;
    self->targetEntities.target->teammaster->owner = NULL;

    infantry_die(self, inflictor, attacker, damage, point );
}

bool FindTarget(edict_t *self);

void turret_driver_think(edict_t *self)
{
    vec3_t  target;
    vec3_t  dir;

	self->nextthink = level.time + FRAME_TIME_S;

    if (self->enemy && (!self->enemy->inuse || self->enemy->health <= 0))
        self->enemy = NULL;

    if (!self->enemy) {
        if (!FindTarget(self))
            return;
        self->monsterinfo.trail_time = level.time;
        self->monsterinfo.aiflags &= ~AI_LOST_SIGHT;
    } else {
        if (visible(self, self->enemy)) {
            if (self->monsterinfo.aiflags & AI_LOST_SIGHT) {
				self->monsterinfo.trail_time = level.time;
                self->monsterinfo.aiflags &= ~AI_LOST_SIGHT;
            }
        } else {
            self->monsterinfo.aiflags |= AI_LOST_SIGHT;
            return;
        }
    }

    // let the turret know where we want it to aim
    VectorCopy(self->enemy->s.origin, target);
    target[2] += self->enemy->viewheight;
    VectorSubtract(target, self->targetEntities.target->s.origin, dir);
    QM_Vector3ToAngles(dir, self->targetEntities.target->move_angles);

    // decide if we should shoot
	sg_time_t reaction_time = sg_time_t::from_sec( 3 - skill->integer );
	if ( ( level.time - self->monsterinfo.trail_time ) < reaction_time )
		return;

	self->monsterinfo.attack_finished = level.time + reaction_time + 1_sec;
    //FIXME how do we really want to pass this along?
    self->targetEntities.target->spawnflags |= 65536;
}

void turret_driver_link(edict_t *self)
{
    vec3_t  vec;
    edict_t *ent;

    self->think = turret_driver_think;
	self->nextthink = level.time + FRAME_TIME_S;

    self->targetEntities.target = SVG_PickTarget(self->targetNames.target);
    self->targetEntities.target->owner = self;
    self->targetEntities.target->teammaster->owner = self;
    VectorCopy(self->targetEntities.target->s.angles, self->s.angles);

    vec[0] = self->targetEntities.target->s.origin[0] - self->s.origin[0];
    vec[1] = self->targetEntities.target->s.origin[1] - self->s.origin[1];
    vec[2] = 0;
    self->move_origin[0] = VectorLength(vec);

    VectorSubtract(self->s.origin, self->targetEntities.target->s.origin, vec);
    QM_Vector3ToAngles(vec, vec);
    AnglesNormalize(vec);
    self->move_origin[1] = vec[1];

    self->move_origin[2] = self->s.origin[2] - self->targetEntities.target->s.origin[2];

    // add the driver to the end of them team chain
    for (ent = self->targetEntities.target->teammaster; ent->teamchain; ent = ent->teamchain)
        ;
    ent->teamchain = self;
    self->teammaster = self->targetEntities.target->teammaster;
    self->flags = static_cast<entity_flags_t>( self->flags | FL_TEAMSLAVE );
}

void SP_turret_driver(edict_t *self)
{
    if (deathmatch->value) {
        SVG_FreeEdict(self);
        return;
    }

    self->movetype = MOVETYPE_PUSH;
    self->solid = SOLID_BOUNDS_BOX;
    self->s.modelindex = gi.modelindex("models/monsters/infantry/tris.md2");
    VectorSet(self->mins, -16, -16, -24);
    VectorSet(self->maxs, 16, 16, 32);

    self->health = 100;
    self->gib_health = 0;
    self->mass = 200;
    self->viewheight = 24;

    self->die = turret_driver_die;
    self->monsterinfo.stand = infantry_stand;

    self->flags = static_cast<entity_flags_t>( self->flags | FL_NO_KNOCKBACK );

    level.total_monsters++;

    self->svflags |= SVF_MONSTER;
    self->s.renderfx |= RF_FRAMELERP;
    self->takedamage = DAMAGE_AIM;
    self->use = monster_use;
    self->clipmask = MASK_MONSTERSOLID;
    VectorCopy(self->s.origin, self->s.old_origin);
    self->monsterinfo.aiflags |= AI_STAND_GROUND | AI_DUCKED;

    if (st.item) {
        self->item = FindItemByClassname(st.item);
        if (!self->item)
            gi.dprintf("%s at %s has bad item: %s\n", self->classname, vtos(self->s.origin), st.item);
    }

    self->think = turret_driver_link;
	self->nextthink = level.time + FRAME_TIME_S;

    gi.linkentity(self);
}
