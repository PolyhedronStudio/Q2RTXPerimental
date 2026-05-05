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

// Uncomment these to allow debug rendering of packet entity bounds via CLG_DebugDrawPacketEntityBounds.


/**
*	@brief	Submit a world-space debug AABB for a packet entity when enabled by cvar.
*	@param	packetEntity	Entity carrying decoded mins/maxs and current state.
*	@note	This is intentionally lightweight and only runs when
*			`cl_debug_draw_entity_bounds` is enabled.
**/
static void CLG_DebugDrawPacketEntityBounds( const centity_t *packetEntity ) {
    /**
    *	Guard against disabled cvar and invalid entity pointer.
    **/
    if ( !cl_debug_draw_entity_bounds || !cl_debug_draw_entity_bounds->integer ) {
        return;
    }
    if ( !packetEntity ) {
        return;
    }

    /**
    *	Skip non-solid entities because they do not have gameplay collision bounds.
    **/
    if ( packetEntity->current.solid == SOLID_NOT ) {
        return;
    }

    /**
    *	Skip brushmodel solids for now, because their bounds are represented via BSP hulls
    *	and not by packed per-entity mins/maxs in this path.
    **/
    if ( packetEntity->current.solid == BOUNDS_BRUSHMODEL ) {
        return;
    }

    /**
    *	Build world-space AABB from decoded model-space mins/maxs and the lerped origin.
    **/
    vec3_t world_mins = {};
    vec3_t world_maxs = {};
    const Vector3 origin = packetEntity->lerpOrigin;
    world_mins[ 0 ] = origin.x + packetEntity->mins.x;
    world_mins[ 1 ] = origin.y + packetEntity->mins.y;
    world_mins[ 2 ] = origin.z + packetEntity->mins.z;
    world_maxs[ 0 ] = origin.x + packetEntity->maxs.x;
    world_maxs[ 1 ] = origin.y + packetEntity->maxs.y;
    world_maxs[ 2 ] = origin.z + packetEntity->maxs.z;

    /**
    *	Queue the bounds via renderer-facing debug API.
    **/
	if ( cl_debug_draw_entity_bounds->integer == 1 ) {
		clgi.R_DrawDebugBox( world_mins, world_maxs, U32_RED );
	}

	/**
	*	Queue the bounds sphere.
	**/
	if ( cl_debug_draw_entity_bounds->integer == 2 ) {
		const Vector3 center = QM_BBox3Center( { world_mins, world_maxs } );
		clgi.R_DrawDebugSphere( &center.x, QM_BBox3Radius( { world_mins, world_maxs } ), U32_RED );
	}

	/**
	*	Queue the bounds cylinder.
	**/
	const float half_size_x = ( world_maxs[ 0 ] - world_mins[ 0 ] ) * 0.5f;
	const float half_size_y = ( world_maxs[ 1 ] - world_mins[ 1 ] ) * 0.5f;

	if ( cl_debug_draw_entity_bounds->integer == 3 ) {
	// Build cylinder endpoints as vertical axis through the AABB center in XY.
		vec3_t cylinder_start = {
			( world_mins[ 0 ] + world_maxs[ 0 ] ) * 0.5f,
			( world_mins[ 1 ] + world_maxs[ 1 ] ) * 0.5f,
			world_mins[ 2 ]
		};
		vec3_t cylinder_end = {
			( world_mins[ 0 ] + world_maxs[ 0 ] ) * 0.5f,
			( world_mins[ 1 ] + world_maxs[ 1 ] ) * 0.5f,
			world_maxs[ 2 ]
		};
		// Use the largest horizontal half-extent so the cylinder encloses the AABB in XY.

		const float cylinder_radius = ( half_size_x > half_size_y ) ? half_size_x : half_size_y;
		clgi.R_DrawDebugCylinder( cylinder_start, cylinder_end, cylinder_radius, U32_RED );
	}

    /**
    *	Queue the bounds capsule.
    **/
	if ( cl_debug_draw_entity_bounds->integer == 4 ) {
		const float center_x = ( world_mins[ 0 ] + world_maxs[ 0 ] ) * 0.5f;
		const float center_y = ( world_mins[ 1 ] + world_maxs[ 1 ] ) * 0.5f;
		const float half_size_z = ( world_maxs[ 2 ] - world_mins[ 2 ] ) * 0.5f;
		const float capsule_radius = ( half_size_x > half_size_y ) ? half_size_x : half_size_y;
		// Keep hemisphere centers inside the AABB height. If the box is too short, collapse to a sphere.
		const float half_segment = ( half_size_z > capsule_radius ) ? ( half_size_z - capsule_radius ) : 0.0f;
		const float center_z = ( world_mins[ 2 ] + world_maxs[ 2 ] ) * 0.5f;
		vec3_t capsule_start = {
			center_x,
			center_y,
			center_z - half_segment
		};
		vec3_t capsule_end = {
			center_x,
			center_y,
			center_z + half_segment
		};
		clgi.R_DrawDebugCapsule( capsule_start, capsule_end, capsule_radius, U32_RED );
	}

	if ( cl_debug_draw_entity_bounds->integer == 5 ) {
        // Use render-time lerped angles so the debug facing matches the visual orientation.
        Vector3 facingForward = QM_Vector3Zero();
        QM_AngleVectors( packetEntity->lerpAngles, &facingForward, nullptr, nullptr );

        // Determine a robust origin near the entity center and a readable direction length.
        const Vector3 center = QM_BBox3Center( { world_mins, world_maxs } );
        const float half_size_z = ( world_maxs[ 2 ] - world_mins[ 2 ] ) * 0.5f;
        const float max_half_size_xy = ( half_size_x > half_size_y ) ? half_size_x : half_size_y;
        const float max_half_size_xyz = ( max_half_size_xy > half_size_z ) ? max_half_size_xy : half_size_z;
        const float facing_length = ( max_half_size_xyz * 2.0f > 16.0f ) ? ( max_half_size_xyz * 2.0f ) : 16.0f;

        // Build a world-space endpoint in the current facing direction.
        const Vector3 facingTarget = {
            center.x + ( facingForward.x * facing_length ),
            center.y + ( facingForward.y * facing_length ),
            center.z + ( facingForward.z * facing_length )
        };

        // Draw both a line and an arrow to make direction and heading immediately obvious.
        clgi.R_DrawDebugLine( &center.x, &facingTarget.x, U32_RED );
        const float arrow_head_length = ( facing_length * 0.25f > 8.0f ) ? ( facing_length * 0.25f ) : 8.0f;
        clgi.R_DrawDebugArrow( &center.x, &facingTarget.x, arrow_head_length, U32_RED );
	}
}


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
	case ET_TEMP_EVENT_ENTITY:
		break; // We don't want to render these.
	case ET_GENERIC:
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
	// Convert the predicted player_state_t into an entity_state_t for the predicted client entity.
	SG_PlayerStateToEntityState( clgi.client->clientNumber, &game.predictedState.currentPs, &game.predictedEntity.current, true, false );
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
            CLG_DebugDrawPacketEntityBounds( packetEntity );
            continue;
        }

        CLG_DebugDrawPacketEntityBounds( packetEntity );
    }
}