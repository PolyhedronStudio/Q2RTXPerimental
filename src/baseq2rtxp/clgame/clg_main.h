/********************************************************************
*
*
*	ClientGame: Main Header
*
*
********************************************************************/
#pragma once


/**
*   @return True if the server, or client, is non active(and/or thus paused).
*	@note	clientOnly, false by default, is optional to check if just the client is paused.
**/
const bool CLG_IsGameplayPaused( const bool clientOnly = false );