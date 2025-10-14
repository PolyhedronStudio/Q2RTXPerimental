/********************************************************************
*
*
*	ServerGame: Player/Client Events Functionality:
*	NameSpace: "".
*
*
********************************************************************/
#pragma once



/**
*   @brief  Checks for player state generated events(usually by PMove) and processed them for execution.
**/
void SVG_CheckClientEvents( svg_base_edict_t *ent, player_state_t *ops, player_state_t *ps );