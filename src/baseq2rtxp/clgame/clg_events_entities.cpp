/********************************************************************
*
*
*	ClientGame: (Entity/Player State) -Events:
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_entities.h"
#include "clgame/clg_effects.h"
#include "clgame/clg_events.h"
#include "clgame/clg_precache.h"

#include "clgame/effects/clg_fx_classic.h"


// Needed:
#include "sharedgame/sg_entity_events.h"
#include "sharedgame/sg_entity_flags.h"


/**
*
* 
*   Debug Helpers:
* 
* 
**/
#if USE_DEBUG
#define DBG_ENTITY_EVENT_PRESENT( cent, funcname ) \
    if ( developer->integer ) { \
        if ( ( cent->current.entityFlags & EF_ENTITY_EVENT_TARGET_OTHER ) != 0 ) { \
            CLG_CheckServerEntityPresent( cent->current.otherEntityNumber, "ERROR EVENT(EF_ENTITY_EVENT_TARGET_OTHER): "#funcname ); \
        } else { \
            CLG_CheckServerEntityPresent( cent->current.number, "ERROR EVENT: "#funcname ); \
        } \
    } \

#else
    #define DBG_ENTITY_EVENT_PRESENT( ... )
#endif






/**
* 
* 
* 
*   Constant Expressions used by events:
*
*
*
**/
/**
*   Default Sound Properties for when not specified by the event:
**/
static constexpr float defaultSoundVolume = 1.f;
static constexpr float defaultSoundAttenuation = 1.f;
static constexpr float defaultSoundTimeOffset = 0.f;



/**
* 
* 
* 
*   Predeclarations for static functions used below:
* 
* 
* 
**/
/*************************
*   Sound Event Handlers:
*************************/
static void CLG_EntityEvent_GeneralSound( centity_t *cent, const int32_t channel, const qhandle_t soundIndex );
static void CLG_EntityEvent_GeneralSoundEx( centity_t *cent, const int32_t channel, const qhandle_t soundIndex );
static void CLG_EntityEvent_PositionedSound( const Vector3 &origin, const int32_t channel, const qhandle_t soundIndex );
static void CLG_EntityEvent_GlobalSound( const qhandle_t soundIndex );

/*************************
*   Item Event Handlers:
*************************/
static void CLG_EntityEvent_ItemRespawn( centity_t *cent, const int32_t entityNumber, const Vector3 &origin );

/*************************
*   Particle FX Event Handlers:
*************************/
static void CLG_EntityEvent_Blood( const Vector3 & origin, const uint8_t direction, const int32_t count );
static void CLG_EntityEvent_MoreBlood( const Vector3 & origin, const uint8_t direction, const int32_t count );

static void CLG_EntityEvent_GunShot( const Vector3 & origin, const uint8_t direction, const int32_t count );
static void CLG_EntityEvent_Sparks( const Vector3 & origin, const uint8_t direction );
static void CLG_EntityEvent_BulletSparks( const Vector3 & origin, const uint8_t direction, const int32_t count );

static void CLG_EntityEvent_Splash( const Vector3 & origin, const uint8_t direction, const int32_t splashType, const int32_t count );


/**
*
*
* 
*
*   Core Entity Event Processing:
*
*
* 
*
**/
/**
*   @brief  Processes the given entity event.
*   @param  eventValue          The event value to process.
*   @param  lerpOrigin          The lerp origin of the entity generating the event.
*   @param  cent                The centity_t generating the event.
*   @param  entityNumber        The entity number of the entity generating the event.
*   @param  clientNumber        The client number of the entity generating the event, if any (-1 otherwise.).
*   @param  clientInfo          The clientinfo_t of the entity generating the event, if any (nullptr otherwise).
**/
void CLG_Events_FireEntityEvent( const int32_t eventValue, const Vector3 &lerpOrigin, centity_t *cent, const int32_t entityNumber, const int32_t clientNumber, clientinfo_t *clientInfo ) {
    // <Q2RTXP>: TODO: Fix so it doesn't do the teleporter at incorrect spawn origin.
    const Vector3 effectOrigin = lerpOrigin; // cent->current.origin 

    // EF_TELEPORTER acts like an event, but is not cleared each frame
    if ( ( cent->current.entityFlags & EF_TELEPORTER ) ) {
        CLG_FX_TeleporterParticles( &effectOrigin.x );
    }

    const int32_t clampedEventValue = eventValue;

    // Handle the event.
    switch ( eventValue ) {
    /**
    *   FootStep Events:
    **/
    case EV_PLAYER_FOOTSTEP:
		// Prevent it from being triggered if it is our own local player entity.
		// We handle that event elsewhere in CLG_Events_FirePlayerStateEvent.
		if ( entityNumber != clgi.client->clientNumber + 1 ) {//clgi.client->frame.ps.clientNumber + 1 ) {
            // Print event name for debugging.
            DEBUG_PRINT_EVENT_NAME( sg_event_string_names[clampedEventValue]);
			// Fire the footstep effect.
            CLG_FX_PlayerFootStep( entityNumber, lerpOrigin );
        }
        break;
    case EV_OTHER_FOOTSTEP:
        // Print event name for debugging.
        DEBUG_PRINT_EVENT_NAME( sg_event_string_names[ clampedEventValue ] );
		// Fire the footstep effect.
        CLG_FX_OtherFootStep( entityNumber, lerpOrigin );
        break;
    // General ladder footstep event:
    case EV_FOOTSTEP_LADDER:
        // Print event name for debugging.
        DEBUG_PRINT_EVENT_NAME( sg_event_string_names[ clampedEventValue ] );
		// Fire the footstep effect.
        CLG_FX_LadderFootStep( entityNumber, lerpOrigin );
        break;

    /**
    *   Sound Events:
    **/
    // General entity sound event:
    case EV_GENERAL_SOUND:
        // Print event name for debugging.
        DEBUG_PRINT_EVENT_NAME( sg_event_string_names[ clampedEventValue ] );
		// Fire the general sound event.
        CLG_EntityEvent_GeneralSound( cent, cent->current.eventParm0, cent->current.eventParm1 );
        break;
    case EV_GENERAL_SOUND_EX:
        // Print event name for debugging.
        DEBUG_PRINT_EVENT_NAME( sg_event_string_names[ clampedEventValue ] );
		// Fire the general sound event with attenuation.
        CLG_EntityEvent_GeneralSoundEx( cent, cent->current.eventParm0, cent->current.eventParm1 );
        break;
    // General positioned sound event:
    case EV_POSITIONED_SOUND:
        // Print event name for debugging.
        DEBUG_PRINT_EVENT_NAME( sg_event_string_names[ clampedEventValue ] );
		// Fire the positioned sound event.
        CLG_EntityEvent_PositionedSound( lerpOrigin, cent->current.eventParm0, cent->current.eventParm1 );
        break;
    // General global sound event:
    case EV_GLOBAL_SOUND:
        // Print event name for debugging.
        DEBUG_PRINT_EVENT_NAME( sg_event_string_names[ clampedEventValue ] );
		// Fire the global sound event.
        CLG_EntityEvent_GlobalSound( cent->current.eventParm0 );
        break;

    ///**
    //*   Fall and Landing Events:
    //**/
    //case EV_FALL_MEDIUM:
    //    clgi.S_StartSound( NULL, entityNumber, CHAN_AUTO, clgi.S_RegisterSound( "player/land01.wav" ), 1, ATTN_NORM, 0 );
    //    break;
    //case EV_FALL_SHORT:
    //    clgi.S_StartSound( NULL, entityNumber, CHAN_AUTO, clgi.S_RegisterSound( "player/fall02.wav" ), 1, ATTN_NORM, 0 );
    //    break;
    //case EV_FALL_FAR:
    //    clgi.S_StartSound( NULL, entityNumber, CHAN_AUTO, clgi.S_RegisterSound( "player/fall01.wav" ), 1, ATTN_NORM, 0 );
    //    break;

    ///**
    //*   Teleport Events:
    //**/
    //case EV_PLAYER_TELEPORT:
    //    clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, clgi.S_RegisterSound( "misc/teleport01.wav" ), 1, ATTN_IDLE, 0 );
    //    CLG_TeleportParticles( &effectOrigin.x );
    //    break;

    /**
    *   Item Events:
    **/
    case EV_ITEM_RESPAWN:
        // Print event name for debugging.
        DEBUG_PRINT_EVENT_NAME( sg_event_string_names[ clampedEventValue ] );
        // Fire the item respawn event.
		CLG_EntityEvent_ItemRespawn( cent, entityNumber, effectOrigin );
        break;

    /**
    *   Particle FX Events:
    **/
	//---------------------------------------------------------------
	
	case EV_FX_BLOOD:
		// Print event name for debugging.
		DEBUG_PRINT_EVENT_NAME( sg_event_string_names[ clampedEventValue ] );
		// Fire the blood effect.
		CLG_EntityEvent_Blood( effectOrigin, cent->current.eventParm1, cent->current.eventParm0 );
		break;
	case EV_FX_MORE_BLOOD:
		// Print event name for debugging.
		DEBUG_PRINT_EVENT_NAME( sg_event_string_names[ clampedEventValue ] );
		// Fire the blood effect.
		CLG_EntityEvent_MoreBlood( effectOrigin, cent->current.eventParm1, cent->current.eventParm0 );
		break;

	//---------------------------------------------------------------

	case EV_FX_IMPACT_GUNSHOT:
		// Print event name for debugging.
		DEBUG_PRINT_EVENT_NAME( sg_event_string_names[ clampedEventValue ] );
		// Fire the gun shot effect.
		CLG_EntityEvent_GunShot( cent->current.origin, cent->current.eventParm1, cent->current.eventParm0 );
		break;
	case EV_FX_IMPACT_SPARKS:
		// Print event name for debugging.
		DEBUG_PRINT_EVENT_NAME( sg_event_string_names[ clampedEventValue ] );
		// Fire the sparks effect.
		CLG_EntityEvent_Sparks( cent->current.origin, cent->current.eventParm0 );
		break;
	case EV_FX_IMPACT_BULLET_SPARKS:
		// Print event name for debugging.
		DEBUG_PRINT_EVENT_NAME( sg_event_string_names[ clampedEventValue ] );
		// Fire the bullet sparks effect.
		CLG_EntityEvent_BulletSparks( cent->current.origin, cent->current.eventParm1, cent->current.eventParm0 );
		break;

	//---------------------------------------------------------------
    case EV_FX_SPLASH_UNKNOWN:
        // Print event name for debugging.
        DEBUG_PRINT_EVENT_NAME( sg_event_string_names[ clampedEventValue ] );
		// Generic splash effect.
        CLG_EntityEvent_Splash( cent->current.origin, cent->current.eventParm1, SPLASH_FX_COLOR_UNKNOWN, cent->current.eventParm0 );
		break;
    case EV_FX_SPLASH_SPARKS:
        // Print event name for debugging.
        DEBUG_PRINT_EVENT_NAME( sg_event_string_names[ clampedEventValue ] );
		// Spark splash effect.
		CLG_EntityEvent_Splash( cent->current.origin, cent->current.eventParm1, SPLASH_FX_COLOR_SPARKS, cent->current.eventParm0 );
		break;
        //! A blue water splash effect.
    case EV_FX_SPLASH_WATER_BLUE:
        // Print event name for debugging.
        DEBUG_PRINT_EVENT_NAME( sg_event_string_names[ clampedEventValue ] );
		// Fire the blue water splash effect.
        CLG_EntityEvent_Splash( cent->current.origin, cent->current.eventParm1, SPLASH_FX_COLOR_BLUE_WATER, cent->current.eventParm0 );
        break;
            //! A blue water splash effect.
    case EV_FX_SPLASH_WATER_BROWN:
        // Print event name for debugging.
        DEBUG_PRINT_EVENT_NAME( sg_event_string_names[ clampedEventValue ] );
		// Fire the brown water splash effect.
        CLG_EntityEvent_Splash( cent->current.origin, cent->current.eventParm1, SPLASH_FX_COLOR_BROWN_WATER, cent->current.eventParm0 );
        break;
            //! A green slime splash effect.
    case EV_FX_SPLASH_SLIME:
        // Print event name for debugging.
        DEBUG_PRINT_EVENT_NAME( sg_event_string_names[ clampedEventValue ] );
		// Fire the slime splash effect.
        CLG_EntityEvent_Splash( cent->current.origin, cent->current.eventParm1, SPLASH_FX_COLOR_SLIME, cent->current.eventParm0 );
        break;
            //! A red lava splash effect.
    case EV_FX_SPLASH_LAVA:
        // Print event name for debugging.
        DEBUG_PRINT_EVENT_NAME( sg_event_string_names[ clampedEventValue ] );
		// Fire the lava splash effect.
        CLG_EntityEvent_Splash( cent->current.origin, cent->current.eventParm1, SPLASH_FX_COLOR_LAVA, cent->current.eventParm0 );
        break;
    case EV_FX_SPLASH_BLOOD:
        // Print event name for debugging.
        DEBUG_PRINT_EVENT_NAME( sg_event_string_names[ clampedEventValue ] );
		// Fire the blood splash effect.
        CLG_EntityEvent_Splash( cent->current.origin, cent->current.eventParm1, SPLASH_FX_COLOR_BLOOD, cent->current.eventParm0 );
        break;

	//---------------------------------------------------------------

    default:
            //clgi.Print( PRINT_DEVELOPER, "%s: Unknown EntityEvent Type(Unclamped: %d, Clamped: %d)\n", __func__, eventValue, clampedEventValue );
        break;
    }
}



/**
*
*
*
*
*   Commonly Used/Utility Functions:
*
* 
*
*
**/
/**
*   @brief  Validates the given sound index, and errors out if invalid.
**/
static const bool ValidateIndexOrError( const qhandle_t index, const int32_t minIndex, const int32_t maxIndex, const char *indexTypeName  ) {
    // Ensure valid sound index.
    if ( minIndex < minIndex ) {
        Com_Error( ERR_DROP, "%s: bad %s index: %d < 0", __func__, indexTypeName, index );
        return false;
    }
    if ( index >= maxIndex ) {
        Com_Error( ERR_DROP, "%s: bad %s index: %d >= MAX_SOUNDS(%d)", __func__, indexTypeName, index, maxIndex );
        return false;
    }

    // Valid.
    return true;
}
/**
*   @brief  Retreives and validates the sound handle for the given sound index.
**/
static inline qhandle_t GetSoundIndexResourceHandle( const int32_t soundIndex ) {
    // Sanity check.
    if ( !ValidateIndexOrError( soundIndex, 0, MAX_SOUNDS, "Sound Index") ) {
        return 0;
    }
    // Get the sound handle.
    qhandle_t handle = clgi.client->sound_precache[ soundIndex ];
    if ( !handle ) {
        clgi.Print( PRINT_DEVELOPER, "%s: NULL sound handle for index: %d\n", __func__, soundIndex );
        return 0;
    }
    // Return it.
    return handle;
}
/**
*   @brief  Retreives the sound name handle for the given sound resource handle.
**/
static inline const std::string GetSoundResourceHandleName( const qhandle_t resourceHandle ) {
    // Sanity check.
    if ( !ValidateIndexOrError( resourceHandle, 0, MAX_SOUNDS, "Sound Resource Handle" ) ) {
        return 0;
    }
    // Get the sound handle.
	return clgi.S_SoundNameForHandle( resourceHandle );
}



/**
*
*
*
*
*
*   [ Sound Events ] Implementations:
*
*
*
*
*
**/
/**
*	@brief	Handle the (temp-) entity event to let the client play a general sound on the entity
*           with the passed in parameters.
*   @note   Always players at full volume.
*           Normal attenuation, and 0 sound offset.
*	@param	channel	The sound channel to play the sound on.
*	@param	soundResourceIndex	The sound resource index to play.
**/
static void CLG_EntityEvent_GeneralSound( centity_t *cent, const int32_t channel, const qhandle_t soundIndex ) {
    // Sanity check.
    if ( !cent ) {
        clgi.Print( PRINT_DEVELOPER, "%s: NULL cent entity passed in!\n", __func__ );
        return;
    }

    // Debug check for valid entity/entities.
    DBG_ENTITY_EVENT_PRESENT( cent, __func__ );
    // Debug the resource name.
    DBG_ENTITY_EVENT_SOUND_NAME_ENTITY( __func__, cent, soundIndex );

    // Get the sound handle.
    qhandle_t soundResourceHandle = GetSoundIndexResourceHandle( soundIndex );
    if ( !soundResourceHandle ) {
        return;
    }
    // Play the sound on the entity.
    if ( ( cent->current.entityFlags & EF_ENTITY_EVENT_TARGET_OTHER ) != 0 && cent->current.otherEntityNumber > 0 ) {
        // Get the other entity.
        clgi.S_StartSound( nullptr, cent->current.otherEntityNumber, channel, soundResourceHandle, defaultSoundVolume, defaultSoundAttenuation, defaultSoundTimeOffset );
    } else {
        clgi.S_StartSound( nullptr, cent->current.number, channel, soundResourceHandle, defaultSoundVolume, defaultSoundAttenuation, defaultSoundTimeOffset );
    }
}
/**
*	@brief	Handle the (temp-) entity event to let the client play a general sound on the entity
*           with the passed in parameters.
*   @note   Will pack the attenuation parameter together with the channel parameter into the eventParameter.
*	@param	channel	The sound channel to play the sound on.
*	@param	attenuation	The sound attenuation to use.
*	@param	soundResourceIndex	The sound resource index to play.
**/
static void CLG_EntityEvent_GeneralSoundEx( centity_t *cent, const int32_t channel, const qhandle_t soundIndex ) {
    // Sanity check.
    if ( !cent ) {
        clgi.Print( PRINT_DEVELOPER, "%s: NULL cent entity passed in!\n", __func__ );
        return;
    }

    // Debug check for valid entity/entities.
    DBG_ENTITY_EVENT_PRESENT( cent, __func__ );
    // Debug the resource name.
    DBG_ENTITY_EVENT_SOUND_NAME_ENTITY( __func__, cent, soundIndex );

    // Get the sound handle.
    qhandle_t soundResourceHandle = GetSoundIndexResourceHandle( soundIndex );
    if ( !soundResourceHandle ) {
        return;
    }

    // Decode the channel and the attenuation from the channel parameter.
    int32_t decodedChannel = (uint8_t)( channel & 0xFF );
    // Stored as 0-3 in the event. <Q2RTXP>: TODO: (0-10 if we implement all attenuation levels)
    const float decodedAttenuation = static_cast<float>( ( channel & 0xFFFF ) >> 8 ) / 64.f;

    // Clear the no phs and reliable flags from the channel.
    // They are intend for svc_sound server commands only.
    constexpr int32_t removeChannelMask = ( CHAN_NO_PHS_ADD | CHAN_RELIABLE );
    decodedChannel &= ~removeChannelMask;

    // Play the sound on the entity.
    if ( ( cent->current.entityFlags & EF_ENTITY_EVENT_TARGET_OTHER ) != 0 && cent->current.otherEntityNumber > 0 ) {
        // Get the other entity.
        clgi.S_StartSound( nullptr, cent->current.otherEntityNumber, decodedChannel, soundResourceHandle, defaultSoundVolume, decodedAttenuation, defaultSoundTimeOffset );
    } else {
        clgi.S_StartSound( nullptr, cent->current.number, decodedChannel, soundResourceHandle, defaultSoundVolume, decodedAttenuation, defaultSoundTimeOffset );
    }
}

/**
*	@brief	Handle the (temp-) entity event at the specified origin to let the
*           client play a positioned general sound.
*   @note   Always players at full volume.
*           Normal attenuation, and 0 sound offset.
*	@param	channel	The sound channel to play the sound on.
*	@param	soundResourceIndex	The sound resource index to play.
**/
static void CLG_EntityEvent_PositionedSound( const Vector3 &origin, const int32_t channel, const qhandle_t soundIndex ) {
    // Debug the resource name.
    DBG_ENTITY_EVENT_SOUND_NAME_ORIGIN( __func__, origin, soundIndex );

    // Get the sound handle.
    qhandle_t handle = GetSoundIndexResourceHandle( soundIndex );
    if ( !handle ) {
        return;
    }
    // Play the sound at the given origin.
    clgi.S_StartSound( &origin.x, ENTITYNUM_WORLD, channel, soundIndex, defaultSoundVolume, defaultSoundAttenuation, defaultSoundTimeOffset );
}

/**
*	@brief	Handle the (temp-) entity event that plays a global sound for all clients
* 		    at their 'head' so it never diminishes.
*   @note   Always players at full volume.
*           Normal attenuation, and 0 sound offset.
*	@param	soundResourceIndex	The sound resource index to play.
**/
static void CLG_EntityEvent_GlobalSound( const qhandle_t soundIndex ) {
    // Debug the resource name.
    DBG_ENTITY_EVENT_SOUND_NAME_ORIGIN( __func__, clgi.client->frame.ps.pmove.origin, soundIndex );

    // Get the sound handle.
    qhandle_t handle = GetSoundIndexResourceHandle( soundIndex );
    if ( !handle ) {
        return;
    }
    // Play the sound at the given origin.
    clgi.S_StartSound( &clgi.client->frame.ps.pmove.origin.x, ENTITYNUM_WORLD, CHAN_AUTO, soundIndex, defaultSoundVolume, defaultSoundAttenuation, defaultSoundTimeOffset );
}



/**
*
* 
*
*
*   [ Item Events ] Implementations:
* 
* 
* 
* 
**/
static void CLG_EntityEvent_ItemRespawn( centity_t *cent, const int32_t entityNumber, const Vector3 &origin ) {
    // Debug check for valid entity/entities.
    DBG_ENTITY_EVENT_PRESENT( cent, __func__ );

    // Play the respawn sound.
    clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, precache.sfx.items.respawn01, 1, ATTN_IDLE, 0 );
    // Spawn the respawn particles.
    CLG_FX_ItemRespawnParticles( origin );
}



/**
*
*
*
* 
* 
*  [ Particle FX Events ] Implementations:
* 
* 
* 
* 
* 
**/
/**************************************
*   [ EV_FX_BLOOD ] Event Handler:
***************************************/
static void CLG_EntityEvent_Blood( const Vector3 &origin, const uint8_t direction, const int32_t count ) {
	// Decode the direction byte to a direction vector.
	vec3_t decodedDirection = { 0.f, 0.f, 0.f };
	ByteToDir( direction, decodedDirection );

	// Call the blood particle effect function.
	CLG_FX_BloodParticleEffect( origin, decodedDirection, 0xe8, count * 10 );

	// <Q2RTXP>: TODO: Bullet hit "smoke and flash" model anim.
	//CLG_FX_SmokeAndFlash( level.parsedMessage.events.tempEntity.pos1 );
}
/**************************************
*   [ EV_FX_BLOOD ] Event Handler:
***************************************/
static void CLG_EntityEvent_MoreBlood( const Vector3 &origin, const uint8_t direction, const int32_t count ) {
	// Decode the direction byte to a direction vector.
	vec3_t decodedDirection = { 0.f, 0.f, 0.f };
	ByteToDir( direction, decodedDirection );

	// Call the blood particle effect function.
	CLG_FX_ParticleEffect( origin, decodedDirection, 0xe8, count * 10 );

	// <Q2RTXP>: TODO: Bullet hit "smoke and flash" model anim.
	//CLG_FX_SmokeAndFlash( level.parsedMessage.events.tempEntity.pos1 );
}

/**************************************
*   [ EV_FX_IMPACT_GUNSHOT ] Event Handler:
***************************************/
static void CLG_EntityEvent_GunShot( const Vector3 &origin, const uint8_t direction, const int32_t count ) {
	// Decode the direction byte to a direction vector.
	vec3_t decodedDirection = { 0.f, 0.f, 0.f };
	ByteToDir( direction, decodedDirection );

	// Call the generic particle effect function.
	CLG_FX_ParticleEffect( &origin.x, decodedDirection, 0, count );

	// <Q2RTXP>: TODO: Bullet hit "smoke and flash" model anim.
	//CLG_FX_SmokeAndFlash( level.parsedMessage.events.tempEntity.pos1 );

	// Impact sound
	const int32_t r = irandom( 15 + 1 );//Q_rand() & 15;

	if ( r == 1 ) {
		clgi.S_StartSound( &origin.x, 0, 0, precache.sfx.ricochets.ric1, 1, ATTN_NORM, 0 );
	} else if ( r == 2 ) {
		clgi.S_StartSound( &origin.x, 0, 0, precache.sfx.ricochets.ric2, 1, ATTN_NORM, 0 );
	} else if ( r == 3 ) {
		clgi.S_StartSound( &origin.x, 0, 0, precache.sfx.ricochets.ric3, 1, ATTN_NORM, 0 );
	}
}
/**************************************
*   [ EV_FX_IMPACT_SPARKS ] Event Handler:
***************************************/
static void CLG_EntityEvent_Sparks( const Vector3 &origin, const uint8_t direction ) {
	// Decode the direction byte to a direction vector.
	vec3_t decodedDirection = { 0.f, 0.f, 0.f };
	ByteToDir( direction, decodedDirection );

	// Call the generic particle effect function.
	CLG_FX_ParticleEffect( &origin.x, decodedDirection, 0xe0, 6 );
}
/**************************************
*   [ EV_FX_IMPACT_BULLET_SPARKS ] Event Handler:
***************************************/
static void CLG_EntityEvent_BulletSparks( const Vector3 &origin, const uint8_t direction, const int32_t count ) {
	// Decode the direction byte to a direction vector.
	vec3_t decodedDirection = { 0.f, 0.f, 0.f };
	ByteToDir( direction, decodedDirection );

	// Call the generic particle effect function.
	CLG_FX_ParticleEffect( &origin.x, decodedDirection, 0xe0, count );

	// <Q2RTXP>: TODO: Bullet hit "smoke and flash" model anim.
	//CLG_FX_SmokeAndFlash( level.parsedMessage.events.tempEntity.pos1 );

	// Impact sound
	const int32_t r = irandom( 15 + 1 );//Q_rand() & 15;
	
	if ( r == 1 ) {
		clgi.S_StartSound( &origin.x, 0, 0, precache.sfx.ricochets.ric1, 1, ATTN_NORM, 0 );
	} else if ( r == 2 ) {
		clgi.S_StartSound( &origin.x, 0, 0, precache.sfx.ricochets.ric2, 1, ATTN_NORM, 0 );
	} else if ( r == 3 ) {
		clgi.S_StartSound( &origin.x, 0, 0, precache.sfx.ricochets.ric3, 1, ATTN_NORM, 0 );
	}
}

/*************************
*   Water Splash FX Event Handler:
*************************/
static void CLG_EntityEvent_Splash( const Vector3 &origin, const uint8_t direction, const int32_t splashType, const int32_t count ) {
    // Splash color values for each splashType.
    static constexpr byte colors[] = {
		0x00,   // Unknown splashes.
        0xe0,   // Sparks.
        0xb0,   // Blue Water.
        0x50,   // Brown Water.
		0xd0,   // Green Slime.
		0xe0,   // Red Lava.
		0xe8    // Red Blood.
    };

    // Range values for splash types.
    static constexpr int32_t minRange = SPLASH_FX_COLOR_UNKNOWN;
    static constexpr int32_t maxRange = SPLASH_FX_COLOR_BLOOD;

    // Default to unknown splash color.
    int32_t splashColor = colors[ std::clamp( splashType, minRange, maxRange ) ];

    //! Unknown splash type effect.
    if ( splashType < SPLASH_FX_COLOR_UNKNOWN && splashType > SPLASH_FX_COLOR_BLOOD ) {
        // Invalid splash type.
        clgi.Print( PRINT_DEVELOPER, "%s: Unknown splash type: %d (range: >=%d .. <=%d )\n", __func__, splashType, minRange, maxRange );
    }

	// Decode the direction byte to a direction vector.
	vec3_t decodedDirection = { 0.f, 0.f, 1.f };
	ByteToDir( direction, decodedDirection );

	// Call the splash effect function.
    CLG_FX_ParticleEffectWaterSplash( &origin.x, decodedDirection, splashType, count );
}
