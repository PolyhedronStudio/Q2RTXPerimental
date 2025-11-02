/********************************************************************
*
*
*	ServerGame: General Utility Functions:
*
*
********************************************************************/
#pragma once



/**
*
*
*
*	Generic Entity Utility Functions:
*
*
*
**/
/**
*	@brief	Basic Trigger initialization mechanism.
**/
void SVG_Util_InitTrigger( svg_base_edict_t *self );

/**
*   @brief	Will set the movedir vector based on the angles.
* 
*	@note	Will clear out the angles if clearAngles is true (default).
* 
*			A value of -1 for any angle will be treated as straight down.
*			A value of -2 for any angle will be treated as straight up.
**/
void SVG_Util_SetMoveDir( Vector3 &angles, Vector3 &movedir, const bool clearAngles = true );

/**
*   @brief  Determines the client that is most near to the entity,
*           and returns its length for ( ent->origin - client->origin ).
**/
const double SVG_Util_ClosestClientForEntity( svg_base_edict_t *ent );



/**
*
*
*
*	(Event-) Entity Utility Functions:
*
*
*
**/
/**
*   @brief  Use for non-pmove events that would also be predicted on the
*           client side: jumppads and item pickups
*           Adds an event+parm and twiddles the event counter
**/
void SVG_Util_AddPredictableEvent( svg_base_edict_t *ent, const sg_entity_events_t event, const int32_t eventParm );
/**
*   @brief Adds an event+parm and twiddles the event counter.
**/
void SVG_Util_AddEvent( svg_base_edict_t *ent, const sg_entity_events_t event, const int32_t eventParm0, const int32_t eventParm1 = 0 );
/**
*   @brief  Adds a temp entity event at the given origin.
*	@param	snapOrigin	If true, will snap the origin to 13 bits float precision.
**/
svg_base_edict_t *SVG_Util_CreateTempEntityEvent( const Vector3 &origin, const sg_entity_events_t event, const int32_t eventParm0, const int32_t eventParm1, const bool snapOrigin = false );



/**
*
*
*
*	Trace Utility Functions:
*
*
*
**/
/**
*	@brief	Wrapper for gi.trace that accepts Vector3 args.
**/
static inline const svg_trace_t SVG_Trace( const Vector3 &start, const Vector3 &mins, const Vector3 &maxs, const Vector3 &end, svg_base_edict_t *passEdict, const cm_contents_t contentMask ) {
	const Vector3 *_mins = ( ( &mins != &qm_vector3_null || mins != qm_vector3_null || mins != vec3_origin ) ? &mins : nullptr );
	const Vector3 *_maxs = ( ( &maxs != &qm_vector3_null || maxs != qm_vector3_null || maxs != vec3_origin ) ? &maxs : nullptr );
	const Vector3 *_start = ( ( &start != &qm_vector3_null || start != qm_vector3_null || start != vec3_origin ) ? &start : nullptr );
	const Vector3 *_end = ( ( &end != &qm_vector3_null || end != qm_vector3_null || end != vec3_origin ) ? &end : nullptr );
	return gi.trace( _start, _mins, _maxs, _end, passEdict, contentMask );
}
/**
*	@brief	Wrapper for gi.clipthat accepts Vector3 args.
**/
static inline const svg_trace_t SVG_Clip( svg_base_edict_t *clipEdict, const Vector3 &start, const Vector3 &mins, const Vector3 &maxs, const Vector3 &end, const cm_contents_t contentMask ) {
	const Vector3 *_mins = ( ( &mins != &qm_vector3_null || mins != qm_vector3_null || mins != vec3_origin ) ? &mins : nullptr );
	const Vector3 *_maxs = ( ( &maxs != &qm_vector3_null || maxs != qm_vector3_null || maxs != vec3_origin ) ? &maxs : nullptr );
	const Vector3 *_start = ( ( &start != &qm_vector3_null || start != qm_vector3_null || start != vec3_origin ) ? &start : nullptr );
	const Vector3 *_end = ( ( &end != &qm_vector3_null || end != qm_vector3_null || end != vec3_origin ) ? &end : nullptr );
	return gi.clip( clipEdict, _start, _mins, _maxs, _end, contentMask );
}



/**
*
*
*
*	Mathemathical Utility Functions:
*		- Vector Projection from Source
*		- Set Move Direction from Angles
*
*
*
**/
/**
*   @brief  Project vector from a source point, to distance, based on forward/right angle vectors.
**/
const Vector3 SVG_Util_ProjectSource( const Vector3 &point, const Vector3 &distance, const Vector3 &forward, const Vector3 &right );
/**
*   @brief  Utility version of above that uses vec3_t args and result.
**/
void SVG_Util_ProjectSource( const vec3_t point, const vec3_t distance, const vec3_t forward, const vec3_t right, vec3_t result );



/**
*
*
*
*	'Touch' Utility Functions:
*
*
*
**/
/**
*   @brief
**/
void SVG_Util_TouchTriggers( svg_base_edict_t *ent );
/**
*   @brief  Scan for projectiles between our movement positions
*           to see if we need to collide against them.
**/
void SVG_Util_TouchProjectiles( svg_base_edict_t *ent, const Vector3 &previous_origin );
/**
*   @brief  Call after linking a new trigger in during gameplay
*           to force all entities it covers to immediately touch it
**/
void SVG_Util_TouchSolids( svg_base_edict_t *ent );

/**
*   @brief  Kills all entities that would touch the proposed new positioning
*           of ent.  Ent should be unlinked before calling this!
**/
const bool SVG_Util_KillBox( svg_base_edict_t *ent, const bool bspClipping, sg_means_of_death_t meansOfDeath );



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
void SVG_MoveWith_AdjustToParent( const Vector3 &deltaParentOrigin, const Vector3 &deltaParentAngles, const Vector3 &parentVUp, const Vector3 &parentVRight, const Vector3 &parentVForward, svg_base_edict_t *parentMover, svg_base_edict_t *childMover );
/**
*   @brief
**/
//void SVG_MoveWith_Init( svg_base_edict_t *self, svg_base_edict_t *parent );
/**
*   @brief
**/
void SVG_MoveWith_SetChildEntityMovement( svg_base_edict_t *self );
/**
*   @note   At the time of calling, parent entity has to reside in its default state.
*           (This so the actual offsets can be calculated easily.)
**/
void SVG_MoveWith_SetTargetParentEntity( const char *targetName, svg_base_edict_t *parentMover, svg_base_edict_t *childMover );
