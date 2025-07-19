/*********************************************************************
*
*
*	SVGame: GameMode(s):
*
*
********************************************************************/
#pragma once


// Forward declarations:
struct svg_gamemode_singleplayer_t;
struct svg_gamemode_cooperative_t;
struct svg_gamemode_deathmatch_t;


/**
*
*
*
*	ServerGame Base GameMode:
*
*
*
**/
/**
*	@note Extends sg_gamemode_base_t.
**/
struct svg_gamemode_t : public sg_gamemode_base_t {
	/**
	*	@brief	Returns true if this gamemode is one of the multiplayer gamemodes.
	**/
	virtual const bool IsMultiplayer() const = 0;
	/**
	*	@brief	Returns true if this gamemode allows saving the game.
	**/
	virtual const bool AllowSaveGames() const = 0; // Singleplayer allows saving games.

	/**
	*	@brief	Called once by client during connection to the server. Called once by
	*			the server when a new game is started.
	**/
	virtual void PrepareCVars() = 0;

	/**
	*	@brief	Called somewhere at the beginning of the game frame. This allows
	*			to determine if conditions are met to engage exitting intermission
	*			mode and/or exit the level.
	**/
	virtual void PreCheckGameRuleConditions() = 0;
	/**
	*	@brief	Called somewhere at the end of the game frame. This allows
	*			to determine if conditions are met to engage into intermission
	*			mode and/or exit the level.
	**/
	virtual void PostCheckGameRuleConditions() = 0;

	/**
	*   @brief  Returns the created target changelevel
	**/
	svg_base_edict_t *CreateTargetChangeLevel( const char *map );
	/**
	*	@brief	Defines the behavior for the game mode when the level has to exit.
	**/
	virtual void ExitLevel();
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
struct svg_gamemode_singleplayer_t : public svg_gamemode_t {
	/**
	*	@brief	Returns the game mode description.
	**/
	const sg_gamemode_type_t GetGameModeType() const override { return GAMEMODE_TYPE_SINGLEPLAYER; }
	/**
	*	@brief	Returns the game mode type.
	**/
	const char *GetGameModeName() const override { return "SinglePlayer"; }
	/**
	*	@brief	Returns the game mode description.
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
	void PrepareCVars() override;

	/**
	*	@brief	Called somewhere at the beginning of the game frame. This allows
	*			to determine if conditions are met to engage exitting intermission
	*			mode and/or exit the level.
	**/
	virtual void PreCheckGameRuleConditions() override;
	/**
	*	@brief	Called somewhere at the end of the game frame. This allows
	*			to determine if conditions are met to engage into intermission
	*			mode and/or exit the level.
	**/
	virtual void PostCheckGameRuleConditions() override;
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
struct svg_gamemode_cooperative_t : public svg_gamemode_t {
	/**
	*	@brief	Returns the game mode description.
	**/
	const sg_gamemode_type_t GetGameModeType() const override { return GAMEMODE_TYPE_COOPERATIVE; }
	/**
	*	@brief	Returns the game mode type.
	**/
	const char *GetGameModeName() const override { return "Cooperative"; }
	/**
	*	@brief	Returns the game mode description.
	**/
	const char *GetGameModeDescription() const override { return "Play cooperatively through an adventure."; }


	/**
	*	@brief	Returns true if this gamemode is one of the multiplayer gamemodes.
	**/
	const bool IsMultiplayer() const override { return true; }
	/**
	*	@brief	Returns true if this gamemode allows saving the game.
	**/
	const bool AllowSaveGames() const override; // Singleplayer allows saving games.


	/**
	*	@brief	Called once by client during connection to the server. Called once by
	*			the server when a new game is started.
	**/
	virtual void PrepareCVars() override;

	/**
	*	@brief	Called somewhere at the beginning of the game frame. This allows
	*			to determine if conditions are met to engage exitting intermission
	*			mode and/or exit the level.
	**/
	virtual void PreCheckGameRuleConditions() override;
	/**
	*	@brief	Called somewhere at the end of the game frame. This allows
	*			to determine if conditions are met to engage into intermission
	*			mode and/or exit the level.
	**/
	virtual void PostCheckGameRuleConditions() override;
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
struct svg_gamemode_deathmatch_t : public svg_gamemode_t {
	/**
*	@brief	Returns the game mode description.
**/
	const sg_gamemode_type_t GetGameModeType() const override { return GAMEMODE_TYPE_DEATHMATCH; }
	/**
	*	@brief	Returns the game mode type.
	**/
	const char *GetGameModeName() const override { return "DeathMatch"; }
	/**
	*	@brief	Returns the game mode description.
	**/
	const char *GetGameModeDescription() const override { return "Play a deathmatch versus all other players."; }


	/**
	*	@brief	Returns true if this gamemode is one of the multiplayer gamemodes.
	**/
	const bool IsMultiplayer() const override { return true; }
	/**
	*	@brief	Returns true if this gamemode allows saving the game.
	**/
	const bool AllowSaveGames() const override; // Singleplayer allows saving games.


	/**
	*	@brief	Called once by client during connection to the server. Called once by
	*			the server when a new game is started.
	**/
	virtual void PrepareCVars() override;

	/**
	*	@brief	Called somewhere at the beginning of the game frame. This allows
	*			to determine if conditions are met to engage exitting intermission
	*			mode and/or exit the level.
	**/
	virtual void PreCheckGameRuleConditions() override;
	/**
	*	@brief	Called somewhere at the end of the game frame. This allows
	*			to determine if conditions are met to engage into intermission
	*			mode and/or exit the level.
	**/
	virtual void PostCheckGameRuleConditions() override;

	/**
	*	@brief	Ends the DeathMatch, switching to intermission mode, and finding
	*			the next level to play. After which it will spawn a TargetChangeLevel entity.
	**/
	void EndDeathMatch();
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
svg_gamemode_t *SVG_AllocateGameModeInstance( const sg_gamemode_type_t gameModeType );

/**
*	@return	True in case the current gamemode allows for saving the game.
*			(This should only be true for single and cooperative play modes.)
**/
const bool SVG_GameModeAllowSaveGames( const bool isDedicatedServer );