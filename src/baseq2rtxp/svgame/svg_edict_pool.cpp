/********************************************************************
*
*
*	SVGame: Edicts Functionalities:
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_edict_pool.h"
#include "svgame/entities/svg_ed_player.h"



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
		return -1;
	}
	// Ensure edicts is a valid ptr.
	if ( edicts == nullptr ) {
		return -1;
	}
	// Check if the edict is within range.
	if ( edict->s.number < 0 || edict->s.number >= max_edicts ) {
		return -1;
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
void svg_edict_pool_t::FreeEdict( svg_base_edict_t *ed ) {
	// Make sure ed is valid.
	if ( !ed ) {
		gi.error( "%s: ed == (nullptr)\n", __func__ );
		return;
	}

	// Already freed.
	if ( !ed->inuse ) {
		return;
	}

	// Unlink it from the world.
	gi.unlinkentity( ed );

	// Get its number and validate it.
	const int32_t edictNumber = ed->s.number;

	// Validate.
	if ( ( edictNumber /*ed - g_edicts*/ ) <= ( maxclients->value + BODY_QUEUE_SIZE ) ) {
		#ifdef _DEBUG
		gi.dprintf( "tried to free special edict(#%d) within special edict range(%d)\n",
			edictNumber, maxclients->value + BODY_QUEUE_SIZE );
		#endif
		return;
	}

	// Get its spawn_count id and increment it. (Used for differentiation checks.)
	int32_t spawnCount = ed->spawn_count + 1;

	// We actually got to make sure that we free the pushmover curve positions data block.
	if ( ed->pushMoveInfo.curve.positions ) {
		ed->pushMoveInfo.curve.positions.release();
	}
	// Clear the arguments std::vector just to be sure.
	ed->delayed.signalOut.arguments.clear();
	#if 0
	// Delete current in-memory entity object.
	delete ed;
	// Reallocate it as a fresh svg_base_edict_t instead.
	ed = g_edict_pool.edicts[ edictNumber ] = new svg_base_edict_t(); //*ed = {};
	#endif
	// Reset the entity to baseline values.
	ed->Reset();

	// Setup the edict as a freed entity.
	ed->s.number = edictNumber;
	ed->classname = "freed";
	ed->freetime = level.time;
	ed->inuse = false;
	ed->spawn_count = spawnCount;
}

/**
*   @brief  Either finds a free edict, or allocates a new one.
*   @remark This function tries to avoid reusing an entity that was recently freed,
*           because it can cause the client to think the entity morphed into something
*           else instead of being removed and recreated, which can cause interpolated
*           angles and bad trails.
**/
svg_base_edict_t *svg_edict_pool_t::EmplaceNextFreeEdict( svg_base_edict_t *ent ) {
	svg_base_edict_t *entity = nullptr;
	svg_base_edict_t *freedEntity = nullptr;

	// Start after the client slots.
	int32_t i = game.maxclients + 1;

	entity = EdictForNumber( i );

	// Iterate and seek.
	for ( i; i < num_edicts; i++ ) {
		entity = EdictForNumber( i );

		// the first couple seconds of server time can involve a lot of
		// freeing and allocating, so relax the replacement policy
		if ( entity != nullptr && !entity->inuse && ( entity->freetime < 2_sec || level.time - entity->freetime > 500_ms ) ) {
			//_InitEdict<EdictType>( entity, i );
			// Restore the actual number.
			ent->s.number = num_edicts;
			// Make sure it is set to 'inuse'.
			ent->inuse = true;
			// Free the old entity.
			SVG_FreeEdict( entity );
			// Initialize the new one.
			edicts[ num_edicts ] = ent;
			// Return its ptr.
			return /*static_cast<EdictType *>*/( edicts[ i ] );
		}

		// this is going to be our second chance to spawn an entity in case all free
		// entities have been freed only recently
		if ( !freedEntity ) {
			freedEntity = entity;
		}
	}

	// If we reached the maximum number of entities.
	if ( i == game.maxentities ) {
		// If we have a freed entity, use it.
		if ( freedEntity ) {
			//_InitEdict<EdictType>( entity, i );
	// Restore the actual number.
			ent->s.number = i;
			// Make sure it is set to 'inuse'.
			ent->inuse = true;
			// Free the old entity.
			SVG_FreeEdict( freedEntity );
			// Initialize the new one.
			edicts[ i ] = ent;
			// Return its ptr.
			return /*static_cast<EdictType *>*/( edicts[ i ] );
		}
		// If we don't have any free edicts, error out.
		gi.error( "SVG_AllocateEdict: no free edicts" );
	}

	// Initialize it.
	//InitEdict<EdictType>( entity, num_edicts );
			// Free the old entity.
	// 
	//SVG_FreeEdict( entity );

	// Initialize the new one.
	edicts[ num_edicts ] = ent;
	// Restore the actual number.
	ent->s.number = num_edicts;
	// Make sure it is set to 'inuse'.
	ent->inuse = true;
	// If we have free edicts left to go, use those instead.
	num_edicts++;

	return ent; //static_cast<EdictType *>( entity );
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
		gi.error( "%s: edictPool == (nullptr)\n", __func__ );
		return nullptr;
	}

	// Check if the edict pool is valid and is already populated by edicts.
	if ( edictPool->edicts != nullptr ) {
		int32_t i = 0;
		// Free any previously allocated edicts.
		for ( int32_t i = 0; i < edictPool->max_edicts; i++ ) {
			if ( edictPool->edicts[ i ] != nullptr ) {
				// Call upon destructor(if any).
				delete edictPool->edicts[ i ];
				// Set to null.
				edictPool->edicts[ i ] = nullptr;
			}
		}
		
		edictPool->edicts = nullptr;
		edictPool->num_edicts = 0;

		// Free any remainings.
		gi.FreeTags( TAG_SVGAME_EDICTS );
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

	// Perform allocation, and set the pointer to the new edict array.
	if ( edictPool->edicts ) {
		edictPool->edicts = (svg_base_edict_t **)gi.TagMallocz( numReservedEntities * sizeof( svg_base_edict_t * ), TAG_SVGAME_EDICTS );
	} else {
		edictPool->edicts = (svg_base_edict_t **)gi.TagReMalloc( edictPool->edicts, numReservedEntities * sizeof( svg_base_edict_t * ) );//new svg_base_edict_t*[ numReservedEntities ];// 
	}
	// Initialize objects.
	for ( int32_t i = 0; i < numReservedEntities; i++ ) {
		if ( i >= 1 && i < game.maxclients + 1 ) {
			edictPool->edicts[ i ] = new svg_player_edict_t();
			static_cast<svg_player_edict_t *>( edictPool->edicts[ i ] )->testVar = 1337 + i;
		} else {
			edictPool->edicts[ i ] = new svg_base_edict_t();
		}
		// Set the number to the current index.
		edictPool->edicts[ i ]->s.number = i;
	}
	// Store the maximum number of reserved entities.
	edictPool->max_edicts = numReservedEntities;

	return edictPool->edicts;
}
