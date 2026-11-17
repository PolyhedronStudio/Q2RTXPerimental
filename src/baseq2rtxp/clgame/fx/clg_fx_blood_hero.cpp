//! CLGame Hero Blood Splatters Implementation
/********************************************************************
*
*
*	ClientGame: FX: "clg_fx_blood_hero.cpp"
*
*	Implementation of 3D fluid Alembic hero splatter ring buffer,
*	attachment projection, exponential decay, and scene rendering.
*	Features O(1) FIFO allocation and O(1) swap-and-pop active tracking.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_world.h"
#include "clgame/fx/clg_fx_blood_hero.h"
#include "clgame/fx/clg_fx_blood_utils.h"
#include "clgame/decals/clg_decals.h"

#include <algorithm>
#include <cmath>

//! Pre-allocated static ring buffer of hero blood splatters.
static clg_hero_blood_splat_t s_hero_splats[ MAX_HERO_BLOOD_SPLATS ];
//! Next insertion index for deterministic O(1) FIFO ring buffer eviction.
static int32_t s_hero_splat_head = 0;
//! Cached model handles for Alembic blood splatter variations.
static qhandle_t s_blood_abc_models[ 4 ] = { 0, 0, 0, 0 };

//! Dense active slot array for O(1) addition, O(1) removal, and O(K) contiguous iteration.
static uint16_t s_hero_active_indices[ MAX_HERO_BLOOD_SPLATS ];
//! Lookup table mapping ring buffer slot index to dense active list position.
static uint16_t s_hero_slot_to_active[ MAX_HERO_BLOOD_SPLATS ];
//! Current count of actively simulated hero blood splatters.
static uint32_t s_hero_active_count = 0;

/**
*	@brief	Activate a slot in the dense active list in O(1) time.
*	@param	slot_idx	Index into the `s_hero_splats` array to activate.
**/
static void Hero_ActivateSlot( const uint16_t slot_idx ) {
	/**
	*	Append slot index to dense array and record its index in the lookup map.
	**/
	s_hero_active_indices[ s_hero_active_count ] = slot_idx;
	s_hero_slot_to_active[ slot_idx ] = static_cast<uint16_t>( s_hero_active_count );
	s_hero_active_count++;
}

/**
*	@brief	Deactivate a slot using O(1) swap-and-pop.
*	@param	slot_idx	Index into the `s_hero_splats` array to deactivate.
**/
static void Hero_DeactivateSlot( const uint16_t slot_idx ) {
	/**
	*	Early out if there are no active splatters.
	**/
	// Guard against underflow when no slots are active.
	if ( s_hero_active_count == 0 ) {
		return;
	}

	/**
	*	Swap target slot with the last active slot to maintain density without holes.
	**/
	const uint32_t active_pos = s_hero_slot_to_active[ slot_idx ];
	const uint32_t last_pos = s_hero_active_count - 1;
	const uint16_t last_slot = s_hero_active_indices[ last_pos ];

	// Overwrite target position with the last active slot.
	s_hero_active_indices[ active_pos ] = last_slot;
	s_hero_slot_to_active[ last_slot ] = static_cast<uint16_t>( active_pos );
	s_hero_active_count--;

	// Clear slot active flag.
	s_hero_splats[ slot_idx ].active = false;
}

/**
*	@brief	Initialize the hero blood splatter subsystem and clear ring buffers.
**/
void CLG_FX_BloodHero_Init( void ) {
	/**
	*	Reset all hero blood splatter slots and dense active tracking tables.
	**/
	// Clear every ring-buffer entry and index mapping to initial defaults.
	for ( int32_t i = 0; i < MAX_HERO_BLOOD_SPLATS; i++ ) {
		s_hero_splats[ i ] = clg_hero_blood_splat_t{};
		s_hero_active_indices[ i ] = 0;
		s_hero_slot_to_active[ i ] = 0;
	}
	s_hero_splat_head = 0;
	s_hero_active_count = 0;
}

/**
*	@brief	Precache 3D Alembic hero blood splatter sequence models.
**/
void CLG_PrecacheHeroBlood( void ) {
	/**
	*	Register Alembic .abc fluid simulation sequences via the client-game import API.
	**/
	// Precache the authoritative blood_alembic01 sequence model.
	s_blood_abc_models[ 0 ] = clgi.R_RegisterModel( "models/fx/blood_alembic01/blood_alembic01.abc" );
	if ( s_blood_abc_models[ 0 ] == 0 ) {
		s_blood_abc_models[ 0 ] = clgi.R_RegisterModel( "models/fx/blood_alembic01.abc" );
	}
	for ( int32_t i = 1; i < 4; i++ ) {
		s_blood_abc_models[ i ] = s_blood_abc_models[ 0 ];
	}

	clgi.Print( PRINT_ALL, "[CLGame]: CLG_PrecacheHeroBlood loaded handle %d\n", s_blood_abc_models[ 0 ] );
}

/**
*	@brief	Shutdown the hero blood splatter subsystem and release assets.
**/
void CLG_FX_BloodHero_Shutdown( void ) {
	/**
	*	Deactivate all splatters and invalidate model handles.
	**/
	// Reset all splatter slots so stale handles are never referenced after map/game shutdown.
	for ( int32_t i = 0; i < MAX_HERO_BLOOD_SPLATS; i++ ) {
		s_hero_splats[ i ].active = false;
		s_hero_splats[ i ].model = 0;
	}
	// Clear cached model handles across all variation slots.
	for ( int32_t i = 0; i < 4; i++ ) {
		s_blood_abc_models[ i ] = 0;
	}
	s_hero_active_count = 0;
}

/**
*	@brief	Spawn a 3D Alembic hero blood splatter at the specified impact location.
*	@param	origin			World-space point of impact.
*	@param	normal			Surface normal or impact reflection direction.
*	@param	velocity		Impact velocity vector to orient splatter trajectory.
*	@param	hit_entity_num	Entity struck (0 = worldspawn, >0 = bmodel or actor).
*	@param	scale			Scale multiplier for splatter volume.
*	@return Pointer to the spawned splatter, or nullptr if rejected.
**/
clg_hero_blood_splat_t *CLG_FX_BloodHero_Spawn( const Vector3 &origin, const Vector3 &normal, const Vector3 &velocity, const int32_t hit_entity_num, const float scale ) {
	/**
	*	Acquire the next FIFO ring buffer slot in O(1) time.
	**/
	const uint16_t slot_idx = static_cast<uint16_t>( s_hero_splat_head );
	s_hero_splat_head = ( s_hero_splat_head + 1 ) % MAX_HERO_BLOOD_SPLATS;

	clg_hero_blood_splat_t &splat = s_hero_splats[ slot_idx ];

	// If slot was previously active, deactivate it from dense list in O(1).
	if ( splat.active ) {
		Hero_DeactivateSlot( slot_idx );
	}

	/**
	*	Select random model variation and resolve actual authored Alembic frame count.
	**/
	const int32_t model_idx = rand() % 4;
	qhandle_t selected_model = s_blood_abc_models[ model_idx ];
	// Fall back to slot 0 if this variation slot is empty.
	if ( selected_model == 0 ) {
		selected_model = s_blood_abc_models[ 0 ];
	}
	// If still 0, perform on-demand precache fallback.
	if ( selected_model == 0 ) {
		CLG_PrecacheHeroBlood();
		selected_model = s_blood_abc_models[ 0 ];
	}

	// Query model metadata so animation playback matches the actual .abc frame count.
	const model_t *model_data = ( selected_model != 0 ) ? clgi.R_GetModelDataForHandle( selected_model ) : nullptr;
	const int32_t authored_frames = ( model_data != nullptr && model_data->numframes > 0 ) ? model_data->numframes : 23;

	// Stamp spawn time from real-time millisecond clock and play back the Alembic sequence at 24 FPS
	// so fluid simulation unfolds smoothly and visibly over ~950 ms instead of flashing past at 60 FPS.
	splat.active = true;
	splat.attached_entity = hit_entity_num;
	splat.model = selected_model;
	splat.spawn_time_ms = ( clgi.GetRealTime != nullptr ) ? static_cast<int64_t>( clgi.GetRealTime() ) : 0;
	splat.anim_duration_ms = std::max( 16, static_cast<int32_t>( ( static_cast<float>( authored_frames ) * 1000.0f ) / 62.0f + 0.5f ) );
	splat.total_frames = authored_frames;
	splat.current_frame = 0.0f;
	splat.current_alpha = 1.0f;
	splat.current_scale = scale;
	splat.scale = scale;
	splat.impact_normal = normal;
	splat.velocity = velocity;

	/**
	*	Attachment calculation: static world vs moving brush entity.
	**/
	if ( hit_entity_num == ENTITYNUM_WORLD ) {
		splat.world_origin = origin;
		Vector3 forward = QM_Vector3Normalize( velocity );
		// Fall back to surface normal if incoming velocity is degenerate.
		if ( QM_Vector3LengthSqr( forward ) < 0.001f ) {
			forward = normal;
		}
		splat.world_angles = QM_Vector3ToAngles( forward );
		// Apply randomized axial roll rotation so each splatter instance presents a unique 3D silhouette.
		splat.world_angles.z = frand() * 360.0f;
	} else {
		// Project impact point into mover's local coordinate frame for frame-to-frame tracking.
		CLG_FX_WorldToEntityLocal( hit_entity_num, origin, normal, &splat.local_origin, &splat.local_normal );
		splat.world_origin = origin;
		splat.world_angles = QM_Vector3ToAngles( normal );
		// Apply randomized axial roll rotation so each splatter instance presents a unique 3D silhouette.
		splat.world_angles.z = frand() * 360.0f;
	}

	/**
	*	Register in dense active array in O(1) time.
	**/
	Hero_ActivateSlot( slot_idx );

	return &splat;
}

/**
*	@brief	Per-frame update of hero blood splatters with O(K) active traversal.
**/
void CLG_FX_BloodHero_Frame( void ) {
	/**
	*	Early exit if no splatters are active or real-time clock is unavailable.
	**/
	// Skip iteration completely when no hero splatters are live or real-time clock is missing.
	if ( s_hero_active_count == 0 || clgi.GetRealTime == nullptr ) {
		return;
	}

	const int64_t current_time = static_cast<int64_t>( clgi.GetRealTime() );

	/**
	*	Strictly iterate over active elements in reverse for safe O(1) deactivation.
	**/
	for ( int32_t a = static_cast<int32_t>( s_hero_active_count ) - 1; a >= 0; a-- ) {
		const uint16_t slot_idx = s_hero_active_indices[ a ];
		clg_hero_blood_splat_t &splat = s_hero_splats[ slot_idx ];

		const int64_t elapsed_ms = std::max<int64_t>( 0, current_time - splat.spawn_time_ms );
		const int32_t max_frame = std::max( 1, splat.total_frames ) - 1;
		constexpr int32_t fade_duration_ms = HERO_BLOOD_FADE_DURATION_MS;
		const int32_t total_lifetime_ms = splat.anim_duration_ms + fade_duration_ms;

		/**
		*	Deactivate slots exceeding total sequence duration plus fade-out dissolve.
		**/
		if ( elapsed_ms >= total_lifetime_ms ) {
			Hero_DeactivateSlot( slot_idx );
			continue;
		}

		/**
		*	Animation frame progression: linear playback across authored frames, holding
		*	the final frame during the subsequent fade-out dissolve.
		**/
		if ( elapsed_ms < splat.anim_duration_ms ) {
			const float anim_progress = static_cast<float>( elapsed_ms ) / static_cast<float>( splat.anim_duration_ms );
			splat.current_frame = anim_progress * static_cast<float>( max_frame );
		} else {
			splat.current_frame = static_cast<float>( max_frame );
		}

		/**
		*	Fade-out dissolve & scale decay: smooth Hermite transition over the final 100 ms.
		**/
		const int32_t fade_start_ms = std::max( 0, splat.anim_duration_ms - 20 );
		if ( elapsed_ms > fade_start_ms ) {
			const float fade_progress = std::clamp(
				static_cast<float>( elapsed_ms - fade_start_ms ) / static_cast<float>( fade_duration_ms ),
				0.0f,
				1.0f
			);
			const float smooth_fade = 1.0f - ( fade_progress * fade_progress * ( 3.0f - 2.0f * fade_progress ) );
			splat.current_alpha = smooth_fade;
			splat.current_scale = splat.scale * ( 0.60f + ( 0.40f * smooth_fade ) );
		} else {
			splat.current_alpha = 1.0f;
			splat.current_scale = splat.scale;
		}

		/**
		*	Reproject mover-attached splatters into current world coordinates.
		**/
		if ( splat.attached_entity != ENTITYNUM_WORLD ) {
			const bool entity_valid = CLG_FX_EntityLocalToWorld(
				splat.attached_entity,
				splat.local_origin,
				splat.local_normal,
				&splat.world_origin,
				&splat.impact_normal
			);
			// Deactivate splatter immediately if its attached entity disappeared.
			if ( !entity_valid ) {
				Hero_DeactivateSlot( slot_idx );
				continue;
			}
			const float current_roll = splat.world_angles.z;
			splat.world_angles = QM_Vector3ToAngles( splat.impact_normal );
			splat.world_angles.z = current_roll;
		}

		/**
		*	Sub-frame velocity advection: smoothly advect the render origin along splatter
		*	velocity during fractional sub-frame intervals and duplicate hold frames.
		**/
		const int32_t frame_idx = std::clamp( static_cast<int32_t>( splat.current_frame ), 0, max_frame );
		const float subframe = splat.current_frame - static_cast<float>( frame_idx );
		const bool is_duplicate_frame = ( splat.total_frames <= 14 )
			? ( frame_idx == 2 || frame_idx == 5 || frame_idx == 8 || frame_idx == 11 )
			: ( frame_idx == 3 || frame_idx == 5 || frame_idx == 8 || frame_idx == 10 ||
			    frame_idx == 13 || frame_idx == 15 || frame_idx == 18 );
		constexpr float frame_time_s = 1.0f / 60.0f;
		const float advection_time_s = ( ( is_duplicate_frame ? 1.0f : 0.0f ) + subframe ) * frame_time_s;
		const Vector3 advection_offset = splat.velocity * advection_time_s;
		const Vector3 render_origin = splat.world_origin + advection_offset;

		/**
		*	Construct entity_t and submit to refresh renderer.
		**/
		if ( splat.model != 0 ) {
			entity_t ent{};
			// Assign a stable nonzero entity ID above packet entities for VKPT temporal history tracking.
			ent.id = MAX_ENTITIES + static_cast<int32_t>( slot_idx ) + 1;
			ent.model = splat.model;
			VectorSet( ent.origin, render_origin.x, render_origin.y, render_origin.z );
			VectorSet( ent.angles, splat.world_angles.x, splat.world_angles.y, splat.world_angles.z );
			// Disable topological morphing between discrete .abc frames by matching frame and oldframe.
			ent.frame = frame_idx;
			ent.oldframe = frame_idx;
			ent.backlerp = 0.0f;
			VectorSet( ent.oldorigin, splat.world_origin.x, splat.world_origin.y, splat.world_origin.z );
			ent.scale = splat.current_scale;

			// Enable alpha blending if faded.
			if ( splat.current_alpha < 0.99f ) {
				ent.flags = RF_TRANSLUCENT;
				ent.alpha = splat.current_alpha;
			}

			clgi.V_AddEntity( &ent );
		}
	}
}

/**
*	@brief		Spawn 3D Alembic fluid mesh animations along a moving gib's trajectory.
*	@details	When a gib is thrown, launches multiple Alembic blood splatter animations
*				following the gib's flight velocity, and continues spawning droplets along
*				its path until the gib slows or comes to rest.
*	@param	packetEntity	Client entity representing the flying gib.
*	@param	currentOrigin	Current lerped world-space origin of the gib.
**/
void CLG_FX_GibBloodTrail( centity_t *packetEntity, const Vector3 &currentOrigin ) {
	/**
	*	Sanity checks: validate entity pointer.
	**/
	if ( packetEntity == nullptr ) {
		return;
	}

	/**
	*	Calculate frame-to-frame displacement and movement speed.
	**/
	const Vector3 moveDelta = QM_Vector3Subtract( currentOrigin, packetEntity->lerpOrigin );
	const float moveDist = QM_Vector3Length( moveDelta );

	// Determine effective frame delta time for instantaneous velocity estimation.
	const float frameTimeSec = ( clgi.frame_time_ms > 0 )
		? ( static_cast<float>( clgi.frame_time_ms ) * 0.001f )
		: ( 1.0f / 60.0f );
	const float gibSpeed = ( frameTimeSec > 0.0001f ) ? ( moveDist / frameTimeSec ) : 0.0f;

	// Determine movement direction vector.
	Vector3 moveDir = ( moveDist > 0.001f )
		? QM_Vector3Normalize( moveDelta )
		: Vector3{ 0.0f, 0.0f, -1.0f };

	// Fallback to server snapshot displacement if frame lerp delta is degenerate (e.g. at initial launch).
	if ( moveDist <= 0.001f ) {
		const Vector3 snapDelta = QM_Vector3Subtract( packetEntity->current.origin, packetEntity->prev.origin );
		const float snapDist = QM_Vector3Length( snapDelta );
		if ( snapDist > 0.1f ) {
			moveDir = QM_Vector3Normalize( snapDelta );
		}
	}

	// Blood splatter decal materials for ground drip pooling.
	static constexpr sg_decal_material_hash_t bloodMaterials[] = {
		SG_DECAL_MATERIAL_HASH_BLOOD_FLESH,
		SG_DECAL_MATERIAL_HASH_BLOOD_FLESH_02,
		SG_DECAL_MATERIAL_HASH_BLOOD_FLESH_03
	};

	/**
	*	1. Initial Throw: When the gib is first launched with motion, spawn multiple
	*	   Alembic blood mesh animations bursting along the throw trajectory.
	**/
	if ( packetEntity->gib_blood_splat_count == 0 && ( moveDist > 0.05f || gibSpeed > 30.0f ) ) {
		// Calculate tangent basis for angular divergence.
		vec3_t forwardArr = { moveDir.x, moveDir.y, moveDir.z };
		vec3_t rightArr = {};
		vec3_t upArr = {};
		MakeNormalVectors( forwardArr, rightArr, upArr );
		const Vector3 rightVec( rightArr );
		const Vector3 upVec( upArr );

		// Spawn 2 to 3 fluid mesh splatters at throw launch popping outwards from the launch point.
		const int32_t numThrowSplatters = 2 + ( ( Q_rand() % 100 ) < 50 ? 1 : 0 );
		for ( int32_t i = 0; i < numThrowSplatters; i++ ) {
			Vector3 splatDir = moveDir;
			float splatScale = 0.85f + ( frand() * 0.35f );
			float splatSpeed = 30.0f + ( frand() * 15.0f );
			Vector3 splatOrg = currentOrigin;

			if ( i > 0 ) {
				// Secondary splatters: deflect slightly off primary trajectory axis (~15-30 deg).
				const float spread = 0.25f + ( frand() * 0.20f );
				const float theta = frand() * 6.2831853f;
				splatDir = QM_Vector3Normalize( moveDir + ( rightVec * cosf( theta ) * spread ) + ( upVec * sinf( theta ) * spread ) );
				splatScale = 0.70f + ( frand() * 0.30f );
				splatOrg = currentOrigin + Vector3{ crand() * 3.0f, crand() * 3.0f, crand() * 3.0f };
			}

			const Vector3 splatVel = splatDir * splatSpeed;
			CLG_FX_BloodHero_Spawn( splatOrg, splatDir, splatVel, ENTITYNUM_WORLD, splatScale );
		}

		// Ground drip decal beneath throw site.
		const Vector3 floorTraceEnd = currentOrigin + Vector3{ 0.0f, 0.0f, -96.0f };
		const cm_trace_t floorTrace = CLG_Clip( currentOrigin, nullptr, nullptr, floorTraceEnd, nullptr, CM_CONTENTMASK_SOLID );
		if ( floorTrace.fraction < 1.0f && !floorTrace.allsolid && !floorTrace.startsolid && floorTrace.plane.normal[ 2 ] > 0.6f ) {
			sg_decal_spawn_params_t decal_params = {};
			VectorCopy( floorTrace.endpos, decal_params.origin );
			VectorCopy( floorTrace.plane.normal, decal_params.normal );
			decal_params.materialHash = bloodMaterials[ Q_rand() % 3 ];
			decal_params.surfaceClass = SG_DECAL_SURFACE_FLESH;
			decal_params.radius = 7.0f + ( frand() * 3.0f );
			decal_params.depth = 6.0f;
			decal_params.rotationRadians = frand() * 6.2831853f;
			decal_params.lifeSeconds = 20.0f;
			decal_params.fadeInSeconds = 0.04f;
			decal_params.fadeOutSeconds = 0.40f;
			decal_params.hitEntityNumber = ENTITYNUM_WORLD;
			decal_params.flags = SG_DECAL_FLAG_DYNAMIC;
			(void)CLG_Decals_QueueSpawn( decal_params );
		}

		packetEntity->gib_blood_splat_count += numThrowSplatters;
		packetEntity->gib_distance_accum = 0.0f;
		return;
	}

	/**
	*	2. Flight Trajectory: Whilst the gib moves towards its direction by velocity,
	*	   spawn Alembic fluid mesh animations along its flight path.
	**/
	if ( gibSpeed < 35.0f || moveDist < 0.1f ) {
		// Gib is stationary or decelerated to rest on the ground; cease emission.
		return;
	}

	// Maximum splatters per single gib entity flight to preserve ring buffer headroom.
	constexpr int32_t MAX_GIB_SPLATTERS = 12;
	if ( packetEntity->gib_blood_splat_count >= MAX_GIB_SPLATTERS ) {
		return;
	}

	packetEntity->gib_distance_accum += moveDist;
	// Emit every 18 units of travel distance along velocity path for continuous trailing droplets.
	constexpr float GIB_SPAWN_INTERVAL_DIST = 18.0f;
	if ( packetEntity->gib_distance_accum >= GIB_SPAWN_INTERVAL_DIST ) {
		packetEntity->gib_distance_accum -= GIB_SPAWN_INTERVAL_DIST;

		const float splatScale = 0.65f + ( frand() * 0.30f );
		const float splatSpeed = 20.0f + ( frand() * 12.0f );
		const Vector3 splatVel = ( moveDir * splatSpeed ) + Vector3{ crand() * 6.0f, crand() * 6.0f, -8.0f };

		CLG_FX_BloodHero_Spawn( currentOrigin, moveDir, splatVel, ENTITYNUM_WORLD, splatScale );
		packetEntity->gib_blood_splat_count++;

		// Cast a downward floor trace from the in-flight position to drop intermittent drip decals.
		const Vector3 floorTraceEnd = currentOrigin + Vector3{ 0.0f, 0.0f, -96.0f };
		const cm_trace_t floorTrace = CLG_Clip( currentOrigin, nullptr, nullptr, floorTraceEnd, nullptr, CM_CONTENTMASK_SOLID );
		if ( floorTrace.fraction < 1.0f && !floorTrace.allsolid && !floorTrace.startsolid && floorTrace.plane.normal[ 2 ] > 0.6f ) {
			sg_decal_spawn_params_t decal_params = {};
			VectorCopy( floorTrace.endpos, decal_params.origin );
			VectorCopy( floorTrace.plane.normal, decal_params.normal );
			decal_params.materialHash = bloodMaterials[ Q_rand() % 3 ];
			decal_params.surfaceClass = SG_DECAL_SURFACE_FLESH;
			decal_params.radius = 5.0f + ( frand() * 2.5f );
			decal_params.depth = 6.0f;
			decal_params.rotationRadians = frand() * 6.2831853f;
			decal_params.lifeSeconds = 20.0f;
			decal_params.fadeInSeconds = 0.04f;
			decal_params.fadeOutSeconds = 0.40f;
			decal_params.hitEntityNumber = ENTITYNUM_WORLD;
			decal_params.flags = SG_DECAL_FLAG_DYNAMIC;
			(void)CLG_Decals_QueueSpawn( decal_params );
		}
	}
}
