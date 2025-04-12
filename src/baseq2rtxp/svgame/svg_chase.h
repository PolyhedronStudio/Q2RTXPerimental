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
void SVG_ChaseCam_Update( svg_edict_t *ent );
/**
*   @brief
**/
void SVG_ChaseCam_Next( svg_edict_t *ent );
/**
*   @brief
**/
void SVG_ChaseCam_Previous( svg_edict_t *ent );
/**
*   @brief
**/
void SVG_ChaseCam_GetTarget( svg_edict_t *ent );