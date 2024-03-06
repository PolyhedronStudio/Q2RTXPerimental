/********************************************************************
*
*
*	ClientGame: Local Entities
*
*
********************************************************************/
#include "clg_local.h"



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
int32_t clg_num_local_entities = 0;


//! All local entity class types.
/**
*
*
*	Local Entity Spawn Callback List:
*
*
**/
/**
*	@brief	Used for hooking up spawn functions to a 'classname' entity type.
**/
typedef struct clg_local_spawn_callback_s {
	//! Classname that 'hooks up' to the specified spawn function pointer.
	const char *classname;
	//! Function pointer which should point to the entity classname type's spawn function.
	void ( *spawn )( clg_local_entity_t *lent, const cm_entity_t *keyValues );
} clg_local_spawn_callback_t;

//! Declarations for linker:
void SP_client_misc_model( clg_local_entity_t *self, const cm_entity_t *keyValues ) {
	// Acquire origin.
	if ( const cm_entity_t *originKv = clgi.CM_EntityKeyValue( keyValues, "origin" ) ) {
		if ( originKv->parsed_type & cm_entity_parsed_type_t::ENTITY_VECTOR3 ) {
			self->locals.origin = originKv->vec3;
		}
	}

	Vector3 origin = self->locals.origin;
	const char *classname = "client_misc_model";
	clgi.Print( PRINT_NOTICE, "CLGame: Spawned local entity(classname: %s) at origin(%f %f %f)\n", classname, origin[ 0 ], origin[ 1 ], origin[ 2 ] );
}
void SP_client_misc_te( clg_local_entity_t *self, const cm_entity_t *keyValues ) {
	// Acquire origin.
	if ( const cm_entity_t *originKv = clgi.CM_EntityKeyValue( keyValues, "origin" ) ) {
		if ( originKv->parsed_type & cm_entity_parsed_type_t::ENTITY_VECTOR3 ) {
			self->locals.origin = originKv->vec3;
		}
	}

	Vector3 origin = self->locals.origin;
	const char *classname = "client_misc_te";
	clgi.Print( PRINT_NOTICE, "CLGame: Spawned local entity(classname: %s) at origin(%f %f %f)\n", classname, origin[ 0 ], origin[ 1 ], origin[ 2 ] );
}

//! List of entity classnames binding to their spawn functions.
static clg_local_spawn_callback_t local_entity_spawns[] = {
	{ "client_misc_model", SP_client_misc_model },
	{ "client_misc_te", SP_client_misc_te },

	{ nullptr, nullptr },
};



/**
*
*
*	Local Entity Functions:
*
*
**/
/**
*	@brief	Will return an incremented pointer to a clg_local_entities index entry.
*
*			TODO: Use a proper system that re-uses empty slots, like in the Server Game.
**/
static clg_local_entity_t *CLG_AllocateLocalEntity() {
	if ( clg_num_local_entities < MAX_CLIENT_ENTITIES ) {
		return &clg_local_entities[ clg_num_local_entities++ ];
	} else {
		// Limit reached.
		return nullptr;
	}
}

/**
*	@brief	Zeros out the local entity's memory structure.
**/
static void CLG_FreeLocalEntity( clg_local_entity_t *lent ) {
	// Free the entity class object if it had any set.
	//if ( lent->clazz ) {
	//	clgi.Z_Free( lent->clazz );
	//}

	// Zero out memory.
	if ( lent ) {
		*lent = {};
	}
}

/**
*	@brief	Will do the key/value dictionary pair iteration for all 'classless/class irrelevant' entity locals variables.
**/
void CLG_ParseEntityLocals( clg_local_entity_t *lent, const cm_entity_t *keyValues ) {
	// Origin:
	if ( const cm_entity_t *originKv = clgi.CM_EntityKeyValue( keyValues, "origin" ) ) {
		if ( originKv->parsed_type & cm_entity_parsed_type_t::ENTITY_VECTOR3 ) {
			lent->locals.origin = originKv->vec3;
		}
	}

	// Angles:
	if ( const cm_entity_t *anglesKv = clgi.CM_EntityKeyValue( keyValues, "angles" ) ) {
		if ( anglesKv->parsed_type & cm_entity_parsed_type_t::ENTITY_VECTOR3 ) {
			lent->locals.angles = anglesKv->vec3;
		}
	// Angle:
	} else if ( const cm_entity_t *anglesKv = clgi.CM_EntityKeyValue( keyValues, "angle" ) ) {
			if ( anglesKv->parsed_type & cm_entity_parsed_type_t::ENTITY_FLOAT ) {
				lent->locals.angles = { 0.f, anglesKv->value, 0.f };
			}
		}
}

/**
*	@brief	Looks up the local entities classname and calls upon its spawn function.
**/
static const bool CLG_SpawnLocalEntity( clg_local_entity_t *lent, const cm_entity_t *keyValues ) {
	// Need a valid lent.
	if ( !lent ) {
		return false;
	}

	// Iterate spawn callbacks for a matching classname.
	for ( clg_local_spawn_callback_t *spawnCallback = local_entity_spawns; spawnCallback && spawnCallback->classname; spawnCallback++ ) {
		if ( !strcmp( spawnCallback->classname, lent->locals.classname ) ) {
			spawnCallback->spawn( lent, keyValues );
			return true;
		}
	}

	return false;
}

/**
*	@brief	Frees all local entities.
**/
void CLG_FreeLocalEntities() {
	// Free local entities.
	for ( int32_t i = 0; i < clg_num_local_entities; i++ ) {
		CLG_FreeLocalEntity( &clg_local_entities[ i ] );
	}

	// Zero out the local entities array and reset number of local entities.
	memset( clg_local_entities, 0, sizeof( clg_local_entities ) );
	clg_num_local_entities = 0;
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
		clg_local_entity_t *localEntity = CLG_AllocateLocalEntity();

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
			clgi.Print( PRINT_DEVELOPER, "%s: %s\n", __func__, "nullptr classname found." );
			// Free up the previously reserved slot, allowing a re-use.
			CLG_FreeLocalEntity( localEntity );
			// Continue iterating over the rest of the collision model entities.
			continue;
		}

		// Assign the classname and its entity dictionary.
		localEntity->locals.classname = entityClassname;
		localEntity->entityDictionary = edict;

		// Lookup the entity classname and fire away its spawn function.
		if ( !CLG_SpawnLocalEntity( localEntity, edict ) ) {
			CLG_FreeLocalEntity( localEntity );
		}
	}
}