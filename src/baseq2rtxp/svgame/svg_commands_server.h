/********************************************************************
*
*
*	SVGame: Server(Game Admin Stuff alike) Console Commands:
*
*
********************************************************************/
#pragma once



/**
*   @brief  SVG_ServerCommand will be called when an "sv" command is issued.
*           The game can issue gi.argc() / gi.argv() commands to get the rest
*           of the parameters
**/
void SVG_ServerCommand( void );

/**
*	@brief	
**/
const bool SVG_FilterPacket( char *from );