/********************************************************************
*
*
*	ClientGame: EAX Effects.
*
*
********************************************************************/
#pragma once

/**
*	@brief	Will 'hard set' instantly, the designated EAX reverb properties. Used when clearing state,
*			as well as on ClientBegin calls.
**/
void CLG_EAX_HardSetEnvironment( const qhandle_t id );
/**
*	@brief	Will cache the current eax effect as its previous before assigning the new one, so that
*			a smooth lerp may engage.
**/
void CLG_EAX_SetEnvironment( const qhandle_t id );
/**
*	@brief	Activates the current eax environment that is set.
**/
void CLG_EAX_ActivateCurrentEnvironment();
/**
*	@brief	Interpolates the EAX reverb effect properties into the destinated mixedProperties.
**/
void CLG_EAX_Interpolate( /*const qhandle_t fromID,*/ const qhandle_t toID, const float lerpFraction, sfx_eax_properties_t *mixedProperties );
/**
*	@brief	Will scan for all 'client_env_sound' entities and test them for appliance. If none is found, the
*			effect resorts to the 'default' instead.
**/
void CLG_EAX_DetermineEffect();

/**
*	@brief	Will load the reverb effect properties from a json file.
**/
sfx_eax_properties_t CLG_EAX_LoadReverbPropertiesFromJSON( const char *filename );