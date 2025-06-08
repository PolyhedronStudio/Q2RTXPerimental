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

#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_callfunction.hpp"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_train.h"

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
    if ( !( other->svflags & SVF_MONSTER ) && ( !other->client ) ) {
        // give it a chance to go away on it's own terms (like gibs)
        SVG_TriggerDamage( other, self, self, vec3_origin, other->s.origin, vec3_origin, 100000, 1, DAMAGE_NONE, MEANS_OF_DEATH_CRUSHED );
        // if it's still there, nuke it
        if ( other ) {
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
    SVG_TriggerDamage( other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, DAMAGE_NONE, MEANS_OF_DEATH_CRUSHED );
}

/**
*   @brief  Wait EndMove Callback.
**/
DEFINE_MEMBER_CALLBACK_PUSHMOVE_ENDMOVE( svg_func_train_t, onTrainWait )( svg_func_train_t *self ) -> void {
    if ( self->targetEntities.target && self->targetEntities.target->targetNames.path ) {
        //static svg_level_qstring_t savetarget;
        //savetarget = {};
        svg_base_edict_t *ent;

        ent = self->targetEntities.target;
        const char *savetarget = (const char *)ent->targetNames.target;
        ent->targetNames.target = ent->targetNames.path;
        SVG_UseTargets( ent, self->activator );
        ent->targetNames.target = savetarget;

        // make sure we didn't get killed by a targetNames.kill
        if ( !self->inuse ) {
            return;
        }
    }

    if ( self->pushMoveInfo.wait ) {
        if ( self->pushMoveInfo.wait > 0 ) {
            self->nextthink = level.time + QMTime::FromSeconds( self->pushMoveInfo.wait );
            self->SetThinkCallback( &self->onThink_MoveToNextTarget );
        } else if ( self->spawnflags & TRAIN_TOGGLE ) { // && wait < 0
            self->onThink_MoveToNextTarget( self );
            self->spawnflags &= ~TRAIN_START_ON;
            VectorClear( self->velocity );
            self->nextthink = 0_ms;
        }

        if ( !( self->flags & FL_TEAMSLAVE ) ) {
            if ( self->pushMoveInfo.sounds.end ) {
                gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sounds.end, 1, ATTN_STATIC, 0 );
            }
            self->s.sound = 0;
        }
    } else {
        self->onThink_MoveToNextTarget( self );
    }

}

/**
*   @brief
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_func_train_t, onThink )( svg_func_train_t *self ) -> void { }
DEFINE_MEMBER_CALLBACK_THINK( svg_func_train_t, onThink_MoveToNextTarget )( svg_func_train_t *self ) -> void {
    svg_base_edict_t *ent;
    vec3_t      dest;
    bool        first;

    first = true;
again:
    if ( !self->targetNames.target ) {
              gi.dprintf ("train_next: no next target\n");
        return;
    }

    ent = SVG_PickTarget( (const char *)self->targetNames.target );
    if ( !ent ) {
        gi.dprintf( "train_next: bad target %s\n", (const char *)self->targetNames.target );
        return;
    }

    self->targetNames.target = ent->targetNames.target;

    // check for a teleport path_corner
    if ( ent->spawnflags & 1 ) {
        if ( !first ) {
            gi.dprintf( "connected teleport path_corners, see %s at %s\n", (const char *)ent->classname, vtos( ent->s.origin ) );
            return;
        }
        first = false;
        VectorSubtract( ent->s.origin, self->mins, self->s.origin );
        VectorCopy( self->s.origin, self->s.old_origin );
        self->s.event = EV_OTHER_TELEPORT;
        gi.linkentity( self );
        goto again;
    }

    self->pushMoveInfo.wait = ent->wait;
    self->targetEntities.target = ent;

    if ( !( self->flags & FL_TEAMSLAVE ) ) {
        if ( self->pushMoveInfo.sounds.start ) {
            gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sounds.start, 1, ATTN_STATIC, 0 );
        }
        self->s.sound = self->pushMoveInfo.sounds.middle;
    }

    VectorSubtract( ent->s.origin, self->mins, dest );
    self->pushMoveInfo.state = PUSHMOVE_STATE_TOP;
    VectorCopy( self->s.origin, self->pushMoveInfo.startOrigin );
    VectorCopy( dest, self->pushMoveInfo.endOrigin );
    self->CalculateDirectionalMove( dest, (svg_pushmove_endcallback)( &self->onTrainWait ) );
    self->spawnflags |= TRAIN_START_ON;
}

/**
*   @brief  Will resume/start the train's movement over its trajectory.
**/
void svg_func_train_t::ResumeMovement( ) {
    svg_base_edict_t *ent;
    vec3_t  dest;

    ent = targetEntities.target;

    VectorSubtract( ent->s.origin, mins, dest );
    pushMoveInfo.state = PUSHMOVE_STATE_TOP;
    VectorCopy( s.origin, pushMoveInfo.startOrigin );
    VectorCopy( dest, pushMoveInfo.endOrigin );
    CalculateDirectionalMove( dest, (svg_pushmove_endcallback)( &onTrainWait ) );
    spawnflags |= TRAIN_START_ON;
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

    VectorSubtract( ent->s.origin, self->mins, self->s.origin );
    gi.linkentity( self );

    // if not triggered, start immediately
    if ( !self->targetname ) {
        self->spawnflags |= TRAIN_START_ON;
    }

    if ( self->spawnflags & TRAIN_START_ON ) {
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
    if ( SVG_HasSpawnFlags( self, TRAIN_TOGGLE ) ) {
        // Toggle:
        if ( useType == ENTITY_USETARGET_TYPE_TOGGLE ) {
            // ON:
            if ( useValue == 0 ) {
                self->spawnflags &= ~TRAIN_START_ON;
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
            self->spawnflags &= ~TRAIN_START_ON;
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
    if ( self->spawnflags & TRAIN_START_ON ) {
        if ( !( self->spawnflags & TRAIN_TOGGLE ) )
            return;
        self->spawnflags &= ~TRAIN_START_ON;
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
    if ( self->spawnflags & TRAIN_BLOCK_STOPS ) {
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
        gi.dprintf( "func_train without a target at %s\n", vtos( self->absmin ) );
    }
}