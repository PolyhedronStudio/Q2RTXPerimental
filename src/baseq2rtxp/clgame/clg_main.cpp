/********************************************************************
*
*
*	ClientGame: Handles the module's GetGameAPI entry point.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_client.h"
#include "clgame/clg_eax.h"
#include "clgame/clg_effects.h"
#include "clgame/clg_entities.h"
#include "clgame/clg_input.h"
#include "clgame/clg_local_entities.h"
#include "clgame/clg_packet_entities.h"
#include "clgame/clg_parse.h"
#include "clgame/clg_precache.h"
#include "clgame/clg_predict.h"
#include "clgame/clg_temp_entities.h"
#include "clgame/clg_screen.h"
#include "clgame/clg_view.h"


#include "sharedgame/sg_gamemode.h"


//! Stores data that remains accross level switches.
game_locals_t game;
//! This structure is cleared as each map is entered, it stores data for the current level session.
clg_level_locals_t level;
//! Function pointers and variables imported from the Client.
clgame_import_t clgi;
//! Function pointers and variables meant to export to the Client.
clgame_export_t globals;


/**
*	Times.
**/
//! Frame time in Seconds.
QMTime FRAME_TIME_S;
//! Frame time in Miliseconds.
QMTime FRAME_TIME_MS;



/**
* 
* 
* 
*	Client(-Game Module) CVars.
* 
* 
* 
**/
/**
*	Client CVars, fetched by the game module:
**/
#if USE_DEBUG
	cvar_t *developer = nullptr;
#endif
cvar_t *sv_running = nullptr;
cvar_t *sv_paused = nullptr;
cvar_t *cl_running = nullptr;
cvar_t *cl_paused = nullptr;

cvar_t *gamemode = nullptr;

cvar_t *maxclients = nullptr;
cvar_t *maxentities = nullptr;
//cvar_t *maxspectators = nullptr;

cvar_t *cl_kickangles = nullptr;
cvar_t *cl_noskins = nullptr;
cvar_t *cl_predict = nullptr;
cvar_t *cl_nolerp = nullptr;
cvar_t *cl_footsteps = nullptr;

// Cheesy workaround for various cvars initialized elsewhere in the client, but we need access.
cvar_t *cvar_pt_particle_emissive = nullptr; // from client FX_Init
cvar_t *cl_particle_num_factor = nullptr; // from client FX_Init

/**
*	Client View CVars:
**/
cvar_t *cl_run_pitch = nullptr;
cvar_t *cl_run_roll = nullptr;
cvar_t *cl_bob_up = nullptr;
cvar_t *cl_bob_pitch = nullptr;
cvar_t *cl_bob_roll = nullptr;

//! Whether to show the player model in 3rd person.
cvar_t *cl_player_model = nullptr;
//! Camera third person angle.
cvar_t *cl_thirdperson_angle = nullptr;
//! Camera third person range.
cvar_t *cl_thirdperson_range = nullptr;
//! View Weapon CVar.
cvar_t *cl_vwep = nullptr;

/**
*	Chat Related CVars:
**/
cvar_t *cl_chat_notify = nullptr;
cvar_t *cl_chat_filter = nullptr;

/**
*	Info String CVars:
**/
cvar_t *info_password = nullptr;
cvar_t *info_spectator = nullptr;
cvar_t *info_name = nullptr;
cvar_t *info_skin = nullptr;
cvar_t *info_rate = nullptr;// WID: C++20: Needed for linkage.
cvar_t *info_fov = nullptr;
cvar_t *info_msg = nullptr;
cvar_t *info_hand = nullptr;
cvar_t *info_uf = nullptr;

/**
*	(Developer-) Gun CVars:
**/
//! Developer gun alpha adjustment.
cvar_t *cl_gunalpha = nullptr;
//! Generic gun scale adjustment.
cvar_t *cl_gunscale = nullptr;
//! Developer gun X offset adjustment.
cvar_t *cl_gun_x = nullptr;
//! Developer gun Y offset adjustment.
cvar_t *cl_gun_y = nullptr;
//! Developer gun Z offset adjustment.
cvar_t *cl_gun_z = nullptr;

/**
*	Other.
**/
centity_t *clg_entities = nullptr;

//! View Commands.
extern cmdreg_t clg_view_cmds[];



/**
*
*
*	Cmd Macros:
* 
* 
**/
/**
*	@brief	Health.
**/
static size_t CL_Health_m( char *buffer, size_t size ) {
	return Q_scnprintf( buffer, size, "%i", clgi.client->frame.ps.stats[ STAT_HEALTH ] );
}
/**
*	@brief	Ammo.
**/
static size_t CL_Ammo_m( char *buffer, size_t size ) {
	return Q_scnprintf( buffer, size, "%i", clgi.client->frame.ps.stats[ STAT_AMMO ] );
}
/**
*	@brief	Armor.
**/
static size_t CL_Armor_m( char *buffer, size_t size ) {
	return Q_scnprintf( buffer, size, "%i", clgi.client->frame.ps.stats[ STAT_ARMOR ] );
}
/**
*	@brief	The Quake Units per second of the player.
**/
static size_t CL_Ups_m( char *buffer, size_t size ) {
	vec3_t vel;

	// Use predicted velocity if we're not in a demo playback and prediction is enabled.
	if ( !clgi.IsDemoPlayback() && cl_predict->integer &&
		!( clgi.client->frame.ps.pmove.pm_flags & PMF_NO_ORIGIN_PREDICTION ) ) {
		VectorCopy( game.predictedState.currentPs.pmove.velocity, vel );
	// Otherwise use the server sent velocity.
	} else {
		VectorCopy( clgi.client->frame.ps.pmove.velocity, vel );
	}
	// scnprintf it into the buffer.
	return Q_scnprintf( buffer, size, "%.f", VectorLength( vel ) );
}
/**
*	@brief	Weapon Model.
**/
static size_t CL_WeaponModel_m( char *buffer, size_t size ) {
	return Q_scnprintf( buffer, size, "%s",
		clgi.client->configstrings[ clgi.client->frame.ps.gun.modelIndex + CS_MODELS ] );
}



/**
*
*
*	CVar Changed Callbacks:
*
*
**/
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
*	Client State:
*
*
**/
/**
*   @return True if the server, or client, is non active(and/or thus paused).
*	@note	clientOnly, false by default, is optional to check if just the client is paused.
**/
const bool CLG_IsGameplayPaused( const bool clientOnly ) {
	// Determine whether paused or not.
	if ( clgi.GetConnectionState() != ca_active 
		// Client is paused.
		|| cl_paused->integer 
		// Optional to check if just the client is paused.
		|| ( clientOnly ? 1 == 1 : sv_paused->integer ) ) {
		// Paused.
		return true;
	}
	return false;
}

/**
*	@brief
**/
void PF_ClearState( void ) {
	// Clear out the player's viewWeapon state.
	game.viewWeapon = {};

	// Hard reset the sound EAX environment.
	CLG_EAX_HardSetEnvironment( SOUND_EAX_EFFECT_DEFAULT );

	// Clear Precache State.
	CLG_Precache_ClearState();

	// Clear Local Entity States.
	CLG_LocalEntity_ClearState();

	// Clear out Client Entities array.
	//memset( clg_entities, 0, globals.entity_size * sizeof( clg_entities[ 0 ] ) );
	for ( int32_t i = 0; i < sizeof( clg_entities ); i++ ) {
		clg_entities[ i ] = {};
	}
	// Clear Temporary Entities.
	CLG_TemporaryEntities_Clear();
	// Clear out remaining effect types.
	CLG_ClearEffects();

	// Clear out level locals.
	//memset( &level, 0, sizeof( level ) );
	std::fill_n( reinterpret_cast<std::byte *>( &level ), sizeof( level ), std::byte{ 0 } ); // level = {}; // Warning: Cc6262 function uses '65832' bytes of stack.
	
}



/**
*
*
*	Entity Receive and Update:
*
*
**/
/**
*   @brief  The sound code makes callbacks to the client for entitiy position
*           information, so entities can be dynamically re-spatialized.
**/
void PF_GetEntitySoundOrigin( const int32_t entityNumber, vec3_t org ) {
	CLG_GetEntitySoundOrigin( entityNumber, org );
}
/**
*	@brief	Parsess entity events.
**/
void PF_ParseEntityEvent( const int32_t entityNumber ) {
	CLG_ParseEntityEvent( entityNumber );
}
/**
*	@brief	Called when a new frame has been received that contains an entity
*			which was not present in the previous frame.
**/
static void PF_EntityState_FrameEnter( centity_t *ent, const entity_state_t *state, const Vector3 *origin ) {
	CLG_EntityState_FrameEnter( ent, state, ( origin ? *origin : QM_Vector3Zero() ) );
}
/**
*	@brief	Called when a new frame has been received that contains an entity
*			already present in the previous frame.
**/
static void PF_EntityState_FrameUpdate( centity_t *ent, const entity_state_t *state, const Vector3 *origin ) {
	CLG_EntityState_FrameUpdate( ent, state, ( origin ? *origin : QM_Vector3Zero() ) );
}
/**
*   Determine whether the player state has to lerp between the current and old frame,
*   or snap 'to'.
**/
static void PF_PlayerState_Transition( server_frame_t *oldframe, server_frame_t *frame, const int32_t framediv ) {
	CLG_PlayerState_Transition( oldframe, frame, framediv );
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
const char *PF_GetGameModeName( int32_t gameModeID ) {
	return SG_GetGameModeName( static_cast<sg_gamemode_type_t>( gameModeID ) );
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
*	(Prediction-) Player Move:
*
*
**/
/**
*   @brief  'ProgFunc' Wrapper for CLG_CheckPredictionError.
**/
void PF_CheckPredictionError( const int64_t frameIndex, const int64_t commandIndex, struct client_movecmd_s *moveCommand ) {
	CLG_CheckPredictionError( frameIndex, commandIndex, moveCommand );
}
/**
*   @brief  'ProgFunc' Wrapper for CLG_PredictAngles.
**/
void PF_PredictAngles() {
	CLG_PredictAngles();
}
/**
*	@return	'ProgFunc' Wrapper for CLG_UsePrediction.
**/
const qboolean PF_UsePrediction() {
	return CLG_UsePrediction();
}
/**
*   @brief  'ProgFunc' Wrapper for CLG_PredictMovement.
**/
void PF_PredictMovement( int64_t acknowledgedCommandNumber, const int64_t currentCommandNumber ) {
	CLG_PredictMovement( acknowledgedCommandNumber, currentCommandNumber );
}



/**
*
*
*	View Rendering:
*
*
**/
/**
*	@brief	'ProgFunc' Wrapper for CLG_CalculateFieldOfView.
**/
static const float PF_CalculateFieldOfView( const float fov_x, const float width, const float height ) {
	return CLG_CalculateFieldOfView( fov_x, width, height );
}
/**
*	@brief	'ProgFunc' Wrapper for CLG_CalculateViewValues.
**/
static void PF_CalculateViewValues() {
	CLG_CalculateViewValues();
}
/**
* @brief	'ProgFunc' Wrapper for CLG_InitViewScene.
**/
static void PF_InitViewScene() {
	CLG_InitViewScene();
}
/**
*	@brief	'ProgFunc' Wrapper for CLG_ShutdownViewScene.
**/
static void PF_ShutdownViewScene() {
	CLG_ShutdownViewScene();
}
/**
* @brief	'ProgFunc' Wrapper for CLG_DrawActiveViewState.
**/
void PF_DrawActiveViewState( refcfg_t *refcfg ) {
	CLG_DrawActiveViewState( refcfg );
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
	//clgi.FreeTags( TAG_CLGAME_LEVEL );
	//clgi.FreeTags( TAG_CLGAME );
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
	mt_rand.seed( (uint32_t)std::chrono::system_clock::now().time_since_epoch().count() );


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

	// Get cvars we'll latch at often.
	gamemode = clgi.CVar( "gamemode", nullptr, 0 );
	game.gamemode = gamemode->integer;
	maxclients = clgi.CVar( "maxclients", nullptr, 0 );
	game.maxclients = maxclients->integer;
	//maxentities = clgi.CVar( "maxentities", nullptr, 0 );

	cvar_pt_particle_emissive = clgi.CVar( "pt_particle_emissive", nullptr, 0 );
	cl_particle_num_factor = clgi.CVar( "cl_particle_num_factor", nullptr, 0 );

	// Client effects.
	cl_footsteps = clgi.CVar_Get( "cl_footsteps", "1", 0 );
	cl_kickangles = clgi.CVar_Get( "cl_kickangles", "1", CVAR_CHEAT );
	cl_noskins = clgi.CVar_Get( "cl_noskins", "0", 0 );
	cl_nolerp = clgi.CVar_Get( "cl_nolerp", "0", 0 );

	// Gun Debugging CVars:
	cl_gunalpha = clgi.CVar_Get( "cl_gunalpha", "1", 0 );
	cl_gunscale = clgi.CVar_Get( "cl_gunscale", "0.25", CVAR_ARCHIVE );
	cl_gun_x = clgi.CVar_Get( "cl_gun_x", "0", 0 );
	cl_gun_y = clgi.CVar_Get( "cl_gun_y", "0", 0 );
	cl_gun_z = clgi.CVar_Get( "cl_gun_z", "0", 0 );

	// View Bob CVars, Read Only:
	cl_run_pitch = clgi.CVar_Get( "run_pitch", "0.01", CVAR_ROM );
	cl_run_roll = clgi.CVar_Get( "run_roll", "0.025", CVAR_ROM );
	cl_bob_up = clgi.CVar_Get( "bob_up", "0.005", CVAR_ROM );
	cl_bob_pitch = clgi.CVar_Get( "bob_pitch", "0.002", CVAR_ROM );
	cl_bob_roll = clgi.CVar_Get( "bob_roll", "0.002", CVAR_ROM );

	// Shared with client thirdperson cvars, since refresh modules requires access to it.
	cl_player_model = clgi.CVar_Get( "cl_player_model", nullptr, 0 );
	// Client game specific thirdperson cvars.
	cl_thirdperson_angle = clgi.CVar_Get( "cl_thirdperson_angle", "0", 0 );
	cl_thirdperson_range = clgi.CVar_Get( "cl_thirdperson_range", "60", 0 );

	cl_chat_notify = clgi.CVar_Get( "cl_chat_notify", "1", 0 );
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
	//info_gender = clgi.CVar_Get( "gender", nullptr, 0 );
	//info_gender->modified = false; // clear this so we know when user sets it manually
	info_uf = clgi.CVar_Get( "uf", nullptr, 0 );

	// Generate a random user name to avoid new users being kicked out of MP servers.
	// The default quake2 config files set the user name to "Player", same as the cvar initialization above.
	if ( Q_strcasecmp( info_name->string, "Q2RTXPerimental" ) == 0 ) {
		int random_number = irandom( 10000 );
		char buf[ MAX_CLIENT_NAME ];
		Q_snprintf( buf, sizeof( buf ), "Q2RTXP-%04d", random_number );
		clgi.CVar_Set( "name", buf );
	}

	// Register view command callbacks.
	clgi.Cmd_Register( clg_view_cmds );

	clgi.Cmd_AddMacro( "cl_health", CL_Health_m );
	clgi.Cmd_AddMacro( "cl_ammo", CL_Ammo_m );
	clgi.Cmd_AddMacro( "cl_armor", CL_Armor_m );
	clgi.Cmd_AddMacro( "cl_ups", CL_Ups_m );
	clgi.Cmd_AddMacro( "cl_weaponmodel", CL_WeaponModel_m );

	/**
	*	Initialize effects and temp entities.
	**/
	CLG_InitEffects();
	CLG_TemporaryEntities_Init();

	/**
	*	Default EAX Environment:
	**/


	/**
	*	Allocate space for entities.
	**/
	game.maxentities = MAX_CLIENT_ENTITIES;
	game.entities = clg_entities = static_cast<centity_t *>( clgi.TagMalloc( game.maxentities * sizeof( clg_entities[ 0 ] ), TAG_CLGAME ) );
	globals.entities = clg_entities;
	globals.max_entities = game.maxentities;

	/**
	*	Allocate space for clients.
	**/
	// initialize all clients for this game
	//game.maxclients = maxclients->value;
	// WID: C++20: Addec cast.
	game.clients = (cclient_t *)clgi.TagMalloc( game.maxclients * sizeof( game.clients[ 0 ] ), TAG_CLGAME );
	//globals.num_edicts = game.maxclients + 1;
	//globals.num_entities = game.maxclients + 1;

	// Get the game mode.
	clgi.Print( print_type_t::PRINT_ALL, "==== Init ClientGame ====\n" );
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
		FRAME_TIME_S = FRAME_TIME_MS = QMTime::FromMilliseconds( clgi.frame_time_ms );

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
		globals.PostSpawnEntities = PF_PostSpawnEntities;

		globals.GetEntitySoundOrigin = PF_GetEntitySoundOrigin;
		globals.ParseEntityEvent = PF_ParseEntityEvent;
		globals.EntityState_FrameEnter = PF_EntityState_FrameEnter;
		globals.EntityState_FrameUpdate = PF_EntityState_FrameUpdate;
		globals.PlayerState_Transition = PF_PlayerState_Transition;

		globals.GetGameModeName = PF_GetGameModeName;

		globals.UsePrediction = PF_UsePrediction;
		globals.CheckPredictionError = PF_CheckPredictionError;
		globals.PredictAngles = PF_PredictAngles;
		globals.PredictMovement = PF_PredictMovement;
		//globals.ConfigurePlayerMoveParameters = PF_ConfigurePlayerMoveParameters;

		globals.PrecacheClientModels = PF_PrecacheClientModels;
		globals.PrecacheClientSounds = PF_PrecacheClientSounds;
		globals.PrecacheViewModels = PF_PrecacheViewModels;
		globals.PrecacheClientInfo = PF_PrecacheClientInfo;
		globals.GetNumberOfViewModels = PF_GetNumberOfViewModels;
		globals.GetViewModelFilename = PF_GetViewModelFilename;

		globals.ScreenInit = PF_SCR_Init;
		globals.ScreenRegisterMedia = PF_SCR_RegisterMedia;
		globals.ScreenModeChanged = PF_SCR_ModeChanged;
		globals.ScreenDeltaFrame = PF_SCR_DeltaFrame;
		globals.ScreenShutdown = PF_SCR_Shutdown;
		globals.DrawActiveViewState = PF_DrawActiveViewState;
		globals.DrawLoadState = PF_DrawLoadState;
		globals.GetScreenFontHandle = PF_GetScreenFontHandle;
		globals.SetScreenHUDAlpha = PF_SetScreenHUDAlpha;

		globals.UpdateConfigString = PF_UpdateConfigString;
		globals.StartServerMessage = PF_StartServerMessage;
		globals.ParseServerMessage = PF_ParseServerMessage;
		globals.EndServerMessage = PF_EndServerMessage;
		globals.SeekDemoMessage = PF_SeekDemoMessage;
		globals.ParsePlayerSkin = PF_ParsePlayerSkin;

		globals.RegisterUserInput = PF_RegisterUserInput;
		globals.UpdateMoveCommand = PF_UpdateMoveCommand;
		globals.FinalizeMoveCommand = PF_FinalizeMoveCommand;
		globals.ClearMoveCommand = PF_ClearMoveCommand;

		globals.CalculateFieldOfView = PF_CalculateFieldOfView;
		globals.CalculateViewValues = PF_CalculateViewValues;
		globals.InitViewScene = PF_InitViewScene;
		globals.ShutdownViewScene = PF_ShutdownViewScene;
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