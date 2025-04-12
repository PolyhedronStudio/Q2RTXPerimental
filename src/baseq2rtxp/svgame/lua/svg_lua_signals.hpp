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
*		svg_edict_t*(if not a nullptr, and entity is inuse, it will push to stack the entity number, -1 otherwise.)
*
*	Implementation for custom types can be added.
*
*
********************************************************************/
#pragma once

//! For debugging purposes.
//#define SVG_LUA_REPORT_SIGNAL_OUT 1

/**
*	@brief	Fires an 'Out' signal, calling and passing it over to the entity's luaName OnSignal function.
*	@return	True if the signal was processed.
*	@note	<discard_this>One has to deal and pop return values themselves.</discard_this>
**/
template <typename... Rest>
static const bool SVG_Lua_SignalOut( sol::state_view &stateView, svg_edict_t *ent, svg_edict_t *other, svg_edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments, const svg_lua_callfunction_verbosity_t verbosity = LUA_CALLFUNCTION_VERBOSE_MISSING, const Rest&&... rest ) {
	// False by default:
	bool executedSuccessfully = false;

	// Entity has to be non (nullptr) and active(in use).
	if ( !SVG_Entity_IsValidLuaEntity( ent, true )
		// has to be non (nullptr) and active(in use).
		|| ( activator && !SVG_Entity_IsActive( activator ) )
		// Other is optional.
		//|| ( other && !SVG_Entity_IsActive( other ) )
		// And a valid Signal Name.
		|| !signalName
		// And the entity has a valid luaName
	|| !ent->luaProperties.luaName ) {
		return executedSuccessfully;
	}

	// Generate function name.
	const std::string luaName = ent->luaProperties.luaName.ptr;
	const std::string functionName = luaName + "_OnSignalIn";

	//if ( !LUA_HasFunction( stateView, functionName ) ) {
	//	gi.dprintf( "%s: Trying to call upon Lua \"%s\" function, but it is nonexistent!\n", __func__, functionName.c_str() );
	//	return false;
	//}

	// Get function object.
	sol::protected_function funcRefSignalOut = stateView[ functionName ];
	// Get type.
	sol::type funcRefType = funcRefSignalOut.get_type();
	
	// Ensure it matches, accordingly
	if ( funcRefType != sol::type::function && verbosity == LUA_CALLFUNCTION_VERBOSE_MISSING /*|| !funcRefSignalOut.is<std::function<void( Rest... )>>() */) {
		// Return if it is LUA_NOREF and luaState == nullptr again.
		gi.bprintf( PRINT_ERROR, "%s: %s but is %s instead! function(%s) not found!\n", __func__, "funcRefType != sol::type::function", sol::type_name( stateView, funcRefType ).c_str(), functionName.c_str() );
		return false;
	}

	// Resulting signal handled or not.
	bool signalHandled = false;
	// Create lua userdata object references to the entities.
	sol::userdata leEnt = sol::make_object<lua_edict_t>( stateView, ent );
	sol::userdata leOther = sol::make_object<lua_edict_t>( stateView, other );
	sol::userdata leActivator = sol::make_object<lua_edict_t>( stateView, activator );
	// Fire SignalOut callback.
	sol::protected_function_result callResult = funcRefSignalOut( leEnt, leOther, leActivator, signalName, signalArguments );
	// If valid, convert result to boolean.
	if ( callResult.valid() ) {
		// Convert.
		signalHandled = true;// callResult.get();
		// Debug print.
		#ifdef SVG_LUA_REPORT_SIGNAL_OUT
		gi.dprintf( "luaName(%s): function(%s) fired signal(\"%s\")\n", luaName.c_str(), functionName.c_str(), signalName, ent->luaProperties.luaName );
		#endif
	// We got an error:
	} else {
		if ( verbosity != LUA_CALLFUNCTION_VERBOSE_NOT ) {
			// Acquire error object.
			sol::error resultError = callResult;
			// Get error string.
			const std::string errorStr = resultError.what();
			// Print the error in case of failure.
			gi.bprintf( PRINT_ERROR, "%s: %s\n", __func__, errorStr.c_str() );
		}
	}

	return signalHandled;
}