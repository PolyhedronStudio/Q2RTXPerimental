/********************************************************************
*
*
*	SVGame: Edicts Functionalities:
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_clients.h"
#include "svgame/svg_trigger.h"
#include "svgame/svg_signalio.h"
#include "svgame/svg_usetargets.h"

#include "sharedgame/sg_usetarget_hints.h"


//! Exact client-array ownership survives game-local clearing and records the constructed element count.
static svg_client_t *s_client_storage = nullptr;
//! Constructed count belongs to the allocation, independent of a later maxclients cvar change.
static int32_t s_client_storage_count = 0;

/**
*	@brief	Destroy the currently owned client array before its tag or map-dependent references retire.
*	@note	Call before bulk TAG_SVGAME release; never infer the old count from newly changed game settings.
**/
void SVG_Clients_Release( void ) {
	/**
	*	An absent allocation makes repeated map/shutdown retirement harmless.
	**/
	if ( s_client_storage == nullptr ) {
		return;
	}
	// Clear the published alias before destructors release the owned storage.
	if ( game.clients == s_client_storage ) {
		game.clients = nullptr;
	}
	/**
	*	Destroy exactly the objects constructed for this allocation, then free its zone block.
	**/
	for ( int32_t index = 0; index < s_client_storage_count; ++index ) {
		s_client_storage[ index ].~svg_client_t();
	}
	gi.TagFree( s_client_storage );
	s_client_storage = nullptr;
	s_client_storage_count = 0;
}

/**
*	@brief	Replace the owned client array with the requested normally initialized clients.
*	@param	maxClients	Bounded number of client objects to construct.
*	@return	New zone-owned array, or nullptr for the supported empty client population.
*	@note	The old array is destroyed first; initialization retains the existing fresh-array behavior.
**/
svg_client_t *SVG_Clients_Reallocate( const int32_t maxClients ) {
	/**
	*	Validate capacity before retiring the old owner or computing its allocation size.
	**/
	if ( maxClients < 0 || maxClients > MAX_CLIENTS ) {
		gi.error( "%s: invalid client count %" PRId32, __func__, maxClients );
		return nullptr;
	}
	SVG_Clients_Release();
	// A zero-client game owns no client allocation.
	if ( maxClients == 0 ) {
		return nullptr;
	}

	/**
	*	Construct the new bounded array and publish its ownership only after initialization.
	**/
	const uint32_t bytes = static_cast<uint32_t>( static_cast<size_t>( maxClients ) * sizeof( svg_client_t ) );
	auto *clients = static_cast<svg_client_t *>( gi.TagMallocz( bytes, TAG_SVGAME ) );
	// Placement construction retains normal defaults and one stable client number per object.
	for ( int32_t index = 0; index < maxClients; ++index ) {
		new( &clients[ index ] ) svg_client_t();
		clients[ index ].clientNum = index;
	}
	s_client_storage = clients;
	s_client_storage_count = maxClients;
	return clients;
}
