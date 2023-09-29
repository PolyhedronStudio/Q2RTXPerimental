#include "clg_local.h"

//game_locals_t   game;
//level_locals_t  level;
clgame_import_t	gi;
clgame_export_t	globals;
//spawn_temp_t    st;

// Times.
//gtime_t FRAME_TIME_S;
//gtime_t FRAME_TIME_MS;

// Mersenne Twister random number generator.
//std::mt19937 mt_rand;
//
//int sm_meat_index;
//int snd_fry;
//int meansOfDeath;
//
//edict_t *g_edicts;
//
//cvar_t *deathmatch;
//cvar_t *coop;
//cvar_t *dmflags;
//cvar_t *skill;
//cvar_t *fraglimit;
//cvar_t *timelimit;
//cvar_t *password;
//cvar_t *spectator_password;
//cvar_t *needpass;
//cvar_t *maxclients;
//cvar_t *maxspectators;
//cvar_t *maxentities;
//cvar_t *g_select_empty;
//cvar_t *dedicated;
//cvar_t *nomonsters;
//cvar_t *aimfix;


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

	gi.dprintf( "%s", text );
}

void Com_Error( error_type_t type, const char *fmt, ... ) {
	va_list     argptr;
	char        text[ MAX_STRING_CHARS ];

	va_start( argptr, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );

	gi.error( "%s", text );
}
#endif