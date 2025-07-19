/*********************************************************************
*
*
*	SVGame: GameMode(s):
*
*
********************************************************************/
#pragma once


// Forward declarations:
struct sg_singleplayer_gamemode_t;
struct sg_cooperative_gamemode_t;
struct sg_deathmatch_gamemode_t;


/**
*
*
*
*	Game Mode Class Objects:
*
*
*
**/
/**
*
*
*
*	SinglePlayer GameMode(Also the default for Unknown ID -1):
*
*
*
**/
struct svg_singleplayer_gamemode_t : public sg_singleplayer_gamemode_t {
	#if 0
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
	#endif

	/**
	*	@brief	Called once by client during connection to the server. Called once by
	*			the server when a new game is started.
	**/
	void PrepareCVars() override;
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
struct svg_cooperative_gamemode_t : public sg_cooperative_gamemode_t {
	#if 0
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
	#endif

	/**
	*	@brief	Called once by client during connection to the server. Called once by
	*			the server when a new game is started.
	**/
	void PrepareCVars() override;
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
struct svg_deathmatch_gamemode_t : public sg_deathmatch_gamemode_t {
	#if 0
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
	#endif

	/**
	*	@brief	Called once by client during connection to the server. Called once by
	*			the server when a new game is started.
	**/
	void PrepareCVars() override;
};

/**
*
*
*
*	Game Mode API:
*
*
*
**/
/**
*	@brief	Factory function to create a game mode object based on the gameModeType.
**/
sg_igamemode_t *SVG_AllocateGameModeInstance( const sg_gamemode_type_t gameModeType );

/**
*	@return	True in case the current gamemode allows for saving the game.
*			(This should only be true for single and cooperative play modes.)
**/
const bool SVG_GameModeAllowSaveGames( const bool isDedicatedServer );