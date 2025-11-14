/********************************************************************
*
*
*	ClientGame: EAX Effects.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_eax.h"
#include "clgame/clg_eax_effects.h"
#include "clgame/clg_local_entities.h"
#include "local_entities/clg_local_entity_classes.h"
#include "local_entities/clg_local_env_sound.h"

// Include nlohman::json library for easy parsing.
#include <nlohmann/json.hpp>

//! Enables material debug output.
#define _DEBUG_EAX_PRINT_JSON_FAILURES 1
//#define _DEBUG_EAX_PRINT_COULDNOTREAD 1

/**
*	@brief	Will 'Hard Set' instantly, to the passed in EAX reverb properties. Used when clearing state,
*			as well as on ClientBegin calls.
**/
void CLG_EAX_HardSetEnvironment( const qhandle_t id ) {
	// Immediately interpolate fully.
	if ( precache.eax.num_effects <= 0 ) {
		return;
	}
	// (Re-)Initializes the EAX Environment back to basics:
	CLG_EAX_SetEnvironment( id );
	level.eaxEffect.lerpFraction = 1.0f;
	CLG_EAX_Interpolate( id, level.eaxEffect.lerpFraction, &level.eaxEffect.mixedProperties );
	// Now apply the 'reset' environment.
	CLG_EAX_ActivateCurrentEnvironment();
}

/**
*	@brief	Will cache the current eax effect as its previous before assigning the new one, so that
*			a smooth lerp may engage.
**/
void CLG_EAX_SetEnvironment( const qhandle_t id ) {
	// OOB, skip.
	if ( id > precache.eax.num_effects ) {
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
		const cm_trace_t trace = clgi.Clip( traceStart, vec3_origin, vec3_origin, traceEnd, nullptr, CM_CONTENTMASK_SOLID );

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
	if ( best_sound_scape == nullptr && !( ( clgi.client->frame.ps.rdflags | game.predictedState.currentPs.rdflags ) & RDF_UNDERWATER ) ) {
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
	if ( /*fromID >= precache.eax.num_effects ||*/ toID < 0 || toID >= precache.eax.num_effects ) {
		clgi.Print( PRINT_WARNING, "%s: Invalid toID(%i)!\n", __func__, toID );
		return;
	}
	//const sfx_eax_properties_t *fromProperties = precache.eax.properties[ fromID ];
	const sfx_eax_properties_t *toProperites = &precache.eax.properties[ toID ];

	mixedProperties->flDensity = QM_Lerp<float>( mixedProperties->flDensity, toProperites->flDensity, lerpFraction );
	mixedProperties->flDiffusion = QM_Lerp<float>( mixedProperties->flDiffusion, toProperites->flDiffusion, lerpFraction);

	mixedProperties->flGain = QM_Lerp<float>( mixedProperties->flGain, toProperites->flGain, lerpFraction );
	mixedProperties->flGainHF = QM_Lerp<float>( mixedProperties->flGainHF, toProperites->flGainHF, lerpFraction );
	mixedProperties->flGainLF = QM_Lerp<float>( mixedProperties->flGainLF, toProperites->flGainLF, lerpFraction );
	
	mixedProperties->flDecayTime = QM_Lerp<float>( mixedProperties->flDecayTime, toProperites->flDecayTime, lerpFraction );
	mixedProperties->flDecayHFRatio = QM_Lerp<float>( mixedProperties->flDecayHFRatio, toProperites->flDecayHFRatio, lerpFraction );
	mixedProperties->flDecayLFRatio = QM_Lerp<float>( mixedProperties->flDecayLFRatio, toProperites->flDecayLFRatio, lerpFraction );

	mixedProperties->flReflectionsGain = QM_Lerp<float>( mixedProperties->flReflectionsGain, toProperites->flReflectionsGain, lerpFraction );
	mixedProperties->flReflectionsDelay = QM_Lerp<float>( mixedProperties->flReflectionsDelay, toProperites->flReflectionsDelay, lerpFraction );
	mixedProperties->flReflectionsPan[ 0 ] = QM_Lerp<float>( mixedProperties->flReflectionsPan[ 0 ], toProperites->flReflectionsPan[ 0 ], lerpFraction );
	mixedProperties->flReflectionsPan[ 1 ] = QM_Lerp<float>( mixedProperties->flReflectionsPan[ 1 ], toProperites->flReflectionsPan[ 1 ], lerpFraction );
	mixedProperties->flReflectionsPan[ 2 ] = QM_Lerp<float>( mixedProperties->flReflectionsPan[ 2 ], toProperites->flReflectionsPan[ 2 ], lerpFraction );

	mixedProperties->flLateReverbGain = QM_Lerp<float>( mixedProperties->flLateReverbGain, toProperites->flLateReverbGain, lerpFraction );
	mixedProperties->flLateReverbDelay = QM_Lerp<float>( mixedProperties->flLateReverbDelay, toProperites->flLateReverbDelay, lerpFraction );
	mixedProperties->flLateReverbPan[ 0 ] = QM_Lerp<float>( mixedProperties->flLateReverbPan[ 0 ], toProperites->flLateReverbPan[ 0 ], lerpFraction );
	mixedProperties->flLateReverbPan[ 1 ] = QM_Lerp<float>( mixedProperties->flLateReverbPan[ 1 ], toProperites->flLateReverbPan[ 1 ], lerpFraction );
	mixedProperties->flLateReverbPan[ 2 ] = QM_Lerp<float>( mixedProperties->flLateReverbPan[ 2 ], toProperites->flLateReverbPan[ 2 ], lerpFraction );

	mixedProperties->flEchoTime = QM_Lerp<float>( mixedProperties->flEchoTime, toProperites->flEchoTime, lerpFraction );
	mixedProperties->flEchoDepth = QM_Lerp<float>( mixedProperties->flEchoDepth, toProperites->flEchoDepth, lerpFraction );
	
	mixedProperties->flModulationTime = QM_Lerp<float>( mixedProperties->flModulationTime, toProperites->flModulationTime, lerpFraction );
	mixedProperties->flModulationDepth = QM_Lerp<float>( mixedProperties->flModulationDepth, toProperites->flModulationDepth, lerpFraction );

	mixedProperties->flAirAbsorptionGainHF = QM_Lerp<float>( mixedProperties->flAirAbsorptionGainHF, toProperites->flAirAbsorptionGainHF, lerpFraction );

	mixedProperties->flHFReference = QM_Lerp<float>( mixedProperties->flHFReference, toProperites->flHFReference, lerpFraction );
	mixedProperties->flLFReference = QM_Lerp<float>( mixedProperties->flLFReference, toProperites->flLFReference, lerpFraction );

	mixedProperties->flRoomRolloffFactor = QM_Lerp<float>( mixedProperties->flRoomRolloffFactor, toProperites->flRoomRolloffFactor, lerpFraction );

	mixedProperties->iDecayHFLimit = QM_Lerp<float>( mixedProperties->iDecayHFLimit, toProperites->iDecayHFLimit, lerpFraction );
}

/**
*	@brief	Will load the reverb effect properties from a json file.
**/
sfx_eax_properties_t CLG_EAX_LoadReverbPropertiesFromJSON( const char *filename ) {
	// The sfx eax properties to load, assigned default values from the hardcoded defualt properties.
	sfx_eax_properties_t eax_reverb_properties = cl_eax_default_properties;

	// Path to the json file.
	char jsonPath[ MAX_OSPATH ] = { };
	// Generate the game folder relative path to the efx properties '.json' file.
	Q_snprintf( jsonPath, MAX_OSPATH, "eax/%s.json", filename );

	// Attempt to load the efx properties from the '.json' that matches the fileame.
	// Buffer meant to be filled with the file's json data.
	char *jsonBuffer = NULL;

	// Ensure the file is existent.
	if ( !clgi.FS_FileExistsEx( jsonPath, 0/*FS_TYPE_ANY*/ ) ) {
		#ifdef _DEBUG_EAX_PRINT_COULDNOTREAD
		clgi.Print( PRINT_DEVELOPER, "%s: Couldn't read %s because it does not exist\n", __func__, jsonPath );
		#endif
		return eax_reverb_properties;
	}
	// Load the json filename path.
	clgi.FS_LoadFile( jsonPath, (void **)&jsonBuffer );

	// Error out if we can't find it.
	if ( !jsonBuffer ) {
		#ifdef _DEBUG_EAX_PRINT_COULDNOTREAD
		clgi.Print( PRINT_DEVELOPER, "%s: Couldn't read %s\n", __func__, jsonPath );
		#endif
		return eax_reverb_properties;
	}

	// Parse JSON using nlohmann::json.
	nlohmann::json json;
	try {
		json = nlohmann::json::parse( jsonBuffer );
	}
	// Catch parsing errors if any.
	catch ( const nlohmann::json::parse_error &e ) {
		// Output parsing error.
		clgi.Print( PRINT_DEVELOPER, "%s: Failed to parse json for file '%s', error(%s)\n", __func__, jsonPath, e.what() );
		// Clear the jsonbuffer buffer.
		clgi.Z_Free( jsonBuffer );
		// Return as we failed to parse the json.
		return eax_reverb_properties;
	}

	// Try and read the eax effect properties.
	try {
		eax_reverb_properties.flDensity = json.at( "density" ).get< float >();
		eax_reverb_properties.flDiffusion = json.at( "diffusion" ).get< float >();
		eax_reverb_properties.flGain = json.at( "gain" ).get< float >();
		eax_reverb_properties.flGainHF = json.at( "gain_hf" ).get< float >();
		eax_reverb_properties.flGainLF = json.at( "gain_lf" ).get< float >();
		eax_reverb_properties.flDecayTime = json.at( "decay_time" ).get< float >();
		eax_reverb_properties.flDecayHFRatio = json.at( "decay_hf_ratio" ).get< float >();
		eax_reverb_properties.flDecayLFRatio = json.at( "decay_lf_ratio" ).get< float >();
		eax_reverb_properties.flReflectionsGain = json.at( "reflections_gain" ).get< float >();
		eax_reverb_properties.flReflectionsDelay = json.at( "reflections_delay" ).get< float >();
		if ( json.at( "reflections_pan" ).size() >= 3 ) {
			eax_reverb_properties.flReflectionsPan[ 0 ] = json.at( "reflections_pan" )[ 0 ].get< float >();
			eax_reverb_properties.flReflectionsPan[ 1 ] = json.at( "reflections_pan" )[ 1 ].get< float >();
			eax_reverb_properties.flReflectionsPan[ 2 ] = json.at( "reflections_pan" )[ 2 ].get< float >();
		}
		eax_reverb_properties.flLateReverbGain = json.at( "late_reverb_gain" ).get< float >();
		eax_reverb_properties.flLateReverbDelay = json.at( "late_reverb_delay" ).get< float >();
		if ( json.at( "late_reverb_pan" ).size() >= 3 ) {
			eax_reverb_properties.flLateReverbPan[ 0 ] = json.at( "late_reverb_pan" )[ 0 ].get< float >();
			eax_reverb_properties.flLateReverbPan[ 1 ] = json.at( "late_reverb_pan" )[ 1 ].get< float >();
			eax_reverb_properties.flLateReverbPan[ 2 ] = json.at( "late_reverb_pan" )[ 2 ].get< float >();
		}
		eax_reverb_properties.flEchoTime = json.at( "echo_time" ).get< float >();
		eax_reverb_properties.flEchoDepth = json.at( "echo_depth" ).get< float >();
		eax_reverb_properties.flModulationTime = json.at( "modulation_time" ).get< float >();
		eax_reverb_properties.flModulationDepth = json.at( "modulation_depth" ).get< float >();
		eax_reverb_properties.flAirAbsorptionGainHF = json.at( "air_absorbtion_gain_hf" ).get< float >();
		eax_reverb_properties.flHFReference = json.at( "hf_reference" ).get< float >();
		eax_reverb_properties.flLFReference = json.at( "lf_reference" ).get< float >();
		eax_reverb_properties.flRoomRolloffFactor = json.at( "room_rolloff_factor" ).get< float >();
		eax_reverb_properties.iDecayHFLimit = json.at( "decay_hf_limit" ).get< int32_t >();
	}
	// Catch any json parsing errors.
	catch ( const nlohmann::json::exception &e ) {
		#ifdef _DEBUG_EAX_PRINT_JSON_FAILURES
		// Output parsing error.
		clgi.Print( PRINT_DEVELOPER, "%s: Failed to parse json for file '%s', error(%s)\n", __func__, jsonPath, e.what() );
		#endif
		// Clear the jsonbuffer buffer.
		clgi.Z_Free( jsonBuffer );
		// Return 0 as we failed to parse the json.
		return eax_reverb_properties;
	}

	// Clear the jsonbuffer buffer.
	clgi.Z_Free( jsonBuffer );

	return eax_reverb_properties;
}

/**
*	@brief	Called by Precache code to precache all EAX Reverb Effects.
**/
void CLG_EAX_Precache() {
	// Required by default and should NOT be REMOVED!!
	precache.eax.properties[ SOUND_EAX_EFFECT_DEFAULT ] = cl_eax_default_properties;
	precache.eax.properties[ SOUND_EAX_EFFECT_UNDERWATER ] = cl_eax_underwater_properties;

	// Any of the EAX effects below can be replaced by whatever suits your needs.
	precache.eax.properties[ SOUND_EAX_EFFECT_ABANDONED ] = CLG_EAX_LoadReverbPropertiesFromJSON( "abandoned" );
	precache.eax.properties[ SOUND_EAX_EFFECT_ALLEY ] = CLG_EAX_LoadReverbPropertiesFromJSON( "alley" );
	precache.eax.properties[ SOUND_EAX_EFFECT_ARENA ] = CLG_EAX_LoadReverbPropertiesFromJSON( "arena" );
	precache.eax.properties[ SOUND_EAX_EFFECT_AUDITORIUM ] = CLG_EAX_LoadReverbPropertiesFromJSON( "auditorium" );
	precache.eax.properties[ SOUND_EAX_EFFECT_BATHROOM ] = CLG_EAX_LoadReverbPropertiesFromJSON( "bathroom" );
	precache.eax.properties[ SOUND_EAX_EFFECT_CARPETED_HALLWAY ] = CLG_EAX_LoadReverbPropertiesFromJSON( "carpetedhallway" );
	precache.eax.properties[ SOUND_EAX_EFFECT_CAVE ] = CLG_EAX_LoadReverbPropertiesFromJSON( "cave" );
	precache.eax.properties[ SOUND_EAX_EFFECT_CHAPEL ] = CLG_EAX_LoadReverbPropertiesFromJSON( "chapel" );
	precache.eax.properties[ SOUND_EAX_EFFECT_CITY ] = CLG_EAX_LoadReverbPropertiesFromJSON( "city" );
	precache.eax.properties[ SOUND_EAX_EFFECT_CITY_STREETS ] = CLG_EAX_LoadReverbPropertiesFromJSON( "citystreets" );
	precache.eax.properties[ SOUND_EAX_EFFECT_CONCERT_HALL ] = CLG_EAX_LoadReverbPropertiesFromJSON( "concerthall" );
	precache.eax.properties[ SOUND_EAX_EFFECT_DIZZY ] = CLG_EAX_LoadReverbPropertiesFromJSON( "dizzy" );
	precache.eax.properties[ SOUND_EAX_EFFECT_DRUGGED ] = CLG_EAX_LoadReverbPropertiesFromJSON( "drugged" );
	precache.eax.properties[ SOUND_EAX_EFFECT_DUSTYROOM ] = CLG_EAX_LoadReverbPropertiesFromJSON( "dustyroom" );
	precache.eax.properties[ SOUND_EAX_EFFECT_FOREST ] = CLG_EAX_LoadReverbPropertiesFromJSON( "forest" );
	precache.eax.properties[ SOUND_EAX_EFFECT_HALLWAY ] = CLG_EAX_LoadReverbPropertiesFromJSON( "hallway" );
	precache.eax.properties[ SOUND_EAX_EFFECT_HANGAR ] = CLG_EAX_LoadReverbPropertiesFromJSON( "hangar" );
	precache.eax.properties[ SOUND_EAX_EFFECT_LIBRARY ] = CLG_EAX_LoadReverbPropertiesFromJSON( "library" );
	precache.eax.properties[ SOUND_EAX_EFFECT_LIVINGROOM ] = CLG_EAX_LoadReverbPropertiesFromJSON( "livingroom" );
	precache.eax.properties[ SOUND_EAX_EFFECT_MOUNTAINS ] = CLG_EAX_LoadReverbPropertiesFromJSON( "mountains" );
	precache.eax.properties[ SOUND_EAX_EFFECT_MUSEUM ] = CLG_EAX_LoadReverbPropertiesFromJSON( "museum" );
	precache.eax.properties[ SOUND_EAX_EFFECT_PADDED_CELL ] = CLG_EAX_LoadReverbPropertiesFromJSON( "paddedcell" );
	precache.eax.properties[ SOUND_EAX_EFFECT_PARKINGLOT ] = CLG_EAX_LoadReverbPropertiesFromJSON( "parkinglot" );
	precache.eax.properties[ SOUND_EAX_EFFECT_PLAIN ] = CLG_EAX_LoadReverbPropertiesFromJSON( "plain" );
	precache.eax.properties[ SOUND_EAX_EFFECT_PSYCHOTIC ] = CLG_EAX_LoadReverbPropertiesFromJSON( "psychotic" );
	precache.eax.properties[ SOUND_EAX_EFFECT_QUARRY ] = CLG_EAX_LoadReverbPropertiesFromJSON( "quarry" );
	precache.eax.properties[ SOUND_EAX_EFFECT_ROOM ] = CLG_EAX_LoadReverbPropertiesFromJSON( "room" );
	precache.eax.properties[ SOUND_EAX_EFFECT_SEWERPIPE ] = CLG_EAX_LoadReverbPropertiesFromJSON( "sewerpipe" );
	precache.eax.properties[ SOUND_EAX_EFFECT_SMALL_WATERROOM ] = CLG_EAX_LoadReverbPropertiesFromJSON( "smallwaterroom" );
	precache.eax.properties[ SOUND_EAX_EFFECT_STONE_CORRIDOR ] = CLG_EAX_LoadReverbPropertiesFromJSON( "stonecorridor" );
	precache.eax.properties[ SOUND_EAX_EFFECT_STONE_ROOM ] = CLG_EAX_LoadReverbPropertiesFromJSON( "stoneroom" );
	precache.eax.properties[ SOUND_EAX_EFFECT_SUBWAY ] = CLG_EAX_LoadReverbPropertiesFromJSON( "subway" );
	precache.eax.properties[ SOUND_EAX_EFFECT_UNDERPASS ] = CLG_EAX_LoadReverbPropertiesFromJSON( "underpass" );

	// We loaded SOUND_EAX_EFFECT_MAX eax effects, make sure the cache is aware of this.
	precache.eax.num_effects = SOUND_EAX_EFFECT_MAX;
}