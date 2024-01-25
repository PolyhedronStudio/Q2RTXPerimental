//
#include "sharedgame/sg_shared.h"

/**
*	@brief	Wrapper for using the appropriate developer print for the specific game module we're building.
**/
static inline void SG_DPrintf( const char *fmt, ... ) {
	static constexpr int32_t MAXPRINTMSG = 4096;
	char        msg[ MAXPRINTMSG ];
	va_list     argptr;

	va_start( argptr, fmt );
	Q_vsnprintf( msg, sizeof( msg ), fmt, argptr );
	va_end( argptr );

	// Developer print.
	clgi.Print( PRINT_DEVELOPER, "%s", msg );
}