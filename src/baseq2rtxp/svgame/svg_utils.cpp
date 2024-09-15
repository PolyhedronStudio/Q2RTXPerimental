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

#include "svg_local.h"

/**
*   @brief  Wraps up the new more modern G_ProjectSource.
**/
void G_ProjectSource( const vec3_t point, const vec3_t distance, const vec3_t forward, const vec3_t right, vec3_t result ) {
    // Call the new more modern G_ProjectSource.
    const Vector3 _result = G_ProjectSource( point, distance, forward, right );
    // Copy the resulting values into the result vec3_t array(ptr).
    VectorCopy( _result, result );
}

/**
*   @brief  Project vector from source.
**/
const Vector3 G_ProjectSource( const Vector3 &point, const Vector3 &distance, const Vector3 &forward, const Vector3 &right ) {
    return {
        point[ 0 ] + forward[ 0 ] * distance[ 0 ] + right[ 0 ] * distance[ 1 ],
        point[ 1 ] + forward[ 1 ] * distance[ 0 ] + right[ 1 ] * distance[ 1 ],
        point[ 2 ] + forward[ 2 ] * distance[ 0 ] + right[ 2 ] * distance[ 1 ] + distance[ 2 ]
    };
}


/*
=============
G_Find

Searches all active entities for the next one that holds
the matching string at fieldofs (use the FOFS() macro) in the structure.

Searches beginning at the edict after from, or the beginning if NULL
NULL will be returned if the end of the list is reached.

=============
*/
// WID: C++20: Added const.
edict_t *G_Find(edict_t *from, int fieldofs, const char *match)
{
    char    *s;

    // WID: Prevent nastyness when match is empty (Q_stricmp)
    if ( !match ) {
        return nullptr;
    }

    if ( !from ) {
        from = g_edicts;
    } else {
        from++;
    }

    for (; from < &g_edicts[globals.num_edicts] ; from++) {
        if (!from->inuse)
            continue;
        s = *(char **)((byte *)from + fieldofs);
        if (!s)
            continue;
        if (!Q_stricmp(s, match))
            return from;
    }

    return NULL;
}


/*
=================
findradius

Returns entities that have origins within a spherical area

findradius (origin, radius)
=================
*/
edict_t *findradius(edict_t *from, vec3_t org, float rad)
{
    vec3_t  eorg;
    int     j;

    if (!from)
        from = g_edicts;
    else
        from++;
    for (; from < &g_edicts[globals.num_edicts]; from++) {
        if (!from->inuse)
            continue;
        if (from->solid == SOLID_NOT)
            continue;
        for (j = 0 ; j < 3 ; j++)
            eorg[j] = org[j] - (from->s.origin[j] + (from->mins[j] + from->maxs[j]) * 0.5f);
        if (VectorLength(eorg) > rad)
            continue;
        return from;
    }

    return NULL;
}


/*
=============
G_PickTarget

Searches all active entities for the next one that holds
the matching string at fieldofs (use the FOFS() macro) in the structure.

Searches beginning at the edict after from, or the beginning if NULL
NULL will be returned if the end of the list is reached.

=============
*/
#define MAXCHOICES  8

edict_t *G_PickTarget(char *targetname)
{
    edict_t *ent = NULL;
    int     num_choices = 0;
    edict_t *choice[MAXCHOICES];

    if (!targetname) {
        gi.dprintf("G_PickTarget called with NULL targetname\n");
        return NULL;
    }

    while (1) {
        ent = G_Find(ent, FOFS(targetname), targetname);
        if (!ent)
            break;
        choice[num_choices++] = ent;
        if (num_choices == MAXCHOICES)
            break;
    }

    if (!num_choices) {
        gi.dprintf("G_PickTarget: target %s not found\n", targetname);
        return NULL;
    }

    return choice[Q_rand_uniform(num_choices)];
}



void Think_Delay(edict_t *ent)
{
    G_UseTargets(ent, ent->activator);
    G_FreeEdict(ent);
}

/*
==============================
G_UseTargets

the global "activator" should be set to the entity that initiated the firing.

If self.delay is set, a DelayedUse entity will be created that will actually
do the SUB_UseTargets after that many seconds have passed.

Centerprints any self.message to the activator.

Search for (string)targetname in all entities that
match (string)self.target and call their .use function

==============================
*/
void G_UseTargets(edict_t *ent, edict_t *activator)
{
    edict_t     *t;

//
// check for a delay
//
    if (ent->delay) {
        // create a temp object to fire at a later time
        t = G_AllocateEdict();
        t->classname = "DelayedUse";
        t->nextthink = level.time + sg_time_t::from_sec(ent->delay);
        t->think = Think_Delay;
        t->activator = activator;
        if (!activator)
            gi.dprintf("Think_Delay with no activator\n");
        t->message = ent->message;
        t->targetNames.target = ent->targetNames.target;
        t->targetNames.kill = ent->targetNames.kill;
        return;
    }


//
// print the message
//
    if ((ent->message) && !(activator->svflags & SVF_MONSTER)) {
        gi.centerprintf(activator, "%s", ent->message);
        if (ent->noise_index)
            gi.sound(activator, CHAN_AUTO, ent->noise_index, 1, ATTN_NORM, 0);
        else
            gi.sound(activator, CHAN_AUTO, gi.soundindex("hud/chat01.wav"), 1, ATTN_NORM, 0);
    }

//
// kill killtargets
//
    if (ent->targetNames.kill) {
        t = NULL;
        while ((t = G_Find(t, FOFS(targetname), ent->targetNames.kill))) {
            G_FreeEdict(t);
            if (!ent->inuse) {
                gi.dprintf("entity was removed while using killtargets\n");
                return;
            }
        }
    }

//
// fire targets
//
    if (ent->targetNames.target) {
        t = NULL;
        while ((t = G_Find(t, FOFS(targetname), ent->targetNames.target))) {
            // doors fire area portals in a specific way
            if (!Q_stricmp(t->classname, "func_areaportal") &&
                (!Q_stricmp(ent->classname, "func_door") || !Q_stricmp(ent->classname, "func_door_rotating")))
                continue;

            if (t == ent) {
                gi.dprintf("WARNING: Entity used itself.\n");
            } else {
                if (t->use)
                    t->use(t, ent, activator);
            }
            if (!ent->inuse) {
                gi.dprintf("entity was removed while using targets\n");
                return;
            }
        }
    }
}


vec3_t VEC_UP       = {0, -1, 0};
vec3_t MOVEDIR_UP   = {0, 0, 1};
vec3_t VEC_DOWN     = {0, -2, 0};
vec3_t MOVEDIR_DOWN = {0, 0, -1};

void G_SetMovedir( vec3_t angles, Vector3 &movedir ) {
    if ( VectorCompare( angles, VEC_UP ) ) {
        VectorCopy( MOVEDIR_UP, movedir );
    } else if ( VectorCompare( angles, VEC_DOWN ) ) {
        VectorCopy( MOVEDIR_DOWN, movedir );
    } else {
        QM_AngleVectors( angles, &movedir, NULL, NULL );
    }

    VectorClear( angles );
}

char *G_CopyString(char *in)
{
    char    *out;

	// WID: C++20: Addec cast.
    out = (char*)gi.TagMalloc(strlen(in) + 1, TAG_SVGAME_LEVEL);
    strcpy(out, in);
    return out;
}


void G_InitEdict(edict_t *e)
{
    e->inuse = true;
    e->classname = "noclass";
    e->gravity = 1.0f;
    e->s.number = e - g_edicts;
	
	// A generic entity type by default.
	e->s.entityType = ET_GENERIC;
}

/*
=================
G_AllocateEdict

Either finds a free edict, or allocates a new one.
Try to avoid reusing an entity that was recently freed, because it
can cause the client to think the entity morphed into something else
instead of being removed and recreated, which can cause interpolated
angles and bad trails.
=================
*/
edict_t *G_AllocateEdict(void)
{
    int32_t i = game.maxclients + 1;
    edict_t *entity = &g_edicts[ game.maxclients + 1 ];
    edict_t *freedEntity = nullptr;

    for ( i; i < globals.num_edicts; i++, entity++ ) {
        // the first couple seconds of server time can involve a lot of
        // freeing and allocating, so relax the replacement policy
        if ( !entity->inuse && ( entity->freetime < 2_sec || level.time - entity->freetime > 500_ms ) ) {
            G_InitEdict( entity );
            return entity;
        }

        // this is going to be our second chance to spawn an entity in case all free
        // entities have been freed only recently
        if ( !freedEntity ) {
            freedEntity = entity;
        }
    }

    if ( i == game.maxentities ) {
        if ( freedEntity ) {
            G_InitEdict( freedEntity );
            return freedEntity;
        }
        gi.error( "G_AllocateEdict: no free edicts" );
    }

    globals.num_edicts++;
    G_InitEdict(entity);
    return entity;
}

/*
=================
G_FreeEdict

Marks the edict as free
=================
*/
void G_FreeEdict(edict_t *ed)
{
    gi.unlinkentity(ed);        // unlink from world

    if ((ed - g_edicts) <= (maxclients->value + BODY_QUEUE_SIZE)) {
        #ifdef _DEBUG
            gi.dprintf("tried to free special edict(#%d) within special edict range(%d)\n", ed - g_edicts, maxclients->value + BODY_QUEUE_SIZE );
        #endif
        return;
    }

    int32_t id = ed->spawn_count + 1;
    memset( ed, 0, sizeof( *ed ) );
    ed->s.number = ed - g_edicts;
    ed->classname = "freed";
    ed->freetime = level.time;
    ed->inuse = false;
    ed->spawn_count = id;
}


/*
============
G_TouchTriggers

============
*/
void    G_TouchTriggers(edict_t *ent)
{
    int         i, num;
    edict_t     *touch[MAX_EDICTS], *hit;

    // dead things don't activate triggers!
    if ((ent->client || (ent->svflags & SVF_MONSTER)) && (ent->health <= 0))
        return;

    num = gi.BoxEdicts(ent->absmin, ent->absmax, touch
                       , MAX_EDICTS, AREA_TRIGGERS);

    // be careful, it is possible to have an entity in this
    // list removed before we get to it (killtriggered)
    for (i = 0 ; i < num ; i++) {
        hit = touch[i];
        if (!hit->inuse)
            continue;
        if (!hit->touch)
            continue;

        hit->touch(hit, ent, NULL, NULL);
    }
}

/*
============
G_TouchSolids

Call after linking a new trigger in during gameplay
to force all entities it covers to immediately touch it
============
*/
void    G_TouchSolids(edict_t *ent)
{
    int         i, num;
    edict_t     *touch[MAX_EDICTS], *hit;

    num = gi.BoxEdicts(ent->absmin, ent->absmax, touch
                       , MAX_EDICTS, AREA_SOLID);

    // be careful, it is possible to have an entity in this
    // list removed before we get to it (killtriggered)
    for (i = 0 ; i < num ; i++) {
        hit = touch[i];
        if (!hit->inuse)
            continue;
        if (ent->touch)
            ent->touch(hit, ent, NULL, NULL);
        if (!ent->inuse)
            break;
    }
}


// [Paril-KEX] scan for projectiles between our movement positions
// to see if we need to collide against them
void G_TouchProjectiles( edict_t *ent, const Vector3 &previous_origin ) {
    struct skipped_projectile {
        edict_t *projectile;
        int32_t spawn_count;
    };
    // a bit ugly, but we'll store projectiles we are ignoring here.
    static std::vector<skipped_projectile> skipped;

    while ( true ) {
        trace_t tr = gi.trace( &previous_origin.x, ent->mins, ent->maxs, ent->s.origin, ent, static_cast<contents_t>( ent->clipmask | CONTENTS_PROJECTILE ) );

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
            && OnSameTeam( ent, tr.ent->owner ) && !( dmflags->integer & DF_NO_FRIENDLY_FIRE ) ) {
            continue;
        }

        // Call impact(touch) triggers.
        SV_Impact( ent, &tr );
    }

    for ( auto &skip : skipped ) {
        if ( skip.projectile->inuse && skip.projectile->spawn_count == skip.spawn_count ) {
            skip.projectile->svflags |= SVF_PROJECTILE;
        }
    }

    skipped.clear();
}

/*
==============================================================================

Kill box

==============================================================================
*/

/*
=================
KillBox

Kills all entities that would touch the proposed new positioning
of ent.  Ent should be unlinked before calling this!
=================
*/
//const bool KillBox(edict_t *ent, const bool bspClipping ) {
//    // don't telefrag as spectator... or in noclip
//    if ( ent->movetype == MOVETYPE_NOCLIP ) {
//        return true;
//    }
//
//    contents_t mask = static_cast<contents_t>( CONTENTS_MONSTER | CONTENTS_PLAYER );
//
//    //// [Paril-KEX] don't gib other players in coop if we're not colliding
//    //if ( from_spawning && ent->client && coop->integer && !G_ShouldPlayersCollide( false ) )
//    //    mask &= ~CONTENTS_PLAYER;
//    static edict_t *touchedEdicts[ MAX_EDICTS ];
//    memset( touchedEdicts, 0, MAX_EDICTS );
//
//    int32_t num = gi.BoxEdicts( ent->absmin, ent->absmax, touchedEdicts, MAX_EDICTS, AREA_SOLID );
//    for ( int32_t i = 0; i < num; i++ ) {
//        // Pointer to touched entity.
//        edict_t *hit = touchedEdicts[ i ];
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
//        trace_t clip = {};
//        if ( ( ent->solid == SOLID_BSP || ( ent->svflags & SVF_HULL ) ) && bspClipping ) {
//            clip = gi.clip(ent, hit->s.origin, hit->mins, hit->maxs, hit->s.origin, G_GetClipMask(hit));
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
//                    T_Damage( hit, ent, ent, dir, ent->s.origin, clip.plane.normal, 100000, 0, DAMAGE_NO_PROTECTION, MEANS_OF_DEATH_TELEFRAGGED );
//                } else {
//                    T_Damage( hit, ent, ent, dir, ent->s.origin, vec3_origin, 100000, 0, DAMAGE_NO_PROTECTION, MEANS_OF_DEATH_TELEFRAGGED );
//                }
//            } else {
//                if ( clip.plane.dist ) {
//                    T_Damage( hit, ent, ent, vec3_origin, ent->s.origin, clip.plane.normal, 100000, 0, DAMAGE_NO_PROTECTION, MEANS_OF_DEATH_TELEFRAGGED );
//                } else {
//                    T_Damage( hit, ent, ent, vec3_origin, ent->s.origin, vec3_origin, 100000, 0, DAMAGE_NO_PROTECTION, MEANS_OF_DEATH_TELEFRAGGED );
//                }
//            }
//        } else {
//            T_Damage( hit, ent, ent, vec3_origin, ent->s.origin, vec3_origin, 100000, 0, DAMAGE_NO_PROTECTION, MEANS_OF_DEATH_TELEFRAGGED );
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
const bool KillBox( edict_t *ent, const bool bspClipping ) {
    // don't telefrag as spectator... or in noclip
    if ( ent->movetype == MOVETYPE_NOCLIP ) {
        return true;
    }

    contents_t mask = static_cast<contents_t>( CONTENTS_MONSTER | CONTENTS_PLAYER );

    //// [Paril-KEX] don't gib other players in coop if we're not colliding
    //if ( from_spawning && ent->client && coop->integer && !G_ShouldPlayersCollide( false ) )
    //    mask &= ~CONTENTS_PLAYER;
    static edict_t *touchedEdicts[ MAX_EDICTS ];
    memset( touchedEdicts, 0, MAX_EDICTS );

    int32_t num = gi.BoxEdicts( ent->absmin, ent->absmax, touchedEdicts, MAX_EDICTS, AREA_SOLID );
    for ( int32_t i = 0; i < num; i++ ) {
        // Pointer to touched entity.
        edict_t *hit = touchedEdicts[ i ];
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

        trace_t clip = {};
        if ( ( ent->solid == SOLID_BSP || ( ent->svflags & SVF_HULL ) ) && bspClipping ) {
            clip = gi.clip( ent, hit->s.origin, hit->mins, hit->maxs, hit->s.origin, G_GetClipMask( hit ) );

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
                    T_Damage( hit, ent, ent, dir, ent->s.origin, clip.plane.normal, 100000, 0, DAMAGE_NO_PROTECTION, MEANS_OF_DEATH_TELEFRAGGED );
                } else {
                    T_Damage( hit, ent, ent, dir, ent->s.origin, vec3_origin, 100000, 0, DAMAGE_NO_PROTECTION, MEANS_OF_DEATH_TELEFRAGGED );
                }
            } else {
                if ( clip.plane.dist ) {
                    T_Damage( hit, ent, ent, vec3_origin, ent->s.origin, clip.plane.normal, 100000, 0, DAMAGE_NO_PROTECTION, MEANS_OF_DEATH_TELEFRAGGED );
                } else {
                    T_Damage( hit, ent, ent, vec3_origin, ent->s.origin, vec3_origin, 100000, 0, DAMAGE_NO_PROTECTION, MEANS_OF_DEATH_TELEFRAGGED );
                }
            }
        } else {
            T_Damage( hit, ent, ent, vec3_origin, ent->s.origin, vec3_origin, 100000, 0, DAMAGE_NO_PROTECTION, MEANS_OF_DEATH_TELEFRAGGED );
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
// WID: TODO: move into g_local.h
static constexpr int32_t PUSHER_MOVEINFO_STATE_TOP      = 0;
static constexpr int32_t PUSHER_MOVEINFO_STATE_BOTTOM   = 1;
static constexpr int32_t PUSHER_MOVEINFO_STATE_UP       = 2;
static constexpr int32_t PUSHER_MOVEINFO_STATE_DOWN     = 3;


//
// WID: TODO: Implement bbox3 type..
//

/**
*   @note   At the time of calling, parent entity has to reside in its default state.
*           (This so the actual offsets can be calculated easily.)
**/
void G_MoveWith_SetTargetParentEntity( const char *targetName, edict_t *parentMover, edict_t *childMover ) {
    // Update targetname.
    childMover->targetNames.movewith = targetName;

    // Determine brushmodel bbox origins.
    Vector3 parentOrigin = QM_BBox3Center(
        QM_BBox3FromMinsMaxs( parentMover->absmin, parentMover->absmax )
    );
    Vector3 childOrigin = QM_BBox3Center(
        QM_BBox3FromMinsMaxs( childMover->absmin, childMover->absmax )
    );

    // Calculate the relative offets for its origin and angles.
    childMover->moveWith.absoluteOrigin = childOrigin;
    childMover->moveWith.absoluteParentOriginOffset = parentOrigin - childOrigin;
    
    // Fetch spawn angles.
    Vector3 childAngles = childMover->s.angles;
    childMover->moveWith.spawnParentAttachAngles = childAngles;
    // Calculate delta angles.
    Vector3 parentAngles = parentMover->s.angles;
    childMover->moveWith.spawnDeltaAngles = childAngles - parentAngles;

    // Add to game movewiths.
    game.moveWithEntities[ game.num_movewithEntityStates ].childNumber = childMover->s.number;
    game.moveWithEntities[ game.num_movewithEntityStates ].parentNumber = parentMover->s.number;

    // Debug
    gi.dprintf( "%s: found parent(%s) for child entity(%s).\n", __func__, parentMover->targetNames.target, childMover->targetNames.movewith );
}

void G_MoveWith_Init( edict_t *self, edict_t *parent ) {

}

/**
*   @brief
**/
void G_MoveWith_SetChildEntityMovement( edict_t *self ) {
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
void G_MoveWith_AdjustToParent( edict_t *parentMover, edict_t *childMover ) {
    // This is the absolute parent entity brush model origin in BSP model space.
    Vector3 parentAbsOrigin = QM_BBox3Center(
        QM_BBox3FromMinsMaxs( parentMover->absmin, parentMover->absmax )
    );
    // This is the absolute child entity brush model origin in BSP model space.
    Vector3 childAbsOrigin = QM_BBox3Center(
        QM_BBox3FromMinsMaxs( childMover->absmin, childMover->absmax )
    );

    //
    /*static */Vector3 lastParentOrigin = parentMover->lastOrigin;
    /*static */Vector3 lastParentAngles = parentMover->lastAngles;

    //// Calculate origin to adjust by.
    Vector3 deltaParentOrigin = parentMover->s.origin - lastParentOrigin;
    Vector3 childOrigin = childMover->s.origin;
    childOrigin += deltaParentOrigin;
    // Adjust desired pusher origins.
    childMover->pusherMoveInfo.start_origin += deltaParentOrigin;
    childMover->pusherMoveInfo.end_origin += deltaParentOrigin;
    childMover->pos1 += deltaParentOrigin;
    childMover->pos2 += deltaParentOrigin;
    VectorCopy( childOrigin, childMover->s.origin );

    //// Calculate angles to adjust by.
    //Vector3 deltaParentAngles = parentMover->s.angles - lastParentAngles;
    //Vector3 childAngles = childMover->s.angles;
    //childAngles = QM_Vector3AngleMod( childAngles + deltaParentAngles );

    //// Adjust desired pusher angles.
    //childMover->pusherMoveInfo.start_angles = QM_Vector3AngleMod( childMover->pusherMoveInfo.start_angles + deltaParentAngles );
    //childMover->pusherMoveInfo.end_angles = QM_Vector3AngleMod( childMover->pusherMoveInfo.end_angles + deltaParentAngles );
    //VectorCopy( childAngles, childMover->s.angles );

    // We're done, link it back in.
    gi.linkentity( childMover );

    // Make sure to store the last ... origins and angles.
    parentMover->lastOrigin = parentMover->s.origin;
    parentMover->lastAngles = parentMover->s.angles;

    //gi.bprintf( PRINT_DEVELOPER, "%s: parentMover->s.origin(%f, %f, %f), childMover->s.origin(%f, %f, %f)\n", __func__,
    //    parentMover->s.origin[ 0 ],
    //    parentMover->s.origin[ 1 ],
    //    parentMover->s.origin[ 2 ],
    //    childMover->s.origin[ 0 ],
    //    childMover->s.origin[ 1 ],
    //    childMover->s.origin[ 2 ]
    //);

    //gi.bprintf( PRINT_DEVELOPER, "%s: parentAbsBoxOrigin(%f, %f, %f), childAbsBoxOrigin(%f, %f, %f)\n", __func__,  
    //    parentAbsOrigin.x,
    //    parentAbsOrigin.y,
    //    parentAbsOrigin.z,
    //    childAbsOrigin.x,
    //    childAbsOrigin.y,
    //    childAbsOrigin.z
    //);
}