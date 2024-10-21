/********************************************************************
*
*
*	ClientGame - Local Entities: Misc
*
*
********************************************************************/
#include "../clg_local.h"
#include "../clg_entities.h"
#include "../clg_local_entities.h"
#include "../clg_precache.h"

// Include actual entity class types.
#include "clg_local_entity_classes.h"



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
		if ( modelKv->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_STRING ) {
			self->model = modelKv->string;
		} else {
			self->model = nullptr;// memset( classLocals->modelname, 0, MAX_QPATH );
		}
	}

	// Key/Value: 'frame':
	if ( const cm_entity_t *frameKv = clgi.CM_EntityKeyValue( keyValues, "frame" ) ) {
		if ( frameKv->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_INTEGER ) {
			self->locals.frame = frameKv->integer;
		} else {
			self->locals.frame = 0;
		}
	}

	// Key/Value: 'skin':
	if ( const cm_entity_t *skinKv = clgi.CM_EntityKeyValue( keyValues, "skin" ) ) {
		if ( skinKv->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_INTEGER ) {
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
	// Setup appropriate mins, maxs.
	self->locals.mins = { -16, -16, 0 };
	self->locals.maxs = { 16, 16, 40 };

	// Link entity for this frame.
	CLG_LocalEntity_Link( self );

	// Setup nextthink.
	self->nextthink = level.time + FRAME_TIME_MS;
}

/**
*	@brief	Think once per local game tick.
**/
void CLG_misc_model_Think( clg_local_entity_t *self ) {
	self->locals.oldframe = self->locals.frame;
	self->locals.frame++;

	if ( self->locals.frame >= 43 ) {
		self->locals.frame = 0;
	}

	// Setup nextthink.
	self->nextthink = level.time + FRAME_TIME_MS;
}

/**
*	@brief	Called each refresh frame.
**/
void CLG_misc_model_RefreshFrame( clg_local_entity_t *self ) {

}

/**
*	@brief	Gives the chance to prepare and add a 'refresh entity' for this client local's frame view.
**/
void CLG_misc_model_PrepareRefreshEntity( clg_local_entity_t *self ) {
	// Get class.
	auto *selfClass = CLG_LocalEntity_GetClass<clg_misc_model_locals_t>( self );

	// Clean slate refresh entity.
	entity_t rent = {};

	// Setup the refresh entity ID to start off at RENTITIY_OFFSET_LOCALENTITIES.
	rent.id = RENTITIY_OFFSET_LOCALENTITIES + self->id;

	// Copy spatial information over into the refresh entity.
	VectorCopy( self->locals.origin, rent.origin );
	VectorCopy( self->locals.origin, rent.oldorigin );
	VectorCopy( self->locals.angles, rent.angles );

	// Copy model information.
	if ( self->locals.modelindex ) {
		rent.model = precache.local_draw_models[ self->locals.modelindex ];
		// Copy skin information.
		//rent.skin = 0; // inline skin, -1 would use rgba.
		//rent.skinnum = self->locals.skinNumber;
	} else {
		rent.model = 0;
		rent.skin = 0; // inline skin, -1 would use rgba.
		rent.skinnum = 0;
	}
	rent.rgba.u32 = MakeColor( 255, 255, 255, 255 );

	// Copy general render properties.
	rent.alpha = 1.0f;
	rent.scale = 1.0f;

	// Copy animation data.
	//if ( self->locals.modelindex == MODELINDEX_PLAYER ) {
	// Calculate back lerpfraction. (10hz.)
	rent.backlerp = 1.0f - ( ( clgi.client->time - ( (float)selfClass->frame_servertime - BASE_FRAMETIME ) ) / 100.f );
	clamp( rent.backlerp, 0.0f, 1.0f );
	rent.frame = self->locals.frame;
	rent.oldframe = self->locals.oldframe;

	// Add entity
	clgi.V_AddEntity( &rent );
}

// Class definition.
const clg_local_entity_class_t client_misc_model = {
	.classname = "client_misc_model",
	.callbackPrecache = CLG_misc_model_Precache,
	.callbackSpawn = CLG_misc_model_Spawn,
	.callbackThink = CLG_misc_model_Think,
	.callbackRFrame = CLG_misc_model_RefreshFrame,
	.callbackPrepareRefreshEntity = CLG_misc_model_PrepareRefreshEntity,
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
	// Get class.
	auto *selfClass = CLG_LocalEntity_GetClass<clg_misc_te_locals_t>( self );

	// Key/Value: 'event':
	if ( const cm_entity_t *eventTypeKv = clgi.CM_EntityKeyValue( keyValues, "event" ) ) {
		if ( eventTypeKv->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_INTEGER ) {
			selfClass->teEvent = static_cast<temp_event_t>( eventTypeKv->integer );
		} else {
			selfClass->teEvent = static_cast<temp_event_t>( 0 );
		}
	}

	// TODO: Other key values for Temp Event Entity.
	self->locals.modelindex = 0;

	// DEBNUG PRINT:
	clgi.Print( PRINT_DEVELOPER, "CLG_misc_te_Precache: event(%d)\n", selfClass->teEvent );
}

/**
*	@brief	Sets up the local client temp entity event entity.
**/
void CLG_misc_te_Spawn( clg_local_entity_t *self ) {
	// Setup appropriate mins, maxs.
	self->locals.mins = { -4, -4, -4 };
	self->locals.maxs = { 4, 4, 4 };

	// Link entity for this frame.
	CLG_LocalEntity_Link( self );

	// Setup nextthink.
	self->nextthink = level.time + FRAME_TIME_MS;
}
/**
*	@brief	Think once per local game tick.
**/
void CLG_misc_te_Think( clg_local_entity_t *self ) {
	// Setup nextthink.
	self->nextthink = level.time + FRAME_TIME_MS;
}
/**
*	@brief	Called each refresh frame.
**/
void CLG_misc_te_RefreshFrame( clg_local_entity_t *self ) {

}
/**
*	@brief	Gives the chance to prepare and add a 'refresh entity' for this client local's frame view.
**/
void CLG_misc_te_PrepareRefreshEntity( clg_local_entity_t *self ) {

}

// Class definition.
const clg_local_entity_class_t client_misc_te = {
	.classname = "client_misc_te",
	.callbackPrecache = CLG_misc_te_Precache,
	.callbackSpawn = CLG_misc_te_Spawn,
	.callbackThink = CLG_misc_te_Think,
	.callbackRFrame = CLG_misc_te_RefreshFrame,
	.callbackPrepareRefreshEntity = CLG_misc_te_PrepareRefreshEntity,
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
	// Get class.
	auto *selfClass = CLG_LocalEntity_GetClass<clg_misc_playerholo_locals_t>( self );

	// Key/Value: 'frame':
	if ( const cm_entity_t *clientnumKv = clgi.CM_EntityKeyValue( keyValues, "clientnum" ) ) {
		if ( clientnumKv->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_INTEGER ) {
			selfClass->clientNumber = clientnumKv->integer;
		} else {
			selfClass->clientNumber = 0;
		}
	}

	// Set up the modelname for precaching the model with.
	self->locals.modelindex = MODELINDEX_PLAYER;
	self->locals.skin = 0;
	self->locals.skinNumber = 0;

	// DEBNUG PRINT:
	clgi.Print( PRINT_DEVELOPER, "CLG_misc_playerholo_Precache: model(%s), local_draw_model_handle(%d) frame(%d), skin(%d)\n", self->model, self->locals.modelindex, self->locals.frame, self->locals.skinNumber );
}

/**
*	@brief	Sets up the local client model entity.
**/
void CLG_misc_playerholo_Spawn( clg_local_entity_t *self ) {
	// Setup appropriate mins, maxs, absmins, absmaxs, size
	self->locals.mins = { -16, -16, -24 };
	self->locals.maxs = { 16, 16, 32 };

	// Link entity for this frame.
	CLG_LocalEntity_Link( self );

	// Setup nextthink.
	self->nextthink = level.time + FRAME_TIME_MS;
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

	// Setup nextthink.
	self->nextthink = level.time + FRAME_TIME_MS;
}

/**
*	@brief	Called each refresh frame.
**/
void CLG_misc_playerholo_RefreshFrame( clg_local_entity_t *self ) {

}

/**
*	@brief	Gives the chance to prepare and add a 'refresh entity' for this client local's frame view.
**/
void CLG_misc_playerholo_PrepareRefreshEntity( clg_local_entity_t *self ) {
	// Spawn flag.
	static constexpr int32_t SPAWNFLAG_CONNECTED_CLIENTS_ONLY = 8192;

	// Get class.
	auto *selfClass = CLG_LocalEntity_GetClass<clg_misc_playerholo_locals_t>( self );

	// Clamp client number value to prevent invalid index accessing.
	const int32_t clientInfoNumber = constclamp( selfClass->clientNumber, 0, MAX_CLIENTS );
	// Get the client info for the client that this client_misc_playerholo entity is a hologram of.
	clientinfo_t *clientInfo = &clgi.client->clientinfo[ selfClass->clientNumber ];
	
	// In case of it not being our base client info, make sure to check it has a valid model and skin.
	if ( clientInfo != &clgi.client->baseclientinfo ) {
		// We need it to have valid model and skin, otherwise resort to our own base client info instead.
		if ( !clientInfo || !clientInfo->model || !clientInfo->skin || !clientInfo->weaponmodel[ 0 ] ) {
			// In case the spawnflag is set for not rendering the model at all when there is no valid client info.
			if ( !(self->spawnflags & SPAWNFLAG_CONNECTED_CLIENTS_ONLY ) ) {
				clientInfo = &clgi.client->baseclientinfo;
			} else {
				clientInfo = nullptr;
			}
		}
	}

	// If not having any valid clientInfo data, don't add ourselves to the render frame's view.
	if ( !clientInfo ) {
		return;
	}

	// Clean slate refresh entity.
	entity_t rent = {};

	// Setup the refresh entity ID to start off at RENTITIY_OFFSET_LOCALENTITIES.
	rent.id = RENTITIY_OFFSET_LOCALENTITIES + self->id;

	// Copy spatial information over into the refresh entity.
	VectorCopy( self->locals.origin, rent.origin );
	VectorCopy( self->locals.origin, rent.oldorigin );
	VectorCopy( self->locals.angles, rent.angles );
	
	// Copy model information.
	rent.model = clientInfo->model; //clgi.client->clientinfo[ selfClass->clientNumber ].model;
	rent.skin = clientInfo->skin; //clgi.client->clientinfo[ selfClass->clientNumber ].skin;
	rent.skinnum = 0;
	rent.rgba.u32 = MakeColor( 255, 255, 255, 255 );

	// Copy general render properties.
	rent.alpha = 1.0f;
	rent.scale = 1.0f;

	// Calculate back lerpfraction at 10hz.
	rent.backlerp = 1.0f - ( ( clgi.client->time - ( (float)selfClass->frame_servertime - BASE_FRAMETIME ) ) / 100.f );
	clamp( rent.backlerp, 0.0f, 1.0f );

	rent.frame = self->locals.frame;
	rent.oldframe = self->locals.oldframe;

	// Add entity
	clgi.V_AddEntity( &rent );

	// Now prepare its weapon entity, to be added later on also.
	rent.skin = 0;
	rent.model = clientInfo->weaponmodel[ 0 ]; //clgi.R_RegisterModel( "players/male/weapon.md2" );
	// Add it to the view.
	clgi.V_AddEntity( &rent );
}

// Class definition.
const clg_local_entity_class_t client_misc_playerholo = {
	.classname = "client_misc_playerholo",
	.callbackPrecache = CLG_misc_playerholo_Precache,
	.callbackSpawn = CLG_misc_playerholo_Spawn,
	.callbackThink = CLG_misc_playerholo_Think,
	.callbackRFrame = CLG_misc_playerholo_RefreshFrame,
	.callbackPrepareRefreshEntity = CLG_misc_playerholo_PrepareRefreshEntity,
	.class_locals_size = sizeof( clg_misc_playerholo_locals_t )
};