/********************************************************************
*
*
*	ClientGame: Local Entities
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_local_entities.h"
#include "clgame/local_entities/clg_local_entity_classes.h"
#include "clgame/local_entities/clg_local_env_sound.h"


/**
*	Debug Configuration:
**/
//! Uncomment to enable printing of a failure where the entity dictionary has no classname key/value entry.
//#define PRINT_NO_CLASSTYPE_FAILURE 1
//! Uncomment to enable printing of a failure while trying to find an entity class type that matches the classname.
//#define PRINT_CLASSTYPE_FIND_FAILURE 1



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
	// "client_env_sound" - Sound Reverb Effect Entity.
	&client_env_sound,

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
*	Local Entities - Allocate/Free:
*
*
**/
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
		clgi.Error( ERR_DROP, "%s: no free local entity slots to allocate.", __func__ );
		return nullptr;
	}

	// Increment count.
	clg_num_local_entities++;
	// Initialize local entity.
	CLG_InitLocalEntity( lent );
	// Return valid pointer.
	return lent;
}
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
void CLG_LocalEntity_Free( clg_local_entity_t *lent ) {
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
	const uint32_t new_id = static_cast<uint32_t>( lent - clg_local_entities );

	*lent = {
		.id = new_id,
		.spawn_count = spawn_count,
		.inuse = false,
		.islinked = false,
		.freetime = level.time,
		.nextthink = 0_ms,
		//.classname = "freed",
		.model = "",

		.locals = { }
	};
}
/**
*	@brief	Frees all local entities.
**/
void CLG_LocalEntity_FreeAllClasses() {
	// Free local entity classes.
	for ( int32_t i = 0; i < clg_num_local_entities; i++ ) {
		CLG_LocalEntity_Free( &clg_local_entities[ i ] );
	}

	// Zero out the local entities array and reset number of local entities.
	memset( clg_local_entities, 0, sizeof( clg_local_entities ) );
	clg_num_local_entities = 0;
}



/**
*
*
*	Local Entities - Physics/PVS
*
*
**/
/**
*	@brief	Runs thinking code for this frame if necessary
**/
bool CLG_LocalEntity_RunThink( clg_local_entity_t *lent ) {
	// Ensure we got class locals.
	if ( !lent->classLocals ) {
		clgi.Print( PRINT_DEVELOPER, "%s: Local Entity(%d) without class locals!\n", __func__, lent->id );
		return true;
	}

	QMTime thinktime = lent->nextthink;
	if ( thinktime <= 0_ms ) {
		return true;
	}
	if ( thinktime > level.time ) {
		return true;
	}

	lent->nextthink = 0_ms;
	if ( !lent->think ) {
		clgi.Error( ERR_DROP, "%s: Local Entity(%d) nullptr think\n", __func__, lent->id );
		return true;
	}
	
	// Call think callback.
	//lent->think( lent );
	CLG_LocalEntity_DispatchThink( lent );

	return false;
}

/**
*	@brief	
**/
void CLG_LocalEntity_Link( clg_local_entity_t *lent ) {
	if ( !lent ) {
		return;
	}

	// Link to PVS leafs.
	lent->areanum = 0;
	lent->areanum2 = 0;

	// Set entity size.
	lent->locals.size = lent->locals.maxs - lent->locals.mins;

	// Set absMin/absMax.
	lent->locals.absMin = lent->locals.origin + lent->locals.mins;
	lent->locals.absMax = lent->locals.origin + lent->locals.maxs;

	// For box leafs.
	vec3_t absMin;
	vec3_t absMax;
	VectorCopy( lent->locals.absMin, absMin );
	VectorCopy( lent->locals.absMax, absMax );

	mleaf_t *leafs[ MAX_TOTAL_ENT_LEAFS ];
	int         clusters[ MAX_TOTAL_ENT_LEAFS ];
	int         num_leafs;
	int         i, j;
	int         area;
	mnode_t *topnode;

	// Get all leafs, including solids.
	num_leafs = clgi.CM_BoxLeafs( absMin, absMax,
		leafs, MAX_TOTAL_ENT_LEAFS, &topnode );

	// Set areas
	for ( i = 0; i < num_leafs; i++ ) {
		clusters[ i ] = leafs[ i ]->cluster;
		area = leafs[ i ]->area;
		if ( area ) {
			// Doors may legally straggle two areas, but nothing should evern need more than that
			if ( lent->areanum && lent->areanum != area ) {
				if ( lent->areanum2 && lent->areanum2 != area ) {
					clgi.Print( PRINT_DEVELOPER, "%s: Object touching 3 areas at %f %f %f\n",
						__func__, absMin[ 0 ], absMin[ 1 ], absMin[ 2 ] );
				}
				lent->areanum2 = area;
			} else
				lent->areanum = area;
		}
	}

	// Link clusters.
	lent->num_clusters = 0;

	if ( num_leafs >= MAX_TOTAL_ENT_LEAFS ) {
		// Assume we missed some leafs, and mark by headnode.
		lent->num_clusters = -1;
		lent->headnode = clgi.CM_NumberForNode( topnode );
	} else {
		lent->num_clusters = 0;
		for ( i = 0; i < num_leafs; i++ ) {
			if ( clusters[ i ] == -1 ) {
				continue; // Not a visible leaf.
			}
			for ( j = 0; j < i; j++ ) {
				if ( clusters[ j ] == clusters[ i ] ) {
					break;
				}
			}
			if ( j == i ) {
				if ( lent->num_clusters == MAX_ENT_CLUSTERS ) {
					// Assume we missed some leafs, and mark by headnode.
					lent->num_clusters = -1;
					lent->headnode = clgi.CM_NumberForNode( topnode );
					break;
				}

				lent->clusternums[ lent->num_clusters++ ] = clusters[ i ];
			}
		}
	}

	// Linked.
	lent->islinked = true;
}
/**
*	@brief	
**/
void CLG_LocalEntity_Unlink( clg_local_entity_t *lent ) {
	if ( !lent ) {
		return;
	}

	lent->areanum = lent->areanum2 = lent->headnode = 0;
	lent->islinked = false;
}



/**
*
*
*	Local Entities - Callback Dispatch Handling:
*
*
**/
/**
*	@brief	Calls the localClass 'Precache' function pointer.
**/
const bool CLG_LocalEntity_DispatchPrecache( clg_local_entity_t *lent, const cm_entity_t *keyValues ) {
	// Need a valid lent and class.
	if ( !lent || !lent->classLocals ) {
		return false;
	}

	// Spawn.
	if ( lent->precache ) {
		lent->precache( lent, keyValues );
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
	if ( lent->spawn ) {
		lent->spawn( lent );
	}

	return true;
}
/**
*	@brief	Calls the localClass 'Spawn' function pointer.
**/
const bool CLG_LocalEntity_DispatchPostSpawn( clg_local_entity_t *lent ) {
	// Need a valid lent and class.
	if ( !lent || !lent->classLocals ) {
		return false;
	}

	// Spawn.
	if ( lent->postSpawn ) {
		lent->postSpawn( lent );
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
	if ( lent->think ) {
		lent->think( lent );
	}

	return true;
}
/**
*	@brief	Calls the localClass 'RefreshFrame' function pointer.
**/
const bool CLG_LocalEntity_DispatchRefreshFrame( clg_local_entity_t *lent ) {
	// Need a valid lent and class.
	if ( !lent || !lent->classLocals ) {
		return false;
	}

	// RefreshFrame.
	if ( lent->rframe) {
		lent->rframe( lent );
	}

	return true;
}
/**
*	@brief	Calls the localClass 'PrepareRefreshEntity' function pointer.
**/
const bool CLG_LocalEntity_DispatchPrepareRefreshEntity( clg_local_entity_t *lent ) {
	// Need a valid lent and class.
	if ( !lent || !lent->classLocals || !lent->islinked ) {
		return false;
	}

	// PrepareRefreshEntity.
	if ( lent->prepareRefreshEntity ) {
		lent->prepareRefreshEntity( lent );
	}

	return true;
}



/**
*
*
*	Local Entities - Spawn Handling:
*
*
**/
/**
*	@brief	Executed by default for each local entity during SpawnEntities. Will do the key/value dictionary pair
*			iteration for all entity and entity locals variables. Excluding the class specifics.
**/
void CLG_LocalEntity_ParseLocals( clg_local_entity_t *lent, const cm_entity_t *keyValues ) {
	// Spawnflags:
	if ( const cm_entity_t *originKv = clgi.CM_EntityKeyValue( keyValues, "spawnflags" ) ) {
		if ( originKv->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_INTEGER ) {
			lent->spawnflags = originKv->integer;
		}
	}

	// Origin:
	if ( const cm_entity_t *originKv = clgi.CM_EntityKeyValue( keyValues, "origin" ) ) {
		if ( originKv->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_VECTOR3 ) {
			lent->locals.origin = originKv->vec3;
		}
	}

	// Angles:
	if ( const cm_entity_t *anglesKv = clgi.CM_EntityKeyValue( keyValues, "angle" ) ) {
		if ( anglesKv->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_FLOAT ) {
			lent->locals.angles = { 0.f, anglesKv->value, 0.f };
		}
		// Angle:
	} else if ( const cm_entity_t *anglesKv = clgi.CM_EntityKeyValue( keyValues, "angles" ) ) {
		if ( anglesKv->parsed_type & cm_entity_parsed_type_t::ENTITY_PARSED_TYPE_VECTOR3 ) {
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
	clgi.Print( PRINT_NOTICE, "ClientGame: %s\n", __func__ );

	// Be sure to clear any previous entities that might still be out there.
	CLG_LocalEntity_FreeAllClasses();

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
			#ifdef PRINT_NO_CLASSTYPE_FAILURE
			// Debug print:
			clgi.Print( PRINT_DEVELOPER, "%s: %s\n", __func__, "entity dictionary without classname found." );
			#endif
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
				#ifdef PRINT_CLASSTYPE_FIND_FAILURE
				// Debug print:
				clgi.Print( PRINT_DEVELOPER, "%s: failed to find classType for classname '%s'\n", __func__, entityClassname );
				#endif
				// Free entity up.
				CLG_LocalEntity_Free( localEntity );
				break;
			} // if ( classType ...

			// If this triggers we found our matching entity classname data type.
			if ( strcmp( classType->classname, entityClassname ) == 0 ) {
				// Store entity class type.
				localEntity->entityClass = classType;
				localEntity->entityDictionary = edict;

				// Iterate and parse the dictionary for the entity's local member key/values data.
				CLG_LocalEntity_ParseLocals( localEntity, localEntity->entityDictionary );

				// Allocate the classLocals memory now.
				localEntity->classLocals = clgi.TagMalloc( classType->class_locals_size, TAG_CLGAME_LEVEL );

				// Setup the default entity function callbacks to those supplied by the 'entity class type'.
				localEntity->precache = classType->callbackPrecache;
				localEntity->spawn = classType->callbackSpawn;
				localEntity->postSpawn = classType->callbackPostSpawn;
				localEntity->think = classType->callbackThink;
				localEntity->rframe = classType->callbackRFrame;
				localEntity->prepareRefreshEntity = classType->callbackPrepareRefreshEntity;

				// Dispatch precache callback, this gives the entity a chance to parse the dictionary
				// for classLocals variables that need their values assigned.
				CLG_LocalEntity_DispatchPrecache( localEntity, localEntity->entityDictionary );

				// Break out of the local entity classes type loop.
				break;
			} // if ( strcmp( classType ...
		} // for ( int32_t j .... )
	
		//!
		// Iterate on to the next collision model entity.
		//!
		
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
*   @brief  Called from CL_Begin, right after finishing precaching. Used for example to access precached data in.
**/
void PF_PostSpawnEntities() {
	// Now that all entities have been precached properly, dispatch their spawn callback functions.
	for ( int32_t i = 0; i < clg_num_local_entities; i++ ) {
		// Get local entity ptr
		clg_local_entity_t *localEntity = &clg_local_entities[ i ];

		// Dispatch spawn.
		CLG_LocalEntity_DispatchPostSpawn( localEntity );
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
*	@return	True if entity is inside the local client's PVS and should be processed to
*			prepare a refresh entity.			
**/
const bool CLG_LocalEntity_InLocalPVS( clg_local_entity_t *lent ) {
	// Default to visible so all code below only has to check for it being invsiible(thus only sets it to false.)
	bool isVisible = true;

	// Area Checks:
	if ( clgi.client->localPVS.lastValidCluster >= 0 && !clgi.CM_AreasConnected( clgi.client->localPVS.leaf->area, lent->areanum ) ) {
		// Doors can legally straddle two areas, so we may need to check another one
		if ( !clgi.CM_AreasConnected( clgi.client->localPVS.leaf->area, lent->areanum2 ) ) {
			// Blocked by a door:
			return false;
		}
	}

	// If area checks succeeded and it is vissible, test the cluster or go by head node.
	if ( isVisible ) {
		// beams just check one point for PHS
		//if ( ent->s.renderfx & RF_BEAM ) {
		//	if ( !Q_IsBitSet( clientphs, ent->clusternums[ 0 ] ) )
		//		ent_visible = false;
		//} else {
			//if ( cull_nonvisible_clientgame_entities ) {
				int i = 0;
				// Too many leafs for individual check, go by headnode:
				if ( lent->num_clusters == -1 ) {
					if ( !clgi.CM_HeadnodeVisible( clgi.CM_NodeForNumber( lent->headnode ), clgi.client->localPVS.pvs ) ) {
						isVisible = false;
					}
				} else {
					// Check the cluster's individual leafs.
					for ( i = 0; i < lent->num_clusters; i++ ) {
						if ( Q_IsBitSet( clgi.client->localPVS.pvs, lent->clusternums[ i ] ) ) {
							// Possibly visible.
							break;
						}
					}
					// Not visible:
					if ( i == lent->num_clusters ) {
						isVisible = false;
					}
				}
			//} //if ( cull_nonvisible_clientgame_entities ) {
		//}
	}

	// Return the final result, visible or not.
	return isVisible;
}

/**
*	@brief	Add local client entities that are 'in-frame' to the view's refdef entities list.
**/
void CLG_AddLocalEntities( void ) {
	// Iterate over all entities.
	for ( int32_t i = 0; i < clg_num_local_entities; i++ ) {
		// Get local entity pointer.
		clg_local_entity_t *lent = &clg_local_entities[ i ];

		// Skip iteration if entity is not in use, or not properly class allocated.
		if ( !lent || !lent->inuse || !lent->classLocals || !lent->islinked ) {
			continue;
		}

		// Determine whether it is visible at all.
		if ( CLG_LocalEntity_InLocalPVS( lent ) ) {
			// Get its class locals.
			CLG_LocalEntity_DispatchPrepareRefreshEntity( lent );
			// Debug print it ain't visible.
			//clgi.Print( PRINT_NOTICE, "%s: lent(#%d) PVS VISIBLE.\n", __func__, lent->id );
		} else {
			// Debug print it ain't visible.
			//clgi.Print( PRINT_NOTICE, "%s: lent(#%d) is not PVS visible.\n", __func__, lent->id );
		}
	}
}

/**
*	@brief	Used by PF_ClearState.
**/
void CLG_LocalEntity_ClearState() {
	// Clear all local entity classes as well as zeroing out the array.
	CLG_LocalEntity_FreeAllClasses();

	// Clear out client entities array.
	memset( clg_entities, 0, globals.entity_size * sizeof( clg_entities[ 0 ] ) );
}