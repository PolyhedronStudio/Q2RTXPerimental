/********************************************************************
*
*
*	ServerGame: func_ entity utilities.
*
*
********************************************************************/
#pragma once


struct svg_func_door_t;

/**
*	@brief
**/
DECLARE_GLOBAL_CLASSNAME_CALLBACK_SPAWN( svg_func_door_t, DoorTrigger_SpawnThink );
/**
*	@brief
**/
//void DoorTrigger_Touch( svg_base_edict_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf );
DECLARE_GLOBAL_CALLBACK_TOUCH( DoorTrigger_Touch );