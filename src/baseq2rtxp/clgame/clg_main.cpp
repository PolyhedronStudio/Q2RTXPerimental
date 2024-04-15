/********************************************************************
*
*
*	ClientGame: Handles the module's GetGameAPI entry point.
*
*
********************************************************************/
#include "clg_local.h"
#include "clg_client.h"
#include "clg_effects.h"
#include "clg_entities.h"
#include "clg_input.h"
#include "clg_local_entities.h"
#include "clg_packet_entities.h"
#include "clg_parse.h"
#include "clg_precache.h"
#include "clg_predict.h"
#include "clg_temp_entities.h"
#include "clg_screen.h"
#include "clg_view.h"


//! Stores data that remains accross level switches.
game_locals_t   game;
//! This structure is cleared as each map is entered, it stores data for the current level session.
level_locals_t  level;
//! Function pointers and variables imported from the Client.
clgame_import_t clgi;
//! Function pointers and variables meant to export to the Client.
clgame_export_t	globals;


/**
*	Times.
**/
//! Frame time in Seconds.
sg_time_t FRAME_TIME_S;
//! Frame time in Miliseconds.
sg_time_t FRAME_TIME_MS;


/**
*	Random Number Generator.
**/
//! Mersenne Twister random number generator.
std::mt19937 mt_rand;


/**
*	CVars.
**/
#if USE_DEBUG
cvar_t *developer = nullptr;
#endif
cvar_t *cl_predict = nullptr;
cvar_t *cl_running = nullptr;
cvar_t *cl_paused = nullptr;
cvar_t *sv_running = nullptr;
cvar_t *sv_paused = nullptr;
//cvar_t *gamemode = nullptr;
//cvar_t *maxclients = nullptr;
//cvar_t *maxspectators = nullptr;
//cvar_t *maxentities = nullptr;
cvar_t *cl_footsteps = nullptr;

cvar_t *cl_kickangles = nullptr;
cvar_t *cl_rollhack = nullptr;
cvar_t *cl_noglow = nullptr;
cvar_t *cl_noskins = nullptr;

cvar_t *cl_gibs = nullptr;

cvar_t *cl_gunalpha = nullptr;
cvar_t *cl_gunscale = nullptr;
cvar_t *cl_gun_x = nullptr;
cvar_t *cl_gun_y = nullptr;
cvar_t *cl_gun_z = nullptr;

cvar_t *cl_player_model = nullptr;
cvar_t *cl_thirdperson_angle = nullptr;
cvar_t *cl_thirdperson_range = nullptr;

cvar_t *cl_chat_notify = nullptr;
cvar_t *cl_chat_sound = nullptr;
cvar_t *cl_chat_filter = nullptr;

cvar_t *cl_vwep = nullptr;

cvar_t *info_password = nullptr;
cvar_t *info_spectator = nullptr;
cvar_t *info_name = nullptr;
cvar_t *info_skin = nullptr;
cvar_t *info_rate = nullptr;// WID: C++20: Needed for linkage.
cvar_t *info_fov = nullptr;
cvar_t *info_msg = nullptr;
cvar_t *info_hand = nullptr;
cvar_t *info_gender = nullptr;
cvar_t *info_uf = nullptr;

// Cheesy workaround for various cvars initialized elsewhere in the client, but we need access.
cvar_t *cvar_pt_particle_emissive = nullptr; // from client FX_Init
cvar_t *cl_particle_num_factor = nullptr; // from client FX_Init


/**
*	Other.
**/
centity_t *clg_entities = nullptr;


/**
*	@return	The actual ID of the current gamemode.
**/
//const int32_t G_GetActiveGamemodeID() {
//	if ( gamemode && gamemode->integer >= GAMEMODE_SINGLEPLAYER && gamemode->integer <= GAMEMODE_COOPERATIVE ) {
//		return gamemode->integer;
//	}
//
//	// Unknown gamemode.
//	return -1;
//}

/**
*
*
*	CVar Changed Callbacks:
*
*
**/
// ugly hack for compatibility
static void cl_chat_sound_changed( cvar_t *self ) {
	if ( !*self->string )
		self->integer = 0;
	else if ( !Q_stricmp( self->string, "misc/talk.wav" ) )
		self->integer = 1;
	else if ( !Q_stricmp( self->string, "misc/talk1.wav" ) )
		self->integer = 2;
	else if ( !self->integer && !COM_IsUint( self->string ) )
		self->integer = 1;
}
static void cl_noskins_changed( cvar_t *self ) {
	int i;
	char *s;
	clientinfo_t *ci;

	if ( clgi.GetConnectionState() < ca_precached ) {
		return;
	}

	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		s = clgi.client->configstrings[ CS_PLAYERSKINS + i ];
		if ( !s[ 0 ] )
			continue;
		ci = &clgi.client->clientinfo[ i ];
		PF_PrecacheClientInfo( ci, s );
	}
}

static void cl_vwep_changed( cvar_t *self ) {
	if ( clgi.GetConnectionState() < ca_precached ) {
		return;
	}

	PF_PrecacheViewModels();
	cl_noskins_changed( self );
}



/**
*
*
*	Init/Shutdown
*
*
**/
/**
*	@brief	Called when the client is about to shutdown, giving us a last minute 
*			shot at accessing possible required data.
**/
void PF_PreShutdownGame( void ) {
	clgi.Print( print_type_t::PRINT_ALL, "==== PreShutdown ClientGame ====\n" );

	// Uncomment after we actually allocate anything using this.
	clgi.FreeTags( TAG_CLGAME_LEVEL );
	clgi.FreeTags( TAG_CLGAME );
}

/**
*	@brief	This will be called when the dll is first loaded, which
*			only happens when a new game is started or a save game
*			is loaded from the main menu without having a game running
*			in the background.
**/
void PF_ShutdownGame( void ) {
	clgi.Print( print_type_t::PRINT_ALL, "==== Shutdown ClientGame ====\n" );

	// Uncomment after we actually allocate anything using this.
	clgi.FreeTags( TAG_CLGAME_LEVEL );
	clgi.FreeTags( TAG_CLGAME );
}

/**
*	@brief	Pre-init.
**/
void PF_PreInitGame( void ) {
	clgi.Print( print_type_t::PRINT_ALL, "==== PreInit ClientGame ====\n" );
}

/**
*	@brief	This will be called when the dll is first loaded, which
*			only happens when a new game is started or a save game
*			is loaded from the main menu without having a game running
*			in the background.
**/
void PF_InitGame( void ) {
	/**
	*	Initialize time seeds, for C as well as C++.
	**/
	// C Random time initializing.
	Q_srand( time( NULL ) );
	// Seed RNG
	mt_rand.seed( (uint32_t)std::chrono::system_clock::now( ).time_since_epoch( ).count( ) );


	/**
	*	CVars.
	**/
	#if USE_DEBUG
	developer = clgi.CVar_Get( "developer", nullptr, 0 );
	#else
	//developer = clgi.CVar_Get( "developer", "0", CVAR_NOSET );
	#endif
	cl_predict = clgi.CVar_Get( "cl_predict", nullptr, 0 );
	cl_running = clgi.CVar_Get( "cl_running", nullptr, 0 );
	cl_paused = clgi.CVar_Get( "cl_paused", nullptr, 0 );
	sv_running = clgi.CVar_Get( "cl_running", nullptr, 0 );
	sv_paused = clgi.CVar_Get( "cl_paused", nullptr, 0 );
	
	//gamemode = clgi.CVar( "gamemode", nullptr, 0 );
	//maxclients = clgi.CVar( "maxclients", nullptr, 0 );
	//maxspectators = clgi.CVar( "maxspectators", nullptr, 0 );
	//maxentities = clgi.CVar( "maxentities", nullptr, 0 );

	cvar_pt_particle_emissive = clgi.CVar( "pt_particle_emissive", nullptr, 0 );
	cl_particle_num_factor = clgi.CVar( "cl_particle_num_factor", nullptr, 0 );

	// Client effects.
	cl_footsteps = clgi.CVar_Get( "cl_footsteps", "1", 0 );
	cl_kickangles = clgi.CVar_Get( "cl_kickangles", "1", CVAR_CHEAT );
	cl_rollhack = clgi.CVar_Get( "cl_rollhack", "1", 0 );
	cl_noglow = clgi.CVar_Get( "cl_noglow", "0", 0 );
	cl_noskins = clgi.CVar_Get( "cl_noskins", "0", 0 );

	// Gibs or no Gibs
	cl_gibs = clgi.CVar_Get( "cl_gibs", "1", 0 );

	// Gun Debugging CVars:
	cl_gunalpha = clgi.CVar_Get( "cl_gunalpha", "1", 0 );
	cl_gunscale = clgi.CVar_Get( "cl_gunscale", "0.25", CVAR_ARCHIVE );
	cl_gun_x = clgi.CVar_Get( "cl_gun_x", "0", 0 );
	cl_gun_y = clgi.CVar_Get( "cl_gun_y", "0", 0 );
	cl_gun_z = clgi.CVar_Get( "cl_gun_z", "0", 0 );

	// Shared with client thirdperson cvars, since refresh modules requires access to it.
	cl_player_model = clgi.CVar_Get( "cl_player_model", nullptr, 0 );
	// Client game specific thirdperson cvars.
	cl_thirdperson_angle = clgi.CVar_Get( "cl_thirdperson_angle", "0", 0 );
	cl_thirdperson_range = clgi.CVar_Get( "cl_thirdperson_range", "60", 0 );

	cl_chat_notify = clgi.CVar_Get( "cl_chat_notify", "1", 0 );
	cl_chat_sound = clgi.CVar_Get( "cl_chat_sound", "1", 0 );
	cl_chat_sound->changed = cl_chat_sound_changed;
	cl_chat_sound_changed( cl_chat_sound );
	cl_chat_filter = clgi.CVar_Get( "cl_chat_filter", "0", 0 );

	cl_vwep = clgi.CVar_Get( "cl_vwep", "1", CVAR_ARCHIVE );
	cl_vwep->changed = cl_vwep_changed;

	/**
	*	UserInfo - Initialized by the client, but we desire access to these user info cvars.
	**/
	info_password = clgi.CVar_Get( "password", nullptr, 0 );
	info_spectator = clgi.CVar_Get( "spectator", nullptr, 0 );
	info_name = clgi.CVar_Get( "name", nullptr, 0 );
	info_skin = clgi.CVar_Get( "skin", nullptr, 0 );
	info_rate = clgi.CVar_Get( "rate", nullptr, 0 );
	info_msg = clgi.CVar_Get( "msg", nullptr, 0 );
	info_hand = clgi.CVar_Get( "hand", nullptr, 0 );
	info_fov = clgi.CVar_Get( "fov", nullptr, 0 );
	info_gender = clgi.CVar_Get( "gender", nullptr, 0 );
	info_gender->modified = false; // clear this so we know when user sets it manually
	info_uf = clgi.CVar_Get( "uf", nullptr, 0 );

	// Generate a random user name to avoid new users being kicked out of MP servers.
	// The default quake2 config files set the user name to "Player", same as the cvar initialization above.
	if ( Q_strcasecmp( info_name->string, "Q2RTXPerimental" ) == 0 ) {
		int random_number = irandom( 10000 );
		char buf[ MAX_CLIENT_NAME ];
		Q_snprintf( buf, sizeof( buf ), "Q2RTXP-%04d", random_number );
		clgi.CVar_Set( "name", buf );
	}

	/**
	*	Initialize effects and temp entities.
	**/
	CLG_InitEffects();
	CLG_InitTEnts();

	/**
	*	Allocate space for entities.
	**/
	// Initialize all entities for this game.
	//game.maxentities = maxentities->value;
	//clamp( game.maxentities, (int)maxclients->value + 1, MAX_CLIENT_ENTITIES );
	game.maxentities = MAX_CLIENT_ENTITIES;
	clg_entities = (centity_t *)clgi.TagMalloc( game.maxentities * sizeof( clg_entities[ 0 ] ), TAG_CLGAME );
	globals.entities = clg_entities;
	globals.max_entities = game.maxentities;

	/**
	*	Allocate space for clients.
	**/
	// initialize all clients for this game
	//game.maxclients = maxclients->value;
	// WID: C++20: Addec cast.
	//game.clients = (cclient_t *)clgi.TagMalloc( game.maxclients * sizeof( game.clients[ 0 ] ), TAG_CLGAME );
	//globals.num_edicts = game.maxclients + 1;
	//globals.num_entities = game.maxclients + 1;

	// Get the game mode.
	clgi.Print( print_type_t::PRINT_ALL, "==== Init ClientGame ====\n" );
}



/**
*
*
*	Client State:
*
*
**/
/**
*	@brief
**/
void PF_ClearState( void ) {
	// Reset the local precache paths.
	precache.num_local_draw_models = 0;
	memset( precache.model_paths, 0, MAX_MODELS * MAX_QPATH );
	precache.num_local_sounds = 0;
	memset( precache.sound_paths, 0, MAX_SOUNDS * MAX_QPATH );

	// Reset the number of view models.
	precache.numViewModels = 0;
	memset( precache.viewModels, 0, MAX_CLIENTVIEWMODELS * MAX_QPATH );

	// Clear out local entities array.
	memset( clg_local_entities, 0, sizeof( clg_local_entities ) );
	clg_num_local_entities = 0;
	// Clear out client entities array.
	memset( clg_entities, 0, globals.entity_size * sizeof( clg_entities[ 0 ] ) );

	// Clear Temporary Entity FX and other Effects.
	CLG_ClearTEnts();
	CLG_ClearEffects();

	// Clear out level locals.
	level = {};
}



/**
*
*
*	Gamemode:
*
*
**/
/**
*	@brief
**/
const char *PF_GetGamemodeName( int32_t gameModeID ) {
	return SG_GetGamemodeName( gameModeID );
}



/**
*
*
*	Player Move:
*
*
**/
/**
*	@brief	
**/
void PF_ConfigurePlayerMoveParameters( pmoveParams_t *pmp ) {
	SG_ConfigurePlayerMoveParameters( pmp );
}



/**
*
*
*	Precache
*
*
**/
/**
*	@brief	Used for the client in a scenario where it might have to download view models.
*	@return	The number of view models.
**/
const uint32_t PF_GetNumberOfViewModels( void ) {
	return precache.numViewModels;
}
/**
*	@brief	Used for the client in a scenario where it might have to download view models.
*	@return	The filename of the view model matching index.
**/
const char *PF_GetViewModelFilename( const uint32_t index ) {
	if ( index >= 0 && index < MAX_CLIENTVIEWMODELS ) {
		return precache.viewModels[ index ];
	} else {
		return "";
	}
}



/**
*
*
*	GetGameAPI
*
*
**/
/**
*	@brief	Returns a pointer to the structure with all entry points
*			and global variables
**/
extern "C" { // WID: C++20: extern "C".
	q_exported clgame_export_t *GetGameAPI( clgame_import_t *import ) {
		clgi = *import;

		// From Q2RE:
		FRAME_TIME_S = FRAME_TIME_MS = sg_time_t::from_ms( clgi.frame_time_ms );

		globals.apiversion = CLGAME_API_VERSION;

		globals.Init = PF_InitGame;
		globals.PreInit = PF_PreInitGame;
		globals.Shutdown = PF_ShutdownGame;
		globals.PreShutdown = PF_PreShutdownGame;

		globals.ClearState = PF_ClearState;
		globals.ClientBegin = PF_ClientBegin;
		globals.ClientConnected = PF_ClientConnected;
		globals.ClientDisconnected = PF_ClientDisconnected;
		globals.ClientLocalFrame = PF_ClientLocalFrame;
		globals.ClientRefreshFrame = PF_ClientRefreshFrame;

		globals.SpawnEntities = PF_SpawnEntities;
		globals.GetEntitySoundOrigin = PF_GetEntitySoundOrigin;
		globals.GetEAXBySoundOrigin = PF_GetEAXBySoundOrigin;
		globals.ParseEntityEvent = PF_ParseEntityEvent;

		globals.GetGamemodeName = PF_GetGamemodeName;

		globals.UsePrediction = PF_UsePrediction;
		globals.AdjustViewHeight = PF_AdjustViewHeight;
		globals.CheckPredictionError = PF_CheckPredictionError;
		globals.PredictAngles = PF_PredictAngles;
		globals.PredictMovement = PF_PredictMovement;
		globals.ConfigurePlayerMoveParameters = PF_ConfigurePlayerMoveParameters;

		globals.PrecacheClientModels = PF_PrecacheClientModels;
		globals.PrecacheClientSounds = PF_PrecacheClientSounds;
		globals.PrecacheViewModels = PF_PrecacheViewModels;
		globals.PrecacheClientInfo = PF_PrecacheClientInfo;
		globals.GetNumberOfViewModels = PF_GetNumberOfViewModels;
		globals.GetViewModelFilename = PF_GetViewModelFilename;

		globals.ScreenInit = PF_SCR_Init;
		globals.ScreenRegisterMedia = PF_SCR_RegisterMedia;
		globals.ScreenModeChanged = PF_SCR_ModeChanged;
		globals.ScreenSetCrosshairColor = PF_SCR_SetCrosshairColor;
		globals.ScreenShutdown = PF_SCR_Shutdown;
		globals.GetScreenVideoRect = PF_GetScreenVideoRect;
		globals.DrawActiveState = PF_DrawActiveState;
		globals.DrawLoadState = PF_DrawLoadState;
		globals.GetScreenFontHandle = PF_GetScreenFontHandle;
		globals.SetScreenHUDAlpha = PF_SetScreenHUDAlpha;

		globals.UpdateConfigString = PF_UpdateConfigString;
		globals.StartServerMessage = PF_StartServerMessage;
		globals.ParseServerMessage = PF_ParseServerMessage;
		globals.EndServerMessage = PF_EndServerMessage;
		globals.SeekDemoMessage = PF_SeekDemoMessage;
		globals.ParsePlayerSkin = PF_ParsePlayerSkin;

		globals.MouseMove = PF_MouseMove;
		globals.RegisterUserInput = PF_RegisterUserInput;
		globals.UpdateMoveCommand = PF_UpdateMoveCommand;
		globals.FinalizeMoveCommand = PF_FinalizeMoveCommand;
		globals.ClearMoveCommand = PF_ClearMoveCommand;

		globals.CalculateFieldOfView = PF_CalculateFieldOfView;
		globals.CalculateViewValues = PF_CalculateViewValues;
		globals.ClearViewScene = PF_ClearViewScene;
		globals.PrepareViewEntities = PF_PrepareViewEntites;

		globals.entity_size = sizeof( centity_t );

		return &globals;
	}
}; // WID: C++20: extern "C".



/**
* 
* 
*	For 'Hard Linking' with Info_Print and Com_SkipPath in q_shared.cpp.
* 
* 
**/
#ifndef CLGAME_HARD_LINKED
/**
*	@brief
**/
void Com_LPrintf( print_type_t type, const char *fmt, ... ) {
	va_list     argptr;
	char        text[ MAX_STRING_CHARS ];

	if ( type == PRINT_DEVELOPER ) {
		return;
	}

	va_start( argptr, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );

	clgi.Print( type, "%s", text );
}
/**
*	@brief  
**/
void Com_Error( error_type_t type, const char *fmt, ... ) {
	va_list     argptr;
	char        text[ MAX_STRING_CHARS ];

	va_start( argptr, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );

	clgi.Error( "%s", text );
}
#endif