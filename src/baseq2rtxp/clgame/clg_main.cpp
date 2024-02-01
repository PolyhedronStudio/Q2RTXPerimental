/********************************************************************
*
*
*	ClientGame: Handles the module's GetGameAPI entry point.
*
*
********************************************************************/
#include "clg_local.h"

struct game_locals_t {
	//// store latched cvars here that we want to get at often
	//int32_t	maxclients;
	int32_t	maxentities;
};
game_locals_t   game;
struct level_locals_t {
	// Nothing yet.
};
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
cvar_t *developer;
#endif
cvar_t *cl_predict;
cvar_t *cl_running;
cvar_t *cl_paused;
cvar_t *sv_running;
cvar_t *sv_paused;
//cvar_t *gamemode;
//cvar_t *maxclients;
//cvar_t *maxspectators;
//cvar_t *maxentities;


/**
*	Other.
**/
centity_t *g_entities;


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

	/**
	*	Allocate space for entities.
	**/
	// Initialize all entities for this game.
	//game.maxentities = maxentities->value;
	//clamp( game.maxentities, (int)maxclients->value + 1, MAX_EDICTS );
	game.maxentities = MAX_EDICTS;
	g_entities = (centity_t *)clgi.TagMalloc( game.maxentities * sizeof( g_entities[ 0 ] ), TAG_CLGAME );
	globals.entities = g_entities;
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
	memset( g_entities, 0, globals.entity_size * MAX_CLIENT_ENTITIES );
}



/**
*
*
*	The player's 'Client':
*
*
**/
/**
*	@brief
**/
void PF_ClientBegin( void ) {
	clgi.Print( PRINT_ERROR, "PF_ClientBegin\n" );
}

/**
*	@brief
**/
void PF_ClientConnected( void ) {
	clgi.Print( PRINT_ERROR, "PF_ClientConnected\n" );
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
void PF_PlayerMove( pmove_t *pmove, pmoveParams_t *params ) {
	SG_PlayerMove( pmove, params );
}

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

		globals.GetGamemodeName = PF_GetGamemodeName;

		globals.AdjustViewHeight = PF_AdjustViewHeight;
		globals.PredictAngles = PF_PredictAngles;
		globals.UsePrediction = PF_UsePrediction;
		globals.PredictMovement = PF_PredictMovement;
		//globals.PlayerMove = PF_PlayerMove;
		globals.ConfigurePlayerMoveParameters = PF_ConfigurePlayerMoveParameters;

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