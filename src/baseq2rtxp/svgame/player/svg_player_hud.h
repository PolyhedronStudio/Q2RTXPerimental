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
void SVG_HUD_BeginIntermission( svg_base_edict_t *targ );
/**
*   @brief  Will move the client to the intermission point, setting the client's origin and viewangles.
*			Sets the client to intermission state, clearing out most of the entity's state and other client related data.
**/
void SVG_HUD_MoveClientToIntermission( svg_base_edict_t *client );
/**
*   @brief  Will update the client's player_state_t stats array with the current client entity's values.
**/
void SVG_HUD_SetStats( svg_base_edict_t *ent );
/**
*   @brief
**/
void SVG_HUD_SetSpectatorStats( svg_base_edict_t *ent );
/**
*   @brief
**/
void SVG_HUD_CheckChaseStats( svg_base_edict_t *ent );
/**
*   @brief	Will engage in send generating and sending the svc_scoreboard message, optionally as part of a Reliable packet.
**/
void SVG_HUD_DeathmatchScoreboardMessage( svg_base_edict_t *ent, svg_base_edict_t *killer, const bool sendAsReliable = false );