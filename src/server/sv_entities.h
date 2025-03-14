/*********************************************************************
*
*
*	Server: Entities.
*
*
********************************************************************/
#pragma once

/**
*	@brief
**/
static inline const bool ES_INUSE( entity_state_t *s ) {
	return ( ( s )->modelindex || ( s )->effects || ( s )->sound || ( s )->event );
}

/**
*	@brief
**/
void SV_BuildClientFrame( client_t *client );
/**
*	@brief
**/
void SV_WriteFrameToClient( client_t *client );