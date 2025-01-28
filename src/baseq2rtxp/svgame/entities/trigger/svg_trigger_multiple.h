/********************************************************************
*
*
*	ServerGame: Trigger Entity 'svg_trigger_multiple'.
*
*
********************************************************************/
#pragma once



/**
*	@brief	The wait time has passed, so set back up for another activation
**/
void multi_wait( edict_t *ent );

/**
*	@brief	The trigger was just activated
*			ent->activator should be set to the activator so it can be held through a delay
*			so wait for the delay time before firing
**/
void multi_trigger( edict_t *ent );