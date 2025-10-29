/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_train'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_misc.h"
#include "svgame/svg_trigger.h"
#include "svgame/svg_utils.h"

#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_callfunction.hpp"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_train.h"
#include "svgame/entities/path/svg_path_corner.h"

#include "sharedgame/sg_means_of_death.h"



/*QUAKED func_train (0 .5 .8) ? START_ON TOGGLE BLOCK_STOPS
Trains are moving platforms that players can ride.
The targets origin specifies the min point of the train at each corner.
The train spawns at the first target it is pointing at.
If the train is the target of a button or trigger, it will not begin moving until activated.
speed   default 100
dmg     default 2
noise   looping sound to play when the train is in motion

*/

/**
*   @brief  Blocked.
**/
DEFINE_MEMBER_CALLBACK_BLOCKED( svg_func_train_t, onBlocked )( svg_func_train_t *self, svg_base_edict_t *other ) -> void {
    if ( !( other->svFlags & SVF_MONSTER ) && ( !other->client ) ) {
        // give it a chance to go away on it's own terms (like gibs)
        SVG_DamageEntity( other, self, self, vec3_origin, other->s.origin, vec3_origin, 100000, 1, DAMAGE_NONE, MEANS_OF_DEATH_CRUSHED );
        // if it's still there, nuke it
        if ( other && other->inUse && other->solid ) {
            SVG_Misc_BecomeExplosion( other, 1 );
        }
        return;
    }

    if ( level.time < self->touch_debounce_time ) {
        return;
    }

    if ( !self->dmg ) {
        return;
    }

    self->touch_debounce_time = level.time + 0.5_sec;
    SVG_DamageEntity( other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, DAMAGE_NONE, MEANS_OF_DEATH_CRUSHED );
}

/**
*   @brief  Wait EndMove Callback.
**/
DEFINE_MEMBER_CALLBACK_PUSHMOVE_ENDMOVE( svg_func_train_t, onTrainWait )( svg_func_train_t *self ) -> void {
    if ( self->targetEntities.target && self->targetEntities.target->targetNames.path ) {
        //static svg_level_qstring_t savetarget;
        //savetarget = {};
        svg_base_edict_t *ent = self->targetEntities.target;
        const char *savetarget = (const char *)ent->targetNames.target;
        ent->targetNames.target = ent->targetNames.path;
        SVG_UseTargets( ent, self->activator );
        ent->targetNames.target = savetarget;

        // make sure we didn't get killed by a targetNames.kill
        if ( !self->inUse ) {
            return;
        }
    }

    if ( self->pushMoveInfo.wait ) {
        if ( self->pushMoveInfo.wait > 0 ) {
            self->nextthink = level.time + QMTime::FromSeconds( self->pushMoveInfo.wait );
            self->SetThinkCallback( &self->onThink_MoveToNextTarget );
        } else if ( self->spawnflags & SPAWNFLAG_TOGGLE ) { // && wait < 0
            // PMM - clear target_ent, let train_next get called when we get used
            //self->onThink_MoveToNextTarget( self );
            self->targetEntities.target = nullptr;
            // pmm
            self->spawnflags &= ~SPAWNFLAG_START_ON;
            VectorClear( self->velocity );
            self->nextthink = 0_ms;
        }

        if ( !( self->flags & FL_TEAMSLAVE ) ) {
            if ( self->pushMoveInfo.sounds.end ) {
                gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sounds.end, 1, ATTN_STATIC, 0 );
            }
        }
        self->s.sound = 0;
    } else {
        self->onThink_MoveToNextTarget( self );
    }

}

/**
*   @brief
**/
// PGM
// <Q2RTXP>: WID: Acts also as onThink_Wait :-)
DEFINE_MEMBER_CALLBACK_THINK( svg_func_train_t, onThink )( svg_func_train_t *self ) -> void { }
// PGM
DEFINE_MEMBER_CALLBACK_THINK( svg_func_train_t, onThink_MoveToNextTarget )( svg_func_train_t *self ) -> void {
    svg_base_edict_t *ent = nullptr;
    vec3_t      dest = {};
    bool first = true;

// For repeating.
again:
    // We need a target string.
    if ( !self->targetNames.target ) {
              gi.dprintf ("train_next: no next target\n");
        return;
    }
    // Find the entity matching the targetname string with target string.
    ent = SVG_PickTarget( (const char *)self->targetNames.target );
    if ( !ent ) {
        gi.dprintf( "train_next: bad target %s\n", (const char *)self->targetNames.target );
        return;
    }
    // We've found the target, assign its string to our own targetname.
    self->targetNames.target = ent->targetNames.target;

    // Check for a teleport path_corner.
    if ( ent->spawnflags & svg_path_corner_t::SPAWNFLAG_TELEPORT_PUSHER ) {
        if ( !first ) {
            gi.dprintf( "connected teleport path_corners, see %s at %s\n", (const char *)ent->classname, vtos( ent->s.origin ) );
            return;
        }
        first = false;
        if ( self->spawnflags & svg_func_train_t::SPAWNFLAG_USE_ORIGIN ) {
            VectorCopy( ent->s.origin, self->s.origin );
        } else {
            VectorSubtract( ent->s.origin, self->mins, self->s.origin );

            if ( self->spawnflags & svg_func_train_t::SPAWNFLAG_FIX_OFFSET ) {
                vec3_t subtraction = { 1.f, 1.f, 1.f };
                VectorSubtract( self->s.origin, subtraction, self->s.origin );
            }
        }
        VectorCopy( self->s.origin, self->s.old_origin );
        //self->s.event = EV_OTHER_TELEPORT;
        SVG_Util_AddEvent( self, EV_OTHER_TELEPORT, 0 );
        gi.linkentity( self );
        goto again;
    }

    // PGM
    // Adjust to path_corner's optional speed settings.
    if ( ent->speed ) {
        self->speed = ent->speed;
        self->pushMoveInfo.speed = ent->speed;
        if ( ent->accel ) {
            self->pushMoveInfo.accel = ent->accel;
        } else {
            self->pushMoveInfo.accel = ent->speed;
        }
        if ( ent->decel ) {
            self->pushMoveInfo.decel = ent->decel;
        } else {
            self->pushMoveInfo.decel = ent->speed;
        }
        self->pushMoveInfo.current_speed = 0;
    }
    // PGM

    self->pushMoveInfo.wait = ent->wait;
    self->targetEntities.target = ent;

    if ( !( self->flags & FL_TEAMSLAVE ) ) {
        if ( self->pushMoveInfo.sounds.start ) {
            gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sounds.start, 1, ATTN_STATIC, 0 );
        }
        self->s.sound = self->pushMoveInfo.sounds.middle;
    }

    if ( self->spawnflags & svg_func_train_t::SPAWNFLAG_USE_ORIGIN ) {
        VectorCopy( ent->s.origin, dest );
    } else {
        VectorSubtract( ent->s.origin, self->mins, dest );
        if ( self->spawnflags & svg_func_train_t::SPAWNFLAG_FIX_OFFSET ) {
            vec3_t subtraction = { 1.f, 1.f, 1.f };
            VectorSubtract( dest, subtraction, dest );
        }
    }
    self->pushMoveInfo.state = PUSHMOVE_STATE_TOP;
    self->pushMoveInfo.startOrigin = self->s.origin;
    self->pushMoveInfo.endOrigin = dest;
    self->CalculateDirectionalMove( dest, (svg_pushmove_endcallback)( &self->onTrainWait ) );
    self->spawnflags |= svg_func_train_t::SPAWNFLAG_START_ON;

    // PGM
    #if 0
    if ( self->spawnflags & SPAWNFLAG_SPAWNFLAG_MOVE_TEAMCHAIN ) ) {
        svg_base_edict_t *e;
        vec3_t	 dir, dst;

        dir = dest - self->s.origin;
        for ( e = self->teamchain; e; e = e->teamchain ) {
            dst = dir + e->s.origin;
            e->moveinfo.start_origin = e->s.origin;
            e->moveinfo.end_origin = dst;

            e->moveinfo.state = STATE_TOP;
            e->speed = self->speed;
            e->moveinfo.speed = self->moveinfo.speed;
            e->moveinfo.accel = self->moveinfo.accel;
            e->moveinfo.decel = self->moveinfo.decel;
            e->movetype = MOVETYPE_PUSH;
            self->CalculateDirectionalMove( dst, (svg_pushmove_endcallback)( &self->onTrainWait );
        }
    }
    #endif
    // PGM
}

/**
*   @brief  Will resume/start the train's movement over its trajectory.
**/
void svg_func_train_t::ResumeMovement( ) {
    vec3_t  dest = { };
    svg_base_edict_t *ent = targetEntities.target;

    if ( !ent ) {
        // <Q2RTXP>: WID: TODO: WARN: DEBUG 
        return;
    }

    if ( spawnflags & svg_func_train_t::SPAWNFLAG_USE_ORIGIN ) {
        VectorCopy( ent->s.origin, dest );
    } else {
        VectorSubtract( ent->s.origin, mins, dest );
        if ( spawnflags & svg_func_train_t::SPAWNFLAG_FIX_OFFSET ) {
            vec3_t subtraction = { 1.f, 1.f, 1.f };
            VectorSubtract( dest, subtraction, dest );
        }
    }
    pushMoveInfo.state = PUSHMOVE_STATE_TOP;
    pushMoveInfo.startOrigin = s.origin;
    pushMoveInfo.endOrigin = dest;
    CalculateDirectionalMove( dest, (svg_pushmove_endcallback)( &onTrainWait ) );
    spawnflags |= svg_func_train_t::SPAWNFLAG_START_ON;
}

/**
*   @brief  
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_func_train_t, onThink_FindNextTarget )( svg_func_train_t *self ) -> void {
    svg_base_edict_t *ent;

    if ( !self->targetNames.target ) {
        gi.dprintf( "train_find: no target\n" );
        return;
    }
    ent = SVG_PickTarget( (const char *)self->targetNames.target );
    if ( !ent ) {
        gi.dprintf( "train_find: target %s not found\n", (const char *)self->targetNames.target );
        return;
    }
    self->targetNames.target = ent->targetNames.target;

    vec3_t dest = {};
    if ( self->spawnflags & svg_func_train_t::SPAWNFLAG_USE_ORIGIN ) {
        VectorCopy( ent->s.origin, dest );
    } else {
        VectorSubtract( ent->s.origin, self->mins, dest );
        if ( self->spawnflags & svg_func_train_t::SPAWNFLAG_FIX_OFFSET ) {
            vec3_t subtraction = { 1.f, 1.f, 1.f };
            VectorSubtract( self->s.origin, subtraction, self->s.origin );
        }
    }

    gi.linkentity( self );

    // if not triggered, start immediately
    if ( !self->targetname ) {
        self->spawnflags |= svg_func_train_t::SPAWNFLAG_START_ON;
    }

    if ( self->spawnflags & svg_func_train_t::SPAWNFLAG_START_ON ) {
        self->nextthink = level.time + FRAME_TIME_S;
        self->SetThinkCallback( &self->onThink_MoveToNextTarget );
        self->activator = self;
    }
}

/**
*   @brief  Use.
**/
DEFINE_MEMBER_CALLBACK_USE( svg_func_train_t, onUse )( svg_func_train_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) -> void {
    self->activator = activator;
    self->other = other;

    #if 1
    // Dealing with a toggleable train?
    if ( SVG_HasSpawnFlags( self, SPAWNFLAG_TOGGLE ) ) {
        // Toggle:
        if ( useType == ENTITY_USETARGET_TYPE_TOGGLE ) {
            // ON:
            if ( useValue == 0 ) {
                self->spawnflags &= ~SPAWNFLAG_START_ON;
                VectorClear( self->velocity );
                self->nextthink = 0_ms;
            }
            // OFF:
            else {
                if ( self->targetEntities.target ) {
                    self->ResumeMovement();
                } else {
                    self->onThink_MoveToNextTarget( self );
                }
            }
        }
        // ON:
        else if ( useType == ENTITY_USETARGET_TYPE_ON || useValue == 1 ) {
            if ( self->targetEntities.target ) {
                self->ResumeMovement();
            } else {
                self->onThink_MoveToNextTarget( self );
            }
        }
        // OFF:
        else if ( useType == ENTITY_USETARGET_TYPE_OFF || useValue == 0 ) {
            self->spawnflags &= ~SPAWNFLAG_START_ON;
            VectorClear( self->velocity );
            self->nextthink = 0_ms;
        }
        //if ( !SVG_UseTarget_ShouldToggle( useType, ( self->useTarget.state & ENTITY_USETARGET_STATE_OFF ) != 0  /*0 != self->s.frame*/ ) ) {
        //    return;
        //}
        //self->s.frame = 0 != self->s.frame ? 0 : 1;
        //if ( 0 != self->s.frame ) {

        //    //SVG_UseTargets( self, activator, entity_usetarget_type_t::ENTITY_USETARGET_TYPE_ON, 1 );
        //} else {

        //    //SVG_UseTargets( self, activator, entity_usetarget_type_t::ENTITY_USETARGET_TYPE_OFF, 0 );
        //}

        // OFF:
        

        // TODO: Continuous? ;-P
        
    // Non toggleable, automatically resume movement if a target is set, 
    // or find the next target to move to next frame.
    } else {
        if ( self->targetEntities.target ) {
            self->ResumeMovement();
        } else {
            self->onThink_MoveToNextTarget( self );
        }
    }
    #else
    if ( self->spawnflags & SPAWNFLAG_START_ON ) {
        if ( !( self->spawnflags & SPAWNFLAG_TOGGLE ) )
            return;
        self->spawnflags &= ~SPAWNFLAG_START_ON;
        VectorClear( self->velocity );
        self->nextthink = 0_ms;
    } else {
        if ( self->targetEntities.target )
            train_resume( self );
        else
            train_next( self );
    }
    #endif
}

/**
*   @brief  Spawn.
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_func_train_t, onSpawn )( svg_func_train_t *self ) -> void {
    Super::onSpawn( self );

    self->movetype = MOVETYPE_PUSH;
    self->s.entityType = ET_PUSHER;

    VectorClear( self->s.angles );
    self->SetBlockedCallback( &self->onBlocked );
    if ( self->spawnflags & SPAWNFLAG_BLOCK_STOPS ) {
        self->dmg = 0;
    } else {
        if ( !self->dmg ) {
            self->dmg = 100;
        }
    }
    self->solid = SOLID_BSP;
    gi.setmodel( self, self->model );

    if ( self->noisePath.ptr && self->noisePath.count ) {
        self->pushMoveInfo.sounds.middle = gi.soundindex( (const char*)self->noisePath );
    }

    if ( !self->speed ) {
        self->speed = 100;
    }
    if ( !self->accel ) {
        self->accel = self->speed;
    }
    if ( !self->decel ) {
        self->decel = self->speed;
    }

    self->pushMoveInfo.speed = self->speed;
    self->pushMoveInfo.accel = self->accel;
    self->pushMoveInfo.decel = self->decel;

    self->SetUseCallback( &self->onUse );

    gi.linkentity( self );

    if ( self->targetNames.target ) {
        // start trains on the second frame, to make sure their targets have had
        // a chance to spawn
        self->nextthink = level.time + FRAME_TIME_S;
        self->SetThinkCallback( &self->onThink_FindNextTarget );
    } else {
        gi.dprintf( "func_train without a target at %s\n", vtos( self->absMin ) );
    }
}