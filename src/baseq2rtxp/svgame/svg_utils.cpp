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

#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_gamelib.hpp"
#include "svgame/lua/svg_lua_signals.hpp"

#include "svgame/entities/svg_entities_pushermove.h"

#include "sharedgame/sg_tempentity_events.h"
#include "sharedgame/sg_means_of_death.h"
#include "sharedgame/sg_misc.h"



/**
*	@brief	Emit a single `TE_DEBUGTRAIL` segment (start->end).
**/
void SVG_DebugDrawLine_TE( const Vector3 &start, const Vector3 &end, const multicast_t multicastType, const bool reliable ) {
	gi.WriteUint8( svc_temp_entity );
	gi.WriteUint8( TE_DEBUGTRAIL );
	gi.WritePosition( &start, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
	gi.WritePosition( &end, MSG_POSITION_ENCODING_TRUNCATED_FLOAT );
	gi.multicast( &start, multicastType, reliable );
}

/**
*	@brief	Draw an axis-aligned bounding box by emitting 12 `TE_DEBUGTRAIL` line segments.
**/
void SVG_DebugDrawBBox_TE( const Vector3 &mins, const Vector3 &maxs, const multicast_t multicastType, const bool reliable ) {
	const Vector3 v000 = { mins[ 0 ], mins[ 1 ], mins[ 2 ] };
	const Vector3 v001 = { mins[ 0 ], mins[ 1 ], maxs[ 2 ] };
	const Vector3 v010 = { mins[ 0 ], maxs[ 1 ], mins[ 2 ] };
	const Vector3 v011 = { mins[ 0 ], maxs[ 1 ], maxs[ 2 ] };
	const Vector3 v100 = { maxs[ 0 ], mins[ 1 ], mins[ 2 ] };
	const Vector3 v101 = { maxs[ 0 ], mins[ 1 ], maxs[ 2 ] };
	const Vector3 v110 = { maxs[ 0 ], maxs[ 1 ], mins[ 2 ] };
	const Vector3 v111 = { maxs[ 0 ], maxs[ 1 ], maxs[ 2 ] };

	// Bottom rectangle
	SVG_DebugDrawLine_TE( v000, v100, multicastType, reliable );
	SVG_DebugDrawLine_TE( v100, v110, multicastType, reliable );
	SVG_DebugDrawLine_TE( v110, v010, multicastType, reliable );
	SVG_DebugDrawLine_TE( v010, v000, multicastType, reliable );

	// Top rectangle
	SVG_DebugDrawLine_TE( v001, v101, multicastType, reliable );
	SVG_DebugDrawLine_TE( v101, v111, multicastType, reliable );
	SVG_DebugDrawLine_TE( v111, v011, multicastType, reliable );
	SVG_DebugDrawLine_TE( v011, v001, multicastType, reliable );

	// Vertical edges
	SVG_DebugDrawLine_TE( v000, v001, multicastType, reliable );
	SVG_DebugDrawLine_TE( v100, v101, multicastType, reliable );
	SVG_DebugDrawLine_TE( v110, v111, multicastType, reliable );
	SVG_DebugDrawLine_TE( v010, v011, multicastType, reliable );
}

/**
*	@brief	Draw a cube centered at `center`.
**/
void SVG_DebugDrawCube_TE( const Vector3 &center, const float halfExtent, const multicast_t multicastType, const bool reliable ) {
	const Vector3 mins = { center[ 0 ] - halfExtent, center[ 1 ] - halfExtent, center[ 2 ] - halfExtent };
	const Vector3 maxs = { center[ 0 ] + halfExtent, center[ 1 ] + halfExtent, center[ 2 ] + halfExtent };
	SVG_DebugDrawBBox_TE( mins, maxs, multicastType, reliable );
}


/**
*
*
*
*	Mathemathical Utility Functions:
*		- Vector Projection from Source
*		- Set Move Direction from Angles
*
*
*
**/
/**
*   @brief  Project vector from a source point, to distance, based on forward/right angle vectors.
**/
const Vector3 SVG_Util_ProjectSource( const Vector3 &point, const Vector3 &distance, const Vector3 &forward, const Vector3 &right ) {
    return {
        point[ 0 ] + forward[ 0 ] * distance[ 0 ] + right[ 0 ] * distance[ 1 ],
        point[ 1 ] + forward[ 1 ] * distance[ 0 ] + right[ 1 ] * distance[ 1 ],
        point[ 2 ] + forward[ 2 ] * distance[ 0 ] + right[ 2 ] * distance[ 1 ] + distance[ 2 ]
    };
}
/**
*   @brief  Utility version of above that uses vec3_t args and result.
**/
void SVG_Util_ProjectSource( const vec3_t point, const vec3_t distance, const vec3_t forward, const vec3_t right, vec3_t result ) {
    // Call the new more modern SVG_Util_ProjectSource.
    const Vector3 _result = SVG_Util_ProjectSource( point, distance, forward, right );
    // Copy the resulting values into the result vec3_t array(ptr).
    VectorCopy( _result, result );
}



/**
*
*
*
*	Generic Entity Utility Functions:
*
*
*
**/
/**
*	@brief	Use to properly set an entity's origin. It will always assign to the authoritative
*			``ent->currentOrigin``, which is used for physics and linking. 
*			
*			Optionally, it can also	assign to the ``ent->s.origin``, which is what will be the 
*			delivered origin for all the clients to see. (This defaults to true.)
**/
void SVG_Util_SetEntityOrigin( svg_base_edict_t *ent, const Vector3 &origin, const bool assignToEntityState ) {
	// Assign to the authoritative currentOrigin.
	ent->currentOrigin = origin;
	// Optionally assign to the entityState origin.
	if ( assignToEntityState ) {
		ent->s.origin = origin;
	}
}
/**
*	@brief	Use to properly set an entity's angles. It will always assign to the authoritative
*			``ent->currentAngles``, which is used for physics and linking.
*
*			Optionally, it can also	assign to the ``ent->s.angles``, which is what will be the
*			delivered origin for all the clients to see. (This defaults to true.)
**/
void SVG_Util_SetEntityAngles( svg_base_edict_t *ent, const Vector3 &angles, const bool assignToEntityState ) {
	// Assign to the authoritative currentAngles.
	ent->currentAngles = angles;
	// Optionally assign to the entityState angles.
	if ( assignToEntityState ) {
		ent->s.angles = angles;
	}
}


/**
*	@brief	Basic Trigger initialization mechanism.
**/
void SVG_Util_InitTrigger( svg_base_edict_t *self ) {
	// Set the movedir vector based on the angles.
    if ( !VectorEmpty( self->s.angles ) ) {
		// Set the movedir vector based on the angles.
        SVG_Util_SetMoveDir( self->s.angles, self->movedir );
		// Assign the angles using the utility method to ensure consistency.
		SVG_Util_SetEntityAngles( self, self->s.angles, true );
    }
    //
    self->solid = SOLID_TRIGGER;
    self->movetype = MOVETYPE_NONE;
	// Set the model.
    if ( self->model.ptr ) {
        gi.setmodel( self, self->model.ptr );
    }
    // Ensure it is never sent over the wire to any clients.
    self->svFlags = SVF_NOCLIENT;
}

/**
*   @brief	Will set the movedir vector based on the angles.
*
*	@note	Will clear out the angles if clearAngles is true (default).
*
*			A value of -1 for any angle will be treated as straight down.
*			A value of -2 for any angle will be treated as straight up.
**/
void SVG_Util_SetMoveDir( Vector3 &angles, Vector3 &movedir, const bool clearAngles ) {
    static constexpr Vector3 VEC_UP = { 0., -1.,  0. };
    static constexpr Vector3 MOVEDIR_UP = { 0.,  0.,  1. };
    static constexpr Vector3 VEC_DOWN = { 0., -2.,  0. };
    static constexpr Vector3 MOVEDIR_DOWN = { 0.,  0., -1. };

    // Check for special cases first.
    // If straight up or down, set the movedir to the proper value.
    if ( VectorCompare( angles, VEC_UP ) ) {
        movedir = MOVEDIR_UP;
    } else if ( VectorCompare( angles, VEC_DOWN ) ) {
        movedir = MOVEDIR_DOWN;
        // Otherwise, use the normal angle to vector conversion.
    } else {
        QM_AngleVectors( angles, &movedir, NULL, NULL );
    }
    // Clear out the angles, we don't need them anymore.
    if ( clearAngles ) {
        VectorClear( angles );
    }
}

/**
*   @brief  Determines the client that is most near to the entity,
*           and returns its length for ( ent->origin - client->origin ).
**/
const double SVG_Util_ClosestClientForEntity( svg_base_edict_t *ent ) {
    // The best distance will always be flt_max.
    double bestDistance = CM_MAX_WORLD_SIZE + 1.f;

	// Ensure entity is active.
    if ( !SVG_Entity_IsActive( ent ) ) {
        // Debug print.
        gi.dprintf( "%s: Entity isn't active.\n", __func__ );
        return bestDistance;
    }

    for ( int32_t n = 1; n <= maxclients->value; n++ ) {
        // Get client.
        svg_base_edict_t *clientEntity = g_edict_pool.EdictForNumber( n );//&g_edicts[ n ];
        // Ensure is active and alive player.
        if ( !SVG_Entity_IsClient( clientEntity, true ) ) {
            continue;
        }

        // Calculate distance.
        const double distanceLength = QM_Vector3Length( Vector3( ent->s.origin ) - clientEntity->s.origin );

        // Assign as best distance if nearer to ent.
        if ( distanceLength < bestDistance ) {
            bestDistance = distanceLength;
        }
    }

    // Return result.
    return bestDistance;
}



/**
*
*
*
*	(Temp-) Entity Event Utility Functions:
*
*
*
**/
/**
*   @brief Adds an event+parm and twiddles the event counter.
**/
void SVG_Util_AddEvent( svg_base_edict_t *ent, const sg_entity_events_t event, const int32_t eventParm0, const int32_t eventParm1 ) {
	// Sanity check.
    if ( !event ) {
		// Debug about zero event.
        gi.dprintf( "%s: zero event added for entity(#%i), lastEventTime(%" PRId64 ")\n", __func__, ent->s.number, ent->eventTime );
        return;
    }

    /**
    *   Clients need to add the event in playerState_t instead of entityState_t.
    **/
    if ( ent->client ) {
		// Set the event.
        int32_t bits = ent->client->ps.externalEvent & EV_EVENT_BITS;
		// EV_EVENT_BIT1 and EV_EVENT_BIT2 are a two bit counter, so go to the next one.
        bits = ( bits + EV_EVENT_BIT1 ) & EV_EVENT_BITS;
		// Set the event and parm.
        ent->client->ps.externalEvent = event | bits;
        ent->client->ps.externalEventParm0 = eventParm0;
        ent->client->ps.externalEventParm1 = eventParm1;
		// Stamp the time of the event.
        ent->client->ps.externalEventTime = level.time.Milliseconds();
    /**
    *   Non-Clients just add it to the entityState_t
    **/
    } else {
		// Set the event.
        int32_t bits = ent->s.event & EV_EVENT_BITS;
		// EV_EVENT_BIT1 and EV_EVENT_BIT2 are a two bit counter, so go to the next one.
        bits = ( bits + EV_EVENT_BIT1 ) & EV_EVENT_BITS;
		// Set the event and parm.
        ent->s.event = event | bits;
        ent->s.eventParm0 = eventParm0;
        ent->s.eventParm1 = eventParm1;
    }
	// Stamp the time of the event.
    ent->eventTime = level.time;
}

/**
*   @brief  Use for non-pmove events that would also be predicted on the
*           client side: jumppads and item pickups
*           Adds an event+parm and twiddles the event counter
**/
void SVG_Util_AddPredictableEvent( svg_base_edict_t *ent, const sg_entity_events_t event, const int32_t eventParm ) {
    // Make sure we have a client.
    if ( !ent->client ) {
        // Warn about it.
        gi.dprintf( "%s: Entity(#%i) is not a client, cannot add predictable event.\n", __func__, ent->s.number );
        return;
    }

    // Otherwise, add the predictable event.
    SG_PlayerState_AddPredictableEvent( event, eventParm, &ent->client->ps );
}

/**
*   @brief  Adds a temp entity event at the given origin.
*	@param	snapOrigin	If true, will snap the origin to 13 bits float precision.
**/
svg_base_edict_t *SVG_Util_CreateTempEventEntity( const Vector3 &origin, const sg_entity_events_t event, const int32_t eventParm0, const int32_t eventParm1, const bool snapOrigin /*= false*/, const QMTime eventDuration /* = QMTime::FromMilliseconds( EVENT_VALID_MSEC )*/ ) {
    /**
    *   We don't need regular spawning etc dispatching and what not.
    * 
	*   Take the raw approach here.
    **/
	// Create the temporary entity event entity.
    //EdictTypeInfo typeInfo = EdictTypeInfo::GetInfoByWorldSpawnClassName( "svg_base_edict_t" );
    //spawnEdict = 
    
    //EdictTypeInfo *typeInfo = EdictTypeInfo::GetInfoByWorldSpawnClassName( "svg_base_edict_t" );
    //svg_base_edict_t *tempEventEntity = typeInfo->allocateEdictInstanceCallback( nullptr );
    //g_edict_pool.EmplaceNextFreeEdict( tempEventEntity );

	// Allocate the next free edict for the temp event entity.
    
    //svg_base_edict_t *tempEventEntity = g_edict_pool.AllocateNextFreeEdict<svg_base_edict_t>();
    svg_base_edict_t *tempEventEntity = g_edict_pool.AllocateNextFreeEdict<svg_base_edict_t>( "svg_temp_event_entity_t" );
    
    // Set the actual entity event to part of entityState_t type.
    tempEventEntity->s.entityType = ET_TEMP_EVENT_ENTITY + event;
	// However, do change the actual 'classname' to something meaningful for temp entities.
	//tempEventEntity->classname = svg_level_qstring_t::from_char_str( "svg_temp_event_entity_t" );

	// Stuff the eventParms in the entityState_t's eventParms.
	tempEventEntity->s.eventParm0 = eventParm0;
    tempEventEntity->s.eventParm1 = eventParm1;

	// Ensure it will be freed properly later after the event has finished processing.
	tempEventEntity->freeAfterEvent = true;
    // Setup the actual time of the event.
	tempEventEntity->eventTime = level.time;
	// Setup the duration of the event.
	tempEventEntity->eventDuration = eventDuration;

    // Last but not least, set the origin, link it and return it.
    // Now snap the origin into the entityState_t.
	// <Q2RTXP>: WID: Use proper snapping function.
    //tempEventEntity->s.origin = ( snapOrigin ? QM_Vector3Snap( origin ) : origin );
	SVG_Util_SetEntityOrigin( tempEventEntity, ( snapOrigin ? QM_Vector3Snap( origin ) : origin ), true );
	
	// Can be used as a temp event entity its second origin to point at.
	// We make sure to initialize it to the origin, to prevent the messaging code from
	// wiring it throughout old_origin using snapped float truncation.
	//tempEventEntity->s.old_origin = tempEventEntity->s.origin;

    // Link it in for PVS etc.
    gi.linkentity( tempEventEntity );

	// Return the temp entity.
    return tempEventEntity;
}



/**
*
*
*
*	'Touch' & 'Solid' Entity Utility Functions:
*
*
*
**/
/**
*   @brief  
**/
void SVG_Util_TouchTriggers(svg_base_edict_t *ent) {
    svg_base_edict_t *hit;

    static svg_base_edict_t *touchedEdicts[ MAX_EDICTS ] = {};
	std::fill( std::begin( touchedEdicts ), std::end( touchedEdicts ), nullptr );

	// Dead Clients and (NPC) Monsters don't activate triggers.
    if ( ( ent->client || ( ent->svFlags & SVF_MONSTER ) ) && ( ent->health <= 0 ) ) {
        return;
    }

	const Vector3 absMin = ent->absMin;
    const Vector3 absMax = ent->absMax;
    const int32_t num = gi.BoxEdicts( &absMin, &absMax, touchedEdicts, MAX_EDICTS, AREA_TRIGGERS );

    // be careful, it is possible to have an entity in this
    // list removed before we get to it (killtriggered)
    for (int32_t i = 0 ; i < num ; i++) {
        hit = touchedEdicts[i];
        if ( !hit ) {
            continue;
        }
        if ( !hit->inUse ) {
            continue;
        }
        if ( !hit->HasTouchCallback() ) {
            continue;
        }

        hit->DispatchTouchCallback( ent, nullptr, nullptr );
		// <Q2RTXP>: WID: Prevent the touch from being called again if the previous
        // touch 'removed' the entity, it is not in use.
        if ( !ent->inUse ) {
            break;
        }
    }
}

/**
*   @brief  Call after linking a new trigger in during gameplay
*           to force all entities it covers to immediately touch it
**/
void SVG_Util_TouchSolids(svg_base_edict_t *ent) {
    svg_base_edict_t *hit = nullptr;

    static svg_base_edict_t *touchedEdicts[ MAX_EDICTS ] = {};
    std::fill( std::begin( touchedEdicts ), std::end( touchedEdicts ), nullptr );

    const Vector3 absMin = ent->absMin;
    const Vector3 absMax = ent->absMax;
    const int32_t num = gi.BoxEdicts( &absMin, &absMax, touchedEdicts, MAX_EDICTS, AREA_SOLID );

    // be careful, it is possible to have an entity in this
    // list removed before we get to it (killtriggered)
    for (int32_t i = 0 ; i < num ; i++) {
        hit = touchedEdicts[i];
        if ( !hit ) {
            continue;
        }
        if ( !hit->inUse ) {
            continue;
        }
        if ( !hit->HasTouchCallback() ) {
            continue;
        }

        hit->DispatchTouchCallback( ent, nullptr, nullptr );
        if ( !ent->inUse ) {
            break;
        }
    }
}


/**
*   @brief  Scan for projectiles between our movement positions
*           to see if we need to collide against them.
**/
void SVG_Util_TouchProjectiles( svg_base_edict_t *ent, const Vector3 &previous_origin ) {
    struct skipped_projectile {
        svg_base_edict_t *projectile;
        int32_t spawn_count;
    };
    // a bit ugly, but we'll store projectiles we are ignoring here.
    static std::vector<skipped_projectile> skipped;

    while ( true ) {
        svg_trace_t tr = SVG_Trace( previous_origin, ent->mins, ent->maxs, ent->s.origin, ent, ( ent->clipMask | CONTENTS_PROJECTILE ) );

        if ( tr.fraction == 1.0f ) {
            break;
        }
        else if ( !( tr.ent->svFlags & SVF_PROJECTILE ) ) {
            break;
        }

        // always skip this projectile since certain conditions may cause the projectile
        // to not disappear immediately
        tr.ent->svFlags &= ~SVF_PROJECTILE;
        skipped.push_back( { tr.ent, tr.ent->spawn_count } );

        // Q2RE: if we're both players and it's coop, allow the projectile to "pass" through
        // However, we got no methods like them, but we do have an optional check for no friendly fire.
        if ( ent->client && tr.ent->owner && tr.ent->owner->client 
            && SVG_OnSameTeam( ent, tr.ent->owner ) && !( dmflags->integer & DF_NO_FRIENDLY_FIRE ) ) {
            continue;
        }

        // Call impact(touch) triggers.
        SVG_Impact( ent, &tr );
    }

    for ( auto &skip : skipped ) {
        if ( skip.projectile->inUse && skip.projectile->spawn_count == skip.spawn_count ) {
            skip.projectile->svFlags |= SVF_PROJECTILE;
        }
    }

    skipped.clear();
}
/*
=================
SVG_Util_KillBox

Kills all entities that would touch the proposed new positioning
of ent.  Ent should be unlinked before calling this!
=================
*/
//const bool SVG_Util_KillBox(svg_base_edict_t *ent, const bool bspClipping ) {
//    // don't telefrag as spectator... or in noclip
//    if ( ent->movetype == MOVETYPE_NOCLIP ) {
//        return true;
//    }
//
//    cm_contents_t mask = ( CONTENTS_MONSTER | CONTENTS_PLAYER );
//
//    //// [Paril-KEX] don't gib other players in coop if we're not colliding
//    //if ( from_spawning && ent->client && coop->integer && !G_ShouldPlayersCollide( false ) )
//    //    mask &= ~CONTENTS_PLAYER;
//    static svg_base_edict_t *touchedEdicts[ MAX_EDICTS ];
//    memset( touchedEdicts, 0, MAX_EDICTS );
//
//    int32_t num = gi.BoxEdicts( ent->absMin, ent->absMax, touchedEdicts, MAX_EDICTS, AREA_SOLID );
//    for ( int32_t i = 0; i < num; i++ ) {
//        // Pointer to touched entity.
//        svg_base_edict_t *hit = touchedEdicts[ i ];
//        // Make sure its valid.
//        if ( hit == nullptr ) {
//            continue;
//        }
//        // Don't killbox ourselves.
//        if ( hit == ent ) {
//            continue;
//        // Skip entities that are not in use, no takedamage, not solid, or solid_bsp/solid_trigger.
//        } else if ( !hit->inUse || !hit->takedamage || !hit->solid || hit->solid == SOLID_TRIGGER || hit->solid == SOLID_BSP ) {
//            continue;
//        } else if ( hit->client && !( mask & CONTENTS_PLAYER ) ) {
//            continue;
//        }
//
//        svg_trace_t clip = {};
//        if ( ( ent->solid == SOLID_BSP || ( ent->svFlags & SVF_HULL ) ) && bspClipping ) {
//            clip = SVG_Trace(ent, hit->s.origin, hit->mins, hit->maxs, hit->s.origin, SVG_GetClipMask(hit));
//
//            if ( clip.fraction == 1.0f ) {
//                continue;
//            }
//        }
//
//        // nail it
//        if ( clip.fraction ) {
//            if ( clip.ent ) {
//                vec3_t dir;
//                VectorSubtract( hit->s.origin, clip.ent->s.origin, dir );
//                VectorNormalize( dir );
//
//                if ( clip.plane.dist ) {
//                    SVG_DamageEntity( hit, ent, ent, dir, ent->s.origin, clip.plane.normal, 100000, 0, DAMAGE_NO_PROTECTION, MEANS_OF_DEATH_TELEFRAGGED );
//                } else {
//                    SVG_DamageEntity( hit, ent, ent, dir, ent->s.origin, vec3_origin, 100000, 0, DAMAGE_NO_PROTECTION, MEANS_OF_DEATH_TELEFRAGGED );
//                }
//            } else {
//                if ( clip.plane.dist ) {
//                    SVG_DamageEntity( hit, ent, ent, vec3_origin, ent->s.origin, clip.plane.normal, 100000, 0, DAMAGE_NO_PROTECTION, MEANS_OF_DEATH_TELEFRAGGED );
//                } else {
//                    SVG_DamageEntity( hit, ent, ent, vec3_origin, ent->s.origin, vec3_origin, 100000, 0, DAMAGE_NO_PROTECTION, MEANS_OF_DEATH_TELEFRAGGED );
//                }
//            }
//        } else {
//            SVG_DamageEntity( hit, ent, ent, vec3_origin, ent->s.origin, vec3_origin, 100000, 0, DAMAGE_NO_PROTECTION, MEANS_OF_DEATH_TELEFRAGGED );
//        }
//
//        // if we didn't kill it, fail
//        if ( hit->solid ) {
//            return false;
//        }
//    }
//
//    return true;        // all clear
//}
/**
*   @brief  Kills all entities that would touch the proposed new positioning
*           of ent.  Ent should be unlinked before calling this!
**/
const bool SVG_Util_KillBox( svg_base_edict_t *ent, const bool bspClipping, sg_means_of_death_t meansOfDeath = MEANS_OF_DEATH_TELEFRAGGED ) {
    // don't telefrag as spectator... or in noclip
    if ( ent->movetype == MOVETYPE_NOCLIP ) {
        return true;
    }

    cm_contents_t mask = ( CONTENTS_MONSTER | CONTENTS_PLAYER );

    //// [Paril-KEX] don't gib other players in coop if we're not colliding
    //if ( from_spawning && ent->client && coop->integer && !G_ShouldPlayersCollide( false ) )
    //    mask &= ~CONTENTS_PLAYER;
    static svg_base_edict_t *touchedEdicts[ MAX_EDICTS ];
    std::fill( std::begin( touchedEdicts ), std::end( touchedEdicts ), nullptr );

    const Vector3 absMin = ent->absMin;
    const Vector3 absMax = ent->absMax;
    int32_t num = gi.BoxEdicts( &absMin, &absMax, touchedEdicts, MAX_EDICTS, AREA_SOLID );
    
    for ( int32_t i = 0; i < num; i++ ) {
        #if 0
        svg_base_edict_t *hit = touchedEdicts[ i ];

        if ( hit == ent )
            continue;
		// Will prevent spawn point telefragging if no client is assigned yet.
        else if ( hit != nullptr && !hit->client && hit->s.entityType == ET_PLAYER )
            continue;
        else if ( !hit->inUse || !hit->takedamage || !hit->solid || hit->solid == SOLID_TRIGGER || hit->solid == SOLID_BSP )
            continue;
        else if ( hit->client && !( mask & CONTENTS_PLAYER ) )
            continue;

        if ( ( ent->solid == SOLID_BSP || ( ent->svFlags & SVF_HULL ) ) && bspClipping ) {
            svg_trace_t clip = gi.clip( ent, hit->s.origin, hit->mins, hit->maxs, hit->s.origin, SVG_GetClipMask( hit ) );

            if ( clip.fraction == 1.0f )
                continue;
        }

        // [Paril-KEX] don't allow telefragging of friends in coop.
        // the player that is about to be telefragged will have collision
        // disabled until another time.
        //if ( ent->client && hit->client && coop->integer ) {
        //    hit->clipMask &= ~CONTENTS_PLAYER;
        //    ent->clipMask &= ~CONTENTS_PLAYER;
        //    continue;
        //}

        SVG_DamageEntity( hit, ent, ent, vec3_origin, ent->s.origin, vec3_origin, 100000, 0, DAMAGE_NO_PROTECTION, meansOfDeath );
        #else
        // Pointer to touched entity.
        svg_base_edict_t *hit = touchedEdicts[ i ];
        // Make sure its valid.
        if ( hit == nullptr ) {
            continue;
        }
        // Don't killbox ourselves.
        if ( hit == ent ) {
            continue;
        // Skip entities that are not in use, no takedamage, not solid, or solid_bsp/solid_trigger.
        } else if ( !hit->inUse || !hit->takedamage || !hit->solid || hit->solid == SOLID_TRIGGER || hit->solid == SOLID_BSP ) {
            continue;
        } else if ( hit->client && !( mask & CONTENTS_PLAYER ) ) {
            continue;
        }

        svg_trace_t clip = {};
        if ( ( ent->solid == SOLID_BSP || ( ent->svFlags & SVF_HULL ) ) && bspClipping ) {
            clip = SVG_Clip( ent, hit->currentOrigin, hit->mins, hit->maxs, hit->currentOrigin, SVG_GetClipMask( hit ) );

            if ( clip.fraction == 1.0f ) {
                continue;
            }
        }

        // nail it
        if ( clip.fraction ) {
            if ( clip.ent ) {
                vec3_t dir;
                VectorSubtract( hit->currentOrigin, clip.ent->currentOrigin, dir );
                VectorNormalize( dir );

                if ( clip.plane.dist ) {
                    SVG_DamageEntity( hit, ent, ent, dir, ent->currentOrigin, clip.plane.normal, 100000, 0, DAMAGE_NO_PROTECTION, MEANS_OF_DEATH_TELEFRAGGED );
                } else {
                    SVG_DamageEntity( hit, ent, ent, dir, ent->currentOrigin, vec3_origin, 100000, 0, DAMAGE_NO_PROTECTION, MEANS_OF_DEATH_TELEFRAGGED );
                }
            } else {
                if ( clip.plane.dist ) {
                    SVG_DamageEntity( hit, ent, ent, vec3_origin, ent->currentOrigin, clip.plane.normal, 100000, 0, DAMAGE_NO_PROTECTION, MEANS_OF_DEATH_TELEFRAGGED );
                } else {
                    SVG_DamageEntity( hit, ent, ent, vec3_origin, ent->currentOrigin, vec3_origin, 100000, 0, DAMAGE_NO_PROTECTION, MEANS_OF_DEATH_TELEFRAGGED );
                }
            }
        } else {
            SVG_DamageEntity( hit, ent, ent, vec3_origin, ent->currentOrigin, vec3_origin, 100000, 0, DAMAGE_NO_PROTECTION, MEANS_OF_DEATH_TELEFRAGGED );
        }

        // if we didn't kill it, fail
        if ( hit->solid ) {
            return false;
        }
        #endif
    }

    return true;        // all clear
}



/**
*
*
*
*   MoveWith for parenting brush entities as a whole.
*
*
*
**/
/**
*   @note   At the time of calling, parent entity has to reside in its default state.
*           (This so the actual offsets can be calculated easily.)
**/
void SVG_MoveWith_SetTargetParentEntity( const char *targetName, svg_base_edict_t *parentMover, svg_base_edict_t *childMover ) {
    if ( !SVG_Entity_IsActive( parentMover ) || !SVG_Entity_IsActive( childMover ) ) {
        return;
    }
    
    // Update targetname.
    childMover->targetNames.movewith = targetName;
    childMover->targetEntities.movewith = parentMover;

    // Determine brushmodel bbox origins.
    Vector3 parentOriginOffset = QM_BBox3Center(
        QM_BBox3FromMinsMaxs( parentMover->absMin, parentMover->absMax )
    );
    Vector3 childOriginOffset = QM_BBox3Center(
        QM_BBox3FromMinsMaxs( childMover->absMin, childMover->absMax )
    );

    // Add it to the chain.
    svg_base_edict_t *nextInlineMover = parentMover;
    while ( nextInlineMover->moveWith.moveNextEntity ) {
        nextInlineMover = nextInlineMover->moveWith.moveNextEntity;
    }
    nextInlineMover->moveWith.moveNextEntity = childMover;
    nextInlineMover->moveWith.moveNextEntity->moveWith.parentMoveEntity = parentMover;

    Vector3 parentOrigin    = parentMover->s.origin;
    Vector3 childOrigin     = childMover->s.origin;

    // Calculate the relative offets for its origin and angles.
    childMover->moveWith.absoluteOrigin = childOrigin;
    childMover->moveWith.originOffset = childOrigin - childOriginOffset;
    childMover->moveWith.relativeDeltaOffset = parentOrigin - childOrigin;

    // Fetch spawn angles.
    Vector3 childAngles = childMover->s.angles;
    childMover->moveWith.spawnParentAttachAngles = childAngles;
    // Calculate delta angles.
    Vector3 parentAngles = parentMover->s.angles;
    childMover->moveWith.spawnDeltaAngles = parentAngles - childAngles;

    // Add to game movewiths.
    game.moveWithEntities[ game.num_movewithEntityStates ].childNumber = childMover->s.number;
    game.moveWithEntities[ game.num_movewithEntityStates ].parentNumber = parentMover->s.number;

    // Debug
    gi.dprintf( "%s: found parent(%s) for child entity(%s).\n", __func__, (const char*)parentMover->targetNames.target, (const char *)childMover->targetNames.movewith );
}

void SVG_MoveWith_Init( svg_base_edict_t *self, svg_base_edict_t *parent ) {

}

/**
*   @brief
**/
void SVG_MoveWith_SetChildEntityMovement( svg_base_edict_t *self ) {
    //// Parent origin.
    //Vector3 parentOrigin = moveWithEntity->s.origin;
    //// Difference atm between parent origin and child origin.
    //Vector3 diffOrigin = parentOrigin - ent->s.origin;
    //// Reposition to parent with its default origin offset, subtract difference.
    //Vector3 newOrigin = parentOrigin + moveWithEntity->moveWith.originOffset + diffOrigin;
    //
    //VectorCopy( newOrigin, ent->s.origin );
}

/**
*   @brief 
**/
#include "svgame/svg_physics.h"
//bool SV_Push( svg_base_edict_t *pusher, vec3_t move, vec3_t amove );
//svg_trace_t SVG_PushEntity( svg_base_edict_t *ent, vec3_t push );

void SVG_MoveWith_AdjustToParent( const Vector3 &deltaParentOrigin, const Vector3 &deltaParentAngles, const Vector3 &parentVUp, const Vector3 &parentVRight, const Vector3 &parentVForward, svg_base_edict_t *parentMover, svg_base_edict_t *childMover ) {
    // Calculate origin to adjust by.
    #if 0

    Vector3 childOrigin = childMover->s.origin;
    childOrigin += deltaParentOrigin;
    // Adjust desired pusher origins.
    childMover->pushMoveInfo.startOrigin += deltaParentOrigin;
    childMover->pushMoveInfo.endOrigin += deltaParentOrigin;
    childMover->pos1 += deltaParentOrigin;
    childMover->pos2 += deltaParentOrigin;
    VectorCopy( childOrigin, childMover->s.origin );
    #else
//  vec3_t angles;
//  VectorAdd( ent->s.angles, ent->moveWith.originAnglesOffset, angles );
//  SVG_Util_SetMoveDir( angles, ent->movedir );
    Vector3 newAngles = childMover->s.angles;// +deltaParentAngles;
    Vector3 tempMoveDir = {};
    SVG_Util_SetMoveDir( newAngles, tempMoveDir );

    Vector3 relativeParentOffset = childMover->moveWith.relativeDeltaOffset;
    //childMover->pos1 = QM_Vector3MultiplyAdd( parentMover->s.origin, -relativeParentOffset.x, parentVForward );
    //childMover->pos1 = QM_Vector3MultiplyAdd( childMover->pos1, relativeParentOffset.y, parentVRight );
    //childMover->pos1 = QM_Vector3MultiplyAdd( childMover->pos1, -relativeParentOffset.z, parentVUp );
    childMover->pos1 = QM_Vector3MultiplyAdd( parentMover->s.origin, -relativeParentOffset.x, parentVForward );
    childMover->pos1 = QM_Vector3MultiplyAdd( childMover->pos1, relativeParentOffset.y, parentVRight );
    childMover->pos1 = QM_Vector3MultiplyAdd( childMover->pos1, -relativeParentOffset.z, parentVUp );
    // 
    // Pos2.
    if ( strcmp( (const char *)childMover->classname, "func_door_rotating" ) != 0 || strcmp( (const char*)childMover->classname, "func_button" ) != 0 ) {
        childMover->pos2 = QM_Vector3MultiplyAdd( childMover->pos1, childMover->pushMoveInfo.distance, childMover->movedir );
        //childMover->pos2 = QM_Vector3MultiplyAdd( parentMover->s.origin, relativeParentOffset.x, parentVForward );
        //childMover->pos2 = QM_Vector3MultiplyAdd( childMover->pos2, relativeParentOffset.y, parentVRight );
        //childMover->pos2 = QM_Vector3MultiplyAdd( childMover->pos2, relativeParentOffset.z, parentVUp );
        //childMover->pos2 = QM_Vector3MultiplyAdd( childMover->pos2, childMover->pushMoveInfo.distance, childMover->movedir );
    } else {
        childMover->pos2 = QM_Vector3MultiplyAdd( parentMover->s.origin, -relativeParentOffset.x, parentVForward );
        childMover->pos2 = QM_Vector3MultiplyAdd( childMover->pos2, relativeParentOffset.y, parentVRight );
        childMover->pos2 = QM_Vector3MultiplyAdd( childMover->pos2, -relativeParentOffset.z, parentVUp );
    }

    childMover->pushMoveInfo.startOrigin = childMover->pos1;
    childMover->pushMoveInfo.endOrigin = childMover->pos2;
    childMover->pushMoveInfo.startAngles = childMover->angles1;
    childMover->pushMoveInfo.endAngles = childMover->angles2;

    #if 1
    if ( childMover->pushMoveInfo.state == 0/*PUSHMOVE_STATE_BOTTOM*/ || childMover->pushMoveInfo.state == 1/*PUSHMOVE_STATE_TOP*/ ) {
        // Velocities for door/button movement are handled in normal
        // movement routines
        VectorCopy( parentMover->velocity, childMover->velocity );
        // Sanity insurance:
        if ( childMover->pushMoveInfo.state == 0/*PUSHMOVE_STATE_BOTTOM*/ ) {
            VectorCopy( childMover->pos1, childMover->s.origin );
        } else {
            VectorCopy( childMover->pos2, childMover->s.origin );
        }
    } else {
        if ( strcmp( (const char *)childMover->classname, "func_door_rotating" ) != 0 || strcmp( (const char *)childMover->classname, "func_button" ) != 0 ) {
            // Calculate what is expected to be the offset from parent origin.
            Vector3 parentMoverOrigin = parentMover->s.origin;
            Vector3 childMoverRelativeParentOffset = parentMoverOrigin;
            childMoverRelativeParentOffset += relativeParentOffset;

            // Subtract
            Vector3 childMoverOrigin = childMover->s.origin;
            Vector3 offset = childMoverRelativeParentOffset - childMoverOrigin;

            // ADjust velocity.
            vec3_t move = {};
            vec3_t amove = {};
            VectorScale( parentMover->velocity, gi.frame_time_s, move );
			SVG_PushMover( childMover, &offset.x, amove );
            //VectorAdd( childMover->velocity, move, childMover->velocity );

            // Add origin.
            //VectorAdd( childMoverOrigin, offset, childMover->s.origin );

            // object is moving
            //SVG_PushEntity( childMover, offset );
            //svg_trace_t SVG_PushEntity( svg_base_edict_t * ent, vec3_t push )
            //    if ( !VectorEmpty( part->velocity ) || !VectorEmpty( part->avelocity ) ) {
            //        // object is moving
            //        VectorScale( part->velocity, gi.frame_time_s, move );
            //        VectorScale( part->avelocity, gi.frame_time_s, amove );

            //        if ( !SV_Push( part, move, amove ) )
            //            break;  // move was blocked
            //    }
        }
    }
    // Calculate angles to adjust by.
    //Vector3 deltaParentAngles = parentMover->s.angles - lastParentAngles;
    //Vector3 childAngles = childMover->s.angles;
    //childAngles = QM_Vector3AngleMod( childAngles + deltaParentAngles );

    // Adjust desired pusher angles.
    //childMover->pushMoveInfo.startAngles = QM_Vector3AngleMod( childMover->pushMoveInfo.startAngles + deltaParentAngles );
    //childMover->pushMoveInfo.endAngles = QM_Vector3AngleMod( childMover->pushMoveInfo.endAngles + deltaParentAngles );
    //VectorCopy( childAngles, childMover->s.angles );
     
    //Vector3 childVelocity = childMover->velocity;
    //gi.dprintf( "%s: ent->classname(\"%s\"), ent->s.number(%i), childVelocity(%g,%g,%g)\n",
    //    __func__,
    //    childMover->classname,
    //    childMover->s.number,
    //    childVelocity.x, childVelocity.y, childVelocity.z
    //);

    #endif
    #endif

    // We're done, link it back in.
    gi.linkentity( childMover );
}

//if ( ent->targetEntities.movewith && ent->inUse && ( ent->movetype == MOVETYPE_PUSH || ent->movetype == MOVETYPE_STOP ) ) {
//    svg_base_edict_t *moveWithEntity = ent->targetEntities.movewith;
//    if ( moveWithEntity->inUse && ( moveWithEntity->movetype == MOVETYPE_PUSH || moveWithEntity->movetype == MOVETYPE_STOP ) ) {
//        // Parent origin.
//        Vector3 parentOrigin = moveWithEntity->s.origin;
//        // Difference atm between parent origin and child origin.
//        Vector3 diffOrigin = parentOrigin - ent->s.origin;
//        // Reposition to parent with its default origin offset, subtract difference.
//        Vector3 newOrigin = parentOrigin + moveWithEntity->moveWith.originOffset + diffOrigin;
//        
//        //VectorCopy( newOrigin, ent->s.origin );
//        #define PUSHMOVE_STATE_TOP           0
//        #define PUSHMOVE_STATE_BOTTOM        1
//        #define STATE_UP            2
//        #define STATE_DOWN          3
//        
//        vec3_t delta_angles, forward, right, up;
//        VectorSubtract( moveWithEntity->s.angles, ent->moveWith.originAnglesOffset, delta_angles );
//        AngleVectors( delta_angles, forward, right, up );
//        VectorNegate( right, right );

//        vec3_t angles;
//        VectorAdd( ent->s.angles, ent->moveWith.originAnglesOffset, angles );
//        SVG_Util_SetMoveDir( angles, ent->movedir );

//        VectorMA( moveWithEntity->s.origin, ent->moveWith.originOffset[ 0 ], forward, ent->pos1 );
//        VectorMA( ent->pos1, ent->moveWith.originOffset[ 1 ], right, ent->pos1 );
//        VectorMA( ent->pos1, ent->moveWith.originOffset[ 2 ], up, ent->pos1 );
//        VectorMA( ent->pos1, ent->pushMoveInfo.distance, ent->movedir, ent->pos2 );
//        VectorCopy( ent->pos1, ent->pushMoveInfo.startOrigin );
//        VectorCopy( ent->s.angles, ent->pushMoveInfo.startAngles );
//        VectorCopy( ent->pos2, ent->pushMoveInfo.endOrigin );
//        VectorCopy( ent->s.angles, ent->pushMoveInfo.endAngles );
//        if ( ent->pushMoveInfo.state == PUSHMOVE_STATE_BOTTOM || ent->pushMoveInfo.state == PUSHMOVE_STATE_TOP ) {
//            // Velocities for door/button movement are handled in normal
//            // movement routines
//            VectorCopy( moveWithEntity->velocity, ent->velocity );
//            // Sanity insurance:
//            if ( ent->pushMoveInfo.state == PUSHMOVE_STATE_BOTTOM ) {
//                VectorCopy( ent->pos1, ent->s.origin );
//            } else {
//                VectorCopy( ent->pos2, ent->s.origin );
//            }
//        }

//        gi.linkentity( ent );

//        //gi.dprintf( "%s: entID(%i), moveWithEntity->origin(%f, %f, %f)\n", __func__, ent->s.number, newMoveWithOrigin.x, newMoveWithOrigin.y, newMoveWithOrigin.z );
//    }
//}