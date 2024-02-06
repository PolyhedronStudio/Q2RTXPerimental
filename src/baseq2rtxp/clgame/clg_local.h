/********************************************************************
*
*
*	ClientGame: Local definitions for Client Game module
*
*
********************************************************************/
#include "shared/shared.h"
#include "shared/list.h"

// Should already have been defined by CMake for this ClientGame target.
// 
// Define CLGAME_INCLUDE so that clgame.h does not define the
// short, server-visible gclient_t and edict_t structures,
// because we define the full size ones in this file
#ifndef CLGAME_INCLUDE
#define CLGAME_INCLUDE
#endif
#include "shared/clgame.h"

// Extern here right after including shared/clgame.h
extern clgame_import_t clgi;
extern clgame_export_t globals;

// SharedGame includes:
#include "../sharedgame/sg_shared.h"

// extern times.
extern sg_time_t FRAME_TIME_S;
extern sg_time_t FRAME_TIME_MS;

// TODO: Fix the whole max shenanigan in shared.h,  because this is wrong...
#undef max

// Just to, hold time, forever.
constexpr sg_time_t HOLD_FOREVER = sg_time_t::from_ms( std::numeric_limits<int64_t>::max( ) );



/**
* 
*	CVars
* 
**/
#if USE_DEBUG
extern cvar_t *developer;
#endif
extern cvar_t *cl_predict;
extern cvar_t *cl_running;
extern cvar_t *cl_paused;
extern cvar_t *sv_running;
extern cvar_t *sv_paused;

extern cvar_t *cl_footsteps;
extern cvar_t *cl_rollhack;
extern cvar_t *cl_noglow;
extern cvar_t *cl_gibs;

extern cvar_t *info_password;
extern cvar_t *info_spectator;
extern cvar_t *info_name;
extern cvar_t *info_skin;
extern cvar_t *info_rate;// WID: C++20: Needed for linkage.
extern cvar_t *info_fov;
extern cvar_t *info_msg;
extern cvar_t *info_hand;
extern cvar_t *info_gender;
extern cvar_t *info_uf;

// Cheesy workaround for these two cvars initialized in client FX_Init
extern cvar_t *cvar_pt_particle_emissive;
extern cvar_t *cl_particle_num_factor;



/**
* 
*	Other.
* 
**/
extern centity_t *clg_entities;



/******************************************************************
* 
*	Q2RE: Random Number Utilities
* 
*******************************************************************/
extern std::mt19937 mt_rand;

// uniform float [0, 1)
[[nodiscard]] inline float frandom( ) {
	return std::uniform_real_distribution<float>( )( mt_rand );
}

// uniform float [min_inclusive, max_exclusive)
[[nodiscard]] inline float frandom( float min_inclusive, float max_exclusive ) {
	return std::uniform_real_distribution<float>( min_inclusive, max_exclusive )( mt_rand );
}

// uniform float [0, max_exclusive)
[[nodiscard]] inline float frandom( float max_exclusive ) {
	return std::uniform_real_distribution<float>( 0, max_exclusive )( mt_rand );
}

// uniform time [min_inclusive, max_exclusive)
[[nodiscard]] inline sg_time_t random_time( sg_time_t min_inclusive, sg_time_t max_exclusive ) {
	return sg_time_t::from_ms( std::uniform_int_distribution<int64_t>( min_inclusive.milliseconds( ), max_exclusive.milliseconds( ) )( mt_rand ) );
}

// uniform time [0, max_exclusive)
[[nodiscard]] inline sg_time_t random_time( sg_time_t max_exclusive ) {
	return sg_time_t::from_ms( std::uniform_int_distribution<int64_t>( 0, max_exclusive.milliseconds( ) )( mt_rand ) );
}

// uniform float [-1, 1)
// note: closed on min but not max
// to match vanilla behavior
[[nodiscard]] inline float crandom( ) {
	return std::uniform_real_distribution<float>( -1.f, 1.f )( mt_rand );
}

// uniform float (-1, 1)
[[nodiscard]] inline float crandom_open( ) {
	return std::uniform_real_distribution<float>( std::nextafterf( -1.f, 0.f ), 1.f )( mt_rand );
}

// raw unsigned int32 value from random
[[nodiscard]] inline uint32_t irandom( ) {
	return mt_rand( );
}

// uniform int [min, max)
// always returns min if min == (max - 1)
// undefined behavior if min > (max - 1)
[[nodiscard]] inline int32_t irandom( int32_t min_inclusive, int32_t max_exclusive ) {
	if ( min_inclusive == max_exclusive - 1 )
		return min_inclusive;

	return std::uniform_int_distribution<int32_t>( min_inclusive, max_exclusive - 1 )( mt_rand );
}

// uniform int [0, max)
// always returns 0 if max <= 0
// note for Q2 code:
// - to fix rand()%x, do irandom(x)
// - to fix rand()&x, do irandom(x + 1)
[[nodiscard]] inline int32_t irandom( int32_t max_exclusive ) {
	if ( max_exclusive <= 0 )
		return 0;

	return irandom( 0, max_exclusive );
}

// uniform random index from given container
template<typename T>
[[nodiscard]] inline int32_t random_index( const T &container ) {
	return irandom( std::size( container ) );
}

// uniform random element from given container
template<typename T>
[[nodiscard]] inline auto random_element( T &container ) -> decltype( *std::begin( container ) ) {
	return *( std::begin( container ) + random_index( container ) );
}

// flip a coin
[[nodiscard]] inline bool brandom( ) {
	return irandom( 2 ) == 0;
}



/******************************************************************
* 
* GameMode
* 
*******************************************************************/



/******************************************************************
* 
* Client Game Entity
* 
*******************************************************************/
typedef struct centity_s {
	//! Current(and thus last acknowledged and received) entity state.
	entity_state_t	current;
	//! Previous entity state. Will always be valid, but might be just a copy of the current state.
	entity_state_t	prev;

	//! Modelspace Mins/Maxs of Bounding Box.
	vec3_t	mins, maxs;
	//! Worldspace absolute Mins/Maxs/Size of Bounding Box.
	vec3_t	absmin, absmax, size;

	//! The (last) serverframe this entity was in. If not current, this entity isn't in the received frame.
	int64_t	serverframe;

	//! For diminishing grenade trails.
	int32_t	trailcount;         // for diminishing grenade trails
	//! for trails (variable hz)
	vec3_t	lerp_origin;

	//! Used for CL_FlyEffect and CL_TrapParticles to determine when to stop the effect.
	int32_t	fly_stoptime;

	//! Entity id for the refresh(render) entity.
	int32_t	id;

	// WID: 40hz
	int32_t	current_frame, last_frame;
	// Server Time of receiving the current frame.
	int64_t	frame_servertime;

	// Server Time of receiving a (state.renderfx & SF_STAIR_STEP) entity.
	int64_t	step_servertime;
	// Actual height of the step taken.
	float	step_height;
	// WID: 40hz

	// the game dll can add anything it wants after
	// this point in the structure
	int64_t someTestVar;
	int64_t someTestVar2;
} centity_t;

/**
*	Memory tag IDs to allow dynamic memory to be cleaned up.
**/
#define TAG_CLGAME			777 // Clear when unloading the dll.
#define TAG_CLGAME_LEVEL	778 // Clear when loading a new level.



/*
*
*	clg_effects.cpp	
* 
*/
/**
*   @brief
**/
void CLG_ClearEffects( void );
/**
*   @brief
**/
void CLG_InitEffects( void );

// Wall Impact Puffs.
void CLG_ParticleEffect( const vec3_t org, const vec3_t dir, int color, int count );
void CLG_ParticleEffectWaterSplash( const vec3_t org, const vec3_t dir, int color, int count );
void CLG_BloodParticleEffect( const vec3_t org, const vec3_t dir, int color, int count );
void CLG_ParticleEffect2( const vec3_t org, const vec3_t dir, int color, int count );
void CLG_TeleporterParticles( const vec3_t org );
void CLG_LogoutEffect( const vec3_t org, int type );
void CLG_ItemRespawnParticles( const vec3_t org );
void CLG_ExplosionParticles( const vec3_t org );
void CLG_BigTeleportParticles( const vec3_t org );
void CLG_BlasterParticles( const vec3_t org, const vec3_t dir );
void CLG_BlasterTrail( const vec3_t start, const vec3_t end );
void CLG_FlagTrail( const vec3_t start, const vec3_t end, int color );
void CLG_DiminishingTrail( const vec3_t start, const vec3_t end, centity_t *old, int flags );
void CLG_RocketTrail( const vec3_t start, const vec3_t end, centity_t *old );
void CLG_OldRailTrail( void );
void CLG_BubbleTrail( const vec3_t start, const vec3_t end );
//static void CL_FlyParticles( const vec3_t origin, int count );
void CLG_FlyEffect( centity_t *ent, const vec3_t origin );
void CLG_BfgParticles( entity_t *ent );
//FIXME combined with CL_ExplosionParticles
void CLG_BFGExplosionParticles( const vec3_t org );
void CLG_TeleportParticles( const vec3_t org );



/*
*
*	effects/clg_fx_dynamiclights.cpp
*
*/
/**
*   @brief
**/
void CLG_ClearDlights( void );
/**
*   @brief
**/
cdlight_t *CLG_AllocDlight( const int32_t key );
/**
*   @brief
**/
void CLG_AddDLights( void );



/*
*
*	effects/clg_fx_lightstyles.cpp
*
*/
/**
*	@brief
**/
void CLG_ClearLightStyles( void );
/**
*   @brief
**/
void CLG_SetLightStyle( const int32_t index, const char *s );
/**
*   @brief
**/
void CLG_AddLightStyles( void );



/*
*
*	effects/clg_fx_muzzleflash.cpp & effects/clg_fx_muzzleflash2.cpp
*
*/
/**
*   @brief  Handles the parsed client muzzleflash effects.
**/
void CLG_MuzzleFlash( void );
/**
*   @brief  Handles the parsed entities/monster muzzleflash effects.
**/
void CLG_MuzzleFlash2( void );



/*
*
*	effects/clg_fx_newfx.cpp
*
*/
void CLG_Flashlight( int ent, const vec3_t pos );
void CLG_ColorFlash( const vec3_t pos, int ent, int intensity, float r, float g, float b );
void CLG_DebugTrail( const vec3_t start, const vec3_t end );
void CLG_ForceWall( const vec3_t start, const vec3_t end, int color );
void CLG_BubbleTrail2( const vec3_t start, const vec3_t end, int dist );
void CLG_Heatbeam( const vec3_t start, const vec3_t forward );
void CLG_ParticleSteamEffect( const vec3_t org, const vec3_t dir, int color, int count, int magnitude );
void CLG_ParticleSteamEffect2( cl_sustain_t *self );
void CLG_TrackerTrail( const vec3_t start, const vec3_t end, int particleColor );
void CLG_Tracker_Shell( const vec3_t origin );
void CLG_MonsterPlasma_Shell( const vec3_t origin );
void CLG_Widowbeamout( cl_sustain_t *self );
void CLG_Nukeblast( cl_sustain_t *self );
void CLG_WidowSplash( void );
void CLG_TagTrail( const vec3_t start, const vec3_t end, int color );
void CLG_ColorExplosionParticles( const vec3_t org, int color, int run );
void CLG_ParticleSmokeEffect( const vec3_t org, const vec3_t dir, int color, int count, int magnitude );
void CLG_BlasterParticles2( const vec3_t org, const vec3_t dir, unsigned int color );
void CLG_BlasterTrail2( const vec3_t start, const vec3_t end );
void CLG_IonripperTrail( const vec3_t start, const vec3_t ent );
void CLG_TrapParticles( centity_t *ent, const vec3_t origin );
void CLG_ParticleEffect3( const vec3_t org, const vec3_t dir, int color, int count );



/*
*
*	effects/clg_fx_particles.cpp
*
*/
/**
*   @brief
**/
void CLG_ClearParticles( void );
/**
*   @brief
**/
cparticle_t *CLG_AllocParticle( void );
/**
*	@brief
**/
void CLG_AddParticles( void );



/*
*
*	clg_entities.cpp
*
*/
#if USE_DEBUG
/**
*	@brief	For debugging problems when out - of - date entity origin is referenced
**/
void CLG_CheckEntityPresent( int32_t entityNumber, const char *what );
#endif
/**
*	@return		The entity bound to the client's view.
*	@remarks	(Can be the one we're chasing, instead of the player himself.)
**/
centity_t *CLG_Self( void );
/**
 * @return True if the specified entity is bound to the local client's view.
 */
const bool CLG_IsSelf( const centity_t *ent );
/**
*	@brief	
**/
void CLG_AddPacketEntities( void );



/*
*
*	clg_parse.cpp
*
*/
/**
*	@brief	Called by the client when it receives a configstring update, this
*			allows us to interscept it and respond to it. If not interscepted the 
*			control is passed back to the client again.
* 
*	@return	True if we interscepted one succesfully, false otherwise.
**/
const bool PF_UpdateConfigString( const int32_t index );
/**
*	@brief	Called by the client BEFORE all server messages have been parsed.
**/
void PF_StartServerMessage( const bool isDemoPlayback );
/**
*	@brief	Called by the client AFTER all server messages have been parsed.
**/
void PF_EndServerMessage( const bool isDemoPlayback );
/**
*	@brief	Called by the client when it does not recognize the server message itself,
*			so it gives the client game a chance to handle and respond to it.
*	@return	True if the message was handled properly. False otherwise.
**/
const bool PF_ParseServerMessage( const int32_t serverMessage );
/**
*	@brief	A variant of ParseServerMessage that skips over non-important action messages,
*			used for seeking in demos.
*	@return	True if the message was handled properly. False otherwise.
**/
const bool PF_SeekDemoMessage( const int32_t serverMessage );
/**
*	@brief	Parsess entity events.
**/
void PF_ParseEntityEvent( const int32_t entityNumber );



/*
*
*	clg_precache.cpp
*
*/
typedef struct precached_media_s {
	//
	// Models:
	//
	qhandle_t cl_mod_explode;
	qhandle_t cl_mod_smoke;
	qhandle_t cl_mod_flash;
	qhandle_t cl_mod_parasite_segment;
	qhandle_t cl_mod_grapple_cable;
	qhandle_t cl_mod_explo4;
	qhandle_t cl_mod_explosions[ 4 ];
	qhandle_t cl_mod_bfg_explo;
	qhandle_t cl_mod_powerscreen;
	qhandle_t cl_mod_laser;
	qhandle_t cl_mod_dmspot;

	qhandle_t cl_mod_lightning;
	qhandle_t cl_mod_heatbeam;
	qhandle_t cl_mod_explo4_big;

	// 
	// Sound Effects:
	//
	qhandle_t cl_sfx_ric1;
	qhandle_t cl_sfx_ric2;
	qhandle_t cl_sfx_ric3;
	qhandle_t cl_sfx_lashit;
	qhandle_t cl_sfx_flare;
	qhandle_t cl_sfx_spark5;
	qhandle_t cl_sfx_spark6;
	qhandle_t cl_sfx_spark7;
	qhandle_t cl_sfx_railg;
	qhandle_t cl_sfx_rockexp;
	qhandle_t cl_sfx_grenexp;
	qhandle_t cl_sfx_watrexp;

	qhandle_t cl_sfx_footsteps[ 4 ];

	qhandle_t cl_sfx_lightning;
	qhandle_t cl_sfx_disrexp;

	//
	// Other:
	//
	// ...
} precached_media_t;

//! Stores qhandles to all precached client game media.
extern precached_media_t precache;

/**
*   @brief
**/
void PF_RegisterTEntModels( void );
/**
*   @brief
**/
void PF_RegisterTEntSounds( void );



/*
*
*	clg_predict.cpp
*
*/
/**
*   @brief  Will shuffle current viewheight into previous, update the current viewheight, and record the time of changing.
**/
void PF_AdjustViewHeight( const int32_t viewHeight );
/**
*   @brief  Sets the predicted view angles.
**/
void PF_PredictAngles( );
/**
*	@return	False if prediction is not desired for. True if it is.
**/
const bool PF_UsePrediction( );
/**
*   @brief  Performs player movement over the yet unacknowledged 'move command' frames, as well
*           as the pending user move command. To finally store the predicted outcome
*           into the cl.predictedState struct.
**/
void PF_PredictMovement( uint64_t acknowledgedCommandNumber, const uint64_t currentCommandNumber );



/*
*
*	clg_temp_entities.cpp
*
*/
/**
*   @brief
**/
void CLG_InitTEnts( void );
/**
*   @brief
**/
void CLG_ClearTEnts( void );
/**
*   @brief
**/
void CLG_ParseTEnt( void );
/**
*   @brief
**/
void CLG_AddTEnts( void );



/*
*
*	temp_entities/clg_te_beams.cpp
*
*/
/**
*   @brief
**/
void CLG_ClearBeams( void );
/**
*   @brief
**/
void CLG_ParseBeam( const qhandle_t model );
/**
*   @brief
**/
void CLG_ParsePlayerBeam( const qhandle_t model );
/**
*   @brief
**/
void CLG_AddBeams( void );
/**
*   @brief  Draw player locked beams. Currently only used by the plasma beam.
**/
void CLG_AddPlayerBeams( void );



/*
*
*	temp_entities/clg_te_explosions.cpp
*
*/
extern cvar_t *cl_disable_particles;
extern cvar_t *cl_disable_explosions;
extern cvar_t *cl_explosion_sprites;
extern cvar_t *cl_explosion_frametime;
extern cvar_t *cl_dlight_hacks;

extern cvar_t *gibs;
/**
*   @brief
**/
void CLG_ClearExplosions( void );
/**
*   @brief
**/
explosion_t *CLG_AllocExplosion( void );
/**
*   @brief
**/
explosion_t *CLG_PlainExplosion( bool big );
/**
*   @brief
**/
void CLG_SmokeAndFlash( const vec3_t origin );
/**
*   @brief
**/
//static void CL_AddExplosionLight( explosion_t *ex, float phase );
/**
*   @brief
**/
void CLG_AddExplosions( void );



/*
*
*	temp_entities/clg_te_lasers.cpp
*
*/
//!
typedef struct {
	vec3_t      start;
	vec3_t      end;

	int32_t     color;
	color_t     rgba;
	int32_t     width;

	int64_t     lifetime, starttime;
} laser_t;
/**
*   @brief
**/
void CLG_ClearLasers( void );
/**
*   @brief
**/
laser_t *CLG_AllocLaser( void );
/**
*   @brief
**/
void CLG_AddLasers( void );
/**
*   @brief
**/
void CLG_ParseLaser( const int32_t colors );



/*
*
*	temp_entities/clg_te_railtrails.cpp
*
*/
/**
*   @brief
**/
void CLG_RailCore( void );
/**
*   @brief
**/
void CLG_RailSpiral( void );
/**
*   @brief
**/
void CLG_RailLights( const color_t color );
/**
*   @brief
**/
void CLG_RailTrail( void );



/*
*
*	temp_entities/clg_te_sustain.cpp
*
*/
/**
*   @brief
**/
void CLG_ClearSustains( void );
/**
*   @brief
**/
cl_sustain_t *CLG_AllocSustain( void );
/**
*   @brief
**/
void CLG_ProcessSustain( void );
/**
*   @brief
**/
void CLG_ParseSteam( void );
/**
*   @brief
**/
void CLG_ParseWidow( void );
/**
*   @brief
**/
void CLG_ParseNuke( void );



/*
*
*	clg_view.cpp
*
*/
/**
*   @brief
**/
void PF_ClearViewScene( void );
/**
*   @brief
**/
void PF_PrepareViewEntites( void );



/**
*	
*	Game/Level Locals:
* 
**/
/**
*	@brief	Stores data that remains accross level switches.
**/
struct game_locals_t {
	//// store latched cvars here that we want to get at often
	//int32_t	maxclients;
	int32_t	maxentities;
};
extern game_locals_t game;

/**
*	@brief	Stores data for the current level session.
**/
struct level_locals_t {
	//! For storing parsed message data that is handled later on by corresponding said
	//! event/effect functions.
	struct {
		// For the 'event' like style messages, store data here so that the later called upon
		// effect(s) functions can access it.
		struct {
			tent_params_t   tempEntity;
			mz_params_t     muzzleFlash;
		} events;
	} parsedMessage;
};
extern level_locals_t level;