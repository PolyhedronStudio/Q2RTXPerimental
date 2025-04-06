/********************************************************************
*
*
*	ServerGame: Lua GameLib API UserType.
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_signalio.h"
#include "svgame/svg_utils.h"

#include "svgame/svg_lua.h"
#include "svgame/lua/svg_lua_gamelib.hpp"
#include "svgame/lua/svg_lua_signals.hpp"


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
            // TODO: Hmm?? Sol has no notion, maybe though using sol::object.as however, of an integer??
            #if 0
        } else if ( signalArgument->type == SIGNAL_ARGUMENT_TYPE_INTEGER ) {
            gi.dprintf( "%s:%d\n", signalArgument->key, signalArgument->value.integer );
            #endif
        } else if ( signalArgument->type == SIGNAL_ARGUMENT_TYPE_NUMBER ) {
            gi.dprintf( "%s:%f\n", signalArgument->key, signalArgument->value.number );
        } else if ( signalArgument->type == SIGNAL_ARGUMENT_TYPE_STRING ) {
            gi.dprintf( "%s:%s\n", signalArgument->key, signalArgument->value.str );
        } else if ( signalArgument->type == SIGNAL_ARGUMENT_TYPE_NULLPTR ) {
            gi.dprintf( "%s:%s\n", signalArgument->key, "(nil)" );
            // Fall through:
            //} else if ( signalArgument->type == SIGNAL_ARGUMENT_TYPE_NONE ) {
        } else {
            gi.dprintf( "%s:%s\n", signalArgument->key, "(invalid type)" );
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
        delayEntity->nextthink = level.time + QMTime::FromSeconds( ent->delay );
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
        /*propogateToLua = */ent->onsignalin( ent, signaller, activator, signalName, signalArguments );
    }
    // If desired, propogate the signal to Lua '_OnSignalIn' callbacks.
    if ( propogateToLua ) {
        SVG_Lua_SignalOut( SVG_Lua_GetSolState(), ent, signaller, activator, signalName, signalArguments, LUA_CALLFUNCTION_VERBOSE_NOT );
    }
}