/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_door'.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_misc.h"
#include "svgame/svg_trigger.h"
#include "svgame/svg_utils.h"

#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_gamelib.hpp"

#include "svgame/entities/svg_entities_pushermove.h"
#include "svgame/entities/func/svg_func_entities.h"
#include "svgame/entities/func/svg_func_areaportal.h"
#include "svgame/entities/func/svg_func_door.h"
#include "svgame/entities/func/svg_func_door_rotating.h"

#include "sharedgame/sg_entity_flags.h"
#include "sharedgame/sg_means_of_death.h"
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
void door_usetarget_update_hint( svg_func_door_t *self ) {
    // Are we an actual use target at all?
    if ( !SVG_Entity_HasUseTargetFlags( self, ENTITY_USETARGET_FLAG_TOGGLE ) ) {
        return;
    }

    // Otherwise, set the hint ID based on the movement state.
    if ( self->pushMoveInfo.state == svg_func_door_t::DOOR_STATE_OPENED || self->pushMoveInfo.state == svg_func_door_t::DOOR_STATE_MOVING_TO_OPENED_STATE ) {
        SVG_Entity_SetUseTargetHintByID( self, USETARGET_HINT_ID_DOOR_CLOSE );
    } else if ( self->pushMoveInfo.state == svg_func_door_t::DOOR_STATE_CLOSED || self->pushMoveInfo.state == svg_func_door_t::DOOR_STATE_MOVING_TO_CLOSED_STATE ) {
        SVG_Entity_SetUseTargetHintByID( self, USETARGET_HINT_ID_DOOR_OPEN );
    }
}

/**
*	@brief  Open or Close the door's area portal.
**/
void svg_func_door_t::SetAreaPortal( const bool isOpen ) {
    svg_base_edict_t *func_area = nullptr;

    if ( !targetNames.target ) {
        return;
    }

	// Find all func_areaportal entities with matching targetname.
	while ( ( func_area = SVG_Entities_Find( func_area, q_offsetof( svg_base_edict_t, targetname ), ( const char * )targetNames.target ) ) ) {
		const bool isAreaPortal =
			( func_area->s.entityType == ET_AREA_PORTAL ) ||
			( func_area->classname && strcmp( ( const char * )func_area->classname, "func_areaportal" ) == 0 );

		// Not an areaportal, skip it.
		if ( !isAreaPortal ) {
			continue;
		}

		// Use target the areaportal.
		svg_func_areaportal_t *areaPortal = static_cast< svg_func_areaportal_t * >( func_area );
		areaPortal->DispatchUseCallback( this, this, ENTITY_USETARGET_TYPE_SET, ( isOpen ? 1 : 0 ) );

        //// Update collision model portal state (flooding will be recalculated there)
        //gi.SetAreaPortalState( func_area->style, isOpen );
        //// Keep the areaportal entity itself in sync so server code / spawn logic
        //// that inspects the entity (and any networked fields) isn't left stale.
        //func_area->count = ( isOpen ? 1 : 0 );
        //// Re-link the entity so the server/client snapshots reflect the change.
        //gi.linkentity( func_area );

        // Developer debug to help track intermittent map-specific failures.
		gi.dprintf( "%s: SetAreaPortal targetname=\"%s\" style=%d isOpen=%d\n",
			__func__, ( func_area->targetname ? ( const char * )func_area->targetname : "<null>" ),
			func_area->style, ( int )func_area->count );
    }
}

/**
*   @brief  Fire use target lua function implementation if existant.
**/
void door_lua_use( svg_func_door_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t &useType, const int32_t useValue ) {
    #if 1
    //SVG_UseTargets( self, self->activator, useType, useValue );
    // Get reference to sol lua state view.
    sol::state_view &solStateView = SVG_Lua_GetSolStateView();
    // We create these ourselves to make sure they are the appropriate type. 
    // (Other types got a constructor(svg_base_edict_t*) as well. So we won't rely on it automatically resolving it, although it does at this moment VS2022)
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
void door_team_toggle( svg_func_door_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const bool stateIsOpen, const bool forceState = false ) {
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
    const bool entityIsCapable = ( SVG_Entity_IsClient( activator ) || activator->svFlags & SVF_MONSTER ? true : false );

    // If the activator is a client or a monster, determine whether to play a locked door sound.
    //if ( entityIsCapable ) {
        // If we're locked while in either opened, or closed state, refuse to use target ourselves and play a locked sound.
        if ( self->pushMoveInfo.state == svg_func_door_t::DOOR_STATE_OPENED || self->pushMoveInfo.state == svg_func_door_t::DOOR_STATE_CLOSED ) {
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
        if ( self->teammaster != self && /*self->teammaster->HasUseCallback() && */self->teammaster->GetTypeInfo()->IsSubClassType<svg_pushmove_edict_t>()/*&& entityIsCapable*/ ) {
            // Pass through to the team master to handle this.
            if ( self->teammaster->HasUseCallback() ) {
                door_team_toggle( (svg_func_door_t *)( self->teammaster ), other, activator, stateIsOpen, forceState );
            }
            return;
        // Default 'Team Slave' behavior:
        } else {
            return;
        }
    }
    #endif

    //if ( open == false || SVG_HasSpawnFlags( self, svg_func_door_t::SPAWNFLAG_TOGGLE ) ) {
    if ( ( forceState == true && stateIsOpen == false ) || ( forceState == false && SVG_HasSpawnFlags( self, svg_func_door_t::SPAWNFLAG_TOGGLE ) ) ) {
        if ( self->pushMoveInfo.state == svg_func_door_t::DOOR_STATE_MOVING_TO_OPENED_STATE || self->pushMoveInfo.state == svg_func_door_t::DOOR_STATE_OPENED ) {
            // trigger all paired doors
            for ( svg_base_edict_t *ent = self->teammaster; ent; ent = ent->teamchain ) {
                ent->message = nullptr;
                ent->SetTouchCallback( nullptr );
                ent->activator = activator; // WID: We need to assign it right?
                ent->other = other;
                if ( ent->GetTypeInfo()->IsSubClassType<svg_func_door_t>() ) {
                    svg_func_door_t *pushMoveEnt = static_cast<svg_func_door_t *>( ent );
                    pushMoveEnt->onThink_CloseMove(pushMoveEnt);
                }
            }
            return;
        }
    }

    //if ( open == true || SVG_HasSpawnFlags( self, svg_func_door_t::SPAWNFLAG_TOGGLE ) ) {
    if ( ( forceState == true && stateIsOpen == true ) || ( forceState == false && SVG_HasSpawnFlags( self, svg_func_door_t::SPAWNFLAG_TOGGLE ) ) ) {
        // trigger all paired doors
        for ( svg_base_edict_t *ent = self->teammaster; ent; ent = ent->teamchain ) {
            ent->message = nullptr;
            ent->SetTouchCallback( nullptr );
            ent->activator = activator; // WID: We need to assign it right?
            ent->other = other;
            if ( ent->GetTypeInfo()->IsSubClassType<svg_func_door_t>() ) {
                svg_func_door_t *pushMoveEnt = static_cast<svg_func_door_t *>( ent );
                pushMoveEnt->onThink_OpenMove( pushMoveEnt/*, activator */ );
            }
        }
    }
}

/**
*   @brief
**/
void door_lock( svg_func_door_t *self ) {
    // Door has to be either open, or closed, in order to allow for 'locking'.
    if ( self->pushMoveInfo.state == svg_func_door_t::DOOR_STATE_OPENED || self->pushMoveInfo.state == svg_func_door_t::DOOR_STATE_CLOSED ) {
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
void door_unlock( svg_func_door_t *self ) {
    // Door has to be either open, or closed, in order to allow for 'unlocking'.
    if ( self->pushMoveInfo.state == svg_func_door_t::DOOR_STATE_OPENED || self->pushMoveInfo.state == svg_func_door_t::DOOR_STATE_CLOSED ) {
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
DEFINE_MEMBER_CALLBACK_ON_SIGNALIN( svg_func_door_t, onSignalIn )( svg_func_door_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments ) -> void {
    /**
    *   Open/Close:
    **/
    // DoorOpen:
    if ( Q_strcasecmp( signalName, "Open" ) == 0 ) {
        self->activator = activator;
        self->other = other;

        // Signal all paired doors to open. (Presuming they are the same state, closed)
        //if ( SVG_HasSpawnFlags( self, svg_func_door_t::SPAWNFLAG_START_OPEN ) ) {
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
        //if ( SVG_HasSpawnFlags( self, svg_func_door_t::SPAWNFLAG_START_OPEN ) ) {
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
DEFINE_MEMBER_CALLBACK_PUSHMOVE_ENDMOVE( svg_func_door_t, onOpenEndMove )( svg_func_door_t *self ) -> void {
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
  //  if ( SVG_HasSpawnFlags( self, svg_func_door_t::SPAWNFLAG_START_OPEN ) ) {
  //      if ( self->GetTypeInfo()->IsSubClassType<svg_func_door_t>() ) {
		//	// Update areaportals for PVS awareness.
		//	self->SetAreaPortal( false );
		//}
  //  }

    //// Adjust areaportal: door is now opened -> portal should be open
    //if ( self->GetTypeInfo()->IsSubClassType<svg_func_door_t>() ) {
    //    static_cast<svg_func_door_t *>( self )->SetAreaPortal( true );
    //}

    // If it is a toggle door, don't set any next think to 'go down' again.
    if ( self->spawnflags & svg_func_door_t::SPAWNFLAG_TOGGLE ) {
        // Thus we exit.
        return;
    }

    // If a wait time is set:
    if ( self->pushMoveInfo.wait >= 0 ) {
        // Assign close move think.
        self->SetThinkCallback( &self->onThink_CloseMove );
        // Tell it when to start closing.
        self->nextthink = level.time + QMTime::FromSeconds( self->pushMoveInfo.wait );
    }
}

/**
*	@brief
**/
DEFINE_MEMBER_CALLBACK_PUSHMOVE_ENDMOVE( svg_func_door_t, onCloseEndMove )( svg_func_door_t *self ) -> void {
    if ( !( self->flags & FL_TEAMSLAVE ) ) {
        if ( self->pushMoveInfo.sounds.end ) {
            gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sounds.end, 1, ATTN_STATIC, 0 );
        }
        self->s.sound = 0;
    }
    
    // Used for condition checking, if we got a damage activating door we don't want to have it support pressing.
    const bool damageActivates = SVG_HasSpawnFlags( self, svg_func_door_t::SPAWNFLAG_DAMAGE_ACTIVATES );
    // Health trigger based door:
    if ( damageActivates ) {
        if ( self->max_health ) {
            self->takedamage = DAMAGE_YES;
            self->lifeStatus = LIFESTATUS_DEAD;
            self->health = self->max_health;
            self->SetPainCallback( &svg_func_door_t::onPain );
        }
    }

    // Adjust state.
    self->pushMoveInfo.state = DOOR_STATE_CLOSED;
    // Dispatch a lua signal.
    SVG_SignalOut( self, self->other, self->activator, "OnClosed" );

    // Adjust areaportal.
    //if ( SVG_HasSpawnFlags( self, svg_func_door_t::SPAWNFLAG_START_OPEN ) ) {
    //    if ( self->GetTypeInfo()->IsSubClassType<svg_func_door_t>() ) {
    //        // Update areaportals for PVS awareness.
    //        self->SetAreaPortal( false );
    //    }
    //}
    // Update areaportals for PVS awareness.
    // Adjust areaportal: door is now closed -> portal should be closed
    if ( self->GetTypeInfo()->IsSubClassType<svg_func_door_t>() ) {
        self->SetAreaPortal( false );
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
DEFINE_MEMBER_CALLBACK_THINK( svg_func_door_t, onThink_CloseMove )( svg_func_door_t *self/*, svg_base_edict_t *activator */ ) -> void {

    if ( !( self->flags & FL_TEAMSLAVE ) ) {
        if ( self->pushMoveInfo.sounds.start ) {
            gi.sound( self, CHAN_NO_PHS_ADD + CHAN_VOICE, self->pushMoveInfo.sounds.start, 1, ATTN_STATIC, 0 );
        }
        self->s.sound = self->pushMoveInfo.sounds.middle;
    }

    //// Used for condition checking, if we got a damage activating door we don't want to have it support pressing.
    //const bool damageActivates = SVG_HasSpawnFlags( self, svg_func_door_t::SPAWNFLAG_DAMAGE_ACTIVATES );
    //// Health trigger based door:
    //if ( damageActivates ) {
    //    if ( self->max_health ) {
    //        self->takedamage = DAMAGE_YES;
    //        self->health = self->max_health;
    //    }
    //}


    // The rotating door type is higher up the hierachy, so test for that first.
    // Path for: func_door_rotating:
    if ( self->GetTypeInfo()->IsSubClassType<svg_func_door_t>( true ) ) {
        // Set state to closing.
        self->pushMoveInfo.state = DOOR_STATE_MOVING_TO_CLOSED_STATE;
        // Engage moving to closed state.
        self->CalculateAngularMove( reinterpret_cast<svg_pushmove_endcallback>( &svg_func_door_rotating_t::onCloseEndMove ) );
    }
    // Path for: func_door
    else if ( self->GetTypeInfo()->IsSubClassType<svg_func_door_t>() ) {
        // Set state to closing.
        self->pushMoveInfo.state = DOOR_STATE_MOVING_TO_CLOSED_STATE;
        // Engage moving to closed state.
        self->CalculateDirectionalMove( self->pushMoveInfo.startOrigin, reinterpret_cast<svg_pushmove_endcallback>( &svg_func_door_t::onCloseEndMove ) );
    }

    // Since we're closing or closed, set usetarget hint ID to 'open door'.
    door_usetarget_update_hint( self );

    // Dispatch a lua signal.
    SVG_SignalOut( self, self->other, self->activator, "OnClose" );

    // Adjust areaportal.
    //if ( SVG_HasSpawnFlags( self, svg_func_door_t::SPAWNFLAG_START_OPEN ) ) {
    //    if ( self->GetTypeInfo()->IsSubClassType<svg_func_door_t>() ) {
    //        // Update areaportals for PVS awareness.
    //        static_cast<svg_func_door_t*>( self )->SetAreaPortal( true );
    //    }
    //}
    //    // Adjust areaportal: starting to close -> mark portal closed
    //if ( self->GetTypeInfo()->IsSubClassType<svg_func_door_t>() ) {
    //    static_cast<svg_func_door_t *>( self )->SetAreaPortal( false );
    //}
}

/**
*	@brief
**/
DEFINE_MEMBER_CALLBACK_THINK( svg_func_door_t, onThink_OpenMove )( svg_func_door_t *self/*, svg_base_edict_t *activator */ ) -> void {
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

    // The rotating door type is higher up the hierachy, so test for that first.
    // Path for: func_door_rotating:
    if ( self->GetTypeInfo()->IsSubClassType<svg_func_door_t>( true ) ) {
        // Set state to opening.
        self->pushMoveInfo.state = DOOR_STATE_MOVING_TO_OPENED_STATE;
        // Engage door opening movement.
        self->CalculateAngularMove( reinterpret_cast<svg_pushmove_endcallback>( &svg_func_door_rotating_t::onOpenEndMove ) );
    // Path for: func_door
    } else if ( self->GetTypeInfo()->IsSubClassType<svg_func_door_t>() ) {
        // Set state to opening.
        self->pushMoveInfo.state = DOOR_STATE_MOVING_TO_OPENED_STATE;
        // Engage door opening movement.
        self->CalculateDirectionalMove( self->pushMoveInfo.endOrigin, reinterpret_cast<svg_pushmove_endcallback>( &svg_func_door_t::onOpenEndMove ) );
    }

    // Since we're closing or closed, set usetarget hint ID to 'open door'.
    door_usetarget_update_hint( self );

    // Fire use targets.
    SVG_UseTargets( self, self->activator );
    // Dispatch a lua signal.
    SVG_SignalOut( self, self->other, self->activator, "OnOpen" );

    // Adjust areaportal: starting to open -> mark portal open
    if ( self->GetTypeInfo()->IsSubClassType<svg_func_door_t>() ) {
        self->SetAreaPortal( true );
    }
}

/**
*	@brief
**/
DEFINE_MEMBER_CALLBACK_USE( svg_func_door_t, onUse )( svg_func_door_t *self, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue ) -> void {
    svg_base_edict_t *ent;
    //gi.dprintf( "(%s:%i) debugging! :-)\n ", __func__, __LINE__ );

    // Determine whether the entity is capable of opening doors.
    const bool entityIsCapable = ( SVG_Entity_IsClient( activator ) || activator->svFlags & SVF_MONSTER ? true : false );

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
            if ( self->teammaster->HasUseCallback() ) {
                self->teammaster->DispatchUseCallback( other, activator, useType, useValue );
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
    const bool isToggle = SVG_HasSpawnFlags( self, svg_func_door_t::SPAWNFLAG_TOGGLE );
    const bool isBothDirections = SVG_HasSpawnFlags( self, svg_func_door_t::SPAWNFLAG_BOTH_DIRECTIONS );
    const bool isRotating = strcmp( (const char *)self->classname, "func_door_rotating" ) == 0;
    const bool isReversed = SVG_HasSpawnFlags( self, svg_func_door_t::SPAWNFLAG_REVERSE );

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
        Vector3 vActivatorOrigin = activator->currentOrigin;
        Vector3 vDir = vActivatorOrigin - self->currentOrigin;

        // Calculate estimated next frame 
        Vector3 vNextFrame = ( vActivatorOrigin + ( vForward * 10 ) ) - self->currentOrigin;
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
                ent->message = nullptr;
                ent->SetTouchCallback( nullptr );
                ent->activator = activator; // WID: We need to assign it right?
                ent->other = other;
                if ( ent->GetTypeInfo()->IsSubClassType<svg_func_door_t>() ) {
                    svg_func_door_t *doorEnt = static_cast<svg_func_door_t *>( ent );
                    doorEnt->onThink_CloseMove( doorEnt );
                }
                // gi.dprintf( "(%s:%i) door_close_move(..) with SIGN=%f\n", __func__, __LINE__, directionSign );
            }
            // Exit.
            return;
        }
    }

    // trigger all paired doors
    for ( ent = self; ent; ent = ent->teamchain ) {
        ent->message = nullptr;
        ent->SetTouchCallback( nullptr );
        ent->activator = activator; // WID: We need to assign it right?
        ent->other = other;
        if ( ent->GetTypeInfo()->IsSubClassType<svg_func_door_t>() ) {
            svg_func_door_t *doorEnt = static_cast<svg_func_door_t *>( ent );
            doorEnt->onThink_OpenMove( doorEnt/*, activator */ );
        }
        //gi.dprintf( "(%s:%i) svg_func_door_t::onThink_OpenMove(..) with SIGN=%f\n", __func__, __LINE__, directionSign );
    }

    // Call upon Lua OnUse.
    door_lua_use( self, other, activator, useType, useValue );
}

/**
*	@brief  Blocked Callback
**/
DEFINE_MEMBER_CALLBACK_BLOCKED( svg_func_door_t, onBlocked )( svg_func_door_t *self, svg_base_edict_t *other ) -> void {
    svg_base_edict_t *ent;

    if ( !( other->svFlags & SVF_MONSTER ) && ( !other->client ) ) {
        // give it a chance to go away on it's own terms (like gibs)
        SVG_DamageEntity( other, self, self, vec3_origin, other->s.origin, vec3_origin, 100000, 1, DAMAGE_NONE, MEANS_OF_DEATH_CRUSHED );
        // if it's still there, nuke it
        if ( other ) {
            SVG_Misc_BecomeExplosion( other, 1 );
        }
        return;
    }
	// Use currentOrigin for clients/monsters, as they are moving around.
    SVG_DamageEntity( other, self, self, vec3_origin, other->currentOrigin, vec3_origin, self->dmg, 1, DAMAGE_NONE, MEANS_OF_DEATH_CRUSHED );

    if ( self->spawnflags & svg_func_door_t::SPAWNFLAG_CRUSHER ) {
        return;
    }

    // if a door has a negative wait, it would never come back if blocked,
    // so let it just squash the object to death real fast
    if ( self->pushMoveInfo.wait >= 0 ) {
        if ( self->pushMoveInfo.state == DOOR_STATE_MOVING_TO_CLOSED_STATE ) {
            for ( ent = self->teammaster; ent; ent = ent->teamchain ) {
                ent->other = other;
                ent->activator = other;
                if ( ent->GetTypeInfo()->IsSubClassType<svg_func_door_t>() ) {
                    svg_func_door_t *pushMoveEnt = static_cast<svg_func_door_t *>( ent );
                    pushMoveEnt->onThink_OpenMove( pushMoveEnt/*, ent->activator */ );
                }
            }
        } else {
            for ( ent = self->teammaster; ent; ent = ent->teamchain ) {
                ent->other = other;
                ent->activator = other;
                if ( ent->GetTypeInfo()->IsSubClassType<svg_func_door_t>() ) {
                    svg_func_door_t *pushMoveEnt = static_cast<svg_func_door_t *>( ent );
                    pushMoveEnt->onThink_CloseMove( pushMoveEnt );
                }
            }
        }
    }
}

/**
*	@brief
**/
DEFINE_MEMBER_CALLBACK_DIE( svg_func_door_t, onDie )( svg_func_door_t *self, svg_base_edict_t *inflictor, svg_base_edict_t *attacker, int32_t damage, Vector3 *point ) -> void {
    svg_base_edict_t *ent;

    // By default it is a 'Team Slave', and thus should exit. However, in the scenario of a client
    // performing a (+usetarget) key action, we want to try and activate the team master. This allows
    // for the team master to determine what to do next. (Which if the door is unlocked, means it'll
    // engage to toggle the door moving states.)
    if ( self->flags & FL_TEAMSLAVE ) {
        // If we got a client/monster entity, prevent ourselves from actually falling into recursion.
        if ( self->teammaster != self ) {
            // Pass through to the team master to handle this.
            if ( self->teammaster && self->teammaster->HasDieCallback() ) {
                self->teammaster->DispatchDieCallback( inflictor, attacker, damage, point );
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
        const bool damageActivates = SVG_HasSpawnFlags( ent, svg_func_door_t::SPAWNFLAG_DAMAGE_ACTIVATES );
        // Health trigger based door:
        if ( damageActivates ) {
            ent->health = ent->max_health;
            ent->takedamage = DAMAGE_NO;
            ent->SetPainCallback( nullptr );
        }

        // Dispatch a signal to each door team member.
        ent->other = inflictor;
        ent->activator = attacker;
        SVG_SignalOut( ent, ent->other, ent->activator, "OnKilled" );
    }

    // Fire its use targets.
    self->DispatchUseCallback( attacker, attacker, entity_usetarget_type_t::ENTITY_USETARGET_TYPE_TOGGLE, 1 );
    //SVG_UseTargets( self, attacker, entity_usetarget_type_t::ENTITY_USETARGET_TYPE_TOGGLE, 1 );
}
/**
*   @brief  Pain for door.
**/
DEFINE_MEMBER_CALLBACK_PAIN( svg_func_door_t, onPain )( svg_func_door_t *self, svg_base_edict_t *other, const float kick, const int32_t damage, const entity_damageflags_t damageFlags ) -> void {
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

    for ( svg_base_edict_t *ent = self->teammaster; ent; ent = ent->teamchain ) {
        // Dispatch a signal to each door team member.
        ent->other = other;
        ent->activator = other;
        SVG_SignalOut( ent, ent, ent->activator, "OnPain", signalArguments );
    }
}

/**
*	@brief  Touch Callback.
**/
DEFINE_MEMBER_CALLBACK_TOUCH( svg_func_door_t, onTouch )( svg_func_door_t *self, svg_base_edict_t *other, const cm_plane_t *plane, cm_surface_t *surf ) -> void {
    if ( !other->client ) {
        return;
    }

    // WID: TODO: Send a 'OnTouch' Out Signal.
    #if 0

    if ( level.time < self->touch_debounce_time ) {
        return;
    }

    self->touch_debounce_time = level.time + 5_sec;

    gi.centerprintf( other, "%s", (const char*)self->messag );
    gi.sound( other, CHAN_AUTO, gi.soundindex( "hud/chat01.wav" ), 1, ATTN_NORM, 0 );
    #endif
}

/**
*	@brief
**/
DEFINE_MEMBER_CALLBACK_POSTSPAWN( svg_func_door_t, onPostSpawn )( svg_func_door_t *self ) -> void {
    if ( self->pushMoveInfo.state == DOOR_STATE_OPENED ) {
        SVG_UseTargets( self, self, ENTITY_USETARGET_TYPE_SET, 1 );
        //self->SetAreaPortal( true );
        //self->pushMoveInfo.state = DOOR_STATE_OPENED;
	} else {
		//self->SetAreaPortal( false );
		SVG_UseTargets( self, self, ENTITY_USETARGET_TYPE_SET, 0 );
	}
}

/**
*	@brief
**/
DEFINE_MEMBER_CALLBACK_SPAWN( svg_func_door_t, onSpawn )( svg_func_door_t *self ) -> void {
    //vec3_t  abs_movedir;
    svg_pushmove_edict_t::onSpawn( self );

    SVG_Util_SetMoveDir( self->s.angles, self->movedir );
    self->movetype = MOVETYPE_PUSH;
    self->solid = SOLID_BSP;
    self->s.entityType = ET_PUSHER;
    self->svFlags |= SVF_DOOR;
    // BSP Model, or otherwise, specified external model.
    gi.setmodel( self, self->model );

    // Default sounds:
    if ( self->sounds != 1 ) {
        self->SetupDefaultSounds();
    }

    // PushMove defaults:
    if ( !self->speed ) {
        self->speed = 100;
    }
    #if 0
    // WID: TODO: We don't want this...
    if ( deathmatch->value ) {
        self->speed *= 2;
    }
    #endif
    if ( !self->accel ) {
        self->accel = self->speed;
    }
    if ( !self->decel ) {
        self->decel = self->speed;
    }
    if ( !self->lip ) {
        self->lip = 8;
    }
    // Trigger defaults:
    if ( !self->wait ) {
        self->wait = 3;
    }
    if ( !self->dmg ) {
        self->dmg = 2;
    }

    // Callbacks.
    self->SetPostSpawnCallback( &svg_func_door_t::onPostSpawn );
    self->SetBlockedCallback( &svg_func_door_t::onBlocked );
    self->SetTouchCallback( &svg_func_door_t::onTouch );
    self->SetUseCallback( &svg_func_door_t::onUse );
    self->SetPainCallback( &svg_func_door_t::onPain );
    self->SetOnSignalInCallback( &svg_func_door_t::onSignalIn );

    // Calculate absolute move distance to get from pos1 to pos2.
    const Vector3 fabsMoveDirection = QM_Vector3Fabs( self->movedir );
    self->pushMoveInfo.distance = QM_Vector3DotProduct( fabsMoveDirection, self->size ) - self->lip;
    // Translate the determined move distance into the move direction to get pos2, our move end origin.
    self->pos1 = self->s.origin;
    self->pos2 = QM_Vector3MultiplyAdd( self->pos1, self->pushMoveInfo.distance, self->movedir );

    // if it starts open, switch the positions
    if ( SVG_HasSpawnFlags( self, svg_func_door_t::SPAWNFLAG_START_OPEN ) ) {
		// Setup the origins and angles for movement.
        VectorCopy( self->pos2, self->s.origin );
        self->pos2 = self->pos1;
        self->pos1 = self->s.origin;
        self->pushMoveInfo.state = DOOR_STATE_OPENED;
    } else {
        // Initial closed state.
        self->pushMoveInfo.state = DOOR_STATE_CLOSED;
    }

    // Used for condition checking, if we got a damage activating door we don't want to have it support pressing.
    const bool damageActivates = SVG_HasSpawnFlags( self, svg_func_door_t::SPAWNFLAG_DAMAGE_ACTIVATES );
    // Health trigger based door:
    if ( damageActivates ) {
        // Set max health, also used to reinitialize the door to revive.
        self->max_health = self->health;
        // Let it take damage.
        self->takedamage = DAMAGE_YES;
        // Die callback.
        self->SetDieCallback( &svg_func_door_t::onDie );
        self->SetPainCallback( &svg_func_door_t::onPain );
        // Apply next think time and method.
        self->nextthink = level.time + FRAME_TIME_S;
        self->SetThinkCallback( &svg_func_door_t::SVG_PushMove_Think_CalculateMoveSpeed );
    // Touch based door:svg_func_door_t::SPAWNFLAG_DAMAGE_ACTIVATES
    } else if ( SVG_HasSpawnFlags( self, svg_func_door_t::SPAWNFLAG_TOUCH_AREA_TRIGGERED ) ) {
        // Set its next think to create the trigger area.
        self->nextthink = level.time + FRAME_TIME_S;
        self->SetThinkCallback( &DoorTrigger_SpawnThink );
        self->SetUseCallback( &svg_func_door_t::onUse );
    } else {
        // Apply next think time and method.
        self->nextthink = level.time + FRAME_TIME_S;
        self->SetThinkCallback( &svg_func_door_t::SVG_PushMove_Think_CalculateMoveSpeed );

        // This door is only toggled, never untoggled, by each (+usetarget) interaction.
        if ( SVG_HasSpawnFlags( self, SPAWNFLAG_USETARGET_PRESSABLE ) ) {
            self->useTarget.flags = ENTITY_USETARGET_FLAG_PRESS;
            // Remove touch door functionality, instead, reside to usetarget functionality.
            self->SetTouchCallback( nullptr );
            self->SetUseCallback( &svg_func_door_t::onUse );
            // This door is dispatches untoggle/toggle callbacks by each (+usetarget) interaction, based on its usetarget state.
        } else if ( SVG_HasSpawnFlags( self, SPAWNFLAG_USETARGET_TOGGLEABLE ) ) {
            self->useTarget.flags = ENTITY_USETARGET_FLAG_TOGGLE;
            // Remove touch door functionality, instead, reside to usetarget functionality.
            self->SetTouchCallback( nullptr );
            self->SetUseCallback( &svg_func_door_t::onUse );
        } else if ( SVG_HasSpawnFlags( self, SPAWNFLAG_USETARGET_HOLDABLE ) ) {
            self->useTarget.flags = ENTITY_USETARGET_FLAG_CONTINUOUS;
            // Remove touch door functionality, instead, reside to usetarget functionality.
            self->SetTouchCallback( nullptr );
            self->SetUseCallback( &svg_func_door_t::onUse );
        }

        // Is usetargetting disabled by default?
        if ( SVG_HasSpawnFlags( self, SPAWNFLAG_USETARGET_DISABLED ) ) {
            self->useTarget.flags |= ENTITY_USETARGET_FLAG_DISABLED;
        }
    }

    // Copy the calculated info into the pushMoveInfo state struct.
    self->pushMoveInfo.speed = self->speed;
    self->pushMoveInfo.accel = self->accel;
    self->pushMoveInfo.decel = self->decel;
    self->pushMoveInfo.wait = self->wait;
    // For PRESSED: pos1 = start, pos2 = end.
    if ( SVG_HasSpawnFlags( self, svg_func_door_t::SPAWNFLAG_START_OPEN ) ) {
        self->pushMoveInfo.state = DOOR_STATE_OPENED;
        self->pushMoveInfo.startOrigin = self->pos1;
        self->pushMoveInfo.startAngles = self->s.angles;
        self->pushMoveInfo.endOrigin = self->pos2;
        self->pushMoveInfo.endAngles = self->s.angles;
    // For UNPRESSED: pos1 = start, pos2 = end.
    } else {
        self->pushMoveInfo.startOrigin = self->pos1;
        self->pushMoveInfo.startAngles = self->s.angles;
        self->pushMoveInfo.endOrigin = self->pos2;
        self->pushMoveInfo.endAngles = self->s.angles;
    }

	// Setup the origins and angles for movement.
	SVG_Util_SetEntityOrigin( self, self->pushMoveInfo.startOrigin, true );
	SVG_Util_SetEntityAngles( self, self->pushMoveInfo.startAngles, true );

    // Since we're closing or closed, set usetarget hint ID to 'open door'.
    door_usetarget_update_hint( self );

    // Animated doors:
    if ( SVG_HasSpawnFlags( self, svg_func_door_t::SPAWNFLAG_ANIMATED ) ) {
        self->s.entityFlags |= EF_ANIM_ALL;
    }
    if ( SVG_HasSpawnFlags( self, svg_func_door_t::SPAWNFLAG_ANIMATED_FAST ) ) {
        self->s.entityFlags |= EF_ANIM_ALLFAST;
    }

    // Locked or not locked?
    if ( SVG_HasSpawnFlags( self, svg_func_door_t::SPAWNFLAG_START_LOCKED ) ) {
        self->pushMoveInfo.lockState.isLocked = true;
    }

    // To simplify logic elsewhere, make non-teamed doors into a team of one
    if ( !self->targetNames.team ) {
        self->teammaster = self;
    }

    // Link it in.
    gi.linkentity( self );
}



/**
*
*
*
*   Member Functions:
* 
* 
**/
void svg_func_door_t::SetupDefaultSounds() {
    pushMoveInfo.sounds.start = gi.soundindex( "doors/door_start_01.wav" );
    pushMoveInfo.sounds.middle = gi.soundindex( "doors/door_mid_01.wav" );
    pushMoveInfo.sounds.end = gi.soundindex( "doors/door_end_01.wav" );

    pushMoveInfo.lockState.lockedSound = gi.soundindex( "misc/door_locked.wav" );
    pushMoveInfo.lockState.lockingSound = gi.soundindex( "misc/door_locking.wav" );
    pushMoveInfo.lockState.unlockingSound = gi.soundindex( "misc/door_unlocking.wav" );
}