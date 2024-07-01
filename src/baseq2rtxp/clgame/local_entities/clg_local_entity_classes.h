/********************************************************************
*
*
*	ClientGame: Local Class Entity Data Structures
*
*
********************************************************************/
#pragma once

//! All local entity classname type descriptors.
extern const clg_local_entity_class_t *local_entity_classes[];

//! Utility function to neaten code up.



/**
*
*	client_env_sound:
*
**/
#include "clg_local_env_sound.h"



/**
*
*	client_misc_model:
* 
**/
//! Extern the 'client_misc_model' entity class type.
extern const clg_local_entity_class_t client_misc_model;
//! Class locals for: client_misc_model 
typedef struct clg_misc_model_locals_s {
	//! Refresh Entity.
	entity_t rent;

	//! Animation Data.
	//! 
	//! If the animationID of head, or torso, is zero, it will resort to
	//! using the base animation which is set by the hip.
	struct {
		//! Animation to display for the head bone.
		uint32_t headID;
		//! Animation to display for the torso bone.
		uint32_t torsoID;
		//! Animation to display for the hip bone.
		uint32_t hipID;

		//! Start time of the headID animation.
		sg_time_t headStartTime;
		//! Start time of the torsoID animation.
		sg_time_t torsoStartTime;
		//! Start time of the headID animation.
		sg_time_t hipStartTime;

		//! Frame of the head animation.
		int32_t currentHeadFrame;
		int32_t lastHeadFrame;
		//! Frame of the torso animation.
		int32_t currentTorsoFrame;
		int32_t lastTorsoFrame;
		//! Frame of the hip animation.
		int32_t currentHipFrame;
		int32_t lastHipFrame;

		// Backlerps.
		double headBackLerp;
		double torsoBackLerp;
		double hipBackLerp;
	} animation;

	//! Pointer to the model data for display.
	const model_t *model;
} clg_misc_model_locals_t;



/**
*
*	client_misc_te:
*
**/
//! Extern the 'client_misc_model' entity class type.
extern const clg_local_entity_class_t client_misc_te;
//! Class locals for: client_misc_te
typedef struct clg_misc_te_locals_s {
	//! Temp Entity Event
	temp_entity_event_t teEvent;

	//! Temp Entity Parameters
	tent_params_t teParameters;
} clg_misc_te_locals_t;



/**
*
*	client_misc_playerholo:
*
**/
//! Extern the 'client_misc_playerholo' entity class type.
extern const clg_local_entity_class_t client_misc_playerholo;
//! Class locals for: client_misc_model 
typedef struct clg_misc_playerholo_locals_s {
	//! Client to mirror. (Disabled, for now, we take the player himself.)
	int32_t clientNumber;

	//! Time at which the frame is at on the server.
	uint64_t frame_servertime;
} clg_misc_playerholo_locals_t;