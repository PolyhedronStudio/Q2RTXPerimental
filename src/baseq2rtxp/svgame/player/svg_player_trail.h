/********************************************************************
*
*
*	ServerGame: Player Trail for Monster Navigation.
*	NameSpace: "".
*
*
********************************************************************/
#pragma once



/**
*   @brief
**/
void PlayerTrail_Init( void );
/**
*   @brief
**/
void PlayerTrail_Add( const Vector3 &spot );
/**
*   @brief
**/
void PlayerTrail_New( const Vector3 &spot );
/**
*   @brief
**/
svg_base_edict_t *PlayerTrail_PickFirst( svg_base_edict_t *self );
/**
*   @brief
**/
svg_base_edict_t *PlayerTrail_PickNext( svg_base_edict_t *self );
/**
*   @brief
**/
svg_base_edict_t *PlayerTrail_LastSpot( void );