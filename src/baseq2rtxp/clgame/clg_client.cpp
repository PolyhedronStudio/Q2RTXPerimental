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
#include "clgame/clg_hud.h"
#include "clgame/clg_local_entities.h"
#include "clgame/clg_predict.h"

#include "sharedgame/pmove/sg_pmove.h"



/**
*	@brief	Called when the client state has moved into being active and the game begins.
**/
void PF_ClientBegin( void ) {
	// Debug notify.
	clgi.Print( PRINT_NOTICE, "[CLGame]: PF_ClientBegin\n" );

	if ( clgi.IsDemoPlayback() ) {
		// init some demo things
		//CL_FirstDemoFrame();
	} else {
		// Set the initial client predicted state values.
		game.predictedState.currentPs = clgi.client->frame.ps;
		game.predictedState.lastPs = game.predictedState.currentPs;
		//VectorCopy(cl.frame.ps.pmove.origin, cl.predictedState.view.origin);//VectorScale(cl.frame.ps.pmove.origin, 0.125f, cl.predicted_origin); // WID: float-movement
		//VectorCopy(cl.frame.ps.pmove.velocity, cl.predictedState.view.velocity);//VectorScale(cl.frame.ps.pmove.velocity, 0.125f, cl.predicted_velocity); // WID: float-movement
		// 
		// Use predicted view angles if we're alive:
		if ( clgi.client->frame.ps.pmove.pm_type < PM_DEAD ) { // OLD Q2PRO: enhanced servers don't send viewangles
			PF_PredictAngles();
		// Otherwise, use whatever server provided.
		} else {
			// just use what server provided
			game.predictedState.currentPs.viewangles = clgi.client->frame.ps.viewangles;
			game.predictedState.lastPs.viewangles = clgi.client->frame.ps.viewangles;
		}
	}

	// Reset local (view-)transitions.
	game.predictedState.transition = {};
	//cl.predictedState.time.height_changed = 0;
	//cl.predictedState.time.step_changed = 0;

	// Reset ground information.
	game.predictedState.ground = {};
	game.predictedState.liquid = {};

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