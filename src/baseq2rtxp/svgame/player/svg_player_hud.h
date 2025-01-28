/********************************************************************
*
*
*	ServerGame: Player HUD Functionality:
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
*   @brief  Draw instead of help message.
*   @note that it isn't that hard to overflow the 1400 byte message limit!
**/
void SVG_HUD_DeathmatchScoreboard( edict_t *ent );
/**
*   @brief
**/
void SVG_HUD_DeathmatchScoreboardMessage( edict_t *ent, edict_t *killer );