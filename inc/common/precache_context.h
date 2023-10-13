//#pragma once 
#ifndef COMMON_PRECACHE_CONTEXT_H
#define COMMON_PRECACHE_CONTEXT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "common/images.h"
#include "common/models.h"

/**
*	during registration it is possible to have more models than could actually
*	be referenced during gameplay, because we don't want to free anything until
*	we are sure we won't need it.
**/
#define PRECACHE_MAX_MODELS		(MAX_MODELS * 2)
#define PRECACHE_MAX_IMAGES		( 2048 ) // Should always be set to and match to the same value as: MAX_RIMAGES.

/**
*	@brief	Used by the client's 'refresh', as well as the server for storing 'pre-cached'
*			image/model/sound data.
**/
typedef struct precache_context_s {
	image_t	images[ PRECACHE_MAX_IMAGES ];
	int		numImages;

	model_t	models[ PRECACHE_MAX_MODELS ];
	int		numModels;

	//! The registration sequence, used to filter out possible unused models leftover from previous map loading.
	int registration_sequence;
} precache_context_t;


/**
*	Models are loaded for both client and server, thus we need to pass along a pointer to the
*	specific precache context we're seeking/(un-)loading into.
**/
model_t *MOD_Alloc( precache_context_t *precacheContext );
model_t *MOD_Find( precache_context_t *precacheContext, const char *name );
model_t *MOD_ForHandle( precache_context_t *precacheContext, qhandle_t h );
void MOD_FreeUnused( precache_context_t *precacheContext );
void MOD_FreeAll( precache_context_t *precacheContext );

#ifdef __cplusplus
};
#endif

#endif // COMMON_PRECACHE_CONTEXT_H