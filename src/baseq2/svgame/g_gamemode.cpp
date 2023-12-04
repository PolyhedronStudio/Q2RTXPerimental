#include "g_local.h"

const char *G_GetGamemodeName( ) {
	// CooperativeL
	if ( gamemode->integer == 1 ) {
		return "Cooperative";
	// Deathmatch:
	} else if ( gamemode->integer == 2 ) {
		return "Deathmatch";
	// Default, Singleplayer
	} else {
		return "Singleplayer";
	}
}