/********************************************************************
*
*
*	ClientGame: Effects Header
*
*
********************************************************************/
#pragma once



/**
*
*
*
*	Precache Functionalities and Entry Points.
*
*
*
**/
/**
*	@brief	Called right after the client finished loading all received configstring (server-) models.
**/
void PF_PrecacheClientModels( void );
/**
*	@brief	Called right after the client finished loading all received configstring (server-) sounds.
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

/**
*   @brief  Registers a model for local entity usage.
*   @return -1 on failure, otherwise a handle to the model index of the precache.local_models array.
**/
const qhandle_t CLG_RegisterLocalModel( const char *name );
/**
*   @brief  Registers a sound for local entity usage.
*   @return -1 on failure, otherwise a handle to the sounds index of the precache.local_sounds array.
**/
const qhandle_t CLG_RegisterLocalSound( const char *name );

/**
*   @brief  Used by PF_ClearState.
**/
void CLG_Precache_ClearState();


/**
*	@brief	Stores all precached client media handles, as well as local media paths for local entities,
* 			which can then at a later time be precached again. (Think about vid_restart etc.)
**/
struct precached_media_s {
	/**
	*	Local Entities:
	*
	*	Acts as a local 'configstring'. When a client entity's precache callback
	*	is called, the CLG_RegisterLocalModel/CL_RegisterLocalSound functions
	*	will store the model/sound name in the designated array.
	*	
	*	This allows for them to be reloaded at the time of a possible restart of
	*	the audio/video subsystems and have their handles be (re-)stored at the
	*	same index.
	**/ 
	char model_paths[ MAX_MODELS ][ MAX_QPATH ];
	char sound_paths[ MAX_SOUNDS ][ MAX_QPATH ];

	//! Stores the model indexes for each local client entity precached model.
	qhandle_t local_draw_models[ MAX_MODELS ];
	int32_t num_local_draw_models;
	//! Stores the sound indexes for each local client entity precached sound.
	qhandle_t local_sounds[ MAX_SOUNDS ];
	int32_t num_local_sounds;


	/**
	*	Models:
	**/
	struct precached_models_s {
		// <Q2RTXP>: Our own sprite/model precache entries.
		qhandle_t sprite_explo00; //! Comes without smoke.
		qhandle_t sprite_explo01; //! This explosion is always high, it comes with smoke, lol joke.
	} models;


	/**
	*	Sound Effects:
	**/
	struct precached_sfx_s {
		//! Explosions.
		struct explosion_sfx_s {
			qhandle_t rocket;
			qhandle_t grenade01;
			qhandle_t grenade02;
			qhandle_t water;
		} explosions;

		struct hud_sfx_s {
			qhandle_t chat01;
		} hud;

		//! Items Sounds:
		struct item_sfx_s {
			qhandle_t respawn01;
			qhandle_t pickup;
		} items;

		//! Player Sounds:
		struct player_sfx_s {
			// Falling sounds, randomly chosen between the two:
			qhandle_t fall01;
			qhandle_t fall02;

			// Landing sound:
			qhandle_t land01;

			// Water sounds:
			qhandle_t water_feet_in01;
			qhandle_t water_splash_in01;
			qhandle_t water_splash_in02;
			qhandle_t water_head_under01;
			qhandle_t water_feet_out01;
			qhandle_t water_body_out01;
		} players;

		//! Ricochet effects:
		struct ricochet_sfx_s {
			//! Seems 'burn' hit like, was called laser_hit. TODO: Rename burn_hit?
			qhandle_t lashit;

			qhandle_t ric1;
			qhandle_t ric2;
			qhandle_t ric3;
		} ricochets;

		//! Weapon Sounds:
		struct weapon_sfx_s {
			// Fist Weapon Sounds:
			struct {
				qhandle_t punch1;
			} fists;

			// Pistol Weapon Sounds:
			struct pistol {
				qhandle_t fire[ 3 ];
			} pistol;
		} weapons;

		//! World effects:
		struct world_sfx_s {
			// Gib sounds:
			qhandle_t gib01;
			qhandle_t gib_drop01;

			// Login/Logout sounds:
			qhandle_t mz_login;
			qhandle_t mz_logout;
			// Respawn sound: (Currently just uses login sound.)
			qhandle_t mz_respawn; // <Q2RTXP>: Needs a sound file..

			// Teleport sound(s):
			qhandle_t teleport01;
		} world;

		//! Footstep sounds for different materials:
		struct footstep_sfx_s {
			// Kind - "default"/"floor" (Used as a default, and for "floor" specific materials):
			static constexpr int32_t NUM_FLOOR_STEPS = 9;
			qhandle_t floor[ NUM_FLOOR_STEPS ];

			// Kind - "carpet":
			static constexpr int32_t NUM_CARPET_STEPS = 8;
			qhandle_t carpet[ NUM_CARPET_STEPS ];
			// Kind - "dirt":
			static constexpr int32_t NUM_DIRT_STEPS = 7;
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


	/**
	*	Sound EAX:
	**/
	struct eax_s {
		//! Stores all the loaded up EAX Effects:
		sfx_eax_properties_t properties[ SOUND_EAX_EFFECT_MAX ];
		//! Number of totally loaded eax effects.
		int32_t num_effects;
	} eax;


	/**
	*	View Models: 
	*	(Moved here from client, was named weapon models but a more generic name is best fit.)
	**/
	char	viewModels[ MAX_CLIENTVIEWMODELS ][ MAX_QPATH ];
	int32_t	numViewModels;


	/**
	*	2D Screen Media / HUD Media
	**/
	/**
	*	@brief	Stores all resource ID's for the screen related media.
	**/
	struct screen_media_s {
		//! Reference to the damage display indicatore picture.
		qhandle_t   damage_display_pic;
		//! Net pic.
		qhandle_t   net_error_pic;
		//! All-round screen font pic.
		qhandle_t   font_pic;
	} screen;

	/**
	*	@brief Stores all resource ID's for the HUD related media.
	**/
	struct hud_s {
		//! Crosshair
		//qhandle_t crosshair_pic;
	} hud;
};
//! Stores qhandles to all precached client game media.
extern precached_media_s precache;