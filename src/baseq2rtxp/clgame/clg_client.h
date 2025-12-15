/********************************************************************
*
*
*	ClientGame:	Client Header
*
*
********************************************************************/
#pragma once

/**
*	@brief	Called when the client state has moved into being active and the game begins.
**/
void CLG_ClientBegin( void );
/**
*	@brief	Called when the client state has moved into being properly connected to server.
**/
void PF_ClientConnected( void );
/**
*	@brief	Called when the client state has moved into a disconnected state, before ending
*			the loading plague and starting to clear its state. (So it is still accessible.)
**/
void PF_ClientDisconnected( void );
/**
*	@brief	Called to update the client's local game entities, it runs at the same framerate
*			as the server game logic does.
**/
void PF_ClientLocalFrame( void );
/**
*	@brief	Called at the rate equal to that of the refresh frames.
**/
void PF_ClientRefreshFrame( void );