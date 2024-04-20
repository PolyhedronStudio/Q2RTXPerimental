/********************************************************************
*
*
*	ClientGame: EAX Effects.
*
*
********************************************************************/
#pragma once

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
void CLG_EAX_Interpolate( const qhandle_t fromID, const qhandle_t toID, const float lerpFraction, sfx_eax_properties_t *mixedProperties );
/**
*	@brief
**/
void CLG_EAX_DetermineEffect();
/**
*	@brief	Interpolates the EAX reverb effect properties into the destinated mixedProperties.
**/
//void CLG_EAX_Interpolate( const qhandle_t fromID, const qhandle_t toID, const float lerpFraction, sfx_eax_properties_t *mixedProperties );