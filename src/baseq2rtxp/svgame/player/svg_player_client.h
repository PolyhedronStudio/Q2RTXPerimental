/********************************************************************
*
*
*	ServerGame: Player/Client Functionality:
*	NameSpace: "".
*
*
********************************************************************/
#pragma once



/**
*   Player:
**/
/**
*   @brief
**/
void player_pain( edict_t *self, edict_t *other, float kick, int damage );
/**
*   @brief
**/
void player_die( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point );
/**
*   @brief  Will reset the entity client's 'Field of View' back to its defaults.
**/
void SVG_Player_ResetPlayerStateFOV( gclient_t *client );
/**
*   @brief  For SinglePlayer: Called only once, at game first initialization.
*           For Multiplayer Modes: Called after each death, and level change.
**/
void SVG_Player_InitPersistantData( edict_t *ent, gclient_t *client );
/**
*   @brief  Clears respawnable client data, which stores the timing of respawn as well as a copy of
*           the client persistent data, to reapply after respawn.
**/
void SVG_Player_InitRespawnData( gclient_t *client );
/**
*    @brief  Some information that should be persistant, like health, is still stored in the edict structure.
*            So it needs to be mirrored out to the client structure before all the edicts are wiped.
**/
void SVG_Player_SaveClientData( void );
/**
*   @brief  Restore the client stored persistent data to reinitialize several client entity fields.
**/
void SVG_Player_RestoreClientData( edict_t *ent );
/**
*   @brief  Only called when pers.spectator changes.
*   @note   That resp.spectator should be the opposite of pers.spectator here
**/
void SVG_Player_SelectSpawnPoint( edict_t *ent, Vector3 &origin, Vector3 &angles );
/**
*   @brief
**/
void SVG_Player_Obituary( edict_t *self, edict_t *inflictor, edict_t *attacker );
/**
*   @brief  Called either when a player connects to a server, OR respawns in a multiplayer game.
*
*           Will look up a spawn point, spawn(placing) the player 'body' into the server and (re-)initializing
*           saved entity and persistant data. (This includes actually raising the weapon up.)
**/
void SVG_Player_PutInServer( edict_t *ent );

/**
*   @brief
**/
void SVG_Client_RespawnPlayer( edict_t *self );
/**
*   @brief  Only called when pers.spectator changes.
*   @note   That resp.spectator should be the opposite of pers.spectator here
**/
void SVG_Client_RespawnSpectator( edict_t *ent );
/**
*   @brief  called whenever the player updates a userinfo variable.
*
*           The game can override any of the settings in place
*           (forcing skins or names, etc) before copying it off.
**/
void SVG_Client_UserinfoChanged( edict_t *ent, char *userinfo );



/**
*
*
*
*   Client UseTargetHint Functionality.:
*
*
*
**/
/**
*   @brief  Unsets the current client stats usetarget info.
**/
void Client_ClearUseTargetHint( edict_t *ent, gclient_t *client, edict_t *useTargetEntity );
/**
*   @brief  Determines the necessary UseTarget Hint information for the hovered entity(if any).
*   @return True if the entity has legitimate UseTarget Hint information. False if unset, or not found at all.
**/
const bool SVG_Client_UpdateUseTargetHint( edict_t *ent, gclient_t *client, edict_t *useTargetEntity );



/**
*
*
*
*   Client Recoil Utilities:
*
*
*
**/
/**
*	@brief	Calculates the to be determined movement induced recoil factor.
**/
void SVG_Client_CalculateMovementRecoil( edict_t *ent );
/**
*   @brief  Calculates the final resulting recoil value, being clamped between -1, to +1.
**/
const double SVG_Client_GetFinalRecoilFactor( edict_t *ent );