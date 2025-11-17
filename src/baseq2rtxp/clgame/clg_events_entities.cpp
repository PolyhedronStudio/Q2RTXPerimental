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

// Needed:
#include "sharedgame/sg_entity_events.h"
#include "sharedgame/sg_entity_flags.h"
#include "sharedgame/sg_entity_types.h"
#include "sharedgame/sg_muzzleflashes.h"


/**
*   Predeclarations for static functions used below:
**/
//! Sound Event Handlers:
static void CLG_EntityEvent_GeneralSound( centity_t *cent, const int32_t channel, const qhandle_t soundIndex );
static void CLG_EntityEvent_GeneralSoundEx( centity_t *cent, const int32_t channel, const qhandle_t soundIndex );
static void CLG_EntityEvent_PositionedSound( const Vector3 &origin, const int32_t channel, const qhandle_t soundIndex );
static void CLG_EntityEvent_GlobalSound( const qhandle_t soundIndex );
//! Item Event Handles:
static void CLG_EntityEvent_ItemRespawn( centity_t *cent, const int32_t entityNumber, const Vector3 &origin );



/**
*
*
*
*   Entity Event Processing:
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
void CLG_Events_ProcessEntityEvent( const int32_t eventValue, const Vector3 &lerpOrigin, centity_t *cent, const int32_t entityNumber, const int32_t clientNumber, clientinfo_t *clientInfo ) {

    // <Q2RTXP>: TODO: Fix so it doesn't do the teleporter at incorrect spawn origin.
    const Vector3 effectOrigin = lerpOrigin; // cent->current.origin 


    // EF_TELEPORTER acts like an event, but is not cleared each frame
    if ( ( cent->current.entityFlags & EF_TELEPORTER ) ) {
        CLG_TeleporterParticles( &effectOrigin.x );
    }

    // Handle the event.
    switch ( eventValue ) {
        /**
        *   FootStep Events:
        **/
    case EV_OTHER_FOOTSTEP:
        CLG_FootstepEvent( entityNumber );
        break;
    // General ladder footstep event:
    case EV_FOOTSTEP_LADDER:
        CLG_FootstepLadderEvent( entityNumber );
        break;

    /**
    *   Sound Events:
    **/
    // General entity sound event:
    case EV_GENERAL_SOUND:
        CLG_EntityEvent_GeneralSound( cent, cent->current.eventParm0, cent->current.eventParm1 );
        break;
    case EV_GENERAL_SOUND_EX:
        CLG_EntityEvent_GeneralSoundEx( cent, cent->current.eventParm0, cent->current.eventParm1 );
        break;
    // General positioned sound event:
    case EV_POSITIONED_SOUND:
        CLG_EntityEvent_PositionedSound( lerpOrigin, cent->current.eventParm0, cent->current.eventParm1 );
        break;
    // General global sound event:
    case EV_GLOBAL_SOUND:
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
		CLG_EntityEvent_ItemRespawn( cent, entityNumber, effectOrigin );
        break;

    default:
        break;
    }
}



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
*   Commonly Used/Utility Functions:
*
*
*
**/
/**
*   @brief  Validates the given sound index, and errors out if invalid.
**/
static const bool ValidSoundIndexOrError( const qhandle_t soundIndex ) {
    // Ensure valid sound index.
    if ( soundIndex < 0 ) {
        Com_Error( ERR_DROP, "%s: bad sound resource index: %d < 0", __func__, soundIndex );
        return false;
    }
    if ( soundIndex >= MAX_SOUNDS ) {
        Com_Error( ERR_DROP, "%s: bad sound resource index: %d >= MAX_SOUNDS(%d)", __func__, soundIndex, MAX_SOUNDS );
        return false;
    }

    // Valid.
    return true;
}
/**
*   @brief  Retreives and validates the sound handle for the given sound index.
**/
static inline qhandle_t GetSoundResourceHandle( const int32_t soundIndex ) {
    // Sanity check.
    if ( !ValidSoundIndexOrError( soundIndex ) ) {
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
*
*
*
*   [ Item Events ] Implementations:
* 
* 
* 
**/
static void CLG_EntityEvent_ItemRespawn( centity_t *cent, const int32_t entityNumber, const Vector3 &origin ) {
    // Play the respawn sound.
    clgi.S_StartSound( NULL, entityNumber, CHAN_WEAPON, clgi.S_RegisterSound( "items/respawn01.wav" ), 1, ATTN_IDLE, 0 );
    // Spawn the respawn particles.
    CLG_ItemRespawnParticles( origin );
}

/**
*
*
*
*   [ Sound Events ] Implementations:
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
    #if USE_DEBUG
    if ( developer->integer ) {
        CLG_CheckEntityPresent( cent->current.otherEntityNumber, __func__ );
    }
    #endif

	// Get the sound handle.
    qhandle_t soundResourceHandle = GetSoundResourceHandle( soundIndex );
    if ( !soundResourceHandle ) {
        return;
    }
    // Play the sound on the entity.
    if ( ( cent->current.entityFlags & EF_OTHER_ENTITY_EVENT ) != 0 && cent->current.otherEntityNumber > 0 ) {
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
    #if USE_DEBUG
    if ( developer->integer ) {
        CLG_CheckEntityPresent( cent->current.otherEntityNumber, __func__ );
    }
    #endif

    // Get the sound handle.
    qhandle_t soundResourceHandle = GetSoundResourceHandle( soundIndex );
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
    if ( ( cent->current.entityFlags & EF_OTHER_ENTITY_EVENT ) != 0 && cent->current.otherEntityNumber ) {
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
    // Get the sound handle.
    qhandle_t handle = GetSoundResourceHandle( soundIndex );
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
    // Get the sound handle.
    qhandle_t handle = GetSoundResourceHandle( soundIndex );
    if ( !handle ) {
        return;
    }
    // Play the sound at the given origin.
    clgi.S_StartSound( &clgi.client->frame.ps.pmove.origin.x, ENTITYNUM_WORLD, CHAN_AUTO, soundIndex, defaultSoundVolume, defaultSoundAttenuation, defaultSoundTimeOffset );
}