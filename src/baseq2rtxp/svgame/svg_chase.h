/*********************************************************************
*
*
*	SVGame: Spectator Chase Camera:
*
*
********************************************************************/
#pragma once



/**
*
*
*
*   ChaseCam:
*
*
*
**/
/**
*   @brief
**/
void SVG_ChaseCam_Update( edict_t *ent );
/**
*   @brief
**/
void SVG_ChaseCam_Next( edict_t *ent );
/**
*   @brief
**/
void SVG_ChaseCam_Previous( edict_t *ent );
/**
*   @brief
**/
void SVG_ChaseCam_GetTarget( edict_t *ent );