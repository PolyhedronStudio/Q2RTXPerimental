#include "shared/shared.h"
#include "shared/list.h"

// define CLGAME_INCLUDE so that game.h does not define the
// short, server-visible gclient_t and edict_t structures,
// because we define the full size ones in this file
#define CLGAME_INCLUDE
#include "shared/clgame.h"

// Extern here right after including shared/clgame.h
extern clgame_import_t clgi;

/**
*	@brief	Wrapper for using the appropriate developer print for the specific game module we're building.
**/
void SG_DPrintf( const char *fmt, ... ) {
	static constexpr int32_t MAXPRINTMSG = 4096;
	char        msg[ MAXPRINTMSG ];
	va_list     argptr;

	va_start( argptr, fmt );
	Q_vsnprintf( msg, sizeof( msg ), fmt, argptr );
	va_end( argptr );

	// Developer print.
	//#ifdef CLGAME_INCLUDE
	clgi.Print( PRINT_DEVELOPER, "[CL]: %s", msg );
	//#endif
}