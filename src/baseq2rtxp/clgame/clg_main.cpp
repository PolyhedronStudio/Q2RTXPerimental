/********************************************************************
*
*
*	ClientGame: Handles the module's GetGameAPI entry point.
*
*
********************************************************************/
#include "clg_local.h"

//game_locals_t   game;
//level_locals_t  level;
clgame_import_t clgi;
clgame_export_t	globals;
//spawn_temp_t    st;

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

//cvar_t *deathmatch;
//cvar_t *coop;
//cvar_t *dmflags;
//cvar_t *skill;

/**
*	@brief	This will be called when the dll is first loaded, which
*			only happens when a new game is started or a save game
*			is loaded from the main menu without having a game running
*			in the background.
**/
void ShutdownGame( void ) {
	clgi.Print( print_type_t::PRINT_ALL, "==== Shutdown ClientGame ====\n" );

	// Uncomment after we actually allocate anything using this.
	//gi.FreeTags( TAG_CLGAME_LEVEL );
	//gi.FreeTags( TAG_CLGAME );
}

/**
*	@brief	This will be called when the dll is first loaded, which
*			only happens when a new game is started or a save game
*			is loaded from the main menu without having a game running
*			in the background.
**/
void InitGame( void ) {
	clgi.Print( print_type_t::PRINT_ALL, "==== Init ClientGame(Gamemode: \"%s\") ====\n" );

	// C Random time initializing.
	Q_srand( time( NULL ) );

	// Seed RNG
	mt_rand.seed( (uint32_t)std::chrono::system_clock::now( ).time_since_epoch( ).count( ) );
}



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

		globals.Init = InitGame;
		globals.Shutdown = ShutdownGame;

		globals.GetGamemodeName = SG_GetGamemodeName;

		globals.PlayerMove = SG_PlayerMove;
		globals.ConfigurePlayerMoveParameters = SG_ConfigurePlayerMoveParameters;

		return &globals;
	}
}; // WID: C++20: extern "C".



/**
* 
* 
*	For 'Hard Linking'.
* 
* 
**/
#ifndef CLGAME_HARD_LINKED
// this is only here so the functions in q_shared.c can link
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

void Com_Error( error_type_t type, const char *fmt, ... ) {
	va_list     argptr;
	char        text[ MAX_STRING_CHARS ];

	va_start( argptr, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );

	clgi.Error( "%s", text );
}
#endif