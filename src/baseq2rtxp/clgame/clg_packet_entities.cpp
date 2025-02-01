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

// WID: TODO: Move to client where it determines old/new states?
#if 1 
/**
*	@brief	Determine the rotation of movement relative to the facing dir.
**/
static void CLG_PacketEntity_DetermineMoveDirection( centity_t *packetEntity, entity_state_t *newState, const bool isLocalClientEntity ) {
    // if it's moving to where is looking, it's moving forward
    // The desired yaw for the lower body.
    static constexpr float DIR_EPSILON = 0.3f;
    static constexpr float WALK_EPSILON = 5.0f;
    static constexpr float RUN_EPSILON = 100.f;

    // Determine...
    //const bool isLocalClientEntity = CLG_IsClientEntity( newState );

    // If we're not the local client entity, we don't want to update these values unless we're in a new serverframe.
    if ( !isLocalClientEntity ) {
        if ( packetEntity->moveInfo.current.serverTime >= QMTime::FromMilliseconds( clgi.client->servertime ) ) {
            return;
        }
    }

    /**
    *   Backup into previous moveInfo state:
    **/
    packetEntity->moveInfo.previous = packetEntity->moveInfo.current;

    // Update otherwise.
    packetEntity->moveInfo.current.serverTime = QMTime::FromMilliseconds( clgi.client->servertime );

    /**
    *   First determine if we need to recalculate the forward/right/up vectors.
    **/
    Vector3 currentAngles = newState->angles;
    Vector3 previousAngles = packetEntity->current.angles;

    // If we're the local client player, just use the PREDICTED player_state_t vAngles instead.
    if ( isLocalClientEntity ) {
        currentAngles = clgi.client->predictedState.currentPs.viewangles;
        previousAngles = clgi.client->predictedState.lastPs.viewangles;

        //packetEntity->vAngles.forward = clgi.client->v_forward;
        //packetEntity->vAngles.right = clgi.client->v_right;
        //packetEntity->vAngles.up = clgi.client->v_up;
    }
    // Avoid usage of more expensive Vector3 compare Operator here. Instead thus any change at all means a recalculation.
    if ( currentAngles.x != previousAngles.x || currentAngles.y != previousAngles.y || currentAngles.x != previousAngles.x ) {
        // Calculate angle vectors derived from current viewing angles.
        QM_AngleVectors( currentAngles, &packetEntity->vAngles.forward, &packetEntity->vAngles.right, &packetEntity->vAngles.up );
    }

    /**
    *   Calculate the origin offset for use with determining the move direction.
    **/    
    // Default to zero.
    Vector3 offset = QM_Vector3Zero();
    Vector3 currentOrigin = newState->origin;
    Vector3 previousOrigin = packetEntity->current.origin;
    // If we're the local client player, use the PREDICTED player_state_t origins instead.
    if ( isLocalClientEntity ) {
        currentOrigin = clgi.client->predictedState.currentPs.pmove.origin;
        previousOrigin = clgi.client->predictedState.lastPs.pmove.origin;
    }
    // Update and adjust ONLY if the origin has changed between frames.
    // Avoid usage of more expensive Vector3 compare Operator here. Instead thus any change at all means a recalculation.
    if ( previousOrigin.x != currentOrigin.x || previousOrigin.y != currentOrigin.y || previousOrigin.z != currentOrigin.z ) {
        // Update offset.
        offset = currentOrigin - previousOrigin;

        // Angle Vectors.
        const Vector3 &vForward = packetEntity->vAngles.forward;
        const Vector3 &vRight = packetEntity->vAngles.right;

        // Get the move direction vectors.
        packetEntity->moveInfo.current.xyDirection = QM_Vector2FromVector3( offset );
        packetEntity->moveInfo.current.xyzDirection = offset;//Vector3( newState->origin ) - Vector3( packetEntity->prev.origin );
        // Normalized move direction vectors.
        packetEntity->moveInfo.current.xyDirectionNormalized = QM_Vector2Normalize( packetEntity->moveInfo.current.xyDirection );
        packetEntity->moveInfo.current.xyzDirectionNormalized = QM_Vector3Normalize( packetEntity->moveInfo.current.xyzDirection );
        // Dot products.
        packetEntity->moveInfo.current.xDot = QM_Vector3DotProduct( packetEntity->moveInfo.current.xyDirectionNormalized, vRight );
        packetEntity->moveInfo.current.yDot = QM_Vector3DotProduct( packetEntity->moveInfo.current.xyDirectionNormalized, vForward );
        // Speeds.
        packetEntity->moveInfo.current.xySpeed = QM_Vector2Length( packetEntity->moveInfo.current.xyDirection );
        packetEntity->moveInfo.current.xyzSpeed = QM_Vector3Length( packetEntity->moveInfo.current.xyzDirection );

        // Clean slate flags.
        packetEntity->moveInfo.current.flags = 0;

        // Determine flags based on 2D plane movement.
        if ( packetEntity->moveInfo.current.xySpeed > 0 ) {
            // Forward:
            if ( ( packetEntity->moveInfo.current.yDot > DIR_EPSILON ) /*|| ( pm->cmd.forwardmove > 0 )*/ ) {
                packetEntity->moveInfo.current.flags |= CLG_MOVEINFOFLAG_DIRECTION_FORWARD;
            // Back:
            } else if ( ( -packetEntity->moveInfo.current.yDot > DIR_EPSILON ) /*|| ( pm->cmd.forwardmove < 0 )*/ ) {
                packetEntity->moveInfo.current.flags |= CLG_MOVEINFOFLAG_DIRECTION_BACKWARD;
            }
            // Right: (Only if the dotproduct is so, or specifically only side move is pressed.)
            if ( ( packetEntity->moveInfo.current.xDot > DIR_EPSILON ) /*|| ( !pm->cmd.forwardmove && pm->cmd.sidemove > 0 )*/ ) {
                packetEntity->moveInfo.current.flags |= CLG_MOVEINFOFLAG_DIRECTION_RIGHT;
            // Left: (Only if the dotproduct is so, or specifically only side move is pressed.)
            } else if ( ( -packetEntity->moveInfo.current.xDot > DIR_EPSILON ) /*|| ( !pm->cmd.forwardmove && pm->cmd.sidemove < 0 )*/ ) {
                packetEntity->moveInfo.current.flags |= CLG_MOVEINFOFLAG_DIRECTION_LEFT;
            }

            // WID: TODO: Determine running, or walking.
        }

        //clgi.Print( PRINT_DEVELOPER, "%s: (localClient) directionFlags(%i), xyzDirectionNormalized(%f, %f, %f)\n", __func__,
        //    packetEntity->moveInfo.directionFlags,
        //    packetEntity->moveInfo.xyzDirectionNormalized.x,
        //    packetEntity->moveInfo.xyzDirectionNormalized.y,
        //    packetEntity->moveInfo.xyzDirectionNormalized.z
        //);
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
            .frame = packetEntity->refreshEntity.frame,
            //.oldframe = packetEntity->refreshEntity.oldframe,
            .id = RENTITIY_RESERVED_COUNT + packetEntity->id,
            .bonePoses = bonePoses,
        };

        // WID: TODO: Move to client where it determines old/new states.
        #if 1 
        /**
        *   Determine certain properties in relation to movement.
        **/
        //CLG_PacketEntity_DetermineMoveDirection( packetEntity, newState );
        #endif

        /**
        *   Act according to the entity Type:
        **/
        switch ( newState->entityType ) {
        // Beams:
        case ET_BEAM:
            CLG_PacketEntity_AddBeam( packetEntity, &packetEntity->refreshEntity, newState );
            break;
        // Gibs:
        case ET_GIB:
            CLG_PacketEntity_AddGib( packetEntity, &packetEntity->refreshEntity, newState );
            break;
        // Items:
        case ET_ITEM:
            CLG_PacketEntity_AddItem( packetEntity, &packetEntity->refreshEntity, newState );
            break;
        // Monsters:
        case ET_MONSTER:
            // First determine movement properties.
            //CLG_PacketEntity_DetermineMoveDirection( packetEntity, newState, false );
            // Add Monster Entity.
            CLG_PacketEntity_AddMonster( packetEntity, &packetEntity->refreshEntity, newState );
            continue;
            break;
        // Players:
        case ET_PLAYER:
            // First determine movement properties.
            CLG_PacketEntity_DetermineMoveDirection( packetEntity, newState, CLG_IsClientEntity( newState ) );
            // Add Player Entity.
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