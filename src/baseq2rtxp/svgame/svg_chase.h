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
void SVG_ChaseCam_Update( svg_base_edict_t *ent );
/**
*   @brief
**/
void SVG_ChaseCam_Next( svg_base_edict_t *ent );
/**
*   @brief
**/
void SVG_ChaseCam_Previous( svg_base_edict_t *ent );
/**
*   @brief
**/
void SVG_ChaseCam_GetTarget( svg_base_edict_t *ent );