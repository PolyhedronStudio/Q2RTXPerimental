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
*	@brief	Precache function for 'client_misc_te' entity class type.
**/
void CLG_misc_model_Precache( clg_local_entity_t *self, const cm_entity_t *keyValues ) {
	// Get class locals.
	clg_misc_model_locals_t *classLocals = static_cast<clg_misc_model_locals_t *>( self->classLocals );

	// Key/Value: 'model':
	if ( const cm_entity_t *modelKv = clgi.CM_EntityKeyValue( keyValues, "model" ) ) {
		if ( modelKv->parsed_type & cm_entity_parsed_type_t::ENTITY_STRING ) {
			Q_strlcpy( classLocals->modelname, modelKv->string, MAX_QPATH);
		} else {
			memset( classLocals->modelname, 0, MAX_QPATH );
		}
	}

	// Key/Value: 'frame':
	if ( const cm_entity_t *frameKv = clgi.CM_EntityKeyValue( keyValues, "frame" ) ) {
		if ( frameKv->parsed_type & cm_entity_parsed_type_t::ENTITY_INTEGER ) {
			self->locals.frame = frameKv->integer;
		} else {
			self->locals.frame = 0;
		}
	}

	// Key/Value: 'skin':
	if ( const cm_entity_t *skinKv = clgi.CM_EntityKeyValue( keyValues, "skin" ) ) {
		if ( skinKv->parsed_type & cm_entity_parsed_type_t::ENTITY_INTEGER ) {
			self->locals.skinNumber = skinKv->integer;
		} else {
			self->locals.skinNumber = 0;
		}
	}

	// Set up the modelname for precaching the model with.
	if ( classLocals->modelname[ 0 ] != '\0' ) {
		self->locals.modelindex = CLG_RegisterLocalModel( classLocals->modelname );
	}

	// DEBNUG PRINT:
	clgi.Print( PRINT_DEVELOPER, "CLG_misc_model_Precache: model(%s), local_draw_model_handle(%d) frame(%d), skin(%d)\n", classLocals->modelname, self->locals.modelindex, self->locals.frame, self->locals.skinNumber );
}

/**
*	@brief	Sets up the local client model entity.
**/
void CLG_misc_model_Spawn( clg_local_entity_t *self ) {
	// Setup appropriate mins, maxs, absmins, absmaxs, size
	VectorCopy( self->locals.origin, self->locals.mins );
	VectorCopy( self->locals.origin, self->locals.maxs );
	VectorCopy( self->locals.origin, self->locals.absmax );
	VectorCopy( self->locals.origin, self->locals.absmin );
	VectorCopy( self->locals.origin, self->locals.size );
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
	// Get class locals.
	clg_misc_te_locals_t *classLocals = static_cast<clg_misc_te_locals_t *>( self->classLocals );

	// Key/Value: 'event':
	if ( const cm_entity_t *eventTypeKv = clgi.CM_EntityKeyValue( keyValues, "event" ) ) {
		if ( eventTypeKv->parsed_type & cm_entity_parsed_type_t::ENTITY_INTEGER ) {
			classLocals->teEvent = static_cast<temp_event_t>( eventTypeKv->integer );
		} else {
			classLocals->teEvent = static_cast<temp_event_t>( 0 );
		}
	}

	// TODO: Other key values for Temp Event Entity.
	self->locals.modelindex = -1;

	// DEBNUG PRINT:
	clgi.Print( PRINT_DEVELOPER, "CLG_misc_te_Precache: event(%d)\n", classLocals->teEvent );
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