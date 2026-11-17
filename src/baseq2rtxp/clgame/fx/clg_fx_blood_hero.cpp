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
	// Prefer the blood_alembic01 subfolder path, falling back to legacy bulletShotImpact01 paths if absent.
	s_blood_abc_models[ 0 ] = clgi.R_RegisterModel( "models/fx/blood_alembic01/blood_alembic01.abc" );
	if ( s_blood_abc_models[ 0 ] == 0 ) {
		s_blood_abc_models[ 0 ] = clgi.R_RegisterModel( "models/fx/blood_alembic01.abc" );
	}
	if ( s_blood_abc_models[ 0 ] == 0 ) {
		s_blood_abc_models[ 0 ] = clgi.R_RegisterModel( "models/fx/bulletShotImpact01/bulletShotImpact01.abc" );
	}
	if ( s_blood_abc_models[ 0 ] == 0 ) {
		s_blood_abc_models[ 0 ] = clgi.R_RegisterModel( "models/fx/bulletShotImpact01.abc" );
	}
	s_blood_abc_models[ 1 ] = s_blood_abc_models[ 0 ];
	s_blood_abc_models[ 2 ] = s_blood_abc_models[ 0 ];
	s_blood_abc_models[ 3 ] = s_blood_abc_models[ 0 ];

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

	// Stamp spawn time from real-time millisecond clock and play back the Alembic sequence at 60 FPS.
	splat.active = true;
	splat.attached_entity = hit_entity_num;
	splat.model = selected_model;
	splat.spawn_time_ms = ( clgi.GetRealTime != nullptr ) ? static_cast<int64_t>( clgi.GetRealTime() ) : 0;
	splat.anim_duration_ms = std::max( 16, static_cast<int32_t>( ( static_cast<float>( authored_frames ) * 1000.0f ) / 60.0f + 0.5f ) );
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
