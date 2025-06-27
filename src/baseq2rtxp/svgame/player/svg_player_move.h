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
*   @brief  This will be called once for each client frame, which will usually
*           be a couple times for each server frame.
**/
void SVG_Client_Think( svg_base_edict_t *ent, usercmd_t *ucmd );