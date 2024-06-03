/********************************************************************
*
*
*	SharedGame: Server Game SG Function Implementations.
*
*
********************************************************************/
#include "shared/shared.h"
#include "shared/util_list.h"

// define SVGAME_INCLUDE so that game.h does not define the
// short, server-visible gclient_t and edict_t structures,
// because we define the full size ones in this file
#include "shared/svgame.h"
#include "../../svgame/g_local.h"

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
	gi.dprintf( "%s", msg );
}

/**
*	@brief	Returns the entity number, -1 if invalid(nullptr, or out of bounds).
**/
const int32_t SG_GetEntityNumber( sgentity_s *sgent ) {
	if ( sgent ) {
		return sgent->s.number;
	} else {
		return -1;
	}
}

/**
*	@brief	Returns the given configstring that sits at index.
**/
configstring_t *SG_GetConfigString( const int32_t configStringIndex ) {
	return gi.GetConfigString( configStringIndex );
}

/**
*	@brief	Server side sharedgame implementation of sg_time_t::frames.
**/
int64_t sg_time_t::frames( ) const {
	return _ms / gi.frame_time_ms;
}

/**
*	@brief	Wraps around CVar_Get
**/
cvar_t *SG_CVar_Get( const char *var_name, const char *value, const int32_t flags ) {
	return gi.cvar( var_name, value, flags );
}