/********************************************************************
*
*
*	ClientGame: Local definitions for Client Game module
*
*
********************************************************************/
#include "shared/shared.h"
#include "shared/util_list.h"

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

extern cvar_t *cl_kickangles;
extern cvar_t *cl_rollhack;
extern cvar_t *cl_noglow;
extern cvar_t *cl_noskins;

extern cvar_t *cl_gibs;

extern cvar_t *cl_gunalpha;
extern cvar_t *cl_gunscale;
extern cvar_t *cl_gun_x;
extern cvar_t *cl_gun_y;
extern cvar_t *cl_gun_z;

extern cvar_t *cl_player_model;
extern cvar_t *cl_thirdperson_angle;
extern cvar_t *cl_thirdperson_range;

extern cvar_t *cl_chat_notify;
extern cvar_t *cl_chat_sound;
extern cvar_t *cl_chat_filter;

extern cvar_t *cl_vwep;

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


/**
*	Client-Game 'client' structure definition: This structure always has to 
*	mirror the 'first part' of the structure defined within the Client.
**/
//typedef struct cclient_s {
//    player_state_t  ps;
//    int32_t			ping;
//	// the game dll can add anything it wants after
//	// this point in the structure
//	//int32_t             clientNum;
//} cclient_t;
/**
*	Client-Game Packet Entity structure definition: This structure always has to
*	mirror the 'first part' of the structure defined within the Client.
**/
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

	/**
	*	The game dll can add anything it wants after this point in the structure.
	**/
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
*   @brief  Stores temp entity data from the last parsed svc_temp_entity message.
**/
typedef struct {
	int32_t type;
	vec3_t  pos1;
	vec3_t  pos2;
	vec3_t  offset;
	vec3_t  dir;
	int32_t count;
	int32_t color;
	int32_t entity1;
	int32_t entity2;
	int64_t time;
} tent_params_t;

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
// Use a static entity ID on some things because the renderer relies on eid to match between meshes
// on the current and previous frames.
static constexpr int32_t RESERVED_ENTITIY_GUN = 1;
static constexpr int32_t RESERVED_ENTITIY_TESTMODEL = 2;
static constexpr int32_t RESERVED_ENTITIY_COUNT = 3;

/**
*   @brief  For debugging problems when out-of-date entity origin is referenced.
**/
#if USE_DEBUG
void CLG_CheckEntityPresent( int32_t entityNumber, const char *what );
#endif
/**
*   @brief  The sound code makes callbacks to the client for entitiy position
*           information, so entities can be dynamically re-spatialized.
**/
void PF_GetEntitySoundOrigin( const int32_t entityNumber, vec3_t org );
///**
//*	@return		A pointer to the entity bound to the client game's view. Unless STAT_CHASE is set this
//*               will return a pointer to the client's own private corresponding entity number slot entity.
//*
//*               (Can be the one we're chasing, instead of the player himself.)Otherwise it'll point
//**/
//centity_t *CLG_Self( void );
///**
//*   @return True if the specified entity is the one that is currently bound to the current local client's view.
//**/
//const bool CLG_IsSelf( const centity_t *ent );
/**
*	@brief	
**/
void CLG_AddPacketEntities( void );



/**
*
*	clg_input.cpp
*
**/
//! Called upon when mouse movement is detected.
void PF_MouseMove( const float deltaX, const float deltaY, const float moveX, const float moveY, const float speed );
//! Called upon to register movement commands.
void PF_RegisterUserInput( void );
/**
*   @brief  Updates msec, angles and builds the interpolated movement vector for local movement prediction.
*           Doesn't touch command forward/side/upmove, these are filled by CL_FinalizeCommand.
**/
void PF_UpdateMoveCommand( const int64_t msec, client_movecmd_t *moveCommand, client_mouse_motion_t *mouseMotion );
/**
*   @brief  Builds the actual movement vector for sending to server. Assumes that msec
*           and angles are already set for this frame by CL_UpdateCommand.
**/
void PF_FinalizeMoveCommand( client_movecmd_t *moveCommand );
/**
*   @brief
**/
void PF_ClearMoveCommand( client_movecmd_t *moveCommand );



/**
*
*	clg_local_entities.cpp
*
**/
// Predeclare, defined a little later.
typedef struct clg_local_entity_s clg_local_entity_t;
//! 'Spawn' local entity class function pointer callback.
typedef void ( *LocalEntity_Precache )( clg_local_entity_t *self, const cm_entity_t *keyValues );
//! 'Spawn' local entity class function pointer callback.
typedef void ( *LocalEntity_Spawn )( clg_local_entity_t *self );
//! 'Think' local entity class function pointer callback.
typedef void ( *LocalEntity_Think )( clg_local_entity_t *self );
//! 'Refresh Frame' local entity class function pointer callback.
typedef void ( *LocalEntity_RefreshFrame )( clg_local_entity_t *self );

/**
*	@brief	Describes the local entity's class type, default callbacks and the 
*			class' specific locals data size.
**/
typedef struct clg_local_entity_class_s {
	//! The actual classname, has to match with the worldspawn's classname key/value in order to spawn.
	const char *classname;

	//! The precache function, called during map load.
	LocalEntity_Precache precache;
	//! The spawn function, called once during spawn time(When Begin_f() has finished.).
	LocalEntity_Spawn spawn;
	//! The 'think' method gets called for each client game logic frame.
	LocalEntity_Think think;
	//! The 'rframe' method gets called for each client refresh frame.
	LocalEntity_RefreshFrame rframe;

	//! The sizeof the class_data.
	size_t class_locals_size;
} clg_local_entity_class_t;

/**
*	@brief	Data structure defined for the client game's own local entity world. The local entities are
*			used to relax the network protocol by allowing them to be used for example:
*				- decorating(using meshes) the world.
*				- emitting particle and/or sounds.
*				- gibs/debris.
**/
typedef struct clg_local_entity_s {
	//! Unique identifier for each entity.
	uint32_t id;
	
	//! A pointer to the entity's class specific data.
	const clg_local_entity_class_t *entityClass;

	//! Points right to the collision model's entity dictionary.
	const cm_entity_t *entityDictionary;

	struct {
		//! The classname.
		const char *classname;
		//! The entity's origin.
		Vector3 origin;
		//! The entity's angles.
		Vector3 angles;

		////! The entities mins/maxs.
		//Vector3 mins, maxs;
		////! The entity's absolute mins maxs and size.
		//Vector3 absMins, absMaxs, size;
		qhandle_t modelIndex;
	} locals;

	//! Precache callback.
	//void ( *Precache )( clg_local_entity_t *self );
	//! Spawn callback.
	//void ( *Spawn )( clg_local_entity_t *self );
	//! Think callback.
	//void ( *Think )( clg_local_entity_t *self );

	//! Will be allocated by precaching for storing classname type specific data.
	void *classLocals;
} clg_local_entity_t;

//! Actual array of local client entities.
extern clg_local_entity_t clg_local_entities[ MAX_CLIENT_ENTITIES ];
//! Current number of local client entities.
extern uint32_t clg_num_local_entities;

/**
*	@brief	Called from the client's Begin command, gives the client game a shot at
*			spawning local client entities so they can join in the image/model/sound
*			precaching context.
**/
void PF_SpawnEntities( const char *mapname, const char *spawnpoint, const cm_entity_t **entities, const int32_t numEntities );



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
const qboolean PF_UpdateConfigString( const int32_t index );
/**
*	@brief	Called by the client BEFORE all server messages have been parsed.
**/
void PF_StartServerMessage( const qboolean isDemoPlayback );
/**
*	@brief	Called by the client AFTER all server messages have been parsed.
**/
void PF_EndServerMessage( const qboolean isDemoPlayback );
/**
*	@brief	Called by the client when it does not recognize the server message itself,
*			so it gives the client game a chance to handle and respond to it.
*	@return	True if the message was handled properly. False otherwise.
**/
const qboolean PF_ParseServerMessage( const int32_t serverMessage );
/**
*	@brief	A variant of ParseServerMessage that skips over non-important action messages,
*			used for seeking in demos.
*	@return	True if the message was handled properly. False otherwise.
**/
const qboolean PF_SeekDemoMessage( const int32_t serverMessage );
/**
*	@brief	Parsess entity events.
**/
void PF_ParseEntityEvent( const int32_t entityNumber );
/**
*	@brief	Parses a clientinfo configstring.
**/
void PF_ParsePlayerSkin( char *name, char *model, char *skin, const char *s );


/*
*
*	clg_precache.cpp
*
*/
typedef struct precached_media_s {
	//
	//	Local Entities:
	//
	//! Acts as a local 'configstring'. The client entities are spawned at the very start of the
	//! Begin command. This leaves us with room to detect if there are any local entities that
	//! should be spawned. This is where the actual paths of those models are stored so we can
	//! load these later on in PF_PrecacheClientModels.
	char localModelPaths[ MAX_MODELS ][ MAX_QPATH ];
	char localSoundPaths[ MAX_SOUNDS ][ MAX_QPATH ];

	//! Stores the model indexes for each local client entity precached model.
	qhandle_t localModels[ MAX_MODELS ];
	int32_t numLocalModels;
	//! Stores the sound indexes for each local client entity precached sound.
	qhandle_t localSoundss[ MAX_SOUNDS ];
	int32_t numLocalSounds;


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
	// View Models: (Moved here from client, was named weapon models but a more generic name is best fit.)
	//
	char	viewModels[ MAX_CLIENTVIEWMODELS ][ MAX_QPATH ];
	int32_t	numViewModels;

	//
	// Other:
	//

	// ...

} precached_media_t;

//! Stores qhandles to all precached client game media.
extern precached_media_t precache;

/**
*	@brief	Called right before loading all received configstring (server-) models.
**/
void PF_PrecacheClientModels( void );
/**
*	@brief	Called right before loading all received configstring (server-) sounds.
**/
void PF_PrecacheClientSounds( void );
/**
*   @brief  Called to precache/update precache of 'View'-models. (Mainly, weapons.)
**/
void PF_PrecacheViewModels( void );
/**
*	@brief	Called to precache client info specific media.
**/
void PF_PrecacheClientInfo( clientinfo_t *ci, const char *s );



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
*   @brief  Checks for prediction if desired. Will determine the error margin
*           between our predicted state and the server returned state. In case
*           the margin is too high, snap back to server provided player state.
**/
void PF_CheckPredictionError( const int64_t frameIndex, const uint64_t commandIndex, const pmove_state_t *in, struct client_movecmd_s *moveCommand, client_predicted_state_t *out );
/**
*   @brief  Sets the predicted view angles.
**/
void PF_PredictAngles( );
/**
*	@return	False if prediction is not desired for. True if it is.
**/
const qboolean PF_UsePrediction( );
/**
*   @brief  Performs player movement over the yet unacknowledged 'move command' frames, as well
*           as the pending user move command. To finally store the predicted outcome
*           into the cl.predictedState struct.
**/
void PF_PredictMovement( uint64_t acknowledgedCommandNumber, const uint64_t currentCommandNumber );



/*
*
*	clg_screen.cpp
*
*
*/
/**
*   @brief  Called for important messages that should stay in the center of the screen
*           for a few moments
**/
void SCR_CenterPrint( const char *str );
/**
*   @brief  Clear the chat HUD.
**/
void SCR_ClearChatHUD_f( void );
/**
*   @brief  Append text to chat HUD.
**/
void SCR_AddToChatHUD( const char *text );
/**
*	@brief
**/
void PF_SCR_Init( void );
/**
*	@brief
**/
void PF_SCR_RegisterMedia( void );
/**
*	@brief
**/
void PF_SCR_ModeChanged( void );
/**
*	@brief
**/
void PF_SCR_SetCrosshairColor( void );
/**
*	@brief
**/
void PF_SCR_Shutdown( void );
/**
*	@return	Pointer to the current frame's render "view rectangle".
**/
vrect_t *PF_GetScreenVideoRect( void );
/**
*	@brief	Prepare and draw the current 'active' state's 2D and 3D views.
**/
void PF_DrawActiveState( refcfg_t *refcfg );
/**
*	@brief	Prepare and draw the loading screen's 2D state.
**/
void PF_DrawLoadState( refcfg_t *refcfg );

/**
*   @return The current active handle to the font image. (Used by ref/vkpt.)
**/
const qhandle_t PF_GetScreenFontHandle( void );
/**
*   @brief  Set the alpha value of the HUD. (Used by ref/vkpt.)
**/
void PF_SetScreenHUDAlpha( const float alpha );



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
const float PF_CalculateFieldOfView( const float fov_x, const float width, const float height );
/**
*   @brief  Sets clgi.client->refdef view values and sound spatialization params.
*           Usually called from CL_PrepareViewEntities, but may be directly called from the main
*           loop if rendering is disabled but sound is running.
**/
void PF_CalculateViewValues( void );
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
	//! Stores state of the view weapon.
	struct {
		//! Current frame on-screen.
		int64_t frame;
		//! Last frame received.
		int64_t last_frame;
		//! Received server frame time of the weapon's frame.
		int64_t server_time;
	} viewWeapon;//cclient_t *clients;

	//! Generated by CLG_InitEffects, cached up regular angular velocity values for particles 'n shit..
	vec3_t avelocities[ NUMVERTEXNORMALS ];

	// Store latched cvars here that we want to get at often.
	int32_t gamemode;
	int32_t	maxclients;
	int32_t	maxentities;
	//int32_t maxspectators;
};
extern game_locals_t game;

/**
*	@brief	This structure is cleared as each map is entered, it stores data for 
*			the current level session.
**/
struct level_locals_t {
	//! For storing parsed message data that is handled later on during 
	//! the frame by corresponding said event/effect functions.
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