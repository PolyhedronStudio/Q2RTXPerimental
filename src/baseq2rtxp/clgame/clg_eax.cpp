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

// Jasmine json parser.
#define JSMN_STATIC
#include "shared/jsmn.h"

//! Enables material debug output.
#define _DEBUG_EAX_PRINT_JSON_FAILURES 1
//#define _DEBUG_EAX_PRINT_COULDNOTREAD 1

/**
*	@brief	Will 'Hard Set' instantly, to the passed in EAX reverb properties. Used when clearing state,
*			as well as on ClientBegin calls.
**/
void CLG_EAX_HardSetEnvironment( const qhandle_t id ) {
	// (Re-)Initializes the EAX Environment back to basics:
	CLG_EAX_SetEnvironment( id );
	// Immediately interpolate fully.
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
	if ( best_sound_scape == nullptr && !( clgi.client->predictedState.currentPs.rdflags & RDF_UNDERWATER ) ) {
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

/**
*	@brief	JSON Support Function:
**/
static const int32_t jsoneq( const char *json, jsmntok_t *tok, const char *s ) {
	// Need a valid token ptr.
	if ( tok == nullptr ) {
		return -1;
	}

	if ( tok->type == JSMN_STRING && (int)strlen( s ) == tok->end - tok->start &&
		strncmp( json + tok->start, s, tok->end - tok->start ) == 0 ) {
		return 0;
	}
	return -1;
}
/**
*	@brief	JSON Support Function:
**/
const float json_token_to_float( const char *jsonBuffer, jsmntok_t *tokens, const uint32_t tokenID ) {
	// Value
	char fieldValue[ MAX_QPATH ] = { };
	// Fetch field value string size.
	const int32_t size = constclamp( tokens[ tokenID + 1 ].end - tokens[ tokenID + 1 ].start, 0, MAX_QPATH );
	// Parse field value into buffer.
	Q_snprintf( fieldValue, size, jsonBuffer + tokens[ tokenID + 1 ].start );

	// Try and convert it to a float for our material.
	return atof( fieldValue );
}
/**
*	@brief	JSON Support Function:
**/
const int32_t json_token_to_int32( const char *jsonBuffer, jsmntok_t *tokens, const uint32_t tokenID ) {
	// Value
	char fieldValue[ MAX_QPATH ] = { };
	// Fetch field value string size.
	const int32_t size = constclamp( tokens[ tokenID + 1 ].end - tokens[ tokenID + 1 ].start, 0, MAX_QPATH );
	// Parse field value into buffer.
	Q_snprintf( fieldValue, size, jsonBuffer + tokens[ tokenID + 1 ].start );

	// Try and convert it to a float for our material.
	return atoi( fieldValue );
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

	// Initialize JSON parser.
	jsmn_parser parser;
	jsmn_init( &parser );

	// Parse JSON into tokens. ( We aren't expecting more than 128 tokens, can be increased if needed though. )
	jsmntok_t tokens[ 512 ];
	const int32_t numTokensParsed = jsmn_parse( &parser, jsonBuffer, strlen( jsonBuffer ), tokens,
		sizeof( tokens ) / sizeof( tokens[ 0 ] ) );

	// If lesser than 0 we failed to parse the json properly.
	if ( numTokensParsed < 0 ) {
		clgi.Print( PRINT_DEVELOPER, "%s: Failed to parse json for file '%s', error(#%d)\n", __func__, jsonPath, numTokensParsed );
		// Clear the jsonbuffer buffer.
		clgi.Z_Free( jsonBuffer );
		return eax_reverb_properties;
	}

	// Assume the top-level element is an object.
	if ( numTokensParsed < 1 || tokens[ 0 ].type != JSMN_OBJECT ) {
		clgi.Print( PRINT_DEVELOPER, "%s: Expected a json Object at the root of file '%s'!\n", __func__, jsonPath );
		// Clear the jsonbuffer buffer.
		clgi.Z_Free( jsonBuffer );
		return eax_reverb_properties;
	}

	// Iterate over json tokens.
	for ( int32_t tokenID = 1; tokenID < numTokensParsed; tokenID++ ) {
		// density:
		if ( jsoneq( jsonBuffer, &tokens[ tokenID ], "density" ) == 0 ) {
			eax_reverb_properties.flDensity = json_token_to_float( jsonBuffer, tokens, tokenID );
		// diffusion:
		} else if ( jsoneq( jsonBuffer, &tokens[ tokenID ], "diffusion" ) == 0 ) {
			eax_reverb_properties.flDiffusion = json_token_to_float( jsonBuffer, tokens, tokenID );

		// gain:
		} else if ( jsoneq( jsonBuffer, &tokens[ tokenID ], "gain" ) == 0 ) {
			// Try and convert it to a float for our eax effect.
			eax_reverb_properties.flGain = json_token_to_float( jsonBuffer, tokens, tokenID );
		// gain_hf:
		} else if ( jsoneq( jsonBuffer, &tokens[ tokenID ], "gain_hf" ) == 0 ) {
			// Try and convert it to a float for our eax effect.
			eax_reverb_properties.flGainHF = json_token_to_float( jsonBuffer, tokens, tokenID );
		// gain_lf:
		} else if ( jsoneq( jsonBuffer, &tokens[ tokenID ], "gain_lf" ) == 0 ) {
			// Try and convert it to a float for our eax effect.
			eax_reverb_properties.flGainLF = json_token_to_float( jsonBuffer, tokens, tokenID );

		// decay_time:
		} else if ( jsoneq( jsonBuffer, &tokens[ tokenID ], "decay_time" ) == 0 ) {
			// Try and convert it to a float for our eax effect.
			eax_reverb_properties.flDecayTime= json_token_to_float( jsonBuffer, tokens, tokenID );
		// decay_hf_ratio:
		} else if ( jsoneq( jsonBuffer, &tokens[ tokenID ], "decay_hf_ratio" ) == 0 ) {
			// Try and convert it to a float for our eax effect.
			eax_reverb_properties.flDecayHFRatio = json_token_to_float( jsonBuffer, tokens, tokenID );
		// decay_lf_ratio:
		} else if ( jsoneq( jsonBuffer, &tokens[ tokenID ], "decay_lf_ratio" ) == 0 ) {
			// Try and convert it to a float for our eax effect.
			eax_reverb_properties.flDecayLFRatio = json_token_to_float( jsonBuffer, tokens, tokenID );

		// reflections_gain:
		} else if ( jsoneq( jsonBuffer, &tokens[ tokenID ], "reflections_gain" ) == 0 ) {
			// Try and convert it to a float for our eax effect.
			eax_reverb_properties.flReflectionsGain = json_token_to_float( jsonBuffer, tokens, tokenID );
		// reflections_delay:
		} else if ( jsoneq( jsonBuffer, &tokens[ tokenID ], "reflections_delay" ) == 0 ) {
			// Try and convert it to a float for our eax effect.
			eax_reverb_properties.flReflectionsDelay = json_token_to_float( jsonBuffer, tokens, tokenID );
		// reflections_pan:
		} else if ( jsoneq( jsonBuffer, &tokens[ tokenID ], "reflections_pan" ) == 0 ) {
			// Skip if not an array.
			if ( tokens[ tokenID + 1 ].type != JSMN_ARRAY ) {
				// TODO: Debug print.
				continue;
			}
			for ( int32_t j = 0; j < tokens[ tokenID + 1 ].size; j++ ) {
				// Prevent OOB.
				if ( j < 3 ) {
					jsmntok_t *g = &tokens[ tokenID + j + 2 ];
					char tokenStr[ MAX_TOKEN_CHARS ] = {};
					Q_strlcpy( tokenStr, jsonBuffer + g->start, g->end - g->start + 1 );

					eax_reverb_properties.flReflectionsPan[ j ] = atof( tokenStr );
				}
			}

			// Increase token count.
			tokenID += tokens[ tokenID + 1 ].size + 1;

		// late_reverb_gain:
		} else if ( jsoneq( jsonBuffer, &tokens[ tokenID ], "late_reverb_gain" ) == 0 ) {
			// Try and convert it to a float for our eax effect.
			eax_reverb_properties.flLateReverbGain = json_token_to_float( jsonBuffer, tokens, tokenID );
		// late_reverb_delay:
		} else if ( jsoneq( jsonBuffer, &tokens[ tokenID ], "late_reverb_delay" ) == 0 ) {
			// Try and convert it to a float for our eax effect.
			eax_reverb_properties.flLateReverbDelay = json_token_to_float( jsonBuffer, tokens, tokenID );
		// reflections_pan:
		} else if ( jsoneq( jsonBuffer, &tokens[ tokenID ], "late_reverb_pan" ) == 0 ) {
			// Skip if not an array.
			if ( tokens[ tokenID + 1 ].type != JSMN_ARRAY ) {
				// TODO: Debug print.
				continue;
			}
			for ( int32_t j = 0; j < tokens[ tokenID + 1 ].size; j++ ) {
				// Prevent OOB.
				if ( j < 3 ) {
					jsmntok_t *g = &tokens[ tokenID + j + 2 ];
					char tokenStr[ MAX_TOKEN_CHARS ] = {};
					Q_strlcpy( tokenStr, jsonBuffer + g->start, g->end - g->start + 1 );

					eax_reverb_properties.flLateReverbPan[ j ] = atof( tokenStr );
				}
			}

			// Increase token count.
			tokenID += tokens[ tokenID + 1 ].size + 1;

		// echo_time:
		} else if ( jsoneq( jsonBuffer, &tokens[ tokenID ], "echo_time" ) == 0 ) {
			// Try and convert it to a float for our eax effect.
			eax_reverb_properties.flEchoTime = json_token_to_float( jsonBuffer, tokens, tokenID );
		// echo_depth:
		} else if ( jsoneq( jsonBuffer, &tokens[ tokenID ], "echo_depth" ) == 0 ) {
			// Try and convert it to a float for our eax effect.
			eax_reverb_properties.flEchoDepth = json_token_to_float( jsonBuffer, tokens, tokenID );

		// modulation_time:
		} else if ( jsoneq( jsonBuffer, &tokens[ tokenID ], "modulation_time" ) == 0 ) {
			// Try and convert it to a float for our eax effect.
			eax_reverb_properties.flModulationTime = json_token_to_float( jsonBuffer, tokens, tokenID );
		// modulation_depth:
		} else if ( jsoneq( jsonBuffer, &tokens[ tokenID ], "modulation_depth" ) == 0 ) {
			// Try and convert it to a float for our eax effect.
			eax_reverb_properties.flModulationDepth = json_token_to_float( jsonBuffer, tokens, tokenID );

		// air_absorbtion_hf:
		} else if ( jsoneq( jsonBuffer, &tokens[ tokenID ], "air_absorbtion_hf" ) == 0 ) {
			// Try and convert it to a float for our eax effect.
			eax_reverb_properties.flAirAbsorptionGainHF = json_token_to_float( jsonBuffer, tokens, tokenID );

		// hf_reference:
		} else if ( jsoneq( jsonBuffer, &tokens[ tokenID ], "hf_reference" ) == 0 ) {
			// Try and convert it to a float for our eax effect.
			eax_reverb_properties.flModulationDepth = json_token_to_float( jsonBuffer, tokens, tokenID );
		// lf_reference:
		} else if ( jsoneq( jsonBuffer, &tokens[ tokenID ], "lf_reference" ) == 0 ) {
			// Try and convert it to a float for our eax effect.
			eax_reverb_properties.flModulationDepth = json_token_to_float( jsonBuffer, tokens, tokenID );

		// room_rolloff_factor:
		} else if ( jsoneq( jsonBuffer, &tokens[ tokenID ], "room_rolloff_factor" ) == 0 ) {
			// Try and convert it to a float for our eax effect.
			eax_reverb_properties.flRoomRolloffFactor = json_token_to_float( jsonBuffer, tokens, tokenID );

		// decay_limit:
		} else if ( jsoneq( jsonBuffer, &tokens[ tokenID ], "decay_limit" ) == 0 ) {
			// Try and convert it to a float for our eax effect.
			eax_reverb_properties.iDecayHFLimit = json_token_to_int32( jsonBuffer, tokens, tokenID );
		} else {
		//#ifdef _DEBUG_EAX_PRINT_JSON_FAILURES
		//	// Field Key
		//	char keyValue[ MAX_QPATH ] = { };
		//	// Fetch field key string size.
		//	const int32_t size = constclamp( tokens[ tokenID ].end - tokens[ tokenID ].start, 0, MAX_QPATH );
		//	// Parse field key into buffer.
		//	Q_snprintf( keyValue, size, jsonBuffer + tokens[ tokenID ].start );
		//	// TODO DEBUG ERROR.
		//	clgi.Print( PRINT_DEVELOPER, "%s: found unknown key: '%s' in file: 'eax/%s.json'\n", __func__, keyValue, filename );
		//#endif
		}
	}

	// Debug print:
	//Com_LPrintf( PRINT_DEVELOPER, "%s: Inserted new material[materialID(#%d), name(\"%s\"), kind(%s), friction(%f)]\n",
	//	__func__, material->materialID, material->name, material->physical.kind, material->physical.friction );

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