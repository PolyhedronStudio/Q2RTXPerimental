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
void PlayerTrail_Add( vec3_t spot );
/**
*   @brief
**/
void PlayerTrail_New( vec3_t spot );
/**
*   @brief
**/
edict_t *PlayerTrail_PickFirst( edict_t *self );
/**
*   @brief
**/
edict_t *PlayerTrail_PickNext( edict_t *self );
/**
*   @brief
**/
edict_t *PlayerTrail_LastSpot( void );