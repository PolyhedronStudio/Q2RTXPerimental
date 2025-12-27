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
*
*
*
*	Entity Chase and View Bindings:
*
*
*
**/
#if 0
/**
*	@return		A pointer into clg_entities that matches to the client we're currently chasing. nullptr if not chasing anyone.
**/
centity_t *CLG_GetChaseBoundEntity( void );
#endif
/**
*	@return		A pointer into clg_entities that matches to the received client frame its player_state_t we're currently chasing.
* 				nullptr if not chasing anyone but the local client entity itself.
**/
centity_t *CLG_GetFrameClientEntity( void );
/**
*	@return		The local client entity pointer, which is a match with the entity in clg_entities
*				for the client number which we received at initial time of connection.
**/
centity_t *CLG_GetLocalClientEntity( void );
/**
*	@return		The predicted client entity pointer, which resides outside of clg_entities.
**/
centity_t *CLG_GetPredictedClientEntity( void );
#if 0
/**
*	@return		A pointer to the entity which our view has to be bound to. If STAT_CHASE is set, it'll point to the chased entity.
* 				Otherwise, it'll point to the local client entity.
*
*				Exception: If no client number is set yet, it'll return a nullptr and print a developer warning.
**/
centity_t *CLG_GetViewBoundEntity( void );
#endif
/**
*	@return		A pointer to the entity which our view is bound to. This can be the local client entity, the
*				actual chased entity(the frame's playerstate clientnumber != local client number) or the predicted entity.
*
*				Exception: If no client number is set yet, it'll return a nullptr and print a developer warning.
**/
centity_t *CLG_GetViewBoundEntity( const bool onlyPredictedClient = false );
/**
*	@brief	Returns true if the entity state's number matches to the view entity number.
**/
const bool CLG_IsCurrentViewEntity( const centity_t *cent );



/**
*
*
*
*   Entity Frame(s), Origin, and Angles (Linear/Extrapolation):
*
*
*
**/
/**
*   @brief  Animates the entity's frame from previousFrameTime to currentTime using the given animationHz.
*	@param previousFrameTime The previous frame time.
*	@param currentTime The current time.
*	@param animationHz The animation hertz to use. Default is BASE_FRAMERATE (40).
*	@return The backLerp fraction for the animation.
**/
const double CLG_GetEntityFrameBackLerp( const QMTime previousFrameTime, const QMTime currentTime, const double animationHz = BASE_FRAMERATE );
/**
*   @brief  Handles the 'lerping' of the entity origins.
*	@param	previousFrameOrigin The previous frame origin.
* 	@param	currentFrameOrigin The current frame origin.
* 	@param	lerpFraction The lerp fraction to use for interpolation.
*	@return A Vector3 containing the lerped origin.
**/
const Vector3 CLG_GetEntityLerpOrigin( const Vector3 &previousFrameOrigin, const Vector3 &currentFrameOrigin, const double lerpFraction );
/**
*   @brief  Handles the 'lerping' of the packet and its corresponding refresh entity angles.
*	@param	previousFrameAngles The previous frame angles.
*	@param	currentFrameAngles The current frame angles.
*	@param	lerpFraction The lerp fraction to use for interpolation.
*	@return A Vector3 containing the lerped angles.
**/
const Vector3 CLG_GetEntityLerpAngles( const Vector3 &previousFrameAngles, const Vector3 &currentFrameAngles, const double lerpFraction );



/**
*
*
*
*   Entity Sound Spatialization (Linear/Extrapolation):
*
*
*
**/
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



