/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
// g_utils.c -- misc utility functions for game module

#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"
#include "svgame/svg_entity_events.h"
#include "sharedgame/sg_entity_flags.h"
#include "sharedgame/sg_means_of_death.h"
#include "sharedgame/sg_misc.h"



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
*   is too far away from the client, or is blocked by walls/brushes (which confine 
*   the other area the entity is in), the client will not receive the event, and 
*   thus not play the sound or show the effect.
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
void SVG_EntityEvent_GeneralSound( svg_base_edict_t *ent, const int32_t channel, const qhandle_t soundResourceIndex ) {
    // Add the general sound event to the entity.
    SVG_Util_AddEvent( ent, EV_GENERAL_SOUND, channel, soundResourceIndex );
}
/**
*   @brief  Applies an EV_GENERAL_SOUND_EX event to the entity's own event field and parameters.
**/
void SVG_EntityEvent_GeneralSoundEx( svg_base_edict_t *ent, const int32_t channel, const qhandle_t soundResourceIndex, const float attenuation ) {
    // Pack the channel with the attenuation.
    const int32_t packedChannel = channel | ( (uint8_t)( std::clamp<uint8_t>( attenuation * 64, 0, 255 ) ) << 8 );
    // Add the general sound event to the entity.
    SVG_Util_AddEvent( ent, EV_GENERAL_SOUND_EX, packedChannel, soundResourceIndex );
}
/**
*   @brief  Applies an EV_POSITIONED_SOUND event to the entity's own event field and parameters.
**/
void SVG_EntityEvent_PositionedSound( svg_base_edict_t *ent, const int32_t channel, const qhandle_t soundResourceIndex, const float attenuation ) {
    // Pack the channel with the attenuation.
    const int32_t packedChannel = channel | ( (uint8_t)( std::clamp<uint8_t>( attenuation * 64, 0, 255 ) ) << 8 );
    // Add the general sound event to the entity.
    SVG_Util_AddEvent( ent, EV_POSITIONED_SOUND, packedChannel, soundResourceIndex );
}



/**
*
*
*	Temp Entity Events:
*
* 
*   These are created and send out to clients as actual entities, where
*   they point to a specific entity for it to play a sound, or show a visual effect.
*
*   They can be modified after creation to set specific properties, such as
*   setting a specific client to send to, or regarding its lifetime duration.
* 
*   By default it will send to all clients. However, this can be changed by modifying
*   the 'sendClientID' property of the returned entity pointer, and setting the appropriate
*   svFlag bits: SVF_SENDCLIENT_SEND_TO_ID flag, the SVF_SENDCLIENT_EXCLUDE_ID flag, or
*   the SVF_SENDCLIENT_TO_ALL flag.
* 
*
**/
/**
*   @brief	Creates a temp entity event with the specified event set.
**/
static inline svg_base_edict_t *CreateTempEntityForEvent( const Vector3 &origin, const bool snapOrigin, const sg_entity_events_t event, const int32_t eventParm0, const int32_t eventParm1 ) {
    // Create a temporary entity event for all other clients.
    svg_base_edict_t *tempEventEntity = SVG_Util_CreateTempEntityEvent(
        // Use the acquired origin.
        origin,
        // General sound event.
        event, eventParm0, eventParm1,
        // Always snap the origin.
        snapOrigin
    );
    // Ensure no client specific sending. (Can be set after creation if needed.)
    tempEventEntity->sendClientID = SENDCLIENT_TO_ALL;
    // Return it.
    return tempEventEntity;
}

/**
*   @brief Utility to calculate the entity's sound origin.
**/
static inline const Vector3 CalculateEntityEventSoundOrigin( svg_base_edict_t *ent ) {
    // Default to just the origin of the entity.
    Vector3 origin = ent->s.origin;
    // Special case for players.
    if ( ent->client ) {
        origin = ent->client->ps.pmove.origin;
        // Special case for brush models.
    } else if ( ent->solid == SOLID_BSP ) {
        // First get the bbox size.
        const Vector3 bboxSize = QM_BBox3Size( { ent->mins, ent->maxs } );
        // So we can get the center origin of the brush model.
        origin = bboxSize + ent->s.origin;
    }
    return origin;
}



/**
*
*	Temp Entity Events -- Sound Playback:
*
**/
/**
*	@brief	Creates a temp entity event to let the client play a general sound on the entity 
*           with the passed in parameters.
*   @note   Always players at full volume.
*           Normal attenuation, and 0 sound offset.
*	@param	channel	            The sound channel to play the sound on.
*	@param	soundResourceIndex	The sound resource index to play.
*   @return A pointer to the created temp event entity. (For further modification if needed.)
**/
svg_base_edict_t *SVG_TempEventEntity_GeneralSound( svg_base_edict_t *ent, const int32_t channel, const qhandle_t soundResourceIndex ) {
    // Default to just the origin of the entity.
    const Vector3 origin = CalculateEntityEventSoundOrigin( ent );
    // Create a temporary entity event for all other clients.
    svg_base_edict_t *tempEventEntity = CreateTempEntityForEvent( origin, true,
        EV_GENERAL_SOUND, 
        channel, soundResourceIndex
    );

    // Set the otherEntity to the source entity.
	tempEventEntity->s.otherEntityNumber = ent->s.number;
    // Set the effect flag to indicate an other entity is the target for this event.
    tempEventEntity->s.entityFlags = EF_ENTITY_EVENT_TARGET_OTHER;

    // Make sure to send it to all clients if requested.
    // This is because a request to ignore PHS culling is made.
    if ( channel & CHAN_NO_PHS_ADD ) {
		// Set the no cull flag.
		tempEventEntity->svFlags |= SVF_NO_CULL;
    }

    // Return the temp entity event pointer.
	return tempEventEntity;
}
/**
*	@brief	Creates a temp entity event to let the client play a general sound on the entity
*           with the passed in parameters.
*   @note   Will pack the attenuation parameter together with the channel parameter into the eventParameter.
*	@param	channel	The sound channel to play the sound on.
*	@param	attenuation	The sound attenuation to use.
*	@param	soundResourceIndex	The sound resource index to play.
*   @return A pointer to the created temp event entity. (For further modification if needed.)
**/
svg_base_edict_t *SVG_TempEventEntity_GeneralSoundEx( svg_base_edict_t *ent, const int32_t channel, const qhandle_t soundResourceIndex, const float attenuation ) {
    // Default to just the origin of the entity.
    const Vector3 origin = CalculateEntityEventSoundOrigin( ent );
	// Pack the attenuation into the higher bits of the channel parameter.
	const int32_t packedChannel = channel | ( (uint8_t)( std::clamp<uint8_t>(attenuation * 64, 0, 255) ) << 8 );
    // Create a temporary entity event for all other clients.
    svg_base_edict_t *tempEventEntity = CreateTempEntityForEvent( origin, true,
        EV_GENERAL_SOUND_EX,
        packedChannel, soundResourceIndex
    );

    // Set the otherEntity to the source entity.
    tempEventEntity->s.otherEntityNumber = ent->s.number;
    // Set the effect flag to indicate an other entity is the target for this event.
    tempEventEntity->s.entityFlags = EF_ENTITY_EVENT_TARGET_OTHER;

    // Make sure to send it to all clients if requested.
    // This is because a request to ignore PHS culling is made.
    if ( channel & CHAN_NO_PHS_ADD ) {
        // Set the no cull flag.
        tempEventEntity->svFlags |= SVF_NO_CULL;
    }

    // Return the temp entity event pointer.
    return tempEventEntity;
}


/**
*	@brief	Creates a temp entity event to let the client play a general sound 
*           at the given origin with the passed in parameters.
*   @note   Always players at full volume.
*           Normal attenuation, and 0 sound offset.
*   @param	origin	            The origin to play the sound at.
*	@param	channel	            The sound channel to play the sound on.
*	@param	soundResourceIndex	The sound resource index to play.
*   @return A pointer to the created temp event entity. (For further modification if needed.)
**/
svg_base_edict_t *SVG_TempEventEntity_PositionedSound( const Vector3 &origin, const int32_t channel, const qhandle_t soundResourceIndex ) {
    // Create a temporary entity event for all other clients.
    svg_base_edict_t *tempEventEntity = CreateTempEntityForEvent( origin, true,
        EV_POSITIONED_SOUND,
        channel, soundResourceIndex
    );

    // Make sure to send it to all clients if requested.
    // This is because a request to ignore PHS culling is made.
    if ( channel & CHAN_NO_PHS_ADD ) {
        // Set the no cull flag.
        tempEventEntity->svFlags |= SVF_NO_CULL;
    }

	// Return the temp entity event pointer.
	return tempEventEntity;
}
/**
*	@brief	Wraps SVG_TempEventEntity_PositionedSound for an entity's origin.
*	@param	ent					The entity to use the origin from.
*	@param	channel				The sound channel to play the sound on.
*	@param	soundResourceIndex	The sound resource index to play.
*   @return A pointer to the created temp event entity. (For further modification if needed.)
**/
svg_base_edict_t *SVG_TempEventEntity_PositionedSound( svg_base_edict_t *ent, const int32_t channel, const qhandle_t soundResourceIndex ) {
    // Default to just the origin of the entity.
    Vector3 origin = ent->s.origin;
    // Special case for players.
    if ( ent->client ) {
        origin = ent->client->ps.pmove.origin;
        // Special case for brush models.
    } else if ( ent->solid == SOLID_BSP ) {
        // First get the bbox size.
        const Vector3 bboxSize = QM_BBox3Size( { ent->mins, ent->maxs } );
        // So we can get the center origin of the brush model.
        origin = bboxSize + ent->s.origin;
    }
	// Create a temporary entity event to play a positioned sound for all clients.
    return SVG_TempEventEntity_PositionedSound( origin, channel, soundResourceIndex );
}

/**
*	@brief	Creates a temp entity event that plays a global sound for all clients
* 		    at their 'head' so it never diminishes.
*   @note   Always player at full volume.
*           Normal attenuation, and 0 sound offset.
*	@param	soundResourceIndex	The sound resource index to play.
*   @return A pointer to the created temp event entity. (For further modification if needed.)
**/
svg_base_edict_t *SVG_TempEventEntity_GlobalSound( const Vector3 &origin, const qhandle_t soundResourceIndex ) {
    // Create the global sound entity event.
    svg_base_edict_t *tempEventEntity = CreateTempEntityForEvent( origin, true,
        EV_GLOBAL_SOUND,
        soundResourceIndex, 0
    );

    // Send it to all clients.
    tempEventEntity->svFlags |= SVF_NO_CULL;

    // Return the temp entity event pointer.
	return tempEventEntity;
}



