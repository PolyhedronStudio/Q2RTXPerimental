/********************************************************************
*
*
*	ClientGame: The local environment sound entity allows to set a
*	reverb effect which is applied when within its proximity radius.
*
*
********************************************************************/
#pragma once



/**
*
*	client_env_sound:
*
**/
//! Extern the 'client_env_sound' entity class type.
extern const clg_local_entity_class_t client_env_sound;
//! Class locals for: client_env_sound
typedef struct clg_env_sound_local_s {
	//! Client number to trace to. (Usually -1, meaning ourselves, but when stat_chasing perhaps an other client entity.)
	int32_t clientNumber;

	//! The radius of the environmental eax reverb effecet.
	float radius;
	//! The reverb type ID, set by the style value at spawn time.
	qhandle_t reverbID;
} clg_env_sound_locals_t;