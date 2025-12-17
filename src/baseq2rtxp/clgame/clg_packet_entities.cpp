/********************************************************************
*
*
*	ClientGame: Packet Entities.
* 
*	Each Packet Entity comes with a specific Entity Type set for it
*	in the nextState. Most are added by CLG_PacketEntity_AddGeneric.
*
*	However for various distinct ET_* types there exist specific
*	submethods to handle those. This should bring some structure to
*	CLG_AddPacketEntities :-)
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_effects.h"
#include "clgame/clg_entities.h"
#include "clgame/clg_packet_entities.h"
#include "clgame/clg_temp_entities.h"

#include "sharedgame/sg_entity_types.h"
#include "sharedgame/sg_entities.h"


// WID: TODO: Move to client where it determines old/new states?
#if 1 
/**
*	@brief	Determine the rotation of movement relative to the facing dir.
**/
static void CLG_PacketEntity_DetermineMoveDirection( centity_t *packetEntity, entity_state_t *nextState, const bool isLocalClientEntity ) {
    // if it's moving to where is looking, it's moving forward
    // The desired yaw for the lower body.
    static constexpr float DIR_EPSILON = 0.3f;
    static constexpr float WALK_EPSILON = 5.0f;
    static constexpr float RUN_EPSILON = 100.f;

    // Determine...
    //const bool isLocalClientEntity = CLG_IsClientEntity( nextState );

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
    Vector3 currentAngles = nextState->angles;
    Vector3 previousAngles = packetEntity->current.angles;

    // If we're the local client player, just use the PREDICTED player_state_t vAngles instead.
    if ( isLocalClientEntity ) {
        currentAngles = game.predictedState.currentPs.viewangles;
        previousAngles = game.predictedState.lastPs.viewangles;

        //packetEntity->vAngles.forward = clgi.client->vForward;
        //packetEntity->vAngles.right = clgi.client->vRight;
        //packetEntity->vAngles.up = clgi.client->vUp;
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
    Vector3 currentOrigin = nextState->origin;
    Vector3 previousOrigin = packetEntity->current.origin;
    // If we're the local client player, use the PREDICTED player_state_t origins instead.
    if ( isLocalClientEntity ) {
        currentOrigin = game.predictedState.currentPs.pmove.origin;
        previousOrigin = game.predictedState.lastPs.pmove.origin;
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
        packetEntity->moveInfo.current.xyzDirection = offset;//Vector3( nextState->origin ) - Vector3( packetEntity->prev.origin );
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
*   @brief  Adds a packet entity according to its entity type.
*   @param  packetEntity    The client game entity to add the packet entity data to.
*   @param  nextState       The new entity state received for this packet entity.
*   @param  base_entity_flags   Base entity flags to apply.
*   @param  autorotate      The autorotate value for rotating items.
*   @return true if the entity is of a type that requires no further processing.
**/
static const bool AddPacketEntity( centity_t *packetEntity, entity_state_t *nextState, const int32_t base_entity_flags, const double autorotate ) {
    switch ( nextState->entityType ) {
        // Beams:
    case ET_BEAM:
        CLG_PacketEntity_AddBeam( packetEntity, &packetEntity->refreshEntity, nextState );
        break;
        // Gibs:
    case ET_GIB:
        CLG_PacketEntity_AddGib( packetEntity, &packetEntity->refreshEntity, nextState );
        break;
        // Items:
    case ET_ITEM:
        CLG_PacketEntity_AddItem( packetEntity, &packetEntity->refreshEntity, nextState );
        break;
        // Monsters:
    case ET_MONSTER:
        // First determine movement properties.
        //CLG_PacketEntity_DetermineMoveDirection( packetEntity, nextState, false );
        // Add Monster Entity.
        CLG_PacketEntity_AddMonster( packetEntity, &packetEntity->refreshEntity, nextState );
        return true;
        break;
        // Players:
    case ET_PLAYER:
        // First determine movement properties.
        CLG_PacketEntity_DetermineMoveDirection( packetEntity, nextState, CLG_IsLocalClientEntity( nextState ) );
        // Add Player Entity.
        CLG_PacketEntity_AddPlayer( packetEntity, &packetEntity->refreshEntity, nextState );
        return true;
        break;
        // Pushers:
    case ET_PUSHER:
        CLG_PacketEntity_AddPusher( packetEntity, &packetEntity->refreshEntity, nextState );
        break;
        // Spotlights:
    case ET_SPOTLIGHT:
        CLG_PacketEntity_AddSpotlight( packetEntity, &packetEntity->refreshEntity, nextState );
        break;
        // ET_GENERIC:
    case ET_GENERIC:
    case ET_TEMP_EVENT_ENTITY:
    default:
        CLG_PacketEntity_AddGeneric( packetEntity, &packetEntity->refreshEntity, nextState );
        break;
    }
    return false;
}

/**
*   @brief  Parses all received frame packet entities' new states, iterating over them
*           and (re-)applying any changes to the current and/or new refresh entity that
*           has a matching ID.
**/
void CLG_AddPacketEntities( void ) {
    // Base entity flags.
    int32_t base_entity_flags = 0;

    // Bonus items rotate at a fixed rate.
    const double autorotate = QM_AngleMod( (double)clgi.client->time * BASE_FRAMETIME_1000 );//AngleMod(clgi.client->time * 0.1f); // WID: 40hz: Adjusted.

    /**
    *   Setup the predicted client entity.
    **/
	SG_PlayerStateToEntityState( clgi.client->clientNumber, &clgi.client->predictedFrame.ps, &game.predictedEntity.current, true, false );
    // Backup a possible pointer to an already allocated cache so we can reapply it.
    skm_transform_t *bonePoses = game.predictedEntity.refreshEntity.bonePoses;
    // Setup the refresh entity ID to match that of the client game entity with the RESERVED_ENTITY_COUNT in mind.
    game.predictedEntity.refreshEntity = {
        // Store the frame of the previous refresh entity iteration so it'll default to that.
        .frame = game.predictedEntity.refreshEntity.frame,
        //.oldframe = packetEntity->refreshEntity.oldframe,
        .id = REFRESHENTITIY_RESERVED_PREDICTED_PLAYER,
        // Restore bone poses cache pointer.
        .bonePoses = bonePoses,
    };
    // Add the predicted client entity.
    AddPacketEntity( &game.predictedEntity, &game.predictedEntity.current, base_entity_flags, autorotate );

    // Iterate over this frame's entity states.
    for ( int32_t frameEntityNumber = 0; frameEntityNumber < clgi.client->frame.numEntities; frameEntityNumber++ ) {
		// Get the new received, to be 'next', entity state.
        entity_state_t *nextState = &clgi.client->entityStates[ ( clgi.client->frame.firstEntity + frameEntityNumber ) & PARSE_ENTITIES_MASK ];
		// Get the packet entity corresponding to the new state.
        centity_t *packetEntity = &clg_entities[ nextState->number ];

        // Backup a possible pointer to an already allocated cache so we can reapply it.
        skm_transform_t *bonePoses = packetEntity->refreshEntity.bonePoses;

		// Clear out the packet entity refresh entity, except for the bone poses cache pointer.
        // ID, and frame.
        //
        // Setup the refresh entity ID to match that of the client game entity with the RESERVED_ENTITY_COUNT in mind.
        packetEntity->refreshEntity = {
			// Store the frame of the previous refresh entity iteration so it'll default to that.
            .frame = packetEntity->refreshEntity.frame,
            //.oldframe = packetEntity->refreshEntity.oldframe,
            .id = REFRESHENTITIY_RESERVED_COUNT + packetEntity->id,
            // Restore bone poses cache pointer.
            .bonePoses = bonePoses,
        };

        // WID: TODO: Move to client where it determines old/new states.
        #if 1 
        /**
        *   Determine certain properties in relation to movement.
        **/
        //CLG_PacketEntity_DetermineMoveDirection( packetEntity, nextState );
        #endif

        /**
        *   Act according to the entity Type:
        **/
        if ( AddPacketEntity( packetEntity, nextState, base_entity_flags, autorotate ) ) {
            continue;
        }
    }
}