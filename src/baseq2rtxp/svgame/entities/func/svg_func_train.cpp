/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_train'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_callfunction.hpp"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_train.h"



#define TRAIN_START_ON      1
#define TRAIN_TOGGLE        2
#define TRAIN_BLOCK_STOPS   4

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
        T_Damage( other, self, self, vec3_origin, other->s.origin, vec3_origin, 100000, 1, 0, MEANS_OF_DEATH_CRUSHED );
        // if it's still there, nuke it
        if ( other )
            BecomeExplosion1( other );
        return;
    }

    if ( level.time < self->touch_debounce_time )
        return;

    if ( !self->dmg )
        return;
    self->touch_debounce_time = level.time + 0.5_sec;
    T_Damage( other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, 0, MEANS_OF_DEATH_CRUSHED );
}

void train_wait( edict_t *self ) {
    if ( self->targetEntities.target->targetNames.path ) {
        char *savetarget;
        edict_t *ent;

        ent = self->targetEntities.target;
        savetarget = ent->targetNames.target;
        ent->targetNames.target = ent->targetNames.path;
        SVG_UseTargets( ent, self->activator );
        ent->targetNames.target = savetarget;

        // make sure we didn't get killed by a targetNames.kill
        if ( !self->inuse )
            return;
    }

    if ( self->pushMoveInfo.wait ) {
        if ( self->pushMoveInfo.wait > 0 ) {
            self->nextthink = level.time + sg_time_t::from_sec( self->pushMoveInfo.wait );
            self->think = train_next;
        } else if ( self->spawnflags & TRAIN_TOGGLE ) { // && wait < 0
            train_next( self );
            self->spawnflags &= ~TRAIN_START_ON;
            VectorClear( self->velocity );
            self->nextthink = 0_ms;
        }

        if ( !( self->flags & FL_TEAMSLAVE ) ) {
            if ( self->pushMoveInfo.sound_end )
                gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sound_end, 1, ATTN_STATIC, 0 );
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

    ent = SVG_PickTarget( self->targetNames.target );
    if ( !ent ) {
        gi.dprintf( "train_next: bad target %s\n", self->targetNames.target );
        return;
    }

    self->targetNames.target = ent->targetNames.target;

    // check for a teleport path_corner
    if ( ent->spawnflags & 1 ) {
        if ( !first ) {
            gi.dprintf( "connected teleport path_corners, see %s at %s\n", ent->classname, vtos( ent->s.origin ) );
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
        if ( self->pushMoveInfo.sound_start )
            gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sound_start, 1, ATTN_STATIC, 0 );
        self->s.sound = self->pushMoveInfo.sound_middle;
    }

    VectorSubtract( ent->s.origin, self->mins, dest );
    self->pushMoveInfo.state = PUSHMOVE_STATE_TOP;
    VectorCopy( self->s.origin, self->pushMoveInfo.start_origin );
    VectorCopy( dest, self->pushMoveInfo.end_origin );
    SVG_PushMove_MoveCalculate( self, dest, train_wait );
    self->spawnflags |= TRAIN_START_ON;
}

void train_resume( edict_t *self ) {
    edict_t *ent;
    vec3_t  dest;

    ent = self->targetEntities.target;

    VectorSubtract( ent->s.origin, self->mins, dest );
    self->pushMoveInfo.state = PUSHMOVE_STATE_TOP;
    VectorCopy( self->s.origin, self->pushMoveInfo.start_origin );
    VectorCopy( dest, self->pushMoveInfo.end_origin );
    SVG_PushMove_MoveCalculate( self, dest, train_wait );
    self->spawnflags |= TRAIN_START_ON;
}

void func_train_find( edict_t *self ) {
    edict_t *ent;

    if ( !self->targetNames.target ) {
        gi.dprintf( "train_find: no target\n" );
        return;
    }
    ent = SVG_PickTarget( self->targetNames.target );
    if ( !ent ) {
        gi.dprintf( "train_find: target %s not found\n", self->targetNames.target );
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

void train_use( edict_t *self, edict_t *other, edict_t *activator ) {
    self->activator = activator;

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
        self->pushMoveInfo.sound_middle = gi.soundindex( st.noise );

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