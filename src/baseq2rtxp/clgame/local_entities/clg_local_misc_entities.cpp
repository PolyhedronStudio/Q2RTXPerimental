/********************************************************************
*
*
*	ClientGame - Local Entities: Misc
*
*
********************************************************************/
#include "../clg_local.h"
#include "clg_local_entities.h"

/**
*
*
*	client_misc_model:
*
*
**/
/**
*	@brief	Sets up the local client model entity.
**/
void CLG_misc_model_Spawn( clg_local_entity_t *self ) {

}
/**
*	@brief	Think once per local game tick.
**/
void CLG_misc_model_Think( clg_local_entity_t *self ) {

}
/**
*	@brief	Called each refresh frame.
**/
void CLG_misc_model_RefreshFrame( clg_local_entity_t *self ) {

}
/**
*	@brief	Precache function for 'client_misc_te' entity class type.
**/
void CLG_misc_model_Precache( clg_local_entity_t *self, const cm_entity_t *keyValues ) {
	// Acquire origin.
	if ( const cm_entity_t *originKv = clgi.CM_EntityKeyValue( keyValues, "origin" ) ) {
		if ( originKv->parsed_type & cm_entity_parsed_type_t::ENTITY_VECTOR3 ) {
			self->locals.origin = originKv->vec3;
		}
	}

	Vector3 origin = self->locals.origin;
	const char *classname = client_misc_model.classname;
	clgi.Print( PRINT_NOTICE, "CLGame: Spawned local entity(classname: %s) at origin(%f %f %f)\n", classname, origin[ 0 ], origin[ 1 ], origin[ 2 ] );
}

// Class definition.
const clg_local_entity_class_t client_misc_model = {
	.classname = "client_misc_model",
	.precache = CLG_misc_model_Precache,
	.spawn = CLG_misc_model_Spawn,
	.think = CLG_misc_model_Think,
	.rframe = CLG_misc_model_RefreshFrame,
	.class_locals_size = sizeof( clg_misc_model_locals_t )
};


/**
*
*
*	client_misc_te (te stands for, temp entity(event).
*
*
**/
/**
*	@brief	Precache function for 'client_misc_te' entity class type.
**/
void CLG_misc_te_Precache( clg_local_entity_t *self, const cm_entity_t *keyValues ) {
	// Acquire origin.
	if ( const cm_entity_t *originKv = clgi.CM_EntityKeyValue( keyValues, "origin" ) ) {
		if ( originKv->parsed_type & cm_entity_parsed_type_t::ENTITY_VECTOR3 ) {
			self->locals.origin = originKv->vec3;
		}
	}

	// Debug print.
	Vector3 origin = self->locals.origin;
	const char *classname = client_misc_model.classname;
	clgi.Print( PRINT_NOTICE, "CLGame: Spawned local entity(classname: %s) at origin(%f %f %f)\n", classname, origin[ 0 ], origin[ 1 ], origin[ 2 ] );
}

/**
*	@brief	Sets up the local client temp entity event entity.
**/
void CLG_misc_te_Spawn( clg_local_entity_t *self ) {

}
/**
*	@brief	Think once per local game tick.
**/
void CLG_misc_te_Think( clg_local_entity_t *self ) {

}
/**
*	@brief	Called each refresh frame.
**/
void CLG_misc_te_RefreshFrame( clg_local_entity_t *self ) {

}


// Class definition.
const clg_local_entity_class_t client_misc_te = {
	.classname = "client_misc_te",
	.precache = CLG_misc_te_Precache,
	.spawn = CLG_misc_te_Spawn,
	.think = CLG_misc_te_Think,
	.rframe = CLG_misc_te_RefreshFrame,
	.class_locals_size = sizeof( clg_misc_te_locals_t )
};