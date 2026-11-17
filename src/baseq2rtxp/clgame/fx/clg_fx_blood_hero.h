//! CLGame Hero Blood Splatters (Tier 1: 3D Alembic Fluid Sequences)
/********************************************************************
*
*
*	ClientGame: FX: "clg_fx_blood_hero.h"
*
*	High-fidelity 3D fluid Alembic mesh sequence emitter and ring-buffer
*	lifecycle manager. Renders macro arterial splatters and gib impacts
*	with pre-built static BLAS fast-paths, O(1) circular FIFO queuing,
*	and O(1) swap-and-pop active tracking for zero cold iterations.
*
*
********************************************************************/
#pragma once

#include "sharedgame/sg_shared.h"
#include <cstdint>

// Forward declarations:
struct centity_s;
typedef struct centity_s centity_t;

/**
*	Constants and Configuration
**/
//! Maximum concurrent hero blood splatters in the world
constexpr int32_t MAX_HERO_BLOOD_SPLATS = 64;
//! Half-life duration in milliseconds for blood splatter opacity dissipation after playback completes
constexpr float HERO_BLOOD_HALF_LIFE_MS = 120.0f;
//! Duration in milliseconds for smooth fade-out and scale decay over the final droplet phase
constexpr int32_t HERO_BLOOD_FADE_DURATION_MS = 200;
//! Maximum animation playback frames per Alembic sequence
constexpr int32_t MAX_HERO_BLOOD_FRAMES = 60;

/**
*	@brief	Hero blood splatter descriptor and lifecycle state.
**/
struct clg_hero_blood_splat_t {
	//! Whether this slot is actively occupied and simulated
	bool active = false;
	//! Entity number to which this splatter is attached (0 / ENTITYNUM_WORLD = static worldspawn)
	int32_t attached_entity = ENTITYNUM_WORLD;
	//! World-space origin when attached to worldspawn
	Vector3 world_origin = { 0.0f, 0.0f, 0.0f };
	//! World-space rotation angles when attached to worldspawn
	Vector3 world_angles = { 0.0f, 0.0f, 0.0f };
	//! Entity-local relative origin when attached to moving bmodel or dynamic actor
	Vector3 local_origin = { 0.0f, 0.0f, 0.0f };
	//! Entity-local relative orientation basis when attached to mover
	Vector3 local_normal = { 0.0f, 0.0f, 1.0f };
	//! Current world-space normal for lighting and projection alignment
	Vector3 impact_normal = { 0.0f, 0.0f, 1.0f };
	//! Trajectory velocity vector for physical motion and sub-frame advection
	Vector3 velocity = { 0.0f, 0.0f, 0.0f };
	//! Model handle for the Alembic .abc fluid sequence
	qhandle_t model = 0;
	//! Timestamp in milliseconds when the splatter was spawned
	int64_t spawn_time_ms = 0;
	//! Total duration in milliseconds for the animation phase
	int32_t anim_duration_ms = 1000;
	//! Current floating-point animation frame index
	float current_frame = 0.0f;
	//! Total frames in the Alembic animation sequence
	int32_t total_frames = 23;
	//! Current opacity [0.0..1.0] calculated via exponential half-life decay
	float current_alpha = 1.0f;
	//! Current dynamic scale including fade-out decay
	float current_scale = 1.0f;
	//! Uniform world-space scale factor assigned at spawn
	float scale = 1.0f;
};

/**
*	Lifecycle and Event API
**/

/**
*	@brief	Precache Alembic models and resources for hero blood splatters.
**/
void CLG_PrecacheHeroBlood( void );

/**
*	@brief	Initialize the hero blood splatter subsystem and clear ring buffers.
**/
void CLG_FX_BloodHero_Init( void );

/**
*	@brief	Shutdown the hero blood splatter subsystem and release assets.
**/
void CLG_FX_BloodHero_Shutdown( void );

/**
*	@brief	Spawn a 3D Alembic hero blood splatter at the specified impact location.
*	@param	origin			World-space point of impact.
*	@param	normal			Surface normal or impact reflection direction.
*	@param	velocity		Impact velocity vector to orient splatter trajectory.
*	@param	hit_entity_num	Entity struck (0 = worldspawn, >0 = bmodel or actor).
*	@param	scale			Scale multiplier for splatter volume.
*	@return Pointer to the spawned splatter, or nullptr if rejected.
**/
clg_hero_blood_splat_t *CLG_FX_BloodHero_Spawn( const Vector3 &origin, const Vector3 &normal, const Vector3 &velocity, const int32_t hit_entity_num, const float scale );

/**
*	@brief	Per-frame update of hero blood splatters, handling animation progression,
*			exponential half-life decay, mover tracking, and scene emission.
**/
void CLG_FX_BloodHero_Frame( void );

/**
*	@brief		Spawn 3D Alembic fluid mesh animations along a moving gib's trajectory.
*	@details	When a gib is thrown, launches multiple Alembic blood splatter animations
*			following the gib's flight velocity, and continues spawning droplets along
*			its path until the gib slows or comes to rest.
*	@param	packetEntity	Client entity representing the flying gib.
*	@param	currentOrigin	Current lerped world-space origin of the gib.
**/
void CLG_FX_GibBloodTrail( centity_t *packetEntity, const Vector3 &currentOrigin );
