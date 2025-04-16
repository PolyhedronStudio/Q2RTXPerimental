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
void Think_SpawnDoorTrigger( svg_base_edict_t *ent );
/**
*	@brief
**/
void Touch_DoorTrigger( svg_base_edict_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf );