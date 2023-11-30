
#pragma once

/*
==============================================================

PLAYER MOVEMENT CODE

Common between server and client so prediction matches

==============================================================
*/
/**
*	@brief	Shard Game Player Movement code implementation:
**/
void SG_PlayerMove( pmove_t *pmove, pmoveParams_t *params );
/**
*	@brief	Useful to in the future, configure the player move parameters to game specific scenarions.
**/
void SG_ConfigurePlayerMoveParameters( pmoveParams_t *pmp );
