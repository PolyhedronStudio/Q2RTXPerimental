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
#include "svgame/svg_entity_events.h"
#include "svgame/svg_utils.h"

#include "sharedgame/sg_means_of_death.h"
#include "sharedgame/sg_tempentity_events.h"

/*
=================
check_dodge

This is a support routine used when a client is firing
a non-instant attack weapon.  It checks to see if a
monster's dodge function should be called.
=================
*/
static void check_dodge(svg_base_edict_t *self, vec3_t start, vec3_t dir, int speed)
{
    // WID: TODO: Monster Reimplement.
    //vec3_t  end;
    //vec3_t  v;
    //svg_trace_t tr;

    //// easy mode only ducks one quarter the time
    //if (skill->value == 0) {
    //    if (random() > 0.25f)
    //        return;
    //}

    //VectorMA(start, CM_MAX_WORLD_SIZE, dir, end);
    //tr = SVG_Trace(start, NULL, NULL, end, self, CM_CONTENTMASK_SHOT);
    //if ((tr.ent) && (tr.ent->svFlags & SVF_MONSTER) && (tr.ent->health > 0) && (tr.ent->monsterinfo.dodge) && infront(tr.ent, self)) {
    //    VectorSubtract(tr.endpos, start, v);
    //    QMTime eta = QMTime::FromMilliseconds(VectorLength(v) - tr.ent->maxs[0]) / speed;
    //    tr.ent->monsterinfo.dodge(tr.ent, self, eta.seconds() );
    //}
}

/**
*   @brief  Projects the muzzleflash destination origin, then performs a trace clipping it to any entity/brushes that are in its way.
*   @return Clipped muzzleflash destination origin.
**/
const Vector3 SVG_MuzzleFlash_ProjectAndTraceToPoint( svg_base_edict_t *ent, const Vector3 &muzzleFlashOffset, const Vector3 &forward, const Vector3 &right ) {
    // Project from source to muzzleflash destination.
    Vector3 muzzleFlashOrigin = {};
    muzzleFlashOrigin = SVG_Player_ProjectDistance( ent, ent->s.origin, QM_Vector3Zero(), forward, right);

    // To stop it accidentally spawning the MZ_PISTOL muzzleflash inside of entities and/or (wall-)brushes,
    // peform a trace from our origin on to the projected start.
    svg_trace_t tr = SVG_Trace( ent->s.origin, qm_vector3_null, qm_vector3_null, &muzzleFlashOrigin.x, ent, CM_CONTENTMASK_SHOT );
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
bool fire_hit(svg_base_edict_t *self, vec3_t aim, int damage, int kick)
{
    svg_trace_t     tr;
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

    tr = SVG_Trace(self->s.origin, NULL, NULL, point, self, CM_CONTENTMASK_SHOT);
    if (tr.fraction < 1) {
        if (!tr.ent->takedamage)
            return false;
        // if it will hit any client/monster then hit the one we wanted to hit
        if ((tr.ent->svFlags & SVF_MONSTER) || (tr.ent->client))
            tr.ent = self->enemy;
    }

    AngleVectors(self->s.angles, forward, right, up);
    VectorMA(self->s.origin, range, forward, point);
    VectorMA(point, aim[1], right, point);
    VectorMA(point, aim[2], up, point);
    VectorSubtract(point, self->enemy->s.origin, dir);

    // do the damage
    SVG_DamageEntity(tr.ent, self, self, dir, point, vec3_origin, damage, kick / 2, DAMAGE_NO_KNOCKBACK, MEANS_OF_DEATH_HIT_FIGHTING );

    if (!(tr.ent->svFlags & SVF_MONSTER) && (!tr.ent->client))
        return false;

    // do our special form of knockback here
    VectorMA(self->enemy->absMin, 0.5f, self->enemy->size, v);
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
const bool fire_hit_punch_impact( svg_base_edict_t *self, const Vector3 &start, const Vector3 &aimDir, const int32_t damage, const int32_t kick ) {
    static constexpr float HIT_RANGE = 32;
    svg_trace_t     tr = {};
    vec3_t      dir = {};
    vec3_t      forward = {}, right = {}, up = {};
    vec3_t      end = {};

    float       r = 0;
    float       u = 0;
    bool        water = false;
    cm_contents_t  content_mask = ( CM_CONTENTMASK_SHOT );

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


    tr = SVG_Trace( self->s.origin, qm_vector3_null, qm_vector3_null, &start.x, self, CM_CONTENTMASK_SHOT );
    if ( !( tr.fraction < 1.0f ) ) {
        QM_Vector3ToAngles( aimDir, dir );
        AngleVectors( dir, forward, right, up );

        r = 1.0f; //r = crandom() * hspread;
        u = 1.0f; //u = crandom() * vspread;
        VectorMA( start, HIT_RANGE, forward, end );
        VectorMA( end, r, right, end );
        VectorMA( end, u, up, end );

        tr = SVG_Trace( &start.x, qm_vector3_null, qm_vector3_null, end, self, content_mask);
    }

    bool isTDamaged = false;
    // Make sure we aren't hitting a sky brush.
    if ( !(tr.surface && tr.surface->flags & CM_SURFACE_FLAG_SKY) ) {
        // We hit something.
        if ( tr.fraction < 1.0f ) {
            // It was an entity, if it takes damage, hit it:
            if ( tr.ent && tr.ent->takedamage >= DAMAGE_YES ) {
                SVG_DamageEntity( tr.ent, self, self, &aimDir.x, tr.endpos, tr.plane.normal, damage, kick, DAMAGE_NONE, MEANS_OF_DEATH_HIT_FIGHTING );
                isTDamaged = true;
            // Otherwise, display something that shows we are hitting something senselessly.
            } else {
				//if ( ( tr.surface->flags & CM_SURFACE_FLAG_SKY ) != 0 ) {
					if ( damage <= 5 ) {
						Vector3 endPos = tr.endpos;
						endPos += Vector3( tr.plane.normal ) * 0.5f;
						SVG_TempEventEntity_Blood( endPos, tr.plane.normal, 14, 22 );
					} else {
						Vector3 endPos = tr.endpos;
						endPos += Vector3( tr.plane.normal ) * 0.5f;
						SVG_TempEventEntity_MoreBlood( endPos, tr.plane.normal, 8, 16 );
					}
     //               gi.WriteUint8( svc_temp_entity );
     //               gi.WriteUint8( TE_BLOOD );
     //               gi.WritePosition( &tr.endpos, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
					//const Vector3 planeNormal = tr.plane.normal;
     //               gi.WriteDir8( &planeNormal );
     //               gi.multicast( &tr.endpos, MULTICAST_PVS, false );

                    if ( self->client ) {
                        SVG_Player_PlayerNoise( self, tr.endpos, PNOISE_IMPACT );
                    }
                //}
            }

        }
    }

    //gi.WriteUint8( svc_temp_entity );
    //gi.WriteUint8( TE_DEBUGTRAIL );
    //gi.WritePosition( start, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
    //gi.WritePosition( tr.endpos, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
    //gi.multicast( start, MULTICAST_PVS, false );

    if ( !( tr.ent ) 
        || ( tr.ent != g_edict_pool.EdictForNumber( 0 ) /* worldspawn */ )
        || ( !(tr.ent->svFlags & SVF_MONSTER) && (!tr.ent->client) )) {
        //gi.dprintf( "%s: no monster flag set for '%s' ?!\n", __func__, tr.ent->classname );
        return false;
    }

    vec3_t      v, point;

    if ( tr.ent != nullptr && ( tr.ent != g_edict_pool.EdictForNumber( 0 ) /* worldspawn */ ) ) {
        // Do our special form of knockback here
        VectorMA( tr.ent->absMin, 0.5f, tr.ent->size, v );
        VectorSubtract( v, point, v );
        VectorNormalize( v );
        VectorMA( tr.ent->velocity, kick, v, tr.ent->velocity );
        if ( tr.ent->velocity[ 2 ] > 0 )
            tr.ent->groundInfo.entityNumber = ENTITYNUM_NONE;
    }

    return isTDamaged;
}


/**
*   @brief  This is an internal support routine used for bullet/pellet based weapons.
**/
static void fire_lead(svg_base_edict_t *self, const Vector3 &start, const Vector3 &aimdir, const float damage, const float kick, const int32_t te_impact, const float hspread, const float vspread, const sg_means_of_death_t meansOfDeath ) {
    Vector3 dir = { };
    Vector3 forward = {}, right = {}, up = {};
    Vector3 end = {};
    Vector3 water_start = {};
    bool    water = false;
    cm_contents_t  content_mask = ( CM_CONTENTMASK_SHOT | CM_CONTENTMASK_LIQUID );
    
	// Trace a line from the origin the supposed bullet shot start point.
    svg_trace_t tr = SVG_Trace(self->s.origin, qm_vector3_null, qm_vector3_null, start, self, CM_CONTENTMASK_SHOT);
	// If we hit something, and it is not sky, then we can continue.
    if ( !( tr.fraction < 1.0f ) ) {
        // Calculate the direction of the bullet.
        
        QM_Vector3ToAngles( aimdir, &dir.x );
		// Get the forward, right, and up vectors.
        QM_AngleVectors(dir, &forward, &right, &up);

		// Calculate the spread of the bullet.
        const float r = crandom_opend() * hspread; //frandom( -hspread, hspread );
        const float u = crandom_opend() * vspread; //frandom( -vspread, vspread );

		// Calculate the end point of the bullet.
        VectorMA(start, CM_MAX_WORLD_SIZE, forward, end);
        VectorMA(end, r, right, end);
        VectorMA(end, u, up, end);

        // Determine if we started from within a water brush.
        if ( gi.pointcontents(&start) & CM_CONTENTMASK_LIQUID ) {
			// We are in water.
            water = true;
			// Copy the start point into the water start point.
            VectorCopy(start, water_start);
			// Remove the water mask from the content mask.
            content_mask = static_cast<cm_contents_t>( content_mask & ~CM_CONTENTMASK_LIQUID ); // content_mask &= ~CM_CONTENTMASK_LIQUID
        }

		// Trace the bullet.
        tr = SVG_Trace(start, qm_vector3_null, qm_vector3_null, &end.x, self, content_mask);

        // See if we hit water.
        if ( tr.contents & CM_CONTENTMASK_LIQUID ) {
            // Splash type.
            sg_entity_events_t splashType = EV_FX_SPLASH_UNKNOWN;

            // We are in water.
            water = true;
            // Copy the start point into the water start point.
            VectorCopy( tr.endpos, water_start );

            // Determine the color of the splash.
            if ( tr.contents & CONTENTS_WATER ) {
                if ( strcmp( tr.surface->name, "*brwater" ) == 0 ) {
                    splashType = EV_FX_SPLASH_WATER_BROWN;
                } else {
                    splashType = EV_FX_SPLASH_WATER_BLUE;
                }
            } else if ( tr.contents & CONTENTS_SLIME ) {
                splashType = EV_FX_SPLASH_SLIME;
            } else if ( tr.contents & CONTENTS_LAVA ) {
                splashType = EV_FX_SPLASH_LAVA;
            }

            if ( splashType != EV_FX_SPLASH_UNKNOWN ) {
				SVG_TempEventEntity_SplashParticles( tr.endpos, tr.plane.normal, splashType, 8, 16 );
            }

            // If trace start != trace end pos.
            if ( !VectorCompare( start, tr.endpos ) ) {
                // Change bullet's course when it has entered enters water
                VectorSubtract( end, start, dir );
                QM_Vector3ToAngles( dir, &dir.x );
                QM_AngleVectors( dir, &forward, &right, &up );
                // Ensure to clamp the ranges properly.
                float hMinArg = std::min( -hspread * 2.0f, hspread * 2.0f );
                float hMaxArg = std::max( -hspread * 2.0f, hspread * 2.0f );
                float vMinArg = std::min( -vspread * 2.0f, vspread * 2.0f );
                float vMaxArg = std::max( -vspread * 2.0f, vspread * 2.0f );
                // Calculate the spread of the bullet.
                const float r = frandom( hMinArg, hMaxArg );
                const float u = frandom( vMinArg, vMaxArg );
                VectorMA( water_start, CM_MAX_WORLD_SIZE, forward, end );
                VectorMA( end, r, right, end );
                VectorMA( end, u, up, end );
            }

            // re-trace ignoring water this time
            tr = SVG_Trace( &water_start.x, qm_vector3_null, qm_vector3_null, &end.x, self, CM_CONTENTMASK_SHOT );
        }
    }

    // send gun puff / flash
    if ( !( ( tr.surface ) && ( tr.surface->flags & CM_SURFACE_FLAG_SKY ) ) ) {
        if ( tr.fraction < 1.0f ) {
            if ( tr.ent->takedamage ) {
                SVG_DamageEntity( tr.ent, self, self, aimdir, tr.endpos, tr.plane.normal, damage, kick, DAMAGE_BULLET, meansOfDeath );
            } else {
                //if ( strncmp( tr.surface->name, "sky", 3 ) != 0 ) {
				//if ( ( tr.surface->flags & CM_SURFACE_FLAG_SKY ) != 0 ) {
					SVG_TempEventEntity_GunShot( tr.endpos, tr.plane.normal, te_impact, 28, 40 );
					//gi.WriteUint8( svc_temp_entity );
                    //gi.WriteUint8( te_impact );
                    //gi.WritePosition( &tr.endpos, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
                    //const Vector3 planeNormal = tr.plane.normal;
                    //gi.WriteDir8( &planeNormal );
                    //gi.multicast( &tr.endpos, MULTICAST_PVS, false );

					if ( self->client ) {
						SVG_Player_PlayerNoise( self, tr.endpos, PNOISE_IMPACT );
					}
                //}
            }
        }
    }

    // if went through water, determine where the end and make a bubble trail
    if ( water ) {
        Vector3  pos;

        VectorSubtract( tr.endpos, water_start, dir );
        VectorNormalize( &dir.x );
        VectorMA( tr.endpos, -2, dir, pos );
        if ( gi.pointcontents( &pos ) & CM_CONTENTMASK_LIQUID )
            VectorCopy( pos, tr.endpos );
        else
            tr = SVG_Trace( pos, qm_vector3_null, qm_vector3_null, &water_start.x, tr.ent, CM_CONTENTMASK_LIQUID );

        VectorAdd( water_start, tr.endpos, pos );
        VectorScale( pos, 0.5f, pos );

		svg_base_edict_t *tempEventEntity = SVG_TempEventEntity_TrailParticles( water_start, tr.endpos, EV_FX_TRAIL_BUBBLES01 );

        //gi.WriteUint8( svc_temp_entity );
        //gi.WriteUint8( TE_BUBBLETRAIL );
        //gi.WritePosition( &water_start, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
        //gi.WritePosition( &tr.endpos, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
        //gi.multicast( &pos, MULTICAST_PVS, false );
    }
}

/**
*   @brief  Fires a single round. Used for machinegun and chaingun.  Would be fine for
*           pistols, rifles, etc....
**/
void fire_bullet(svg_base_edict_t *self, const vec3_t start, const vec3_t aimdir, const float damage, const float kick, const float hspread, const float vspread, const sg_means_of_death_t meansOfDeath ) {
    fire_lead(self, start, aimdir, damage, kick, EV_FX_IMPACT_GUNSHOT, hspread, vspread, meansOfDeath );
}

/**
*   @brief  Shoots shotgun pellets.  Used by shotgun and super shotgun.
**/
void fire_shotgun(svg_base_edict_t *self, const vec3_t start, const vec3_t aimdir, const float damage, const float kick, const float hspread, const float vspread, int count, const sg_means_of_death_t meansOfDeath ) {
    int     i;

    for (i = 0; i < count; i++)
        fire_lead( self, start, aimdir, damage, kick, EV_FX_IMPACT_GUNSHOT, hspread, vspread, meansOfDeath );
}

//static const bool SVG_ShouldPlayersCollideProjectile( svg_base_edict_t *self ) {
//    // In Coop they don't.
//    if ( SG_GetRequestedGameModeType() == GAMEMODE_TYPE_COOPERATIVE ) {
//        return false;
//    }
//
//    // Otherwise they do.
//    return true;
//}