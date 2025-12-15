/********************************************************************
*
*
*	ClientGame:	General Entities Header
*
*
********************************************************************/
#pragma once



// Use a static entity ID on some things because the renderer relies on eid to match between meshes
// on the current and previous frames.
static constexpr int32_t REFRESHENTITIY_RESERVED_GUN = 1;
static constexpr int32_t REFRESHENTITIY_RESERVED_PREDICTED_PLAYER = 1;
static constexpr int32_t REFRESHENTITIY_RESERVED_TESTMODEL = 3;
static constexpr int32_t REFRESHENTITIY_RESERVED_COUNT = 4;


static constexpr int32_t REFRESHENTITIY_OFFSET_LOCALENTITIES = ( MAX_MODELS + REFRESHENTITIY_RESERVED_COUNT ) / 2; // TODO: Increase MAX_MODELS * 2 for refresh and all that.

/**
*   @brief  For debugging problems when out-of-date entity origin is referenced.
**/
#if USE_DEBUG
	/**
	*   @brief  For debugging problems when out-of-date entity origin is referenced.
	**/
	void CLG_CheckServerEntityPresent( const int32_t entityNumber, const char *what );
#else
	// Stub out for release builds.
	#define CLG_CheckServerEntityPresent(...)
#endif
/**
*   @brief  The sound code makes callbacks to the client for entitiy position
*           information, so entities can be dynamically re-spatialized.
**/
const Vector3 CLG_GetEntitySoundOrigin( const int32_t entityNumber );






/**
*
*
*
*	Entity Identification:
*
*
*
**/
/**
*	@brief	Returns true if the entity state's number matches to our local client entity number,
*			received at initial time of connection.
**/
const bool CLG_IsLocalClientEntity( const server_frame_t *frame );
/**
*	@brief	Returns true if the entity state's number matches to our local client entity number,
*			received at initial time of connection.
**/
const bool CLG_IsLocalClientEntity( const entity_state_t *state );
/**
*	@brief	Returns true if the entityNumber matches to our local connection received entity number.
**/
const bool CLG_IsLocalClientEntity( const int32_t entityNumber );



/**
*
*
*
*	Entity Chase and View Bindings:
*
*
*
**/
/**
*	@return		A pointer into clg_entities that matches to the client we're currently chasing. nullptr if not chasing anyone.
**/
centity_t *CLG_GetChaseBoundEntity( void );
/**
*	@return		The local client entity pointer, which is a match with the entity for the client number which we received at initial time of connection.
**/
centity_t *CLG_GetLocalClientEntity( void );

/**
*	@return		A pointer to the entity which our view has to be bound to. If STAT_CHASE is set, it'll point to the chased entity.
* 				Otherwise, it'll point to the local client entity.
* 
*				Exception: If no client number is set yet, it'll return a nullptr and print a developer warning.
**/
centity_t *CLG_GetViewBoundEntity( void );

/**
*	@brief	Returns true if the entity state's number matches to the view entity number.
**/
const bool CLG_IsCurrentViewEntity( const centity_t *cent );