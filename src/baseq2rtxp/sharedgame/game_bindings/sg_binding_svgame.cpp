#include "shared/shared.h"
#include "shared/list.h"

// define SVGAME_INCLUDE so that game.h does not define the
// short, server-visible gclient_t and edict_t structures,
// because we define the full size ones in this file
#define SVGAME_INCLUDE
#include "shared/svgame.h"

// Extern here right after including shared/clgame.h
extern svgame_import_t gi;

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
	//#ifdef SVGAME_INCLUDE
	gi.dprintf( "[SV]: %s", msg );
	//#endif
}

/**
*	@brief	Returns the given configstring that sits at index.
**/
configstring_t *SG_GetConfigString( const int32_t configStringIndex ) {
	return gi.GetConfigString( configStringIndex );
}