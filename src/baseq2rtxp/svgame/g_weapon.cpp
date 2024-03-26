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
=================
check_dodge

This is a support routine used when a client is firing
a non-instant attack weapon.  It checks to see if a
monster's dodge function should be called.
=================
*/
static void check_dodge(edict_t *self, vec3_t start, vec3_t dir, int speed)
{
    vec3_t  end;
    vec3_t  v;
    trace_t tr;

    // easy mode only ducks one quarter the time
    if (skill->value == 0) {
        if (random() > 0.25f)
            return;
    }

    VectorMA(start, 8192, dir, end);
    tr = gi.trace(start, NULL, NULL, end, self, MASK_SHOT);
    if ((tr.ent) && (tr.ent->svflags & SVF_MONSTER) && (tr.ent->health > 0) && (tr.ent->monsterinfo.dodge) && infront(tr.ent, self)) {
        VectorSubtract(tr.endpos, start, v);
        sg_time_t eta = sg_time_t::from_sec(VectorLength(v) - tr.ent->maxs[0]) / speed;
        tr.ent->monsterinfo.dodge(tr.ent, self, eta.seconds() );
    }
}


/*
=================
fire_hit

Used for all impact (hit/punch/slash) attacks
=================
*/
bool fire_hit(edict_t *self, vec3_t aim, int damage, int kick)
{
    trace_t     tr;
    vec3_t      forward, right, up;
    vec3_t      v;
    vec3_t      point;
    float       range;
    vec3_t      dir;

    //see if enemy is in range
    VectorSubtract(self->enemy->s.origin, self->s.origin, dir);
    range = VectorLength(dir);
    if (range > aim[0])
        return false;

    if (aim[1] > self->mins[0] && aim[1] < self->maxs[0]) {
        // the hit is straight on so back the range up to the edge of their bbox
        range -= self->enemy->maxs[0];
    } else {
        // this is a side hit so adjust the "right" value out to the edge of their bbox
        if (aim[1] < 0)
            aim[1] = self->enemy->mins[0];
        else
            aim[1] = self->enemy->maxs[0];
    }

    VectorMA(self->s.origin, range, dir, point);

    tr = gi.trace(self->s.origin, NULL, NULL, point, self, MASK_SHOT);
    if (tr.fraction < 1) {
        if (!tr.ent->takedamage)
            return false;
        // if it will hit any client/monster then hit the one we wanted to hit
        if ((tr.ent->svflags & SVF_MONSTER) || (tr.ent->client))
            tr.ent = self->enemy;
    }

    AngleVectors(self->s.angles, forward, right, up);
    VectorMA(self->s.origin, range, forward, point);
    VectorMA(point, aim[1], right, point);
    VectorMA(point, aim[2], up, point);
    VectorSubtract(point, self->enemy->s.origin, dir);

    // do the damage
    T_Damage(tr.ent, self, self, dir, point, vec3_origin, damage, kick / 2, DAMAGE_NO_KNOCKBACK, MOD_HIT);

    if (!(tr.ent->svflags & SVF_MONSTER) && (!tr.ent->client))
        return false;

    // do our special form of knockback here
    VectorMA(self->enemy->absmin, 0.5f, self->enemy->size, v);
    VectorSubtract(v, point, v);
    VectorNormalize(v);
    VectorMA(self->enemy->velocity, kick, v, self->enemy->velocity);
    if (self->enemy->velocity[2] > 0)
        self->enemy->groundentity = NULL;
    return true;
}


/*
=================
fire_lead

This is an internal support routine used for bullet/pellet based weapons.
=================
*/
static void fire_lead(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int te_impact, int hspread, int vspread, int mod)
{
    trace_t     tr;
    vec3_t      dir;
    vec3_t      forward, right, up;
    vec3_t      end;
    float       r;
    float       u;
    vec3_t      water_start;
    bool        water = false;
    contents_t  content_mask = static_cast<contents_t>( MASK_SHOT | MASK_WATER );

    tr = gi.trace(self->s.origin, NULL, NULL, start, self, MASK_SHOT);
    if (!(tr.fraction < 1.0f)) {
        QM_Vector3ToAngles(aimdir, dir);
        AngleVectors(dir, forward, right, up);

        r = crandom() * hspread;
        u = crandom() * vspread;
        VectorMA(start, 8192, forward, end);
        VectorMA(end, r, right, end);
        VectorMA(end, u, up, end);

        if (gi.pointcontents(start) & MASK_WATER) {
            water = true;
            VectorCopy(start, water_start);
            content_mask = static_cast<contents_t>( content_mask & ~MASK_WATER ); // content_mask &= ~MASK_WATER
        }

        tr = gi.trace(start, NULL, NULL, end, self, content_mask);

        // see if we hit water
        if (tr.contents & MASK_WATER) {
            int     color;

            water = true;
            VectorCopy(tr.endpos, water_start);

            if (!VectorCompare(start, tr.endpos)) {
                if (tr.contents & CONTENTS_WATER) {
                    if (strcmp(tr.surface->name, "*brwater") == 0)
                        color = SPLASH_BROWN_WATER;
                    else
                        color = SPLASH_BLUE_WATER;
                } else if (tr.contents & CONTENTS_SLIME)
                    color = SPLASH_SLIME;
                else if (tr.contents & CONTENTS_LAVA)
                    color = SPLASH_LAVA;
                else
                    color = SPLASH_UNKNOWN;

                if (color != SPLASH_UNKNOWN) {
                    gi.WriteUint8(svc_temp_entity);
                    gi.WriteUint8(TE_SPLASH);
                    gi.WriteUint8(8);
                    gi.WritePosition( tr.endpos, false );
                    gi.WriteDir8(tr.plane.normal);
                    gi.WriteUint8(color);
                    gi.multicast( tr.endpos, MULTICAST_PVS, false );
                }

                // change bullet's course when it enters water
                VectorSubtract(end, start, dir);
                QM_Vector3ToAngles(dir, dir);
                AngleVectors(dir, forward, right, up);
                r = crandom() * hspread * 2;
                u = crandom() * vspread * 2;
                VectorMA(water_start, 8192, forward, end);
                VectorMA(end, r, right, end);
                VectorMA(end, u, up, end);
            }

            // re-trace ignoring water this time
            tr = gi.trace(water_start, NULL, NULL, end, self, MASK_SHOT);
        }
    }

    // send gun puff / flash
    if (!((tr.surface) && (tr.surface->flags & SURF_SKY))) {
        if (tr.fraction < 1.0f) {
            if (tr.ent->takedamage) {
                T_Damage(tr.ent, self, self, aimdir, tr.endpos, tr.plane.normal, damage, kick, DAMAGE_BULLET, mod);
            } else {
                if (strncmp(tr.surface->name, "sky", 3) != 0) {
                    gi.WriteUint8(svc_temp_entity);
                    gi.WriteUint8(te_impact);
                    gi.WritePosition( tr.endpos, false );
                    gi.WriteDir8(tr.plane.normal);
                    gi.multicast( tr.endpos, MULTICAST_PVS, false );

                    if (self->client)
                        PlayerNoise(self, tr.endpos, PNOISE_IMPACT);
                }
            }
        }
    }

    // if went through water, determine where the end and make a bubble trail
    if (water) {
        vec3_t  pos;

        VectorSubtract(tr.endpos, water_start, dir);
        VectorNormalize(dir);
        VectorMA(tr.endpos, -2, dir, pos);
        if (gi.pointcontents(pos) & MASK_WATER)
            VectorCopy(pos, tr.endpos);
        else
            tr = gi.trace(pos, NULL, NULL, water_start, tr.ent, MASK_WATER);

        VectorAdd(water_start, tr.endpos, pos);
        VectorScale(pos, 0.5f, pos);

        gi.WriteUint8(svc_temp_entity);
        gi.WriteUint8(TE_BUBBLETRAIL);
        gi.WritePosition( water_start, false );
        gi.WritePosition( tr.endpos, false );
        gi.multicast( pos, MULTICAST_PVS, false );
    }
}


/*
=================
fire_bullet

Fires a single round.  Used for machinegun and chaingun.  Would be fine for
pistols, rifles, etc....
=================
*/
void fire_bullet(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, int mod)
{
    fire_lead(self, start, aimdir, damage, kick, TE_GUNSHOT, hspread, vspread, mod);
}


/*
=================
fire_shotgun

Shoots shotgun pellets.  Used by shotgun and super shotgun.
=================
*/
void fire_shotgun(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, int count, int mod)
{
    int     i;

    for (i = 0; i < count; i++)
        fire_lead(self, start, aimdir, damage, kick, TE_SHOTGUN, hspread, vspread, mod);
}


/*
=================
fire_blaster

Fires a single blaster bolt.  Used by the blaster and hyper blaster.
=================
*/
void blaster_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    int     mod;

    if (other == self->owner)
        return;

    if (surf && (surf->flags & SURF_SKY)) {
        G_FreeEdict(self);
        return;
    }

    if (self->owner->client)
        PlayerNoise(self->owner, self->s.origin, PNOISE_IMPACT);

    if (other->takedamage) {
        if (self->spawnflags & 1)
            mod = MOD_HYPERBLASTER;
        else
            mod = MOD_BLASTER;
        T_Damage(other, self, self->owner, self->velocity, self->s.origin, plane->normal, self->dmg, 1, DAMAGE_ENERGY, mod);
    } else {
        gi.WriteUint8(svc_temp_entity);
        gi.WriteUint8(TE_BLASTER);
        gi.WritePosition( self->s.origin, false );
        if (!plane)
            gi.WriteDir8(vec3_origin);
        else
            gi.WriteDir8(plane->normal);
        gi.multicast( self->s.origin, MULTICAST_PVS, false );
    }

    G_FreeEdict(self);
}

static const bool G_ShouldPlayersCollideProjectile( edict_t *self ) {
    // In Coop they don't.
    if ( G_GetActiveGamemodeID() == GAMEMODE_COOPERATIVE ) {
        return false;
    }

    // Otherwise they do.
    return true;
}
void fire_blaster(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int effect, bool hyper)
{
    edict_t *bolt;
    trace_t tr;

    VectorNormalize(dir);

    bolt = G_AllocateEdict();
    bolt->svflags = SVF_PROJECTILE; // Special net code for projectiles. 
    // SVF_DEADMONSTER; // The following is now irrelevant:
    // yes, I know it looks weird that projectiles are deadmonsters
    // what this means is that when prediction is used against the object
    // (blaster/hyperblaster shots), the player won't be solid clipped against
    // the object.  Right now trying to run into a firing hyperblaster
    // is very jerky since you are predicted 'against' the shots.
    VectorCopy(start, bolt->s.origin);
    VectorCopy(start, bolt->s.old_origin);
    QM_Vector3ToAngles(dir, bolt->s.angles);
    VectorScale(dir, speed, bolt->velocity);
    bolt->movetype = MOVETYPE_FLYMISSILE;
    bolt->clipmask = MASK_PROJECTILE;
    if ( self->client && G_ShouldPlayersCollideProjectile( self ) ) {
        bolt->clipmask = static_cast<contents_t>( bolt->clipmask & ~CONTENTS_PLAYER );
    }
    bolt->flags = static_cast<ent_flags_t>( bolt->flags | FL_DODGE );
    bolt->solid = SOLID_BOUNDS_BOX;
    bolt->s.effects |= effect;
    bolt->s.renderfx |= RF_NOSHADOW;
    VectorClear(bolt->mins);
    VectorClear(bolt->maxs);
    bolt->s.modelindex = gi.modelindex("models/objects/laser/tris.md2");
    bolt->s.sound = gi.soundindex("misc/lasfly.wav");
    bolt->owner = self;
    bolt->touch = blaster_touch;
    bolt->nextthink = level.time + 2_sec;
    bolt->think = G_FreeEdict;
    bolt->dmg = damage;
    bolt->classname = "bolt";
    if (hyper)
        bolt->spawnflags = 1;
    gi.linkentity(bolt);

    if (self->client)
        check_dodge(self, bolt->s.origin, dir, speed);

    tr = gi.trace(self->s.origin, NULL, NULL, bolt->s.origin, bolt, MASK_SHOT);
    if (tr.fraction < 1.0f) {
        VectorMA(bolt->s.origin, -10, dir, bolt->s.origin);
        bolt->touch(bolt, tr.ent, NULL, NULL);
    }
}


/*
=================
fire_grenade
=================
*/
void Grenade_Explode(edict_t *ent)
{
    vec3_t      origin;
    int         mod;

    if (ent->owner->client)
        PlayerNoise(ent->owner, ent->s.origin, PNOISE_IMPACT);

    //FIXME: if we are onground then raise our Z just a bit since we are a point?
    if (ent->enemy) {
        float   points;
        vec3_t  v;
        vec3_t  dir;

        VectorAdd(ent->enemy->mins, ent->enemy->maxs, v);
        VectorMA(ent->enemy->s.origin, 0.5f, v, v);
        VectorSubtract(ent->s.origin, v, v);
        points = ent->dmg - 0.5f * VectorLength(v);
        VectorSubtract(ent->enemy->s.origin, ent->s.origin, dir);
        if (ent->spawnflags & 1)
            mod = MOD_HANDGRENADE;
        else
            mod = MOD_GRENADE;
        T_Damage(ent->enemy, ent, ent->owner, dir, ent->s.origin, vec3_origin, (int)points, (int)points, DAMAGE_RADIUS, mod);
    }

    if (ent->spawnflags & 2)
        mod = MOD_HELD_GRENADE;
    else if (ent->spawnflags & 1)
        mod = MOD_HG_SPLASH;
    else
        mod = MOD_G_SPLASH;
    T_RadiusDamage(ent, ent->owner, ent->dmg, ent->enemy, ent->dmg_radius, mod);

    VectorMA(ent->s.origin, -0.02f, ent->velocity, origin);
    gi.WriteUint8(svc_temp_entity);
    if (ent->waterlevel) {
        if (ent->groundentity)
            gi.WriteUint8(TE_GRENADE_EXPLOSION_WATER);
        else
            gi.WriteUint8(TE_ROCKET_EXPLOSION_WATER);
    } else {
        if (ent->groundentity)
            gi.WriteUint8(TE_GRENADE_EXPLOSION);
        else
            gi.WriteUint8(TE_ROCKET_EXPLOSION);
    }
    gi.WritePosition( origin, false );
    gi.multicast( ent->s.origin, MULTICAST_PHS, false );

    G_FreeEdict(ent);
}

void Grenade_Touch(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    if (other == ent->owner)
        return;

    if (surf && (surf->flags & SURF_SKY)) {
        G_FreeEdict(ent);
        return;
    }

    if (!other->takedamage) {
        if (ent->spawnflags & 1) {
            if (random() > 0.5f)
                gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/hgrenb1a.wav"), 1, ATTN_NORM, 0);
            else
                gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/hgrenb2a.wav"), 1, ATTN_NORM, 0);
        } else {
            gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/grenlb1b.wav"), 1, ATTN_NORM, 0);
        }
        return;
    }

    ent->enemy = other;
    Grenade_Explode(ent);
}

void fire_grenade(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, sg_time_t timer, float damage_radius)
{
    edict_t *grenade;
    vec3_t  dir;
    vec3_t  forward, right, up;
    float   scale;

    QM_Vector3ToAngles(aimdir, dir);
    AngleVectors(dir, forward, right, up);

    grenade = G_AllocateEdict();
    VectorCopy(start, grenade->s.origin);
    VectorScale(aimdir, speed, grenade->velocity);
    scale = 200 + crandom() * 10.0f;
    VectorMA(grenade->velocity, scale, up, grenade->velocity);
    scale = crandom() * 10.0f;
    VectorMA(grenade->velocity, scale, right, grenade->velocity);
    VectorSet(grenade->avelocity, 300, 300, 300);
    grenade->movetype = MOVETYPE_BOUNCE;
    grenade->clipmask = MASK_PROJECTILE;
    if ( self->client && G_ShouldPlayersCollideProjectile( self ) ) {
        grenade->clipmask = static_cast<contents_t>( grenade->clipmask & ~CONTENTS_PLAYER );
    }
    grenade->flags = static_cast<ent_flags_t>( grenade->flags | FL_DODGE /*| FL_TRAP */);
    grenade->svflags |= SVF_PROJECTILE;
    grenade->solid = SOLID_BOUNDS_BOX;
    grenade->s.effects |= EF_GRENADE;
    VectorClear(grenade->mins);
    VectorClear(grenade->maxs);
    grenade->s.modelindex = gi.modelindex("models/objects/grenade/tris.md2");
    grenade->owner = self;
    grenade->touch = Grenade_Touch;
	grenade->nextthink = level.time + timer;
    grenade->think = Grenade_Explode;
    grenade->dmg = damage;
    grenade->dmg_radius = damage_radius;
    grenade->classname = "grenade";

    gi.linkentity(grenade);
}

void fire_grenade2(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, sg_time_t timer, float damage_radius, bool held)
{
    edict_t *grenade;
    vec3_t  dir;
    vec3_t  forward, right, up;
    float   scale;

    QM_Vector3ToAngles(aimdir, dir);
    AngleVectors(dir, forward, right, up);

    grenade = G_AllocateEdict();
    VectorCopy(start, grenade->s.origin);
    VectorScale(aimdir, speed, grenade->velocity);
    scale = 200 + crandom() * 10.0f;
    VectorMA(grenade->velocity, scale, up, grenade->velocity);
    scale = crandom() * 10.0f;
    VectorMA(grenade->velocity, scale, right, grenade->velocity);
    VectorSet(grenade->avelocity, 300, 300, 300);
    grenade->movetype = MOVETYPE_BOUNCE;
    grenade->clipmask = MASK_PROJECTILE;
    if ( self->client && G_ShouldPlayersCollideProjectile( self ) ) {
        grenade->clipmask = static_cast<contents_t>( grenade->clipmask & ~CONTENTS_PLAYER );
    }
    grenade->flags = static_cast<ent_flags_t>( grenade->flags | FL_DODGE /*| FL_TRAP */ );
    grenade->svflags |= SVF_PROJECTILE;
    grenade->solid = SOLID_BOUNDS_BOX;
    grenade->s.effects |= EF_GRENADE;
    VectorClear(grenade->mins);
    VectorClear(grenade->maxs);
    grenade->s.modelindex = gi.modelindex("models/objects/grenade2/tris.md2");
    grenade->owner = self;
    grenade->touch = Grenade_Touch;
	grenade->nextthink = level.time + timer;
    grenade->think = Grenade_Explode;
    grenade->dmg = damage;
    grenade->dmg_radius = damage_radius;
    grenade->classname = "hgrenade";
    if (held)
        grenade->spawnflags = 3;
    else
        grenade->spawnflags = 1;
    grenade->s.sound = gi.soundindex("weapons/hgrenc1b.wav");

    if (timer <= 0_ms)
        Grenade_Explode(grenade);
    else {
        gi.sound(self, CHAN_WEAPON, gi.soundindex("weapons/hgrent1a.wav"), 1, ATTN_NORM, 0);
        gi.linkentity(grenade);
    }
}


/*
=================
fire_rocket
=================
*/
void rocket_touch(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    vec3_t      origin;
    int         n;

    if (other == ent->owner)
        return;

    if (surf && (surf->flags & SURF_SKY)) {
        G_FreeEdict(ent);
        return;
    }

    if (ent->owner->client)
        PlayerNoise(ent->owner, ent->s.origin, PNOISE_IMPACT);

    // calculate position for the explosion entity
    VectorMA(ent->s.origin, -0.02f, ent->velocity, origin);

    if (other->takedamage) {
        T_Damage(other, ent, ent->owner, ent->velocity, ent->s.origin, plane->normal, ent->dmg, 0, 0, MOD_ROCKET);
    } else {
        // don't throw any debris in net games
        if (!deathmatch->value && !coop->value) {
            if ((surf) && !(surf->flags & (SURF_WARP | SURF_TRANS33 | SURF_TRANS66 | SURF_FLOWING))) {
                n = Q_rand() % 5;
                while (n--)
                    ThrowDebris(ent, "models/objects/debris2/tris.md2", 2, ent->s.origin);
            }
        }
    }

    T_RadiusDamage(ent, ent->owner, ent->radius_dmg, other, ent->dmg_radius, MOD_R_SPLASH);

    gi.WriteUint8(svc_temp_entity);
    if (ent->waterlevel)
        gi.WriteUint8(TE_ROCKET_EXPLOSION_WATER);
    else
        gi.WriteUint8(TE_ROCKET_EXPLOSION);
    gi.WritePosition( origin, false );
    gi.multicast( ent->s.origin, MULTICAST_PHS, false );

    G_FreeEdict(ent);
}

void fire_rocket(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, float damage_radius, int radius_damage)
{
    edict_t *rocket;

    rocket = G_AllocateEdict();
    VectorCopy(start, rocket->s.origin);
    VectorCopy(dir, rocket->movedir);
    QM_Vector3ToAngles(dir, rocket->s.angles);
    VectorScale(dir, speed, rocket->velocity);
    rocket->movetype = MOVETYPE_FLYMISSILE;
    rocket->clipmask = MASK_PROJECTILE;
    if ( self->client && G_ShouldPlayersCollideProjectile( self ) ) {
        rocket->clipmask = static_cast<contents_t>( rocket->clipmask & ~CONTENTS_PLAYER );
    }
    rocket->flags = static_cast<ent_flags_t>( rocket->flags | FL_DODGE /*| FL_TRAP */ );
    rocket->svflags |= SVF_PROJECTILE;
    rocket->solid = SOLID_BOUNDS_BOX;
    rocket->s.effects |= EF_ROCKET;
    VectorClear(rocket->mins);
    VectorClear(rocket->maxs);
    rocket->s.modelindex = gi.modelindex("models/objects/rocket/tris.md2");
    rocket->owner = self;
    rocket->touch = rocket_touch;
	rocket->nextthink = level.time + sg_time_t::from_sec( 8000.f / speed );
    rocket->think = G_FreeEdict;
    rocket->dmg = damage;
    rocket->radius_dmg = radius_damage;
    rocket->dmg_radius = damage_radius;
    rocket->s.sound = gi.soundindex("weapons/rockfly.wav");
    rocket->classname = "rocket";

    if (self->client)
        check_dodge(self, rocket->s.origin, dir, speed);

    gi.linkentity(rocket);
}


/*
=================
fire_rail
=================
*/
void fire_rail(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick)
{
    vec3_t      from;
    vec3_t      end;
    trace_t     tr;
    edict_t     *ignore;
    contents_t  mask;
    bool        water;
    float       lastfrac;

    VectorMA(start, 8192, aimdir, end);
    VectorCopy(start, from);
    ignore = self;
    water = false;
    mask = static_cast<contents_t>( MASK_SHOT | CONTENTS_SLIME | CONTENTS_LAVA );
    lastfrac = 1;
    while (ignore) {
        tr = gi.trace(from, NULL, NULL, end, ignore, mask);

        if (tr.contents & (CONTENTS_SLIME | CONTENTS_LAVA)) {
            mask = static_cast<contents_t>( ~( CONTENTS_SLIME | CONTENTS_LAVA ) );
            water = true;
        } else {
            //ZOID--added so rail goes through SOLID_BOUNDS_BOX entities (gibs, etc)
            if (((tr.ent->svflags & SVF_MONSTER) || (tr.ent->client) ||
                (tr.ent->solid == SOLID_BOUNDS_BOX)) && (lastfrac + tr.fraction > 0))
                ignore = tr.ent;
            else
                ignore = NULL;

            if ((tr.ent != self) && (tr.ent->takedamage))
                T_Damage(tr.ent, self, self, aimdir, tr.endpos, tr.plane.normal, damage, kick, 0, MOD_RAILGUN);
        }

        VectorCopy(tr.endpos, from);
        lastfrac = tr.fraction;
    }

    // send gun puff / flash
    gi.WriteUint8(svc_temp_entity);
    gi.WriteUint8(TE_RAILTRAIL);
    gi.WritePosition( start, false );
    gi.WritePosition( tr.endpos, false );
    gi.multicast( self->s.origin, MULTICAST_PHS, false );
//  gi.multicast (start, MULTICAST_PHS);
    if (water) {
        gi.WriteUint8(svc_temp_entity);
        gi.WriteUint8(TE_RAILTRAIL);
        gi.WritePosition( start, false );
        gi.WritePosition( tr.endpos, false );
        gi.multicast( tr.endpos, MULTICAST_PHS, false );
    }

    if (self->client)
        PlayerNoise(self, tr.endpos, PNOISE_IMPACT);
}


/*
=================
fire_bfg
=================
*/
void bfg_explode(edict_t *self)
{
    edict_t *ent;
    float   points;
    vec3_t  v;
    float   dist;

    if (self->s.frame == 0) {
        // the BFG effect
        ent = NULL;
        while ((ent = findradius(ent, self->s.origin, self->dmg_radius)) != NULL) {
            if (!ent->takedamage)
                continue;
            if (ent == self->owner)
                continue;
            if (!CanDamage(ent, self))
                continue;
            if (!CanDamage(ent, self->owner))
                continue;

            VectorAdd(ent->mins, ent->maxs, v);
            VectorMA(ent->s.origin, 0.5f, v, v);
            VectorSubtract(self->s.origin, v, v);
            dist = VectorLength(v);
            points = self->radius_dmg * (1.0f - sqrtf(dist / self->dmg_radius));
            if (ent == self->owner)
                points = points * 0.5f;

            gi.WriteUint8(svc_temp_entity);
            gi.WriteUint8(TE_BFG_EXPLOSION);
            gi.WritePosition( ent->s.origin, false );
            gi.multicast( ent->s.origin, MULTICAST_PHS, false );
            T_Damage(ent, self, self->owner, self->velocity, ent->s.origin, vec3_origin, (int)points, 0, DAMAGE_ENERGY, MOD_BFG_EFFECT);
        }
    }

	self->nextthink = level.time + 10_hz;
    self->s.frame++;
    if (self->s.frame == 5)
        self->think = G_FreeEdict;
}

void bfg_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    if (other == self->owner)
        return;

    if (surf && (surf->flags & SURF_SKY)) {
        G_FreeEdict(self);
        return;
    }

    if (self->owner->client)
        PlayerNoise(self->owner, self->s.origin, PNOISE_IMPACT);

    // core explosion - prevents firing it into the wall/floor
    if (other->takedamage)
        T_Damage(other, self, self->owner, self->velocity, self->s.origin, plane->normal, 200, 0, 0, MOD_BFG_BLAST);
    T_RadiusDamage(self, self->owner, 200, other, 100, MOD_BFG_BLAST);

    gi.sound(self, CHAN_VOICE, gi.soundindex("weapons/bfg__x1b.wav"), 1, ATTN_NORM, 0);
    self->solid = SOLID_NOT;
    self->touch = NULL;
    VectorMA(self->s.origin, -1 * FRAMETIME, self->velocity, self->s.origin);
    VectorClear(self->velocity);
    self->s.modelindex = gi.modelindex("sprites/s_bfg3.sp2");
    self->s.frame = 0;
    self->s.sound = 0;
    self->s.effects &= ~EF_ANIM_ALLFAST;
    self->think = bfg_explode;
    self->nextthink = level.time + 10_hz;
    self->enemy = other;

    gi.WriteUint8(svc_temp_entity);
    gi.WriteUint8(TE_BFG_BIGEXPLOSION);
    gi.WritePosition( self->s.origin, false );
    gi.multicast( self->s.origin, MULTICAST_PVS, false );
}


void bfg_think(edict_t *self)
{
    edict_t *ent;
    edict_t *ignore;
    vec3_t  point;
    vec3_t  dir;
    vec3_t  start;
    vec3_t  end;
    int     dmg;
    trace_t tr;

    if (deathmatch->value)
        dmg = 5;
    else
        dmg = 10;

    ent = NULL;
    while ((ent = findradius(ent, self->s.origin, 256)) != NULL) {
        if (ent == self)
            continue;

        if (ent == self->owner)
            continue;

        if (!ent->takedamage)
            continue;

        if (!(ent->svflags & SVF_MONSTER) && (!ent->client) && (strcmp(ent->classname, "misc_explobox") != 0))
            continue;

        VectorMA(ent->absmin, 0.5f, ent->size, point);

        VectorSubtract(point, self->s.origin, dir);
        VectorNormalize(dir);

        ignore = self;
        VectorCopy(self->s.origin, start);
        VectorMA(start, 2048, dir, end);
        while (1) {
            tr = gi.trace( start, NULL, NULL, end, ignore, static_cast<contents_t>( CONTENTS_SOLID | CONTENTS_MONSTER | CONTENTS_DEADMONSTER ) );

            if (!tr.ent)
                break;

            // hurt it if we can
            if ((tr.ent->takedamage) && !(tr.ent->flags & FL_IMMUNE_LASER) && (tr.ent != self->owner))
                T_Damage(tr.ent, self, self->owner, dir, tr.endpos, vec3_origin, dmg, 1, DAMAGE_ENERGY, MOD_BFG_LASER);

            // if we hit something that's not a monster or player we're done
            if (!(tr.ent->svflags & SVF_MONSTER) && (!tr.ent->client)) {
                gi.WriteUint8(svc_temp_entity);
                gi.WriteUint8(TE_LASER_SPARKS);
                gi.WriteUint8(4);
                gi.WritePosition( tr.endpos, false );
                gi.WriteDir8(tr.plane.normal);
                gi.WriteUint8(self->s.skinnum);
                gi.multicast( tr.endpos, MULTICAST_PVS, false );
                break;
            }

            ignore = tr.ent;
            VectorCopy(tr.endpos, start);
        }

        gi.WriteUint8(svc_temp_entity);
        gi.WriteUint8(TE_BFG_LASER);
        gi.WritePosition( self->s.origin, false );
        gi.WritePosition( tr.endpos, false );
        gi.multicast( self->s.origin, MULTICAST_PHS, false );
    }

    self->nextthink = level.time + 10_hz;
}


void fire_bfg(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, float damage_radius)
{
    edict_t *bfg;

    bfg = G_AllocateEdict();
    VectorCopy(start, bfg->s.origin);
    VectorCopy(dir, bfg->movedir);
    QM_Vector3ToAngles(dir, bfg->s.angles);
    VectorScale(dir, speed, bfg->velocity);
    bfg->movetype = MOVETYPE_FLYMISSILE;
    bfg->clipmask = MASK_PROJECTILE;
    bfg->svflags = SVF_PROJECTILE;
    // [Paril-KEX]
    if ( self->client && !G_ShouldPlayersCollideProjectile( self ) ) {
        bfg->clipmask = static_cast<contents_t>( bfg->clipmask & ~CONTENTS_PLAYER );
    }
    bfg->solid = SOLID_BOUNDS_BOX;
    bfg->s.effects |= EF_BFG | EF_ANIM_ALLFAST;
    VectorClear(bfg->mins);
    VectorClear(bfg->maxs);
    bfg->s.modelindex = gi.modelindex("sprites/s_bfg1.sp2");
    bfg->owner = self;
    bfg->touch = bfg_touch;
	bfg->nextthink = level.time + sg_time_t::from_sec( 8000.f / speed );
    bfg->think = G_FreeEdict;
    bfg->radius_dmg = damage;
    bfg->dmg_radius = damage_radius;
    bfg->classname = "bfg blast";
    bfg->s.sound = gi.soundindex("weapons/bfg__l1a.wav");

    bfg->think = bfg_think;
    bfg->nextthink = level.time + FRAME_TIME_S;
    bfg->teammaster = bfg;
    bfg->teamchain = NULL;

    if (self->client)
        check_dodge(self, bfg->s.origin, dir, speed);

    gi.linkentity(bfg);
}

/*
 * Drops a spark from the flare flying thru the air.  Checks to make
 * sure we aren't in the water.
 */
void flare_sparks(edict_t *self)
{
	vec3_t dir;
	vec3_t forward, right, up;
	// Spawn some sparks.  This isn't net-friendly at all, but will 
	// be fine for single player. 
	// 
	gi.WriteUint8(svc_temp_entity);
	gi.WriteUint8(TE_FLARE);

    gi.WriteInt16((int)(self - g_edicts));
    // if this is the first tick of flare, set count to 1 to start the sound
	// WID: sg_time_t: WARNING: Did we do this properly?
    gi.WriteUint8( (self->timestamp - level.time) < 14.75_sec ? 0 : 1 );

    gi.WritePosition( self->s.origin, false );

	// If we are still moving, calculate the normal to the direction 
	 // we are travelling. 
	 // 
	if (VectorLength(self->velocity) > 0.0)
	{
		QM_Vector3ToAngles(self->velocity, dir);
		AngleVectors(dir, forward, right, up);

		gi.WriteDir8(up);
	}
	// If we're stopped, just write out the origin as our normal 
	// 
	else
	{
		gi.WriteDir8(vec3_origin);
	}
	gi.multicast( self->s.origin, MULTICAST_PVS, false );
}

/*
   void flare_think( edict_t *self )

   Purpose: The think function of a flare round.  It generates sparks
			on the flare using a temp entity, and kills itself after
			self->timestamp runs out.
   Parameters:
	 self: A pointer to the edict_t structure representing the
		   flare round.  self->timestamp is the value used to
		   measure the lifespan of the round, and is set in
		   fire_flaregun blow.

   Notes:
	 - I'm not sure how much bandwidth is eaten by spawning a temp
	   entity every FRAMETIME seconds.  It might very well turn out
	   that the sparks need to go bye-bye in favor of less bandwidth
	   usage.  Then again, why the hell would you use this gun on
	   a DM server????

	 - I haven't seen self->timestamp used anywhere else in the code,
	   but I never really looked that hard.  It doesn't seem to cause
	   any problems, and is aptly named, so I used it.
 */
void flare_think(edict_t *self)
{
	// self->timestamp is 15 seconds after the flare was spawned. 
	// 
	if (level.time > self->timestamp)
	{
		G_FreeEdict(self);
		return;
	}

	// We're still active, so lets shoot some sparks. 
	// 
	flare_sparks(self);
	
	// We'll think again in .2 seconds 
	// 
	self->nextthink = level.time + sg_time_t::from_sec(.2f);
}

void flare_touch(edict_t *ent, edict_t *other,
	cplane_t *plane, csurface_t *surf)
{
	// Flares don't weigh that much, so let's have them stop 
	// the instant they whack into anything. 
	// 
	VectorClear(ent->velocity);
}

void fire_flaregun(edict_t *self, vec3_t start, vec3_t aimdir,
	int damage, int speed, float timer,
	float damage_radius)
{
	edict_t *flare;
	vec3_t dir;
	vec3_t forward, right, up;

	QM_Vector3ToAngles(aimdir, dir);
	AngleVectors(dir, forward, right, up);

	flare = G_AllocateEdict();
	VectorCopy(start, flare->s.origin);
	VectorScale(aimdir, speed, flare->velocity);
	VectorSet(flare->avelocity, 300, 300, 300);
	flare->movetype = MOVETYPE_BOUNCE;
	flare->clipmask = MASK_SHOT;
	flare->solid = SOLID_BOUNDS_BOX;

	const float size = 4;
	VectorSet(flare->mins, -size, -size, -size);
	VectorSet(flare->maxs, size, size, size);

	flare->s.modelindex = gi.modelindex("models/objects/flare/tris.md2");
	flare->owner = self;
	flare->touch = flare_touch;
	flare->nextthink = level.time + .2_sec;
	flare->think = flare_think;
	flare->radius_dmg = damage;
	flare->dmg_radius = damage_radius;
	flare->classname = "flare";
	flare->timestamp = level.time + 15_sec; //live for 15 seconds 
	gi.linkentity(flare);
}