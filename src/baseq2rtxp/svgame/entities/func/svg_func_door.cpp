/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_door'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_lua.h"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_door.h"



/**
*   For readability's sake:
**/
static constexpr int32_t DOOR_STATE_OPENED = PUSHMOVE_STATE_TOP;
static constexpr int32_t DOOR_STATE_CLOSED = PUSHMOVE_STATE_BOTTOM;
static constexpr int32_t DOOR_STATE_MOVING_TO_OPENED_STATE = PUSHMOVE_STATE_MOVING_UP;
static constexpr int32_t DOOR_STATE_MOVING_TO_CLOSED_STATE = PUSHMOVE_STATE_MOVING_DOWN;



/*
======================================================================

DOORS

  spawn a trigger surrounding the entire team unless it is
  already targeted by another

======================================================================
*/

/*QUAKED func_door (0 .5 .8) ? START_OPEN x CRUSHER NOMONSTER ANIMATED TOGGLE ANIMATED_FAST
TOGGLE      Wait in both the start and end states for a trigger event.
START_OPEN  The door to moves to its destination when spawned, and operate in reverse.  
            It is used to temporarily or permanently close off an area when triggered (not useful for touch or takedamage doors).
NOMONSTER   Monsters will not trigger this door.

"message"   Is printed when the door is touched if it is a trigger door and it hasn't been fired yet
"angle"     Determines the opening direction
"targetname" If set, no touch field will be spawned and a remote button or trigger field activates the door.
"health"    If set, door must be shot open
"speed"     Movement speed (100 default)
"wait"      Wait before returning (3 default, -1 = never return)
"lip"       Lip remaining at end of move (8 default)
"dmg"       Damage to inflict when blocked (2 default)
"sounds"
1)  silent
2)  light
3)  medium
4)  heavy
*/

/**
*	@brief  Open/Close the door's area portals.
**/
void door_use_areaportals( edict_t *self, const bool open ) {
    edict_t *t = NULL;

    if ( !self->targetNames.target )
        return;

    while ( ( t = SVG_Find( t, FOFS( targetname ), self->targetNames.target ) ) ) {
        //if (Q_stricmp(t->classname, "func_areaportal") == 0) {
        if ( t->s.entityType == ET_AREA_PORTAL ) {
            gi.SetAreaPortalState( t->style, open );

            //self->s.event = ( open ? EV_AREAPORTAL_OPEN : EV_AREAPORTAL_CLOSE );
            //self->s.event[ 1 ] = t->style;
            //self->s.event[ 2 ] = open;
        }
    }
}

/**
*   @brief  Fire use target lua function implementation if existant.
**/
void door_lua_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t &useType, const int32_t useValue ) {
    // Need the luaName.
    if ( self->luaProperties.luaName ) {
        // Generate function 'callback' name.
        const std::string luaFunctionName = std::string( self->luaProperties.luaName ) + "_Use";
        // Call if it exists.
        if ( LUA_HasFunction( SVG_Lua_GetMapLuaState(), luaFunctionName ) ) {
            LUA_CallFunction( SVG_Lua_GetMapLuaState(), luaFunctionName, 1, 5, LUA_CALLFUNCTION_VERBOSE_MISSING,
                /*[lua args]:*/ self, other, activator, useType, useValue );
        }
    }
}


/**
*   @brief  Toggles the entire door team.
**/
void door_close_move( edict_t *self );
void door_open_move( edict_t *self/*, edict_t *activator */ );
void door_team_toggle( edict_t *self, edict_t *other, edict_t *activator, const bool open ) {
    // Actually determine properly whether the door(its master) is locked or not.
    #if 0
    // Slave to team.
    if ( self->flags & FL_TEAMSLAVE ) {
        return;
    }

    // Play locked sound.
    if ( self->pushMoveInfo.lockState.isLocked ) {
        // Only play locked sound if toggled while door is idle.
        if ( self->pushMoveInfo.state == DOOR_STATE_OPENED || self->pushMoveInfo.state == DOOR_STATE_CLOSED ) {
            if ( self->pushMoveInfo.lockState.lockedSound ) {
                gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.lockState.lockedSound , 1, ATTN_STATIC, 0 );
            }
        }
        // Return, we're locked..
        return;
    }
    #else
    // Determine whether the entity is capable of opening doors.
    const bool entityIsCapable = ( SVG_IsClientEntity( activator ) || activator->svflags & SVF_MONSTER ? true : false );

    // If the activator is a client or a monster, determine whether to play a locked door sound.
    if ( entityIsCapable ) {
        // If we're locked while in either opened, or closed state, refuse to use target ourselves and play a locked sound.
        if ( self->pushMoveInfo.state == DOOR_STATE_OPENED || self->pushMoveInfo.state == DOOR_STATE_CLOSED ) {
            if ( self->pushMoveInfo.lockState.isLocked && self->pushMoveInfo.lockState.lockedSound ) {
                gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.lockState.lockedSound, 1, ATTN_STATIC, 0 );
                return;
            }
        }
    }

    // By default it is a 'Team Slave', and thus should exit. However, in the scenario of a client
    // performing a (+usetarget) key action, we want to try and activate the team master. This allows
    // for the team master to determine what to do next. (Which if the door is unlocked, means it'll
    // engage to toggle the door moving states.)
    if ( self->flags & FL_TEAMSLAVE ) {
        // Prevent ourselves from actually falling into recursion, make sure we got a client entity.
        //if ( self->teammaster != self && entityIsCapable ) {
        //    // Pass through to the team master to handle this.
        //    if ( self->teammaster->use ) {
        //        self->teammaster->use( self->teammaster, other, activator, useType, useValue );
        //    }
        //    return;
        //    // Default 'Team Slave' behavior:
        //} else {
        //    return;
        //}
        if ( self->teammaster == self || !entityIsCapable ) {
            return;
        // Default 'Team Slave' behavior:
        } else {
            return;
        }
    }
    #endif

    if ( SVG_HasSpawnFlags( self, DOOR_SPAWNFLAG_TOGGLE ) || open != true ) {
        if ( self->pushMoveInfo.state == DOOR_STATE_MOVING_TO_OPENED_STATE || self->pushMoveInfo.state == DOOR_STATE_OPENED ) {
            // trigger all paired doors
            for ( edict_t *ent = self; ent; ent = ent->teamchain ) {
                ent->message = NULL;
                ent->touch = NULL;
                ent->activator = activator; // WID: We need to assign it right?
                ent->other = other;
                door_close_move( ent );
            }
            return;
        }
    }

    if ( SVG_HasSpawnFlags( self, DOOR_SPAWNFLAG_TOGGLE ) || open != false ) {
        // trigger all paired doors
        for ( edict_t *ent = self; ent; ent = ent->teamchain ) {
            ent->message = NULL;
            ent->touch = NULL;
            ent->activator = activator; // WID: We need to assign it right?
            ent->other = other;
            door_open_move( ent/*, activator */ );
        }
    }
}

/**
*   @brief
**/
void door_lock( edict_t *self ) {
    // Door has to be either open, or closed, in order to allow for 'locking'.
    if ( self->pushMoveInfo.state == DOOR_STATE_OPENED || self->pushMoveInfo.state == DOOR_STATE_CLOSED ) {
        // Of course it has to be locked if we want to play a sound.
        if ( !self->pushMoveInfo.lockState.isLocked && self->pushMoveInfo.lockState.lockingSound ) {
            gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.lockState.lockingSound, 1, ATTN_STATIC, 0 );
        }
        // Last but not least, unlock its state.
        self->pushMoveInfo.lockState.isLocked = true;
    }
}
/**
*   @brief
**/
void door_unlock( edict_t *self ) {
    // Door has to be either open, or closed, in order to allow for 'unlocking'.
    if ( self->pushMoveInfo.state == DOOR_STATE_OPENED || self->pushMoveInfo.state == DOOR_STATE_CLOSED ) {
        // Of course it has to be locked if we want to play a sound.
        if ( self->pushMoveInfo.lockState.isLocked && self->pushMoveInfo.lockState.unlockingSound ) {
            gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.lockState.unlockingSound, 1, ATTN_STATIC, 0 );
        }
        // Last but not least, unlock its state.
        self->pushMoveInfo.lockState.isLocked = false;
    }
}
/**
*   @brief  Signal Receiving:
**/
void door_onsignalin( edict_t *self, edict_t *other, edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments ) {
    // DoorOpen:
    if ( Q_strcasecmp( signalName, "DoorOpen" ) == 0 ) {
        self->activator = activator;
        self->other = other;
        // Signal all paired doors to open. (Presuming they are the same state, closed)
        door_team_toggle( self, self->other, self->activator, true );
    }
    // DoorClose:
    if ( Q_strcasecmp( signalName, "DoorClose" ) == 0 ) {
        self->activator = activator;
        self->other = other;
        // Signal all paired doors to close. (Presuming they are the same state, opened)
        door_team_toggle( self, self->other, self->activator, false );
    }

    // DoorLock:
    if ( Q_strcasecmp( signalName, "DoorLock" ) == 0 ) {
        self->activator = activator;
        self->other = other;
        door_lock( self );
    }
    // DoorUnlock:
    if ( Q_strcasecmp( signalName, "DoorUnlock" ) == 0 ) {
        self->activator = activator;
        self->other = other;
        // If we're locked while in either opened, or closed state, unlock the door.
        door_unlock( self );
    }
    // DoorLockToggle:
    if ( Q_strcasecmp( signalName, "DoorLockToggle" ) == 0 ) {
        self->activator = activator;
        self->other = other;
        // Lock if unlocked:
        if ( !self->pushMoveInfo.lockState.isLocked ) {
            door_lock( self );
        // Unlock if locked:
        } else {
            door_unlock( self );
        }
    }

    // WID: Useful for debugging.
    #if 0
    const int32_t otherNumber = ( other ? other->s.number : -1 );
    const int32_t activatorNumber= ( activator ? activator->s.number : -1 );
    gi.dprintf( "door_onsignalin[ self(#%d), \"%s\", other(#%d), activator(%d) ]\n", self->s.number, signalName, otherNumber, activatorNumber );
    #endif
}


/**
*
*
*
*   PushMove - EndMove CallBacks:
*
*
*
**/
/**
*	@brief
**/
void door_close_move( edict_t *self );

/**
*	@brief
**/
void door_open_move_done( edict_t *self ) {
    if ( !( self->flags & FL_TEAMSLAVE ) ) {
        if ( self->pushMoveInfo.sound_end )
            gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sound_end, 1, ATTN_STATIC, 0 );
        self->s.sound = 0;
    }
    // Apply state.
    self->pushMoveInfo.state = DOOR_STATE_OPENED;

    // Dispatch a lua signal.
    SVG_SignalOut( self, self->other, self->activator, "OnOpened" );

    // If it is a toggle door, don't set any next think to 'go down' again.
    if ( self->spawnflags & DOOR_SPAWNFLAG_TOGGLE ) {
        return;
    }

    if ( self->pushMoveInfo.wait >= 0 ) {
        self->think = door_close_move;
        self->nextthink = level.time + sg_time_t::from_sec( self->pushMoveInfo.wait );
    }
}

/**
*	@brief
**/
void door_close_move_done( edict_t *self ) {
    if ( !( self->flags & FL_TEAMSLAVE ) ) {
        if ( self->pushMoveInfo.sound_end )
            gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sound_end, 1, ATTN_STATIC, 0 );
        self->s.sound = 0;
    }
    self->pushMoveInfo.state = DOOR_STATE_CLOSED;
    door_use_areaportals( self, false );

    // Dispatch a lua signal.
    SVG_SignalOut( self, self->other, self->activator, "OnClosed" );
}



/**
*
*
*
*   Close/Open Initiators:
*
*
*
**/
/**
*	@brief
**/
void door_close_move( edict_t *self ) {
    if ( !( self->flags & FL_TEAMSLAVE ) ) {
        if ( self->pushMoveInfo.sound_start )
            gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sound_start, 1, ATTN_STATIC, 0 );
        self->s.sound = self->pushMoveInfo.sound_middle;
    }
    if ( self->max_health ) {
        self->takedamage = DAMAGE_YES;
        self->health = self->max_health;
    }

    self->pushMoveInfo.state = DOOR_STATE_MOVING_TO_CLOSED_STATE;
    if ( strcmp( self->classname, "func_door" ) == 0 )
        SVG_PushMove_MoveCalculate( self, self->pushMoveInfo.start_origin, door_close_move_done );
    else if ( strcmp( self->classname, "func_door_rotating" ) == 0 )
        SVG_PushMove_AngleMoveCalculate( self, door_close_move_done );

    // Dispatch a lua signal.
    SVG_SignalOut( self, self->other, self->activator, "OnClose" );
}

/**
*	@brief
**/
void door_open_move( edict_t *self/*, edict_t *activator */) {
    if ( self->pushMoveInfo.state == DOOR_STATE_MOVING_TO_OPENED_STATE ) {
        return;     // already going up
    }

    if ( self->pushMoveInfo.state == DOOR_STATE_OPENED ) {
        // reset top wait time
        if ( self->pushMoveInfo.wait >= 0 ) {
            self->nextthink = level.time + sg_time_t::from_sec( self->pushMoveInfo.wait );
        }
        return;
    }

    if ( !( self->flags & FL_TEAMSLAVE ) ) {
        if ( self->pushMoveInfo.sound_start ) {
            gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sound_start, 1, ATTN_STATIC, 0 );
        }
        self->s.sound = self->pushMoveInfo.sound_middle;
    }
    self->pushMoveInfo.state = DOOR_STATE_MOVING_TO_OPENED_STATE;
    if ( strcmp( self->classname, "func_door" ) == 0 ) {
        SVG_PushMove_MoveCalculate( self, self->pushMoveInfo.end_origin, door_open_move_done );
    } else if ( strcmp( self->classname, "func_door_rotating" ) == 0 ) {
        SVG_PushMove_AngleMoveCalculate( self, door_open_move_done );
    }

    SVG_UseTargets( self, self->activator );
    door_use_areaportals( self, true );

    // Dispatch a lua signal.
    SVG_SignalOut( self, self->other, self->activator, "OnOpen" );
}

/**
*	@brief
**/
void door_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    edict_t *ent;
    //gi.dprintf( "(%s:%i) debugging! :-)\n ", __func__, __LINE__ );

    // Determine whether the entity is capable of opening doors.
    const bool entityIsCapable = ( SVG_IsClientEntity( activator ) || activator->svflags & SVF_MONSTER ? true : false );

    // If the activator is a client or a monster, determine whether to play a locked door sound.
    if ( entityIsCapable ) {
        // If we're locked while in either opened, or closed state, refuse to use target ourselves and play a locked sound.
        if ( self->pushMoveInfo.state == DOOR_STATE_OPENED || self->pushMoveInfo.state == DOOR_STATE_CLOSED ) {
            if ( self->pushMoveInfo.lockState.isLocked && self->pushMoveInfo.lockState.lockedSound ) {
                gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.lockState.lockedSound, 1, ATTN_STATIC, 0 );
                return;
            }
        }
    }

    // By default it is a 'Team Slave', and thus should exit. However, in the scenario of a client
    // performing a (+usetarget) key action, we want to try and activate the team master. This allows
    // for the team master to determine what to do next. (Which if the door is unlocked, means it'll
    // engage to toggle the door moving states.)
    if ( self->flags & FL_TEAMSLAVE ) {
        // Prevent ourselves from actually falling into recursion, make sure we got a client entity.
        if ( self->teammaster != self && entityIsCapable ) {
            // Pass through to the team master to handle this.
            if ( self->teammaster->use ) {
                self->teammaster->use( self->teammaster, other, activator, useType, useValue );
            }
            return;
        // Default 'Team Slave' behavior:
        } else {
            return;
        }
    }

    //gi.dprintf( "(%s:%i) debugging! :-)\n ", __func__, __LINE__ );
    // Toggle door.
    if ( self->spawnflags & DOOR_SPAWNFLAG_TOGGLE ) {
        if ( self->pushMoveInfo.state == DOOR_STATE_MOVING_TO_OPENED_STATE || self->pushMoveInfo.state == DOOR_STATE_OPENED ) {
            // trigger all paired doors
            for ( ent = self; ent; ent = ent->teamchain ) {
                ent->message = NULL;
                ent->touch = NULL;
                ent->activator = activator; // WID: We need to assign it right?
                ent->other = other;
                door_close_move( ent );
                //gi.dprintf( "(%s:%i) debugging! :-)\n ", __func__, __LINE__ );
            }
            return;
        }
    }

    // trigger all paired doors
    for ( ent = self; ent; ent = ent->teamchain ) {
        ent->message = NULL;
        ent->touch = NULL;
        ent->activator = activator; // WID: We need to assign it right?
        ent->other = other;
        door_open_move( ent/*, activator */);
        //gi.dprintf( "(%s:%i) debugging! :-)\n ", __func__, __LINE__ );
    }

    // Call upon Lua OnUse.
    door_lua_use( self, other, activator, useType, useValue );
}

/**
*	@brief
**/
void door_blocked( edict_t *self, edict_t *other ) {
    edict_t *ent;

    if ( !( other->svflags & SVF_MONSTER ) && ( !other->client ) ) {
        // give it a chance to go away on it's own terms (like gibs)
        SVG_TriggerDamage( other, self, self, vec3_origin, other->s.origin, vec3_origin, 100000, 1, 0, MEANS_OF_DEATH_CRUSHED );
        // if it's still there, nuke it
        if ( other ) {
            SVG_Misc_BecomeExplosion1( other );
        }
        return;
    }

    SVG_TriggerDamage( other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, 0, MEANS_OF_DEATH_CRUSHED );

    if ( self->spawnflags & DOOR_SPAWNFLAG_CRUSHER ) {
        return;
    }

    // if a door has a negative wait, it would never come back if blocked,
    // so let it just squash the object to death real fast
    if ( self->pushMoveInfo.wait >= 0 ) {
        if ( self->pushMoveInfo.state == DOOR_STATE_MOVING_TO_CLOSED_STATE ) {
            for ( ent = self->teammaster; ent; ent = ent->teamchain ) {
                ent->other = other;
                ent->activator = other;
                door_open_move( ent/*, ent->activator */ );
            }
        } else {
            for ( ent = self->teammaster; ent; ent = ent->teamchain ) {
                ent->other = other;
                ent->activator = other;
                door_close_move( ent );
            }
        }
    }
}

/**
*	@brief
**/
void door_killed( edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point ) {
    edict_t *ent;

    for ( ent = self->teammaster; ent; ent = ent->teamchain ) {
        ent->health = ent->max_health;
        ent->takedamage = DAMAGE_NO;

        // Dispatch a signal to each door team member.
        ent->other = inflictor;
        ent->activator = attacker;
        SVG_SignalOut( ent, ent->other, ent->activator, "OnKilled" );
    }

    // Fire its use targets.
    door_use( self->teammaster, attacker, attacker, entity_usetarget_type_t::ENTITY_USETARGET_TYPE_TOGGLE, 0 );

    //// Dispatch a signal.
    //self->other = inflictor;
    //self->activator = attacker;
    //SVG_SignalOut( self, self->other, self->activator, "OnKilled" );
}

/**
*	@brief
**/
void door_touch( edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf ) {
    if ( !other->client ) {
        return;
    }

    // WID: TODO: Send a 'OnTouch' Out Signal.
    #if 0

    if ( level.time < self->touch_debounce_time ) {
        return;
    }

    self->touch_debounce_time = level.time + 5_sec;

    gi.centerprintf( other, "%s", self->message );
    gi.sound( other, CHAN_AUTO, gi.soundindex( "hud/chat01.wav" ), 1, ATTN_NORM, 0 );
    #endif
}

/**
*	@brief
**/
void door_postspawn( edict_t *self ) {
    //if ( self->spawnflags & DOOR_START_OPEN ) {
    //    //SVG_UseTargets( self, self );
    //    door_use_areaportals( self, true );
    //    //self->pushMoveInfo.state = DOOR_STATE_OPENED;
    //}
}

/**
*	@brief
**/
void SP_func_door( edict_t *ent ) {

    SVG_SetMoveDir( ent->s.angles, ent->movedir );
    ent->movetype = MOVETYPE_PUSH;
    ent->solid = SOLID_BSP;
    ent->s.entityType = ET_PUSHER;
    // BSP Model, or otherwise, specified external model.
    gi.setmodel( ent, ent->model );

    // Default sounds:
    if ( ent->sounds != 1 ) {
        ent->pushMoveInfo.sound_start = gi.soundindex( "doors/door_start_01.wav" );
        ent->pushMoveInfo.sound_middle = gi.soundindex( "doors/door_mid_01.wav" );
        ent->pushMoveInfo.sound_end = gi.soundindex( "doors/door_end_01.wav" );

        ent->pushMoveInfo.lockState.lockedSound = gi.soundindex( "misc/door_locked.wav" );
        ent->pushMoveInfo.lockState.lockingSound = gi.soundindex( "misc/door_locking.wav" );
        ent->pushMoveInfo.lockState.unlockingSound = gi.soundindex( "misc/door_unlocking.wav" );
    }

    // PushMove defaults:
    if ( !ent->speed ) {
        ent->speed = 100;
    }
    #if 0
    // WID: TODO: We don't want this...
    if ( deathmatch->value ) {
        ent->speed *= 2;
    }
    #endif
    if ( !ent->accel ) {
        ent->accel = ent->speed;
    }
    if ( !ent->decel ) {
        ent->decel = ent->speed;
    }
    if ( !st.lip ) {
        st.lip = 8;
    }
    // Trigger defaults:
    if ( !ent->wait ) {
        ent->wait = 3;
    }
    if ( !ent->dmg ) {
        ent->dmg = 2;
    }

    // Callbacks.
    ent->postspawn = door_postspawn;
    ent->blocked = door_blocked;
    ent->touch = door_touch;
    ent->use = door_use;
    ent->onsignalin = door_onsignalin;

    // Calculate absolute move distance to get from pos1 to pos2.
    const Vector3 fabsMoveDirection = QM_Vector3Fabs( ent->movedir );
    ent->pushMoveInfo.distance = QM_Vector3DotProduct( fabsMoveDirection, ent->size ) - st.lip;
    // Translate the determined move distance into the move direction to get pos2, our move end origin.
    ent->pos1 = ent->s.origin;
    ent->pos2 = QM_Vector3MultiplyAdd( ent->pos1, ent->pushMoveInfo.distance, ent->movedir );

    // if it starts open, switch the positions
    if ( SVG_HasSpawnFlags( ent, DOOR_SPAWNFLAG_START_OPEN ) ) {
        VectorCopy( ent->pos2, ent->s.origin );
        ent->pos2 = ent->pos1;
        ent->pos1 = ent->s.origin;
        ent->pushMoveInfo.state = DOOR_STATE_OPENED;
    } else {
        // Initial closed state.
        ent->pushMoveInfo.state = DOOR_STATE_CLOSED;
    }

    // Used for condition checking, if we got a damage activating door we don't want to have it support pressing.
    const bool damageActivates = SVG_HasSpawnFlags( ent, DOOR_SPAWNFLAG_DAMAGE_ACTIVATES );
    // Health trigger based door:
    if ( damageActivates ) {
        // Set max health, also used to reinitialize the door to revive.
        ent->max_health = ent->health;
        // Let it take damage.
        ent->takedamage = DAMAGE_YES;
        // Die callback.
        ent->die = door_killed;
        // Apply next think time and method.
        ent->nextthink = level.time + FRAME_TIME_S;
        ent->think = Think_CalcMoveSpeed;
    // Touch based door:DOOR_SPAWNFLAG_DAMAGE_ACTIVATES
    } else if ( SVG_HasSpawnFlags( ent, DOOR_SPAWNFLAG_TOUCH_AREA_TRIGGERED ) ) {
        // Set its next think to create the trigger area.
        ent->nextthink = level.time + FRAME_TIME_S;
        ent->think = Think_SpawnDoorTrigger;
    } else {
        // Apply next think time and method.
        ent->nextthink = level.time + FRAME_TIME_S;
        ent->think = Think_CalcMoveSpeed;

        // This door is only toggled, never untoggled, by each (+usetarget) interaction.
        if ( SVG_HasSpawnFlags( ent, SPAWNFLAG_USETARGET_PRESSABLE ) ) {
            ent->useTarget.flags = ENTITY_USETARGET_FLAG_PRESS;
            // Remove touch door functionality, instead, reside to usetarget functionality.
            ent->touch = nullptr;
            ent->use = door_use;
            // This door is dispatches untoggle/toggle callbacks by each (+usetarget) interaction, based on its usetarget state.
        } else if ( SVG_HasSpawnFlags( ent, SPAWNFLAG_USETARGET_TOGGLEABLE ) ) {
            ent->useTarget.flags = ENTITY_USETARGET_FLAG_TOGGLE;
            // Remove touch door functionality, instead, reside to usetarget functionality.
            ent->touch = nullptr;
            ent->use = door_use;
        } else if ( SVG_HasSpawnFlags( ent, SPAWNFLAG_USETARGET_HOLDABLE ) ) {
            ent->useTarget.flags = ENTITY_USETARGET_FLAG_CONTINUOUS;
            // Remove touch door functionality, instead, reside to usetarget functionality.
            ent->touch = nullptr;
            ent->use = door_use;
        }

        // Is usetargetting disabled by default?
        if ( SVG_HasSpawnFlags( ent, SPAWNFLAG_USETARGET_DISABLED ) ) {
            ent->useTarget.flags = (entity_usetarget_flags_t)( ent->useTarget.flags | ENTITY_USETARGET_FLAG_DISABLED );
        }
    }

    // Copy the calculated info into the pushMoveInfo state struct.
    ent->pushMoveInfo.speed = ent->speed;
    ent->pushMoveInfo.accel = ent->accel;
    ent->pushMoveInfo.decel = ent->decel;
    ent->pushMoveInfo.wait = ent->wait;
    // For PRESSED: pos1 = start, pos2 = end.
    if ( SVG_HasSpawnFlags( ent, DOOR_SPAWNFLAG_START_OPEN ) ) {
        ent->pushMoveInfo.state = DOOR_STATE_OPENED;
        ent->pushMoveInfo.start_origin = ent->pos2;
        ent->pushMoveInfo.start_angles = ent->s.angles;
        ent->pushMoveInfo.end_origin = ent->pos1;
        ent->pushMoveInfo.end_angles = ent->s.angles;
    // For UNPRESSED: pos1 = start, pos2 = end.
    } else {
        ent->pushMoveInfo.start_origin = ent->pos1;
        ent->pushMoveInfo.start_angles = ent->s.angles;
        ent->pushMoveInfo.end_origin = ent->pos2;
        ent->pushMoveInfo.end_angles = ent->s.angles;
    }

    // Animated doors:
    if ( SVG_HasSpawnFlags( ent, DOOR_SPAWNFLAG_ANIMATED ) ) {
        ent->s.effects |= EF_ANIM_ALL;
    }
    if ( SVG_HasSpawnFlags( ent, DOOR_SPAWNFLAG_ANIMATED_FAST ) ) {
        ent->s.effects |= EF_ANIM_ALLFAST;
    }

    // Locked or not locked?
    if ( SVG_HasSpawnFlags( ent, DOOR_SPAWNFLAG_START_LOCKED ) ) {
        ent->pushMoveInfo.lockState.isLocked = true;
    }

    // To simplify logic elsewhere, make non-teamed doors into a team of one
    if ( !ent->targetNames.team ) {
        ent->teammaster = ent;
    }

    // Link it in.
    gi.linkentity( ent );
}
