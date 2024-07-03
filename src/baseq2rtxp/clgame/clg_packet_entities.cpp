/********************************************************************
*
*
*	ClientGame: Packet Entities.
* 
*	Each Packet Entity comes with a specific Entity Type set for it
*	in the newState. Most are added by CLG_PacketEntity_AddGeneric.
*
*	However for various distinct ET_* types there exist specific
*	submethods to handle those. This should bring some structure to
*	CLG_AddPacketEntities :-)
*
********************************************************************/
#include "clg_local.h"
#include "clg_effects.h"
#include "clg_entities.h"
#include "clg_packet_entities.h"
#include "clg_temp_entities.h"



/**
*   @brief  Parses all received frame packet entities' new states, iterating over them
*           and (re-)applying any changes to the current and/or new refresh entity that
*           has a matching ID.
**/
void CLG_AddPacketEntities( void ) {
    // Get the current local client's player view entity. (Can be one we're chasing.)
    clgi.client->clientEntity = CLG_ViewBoundEntity();

    // Base entity flags.
    int32_t base_entity_flags = 0;

    // Bonus items rotate at a fixed rate.
    const float autorotate = AngleMod( clgi.client->time * BASE_FRAMETIME_1000 );//AngleMod(clgi.client->time * 0.1f); // WID: 40hz: Adjusted.

        // Iterate over this frame's entity states.
    for ( int32_t frameEntityNumber = 0; frameEntityNumber < clgi.client->frame.numEntities; frameEntityNumber++ ) {
        // Get the entity state index for the entity's newly received state.
        const int32_t entityStateIndex = ( clgi.client->frame.firstEntity + frameEntityNumber ) & PARSE_ENTITIES_MASK;
        // Get the pointer to the newly received entity's state.
        entity_state_t *newState = &clgi.client->entityStates[ entityStateIndex ];
        // Get a pointer to the client game entity that matches the state's number.
        centity_t *packetEntity = &clg_entities[ newState->number ];
        // Backup a possible pointer to an already allocated cache so we can reapply it.
        skm_transform_t *bonePoses = packetEntity->refreshEntity.bonePoses;
        // Setup the refresh entity ID to match that of the client game entity with the RESERVED_ENTITY_COUNT in mind.
        packetEntity->refreshEntity = {
            .id = RENTITIY_RESERVED_COUNT + packetEntity->id,
            .bonePoses = bonePoses
        };

        switch ( newState->entityType ) {
        // Beams:
        case ET_BEAM:
            CLG_PacketEntity_AddBeam( packetEntity, &packetEntity->refreshEntity, newState );
            break;
        case ET_GIB:
            CLG_PacketEntity_AddGib( packetEntity, &packetEntity->refreshEntity, newState );
            break;
        //// Items:
        case ET_ITEM:
            CLG_PacketEntity_AddItem( packetEntity, &packetEntity->refreshEntity, newState );
            break;
        // Monsters:
        case ET_MONSTER:
            CLG_PacketEntity_AddMonster( packetEntity, &packetEntity->refreshEntity, newState );
            continue;
            break;
        // Players:
        case ET_PLAYER:
            CLG_PacketEntity_AddPlayer( packetEntity, &packetEntity->refreshEntity, newState );
            continue;
            break;
        // Pushers:
        case ET_PUSHER:
            CLG_PacketEntity_AddPusher( packetEntity, &packetEntity->refreshEntity, newState );
            break;
        // Spotlights:
        case ET_SPOTLIGHT:
            CLG_PacketEntity_AddSpotlight( packetEntity, &packetEntity->refreshEntity, newState );
           break;

        // ET_GENERIC:
        case ET_GENERIC:
        default:
            CLG_PacketEntity_AddGeneric( packetEntity, &packetEntity->refreshEntity, newState );
            break;
        }
    }
}