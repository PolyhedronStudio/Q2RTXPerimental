/********************************************************************
*
*
*	ClientGame: Local Entities
*
*
********************************************************************/
/**
*	@return	The casted pointer to the entity's class type.
**/
template<typename T> static inline auto CLG_LocalEntity_GetClass( clg_local_entity_t *self ) -> T * {
	return static_cast<T *>( self->classLocals );
}

/********************************************************************
*
*
*	ClientGame: Local Entities
*
*
********************************************************************/
//#include "clg_local.h"
//#include "local_entities/clg_local_entity_classes.h"

//! Extern the 'client_misc_model' entity class type.
extern const clg_local_entity_class_t client_misc_te;
/**
*
*
*	Local Entities:
*
*
**/




/**
*
*
*	Local Entities - Allocate/Free:
*
*
**/
/**
*	@brief	Will always return a valid pointer to a free local entity slot, and
*			errors out in case of no available slots to begin with.
**/
static clg_local_entity_t *CLG_LocalEntity_Allocate();
/**
*	@brief	Initialize a fresh local entity.
**/
const clg_local_entity_t *CLG_InitLocalEntity( clg_local_entity_t *lent );
/**
*	@brief	Frees up the local entity, increasing spawn_count to differentiate
*			for a new entity using an identical slot, as well as storing the
*			actual freed time. This'll prevent lerp issues etc.
**/
void CLG_LocalEntity_Free( clg_local_entity_t *lent );
/**
*	@brief	Frees all local entities.
**/
void CLG_FreeLocalEntities();



/**
*
*
*	Local Entities - Physics/PVS
*
*
**/
/**
*	@brief	Runs thinking code for this frame if necessary
**/
bool CLG_LocalEntity_RunThink( clg_local_entity_t *lent );
/**
*	@brief
**/
void CLG_LocalEntity_Link( clg_local_entity_t *lent );
/**
*	@brief
**/
void CLG_LocalEntity_Unlink( clg_local_entity_t *lent );



/**
*
*
*	Local Entities - Callback Dispatch Handling:
*
*
**/
/**
*	@brief	Calls the localClass 'Precache' function pointer.
**/
const bool CLG_LocalEntity_DispatchPrecache( clg_local_entity_t *lent, const cm_entity_t *keyValues );
/**
*	@brief	Calls the 'Spawn' function pointer.
**/
const bool CLG_LocalEntity_DispatchSpawn( clg_local_entity_t *lent );
/**
*	@brief	Calls the 'Think' function pointer.
**/
const bool CLG_LocalEntity_DispatchThink( clg_local_entity_t *lent );
/**
*	@brief	Calls the 'RefreshFrame' function pointer.
**/
const bool CLG_LocalEntity_DispatchRefreshFrame( clg_local_entity_t *lent );
/**
*	@brief	Calls the 'PrepareRefreshEntity' function pointer.
**/
const bool CLG_LocalEntity_DispatchPrepareRefreshEntity( clg_local_entity_t *lent );



/**
*
*
*	Local Entities - Spawn Handling:
*
*
**/
/**
*	@brief	Executed by default for each local entity during SpawnEntities. Will do the key/value dictionary pair
*			iteration for all entity and entity locals variables. Excluding the class specifics.
**/
void CLG_LocalEntity_ParseLocals( clg_local_entity_t *lent, const cm_entity_t *keyValues );
/**
*	@brief	Called from the client's Begin command, gives the client game a shot at
*			spawning local client entities so they can join in the image/model/sound
*			precaching context.
**/
void PF_SpawnEntities( const char *mapname, const char *spawnpoint, const cm_entity_t **entities, const int32_t numEntities );



/**
*
*
*	Local Entities - View Handling:
*
*
**/
/**
*	@brief	Add local client entities that are 'in-frame' to the view's refdef entities list.
**/
void CLG_AddLocalEntities( void );

///**
//*	@brief	Calls the localClass 'Precache' function pointer.
//**/
//const bool CLG_LocalEntity_DispatchPrecache( clg_local_entity_t *lent, const cm_entity_t *keyValues );
///**
//*	@brief	Calls the localClass 'Spawn' function pointer.
//**/
//const bool CLG_LocalEntity_DispatchSpawn( clg_local_entity_t *lent );
///**
//*	@brief	Calls the localClass 'Think' function pointer.
//**/
//const bool CLG_LocalEntity_DispatchThink( clg_local_entity_t *lent );
///**
//*	@brief	Calls the localClass 'RefreshFrame' function pointer.
//**/
//const bool CLG_LocalEntity_DispatchRefreshFrame( clg_local_entity_t *lent );
///**
//*	@brief	Calls the localClass 'RefreshFrame' function pointer.
//**/
//const bool CLG_LocalEntity_DispatchPrepareRefreshEntity( clg_local_entity_t *lent );
