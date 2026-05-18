/********************************************************************
*
*
*	ServerGame: Player HUD Functionality:
*	NameSpace: "".
*
*
********************************************************************/
#pragma once



/***
*
*
*
*	GameUI HUD Core:
*
*
*
***/
enum class sg_game_ui_menu_id : int64_t;
/**
*	@brief	Opens up the MicroUI GameUI menu that matches the given menu name.
*	@note	If the menu name is not recognized, prints an error message to the client if possible,
*			otherwise prints it to all connected clients.
**/
void SVG_GameUI_OpenMenu( svg_base_edict_t *ent, const sg_game_ui_menu_id &menuID );



/***
*
*
*
*   Intermission:
*
*
*
***/
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