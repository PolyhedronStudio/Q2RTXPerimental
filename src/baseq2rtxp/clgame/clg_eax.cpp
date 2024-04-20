/********************************************************************
*
*
*	ClientGame: EAX Effects.
*
*
********************************************************************/
#include "clg_local.h"
#include "clg_eax.h"
#include "clg_eax_effects.h"
#include "clg_local_entities.h"
#include "local_entities/clg_local_entity_classes.h"
#include "local_entities/clg_local_env_sound.h"

/**
*	@brief	Will 'Hard Set' instantly, to the passed in EAX reverb properties. Used when clearing state,
*			as well as on ClientBegin calls.
**/
void CLG_EAX_HardSetEnvironment( const qhandle_t id ) {
	// (Re-)Initializes the EAX Environment back to basics:
	CLG_EAX_SetEnvironment( SOUND_EAX_EFFECT_DEFAULT );
	// Immediately interpolate fully.
	level.eaxEffect.lerpFraction = 1.0f;
	CLG_EAX_Interpolate( SOUND_EAX_EFFECT_DEFAULT, level.eaxEffect.lerpFraction, &level.eaxEffect.mixedProperties );
	// Now apply the 'reset' environment.
	CLG_EAX_ActivateCurrentEnvironment();
}

/**
*	@brief	Will cache the current eax effect as its previous before assigning the new one, so that
*			a smooth lerp may engage.
**/
void CLG_EAX_SetEnvironment( const qhandle_t id ) {
	// OOB, skip.
	if ( id > precache.cl_num_eax_effects ) {
		return;
	}

	// Identical, skip.
	if ( level.eaxEffect.currentID == id ) {
		return;
	}

	//if ( level.eaxEffect.lerp < 1.0f ) {
		// Swap current to old.
		level.eaxEffect.previousID = level.eaxEffect.currentID;
		level.eaxEffect.currentID = id;
		level.eaxEffect.lerpFraction = 0.f;
		//level.lerp = 0.f;
	//} else {
	//}
}

/**
*	@brief	Activates the current eax environment that is set.
**/
void CLG_EAX_ActivateCurrentEnvironment() {
	// Apply the 'current' EAX reverb effect.
	clgi.S_SetEAXEnvironmentProperties( &level.eaxEffect.mixedProperties );
}

/**
*	@brief	Will scan for all 'client_env_sound' entities and test them for appliance. If none is found, the
*			effect resorts to the 'default' instead.
**/
void CLG_EAX_DetermineEffect() {
	static qhandle_t old_dsp = SOUND_EAX_EFFECT_DEFAULT;

	// Acquire player entity origin.
	const Vector3 playerOrigin = clgi.client->playerEntityOrigin;

	// Iterate over the env_sound entities, if we're within their radius, return the appropriate reverb eax id.
	float bestDistance = CM_MAX_WORLD_SIZE;
	clg_local_entity_t *best_sound_scape = nullptr;

	// Iterate over all env_sound entities that were registerd during load time.
	for ( int32_t i = 0; i < level.env_sound_list_count; i++ ) {
		// Get local entity ptr.
		clg_local_entity_t *env_sound = level.env_sound_list[ i ];
		// Get class locals ptr.
		clg_env_sound_locals_t *classLocals = CLG_LocalEntity_GetClass<clg_env_sound_locals_t>( env_sound );

		// Perform a clipping trace on the world, from the env_sound its origin onto the player's (predicted-)origin.
		const vec3_t traceStart = { env_sound->locals.origin.x, env_sound->locals.origin.y, env_sound->locals.origin.z };
		const vec3_t traceEnd = { playerOrigin.x, playerOrigin.y, playerOrigin.z };
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
		// Set the environment effect ID.
		CLG_EAX_SetEnvironment( classLocals->reverbID );
		// Turn the best entity nullptr into a valid pointer, so the code below won't revert to the default effect.
		best_sound_scape = env_sound;
	}

	// If no env_sound scape was found nearby, and we're not underwater either, resort to the default.
	if ( best_sound_scape == nullptr && !( clgi.client->predictedState.view.rdflags & RDF_UNDERWATER ) ) {
		CLG_EAX_SetEnvironment( SOUND_EAX_EFFECT_DEFAULT );
	}

	// Bail out if it is the same property.
	if ( old_dsp == level.eaxEffect.currentID ) {
		return;
	}

	// Lerp mix them otherwise:
	if ( level.eaxEffect.lerpFraction < 1.0 ) {
		// Lerp mix the old and current eax effects' properties.
		CLG_EAX_Interpolate( level.eaxEffect.currentID, level.eaxEffect.lerpFraction, &level.eaxEffect.mixedProperties );
		
		// Apply the mixed EAX properties.
		CLG_EAX_ActivateCurrentEnvironment( );

		// Increment lerp fraction.
		level.eaxEffect.lerpFraction += CL_1_FRAMETIME;
	// When done lerping, store it in the static old_dsp.
	} else {
		old_dsp = level.eaxEffect.currentID;
	}
}

/**
*	@brief	Interpolates the EAX reverb effect properties into the destinated mixedProperties.
**/
void CLG_EAX_Interpolate( /*const qhandle_t fromID, */ const qhandle_t toID, const float lerpFraction, sfx_eax_properties_t *mixedProperties ) {
	if ( /*fromID >= precache.cl_num_eax_effects ||*/ toID < 0 || toID >= precache.cl_num_eax_effects ) {
		clgi.Print( PRINT_WARNING, "%s: Invalid toID(%i)!\n", __func__, toID );
		return;
	}
	//const sfx_eax_properties_t *fromProperties = precache.cl_eax_effects[ fromID ];
	const sfx_eax_properties_t *toProperites = precache.cl_eax_effects[ toID ];

	mixedProperties->flDensity = QM_Lerp( mixedProperties->flDensity, toProperites->flDensity, lerpFraction );
	mixedProperties->flDiffusion = QM_Lerp( mixedProperties->flDiffusion, toProperites->flDiffusion, lerpFraction);

	mixedProperties->flGain = QM_Lerp( mixedProperties->flGain, toProperites->flGain, lerpFraction );
	mixedProperties->flGainHF = QM_Lerp( mixedProperties->flGainHF, toProperites->flGainHF, lerpFraction );
	mixedProperties->flGainLF = QM_Lerp( mixedProperties->flGainLF, toProperites->flGainLF, lerpFraction );
	
	mixedProperties->flDecayTime = QM_Lerp( mixedProperties->flDecayTime, toProperites->flDecayTime, lerpFraction );
	mixedProperties->flDecayHFRatio = QM_Lerp( mixedProperties->flDecayHFRatio, toProperites->flDecayHFRatio, lerpFraction );
	mixedProperties->flDecayLFRatio = QM_Lerp( mixedProperties->flDecayLFRatio, toProperites->flDecayLFRatio, lerpFraction );

	mixedProperties->flReflectionsGain = QM_Lerp( mixedProperties->flReflectionsGain, toProperites->flReflectionsGain, lerpFraction );
	mixedProperties->flReflectionsDelay = QM_Lerp( mixedProperties->flReflectionsDelay, toProperites->flReflectionsDelay, lerpFraction );
	mixedProperties->flReflectionsPan[ 0 ] = QM_Lerp( mixedProperties->flReflectionsPan[ 0 ], toProperites->flReflectionsPan[ 0 ], lerpFraction );
	mixedProperties->flReflectionsPan[ 1 ] = QM_Lerp( mixedProperties->flReflectionsPan[ 1 ], toProperites->flReflectionsPan[ 1 ], lerpFraction );
	mixedProperties->flReflectionsPan[ 2 ] = QM_Lerp( mixedProperties->flReflectionsPan[ 2 ], toProperites->flReflectionsPan[ 2 ], lerpFraction );

	mixedProperties->flLateReverbGain = QM_Lerp( mixedProperties->flLateReverbGain, toProperites->flLateReverbGain, lerpFraction );
	mixedProperties->flLateReverbDelay = QM_Lerp( mixedProperties->flLateReverbDelay, toProperites->flLateReverbDelay, lerpFraction );
	mixedProperties->flLateReverbPan[ 0 ] = QM_Lerp( mixedProperties->flLateReverbPan[ 0 ], toProperites->flLateReverbPan[ 0 ], lerpFraction );
	mixedProperties->flLateReverbPan[ 1 ] = QM_Lerp( mixedProperties->flLateReverbPan[ 1 ], toProperites->flLateReverbPan[ 1 ], lerpFraction );
	mixedProperties->flLateReverbPan[ 2 ] = QM_Lerp( mixedProperties->flLateReverbPan[ 2 ], toProperites->flLateReverbPan[ 2 ], lerpFraction );

	mixedProperties->flEchoTime = QM_Lerp( mixedProperties->flEchoTime, toProperites->flEchoTime, lerpFraction );
	mixedProperties->flEchoDepth = QM_Lerp( mixedProperties->flEchoDepth, toProperites->flEchoDepth, lerpFraction );
	
	mixedProperties->flModulationTime = QM_Lerp( mixedProperties->flModulationTime, toProperites->flModulationTime, lerpFraction );
	mixedProperties->flModulationDepth = QM_Lerp( mixedProperties->flModulationDepth, toProperites->flModulationDepth, lerpFraction );

	mixedProperties->flAirAbsorptionGainHF = QM_Lerp( mixedProperties->flAirAbsorptionGainHF, toProperites->flAirAbsorptionGainHF, lerpFraction );

	mixedProperties->flHFReference = QM_Lerp( mixedProperties->flHFReference, toProperites->flHFReference, lerpFraction );
	mixedProperties->flLFReference = QM_Lerp( mixedProperties->flLFReference, toProperites->flLFReference, lerpFraction );

	mixedProperties->flRoomRolloffFactor = QM_Lerp( mixedProperties->flRoomRolloffFactor, toProperites->flRoomRolloffFactor, lerpFraction );

	mixedProperties->iDecayHFLimit = QM_Lerp( mixedProperties->iDecayHFLimit, toProperites->iDecayHFLimit, lerpFraction );
}