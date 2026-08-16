/********************************************************************
*
*
*	ServerGame: Crowd Console Commands
*	File: svg_crowd_commands.h
*	Description:
*		Developer console and test commands for controlling crowds,
*		moving to origins, following entities, changing styles, and listing state.
*
*
********************************************************************/
#pragma once

/**
*	@brief	Register all crowd developer console commands.
**/
void SVG_Crowd_RegisterCommands( void );

/**
*	@brief	Dispatch crowd server commands issued via 'sv <cmd>'.
*	@param	cmd	Command token (e.g. 'crowd_move', 'crowd_follow', etc.).
*	@return	True if the command was recognized and handled.
**/
bool SVG_Crowd_ServerCommand( const char *cmd );
