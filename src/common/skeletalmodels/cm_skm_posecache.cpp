/********************************************************************
*
*
*	Temporary Bone Pose Cache, cached storage space for animation 
*	blending. A buffer that grows in capacity, does not shrink, and
*	can be reused each game/refresh frame.
*
*	Note that bone caches are identified by an opaque qhandle_t type
*	so that we can sanely use this API in the few C only code files
*	that this project deals with.
* 
*	As such, the client, and server, will both have to create their
*	own cache.
*
********************************************************************/
#include "shared/shared.h"
#include "shared/util_list.h"

#include "refresh/shared_types.h"

#include "common/cmd.h"
#include "common/common.h"
#include "common/cvar.h"
#include "common/files.h"
#include "common/math.h"
#include "common/skeletalmodels/cm_skm.h"
#include "common/skeletalmodels/cm_skm_posecache.h"
#include "common/skeletalmodels/cm_skm_configuration.h"

#include "system/hunk.h"

/**
*	@brief	The actual pose cache that is unique to a qhandle_t
**/
typedef struct skm_posecache_s {
	//! The actual cache, a vector of iqm_transform_t.
	std::vector<iqm_transform_t> data;
} skm_posecache_t;

//! The TOTAL maximum amount of temporary bones the cache can reserve during a frame.
//! Currently it is set to the same value as IQM_MAX_MATRICES (Look it up: /src/refresh/vkpt/shader/vertex_buffer.h)
static constexpr int32_t TBC_SIZE_MAX_CACHEBLOCK = 32768;

//! The TOTAL maximum size for each allocated pose block.
static constexpr int32_t TBC_SIZE_MAX_POSEBLOCK = IQM_MAX_JOINTS;

//! The size of each extra reserved bone cache block.
static constexpr int32_t TBC_SIZE_BLOCK_RESERVE = 8192;

//! The actual default size of our cache.
static constexpr int32_t TBC_SIZE_INITIAL = 8192; // Should allow for 32 distinct poses of size IQM_MAX_JOINTS(256).



//! This is the list of actual pose caches.
static std::vector<skm_posecache_t> poseCaches;

//! Serves as a means of working with the std::vector::insert method.
static std::vector<iqm_transform_t> _cleanBonePoses( IQM_MAX_JOINTS );



/**
*	@brief	Returns a pointer to the poseCache matching the handle. (nullptr) on failure.
**/
static inline skm_posecache_t *SKM_PoseCache_GetFromHandle( const qhandle_t poseCacheHandle ) {
	// Invalid handle.
	if ( poseCacheHandle <= 0 || poseCacheHandle > poseCaches.capacity() ) {
		Com_LPrintf( PRINT_WARNING, "%s: poseCacheHandle(#%i) is ( poseCacheHandle <= 0 || poseCacheHandle > poseCaches.capacity() )!\n", __func__, poseCacheHandle );
		return nullptr;
	}

	return &poseCaches[ poseCacheHandle - 1];
}

/**
*	@brief	Allocates another pose cache and returns the index handle.
**/
qhandle_t SKM_PoseCache_AllocatePoseCache() {
	// Push back a zeroed out memory pose cache.
	poseCaches.push_back( {} );
	// Return handle.
	return poseCaches.size();
}

/**
*	@brief	Will clear out the cache list as well as each cache contained inside of it.
**/
void SKM_PoseCache_ClearAllCaches() {
	// Clear out each cache's data.
	for ( int32_t i = 0; i < poseCaches.capacity(); i++ ) {
		poseCaches[ i ].data.clear();
	}
	// Clear out the pose caches.
	poseCaches.clear();
}

/**
*	@brief	Clears the Temporary Bone Cache. Does NOT reset its size to defaults. Memory stays allocated as it was.
*	@note	poseCacheHandle's value will be set to 0.
**/
void SKM_PoseCache_ClearCache( const qhandle_t poseCacheHandle ) {
	// Ignore, but do not error out, on a 0 value for poseCacheHandle.
	// It's 0 when simply none is registered for the client/server.
	if ( poseCacheHandle == 0 ) {
		return;
	}

	// Get cache.
	skm_posecache_t *cache = SKM_PoseCache_GetFromHandle( poseCacheHandle );
	// Return on failure, print a warning.
	if ( !cache ) {
		Com_LPrintf( PRINT_WARNING, "%s: Invalid poseCacheHandle(#%i)!\n", __func__, poseCacheHandle );
		return;
	}

	// Clear the cache.
	cache->data.clear();
}

/**
*	@brief	Clears, AND resets the Temporary Bone Cache to its default size.
**/
void SKM_PoseCache_ResetCache( qhandle_t *poseCacheHandle ) {
	// Get cache.
	skm_posecache_t *cache = SKM_PoseCache_GetFromHandle( *poseCacheHandle );
	// Return on failure, print a warning.
	if ( !cache ) {
		Com_LPrintf( PRINT_WARNING, "%s: Invalid poseCacheHandle(#%i)!\n", __func__, *poseCacheHandle );
		return;
	}

	// Clear the cache.
	cache->data.clear();

	// Reserve actual cache data.
	cache->data.reserve( TBC_SIZE_INITIAL );

	// Ask kindly of this container will shrink to fit. (Implementation dependant.)
	cache->data.shrink_to_fit();
}

/**
*	@brief	See @return for description. The maximum size of an allocated block is hard limited to TBC_SIZE_MAX_POSEBLOCK.
*
*	@return	A pointer to a prepared block of memory in the bone cache. If needed, the bone cache resizes to meet
*			demands. Note that if it returns a nullptr it means that the actual limit of TBC_SIZE_MAX_POSEBLOCK has
*			been reached. It should always be the same value of IQM_MAX_MATRICES, which is defined in:
*			/src/refresh/vkpt/shader/vertex_buffer.h
*
*			If you need a larger temporary bone cache than both IQM_MAX_MATRICES as well as TBC_SIZE_MAX_POSEBLOCK
*			need to be increased in a higher, and equally same number.
**/
iqm_transform_t *SKM_PoseCache_AcquireCachedMemoryBlock( const qhandle_t poseCacheHandle, uint32_t size ) {
	// Get cache.
	skm_posecache_t *cache = SKM_PoseCache_GetFromHandle( poseCacheHandle );
	// Return on failure, print a warning.
	if ( !cache ) {
		Com_LPrintf( PRINT_WARNING, "%s: Invalid poseCacheHandle(#%i)!\n", __func__, poseCacheHandle );
		return nullptr;
	}

	/**
	*	#0: Ensure we can properly allocate this block of memory.
	**/
	const size_t sizeDemand = cache->data.size() + size - 1;
	const size_t currentCapacity = cache->data.capacity();

	// In case the size exceeds MAX_IQM_JOINTS, nullptr.
	if ( size > IQM_MAX_JOINTS ) {
		Com_DPrintf( "if ( size > IQM_MAX_JOINTS ) where size=%i\n", size );
		return nullptr;
	}

	// Reserve extra cache memory if needed.
	if ( sizeDemand > currentCapacity ) {
		// We still need to be careful to never exceed TBC_SIZE_MAX_POSEBLOCK here.
		// If it 
		if ( sizeDemand > TBC_SIZE_MAX_CACHEBLOCK ) {
			// TODO: Oughta warn here, or just bail out altogether.
			Com_DPrintf( "sizeDemand > TBC_SIZE_MAX_CACHEBLOCK where sizeDemand=%i\n", sizeDemand );
			return nullptr;
		}

		// It isn't exceeding TBC_SIZE_MAX_POSEBLOCK, so we can safely reserve another block.
		cache->data.reserve( currentCapacity + TBC_SIZE_BLOCK_RESERVE );
	}

	/**
	*	#1: Insert (actually initialize and allocate memory if it has not done so before.), and return address.
	**/
	// Insert into our container to initialize the needed memory.
	return &( *cache->data.insert( cache->data.end(), _cleanBonePoses.begin(), _cleanBonePoses.begin() + size - 1 ) );
}