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

#include "local_entities/clg_local_entity_classes.h"
#include "local_entities/clg_local_env_sound.h"

void CLG_DetermineEAXEffect();

/**
*	@brief	Called when the client state has moved into being active and the game begins.
**/
void PF_ClientBegin( void ) {
	// Debug notify.
	clgi.Print( PRINT_NOTICE, "[CLGame]: PF_ClientBegin\n" );

	// Set the default reverb effect.
	clgi.S_SetActiveReverbEffect( precache.cl_eax_reverb_effects[ SOUND_EAX_EFFECT_DEFAULT ] );
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

	// Figure out the exact 'reverb' effect for our client to use.
	CLG_DetermineEAXEffect();

	// Debug print.
	//clgi.Print( PRINT_DEVELOPER, "%s: framenum(%ld), time(%ld)\n", 
	//	__func__, level.framenum, level.time.milliseconds() );
}
/**
*	@brief	Called at the rate equal to that of the refresh frames.
**/
void PF_ClientRefreshFrame( void ) {
	//clgi.Print( PRINT_NOTICE, "%s\n", __func__ );
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
	*	We update the reverb effects at the rate of our rendering so it always feels smooth and real-time.
	**/
	if ( clgi.client->predictedState.view.rdflags & RDF_UNDERWATER ) {
		clgi.S_SetActiveReverbEffect( precache.cl_eax_reverb_effects[ SOUND_EAX_EFFECT_UNDERWATER ] );
	} else {
		// Apply the default reverb effect.
		//clgi.S_SetActiveReverbEffect( precache.cl_eax_reverb_effects[ SOUND_EAX_EFFECT_DEFAULT ] );
		// Apply the current level set reverb effect.
		clgi.S_SetActiveReverbEffect( precache.cl_eax_reverb_effects[ level.reverbEffectID ] );
	}
}




static qhandle_t eaxOld = SOUND_EAX_EFFECT_DEFAULT;
static qhandle_t eaxID = SOUND_EAX_EFFECT_DEFAULT;
static float eaxTime = 0.f;

void CLG_EAX_SetEnvironment( qhandle_t id ) {
	// OOB, skip.
	if ( id > precache.cl_num_eax_reverb_effects ) {
		return;
	}

	// Identical, skip.
	if ( eaxID == id ) {
		return;
	}

	// Swap current to old.
	eaxOld = eaxID;
	eaxID = id;
	eaxTime = 0.f;

	//eaxOld = SOUND_EAX_EFFECT_DEFAULT;
	//eaxID = SOUND_EAX_EFFECT_DEFAULT;
	//eaxTime = 0.f;
}
void CLG_DetermineEAXEffect() {
	// Acquire player entity origin.
	const Vector3 playerOrigin = clgi.client->playerEntityOrigin;

	// Iterate over the env_sound entities, if we're within their radius, return the appropriate reverb eax id.
	float bestDistance = CM_MAX_WORLD_SIZE;
	clg_local_entity_t *best_sound_scape = nullptr;

	for ( int32_t i = 0; i < level.env_sound_list_count; i++ ) {
		// Get ptr to entity.
		clg_local_entity_t *env_sound = level.env_sound_list[ i ];
		// Get class locals.
		clg_env_sound_locals_t *classLocals = CLG_LocalEntity_GetClass<clg_env_sound_locals_t>( env_sound );

		// Perform a trace from the env_sound to the player origin.
		vec3_t traceStart = {};
		VectorCopy( env_sound->locals.origin, traceStart );
		vec3_t traceEnd = {};
		VectorCopy( playerOrigin, traceEnd );
		const trace_t trace = clgi.Clip( traceStart, vec3_origin, vec3_origin, traceEnd, nullptr, MASK_SOLID );

		// If we hit something, skip this env_sound entity.
		if ( trace.fraction < 1.0f ) {
			continue;
		}

		// See if we're within distance of the current env_sound.
		const float dist = QM_Vector3Length( env_sound->locals.origin - playerOrigin );
		// Skip if we're NOT:
		if ( dist > classLocals->radius ) {
			continue;
		}
		// If larger than the best distance we got so far, skip.
		if ( dist > bestDistance ) {
			continue;
		}

		// Otherwise store best distance.
		bestDistance = dist;
		// Store the effect ID.
		level.reverbEffectID = classLocals->reverbID;
		// Turn the best entity nullptr into a valid pointer, so the code below won't revert to the default effect.
		best_sound_scape = env_sound;
		//// Fetch env_sound origin.
		//Vector3 env_sound_origin = env_sound->locals.origin;
		//// Determine distance between 
		//const float dist = QM_Vector3Distance( env_sound_origin, playerOrigin );

		//clg_env_sound_locals_t *classLocals = CLG_LocalEntity_GetClass<clg_env_sound_locals_t>( sound_scape_entity );

		//if ( dist < bestDistance && dist < classLocals->radius ) {
		//	bestDistance = dist;
		//	best_sound_scape = level.env_sound_list[ i ];
		//}
	}

	if ( best_sound_scape == nullptr ) {
		level.reverbEffectID = SOUND_EAX_EFFECT_DEFAULT;
	}
	//// Did we find any specific nearby sound scape? If so, use its reverb properties instead.
	//qhandle_t reverb_eax_id = SOUND_EAX_EFFECT_DEFAULT;
	//if ( best_sound_scape ) {
	//	clg_env_sound_locals_t *classLocals = CLG_LocalEntity_GetClass<clg_env_sound_locals_t>( best_sound_scape );
	//	reverb_eax_id = classLocals->reverbID;
	//}
}
//void
//EFX_SetEnvironment( int id ) {
//	/* out of bounds... MAP BUG! */
//	if ( id >= g_efx_count )
//		return;
//
//	/* same as before, skip */
//	if ( g_iEFX == id )
//		return;
//
//	g_iEFXold = g_iEFX;
//	g_iEFX = id;
//	g_flEFXTime = 0.0f;
//}
//
//void
//EFX_Interpolate( int id ) {
//	mix.flDensity = Math_Lerp( mix.flDensity, g_efx[ id ].flDensity, g_flEFXTime );
//	mix.flDiffusion = Math_Lerp( mix.flDiffusion, g_efx[ id ].flDiffusion, g_flEFXTime );
//	mix.flGain = Math_Lerp( mix.flGain, g_efx[ id ].flGain, g_flEFXTime );
//	mix.flGainHF = Math_Lerp( mix.flGainHF, g_efx[ id ].flGainHF, g_flEFXTime );
//	mix.flGainLF = Math_Lerp( mix.flGainLF, g_efx[ id ].flGainLF, g_flEFXTime );
//	mix.flDecayTime = Math_Lerp( mix.flDecayTime, g_efx[ id ].flDecayTime, g_flEFXTime );
//	mix.flDecayHFRatio = Math_Lerp( mix.flDecayHFRatio, g_efx[ id ].flDecayHFRatio, g_flEFXTime );
//	mix.flDecayLFRatio = Math_Lerp( mix.flDecayLFRatio, g_efx[ id ].flDecayLFRatio, g_flEFXTime );
//	mix.flReflectionsGain = Math_Lerp( mix.flReflectionsGain, g_efx[ id ].flReflectionsGain, g_flEFXTime );
//	mix.flReflectionsDelay = Math_Lerp( mix.flReflectionsDelay, g_efx[ id ].flReflectionsDelay, g_flEFXTime );
//	mix.flReflectionsPan[ 0 ] = Math_Lerp( mix.flReflectionsPan[ 0 ], g_efx[ id ].flReflectionsPan[ 0 ], g_flEFXTime );
//	mix.flReflectionsPan[ 1 ] = Math_Lerp( mix.flReflectionsPan[ 1 ], g_efx[ id ].flReflectionsPan[ 1 ], g_flEFXTime );
//	mix.flReflectionsPan[ 1 ] = Math_Lerp( mix.flReflectionsPan[ 2 ], g_efx[ id ].flReflectionsPan[ 2 ], g_flEFXTime );
//	mix.flLateReverbGain = Math_Lerp( mix.flLateReverbGain, g_efx[ id ].flLateReverbGain, g_flEFXTime );
//	mix.flLateReverbDelay = Math_Lerp( mix.flLateReverbDelay, g_efx[ id ].flLateReverbDelay, g_flEFXTime );
//	mix.flLateReverbPan[ 0 ] = Math_Lerp( mix.flLateReverbPan[ 0 ], g_efx[ id ].flLateReverbPan[ 0 ], g_flEFXTime );
//	mix.flLateReverbPan[ 1 ] = Math_Lerp( mix.flLateReverbPan[ 1 ], g_efx[ id ].flLateReverbPan[ 1 ], g_flEFXTime );
//	mix.flLateReverbPan[ 2 ] = Math_Lerp( mix.flLateReverbPan[ 2 ], g_efx[ id ].flLateReverbPan[ 2 ], g_flEFXTime );
//	mix.flEchoTime = Math_Lerp( mix.flEchoTime, g_efx[ id ].flEchoTime, g_flEFXTime );
//	mix.flEchoDepth = Math_Lerp( mix.flEchoDepth, g_efx[ id ].flEchoDepth, g_flEFXTime );
//	mix.flModulationTime = Math_Lerp( mix.flModulationTime, g_efx[ id ].flModulationTime, g_flEFXTime );
//	mix.flModulationDepth = Math_Lerp( mix.flModulationDepth, g_efx[ id ].flModulationDepth, g_flEFXTime );
//	mix.flAirAbsorptionGainHF = Math_Lerp( mix.flAirAbsorptionGainHF, g_efx[ id ].flAirAbsorptionGainHF, g_flEFXTime );
//	mix.flHFReference = Math_Lerp( mix.flHFReference, g_efx[ id ].flHFReference, g_flEFXTime );
//	mix.flLFReference = Math_Lerp( mix.flLFReference, g_efx[ id ].flLFReference, g_flEFXTime );
//	mix.flRoomRolloffFactor = Math_Lerp( mix.flRoomRolloffFactor, g_efx[ id ].flRoomRolloffFactor, g_flEFXTime );
//	mix.iDecayHFLimit = Math_Lerp( mix.iDecayHFLimit, g_efx[ id ].iDecayHFLimit, g_flEFXTime );
//}
//
//void
//EFX_UpdateListener( NSView playerView ) {
//	static int old_dsp;
//
//	vector vecPlayer;
//
//	if ( autocvar_s_al_use_reverb == FALSE ) {
//		return;
//	}
//
//	int s = (float)getproperty( VF_ACTIVESEAT );
//	pSeat = &g_seats[ s ];
//	vecPlayer = pSeat->m_vecPredictedOrigin;
//
//	float bestdist = 999999;
//	for ( entity e = world; ( e = find( e, classname, "env_sound" ) );) {
//		env_sound scape = (env_sound)e;
//
//		other = world;
//		traceline( scape.origin, vecPlayer, MOVE_OTHERONLY, scape );
//		if ( trace_fraction < 1.0f ) {
//			continue;
//		}
//
//		float dist = vlen( e.origin - vecPlayer );
//		if ( dist > scape.m_iRadius ) {
//			continue;
//		}
//
//		if ( dist > bestdist ) {
//			continue;
//		}
//
//		bestdist = dist;
//		EFX_SetEnvironment( scape.m_iRoomType );
//	}
//
//	makevectors( playerView.GetCameraAngle() );
//	SetListener( playerView.GetCameraOrigin(), v_forward, v_right, v_up, 12 );
//
//	if ( old_dsp == g_iEFX ) {
//		return;
//	}
//
//	#ifdef EFX_LERP
//	if ( g_flEFXTime < 1.0 ) {
//		EFX_Interpolate( g_iEFX );
//		setup_reverb( 12, &mix, sizeof( reverbinfo_t ) );
//	} else {
//		old_dsp = g_iEFX;
//	}
//	g_flEFXTime += clframetime;
//	#else
//	SndLog( "Changed style to %s (%i)",
//		g_efx_name[ g_iEFX ], g_iEFX );
//
//	old_dsp = g_iEFX;
//	setup_reverb( 12, &g_efx[ g_iEFX ], sizeof( reverbinfo_t ) );
//	#endif
//}