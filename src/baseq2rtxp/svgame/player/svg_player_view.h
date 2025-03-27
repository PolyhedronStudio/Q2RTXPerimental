/********************************************************************
*
*
*	ServerGame: Player View
*	NameSpace: "".
*
*
********************************************************************/
#pragma once


/**
*   @brief  This will be called once for each server frame, before running any other entities in the world.
**/
void SVG_Client_BeginServerFrame( svg_edict_t *ent );
/**
*	@brief	Called for each player at the end of the server frame, and right after spawning.
**/
void SVG_Client_EndServerFrame( svg_edict_t *ent );