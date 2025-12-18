/********************************************************************
*
*
*	ServerGame: Temporary Event Entity Functions:
*
*
********************************************************************/
#pragma once



/**
*
*
*	(Regular-) Entity Events:
*
*
*   These events are set on the entity's 's.event' field, and thus are tagged along
*   with the entity during its normal state updates to clients. Use these to trigger
*   effects that are directly related to the entity itself.
*
*   It is important to note that these events are only sent to clients that
*   are in the Potentially Hearable Set (PHS) of the entity. So if the entity
*   is too far away from the client, or is blocked by walls/brushes, the
*   client will not receive the event, and thus not play the sound or show
*   the effect.
*
*
**/
/**
*
*	Regular Entity Events -- Sound Playback:
*
**/
/**
*   @brief  Applies an EV_GENERAL_SOUND event to the entity's own event field and parameters.
**/
void SVG_EntityEvent_GeneralSound( svg_base_edict_t *ent, const int32_t channel, const qhandle_t soundResourceIndex );
/**
*   @brief  Applies an EV_GENERAL_SOUND_EX event to the entity's own event field and parameters.
**/
void SVG_EntityEvent_GeneralSoundEx( svg_base_edict_t *ent, const int32_t channel, const qhandle_t soundResourceIndex, const float attenuation );
/**
*   @brief  Applies an EV_POSITIONED_SOUND event to the entity's own event field and parameters.
**/
void SVG_EntityEvent_PositionedSound( svg_base_edict_t *ent, const int32_t channel, const qhandle_t soundResourceIndex, const float attenuation );



/**
*
*
*
*
*	Temp Entity Events -- Blood Particles:
*
*
*
*
**/
/**
*	@brief	Creates a temp entity event to let the client spawn blood particles at the given origin.
*	@param	origin	        The origin to spawn the particles at.
*	@param	normal	        The normal vector of the impacted plane.
*	@param	minCount	    The minimum amount of particles to spawn.
*	@param	maxCount	    The maximum amount of particles to spawn.
*	@return A pointer to the created temp event entity. (For further modification if needed.)
***/
svg_base_edict_t *SVG_TempEventEntity_Blood( const Vector3 &origin, const Vector3 &normal, const int32_t minCount = 75, const int32_t maxCount = 100 );
/**
*	@brief	Creates a temp entity event to let the client spawn "MORE" blood particles at the given origin.
*	@param	origin	        The origin to spawn the particles at.
*	@param	normal	        The normal vector of the impacted plane.
*	@param	minCount	    The minimum amount of particles to spawn.
*	@param	maxCount	    The maximum amount of particles to spawn.
*	@return A pointer to the created temp event entity. (For further modification if needed.)
***/
svg_base_edict_t *SVG_TempEventEntity_MoreBlood( const Vector3 &origin, const Vector3 &normal, const int32_t minCount = 17, const int32_t maxCount = 25 );




/**
*
*
*
*
*	Temp Entity Events -- Impact Particles:
*
*
*
*
**/
/**
*	@brief	Creates a temp entity event to let the client spawn impact particles at the given origin.
*	@param	origin	        The origin to spawn the particles at.
*	@param	normal	        The normal vector of the impacted plane.
*	@param	splashType	    The type of particles to spawn. (sg_splash_types_t)
*	@param	minCount	    The minimum amount of particles to spawn.
*	@param	maxCount	    The maximum amount of particles to spawn.
*	@return A pointer to the created temp event entity. (For further modification if needed.)
***/
svg_base_edict_t *SVG_TempEventEntity_GunShot( const Vector3 &origin, const Vector3 &normal, const int32_t impactType = EV_FX_IMPACT_GUNSHOT, const int32_t minCount = 28, const int32_t maxCount = 40 );



/**
*
*
*
*
*	Temp Entity Events -- Splash Particles:
*
*
*
*
**/
/**
*	@brief	Creates a temp entity event to let the client spawn splash particles at the given origin.
*	@param	origin	        The origin to spawn the splash particles at.
*	@param	normal	        The normal vector of the splash plane.
*	@param	splashType	    The type of splash particles to spawn. (sg_splash_types_t)
*	@param	minCount	    The minimum amount of splash particles to spawn.
*	@param	maxCount	    The maximum amount of splash particles to spawn.
*	@return A pointer to the created temp event entity. (For further modification if needed.)
***/
svg_base_edict_t *SVG_TempEventEntity_SplashParticles( const Vector3 &origin, const Vector3 &normal, const int32_t splashType = EV_FX_SPLASH_WATER_BLUE, const int32_t minCount = 8, const int32_t maxCount = 16 );



/**
*
*
*
*
*	Temp Entity Events -- Sound Playback:
*
*
*
*
**/
/**
*	@brief	Creates a temp entity event to let the client play a general sound on the entity
*           with the passed in parameters.
*   @note   Always players at full volume.
*           Normal attenuation, and 0 sound offset.
*	@param	channel	The sound channel to play the sound on.
*	@param	soundResourceIndex	The sound resource index to play.
*   @return A pointer to the created temp event entity. (For further modification if needed.)
**/
svg_base_edict_t *SVG_TempEventEntity_GeneralSound( svg_base_edict_t *ent, const int32_t channel, const qhandle_t soundResourceIndex );
/**
*	@brief	Creates a temp entity event to let the client play a general sound on the entity
*           with the passed in parameters.
*   @note   Will pack the attenuation parameter together with the channel parameter into the eventParameter.
*	@param	channel	The sound channel to play the sound on.
*	@param	attenuation	The sound attenuation to use.
*	@param	soundResourceIndex	The sound resource index to play.
*   @return A pointer to the created temp event entity. (For further modification if needed.)
**/
svg_base_edict_t *SVG_TempEventEntity_GeneralSoundEx( svg_base_edict_t *ent, const int32_t channel, const qhandle_t soundResourceIndex, const float attenuation );


/**
*	@brief	Creates a temp entity event to let the client play a general sound
*           at the given origin with the passed in parameters.
*   @note   Always players at full volume.
*           Normal attenuation, and 0 sound offset.
*   @param	origin	The origin to play the sound at.
*	@param	channel	The sound channel to play the sound on.
*	@param	soundResourceIndex	The sound resource index to play.
*   @return A pointer to the created temp event entity. (For further modification if needed.)
**/
svg_base_edict_t *SVG_TempEventEntity_PositionedSound( const Vector3 &origin, const int32_t channel, const qhandle_t soundResourceIndex );
/**
*	@brief	Wraps SVG_TempEventEntity_PositionedSound for an entity's origin.
*	@param	ent					The entity to use the origin from.
*	@param	channel				The sound channel to play the sound on.
*	@param	soundResourceIndex	The sound resource index to play.
*   @return A pointer to the created temp event entity. (For further modification if needed.)
**/
svg_base_edict_t *SVG_TempEventEntity_PositionedSound( svg_base_edict_t *ent, const int32_t channel, const qhandle_t soundResourceIndex );

/**
*	@brief	Creates a temp entity event that plays a global sound for all clients
* 		    at their 'head' so it never diminishes.
*   @note   Always player at full volume.
*           Normal attenuation, and 0 sound offset.
*	@param	soundResourceIndex	The sound resource index to play.
*   @return A pointer to the created temp event entity. (For further modification if needed.)
**/
svg_base_edict_t *SVG_TempEventEntity_GlobalSound( const Vector3 &origin, const int32_t channel, const qhandle_t soundResourceIndex );