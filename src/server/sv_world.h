/*********************************************************************
*
*
*	Server: World.
*
*
********************************************************************/
#pragma once


/**
* 
* 
* 
*   high level object sorting to reduce interaction tests
* 
* 
* 
**/
/**
*   @brief  Called after the world model has been loaded, before linking any entities.
**/
void SV_ClearWorld( void );

/**
*   @brief  Call before removing an entity, and before trying to move one,
*           so it doesn't clip against itself.
**/
void PF_UnlinkEdict( edict_t *ent );


/**
*   @brief  Needs to be called any time an entity changes origin, mins, maxs,
*           or solid.  Automatically unlinks if needed.
*           sets ent->v.absmin and ent->v.absmax
*           sets ent->leafnums[] for pvs determination even if the entity.
*           is not solid.
**/
void SV_LinkEdict( cm_t *cm, edict_t *ent );
void PF_LinkEdict( edict_t *ent );



/**
*
*
*
*   Actual object "interaction" tests:
*
*
*
**/
/**
*   @brief  fills in a table of edict pointers with edicts that have bounding boxes
*           that intersect the given area.  It is possible for a non-axial bmodel
*           to be returned that doesn't actually intersect the area on an exact
*           test.
*   @todo: Does this always return the world?
*   @return The number of pointers filled in.
**/
const int32_t SV_AreaEdicts( const vec3_t mins, const vec3_t maxs, edict_t **list, const int32_t maxcount, const int32_t areatype );

/**
*	@return	The CONTENTS_* value from the world at the given point.
*			Quake 2 extends this to also check entities, to allow moving liquids
**/
const contents_t SV_PointContents( const vec3_t p );

/**
*	@description	mins and maxs are relative
*
*					if the entire move stays in a solid volume, trace.allsolid will be set,
*					trace.startsolid will be set, and trace.fraction will be 0
*
*					if the starting point is in a solid, it will be allowed to move out
*					to an open area
*
*					passedict is explicitly excluded from clipping checks (normally NULL)
**/
const cm_trace_t q_gameabi SV_Trace( const vec3_t start, const vec3_t mins,
    const vec3_t maxs, const vec3_t end,
    edict_t *passedict, const contents_t contentmask );

/**
*	@brief	Like SV_Trace(), but clip to specified entity only.
*			Can be used to clip to SOLID_TRIGGER by its BSP tree.
**/
const cm_trace_t q_gameabi SV_Clip( edict_t *clip, const vec3_t start, const vec3_t mins,
    const vec3_t maxs, const vec3_t end, const contents_t contentmask );
