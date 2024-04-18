/********************************************************************
*
*
*	ClientGame: EAX Effects.
*
*
********************************************************************/
#include "clg_local.h"
#include "clg_eax.h"

#include "clg_local_entities.h"
#include "local_entities/clg_local_entity_classes.h"
#include "local_entities/clg_local_env_sound.h"

/**
*	@brief	
**/
void CLG_EAX_SetEnvironment( const qhandle_t id ) {
	// OOB, skip.
	if ( id > precache.cl_num_eax_reverb_effects ) {
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
		//level.lerp = 0.f;
	//} else {
	//}
}

/**
*	@brief	Activates the current eax environment that is set.
**/
void CLG_EAX_ActivateCurrentEnvironment() {
	// Apply the 'current' EAX reverb effect.
	clgi.S_SetActiveReverbEffect( precache.cl_eax_reverb_effects[ level.eaxEffect.currentID ] );
}

/**
*	@brief
**/
void CLG_EAX_DetermineEffect() {
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

	if ( best_sound_scape == nullptr ) {
		CLG_EAX_SetEnvironment( SOUND_EAX_EFFECT_DEFAULT );
	}
}

/**
*	@brief	Interpolates the EAX reverb effect properties into the destinated mixedProperties.
**/
//void CLG_EAX_Interpolate( const qhandle_t fromID, const qhandle_t toID, const float lerpFraction, sfx_reverb_properties_t *mixedProperties ) {
//	sfx_reverb_properties_t *fromProperties = clgi.S_GetReverbEffectProperties( fromID );
//	sfx_reverb_properties_t *toProperites = clgi.S_GetReverbEffectProperties( toID );
//
//	mixedProperties->flDensity = QM_Lerp( fromProperties->flDensity, toProperites->flDensity, lerpFraction );
//	mixedProperties->flDiffusion = QM_Lerp( fromProperties->flDiffusion, toProperites->flDiffusion, lerpFraction);
//
//	mixedProperties->flGain = QM_Lerp( fromProperties->flGain, toProperites->flGain, lerpFraction );
//	mixedProperties->flGainHF = QM_Lerp( fromProperties->flGainHF, toProperites->flGainHF, lerpFraction );
//	mixedProperties->flGainLF = QM_Lerp( fromProperties->flGainLF, toProperites->flGainLF, lerpFraction );
//
//	mixedProperties->flDecayTime = QM_Lerp( fromProperties->flDecayTime, toProperites->flDecayTime, lerpFraction );
//	mixedProperties->flDecayHFRatio = QM_Lerp( fromProperties->flDecayHFRatio, toProperites->flDecayHFRatio, lerpFraction );
//	mixedProperties->flDecayLFRatio = QM_Lerp( fromProperties->flDecayLFRatio, toProperites->flDecayLFRatio, lerpFraction );
//
//	mixedProperties->flReflectionsGain = QM_Lerp( fromProperties->flReflectionsGain, toProperites->flReflectionsGain, lerpFraction );
//	mixedProperties->flReflectionsDelay = QM_Lerp( fromProperties->flReflectionsDelay, toProperites->flReflectionsDelay, lerpFraction );
//	mixedProperties->flReflectionsPan[ 0 ] = QM_Lerp(fromProperties->flReflectionsPan[ 0 ], toProperites->flReflectionsPan[ 0 ], lerpFraction );
//	mixedProperties->flReflectionsPan[ 1 ] = QM_Lerp( fromProperties->flReflectionsPan[ 1 ], toProperites->flReflectionsPan[ 1 ], lerpFraction );
//	mixedProperties->flReflectionsPan[ 2 ] = QM_Lerp( fromProperties->flReflectionsPan[ 2 ], toProperites->flReflectionsPan[ 2 ], lerpFraction );
//
//	mixedProperties->flLateReverbGain = QM_Lerp( fromProperties->flLateReverbGain, toProperites->flLateReverbGain, lerpFraction );
//	mixedProperties->flLateReverbDelay = QM_Lerp( fromProperties->flLateReverbDelay, toProperites->flLateReverbDelay, lerpFraction );
//	mixedProperties->flLateReverbPan[ 0 ] = QM_Lerp( fromProperties->flLateReverbPan[ 0 ], toProperites->flLateReverbPan[ 0 ], lerpFraction );
//	mixedProperties->flLateReverbPan[ 1 ] = QM_Lerp( fromProperties->flLateReverbPan[ 1 ], toProperites->flLateReverbPan[ 1 ], lerpFraction );
//	mixedProperties->flLateReverbPan[ 2 ] = QM_Lerp( fromProperties->flLateReverbPan[ 2 ], toProperites->flLateReverbPan[ 2 ], lerpFraction );
//
//	mixedProperties->flEchoTime = QM_Lerp( fromProperties->flEchoTime, toProperites->flEchoTime, lerpFraction );
//	mixedProperties->flEchoDepth = QM_Lerp( fromProperties->flEchoDepth, toProperites->flEchoDepth, lerpFraction );
//	
//	mixedProperties->flModulationTime = QM_Lerp( fromProperties->flModulationTime, toProperites->flModulationTime, lerpFraction );
//	mixedProperties->flModulationDepth = QM_Lerp( fromProperties->flModulationDepth, toProperites->flModulationDepth, lerpFraction );
//
//	mixedProperties->flAirAbsorptionGainHF = QM_Lerp( fromProperties->flAirAbsorptionGainHF, toProperites->flAirAbsorptionGainHF, lerpFraction );
//
//	mixedProperties->flHFReference = QM_Lerp( fromProperties->flHFReference, toProperites->flHFReference, lerpFraction );
//	mixedProperties->flLFReference = QM_Lerp( fromProperties->flLFReference, toProperites->flLFReference, lerpFraction );
//
//	mixedProperties->flRoomRolloffFactor = QM_Lerp( fromProperties->flRoomRolloffFactor, toProperites->flRoomRolloffFactor, lerpFraction );
//
//	mixedProperties->iDecayHFLimit = QM_Lerp( fromProperties->iDecayHFLimit, toProperites->iDecayHFLimit, level.eaxLerp );
//}