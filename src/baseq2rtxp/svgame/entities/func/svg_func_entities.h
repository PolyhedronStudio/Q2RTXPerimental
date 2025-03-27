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
void Think_SpawnDoorTrigger( svg_edict_t *ent );
/**
*	@brief
**/
void Touch_DoorTrigger( svg_edict_t *self, svg_edict_t *other, cplane_t *plane, csurface_t *surf );