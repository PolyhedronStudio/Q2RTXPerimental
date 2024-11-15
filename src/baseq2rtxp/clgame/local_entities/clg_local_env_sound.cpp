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
*	client_env_sound:
*
*
**/
/**
*	@brief	Precache function for 'client_env_sound' entity class type.
**/
void CLG_env_sound_Precache( clg_local_entity_t *self, const cm_entity_t *keyValues ) {
	// Get class locals.
	clg_env_sound_locals_t *classLocals = CLG_LocalEntity_GetClass<clg_env_sound_locals_t>( self );

	// Key/Value: 'radius':
	if ( const cm_entity_t *modelKv = clgi.CM_EntityKeyValue( keyValues, "radius" ) ) {
		if ( modelKv->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_FLOAT ) {
			classLocals->radius = modelKv->value;
		// Resort to some default:
		} else {
			classLocals->radius = 250.f;
		}
	}

	// Key/Value: 'reverb_effect_id':
	if ( const cm_entity_t *reverb_effect_idKv = clgi.CM_EntityKeyValue( keyValues, "reverb_effect_id" ) ) {
		if ( reverb_effect_idKv->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_INTEGER ) {
			classLocals->reverbID = reverb_effect_idKv->integer;
		// Resort to the default one:
		} else {
			classLocals->reverbID = 0;
		}
	}

	//// Key/Value: 'skin':
	//if ( const cm_entity_t *skinKv = clgi.CM_EntityKeyValue( keyValues, "skin" ) ) {
	//	if ( skinKv->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_INTEGER ) {
	//		self->locals.skinNumber = skinKv->integer;
	//	} else {
	//		self->locals.skinNumber = 0;
	//	}
	//}

	//// Set up the modelname for precaching the model with.
	//if ( self->model && self->model[ 0 ] != '\0' ) {
	//	self->locals.modelindex = CLG_RegisterLocalModel( self->model );
	//} else {
	//	clgi.Print( PRINT_DEVELOPER, "%s: empty 'model' field. Unable to register local entity model.\n", __func__ );
	//}

	// DEBNUG PRINT:
	//clgi.Print( PRINT_DEVELOPER, "CLG_misc_env_sound: radius(%f), reverb_effect_id(%i)\n", classLocals->radius, classLocals->reverbID );
}

/**
*	@brief	Sets up the local client model entity.
**/
void CLG_env_sound_Spawn( clg_local_entity_t *self ) {
	// Setup appropriate mins, maxs.
	self->locals.mins = { -16, -16, 0 };
	self->locals.maxs = { 16, 16, 40 };

	// Link entity for this frame.
	CLG_LocalEntity_Link( self );

	// Setup nextthink.
	self->nextthink = level.time + FRAME_TIME_MS;

	// Add itself to the array of local env_sound entities.
	level.env_sound_list[ level.env_sound_list_count ] = self;
	level.env_sound_list_count++;
}

/**
*	@brief	Think once per local game tick.
**/
void CLG_env_sound_Think( clg_local_entity_t *self ) {
	//self->locals.oldframe = self->locals.frame;
	//self->locals.frame++;

	//if ( self->locals.frame >= 43 ) {
	//	self->locals.frame = 0;
	//}

	// Setup nextthink.
	self->nextthink = level.time + FRAME_TIME_MS;
}

/**
*	@brief	Called each refresh frame.
**/
void CLG_env_sound_RefreshFrame( clg_local_entity_t *self ) {

}

/**
*	@brief	Gives the chance to prepare and add a 'refresh entity' for this client local's frame view.
**/
void CLG_env_sound_PrepareRefreshEntity( clg_local_entity_t *self ) {
	//// Get class.
	//auto *selfClass = CLG_LocalEntity_GetClass<clg_misc_model_locals_t>( self );

	//// Clean slate refresh entity.
	//entity_t rent = {};

	//// Setup the refresh entity ID to start off at RENTITIY_OFFSET_LOCALENTITIES.
	//rent.id = RENTITIY_OFFSET_LOCALENTITIES + self->id;

	//// Copy spatial information over into the refresh entity.
	//VectorCopy( self->locals.origin, rent.origin );
	//VectorCopy( self->locals.origin, rent.oldorigin );
	//VectorCopy( self->locals.angles, rent.angles );

	//// Copy model information.
	//if ( self->locals.modelindex ) {
	//	rent.model = precache.local_draw_models[ self->locals.modelindex ];
	//	// Copy skin information.
	//	//rent.skin = 0; // inline skin, -1 would use rgba.
	//	//rent.skinnum = self->locals.skinNumber;
	//} else {
	//	rent.model = 0;
	//	rent.skin = 0; // inline skin, -1 would use rgba.
	//	rent.skinnum = 0;
	//}
	//rent.rgba.u32 = MakeColor( 255, 255, 255, 255 );

	//// Copy general render properties.
	//rent.alpha = 1.0f;
	//rent.scale = 1.0f;

	//// Copy animation data.
	////if ( self->locals.modelindex == MODELINDEX_PLAYER ) {
	//// Calculate back lerpfraction. (10hz.)
	//rent.backlerp = 1.0f - ( ( clgi.client->time - ( (float)selfClass->frame_servertime - BASE_FRAMETIME ) ) / 100.f );
	//clamp( rent.backlerp, 0.0f, 1.0f );
	//rent.frame = self->locals.frame;
	//rent.oldframe = self->locals.oldframe;

	//// Add entity
	//clgi.V_AddEntity( &rent );
}

// Class definition.
const clg_local_entity_class_t client_env_sound = {
	.classname = "client_env_sound",
	.callbackPrecache = CLG_env_sound_Precache,
	.callbackSpawn = CLG_env_sound_Spawn,
	.callbackThink = CLG_env_sound_Think,
	.callbackRFrame = CLG_env_sound_RefreshFrame,
	.callbackPrepareRefreshEntity = CLG_env_sound_PrepareRefreshEntity,
	.class_locals_size = sizeof( clg_env_sound_locals_t )
};
