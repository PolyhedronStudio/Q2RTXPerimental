/********************************************************************
*
*
*	ServerGmae: Lua Signal Utilities: -
*
*	Uses variadic arg template method to allow for multiple arguments.
*	Default supported types are:
*		uint64_t
*		int64_t
*		uint32_t
*		int32_t
*		double
*		float
*		std::string
*		char*(because of std::string)
*		edict_t*(if not a nullptr, and entity is inuse, it will push to stack the entity number, -1 otherwise.)
*
*	Implementation for custom types can be added.
*
*
********************************************************************/
#pragma once


/**
*	@brief	Fires an 'Out' signal, calling and passing it over to the entity's luaName OnSignal function.
*	@return	True if the signal was processed.
*	@note	<discard_this>One has to deal and pop return values themselves.</discard_this>
**/
template <typename... Rest>
static const bool SVG_Lua_SignalOut( lua_State *L, edict_t *ent, edict_t *activator, const std::string &signalName, const svg_lua_callfunction_verbosity_t verbosity = LUA_CALLFUNCTION_VERBOSE_MISSING, const Rest&... rest ) {
	// False by default:
	bool executedSuccessfully = false;

	// Activator needs to be active(in use) if not nullptr.
	if ( !SVG_IsValidLuaEntity( ent ) || ( activator && !SVG_IsActiveEntity( activator ) ) || !L || signalName.empty() ) {
		return executedSuccessfully;
	}

	// Generate function name.
	const std::string functionName = std::string( ent->luaProperties.luaName ) + "_OnSignal";

	// Call function, verbose, because OnSignals may not exist.
	executedSuccessfully = LUA_CallFunction( L, functionName, 1, verbosity, 
		/*[lua args]:*/  ent, activator, signalName, rest... );

	// Debug print.
	gi.dprintf( "luaNameTarget(%s): fired signal(%s)\n", functionName.c_str(), signalName.c_str(), ent->luaProperties.luaName );

	// If it did successfully execute..
	if ( executedSuccessfully ) {
		// Get from stack.
		lua_Integer retval = lua_toboolean( L, -1 );
		// Did signal return true?
		if ( static_cast<bool>( retval ) == true ) {
			//lua_pop( L, 1 );
			// Pop stack.
			lua_pop( L, lua_gettop( L ) );
			return true;
		}
	} else {
		// Pop stack.
		lua_pop( L, lua_gettop( L ) );
	}

	// Return failure.
	return false/*executedSuccessfully*/;
}