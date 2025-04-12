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


/**
*
*
*
*   Vector Utilities:
*
*
*
**/

/**
*   @brief  Project vector from source.
**/
const Vector3 SVG_Util_ProjectSource( const Vector3 &point, const Vector3 &distance, const Vector3 &forward, const Vector3 &right ) {
    return {
        point[ 0 ] + forward[ 0 ] * distance[ 0 ] + right[ 0 ] * distance[ 1 ],
        point[ 1 ] + forward[ 1 ] * distance[ 0 ] + right[ 1 ] * distance[ 1 ],
        point[ 2 ] + forward[ 2 ] * distance[ 0 ] + right[ 2 ] * distance[ 1 ] + distance[ 2 ]
    };
}
/**
*   @brief  Wraps up the new more modern SVG_Util_ProjectSource.
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
*   Move Direction for PushMovers: 
* 
* 
* 
**/
vec3_t VEC_UP       = {0, -1, 0};
vec3_t MOVEDIR_UP   = {0, 0, 1};
vec3_t VEC_DOWN     = {0, -2, 0};
vec3_t MOVEDIR_DOWN = {0, 0, -1};

void SVG_Util_SetMoveDir( vec3_t angles, Vector3 &movedir ) {
    if ( VectorCompare( angles, VEC_UP ) ) {
        VectorCopy( MOVEDIR_UP, movedir );
    } else if ( VectorCompare( angles, VEC_DOWN ) ) {
        VectorCopy( MOVEDIR_DOWN, movedir );
    } else {
        QM_AngleVectors( angles, &movedir, NULL, NULL );
    }

    VectorClear( angles );
}



/**
*
*
*
*   Strings:
*
*
*
**/




/**
*
*
*
*   Touch... Implementations:
*
*
*
**/
/**
*   @brief  
**/
void SVG_Util_TouchTriggers(svg_edict_t *ent) {
    int         i, num;
    svg_edict_t     *touch[MAX_EDICTS], *hit;

    // dead things don't activate triggers!
    if ((ent->client || (ent->svflags & SVF_MONSTER)) && (ent->health <= 0))
        return;

    num = gi.BoxEdicts(ent->absmin, ent->absmax, touch
                       , MAX_EDICTS, AREA_TRIGGERS);

    // be careful, it is possible to have an entity in this
    // list removed before we get to it (killtriggered)
    for (i = 0 ; i < num ; i++) {
        hit = touch[i];
        if ( !hit->inuse ) {
            continue;
        }
        if ( !hit->touch ) {
            continue;
        }

        hit->touch(hit, ent, NULL, NULL);
    }
}

/**
*   @brief  Call after linking a new trigger in during gameplay
*           to force all entities it covers to immediately touch it
**/
void SVG_Util_TouchSolids(svg_edict_t *ent) {
    int         i, num;
    svg_edict_t     *touch[MAX_EDICTS], *hit;

    num = gi.BoxEdicts(ent->absmin, ent->absmax, touch
                       , MAX_EDICTS, AREA_SOLID);

    // be careful, it is possible to have an entity in this
    // list removed before we get to it (killtriggered)
    for (i = 0 ; i < num ; i++) {
        hit = touch[i];
        if ( !hit->inuse ) {
            continue;
        }
        if ( ent->touch ) {
            ent->touch( hit, ent, NULL, NULL );
        }
        if ( !ent->inuse ) {
            break;
        }
    }
}


/**
*   @brief  Scan for projectiles between our movement positions
*           to see if we need to collide against them.
**/
void SVG_Util_TouchProjectiles( svg_edict_t *ent, const Vector3 &previous_origin ) {
    struct skipped_projectile {
        svg_edict_t *projectile;
        int32_t spawn_count;
    };
    // a bit ugly, but we'll store projectiles we are ignoring here.
    static std::vector<skipped_projectile> skipped;

    while ( true ) {
        svg_trace_t tr = SVG_Trace( previous_origin, ent->mins, ent->maxs, ent->s.origin, ent, ( ent->clipmask | CONTENTS_PROJECTILE ) );

        if ( tr.fraction == 1.0f ) {
            break;
        }
        else if ( !( tr.ent->svflags & SVF_PROJECTILE ) ) {
            break;
        }

        // always skip this projectile since certain conditions may cause the projectile
        // to not disappear immediately
        tr.ent->svflags &= ~SVF_PROJECTILE;
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
        if ( skip.projectile->inuse && skip.projectile->spawn_count == skip.spawn_count ) {
            skip.projectile->svflags |= SVF_PROJECTILE;
        }
    }

    skipped.clear();
}



/**
*
*
*
*   Triggers:
*
*
*
**/
/**
*	@brief	Basic Trigger initialization mechanism.
**/
void SVG_Util_InitTrigger( svg_edict_t *self ) {
    if ( !VectorEmpty( self->s.angles ) ) {
        SVG_Util_SetMoveDir( self->s.angles, self->movedir );
    }

    self->solid = SOLID_TRIGGER;
    self->movetype = MOVETYPE_NONE;
    if ( self->model ) {
        gi.setmodel( self, self->model );
    }
    self->svflags = SVF_NOCLIENT;
}



/**
*
*
*
*   SVG_Util_KillBox:
*
*
*
**/
/*
=================
SVG_Util_KillBox

Kills all entities that would touch the proposed new positioning
of ent.  Ent should be unlinked before calling this!
=================
*/
//const bool SVG_Util_KillBox(svg_edict_t *ent, const bool bspClipping ) {
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
//    static svg_edict_t *touchedEdicts[ MAX_EDICTS ];
//    memset( touchedEdicts, 0, MAX_EDICTS );
//
//    int32_t num = gi.BoxEdicts( ent->absmin, ent->absmax, touchedEdicts, MAX_EDICTS, AREA_SOLID );
//    for ( int32_t i = 0; i < num; i++ ) {
//        // Pointer to touched entity.
//        svg_edict_t *hit = touchedEdicts[ i ];
//        // Make sure its valid.
//        if ( hit == nullptr ) {
//            continue;
//        }
//        // Don't killbox ourselves.
//        if ( hit == ent ) {
//            continue;
//        // Skip entities that are not in use, no takedamage, not solid, or solid_bsp/solid_trigger.
//        } else if ( !hit->inuse || !hit->takedamage || !hit->solid || hit->solid == SOLID_TRIGGER || hit->solid == SOLID_BSP ) {
//            continue;
//        } else if ( hit->client && !( mask & CONTENTS_PLAYER ) ) {
//            continue;
//        }
//
//        svg_trace_t clip = {};
//        if ( ( ent->solid == SOLID_BSP || ( ent->svflags & SVF_HULL ) ) && bspClipping ) {
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
//                    SVG_TriggerDamage( hit, ent, ent, dir, ent->s.origin, clip.plane.normal, 100000, 0, DAMAGE_NO_PROTECTION, MEANS_OF_DEATH_TELEFRAGGED );
//                } else {
//                    SVG_TriggerDamage( hit, ent, ent, dir, ent->s.origin, vec3_origin, 100000, 0, DAMAGE_NO_PROTECTION, MEANS_OF_DEATH_TELEFRAGGED );
//                }
//            } else {
//                if ( clip.plane.dist ) {
//                    SVG_TriggerDamage( hit, ent, ent, vec3_origin, ent->s.origin, clip.plane.normal, 100000, 0, DAMAGE_NO_PROTECTION, MEANS_OF_DEATH_TELEFRAGGED );
//                } else {
//                    SVG_TriggerDamage( hit, ent, ent, vec3_origin, ent->s.origin, vec3_origin, 100000, 0, DAMAGE_NO_PROTECTION, MEANS_OF_DEATH_TELEFRAGGED );
//                }
//            }
//        } else {
//            SVG_TriggerDamage( hit, ent, ent, vec3_origin, ent->s.origin, vec3_origin, 100000, 0, DAMAGE_NO_PROTECTION, MEANS_OF_DEATH_TELEFRAGGED );
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
const bool SVG_Util_KillBox( svg_edict_t *ent, const bool bspClipping ) {
    // don't telefrag as spectator... or in noclip
    if ( ent->movetype == MOVETYPE_NOCLIP ) {
        return true;
    }

    cm_contents_t mask = ( CONTENTS_MONSTER | CONTENTS_PLAYER );

    //// [Paril-KEX] don't gib other players in coop if we're not colliding
    //if ( from_spawning && ent->client && coop->integer && !G_ShouldPlayersCollide( false ) )
    //    mask &= ~CONTENTS_PLAYER;
    static svg_edict_t *touchedEdicts[ MAX_EDICTS ];
    memset( touchedEdicts, 0, MAX_EDICTS );

    int32_t num = gi.BoxEdicts( ent->absmin, ent->absmax, touchedEdicts, MAX_EDICTS, AREA_SOLID );
    for ( int32_t i = 0; i < num; i++ ) {
        // Pointer to touched entity.
        svg_edict_t *hit = touchedEdicts[ i ];
        // Make sure its valid.
        if ( hit == nullptr ) {
            continue;
        }
        // Don't killbox ourselves.
        if ( hit == ent ) {
            continue;
        // Skip entities that are not in use, no takedamage, not solid, or solid_bsp/solid_trigger.
        } else if ( !hit->inuse || !hit->takedamage || !hit->solid || hit->solid == SOLID_TRIGGER || hit->solid == SOLID_BSP ) {
            continue;
        } else if ( hit->client && !( mask & CONTENTS_PLAYER ) ) {
            continue;
        }

        svg_trace_t clip = {};
        if ( ( ent->solid == SOLID_BSP || ( ent->svflags & SVF_HULL ) ) && bspClipping ) {
            clip = SVG_Clip( ent, hit->s.origin, hit->mins, hit->maxs, hit->s.origin, SVG_GetClipMask( hit ) );

            if ( clip.fraction == 1.0f ) {
                continue;
            }
        }

        // nail it
        if ( clip.fraction ) {
            if ( clip.ent ) {
                vec3_t dir;
                VectorSubtract( hit->s.origin, clip.ent->s.origin, dir );
                VectorNormalize( dir );

                if ( clip.plane.dist ) {
                    SVG_TriggerDamage( hit, ent, ent, dir, ent->s.origin, clip.plane.normal, 100000, 0, DAMAGE_NO_PROTECTION, MEANS_OF_DEATH_TELEFRAGGED );
                } else {
                    SVG_TriggerDamage( hit, ent, ent, dir, ent->s.origin, vec3_origin, 100000, 0, DAMAGE_NO_PROTECTION, MEANS_OF_DEATH_TELEFRAGGED );
                }
            } else {
                if ( clip.plane.dist ) {
                    SVG_TriggerDamage( hit, ent, ent, vec3_origin, ent->s.origin, clip.plane.normal, 100000, 0, DAMAGE_NO_PROTECTION, MEANS_OF_DEATH_TELEFRAGGED );
                } else {
                    SVG_TriggerDamage( hit, ent, ent, vec3_origin, ent->s.origin, vec3_origin, 100000, 0, DAMAGE_NO_PROTECTION, MEANS_OF_DEATH_TELEFRAGGED );
                }
            }
        } else {
            SVG_TriggerDamage( hit, ent, ent, vec3_origin, ent->s.origin, vec3_origin, 100000, 0, DAMAGE_NO_PROTECTION, MEANS_OF_DEATH_TELEFRAGGED );
        }

        // if we didn't kill it, fail
        if ( hit->solid ) {
            return false;
        }
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
void SVG_MoveWith_SetTargetParentEntity( const char *targetName, svg_edict_t *parentMover, svg_edict_t *childMover ) {
    if ( !SVG_Entity_IsActive( parentMover ) || !SVG_Entity_IsActive( childMover ) ) {
        return;
    }
    
    // Update targetname.
    childMover->targetNames.movewith = targetName;

    // Determine brushmodel bbox origins.
    Vector3 parentOriginOffset = QM_BBox3Center(
        QM_BBox3FromMinsMaxs( parentMover->absmin, parentMover->absmax )
    );
    Vector3 childOriginOffset = QM_BBox3Center(
        QM_BBox3FromMinsMaxs( childMover->absmin, childMover->absmax )
    );

    // Add it to the chain.
    svg_edict_t *nextInlineMover = parentMover;
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

void SVG_MoveWith_Init( svg_edict_t *self, svg_edict_t *parent ) {

}

/**
*   @brief
**/
void SVG_MoveWith_SetChildEntityMovement( svg_edict_t *self ) {
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
bool SV_Push( svg_edict_t *pusher, vec3_t move, vec3_t amove );
svg_trace_t SV_PushEntity( svg_edict_t *ent, vec3_t push );

void SVG_MoveWith_AdjustToParent( const Vector3 &deltaParentOrigin, const Vector3 &deltaParentAngles, const Vector3 &parentVUp, const Vector3 &parentVRight, const Vector3 &parentVForward, svg_edict_t *parentMover, svg_edict_t *childMover ) {
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
    SVG_Util_SetMoveDir( &newAngles.x, tempMoveDir );

    Vector3 relativeParentOffset = childMover->moveWith.relativeDeltaOffset;
    childMover->pos1 = QM_Vector3MultiplyAdd( parentMover->s.origin, relativeParentOffset.x, parentVForward );
    childMover->pos1 = QM_Vector3MultiplyAdd( childMover->pos1, relativeParentOffset.y, parentVRight );
    childMover->pos1 = QM_Vector3MultiplyAdd( childMover->pos1, relativeParentOffset.z, parentVUp );
    // 
    // Pos2.
    if ( strcmp( (const char *)childMover->classname, "func_door_rotating" ) != 0 ) {
        childMover->pos2 = QM_Vector3MultiplyAdd( childMover->pos1, childMover->pushMoveInfo.distance, childMover->movedir );
        //childMover->pos2 = QM_Vector3MultiplyAdd( parentMover->s.origin, relativeParentOffset.x, parentVForward );
        //childMover->pos2 = QM_Vector3MultiplyAdd( childMover->pos2, relativeParentOffset.y, parentVRight );
        //childMover->pos2 = QM_Vector3MultiplyAdd( childMover->pos2, relativeParentOffset.z, parentVUp );
        //childMover->pos2 = QM_Vector3MultiplyAdd( childMover->pos2, childMover->pushMoveInfo.distance, childMover->movedir );
    } else {
        childMover->pos2 = QM_Vector3MultiplyAdd( parentMover->s.origin, relativeParentOffset.x, parentVForward );
        childMover->pos2 = QM_Vector3MultiplyAdd( childMover->pos2, relativeParentOffset.y, parentVRight );
        childMover->pos2 = QM_Vector3MultiplyAdd( childMover->pos2, relativeParentOffset.z, parentVUp );
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
        if ( strcmp( (const char *)childMover->classname, "func_door_rotating" ) != 0 ) {
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
            SV_Push( childMover, &offset.x, amove );
            //VectorAdd( childMover->velocity, move, childMover->velocity );

            // Add origin.
            //VectorAdd( childMoverOrigin, offset, childMover->s.origin );

            // object is moving
            //SV_PushEntity( childMover, offset );
            //svg_trace_t SV_PushEntity( svg_edict_t * ent, vec3_t push )
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

//if ( ent->targetEntities.movewith && ent->inuse && ( ent->movetype == MOVETYPE_PUSH || ent->movetype == MOVETYPE_STOP ) ) {
//    svg_edict_t *moveWithEntity = ent->targetEntities.movewith;
//    if ( moveWithEntity->inuse && ( moveWithEntity->movetype == MOVETYPE_PUSH || moveWithEntity->movetype == MOVETYPE_STOP ) ) {
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