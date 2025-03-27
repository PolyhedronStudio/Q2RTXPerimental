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
void SVG_HUD_BeginIntermission( svg_entity_t *targ );
/**
*   @brief
**/
void SVG_HUD_MoveClientToIntermission( svg_entity_t *client );
/**
*   @brief  Will update the client's player_state_t stats array with the current client entity's values.
**/
void SVG_HUD_SetStats( svg_entity_t *ent );
/**
*   @brief
**/
void SVG_HUD_SetSpectatorStats( svg_entity_t *ent );
/**
*   @brief
**/
void SVG_HUD_CheckChaseStats( svg_entity_t *ent );
/**
*   @brief	Will engage in send generating and sending the svc_scoreboard message, optionally as part of a Reliable packet.
**/
void SVG_HUD_DeathmatchScoreboardMessage( svg_entity_t *ent, svg_entity_t *killer, const bool sendAsReliable = false );