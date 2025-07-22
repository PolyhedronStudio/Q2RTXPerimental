/*********************************************************************
*
*
*	SVGame: GameMode SinglePlayer:
*
*
********************************************************************/
#pragma once


// Forward declarations:
struct svg_gamemode_t;


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
	virtual ~svg_gamemode_singleplayer_t() = default; // Virtual destructor for proper cleanup.

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
	*   @details    Called when a player begins connecting to the server.
	*
	*               The game can refuse entrance to a client by returning false.
	*
	*               If the client is allowed, the connection process will continue
	*               and eventually get to ClientBegin()
	*
	*               Changing levels will NOT cause this to be called again, but
	*               loadgames WILL.
	**/
	virtual const bool ClientConnect( svg_player_edict_t *ent, char *userinfo ) override;
	/**
	*   @brief  called whenever the player updates a userinfo variable.
	*
	*           The game can override any of the settings in place
	*           (forcing skins or names, etc) before copying it off.
	**/
	virtual void ClientUserinfoChanged( svg_player_edict_t *ent, char *userinfo ) override;
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
	*	@brief	Sets the spawn origin and angles to that matching the found spawn point.
	**/
	virtual svg_base_edict_t *SelectSpawnPoint( svg_player_edict_t *ent, Vector3 &origin, Vector3 &angles ) override;
};