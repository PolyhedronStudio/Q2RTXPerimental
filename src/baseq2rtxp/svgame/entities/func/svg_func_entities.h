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
void Touch_DoorTrigger( svg_entity_t *self, svg_entity_t *other, const cm_plane_t *plane, cm_surface_t *surf );