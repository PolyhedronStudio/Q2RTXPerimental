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
*	@brief	Precache function for 'client_misc_model' entity class type.
**/
void CLG_misc_model_Precache( clg_local_entity_t *self, const cm_entity_t *keyValues ) {
	// Get class locals.
	clg_misc_model_locals_t *classLocals = static_cast<clg_misc_model_locals_t *>( self->classLocals );

	// Key/Value: 'model':
	if ( const cm_entity_t *modelKv = clgi.CM_EntityKeyValue( keyValues, "model" ) ) {
		if ( modelKv->parsed_type & cm_entity_parsed_type_t::ENTITY_STRING ) {
			self->model = modelKv->string;
		} else {
			self->model = nullptr;// memset( classLocals->modelname, 0, MAX_QPATH );
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
	if ( self->model && self->model[ 0 ] != '\0' ) {
		self->locals.modelindex = CLG_RegisterLocalModel( self->model );
	} else {
		clgi.Print( PRINT_DEVELOPER, "%s: empty 'model' field. Unable to register local entity model.\n", __func__ );
	}

	// DEBNUG PRINT:
	clgi.Print( PRINT_DEVELOPER, "CLG_misc_model_Precache: model(%s), local_draw_model_handle(%d) frame(%d), skin(%d)\n", self->model, self->locals.modelindex, self->locals.frame, self->locals.skinNumber );
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



/**
*
*
*	client_misc_playermirror:
*
*
**/
/**
*	@brief	Precache function for 'client_misc_te' entity class type.
**/
void CLG_misc_playerholo_Precache( clg_local_entity_t *self, const cm_entity_t *keyValues ) {
	auto *selfClass = CLG_LocalEntity_GetClass<clg_misc_playerholo_locals_t>( self );

	// Key/Value: 'model':
	//if ( const cm_entity_t *modelKv = clgi.CM_EntityKeyValue( keyValues, "model" ) ) {
	//	if ( modelKv->parsed_type & cm_entity_parsed_type_t::ENTITY_STRING ) {
	//		self->model = modelKv->string;
	//	} else {
	//		self->model = nullptr;// memset( classLocals->modelname, 0, MAX_QPATH );
	//	}
	//}
	self->model = "models/playerholo/tris.md2";

	//// Key/Value: 'frame':
	if ( const cm_entity_t *clientnumKv = clgi.CM_EntityKeyValue( keyValues, "clientnum" ) ) {
		if ( clientnumKv->parsed_type & cm_entity_parsed_type_t::ENTITY_INTEGER ) {
			selfClass->clientNumber = clientnumKv->integer;
		} else {
			selfClass->clientNumber = 0;
		}
	}

	//// Key/Value: 'skin':
	//if ( const cm_entity_t *skinKv = clgi.CM_EntityKeyValue( keyValues, "skin" ) ) {
	//	if ( skinKv->parsed_type & cm_entity_parsed_type_t::ENTITY_INTEGER ) {
	//		self->locals.skinNumber = skinKv->integer;
	//	} else {
	//		self->locals.skinNumber = 0;
	//	}
	//}

	// Set up the modelname for precaching the model with.
	self->locals.modelindex = MODELINDEX_PLAYER;// CLG_RegisterLocalModel( self->model );
	self->locals.skin = clgi.R_RegisterSkin( "models/playerholo/skin.pcx" );
	self->locals.skinNumber = 0;

	//if ( self->model && self->model[ 0 ] != '\0' ) {
	//} else {
	//	clgi.Print( PRINT_DEVELOPER, "%s: empty 'model' field. Unable to register local entity model.\n", __func__ );
	//}

	// DEBNUG PRINT:
	clgi.Print( PRINT_DEVELOPER, "CLG_misc_playerholo_Precache: model(%s), local_draw_model_handle(%d) frame(%d), skin(%d)\n", self->model, self->locals.modelindex, self->locals.frame, self->locals.skinNumber );
}

/**
*	@brief	Sets up the local client model entity.
**/
void CLG_misc_playerholo_Spawn( clg_local_entity_t *self ) {
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
void CLG_misc_playerholo_Think( clg_local_entity_t *self ) {
	// Get class.
	auto *selfClass = CLG_LocalEntity_GetClass<clg_misc_playerholo_locals_t>( self );

	// Update animation based on the client entity's last and current frame.
	centity_t *cent = clgi.client->clientEntity;
	if ( cent && cent->last_frame != cent->current_frame ) {
		// Update regular frame locals to the newly found client entity last and current frame.
		self->locals.frame = cent->current_frame;
		self->locals.oldframe = cent->last_frame;

		// Store entity specific needed frame_servertime as well.
		selfClass->frame_servertime = cent->frame_servertime;
	}

	// Negate angles so we face the client entity always.
	self->locals.angles = QM_Vector3Negate( clgi.client->playerEntityAngles );
}

/**
*	@brief	Called each refresh frame.
**/
void CLG_misc_playerholo_RefreshFrame( clg_local_entity_t *self ) {

}

// Class definition.
const clg_local_entity_class_t client_misc_playerholo = {
	.classname = "client_misc_playerholo",
	.precache = CLG_misc_playerholo_Precache,
	.spawn = CLG_misc_playerholo_Spawn,
	.think = CLG_misc_playerholo_Think,
	.rframe = CLG_misc_playerholo_RefreshFrame,
	.class_locals_size = sizeof( clg_misc_playerholo_locals_t )
};