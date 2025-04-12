/********************************************************************
*
*
*	SharedGame: Client Game SG Function Implementations.
*
*
********************************************************************/
#include "shared/shared.h"
#include "shared/util/util_list.h"

// define CLGAME_INCLUDE so that game.h does not define the
// short, server-visible svg_client_t and edict_t structures,
// because we define the full size ones in this file
#include "shared/client/cl_game.h"
#include "../../clgame/clg_local.h"

// Extern here right after including shared/client/cl_game.h
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
	clgi.Print( PRINT_DEVELOPER, "%s", msg );
	//Com_LPrintf( PRINT_DEVELOPER, "%s", msg );
}

/**
*	@brief	Returns the entity number, -1 if invalid(nullptr, or out of bounds).
**/
const int32_t SG_GetEntityNumber( sgentity_s *sgent ) {
	if ( sgent ) {
		return sgent->current.number;
	} else {
		return -1;
	}
}

/**
*	@brief	Returns the given configstring that sits at index.
**/
configstring_t *SG_GetConfigString( const int32_t configStringIndex ) {
	return clgi.GetConfigString( configStringIndex );
}

/**
*	@brief	Client side sharedgame implementation of QMTime::frames.
**/
int64_t QMTime::frames() const {
	return _ms / clgi.frame_time_ms;
}