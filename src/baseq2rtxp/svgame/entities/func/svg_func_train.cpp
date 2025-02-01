/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_train'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_misc.h"

#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_callfunction.hpp"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_train.h"



//! Has the train moving instantly after spawning.
static constexpr int32_t TRAIN_START_ON = BIT( 0 );
//! Train movement can be toggled on/off.
static constexpr int32_t TRAIN_TOGGLE = BIT( 1 );
//! The train will stop instead of destructing the blocking entity.
static constexpr int32_t TRAIN_BLOCK_STOPS = BIT( 2 );


/*QUAKED func_train (0 .5 .8) ? START_ON TOGGLE BLOCK_STOPS
Trains are moving platforms that players can ride.
The targets origin specifies the min point of the train at each corner.
The train spawns at the first target it is pointing at.
If the train is the target of a button or trigger, it will not begin moving until activated.
speed   default 100
dmg     default 2
noise   looping sound to play when the train is in motion

*/
void train_next( edict_t *self );

void train_blocked( edict_t *self, edict_t *other ) {
    if ( !( other->svflags & SVF_MONSTER ) && ( !other->client ) ) {
        // give it a chance to go away on it's own terms (like gibs)
        SVG_TriggerDamage( other, self, self, vec3_origin, other->s.origin, vec3_origin, 100000, 1, DAMAGE_NONE, MEANS_OF_DEATH_CRUSHED );
        // if it's still there, nuke it
        if ( other )
            SVG_Misc_BecomeExplosion1( other );
        return;
    }

    if ( level.time < self->touch_debounce_time )
        return;

    if ( !self->dmg )
        return;
    self->touch_debounce_time = level.time + 0.5_sec;
    SVG_TriggerDamage( other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, DAMAGE_NONE, MEANS_OF_DEATH_CRUSHED );
}

void train_wait( edict_t *self ) {
    if ( self->targetEntities.target->targetNames.path ) {
        char *savetarget;
        edict_t *ent;

        ent = self->targetEntities.target;
        savetarget = (char *)ent->targetNames.target;
        ent->targetNames.target = ent->targetNames.path;
        SVG_UseTargets( ent, self->activator );
        ent->targetNames.target = savetarget;

        // make sure we didn't get killed by a targetNames.kill
        if ( !self->inuse )
            return;
    }

    if ( self->pushMoveInfo.wait ) {
        if ( self->pushMoveInfo.wait > 0 ) {
            self->nextthink = level.time + QMTime::FromSeconds( self->pushMoveInfo.wait );
            self->think = train_next;
        } else if ( self->spawnflags & TRAIN_TOGGLE ) { // && wait < 0
            train_next( self );
            self->spawnflags &= ~TRAIN_START_ON;
            VectorClear( self->velocity );
            self->nextthink = 0_ms;
        }

        if ( !( self->flags & FL_TEAMSLAVE ) ) {
            if ( self->pushMoveInfo.sounds.end )
                gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sounds.end, 1, ATTN_STATIC, 0 );
            self->s.sound = 0;
        }
    } else {
        train_next( self );
    }

}

void train_next( edict_t *self ) {
    edict_t *ent;
    vec3_t      dest;
    bool        first;

    first = true;
again:
    if ( !self->targetNames.target ) {
        //      gi.dprintf ("train_next: no next target\n");
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
        if ( self->pushMoveInfo.sounds.start )
            gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sounds.start, 1, ATTN_STATIC, 0 );
        self->s.sound = self->pushMoveInfo.sounds.middle;
    }

    VectorSubtract( ent->s.origin, self->mins, dest );
    self->pushMoveInfo.state = PUSHMOVE_STATE_TOP;
    VectorCopy( self->s.origin, self->pushMoveInfo.startOrigin );
    VectorCopy( dest, self->pushMoveInfo.endOrigin );
    SVG_PushMove_MoveCalculate( self, dest, train_wait );
    self->spawnflags |= TRAIN_START_ON;
}

void train_resume( edict_t *self ) {
    edict_t *ent;
    vec3_t  dest;

    ent = self->targetEntities.target;

    VectorSubtract( ent->s.origin, self->mins, dest );
    self->pushMoveInfo.state = PUSHMOVE_STATE_TOP;
    VectorCopy( self->s.origin, self->pushMoveInfo.startOrigin );
    VectorCopy( dest, self->pushMoveInfo.endOrigin );
    SVG_PushMove_MoveCalculate( self, dest, train_wait );
    self->spawnflags |= TRAIN_START_ON;
}

void func_train_find( edict_t *self ) {
    edict_t *ent;

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
    if ( !self->targetname )
        self->spawnflags |= TRAIN_START_ON;

    if ( self->spawnflags & TRAIN_START_ON ) {
        self->nextthink = level.time + FRAME_TIME_S;
        self->think = train_next;
        self->activator = self;
    }
}

void train_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    self->activator = activator;
    self->other = other;

    #if 1
    // Dealing with a toggleable train?
    if ( SVG_HasSpawnFlags( self, TRAIN_TOGGLE ) ) {

        // ON:
        if ( useType == ENTITY_USETARGET_TYPE_ON || useValue == 1 ) {
            if ( self->targetEntities.target ) {
                train_resume( self );
            } else {
                train_next( self );
            }
        }
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
        if ( self->targetEntities.target )
            train_resume( self );
        else
            train_next( self );
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

void SP_func_train( edict_t *self ) {
    self->movetype = MOVETYPE_PUSH;
    self->s.entityType = ET_PUSHER;

    VectorClear( self->s.angles );
    self->blocked = train_blocked;
    if ( self->spawnflags & TRAIN_BLOCK_STOPS )
        self->dmg = 0;
    else {
        if ( !self->dmg )
            self->dmg = 100;
    }
    self->solid = SOLID_BSP;
    gi.setmodel( self, self->model );

    if ( st.noise )
        self->pushMoveInfo.sounds.middle = gi.soundindex( st.noise );

    if ( !self->speed )
        self->speed = 100;

    self->pushMoveInfo.speed = self->speed;
    self->pushMoveInfo.accel = self->pushMoveInfo.decel = self->pushMoveInfo.speed;

    self->use = train_use;

    gi.linkentity( self );

    if ( self->targetNames.target ) {
        // start trains on the second frame, to make sure their targets have had
        // a chance to spawn
        self->nextthink = level.time + FRAME_TIME_S;
        self->think = func_train_find;
    } else {
        gi.dprintf( "func_train without a target at %s\n", vtos( self->absmin ) );
    }
}