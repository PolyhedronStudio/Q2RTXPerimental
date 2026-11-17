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
	// If this is a blood particle effect (color 0xe8..0xef), redirect to modern 3D Alembic fluid mesh effect.
	if ( ( color & ~7 ) == 0xe8 ) {
		CLG_FX_BloodParticleEffect( org, dir, color, count );
		return;
	}

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
	// If this is a blood splash, redirect to modern 3D Alembic fluid mesh effect.
	if ( color == SPLASH_FX_COLOR_BLOOD || ( color & ~7 ) == 0xe8 ) {
		CLG_FX_BloodParticleEffect( org, dir, 0xe8, count );
		return;
	}

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
	const Vector3 floorTraceEnd = org + Vector3{ crand() * 4.0f, crand() * 4.0f, -128.0f };
	const cm_trace_t floorTrace = CLG_Clip( org, nullptr, nullptr, floorTraceEnd, nullptr, CM_CONTENTMASK_SOLID );
	const bool hitFloorSolid = ( floorTrace.fraction < 1.0f && !floorTrace.allsolid && !floorTrace.startsolid );
	const bool hitFloorSky = ( floorTrace.surface != nullptr && ( floorTrace.surface->flags & CM_SURFACE_FLAG_SKY ) != 0 );

	// Only spawn a floor drip if we struck upward-facing solid world geometry.
	if ( hitFloorSolid && !hitFloorSky && floorTrace.plane.normal[ 2 ] > 0.7f ) {
		sg_decal_spawn_params_t decal_params = {};
		VectorCopy( floorTrace.endpos, decal_params.origin );
		VectorCopy( floorTrace.plane.normal, decal_params.normal );
		decal_params.materialHash = bloodMaterials[ Q_rand() % 3 ];
		decal_params.surfaceClass = SG_DECAL_SURFACE_FLESH;
		// Authoritative floor drip pool radius (diameter 16 - 22 units), proportional to character scale.
		decal_params.radius = 8.0f + ( frand() * 3.0f );
		decal_params.depth = 6.0f;
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
	*	   Only vertical/near-vertical surfaces (walls, normal.z < 0.35) within close proximity (<= 80 units)
	*	   receive backsplatter. This strictly prevents grazing-angle decal projection onto the floor.
	**/
	const Vector3 exitTraceEnd = org + ( dir * -80.0f );
	const cm_trace_t exitTrace = CLG_Clip( org, nullptr, nullptr, exitTraceEnd, nullptr, CM_CONTENTMASK_SOLID );
	const bool hitExitSolid = ( exitTrace.fraction < 1.0f && !exitTrace.allsolid && !exitTrace.startsolid );
	const bool hitExitSky = ( exitTrace.surface != nullptr && ( exitTrace.surface->flags & CM_SURFACE_FLAG_SKY ) != 0 );

	// Only spawn backsplatter onto vertical or near-vertical world geometry.
	if ( hitExitSolid && !hitExitSky && fabsf( exitTrace.plane.normal[ 2 ] ) < 0.35f ) {
		sg_decal_spawn_params_t decal_params = {};
		VectorCopy( exitTrace.endpos, decal_params.origin );
		VectorCopy( exitTrace.plane.normal, decal_params.normal );
		decal_params.materialHash = bloodMaterials[ Q_rand() % 3 ];
		decal_params.surfaceClass = SG_DECAL_SURFACE_FLESH;
		// Wall backsplatter radius (diameter 20 - 28 units).
		decal_params.radius = 10.0f + ( frand() * 4.0f );
		decal_params.depth = 6.0f;
		decal_params.rotationRadians = frand() * 6.2831853f;
		decal_params.lifeSeconds = 20.0f;
		decal_params.fadeInSeconds = 0.04f;
		decal_params.fadeOutSeconds = 0.40f;
		decal_params.hitEntityNumber = ENTITYNUM_WORLD;
		decal_params.flags = SG_DECAL_FLAG_DYNAMIC;
		(void)CLG_Decals_QueueSpawn( decal_params );
	}

	/**
	*	Spawn 3D Alembic hero fluid splatter meshes at the wound impact site.
	*	Spawns multiple splatter instances based on damage severity (count) with slight
	*	angular and positional offsets from the primary burst, each randomly scaled.
	**/
	// Emit hero fluid geometry whenever positive blood severity is requested.
	if ( count > 0 ) {
		// Determine the number of blood splatter mesh particle effects based on damage severity and randomness.
		int32_t numSplatters = 1;
		// Categorize splatter count into tiers based on wound severity thresholds (count = take * 5).
		if ( count > 320 ) {
			// Severe or explosive damage (>= 65 dmg): 4 splatters with a 50% chance for a 5th.
			numSplatters = 4 + ( ( Q_rand() % 100 ) < 50 ? 1 : 0 );
		} else if ( count > 190 ) {
			// Heavy impact (e.g. shotgun blast, ~40-60 dmg): 3 splatters with a 50% chance for a 4th.
			numSplatters = 3 + ( ( Q_rand() % 100 ) < 50 ? 1 : 0 );
		} else if ( count > 100 ) {
			// Moderate impact (e.g. blaster / automatic, ~20-35 dmg): 2 splatters with a 50% chance for a 3rd.
			numSplatters = 2 + ( ( Q_rand() % 100 ) < 50 ? 1 : 0 );
		} else {
			// Light graze or minor wound (e.g. pistol, ~10-15 dmg): 1 splatter with a 30% chance for a 2nd.
			numSplatters = 1 + ( ( Q_rand() % 100 ) < 30 ? 1 : 0 );
		}
		// Clamp splatter count within safe upper bounds to avoid exhausting the hero splatter ring buffer.
		numSplatters = std::clamp( numSplatters, 1, 5 );

		// Compute base scale factor driven by damage severity across a wide, noticeable dynamic range [0.45f .. 1.85f].
		const float damageNormalized = std::clamp( ( static_cast<float>( count ) - 40.0f ) / 360.0f, 0.0f, 1.0f );
		const float baseScale = 0.45f + ( damageNormalized * 1.40f );

		// Compute orthonormal tangent basis from the primary impact direction for angular and positional offsets.
		vec3_t forwardArr = { dir.x, dir.y, dir.z };
		vec3_t rightArr = {};
		vec3_t upArr = {};
		MakeNormalVectors( forwardArr, rightArr, upArr );
		const Vector3 rightVec( rightArr );
		const Vector3 upVec( upArr );

		// Spawn each individual splatter mesh instance with its dedicated angular offset, position jitter, and scale.
		for ( int32_t i = 0; i < numSplatters; i++ ) {
			Vector3 splatterOrigin = org;
			Vector3 splatterDir = dir;
			float splatterScale = baseScale;
			float splatterSpeed = 150.0f;

			// Differentiate between primary impact burst (index 0) and secondary divergent splatters (index > 0).
			if ( i == 0 ) {
				// Primary splatter size is impacted by damage with a subtle +/- 10% randomness factor.
				const float primaryRandomFactor = 0.90f + ( frand() * 0.20f );
				splatterScale = std::clamp( baseScale * primaryRandomFactor, 0.40f, 2.10f );
				splatterSpeed = 140.0f + ( crand() * 20.0f );
			} else {
				// Secondary splatters: offset position slightly along the wound perimeter.
				const float radialOffset = 1.0f + ( frand() * 3.5f );
				const float posTheta = frand() * 6.2831853f;
				const float posRight = cosf( posTheta ) * radialOffset;
				const float posUp = sinf( posTheta ) * radialOffset;
				const float posFwd = crand() * 1.5f;
				splatterOrigin = org + ( rightVec * posRight ) + ( upVec * posUp ) + ( dir * posFwd );

				// Angular offset: perturb trajectory direction by a narrow cone deflection (~8 to 22 degrees).
				const float angularSpread = 0.15f + ( frand() * 0.25f );
				const float dirPhi = frand() * 6.2831853f;
				const float dirRight = cosf( dirPhi ) * angularSpread;
				const float dirUp = sinf( dirPhi ) * angularSpread;
				splatterDir = QM_Vector3Normalize( dir + ( rightVec * dirRight ) + ( upVec * dirUp ) );

				// Secondary splatters are sized proportionally to damage with greater random variation (50% to 105%).
				const float secondaryRandomFactor = 0.50f + ( frand() * 0.55f );
				splatterScale = std::clamp( baseScale * secondaryRandomFactor, 0.25f, 1.75f );

				// Stagger velocity slightly between secondary droplets.
				splatterSpeed = 110.0f + ( frand() * 70.0f );
			}

			// Spawn 3D Alembic hero splatter mesh with its calculated origin, angular direction, velocity, and scale.
			const Vector3 velocity = splatterDir * splatterSpeed;
			CLG_FX_BloodHero_Spawn( splatterOrigin, splatterDir, velocity, ENTITYNUM_WORLD, splatterScale );
		}
	}
}

/**
*	@brief		Spawn a multi-mesh 3D Alembic blood splatter effect for monster death.
*	@details	Replaces legacy point-based death particles with multiple animated Alembic
*				fluid meshes along distinct arterial trajectory routes, complete with
*				ground drip pooling decals.
*	@param	origin			World-space feet or base origin of the dying monster.
*	@param	angles			Orientation angles of the dying monster.
*	@param	velocity		Movement or knockback velocity vector of the dying monster.
*	@param	entityNumber	Entity index of the dying monster.
**/
void CLG_FX_MonsterDeathBloodSplatter( const Vector3 &origin, const Vector3 &angles, const Vector3 &velocity, [[maybe_unused]] const int32_t entityNumber ) {
	clgi.Print( PRINT_ALL, "[CLGame]: CLG_FX_MonsterDeathBloodSplatter triggered for ent %d!\n", entityNumber );

	/**
	*	Array of available blood splatter decal material hashes for random texture variation.
	**/
	static constexpr sg_decal_material_hash_t bloodMaterials[] = {
		SG_DECAL_MATERIAL_HASH_BLOOD_FLESH,
		SG_DECAL_MATERIAL_HASH_BLOOD_FLESH_02,
		SG_DECAL_MATERIAL_HASH_BLOOD_FLESH_03
	};

	/**
	*	Calculate torso center and construct directional orientation basis.
	**/
	// Torso / chest emission center (approximately 28 units above feet origin).
	const Vector3 chestOrigin = origin + Vector3{ 0.0f, 0.0f, 28.0f };

	// Compute forward, right, and world-up vectors from victim orientation angles.
	Vector3 forward = {}, right = {}, upVec = {};
	QM_AngleVectors( angles, &forward, &right, &upVec );
	const Vector3 up( 0.0f, 0.0f, 1.0f );

	// Determine primary directional bias from knockback velocity if significant, otherwise victim facing.
	Vector3 biasDir = forward;
	const float velLen2 = ( velocity.x * velocity.x ) + ( velocity.y * velocity.y ) + ( velocity.z * velocity.z );
	// If the victim has significant movement or knockback velocity, align primary spray with momentum.
	if ( velLen2 > ( 10.0f * 10.0f ) ) {
		biasDir = QM_Vector3Normalize( velocity );
	}

	/**
	*	Trajectory branch definitions replicating the classic point particle dispersion route
	*	with high-fidelity 3D Alembic fluid mesh sequences.
	**/
	struct death_splat_def_t {
		Vector3 dir;
		float speed;
		float scale;
		float offsetDist;
	};

	// Construct 6 distinct arterial trajectory branches covering the 3D burst envelope:
	const death_splat_def_t branches[ 6 ] = {
		// 0: Primary forward-up arterial geyser (main exiting blood jet).
		{
			QM_Vector3Normalize( ( biasDir * 0.82f ) + ( up * 0.57f ) + ( right * crand() * 0.10f ) ),
			190.0f + ( frand() * 30.0f ),
			1.30f + ( frand() * 0.25f ),
			4.0f
		},
		// 1: High vertical fountain erupting straight up from torso / neck wound.
		{
			QM_Vector3Normalize( ( biasDir * 0.15f ) + ( right * crand() * 0.20f ) + ( up * 0.98f ) ),
			170.0f + ( frand() * 30.0f ),
			1.10f + ( frand() * 0.25f ),
			3.5f
		},
		// 2: Flank right spray arcing outward and forward.
		{
			QM_Vector3Normalize( ( biasDir * 0.48f ) + ( right * 0.80f ) + ( up * 0.36f ) ),
			150.0f + ( frand() * 30.0f ),
			0.95f + ( frand() * 0.20f ),
			3.0f
		},
		// 3: Flank left spray arcing outward and forward.
		{
			QM_Vector3Normalize( ( biasDir * 0.48f ) - ( right * 0.80f ) + ( up * 0.36f ) ),
			150.0f + ( frand() * 30.0f ),
			0.95f + ( frand() * 0.20f ),
			3.0f
		},
		// 4: Rear-upward spill / back-splatter dripping behind victim.
		{
			QM_Vector3Normalize( ( -biasDir * 0.55f ) + ( right * crand() * 0.35f ) + ( up * 0.25f ) ),
			110.0f + ( frand() * 25.0f ),
			0.80f + ( frand() * 0.20f ),
			2.5f
		},
		// 5: Forward low downward-slanted spray splashing towards the ground in front.
		{
			QM_Vector3Normalize( ( biasDir * 0.90f ) - ( up * 0.18f ) + ( right * crand() * 0.20f ) ),
			140.0f + ( frand() * 25.0f ),
			0.90f + ( frand() * 0.20f ),
			4.0f
		}
	};

	/**
	*	Spawn each individual Alembic fluid splatter mesh instance and its associated floor decals.
	**/
	for ( int32_t i = 0; i < 6; i++ ) {
		const death_splat_def_t &b = branches[ i ];

		// Offset splatter spawn origin along branch direction so meshes do not overlap at source.
		const Vector3 splatterOrigin = chestOrigin + ( b.dir * b.offsetDist ) + Vector3{ crand() * 1.5f, crand() * 1.5f, crand() * 1.5f };
		const Vector3 splatterVel = b.dir * b.speed;

		// Spawn 3D Alembic fluid mesh entity.
		CLG_FX_BloodHero_Spawn( splatterOrigin, b.dir, splatterVel, ENTITYNUM_WORLD, b.scale );

		// Cast a downward ray from the trajectory arc onto solid world geometry for ground pooling decals.
		const Vector3 floorTraceStart = splatterOrigin + ( b.dir * 20.0f );
		const Vector3 floorTraceEnd = floorTraceStart + Vector3{ 0.0f, 0.0f, -128.0f };
		const cm_trace_t floorTrace = CLG_Clip( floorTraceStart, nullptr, nullptr, floorTraceEnd, nullptr, CM_CONTENTMASK_SOLID );

		// Check if ray struck upward-facing solid world geometry.
		if ( floorTrace.fraction < 1.0f && !floorTrace.allsolid && !floorTrace.startsolid && floorTrace.plane.normal[ 2 ] > 0.60f ) {
			sg_decal_spawn_params_t decal_params = {};
			VectorCopy( floorTrace.endpos, decal_params.origin );
			VectorCopy( floorTrace.plane.normal, decal_params.normal );
			decal_params.materialHash = bloodMaterials[ Q_rand() % 3 ];
			decal_params.surfaceClass = SG_DECAL_SURFACE_FLESH;
			// Diameter proportional to spray volume (16 to 24 units).
			decal_params.radius = 8.0f + ( frand() * 4.0f );
			decal_params.depth = 6.0f;
			decal_params.rotationRadians = frand() * 6.2831853f;
			decal_params.lifeSeconds = 25.0f;
			decal_params.fadeInSeconds = 0.04f;
			decal_params.fadeOutSeconds = 0.50f;
			decal_params.hitEntityNumber = ENTITYNUM_WORLD;
			decal_params.flags = SG_DECAL_FLAG_DYNAMIC;
			(void)CLG_Decals_QueueSpawn( decal_params );
		}
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
				// Suppress point particles for gib trails so 3D fluid meshes are exclusively used.
				continue;
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
