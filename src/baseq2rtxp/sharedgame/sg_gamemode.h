//
// g_gamemode.cpp
//
static constexpr int32_t GAMEMODE_SINGLEPLAYER = 0;	//! Index of SP gamemode.
static constexpr int32_t GAMEMODE_DEATHMATCH = 1;	//! Index of MP deathmatch mode.
static constexpr int32_t GAMEMODE_COOPERATIVE = 2;	//! Index of MP cooperative mode.
static constexpr int32_t GAMEMODE_MAX = 3;			//! All gamemode IDs from 0 up to GAMEMODE_MAX are valid gamemodes for IsGameModeIDValid.
/**
*	@return	A string representative of the passed in gameModeID.
**/
const char *SG_GetGamemodeName( int32_t gameModeID );