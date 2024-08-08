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

// WID: TODO: Move to client where it determines old/new states.
#if 0 
/**
*	@brief	Determine the rotation of movement relative to the facing dir.
**/
static void CLG_PacketEntity_DetermineMoveDirection( centity_t *packetEntity, entity_state_t *newState ) {
    // if it's moving to where is looking, it's moving forward
    // The desired yaw for the lower body.
    static constexpr float DIR_EPSILON = 0.3f;
    static constexpr float WALK_EPSILON = 5.0f;
    static constexpr float RUN_EPSILON = 100.f;

    // Update angle vectors if necessary.
    if ( packetEntity->current.angles[ 0 ] != newState->angles[ 0 ]
        || packetEntity->current.angles[ 1 ] != newState->angles[ 1 ]
        || packetEntity->current.angles[ 2 ] != newState->angles[ 2 ]
    ) {
        // Calculate angle vectors derived from current viewing angles.
        QM_AngleVectors( newState->angles, &packetEntity->vAngles.forward, &packetEntity->vAngles.right, &packetEntity->vAngles.up );
    }

    // Angle Vectors.
    const Vector3 vForward = packetEntity->vAngles.forward;
    const Vector3 vRight = packetEntity->vAngles.right;

    // Determine velocity based on actual old and current origin.
    packetEntity->moveInfo.xyzDirection = newState->angles;//Vector3( newState->origin ) - Vector3( packetEntity->prev.origin );
    // Get the move direction vectors.
    packetEntity->moveInfo.xyDirection = QM_Vector2FromVector3( packetEntity->moveInfo.xyzDirection );
    // Normalized move direction vectors.
    packetEntity->moveInfo.xyDirectionNormalized = QM_Vector2Normalize( packetEntity->moveInfo.xyDirection );
    packetEntity->moveInfo.xyzDirectionNormalized = QM_Vector3Normalize( packetEntity->moveInfo.xyzDirection );

    // Dot products.
    packetEntity->moveInfo.xDot = QM_Vector3DotProduct( packetEntity->moveInfo.xyDirectionNormalized, vRight );
    packetEntity->moveInfo.yDot = QM_Vector3DotProduct( packetEntity->moveInfo.xyDirectionNormalized, vForward );

    packetEntity->moveInfo.xySpeed = QM_Vector2Length( packetEntity->moveInfo.xyDirection );
    packetEntity->moveInfo.xyzSpeed = QM_Vector3Length( packetEntity->moveInfo.xyzDirection );

    // Resulting move flags.
    packetEntity->moveInfo.directionFlags = 0;


    if ( packetEntity->current.number == clgi.client->clientNumber + 1 ) {
        clgi.Print( PRINT_DEVELOPER, "%s: xDot(%f), yDot(%f)\n", __func__,
            packetEntity->moveInfo.xDot,
            packetEntity->moveInfo.yDot
        );
        clgi.Print( PRINT_DEVELOPER, "%s: xDirection(%f), yDirection(%f), zDirection(%f)\n", __func__,
            packetEntity->moveInfo.xyzDirection.x,
            packetEntity->moveInfo.xyzDirection.y,
            packetEntity->moveInfo.xyzDirection.z
        );
    }
    // Are we even moving enough?
    if ( packetEntity->moveInfo.xySpeed > 0 ) {
        // Forward:
        if ( ( packetEntity->moveInfo.yDot > DIR_EPSILON ) /*|| ( pm->cmd.forwardmove > 0 )*/ ) {
            packetEntity->moveInfo.directionFlags |= PM_MOVEDIRECTION_FORWARD;
        // Back:
        } else if ( ( -packetEntity->moveInfo.yDot > DIR_EPSILON ) /*|| ( pm->cmd.forwardmove < 0 )*/ ) {
            packetEntity->moveInfo.directionFlags |= PM_MOVEDIRECTION_BACKWARD;
        }
        // Right: (Only if the dotproduct is so, or specifically only side move is pressed.)
        if ( ( packetEntity->moveInfo.xDot > DIR_EPSILON ) /*|| ( !pm->cmd.forwardmove && pm->cmd.sidemove > 0 )*/ ) {
            packetEntity->moveInfo.directionFlags |= PM_MOVEDIRECTION_RIGHT;
        // Left: (Only if the dotproduct is so, or specifically only side move is pressed.)
        } else if ( ( -packetEntity->moveInfo. xDot > DIR_EPSILON ) /*|| ( !pm->cmd.forwardmove && pm->cmd.sidemove < 0 )*/ ) {
            packetEntity->moveInfo.directionFlags |= PM_MOVEDIRECTION_LEFT;
        }

        //// Running:
        //if ( packetEntity->moveInfo.xySpeed > 0 ) {
        //    packetEntity->moveInfo.directionFlags |= PM_MOVEDIRECTION_RUN;
        //// Walking:
        //} else if ( packetEntity->moveInfo.xySpeed > 1 ) {
        //    packetEntity->moveInfo.directionFlags |= PM_MOVEDIRECTION_WALK;
        //}
    }
}
#endif

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

        // WID: TODO: Move to client where it determines old/new states.
        #if 0 
        /**
        *   Determine certain properties in relation to movement.
        **/
        CLG_PacketEntity_DetermineMoveDirection( packetEntity, newState );
        #endif

        /**
        *   Act according to the entity Type:
        **/
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