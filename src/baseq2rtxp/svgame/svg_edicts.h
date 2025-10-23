/********************************************************************
*
*
*	ServerGame: Edicts Entity Data
*	NameSpace: "".
*
*
********************************************************************/
#pragma once


#include "sharedgame/sg_entity_events.h"
#include "sharedgame/sg_entity_types.h"



/**
*
*
*
*   Edict Init/Alloc/Free:
*
*
*
**/
/**
*   @brief  (Re-)initialize an edict.
**/
void SVG_InitEdict( svg_base_edict_t *e, const int32_t stateNumber );
#if 0
/**
*   @brief  Either finds a free edict, or allocates a new one.
*   @remark This function tries to avoid reusing an entity that was recently freed,
*           because it can cause the client to think the entity morphed into something
*           else instead of being removed and recreated, which can cause interpolated
*           angles and bad trails.
**/
svg_base_edict_t *SVG_AllocateEdict( void );
#endif

/**
*   @brief  Marks the edict as free
**/
DECLARE_GLOBAL_CALLBACK_THINK( SVG_FreeEdict );


/**
*
*
*
*   The Player(A client) Entity Body Queue, simply a que of dead body entities:
*
*
*
**/
/**
*   @brief
**/
void SVG_Entities_InitBodyQue( void );
/**
*   @brief
**/
void SVG_Entities_BodyQueueAddForPlayer( svg_base_edict_t *ent );



/**
*
*
*
*   Entity Field Finding:
*
*
*
**/
/**
*   @brief  Searches all active entities for the next one that holds
*           the matching string at fieldofs (use the FOFS_GENTITY() macro) in the structure.
*
*   @remark Searches beginning at the edict after from, or the beginning if NULL
*           NULL will be returned if the end of the list is reached.
**/
svg_base_edict_t *SVG_Entities_Find( svg_base_edict_t *from, const int32_t fieldofs, const char *match ); // WID: C++20: Added const.
/**
*   @brief  Similar to SVG_Entities_Find, but, returns entities that have origins within a spherical area.
**/
svg_base_edict_t *SVG_Entities_FindWithinRadius( svg_base_edict_t *from, const Vector3 &org, const float rad );



/**
*
*
*
*   Entity (State-)Testing:
*
*
*
**/
/**
*   @brief  Returns true if, ent != nullptr, ent->inuse == true.
**/
static inline const bool SVG_Entity_IsActive( const svg_base_edict_t *ent ) {
    // nullptr:
    if ( !ent ) {
        return false;
    }
    // Inactive:
    if ( !ent->inuse ) {
        return false;
    }
    // Active.
    return true;
}
/**
*   @brief  Returns true if the entity is active and has a client assigned to it.
**/
static inline const bool SVG_Entity_IsClient( const svg_base_edict_t *ent, const bool healthCheck = false ) {
    // Inactive Entity:
    if ( !SVG_Entity_IsActive( ent ) ) {
        return false;
    }
    // No Client:
    if ( !ent->client ) {
        return false;
    }

    // Health Check, require > 0
    if ( healthCheck && ent->health <= 0 ) {
        return false;
    }
    // Has Client.
    return true;
}
/**
*   @brief  Returns true if the entity is active and is an actual monster.
**/
static inline const bool SVG_Entity_IsMonster( const svg_base_edict_t *ent/*, const bool healthCheck = false */ ) {
    // Inactive Entity:
    if ( !SVG_Entity_IsActive( ent ) ) {
        return false;
    }
    // Monster entity:
    if ( ( ent->svflags & SVF_MONSTER ) || ent->s.entityType == ET_MONSTER || ent->s.entityType == ET_MONSTER_CORPSE ) {
        return true;
    }
    // No monster entity.
    return false;
}
/**
*   @brief  Returns true if the entity is active(optional, true by default) and has a luaName set to it.
*   @param  mustBeActive    If true, it will also check for whether the entity is an active entity.
**/
static inline const bool SVG_Entity_IsValidLuaEntity( const svg_base_edict_t *ent, const bool mustBeActive = true ) {
    // Inactive Entity:
    if ( mustBeActive && !SVG_Entity_IsActive( ent ) ) {
        return false;
    }
    // Has a luaName set:
    if ( ent && ent->luaProperties.luaName ) {
        return true;
    }
    // No Lua Entity.
    return false;
}



/**
*
*
*
*   Entity SpawnFlags:
*
*
*
**/
/**
*   @brief  Returns true if the entity has specified spawnFlags set.
**/
static inline const bool SVG_HasSpawnFlags( const svg_base_edict_t *ent, const int32_t spawnFlags ) {
    return ( ent->spawnflags & spawnFlags ) != 0;
}



/**
*
*
*
*   UseTarget Functionality:
*
*
*
**/
/**
*   @brief  Find the matching information for the ID and assign it to the entity's useTarget.hintInfo.
**/
void SVG_Entity_SetUseTargetHintByID( svg_base_edict_t *ent, const int32_t id );
/**
*   @brief  Returns true if the entity has the specified useTarget flags set.
**/
static inline const bool SVG_Entity_HasUseTargetFlags( const svg_base_edict_t *ent, const entity_usetarget_flags_t useTargetFlags ) {
    return ( ( ent->useTarget.flags & useTargetFlags ) ) != 0 ? true : false;
}
/**
*   @brief  Returns true if the entity has the specified useTarget flags set.
**/
static inline const bool SVG_Entity_HasUseTargetState( const svg_base_edict_t *ent, const entity_usetarget_state_t useTargetState ) {
    return ( ent->useTarget.state & useTargetState ) != 0 ? true : false;
}



/**
*
*
*
*   Visibility Testing:
*
*
*
**/
/**
*   @brief  Tests for visibility even if the entity is visible to self, and also if not 'infront'.
*           The testing is done by a trace line (self->origin + self->viewheight) to (other->origin + other->viewheight),
*           which if hits nothing, means the entity is visible.
*   @return True if the entity 'other' is visible to 'self'.
**/
const bool SVG_Entity_IsVisible( svg_base_edict_t *self, svg_base_edict_t *other );
/**
*   @return True if the entity is in front (in sight) of self
**/
const bool SVG_Entity_IsInFrontOf( svg_base_edict_t *self, svg_base_edict_t *other, const float dotRangeArea = 3.0f );
/**
*   @return True if the testOrigin point is in front of entity 'self'.
**/
const bool SVG_Entity_IsInFrontOf( svg_base_edict_t *self, const Vector3 &testOrigin, const float dotRangeArea = 3.0f );
