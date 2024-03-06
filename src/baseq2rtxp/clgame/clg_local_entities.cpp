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
	// "client_misc_te" - Temp Event Entity.
	&client_misc_te,

	// nullptr - Last in line for iterating.
	nullptr
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
	if ( lent->classLocals ) {
		clgi.TagFree( lent->classLocals );
	}

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
static const bool CLG_SpawnLocalEntity( clg_local_entity_t *lent ) {
	// Need a valid lent.
	if ( !lent ) {
		return false;
	}

	// Spawn.
	if ( lent->entityClass->spawn ) {
		lent->entityClass->spawn( lent );
	}

	return true;
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
		if ( !strlen( entityClassname ) ) {
			// Debug print:
			clgi.Print( PRINT_DEVELOPER, "%s: %s\n", __func__, "entity dictionary without classname found." );
			// Free up the previously reserved slot, allowing a re-use.
			CLG_FreeLocalEntity( localEntity );
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
				CLG_FreeLocalEntity( localEntity );
				break;
			} // if ( classType ...

			// Does the classname match that which we found in the entity dictionary?
			if ( strcmp( classType->classname, entityClassname ) == 0 ) {
				// TODO: Should ID be set by Allocate function instead? I suppose so.
				*localEntity = {
					/*localEntity->*/.id = MAX_EDICTS + clg_num_local_entities,
					/*localEntity->*/.entityClass = classType,
					/*localEntity->*/.entityDictionary = edict,
				};

				// Iterate and parse the dictionary for the entity's local member key/values data.
				CLG_ParseEntityLocals( localEntity, edict );

				// Allocate the classLocals memory.
				localEntity->classLocals = clgi.TagMalloc( classType->class_locals_size, TAG_CLGAME_LEVEL );
				
				if ( localEntity->entityClass && localEntity->entityClass->precache ) {
					// Call upon the entity's class precache method.
					localEntity->entityClass->precache( localEntity, edict );
				}

				// Break out of the local entity classes type loop.
				break;
			} // if ( strcmp( classType ...
		} // for ( int32_t j .... )

		// Iterate on to the next collision model entity.

	} // for ( int32_t i = 0; i < numEntities; i++ ) {

	// Now that all entities have been precached properly, call their spawn functions.
	for ( int32_t i = 0; i < clg_num_local_entities; i++ ) {
		// Get local entity ptr
		clg_local_entity_t *localEntity = &clg_local_entities[ i ];

		// Make sure it has an entity class.
		if ( localEntity->entityClass && localEntity->entityClass->spawn ) {
			localEntity->entityClass->spawn( localEntity );
		}
	}
}