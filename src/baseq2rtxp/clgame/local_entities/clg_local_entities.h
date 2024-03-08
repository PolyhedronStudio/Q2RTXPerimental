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
*	client_misc_model:
* 
**/
//! Extern the 'client_misc_model' entity class type.
extern const clg_local_entity_class_t client_misc_model;
//! Class locals for: client_misc_model 
typedef struct clg_misc_model_locals_s {
	//! Actual model filename.
	//char modelname[ MAX_QPATH ];

	////! Model frame.
	//int32_t frame;
	////! Skin number.
	//int32_t skinNumber;
	////! Model handle.
	//qhandle_t model;
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
	temp_event_t teEvent;

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

	uint64_t frame_servertime;
} clg_misc_playerholo_locals_t;