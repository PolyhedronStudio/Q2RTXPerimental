/********************************************************************
*
*
*	ServerGame: Player UseTarget Functionality:
*	NameSpace: "".
*
*
********************************************************************/
#pragma once



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
*   @brief
**/
void SVG_Player_TraceForUseTarget( svg_player_edict_t *ent, svg_client_t *client, const bool processUserInput = false );
/**
*   @brief  Unsets the current client stats usetarget info.
**/
void SVG_Player_ClearUseTargetHint( svg_base_edict_t *ent, svg_client_t *client, svg_base_edict_t *useTargetEntity );
