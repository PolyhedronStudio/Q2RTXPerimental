/********************************************************************
*
*
*	ClientGame: Similar to ServerGame, the ClientGame's implementation
*	of the ClientXXXX functions.
*
*
********************************************************************/
#include "clg_local.h"
#include "clg_client.h"
#include "clg_local_entities.h"
#include "clg_screen.h"

/**
*	@brief	Called when the client state has moved into being active and the game begins.
**/
void PF_ClientBegin( void ) {
	// Debug notify.
	clgi.Print( PRINT_NOTICE, "[CLGame]: PF_ClientBegin\n" );
}

/**
*	@brief	Called when the client state has moved into being properly connected to server.
**/
void PF_ClientConnected( void ) {
	// Debug notify.
	clgi.Print( PRINT_NOTICE, "[CLGame]: PF_ClientConnected\n" );
}

/**
*	@brief	Called when the client state has moved into a disconnected state, before ending
*			the loading plague and starting to clear its state. (So it is still accessible.)
**/
void PF_ClientDisconnected( void ) {
	// Clear chat HUD when disconnected.
	SCR_ClearChatHUD_f();

	// Debug notify.
	clgi.Print( PRINT_NOTICE, "[CLGame]: PF_ClientDisconnected\n" );
}
/**
*	@brief	Called to update the client's local game entities, it runs at the same framerate
*			as the server game logic does.
**/
void PF_ClientLocalFrame( void ) {
	// Increase the frame number we're in for this level..
	level.framenum++;
	// Increase the amount of time that has passed for this level.
	level.time += FRAME_TIME_MS;

	// Let all local entities think
	for ( int32_t i = 0; i < clg_num_local_entities; i++ ) {
		// Get local entity pointer.
		clg_local_entity_t *lent = &clg_local_entities[ i ];

		// Skip iteration if entity is not in use, or not properly class allocated.
		if ( !lent || !lent->inuse || !lent->classLocals || !lent->islinked ) {
			continue;
		}

		// Dispatch think callback since the entity is in-use and properly class allocated.
		CLG_LocalEntity_RunThink( lent );
		//CLG_LocalEntity_DispatchThink( lent );
	}
	// Debug print.
	//clgi.Print( PRINT_DEVELOPER, "%s: framenum(%ld), time(%ld)\n", 
	//	__func__, level.framenum, level.time.milliseconds() );
}
/**
*	@brief	Called at the rate equal to that of the refresh frames.
**/
void PF_ClientRefreshFrame( void ) {
	//clgi.Print( PRINT_NOTICE, "%s\n", __func__ );
	// Let all local entities think
	for ( int32_t i = 0; i < clg_num_local_entities; i++ ) {
		// Get local entity pointer.
		clg_local_entity_t *lent = &clg_local_entities[ i ];

		// Skip iteration if entity is not in use, or not properly class allocated.
		if ( !lent || !lent->inuse || !lent->classLocals ) {
			continue;
		}

		// Dispatch think callback since the entity is in-use and properly class allocated.
		CLG_LocalEntity_DispatchRefreshFrame( lent );
	}
}
