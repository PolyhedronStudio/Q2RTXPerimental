//
// g_gamemode.cpp
//
#define GAMEMODE_SINGLEPLAYER	0	// Index of SP gamemode.
#define GAMEMODE_DEATHMATCH		1	// Index of MP deathmatch mode.
#define GAMEMODE_COOPERATIVE	2	// Index of MP cooperative mode.

/**
*	@return	A string representative of the passed in gameModeID.
**/
const char *SG_GetGamemodeName( int32_t gameModeID );