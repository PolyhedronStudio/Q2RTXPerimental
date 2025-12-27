/********************************************************************
*
*
*	ClientGame: Similar to ServerGame, the ClientGame's implementation
*	of the ClientXXXX functions.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_client.h"
#include "clgame/clg_eax.h"
#include "clgame/clg_entities.h"
#include "clgame/clg_hud.h"
#include "clgame/clg_local_entities.h"
#include "clgame/clg_predict.h"

#include "sharedgame/sg_entities.h"
#include "sharedgame/pmove/sg_pmove.h"



/**
*
*
*
*	Utility Functions:
*
*
*
**/



/**
*	@brief	Called when the client state has moved into being active and the game begins.
**/
void CLG_ClientBegin( void ) {
	// Debug notify.
	clgi.Print( PRINT_NOTICE, "[CLGame]: CLG_ClientBegin\n" );

	/**
	*	Initialize the 'Initial' Predicted State:
	**/
	// Set the initial client predicted state values.
	game.predictedState.currentPs = clgi.client->frame.ps;
	game.predictedState.lastPs = game.predictedState.currentPs;
	// Use predicted view angles if we're alive:
	if ( clgi.client->frame.ps.pmove.pm_type < PM_DEAD ) { // OLD Q2PRO: enhanced servers don't send viewangles
		CLG_PredictAngles();
	// Otherwise, use whatever server provided:
	} else {
		game.predictedState.currentPs.viewangles = clgi.client->frame.ps.viewangles;
		game.predictedState.lastPs.viewangles = clgi.client->frame.ps.viewangles;
	}

	/**
	*	Reset predicted Sequence and State Events:
	**/
	// Reset predicted state eventSequence.
	game.predictedState.eventSequence = clgi.client->frame.ps.eventSequence;
	// Clear predicted state events.
	for ( int32_t i = 0; i < client_predicted_state_t::MAX_PREDICTED_EVENTS; i++ ) {
		game.predictedState.events[ i ] = EV_NONE;
	}
	/**
	*	Reset local(view - )transitions.
	**/
	game.predictedState.transition = {};
	/**
	*	Reset ground information.
	**/
	game.predictedState.ground = {};
	game.predictedState.liquid = {};

	// Setup predicted player entity's current entity state.
	SG_PlayerStateToEntityState( clgi.client->clientNumber, &game.predictedState.currentPs, &game.predictedEntity.current, true, false );

	// Setup predicted player entity's refresh entity.
	game.predictedEntity.refreshEntity = {
		// Store the frame of the previous refresh entity iteration so it'll default to that.
		.frame = game.predictedEntity.refreshEntity.frame,
		//.oldframe = packetEntity->refreshEntity.oldframe,
		.id = REFRESHENTITIY_RESERVED_PREDICTED_PLAYER,
		// Restore bone poses cache pointer.
		.bonePoses = game.predictedEntity.refreshEntity.bonePoses,
	};

	// Set the default environment reverb effect.
	CLG_EAX_HardSetEnvironment( SOUND_EAX_EFFECT_DEFAULT );
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
	CLG_HUD_ClearChat_f();

	// Debug notify.
	clgi.Print( PRINT_NOTICE, "[CLGame]: PF_ClientDisconnected\n" );
}

/**
*	@brief	Simulate similar the server side, a ClientBeginFrame function.
**/
static void CLG_ClientBeginLocalFrame( void ) {

}

/**
*	@brief	Simulate similar the server side, a ClientEndFrame function.
**/
static void CLG_ClientEndLocalFrame( void ) {
	// Figure out the exact 'reverb' effect for our client to use.
	CLG_EAX_DetermineEffect();
}

/**
*	@brief	Called to update the client's local game entities, it runs at the same framerate
*			as the server game logic does.
**/
void PF_ClientLocalFrame( void ) {
	// Increase the frame number we're in for this level..
	level.frameNumber++;
	// Increase the amount of time that has passed for this level.
	level.time += FRAME_TIME_MS;

	// Reseed the mersennery twister.
	mt_rand.seed( clgi.client->frame.number );

	//clgi.Print( PRINT_NOTICE, "%ull\n", clgi.client->frame.number );

	// Call upon ClientBeginFrame.
	CLG_ClientBeginLocalFrame();

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
	}

	// The client's frame ending functionality.
	CLG_ClientEndLocalFrame();

// Debug print: framenum, level time.
#if 0
	clgi.Print( PRINT_DEVELOPER, "%s: framenum(%ld), time(%ld)\n", 
		__func__, level.frameNumber, level.time.Milliseconds() );
#endif
}
/**
*	@brief	Called at the rate equal to that of the refresh frames.
**/
void PF_ClientRefreshFrame( void ) {
	// Give local entities a chance at being added to the current render frame.
	for ( int32_t i = 0; i < clg_num_local_entities; i++ ) {
		// Get local entity pointer.
		clg_local_entity_t *lent = &clg_local_entities[ i ];

		// Skip iteration if entity is not in use, or not properly class allocated.
		if ( !lent || !lent->inuse || !lent->classLocals ) {
			continue;
		}

		// Dispatch 'refresh frame'.
		CLG_LocalEntity_DispatchRefreshFrame( lent );
	}

	/**
	*	We update the reverb effect if needed due to client prediction, by the framerate's rate.
	**/
	// Force orverride it for underwater flag.
	if ( ( clgi.client->frame.ps.rdflags | game.predictedState.currentPs.rdflags ) & RDF_UNDERWATER ) {
		// Apply underwater reverb.
		CLG_EAX_SetEnvironment( SOUND_EAX_EFFECT_UNDERWATER );

		// Persue to now activate eax environment that has been set.
		CLG_EAX_ActivateCurrentEnvironment();
	}
}