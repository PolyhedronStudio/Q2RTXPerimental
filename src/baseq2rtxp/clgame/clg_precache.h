/********************************************************************
*
*
*	ClientGame: Effects Header
*
*
********************************************************************/
#pragma once



/**
*	@brief	Called right after the client finished loading all received configstring (server-) models.
**/
void PF_PrecacheClientModels( void );
/**
*	@brief	Called right after the client finished loading all received configstring (server-) sounds.
**/
void PF_PrecacheClientSounds( void );
/**
*   @brief  Called to precache/update precache of 'View'-models. (Mainly, weapons.)
**/
void PF_PrecacheViewModels( void );
/**
*	@brief	Called to precache client info specific media.
**/
void PF_PrecacheClientInfo( clientinfo_t *ci, const char *s );

/**
*   @brief  Registers a model for local entity usage.
*   @return -1 on failure, otherwise a handle to the model index of the precache.local_models array.
**/
const qhandle_t CLG_RegisterLocalModel( const char *name );
/**
*   @brief  Registers a sound for local entity usage.
*   @return -1 on failure, otherwise a handle to the sounds index of the precache.local_sounds array.
**/
const qhandle_t CLG_RegisterLocalSound( const char *name );

/**
*   @brief  Used by PF_ClearState.
**/
void CLG_Precache_ClearState();