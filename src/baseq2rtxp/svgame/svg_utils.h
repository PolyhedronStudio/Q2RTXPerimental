/********************************************************************
*
*
*	ServerGame: General Utility Functions:
*
*
********************************************************************/
#pragma once


/**
*   @brief  Wraps up the new more modern SVG_Util_ProjectSource.
**/
void SVG_Util_ProjectSource( const vec3_t point, const vec3_t distance, const vec3_t forward, const vec3_t right, vec3_t result );
/**
*   @brief  Project vector from source.
**/
const Vector3 SVG_Util_ProjectSource( const Vector3 &point, const Vector3 &distance, const Vector3 &forward, const Vector3 &right );

/**
*   @brief
**/
void SVG_Util_SetMoveDir( vec3_t angles, Vector3 &movedir );

/**
*   @brief
**/
void SVG_Util_TouchTriggers( svg_entity_t *ent );
/**
*   @brief  Scan for projectiles between our movement positions
*           to see if we need to collide against them.
**/
void SVG_Util_TouchProjectiles( svg_entity_t *ent, const Vector3 &previous_origin );
/**
*   @brief  Call after linking a new trigger in during gameplay
*           to force all entities it covers to immediately touch it
**/
void SVG_Util_TouchSolids( svg_entity_t *ent );

/**
*	@brief	Basic Trigger initialization mechanism.
**/
void SVG_Util_InitTrigger( svg_entity_t *self );

/**
*   @brief  Kills all entities that would touch the proposed new positioning
*           of ent.  Ent should be unlinked before calling this!
**/
const bool SVG_Util_KillBox( svg_entity_t *ent, const bool bspClipping );



/**
*
*
*
*   MoveWith Functionality:
*
*
*
**/
/**
*   @brief
**/
void SVG_MoveWith_AdjustToParent( const Vector3 &deltaParentOrigin, const Vector3 &deltaParentAngles, const Vector3 &parentVUp, const Vector3 &parentVRight, const Vector3 &parentVForward, svg_entity_t *parentMover, svg_entity_t *childMover );
/**
*   @brief
**/
//void SVG_MoveWith_Init( svg_entity_t *self, svg_entity_t *parent );
/**
*   @brief
**/
void SVG_MoveWith_SetChildEntityMovement( svg_entity_t *self );
/**
*   @note   At the time of calling, parent entity has to reside in its default state.
*           (This so the actual offsets can be calculated easily.)
**/
void SVG_MoveWith_SetTargetParentEntity( const char *targetName, svg_entity_t *parentMover, svg_entity_t *childMover );
