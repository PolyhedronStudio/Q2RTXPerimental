/********************************************************************
*
*
*	SVGame: Edicts Functionalities:
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_edict_pool.h"



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
svg_edict_t *svg_edict_pool_t::operator[]( size_t index ) {
	return ( index >= 0 && index < MAX_EDICTS && edicts != nullptr ? &edicts[ index ] : nullptr );
}

/**
*	@brief	Returns a pointer to the edict matching the number.
*	@return	The edict for the given number. (nullptr if out of range).
**/
svg_edict_t *svg_edict_pool_t::EdictForNumber( const int32_t number ) {
	// Ensure edicts is a valid ptr.
	if ( !edicts ) {
		return nullptr;
	}
	// Check if the number is within range.
	if ( number < 0 || number >= max_edicts ) {
		return nullptr;
	}
	// Return the edict at the given number.
	return &edicts[ number ];
}

/**
*	@brief		Returns the slot index number for a given edict.
*	@param		edict A pointer to the edict whose slot index number is to be determined.
*	@return		The slot index number of the given edict, or -1 if the edict is out of range.
**/
const int32_t svg_edict_pool_t::NumberForEdict( const svg_edict_t *edict ) {
	// Ensure the pool is there.
	//if ( globals.edictPool == nullptr ) {
	//	return -1;
	//}
	// Ensure edicts is a valid ptr.
	if ( edicts == nullptr ) {
		return -1;
	}
	// Check if the edict is within range.
	if ( edict < edicts || edict >= &edicts[ max_edicts ] ) {
		return -1;
	}
	// Return the number of the given edict.
	return edict - edicts;
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
*   @brief  (Re-)initializes the edict pool.
*   @param  edictPool The edict pool to be initialized.
*   @param  numReservedEntities The number of reserved MAXIMUM entities to be allocated.
*	@return	A pointer to the pool's allocated edict array.
**/
svg_edict_t *SVG_EdictPool_Reallocate( svg_edict_pool_t *edictPool, const int32_t numReservedEntities ) {
	//// If it had a previous edicts pointer, free the edicts array and nullify the ptr.
	//if ( globals.edictPool->edicts != nullptr ) {
	//	// Clear out the old edict pool.
	//	gi.TagFree( globals.edictPool->edicts );
	//	// Unset the pointer to the old edict array.
	//	globals.edictPool->edicts = nullptr;
	//}
	// Allocate the edict pool.
	//globals.edictPool = (svg_edict_pool_t *)gi.TagMalloc( sizeof( *globals.edictPool ), TAG_SVGAME );
	// Use in-place constructing.
	//new( globals.edictPool ) svg_edict_pool_t();
	
	// Reset edict pool.
	*edictPool = svg_edict_pool_t();
	
	// Perform allocation, and set the pointer to the new edict array.
	edictPool->edicts = (svg_edict_t *)gi.TagMalloc( numReservedEntities * sizeof( edictPool->edicts[ 0 ] ), TAG_SVGAME );
	// Initialize objects.
	for ( int32_t i = 0; i < numReservedEntities; i++ ) {
		// Use in-place constructing.
		new( &edictPool->edicts[ i ] ) svg_edict_t();
		// Set the number to the current index.
		edictPool->edicts[ i ].s.number = i;
	}
	// Store the maximum number of reserved entities.
	edictPool->max_edicts = numReservedEntities;

	return edictPool->edicts;
}