/********************************************************************
*
*
*	ServerGame: Player Weapon Functionality:
*	NameSpace: "".
*
*
********************************************************************/
#pragma once



/**
*   @brief
**/
void SVG_HUD_BeginIntermission( edict_t *targ );
/**
*   @brief
**/
void SVG_HUD_MoveClientToIntermission( edict_t *client );
/**
*   @brief  Will update the client's player_state_t stats array with the current client entity's values.
**/
void SVG_HUD_SetStats( edict_t *ent );
/**
*   @brief
**/
void SVG_HUD_SetSpectatorStats( edict_t *ent );
/**
*   @brief
**/
void SVG_HUD_CheckChaseStats( edict_t *ent );
/**
*   @brief
**/
void SVG_HUD_ValidateSelectedItem( edict_t *ent );
/**
*   @brief
**/
void SVG_HUD_DeathmatchScoreboardMessage( edict_t *client, edict_t *killer );
