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
#pragma once

#ifdef __cplusplus
extern "C" {
#endif // #ifdef __cplusplus

/**
*	@brief	Allocates another pose cache and returns the index handle.
**/
qhandle_t SKM_PoseCache_AllocatePoseCache();
/**
*	@brief	Will clear out the cache list as well as each cache contained inside of it.
**/
void SKM_PoseCache_ClearAllCaches();

/**
*	@brief	Clears the Temporary Bone Cache. Does NOT reset its size to defaults. Memory stays allocated as it was.
*	@note	poseCacheHandle's value will be set to 0.
**/
void SKM_PoseCache_ClearCache( const qhandle_t poseCacheHandle );
/**
*	@brief	Clears, AND resets the Temporary Bone Cache to its default size.
**/
void SKM_PoseCache_ResetCache( qhandle_t *poseCacheHandle );

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
skm_transform_t *SKM_PoseCache_AcquireCachedMemoryBlock( const qhandle_t poseCacheHandle, const uint32_t size );

#ifdef __cplusplus
}
#endif // #ifdef __cplusplus