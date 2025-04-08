/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_door'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_misc.h"
#include "svgame/svg_utils.h"

#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_gamelib.hpp"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_door.h"

#include "sharedgame/sg_usetarget_hints.h"


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
*
*
*
*   Use Callbacks:
*
*
*
**/
void door_usetarget_update_hint( edict_t *self ) {
    // Are we an actual use target at all?
    if ( !SVG_Entity_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_TOGGLE ) ) {
        return;
    }

    // Otherwise, set the hint ID based on the movement state.
    if ( self->pushMoveInfo.state == DOOR_STATE_OPENED || self->pushMoveInfo.state == DOOR_STATE_MOVING_TO_OPENED_STATE ) {
        SVG_Entity_SetUseTargetHintByID( self, USETARGET_HINT_ID_DOOR_CLOSE );
    } else if ( self->pushMoveInfo.state == DOOR_STATE_CLOSED || self->pushMoveInfo.state == DOOR_STATE_MOVING_TO_CLOSED_STATE ) {
        SVG_Entity_SetUseTargetHintByID( self, USETARGET_HINT_ID_DOOR_OPEN );
    }
}

/**
*	@brief  Open/Close the door's area portals.
**/
void door_use_areaportals( edict_t *self, const bool open ) {
    edict_t *t = NULL;

    if ( !self->targetNames.target )
        return;

    while ( ( t = SVG_Entities_Find( t, FOFS_GENTITY( targetname ), (const char *)self->targetNames.target ) ) ) {
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
    #if 1
    //SVG_UseTargets( self, self->activator, useType, useValue );
    // Get reference to sol lua state view.
    sol::state_view &solStateView = SVG_Lua_GetSolStateView();
    // We create these ourselves to make sure they are the appropriate type. 
    // (Other types got a constructor(edict_t*) as well. So we won't rely on it automatically resolving it, although it does at this moment VS2022)
    auto leSelf = sol::make_object<lua_edict_t>( solStateView, lua_edict_t( self ) );
    auto leOther = sol::make_object<lua_edict_t>( solStateView, lua_edict_t( other ) );
    auto leActivator = sol::make_object<lua_edict_t>( solStateView, lua_edict_t( activator ) );
    // Call into function.
    bool callReturnValue = false;
    bool calledFunction = LUA_CallLuaNameEntityFunction( self, "Use", 
        solStateView, 
        callReturnValue, LUA_CALLFUNCTION_VERBOSE_MISSING,
        leSelf, leOther, leActivator, useType, useValue 
    );
    #else
    // Need the luaName.
    if ( self->luaProperties.luaName ) {
        //// Generate function 'callback' name.
        const std::string luaFunctionName = std::string( self->luaProperties.luaName ) + "_Use";

        // Get reference to sol lua state view.
        sol::state_view &solState = SVG_Lua_GetSolState();

        // Get function object.
        sol::protected_function funcRefUse = solState[ luaFunctionName ];
        // Get type.
        sol::type funcRefType = funcRefUse.get_type();
        // Ensure it matches, accordingly
        if ( funcRefType != sol::type::function /*|| !funcRefSignalOut.is<std::function<void( Rest... )>>() */ ) {
            // Return if it is LUA_NOREF and luaState == nullptr again.
            // TODO: Error?
            return;
        }

        // Create lua userdata object references to the entities.
        auto leSelf = sol::make_object<lua_edict_t>( solState, lua_edict_t( self ) );
        auto leOther = sol::make_object<lua_edict_t>( solState, lua_edict_t( other ) );
        auto leActivator = sol::make_object<lua_edict_t>( solState, lua_edict_t( activator ) );
        // Fire SignalOut callback.
        auto callResult = funcRefUse( leSelf, leOther, leActivator, useType, useValue );
        // If valid, convert result to boolean.
        if ( callResult.valid() ) {
            // Convert.
            bool signalHandled = callResult.get<bool>();
            // Debug print.
            // We got an error:
        } else {
            // Acquire error object.
            sol::error resultError = callResult;
            // Get error string.
            const std::string errorStr = resultError.what();
            // Print the error in case of failure.
            gi.bprintf( PRINT_ERROR, "%s: %s\n ", __func__, errorStr.c_str() );
        }
    }
    #endif
}



/**
*
*
*
*   Open/Close/Toggle, Lock/UnLock:
*
*
*
**/
/**
*   @brief  Toggles the entire door team.
**/
void door_close_move( edict_t *self );
void door_open_move( edict_t *self/*, edict_t *activator */ );
void door_team_toggle( edict_t *self, edict_t *other, edict_t *activator, const bool stateIsOpen, const bool forceState = false ) {
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
    const bool entityIsCapable = ( SVG_Entity_IsClient( activator ) || activator->svflags & SVF_MONSTER ? true : false );

    // If the activator is a client or a monster, determine whether to play a locked door sound.
    //if ( entityIsCapable ) {
        // If we're locked while in either opened, or closed state, refuse to use target ourselves and play a locked sound.
        if ( self->pushMoveInfo.state == DOOR_STATE_OPENED || self->pushMoveInfo.state == DOOR_STATE_CLOSED ) {
            if ( self->pushMoveInfo.lockState.isLocked ) {
                if ( self->pushMoveInfo.lockState.lockedSound ) {
                    gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.lockState.lockedSound, 1, ATTN_STATIC, 0 );
                }
            }
        }
        // Escape if locked.
        if ( self->pushMoveInfo.lockState.isLocked ) {
            return;
        }
    //}

    // By default it is a 'Team Slave', and thus should exit. However, in the scenario of a client
    // performing a (+usetarget) key action, we want to try and activate the team master. This allows
    // for the team master to determine what to do next. (Which if the door is unlocked, means it'll
    // engage to toggle the door moving states.)
    if ( self->flags & FL_TEAMSLAVE ) {
        // Prevent ourselves from actually falling into recursion, make sure we got a client entity.
        if ( self->teammaster != self && self->teammaster->use/*&& entityIsCapable*/ ) {
            // Pass through to the team master to handle this.
            if ( self->teammaster->use ) {
                door_team_toggle( self->teammaster, other, activator, stateIsOpen, forceState );
            }
                return;
        // Default 'Team Slave' behavior:
        } else {
            return;
        }
    }
    #endif

    //if ( open == false || SVG_HasSpawnFlags( self, DOOR_SPAWNFLAG_TOGGLE ) ) {
    if ( ( forceState == true && stateIsOpen == false ) || ( forceState == false && SVG_HasSpawnFlags( self, DOOR_SPAWNFLAG_TOGGLE ) ) ) {
        if ( self->pushMoveInfo.state == DOOR_STATE_MOVING_TO_OPENED_STATE || self->pushMoveInfo.state == DOOR_STATE_OPENED ) {
            // trigger all paired doors
            for ( edict_t *ent = self->teammaster; ent; ent = ent->teamchain ) {
                ent->message = NULL;
                ent->touch = NULL;
                ent->activator = activator; // WID: We need to assign it right?
                ent->other = other;
                door_close_move( ent );
            }
            return;
        }
    }

    //if ( open == true || SVG_HasSpawnFlags( self, DOOR_SPAWNFLAG_TOGGLE ) ) {
    if ( ( forceState == true && stateIsOpen == true ) || ( forceState == false && SVG_HasSpawnFlags( self, DOOR_SPAWNFLAG_TOGGLE ) ) ) {
        // trigger all paired doors
        for ( edict_t *ent = self->teammaster; ent; ent = ent->teamchain ) {
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
*
*
*
*   SignalIn:
*
*
*
**/
/**
*   @brief  Signal Receiving:
**/
void door_onsignalin( edict_t *self, edict_t *other, edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments ) {
    /**
    *   Open/Close:
    **/
    // DoorOpen:
    if ( Q_strcasecmp( signalName, "Open" ) == 0 ) {
        self->activator = activator;
        self->other = other;

        // Signal all paired doors to open. (Presuming they are the same state, closed)
        //if ( SVG_HasSpawnFlags( self, DOOR_SPAWNFLAG_START_OPEN ) ) {
        //    door_team_toggle( self, self->other, self->activator, false, true );
        //} else {
            door_team_toggle( self, self->other, self->activator, true, true );
        //}
    }
    // DoorClose:
    if ( Q_strcasecmp( signalName, "Close" ) == 0 ) {
        self->activator = activator;
        self->other = other;
        // Signal all paired doors to open. (Presuming they are the same state, closed)
        //if ( SVG_HasSpawnFlags( self, DOOR_SPAWNFLAG_START_OPEN ) ) {
            door_team_toggle( self, self->other, self->activator, false, true );
        //} else {
        //    door_team_toggle( self, self->other, self->activator, true, true );
        //}
    }

    /**
    *   Lock/UnLock:
    **/
    // DoorLock:
    if ( Q_strcasecmp( signalName, "Lock" ) == 0 ) {
        self->activator = activator;
        self->other = other;
        door_lock( self );
    }
    // DoorUnlock:
    if ( Q_strcasecmp( signalName, "UnLock" ) == 0 ) {
        self->activator = activator;
        self->other = other;
        // If we're locked while in either opened, or closed state, unlock the door.
        door_unlock( self );
    }
    // DoorLockToggle:
    if ( Q_strcasecmp( signalName, "LockToggle" ) == 0 ) {
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
        if ( self->pushMoveInfo.sounds.end ) {
            gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sounds.end, 1, ATTN_STATIC, 0 );
        }
        self->s.sound = 0;
    }
    // Apply state.
    self->pushMoveInfo.state = DOOR_STATE_OPENED;
    // Dispatch a lua signal.
    SVG_SignalOut( self, self->other, self->activator, "OnOpened" );

    // Adjust areaportal.
    if ( SVG_HasSpawnFlags( self, DOOR_SPAWNFLAG_START_OPEN ) ) {
        // Update areaportals for PVS awareness.
        door_use_areaportals( self, false );
    }

    // If it is a toggle door, don't set any next think to 'go down' again.
    if ( self->spawnflags & DOOR_SPAWNFLAG_TOGGLE ) {
        // Thus we exit.
        return;
    }

    // If a wait time is set:
    if ( self->pushMoveInfo.wait >= 0 ) {
        // Assign close move think.
        self->think = door_close_move;
        // Tell it when to start closing.
        self->nextthink = level.time + QMTime::FromSeconds( self->pushMoveInfo.wait );
    }

}

/**
*	@brief
**/
void door_close_move_done( edict_t *self ) {
    if ( !( self->flags & FL_TEAMSLAVE ) ) {
        if ( self->pushMoveInfo.sounds.end ) {
            gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sounds.end, 1, ATTN_STATIC, 0 );
        }
        self->s.sound = 0;
    }
    
    // Used for condition checking, if we got a damage activating door we don't want to have it support pressing.
    const bool damageActivates = SVG_HasSpawnFlags( self, DOOR_SPAWNFLAG_DAMAGE_ACTIVATES );
    // Health trigger based door:
    if ( damageActivates ) {
        if ( self->max_health ) {
            self->takedamage = DAMAGE_YES;
            self->lifeStatus = LIFESTATUS_DEAD;
            self->health = self->max_health;
            self->pain = door_pain;
        }
    }

    // Adjust state.
    self->pushMoveInfo.state = DOOR_STATE_CLOSED;
    // Dispatch a lua signal.
    SVG_SignalOut( self, self->other, self->activator, "OnClosed" );

    // Adjust areaportal.
    if ( SVG_HasSpawnFlags( self, DOOR_SPAWNFLAG_START_OPEN ) ) {
        // Update areaportals for PVS awareness.
        door_use_areaportals( self, false );
    }
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
        if ( self->pushMoveInfo.sounds.start ) {
            gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sounds.start, 1, ATTN_STATIC, 0 );
        }
        self->s.sound = self->pushMoveInfo.sounds.middle;
    }

    //// Used for condition checking, if we got a damage activating door we don't want to have it support pressing.
    //const bool damageActivates = SVG_HasSpawnFlags( self, DOOR_SPAWNFLAG_DAMAGE_ACTIVATES );
    //// Health trigger based door:
    //if ( damageActivates ) {
    //    if ( self->max_health ) {
    //        self->takedamage = DAMAGE_YES;
    //        self->health = self->max_health;
    //    }
    //}


    // Engage moving to closed state.
    if ( strcmp( (const char *)self->classname, "func_door" ) == 0 ) {
        // Set state to closing.
        self->pushMoveInfo.state = DOOR_STATE_MOVING_TO_CLOSED_STATE;
        // Engage moving to closed state.
        SVG_PushMove_MoveCalculate( self, self->pushMoveInfo.startOrigin, door_close_move_done );
    } else if ( strcmp( (const char *)self->classname, "func_door_rotating" ) == 0 ) {
        // Set state to closing.
        self->pushMoveInfo.state = DOOR_STATE_MOVING_TO_CLOSED_STATE;
        // Engage moving to closed state.
        SVG_PushMove_AngleMoveCalculate( self, door_close_move_done );
    }

    // Since we're closing or closed, set usetarget hint ID to 'open door'.
    door_usetarget_update_hint( self );

    // Dispatch a lua signal.
    SVG_SignalOut( self, self->other, self->activator, "OnClose" );

    // Adjust areaportal.
    if ( SVG_HasSpawnFlags( self, DOOR_SPAWNFLAG_START_OPEN ) ) {
        // Update areaportals for PVS awareness.
        door_use_areaportals( self, true );
    }
}

/**
*	@brief
**/
void door_open_move( edict_t *self/*, edict_t *activator */) {
    if ( self->pushMoveInfo.state == DOOR_STATE_MOVING_TO_OPENED_STATE ) {
        return;     // already going up
    }

    // If we are already opened, and re-used:
    if ( self->pushMoveInfo.state == DOOR_STATE_OPENED ) {
        // Reset 'top pusher state'/'door opened state' wait time
        if ( self->pushMoveInfo.wait > 0 ) {
            self->nextthink = level.time + QMTime::FromSeconds( self->pushMoveInfo.wait );
        }
        // And exit. This results in having to wait longer if trigger/button spamming.
        return;
    }

    // Team Masters dictate the audio effects:
    if ( !( self->flags & FL_TEAMSLAVE ) ) {
        // Play start sound using start sound message.
        if ( self->pushMoveInfo.sounds.start ) {
            gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sounds.start, 1, ATTN_STATIC, 0 );
        }
        // Apply the 'moving' sound to the entity state itself.
        self->s.sound = self->pushMoveInfo.sounds.middle;
    }

    // Path for: func_door
    if ( strcmp( (const char *)self->classname, "func_door" ) == 0 ) {
        // Set state to opening.
        self->pushMoveInfo.state = DOOR_STATE_MOVING_TO_OPENED_STATE;
        // Engage door opening movement.
        SVG_PushMove_MoveCalculate( self, self->pushMoveInfo.endOrigin, door_open_move_done );
    // Path for: func_door_rotating:
    } else if ( strcmp( (const char *)self->classname, "func_door_rotating" ) == 0 ) {
        // Set state to opening.
        self->pushMoveInfo.state = DOOR_STATE_MOVING_TO_OPENED_STATE;
        // Engage door opening movement.
        SVG_PushMove_AngleMoveCalculate( self, door_open_move_done );
    }

    // Since we're closing or closed, set usetarget hint ID to 'open door'.
    door_usetarget_update_hint( self );

    // Fire use targets.
    SVG_UseTargets( self, self->activator );
    // Dispatch a lua signal.
    SVG_SignalOut( self, self->other, self->activator, "OnOpen" );

    if ( !SVG_HasSpawnFlags( self, DOOR_SPAWNFLAG_START_OPEN ) ) {
        // Update areaportals for PVS awareness.
        door_use_areaportals( self, true );
    }
}

/**
*	@brief
**/
void door_use( edict_t *self, edict_t *other, edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) {
    edict_t *ent;
    //gi.dprintf( "(%s:%i) debugging! :-)\n ", __func__, __LINE__ );

    // Determine whether the entity is capable of opening doors.
    const bool entityIsCapable = ( SVG_Entity_IsClient( activator ) || activator->svflags & SVF_MONSTER ? true : false );

    // If the activator is a client or a monster, determine whether to play a locked door sound.
    if ( entityIsCapable ) {
        // If we're locked while in either opened, or closed state, refuse to use target ourselves and play a locked sound.
        if ( self->pushMoveInfo.state == DOOR_STATE_OPENED || self->pushMoveInfo.state == DOOR_STATE_CLOSED ) {
            if ( self->pushMoveInfo.lockState.isLocked && self->pushMoveInfo.lockState.lockedSound ) {
                gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.lockState.lockedSound, 1, ATTN_STATIC, 0 );
            }
            if ( self->pushMoveInfo.lockState.isLocked ) {
                return;
            }
        }
    }

    // By default it is a 'Team Slave', and thus should exit. However, in the scenario of a client
    // performing a (+usetarget) key action, we want to try and activate the team master. This allows
    // for the team master to determine what to do next. (Which if the door is unlocked, means it'll
    // engage to toggle the door moving states.)
    if ( self->flags & FL_TEAMSLAVE ) {
        // If we got a client/monster entity, prevent ourselves from actually falling into recursion.
        // In case of single non teamed doors, self->teammaster can be self.
        if ( self->teammaster != self && entityIsCapable ) {
            // Pass through to the team master to handle this.
            if ( self->teammaster->use ) {
                self->teammaster->use( self->teammaster, other, activator, useType, useValue );
            }
            // Exit.
            return;
        // Default 'Team Slave' behavior:
        } else {
            // Exit.
            return;
        }
    }

    // Get some info.
    const bool isToggle = SVG_HasSpawnFlags( self, DOOR_SPAWNFLAG_TOGGLE );
    const bool isBothDirections = SVG_HasSpawnFlags( self, DOOR_SPAWNFLAG_BOTH_DIRECTIONS );
    const bool isRotating = strcmp( (const char *)self->classname, "func_door_rotating" ) == 0;
    const bool isReversed = SVG_HasSpawnFlags( self, DOOR_SPAWNFLAG_REVERSE );

    // Default sign, multiplied by -1 later on in case we're on the other side.
    float directionSign = ( isBothDirections ? ( isReversed ? -1.0f : 1.0f ) : 1.0f );
    // Only +usetargets capable entities can do this currently...
    if ( entityIsCapable && isBothDirections ) {
        // Angles, we multiply by movedir the move direction(unit vector) so that we are left
        // with the actual matching axis angle that we want to test against.
        Vector3 vActivatorAngles = self->movedir * activator->s.angles;
        //activatorAngles.x = 0;
        //activatorAngles.z = 0;
        // Calculate forward vector for activator.
        Vector3 vForward = {};
        QM_AngleVectors( vActivatorAngles, &vForward, nullptr, nullptr );
        // Calculate direction.
        Vector3 vActivatorOrigin = activator->s.origin;
        Vector3 vDir = vActivatorOrigin - self->s.origin;

        // Calculate estimated next frame 
        Vector3 vNextFrame = ( vActivatorOrigin + ( vForward * 10 ) ) - self->s.origin;
        // Determine direction sign.
        if ( ( vDir.x * vNextFrame.y - vDir.y * vNextFrame.x ) > 0 ) {
            directionSign *= -1.0f;
        }
    }

    // Recalculate endAngles in case we got a closed toggle usetargets door that goes both directions.
    if ( isToggle && isBothDirections && self->pushMoveInfo.state == DOOR_STATE_CLOSED ) {
        self->pushMoveInfo.endAngles = self->angles2 * directionSign;
    }

    if ( isToggle ) {
        // Engage closing if opened or moving into opened state.
        if ( self->pushMoveInfo.state == DOOR_STATE_MOVING_TO_OPENED_STATE || self->pushMoveInfo.state == DOOR_STATE_OPENED ) {
            // trigger all paired doors
            for ( ent = self; ent; ent = ent->teamchain ) {
                ent->message = NULL;
                ent->touch = NULL;
                ent->activator = activator; // WID: We need to assign it right?
                ent->other = other;
                door_close_move( ent );
                // gi.dprintf( "(%s:%i) door_close_move(..) with SIGN=%f\n", __func__, __LINE__, directionSign );
            }
            // Exit.
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
        //gi.dprintf( "(%s:%i) door_open_move(..) with SIGN=%f\n", __func__, __LINE__, directionSign );
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
        SVG_TriggerDamage( other, self, self, vec3_origin, other->s.origin, vec3_origin, 100000, 1, DAMAGE_NONE, MEANS_OF_DEATH_CRUSHED );
        // if it's still there, nuke it
        if ( other ) {
            SVG_Misc_BecomeExplosion( other, 1 );
        }
        return;
    }

    SVG_TriggerDamage( other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, DAMAGE_NONE, MEANS_OF_DEATH_CRUSHED );

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

    // By default it is a 'Team Slave', and thus should exit. However, in the scenario of a client
    // performing a (+usetarget) key action, we want to try and activate the team master. This allows
    // for the team master to determine what to do next. (Which if the door is unlocked, means it'll
    // engage to toggle the door moving states.)
    if ( self->flags & FL_TEAMSLAVE ) {
        // If we got a client/monster entity, prevent ourselves from actually falling into recursion.
        if ( self->teammaster != self ) {
            // Pass through to the team master to handle this.
            if ( self->teammaster && self->teammaster->use ) {
                door_killed( self->teammaster, inflictor, attacker, damage, point );
            }
            // Exit.
            return;
        // Default 'Team Slave' behavior:
        } else {
            // Exit.
            return;
        }
    }

    for ( ent = self->teammaster; ent; ent = ent->teamchain ) {
        // Used for condition checking, if we got a damage activating door we don't want to have it support pressing.
        const bool damageActivates = SVG_HasSpawnFlags( ent, DOOR_SPAWNFLAG_DAMAGE_ACTIVATES );
        // Health trigger based door:
        if ( damageActivates ) {
            ent->health = ent->max_health;
            ent->takedamage = DAMAGE_NO;
            ent->pain = nullptr;
        }

        // Dispatch a signal to each door team member.
        ent->other = inflictor;
        ent->activator = attacker;
        SVG_SignalOut( ent, ent->other, ent->activator, "OnKilled" );
    }

    // Fire its use targets.
    door_use( self, attacker, attacker, entity_usetarget_type_t::ENTITY_USETARGET_TYPE_TOGGLE, 1 );
    //SVG_UseTargets( self, attacker, entity_usetarget_type_t::ENTITY_USETARGET_TYPE_TOGGLE, 1 );
}
/**
*   @brief  Pain for door.
**/
void door_pain( edict_t *self, edict_t *other, float kick, int damage ) {
    const svg_signal_argument_array_t signalArguments = {
            {
                .type = SIGNAL_ARGUMENT_TYPE_NUMBER,
                .key = "kick",
                .value = {
                    .number = kick
                }
            },
            {
                .type = SIGNAL_ARGUMENT_TYPE_NUMBER, // TODO: WID: Was before sol, TYPE_INTEGER
                .key = "damage",
                .value = {
                    .integer = damage
                }
            }
    };

    for ( edict_t *ent = self->teammaster; ent; ent = ent->teamchain ) {
        // Dispatch a signal to each door team member.
        ent->other = other;
        ent->activator = other;
        SVG_SignalOut( ent, ent, ent->activator, "OnPain", signalArguments );
    }
}

/**
*	@brief
**/
void door_touch( edict_t *self, edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) {
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

    SVG_Util_SetMoveDir( ent->s.angles, ent->movedir );
    ent->movetype = MOVETYPE_PUSH;
    ent->solid = SOLID_BSP;
    ent->s.entityType = ET_PUSHER;
    ent->svflags |= SVF_DOOR;
    // BSP Model, or otherwise, specified external model.
    gi.setmodel( ent, ent->model );

    // Default sounds:
    if ( ent->sounds != 1 ) {
        ent->pushMoveInfo.sounds.start = gi.soundindex( "doors/door_start_01.wav" );
        ent->pushMoveInfo.sounds.middle = gi.soundindex( "doors/door_mid_01.wav" );
        ent->pushMoveInfo.sounds.end = gi.soundindex( "doors/door_end_01.wav" );

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
    ent->pain = door_pain;
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
        ent->pain = door_pain;
        // Apply next think time and method.
        ent->nextthink = level.time + FRAME_TIME_S;
        ent->think = SVG_PushMove_Think_CalculateMoveSpeed;
    // Touch based door:DOOR_SPAWNFLAG_DAMAGE_ACTIVATES
    } else if ( SVG_HasSpawnFlags( ent, DOOR_SPAWNFLAG_TOUCH_AREA_TRIGGERED ) ) {
        // Set its next think to create the trigger area.
        ent->nextthink = level.time + FRAME_TIME_S;
        ent->think = Think_SpawnDoorTrigger;
    } else {
        // Apply next think time and method.
        ent->nextthink = level.time + FRAME_TIME_S;
        ent->think = SVG_PushMove_Think_CalculateMoveSpeed;

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
            ent->useTarget.flags |= ENTITY_USETARGET_FLAG_DISABLED;
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
        ent->pushMoveInfo.startOrigin = ent->pos2;
        ent->pushMoveInfo.startAngles = ent->s.angles;
        ent->pushMoveInfo.endOrigin = ent->pos1;
        ent->pushMoveInfo.endAngles = ent->s.angles;
    // For UNPRESSED: pos1 = start, pos2 = end.
    } else {
        ent->pushMoveInfo.startOrigin = ent->pos1;
        ent->pushMoveInfo.startAngles = ent->s.angles;
        ent->pushMoveInfo.endOrigin = ent->pos2;
        ent->pushMoveInfo.endAngles = ent->s.angles;
    }

    // Since we're closing or closed, set usetarget hint ID to 'open door'.
    door_usetarget_update_hint( ent );

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
