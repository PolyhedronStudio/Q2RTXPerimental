/********************************************************************
*
*
*	ClientGame: Local Entities
*
*
********************************************************************/
#include "clg_local.h"


//#define RESERVED_ENTITIY_GUN 1
//#define RESERVED_ENTITIY_TESTMODEL 2
//#define RESERVED_ENTITIY_COUNT 3


/**
*
*
*	Local Entities:
*
*
**/
typedef struct clg_local_entity_s clg_local_entity_t;

typedef struct clg_local_entity_class_s {
	const char *classname;
	void ( *Precache )( clg_local_entity_t *self );
	void ( *Spawn )( clg_local_entity_t *self );
	void ( *Think )( clg_local_entity_t *self );
} clg_local_entity_class_t;

typedef struct clg_local_entity_s {
	//! Unique identifier for each entity.
	uint32_t id;

	//! Points right to the collision model's entity dictionary.
	const cm_entity_t *edict;

	//! A pointer to the entity's class specific data.
	clg_local_entity_class_t *clazz;
} clg_local_entity_t;

/**
*
*
*	Spawn Entities:
*
*
**/
static clg_local_entity_t clg_local_entities[ MAX_CLIENT_ENTITIES ];
static int32_t clg_num_local_entities = 0;

static clg_local_entity_t *GetNextLocalEntity() {
	if ( clg_num_local_entities < MAX_CLIENT_ENTITIES ) {
		return &clg_local_entities[ clg_num_local_entities++ ];
	} else {
		// Limit reached.
		return nullptr;
	}
}
void PF_SpawnEntities() {
	//// Get collision model.
	//const cm_t *cm = &clgi.client->collisionModel;

	//// Get next free entity.
	//clg_local_entity_t *localEntity = GetNextLocalEntity();

	//// Iterate over its entities.
	//for ( int32_t i = 0; i < cm->numentities; i++ ) {
	//	// Acquire a pointer to the entity 'dictionary'.
	//	const cm_entity_t *edict = cm->entities[ i ];

	//	// Break if it is a nullptr, it indicates the limit has been reached.
	//	if ( localEntity == nullptr ) {
	//		break;
	//	}

	//	// Assign its dictionary of key/values.
	//	//localEntity->edict = edict;
	//	const char *entityClassname = clgi.CM_EntityKeyValue( edict, "classname" )->nullable_string;
	//	
	//	// Ensure it has a classname
	//	if ( entityClassname ) {
	//		clgi.Print( PRINT_DEVELOPER, "ClientGame Local Entity #%d: classname(%s)\n", i, ( entityClassname ? entityClassname : "nullable_string!!" )  );

	//	}

	//	// Get the next entity in line of the dictionary entity key/value pairs.
	//	localEntity = GetNextLocalEntity();
	//}
}