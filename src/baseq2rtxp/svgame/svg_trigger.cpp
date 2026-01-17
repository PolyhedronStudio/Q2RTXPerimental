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
#include "svgame/svg_local.h"
#include "svgame/svg_entity_events.h"
#include "svgame/svg_trigger.h"
#include "svgame/svg_utils.h"

#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_gamelib.hpp"



/**
*
*
*
*   Utilities, common for UseTargets trigger system. (Also used in lua usetargets code.)
*
*
*
**/
/**
*   @brief  Calls the (usually key/value field luaName).."_Use" matching Lua function.
**/
const bool SVG_Trigger_DispatchLuaUseCallback( sol::state_view &stateView, const std::string &luaName, bool &functionReturnValue, svg_base_edict_t *entity, svg_base_edict_t *other, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue, const bool verboseIfMissing ) {
    if ( luaName.empty() ) {
        return false;
    }

    // Generate function 'callback' name.
    const std::string luaFunctionName = luaName + "_Use";
    // Call if it exists.
    if ( LUA_HasFunction( stateView, luaFunctionName ) ) {
        // Create  lua edict handle structures to work with.
        sol::userdata leEntity = sol::make_object<lua_edict_t>( stateView, lua_edict_t( entity ) );
        sol::userdata leOther = sol::make_object<lua_edict_t>( stateView, lua_edict_t( other ) );
        sol::userdata leActivator = sol::make_object<lua_edict_t>( stateView, lua_edict_t( activator ) );

        // Call upon the function.
        bool returnValue = false;
        bool calledFunction = LUA_CallFunction(
            stateView, luaFunctionName, returnValue,
            ( verboseIfMissing ? LUA_CALLFUNCTION_VERBOSE_MISSING : LUA_CALLFUNCTION_VERBOSE_NOT ),
            /*[lua args]:*/
            leEntity, leOther, leActivator, useType, useValue
        );
        // Dispatched callback.
        return returnValue;
    }

    // Didn't dispatch callback.
    return false;
}
/**
*   @brief  Centerprints the trigger message and plays a set sound, or default chat hud sound.
**/
void SVG_Trigger_PrintMessage( svg_base_edict_t *self, svg_base_edict_t *activator ) {
    // If a message was set, the activator is not a monster, then center print it.
    if ( ( self->message ) && !( activator && activator->svFlags & SVF_MONSTER ) ) {
        // Print.
        if ( activator && activator->client ) {
            gi.centerprintf( activator, "%s", (const char *)self->message );
        // <Q2RTXP>: WID: Do we want this else statement and condition?
        } else {
			//gi.cprintf(nullptr, PRINT_ALL, "%s: %s\n", (const char*)self->classname, self->message.ptr );
            gi.cprintf( nullptr, PRINT_ALL, "%s\n", (const char *)self->message );
        }
        // Play custom set audio.
        if ( self->noise_index ) {
            //gi.sound( activator, CHAN_AUTO, self->noise_index, 1, ATTN_NORM, 0 );
			SVG_EntityEvent_GeneralSoundEx( activator, CHAN_AUTO, self->noise_index, ATTN_NORM );
            // Play default "chat" hud sound.
        } else {
            //gi.sound( activator, CHAN_AUTO, gi.soundindex( "hud/chat01.wav" ), 1, ATTN_NORM, 0 );
			SVG_EntityEvent_GeneralSoundEx( activator, CHAN_AUTO, gi.soundindex( "hud/chat01.wav" ), ATTN_NORM );
        }
    }
}
/**
*   @brief  Kills all entities matching the killtarget name.
**/
const int32_t SVG_Trigger_KillTargets( svg_base_edict_t *self ) {
    if ( self->targetNames.kill ) {
        svg_base_edict_t *killTargetEntity = nullptr;
        while ( ( killTargetEntity = SVG_Entities_Find( killTargetEntity, q_offsetof( svg_base_edict_t, targetname ), (const char *)self->targetNames.kill ) ) ) {
            SVG_FreeEdict( killTargetEntity );
            if ( !self->inUse ) {
                gi.dprintf( "%s: entity(#%d, \"%s\") was removed while using killtargets\n", __func__, self->s.number, (const char *)self->classname );
                return false;
            }
        }
    }

    return true;
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
//! Maximum number of entities to pick target from.
static constexpr int32_t PICKTARGET_MAX = 8;
/**
*   @brief  Pick a random target of entities with a matching targetname.
**/
svg_base_edict_t *SVG_PickTarget( const char *targetname ) {
    svg_base_edict_t *ent = NULL;
    int     num_choices = 0;
    svg_base_edict_t *choice[ PICKTARGET_MAX ];

    if ( !targetname ) {
        gi.dprintf( "SVG_PickTarget called with NULL targetname\n" );
        return NULL;
    }

    while ( 1 ) {
        ent = SVG_Entities_Find( ent, q_offsetof( svg_base_edict_t, targetname ), targetname );
        if ( !ent )
            break;
        choice[ num_choices++ ] = ent;
        if ( num_choices == PICKTARGET_MAX )
            break;
    }

    if ( !num_choices ) {
        gi.dprintf( "SVG_PickTarget: target %s not found\n", targetname );
        return NULL;
    }

    return choice[ Q_rand_uniform( num_choices ) ];
}

/**
*   @brief  Support routine for delayed trigger Use Targets.
**/
DECLARE_GLOBAL_CALLBACK_THINK( Think_UseTargetsDelay );
DEFINE_GLOBAL_CALLBACK_THINK( Think_UseTargetsDelay )( svg_base_edict_t *self ) -> void {
    SVG_UseTargets( self, self->activator, self->delayed.useTarget.useType, self->delayed.useTarget.useValue, true );
    SVG_FreeEdict( self );
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
void SVG_UseTargets( svg_base_edict_t *ent, svg_base_edict_t *activator, const entity_usetarget_type_t useType, const int32_t useValue, const bool isDelayed ) {
    //
    // Check for a delay
    //
    if ( ent->delay ) {
        // create a temp object to fire at a later time
        svg_base_edict_t *delayEntity = g_edict_pool.AllocateNextFreeEdict<svg_base_edict_t>();
        delayEntity->classname = "DelayedUseTargets";
        delayEntity->nextthink = level.time + QMTime::FromSeconds( ent->delay );
        delayEntity->SetThinkCallback( Think_UseTargetsDelay );
        if ( !activator ) {
            gi.dprintf( "Think_UseTargetsDelay with no activator\n" );
        }
        delayEntity->activator = activator;
        delayEntity->other = ent->other;
        delayEntity->message = ent->message;

        delayEntity->targetEntities.target = ent->targetEntities.target;
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
    SVG_Trigger_PrintMessage( ent, activator );

    //
    // kill killtargets
    //
    SVG_Trigger_KillTargets( ent );

    //
    // fire targets
    //
    if ( ent->targetNames.target ) {
        svg_base_edict_t *fireTargetEntity = nullptr;
        while ( ( fireTargetEntity = SVG_Entities_Find( fireTargetEntity, q_offsetof( svg_base_edict_t, targetname ), (const char *)ent->targetNames.target ) ) ) {
            // Doors fire area portals in a specific way
            if ( !Q_stricmp( (const char *)fireTargetEntity->classname, "func_areaportal" )
                && ( !Q_stricmp( (const char *)ent->classname, "func_door" ) || !Q_stricmp( (const char *)ent->classname, "func_door_rotating" ) ) ) {
                continue;
            }

            if ( fireTargetEntity == ent ) {
                gi.dprintf( "%s: entity(#%d, \"%s\") used itself!\n", __func__, ent->s.number, (const char *)ent->classname );
            } else {
                if ( fireTargetEntity->HasUseCallback() ) {
                    fireTargetEntity->DispatchUseCallback( ent, activator, useType, useValue );
                }

                if ( fireTargetEntity->luaProperties.luaName ) {
                    // Generate function 'callback' name.
                    const std::string luaFunctionName = std::string( fireTargetEntity->luaProperties.luaName.ptr ) + "_Use";
                    // Get reference to sol lua state view.
                    sol::state_view &solStateView = SVG_Lua_GetSolState();

                    bool returnValue = false;
                    const bool functionCalled = SVG_Trigger_DispatchLuaUseCallback( solStateView, fireTargetEntity->luaProperties.luaName.ptr,
                        returnValue,
                        fireTargetEntity, ent, activator, useType, useValue, true );
                    //// Get function object.
                    //sol::protected_function funcRefUse = solState[ luaFunctionName ];
                    //// Get type.
                    //sol::type funcRefType = funcRefUse.get_type();
                    //// Ensure it matches, accordingly
                    //if ( funcRefType != sol::type::function /*|| !funcRefSignalOut.is<std::function<void( Rest... )>>() */ ) {
                    //    // Return if it is LUA_NOREF and luaState == nullptr again.
                    //    // TODO: Error?
                    //    return;
                    //}

                    //// Create lua userdata object references to the entities.
                    //auto leSelf = sol::make_object<lua_edict_t>( solState, lua_edict_t( fireTargetEntity ) );
                    //auto leOther = sol::make_object<lua_edict_t>( solState, lua_edict_t( ent ) );
                    //auto leActivator = sol::make_object<lua_edict_t>( solState, lua_edict_t( activator ) );
                    //// Fire SignalOut callback.
                    //auto callResult = funcRefUse( leSelf, leOther, leActivator, useType, useValue );
                    //// If valid, convert result to boolean.
                    //if ( callResult.valid() ) {
                    //    // Convert.
                    //    bool signalHandled = callResult.get<bool>();
                    //    // Debug print.
                    //// We got an error:
                    //} else {
                    //    // Acquire error object.
                    //    sol::error resultError = callResult;
                    //    // Get error string.
                    //    const std::string errorStr = resultError.what();
                    //    // Print the error in case of failure.
                    //    gi.bprintf( PRINT_ERROR, "%s: %s\n ", __func__, errorStr.c_str() );
                    //}
                }
            }
            if ( !ent->inUse ) {
                gi.dprintf( "%s: entity(#%d, \"%s\") was removed while using killtargets\n", __func__, ent->s.number, (const char *)ent->classname );
                return;
            }
        }
    }
}