/*********************************************************************
*
*
*	Server: Init.
*
*
********************************************************************/
#pragma once


/**
*	@brief
**/
void SV_ClientReset( client_t *client );
/**
*	@brief
**/
void SV_SpawnServer( mapcmd_t *cmd );
/**
*	@brief
**/
const bool SV_ParseMapCmd( mapcmd_t *cmd );
/**
*	@brief
**/
void SV_InitGame( void );