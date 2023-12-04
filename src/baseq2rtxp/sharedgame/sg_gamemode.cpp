#include "shared/shared.h"
#include "sg_shared.h"

/**
*	@return	A string representative of the passed in gameModeID.
**/
const char *SG_GetGamemodeName( int32_t gameModeID ) {
	// CooperativeL
	if ( gameModeID == GAMEMODE_COOPERATIVE ) {
		return "Cooperative";
	// Deathmatch:
	} else if ( gameModeID == GAMEMODE_DEATHMATCH ) {
		return "Deathmatch";
	// Default, Singleplayer
	} else {
		return "Singleplayer";
	}
}