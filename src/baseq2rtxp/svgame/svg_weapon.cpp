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
#include "svg_local.h"


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
    // WID: TODO: Monster Reimplement.
    //vec3_t  end;
    //vec3_t  v;
    //trace_t tr;

    //// easy mode only ducks one quarter the time
    //if (skill->value == 0) {
    //    if (random() > 0.25f)
    //        return;
    //}

    //VectorMA(start, CM_MAX_WORLD_SIZE, dir, end);
    //tr = gi.trace(start, NULL, NULL, end, self, MASK_SHOT);
    //if ((tr.ent) && (tr.ent->svflags & SVF_MONSTER) && (tr.ent->health > 0) && (tr.ent->monsterinfo.dodge) && infront(tr.ent, self)) {
    //    VectorSubtract(tr.endpos, start, v);
    //    sg_time_t eta = sg_time_t::from_sec(VectorLength(v) - tr.ent->maxs[0]) / speed;
    //    tr.ent->monsterinfo.dodge(tr.ent, self, eta.seconds() );
    //}
}

/**
*   @brief  Projects the muzzleflash destination origin, then performs a trace clipping it to any entity/brushes that are in its way.
*   @return Clipped muzzleflash destination origin.
**/
const Vector3 SVG_MuzzleFlash_ProjectAndTraceToPoint( edict_t *ent, const Vector3 &muzzleFlashOffset, const Vector3 &forward, const Vector3 &right ) {
    // Project from source to muzzleflash destination.
    Vector3 muzzleFlashOrigin = {};
    muzzleFlashOrigin = SVG_Player_ProjectDistance( ent, ent->s.origin, muzzleFlashOffset, forward, right );

    // To stop it accidentally spawning the MZ_PISTOL muzzleflash inside of entities and/or (wall-)brushes,
    // peform a trace from our origin on to the projected start.
    trace_t tr = gi.trace( ent->s.origin, NULL, NULL, &muzzleFlashOrigin.x, ent, MASK_SHOT );
    // We hit something, clip to trace endpos so the muzzleflash will play in our non-solid area:
    if ( tr.fraction < 1.0 ) {
        muzzleFlashOrigin = tr.endpos;
    }

    // Return the final result.
    return muzzleFlashOrigin;
}
/*
=================
fire_hit

Used for all impact (hit/punch/slash) attacks
=================
*/
#if 0
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
    SVG_TriggerDamage(tr.ent, self, self, dir, point, vec3_origin, damage, kick / 2, DAMAGE_NO_KNOCKBACK, MEANS_OF_DEATH_HIT_FIGHTING );

    if (!(tr.ent->svflags & SVF_MONSTER) && (!tr.ent->client))
        return false;

    // do our special form of knockback here
    VectorMA(self->enemy->absmin, 0.5f, self->enemy->size, v);
    VectorSubtract(v, point, v);
    VectorNormalize(v);
    VectorMA(self->enemy->velocity, kick, v, self->enemy->velocity);
    if (self->enemy->velocity[2] > 0)
        self->enemy->groundInfo.entity = NULL;
    return true;
}
#endif

/**
*   @brief  Used for all impact (fighting kick/punch) attacks
**/
const bool fire_hit_punch_impact( edict_t *self, const Vector3 &start, const Vector3 &aimDir, const int32_t damage, const int32_t kick ) {
    static constexpr float HIT_RANGE = 32;
    trace_t     tr = {};
    vec3_t      dir = {};
    vec3_t      forward = {}, right = {}, up = {};
    vec3_t      end = {};

    float       r = 0;
    float       u = 0;
    bool        water = false;
    contents_t  content_mask = ( MASK_SHOT );

    ////see if enemy is in range
    //VectorSubtract( self->enemy->s.origin, self->s.origin, dir );
    //range = VectorLength( dir );
    //if ( range > aim[ 0 ] )
    //    return false;

    //if ( aim[ 1 ] > self->mins[ 0 ] && aim[ 1 ] < self->maxs[ 0 ] ) {
    //    // the hit is straight on so back the range up to the edge of their bbox
    //    range -= self->enemy->maxs[ 0 ];
    //} else {
    //    // this is a side hit so adjust the "right" value out to the edge of their bbox
    //    if ( aim[ 1 ] < 0 )
    //        aim[ 1 ] = self->enemy->mins[ 0 ];
    //    else
    //        aim[ 1 ] = self->enemy->maxs[ 0 ];
    //}


    tr = gi.trace( self->s.origin, NULL, NULL, &start.x, self, MASK_SHOT );
    if ( !( tr.fraction < 1.0f ) ) {
        QM_Vector3ToAngles( aimDir, dir );
        AngleVectors( dir, forward, right, up );

        r = 1.0f; //r = crandom() * hspread;
        u = 1.0f; //u = crandom() * vspread;
        VectorMA( start, HIT_RANGE, forward, end );
        VectorMA( end, r, right, end );
        VectorMA( end, u, up, end );

        tr = gi.trace( &start.x, NULL, NULL, end, self, content_mask );
    }

    bool isTDamaged = false;
    // Make sure we aren't hitting a sky brush.
    if ( !(tr.surface && tr.surface->flags & SURF_SKY) ) {
        // We hit something.
        if ( tr.fraction < 1.0f ) {
            // It was an entity, if it takes damage, hit it:
            if ( tr.ent && tr.ent->takedamage >= DAMAGE_YES ) {
                SVG_TriggerDamage( tr.ent, self, self, &aimDir.x, tr.endpos, tr.plane.normal, damage, kick, DAMAGE_NONE, MEANS_OF_DEATH_HIT_FIGHTING );
                isTDamaged = true;
            // Otherwise, display something that shows we are hitting something senselessly.
            } else {
                if ( strncmp( tr.surface->name, "sky", 3 ) != 0 ) {
                    gi.WriteUint8( svc_temp_entity );
                    gi.WriteUint8( TE_BLOOD );
                    gi.WritePosition( tr.endpos, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
                    gi.WriteDir8( tr.plane.normal );
                    gi.multicast( tr.endpos, MULTICAST_PVS, false );

                    if ( self->client )
                        SVG_Player_PlayerNoise( self, tr.endpos, PNOISE_IMPACT );
                }
            }

        }
    }

    //gi.WriteUint8( svc_temp_entity );
    //gi.WriteUint8( TE_DEBUGTRAIL );
    //gi.WritePosition( start, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
    //gi.WritePosition( tr.endpos, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
    //gi.multicast( start, MULTICAST_PVS, false );

    if ( !( tr.ent ) || ( tr.ent != &g_edicts[ 0 ] ) || ( !( tr.ent->svflags & SVF_MONSTER ) && ( !tr.ent->client ) ) ) {
        //gi.dprintf( "%s: no monster flag set for '%s' ?!\n", __func__, tr.ent->classname );
        return false;
    }

    vec3_t      v, point;

    if ( tr.ent != nullptr && ( tr.ent != &g_edicts[ 0 ] ) ) {
        // Do our special form of knockback here
        VectorMA( tr.ent->absmin, 0.5f, tr.ent->size, v );
        VectorSubtract( v, point, v );
        VectorNormalize( v );
        VectorMA( tr.ent->velocity, kick, v, tr.ent->velocity );
        if ( tr.ent->velocity[ 2 ] > 0 )
            tr.ent->groundInfo.entity = NULL;
    }

    return isTDamaged;
}


/**
*   @brief  This is an internal support routine used for bullet/pellet based weapons.
**/
static void fire_lead(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int te_impact, int hspread, int vspread, const sg_means_of_death_t meansOfDeath ) {
    trace_t     tr = {};
    vec3_t      dir = {};
    vec3_t      forward = {}, right = {}, up = {};
    vec3_t      end = {};
    float       r = 0;
    float       u = 0;
    vec3_t      water_start = {};
    bool        water = false;
    contents_t  content_mask = ( MASK_SHOT | MASK_WATER );

    tr = gi.trace(self->s.origin, NULL, NULL, start, self, MASK_SHOT);
    if (!(tr.fraction < 1.0f)) {
        QM_Vector3ToAngles(aimdir, dir);
        AngleVectors(dir, forward, right, up);

        r = crandom() * hspread;
        u = crandom() * vspread;
        VectorMA(start, CM_MAX_WORLD_SIZE, forward, end);
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
                    gi.WritePosition( tr.endpos, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
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
                VectorMA(water_start, CM_MAX_WORLD_SIZE, forward, end);
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
                SVG_TriggerDamage(tr.ent, self, self, aimdir, tr.endpos, tr.plane.normal, damage, kick, DAMAGE_BULLET, meansOfDeath );
            } else {
                if (strncmp(tr.surface->name, "sky", 3) != 0) {
                    gi.WriteUint8(svc_temp_entity);
                    gi.WriteUint8(te_impact);
                    gi.WritePosition( tr.endpos, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
                    gi.WriteDir8(tr.plane.normal);
                    gi.multicast( tr.endpos, MULTICAST_PVS, false );

                    if (self->client)
                        SVG_Player_PlayerNoise(self, tr.endpos, PNOISE_IMPACT);
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
        gi.WritePosition( water_start, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
        gi.WritePosition( tr.endpos, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
        gi.multicast( pos, MULTICAST_PVS, false );
    }
}

/**
*   @brief  Fires a single round. Used for machinegun and chaingun.  Would be fine for
*           pistols, rifles, etc....
**/
void fire_bullet(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, const sg_means_of_death_t meansOfDeath ) {
    fire_lead(self, start, aimdir, damage, kick, TE_GUNSHOT, hspread, vspread, meansOfDeath );
}

/**
*   @brief  Shoots shotgun pellets.  Used by shotgun and super shotgun.
**/
void fire_shotgun(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, int count, const sg_means_of_death_t meansOfDeath ) {
    int     i;

    for (i = 0; i < count; i++)
        fire_lead( self, start, aimdir, damage, kick, TE_GUNSHOT, hspread, vspread, meansOfDeath );
}

//static const bool SVG_ShouldPlayersCollideProjectile( edict_t *self ) {
//    // In Coop they don't.
//    if ( SG_GetActiveGameModeType() == GAMEMODE_TYPE_COOPERATIVE ) {
//        return false;
//    }
//
//    // Otherwise they do.
//    return true;
//}