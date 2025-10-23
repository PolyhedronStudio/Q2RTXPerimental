/*********************************************************************
*
*
*	SVGame: GameMode Cooperative:
*
*
********************************************************************/
#pragma once



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
	virtual ~svg_gamemode_cooperative_t() = default; // Virtual destructor for proper cleanup.

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
	*	@brief	Called when a a player purposely disconnects, or is dropped due to
	*			connectivity problems.
	**/
	virtual void ClientDisconnect( svg_player_edict_t *ent ) override;
	/**
	*   @brief  called whenever the player updates a userinfo variable.
	*
	*           The game can override any of the settings in place
	*           (forcing skins or names, etc) before copying it off.
	**/
	virtual void ClientUserinfoChanged( svg_player_edict_t *ent, char *userinfo ) override;
	/**
	*   @brief  Called either when a player connects to a server, OR (to respawn thus) respawns in a multiplayer game.
	*
	*           Will look up a spawn point, spawn(placing) the player 'body' into the server and (re-)initializing
	*           saved entity and persistant data. (This includes actually raising the weapon up.)
	**/
	virtual void ClientSpawnInBody( svg_player_edict_t *ent ) override;
	/**
	*   @brief  Called when a client has finished connecting, and is ready
	*           to be placed into the game. This will happen every level load.
	**/
	virtual void ClientBegin( svg_player_edict_t *ent ) override;


	/**
	*	@brief	Overrides to update client scores when killing monsters in coop.
	**/
	virtual void EntityKilled( svg_base_edict_t *targ, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int32_t damage, Vector3 *point ) override;


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
	*   @brief  This will be called once for all clients on each server frame, before running any other entities in the world.
	**/
	virtual void BeginServerFrame( svg_player_edict_t *ent ) override;
	/**
	*	@brief	Called for each player at the end of the server frame, also right after spawning as well to finalize the view.
	**/
	virtual void EndServerFrame( svg_player_edict_t *ent ) override;

	/**
	*   @brief  This function is used to apply damage to an entity.
	*           It handles the damage calculation, knockback, and any special
	*           effects based on the type of damage and the entities involved.
	**/
	virtual void DamageEntity( svg_base_edict_t *targ, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, const Vector3 &dir, Vector3 &point, const Vector3 &normal, const int32_t damage, const int32_t knockBack, const entity_damageflags_t damageFlags, const sg_means_of_death_t meansOfDeath ) override;

	/**
	*	@brief	Sets the spawn origin and angles to that matching the found spawn point.
	**/
	virtual svg_base_edict_t *SelectSpawnPoint( svg_player_edict_t *ent, Vector3 &origin, Vector3 &angles ) override;

private:
	/**
	*	@brief  Will select a coop spawn point for the player.
	**/
	svg_base_edict_t *SelectCoopSpawnPoint( svg_player_edict_t *ent );

	/**
	*   @brief  A client connected by loadgame(assuming singleplayer), there is already
	*           a body waiting for us to use. We just need to adjust its angles.
	**/
	void SetupLoadGameBody( svg_player_edict_t *ent );
	/**
	*   @brief  There is no body waiting for us yet, so (re-)initialize the entity we have with a full new 'body'.
	**/
	void SetupNewGameBody( svg_player_edict_t *ent );
};