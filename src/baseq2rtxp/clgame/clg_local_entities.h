/********************************************************************
*
*
*	ClientGame: Local Entities
*
*
********************************************************************/
/**
*	@brief	Called from the client's Begin command, gives the client game a shot at
*			spawning local client entities so they can join in the image/model/sound
*			precaching context.
**/
void PF_SpawnEntities( const char *mapname, const char *spawnpoint, const cm_entity_t **entities, const int32_t numEntities );

/**
*	@return	The casted pointer to the entity's class type.
**/
template<typename T> static inline auto CLG_LocalEntity_GetClass( clg_local_entity_t *self ) -> T * {
	return static_cast<T *>( self->classLocals );
}

/**
*	@brief	Calls the localClass 'Precache' function pointer.
**/
const bool CLG_LocalEntity_DispatchPrecache( clg_local_entity_t *lent, const cm_entity_t *keyValues );
/**
*	@brief	Calls the localClass 'Spawn' function pointer.
**/
const bool CLG_LocalEntity_DispatchSpawn( clg_local_entity_t *lent );
/**
*	@brief	Calls the localClass 'Think' function pointer.
**/
const bool CLG_LocalEntity_DispatchThink( clg_local_entity_t *lent );
/**
*	@brief	Calls the localClass 'RefreshFrame' function pointer.
**/
const bool CLG_LocalEntity_DispatchRefreshFrame( clg_local_entity_t *lent );
/**
*	@brief	Calls the localClass 'RefreshFrame' function pointer.
**/
const bool CLG_LocalEntity_DispatchPrepareRefreshEntity( clg_local_entity_t *lent );
