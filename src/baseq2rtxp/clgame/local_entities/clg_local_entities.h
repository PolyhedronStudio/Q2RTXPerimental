/********************************************************************
*
*
*	ClientGame: Local Class Entity Data Structures
*
*
********************************************************************/
#pragma once


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
	const char modelname[ MAX_QPATH ];

	//! Model frame.
	int32_t frame;

	//! Skin number.
	int32_t skinNumber;

	//! Model handle.
	qhandle_t model;
} clg_misc_model_locals_t;



/**
*
*	client_misc_te:
*
**/
//! Extern the 'client_misc_model' entity class type.
extern const clg_local_entity_class_t client_misc_model;
//! Class locals for: client_misc_te
typedef struct clg_misc_te_locals_s {
	//! Temp Entity Event
	temp_event_t teEvent;

	//! Temp Entity Parameters
	tent_params_t teParameters;
} clg_misc_te_locals_t;

