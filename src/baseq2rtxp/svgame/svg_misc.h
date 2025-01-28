/********************************************************************
*
*
*	ServerGame: Misc.
*
********************************************************************/
#pragma once



/**
*   @brief	Returns a random velocity matching the specified damage count.
**/
const Vector3 SVG_Misc_VelocityForDamage( const int32_t damage );

/**
*   @brief
**/
void SVG_Misc_ThrowGib( edict_t *self, const char *gibname, const int32_t damage, const int32_t type );
/**
*   @brief
**/
void SVG_Misc_ThrowHead( edict_t *self, const char *gibname, const int32_t damage, const int32_t type );
/**
*   @brief
**/
void SVG_Misc_ThrowClientHead( edict_t *self, const int32_t damage );
/**
*   @brief
**/
void SVG_Misc_ThrowGib( edict_t *self, const char *gibname, const int32_t damage, const int32_t type );
/**
*   @brief	
**/
void SVG_Misc_ThrowDebris( edict_t *self, const char *modelname, const float speed, vec3_t origin );



/***
*
*
*
*   Explosions:
*
*
*
***/
/**
*   @brief	Spawns a temp entity explosion effect at the entity's origin, and frees the entity.
**/
void SVG_Misc_BecomeExplosion1( edict_t *self );
/**
*   @brief	Spawns a temp entity explosion effect at the entity's origin, and frees the entity.
**/
void SVG_Misc_BecomeExplosion2( edict_t *self );