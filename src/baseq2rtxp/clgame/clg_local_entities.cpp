/********************************************************************
*
*
*	ClientGame: Local Entities
*
*
********************************************************************/
#include "clg_local.h"

#include "local_entities/clg_local_entities.h"


/**
*
*
*	Local Entities:
*
*
**/
//! Actual array of local client entities.
clg_local_entity_t clg_local_entities[ MAX_CLIENT_ENTITIES ];
//! Current number of local client entities.
uint32_t clg_num_local_entities = 0;

//! All local entity classname type descriptors.
const clg_local_entity_class_t *local_entity_classes[] = {
	// "client_misc_model" - Model Decorating Entity.
	&client_misc_model,
	// "client_misc_playerholo" - Mirrors a client's entity model.
	&client_misc_playerholo,
	// "client_misc_te" - Temp Event Entity.
	&client_misc_te,

	// nullptr - Last in line for iterating.
	nullptr
};



/**
*
*
*	Local Entities - Spawn/General Functionality:
*
*
**/
/**
*	@brief	Initialize a fresh local entity.
**/
const clg_local_entity_t *CLG_InitLocalEntity( clg_local_entity_t *lent ) {
	lent->inuse = true;
	//lent->classname = "noclass";
	//lent->locals.gravity = 1.0f;
	lent->id = lent - clg_local_entities;

	return lent;
}


/**
*	@brief	Frees up the local entity, increasing spawn_count to differentiate
*			for a new entity using an identical slot, as well as storing the
*			actual freed time. This'll prevent lerp issues etc.
**/
static void CLG_LocalEntity_Free( clg_local_entity_t *lent ) {
	if ( !lent ) {
		return;
	}

	// Free the entity class object if it had any set.
	if ( lent->classLocals ) {
		clgi.TagFree( lent->classLocals );
	}

	// Increment spawn count.
	const uint32_t spawn_count = lent->spawn_count + 1;

	// Zero out memory.
	*lent = {
		.id = static_cast<uint32_t>( lent - clg_local_entities ),
		.spawn_count = spawn_count,
		.inuse = false,
		.freetime = level.time,

		//.classname = "freed",
		.model = "",

		.locals = { }
	};
}

/**
*	@brief	Will always return a valid pointer to a free local entity slot, and
*			errors out in case of no available slots to begin with.
**/
static clg_local_entity_t *CLG_LocalEntity_Allocate() {
	// Total iterations taken at finding a previously freed up slot.
	int32_t i = 0;
	
	// Pointer to local entity.
	clg_local_entity_t *lent = clg_local_entities;
	
	for ( i = 0; i < clg_num_local_entities; i++, lent++ ) {
		if ( !lent->inuse && ( lent->freetime < 2_sec || level.time - lent->freetime > 500_ms ) ) {
			// Initialize local entity.
			CLG_InitLocalEntity( lent );
			// Return valid pointer.
			return lent;
		}
	}

	if ( i >= MAX_CLIENT_ENTITIES ) { //if ( lent == game.maxentities ) {
		// TODO: Do we want to error out on this? Why not return nullptr instead?
		clgi.Error( "%s: no free local entity slots to allocate.", __func__ );
	}

	// Increment count.
	clg_num_local_entities++;
	// Initialize local entity.
	CLG_InitLocalEntity( lent );
	// Return valid pointer.
	return lent;
}

/**
*	@brief	Frees all local entities.
**/
void CLG_FreeLocalEntities() {
	// Free local entities.
	for ( int32_t i = 0; i < clg_num_local_entities; i++ ) {
		CLG_LocalEntity_Free( &clg_local_entities[ i ] );
	}

	// Zero out the local entities array and reset number of local entities.
	memset( clg_local_entities, 0, sizeof( clg_local_entities ) );
	clg_num_local_entities = 0;
}

/**
*	@brief	Calls the localClass 'Precache' function pointer.
**/
const bool CLG_LocalEntity_DispatchPrecache( clg_local_entity_t *lent, const cm_entity_t *keyValues ) {
	// Need a valid lent and class.
	if ( !lent || !lent->classLocals ) {
		return false;
	}

	// Spawn.
	if ( lent->entityClass->precache ) {
		lent->entityClass->precache( lent, keyValues );
	}

	return true;
}
/**
*	@brief	Calls the localClass 'Spawn' function pointer.
**/
const bool CLG_LocalEntity_DispatchSpawn( clg_local_entity_t *lent ) {
	// Need a valid lent and class.
	if ( !lent || !lent->classLocals ) {
		return false;
	}

	// Spawn.
	if ( lent->entityClass->spawn ) {
		lent->entityClass->spawn( lent );
	}

	return true;
}
/**
*	@brief	Calls the localClass 'Think' function pointer.
**/
const bool CLG_LocalEntity_DispatchThink( clg_local_entity_t *lent ) {
	// Need a valid lent and class.
	if ( !lent || !lent->classLocals ) {
		return false;
	}

	// Spawn.
	if ( lent->entityClass->think ) {
		lent->entityClass->think( lent );
	}

	return true;
}
/**
*	@brief	Will do the key/value dictionary pair iteration for all 'classless/class irrelevant' entity locals variables.
**/
void CLG_LocalEntity_ParseLocals( clg_local_entity_t *lent, const cm_entity_t *keyValues ) {
	// Origin:
	if ( const cm_entity_t *originKv = clgi.CM_EntityKeyValue( keyValues, "origin" ) ) {
		if ( originKv->parsed_type & cm_entity_parsed_type_t::ENTITY_VECTOR3 ) {
			lent->locals.origin = originKv->vec3;
		}
	}

	// Angles:
	if ( const cm_entity_t *anglesKv = clgi.CM_EntityKeyValue( keyValues, "angle" ) ) {
		if ( anglesKv->parsed_type & cm_entity_parsed_type_t::ENTITY_FLOAT ) {
			lent->locals.angles = { 0.f, anglesKv->value, 0.f };
		}
	// Angle:
	} else if ( const cm_entity_t *anglesKv = clgi.CM_EntityKeyValue( keyValues, "angles" ) ) {
		if ( anglesKv->parsed_type & cm_entity_parsed_type_t::ENTITY_VECTOR3 ) {
			lent->locals.angles = anglesKv->vec3;
		}
	}
}

/**
*	@brief	Called from the client's Begin command, gives the client game a shot at
*			spawning local client entities so they can join in the image/model/sound
*			precaching context.
**/
void PF_SpawnEntities( const char *mapname, const char *spawnpoint, const cm_entity_t **entities, const int32_t numEntities ) {
	// Notify.
	clgi.Print( PRINT_NOTICE, "ClientGame: PF_SpawnEntities!\n" );

	// Be sure to clear any previous entities that might still be out there.
	CLG_FreeLocalEntities();

	// Iterate over its entities.
	for ( int32_t i = 0; i < numEntities; i++ ) {
		// Try and allocate a new local entity.
		clg_local_entity_t *localEntity = CLG_LocalEntity_Allocate();

		// Break if it is a nullptr, it indicates the limit has been reached.
		if ( localEntity == nullptr ) {
			// Debug print:
			clgi.Print( PRINT_DEVELOPER, "%s: %s\n", __func__, "local entity limit reached." );
			// Stop iterating.
			break;
		}

		// Acquire a pointer to the entity 'dictionary'.
		const cm_entity_t *edict = entities[ i ];

		// We need a classname to be set in order for us to look it up and possibly spawn this entity.
		const char *entityClassname = clgi.CM_EntityKeyValue( edict, "classname" )->nullable_string;
		if ( !entityClassname ) {
			// Debug print:
			clgi.Print( PRINT_DEVELOPER, "%s: %s\n", __func__, "entity dictionary without classname found." );
			// Free up the previously reserved slot, allowing a re-use.
			CLG_LocalEntity_Free( localEntity );
			// Continue iterating over the rest of the collision model entities.
			continue;
		} // if ( !strlen( entityClassname ) ...

		// Look up the class in the entity class list.
		for ( int32_t j = 0; j < q_countof( local_entity_classes ); j++ ) {
			// Get the class type.
			const clg_local_entity_class_t *classType = local_entity_classes[ j ];

			// Break if a nullptr.
			if ( classType == nullptr ) {
				// Debug print:
				clgi.Print( PRINT_DEVELOPER, "%s: failed to find classType for classname '%s'\n", __func__, entityClassname );
				// Free entity up.
				CLG_LocalEntity_Free( localEntity );
				break;
			} // if ( classType ...

			// If this triggers we found our matching entity classname data type.
			if ( strcmp( classType->classname, entityClassname ) == 0 ) {
				//
				localEntity->entityClass = classType;
				localEntity->entityDictionary = edict;

				// Iterate and parse the dictionary for the entity's local member key/values data.
				CLG_LocalEntity_ParseLocals( localEntity, localEntity->entityDictionary );

				// Allocate the classLocals memory.
				localEntity->classLocals = clgi.TagMalloc( classType->class_locals_size, TAG_CLGAME_LEVEL );

				// Dispatch precache callback.
				CLG_LocalEntity_DispatchPrecache( localEntity, localEntity->entityDictionary );

				// Break out of the local entity classes type loop.
				break;
			} // if ( strcmp( classType ...
		} // for ( int32_t j .... )

		// Iterate on to the next collision model entity.

	} // for ( int32_t i = 0; i < numEntities; i++ ) {

	// Now that all entities have been precached properly, dispatch their spawn callback functions.
	for ( int32_t i = 0; i < clg_num_local_entities; i++ ) {
		// Get local entity ptr
		clg_local_entity_t *localEntity = &clg_local_entities[ i ];
		// Dispatch spawn.
		CLG_LocalEntity_DispatchSpawn( localEntity );
	} // for ( int32_t i = 0; i < clg_num_local_entities; i++ ) {
}



/**
*
*
*	Local Entities - View Handling:
*
*
**/
/**
*	@brief	Add local client entities that are 'in-frame' to the view's refdef entities list.
**/
void CLG_AddLocalEntities( void ) {
	// Iterate over all entities.
	for ( int32_t i = 0; i < clg_num_local_entities; i++ ) {
		// Pointer to local entity.
		clg_local_entity_t *lent = &clg_local_entities[ i ];

		// We need a model index otherwise there is nothing to render.
		//if ( !lent->locals.modelindex ) {
		//	continue;
		//}

		// Clean slate refresh entity.
		entity_t rent = {};

		// Setup the refresh entity ID to start off at RENTITIY_OFFSET_LOCALENTITIES.
		rent.id = RENTITIY_OFFSET_LOCALENTITIES + lent->id;

		// Copy spatial information over into the refresh entity.
		VectorCopy( lent->locals.origin, rent.origin );
		VectorCopy( lent->locals.origin, rent.oldorigin );
		VectorCopy( lent->locals.angles, rent.angles );

		// Copy model information.
		if ( lent->locals.modelindex == MODELINDEX_PLAYER ) {
			rent.model = clgi.client->baseclientinfo.model;
			rent.skin = clgi.client->baseclientinfo.skin;
			rent.skinnum = 0;
		} else if ( lent->locals.modelindex ) {
			rent.model = precache.local_draw_models[ lent->locals.modelindex ];
			// Copy skin information.
			rent.skin = 0; // inline skin, -1 would use rgba.
			rent.skinnum = lent->locals.skinNumber;
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
		if ( lent->locals.modelindex == MODELINDEX_PLAYER ) {
			auto *lentClass = CLG_LocalEntity_GetClass<clg_misc_playerholo_locals_t>( lent );
			// Calculate back lerpfraction. (10hz.)
			rent.backlerp = 1.0f - ( ( clgi.client->time - ( (float)lentClass->frame_servertime - BASE_FRAMETIME ) ) / 100.f );
			clamp( rent.backlerp, 0.0f, 1.0f );
			rent.frame = lent->locals.frame;
			rent.oldframe = lent->locals.oldframe;

			// Add entity
			clgi.V_AddEntity( &rent );

			// Now prepare its weapon entity, to be added later on also.
			rent.model = clgi.client->baseclientinfo.weaponmodel[ 0 ]; //clgi.R_RegisterModel( "players/male/weapon.md2" );
		} else {
			rent.frame = lent->locals.frame;
			rent.oldframe = lent->locals.oldframe;
		}
		
		// Add it to the view.
		clgi.V_AddEntity( &rent );
	}
}