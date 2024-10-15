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
#include "svg_lua.h"



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
*   @brief  Wraps up the new more modern SVG_ProjectSource.
**/
void SVG_ProjectSource( const vec3_t point, const vec3_t distance, const vec3_t forward, const vec3_t right, vec3_t result ) {
    // Call the new more modern SVG_ProjectSource.
    const Vector3 _result = SVG_ProjectSource( point, distance, forward, right );
    // Copy the resulting values into the result vec3_t array(ptr).
    VectorCopy( _result, result );
}

/**
*   @brief  Project vector from source.
**/
const Vector3 SVG_ProjectSource( const Vector3 &point, const Vector3 &distance, const Vector3 &forward, const Vector3 &right ) {
    return {
        point[ 0 ] + forward[ 0 ] * distance[ 0 ] + right[ 0 ] * distance[ 1 ],
        point[ 1 ] + forward[ 1 ] * distance[ 0 ] + right[ 1 ] * distance[ 1 ],
        point[ 2 ] + forward[ 2 ] * distance[ 0 ] + right[ 2 ] * distance[ 1 ] + distance[ 2 ]
    };
}



/**
*
*
*
*   Signaling:
*
*
*
**/
/**
*   @brief  'Think' support routine for delayed SignalOut signalling.
**/
void Think_SignalOutDelay( edict_t *ent ) {
    // SignalOut again, keep in mind that ent now has no delay set, so it will actually
    // proceed to calling the OnSignalIn functions.
    SVG_SignalOut( ent, ent->other, ent->activator, ent->delayed.signalOut.name );
    // Free ourselves again.
    SVG_FreeEdict( ent );
}



/**
*   @brief  Will iterate over the signal argument list and push all its key/values into the
*           lua table stack.
**/
static void SVG_SignalOut_DebugPrintArguments( const svg_signal_argument_array_t &signalArguments ) {
    // Need sane signal args and number of signal args.
    if ( signalArguments.empty() ) {
        return;
    }
    
    gi.dprintf( "%s:\n", "-------------------------------------------" );
    gi.dprintf( "%s:\n", __func__ );

    // Iterate the arguments.
    for ( int32_t argumentIndex = 0; argumentIndex < signalArguments.size(); argumentIndex++ ) {
        // Get access to argument.
        const svg_signal_argument_t *signalArgument = &signalArguments[ argumentIndex ];

        // Act based on its type.
                // Act based on its type.
        if ( signalArgument->type == SIGNAL_ARGUMENT_TYPE_BOOLEAN ) {
            gi.dprintf( "%s:%s\n", signalArgument->key, ( signalArgument->value.boolean ? "true" : "false" ) );
        } else if ( signalArgument->type == SIGNAL_ARGUMENT_TYPE_INTEGER ) {
            gi.dprintf( "%s:%d\n", signalArgument->key, signalArgument->value.integer );
        } else if ( signalArgument->type == SIGNAL_ARGUMENT_TYPE_NUMBER ) {
            gi.dprintf( "%s:%f\n", signalArgument->key, signalArgument->value.number );
        } else if ( signalArgument->type == SIGNAL_ARGUMENT_TYPE_STRING ) {
            gi.dprintf( "%s:%s\n", signalArgument->key, signalArgument->value.str );
        }else if ( signalArgument->type == SIGNAL_ARGUMENT_TYPE_NULLPTR ) {
            gi.dprintf( "%s:%s\n", signalArgument->key, "(nil)" );
        // Fall through:
        //} else if ( signalArgument->type == SIGNAL_ARGUMENT_TYPE_NONE ) {
        } else {
            gi.dprintf( "%s:%s\n", signalArgument->key, "(invalid type)");
        }
    }
}

/**
*   @brief  Will call upon the entity's OnSignalIn(C/Lua) using the signalName.
*   @param  other   (optional) The entity which Send Out the Signal.
*   @param  activator   The entity which initiated the process that resulted in sending out a signal.
**/
//void SVG_SignalOut( edict_t *ent, edict_t *sender, edict_t *activator, const char *signalName, const svg_signal_argument_t *signalArguments, const int32_t numberOfSignalArguments ) {
void SVG_SignalOut( edict_t *ent, edict_t *signaller, edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments ) {
    //
    // check for a delay
    //
    if ( ent->delay ) {
        // create a temp object to fire at a later time
        edict_t *delayEntity = SVG_AllocateEdict();
        delayEntity->classname = "DelayedSignalOut";
        delayEntity->nextthink = level.time + sg_time_t::from_sec( ent->delay );
        delayEntity->think = Think_SignalOutDelay;
        delayEntity->activator = activator;
        delayEntity->other = signaller;
        if ( !activator ) {
            gi.dprintf( "Think_SignalOutDelay with no activator\n" );
        }
        delayEntity->message = ent->message;

        delayEntity->targetNames.target = ent->targetNames.target;
        delayEntity->targetNames.kill = ent->targetNames.kill;
        
        // The luaName of the actual original entity.
        delayEntity->luaProperties.luaName = ent->luaProperties.luaName;
        
        // The entity which created this temporary delay signal entity.
        delayEntity->delayed.signalOut.creatorEntity = ent;
        // The arguments of said signal.
        delayEntity->delayed.signalOut.arguments = signalArguments;
        // The actual string comes from lua so we need to copy it in instead.
        memset( delayEntity->delayed.signalOut.name, 0, sizeof( delayEntity->delayed.signalOut.name ) );
        Q_strlcpy( delayEntity->delayed.signalOut.name, signalName, strlen( signalName ) + 1 );

        return;
    }

    // Whether to por
    bool propogateToLua = true;
    if ( ent->onsignalin ) {
        ent->activator = activator;
        ent->other = signaller;

        // Notify of the signal coming in.
        /*propogateToLua = */ent->onsignalin(ent, signaller, activator, signalName, signalArguments );
    }
    // If desired, propogate the signal to Lua '_OnSignalIn' callbacks.
    if ( propogateToLua ) {
        SVG_Lua_SignalOut( SVG_Lua_GetMapLuaState(), ent, signaller, activator, signalName, signalArguments );
    }
}



/**
*
*
*
*   UseTarget Functionality:
*
*
*
**/
#define MAXCHOICES  8

edict_t *SVG_PickTarget( char *targetname ) {
    edict_t *ent = NULL;
    int     num_choices = 0;
    edict_t *choice[ MAXCHOICES ];

    if ( !targetname ) {
        gi.dprintf( "SVG_PickTarget called with NULL targetname\n" );
        return NULL;
    }

    while ( 1 ) {
        ent = SVG_Find( ent, FOFS( targetname ), targetname );
        if ( !ent )
            break;
        choice[ num_choices++ ] = ent;
        if ( num_choices == MAXCHOICES )
            break;
    }

    if ( !num_choices ) {
        gi.dprintf( "SVG_PickTarget: target %s not found\n", targetname );
        return NULL;
    }

    return choice[ Q_rand_uniform( num_choices ) ];
}



void Think_UseTargetsDelay( edict_t *ent ) {
    SVG_UseTargets( ent, ent->activator );
    SVG_FreeEdict( ent );
}

/*
==============================
SVG_UseTargets

the global "activator" should be set to the entity that initiated the firing.

If self.delay is set, a DelayedUse entity will be created that will actually
do the SUB_UseTargets after that many seconds have passed.

Centerprints any self.message to the activator.

Search for (string)targetname in all entities that
match (string)self.target and call their .use function

==============================
*/
void SVG_UseTargets( edict_t *ent, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    //
    // Check for a delay
    //
    if ( ent->delay ) {
        // create a temp object to fire at a later time
        edict_t *delayEntity = SVG_AllocateEdict();
        delayEntity->classname = "DelayedUseTargets";
        delayEntity->nextthink = level.time + sg_time_t::from_sec( ent->delay );
        delayEntity->think = Think_UseTargetsDelay;
        if ( !activator ) {
            gi.dprintf( "Think_UseTargetsDelay with no activator\n" );
        }
        delayEntity->activator = activator;
        delayEntity->other = ent->other;
        delayEntity->message = ent->message;
        
        delayEntity->targetNames.target = ent->targetNames.target;
        delayEntity->targetNames.kill = ent->targetNames.kill;

        delayEntity->luaProperties.luaName = ent->luaProperties.luaName;
        delayEntity->delayed.useTarget.creatorEntity = ent;
        delayEntity->delayed.useTarget.useType = useType;
        delayEntity->delayed.useTarget.useValue = useValue;

        return;
    }


    //
    // print the message
    //
    if ( ( ent->message ) && !( activator->svflags & SVF_MONSTER ) ) {
        gi.centerprintf( activator, "%s", ent->message );
        if ( ent->noise_index ) {
            gi.sound( activator, CHAN_AUTO, ent->noise_index, 1, ATTN_NORM, 0 );
        } else {
            gi.sound( activator, CHAN_AUTO, gi.soundindex( "hud/chat01.wav" ), 1, ATTN_NORM, 0 );
        }
    }

    //
    // kill killtargets
    //
    if ( ent->targetNames.kill ) {
        edict_t *killTargetEntity = nullptr;
        while ( ( killTargetEntity = SVG_Find( killTargetEntity, FOFS( targetname ), ent->targetNames.kill ) ) ) {
            SVG_FreeEdict( killTargetEntity );
            if ( !ent->inuse ) {
                gi.dprintf( "%s: entity(#%d, \"%s\") was removed while using killtargets\n", __func__, ent->s.number, ent->classname );
                return;
            }
        }
    }

    //
    // fire targets
    //
    if ( ent->targetNames.target ) {
        edict_t *fireTargetEntity = nullptr;
        while ( ( fireTargetEntity = SVG_Find( fireTargetEntity, FOFS( targetname ), ent->targetNames.target ) ) ) {
            // Doors fire area portals in a specific way
            if ( !Q_stricmp( fireTargetEntity->classname, "func_areaportal" )
                && ( !Q_stricmp( ent->classname, "func_door" ) || !Q_stricmp( ent->classname, "func_door_rotating" ) ) ) {
                continue;
            }

            if ( fireTargetEntity == ent ) {
                gi.dprintf( "%s: entity(#%d, \"%s\") used itself!\n", __func__, ent->s.number, ent->classname );
            } else {
                if ( fireTargetEntity->use ) {
                    fireTargetEntity->use( fireTargetEntity, ent, activator, useType, useValue );
                }

                if ( fireTargetEntity->luaProperties.luaName ) {
                    // Generate function 'callback' name.
                    const std::string luaFunctionName = std::string( fireTargetEntity->luaProperties.luaName ) + "_Use";
                    // Call if it exists.
                    if ( LUA_HasFunction( SVG_Lua_GetMapLuaState(), luaFunctionName ) ) {
                        LUA_CallFunction( SVG_Lua_GetMapLuaState(), luaFunctionName, 1, 5, LUA_CALLFUNCTION_VERBOSE_MISSING,
                            /*[lua args]:*/ fireTargetEntity, ent, activator, useType, useValue );
                    }
                }
            }
            if ( !ent->inuse ) {
                gi.dprintf( "%s: entity(#%d, \"%s\") was removed while using killtargets\n", __func__, ent->s.number, ent->classname );
                return;
            }
        }
    }
}
/**
*   @brief  True if the entity should 'toggle'.
**/
const bool SVG_UseTarget_ShouldToggle( const entity_usetarget_type_t useType, const int32_t currentState ) {
    // We always toggle for USE_TOGGLE and USE_SET:
    if ( useType != ENTITY_USETARGET_TYPE_TOGGLE && useType != ENTITY_USETARGET_TYPE_SET ) {
        // If its current state is 'ON' and useType is 'ON', don't toggle:
        if ( currentState && useType == ENTITY_USETARGET_TYPE_ON ) {
            return false;
        }
        // If its current state is 'OFF' and useType is 'OFF', don't toggle:
        if ( !currentState && useType == ENTITY_USETARGET_TYPE_OFF ) {
            return false;
        }
    }

    return true;
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

void SVG_SetMoveDir( vec3_t angles, Vector3 &movedir ) {
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
char *SVG_CopyString(char *in)
{
    char    *out;

	// WID: C++20: Addec cast.
    out = (char*)gi.TagMalloc(strlen(in) + 1, TAG_SVGAME_LEVEL);
    strcpy(out, in);
    return out;
}



/**
*
*
*
*   Touch... Implementations:
*
*
*
**/
/*
============
SVG_TouchTriggers

============
*/
void    SVG_TouchTriggers(edict_t *ent)
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
        if ( !hit->inuse ) {
            continue;
        }
        if ( !hit->touch ) {
            continue;
        }

        hit->touch(hit, ent, NULL, NULL);
    }
}

/*
============
SVG_TouchSolids

Call after linking a new trigger in during gameplay
to force all entities it covers to immediately touch it
============
*/
void    SVG_TouchSolids(edict_t *ent)
{
    int         i, num;
    edict_t     *touch[MAX_EDICTS], *hit;

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


// [Paril-KEX] scan for projectiles between our movement positions
// to see if we need to collide against them
void SVG_TouchProjectiles( edict_t *ent, const Vector3 &previous_origin ) {
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
*   KillBox:
*
*
*
**/

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
//            clip = gi.clip(ent, hit->s.origin, hit->mins, hit->maxs, hit->s.origin, SVG_GetClipMask(hit));
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
            clip = gi.clip( ent, hit->s.origin, hit->mins, hit->maxs, hit->s.origin, SVG_GetClipMask( hit ) );

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
void SVG_MoveWith_SetTargetParentEntity( const char *targetName, edict_t *parentMover, edict_t *childMover ) {
    // Update targetname.
    //childMover->targetNames.movewith = targetName;

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

void SVG_MoveWith_Init( edict_t *self, edict_t *parent ) {

}

/**
*   @brief
**/
void SVG_MoveWith_SetChildEntityMovement( edict_t *self ) {
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
void SVG_MoveWith_AdjustToParent( const Vector3 &deltaParentOrigin, const Vector3 &deltaParentAngles, const Vector3 &parentVUp, const Vector3 &parentVRight, const Vector3 &parentVForward, edict_t *parentMover, edict_t *childMover ) {
    // This is the absolute parent entity brush model origin in BSP model space.
    //Vector3 parentAbsOrigin = QM_BBox3Center(
    //    QM_BBox3FromMinsMaxs( parentMover->absmin, parentMover->absmax )
    //);
    //// This is the absolute child entity brush model origin in BSP model space.
    //Vector3 childAbsOrigin = QM_BBox3Center(
    //    QM_BBox3FromMinsMaxs( childMover->absmin, childMover->absmax )
    //);
    //BBox3 bboxParentMover = QM_BBox3FromCenterSize(
    //    QM_BBox3Size(
    //        QM_BBox3FromMinsMaxs( parentMover->absmin, parentMover->absmax )
    //    ),
    //    parentMover->move_origin
    //);
    //BBox3 bboxChildMover = QM_BBox3FromCenterSize( 
    //    QM_BBox3Size( 
    //        QM_BBox3FromMinsMaxs( childMover->absmin, childMover->absmax ) 
    //    ),
    //    childMover->move_origin 
    //);

    //// This is the absolute parent entity brush model origin in BSP model space.
    //Vector3 parentAbsOrigin = QM_BBox3Center(
    //    bboxParentMover
    //);
    //// This is the absolute child entity brush model origin in BSP model space.
    //Vector3 childAbsOrigin = QM_BBox3Center(
    //    bboxChildMover
    //);

    Vector3 childOrigin = childMover->s.origin;
    if ( childMover->solid == SOLID_BSP ) {
        const mmodel_t *bspModel = gi.GetInlineModelDataForHandle( childMover->s.modelindex );
        Vector3 childAbsOrigin = bspModel->origin;
        // This is the absolute child entity brush model origin in BSP model space.
        //Vector3 childAbsOrigin = QM_BBox3Center(
        //    QM_BBox3FromMinsMaxs( childMover->absmin, childMover->absmax )
        //);
        childOrigin = childAbsOrigin - childOrigin;
    }
    Vector3 parentOrigin = parentMover->s.origin;
    if ( parentMover->solid == SOLID_BSP ) {
        const mmodel_t *bspModel = gi.GetInlineModelDataForHandle( parentMover->s.modelindex );
        Vector3 parentAbsOrigin = bspModel->origin;
        //Vector3 parentAbsOrigin = QM_BBox3Center(
        //    QM_BBox3FromMinsMaxs( parentMover->absmin, parentMover->absmax )
        //);
        parentOrigin = parentAbsOrigin - parentOrigin;
    }

    Vector3 _deltaParentOrigin = parentOrigin - childOrigin;

    //// Calculate origin to adjust by.
    #if 0
    Vector3 childOrigin = childMover->s.origin;
    childOrigin += deltaParentOrigin;
    // Adjust desired pusher origins.
    childMover->pushMoveInfo.start_origin += deltaParentOrigin;
    childMover->pushMoveInfo.end_origin += deltaParentOrigin;
    childMover->pos1 += deltaParentOrigin;
    childMover->pos2 += deltaParentOrigin;
    VectorCopy( childOrigin, childMover->s.origin );
    #else
//  vec3_t angles;
//  VectorAdd( ent->s.angles, ent->moveWith.originAnglesOffset, angles );
//  SVG_SetMoveDir( angles, ent->movedir );
    Vector3 newAngles = childMover->s.angles + deltaParentAngles;
    Vector3 tempMoveDir = {};
    SVG_SetMoveDir( &newAngles.x, tempMoveDir );

//  VectorMA( moveWithEntity->s.origin, ent->moveWith.originOffset[ 0 ], forward, ent->pos1 );
//  VectorMA( ent->pos1, ent->moveWith.originOffset[ 1 ], right, ent->pos1 );
//  VectorMA( ent->pos1, ent->moveWith.originOffset[ 2 ], up, ent->pos1 );
//  VectorMA( ent->pos1, ent->pushMoveInfo.distance, ent->movedir, ent->pos2 );
    
    //VectorCopy( childOrigin, childMover->s.origin );
    // Pos1.
    //childMover->pos1 += _deltaParentOrigin;
    childMover->pos1 = QM_Vector3MultiplyAdd( parentMover->s.origin, _deltaParentOrigin.x, parentVForward );
    childMover->pos1 = QM_Vector3MultiplyAdd( childMover->pos1, _deltaParentOrigin.y, parentVRight );
    childMover->pos1 = QM_Vector3MultiplyAdd( childMover->pos1, _deltaParentOrigin.z, parentVUp );
    // 
    // Pos2.
    //if ( strcmp( childMover->classname, "func_door_rotating" ) != 0 ) {
    //    childMover->pos2 = QM_Vector3MultiplyAdd( childMover->pos1, childMover->pushMoveInfo.distance, tempMoveDir );
    //} else {
        //childMover->pos2 += _deltaParentOrigin;
        childMover->pos2 = QM_Vector3MultiplyAdd( parentMover->s.origin, _deltaParentOrigin.x, parentVForward );
        childMover->pos2 = QM_Vector3MultiplyAdd( childMover->pos2, _deltaParentOrigin.y, parentVRight );
        childMover->pos2 = QM_Vector3MultiplyAdd( childMover->pos2, _deltaParentOrigin.z, parentVUp );
    //}


//  VectorCopy( ent->pos1, ent->pushMoveInfo.start_origin );
//  VectorCopy( ent->s.angles, ent->pushMoveInfo.start_angles );
//  VectorCopy( ent->pos2, ent->pushMoveInfo.end_origin );
//  VectorCopy( ent->s.angles, ent->pushMoveInfo.end_angles );
    childMover->pushMoveInfo.start_origin = childMover->pos1;
    childMover->pushMoveInfo.end_origin = childMover->pos2;
    childMover->pushMoveInfo.start_angles = childMover->angles1;
    childMover->pushMoveInfo.end_angles = childMover->angles2;

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
    }
    #endif
    #endif






    //// Calculate angles to adjust by.
    //Vector3 deltaParentAngles = parentMover->s.angles - lastParentAngles;
    //Vector3 childAngles = childMover->s.angles;
    //childAngles = QM_Vector3AngleMod( childAngles + deltaParentAngles );

    //// Adjust desired pusher angles.
    //childMover->pushMoveInfo.start_angles = QM_Vector3AngleMod( childMover->pushMoveInfo.start_angles + deltaParentAngles );
    //childMover->pushMoveInfo.end_angles = QM_Vector3AngleMod( childMover->pushMoveInfo.end_angles + deltaParentAngles );
    //VectorCopy( childAngles, childMover->s.angles );

    // We're done, link it back in.
    gi.linkentity( childMover );

    if ( std::string( childMover->classname ) == "func_door" ) {
        int x = 10;
    }

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

//if ( ent->targetEntities.movewith && ent->inuse && ( ent->movetype == MOVETYPE_PUSH || ent->movetype == MOVETYPE_STOP ) ) {
//    edict_t *moveWithEntity = ent->targetEntities.movewith;
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
//        SVG_SetMoveDir( angles, ent->movedir );

//        VectorMA( moveWithEntity->s.origin, ent->moveWith.originOffset[ 0 ], forward, ent->pos1 );
//        VectorMA( ent->pos1, ent->moveWith.originOffset[ 1 ], right, ent->pos1 );
//        VectorMA( ent->pos1, ent->moveWith.originOffset[ 2 ], up, ent->pos1 );
//        VectorMA( ent->pos1, ent->pushMoveInfo.distance, ent->movedir, ent->pos2 );
//        VectorCopy( ent->pos1, ent->pushMoveInfo.start_origin );
//        VectorCopy( ent->s.angles, ent->pushMoveInfo.start_angles );
//        VectorCopy( ent->pos2, ent->pushMoveInfo.end_origin );
//        VectorCopy( ent->s.angles, ent->pushMoveInfo.end_angles );
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