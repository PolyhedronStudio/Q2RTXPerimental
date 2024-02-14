/********************************************************************
*
*
*	ClientGame: Handles the module's GetGameAPI entry point.
*
*
********************************************************************/
#include "clg_local.h"

game_locals_t   game;
level_locals_t  level;
clgame_import_t clgi;
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
//cvar_t *gamemode;
//cvar_t *maxclients;
//cvar_t *maxspectators;
//cvar_t *maxentities;
cvar_t *cl_footsteps = nullptr;
cvar_t *cl_rollhack = nullptr;
cvar_t *cl_noglow = nullptr;
cvar_t *cl_gibs = nullptr;

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
*	Init/Shutdown
*
*
**/
/**
*	@brief	This will be called when the dll is first loaded, which
*			only happens when a new game is started or a save game
*			is loaded from the main menu without having a game running
*			in the background.
**/
void PF_ShutdownGame( void ) {
	clgi.Print( print_type_t::PRINT_ALL, "==== Shutdown ClientGame ====\n" );

	// Uncomment after we actually allocate anything using this.
	//gi.FreeTags( TAG_CLGAME_LEVEL );
	clgi.FreeTags( TAG_CLGAME );
}

/**
*	@brief	Pre-init.
**/
void PF_PreInitGame( void ) {
	clgi.Print( print_type_t::PRINT_ALL, "==== PreInit ClientGame ====\n" );

	// Initialize effects and temp entities.
	CLG_InitEffects();
	CLG_InitTEnts();
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
	#endif
	cl_predict = clgi.CVar_Get( "cl_predict", nullptr, 0 );
	cl_running = clgi.CVar_Get( "cl_running", nullptr, 0 );
	cl_paused = clgi.CVar_Get( "cl_paused", nullptr, 0 );
	sv_running = clgi.CVar_Get( "cl_running", nullptr, 0 );
	sv_paused = clgi.CVar_Get( "cl_paused", nullptr, 0 );
	//gamemode = clgi.CVar( "gamemode", "", 0 );
	//maxclients = clgi.CVar( "maxclients", "", 0 );
	//maxspectators = clgi.CVar( "maxspectators", "", 0 );
	//maxentities = clgi.CVar( "maxentities", "", 0 );
		//cl_disable_particles = Cvar_Get("cl_disable_particles", "0", 0);
	//cl_disable_explosions = Cvar_Get("cl_disable_explosions", "0", 0);
	//cl_explosion_sprites = Cvar_Get("cl_explosion_sprites", "1", 0);
	//cl_explosion_frametime = Cvar_Get("cl_explosion_frametime", "20", 0);
	//cl_dlight_hacks = Cvar_Get("cl_dlight_hacks", "0", 0);

	cvar_pt_particle_emissive = clgi.CVar( "pt_particle_emissive", nullptr, 0 );
	cl_particle_num_factor = clgi.CVar( "cl_particle_num_factor", nullptr, 0 );

	cl_footsteps = clgi.CVar_Get( "cl_footsteps", "1", 0 );
	cl_rollhack = clgi.CVar_Get( "cl_rollhack", "1", 0 );
	cl_noglow = clgi.CVar_Get( "cl_noglow", "0", 0 );

	cl_gibs = clgi.CVar_Get( "cl_gibs", "1", 0 );

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
		int random_number = Q_rand() % 10000;
		char buf[ MAX_CLIENT_NAME ];
		Q_snprintf( buf, sizeof( buf ), "Q2RTXP-%04d", random_number );
		clgi.CVar_Set( "name", buf );
	}


	/**
	*	Allocate space for entities.
	**/
	// Initialize all entities for this game.
	//game.maxentities = maxentities->value;
	//clamp( game.maxentities, (int)maxclients->value + 1, MAX_EDICTS );
	game.maxentities = MAX_EDICTS;
	clg_entities = (centity_t *)clgi.TagMalloc( game.maxentities * sizeof( clg_entities[ 0 ] ), TAG_CLGAME );
	globals.entities = clg_entities;
	globals.max_entities = game.maxentities;

	/**
	*	Allocate space for clients.
	**/
	// initialize all clients for this game
	//game.maxclients = maxclients->value;
	// WID: C++20: Addec cast.
	//game.clients = (gclient_t *)gi.TagMalloc( game.maxclients * sizeof( game.clients[ 0 ] ), TAG_SVGAME );
	//globals.num_edicts = game.maxclients + 1;
	globals.num_entities = 0;

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
	// Clear out client entities array.
	memset( clg_entities, 0, globals.entity_size * sizeof( clg_entities[ 0 ] ) );

	CLG_ClearTEnts();
	CLG_ClearEffects();
}



/**
*
*
*	The player's 'Client':
*
*
**/
/**
*	@brief	Called when the client state has moved into being active and the game begins.
**/
void PF_ClientBegin( void ) {
	clgi.Print( PRINT_NOTICE, "[CLGame]: PF_ClientBegin\n" );
}

/**
*	@brief	Called when the client state has moved into being properly connected to server.
**/
void PF_ClientConnected( void ) {
	clgi.Print( PRINT_NOTICE, "[CLGame]: PF_ClientConnected\n" );
}

/**
*	@brief	Called when the client state has moved into being properly connected to server.
**/
void PF_ClientDisconnected( void ) {
	clgi.Print( PRINT_NOTICE, "[CLGame]: PF_ClientDisconnected\n" );
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

		globals.ClearState = PF_ClearState;
		globals.ClientBegin = PF_ClientBegin;
		globals.ClientConnected = PF_ClientConnected;
		globals.ClientDisconnected = PF_ClientDisconnected;

		globals.GetGamemodeName = PF_GetGamemodeName;

		globals.AdjustViewHeight = PF_AdjustViewHeight;
		globals.PredictAngles = PF_PredictAngles;
		globals.UsePrediction = PF_UsePrediction;
		globals.PredictMovement = PF_PredictMovement;
		globals.ConfigurePlayerMoveParameters = PF_ConfigurePlayerMoveParameters;

		globals.RegisterTEntModels = PF_RegisterTEntModels;
		globals.RegisterTEntSounds = PF_RegisterTEntSounds;

		globals.UpdateConfigString = PF_UpdateConfigString;
		globals.StartServerMessage = PF_StartServerMessage;
		globals.ParseServerMessage = PF_ParseServerMessage;
		globals.EndServerMessage = PF_EndServerMessage;
		globals.SeekDemoMessage = PF_SeekDemoMessage;
		globals.ParseEntityEvent = PF_ParseEntityEvent;

		globals.CalculateFieldOfView = PF_CalculateFieldOfView;
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

	clgi.Print( print_type_t::PRINT_ALL, "%s", text );
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