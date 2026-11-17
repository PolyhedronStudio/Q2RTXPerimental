/********************************************************************
*
*
*	ClientGame: Used to be just 'effects.c', therefor, now named classic.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_effects.h"
#include "clgame/effects/clg_fx_classic.h"
#include "clgame/clg_world.h"
#include "clgame/decals/clg_decals.h"
#include "clgame/fx/clg_fx_blood_hero.h"

#include "sharedgame/sg_entity_flags.h"
#include "sharedgame/sg_muzzleflashes.h"

#include <algorithm>

// Wall Impact Puffs.
void CLG_FX_ParticleEffect( const Vector3 &org, const Vector3 &dir, int color, int count ) {
    vec3_t oy;
    VectorSet( oy, 0.0f, 1.0f, 0.0f );
    if ( fabsf( DotProduct( oy, dir ) ) > 0.95f )
        VectorSet( oy, 1.0f, 0.0f, 0.0f );

    vec3_t ox;
    CrossProduct( oy, dir, ox );

    count *= cl_particle_num_factor->value;
    const int spark_count = count / 10;

    const float dirt_horizontal_spread = 2.0f;
    const float dirt_vertical_spread = 1.0f;
    const float dirt_base_velocity = 40.0f;
    const float dirt_rand_velocity = 70.0f;

    const float spark_horizontal_spread = 1.0f;
    const float spark_vertical_spread = 1.0f;
    const float spark_base_velocity = 50.0f;
    const float spark_rand_velocity = 130.0f;

    for ( int i = 0; i < count; i++ ) {
        clg_particle_t *p = CLG_AllocateParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;

        p->color = color + ( Q_rand() & 7 );
        p->brightness = 0.5f;

        vec3_t origin;
        VectorCopy( org, origin );
        VectorMA( origin, dirt_horizontal_spread * crand(), ox, origin );
        VectorMA( origin, dirt_horizontal_spread * crand(), oy, origin );
        VectorMA( origin, dirt_vertical_spread * frand() + 1.0f, dir, origin );
        VectorCopy( origin, p->org );

        vec3_t velocity;
        VectorSubtract( origin, org, velocity );
        VectorNormalize( velocity );
        VectorScale( velocity, dirt_base_velocity + frand() * dirt_rand_velocity, p->vel );

        p->accel[ 0 ] = p->accel[ 1 ] = 0;
        p->accel[ 2 ] = -PARTICLE_GRAVITY;
        p->alpha = 1.0f;

        p->alphavel = -1.0f / ( 0.5f + frand() * 0.3f );
    }

    for ( int i = 0; i < spark_count; i++ ) {
        clg_particle_t *p = CLG_AllocateParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;

        p->color = 0xe0 + ( Q_rand() & 7 );
        p->brightness = cvar_pt_particle_emissive->value;

        vec3_t origin;
        VectorCopy( org, origin );
        VectorMA( origin, spark_horizontal_spread * crand(), ox, origin );
        VectorMA( origin, spark_horizontal_spread * crand(), oy, origin );
        VectorMA( origin, spark_vertical_spread * frand() + 1.0f, dir, origin );
        VectorCopy( origin, p->org );

        vec3_t velocity;
        VectorSubtract( origin, org, velocity );
        VectorNormalize( velocity );
        VectorScale( velocity, spark_base_velocity + powf( frand(), 2.0f ) * spark_rand_velocity, p->vel );

        p->accel[ 0 ] = p->accel[ 1 ] = 0;
        p->accel[ 2 ] = -PARTICLE_GRAVITY;
        p->alpha = 1.0f;

        p->alphavel = -2.0f / ( 0.5f + frand() * 0.3f );
    }
}

void CLG_FX_ParticleEffectWaterSplash( const Vector3 &org, const Vector3 &dir, int color, int count ) {
    vec3_t oy;
    VectorSet( oy, 0.0f, 1.0f, 0.0f );
    if ( fabsf( DotProduct( oy, dir ) ) > 0.95f )
        VectorSet( oy, 1.0f, 0.0f, 0.0f );

    vec3_t ox;
    CrossProduct( oy, dir, ox );

    count *= cl_particle_num_factor->value;

    const float water_horizontal_spread = 0.25f;
    const float water_vertical_spread = 1.0f;
    const float water_base_velocity = 80.0f;
    const float water_rand_velocity = 150.0f;

    for ( int i = 0; i < count; i++ ) {
        clg_particle_t *p = CLG_AllocateParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;

        p->color = color + ( Q_rand() & 7 );
        p->brightness = 1.0f;

        vec3_t origin;
        VectorCopy( org, origin );
        VectorMA( origin, water_horizontal_spread * crand(), ox, origin );
        VectorMA( origin, water_horizontal_spread * crand(), oy, origin );
        VectorMA( origin, water_vertical_spread * frand() + 1.0f, dir, origin );
        VectorCopy( origin, p->org );

        vec3_t velocity;
        VectorSubtract( origin, org, velocity );
        VectorNormalize( velocity );
        VectorScale( velocity, water_base_velocity + frand() * water_rand_velocity, p->vel );

        p->accel[ 0 ] = p->accel[ 1 ] = 0;
        p->accel[ 2 ] = -PARTICLE_GRAVITY;
        p->alpha = 1.0f;

        p->alphavel = -1.0f / ( 0.5f + frand() * 0.3f );
    }
}

/**
*	@brief	Spawns blood decals and 3D Alembic hero fluid meshes for flesh wound impacts.
*	@note	Legacy point-based particle cloud allocation is removed so that only the modern
*			3D Alembic fluid mesh and path-traced decals appear on monster and player hits.
*	@param	org		Origin of the blood effect.
*	@param	dir		Direction vector of the blood spray / impact force.
*	@param	color	Legacy base color palette index (unused).
*	@param	count	Severity of the wound / particle count factor.
**/
void CLG_FX_BloodParticleEffect( const Vector3 &org, const Vector3 &dir, [[maybe_unused]] int color, int count ) {
	/**
	*	Array of available blood splatter decal material hashes for random texture variation.
	**/
	static constexpr sg_decal_material_hash_t bloodMaterials[] = {
		SG_DECAL_MATERIAL_HASH_BLOOD_FLESH,
		SG_DECAL_MATERIAL_HASH_BLOOD_FLESH_02,
		SG_DECAL_MATERIAL_HASH_BLOOD_FLESH_03
	};

	/**
	*	1. Downward drip: cast a ray straight down from the wound towards the floor beneath the victim.
	**/
	const Vector3 floorTraceEnd = org + Vector3{ crand() * 6.0f, crand() * 6.0f, -140.0f };
	const cm_trace_t floorTrace = CLG_Clip( org, nullptr, nullptr, floorTraceEnd, nullptr, CM_CONTENTMASK_SOLID );
	const bool hitFloorSolid = ( floorTrace.fraction < 1.0f && !floorTrace.allsolid && !floorTrace.startsolid );
	const bool hitFloorSky = ( floorTrace.surface != nullptr && ( floorTrace.surface->flags & CM_SURFACE_FLAG_SKY ) != 0 );

	// Only spawn a floor drip if we struck upward-facing solid world geometry.
	if ( hitFloorSolid && !hitFloorSky && floorTrace.plane.normal[ 2 ] > 0.5f ) {
		sg_decal_spawn_params_t decal_params = {};
		VectorCopy( floorTrace.endpos, decal_params.origin );
		VectorCopy( floorTrace.plane.normal, decal_params.normal );
		decal_params.materialHash = bloodMaterials[ Q_rand() % 3 ];
		decal_params.surfaceClass = SG_DECAL_SURFACE_FLESH;
		// Authoritative floor drip pool radius (diameter 20 - 28 units), proportional to character scale (32-unit width).
		decal_params.radius = 10.0f + ( frand() * 4.0f );
		decal_params.depth = 8.0f;
		decal_params.rotationRadians = frand() * 6.2831853f;
		decal_params.lifeSeconds = 20.0f;
		decal_params.fadeInSeconds = 0.04f;
		decal_params.fadeOutSeconds = 0.40f;
		decal_params.hitEntityNumber = ENTITYNUM_WORLD;
		decal_params.flags = SG_DECAL_FLAG_DYNAMIC;
		(void)CLG_Decals_QueueSpawn( decal_params );
	}

	/**
	*	2. Exit spray: cast a short ray along the bullet exit vector (-dir) onto a close rear wall.
	*	   Only surfaces within close proximity (<= 100 units) receive backsplatter.
	**/
	const Vector3 exitTraceEnd = org + ( dir * -100.0f ) + Vector3{ 0.0f, 0.0f, -10.0f };
	const cm_trace_t exitTrace = CLG_Clip( org, nullptr, nullptr, exitTraceEnd, nullptr, CM_CONTENTMASK_SOLID );
	const bool hitExitSolid = ( exitTrace.fraction < 1.0f && !exitTrace.allsolid && !exitTrace.startsolid );
	const bool hitExitSky = ( exitTrace.surface != nullptr && ( exitTrace.surface->flags & CM_SURFACE_FLAG_SKY ) != 0 );

	// Avoid duplicate floor streaks if the floor drip already covered the floor.
	if ( hitExitSolid && !hitExitSky && ( fabsf( exitTrace.plane.normal[ 2 ] ) < 0.7f || !hitFloorSolid ) ) {
		sg_decal_spawn_params_t decal_params = {};
		VectorCopy( exitTrace.endpos, decal_params.origin );
		VectorCopy( exitTrace.plane.normal, decal_params.normal );
		decal_params.materialHash = bloodMaterials[ Q_rand() % 3 ];
		decal_params.surfaceClass = SG_DECAL_SURFACE_FLESH;
		// Wall backsplatter radius (diameter 24 - 32 units).
		decal_params.radius = 12.0f + ( frand() * 4.0f );
		decal_params.depth = 8.0f;
		decal_params.rotationRadians = frand() * 6.2831853f;
		decal_params.lifeSeconds = 20.0f;
		decal_params.fadeInSeconds = 0.04f;
		decal_params.fadeOutSeconds = 0.40f;
		decal_params.hitEntityNumber = ENTITYNUM_WORLD;
		decal_params.flags = SG_DECAL_FLAG_DYNAMIC;
		(void)CLG_Decals_QueueSpawn( decal_params );
	}

	/**
	*	Spawn a 3D Alembic hero fluid splatter at the wound impact site on every blood impact.
	**/
	// Emit hero fluid geometry whenever positive blood severity is requested.
	if ( count > 0 ) {
		// Scale splatter volume so the ~10-unit Alembic mesh reads clearly at Quake character scale without stretching.
		const float heroScale = std::clamp( 1.0f + ( static_cast<float>( count ) / 50.0f ), 1.0f, 1.8f );
		const Vector3 velocity = dir * 150.0f;
		CLG_FX_BloodHero_Spawn( org, dir, velocity, ENTITYNUM_WORLD, heroScale );
	}
}


/*
===============
CLG_FX_ParticleEffect2
===============
*/
void CLG_FX_ParticleEffect2( const Vector3 &org, const Vector3 &dir, int color, int count ) {
    int         i, j;
    clg_particle_t *p;
    float       d;

    count *= cl_particle_num_factor->value;

    for ( i = 0; i < count; i++ ) {
        p = CLG_AllocateParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;

        p->color = color;
        p->brightness = 1.0f;

        d = Q_rand() & 7;
        for ( j = 0; j < 3; j++ ) {
            p->org[ j ] = org[ j ] + ( (int)( Q_rand() & 7 ) - 4 ) + d * dir[ j ];
            p->vel[ j ] = crand() * 20;
        }

        p->accel[ 0 ] = p->accel[ 1 ] = 0;
        p->accel[ 2 ] = -PARTICLE_GRAVITY;
        p->alpha = 1.0f;

        p->alphavel = -1.0f / ( 0.5f + frand() * 0.3f );
    }
}


/*
===============
CLG_FX_TeleporterParticles
===============
*/
void CLG_FX_TeleporterParticles( const Vector3 &org ) {
    int         i, j;
    clg_particle_t *p;

    const int count = 8 * cl_particle_num_factor->value;

    for ( i = 0; i < count; i++ ) {
        p = CLG_AllocateParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;

		// <Q2RTXP>: Changed color to use RGBA and be "California Orange".
        //p->color = 0xdb;
		p->color = -1;
		p->rgba = color_t{
			.u32 = MakeColor( 210, 125, 40, 255 ) // "California Orange"
		};
        p->brightness = 1.0f;

        for ( j = 0; j < 2; j++ ) {
            p->org[ j ] = org[ j ] - 16 + ( Q_rand() & 31 );
            p->vel[ j ] = crand() * 14;
        }

        p->org[ 2 ] = org[ 2 ] - 8 + ( Q_rand() & 7 );
        p->vel[ 2 ] = 80 + ( Q_rand() & 7 );

        p->accel[ 0 ] = p->accel[ 1 ] = 0;
        p->accel[ 2 ] = -PARTICLE_GRAVITY;
        p->alpha = 1.0f;

        p->alphavel = -0.5f;
    }
}


/*
===============
CLG_FX_LogoutEffect

===============
*/
void CLG_FX_LogoutEffect( const Vector3 &org, int type ) {
    int         i, j;
    clg_particle_t *p;

    for ( i = 0; i < 500; i++ ) {
        p = CLG_AllocateParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;

        int color;
        if ( type == MZ_LOGIN )
            color = 0xd0 + ( Q_rand() & 7 ); // green
        else if ( type == MZ_LOGOUT )
            color = 0x40 + ( Q_rand() & 7 ); // red
        else
            color = 0xe0 + ( Q_rand() & 7 ); // yellow

        p->color = color;
        p->brightness = 1.0f;

        p->org[ 0 ] = org[ 0 ] - 16 + frand() * 32;
        p->org[ 1 ] = org[ 1 ] - 16 + frand() * 32;
        p->org[ 2 ] = org[ 2 ] - 24 + frand() * 56;

        for ( j = 0; j < 3; j++ )
            p->vel[ j ] = crand() * 20;

        p->accel[ 0 ] = p->accel[ 1 ] = 0;
        p->accel[ 2 ] = -PARTICLE_GRAVITY;
        p->alpha = 1.0f;

        p->alphavel = -1.0f / ( 1.0f + frand() * 0.3f );
    }
}


/*
===============
CLG_FX_ItemRespawnParticles

===============
*/
void CLG_FX_ItemRespawnParticles( const Vector3 &org ) {
    int         i, j;
    clg_particle_t *p;

    const int count = 64 * cl_particle_num_factor->value;

    for ( i = 0; i < count; i++ ) {
        p = CLG_AllocateParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;

        p->color = 0xd4 + ( Q_rand() & 3 ); // green
        p->brightness = 1.0f;

        p->org[ 0 ] = org[ 0 ] + crand() * 8;
        p->org[ 1 ] = org[ 1 ] + crand() * 8;
        p->org[ 2 ] = org[ 2 ] + crand() * 8;

        for ( j = 0; j < 3; j++ )
            p->vel[ j ] = crand() * 8;

        p->accel[ 0 ] = p->accel[ 1 ] = 0;
        p->accel[ 2 ] = -PARTICLE_GRAVITY * 0.2f;
        p->alpha = 1.0f;

        p->alphavel = -1.0f / ( 1.0f + frand() * 0.3f );
    }
}


/*
===============
CLG_FX_ExplosionParticles
===============
*/
void CLG_FX_ExplosionParticles( const Vector3 &org ) {
    int         i, j;
    clg_particle_t *p;

    const int count = 256 * cl_particle_num_factor->value;

    for ( i = 0; i < count; i++ ) {
        p = CLG_AllocateParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;

        p->color = 0xe0 + ( Q_rand() & 7 );
        p->brightness = cvar_pt_particle_emissive->value;

        for ( j = 0; j < 3; j++ ) {
            p->org[ j ] = org[ j ] + ( (int)( irandom( 32 ) ) - 16 );
            p->vel[ j ] = (int)( irandom( 384 ) ) - 192;
        }

        p->accel[ 0 ] = p->accel[ 1 ] = 0;
        p->accel[ 2 ] = -PARTICLE_GRAVITY;
        p->alpha = 1.0f;

        p->alphavel = -0.8f / ( 0.5f + frand() * 0.3f );
    }
}

/*
===============
CLG_FX_BigTeleportParticles
===============
*/
void CLG_FX_BigTeleportParticles( const Vector3 &org ) {
    static const byte   colortable[ 4 ] = { 2 * 8, 13 * 8, 21 * 8, 18 * 8 };
    int         i;
    clg_particle_t *p;
    float       angle, dist;

    for ( i = 0; i < 4096; i++ ) {
        p = CLG_AllocateParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;

        p->color = colortable[ Q_rand() & 3 ];
        p->brightness = 1.0f;

        angle = ( Q_rand() & 1023 ) * ( M_PI * 2 / 1023 );
        dist = Q_rand() & 31;
        p->org[ 0 ] = org[ 0 ] + cos( angle ) * dist;
        p->vel[ 0 ] = cos( angle ) * ( 70 + ( Q_rand() & 63 ) );
        p->accel[ 0 ] = -cos( angle ) * 100;

        p->org[ 1 ] = org[ 1 ] + sin( angle ) * dist;
        p->vel[ 1 ] = sin( angle ) * ( 70 + ( Q_rand() & 63 ) );
        p->accel[ 1 ] = -sin( angle ) * 100;

        p->org[ 2 ] = org[ 2 ] + 8 + ( irandom( 90 ) );
        p->vel[ 2 ] = -100 + ( Q_rand() & 31 );
        p->accel[ 2 ] = PARTICLE_GRAVITY * 4;
        p->alpha = 1.0f;

        p->alphavel = -0.3f / ( 0.5f + frand() * 0.3f );
    }
}


/*
===============
CLG_FX_BlasterParticles

Wall impact puffs
===============
*/
void CLG_FX_BlasterParticles( const Vector3 &org, const Vector3 &dir ) {
    int         i, j;
    clg_particle_t *p;
    float       d;

    const int count = 40 * cl_particle_num_factor->value;

    for ( i = 0; i < count; i++ ) {
        p = CLG_AllocateParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;

        p->color = 0xe0 + ( Q_rand() & 7 );
        p->brightness = cvar_pt_particle_emissive->value;

        d = Q_rand() & 15;
        for ( j = 0; j < 3; j++ ) {
            p->org[ j ] = org[ j ] + ( (int)( Q_rand() & 7 ) - 4 ) + d * dir[ j ];
            p->vel[ j ] = dir[ j ] * 30 + crand() * 40;
        }

        p->accel[ 0 ] = p->accel[ 1 ] = 0;
        p->accel[ 2 ] = -PARTICLE_GRAVITY;
        p->alpha = 1.0f;

        p->alphavel = -1.0f / ( 0.5f + frand() * 0.3f );
    }
}


/*
===============
CLG_FX_BlasterTrail

===============
*/
void CLG_FX_BlasterTrail( const Vector3 &start, const Vector3 &end ) {
    vec3_t      move;
    vec3_t      vec;
    float       len;
    int         j;
    clg_particle_t *p;
    int         dec;

    VectorCopy( start, move );
    VectorSubtract( end, start, vec );
    len = VectorNormalize( vec );

    dec = 5;
    VectorScale( vec, 5, vec );

    // FIXME: this is a really silly way to have a loop
    while ( len > 0 ) {
        len -= dec;

        p = CLG_AllocateParticle();
        if ( !p )
            return;
        VectorClear( p->accel );

        p->time = clgi.client->time;

        p->alpha = 1.0;
        p->alphavel = -1.0f / ( 0.3f + frand() * 0.2f );

        p->color = 0xe0;
        p->brightness = cvar_pt_particle_emissive->value;

        for ( j = 0; j < 3; j++ ) {
            p->org[ j ] = move[ j ] + crand();
            p->vel[ j ] = crand() * 5;
            p->accel[ j ] = 0;
        }

        VectorAdd( move, vec, move );
    }
}

/*
===============
CLG_FX_FlagTrail

===============
*/
void CLG_FX_FlagTrail( const Vector3 &start, const Vector3 &end, int color ) {
    vec3_t      move;
    vec3_t      vec;
    float       len;
    int         j;
    clg_particle_t *p;
    int         dec;

    VectorCopy( start, move );
    VectorSubtract( end, start, vec );
    len = VectorNormalize( vec );

    dec = 5;
    VectorScale( vec, 5, vec );

    while ( len > 0 ) {
        len -= dec;

        p = CLG_AllocateParticle();
        if ( !p )
            return;
        VectorClear( p->accel );

        p->time = clgi.client->time;

        p->alpha = 1.0;
        p->alphavel = -1.0f / ( 0.8f + frand() * 0.2f );

        p->color = color;
        p->brightness = 1.0f;

        for ( j = 0; j < 3; j++ ) {
            p->org[ j ] = move[ j ] + crand() * 16;
            p->vel[ j ] = crand() * 5;
            p->accel[ j ] = 0;
        }

        VectorAdd( move, vec, move );
    }
}

/*
===============
CLG_FX_DiminishingTrail

===============
*/
void CLG_FX_DiminishingTrail( const Vector3 &start, const Vector3 &end, centity_t *old, int flags ) {
    vec3_t      move;
    vec3_t      vec;
    float       len;
    int         j;
    clg_particle_t *p;
    float       dec;
    float       orgscale;
    float       velscale;

    VectorCopy( start, move );
    VectorSubtract( end, start, vec );
    len = VectorNormalize( vec );

    dec = 0.5f;
    VectorScale( vec, dec, vec );

    if ( old->trailCount > 900 ) {
        orgscale = 4;
        velscale = 15;
    } else if ( old->trailCount > 800 ) {
        orgscale = 2;
        velscale = 10;
    } else {
        orgscale = 1;
        velscale = 5;
    }

    while ( len > 0 ) {
        len -= dec;

        // drop less particles as it flies
        if ( ( Q_rand() & 1023 ) < old->trailCount ) {
            p = CLG_AllocateParticle();
            if ( !p )
                return;
            VectorClear( p->accel );

            p->time = clgi.client->time;

            if ( flags & EF_GIB ) {
                p->alpha = 1.0;
                p->alphavel = -1.0f / ( 1 + frand() * 0.4f );

                p->color = 0xe8 + ( Q_rand() & 7 );
                p->brightness = 1.0f;

                for ( j = 0; j < 3; j++ ) {
                    p->org[ j ] = move[ j ] + crand() * orgscale;
                    p->vel[ j ] = crand() * velscale;
                    p->accel[ j ] = 0;
                }
                p->vel[ 2 ] -= PARTICLE_GRAVITY;
            } else {
                p->alpha = 1.0f;
                p->alphavel = -1.0f / ( 1 + frand() * 0.2f );

                p->color = 4 + ( Q_rand() & 7 );
                p->brightness = 1.0f;

                for ( j = 0; j < 3; j++ ) {
                    p->org[ j ] = move[ j ] + crand() * orgscale;
                    p->vel[ j ] = crand() * velscale;
                }
                p->accel[ 2 ] = 20;
            }
        }

        old->trailCount -= 5;
        if ( old->trailCount < 100 )
            old->trailCount = 100;
        VectorAdd( move, vec, move );
    }
}

/*
===============
CLG_FX_RocketTrail

===============
*/
void CLG_FX_RocketTrail( const Vector3 &start, const Vector3 &end, centity_t *old ) {
    vec3_t      move;
    vec3_t      vec;
    float       len;
    int         j;
    clg_particle_t *p;
    float       dec;

    // smoke
    CLG_FX_DiminishingTrail( start, end, old, 0/*EF_ROCKET*/ );

    // fire
    VectorCopy( start, move );
    VectorSubtract( end, start, vec );
    len = VectorNormalize( vec );

    dec = 1;
    VectorScale( vec, dec, vec );

    while ( len > 0 ) {
        len -= dec;

        if ( ( Q_rand() & 7 ) == 0 ) {
            p = CLG_AllocateParticle();
            if ( !p )
                return;

            VectorClear( p->accel );
            p->time = clgi.client->time;

            p->alpha = 1.0;
            p->alphavel = -1.0f / ( 1 + frand() * 0.2f );

            p->color = 0xdc + ( Q_rand() & 3 );
            p->brightness = cvar_pt_particle_emissive->value;

            for ( j = 0; j < 3; j++ ) {
                p->org[ j ] = move[ j ] + crand() * 5;
                p->vel[ j ] = crand() * 20;
            }
            p->accel[ 2 ] = -PARTICLE_GRAVITY;
        }
        VectorAdd( move, vec, move );
    }
}

/*
===============
CLG_FX_RailTrail

===============
*/
void CLG_FX_OldRailTrail( void ) {
    vec3_t      move;
    vec3_t      vec;
    float       len;
    int         j;
    clg_particle_t *p;
    float       dec;
    vec3_t      right, up;
    int         i;
    float       d, c, s;
    vec3_t      dir;
    byte        clr = 0x74;

    VectorCopy( level.parsedMessage.events.tempEntity.pos1, move );
    VectorSubtract( level.parsedMessage.events.tempEntity.pos2, level.parsedMessage.events.tempEntity.pos1, vec );
    len = VectorNormalize( vec );

    MakeNormalVectors( vec, right, up );

    for ( i = 0; i < len; i++ ) {
        p = CLG_AllocateParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;
        VectorClear( p->accel );

        d = i * 0.1;
        c = cos( d );
        s = sin( d );

        VectorScale( right, c, dir );
        VectorMA( dir, s, up, dir );

        p->alpha = 1.0;
        p->alphavel = -1.0f / ( 1 + frand() * 0.2f );

        p->color = clr + ( Q_rand() & 7 );
        p->brightness = cvar_pt_particle_emissive->value;

        for ( j = 0; j < 3; j++ ) {
            p->org[ j ] = move[ j ] + dir[ j ] * 3;
            p->vel[ j ] = dir[ j ] * 6;
        }

        VectorAdd( move, vec, move );
    }

    dec = 0.75f;
    VectorScale( vec, dec, vec );
    VectorCopy( level.parsedMessage.events.tempEntity.pos1, move );

    while ( len > 0 ) {
        len -= dec;

        p = CLG_AllocateParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;
        VectorClear( p->accel );

        p->alpha = 1.0;
        p->alphavel = -1.0f / ( 0.6f + frand() * 0.2f );

        p->color = Q_rand() & 15;
        p->brightness = 1.0f;

        for ( j = 0; j < 3; j++ ) {
            p->org[ j ] = move[ j ] + crand() * 3;
            p->vel[ j ] = crand() * 3;
            p->accel[ j ] = 0;
        }

        VectorAdd( move, vec, move );
    }
}


/*
===============
CLG_FX_BubbleTrail

===============
*/
void CLG_FX_BubbleTrail( const Vector3 &start, const Vector3 &end ) {
    vec3_t      move;
    vec3_t      vec;
    float       len;
    int         i, j;
    clg_particle_t *p;
    float       dec;

    VectorCopy( start, move );
    VectorSubtract( end, start, vec );
    len = VectorNormalize( vec );

    dec = 32;
    VectorScale( vec, dec, vec );

    for ( i = 0; i < len; i += dec ) {
        p = CLG_AllocateParticle();
        if ( !p )
            return;

        VectorClear( p->accel );
        p->time = clgi.client->time;

        p->alpha = 1.0;
        p->alphavel = -1.0f / ( 1 + frand() * 0.2 );

        p->color = 4 + ( Q_rand() & 7 );
        p->brightness = 1.0f;

        for ( j = 0; j < 3; j++ ) {
            p->org[ j ] = move[ j ] + crand() * 2;
            p->vel[ j ] = crand() * 5;
        }
        p->vel[ 2 ] += 6;

        VectorAdd( move, vec, move );
    }
}


/*
===============
CLG_FX_FlyParticles
===============
*/

#define BEAMLENGTH  16

static void CLG_FX_FlyParticles( const Vector3 &origin, int count ) {
    int         i;
    clg_particle_t *p;
    float       angle;
    float       sp, sy, cp, cy;
    vec3_t      forward;
    float       dist;
    float       ltime;

    if ( count > NUMVERTEXNORMALS )
        count = NUMVERTEXNORMALS;

    ltime = clgi.client->time * 0.001f;
    for ( i = 0; i < count; i += 2 ) {
        angle = ltime * game.avelocities[ i ][ 0 ];
        sy = sin( angle );
        cy = cos( angle );
        angle = ltime * game.avelocities[ i ][ 1 ];
        sp = sin( angle );
        cp = cos( angle );

        forward[ 0 ] = cp * cy;
        forward[ 1 ] = cp * sy;
        forward[ 2 ] = -sp;

        p = CLG_AllocateParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;

        dist = sin( ltime + i ) * 64;
        p->org[ 0 ] = origin[ 0 ] + bytedirs[ i ][ 0 ] * dist + forward[ 0 ] * BEAMLENGTH;
        p->org[ 1 ] = origin[ 1 ] + bytedirs[ i ][ 1 ] * dist + forward[ 1 ] * BEAMLENGTH;
        p->org[ 2 ] = origin[ 2 ] + bytedirs[ i ][ 2 ] * dist + forward[ 2 ] * BEAMLENGTH;

        VectorClear( p->vel );
        VectorClear( p->accel );

        p->color = 0;
        p->brightness = 1.0f;

        p->alpha = 1;
        p->alphavel = INSTANT_PARTICLE;
    }
}

void CLG_FX_FlyEffect( centity_t *ent, const Vector3 &origin ) {
    int     n;
    int     count;
    int     starttime;

    if ( ent->fly_stopTime < clgi.client->time ) {
        starttime = clgi.client->time;
        ent->fly_stopTime = clgi.client->time + 60000;
    } else {
        starttime = ent->fly_stopTime - 60000;
    }

    n = clgi.client->time - starttime;
    if ( n < 20000 )
        count = n * NUMVERTEXNORMALS / 20000;
    else {
        n = ent->fly_stopTime - clgi.client->time;
        if ( n < 20000 )
            count = n * NUMVERTEXNORMALS / 20000;
        else
            count = NUMVERTEXNORMALS;
    }

    CLG_FX_FlyParticles( origin, count );
}

/*
===============
CLG_FX_BfgParticles
===============
*/
void CLG_FX_BfgParticles( entity_t *ent ) {
    int         i;
    clg_particle_t *p;
    float       angle;
    float       sp, sy, cp, cy;
    vec3_t      forward;
    float       dist;
    float       ltime;

    const int count = NUMVERTEXNORMALS * cl_particle_num_factor->value;

    ltime = clgi.client->time * 0.001f;
    for ( i = 0; i < count; i++ ) {
        angle = ltime * game.avelocities[ i ][ 0 ];
        sy = sin( angle );
        cy = cos( angle );
        angle = ltime * game.avelocities[ i ][ 1 ];
        sp = sin( angle );
        cp = cos( angle );

        forward[ 0 ] = cp * cy;
        forward[ 1 ] = cp * sy;
        forward[ 2 ] = -sp;

        p = CLG_AllocateParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;

        dist = sin( ltime + i ) * 64;
        p->org[ 0 ] = ent->origin[ 0 ] + bytedirs[ i ][ 0 ] * dist + forward[ 0 ] * BEAMLENGTH;
        p->org[ 1 ] = ent->origin[ 1 ] + bytedirs[ i ][ 1 ] * dist + forward[ 1 ] * BEAMLENGTH;
        p->org[ 2 ] = ent->origin[ 2 ] + bytedirs[ i ][ 2 ] * dist + forward[ 2 ] * BEAMLENGTH;

        VectorClear( p->vel );
        VectorClear( p->accel );

        dist = Distance( p->org, ent->origin ) / 90.0f;
        p->color = floor( 0xd0 + dist * 7 );
        p->brightness = cvar_pt_particle_emissive->value;

        p->alpha = 1.0f - dist;
        p->alphavel = INSTANT_PARTICLE;
    }
}


/*
===============
CLG_FX_BFGExplosionParticles
===============
*/
//FIXME combined with CLG_FX_ExplosionParticles
void CLG_FX_BFGExplosionParticles( const Vector3 &org ) {
    int         i, j;
    clg_particle_t *p;

    const int count = 256 * cl_particle_num_factor->value;

    for ( i = 0; i < count; i++ ) {
        p = CLG_AllocateParticle();
        if ( !p )
            return;

        p->time = clgi.client->time;

        p->color = 0xd0 + ( Q_rand() & 7 );
        p->brightness = cvar_pt_particle_emissive->value;

        for ( j = 0; j < 3; j++ ) {
            p->org[ j ] = org[ j ] + ( (int)( irandom( 32 ) ) - 16 );
            p->vel[ j ] = (int)( irandom( 384 ) ) - 192;
        }

        p->accel[ 0 ] = p->accel[ 1 ] = 0;
        p->accel[ 2 ] = -PARTICLE_GRAVITY;
        p->alpha = 1.0f;

        p->alphavel = -0.8f / ( 0.5f + frand() * 0.3f );
    }
}


/*
===============
CLG_FX_PlayerTeleportParticles

===============
*/
void CLG_FX_PlayerTeleportParticles( const Vector3 &org ) {
    int         i, j, k;
    clg_particle_t *p;
    float       vel;
    vec3_t      dir;

    for ( i = -16; i <= 16; i += 4 )
        for ( j = -16; j <= 16; j += 4 )
            for ( k = -16; k <= 32; k += 4 ) {
                p = CLG_AllocateParticle();
                if ( !p )
                    return;

                p->time = clgi.client->time;

				// <Q2RTXP>: Changed color to use RGBA and be "California Orange".
                //p->color = 7 + ( Q_rand() & 7 );
				p->color = -1;
				p->rgba = color_t{
					.u32 = MakeColor( 210, 125, 40, 255 ) // "California Orange"
				};
                p->brightness = 1.0f;

                p->alpha = 1.0f;
                p->alphavel = -1.0f / ( 0.3f + ( Q_rand() & 7 ) * 0.02f );

                p->org[ 0 ] = org[ 0 ] + i + ( Q_rand() & 3 );
                p->org[ 1 ] = org[ 1 ] + j + ( Q_rand() & 3 );
                p->org[ 2 ] = org[ 2 ] + k + ( Q_rand() & 3 );

                dir[ 0 ] = j * 8;
                dir[ 1 ] = i * 8;
                dir[ 2 ] = k * 8;

                VectorNormalize( dir );
                vel = 50 + ( Q_rand() & 63 );
                VectorScale( dir, vel, p->vel );

                p->accel[ 0 ] = p->accel[ 1 ] = 0;
                p->accel[ 2 ] = -PARTICLE_GRAVITY;
            }
}
