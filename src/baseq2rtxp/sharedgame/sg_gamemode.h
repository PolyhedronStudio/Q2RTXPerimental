/********************************************************************
*
*
*	SharedGame: Game Mode
*
*
********************************************************************/
#pragma once



//
// g_gamemode.cpp
//
typedef enum sg_gamemode_type_e {
	//! If you ever get this value, something is badly wrong.
	GAMEMODE_TYPE_UNKNOWN = -1,

	//! Start and save/load an adventure of your own against monsters.
	GAMEMODE_TYPE_SINGLEPLAYER = 0,
	//! Player with other clients against monsters.
	GAMEMODE_TYPE_COOPERATIVE = 1,

	//! Play versus other clients.
	GAMEMODE_TYPE_DEATHMATCH = 2,

	//! Total limit of gamemodes. All gamemode IDs from 0 up to GAMEMODE_MAX are valid gamemodes for IsGameModeIDValid.
	GAMEMODE_TYPE_MAX, // Will always be last gamemode type ID + 1.
} sg_gamemode_type_t;

/**
*	@brief	Gamemode base interface.
**/
struct sg_igamemode_t {
	/**
	*   @brief  Returns the game mode type.
	**/
	virtual const sg_gamemode_type_t GetGameModeType() const = 0;
	/**
	*   @brief  Returns the game mode name.
	**/
	virtual const char *GetGameModeName() const = 0;
	/**
	*   @brief  Returns the game mode description.
	**/
	virtual const char *GetGameModeDescription() const = 0;

	/**
	*	@brief	Returns true if this gamemode is one of the multiplayer gamemodes.
	**/
	virtual const bool IsMultiplayer() const = 0;
	/**
	*	@brief	Returns true if this gamemode allows saving the game.
	**/
	virtual const bool AllowSaveGames() const = 0;

	/**
	*	@brief	Called once by client during connection to the server. Called once by
	*			the server when a new game is started.
	**/
	virtual void PrepareCVars() = 0;
};


/**
*
*
*
*	SinglePlayer GameMode(Also the default for Unknown ID -1):
*
*
*
**/
struct sg_singleplayer_gamemode_t : public sg_igamemode_t {
	/**
	*   @brief  Returns the game mode type.
	**/
	const sg_gamemode_type_t GetGameModeType() const override { return GAMEMODE_TYPE_SINGLEPLAYER; }
	/**
	*   @brief  Returns the game mode name.
	**/
	const char *GetGameModeName() const override { return "SinglePlayer"; }
	/**
	*   @brief  Returns the game mode description.
	**/
	const char *GetGameModeDescription() const override { return "Play through a singleplayer adventure."; }

	/**
	*	@brief	Returns true if this gamemode is one of the multiplayer gamemodes.
	**/
	const bool IsMultiplayer() const override { return false; }
	/**
	*	@brief	Returns true if this gamemode allows saving the game.
	**/
	const bool AllowSaveGames() const override; // Singleplayer allows saving games.

	/**
	*	@brief	Called once by client during connection to the server. Called once by
	*			the server when a new game is started.
	**/
	virtual void PrepareCVars() override { };
};



/**
*
*
*
*	Cooperative GameMode:
*
*
*
**/
struct sg_cooperative_gamemode_t : public sg_igamemode_t {
	/**
	*   @brief  Returns the game mode type.
	**/
	const sg_gamemode_type_t GetGameModeType() const override { return GAMEMODE_TYPE_COOPERATIVE; }
	/**
	*   @brief  Returns the game mode name.
	**/
	const char *GetGameModeName() const override { return "Cooperative"; }
	/**
	*   @brief  Returns the game mode description.
	**/
	const char *GetGameModeDescription() const override { return "Play with other clients against monsters."; }

	/**
	*	@brief	Returns true if this gamemode is one of the multiplayer gamemodes.
	**/
	const bool IsMultiplayer() const override { return true; }
	/**
	*	@brief	Returns true if this gamemode allows saving the game.
	**/
	const bool AllowSaveGames() const override; // Cooperative allows saving games.

	/**
	*	@brief	Called once by client during connection to the server. Called once by
	*			the server when a new game is started.
	**/
	virtual void PrepareCVars() override { };
};


/**
*
*
*
*	DeathMatch GameMode:
*
*
*
**/
struct sg_deathmatch_gamemode_t : public sg_igamemode_t {
	/**
	*   @brief  Returns the game mode type.
	**/
	const sg_gamemode_type_t GetGameModeType() const override { return GAMEMODE_TYPE_DEATHMATCH; }
	/**
	*   @brief  Returns the game mode name.
	**/
	const char *GetGameModeName() const override { return "Deathmatch"; }
	/**
	*   @brief  Returns the game mode description.
	**/
	const char *GetGameModeDescription() const override { return "Play versus other clients."; }

	/**
	*	@brief	Returns true if this gamemode is one of the multiplayer gamemodes.
	**/
	const bool IsMultiplayer() const override { return true; }
	/**
	*	@brief	Returns true if this gamemode allows saving the game.
	**/
	const bool AllowSaveGames() const override;	// Deathmatch does not allow saving games.

	/**
	*	@brief	Called once by client during connection to the server. Called once by
	*			the server when a new game is started.
	**/
	virtual void PrepareCVars() override { };
};



/**
*
*
*
*	Team DeathMatch GameMode:
*
*
*
**/



/**
*
*
*
*	Game Mode API:
*
*
*
**/
#if 0
/**
*	@brief	Factory function to create a game mode object based on the gameModeType.
**/
sg_igamemode_t *SG_CreateGameMode( const sg_gamemode_type_t gameModeType );
#endif

/**
*
*
*
*	Game Mode related Game API Entry Points:
*
*
*
**/
/**
*	@return	True if the game mode is a legitimate existing one.
**/
const bool SG_IsValidGameModeType( const sg_gamemode_type_t gameModeType );
/**
*   @return True if the game mode is multiplayer.
**/
const bool SG_IsMultiplayerGameMode( const sg_gamemode_type_t gameModeType );
/**
*	@return	The default game mode which is to be set. Used in case of booting a dedicated server without gamemode args.
**/
const sg_gamemode_type_t SG_GetDefaultMultiplayerGameModeType();
/**
*	@return	The actual ID of the current gamemode type.
**/
const sg_gamemode_type_t SG_GetRequestedGameModeType();

/**
*	@return	A string representative of the passed in gameModeType.
**/
const char *SG_GetGameModeName( const sg_gamemode_type_t gameModeType );