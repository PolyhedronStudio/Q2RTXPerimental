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
void Think_SpawnDoorTrigger( svg_entity_t *ent );
/**
*	@brief
**/
void Touch_DoorTrigger( svg_entity_t *self, svg_entity_t *other, cplane_t *plane, csurface_t *surf );