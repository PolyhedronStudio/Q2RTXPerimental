/*********************************************************************
*
*
*	SVGame: GameMode Base:
*
*
********************************************************************/
#pragma once


// Forward declarations:
struct sg_gamemode_base_t;


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
	virtual ~svg_gamemode_t() = default; // Virtual destructor for proper cleanup.

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
	*
	*	Must Implement:
	*
	**/
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
	virtual const bool ClientConnect( svg_player_edict_t *ent, char *userinfo ) = 0;
	/**
	*	@brief	Called when a a player purposely disconnects, or is dropped due to
	*			connectivity problems.
	**/
	virtual void ClientDisconnect( svg_player_edict_t *ent ) = 0;
	/**
	*   @brief  called whenever the player updates a userinfo variable.
	*
	*           The game can override any of the settings in place
	*           (forcing skins or names, etc) before copying it off.
	**/
	virtual void ClientUserinfoChanged( svg_player_edict_t *ent, char *userinfo ) = 0;
	/**
	*   @brief  Called either when a player connects to a server, OR (to respawn thus) respawns in a multiplayer game.
	*
	*           Will look up a spawn point, spawn(placing) the player 'body' into the server and (re-)initializing
	*           saved entity and persistant data. (This includes actually raising the weapon up.)
	**/
	virtual void ClientSpawnInBody( svg_player_edict_t *ent ) = 0;
	/**
	*   @brief  Called when a client has finished connecting, and is ready
	*           to be placed into the game. This will happen every level load.
	**/
	virtual void ClientBegin( svg_player_edict_t *ent ) = 0;

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
	*   @brief  This will be called once for all clients on each server frame, before running any other entities in the world.
	**/
	virtual void BeginServerFrame( svg_player_edict_t *ent ) = 0;
	/**
	*	@brief	Called for each player at the end of the server frame, also right after spawning as well to finalize the view.
	**/
	virtual void EndServerFrame( svg_player_edict_t *ent ) = 0;


	/**
	*
	*	Generic GameMode API:
	*
	**/
	/**
	*	@brief	Sets the spawn origin and angles to that matching the found spawn point.
	**/
	virtual svg_base_edict_t *SelectSpawnPoint( svg_player_edict_t *ent, Vector3 &origin, Vector3 &angles );
	/**
	*	@brief	Defines the behavior for the game mode when the level has to exit.
	**/
	virtual void ExitLevel();

	/**
	*   @brief  Returns the created target changelevel
	**/
	svg_base_edict_t *CreateTargetChangeLevel( const char *map );

/**
*	Support Routines:
**/
private:

};
