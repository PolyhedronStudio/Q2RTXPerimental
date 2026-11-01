/********************************************************************
*
*
*	ServerGame: Game Client Functions
*	NameSpace: "".
*
*
********************************************************************/
#pragma once




/**
*
*
*
*   Client Functions:
*
*
*
**/
/**
*   @brief  Allocates a new array of clients, and returns a pointer to it.
**/
svg_client_t *SVG_Clients_Reallocate( const int32_t maxClients );
/**
*	@brief	Destroy the owned client array before bulk game-tag or old-level retirement.
*	@note	Uses the allocation's constructed count rather than the current maxclients setting.
**/
void SVG_Clients_Release( void );
