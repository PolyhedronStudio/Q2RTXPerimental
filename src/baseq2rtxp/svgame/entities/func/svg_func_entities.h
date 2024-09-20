/********************************************************************
*
*
*	ServerGame: func_ entity utilities.
*
*
********************************************************************/
#pragma once


/**
*	@brief
**/
void Think_SpawnDoorTrigger( edict_t *ent );
/**
*	@brief
**/
void Think_CalcMoveSpeed( edict_t *self );
/**
*	@brief
**/
void Touch_DoorTrigger( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf );