/********************************************************************
*
*
*	SVGame: Edicts Functionalities:
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_edict_pool.h"
#include "svgame/svg_trigger.h"
#include "svgame/svg_signalio.h"
#include "svgame/svg_usetargets.h"
#include <functional>
#include "sharedgame/sg_usetarget_hints.h"
#include "svgame/entities/svg_player_edict.h"
#include "svgame/entities/svg_worldspawn_edict.h"



/**
*
*
*
*	ServerGame Edict Pool Interface Implementation:
*
*
*
**/
/**
*	@brief	Easy access to an edict within the edict pool.
**/
svg_base_edict_t *svg_edict_pool_t::operator[]( size_t index ) {
	return ( index >= 0 && index < MAX_EDICTS && edicts != nullptr ? edicts[ index ] : nullptr );
}

/**
*	@brief	Returns a pointer to the edict matching the number.
*	@return	The edict for the given number. (nullptr if out of range).
**/
svg_base_edict_t *svg_edict_pool_t::EdictForNumber( const int32_t number ) {
	// Ensure edicts is a valid ptr.
	if ( !edicts ) {
		return nullptr;
	}
	// Check if the number is within range.
	if ( number < 0 || number >= max_edicts ) {
		return nullptr;
	}
	// Return the edict at the given number.
	return edicts[ number ];
}

/**
*	@brief		Returns the slot index number for a given edict.
*	@param		edict A pointer to the edict whose slot index number is to be determined.
*	@return		The slot index number of the given edict, or -1 if the edict is out of range.
**/
const int32_t svg_edict_pool_t::NumberForEdict( const svg_base_edict_t *edict ) {
	// Ensure the pool is there.
	//if ( globals.edictPool == nullptr ) {
	//	return -1;
	//}
	// Ensure edict is a valid ptr.
	if ( !edict ) {
		return ENTITYNUM_NONE;
	}
	// Ensure edicts is a valid ptr.
	if ( edicts == nullptr ) {
		return ENTITYNUM_NONE;
	}
	// Check if the edict is within range.
	if ( edict->s.number < 0 || edict->s.number >= max_edicts ) {
		return ENTITYNUM_NONE;
	}
	//if ( edict < edicts || edict >= &edicts[ max_edicts ] ) {
	//	return -1;
	//}
	// Return the number of the given edict.
	return edict->s.number;//edict - edicts;
}

/**
*   @brief  Marks the edict as free
**/
void svg_edict_pool_t::FreeEdict( svg_base_edict_t *ed, const bool forceEvenIfSpecialEntity ) {
	// Make sure ed is valid.
	if ( !ed ) {
		gi.error( "%s: ed == (nullptr)\n", __func__ );
		return;
	}

	// Already freed.
	if ( !ed->inUse ) {
		return;
	}

	// Unlink it from the world.
	gi.unlinkentity( ed );

	// If never freed is set, we don't free it, just only unlinked it.
	if ( ed->neverFreeOnlyUnlink == true ) {
		return;
	}

	// Get its number and validate it.
	const int32_t edictNumber = ed->s.number;

	// Validate.
	if ( !forceEvenIfSpecialEntity && ( edictNumber /*ed - g_edicts*/ ) <= ( maxclients->value + BODY_QUEUE_SIZE ) ) {
		#ifdef _DEBUG
		gi.dprintf( "tried to free special edict(#%d) within special edict range(%d)\n",
			edictNumber, maxclients->value + BODY_QUEUE_SIZE );
		#endif
		return;
	}

	// Get its spawn_count id and increment it. (Used for differentiation checks.)
	int32_t nextSpawnCount = ed->spawn_count + 1;

	// We actually got to make sure that we free the pushmover curve positions data block.
	if ( ed->pushMoveInfo.curve.positions.ptr ) {
		ed->pushMoveInfo.curve.positions.release();
	}
	// Clear the arguments std::vector just to be sure.
	ed->delayed.signalOut.arguments.clear();

	// Reset the entity to baseline values.
	ed->Reset( ed->entityDictionary != nullptr );

	// Reinitialize string members to a fresh empty tag to avoid double free on later destruction.
	ed->classname = svg_level_qstring_t::from_char_str( "" );
	ed->classname_hash = 0;
	ed->model = nullptr;

	// Setup the edict as a freed entity.
	// (s.number is preserved by Reset, but we ensure it anyway)
	ed->s.number = edictNumber;
	ed->freetime = level.time;
	ed->inUse = false;
	ed->owner = nullptr;
	ed->spawn_count = nextSpawnCount;

	// Push it onto the free list for future reuse.
	PushFree( ed );
}

/**
*   @brief  Either finds a free edict, or allocates a new one.
*   @remark This function tries to avoid reusing an entity that was recently freed,
*           because it can cause the client to think the entity morphed into something
*           else instead of being removed and recreated, which can cause interpolated
*           angles and bad trails.
**/
svg_base_edict_t *svg_edict_pool_t::AllocateNextFreeEdict( EdictTypeInfo *typeInfo, const cm_entity_t *cm_entity, const char *classnameOverRuler ) {
	svg_base_edict_t *entity = nullptr;

	// Search free list for matching type
	for ( auto it = freeEdicts.begin(); it != freeEdicts.end(); ++it ) {
		if ( (*it)->GetTypeInfo() == typeInfo ) {
			entity = *it;
			freeEdicts.erase(it);
			break;
		}
	}

	if ( !entity ) {
		// No matching entity in free list. Either pick from free list and reallocate, or pick from end of active pool.
		// Usually we pick from end of pool if we haven't reached maxentities.
		if ( num_edicts < game.maxentities ) {
			// Get the next pre-allocated slot.
			int32_t slot = num_edicts++;
			entity = edicts[ slot ];
			
			// If the existing entity is a different type than requested, we must replace it.
			if ( entity && entity->GetTypeInfo() != typeInfo ) {
				int32_t spawn_count = entity->spawn_count;
				delete entity;
				entity = edicts[ slot ] = typeInfo->allocateEdictInstanceCallback( cm_entity );
				entity->s.number = slot;
				entity->spawn_count = spawn_count;
			}
		} else {
			// Pool is completely full. Try to recycle a non-matching freed entity.
			if ( !freeEdicts.empty() ) {
				entity = freeEdicts.back();
				freeEdicts.pop_back();
				
				int32_t slot = entity->s.number;
				int32_t spawn_count = entity->spawn_count;
				delete entity;
				entity = edicts[ slot ] = typeInfo->allocateEdictInstanceCallback( cm_entity );
				entity->s.number = slot;
				entity->spawn_count = spawn_count;
			} else {
				gi.error( "SVG_AllocateEdict: no free edicts" );
				return nullptr;
			}
		}
	}

	// Make sure it is set to 'inUse'.
	if ( !entity ) {
		gi.error( "(nullptr) entity in %s", __FUNCTION__ );
		return nullptr;
	}
	entity->inUse = true;
	entity->classname = svg_level_qstring_t::from_char_str( classnameOverRuler ? classnameOverRuler : typeInfo->worldSpawnClassName );
	entity->classname_hash = std::hash<std::string>{}( classnameOverRuler ? classnameOverRuler : typeInfo->worldSpawnClassName );
	entity->gravity = 1.0f;
	// s.number is already set.
	entity->owner = nullptr;
	entity->s.ownerNumber = ENTITYNUM_NONE;
	entity->s.entityType = ET_GENERIC;
	entity->gravityVector = QM_Vector3Gravity();

	return entity;
}



/**
* 
* 
* 
*	Edict Pool Functionality:
* 
* 
* 
**/
/**
*   @brief	Frees any previously allocated edicts in the pool.
**/
svg_base_edict_t **SVG_EdictPool_Release( svg_edict_pool_t *edictPool ) {
	// Need a valid pool to deal with.
	if ( !edictPool ) {
		return nullptr;
	}

	// Check if the edict pool is valid and is already populated by edicts.
	if ( edictPool->edicts != nullptr ) {
		// Free any previously allocated edicts.
		// NOTE: maxentities could have changed, but max_edicts tracks the allocated array size.
		for ( int32_t i = 0; i < edictPool->max_edicts; i++ ) {
			if ( edictPool->edicts[ i ] != nullptr ) {
				// Ensure it is unlinked.
				gi.unlinkentity( edictPool->edicts[ i ] );
				// Delete the object memory to prevent leaks.
				delete edictPool->edicts[ i ];
				edictPool->edicts[ i ] = nullptr;
			}
		}

		edictPool->freeEdicts.clear();

		// Free the actual array itself.
		gi.TagFree( edictPool->edicts );
		edictPool->edicts = nullptr;
		edictPool->num_edicts = 0;
		edictPool->max_edicts = 0;
	}

	return edictPool->edicts;
}

/**
*   @brief  (Re-)initializes the edict pool.
*   @param  edictPool The edict pool to be initialized.
*   @param  numReservedEntities The number of reserved MAXIMUM entities to be allocated.
*	@return	A pointer to the pool's allocated edict array.
**/
svg_base_edict_t **SVG_EdictPool_Allocate( svg_edict_pool_t *edictPool, const int32_t numReservedEntities ) {
	// Need a valid pool to deal with.
	if ( !edictPool ) {
		gi.error( "%s: edictPool == (nullptr)\n", __func__ );
		return nullptr;
	}

	// Release any previously allocated edicts to prevent memory leaks and double-free crashes.
	SVG_EdictPool_Release( edictPool );

	int32_t allocCount = numReservedEntities;
	// Clamp to MAX_EDICTS and assert
	if (allocCount > MAX_EDICTS) { Q_assert(allocCount <= MAX_EDICTS); allocCount = MAX_EDICTS; }

	edictPool->edicts = (svg_base_edict_t **)gi.TagMallocz( allocCount * sizeof( svg_base_edict_t * ), TAG_SVGAME_EDICTS );
	edictPool->num_edicts = 0;
	// Store the maximum number of reserved entities.
	edictPool->max_edicts = allocCount;

	// Initialize objects for worldspawn and clients.
	for ( int32_t i = 0; i < allocCount; i++ ) {
		// If edict number == 0, it is the worldspawn entity.
		if ( i == 0 ) {
			EdictTypeInfo *typeInfo = EdictTypeInfo::GetInfoByWorldSpawnClassName( "worldspawn" );
			svg_base_edict_t *spawnEdict = edictPool->edicts[ i ] = typeInfo->allocateEdictInstanceCallback( nullptr );
			spawnEdict->s.number = i;
			spawnEdict->classname = svg_level_qstring_t::from_char_str( typeInfo->worldSpawnClassName );
			spawnEdict->classname_hash = std::hash<std::string>{}( typeInfo->worldSpawnClassName );
			edictPool->num_edicts++;
		// And if edict number is within the range of maxclients, it is a player entity.
		} else if ( i >= 1 && i < game.maxclients + 1 ) {
			EdictTypeInfo *typeInfo = EdictTypeInfo::GetInfoByWorldSpawnClassName( "player" );
			svg_base_edict_t *spawnEdict = edictPool->edicts[ i ] = typeInfo->allocateEdictInstanceCallback( nullptr );
			spawnEdict->s.number = i;
			spawnEdict->classname = svg_level_qstring_t::from_char_str( typeInfo->worldSpawnClassName );
			spawnEdict->classname_hash = std::hash<std::string>{}( typeInfo->worldSpawnClassName );
			edictPool->num_edicts++;
		// Otherwise, it is a generic entity pre-allocated for legacy safety.
		} else {
			EdictTypeInfo *typeInfo = EdictTypeInfo::GetInfoByWorldSpawnClassName( "svg_base_edict_t" );
			svg_base_edict_t *spawnEdict = edictPool->edicts[ i ] = typeInfo->allocateEdictInstanceCallback( nullptr );
			spawnEdict->s.number = i;
			spawnEdict->classname = svg_level_qstring_t::from_char_str( "freed" );
			spawnEdict->classname_hash = std::hash<std::string>{}( "freed" );
			spawnEdict->inUse = false;
		}
	}
	// Store the maximum number of reserved entities.
	edictPool->max_edicts = allocCount;

	return edictPool->edicts;
}
