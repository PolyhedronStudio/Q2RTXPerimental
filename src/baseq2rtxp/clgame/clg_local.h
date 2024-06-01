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

extern cvar_t *cl_run_pitch;
extern cvar_t *cl_run_roll;
extern cvar_t *cl_bob_up;
extern cvar_t *cl_bob_pitch;
extern cvar_t *cl_bob_roll;

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



/**
*
*
*   Client:
*
*
**/
/**
*	Client-Game 'client' structure definition: This structure always has to 
*	mirror the 'first part' of the structure defined within the Client.
**/
typedef struct cclient_s {
//    player_state_t  ps;
//    int32_t			ping;
//	// the game dll can add anything it wants after
//	// this point in the structure
	int32_t             clientNum;
} cclient_t;



/**
*
*
*   (Client/Packet-) Entities:
*
*
**/
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
	//int64_t someTestVar;
	//int64_t someTestVar2;
} centity_t;

/**
*	Memory tag IDs to allow dynamic memory to be cleaned up.
**/
#define TAG_CLGAME			777 // Clear when unloading the dll.
#define TAG_CLGAME_LEVEL	778 // Clear when loading a new level.



/**
*
*
*   Temporary Event/Entity Parameters:
*
*
**/
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
*   @brief  Stores muzzleflash data from the last parsed svc_muzzleflash message.
**/
typedef struct {
	int32_t entity;
	int32_t weapon;
	qboolean silenced;
} mz_params_t;



/**
*
*
*	Environmental Sound Reverb Effects:
* 
*
**/
//! These two, 'default', and 'underwater' are hard-coded:
static constexpr int32_t SOUND_EAX_EFFECT_DEFAULT = 0;
static constexpr int32_t SOUND_EAX_EFFECT_UNDERWATER = 1;

//! The following are loaded from their respective .json property defining files.
static constexpr int32_t SOUND_EAX_EFFECT_ABANDONED = 2;
static constexpr int32_t SOUND_EAX_EFFECT_ALLEY = 3;
static constexpr int32_t SOUND_EAX_EFFECT_ARENA = 4;
static constexpr int32_t SOUND_EAX_EFFECT_AUDITORIUM = 5;
static constexpr int32_t SOUND_EAX_EFFECT_BATHROOM = 6;
static constexpr int32_t SOUND_EAX_EFFECT_CARPETED_HALLWAY = 7;
static constexpr int32_t SOUND_EAX_EFFECT_CAVE = 8;
static constexpr int32_t SOUND_EAX_EFFECT_CHAPEL = 9;
static constexpr int32_t SOUND_EAX_EFFECT_CITY = 10;
static constexpr int32_t SOUND_EAX_EFFECT_CITY_STREETS = 11;
static constexpr int32_t SOUND_EAX_EFFECT_CONCERT_HALL = 12;
static constexpr int32_t SOUND_EAX_EFFECT_DIZZY = 13;
static constexpr int32_t SOUND_EAX_EFFECT_DRUGGED = 14;
static constexpr int32_t SOUND_EAX_EFFECT_DUSTYROOM = 15;
static constexpr int32_t SOUND_EAX_EFFECT_FOREST = 16;
static constexpr int32_t SOUND_EAX_EFFECT_HALLWAY = 17;
static constexpr int32_t SOUND_EAX_EFFECT_HANGAR = 18;
static constexpr int32_t SOUND_EAX_EFFECT_LIBRARY = 19;
static constexpr int32_t SOUND_EAX_EFFECT_LIVINGROOM = 20;
static constexpr int32_t SOUND_EAX_EFFECT_MOUNTAINS = 21;
static constexpr int32_t SOUND_EAX_EFFECT_MUSEUM = 22;
static constexpr int32_t SOUND_EAX_EFFECT_PADDED_CELL = 23;
static constexpr int32_t SOUND_EAX_EFFECT_PARKINGLOT = 24;
static constexpr int32_t SOUND_EAX_EFFECT_PLAIN = 25;
static constexpr int32_t SOUND_EAX_EFFECT_PSYCHOTIC = 26;
static constexpr int32_t SOUND_EAX_EFFECT_QUARRY = 27;
static constexpr int32_t SOUND_EAX_EFFECT_ROOM = 28;
static constexpr int32_t SOUND_EAX_EFFECT_SEWERPIPE = 29;
static constexpr int32_t SOUND_EAX_EFFECT_SMALL_WATERROOM = 30;
static constexpr int32_t SOUND_EAX_EFFECT_STONE_CORRIDOR = 31;
static constexpr int32_t SOUND_EAX_EFFECT_STONE_ROOM = 32;
static constexpr int32_t SOUND_EAX_EFFECT_SUBWAY = 33;
static constexpr int32_t SOUND_EAX_EFFECT_UNDERPASS = 34;

//! NOTE: Determines the size of the actual array storing these effects, so make sure to adjust it if needed.
static constexpr int32_t SOUND_EAX_EFFECT_MAX = 35;



/**
*
*
*   Local Entities:
*
*
**/
// Predeclare, defined a little later.
typedef struct clg_local_entity_s clg_local_entity_t;

//! 'Precache' local entity class function pointer callback.
//! Used to parse the entity dictionary, apply values, and
//! precache any required render/sound data.
typedef void ( *LocalEntityCallback_Precache )( clg_local_entity_t *self, const cm_entity_t *keyValues );
//! 'Spawn' local entity class function pointer callback.
//! Called to spawn(prepare) the entity for gameplay.
typedef void ( *LocalEntityCallback_Spawn )( clg_local_entity_t *self );
//! 'Think' local entity class function pointer callback.
//! Used for game tick rate logic.
typedef void ( *LocalEntityCallback_Think )( clg_local_entity_t *self );
//! 'Refresh Frame' local entity class function pointer callback.
//! Used for effects updating.
typedef void ( *LocalEntityCallback_RefreshFrame )( clg_local_entity_t *self );
//! 'Prepare Refresh Entity' local entity class function pointer callback.
//! Used to setup a refresh entity for the client's current frame.
typedef void ( *LocalEntityCallback_PrepareRefreshEntity)( clg_local_entity_t *self );


/**
*	@brief	Describes the local entity's class type, default callbacks and the 
*			class' specific locals data size.
**/
typedef struct clg_local_entity_class_s {
	//! The actual classname, has to match with the worldspawn's classname key/value in order to spawn.
	const char *classname;

	//! The precache function, called during map load.
	LocalEntityCallback_Precache callbackPrecache;
	//! The spawn function, called once during spawn time(When Begin_f() has finished.).
	LocalEntityCallback_Spawn callbackSpawn;
	//! The 'think' method gets called for each client game logic frame.
	LocalEntityCallback_Think callbackThink;
	//! The 'rframe' method gets called for each client refresh frame.
	LocalEntityCallback_RefreshFrame callbackRFrame;
	//! The 'prepare refresh entity' method gets called each time the current
	//! scene to be rendered needs to be setup.
	LocalEntityCallback_PrepareRefreshEntity callbackPrepareRefreshEntity;

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
	//! Used to differentiate different entities that may be in the same slot
	uint32_t spawn_count;
	//! Whether this local entity is 'in-use' or not.
	qboolean inuse;
	//! Whether this entity is linked or not.
	qboolean islinked;

	int32_t areanum, areanum2;
	int32_t num_clusters;
	int32_t clusternums[ MAX_ENT_CLUSTERS ];
	int32_t headnode;


	//! Client game level time at which this entity was freed.
	sg_time_t freetime;
	//! Client game level time at which this entity is allowed to 'think' again.
	sg_time_t nextthink;

	//! Name of the model(if any).
	const char *model;
	//! Spawnflags parsed(if any).
	uint32_t spawnflags;

	//! Points right to the collision model's entity dictionary.
	const cm_entity_t *entityDictionary;
	//! A pointer to the entity's 'classname type' specifics.
	const clg_local_entity_class_t *entityClass;

	//! Callback Function Pointers:
	//! The precache function, called during map load.
	LocalEntityCallback_Precache precache;
	//! The spawn function, called once during spawn time(When Begin_f() has finished.).
	LocalEntityCallback_Spawn spawn;
	//! The 'think' method gets called for each client game logic frame.
	LocalEntityCallback_Think think;
	//! The 'rframe' method gets called for each client refresh frame.
	LocalEntityCallback_RefreshFrame rframe;
	//! The 'prepare refresh entity' method gets called each time the current
	//! scene to be rendered needs to be setup.
	LocalEntityCallback_PrepareRefreshEntity prepareRefreshEntity;
	//! 

	//! Meant for non 'classname type' specifically related data,
	//! which is generally shared across all entities.
	struct {
		//! The entity's origin.
		Vector3 origin;
		//! The entity's angles.
		Vector3 angles;

		//! Modelspace Mins/Maxs of Bounding Box.
		Vector3	mins, maxs;
		//! Worldspace absolute Mins/Maxs/Size of Bounding Box.
		Vector3	absmin, absmax, size;

		//! ModelIndex #1 handle.
		qhandle_t modelindex;
		//! ModelIndex #2 handle.
		qhandle_t modelindex2;
		//! ModelIndex #3 handle.
		qhandle_t modelindex3;
		//! ModelIndex #4 handle.
		qhandle_t modelindex4;

		//! Model/Sprite frame.
		int32_t frame;
		//! Model/Sprite old frame.
		int32_t oldframe;

		//! Model skin.
		int32_t skin;
		//! Model skin index number.
		int32_t skinNumber;
	} locals = { };
	
	//! Will be allocated by precaching for storing 'classname type' specific data.
	void *classLocals;
} clg_local_entity_t;

//! Actual array of local client entities.
extern clg_local_entity_t clg_local_entities[ MAX_CLIENT_ENTITIES ];
//! Current number of local client entities.
extern uint32_t clg_num_local_entities;



/**
*
*
*   Precache.
*
*
**/
typedef struct precached_media_s {
	//
	//	Local Entities:
	//
	//! Acts as a local 'configstring'. When a client entity's precache callback
	//! is called, the CLG_RegisterLocalModel/CL_RegisterLocalSound functions
	//! will store the model/sound name in the designated array.
	//! 
	//! This allows for them to be reloaded at the time of a possible restart of
	//! the audio/video subsystems and have their handles be (re-)stored at the
	//! same index.
	char model_paths[ MAX_MODELS ][ MAX_QPATH ];
	char sound_paths[ MAX_SOUNDS ][ MAX_QPATH ];

	//! Stores the model indexes for each local client entity precached model.
	qhandle_t local_draw_models[ MAX_MODELS ];
	int32_t num_local_draw_models;
	//! Stores the sound indexes for each local client entity precached sound.
	qhandle_t local_sounds[ MAX_SOUNDS ];
	int32_t num_local_sounds;



	//
	// Models:
	//
	struct {
		qhandle_t explode;
		qhandle_t smoke;
		qhandle_t flash;
		qhandle_t parasite_segment;
		qhandle_t grapple_cable;
		qhandle_t explo4;
		qhandle_t explosions[ 4 ];
		qhandle_t bfg_explo;
		qhandle_t powerscreen;
		qhandle_t laser;
		qhandle_t dmspot;

		qhandle_t lightning;
		qhandle_t heatbeam;
		qhandle_t explo4_big;
	} models;

	// 
	// Sound Effects:
	//
	struct {
		qhandle_t ric1;
		qhandle_t ric2;
		qhandle_t ric3;
		qhandle_t lashit;
		qhandle_t flare;
		qhandle_t spark5;
		qhandle_t spark6;
		qhandle_t spark7;
		qhandle_t railg;
		qhandle_t rockexp;
		qhandle_t grenexp;
		qhandle_t watrexp;

		//qhandle_t footsteps[ 14 ];

		qhandle_t lightning;
		qhandle_t disrexp;

		struct {
			// Kind - "default"/"floor" (Used as a default, and for "floor" specific materials):
			static constexpr int32_t NUM_FLOOR_STEPS = 9;
			qhandle_t floor[ NUM_FLOOR_STEPS ];

			// Kind - "carpet":
			static constexpr int32_t NUM_CARPET_STEPS = 8;
			qhandle_t carpet[ NUM_CARPET_STEPS ];
			// Kind - "dirt":
			static constexpr int32_t NUM_DIRT_STEPS = 8;
			qhandle_t dirt[ NUM_DIRT_STEPS ];
			// Kind - "grass":
			static constexpr int32_t NUM_GRASS_STEPS = 9;
			qhandle_t grass[ NUM_GRASS_STEPS ];
			// Kind - "gravel":
			static constexpr int32_t NUM_GRAVEL_STEPS = 10;
			qhandle_t gravel[ NUM_GRAVEL_STEPS ];
			// Kind - "metal":
			static constexpr int32_t NUM_METAL_STEPS = 8;
			qhandle_t metal[ NUM_METAL_STEPS ];
			// Kind - "snow":
			static constexpr int32_t NUM_SNOW_STEPS = 8;
			qhandle_t snow[ NUM_SNOW_STEPS ];
			// Kind - "tile":
			static constexpr int32_t NUM_TILE_STEPS = 9;
			qhandle_t tile[ NUM_TILE_STEPS ];
			// Kind - "water":
			static constexpr int32_t NUM_WATER_STEPS = 5;
			qhandle_t water[ NUM_WATER_STEPS ];
			// Kind - "wood":
			static constexpr int32_t NUM_WOOD_STEPS = 9;
			qhandle_t wood[ NUM_WOOD_STEPS ];
		} footsteps;
	} sfx;

	//
	// Sound EAX:
	// 
	struct {
		//! Stores all the loaded up EAX Effects:
		sfx_eax_properties_t properties[ SOUND_EAX_EFFECT_MAX ];
		//! Number of totally loaded eax effects.
		int32_t num_effects;
	} eax;

	//
	// View Models: (Moved here from client, was named weapon models but a more generic name is best fit.)
	//
	char	viewModels[ MAX_CLIENTVIEWMODELS ][ MAX_QPATH ];
	int32_t	numViewModels;

	//
	// 2D Screen Media / HUD Media
	// 
	/**
	*	@brief	Stores all resource ID's for the screen related media.
	**/
	struct {
		//! Reference to the 'Paused' center-screen display picture.
		qhandle_t   pause_pic;
		//! Reference to the 'Loading' center-screen display picture.
		qhandle_t   loading_pic;
		//! Reference to the damage display indicatore picture.
		qhandle_t   damage_display_pic;

		//! Status Buffer pics.
		static constexpr int32_t STAT_PICS = 11;
		qhandle_t   sb_pics[ 2 ][ STAT_PICS ];
		//! Inventory pic.
		qhandle_t   inven_pic;
		//! Field pic.
		qhandle_t   field_pic;
		
		//! Net pic.
		qhandle_t   net_pic;

		//! All-round screen font pic.
		qhandle_t   font_pic;
	} screen;
	/**
	*	@brief Stores all resource ID's for the HUD related media.
	**/
	struct {
		//! Crosshair
		qhandle_t crosshair_pic;
	} hud;

	//
	// Other:
	//
	// ...
} precached_media_t;
//! Stores qhandles to all precached client game media.
extern precached_media_t precache;



/**
*
*
*	Game/Level Locals:
*
*
**/
/**
*	@brief	Stores data that remains accross level switches.
*
*	@todo	In the future, look into saving its state in: client.clsv
**/
struct game_locals_t {
	//! Stores zone allocated clients[maxclients];
	cclient_t *clients;
	//! Stores zone allocated entities[maxentities];
	centity_t *entities;

	//! Stores state of the view weapon.
	struct {
		//! Current frame on-screen.
		int64_t frame;
		//! Last frame received.
		int64_t last_frame;
		//! Received server frame time of the weapon's frame.
		int64_t server_time;
		//! If true, we're engaging a FOV change.
		qboolean isAiming;
	} viewWeapon;

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
*
*	@todo	In the future, look into saving its state in: level.clsv
**/
struct level_locals_t {
	//! Frame number, starts incrementing when the level session has begun..
	int64_t		framenum;
	//! Time passed, also starts incrementing when the level session has begun.
	sg_time_t	time;

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

	//! For client-side view bobbing.
	struct {
		//! Flattened speed.
		double	xySpeed;
		//! Odd cycles are right foot going forward.
		int64_t cycle/*, cycle_run*/;
		//! sin(bobfrac*M_PI)
		double	fracSin;
		#if 0
		//! Timer.
		//double time;
		//! Old Time of Bob.
		//double oldTime;
		//! move.
		//double	move;
		//! Scale.
		//double bobScale;
		#endif
	} viewBob;

	//! A list of all 'client_env_sound' entities present in the current map. Used as a performance
	//! saver to prevent having to iterate ALL entities each time around.
	//! 
	//! NOTE: These are not indexed or sorted by their actual local entity ID, we just keep a huge
	//! buffer for simplicity.
	clg_local_entity_t *env_sound_list[ MAX_CLIENT_ENTITIES ];
	//! Keeps track of how many env_sound are actually stored in the list.
	uint32_t env_sound_list_count;

	struct {
		//! Actual lerped sfx reverb eax effect properties.
		sfx_eax_properties_t mixedProperties;
		//! Active client reverb EAX effect ID.
		qhandle_t currentID;
		//! Previous client reverb EAX effect ID: Used for lerping it smoothly to the new current one.
		qhandle_t previousID;
		//! Total frametiem for eax lerp.
		float lerpFraction;
	} eaxEffect;
};
extern level_locals_t level;



/**
*
*
*   Special Effects Data Structures:
*
*
**/
/**
*	Dynamic Lights:
**/
typedef struct clg_dlight_s {
	int32_t key;        // so entities can reuse same entry
	vec3_t  color;
	vec3_t  origin;
	float   radius;
	float   die;        // stop lighting after this time
	float   decay;      // drop this each second
	vec3_t  velosity;     // move this far each second
} clg_dlight_t;

#define DLHACK_ROCKET_COLOR         1
#define DLHACK_SMALLER_EXPLOSION    2
#define DLHACK_NO_MUZZLEFLASH       4

/**
*	Explosions:
**/
#define MAX_EXPLOSIONS  32

#define NOEXP_GRENADE   1
#define NOEXP_ROCKET    2

/**
*   @brief  Explosion struct for varying explosion type effects.
*/
typedef struct clg_explosion_s {
	//! Explosion Type.
	enum {
		ex_free,
		//ex_explosion, Somehow unused. lol. TODO: Probably implement some day? xD
		ex_misc,
		ex_flash,
		ex_mflash,
		ex_poly,
		ex_poly2,
		ex_light,
		ex_blaster,
		ex_flare
	} explosion_type;
	int32_t     type;

	//! Render Entity.
	entity_t    ent;
	//! Amount of sprite frames.
	int         frames;
	//! Light intensity.
	float       light;
	//! Light RGB color.
	vec3_t      lightcolor;
	//! 
	float       start;
	//! Frame offset into the sprite.
	int64_t     baseframe;
	//! Frametime in miliseconds.
	int64_t     frametime;
} clg_explosion_t;

#define NOPART_GRENADE_EXPLOSION    1
#define NOPART_GRENADE_TRAIL        2
#define NOPART_ROCKET_EXPLOSION     4
#define NOPART_ROCKET_TRAIL         8
#define NOPART_BLOOD                16

/**
*	Particles:
**/
#define PARTICLE_GRAVITY        120
#define BLASTER_PARTICLE_COLOR  0xe0
#define INSTANT_PARTICLE    -10000.0f

typedef struct clg_particle_s {
	struct clg_particle_s *next;

	double   time;

	vec3_t  org;
	vec3_t  vel;
	vec3_t  accel;
	int     color;      // -1 => use rgba
	float   alpha;
	float   alphavel;
	color_t rgba;
	float   brightness;
} clg_particle_t;

/**
*	Lasers:
**/
typedef struct laser_s {
	vec3_t      start;
	vec3_t      end;

	int32_t     color;
	color_t     rgba;
	int32_t     width;

	int64_t     lifetime, starttime;
} laser_t;

/**
*	Sustains:
**/
typedef struct clg_sustain_s {
	int     id;
	int     type;
	int64_t endtime;
	int64_t nextthink;
	vec3_t  org;
	vec3_t  dir;
	int     color;
	int     count;
	int     magnitude;
	void    ( *think )( struct clg_sustain_s *self );
} clg_sustain_t;